// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"
#include <assert.h>

namespace leveldb {
	//z 常量变量名k开头
	static const int kBlockSize = 4096;

	//z 初始化
	Arena::Arena() {
		blocks_memory_ = 0;
		alloc_ptr_ = NULL;  // First allocation will allocate a block
		alloc_bytes_remaining_ = 0;
	}

	Arena::~Arena() {
		//z 删除申请的内存
		for (size_t i = 0; i < blocks_.size(); i++) {
			delete[] blocks_[i];
		}
	}

	char* Arena::AllocateFallback(size_t bytes) {
		//z 如果申请的bytes > 1024
		if (bytes > kBlockSize / 4) {
			// Object is more than a quarter of our block size.  Allocate it separately
			// to avoid wasting too much space in leftover bytes.
			char* result = AllocateNewBlock(bytes);
			return result;
		}

		// We waste the remaining space in the current block.
		//z 不大于1024时，分配一块标准大小的内存
		alloc_ptr_ = AllocateNewBlock(kBlockSize);
		alloc_bytes_remaining_ = kBlockSize;

		//z 指定返回的位置
		char* result = alloc_ptr_;
		//z 移动指针位置至空闲内存开始的地方
		alloc_ptr_ += bytes;
		//z 记录还剩下多少内存
		alloc_bytes_remaining_ -= bytes;
		return result;
	}

	char* Arena::AllocateAligned(size_t bytes) {
		//z 这个值是固定的，不用每次都求一次？但是代价非常小，求一次也无所为？
		const int align = sizeof(void*);    // We'll align to pointer size
		assert((align & (align-1)) == 0);   // Pointer size should be a power of 2
		//z 求的其余数
		size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align-1);
		size_t slop = (current_mod == 0 ? 0 : align - current_mod);
		size_t needed = bytes + slop;
		char* result;
		//z 如果剩余的空间足以容纳所需要的内存
		if (needed <= alloc_bytes_remaining_) {
			//z 对齐返回地址
			result = alloc_ptr_ + slop;
			alloc_ptr_ += needed;
			alloc_bytes_remaining_ -= needed;
		} else {
			// AllocateFallback always returned aligned memory
			//z 否则直接分配一块新的内存
			//z 在这种情况下这块内存可能很小
			result = AllocateFallback(bytes);
		}
		//z 确保返回地址是对齐的
		assert((reinterpret_cast<uintptr_t>(result) & (align-1)) == 0);
		return result;
	}
	
	//z 在不小于一个page的时候，直接采用这种方式
	char* Arena::AllocateNewBlock(size_t block_bytes) {
		char* result = new char[block_bytes];
		blocks_memory_ += block_bytes;
		blocks_.push_back(result);
		return result;
	}

}
