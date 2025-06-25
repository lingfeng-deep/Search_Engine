#include "../include/SearchServer.h"
#include <chrono>
#include <sw/redis++/redis++.h>

using namespace sw::redis;

namespace {
    // 二次优化后的最小编辑距离算法
    int editDistance(const string& s1, const string& s2) {
        // 添加长度过滤
        if (std::abs(int(s1.size()) - int(s2.size())) > 32)
            return std::max(s1.size(), s2.size());

        // 添加UTF-8字符缓存
        static thread_local vector<uint32_t> codes1, codes2;
        codes1.clear();
        codes2.clear();
        utf8::utf8to32(s1.begin(), s1.end(), back_inserter(codes1));
        utf8::utf8to32(s2.begin(), s2.end(), back_inserter(codes2));

        // 使用滚动数组计算
        const size_t m = codes1.size();
        const size_t n = codes2.size();

        vector<int> dp(n + 1);
        std::iota(dp.begin(), dp.end(), 0);

        for (size_t i = 1; i <= m; ++i) {
            int prev = dp[0];
            dp[0] = i;
            for (size_t j = 1; j <= n; ++j) {
                int temp = dp[j];
                if (codes1[i-1] == codes2[j-1]) {
                    dp[j] = prev;
                } else {
                    dp[j] = 1 + std::min({prev, dp[j], dp[j-1]});
                }
                prev = temp;
            }
        }
        return dp[n];
    }

}


void SearchServer::register_modules()
{
    // 设置静态资源的路由
    register_static_resources_module();
    // 设置关键字请求的路由
    register_keyword_module();
    // 设置网页搜索请求的路由
    register_search_module();
}

// 设置静态资源的路由
void SearchServer::register_static_resources_module()
{
    m_server.GET("/homepage", [](const HttpReq *, HttpResp * resp){
        resp->File("../static/index.html");
    });

}

