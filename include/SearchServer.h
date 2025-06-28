#pragma once

#include <wfrest/HttpServer.h>
#include <workflow/WFFacilities.h>
#include "../src/reactor/CandidateComparator.hpp"
#include "../src/reactor/Util.hpp"
#include "MyLRUCache.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <string>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <queue>
#include <algorithm>
#include <regex>
#include <locale>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>      
#include <unistd.h>
#include <cstdio>
#include <vector>
#include <workflow/MySQLResult.h>
#include <nlohmann/json.hpp>
#include <wfrest/PathUtil.h>
#include "utfcpp/utf8.h"
#include "nlohmann/json.hpp"
#include "cppjieba/Jieba.hpp"
#include "workflow/WFFacilities.h"

using namespace std;
using namespace wfrest;
using namespace protocol;
using json = nlohmann::json;

// 装饰者模式 (套壳)
// CloudiskServer 的用法和 HttpServer 的用法非常类似
// 接口一致，可以降低用户的学习成本
class SearchServer
{
public:
    SearchServer() {
        // 构造函数中同步加载, 服务启动时一次性加载所有资源

        // 加载英文词典资源
        load_en_dict();
        // 加载中文词典资源
        load_cn_dict();
        // 加载英文词典索引资源
        load_en_index();
        // 加载中文词典索引资源
        load_cn_index();
        // 加载倒排索引库资源
        load_invertedIndex();
        // 加载网页偏移库资源
        load_pageOffset();
        // 加载中英文停用词资源
        load_stopWords();
    }

    void start_timer(); // 启动定时器

    void register_modules();

    int start(unsigned short port) { return m_server.start(port); }
    
    void stop() { stop_flag = true; m_server.stop(); }

    void list_routes() { m_server.list_routes(); }

    SearchServer& track()
    {
        m_server.track();
        return *this;
    }

private:
    bool stop_flag = false; // 添加关闭标志, 用来停止定时器

    struct ThreadCache {
        MyLRUCache cache;
        MyLRUCache patch = std::pow(1024, 2);
    };

private:
    void maintenance_task();        // 定时器维护缓存同步任务的函数

    // 注册路由
    void register_static_resources_module();
    void register_keyword_module();
    void register_search_module();

    // 加载英文词典资源
    void load_en_dict();
    // 加载中文词典资源
    void load_cn_dict();
    // 加载英文词典索引资源
    void load_en_index();
    // 加载中文词典索引资源
    void load_cn_index();
    // 加载倒排索引库资源
    void load_invertedIndex();
    // 加载网页偏移库资源
    void load_pageOffset();
    // 加载中英文停用词资源
    void load_stopWords();

private:

    // 名字中最好不要带具体的实现细节
    // 方便以后修改具体的实现
    wfrest::HttpServer m_server;    // 组合: 有选择的暴露接口

    // 资源缓存机制: 将存储文件内容的数据结构作为类的数据成员, 避免每次收到请求都要进行重复的文件 I/O 操作
    vector<pair<string, int>> en_dict; // 英文词典
    vector<pair<string, int>> cn_dict; // 中文词典
    map<string, set<int>> en_index; // 英文词典索引
    map<string, set<int>> cn_index; // 中文词典索引
    unordered_map<string, unordered_map<int, double>> invertedIndex; // 倒排索引
    vector<pair<int, int>> pageOffset; // 网页偏移库, 下标是doc_id - 1
    std::unordered_set<std::string> stopWords; // 把中英文停用词加载到内存中

    cppjieba::Jieba tokenizer; // 另一种优化方案, 和static的区别是没有延迟初始化
    std::mutex tokenizer_mutex;

    // 关键字推荐LRU缓存
    unordered_map<pthread_t, ThreadCache> key_word_thread_caches; // 将线程和缓存、补丁绑定
    MyLRUCache key_word_main_cache; // 操作主缓存不需要加锁, 因为添加和复制是在一个线程里完成的

    // 网页搜索LRU缓存
    unordered_map<pthread_t, ThreadCache> web_search_thread_caches; // 将线程和缓存、补丁绑定
    MyLRUCache web_search_main_cache; // 操作主缓存不需要加锁, 因为添加和复制是在一个线程里完成的
};

