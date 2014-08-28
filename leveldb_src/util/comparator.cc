// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <algorithm>
#include <stdint.h>
#include "leveldb/comparator.h"
#include "leveldb/slice.h"
#include "util/logging.h"

namespace leveldb
{

Comparator::~Comparator() { }

namespace
{
class BytewiseComparatorImpl : public Comparator
{
public:
    BytewiseComparatorImpl() { }

    //z 名称
    virtual const char* Name() const
    {
        return "leveldb.BytewiseComparator";
    }

    //z 比较 Slice 字符串
    virtual int Compare(const Slice& a, const Slice& b) const
    {
        return a.compare(b);
    }

    virtual void FindShortestSeparator(
        std::string* start,
        const Slice& limit) const
    {
        // Find length of common prefix
        size_t min_length = std::min(start->size(), limit.size());
        size_t diff_index = 0;
        //z 相同的部分
        while ((diff_index < min_length) &&
                ((*start)[diff_index] == limit[diff_index]))
        {
            diff_index++;
        }

        if (diff_index >= min_length)
        {
            // Do not shorten if one string is a prefix of the other
        }
        else
        {
            uint8_t diff_byte = static_cast<uint8_t>((*start)[diff_index]);
            if (diff_byte < static_cast<uint8_t>(0xff) &&
                    diff_byte + 1 < static_cast<uint8_t>(limit[diff_index]))
            {
                //z 这里为何要将最后一个字符串++了？
                (*start)[diff_index]++;
                //z 截断字符串
                start->resize(diff_index + 1);
                assert(Compare(*start, limit) < 0);
            }
        }
    }

    virtual void FindShortSuccessor(std::string* key) const
    {
        // Find first character that can be incremented
        size_t n = key->size();
        for (size_t i = 0; i < n; i++)
        {
            const uint8_t byte = (*key)[i];
            //z 找出第一个不为 0xff 的字节，然后返回
            if (byte != static_cast<uint8_t>(0xff))
            {
                (*key)[i] = byte + 1;
                key->resize(i+1);
                return;
            }
        }
        // *key is a run of 0xffs.  Leave it alone.
    }
};
}
static const BytewiseComparatorImpl bytewise;

const Comparator* BytewiseComparator()
{
    return &bytewise;
}

}
