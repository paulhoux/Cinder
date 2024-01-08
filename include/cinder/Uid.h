/*
 Copyright (c) 2024, The Cinder Project
 All rights reserved.

 This code is designed for use with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

	* Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include <deque>

namespace cinder {

//! Unique ID-generator, similar to OpenGL names. Not thread-safe.
template <typename T = unsigned, std::enable_if_t<std::is_integral<T>::value && !std::is_signed<T>::value, bool> = true>
class Uid {
	struct Range {
		T min;
		T max;

		bool operator<( T id ) const { return min < id; }
	};

  public:
	Uid() = default;

	//! Generates \a count id's.
	void generate( T count, T *ids )
	{
		for( T i = 0; i < count; ++i )
			*ids++ = pop();
	}

	//! Destroys \a count id's.
	void destroy( T count, const T *ids )
	{
		for( T i = 0; i < count; ++i )
			push( *ids++ );
	}

  private:
	T pop()
	{
		if( mRanges.empty() )
			return 0; // Return 0 on fail.

		auto &range = mRanges.front();

		T result = range.min++;
		if( result == range.max )
			mRanges.pop_front();

		return result;
	}

	void push( T id )
	{
		// Silently ignore pushing zeroes.
		if( !id )
			return;

		if( mRanges.empty() ) {
			// Insert new range if no ranges available.
			mRanges.insert( mRanges.end(), { id, id } );
		}
		else {
			// Find range where min >= id.
			auto it = std::lower_bound( mRanges.begin(), mRanges.end(), id );
			if( it != mRanges.begin() ) {
				auto prev = std::prev( it );

				if( id == prev->max + 1 ) {
					++prev->max; // Extend range to the right.
					merge( prev );
				}
				else if( it != mRanges.end() && id == it->min - 1 ) {
					--it->min; // Extend range to the left.
					merge( prev );
				}
				else if( id > prev->max + 1 )
					mRanges.insert( it, { id, id } ); // Insert new range.
			}
			else if( id < it->min ) {
				if( id == it->min - 1 ) {
					--it->min; // Extend range to the left.
					merge( it );
				}
				else
					mRanges.insert( it, { id, id } ); // Insert new range.
			}
		}
	}

	void merge( typename std::deque<Range>::iterator it )
	{
		auto next = std::next( it );

		if( next != mRanges.end() && it->max + 1 == next->min ) {
			it->max = next->max;
			mRanges.erase( next );
		}
	}

	std::deque<Range> mRanges{ { T{ 1 }, std::numeric_limits<T>::max() } };
};

//! Unique ID-generator, similar to OpenGL names. Thread-safe implementation.
template <typename T = unsigned, std::enable_if_t<std::is_integral<T>::value && !std::is_signed<T>::value, bool> = true>
class ConcurrentUid {
  public:
	ConcurrentUid() = default;

	//! Generates \a count id's.
	void generate( T count, T *ids )
	{
		std::lock_guard lock( mMutex );
		mUid.generate( count, ids );
	}

	//! Destroys \a count id's.
	void destroy( T count, const T *ids )
	{
		std::lock_guard lock( mMutex );
		mUid.destroy( count, ids );
	}

  private:
	std::mutex mMutex;
	Uid<T>     mUid;
};

} // namespace cinder