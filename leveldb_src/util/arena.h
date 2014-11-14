// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_ARENA_H_
#define STORAGE_LEVELDB_UTIL_ARENA_H_

#include <cstddef>
#include <vector>
#include <assert.h>
#include <stdint.h>

namespace leveldb
{
/*//z
思路：
先在上一次分配的内存块中查找是否剩余空间能否容纳当前申请的空间；如果足以容纳，直接使用剩余空间
否则视其大小重新分配一块空间：如果大于1024，直接分配一块新空间，大小与申请空间大小同
如果小于1024，说明上一块空间剩余空间小于1024，那么重新分配一块4096大小的空间，使用该空间来存放待申请的空间
*/
class Arena
{
public:
    Arena();
    ~Arena();

    // Return a pointer to a newly allocated memory block of "bytes" bytes.
    char* Allocate(size_t bytes);

    // Allocate memory with the normal alignment guarantees provided by malloc
    char* AllocateAligned(size_t bytes);

    // Returns an estimate of the total memory usage of data allocated
    // by the arena (including space allocated but not yet used for user
    // allocations).
    size_t MemoryUsage() const
    {
        //z 分配内存空间，以及存放char*指针的vector所大概占据的空间。
        return blocks_memory_ + blocks_.capacity() * sizeof(char*);
    }

private:
    char* AllocateFallback(size_t bytes);
    char* AllocateNewBlock(size_t block_bytes);

    // Allocation state
    char* alloc_ptr_;
    size_t alloc_bytes_remaining_;

    // Array of new[] allocated memory blocks
    //z 存放 char* 指针
    std::vector<char*> blocks_;

    // Bytes of memory in blocks allocated so far
    size_t blocks_memory_;

    // No copying allowed
    Arena(const Arena&);
    void operator=(const Arena&);
};

inline char* Arena::Allocate(size_t bytes)
{
    // The semantics of what to return are a bit messy if we allow
    // 0-byte allocations, so we disallow them here (we don't need
    // them for our internal use).
    assert(bytes > 0);
    //z 在一块直接分配好的内存上移动一下指针即可；事实上可能会存在很多的浪费。
    if (bytes <= alloc_bytes_remaining_)
    {
        char* result = alloc_ptr_;
        alloc_ptr_ += bytes;
        alloc_bytes_remaining_ -= bytes;
        return result;
    }

    //z 如果剩余控件不足与容纳，那么根据bytes大小决定是否重新分配一块标准大小的内存（4096），或者要是bytes大于1024，直接
    //z 分配其大小的内存，原标准块内存仍旧有用。
    //z 如果小于 1024 ，说明原标准块剩余内存不足以容纳1024，新分配一块标准块内存，用此来为bytes分配对应内存。
    return AllocateFallback(bytes);
}

}

#endif  // STORAGE_LEVELDB_UTIL_ARENA_H_
