// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/coding.h"

namespace leveldb {

	void EncodeFixed32(char* buf, uint32_t value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		//z 如果cpu是LE的，那么直接拷贝。intel cpu系列是LE的
		memcpy(buf, &value, sizeof(value));
#else
		buf[0] = value & 0xff;
		buf[1] = (value >> 8) & 0xff;
		buf[2] = (value >> 16) & 0xff;
		buf[3] = (value >> 24) & 0xff;
#endif
	}

	void EncodeFixed64(char* buf, uint64_t value) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		//z 与32位类似，只是拷贝的时候尺寸不一样
		memcpy(buf, &value, sizeof(value));
#else
		buf[0] = value & 0xff;
		buf[1] = (value >> 8) & 0xff;
		buf[2] = (value >> 16) & 0xff;
		buf[3] = (value >> 24) & 0xff;
		buf[4] = (value >> 32) & 0xff;
		buf[5] = (value >> 40) & 0xff;
		buf[6] = (value >> 48) & 0xff;
		buf[7] = (value >> 56) & 0xff;
#endif
	}

	void PutFixed32(std::string* dst, uint32_t value) {
		char buf[sizeof(value)];
		//z 将 value 以 LE 的方式添加到 buf 中去。
		EncodeFixed32(buf, value);
		//z 然后直接将各位数字放到buf中去
		dst->append(buf, sizeof(buf));
	}

	void PutFixed64(std::string* dst, uint64_t value) {
		char buf[sizeof(value)];
		EncodeFixed64(buf, value);
		dst->append(buf, sizeof(buf));
	}

	//z 以7位为一个单位，一共 7 + 7 + 7 + 7 + 4
	//z 变长编码，一个字符的第一位为0时表示是一个变量的结束，类似分隔符。
	//z 这样在序列中能够找出起止的地方。
	char* EncodeVarint32(char* dst, uint32_t v) {
		// Operate on characters as unsigneds
		//z 将 uint32_t 类型重新解释为 unsigned char 类型，使用 reinterpret_cast 来完成
		unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
		static const int B = 128;//z 2^7 0b1000000 0x80
		if (v < (1<<7)) {//z 128，即范围在 0->127
			*(ptr++) = v;
		} else if (v < (1<<14)) {//z 14位以内
			*(ptr++) = v | B;
			*(ptr++) = v>>7;
		} else if (v < (1<<21)) {//z 21位以内
			*(ptr++) = v | B;
			*(ptr++) = (v>>7) | B;
			*(ptr++) = v>>14;
		} else if (v < (1<<28)) {//z 28位以内
			*(ptr++) = v | B;//z 二进制 1000 0000
			*(ptr++) = (v>>7) | B;
			*(ptr++) = (v>>14) | B;
			*(ptr++) = v>>21;
		} else {
			//z 从这里可以看出来是非最后一位都要的char字符，头一个bit都是1，如果头一个bit为0，则意味着字符串的结束
			*(ptr++) = v | B;
			*(ptr++) = (v>>7) | B;
			*(ptr++) = (v>>14) | B;
			*(ptr++) = (v>>21) | B;
			*(ptr++) = v>>28;
		}
		return reinterpret_cast<char*>(ptr);
	}

	void PutVarint32(std::string* dst, uint32_t v) {
		char buf[5];
		char* ptr = EncodeVarint32(buf, v);
		//z 将编码之后的内容给附加到buf上去的
		dst->append(buf, ptr - buf);
	}
	
	char* EncodeVarint64(char* dst, uint64_t v) {
		static const int B = 128;
		unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
		while (v >= B) {
			*(ptr++) = (v & (B-1)) | B;
			v >>= 7;
		}
		*(ptr++) = static_cast<unsigned char>(v);
		return reinterpret_cast<char*>(ptr);
	}

	void PutVarint64(std::string* dst, uint64_t v) {
		char buf[10];
		char* ptr = EncodeVarint64(buf, v);
		dst->append(buf, ptr - buf);
	}

	void PutLengthPrefixedSlice(std::string* dst, const Slice& value) {
		PutVarint32(dst, value.size());
		dst->append(value.data(), value.size());
	}

	int VarintLength(uint64_t v) {
		int len = 1;
		while (v >= 128) {
			v >>= 7;
			len++;
		}
		return len;
	}

	const char* GetVarint32PtrFallback(const char* p,
		const char* limit,
		uint32_t* value) {
			uint32_t result = 0;
			for (uint32_t shift = 0; shift <= 28 && p < limit; shift += 7) {
				uint32_t byte = *(reinterpret_cast<const unsigned char*>(p));
				p++;//z 指向下一个8位
				if (byte & 128) {
					// More bytes are present
					result |= ((byte & 127) << shift);
				} else {//z 此时其值为 1000 0000，如果遇到此值就返回。不明白为啥？但是正常的char值不应该有此值。
					result |= (byte << shift);
					*value = result;
					return reinterpret_cast<const char*>(p);
				}
			}
			return NULL;
	}

	bool GetVarint32(Slice* input, uint32_t* value) {
		const char* p = input->data();
		const char* limit = p + input->size();
		const char* q = GetVarint32Ptr(p, limit, value);
		if (q == NULL) {
			return false;
		} else {
			*input = Slice(q, limit - q);
			return true;
		}
	}

	const char* GetVarint64Ptr(const char* p, const char* limit, uint64_t* value) {
		uint64_t result = 0;
		for (uint32_t shift = 0; shift <= 63 && p < limit; shift += 7) {
			uint64_t byte = *(reinterpret_cast<const unsigned char*>(p));
			p++;
			if (byte & 128) {
				// More bytes are present
				result |= ((byte & 127) << shift);
			} else {
				result |= (byte << shift);
				*value = result;
				return reinterpret_cast<const char*>(p);
			}
		}
		return NULL;
	}

	bool GetVarint64(Slice* input, uint64_t* value) {
		const char* p = input->data();
		const char* limit = p + input->size();
		const char* q = GetVarint64Ptr(p, limit, value);
		if (q == NULL) {
			return false;
		} else {
			*input = Slice(q, limit - q);
			return true;
		}
	}

	const char* GetLengthPrefixedSlice(const char* p, const char* limit,
		Slice* result) {
			uint32_t len;
			p = GetVarint32Ptr(p, limit, &len);
			if (p == NULL) return NULL;
			if (p + len > limit) return NULL;
			*result = Slice(p, len);
			return p + len;
	}

	bool GetLengthPrefixedSlice(Slice* input, Slice* result) {
		uint32_t len;
		if (GetVarint32(input, &len) &&
			input->size() >= len) {
				*result = Slice(input->data(), len);
				input->remove_prefix(len);
				return true;
		} else {
			return false;
		}
	}

}
