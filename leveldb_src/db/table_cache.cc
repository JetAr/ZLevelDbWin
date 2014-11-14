// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/table_cache.h"

#include "db/filename.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "util/coding.h"

namespace leveldb
{

struct TableAndFile
{
    //z .sst 文件
    RandomAccessFile* file;
    //z .sst 文件在内存中的映像，有.sst文件和index block数据。
    Table* table;
};

static void DeleteEntry(const Slice& key, void* value)
{
    TableAndFile* tf = reinterpret_cast<TableAndFile*>(value);
    delete tf->table;
    delete tf->file;
    delete tf;
}

static void UnrefEntry(void* arg1, void* arg2)
{
    Cache* cache = reinterpret_cast<Cache*>(arg1);
    Cache::Handle* h = reinterpret_cast<Cache::Handle*>(arg2);
    cache->Release(h);
}

TableCache::TableCache(const std::string& dbname,
                       const Options* options,
                       int entries)
    : env_(options->env),
      dbname_(dbname),
      options_(options),
      cache_(NewLRUCache(entries))//z 使用的也是LRU cache。
{
}

TableCache::~TableCache()
{
    delete cache_;
}

Iterator* TableCache::NewIterator(const ReadOptions& options,
                                  uint64_t file_number,
                                  uint64_t file_size,
                                  Table** tableptr)
{
    if (tableptr != NULL)
    {
        *tableptr = NULL;
    }

    char buf[sizeof(file_number)];
    //z 统一编码为 le 
    EncodeFixed64(buf, file_number);
    Slice key(buf, sizeof(buf));
    //z 在cache中查找
    Cache::Handle* handle = cache_->Lookup(key);
    //z 如果没有找到
    if (handle == NULL)
    {
        //z 根据dbname 以及 file_number 构造出一个 table file name
        std::string fname = TableFileName(dbname_, file_number);
        RandomAccessFile* file = NULL;
        Table* table = NULL;
        //z 根据 table file name 创建文件
        Status s = env_->NewRandomAccessFile(fname, &file);
        if (s.ok())
        {
            //z 打开文件
            s = Table::Open(*options_, file, file_size, &table);
        }

        if (!s.ok())
        {
            assert(table == NULL);
            delete file;
            // We do not cache error results so that if the error is transient,
            // or somebody repairs the file, we recover automatically.
            return NewErrorIterator(s);
        }

        //z 创建结构，用于存储对应的 file name 以及 table 等。
        TableAndFile* tf = new TableAndFile;
        tf->file = file;
        tf->table = table;
        handle = cache_->Insert(key, tf, 1, &DeleteEntry);
    }

    Table* table = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
    Iterator* result = table->NewIterator(options);
    result->RegisterCleanup(&UnrefEntry, cache_, handle);
    if (tableptr != NULL)
    {
        *tableptr = table;
    }
    return result;
}

void TableCache::Evict(uint64_t file_number)
{
    char buf[sizeof(file_number)];
    EncodeFixed64(buf, file_number);
    cache_->Erase(Slice(buf, sizeof(buf)));
}

}