// 设置关键字推荐请求的路由
void SearchServer::register_keyword_module() {
    m_server.POST("/keywords", [this](const HttpReq *req, HttpResp * resp, SeriesWork* series){
        // 记录开始时间点
        auto st = std::chrono::high_resolution_clock::now();

        // 自动解析JSON
        Json body = req->json();
    
        // 提取字段
        std::string value = body["value"];
        /* int tag = body["tag"]; */ 

        // 对客户端发来的关键词切割, 使用utfcpp切割成单个字符
        // 然后排序, 查找缓存
        vector<string> phrase;
        const char* it = value.c_str();
        const char* end = value.c_str() + value.size();
        while (it < end) {
            const char* start = it; 
            utf8::next(it, end);    // 将it移动到下一个utf8字符的起始位置 
            std::string alpha {start, it}; // 拿到了单个字符
            if(alpha.size() == 1) { // 字符是英文字符
                if(this->en_index.find(alpha) != this->en_index.end()) { // 这里其实就去掉了停用词和非法字符
                    phrase.push_back(alpha);
                }
            } else { // 字符是中文字符
                if(this->cn_index.find(alpha) != this->cn_index.end()) {
                    phrase.push_back(alpha);
                }
            }
        }

        // 使用标准库排序（默认字母表顺序）
        std::sort(phrase.begin(), phrase.end());
        // 字符串拼接
        string result;
        for(auto & alpha : phrase) {
            result += alpha;
        }

        // 判断是否命中LRU缓存
        // 先获取当前线程ID
        pthread_t tid = pthread_self();
        string res = key_word_thread_caches[tid].cache.get(result); // 查询LRU缓存

        bool LRU_hit = false; // 是否命中LRU缓存, 默认否
        bool Redis_hit = false; // 是否命中Redis缓存, 默认否

        if(res.empty() == false) { // 命中LRU缓存
            resp->String(res); 
            LRU_hit = true;
        } else {
            // 走到这里说明没有命中LRU缓存

            // 引入Redis缓存, 存全量/历史数据(容量大, 覆盖LRU未命中的长尾请求)
            // 判断是否命中Redis缓存, 关键字推荐使用0号数据库
            Redis redis("tcp://127.0.0.1:6379/0");
            auto redis_res = redis.get(result);
            if(redis_res.has_value()) { // 命中Redis缓存
                resp->String(redis_res.value());
                // 把结果存入缓存和补丁缓存
                key_word_thread_caches[tid].cache.put(result, redis_res.value());
                key_word_thread_caches[tid].patch.put(result, redis_res.value());
                Redis_hit = true;
            } else {
                // 走到这里说明没有命中Redis缓存
                // 找到所有中英文候选词, 对候选词进行排序, 选择优先级队列, 使用自定义比较器
                unordered_map<string, int> candidate_words_dict; // 所有候选词的词频
                vector<string> candi_words; // 所有的候选词
                for(auto & alpha : phrase) {
                    if(alpha.size() == 1) { // 字符是英文字符
                        for(auto i : this->en_index[alpha]) {
                            // 替换set取并集, 用这种方式不仅取了并集, 而且去了重
                            if(candidate_words_dict.find(this->en_dict[i - 1].first) == candidate_words_dict.end()) {
                                candidate_words_dict.insert(this->en_dict[i - 1]);
                                candi_words.push_back(this->en_dict[i - 1].first);
                            }
                        }
                    } else { // 字符是中文字符
                        for(auto i : this->cn_index[alpha]) {
                            if(candidate_words_dict.find(this->cn_dict[i - 1].first) == candidate_words_dict.end()) {
                                candidate_words_dict.insert(this->cn_dict[i - 1]);
                                candi_words.push_back(this->cn_dict[i - 1].first);
                            }   
                        }
                    }
                }   

                unordered_map<std::string_view, int> edit_distance; // 预存所有候选词的最小编辑距离
                for(auto & ele : candidate_words_dict) {
                    edit_distance[ele.first] = editDistance(ele.first, value); // 改用C++17版本的std::string_view, 避免字符串的拷贝
                }
                /* priority_queue<string, vector<string>, CandidateComparator> candidates_sort(CandidateComparator(value, candidate_words_dict)); */

                int K = 5; // 找前5个候选词
                /* std::nth_element( */
                /*                  candi_words.begin(),          // 起始迭代器 */
                /*                  candi_words.begin() + K - 1,  // 第K小的位置 */
                /*                  candi_words.end(),            // 结束迭代器 */
                /*                  CandidateComparator(value, candidate_words_dict)); */

                K = min((int)candi_words.size(), K); // 防止候选词没有5个导致下面数组下标访问越界
                                                     // std::nth_element
                                                     // std::partial_sort
                std::partial_sort(
                                  candi_words.begin(),          // 起始迭代器
                                  candi_words.begin() + K,  // partial_sort第二个参数不减一
                                  candi_words.end(),            // 结束迭代器
                                  CandidateComparator(ref(value), ref(candidate_words_dict), ref(edit_distance)));

                /* cout << "候选词的数量: " << candidate_words_dict.size() << "\n"; */

                /* int i = 1; */
                /* for(auto & ele : candidate_words_dict) { */
                /*     candidates_sort.push(ele.first); */
                /*     /1* cout << i++ << "\n"; *1/ */
                /* } */

                // 取出优先级队列中前K个候选词, 转成json::Array格式
                json cands = json::array();

                for(int i = 0; i < K; ++i) {
                    /* cands[i] = candi_words[i]; */
                    cands.push_back(candi_words[i]);
                }

                resp->String(cands.dump());

                // 把结果存入缓存和补丁缓存
                key_word_thread_caches[tid].cache.put(result, cands.dump());
                key_word_thread_caches[tid].patch.put(result, cands.dump());

                // 把结果放入Redis缓存
                redis.set(result, cands.dump());

            }
        }
        // 记录结束时间点
        auto ed = std::chrono::high_resolution_clock::now();

        // 计算时间差（单位自动转换）
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(ed - st);

        /* cout << "候选词的数量: " << candidate_words_dict.size() << "\n"; */

        /* cout << cands.dump() << "\n"; */

        cout << "Keyword recommendation : " << value << endl;

        if(LRU_hit == true) {
            cout << "Hit the LRU cache." << endl;
        } else {
            cout << "Not Hit the LRU cache." << endl;
        }

        if(Redis_hit == true) {
            cout << "Hit the Redis cache." << endl;
        } else {
            cout << "Not Hit the Redis cache." << endl;
        }

        /* cout << res << endl; */

         // 输出结果
        std::cout << "执行耗时: " 
                    << duration.count() << " 微秒 ("
                     << duration.count() / 1000.0 << " 毫秒)"
                    << std::endl;

    });
}

