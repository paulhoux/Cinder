/*
 Copyright (c) 2010, The Barbarian Group
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
		BinPackerBase() : mBinWidth( 512 ), mBinHeight( 512 ) { clear(); }
		BinPackerBase( int width, int height ) :
			mBinWidth( width ), mBinHeight( height ) { clear(); }

		virtual ~BinPackerBase() {}

		//! Sets the width and height of the bin.
		virtual BinPackerBase&	setSize( uint32_t width, uint32_t height ) = 0;
		//! Sets the width and height of the bin.
		virtual BinPackerBase&	setSize( const ivec2 &size ) = 0;

		//! Takes a list of \a areas, packs them and returns a list of packed areas.
		virtual std::vector<PackedArea>	pack( const std::vector<Area> &areas, bool allowRotation = false ) = 0;

		//! Returns the size of the bin.
		ivec2		getSize() const { return ivec2( mBinWidth, mBinHeight ); }
		//! Returns the width of the bin.
		int			getWidth() const { return mBinWidth; }
		//! Returns the height of the bin.
		int			getHeight() const { return mBinHeight; }

	public:
		struct Rect {
			int  x;
			int  y;
			int  w;
			int  h;
			int  order;
			int  children[2];
			bool rotated;
			bool packed;

			Rect( int w, int h, int order = -1 )
				: x( 0 ), y( 0 ), w( w ), h( h ), order( order ), rotated( false ), packed( false )
			{
				children[0] = -1;
				children[1] = -1;
			}

			Rect( int x, int y, int w, int h, int order = -1 )
				: x( x ), y( y ), w( w ), h( h ), order( order ), rotated( false ), packed( false )
			{
				children[0] = -1;
				children[1] = -1;
			}

			int getArea() const
			{
				return w * h;
			}

			void rotate()
			{
				std::swap( w, h );
				rotated = !rotated;
			}

			bool operator<( const Rect& rhs ) const
			{
				return getArea() < rhs.getArea();
			}
		};

	protected:
		int					mBinWidth;
		int					mBinHeight;

		int					mNumPacked;
		std::vector<Rect>	mRects;
		std::vector<Rect>	mPacks;

		virtual void clear();

		void fill( int pack, bool allowRotation );
		void split( int pack, int rect );
		bool fits( Rect& rect1, const Rect& rect2, bool allowRotation ) const;

		bool rectIsValid( int i ) const;
		bool packIsValid( int i ) const;
	};

	class BinPacker /*final*/ : public BinPackerBase {
	public:
		BinPacker() : BinPackerBase( 512, 512 ) {}
		BinPacker( int width, int height ) : BinPackerBase( width, height ) {}
		BinPacker( const ivec2 &size ) : BinPackerBase( size.x, size.y ) {}

		~BinPacker() {}

		//! Sets the width and height of the bin.
		BinPacker&	setSize( uint32_t width, uint32_t height ) override { mBinWidth = width; mBinHeight = height; return *this; }
		//! Sets the width and height of the bin.
		BinPacker&	setSize( const ivec2 &size ) override { mBinWidth = size.x; mBinHeight = size.y; return *this; }

		//! Takes a list of \a areas, packs them and returns a list of packed areas.
		std::vector<PackedArea>	pack( const std::vector<Area> &rects, bool allowRotation = false ) override;
	};

	class MultiBinPacker /*final*/ : public BinPackerBase {
	public:
		MultiBinPacker() : BinPackerBase( 512, 512 ) {}
		MultiBinPacker( int width, int height ) : BinPackerBase( width, height ) {}
		MultiBinPacker( const ivec2 &size ) : BinPackerBase( size.x, size.y ) {}

		~MultiBinPacker() {}

		//! Sets the width and height of each bin.
		MultiBinPacker&	setSize( uint32_t width, uint32_t height ) override { mBinWidth = width; mBinHeight = height; return *this; }
		//! Sets the width and height of each bin.
		MultiBinPacker&	setSize( const ivec2 &size ) override { mBinWidth = size.x; mBinHeight = size.y; return *this; }

		//! Takes a list of \a areas, packs them and returns a list of packed areas.
		std::vector<PackedArea>	pack( const std::vector<Area> &rects, bool allowRotation = false ) override;

	private:
		void clear();

		std::vector<uint32_t>	mBins;
	};

	class PackedArea : public Area {
	public:
		PackedArea()
			: Area(), mBin( -1 ), mIsRotated( false ) {}
		PackedArea( int32_t bin, bool isRotated = false )
			: Area(), mBin( bin ), mIsRotated( isRotated ) {}

		PackedArea( const ivec2 &UL, const ivec2 &LR )
			: Area( UL, LR ), mBin( -1 ), mIsRotated( false ) {}
		PackedArea( const ivec2 &UL, const ivec2 &LR, int32_t bin, bool isRotated = false )
			: Area( UL, LR ), mBin( bin ), mIsRotated( isRotated ) {}

		PackedArea( int32_t aX1, int32_t aY1, int32_t aX2, int32_t aY2 )
			: mBin( -1 ), mIsRotated( false )
		{
			set( aX1, aY1, aX2, aY2 );
		}

		PackedArea( int32_t aX1, int32_t aY1, int32_t aX2, int32_t aY2, int32_t bin, bool isRotated = false )
			: mBin( bin ), mIsRotated( isRotated )
		{
			set( aX1, aY1, aX2, aY2 );
		}

		PackedArea( const BinPackerBase::Rect &rect, uint32_t bin = 0 )
			: Area( rect.x, rect.y, rect.x + rect.w, rect.y + rect.h ), mBin( bin ), mIsRotated( rect.rotated )
		{
		}

		explicit PackedArea( const RectT<float> &r )
			: Area( r ), mBin( -1 ), mIsRotated( false ) {}
		explicit PackedArea( const RectT<float> &r, int32_t bin, bool isRotated = false )
			: Area( r ), mBin( bin ), mIsRotated( isRotated ) {}

		explicit PackedArea( const Area &area )
			: Area( area ), mBin( -1 ), mIsRotated( false ) {}
		explicit PackedArea( const Area &area, int32_t bin, bool isRotated = false )
			: Area( area ), mBin( bin ), mIsRotated( isRotated ) {}

		//! Returns the bin number in which this area is packed.
		int32_t getBin() const { return mBin; }
		//! Returns \c true if the area is rotated.
		bool isRotated() const { return mIsRotated; }

	private:
		int32_t		mBin;
		bool		mIsRotated;
	};

	class BinPackerTooSmallExc : public std::exception {
	public:
		virtual const char* what() const throw( ) { return "Bin size is too small to fit all rectangles."; }
	};

} // namespace cinder
