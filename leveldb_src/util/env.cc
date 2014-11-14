// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/env.h"

namespace leveldb
{

Env::~Env()
{
}

SequentialFile::~SequentialFile()
{
}

RandomAccessFile::~RandomAccessFile()
{
}

WritableFile::~WritableFile()
{
}

Logger::~Logger()
{
}

FileLock::~FileLock()
{
}

void Log(Logger* info_log, const char* format, ...)
{
    if (info_log != NULL)
    {
        va_list ap;
        va_start(ap, format);
        info_log->Logv(format, ap);
        va_end(ap);
    }
}

Status WriteStringToFile(Env* env, const Slice& data,
                         const std::string& fname)
{
    //z 文件指针
    WritableFile* file;
    //z 根据文件名生成对应的WritableFile
    Status s = env->NewWritableFile(fname, &file);
    if (!s.ok())
    {
        return s;
    }
    //z 将数据追加到文件尾
    s = file->Append(data);
    if (s.ok())
    {
        //z 关闭文件
        s = file->Close();
    }
    delete file;  // Will auto-close if we did not close above

    //z 如果没有正常关闭，那么关闭文件。
    if (!s.ok())
    {
        env->DeleteFile(fname);
    }
    return s;
}

//z 将数据从文件fname中读取到data中。
Status ReadFileToString(Env* env, const std::string& fname, std::string* data)
{
    data->clear();
    SequentialFile* file;
    Status s = env->NewSequentialFile(fname, &file);
    if (!s.ok())
    {
        return s;
    }
    static const int kBufferSize = 8192;
    char* space = new char[kBufferSize];
    while (true)
    {
        Slice fragment;
        s = file->Read(kBufferSize, &fragment, space);
        if (!s.ok())
        {
            break;
        }
        data->append(fragment.data(), fragment.size());
        if (fragment.empty())
        {
            break;
        }
    }
    delete[] space;
    delete file;
    return s;
}

EnvWrapper::~EnvWrapper()
{
}

}
