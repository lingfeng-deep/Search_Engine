#include "../include/PageProcessor.h"
#include <dirent.h>     // dirent是directory entry的简写，就是目录项的意思
#include <sys/types.h>
#include <dirent.h>
#include <math.h>
#include <regex>
#include <locale>
#include <codecvt>
#include <fstream>
#include <unordered_map>
#include "tinyxml2.h"

using namespace tinyxml2;

/*
 * 处理dir下的所有.xml网页文件
 * dir = "../corpus/webpages/"
 */
void PageProcessor::process(const std::string & dir) {
    extract_documents(dir); // 提取Document, 把判重后的Document存入vector
    build_pages_and_offsets("../data/page.dat", "../data/page_offset.dat"); // 创建网页库和网页偏移库
    obtainStopwords("../corpus/stopwords/en_stopwords.txt", "../corpus/stopwords/cn_stopwords.txt"); // 获取中英文停用词
    build_inverted_index("../data/invertIndex.dat"); // 生成倒排索引库
}

/*
 * 先判断.xml中的每一个网页内容是否与之前的重复, 如果不重复, 提取成Document, 存储在vector中
 * dir = "../corpus/webpages/"
 */
void PageProcessor::extract_documents(const std::string & dir) {
    int document_id = 1;
    DIR * dirp = opendir(dir.c_str());
    if(dirp == NULL) {
        perror("opendir");
        return ;
    }
    
    struct dirent * pdirent;
    XMLDocument doc;
    // 循环读目录项获取文件名，循环结束的条件是返回值为NULL   
    while((pdirent = readdir(dirp)) != NULL) {
        std::string filename = pdirent->d_name;

        // 跳过特殊目录项
        if(filename == "." || filename == "..") {
            continue;
        }

        std::string file_rel_path = dir + filename; // 网页文件的相对路径名
        
        // 使用tinyxml2解析原始.xml文件
        doc.LoadFile(file_rel_path.c_str());
        XMLElement *root = doc.RootElement();
        if(!root) {
            std::cerr << ".xml网页文件格式有误." << std::endl;
            continue;
        }
        XMLElement *channelNode = root->FirstChildElement();
        XMLElement *itemNode = channelNode->FirstChildElement("item");
        while(itemNode) {
            Document document;

            // 处理title
            XMLElement *titleNode = itemNode->FirstChildElement("title");
            std::string title = titleNode->GetText();
            document.title = parse_cdata_html(title);
            
            // 处理link
            XMLElement *linkNode = itemNode->FirstChildElement("link");
            document.link = linkNode->GetText();

            // 处理description
            XMLElement *descNode = itemNode->FirstChildElement("description");
            if(descNode) {
                std::string desc = descNode->GetText();
                document.content = parse_cdata_html(desc);
            }

            // 处理content
            XMLElement *contNode = itemNode->FirstChildElement("content");
            if(contNode) {
                std::string cont = contNode->GetText();
                document.content = parse_cdata_html(cont);
            }
            
            // 网页内容没有重复
            if(isduplicate_documents(document.content) == false) {
                document.id = document_id++; // Document构建完成
                _documents.push_back(document);
            }

            // 继续找下一个item标签
            itemNode = itemNode->NextSiblingElement("item");
        }
    }
    closedir(dirp);
}

/*
 * 计算网页内容的simhash, 判断是否重复
 */
bool PageProcessor::isduplicate_documents(const std::string content) {
    uint64_t u1; // 传入内容的哈希
    _hasher.make(content, 5, u1); 

    for(auto & hash : _simhash_set) {
        if(simhash::Simhasher::isEqual(u1, hash)) return true;
    }
    _simhash_set.insert(u1);
    return false;
}

/*
 * 根据_documents创建网页库, 创建网页库的同时也创建网页偏移库
 * pages = "../data/page.dat"
 * offsets = "../data/page_offset.dat"
 */
void PageProcessor::build_pages_and_offsets(const std::string & pages, const std::string & offsets) {
    std::ofstream ofs_page(pages);
    if(!ofs_page.is_open()) {
        std::cerr << pages << " open failed." << std::endl;
        return ;
    }
    
    std::ofstream ofs_off(offsets);
    if(!ofs_off.is_open()) {
        std::cerr << offsets << " open failed." << std::endl;
        return ;
    }

    // 根据_documents创建网页库
    for(auto & document : _documents) {
        // 把document转成xml格式的字符串
        std::string doc_to_str = "<doc>\n"
                                 "    <id>" + std::to_string(document.id) + "</docid>\n"
                                 "    <url>" + document.link + "</url>\n"
                                 "    <title>" + document.title + "</title>\n"
                                 "    <content>" + document.content + "</content>\n"
                                 "</doc>";
        // 获取当前网页库文件偏移位置
        std::streampos startPos = ofs_page.tellp();

        // 写入文档
        ofs_page << doc_to_str << std::endl;

        // 获取写入后的网页库文件偏移位置
        std::streampos endPos = ofs_page.tellp();

        // 计算文档大小(字节数)
        std::streamoff docSize = endPos - startPos;

        // 把当前存入网页库的doc_id, doc的起始位置, doc所占字节大小这三个信息写入网页偏移库
        ofs_off << document.id << " " << startPos << " " << docSize << std::endl;
    }

    ofs_page.close();
    ofs_off.close();
}

