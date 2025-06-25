#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

class Util
{
public:
    static std::string readFileChunk(const std::string& filename, size_t offset, size_t length) {
        std::ifstream file(filename); 
        if (file.is_open() == false) {
            std::cerr << filename << " open failed." << "\n";
        }

        file.seekg(offset, std::ios::beg); // 移动到偏移量位置
        std::vector<char> buffer(length);  // 分配足够的内存

        file.read(buffer.data(), length); // 读取指定长度
        std::string buf(buffer.begin(), buffer.end());

        return buf;
    }

    /* static std::string extractTitle(const std::string& content, const std::string &startTag, const std::string &endTag) { */
    /*     size_t startPos = content.find(startTag); */
    /*     if (startPos == std::string::npos) { */
    /*         return ""; // 未找到 <title> */
    /*     } */

    /*     startPos += startTag.length(); // 跳过 <title> */
    /*     size_t endPos = content.find(endTag, startPos); */
    /*     if (endPos == std::string::npos) { */
    /*         return ""; // 未找到 </title> */
    /*     } */

    /*     return content.substr(startPos, endPos - startPos); */
    /* } */

    // 改进的 extractTitle：避免截断 UTF-8
    static std::string extractTitle(const std::string& content, const std::string& startTag, const std::string& endTag) {
        size_t start = content.find(startTag);
        if (start == std::string::npos) return "";
        start += startTag.length();

        size_t end = content.find(endTag, start);
        if (end == std::string::npos) return "";

        std::string title = content.substr(start, end - start);
        return fixUtf8(title);  // 修复可能的 UTF-8 错误
    }

    /* // 获取前 n 个 UTF-8 字符 */
    /* static std::string utf8Substr(const std::string& str, size_t n) { */
    /*     std::string result; */
    /*     size_t count = 0; */
    /*     for (size_t i = 0; i < str.size() && count < n; ) { */
    /*         if (isUtf8LeadByte(str[i])) { */
    /*             count++; // 新字符开始 */
    /*         } */
    /*         result += str[i]; */
    /*         i++; */
    /*     } */
    /*     return result; */
    /* } */

    // 改进的 utf8Substr：安全截断
    static std::string utf8Substr(const std::string& str, size_t maxChars) {
        std::string result;
        size_t chars = 0;
        for (size_t i = 0; i < str.size() && chars < maxChars; ) {
            unsigned char c = str[i];
            size_t n;
            if (c <= 0x7F) n = 1;
            else if ((c & 0xE0) == 0xC0) n = 2;
            else if ((c & 0xF0) == 0xE0) n = 3;
            else if ((c & 0xF8) == 0xF0) n = 4;
            else { n = 1; }  // 跳过非法字节

            if (i + n > str.size()) break;  // 不完整字符，丢弃
            result.append(str.substr(i, n));
            i += n;
            chars++;
        }
        return result;
    }

private:
    // 检查字节是否是 UTF-8 字符的起始字节
    static bool isUtf8LeadByte(char c) {
        return (c & 0xC0) != 0x80; // 非 10xxxxxx
    }

    // 修复无效 UTF-8（移除非法字符）
    static std::string fixUtf8(const std::string& str) {
        std::string fixed;
        int n;
        for (size_t i = 0; i < str.size(); i += n) {
            unsigned char c = str[i];
            if (c <= 0x7F) n = 1;
            else if ((c & 0xE0) == 0xC0) n = 2;
            else if ((c & 0xF0) == 0xE0) n = 3;
            else if ((c & 0xF8) == 0xF0) n = 4;
            else { n = 1; continue; }  // 跳过非法字节

            if (i + n > str.size()) break;  // 不完整字符，直接丢弃

            bool valid = true;
            for (int j = 1; j < n; j++) {
                if ((str[i + j] & 0xC0) != 0x80) {
                    valid = false;
                    break;
                }
            }
            if (valid) fixed.append(str.substr(i, n));
        }
        return fixed;
    }

    Util() {}

};

