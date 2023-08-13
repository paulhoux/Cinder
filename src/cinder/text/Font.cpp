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

#include "cinder/Cinder.h"
#include "cinder/text/Text.h"
#include "cinder/text/Font.h"
#include "cinder/text/Face.h"
#include "cinder/text/AttrString.h"
#include "cinder/Unicode.h"
#include "cinder/Channel.h"
#include "cinder/Surface.h"
#include "cinder/Shape2d.h"
#include "cinder/ip/Fill.h"
#include "cinder/ip/Resize.h"
#include "cinder/ip/Blend.h"

#include <string>

#include <freetype/ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftoutln.h>
#include <freetype/ftsizes.h>
#include <freetype/ftglyph.h>
#include <freetype/ftmm.h>

#include <hb.h>
#include <hb-ft.h>

using namespace std;
using namespace cinder;

namespace cinder { namespace text {

Font::Font( Face *face, float size )
	: Font( face, size, face->getDefaultVariation() )
{}

Font::Font( Face *face, float size, const Face::Variation &variation )
	: mFace( face ), mSize( size ), mVariation( variation ), mHbFont( nullptr, hb_font_destroy )
{
	if( variation.getFace() != face )
		throw InvalidVariationFace();

	FT_New_Size( face->getFtFace(), &mFtSize );
	
	face->lock();
	FT_Activate_Size( mFtSize );
	bool fixedSize = false;
	if( face->hasColor() && ! face->getFixedSizes().empty() ) { // color bitmap
		FT_Select_Size( face->getFtFace(), 0 );
		fixedSize = true;
	}
	else
		FT_Set_Char_Size( face->getFtFace(), (FT_F26Dot6)(0), (FT_F26Dot6)(size * 64), 72, 72 );	

	if( face->hasVariations() )
		FT_Set_Var_Design_Coordinates( face->getFtFace(), (FT_UInt)variation.getFixedValues().size(), (FT_Fixed*)variation.getFixedValues().data() );
	
	if( ! fixedSize ) {
		mAscender = FT_MulFix(face->getFtFace()->ascender, face->getFtFace()->size->metrics.y_scale) / 64.0f;
		mDescender = FT_MulFix(face->getFtFace()->descender, face->getFtFace()->size->metrics.y_scale) / 64.0f;
		mHeight = FT_MulFix(face->getFtFace()->height, face->getFtFace()->size->metrics.y_scale) / 64.0f;
	}
	else {
		float scale = 1.0f;
		mAscender = FT_MulFix(face->getFtFace()->size->metrics.ascender, face->getFtFace()->size->metrics.y_scale) / 64.0f * scale;
		mDescender = FT_MulFix(face->getFtFace()->size->metrics.descender, face->getFtFace()->size->metrics.y_scale) / 64.0f;
		mHeight = FT_MulFix(face->getFtFace()->size->metrics.height, face->getFtFace()->size->metrics.y_scale) / 64.0f;
	}
	mLineGap = mHeight - ( mAscender - mDescender );

	mHbFont = std::unique_ptr<hb_font_t,void(*)(hb_font_t*)>( hb_ft_font_create_referenced( face->getFtFace() ), hb_font_destroy );
	hb_font_set_scale( mHbFont.get(),
		(int)(((uint64_t)face->getFtFace()->size->metrics.x_scale * (uint64_t)face->getFtFace()->units_per_EM + (1u << 15)) >> 16),
		(int)(((uint64_t)face->getFtFace()->size->metrics.y_scale * (uint64_t)face->getFtFace()->units_per_EM + (1u << 15)) >> 16));
	
	cacheMetrics();
	mBitmapCache = std::vector<Font::BitmapInfo>( face->getNumGlyphs() );

	face->unlock();
}

Font::~Font()
{
	if( mHbFont ) {
		FT_Done_Size( mFtSize );
	}
}

void Font::lock() const
{
	mFace->lock();
	FT_Activate_Size( mFtSize );
	if( mFace->hasVariations() )
		FT_Set_Var_Design_Coordinates( mFace->getFtFace(), (FT_UInt)mVariation.getFixedValues().size(), (FT_Fixed*)mVariation.getFixedValues().data() );
}

void Font::unlock() const
{
	mFace->unlock();
}

// assumes font is locked
void Font::cacheMetrics()
{
	for( size_t g = 0; g < mFace->getNumGlyphs(); ++g ) {
		if( FT_Error err = FT_Load_Glyph( mFace->getFtFace(), (FT_UInt)g, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP ) )
			throw text::FreeTypeExc( err );
		auto ftMetrics = mFace->getFtFace()->glyph->metrics;
		mGlyphMetrics.push_back( { FT_MulFix(ftMetrics.width, mFtSize->metrics.x_scale) / 64.0f,
				FT_MulFix(ftMetrics.height, mFtSize->metrics.y_scale) / 64.0f,
				FT_MulFix(ftMetrics.horiBearingX, mFtSize->metrics.x_scale) / 64.0f,
				FT_MulFix(ftMetrics.horiBearingY, mFtSize->metrics.y_scale) / 64.0f,
				FT_MulFix(ftMetrics.horiAdvance, mFtSize->metrics.x_scale) / 64.0f } );
	}
}

inline ci::Channel8u wrapBitmap( const FT_Bitmap &bitmap )
{
	return ci::Channel8u( bitmap.width, bitmap.rows, bitmap.pitch, 1, bitmap.buffer ); 
}

cinder::Channel8u Font::getGlyphBitmap( uint32_t glyphIndex, int32_t *outOffsetLeft, int32_t *outOffsetTop ) const
{
	auto &cached = mBitmapCache[glyphIndex];

	if( ! cached.mChannel ) {
		mFace->lock();
		lock();
		FT_Activate_Size( mFtSize );
		if( FT_Error err = FT_Load_Glyph( mFace->getFtFace(), glyphIndex, FT_LOAD_DEFAULT ) )
			throw text::FreeTypeExc( err );
		if( FT_Error err = FT_Render_Glyph( mFace->getFtFace()->glyph, FT_RENDER_MODE_NORMAL ) )
			throw text::FreeTypeExc( err );

		const FT_Bitmap &ftBitmap = mFace->getFtFace()->glyph->bitmap; 
		cached.mChannel = make_unique<Channel8u>( ftBitmap.width, ftBitmap.rows );
		cached.mChannel->copyFrom( ci::Channel8u( ftBitmap.width, ftBitmap.rows, ftBitmap.pitch, 1, ftBitmap.buffer ), Area( 0, 0, ftBitmap.width, ftBitmap.rows ) );
		cached.mOffsetLeft = mFace->getFtFace()->glyph->bitmap_left;
		cached.mOffsetTop = mFace->getFtFace()->glyph->bitmap_top;
		unlock();
		mFace->unlock();
	}

	if( outOffsetLeft )
		*outOffsetLeft = cached.mOffsetLeft;
	if( outOffsetTop )
		*outOffsetTop = cached.mOffsetTop;
	
	return ci::Channel8u( cached.mChannel->getWidth(), cached.mChannel->getHeight(), cached.mChannel->getRowBytes(), 1, cached.mChannel->getData() );
}

cinder::Surface8u Font::getGlyphBitmapColor( uint32_t glyphIndex, int32_t *outOffsetLeft, int32_t *outOffsetTop, float *outScale ) const
{
	auto &cached = mBitmapCache[glyphIndex];

	if( ! cached.mChannel && ! cached.mSurface ) {
		mFace->lock();
		FT_Activate_Size( mFtSize );
		if( mFace->hasColor() ) {
			if( FT_Error err = FT_Load_Glyph( mFace->getFtFace(), glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_COLOR | FT_LOAD_NO_HINTING ) )
				throw text::FreeTypeExc( err );
			/*if( FT_Error err = FT_Render_Glyph( mFace->getFtFace()->glyph, FT_RENDER_MODE_NORMAL ) )
				throw text::FreeTypeExc( err );*/

			const FT_Bitmap &ftBitmap = mFace->getFtFace()->glyph->bitmap;
			Surface8u tempSurface( ftBitmap.buffer, ftBitmap.width, ftBitmap.rows, ftBitmap.pitch, SurfaceChannelOrder::BGRA );
			if( true ) {
				float aspect = ftBitmap.width / (float)ftBitmap.rows;
				float scale = getSize() / (float)mFace->getFixedSizes()[0];
				int32_t height = static_cast<int32_t>( getSize() );
				int32_t width = static_cast<int32_t>( height * aspect + 0.5f );
				cached.mSurface = make_unique<Surface8u>( width, height, true, SurfaceChannelOrder::BGRA );
				ip::resize( tempSurface, cached.mSurface.get() );
				cached.mOffsetLeft = static_cast<int16_t>( mFace->getFtFace()->glyph->bitmap_left * scale );
				cached.mOffsetTop = static_cast<int16_t>( mFace->getFtFace()->glyph->bitmap_top * scale );
				cached.mScale = scale;
			}
			else {
				cached.mSurface = make_unique<Surface8u>( ftBitmap.width, ftBitmap.rows, true, SurfaceChannelOrder::BGRA );
				cached.mSurface->copyFrom( tempSurface, Area( 0, 0, ftBitmap.width, ftBitmap.rows ) );
				cached.mOffsetLeft = mFace->getFtFace()->glyph->bitmap_left;
				cached.mOffsetTop = mFace->getFtFace()->glyph->bitmap_top;
				cached.mScale = 1.0f;
			}
			cached.mSurface->setPremultiplied( true );
		}

		mFace->unlock();
	}

	if( outOffsetLeft )
		*outOffsetLeft = cached.mOffsetLeft;
	if( outOffsetTop )
		*outOffsetTop = cached.mOffsetTop;
	if( outScale )
		*outScale = cached.mScale;

	ci::Surface8u result( cached.mSurface->getData(), cached.mSurface->getWidth(), cached.mSurface->getHeight(), cached.mSurface->getRowBytes(), SurfaceChannelOrder::BGRA );
	result.setPremultiplied();
	return result;
}

static int ftShape2dMoveTo(const FT_Vector *to, void *user)
{
	Shape2d *shape = reinterpret_cast<Shape2d*>(user);
	shape->moveTo((float)to->x / 4096.f, (float)to->y / 4096.f);
	return 0;
}

static int ftShape2dLineTo(const FT_Vector *to, void *user)
{
	Shape2d *shape = reinterpret_cast<Shape2d*>(user);
	shape->lineTo((float)to->x / 4096.f, (float)to->y / 4096.f);
	return 0;
}

static int ftShape2dConicTo(const FT_Vector *control, const FT_Vector *to, void *user)
{
	Shape2d *shape = reinterpret_cast<Shape2d*>(user);
	shape->quadTo((float)control->x / 4096.f, (float)control->y / 4096.f, (float)to->x / 4096.f, (float)to->y / 4096.f);
	return 0;
}

static int ftShape2dCubicTo(const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user)
{
	Shape2d *shape = reinterpret_cast<Shape2d*>(user);
	shape->curveTo((float)control1->x / 4096.f, (float)control1->y / 4096.f, (float)control2->x / 4096.f, (float)control2->y / 4096.f, (float)to->x / 4096.f, (float)to->y / 4096.f);
	return 0;
}

cinder::Shape2d	Font::getGlyphShape( uint32_t glyphIndex ) const
{
	mFace->lock();
	FT_Activate_Size( mFtSize );

	FT_Load_Glyph( mFace->getFtFace(), glyphIndex, FT_LOAD_DEFAULT );
	FT_Outline outline = mFace->getFtFace()->glyph->outline;
	FT_Outline_Funcs funcs;
	funcs.move_to = ftShape2dMoveTo;
	funcs.line_to = ftShape2dLineTo;
	funcs.conic_to = ftShape2dConicTo;
	funcs.cubic_to = ftShape2dCubicTo;
	funcs.shift = 6;
	funcs.delta = 0;

	Shape2d result;
	FT_Outline_Decompose( &outline, &funcs, &result );
	if( result.getNumContours() )
		result.close();
	result.scale(vec2(1, -1));

	mFace->unlock();
	
	return result;
}

float Font::calcStringWidth( const char *utf8String, float tracking ) const
{
	hb_buffer_t *buf = hb_buffer_create();

	// Set buffer to LTR direction, common script and default language
	hb_buffer_set_direction( buf, HB_DIRECTION_LTR );
	hb_buffer_set_script( buf, HB_SCRIPT_COMMON );
	hb_buffer_set_language( buf, hb_language_get_default() );

	// Add text and layout it
	hb_buffer_add_utf8( buf, utf8String, -1, 0, -1 );

	mFace->lock();
	FT_Activate_Size( mFtSize );
	hb_shape( mHbFont.get(), buf, nullptr, 0 );
	mFace->unlock();

	// Get buffer data
	unsigned int        glyphCount = hb_buffer_get_length( buf );
	hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(buf, NULL);

	double stringWidthPx = 0;
	for( unsigned int i = 0; i < glyphCount; ++i ) {
		stringWidthPx += glyphPos[i].x_advance / 64.0;
		stringWidthPx += tracking;
	}
	if( glyphCount > 1 )
		stringWidthPx -= tracking;
	
	hb_buffer_destroy( buf );
	
	return (float)stringWidthPx;
}

namespace {
std::unique_ptr<hb_buffer_t,void(*)(hb_buffer_t*)> createBuffer( const ShapingOptions &options )
{
	hb_buffer_t *buf = hb_buffer_create();

	// Set buffer to LTR direction, common script and default language
	hb_buffer_set_direction( buf, HB_DIRECTION_LTR );
	hb_buffer_set_script( buf, HB_SCRIPT_COMMON );
	hb_buffer_set_language( buf, hb_language_get_default() );

	return unique_ptr<hb_buffer_t,void(*)(hb_buffer_t*)>( buf, hb_buffer_destroy );
}

std::vector<hb_feature_t> createFeatures( const ShapingOptions &options )
{
	std::vector<hb_feature_t> result;
	for( auto feature : options.getFeatures() )
		result.push_back( { feature.first, (uint32_t)feature.second, HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END } );

	return result;
}

std::vector<size_t> findHardBreaks( const char32_t *utf32String, size_t length )
{
	std::vector<size_t> result;

	for( size_t i = 0; i < length; ++i )
		if( utf32String[i] == '\n' )
			result.push_back( i );

	return result;
}
}

/*void Font::shapeString( const ShapingOptions &options, const char *utf8String, float tracking, vector<uint32_t> *outGlyphIndices, vector<uint32_t> *outClusters, vector<float> *outGlyphPositions, vector<float> *outGlyphAdvances, vector<float> *outMaxXs, float *outPixelWidth ) const
{
	auto buf = createBuffer( options );
	auto features = createFeatures( options );
	hb_buffer_add_utf8( buf.get(), utf8String, -1, 0, -1 );
	shapeBuffer( buf.get(), features, options.getIgnoreMissingGlyphs(), tracking, outGlyphIndices, outClusters, outGlyphPositions, outGlyphAdvances, outMaxXs, outPixelWidth );
}*/

size_t Font::shapeString( const ShapingOptions &options, const char *utf8String, size_t length, float tracking, std::vector<uint32_t> *outGlyphIndices, std::vector<uint32_t> *outClusters, std::vector<vec2> *outGlyphPositions, std::vector<float> *outGlyphXAdvances,
								std::vector<float> *outMaxXs, float *outPixelWidth ) const
{
	u32string u32 = toUtf32( utf8String, length );
	return shapeString( options, u32.data(), u32.length(), tracking, outGlyphIndices, outClusters, outGlyphPositions, outGlyphXAdvances, outMaxXs, outPixelWidth );
}


size_t Font::shapeString( const ShapingOptions &options, const char32_t *utf32String, size_t length, float tracking, vector<uint32_t> *outGlyphIndices, std::vector<uint32_t> *outClusters, vector<vec2> *outGlyphPositions, vector<float> *outGlyphXAdvances, vector<float> *outMaxXs, float *outPixelWidth ) const
{
	auto buf = createBuffer( options );
	auto features = createFeatures( options );
	vector<size_t> hardBreakIndices = options.getIgnoreMissingGlyphs() ? findHardBreaks( utf32String, length ) : std::vector<size_t>();
	hb_buffer_add_utf32( buf.get(), (const uint32_t*)utf32String, (int)length, 0, -1 );
	return shapeBuffer( buf.get(), features, options.getIgnoreMissingGlyphs(), hardBreakIndices, tracking, outGlyphIndices, outClusters, outGlyphPositions, outGlyphXAdvances, outMaxXs, outPixelWidth );
}

size_t Font::shapeBuffer( hb_buffer_t *buf, const vector<hb_feature_t> &features, bool ignoreMissing, const vector<size_t> &hardBreakIndices, float tracking,
							vector<uint32_t> *outGlyphIndices, vector<uint32_t> *outClusters, vector<vec2> *outGlyphPositions, vector<float> *outGlyphXAdvances, vector<float> *outMaxXs, float *outPixelWidth ) const
{
	lock();
	//FT_Activate_Size( mFtSize );
	hb_shape( mHbFont.get(), buf, features.data(), (unsigned int)features.size() );
	const uint32_t missingGlyph = 0;

	// Get buffer data
	unsigned int glyphCount = hb_buffer_get_length( buf );
	size_t result = ignoreMissing ? 0 : (size_t)glyphCount; // will be counted manually if 'ignoreMissing'
	const hb_glyph_info_t *glyph_infos = nullptr;
	size_t hardBreakIndicesIdx = 0;
	if( outGlyphIndices || outClusters || outMaxXs || ignoreMissing ) {
		glyph_infos = hb_buffer_get_glyph_infos( buf, nullptr );
		for( unsigned int g = 0; g < glyphCount; ++g ) {
			bool missing = ignoreMissing && glyph_infos[g].codepoint == missingGlyph;
			if( missing && hardBreakIndicesIdx < hardBreakIndices.size() && hardBreakIndices[hardBreakIndicesIdx] == glyph_infos[g].cluster ) {
				missing = false;
				++hardBreakIndicesIdx;
			}
			if( outGlyphIndices )
				if( ! missing )
					(*outGlyphIndices).push_back( glyph_infos[g].codepoint );
			if( outClusters )
				if( ! missing )
					(*outClusters).push_back( glyph_infos[g].cluster );
			if( outMaxXs )
				if( ! missing )
					(*outMaxXs).push_back( getGlyphMaxX( glyph_infos[g].codepoint ) );
			if( ignoreMissing && ! missing )
				++result;
		}
	} 

	hardBreakIndicesIdx = 0;
	if( outGlyphPositions || outGlyphXAdvances || outPixelWidth ) {
		const hb_glyph_position_t *glyph_positions = hb_buffer_get_glyph_positions( buf, nullptr );
		double penX = 0, penY = 0;
		for( unsigned int g = 0; g < glyphCount; ++g ) {
			bool missing = ignoreMissing && glyph_infos[g].codepoint == missingGlyph;
			if( missing && hardBreakIndicesIdx < hardBreakIndices.size() && hardBreakIndices[hardBreakIndicesIdx] == glyph_infos[g].cluster ) {
				missing = false;
				++hardBreakIndicesIdx;
			}
			if( outGlyphPositions )
				if( ! missing )
					(*outGlyphPositions).emplace_back( (float)penX + glyph_positions[g].x_offset / 64.0f, (float)penY + glyph_positions[g].y_offset / 64.0f );
			if( outGlyphXAdvances )
				if( ! missing )
					(*outGlyphXAdvances).push_back( glyph_positions[g].x_advance / 64.0f + tracking );
			if( ! missing ) {
				penX += glyph_positions[g].x_advance / 64.0;
				penX += (double)tracking;
				penY += glyph_positions[g].y_advance / 64.0;
			}
		}
		penX -= ( glyphCount > 1 ) ? tracking : 0;
		
		if( outPixelWidth )
			*outPixelWidth = (float)penX;
	}

	unlock();

	return result;
}

void Font::drawGlyphs( size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float baseline, Channel8u &channel ) const
{
	for( size_t i = 0; i < len; ++i ) {
		try {
			int32_t offsetLeft, offsetTop;
			Channel8u glyph = getGlyphBitmap( glyphIndices[i], &offsetLeft, &offsetTop );
			ip::blend( &channel, glyph, glyph.getBounds(), ivec2( (int32_t)(penX + glyphPositions[i].x + 0.5f), (int32_t)(glyphPositions[i].y + baseline - offsetTop + 0.5f) ) - ivec2( -offsetLeft, 0 ) );
		}
		catch( ... ) { // getGlyphBitmap() will throw on missing glyph
		}
	}
}

void Font::drawGlyphs( const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float baseline, Surface8u &surface, bool srgb ) const
{
	for( size_t i = 0; i < len; ++i ) {
		try {
			int32_t offsetLeft, offsetTop;
			if( ! getFace()->hasColor() ) {
				Channel8u glyph = getGlyphBitmap( glyphIndices[i], &offsetLeft, &offsetTop );
				if( srgb )
					ip::blendColorSrgb( &surface, color, glyph, glyph.getBounds(), ivec2( (int32_t)(penX + glyphPositions[i].x + 0.5f), (int32_t)(baseline + glyphPositions[i].y - offsetTop + 0.5f) ) - ivec2( -offsetLeft, 0 ) );					
				else
					ip::blendColor( &surface, color, glyph, glyph.getBounds(), ivec2( (int32_t)(penX + glyphPositions[i].x + 0.5f), (int32_t)(baseline + glyphPositions[i].y - offsetTop + 0.5f) ) - ivec2( -offsetLeft, 0 ) );
			}
			else {
				float scale;
				Surface8u glyph = getGlyphBitmapColor( glyphIndices[i], &offsetLeft, &offsetTop, &scale );
				ip::blend( &surface, glyph, glyph.getBounds(), ivec2( (int32_t)(penX + glyphPositions[i].x + 0.5f), (int32_t)(baseline + glyphPositions[i].y - offsetTop + 0.5f) ) - ivec2( -offsetLeft, 0 ) );
				//surface.copyFrom( glyph, glyph.getBounds(), ivec2( penX, baseline - glyph.getHeight()/*offsetTop*/ ) - ivec2( -offsetLeft, 0 ) );
			}
		}
		catch ( ... ) { // getGlyphBitmap() will throw on missing glyph
		}
	}
}

void Font::drawGlyphsPrecise( const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], const vec2 glyphOrientations[], float penX, float baseline, Surface8u &surface, bool srgb ) const
{
	mFace->lock();
	lock();
	FT_Activate_Size( mFtSize );
	for( size_t i = 0; i < len; ++i ) {
		float intPenX;
		float fracX = modff( penX + glyphPositions[i].x, &intPenX );
		float intBaseline;
		float fracY = modff( baseline + glyphPositions[i].y, &intBaseline );
		FT_Vector offset = { (int)(fracX * 64), (int)(-fracY * 64) };
		if( glyphOrientations ) {
			FT_Matrix matrix;
			matrix.xx = -(FT_Fixed)(glyphOrientations[i].y * 65536.0f);
			matrix.xy = (FT_Fixed)(glyphOrientations[i].x * 65536.0f);
			matrix.yx = -(FT_Fixed)(glyphOrientations[i].x * 65536.0f);
			matrix.yy = -(FT_Fixed)(glyphOrientations[i].y * 65536.0f);
			FT_Set_Transform( mFace->getFtFace(), &matrix, &offset );
		}
		else
			FT_Set_Transform( mFace->getFtFace(), nullptr, &offset );
		if( FT_Error err = FT_Load_Glyph( mFace->getFtFace(), glyphIndices[i], FT_LOAD_DEFAULT ) )
			throw text::FreeTypeExc( err );
		if( FT_Error err = FT_Render_Glyph( mFace->getFtFace()->glyph, FT_RENDER_MODE_NORMAL ) )
			throw text::FreeTypeExc( err );

		auto channel = wrapBitmap( mFace->getFtFace()->glyph->bitmap );
		if( srgb )
			ip::blendColorSrgb( &surface, color, channel, channel.getBounds(), ivec2( (int32_t)intPenX, (int32_t)intBaseline - mFace->getFtFace()->glyph->bitmap_top ) - ivec2( -mFace->getFtFace()->glyph->bitmap_left, 0 ) );
		else
			ip::blendColor( &surface, color, channel, channel.getBounds(), ivec2( (int32_t)intPenX, (int32_t)intBaseline - mFace->getFtFace()->glyph->bitmap_top ) - ivec2( -mFace->getFtFace()->glyph->bitmap_left, 0 ) );
	}
	FT_Set_Transform( mFace->getFtFace(), nullptr, nullptr ); // reset transform
	unlock();
	mFace->unlock();
}

void Font::drawGlyphsPrecise( size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], const vec2 glyphOrientations[], float penX, float baseline, Channel8u &channel, bool srgb ) const
{
	mFace->lock();
	lock();
	FT_Activate_Size( mFtSize );
	for( size_t i = 0; i < len; ++i ) {
		float intPenX;
		float fracX = modff( penX + glyphPositions[i].x, &intPenX );
		float intBaseline;
		float fracY = modff( baseline + glyphPositions[i].y, &intBaseline );
		FT_Vector offset = { (int)(fracX * 64), (int)(-fracY * 64) };
		if( glyphOrientations ) {
			FT_Matrix matrix;
			matrix.xx = (FT_Fixed)(glyphOrientations[i].y * 65536.0f);
			matrix.xy = (FT_Fixed)(-glyphOrientations[i].x * 65536.0f);
			matrix.yx = (FT_Fixed)(glyphOrientations[i].x * 65536.0f);
			matrix.yy = (FT_Fixed)(glyphOrientations[i].y * 65536.0f);
			FT_Set_Transform( mFace->getFtFace(), &matrix, &offset );
		}
			FT_Set_Transform( mFace->getFtFace(), nullptr, &offset );
		if( FT_Error err = FT_Load_Glyph( mFace->getFtFace(), glyphIndices[i], FT_LOAD_DEFAULT ) )
			throw text::FreeTypeExc( err );
		if( FT_Error err = FT_Render_Glyph( mFace->getFtFace()->glyph, FT_RENDER_MODE_NORMAL ) )
			throw text::FreeTypeExc( err );

		auto glyphChannel = wrapBitmap( mFace->getFtFace()->glyph->bitmap );
		ip::blend( &channel, glyphChannel, glyphChannel.getBounds(), ivec2( (int32_t)intPenX, (int32_t)intBaseline - mFace->getFtFace()->glyph->bitmap_top ) - ivec2( -mFace->getFtFace()->glyph->bitmap_left, 0 ) );
	}
	FT_Set_Transform( mFace->getFtFace(), nullptr, nullptr ); // reset transform
	unlock();
	mFace->unlock();
}