/*
 * 创建倒排索引库
 * filename = "../data/invertIndex.dat"
 */
void PageProcessor::build_inverted_index(const std::string & filename) {
    // 外层键：单词(string)
    // 内层键：doc_id
    // 内层值：该词在当前文件中的出现次数(int)
    std::unordered_map<std::string, std::unordered_map<int, int>> wordFileCounts;
    std::unordered_map<std::string, int> wordDocumentFrequency; // 存放包含关键字的文档数目
    std::unordered_map<int, double> docWeightSum; // 存放每个文档的权重和, 第一个参数是doc_id
    // 遍历Document集合
    for(auto & document : _documents) {
        std::string content = document.content;
        int doc_id = document.id;

        // 清洗文件内容
        // 把'\r', '\n'去掉
        // 空白字符, 字母, 数字, 标点交给下面切割内容之后去做
        content.erase(std::remove_if(content.begin(), content.end(),
                                        [](char c) {
                                        /* return std::isspace(c) || std::isalnum(c) || std::ispunct(c) || c == '\r' || c == '\n'; */
                                        return c == '\r' || c == '\n';
                                    }), content.end());

        // 使用分词工具对内容进行切割
        std::vector<std::string> words;
        _tokenizer.Cut(content, words);;

        // 匹配包含汉字和英文的字符串（允许混合）
        std::wregex pattern(LR"(^[\u4E00-\u9FFFa-zA-Z]+$)");
        for(auto & word : words) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::wstring temp = converter.from_bytes(word);
            // 如果词组是汉字和英文的字符串并且不是停用词, 处理它
            if(std::regex_match(temp, pattern) && _stopWords.count(word) == 0) {
                ++wordFileCounts[word][doc_id];
            }
        }
    }

    // 给wordDocumentFrequency赋值
    for(auto & ele : wordFileCounts) {
        int fre = ele.second.size();
        wordDocumentFrequency[ele.first] = fre;
    } 

    // 计算每个关键词的权重
    for(auto & ele : wordDocumentFrequency) {
        int DF = ele.second;
        for(auto & p : wordFileCounts[ele.first]) {
            int doc_id = p.first;
            int TF = p.second;
            int N = _documents.size();
            double IDF = log2((double)N / (DF + 1) + 1);
            double w = TF * IDF;
            docWeightSum[doc_id] += (w * w);
            _invertedIndex[ele.first][doc_id] = w;
        }
    }

    // 归一化
    for(auto & ele : _invertedIndex) {
        for(auto & p : ele.second) {
            int doc_id = p.first;
            p.second = p.second / sqrt(docWeightSum[doc_id]);
        }
    }

    // 写入倒排索引文件
    std::ofstream ofs(filename);
    if(!ofs.is_open()) {
        std::cerr << filename << " open failed." << std::endl;
        return ;
    }

    for(auto & ele : _invertedIndex) {
        ofs << ele.first << " ";
        for(auto & p : ele.second) {
            ofs << p.first << " " << p.second << " ";
        }
        ofs << std::endl;
    }

    ofs.close();
}

/*
 * 解析出.xml标签中的内容
 */
std::string PageProcessor::parse_cdata_html(const std::string & html) {
    // 1. 提取CDATA内容
    static const std::regex cdata_pattern(R"(<!\[CDATA\[([\s\S]*?)\]\]>)");
    std::string cdata = regex_replace(html, cdata_pattern, "$1");

    // 2. 去除HTML标签
    static const std::regex tag_pattern(R"(</?[^>]+>)");
    return regex_replace(cdata, tag_pattern, "");
}

/*
 * 获取中英文停用词
 * en_filepath = "../corpus/stopwords/en_stopwords.txt"
 * cn_filepath = "../corpus/stopwords/cn_stopwords.txt"
 */
void PageProcessor::obtainStopwords(const std::string & en_filepath, const std::string & cn_filepath) {
    std::ifstream ifs_en(en_filepath);
    if(!ifs_en.is_open()) {
        std::cerr << en_filepath << " --> stopword file open failed." << std::endl;
    }
    std::string stopword;
    while(ifs_en >> stopword) { // 输入流符号会自动处理'\r'
        _stopWords.insert(stopword);
    }
    ifs_en.close();

    std::ifstream ifs_cn(cn_filepath);
    if(!ifs_cn.is_open()) {
        std::cerr << cn_filepath << " --> stopword file open failed." << std::endl;
    }

    while(ifs_cn >> stopword) { // 输入流符号会自动处理'\r'
        _stopWords.insert(stopword);
    }
    ifs_cn.close();
}
