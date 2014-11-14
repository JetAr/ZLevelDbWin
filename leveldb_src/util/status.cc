// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <stdio.h>
#include "port/port.h"
#include "leveldb/status.h"

namespace leveldb
{

// OK status has a NULL state_.  Otherwise, state_ is a new[] array
// of the following form:
//    state_[0..3] == length of message
//    state_[4]    == code
//    state_[5..]  == message
const char* Status::CopyState(const char* state)
{
    uint32_t size;
    //z 得到 message 长度
    memcpy(&size, state, sizeof(size));
    char* result = new char[size + 5];//z 5从何而来？见前面所述，除了message的长度再加上前面message header的长度
    memcpy(result, state, size + 5);
    return result;
}

Status::Status(Code code, const Slice& msg, const Slice& msg2)
{
    assert(code != kOk);
    const uint32_t len1 = msg.size();
    const uint32_t len2 = msg2.size();
    //z 加上2，是因为在两条message之间添加了": "
    const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
    char* result = new char[size + 5];
    //z 保存 message 长度
    memcpy(result, &size, sizeof(size));
    //z message code
    result[4] = static_cast<char>(code);
    memcpy(result + 5, msg.data(), len1);
    if (len2)
    {
        result[5 + len1] = ':';
        result[6 + len1] = ' ';
        memcpy(result + 7 + len1, msg2.data(), len2);
    }
    state_ = result;
}

std::string Status::ToString() const
{
    if (state_ == NULL)
    {
        return "OK";
    }
    else
    {
        char tmp[30];
        const char* type;
        switch (code())
        {
        case kOk:
            type = "OK";
            break;
        case kNotFound:
            type = "NotFound: ";
            break;
        case kCorruption:
            type = "Corruption: ";
            break;
        case kNotSupported:
            type = "Not implemented: ";
            break;
        case kInvalidArgument:
            type = "Invalid argument: ";
            break;
        case kIOError:
            type = "IO error: ";
            break;
        default:
            snprintf(tmp, sizeof(tmp), "Unknown code(%d): ",
                     static_cast<int>(code()));
            type = tmp;
            break;
        }
        std::string result(type);
        uint32_t length;
        memcpy(&length, state_, sizeof(length));
        result.append(state_ + 5, length);
        return result;
    }
}

}