// 设置网页搜索请求的路由
void SearchServer::register_search_module() {
    m_server.POST("/search", [this](const HttpReq *req, HttpResp * resp, SeriesWork* series) {
        // 记录开始时间点
        auto st = std::chrono::high_resolution_clock::now();

        // 自动解析JSON
        Json body = req->json();
    
        // 提取字段
        std::string content = body["value"];
        /* int tag = body["tag"]; */ 

        /* cout << "111" << endl; // 用来做性能测试的 */

        // 对客户端发送过来的内容分词
        // 使用cppjieba分词, static延迟初始化, 第一次被实际需要时才创建
        /* static cppjieba::Jieba tokenizer; // 测试发现创建Jieba对象耗时很长，使用static代替单例模式方案 */
        std::vector<std::string> words; // 分割后的所有关键词
        {
            std::lock_guard<std::mutex> lock(tokenizer_mutex);
            this->tokenizer.Cut(content, words);
        }
        multiset<string> nostopWors; // 清洗停用词和非法字符后的所有关键词
        std::wregex pattern(LR"(^[\u4E00-\u9FFFa-zA-Z]+$)");
        for(auto & word : words) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::wstring temp = converter.from_bytes(word);
            if(std::regex_match(temp, pattern) && this->stopWords.find(word) == this->stopWords.end()) {
                nostopWors.insert(word);
            }
        }

        // 字符串拼接
        string result;
        for(auto & word : nostopWors) {
            result += word;
        }

        // 判断是否LRU命中缓存
        // 先获取当前线程ID
        pthread_t tid = pthread_self();
        string res = web_search_thread_caches[tid].cache.get(result); // 查询LRU缓存

        bool LRU_hit = false; // 是否命中LRU缓存, 默认否
        bool Redis_hit = false; // 是否命中Redis缓存, 默认否

        if(res.empty() == false) { // 命中LRU缓存
            resp->String(res); 
            LRU_hit = true;
        } else {
            // 走到这里说明没有命中LRU缓存

            // 引入Redis缓存, 存全量/历史数据(容量大, 覆盖LRU未命中的长尾请求)
            // 判断是否命中Redis缓存, 网页搜索使用1号数据库
            Redis redis("tcp://127.0.0.1:6379/1");
            auto redis_res = redis.get(result);
            if(redis_res.has_value()) { // 命中Redis缓存
                resp->String(redis_res.value());
                // 把结果存入缓存和补丁缓存
                web_search_thread_caches[tid].cache.put(result, redis_res.value());
                web_search_thread_caches[tid].patch.put(result, redis_res.value());
                Redis_hit = true;
            } else {
                // 走到这里说明没有命中Redis缓存
                // 通过倒排索引查找包含所有关键字(去掉停用词)的网页, 只要有查询词不在索引表, 返回的就是空
                set<int> docids;
                for(auto & word : nostopWors) { // 遍历关键词
                    if(this->invertedIndex.find(word) == this->invertedIndex.end()) { // 查询词不在索引表
                        docids.clear();
                        break;
                    }
                    set<int> temp;
                    for(auto & e : this->invertedIndex[word]) {
                        temp.insert(e.first);
                    }
                    if(docids.size() == 0) {
                        docids = std::move(temp);
                        continue;
                    }
                    set<int> res;
                    std::set_intersection(
                                          docids.begin(), docids.end(),
                                          temp.begin(), temp.end(),
                                          std::inserter(res, res.begin())
                                         );
                    docids = std::move(res);
                } 

                // 计算用户输入内容的每个关键词的权重
                vector<double> reference_vec;
                double wsum = 0;
                for(auto & word : nostopWors) {
                    int TF = nostopWors.count(word);
                    int DF = 1;
                    double IDF = log2(1 / ((double)DF + 1) + 1);
                    double w = TF * IDF;
                    wsum += (w * w);
                    reference_vec.push_back(w);
                }
                // 归一化
                for(auto & w : reference_vec) {
                    w = w / sqrt(wsum);
                }

                /* multimap<double, int, greater<double>> cosine_docid; // 存储余弦和docid */
                vector<pair<double, int>> cosine_docid;
                // 遍历所有的筛选后的网页, 拿到每个网页里所有关键词的权重, 计算余弦相似度
                for(auto docid : docids) {
                    vector<double> wvec; // 每篇文章的向量
                    for(auto & word : nostopWors) {
                        wvec.push_back(this->invertedIndex[word][docid]);
                    }
                    double molecule = 0;
                    double ben = 0;
                    double other = 0;
                    for(int i = 0; i < (int)wvec.size(); ++i) {
                        molecule += (wvec[i] * reference_vec[i]);
                        ben += (reference_vec[i] * reference_vec[i]);
                        other += (wvec[i] * wvec[i]);
                    }
                    double cosine = molecule / (sqrt(ben) * sqrt(other));
                    cosine_docid.emplace_back(cosine, docid);
                }

                /* cout << "444" << endl; */

                // 部分排序
                int k = 15;
                k = min((int)cosine_docid.size(), k);
                std::partial_sort(
                                  cosine_docid.begin(),
                                  cosine_docid.begin() + k,
                                  cosine_docid.end(),
                                  [](const pair<double, int>& a, const pair<double, int>& b) {
                                  return a.first > b.first; // 降序排序
                                  }
                                 );

                // 全排序 - 最佳通用方案
                /* std::sort(cosine_docid.begin(), cosine_docid.end(), */
                /*           [](const pair<double, int>& a, const pair<double, int>& b) { */
                /*           return a.first > b.first; // 降序排序 */
                /*           }); */

                json data = json::array();
                for(int i = 0; i < k; ++i) {
                    data[i]["id"] = cosine_docid[i].second;
                    string content = Util::readFileChunk("../data/page.dat", this->pageOffset[cosine_docid[i].second - 1].first, this->pageOffset[cosine_docid[i].second - 1].second);
                    data[i]["title"] = Util::extractTitle(content, "<title>", "</title>");
                    data[i]["url"] = Util::extractTitle(content, "<url>", "</url>");
                    string desc = Util::extractTitle(content, "<content>", "</content>");
                    string abstract = Util::utf8Substr(desc, 50);
                    data[i]["abstract"] = abstract;
                }

                resp->String(data.dump());

                // 把结果存入缓存和补丁缓存
                web_search_thread_caches[tid].cache.put(result, data.dump());
                web_search_thread_caches[tid].patch.put(result, data.dump());

                // 把结果放入Redis缓存
                redis.set(result, data.dump());
            }
        }
        // 记录结束时间点
        auto ed = std::chrono::high_resolution_clock::now();

        // 计算时间差（单位自动转换）
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(ed - st);

        /* cout << data.dump(2) << "\n"; // 放到后面打印，这样不影响回复客户端的效率 */
        /* cout << "web search results : " << k << endl; */

        cout << "Web search : " << content << endl;

        if(LRU_hit == true) {
            cout << "Hit the LRU cache." << endl;
        } else {
            cout << "Not Hit the LRU cache." << endl;
        }

        if(Redis_hit == true) {
            cout << "Hit the Redis cache." << endl;
        } else {
            cout << "Not Hit the Redis cache." << endl;
        }

         // 输出结果
        std::cout << "执行耗时: " 
                    << duration.count() << " 微秒 ("
                     << duration.count() / 1000.0 << " 毫秒)"
                    << std::endl;

    });
}

