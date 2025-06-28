#pragma once

#include <string>
#include <vector>

using std::vector;
using std::string;

/*
 * 分词工具类 --> 接口类
 * 这是一个虚基类, 由具体的分词工具类继承它
 */
class SplitTool
{
public:
    virtual ~SplitTool() = 0;
    virtual vector<string> cut(const string & sentence) = 0;
};

// 提供纯虚析构函数的实现
inline SplitTool::~SplitTool() {}
