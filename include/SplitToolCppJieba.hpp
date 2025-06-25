#pragma once

#include "SplitTool.h"
#include "cppjieba/Jieba.hpp"


/*
 * cppjieba分词工具
 */ 
class SplitToolCppJieba
: public SplitTool
{
public:
    virtual vector<string> cut(const string & sentence) override {
        vector<std::string> words;
        tokenizer.Cut(sentence, words);
        return words;
    }
    /* ~SplitToolCppJieba() {} */
private:
    // 结巴对象
    cppjieba::Jieba tokenizer;

};