// 加载英文词典资源
void SearchServer::load_en_dict() {
    // 把英文词典库加载到内存里
    ifstream ifs_dict_en("../data/dict_en.dat");
    if(ifs_dict_en.is_open() == false) {
        cerr << "dict_en.dat open failed." << "\n";
        return ;
    }
    string word;
    int freq;
    while(ifs_dict_en >> word >> freq) {
        en_dict.push_back(pair(word, freq));
    }
    ifs_dict_en.close();
}

// 加载中文词典资源
void SearchServer::load_cn_dict() {
    // 把中文词典库加载到内存里
    ifstream ifs_dict_cn("../data/dict_cn.dat");
    if(ifs_dict_cn.is_open() == false) {
        cerr << "dict_cn.dat open failed." << "\n";
        return ;
    }
    string word;
    int freq;
    while(ifs_dict_cn >> word >> freq) {
        cn_dict.push_back(pair(word, freq));
    }
    ifs_dict_cn.close();
}

// 加载英文词典索引资源
void SearchServer::load_en_index() {
    // 把英文词典索引库加载到内存里
    ifstream ifs_index_en("../data/dictIndex_en.dat");
    if(ifs_index_en.is_open() == false) {
        cerr << "dictIndex_en.dat open failed." << "\n";
        return ;
    }
    string line;
    string word;
    while(getline(ifs_index_en, line)) {
        istringstream iss(line);
        iss >> word;
        int line_num;
        while(iss >> line_num) {
            en_index[word].insert(line_num);
        }
    }
    ifs_index_en.close();
}

