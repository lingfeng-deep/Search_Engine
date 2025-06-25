#pragma once

#include "SplitToolCppJieba.hpp"
#include <vector>
#include <map>
#include <set>
#include <string>

using std::vector;
using std::string;
using std::map;
using std::set;
using std::pair;

/*
 * 词典创造类
 */
class DictProducer
{
public:
    /*
     * dir = ../corpus/EN/
     */
    DictProducer(const string & dir); // 处理英文语料
    /*
     * dir = ../corpus/CN/
     */
    DictProducer(const string & dir, SplitTool *splitTool); // 处理中文语料
    ~DictProducer() {}
private:
    void createFiles(const string & dir); // 创建语料库文件的相对路径集合
    void buildEnDict(); // 创建英文词典
    void buildCnDict(); // 创建中文词典
    void storeDict(const string & filepath); // 将词典写入文件
    void createIndex(); // 创建词典索引
    void storeIndex(const string & filepath); // 将词典索引写入文件
    void obtainStopwords(const string & filepath); // 获取停用词

    vector<string> _files; // 语料库文件的相对路径集合
    vector<pair<string, int>> _dict; // 词典
    map<string, set<int>> _index; // 词典索引
    set<string> _stopwords; // 停用词
    SplitTool *_splitTool; // 分词工具, 用于把汉语句子分割成词组
};

