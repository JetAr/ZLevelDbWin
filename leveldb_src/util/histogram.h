// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_UTIL_HISTOGRAM_H_
#define STORAGE_LEVELDB_UTIL_HISTOGRAM_H_

#include <string>

namespace leveldb
{

//z 搜索了下代码，目前代码中没有使用该类
class Histogram
{
public:
	Histogram() { }
	~Histogram() { }

    //z 清除所有的值
	void Clear();
    //z 增加一个值
	void Add(double value);
    //z 合并两个直方图
	void Merge(const Histogram& other);

    //z 格式化输出直方图一些信息，如果均值，中位值，样本个数等等
	std::string ToString() const;

private:
	double min_;
	double max_;
	double num_;
	double sum_;
	double sum_squares_;

	enum { kNumBuckets = 154 };
	static const double kBucketLimit[kNumBuckets];
    //z 存储 在对应范围的
	double buckets_[kNumBuckets];

    //z 中位数
	double Median() const;
	double Percentile(double p) const;
    //z 平均值
	double Average() const;
    //z 方差
	double StandardDeviation() const;
};

}

#endif	// STORAGE_LEVELDB_UTIL_HISTOGRAM_H_
