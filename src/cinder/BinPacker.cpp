/*
 Copyright (c) 2015, The Barbarian Group
 All rights reserved.

 Portions of this code (C) Paul Houx
 All rights reserved.

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

#include "cinder/BinPacker.h"

#include <cassert>
#include <algorithm>

namespace cinder {

	void BinPackerBase::clear()
	{
		mAvailable.clear();
	}

	bool BinPackerBase::fits( const PackedArea& a, const PackedArea& b ) const
	{
		return ( a.getWidth() <= b.getWidth() && a.getHeight() <= b.getHeight() );
	}

	PackedArea BinPackerBase::split( PackedArea *area, int32_t width, int32_t height ) const
	{
		// Original width and height.
		int32_t w = area->getWidth();
		int32_t h = area->getHeight();

		// Maximize new area.
		int32_t left = width;
		int32_t right = w - width;
		int32_t top = height;
		int32_t bottom = h - height;

		uint64_t areaLeft = left * bottom;
		uint64_t areaRight = right * h;
		uint64_t areaTop = right * top;
		uint64_t areaBottom = w * bottom;

		uint64_t maxLeftRight = math<uint64_t>::max( areaLeft, areaRight );
		uint64_t maxTopBottom = math<uint64_t>::max( areaTop, areaBottom );

		if( maxLeftRight > maxTopBottom ) {
			// right
			PackedArea a( area->x1 + width, area->y1, area->x1 + w, area->y1 + h );
			// left
			area->x2 = area->x1 + left;
			area->y1 = area->y1 + height;
			return a;
		}
		else {
			// top
			PackedArea a( area->x1 + width, area->y1, area->x1 + w, area->y1 + height );
			// bottom
			area->y1 = area->y1 + height;
			area->y2 = area->y1 + bottom;
			return a;
		}
	}

	bool BinPackerBase::pack( PackedArea *area )
	{
		if( mAvailable.empty() )
			mAvailable.push_back( PackedArea( ivec2( 0 ), getSize() ) );

		auto itr = mAvailable.begin();
		while( itr != mAvailable.end() ) {
			auto &available = *itr;

			if( fits( *area, available ) ) {
				// Place area in top left corner of available space.
				int32_t w = area->getWidth();
				int32_t h = area->getHeight();
				area->x1 = available.x1;
				area->x2 = available.x1 + w;
				area->y1 = available.y1;
				area->y2 = available.y1 + h;

				// Split remaining space into 2 new regions.
				PackedArea a = split( &available, w, h );

				if( available.getWidth() <= 0 || available.getHeight() <= 0 )
					mAvailable.erase( itr ); // Note: invalidates 'available'!

				if( a.getWidth() > 0 && a.getHeight() > 0 )
					mAvailable.push_back( a ); // Note: invalidates 'available'!

				// (TODO) Merge regions to maximize space.

				// Sort regions from small to large.
				std::sort( mAvailable.begin(), mAvailable.end() );

				return true;
			}

			++itr;
		}

		return false;
	}

	std::vector<PackedArea> BinPacker::insert( const std::vector<Area> &areas )
	{
		std::vector<PackedArea> result;

		if( areas.empty() )
			return result;

		// Create a list of packed areas, sorted from large to small.
		result.reserve( areas.size() );

		size_t order = 0;
		for( auto &area : areas )
			result.push_back( PackedArea( area, order++ ) );

		std::sort( result.rbegin(), result.rend() );

		// Attempt to pack all of them.
		for( auto &area : result ) {
			if( !pack( &area ) )
				throw BinPackerTooSmallExc();
		}

		// Sort result by order.
		std::sort( result.begin(), result.end(), PackedArea::sortByOrder );

		return result;
	}

	PackedArea BinPackerBase::insert( const Area &area )
	{
		PackedArea result( area );

		// Attempt to pack all of them.
		if( !pack( &result ) )
			throw BinPackerTooSmallExc();

		return result;
	}

	std::vector<PackedArea> MultiBinPacker::insert( const std::vector<Area> &areas )
	{
		std::vector<PackedArea> result;

		if( areas.empty() )
			return result;

		// Create a list of packed areas, sorted from large to small.
		result.reserve( areas.size() );

		size_t order = 0;
		for( auto &area : areas )
			result.push_back( PackedArea( area, order++ ) );

		std::sort( result.rbegin(), result.rend() );

		// Attempt to pack all of them.
		for( auto &area : result ) {
			if( pack( &area ) ) {
				area.mBin = mBin;
			}
			else {
				// Create a new bin.
				BinPackerBase::clear();
				mBin++;

				// Try again.
				if( pack( &area ) )
					area.mBin = mBin;
				else
					throw BinPackerTooSmallExc();
			}
		}

		// (TODO) Sort result on order.

		return result;
	}

	void MultiBinPacker::clear()
	{
		BinPackerBase::clear();
		mBin = 0;
	}

} // namespace cinder