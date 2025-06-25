#pragma once

#include <iostream>
#include <unordered_map>
#include <list>
#include <cmath>
#include <mutex>

using namespace std;

class MyLRUCache {
private:
    unordered_map<string, list<pair<string, string>>::iterator> key_iterator;
    list<pair<string, string>> m_lst; 
    size_t m_capacity;
    std::mutex catche_mutex; // mutex不能拷贝构造
public:
    MyLRUCache(size_t capacity = 10 * std::pow(1024, 2)) : m_capacity(capacity) {}

    string get(string key) {
        std::lock_guard<std::mutex> lock(catche_mutex);
        if(key_iterator.count(key)) {
            auto list_it = key_iterator[key];
            m_lst.splice(m_lst.end(), m_lst, list_it);
            key_iterator[key] = --m_lst.end();
            return key_iterator[key]->second;
        }
        return "";
    }

    void put(string key, string value) {
        std::lock_guard<std::mutex> lock(catche_mutex);
        if(key_iterator.count(key)) {
            key_iterator[key]->second = value;
            auto list_it = key_iterator[key];
            m_lst.splice(m_lst.end(), m_lst, list_it);
            key_iterator[key] = --m_lst.end();
        } else {
            m_lst.push_back(pair<string, string>(key, value));
            key_iterator[key] = --m_lst.end();
            if(m_lst.size() > m_capacity) {
                string first_key = m_lst.begin()->first;
                m_lst.pop_front();
                auto ki_it = key_iterator.find(first_key);
                key_iterator.erase(ki_it);
            }
        }
    }

    // 新增函数：获取所有键值对
    list<pair<string, string>> get_all_items() {
        std::lock_guard<std::mutex> lock(catche_mutex);
        // 返回副本而不是原始数据
        return list<pair<string, string>>(m_lst.begin(), m_lst.end());
    }

    // 新增函数：复制另一个缓存的数据成员
    void copy_from(const MyLRUCache& other) {
        // 加锁保护当前缓存
        std::lock_guard<std::mutex> lock_this(catche_mutex);

        // 注意：这里不能直接复制另一个缓存的迭代器，因为迭代器指向的是不同的链表
        // 所以先复制链表内容，然后重建迭代器映射

        // 1. 复制链表内容
        m_lst = list<pair<string, string>>(other.m_lst.begin(), other.m_lst.end());

        // 2. 重建迭代器映射
        key_iterator.clear();
        auto it = m_lst.begin();
        while (it != m_lst.end()) {
            key_iterator[it->first] = it;
            ++it;
        }
    }
};
