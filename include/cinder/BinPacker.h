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

#pragma once

#include "cinder/Area.h"

#include <vector>
#include <map>

namespace cinder {

	class PackedArea;

	class BinPackerBase {
	public:
		BinPackerBase() {}
		virtual ~BinPackerBase() {}

		//! Sets the width and height of the bin.
		virtual void setSize( uint32_t width, uint32_t height ) { setSize( ivec2( width, height ) ); }
		//! Sets the width and height of the bin.
		virtual void setSize( const ivec2 &size ) { mSize = size; }

		//! Takes a list of \a areas, packs them and returns a list of packed areas.
		virtual std::vector<PackedArea>	insert( const std::vector<Area> &areas ) = 0;
		//! Packs the \a area and returns it as a PackedArea.
		virtual PackedArea insert( const Area &area );

		//! Returns the size of the bin.
		ivec2		getSize() const { return mSize; }
		//! Returns the width of the bin.
		int32_t		getWidth() const { return mSize.x; }
		//! Returns the height of the bin.
		int32_t		getHeight() const { return mSize.y; }

		//! Clears the internal data structures.
		virtual void clear();

	protected:
		//! Returns \c true if \a a fits inside \a b.
		bool fits( const PackedArea& a, const PackedArea& b ) const;
		//! Splits \a area into 2 new areas.
		PackedArea split( PackedArea *area, int32_t width, int32_t height ) const;
		//! Packs the area. Returns \c true if successful.
		bool pack( PackedArea *area );

	protected:
		ivec2  mSize;

		//! List of available space to fill.
		std::vector<PackedArea>  mAvailable;
	};

	class BinPacker : public BinPackerBase {
	public:
		BinPacker() { clear(); }
		~BinPacker() {}

		//! Sets the width and height of the bin.
		BinPacker& size( uint32_t width, uint32_t height ) { setSize( width, height ); return *this; }
		//! Sets the width and height of the bin.
		BinPacker& size( const ivec2 &size ) { setSize( size ); return *this; }

		//! Adds \a areas to the already packed areas, packs them (online) and returns a list of packed areas.
		std::vector<PackedArea> insert( const std::vector<Area> &areas ) override;
	};

	class MultiBinPacker : public BinPackerBase {
	public:
		MultiBinPacker() { clear(); }
		~MultiBinPacker() {}

		//! Sets the width and height of the bin.
		MultiBinPacker& size( uint32_t width, uint32_t height ) { setSize( width, height ); return *this; }
		//! Sets the width and height of the bin.
		MultiBinPacker& size( const ivec2 &size ) { setSize( size ); return *this; }

		//! Adds \a areas to the already packed areas, packs them (online) and returns a list of packed areas.
		std::vector<PackedArea> insert( const std::vector<Area> &areas ) override;

		//! Clears the internal data structures.
		void clear() override;

	private:
		uint32_t mBin;
	};

	class PackedArea : public Area {
	public:
		PackedArea()
			: mOrder( 0 ), mBin( 0 ) {}
		PackedArea( const ivec2 &UL, const ivec2 &LR, uint32_t order = 0 )
			: Area( UL, LR ), mOrder( order ), mBin( 0 ) {}
		PackedArea( int32_t aX1, int32_t aY1, int32_t aX2, int32_t aY2, uint32_t order = 0 )
			: Area( aX1, aY1, aX2, aY2 ), mOrder( order ), mBin( 0 ) {}
		explicit PackedArea( const RectT<float> &r, uint32_t order = 0 )
			: Area( r ), mOrder( order ), mBin( 0 ) {}
		explicit PackedArea( const Area &area, uint32_t order = 0 )
			: Area( area ), mOrder( order ), mBin( 0 ) {}

		//! Returns the bin number in which this area is packed.
		uint32_t getBin() const { return mBin; }

		//! Allow sorting by area.
		bool operator<( const PackedArea &rhs )
		{
			return calcArea() < rhs.calcArea();
		}

		//! Allow sorting by area.
		static bool sortByArea( const PackedArea& a, const PackedArea& b )
		{
			return a.calcArea() < b.calcArea();
		}

		//! Allow sorting by order.
		static bool sortByOrder( const PackedArea& a, const PackedArea& b )
		{
			return a.mOrder < b.mOrder;
		}

	private:
		friend class MultiBinPacker;

		uint32_t  mOrder;
		uint32_t  mBin;
	};

	class BinPackerTooSmallExc : public std::exception {
	public:
		virtual const char* what() const throw( ) { return "Bin size is too small to fit all areas."; }
	};

} // namespace cinder
