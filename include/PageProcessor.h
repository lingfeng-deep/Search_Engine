#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include "cppjieba/Jieba.hpp"
#include "simhash/Simhasher.hpp"

/*
 * 网页信息处理类
 */
class PageProcessor
{
public:
    PageProcessor() {}
    // dir = "../corpus/webpages/"
    void process(const std::string & dir); // 处理dir下的所有.xml网页文件

private:
    struct Document {
        int id;
        std::string link;
        std::string title;
        std::string content;
    };

    // 先判断.xml中的每一个网页内容是否与之前的重复, 如果不重复, 提取成Document, 存储在vector中
    void extract_documents(const std::string & dir);
    bool isduplicate_documents(const std::string content); // 计算网页内容的simhash, 判断是否重复
    void build_pages_and_offsets(const std::string & pages, const std::string & offsets); // 创建网页库和网页偏移库
    void build_inverted_index(const std::string & filename); // 创建倒排索引库
    std::string parse_cdata_html(const std::string & html); // 解析出.xml标签中的内容
    void obtainStopwords(const std::string & en_filepath, const std::string & cn_filepath); // 获取中英文停用词 

    cppjieba::Jieba _tokenizer;
    simhash::Simhasher _hasher;
    std::set<std::string> _stopWords;
    std::vector<Document> _documents; // 去重后的Document集合
    // 倒排索引, 使用map而不是unordered_map, 是为了方便查看清洗数据的效果
    std::map<std::string, std::map<int, double>> _invertedIndex;
    std::unordered_set<uint64_t> _simhash_set; // 使用哈希集合存储simhash值
};

