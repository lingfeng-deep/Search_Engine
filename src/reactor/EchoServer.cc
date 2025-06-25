#include <my_header.h>
#include "EchoServer.h"
#include "TcpConnection.h"
#include "CandidateComparator.hpp"
#include "Util.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <set>
#include <map>
#include <unordered_map>
#include <functional>
#include <queue>
#include <algorithm>
#include <regex>
#include <locale>
#include "utfcpp/utf8.h"
#include "nlohmann/json.hpp"
#include "cppjieba/Jieba.hpp"

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

    void PreStoreEditDistance() {
    }
}

using namespace std;
using json = nlohmann::json;

MyTask::MyTask(const Message &msg, const TcpConnectionPtr &con)
: _msg(msg)
, _con(con)
{

}

void MyTask::process()
{
    if(_msg.tag == 1) { // 客户端是关键字推荐请求

        vector<pair<string, int>> en_dict; // 英文词典
        vector<pair<string, int>> cn_dict; // 中文词典
        map<string, set<int>> en_index; // 英文词典索引
        map<string, set<int>> cn_index; // 中文词典索引

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

        // 把中文词典库加载到内存里
        ifstream ifs_dict_cn("../data/dict_cn.dat");
        if(ifs_dict_cn.is_open() == false) {
            cerr << "dict_cn.dat open failed." << "\n";
            return ;
        }
        while(ifs_dict_cn >> word >> freq) {
            cn_dict.push_back(pair(word, freq));
        }
        ifs_dict_cn.close();

        // 把英文词典索引库加载到内存里
        ifstream ifs_index_en("../data/dictIndex_en.dat");
        if(ifs_index_en.is_open() == false) {
            cerr << "dictIndex_en.dat open failed." << "\n";
            return ;
        }
        string line;
        while(getline(ifs_index_en, line)) {
            istringstream iss(line);
            iss >> word;
            int line_num;
            while(iss >> line_num) {
                en_index[word].insert(line_num);
            }
        }
        ifs_index_en.close();

        // 把中文词典索引库加载到内存里
        ifstream ifs_index_cn("../data/dictIndex_cn.dat");
        if(ifs_index_cn.is_open() == false) {
            cerr << "dictIndex_cn.dat open failed." << "\n";
            return ;
        }
        while(getline(ifs_index_cn, line)) {
            istringstream iss(line);
            iss >> word;
            int line_num;
            while(iss >> line_num) {
                cn_index[word].insert(line_num);
            }
        }
        ifs_index_cn.close();

        // 对客户端发来的关键词切割, 使用utfcpp切割成单个字符 
        string value = _msg.value;
        set<int> en_all_line_nums; // 所有的英文候选词行号
        set<int> cn_all_line_nums; // 所有的中文候选词行号
        const char* it = value.c_str();
        const char* end = value.c_str() + value.size();
        while (it < end) {
            const char* start = it; 
            utf8::next(it, end);    // 将it移动到下一个utf8字符的起始位置 
            std::string alpha {start, it}; // 拿到了单个字符
            if(alpha.size() == 1) { // 字符是英文字符
                if(en_index.find(alpha) != en_index.end()) {
                    for(auto i : en_index[alpha]) {
                        en_all_line_nums.insert(i);
                    }
                }
            } else { // 字符是中文字符
                if(cn_index.find(alpha) != cn_index.end()) {
                    for(auto i : cn_index[alpha]) {
                        cn_all_line_nums.insert(i);
                    }
                }
            }
        }

        // 此时已有中英文候选词, 对候选词进行排序, 选择优先级队列, 使用自定义比较器
        unordered_map<string, int> candidate_words_dict; // 所有候选词的词频
        vector<string> candi_words; // 所有的候选词
        for(auto num : en_all_line_nums) {
            candidate_words_dict.insert(en_dict[num - 1]);
            candi_words.push_back(en_dict[num - 1].first);
        }
        for(auto num : cn_all_line_nums) {
            candidate_words_dict.insert(cn_dict[num - 1]);
            candi_words.push_back(cn_dict[num - 1].first);
        }

        unordered_map<std::string_view, int> edit_distance; // 预存所有候选词的最小编辑距离
        for(auto & ele : candidate_words_dict) {
            edit_distance[ele.first] = editDistance(ele.first, value); // 改用C++17版本的std::string_view, 避免字符串的拷贝
        }
        /* priority_queue<string, vector<string>, CandidateComparator> candidates_sort(CandidateComparator(value, candidate_words_dict)); */

        int K = 5; // 找前5个候选词
        std::nth_element(
                         candi_words.begin(),          // 起始迭代器
                         candi_words.begin() + K - 1,  // 第K小的位置
                         candi_words.end(),            // 结束迭代器
                         CandidateComparator(ref(value), ref(candidate_words_dict), ref(edit_distance)));

        cout << "候选词的数量: " << candidate_words_dict.size() << "\n";
        
        /* int i = 1; */
        /* for(auto & ele : candidate_words_dict) { */
        /*     candidates_sort.push(ele.first); */
        /*     /1* cout << i++ << "\n"; *1/ */
        /* } */

        // 取出优先级队列中前五个候选词, 转成json::Array格式
        json cands = json::array();
        for(int i = 0; i < K; ++i) {
            cands[i] = candi_words[i];
        }

        string cands_to_str = cands.dump();

        Message message;
        message.tag = 1;
        message.length = cands_to_str.size();
        message.value = cands_to_str;

        cout << message.value << "\n";

        _con->sendInLoop(message);

    } else if(_msg.tag == 2) { // 客户端是网页查询请求

        unordered_map<string, map<int, double>> invertedIndex; // 倒排索引
        vector<pair<int, int>> pageOffset; // 网页偏移库, 下标是doc_id - 1

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

        // 把停用词加载到内存中
        std::set<std::string> stopWords;
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

        // 对客户端发送过来的内容分词
        string content = _msg.value;
        // 使用cppjieba分词
        cppjieba::Jieba tokenizer;
        std::vector<std::string> words; // 分割后的所有关键词
        tokenizer.Cut(content, words);
        multiset<string> nostopWors; // 清洗停用词和非法字符后的所有关键词
        std::wregex pattern(LR"(^[\u4E00-\u9FFFa-zA-Z]+$)");
        for(auto & word : words) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::wstring temp = converter.from_bytes(word);
            if(std::regex_match(temp, pattern) && stopWords.find(word) == stopWords.end()) {
                nostopWors.insert(word);
            }
        }

        // 通过倒排索引查找包含所有关键字(去掉停用词)的网页, 只要有查询词不在索引表, 返回的就是空
        set<int> docids;
        for(auto & word : nostopWors) { // 遍历关键词
            if(invertedIndex.find(word) == invertedIndex.end()) { // 查询词不在索引表
                docids.clear();
                break;
            }
            set<int> temp;
            for(auto & e : invertedIndex[word]) {
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

        multimap<double, int, greater<double>> cosine_docid; // 存储余弦和docid
        // 遍历所有的筛选后的网页, 拿到每个网页里所有关键词的权重, 计算余弦相似度
        for(auto docid : docids) {
            vector<double> wvec; // 每篇文章的向量
            for(auto & word : nostopWors) {
                wvec.push_back(invertedIndex[word][docid]);
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
            cosine_docid.insert({cosine, docid});
        }

        json data = json::array();
        int i = 0;
        for(auto & ele : cosine_docid) {
            data[i]["id"] = ele.second;
            string content = Util::readFileChunk("../data/page.dat", pageOffset[ele.second - 1].first, pageOffset[ele.second - 1].second);
            data[i]["title"] = Util::extractTitle(content, "<title>", "</title>");
            data[i]["url"] = Util::extractTitle(content, "<url>", "</url>");
            string desc = Util::extractTitle(content, "<content>", "</content>");
            string abstract = Util::utf8Substr(desc, 50);
            data[i]["abstract"] = abstract;
            ++i;
            if(i >= 5) break;
        }

        string data_to_str = data.dump(2);

        Message message;
        message.tag = 2;
        message.length = data_to_str.size();
        message.value = std::move(data_to_str);

        cout << message.value << "\n";
        /* cout << data.dump(2) << "\n"; */

        _con->sendInLoop(message);



    } else {
        Message message;
        message.tag = 3;
        message.value = "tag is error.";
        message.length = message.value.size();

        cerr << message.value << "\n";

        _con->sendInLoop(message);

    }

}

EchoServer::EchoServer(size_t threadNum, size_t queSize
                       , const string &ip
                       , unsigned short port)
: _pool(threadNum, queSize)
, _server(ip, port)
{
    /* start(); */
}

EchoServer::~EchoServer()
{

}

//服务器的启动与停止
void EchoServer::start()
{
    _pool.start();
    //function<void(const TcpConnectionPtr &)>
    using namespace std::placeholders;
    _server.setAllCallback(std::bind(&EchoServer::onNewConnection, this, _1)
                           , std::bind(&EchoServer::onMessage, this, _1)
                           , std::bind(&EchoServer::onClose, this, _1));
    _server.start();

}
void EchoServer::stop()
{
    _pool.stop();
    _server.stop();
}

//三个回调
void EchoServer::onNewConnection(const TcpConnectionPtr &con)
{
    cout << con->toString() << " has connected!!!" << "\n";
}

/* void EchoServer::onMessage(const TcpConnectionPtr &con) */
/* { */
/*     string msg = con->receive(); */
/*     cout << ">>recv msg from client: " << msg << "\n"; */

/*     MyTask task(msg, con); */
/*     _pool.addTask(std::bind(&MyTask::process, task)); */
/* } */

void EchoServer::onMessage(const TcpConnectionPtr &con)
{
    Message msg = con->receiveTrain();
    cout << "tag : " << msg.tag << "\n";
    cout << "length: " << msg.length << "\n";
    cout << "value : " << msg.value << "\n";

    MyTask task(msg, con);
    _pool.addTask(std::bind(&MyTask::process, task));
}

void EchoServer::onClose(const TcpConnectionPtr &con)
{
    cout << con->toString() << " has close!!!" << "\n";

}
