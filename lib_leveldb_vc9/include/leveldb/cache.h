// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
//z 将keys 和values对应起来
// A Cache is an interface that maps keys to values.  It has internal
// synchronization and may be safely accessed concurrently from
// multiple threads.  It may automatically evict entries to make room
// for new entries.  Values have a specified charge against the cache
// capacity.  For example, a cache where the values are variable
// length strings, may use the length of the string as the charge for
// the string.
//
// A builtin cache implementation with a least-recently-used eviction
// policy is provided.  Clients may use their own implementations if
// they want something more sophisticated (like scan-resistance, a
// custom eviction policy, variable cache sizing, etc.)

#ifndef STORAGE_LEVELDB_INCLUDE_CACHE_H_
#define STORAGE_LEVELDB_INCLUDE_CACHE_H_

#include "win32exports.h"
#include <stdint.h>
#include "leveldb/slice.h"

namespace leveldb
{

class Cache;

//z 使用了 LRU 策略，比如 cache 满后最近最少使用将会被从cache中移除
// Create a new cache with a fixed size capacity.  This implementation
// of Cache uses a least-recently-used eviction policy.
extern Cache* NewLRUCache(size_t capacity);

class LEVELDB_EXPORT Cache
{
public:
    Cache() { }

    // Destroys all existing entries by calling the "deleter"
    // function that was passed to the constructor.
    virtual ~Cache();

    // Opaque handle to an entry stored in the cache.
    //z 一个不透明的handle。
    struct Handle { };

    //z 存储key-value值对到cache中，并根据整个cache的容量将之赋值到指定的charge
    // Insert a mapping from key->value into the cache and assign it
    // the specified charge against the total cache capacity.
    //
    //z 返回一个指向mapping的handle。在不再返回的mapping时，需要调用 this->Release(handle);
    // Returns a handle that corresponds to the mapping.  The caller
    // must call this->Release(handle) when the returned mapping is no
    // longer needed.
    //
    //z 当 inserted entry 不再需要时，key 和 value 将会返回给 deleter 。
    // When the inserted entry is no longer needed, the key and
    // value will be passed to "deleter".
    //z 使用字符串作为key，value则不一定了；charge是做什么用了？
    virtual Handle* Insert(const Slice& key, void* value, size_t charge,
                           void (*deleter)(const Slice& key, void* value)) = 0;

    // If the cache has no mapping for "key", returns NULL.
    //
    // Else return a handle that corresponds to the mapping.  The caller
    // must call this->Release(handle) when the returned mapping is no
    // longer needed.
    //z 查询，如果key没有mapping，那么返回NULL。
    //z 否则，返回一个对应mapping的handle
    //z 当返回mapping不再需要的时候，调用者必须调用 this->Release(handle)。
    virtual Handle* Lookup(const Slice& key) = 0;
    //z 返回一个 handle，当不再需要时，需要调用 this->Release(handle)s

    // Release a mapping returned by a previous Lookup().
    // REQUIRES: handle must not have been released yet.
    // REQUIRES: handle must have been returned by a method on *this.
    //z 释放由Lookup返回的一个mapping
    //z 条件：handle必须未曾被释放过
    //z 条件：handle必须是由*this上的一个method返回的
    virtual void Release(Handle* handle) = 0;

    // Return the value encapsulated in a handle returned by a
    // successful Lookup().
    // REQUIRES: handle must not have been released yet.
    // REQUIRES: handle must have been returned by a method on *this.
    //z 返回由handle封装的值，handle由Lookup所返回
    //z handle 必须未曾释放
    //z handle 必须由*this的一个method返回
    virtual void* Value(Handle* handle) = 0;

    // If the cache contains entry for key, erase it.  Note that the
    // underlying entry will be kept around until all existing handles
    // to it have been released.
    //z 如果 cache 中包含了该key，那么将之擦除。
    //z 注意，如果下面的entry将会被保留直到没有任何的handle指向它。
    virtual void Erase(const Slice& key) = 0;

    // Return a new numeric id.  May be used by multiple clients who are
    // sharing the same cache to partition the key space.  Typically the
    // client will allocate a new id at startup and prepend the id to
    // its cache keys.
    //z 返回一个新的 numeric id。
    virtual uint64_t NewId() = 0;

private:
    void LRU_Remove(Handle* e);
    void LRU_Append(Handle* e);
    void Unref(Handle* e);

    struct Rep;
    Rep* rep_;

    // No copying allowed
    Cache(const Cache&);
    void operator=(const Cache&);
};

}

#endif  // STORAGE_LEVELDB_UTIL_CACHE_H_