void Font::setRendererData( uint16_t rendererId, Font::Data *data ) const
{
	for( auto &r : mRendererData )
		if( r.first == rendererId ) {
			r.second = std::shared_ptr<Font::Data>( data );
			return;
		}

	mRendererData.push_back( std::make_pair( rendererId, std::shared_ptr<Font::Data>( data ) ) );
}

Font::Data* Font::getRendererData( uint16_t rendererId ) const
{
	for( auto &r : mRendererData )
		if( r.first == rendererId )
			return r.second.get();

	return nullptr;
}

std::ostream& operator<<( std::ostream& os, const Font& f )
{
	os << f.getFace()->getFamilyName() << " @ " << f.getSize();
	return os;
}

void Font::clearCaches()
{
	for( auto &c : mBitmapCache ) {
		c.mChannel.reset();
		c.mSurface.reset();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DynamicFont
DynamicFont::DynamicFont( std::unique_ptr<Face> face, float size )
	: Font( face.get(), size ), mFaceUnique( std::move(face) )
{
}

std::unique_ptr<DynamicFont> DynamicFont::create( Face *face, float size )
{
	return std::unique_ptr<DynamicFont>( new DynamicFont( std::move(std::unique_ptr<Face>( face->clone() )), size ) );
}

void DynamicFont::setSize( float size )
{
	mSize = size;
	FT_Activate_Size( mFtSize );
	FT_Set_Char_Size( mFace->getFtFace(), (FT_F26Dot6)(0), (FT_F26Dot6)(mSize * 64), 72, 72 );
	hb_ft_font_changed( mHbFont.get() );
	clearCaches();
}

void DynamicFont::setVariation( const Face::Variation &variation )
{
	if( mFace->hasVariations() ) {
		mVariation = variation;
		FT_Set_Var_Design_Coordinates( mFace->getFtFace(), (FT_UInt)mVariation.getFixedValues().size(), (FT_Fixed*)mVariation.getFixedValues().data() );
		hb_ft_font_changed( mHbFont.get() );
		clearCaches();
	}
}

GlyphMetrics DynamicFont::getGlyphMetrics( uint32_t glyphIndex ) const
{
	if( FT_Error err = FT_Load_Glyph( mFace->getFtFace(), (FT_UInt)glyphIndex, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP ) )
		throw text::FreeTypeExc( err );

	auto ftMetrics = mFace->getFtFace()->glyph->metrics;
	return GlyphMetrics( { FT_MulFix(ftMetrics.width, mFtSize->metrics.x_scale) / 64.0f,
							FT_MulFix(ftMetrics.height, mFtSize->metrics.y_scale) / 64.0f,
							FT_MulFix(ftMetrics.horiBearingX, mFtSize->metrics.x_scale) / 64.0f,
							FT_MulFix(ftMetrics.horiBearingY, mFtSize->metrics.y_scale) / 64.0f,
							FT_MulFix(ftMetrics.horiAdvance, mFtSize->metrics.x_scale) / 64.0f } );
}

float DynamicFont::getGlyphMaxX( uint32_t glyphIndex ) const
{
	if( FT_Error err = FT_Load_Glyph( mFace->getFtFace(), (FT_UInt)glyphIndex, FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP ) )
		throw text::FreeTypeExc( err );

	auto ftMetrics = mFace->getFtFace()->glyph->metrics;
	return FT_MulFix(ftMetrics.horiBearingX, mFtSize->metrics.x_scale) / 64.0f + FT_MulFix(ftMetrics.width, mFtSize->metrics.x_scale) / 64.0f;
}


} } // namespace cinder::text
