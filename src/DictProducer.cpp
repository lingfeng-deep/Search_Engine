#include "../include/DictProducer.h"
#include <dirent.h>     // dirent是directory entry的简写，就是目录项的意思
#include <sys/types.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <locale>
#include <codecvt>
#include "utfcpp/utf8.h"

using std::ifstream;
using std::ofstream;
using std::istringstream;

/*
 * 处理英文语料
 * dir = ../corpus/EN/
 */
DictProducer::DictProducer(const string & dir) {
    createFiles(dir); // 创建语料库文件的相对路径集合
    obtainStopwords("../corpus/stopwords/en_stopwords.txt"); // 获取停用词 
    buildEnDict(); // 创建英文词典
    storeDict("../data/dict_en.dat"); // 将词典写入文件
    createIndex(); // 创建词典索引
    storeIndex("../data/dictIndex_en.dat"); // 将词典索引写入文件
}

/*
 * 处理中文语料
 * dir = ../corpus/CN/
 */
DictProducer::DictProducer(const string & dir, SplitTool *splitTool) 
: _splitTool(splitTool)
{
    createFiles(dir); // 创建语料库文件的相对路径集合
    obtainStopwords("../corpus/stopwords/cn_stopwords.txt"); // 获取停用词 
    buildCnDict(); // 创建中文词典
    storeDict("../data/dict_cn.dat"); // 将词典写入文件
    createIndex(); // 创建词典索引
    storeIndex("../data/dictIndex_cn.dat"); // 将词典索引写入文件
}

/*
 * 创建语料库文件的相对路径集合
 */
void DictProducer::createFiles(const string & dir) {
    DIR * dirp = opendir(dir.c_str());
    if(dirp == NULL) {
        perror("opendir");
        return ;
    }

    struct dirent * pdirent;
    // 循环读目录项获取文件名，循环结束的条件是返回值为NULL   
    while((pdirent = readdir(dirp)) != NULL) {
        string filename = pdirent->d_name;
        string file_rel_path = dir + filename;
        _files.push_back(file_rel_path);
    }

    closedir(dirp);
}

/*
 * 创建英文词典
 */
void DictProducer::buildEnDict() {
    map<string, int> dict;
    // 读语料库的每一个文件
    for(auto & file : _files) {
        ifstream ifs(file);
        if(!ifs.is_open()) {
            std::cerr << "en_corpus file open failed." << std::endl;
            return ;
        }
        // 把文件内容一次性读到字符串中
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        string content = buffer.str();

        ifs.close();

        // 清洗文件内容
        // 把'\r'换成' '
        std::replace(content.begin(), content.end(), '\r', ' ');
        // 把大写字母转成小写, 标点和数字转成空格
        for(char & c : content) {
            if(std::isupper(c)) c = std::towlower(c);
            if(std::ispunct(c)) c = ' ';
            if(std::isdigit(c)) c = ' ';
        }

        // 对文件内容进行切割, 把单词和词频先存在map里
        istringstream iss(content);       
        string word;
        while(iss >> word) {
            // 如果单词不是停用词, 再加入词典
            if(_stopwords.count(word) == 0) ++dict[word];
        }
    }
    // 把map的数据存到_dict
    for(auto & p : dict) {
        _dict.push_back(p);
    }
}

/*
 * 创建中文词典
 */
void DictProducer::buildCnDict() {
    map<string, int> dict;
    // 读语料库的每一个文件
    for(auto & file : _files) {
        ifstream ifs(file);
        if(!ifs.is_open()) {
            std::cerr << "cn_corpus file open failed." << std::endl;
            return ;
        }
        // 把文件内容一次性读到字符串中
        std::stringstream buffer;
        buffer << ifs.rdbuf();
        string content = buffer.str();

        ifs.close();

        // 清洗文件内容
        // 把'\r', '\n'去掉
        // 空白字符, 字母, 数字, 标点交给下面切割内容之后去做
        content.erase(std::remove_if(content.begin(), content.end(),
                                        [](char c) {
                                        /* return std::isspace(c) || std::isalnum(c) || std::ispunct(c) || c == '\r' || c == '\n'; */
                                        return c == '\r' || c == '\n';
                                    }), content.end());

        // 使用分词工具对内容进行切割
        vector<string> words = _splitTool->cut(content);

        // 把单词和词频先存在map里
        // 正则匹配纯汉字, 根据汉字Unicode范围, \u4E00-\u9FFF: 匹配基本汉字(CJK Unified Ideographs)
        std::wregex pattern(LR"(^[\u4E00-\u9FFF]+$)");
        for(auto & word : words) {
            std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
            std::wstring temp = converter.from_bytes(word);
            // 如果词组是纯汉字并且不是停用词, 再加入词典
            if(std::regex_match(temp, pattern) && _stopwords.count(word) == 0) ++dict[word];
        }
    }
    // 把map的数据存到_dict
    for(auto & p : dict) {
        _dict.push_back(p);
    }
}

/*
 * 将词典写入文件
 */
void DictProducer::storeDict(const string & filepath) {
    ofstream ofs(filepath);
    if(!ofs.is_open()) {
        std::cerr << filepath << " --> open failed." << std::endl;
        return ;
    }
    for(auto & p : _dict) {
        ofs << p.first << " " << p.second << std::endl;
    }
    ofs.close();
}

/*
 * 创建词典索引
 */
void DictProducer::createIndex() {
    // 遍历词典, 把词分解成单个字母/汉字
    for(int i = 0; i < (int)_dict.size(); ++i) {
        string word = _dict[i].first;

        // 拆分词
        // 迭代器是指针的泛化，指针就是迭代器
        const char* it = word.c_str();
        const char* end = word.c_str() + word.size();

        while (it < end) {
            const char* start = it; 
            utf8::next(it, end);    // 将it移动到下一个utf8字符的起始位置 
            // 拿到了单个字母/汉字
            std::string alpha {start, it};  // 在C++中，我们可以用一个std::string 表示一个汉字
            // 把单个字母/汉字和当前词典行号存入词典索引
            _index[alpha].insert(i + 1);
        }
    }
}

/*
 * 将词典索引写入文件
 */
void DictProducer::storeIndex(const string & filepath) {
    ofstream ofs(filepath);
    if(!ofs.is_open()) {
        std::cerr << filepath << " --> open failed." << std::endl;
        return ;
    }
    for(auto & p : _index) {
        ofs << p.first << " ";
        for(auto index : p.second) {
            ofs << index << " ";
        }
        ofs << std::endl;
    }
    ofs.close();
}

/*
 * 获取停用词
 */
void DictProducer::obtainStopwords(const string & filepath) {
    ifstream ifs(filepath);
    if(!ifs.is_open()) {
        std::cerr << filepath << " --> stopword file open failed." << std::endl;
    }
    string stopword;
    while(ifs >> stopword) { // 输入流符号会自动处理'\r'
        _stopwords.insert(stopword);
    }
    ifs.close();
}