// 加载中文词典索引资源
void SearchServer::load_cn_index() {
    // 把中文词典索引库加载到内存里
    ifstream ifs_index_cn("../data/dictIndex_cn.dat");
    if(ifs_index_cn.is_open() == false) {
        cerr << "dictIndex_cn.dat open failed." << "\n";
        return ;
    }
    string line;
    string word;
    while(getline(ifs_index_cn, line)) {
        istringstream iss(line);
        iss >> word;
        int line_num;
        while(iss >> line_num) {
            cn_index[word].insert(line_num);
        }
    }
    ifs_index_cn.close();
}

// 加载倒排索引库资源
void SearchServer::load_invertedIndex() {
    // 把倒排索引库加载到内存里
    ifstream ifs_index("../data/invertIndex.dat");
    if(ifs_index.is_open() == false) {
        cerr << "invertIndex.dat open failed." << "\n";
        return ;
    }
    string line;
    string word;
    while(getline(ifs_index, line)) {
        istringstream iss(line);
        iss >> word;
        int doc_id;
        double weight;
        while(iss >> doc_id >> weight) {
            invertedIndex[word].insert({doc_id, weight});
        }
    }
    ifs_index.close();
}

// 加载网页偏移库资源
void SearchServer::load_pageOffset() {
    // 把网页偏移库加载到内存里
    ifstream ifs_off("../data/page_offset.dat");
    if(ifs_off.is_open() == false) {
        cerr << "page_offset.dat open failed." << "\n";
        return ;
    }
    int doc_id;
    int pos;
    int len;
    while(ifs_off >> doc_id >> pos >> len) {
        pageOffset.push_back(pair(pos, len));
    }
    ifs_off.close();
}

// 加载中英文停用词资源
void SearchServer::load_stopWords() {
    std::ifstream ifs_en("../corpus/stopwords/en_stopwords.txt");
    if(!ifs_en.is_open()) {
        std::cerr << "en_stopwords.txt open failed." << "\n";
    }
    std::string stopword;
    while(ifs_en >> stopword) { // 输入流符号会自动处理'\r'
        stopWords.insert(stopword);
    }
    ifs_en.close();

    std::ifstream ifs_cn("../corpus/stopwords/cn_stopwords.txt");
    if(!ifs_cn.is_open()) {
        std::cerr << "cn_stopwords.txt open failed." << "\n";
    }

    while(ifs_cn >> stopword) { // 输入流符号会自动处理'\r'
        stopWords.insert(stopword);
    }
    ifs_cn.close();
}

// 启动定时器
void SearchServer::start_timer() {
    // 创建定时器任务，每10秒执行一次
    WFTimerTask *timer_task = WFTaskFactory::create_timer_task(10, 0, [this](WFTimerTask *) {

        if(stop_flag) return;
        // 直接执行缓存同步任务
        this->maintenance_task();

        if(stop_flag) return;
        // 重新设置定时器，实现循环
        this->start_timer();
    });

    // 启动定时器任务
    timer_task->start();
}

// 进行缓存同步
void SearchServer::maintenance_task() {
    // 关键字推荐缓存同步
    // 把每个线程的patch添加到主缓存
    for(auto & pr : key_word_thread_caches) {
        for(auto & kv : pr.second.patch.get_all_items()) {
            key_word_main_cache.put(kv.first, kv.second);
        }
    }

    // 把主缓存复制给每个线程的cache
    for(auto & pr : key_word_thread_caches) {
        pr.second.cache.copy_from(key_word_main_cache);
    }

    // 网页搜索缓存同步
    // 把每个线程的patch添加到主缓存
    for(auto & pr : web_search_thread_caches) {
        for(auto & kv : pr.second.patch.get_all_items()) {
            web_search_main_cache.put(kv.first, kv.second);
        }
    }

    // 把主缓存复制给每个线程的cache
    for(auto & pr : web_search_thread_caches) {
        pr.second.cache.copy_from(web_search_main_cache);
    }
}
