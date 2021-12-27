/*
 * Copyright (c) 2016, Google Inc.
 * All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.

https://github.com/google/perf_data_converter/blob/master/src/intervalmap.h
 */

#pragma once

#include "cinder/Cinder.h"

#include <cstdlib>
#include <iostream>
#include <iterator>
#include <map>
#include <sstream>

namespace cinder {

template <class V>
class IntervalMap {
 public:
  IntervalMap();

  // Set [start, limit) to value. If this interval overlaps one currently in the
  // map, the overlapping section will be overwritten by the new interval.
  void set( size_t start, size_t limit, const V& value );

  // Finds the value associated with the interval containing key. Returns false
  // if no interval contains key.
  bool Lookup(size_t key, V* value) const;

  // Find the interval containing key, or the next interval containing
  // something greater than key. Returns false if one is not found, otherwise
  // it sets start, limit, and value to the corresponding values from the
  // interval.
  bool findNext( size_t key, size_t* start, size_t* limit, V* value ) const;

  // Remove all entries from the map.
  void clear();

  // Clears everything in the interval map from [clear_start,
  // clear_limit). This may cut off sections or entire intervals in
  // the map. This will invalidate iterators to intervals which have a
  // start value residing in [clear_start, clear_limit).
  void clearInterval(size_t clear_start, size_t clear_limit);

  size_t size() const;

  bool	empty() const { return interval_start_.empty(); }

  struct Value {
    size_t limit;
    V value;
  };

  using MapIter = typename std::map<size_t, Value>::iterator;
  using ConstMapIter = typename std::map<size_t, Value>::const_iterator;

  ConstMapIter begin() const { return interval_start_.begin(); }
  ConstMapIter end() const { return interval_start_.end(); }
  
  const std::map<size_t, Value>& getIntervals() const { return interval_start_; }

  void  setEndValue( V value ) { interval_start_.rbegin()->second.value = value; }
  void  setEndLimit( size_t limit ) { interval_start_.rbegin()->second.limit = limit; }
  bool  endIsZeroLength() const { return interval_start_.rbegin()->first == interval_start_.rbegin()->second.limit; }

  // Map from the start of the interval to the limit of the interval and the
  // corresponding value.
  std::map<size_t, Value> interval_start_;

 private:
  // For an interval to be valid, start must be strictly less than limit.
  void AssertValidInterval(size_t start, size_t limit) const;

  // Returns an iterator pointing to the interval containing the given key, or
  // end() if one was not found.
  ConstMapIter GetContainingInterval(size_t point) const {
    auto bound = interval_start_.upper_bound(point);
    if (!Decrement(&bound)) {
      return interval_start_.end();
    }
    if (bound->second.limit <= point) {
      return interval_start_.end();
    }
    return bound;
  }

  MapIter GetContainingInterval(size_t point) {
    auto bound = interval_start_.upper_bound(point);
    if (!Decrement(&bound)) {
      return interval_start_.end();
    }
    if (bound->second.limit <= point) {
      return interval_start_.end();
    }
    return bound;
  }

  // Decrements the provided iterator to interval_start_, or returns false if
  // iter == begin().
  bool Decrement(ConstMapIter* iter) const;
  bool Decrement(MapIter* iter);

  // Removes everything in the interval map from [remove_start,
  // remove_limit). This may cut off sections or entire intervals in
  // the map. This will invalidate iterators to intervals which have a
  // start value residing in [remove_start, remove_limit).
  void RemoveInterval(size_t remove_start, size_t remove_limit);

  void Insert(size_t start, size_t limit, const V& value);

  // Split an interval into two intervals, [iter.start, point) and
  // [point, iter.limit). If point is not within (iter.start, point) or iter is
  // end(), it is a noop.
  void SplitInterval(MapIter iter, size_t point);
};

template <class V>
IntervalMap<V>::IntervalMap() {}

template <class V>
void IntervalMap<V>::set(size_t start, size_t limit, const V& value) {
  AssertValidInterval(start, limit);
  RemoveInterval(start, limit);
  Insert(start, limit, value);
}

template <class V>
bool IntervalMap<V>::Lookup(size_t key, V* value) const {
  const auto contain = GetContainingInterval(key);
  if (contain == interval_start_.end()) {
    return false;
  }
  *value = contain->second.value;
  return true;
}

template<class V>
bool IntervalMap<V>::findNext( size_t key, size_t* start, size_t* limit, V* value ) const
{
  auto iter = interval_start_.upper_bound(key);
  if (iter == interval_start_.end()) {
    return false;
  }
  *start = iter->first;
  *limit = iter->second.limit;
  *value = iter->second.value;
  return true;
}

template <class V>
void IntervalMap<V>::clear() {
  interval_start_.clear();
}

template <class V>
void IntervalMap<V>::clearInterval(size_t clear_start, size_t clear_limit) {
  AssertValidInterval(clear_start, clear_limit);
  RemoveInterval(clear_start, clear_limit);
}

template <class V>
size_t IntervalMap<V>::size() const {
  return interval_start_.size();
}

template <class V>
void IntervalMap<V>::RemoveInterval(size_t remove_start, size_t remove_limit) {
  if (remove_start > remove_limit) {
    return;
  }
  else if (remove_start == remove_limit) {
    auto it = interval_start_.find(remove_start);
    if( it != interval_start_.end() )
      interval_start_.erase( it );
    return;
  }
  // It starts by splitting intervals that will only be partly cleared into two,
  // where one of those will be fully cleared and the other will not be cleared.
  SplitInterval(GetContainingInterval(remove_limit), remove_limit);
  SplitInterval(GetContainingInterval(remove_start), remove_start);

  auto remove_interval_start = interval_start_.lower_bound(remove_start);
  auto remove_interval_end = interval_start_.lower_bound(remove_limit);
  // Note that if there are no intervals to be cleared, then
  // clear_interval_start == clear_interval_end and the erase will be a noop.
  interval_start_.erase(remove_interval_start, remove_interval_end);
}

template <class V>
void IntervalMap<V>::SplitInterval(MapIter iter, size_t point) {
  if (iter == interval_start_.end() || point <= iter->first ||
      point >= iter->second.limit) {
    return;
  }
  const auto larger_limit = iter->second.limit;
  iter->second.limit = point;
  Insert(point, larger_limit, iter->second.value);
}

template <class V>
bool IntervalMap<V>::Decrement(ConstMapIter* iter) const {
  if ((*iter) == interval_start_.begin()) {
    return false;
  }
  --(*iter);
  return true;
}

template <class V>
bool IntervalMap<V>::Decrement(MapIter* iter) {
  if ((*iter) == interval_start_.begin()) {
    return false;
  }
  --(*iter);
  return true;
}

template <class V>
void IntervalMap<V>::Insert(size_t start, size_t limit, const V& value) {
  interval_start_.emplace(std::pair<size_t, Value>{start, {limit, value}});
}

template <class V>
void IntervalMap<V>::AssertValidInterval(size_t start, size_t limit) const {
  if (start > limit) {
    std::cerr << "Invalid interval. Start may not be > limit." << std::endl
              << "Start: " << start << std::endl
              << "Limit: " << limit << std::endl;
    abort();
  }
}

} // namespace cinder
