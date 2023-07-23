/*
 Copyright (c) 2020, The Cinder Project: http://libcinder.org
 All rights reserved.

 This code is intended for use with the Cinder C++ library: http://libcinder.org

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

#include "cinder/Cinder.h"
#include "cinder/text/Face.h"
#include "cinder/Color.h"
#include <iosfwd>

typedef struct FT_FaceRec_		*FT_Face;
typedef struct FT_SizeRec_		*FT_Size;
typedef struct hb_font_t 		hb_font_t;
typedef struct hb_buffer_t 		hb_buffer_t;
typedef struct hb_feature_t		hb_feature_t;

namespace cinder {
	template<typename T> 		class ChannelT;
	typedef ChannelT<uint8_t>	Channel8u;
	template<typename T> 		class SurfaceT;
	typedef SurfaceT<uint8_t>	Surface8u;
	class Shape2d;
} // cinder forward declrations

namespace cinder { namespace text {

class AttrString;
class Manager;
struct ShapingOptions;

struct CI_API GlyphMetrics {
	float	width;
	float	height;
	float	horizontalBearingX;
	float	horizontalBearingY;
	float	horizontalAdvance;
};

class CI_API Font {
  public:
//	Font( Font &&rhs ) = default;
	virtual ~Font();

	Face*		getFace() { return mFace; }
	const Face*	getFace() const { return mFace; }

	//! Text ascender in pixels
	float		getAscender() const { return mAscender; }
	//! Text descender in pixels. Note: negative value
	float		getDescender() const { return mDescender; }
	//! Text height in pixels
	float		getHeight() const { return mHeight; }
	//! Line gap in pixels
  	float		getLineGap() const { return mLineGap; }

  	float					getSize() const { return mSize; }
	const Face::Variation&	getVariation() const { return mVariation; }

	void		lock() const;
	void		unlock() const;
  	
	//! Returns a font-relative index for UTF-32 codepoint \a utf32Char. Returns \c 0 if the font cannot represent \a utf32Char. Passes through to Face::getCharIndex()
	uint32_t		getCharIndex( uint32_t utf32Char ) const { return mFace->getCharIndex( utf32Char ); }
  	
	//! Returns a Channel8u containing the rasterized glyph \a glyphIndex. Note that this index is not a Unicode codepoint, and can be obtained with \a getCharIndex().
	cinder::Channel8u		getGlyphBitmap( uint32_t glyphIndex, int32_t *outOffsetLeft = nullptr, int32_t *outOffsetTop = nullptr ) const;
	cinder::Surface8u		getGlyphBitmapColor( uint32_t glyphIndex, int32_t *outOffsetLeft = nullptr, int32_t *outOffsetTop = nullptr, float *outScale = nullptr ) const;

	//! Returns a Shape2d containing the outline for glyph \a glyphIndex. Note that this index is not a Unicode codepoint, and can be obtained with \a getCharIndex().
	cinder::Shape2d			getGlyphShape( uint32_t glyphIndex ) const;

	hb_font_t*		getHbFont() { return mHbFont.get(); }

	//! Returns string width in pixels of UTF-8 string \a utf8String
	float 			calcStringWidth( const char *utf8String, float tracking = 0 ) const;
	
	////! Returns string width in pixels
	//void 			shapeString( const ShapingOptions &options, const char *utf8String, float tracking, std::vector<uint32_t> *outGlyphIndices, std::vector<uint32_t> *outClusters, std::vector<float> *outGlyphPositions, std::vector<float> *outGlyphAdvances = nullptr, std::vector<float> *outMaxXs = nullptr, float *outPixelWidth = nullptr ) const;
	//! Appends to out* vectors. Returns number of glyphs appended
	size_t 			shapeString( const ShapingOptions &options, const char32_t *utf32String, size_t length, float tracking,
									std::vector<uint32_t> *outGlyphIndices, std::vector<uint32_t> *outClusters = nullptr, std::vector<vec2> *outGlyphPositions = nullptr, std::vector<float> *outGlyphXAdvances = nullptr,
									std::vector<float> *outMaxXs = nullptr, float *outPixelWidth = nullptr ) const;
//	void 			shapeString( const char32_t *utf32String, size_t length, float tracking, uint32_t *outGlyphIndices, float *outGlyphPositions = nullptr, float *outGlyphAdvances = nullptr, float *outPixelWidth = nullptr ) const;
	void			drawGlyphs( size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float baseline, Channel8u &channel ) const;
	void			drawGlyphs( const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float baseline, Surface8u &surface, bool srgb = false ) const;
	//! thread-safe but not thread-efficient
	void			drawGlyphsPrecise( const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float baseline, Surface8u &surface, bool srgb = false ) const;
	void			drawGlyphsPrecise( size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float baseline, Channel8u &channel, bool srgb = false ) const;

	virtual GlyphMetrics	getGlyphMetrics( uint32_t glyphIndex ) const { return mGlyphMetrics[glyphIndex]; }
	virtual float			getGlyphMaxX( uint32_t glyphIndex ) const { return mGlyphMetrics[glyphIndex].horizontalBearingX + mGlyphMetrics[glyphIndex].width; }

	struct CI_API Data {
		virtual ~Data() {}
	};

	void		setRendererData( uint16_t rendererId, Data *data ) const;
	Data*		getRendererData( uint16_t rendererId ) const;

  	void		clearCaches();

  protected:
	Font( Face *face, float size );
	Font( Face *face, float size, const Face::Variation &variation );

	void		cacheMetrics();
	size_t 		shapeBuffer( hb_buffer_t *buf, const std::vector<hb_feature_t> &features, bool ignoreMissing, const std::vector<size_t>& hardBreakIndices, float tracking,
								std::vector<uint32_t> *outGlyphIndices, std::vector<uint32_t> *outClusters, std::vector<vec2> *outGlyphPositions, std::vector<float> *outGlyphXAdvances,
								std::vector<float> *outMaxXs, float *outPixelWidth ) const;

	//! Returns size as 26.6 fixed point
	int32_t		getDiscreteSize() const { return static_cast<int32_t>( mSize * 64 ); }
	
	Face			*mFace;
	FT_Size			mFtSize;
	Face::Variation	mVariation;
	float			mAscender, mDescender, mHeight, mLineGap;
	float			mSize;
	
	std::unique_ptr<hb_font_t,void(*)(hb_font_t*)>	mHbFont;
	
	std::vector<GlyphMetrics>			mGlyphMetrics;

	struct BitmapInfo {
		std::unique_ptr<Channel8u>		mChannel;
		std::unique_ptr<Surface8u>		mSurface;
		int16_t							mOffsetLeft, mOffsetTop;
		float							mScale;
	};
	mutable std::vector<BitmapInfo>		mBitmapCache;

	mutable std::vector<std::pair<uint16_t,std::shared_ptr<Data>>>	mRendererData;

	friend Manager;
};

//! Does not participate in the normal text::Manager system. Caching behavior designed to have size and Variation changed frequently
class CI_API DynamicFont : public Font {
  public:
	static std::unique_ptr<DynamicFont> create( Face *face, float size );

	void		setSize( float size );
	void		setVariation( const Face::Variation &variation );

	virtual GlyphMetrics	getGlyphMetrics( uint32_t glyphIndex ) const override;
	virtual float			getGlyphMaxX( uint32_t glyphIndex ) const override;

  protected:
	DynamicFont( std::unique_ptr<Face> face, float size );

	std::unique_ptr<Face>		mFaceUnique;
};

CI_API std::ostream& operator<<( std::ostream &os, const Font &f );

} } // namespace cinder::text
