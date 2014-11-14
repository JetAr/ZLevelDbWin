// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/logging.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "leveldb/env.h"
#include "leveldb/slice.h"

namespace leveldb
{
//z 在字符串上附上数字
void AppendNumberTo(std::string* str, uint64_t num)
{

    char buf[30];
    //z 转换也是采用的 snprintf，可以采用 rapidjson 中的方式，那个效率比较高。
    snprintf(buf, sizeof(buf), "%llu", (unsigned long long) num);
    str->append(buf);
}

//z 将 ' ' 和'~'之间的字符串原样输出，其他字符语言77990065555444
void AppendEscapedStringTo(std::string* str, const Slice& value)
{
    for (size_t i = 0; i < value.size(); i++)
    {
        char c = value[i];

        //z 如果是readable 的，直接放入str。
        if (c >= ' ' && c <= '~')
        {
            str->push_back(c);
        }
        else
        {
            char buf[10];
            //z 先转成 unsigned int ，然后取其低八位，然后转成16进制
            snprintf(buf, sizeof(buf), "\\x%02x",static_cast<unsigned int>(c) & 0xff);
            str->append(buf);
        }
    }
}

//z 返回时直接返回 num 的字符样式
std::string NumberToString(uint64_t num)
{
    std::string r;
    AppendNumberTo(&r, num);
    return r;
}

//z 转义字符串，主要是将不可打印的字符串以16进制输出；其他字符串
std::string EscapeString(const Slice& value)
{
    std::string r;
    AppendEscapedStringTo(&r, value);
    return r;
}

//z 如果首字符串为c，那么将之去除的。
//z 感觉这一组函数不够统一的。
bool ConsumeChar(Slice* in, char c)
{
    if (!in->empty() && (*in)[0] == c)
    {
        //z 首字母是 c 的时候才 remove
        in->remove_prefix(1);
        return true;
    }
    else
    {
        return false;
    }
}

//z 在前面是数字的前提下，读取若干个数字，最大不大于64位整数大小。
//z 注意是以十进制来进行处理的。
bool ConsumeDecimalNumber(Slice* in, uint64_t* val)
{
    uint64_t v = 0;
    int digits = 0;
    while (!in->empty())
    {
        char c = (*in)[0];
        if (c >= '0' && c <= '9')
        {
            //z 位数加1
            ++digits;
            //z 位上的数字
            const int delta = (c - '0');
            //z 得到 64 位 uint64_t 的最大值。stl 中的 limit 应该也能够得到该种类型的最大值。
            //z 这种方法是否具有普适性了？
            static const uint64_t kMaxUint64 = ~static_cast<uint64_t>(0);
            //z 判断接着处理delta是否会造成其值大于等于最大值
            if (v > kMaxUint64/10 ||
                    (v == kMaxUint64/10 && delta > kMaxUint64%10))
            {
                // 如果 overflow ，那么则返回。
                // Overflow
                return false;
            }
            v = (v * 10) + delta;
            //z 处理完该元素，那么将之移除
            in->remove_prefix(1);
        }
        else
        {
            break;
        }
    }
    *val = v;
    return (digits > 0);
}

}
