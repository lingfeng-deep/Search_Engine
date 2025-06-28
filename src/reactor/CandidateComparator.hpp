#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <numeric>
#include <string_view>
#include "utfcpp/utf8.h" // utfcpp 库头文件#include <unordered_map>

using std::string;
using std::unordered_map;
using std::vector;

/*
 * 自定义候选词比较器
 */
class CandidateComparator
{
public:
    CandidateComparator(string & keyWord, unordered_map<string, int> & candidate_words_dict, unordered_map<std::string_view, int> & edit_distance) 
    : _keyWord(keyWord)
    , _candidate_words_dict(candidate_words_dict)
    , _edit_distance(edit_distance)
    {
        /* PreStoreEditDistance(); */
    }

    bool operator()(const string & lhs, const string & rhs) {
        // 比较左右操作数和关键词的最小编辑距离
        int lhs_editDistance = _edit_distance[lhs];
        int rhs_editDistance = _edit_distance[rhs];

        if(lhs_editDistance == rhs_editDistance) {
            // 比较左右操作数的词频
            int lhs_freq = _candidate_words_dict[lhs];
            int rhs_freq = _candidate_words_dict[rhs];
            
            if(lhs_freq == rhs_freq) {
                // 比较字母表顺序
                return lhs < rhs;
            }

            return lhs_freq > rhs_freq;
        }

        return lhs_editDistance < rhs_editDistance;
    }
    ~CandidateComparator() {}

private:
    // 使用 utfcpp 计算 UTF-8 字符串的字符长度
    size_t utf8_length(const string& str) {
        return utf8::distance(str.begin(), str.end());
    }

    // 使用 utfcpp 获取 UTF-8 字符串中的第 n 个字符
    string utf8_substr(const string& str, size_t start, size_t length = 1) {
        auto it = str.begin();
        utf8::advance(it, start, str.end());

        if (length == 1) {
            auto next = it;
            utf8::next(next, str.end());
            return string(it, next);
        } else {
            auto end_it = it;
            utf8::advance(end_it, length, str.end());
            return string(it, end_it);
        }
    }

    // 中英文通用的最小编辑距离算法
    /* int editDistance(const string& lhs, const string& rhs) { */
    /*     size_t lhs_len = utf8_length(lhs); */
    /*     size_t rhs_len = utf8_length(rhs); */

    /*     // 使用 vector 创建动态大小的二维数组 */
    /*     vector<vector<int>> editDist(lhs_len + 1, vector<int>(rhs_len + 1)); */

    /*     // 初始化边界条件 */
    /*     for (size_t i = 0; i <= lhs_len; ++i) { */
    /*         editDist[i][0] = i; */
    /*     } */
    /*     for (size_t j = 0; j <= rhs_len; ++j) { */
    /*         editDist[0][j] = j; */
    /*     } */

    /*     // 计算编辑距离 */
    /*     for (size_t i = 1; i <= lhs_len; ++i) { */
    /*         string lhs_char = utf8_substr(lhs, i-1); */
    /*         for (size_t j = 1; j <= rhs_len; ++j) { */
    /*             string rhs_char = utf8_substr(rhs, j-1); */

    /*             if (lhs_char == rhs_char) { */
    /*                 editDist[i][j] = editDist[i-1][j-1]; */
    /*             } else { */
    /*                 editDist[i][j] = std::min({ */
    /*                                           editDist[i][j-1] + 1,    // 插入 */
    /*                                           editDist[i-1][j] + 1,    // 删除 */
    /*                                           editDist[i-1][j-1] + 1   // 替换 */
    /*                                           }); */
    /*             } */
    /*         } */
    /*     } */

    /*     return editDist[lhs_len][rhs_len]; */
    /* } */

    // 在 CandidateComparator 类中替换 editDistance 方法
    /* int editDistance(const string& s1, const string& s2) { */
    /*     const size_t m = utf8_length(s1); */
    /*     const size_t n = utf8_length(s2); */

    /*     // 仅使用两行空间 (O(n)空间复杂度) */
    /*     vector<int> prev(n + 1, 0); */
    /*     vector<int> curr(n + 1, 0); */

    /*     // 初始化第一行 */
    /*     for (size_t j = 0; j <= n; ++j) { */
    /*         prev[j] = j; */
    /*     } */

    /*     for (size_t i = 1; i <= m; ++i) { */
    /*         curr[0] = i; */
    /*         string char1 = utf8_substr(s1, i - 1); */

    /*         for (size_t j = 1; j <= n; ++j) { */
    /*             string char2 = utf8_substr(s2, j - 1); */

    /*             if (char1 == char2) { */
    /*                 curr[j] = prev[j - 1]; */
    /*             } else { */
    /*                 curr[j] = 1 + std::min({prev[j],     // 删除 */
    /*                                        curr[j - 1], // 插入 */
    /*                                        prev[j - 1]}); // 替换 */
    /*             } */
    /*         } */

    /*         // 交换当前行和前一行 */
    /*         std::swap(prev, curr); */
    /*     } */

    /*     return prev[n]; */
    /* } */

    // 二次优化后的最小编辑距离算法
    /* int editDistance(const string& s1, const string& s2) { */
    /*     // 添加长度过滤 */
    /*     if (std::abs(int(s1.size()) - int(s2.size())) > 32) */
    /*         return std::max(s1.size(), s2.size()); */

    /*     // 添加UTF-8字符缓存 */
    /*     static thread_local vector<uint32_t> codes1, codes2; */
    /*     codes1.clear(); */
    /*     codes2.clear(); */
    /*     utf8::utf8to32(s1.begin(), s1.end(), back_inserter(codes1)); */
    /*     utf8::utf8to32(s2.begin(), s2.end(), back_inserter(codes2)); */

    /*     // 使用滚动数组计算 */
    /*     const size_t m = codes1.size(); */
    /*     const size_t n = codes2.size(); */

    /*     vector<int> dp(n + 1); */
    /*     std::iota(dp.begin(), dp.end(), 0); */

    /*     for (size_t i = 1; i <= m; ++i) { */
    /*         int prev = dp[0]; */
    /*         dp[0] = i; */
    /*         for (size_t j = 1; j <= n; ++j) { */
    /*             int temp = dp[j]; */
    /*             if (codes1[i-1] == codes2[j-1]) { */
    /*                 dp[j] = prev; */
    /*             } else { */
    /*                 dp[j] = 1 + std::min({prev, dp[j], dp[j-1]}); */
    /*             } */
    /*             prev = temp; */
    /*         } */
    /*     } */
    /*     return dp[n]; */
    /* } */


    // 把比较器数据成员改成引用, 否则的话排序时会多次创建比较器对象, 容器内容会多次拷贝(形参给数据成员赋值时)
    string &_keyWord;
    unordered_map<string, int> &_candidate_words_dict; // 所有候选词的词频
    unordered_map<std::string_view, int> &_edit_distance;
};

