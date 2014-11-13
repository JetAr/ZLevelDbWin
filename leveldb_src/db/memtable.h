// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_MEMTABLE_H_
#define STORAGE_LEVELDB_DB_MEMTABLE_H_

#include <string>
#include "leveldb/db.h"
#include "db/dbformat.h"
#include "db/skiplist.h"
#include "util/arena.h"

namespace leveldb {
    class InternalKeyComparator;
    class Mutex;
    class MemTableIterator;

    class MemTable {
    public:
        // MemTables are reference counted.  The initial reference count
        // is zero and the caller must call Ref() at least once.
        //z 采用了引用计数的形式。
        //z 其值初始为0，调用者必须至少调用一次 Ref() 。
        explicit MemTable(const InternalKeyComparator& comparator);

        // Increase reference count.
        //z 增加引用计数
        void Ref() { ++refs_; }

        // Drop reference count.  Delete if no more references exist.
        //z 减少引用计数，如果引用计数不大于0，那么销毁对象
        void Unref() {
            --refs_;
            assert(refs_ >= 0);
            if (refs_ <= 0) {
                delete this;
            }
        }

        // Returns an estimate of the number of bytes of data in use by this
        // data structure.
        //
        // REQUIRES: external synchronization to prevent simultaneous
        // operations on the same MemTable.
        //z 返回一个占用内存的估计值
        //z 前提：在同一个 MemTable 上操作需要外部同步
        size_t ApproximateMemoryUsage();

        // Return an iterator that yields the contents of the memtable.
        //
        // The caller must ensure that the underlying MemTable remains live
        // while the returned iterator is live.  The keys returned by this
        // iterator are internal keys encoded by AppendInternalKey in the
        // db/format.{h,cc} module.
        Iterator* NewIterator();

        // Add an entry into memtable that maps key to value at the
        // specified sequence number and with the specified type.
        // Typically value will be empty if type==kTypeDeletion.
        void Add(SequenceNumber seq, ValueType type,
            const Slice& key,
            const Slice& value);

        // If memtable contains a value for key, store it in *value and return true.
        // If memtable contains a deletion for key, store a NotFound() error
        // in *status and return true.
        // Else, return false.
        bool Get(const LookupKey& key, std::string* value, Status* s);

    private:
        ~MemTable();  // Private since only Unref() should be used to delete it

        struct KeyComparator {
            const InternalKeyComparator comparator;
            explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) { }
            int operator()(const char* a, const char* b) const;
        };
        friend class MemTableIterator;
        friend class MemTableBackwardIterator;

        //z 看起来使用了一个 SkipList
        typedef SkipList<const char*, KeyComparator> Table;

        KeyComparator comparator_;
        int refs_;
        Arena arena_;
        Table table_;

        // No copying allowed
        MemTable(const MemTable&);
        void operator=(const MemTable&);
    };
}

#endif  // STORAGE_LEVELDB_DB_MEMTABLE_H_
