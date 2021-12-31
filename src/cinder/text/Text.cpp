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
#include "cinder/Filesystem.h"
#include "cinder/Buffer.h"
#include "cinder/text/Text.h"
#include "cinder/text/SystemFonts.h"
#include "cinder/text/AttrString.h"
#include "cinder/Utilities.h"
#include "cinder/Channel.h"
#include "cinder/ImageIo.h"
#include "cinder/ip/Fill.h"
#include "cinder/ip/Blend.h"
#include "cinder/Log.h"

#include <memory>

#include <freetype/ft2build.h>
#include <freetype/freetype.h>

#include "linebreak.h"
#include "wordbreak.h"

using namespace std;

static constexpr uint16_t SOFTWARE_RENDERER_ID = 0x01; 

namespace cinder { namespace text {

Manager* Manager::get()
{
	static Manager instance;
	return &instance;
}

Manager::Manager()
{
	mLibraryPtr = make_unique<FT_Library>();
	FT_Init_FreeType( mLibraryPtr.get() );
}

Manager::~Manager()
{
	mFonts.clear();
	mFaces.clear();
	FT_Done_FreeType( *mLibraryPtr );
}

Face* Manager::loadFace( const DataSourceRef &dataSource, int faceIndex )
{
//	if( dataSource->isFilePath() )
//		return loadFace( dataSource->getFilePath(), faceIndex );

	auto tempBuffer = dataSource->getBuffer();
	auto result = loadFace( tempBuffer->getData(), tempBuffer->getSize(), faceIndex );
	if( result )
		result->setFilePath( dataSource->getFilePath(), faceIndex );

	return result;
}

Face* Manager::loadFace( const void *data, size_t dataSize, int faceIndex )
{
	FT_Face ftFace;
	auto buffer = Buffer::create( dataSize );
	buffer->copyFrom( data, dataSize );
	FT_Error error = FT_New_Memory_Face( *mLibraryPtr, (const FT_Byte*)buffer->getData(), (FT_Long)buffer->getSize(), faceIndex, &ftFace );
	if( error )
	 	throw FreeTypeExc( error );
	
	Face *result;
	{
		lock_guard<mutex> lock( mFaceMutex );
		mFaces.emplace_back( new Face( ftFace ) );
		mFaces.back()->setFilePath( "", faceIndex );
		mFaces.back()->mDataBuffer = buffer;
		result = mFaces.back().get(); 
	}
	
	return result;
}

Face* Manager::loadFace( const ci::fs::path &path, int faceIndex )
{
	FT_Face ftFace;
	FT_Error error = FT_New_Face( *mLibraryPtr, path.string().c_str(), faceIndex, &ftFace );
	if( error )
	 	throw FreeTypeExc( error );
	
	Face *result;
	{
		lock_guard<mutex> lock( mFaceMutex );
		mFaces.emplace_back( new Face( ftFace ) );
		mFaces.back()->setFilePath( path, faceIndex );
		result = mFaces.back().get(); 
	}
	
	return result;
}

Face* Manager::loadSystemFace( const std::string &name )
{
	auto found = findFace( name );
	if( found )
		return found;

	auto data = loadSystemFaceData( name );
	if( ! data )
		return nullptr;
	else {
		auto result = loadFace( data->getData(), data->getSize(), 0 );
		result->mSystemFace = true;
		return result;
	}
}

Face* Manager::systemDefaultFace()
{
	lock_guard<mutex> lock( mSystemDefaultFaceMutex );
	if( ! mSystemDefaultFace ) {
		if( auto data = loadSystemDefaultFaceData() ) {
			mSystemDefaultFace = loadFace( data->getData(), data->getSize(), 0 );
			mSystemDefaultFace->mSystemFace = true;
		}
	}

	return mSystemDefaultFace;
}

Face* Manager::findFace( const std::string &name ) const
{
	lock_guard<mutex> lock( mFaceMutex );
	for( auto &face : mFaces ) {
		if( asciiCaseCmp( face->getFamilyName().c_str(), name.c_str() ) == 0 )
			return face.get();
	}

	return nullptr;
}

Font* Manager::findFont( const Face *face, float size ) const
{
	return findFont( face, size, face->getDefaultVariation() );
}

Font* Manager::findFont( const Face *face, float size, const Face::Variation &variation ) const
{
	int32_t discreteSize = static_cast<int32_t>( size * 64 ); // 26.6 fixed point
	for( const auto &font : mFonts )
		if( font->getFace() == face && font->getDiscreteSize() == discreteSize && font->getVariation() == variation )
		   	return font.get();

	return nullptr;
}

Font* Manager::loadFont( Face *face, float size )
{
	if( face ) {
		return loadFont( face, size, face->getDefaultVariation() );
	}
	return nullptr;
}

Font* Manager::loadFont( Face *face, float size, const Face::Variation &variation )
{
	Font *result;
	result = findFont( face, size, variation );
	if( ! result ) { // couldn't find a match - create a new Font
		lock_guard<mutex> lock( mFaceMutex );
		mFonts.emplace_back( new Font( face, size, variation ) );
		result = mFonts.back().get(); 
	}	
	
	return result;	
}

//! Returns a list of face names installed in the system which can be used in loadSystemFace() 
std::vector<std::string> Manager::getSystemFaceNames()
{
	return copySystemFaceNames();
}

///////////////////////////////////////////////////////////////////////////////////////////
// free functions
Font* font( const std::string &name, float size )
{
	Face *face = Manager::get()->findFace( name );
	if( ! face )
		face = loadSystemFace( name );
	if( face )
		return loadFont( face, size );
	else
		return loadFont( systemDefaultFace(), size );
}

Font* font( const std::vector<std::pair<std::string,float>> &fonts )
{
	for( auto &tryFont : fonts ) {
		Face *face = Manager::get()->findFace( tryFont.first ); // try to find among loaded
		if( ! face )
			face = loadSystemFace( tryFont.first );
		if( face )
			return loadFont( face, tryFont.second );
	}

	if( fonts.empty() )
		return loadFont( systemDefaultFace(), 12 );
	else
		return loadFont( systemDefaultFace(), fonts.front().second );
}

Channel8u renderString( const Font *font, const char *utf8String, float tracking )
{ 
	return font->renderString( utf8String, tracking );
}

void measureString( const AttrString& attrString, float *resultWidth, float *resultHeight, float *resultBaseline )
{
	ShapingOptions defaultShapingOptions{};
	float measuredWidth = 0, measuredHeight = 0, measuredBaseline = 0;
	auto runIt = attrString.iterate( TypesetOptions().getDefaultFont() );
	while( runIt.nextRun() ) {
		float glyphsWidth;
		float tracking = runIt.getTracking();
		runIt.getFont()->shapeString( runIt.getShapingOptions( defaultShapingOptions ), runIt.getStrPtr(), runIt.getLengthCh(), tracking, nullptr, nullptr, nullptr, nullptr, nullptr, &glyphsWidth );
		measuredWidth += glyphsWidth;
		measuredHeight = std::max<float>( measuredHeight, runIt.getFont()->getHeight() );
		measuredBaseline = std::max<float>( measuredBaseline, runIt.getFont()->getAscender() );
	}
	
	if( resultWidth )
	 	*resultWidth = measuredWidth;
	if( resultHeight )
		*resultHeight = measuredHeight;
	if( resultBaseline )
		*resultBaseline = measuredBaseline;
}

void drawRun( const Font *font, size_t len, const uint32_t glyphIndices[], const float glyphAdvances[], float penX, float baseline, Channel8u &channel )
{
	font->lock();
	for( size_t i = 0; i < len; ++i ) {
		try {
			int32_t offsetLeft, offsetTop;
			Channel8u glyph = font->getGlyphBitmap( glyphIndices[i], &offsetLeft, &offsetTop );
			ip::blend( &channel, glyph, glyph.getBounds(), ivec2( penX, baseline - offsetTop ) - ivec2( -offsetLeft, 0 ) );
		}
		catch( ... ) { // getGlyphBitmap() will throw on missing glyph
		}
		penX += glyphAdvances[i];
	}
	font->unlock();
}

void drawRun( const Font *font, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const float glyphAdvances[], float penX, float baseline, Surface8u &surface )
{
	font->lock();
	for( size_t i = 0; i < len; ++i ) {
		try {
			int32_t offsetLeft, offsetTop;
			if( ! font->getFace()->hasColor() ) {
				Channel8u glyph = font->getGlyphBitmap( glyphIndices[i], &offsetLeft, &offsetTop );
				ip::blendColor( &surface, color, glyph, glyph.getBounds(), ivec2( penX, baseline - offsetTop ) - ivec2( -offsetLeft, 0 ) );
				penX += glyphAdvances[i];
			}
			else {
				float scale;
				Surface8u glyph = font->getGlyphBitmapColor( glyphIndices[i], &offsetLeft, &offsetTop, &scale );
				ip::blend( &surface, glyph, glyph.getBounds(), ivec2( penX, baseline - glyph.getHeight()/*offsetTop*/ ) - ivec2( -offsetLeft, 0 ) );
				//surface.copyFrom( glyph, glyph.getBounds(), ivec2( penX, baseline - glyph.getHeight()/*offsetTop*/ ) - ivec2( -offsetLeft, 0 ) );
				penX += glyphAdvances[i] * scale;
			}
		}
		catch ( ... ) { // getGlyphBitmap() will throw on missing glyph
		}
	}
	font->unlock();
}

bool mustBreak( const char breaks[], size_t offset )
{
	return breaks[offset] == 0;
}

// if cannot find break before \a runStart returns \c false and *runPos unchanged
bool findCanBreak( const char breaks[], const uint32_t clusters[], size_t glyphStart, size_t *glyphPos )
{
	size_t originalGlyphPos = *glyphPos;

	while( *glyphPos > glyphStart ) {
		if( breaks[clusters[*glyphPos]] == 0 || breaks[clusters[*glyphPos]] == 1 || breaks[clusters[*glyphPos]] == 4 )
			return true;
		else
			--*glyphPos;
	}

	*glyphPos = originalGlyphPos;
	return false;
}

// *glyphPos points to the next can or must break, or the last glyph
bool findNextWord( const char breaks[], const uint32_t clusters[], size_t totalGlyphs, size_t *glyphPos )
{
	while( *glyphPos < totalGlyphs ) {
		if( breaks[clusters[*glyphPos]] == 0 /* must */ || breaks[clusters[*glyphPos]] == 1 /* can */ || breaks[clusters[*glyphPos]] == 4 /* indet */ )
			return true;
		else
			++*glyphPos;
	}

	return false;
}

// *glyphPos -> index of first] glyph which is not whitespace
void findNonWhitespace( const char32_t *chars, const uint32_t clusters[], size_t lenCh, size_t *glyphPos )
{
	while( clusters[*glyphPos] < lenCh ) {
		char32_t ch = chars[clusters[*glyphPos]];
		if( ch == 0x20 || ch == 0xA0 || ( ch >= 0x2000 && ch <= 0x200A ) )
			++*glyphPos;
		else
			return;
	}
}

struct RunData {
	const Font *font;
	Alignment alignment;
	Leading leading;
	ColorAf color;
	size_t start, len;
	size_t startCh;
};

struct SpanData {
	size_t glyphStart, glyphLen;
	size_t chStart, chLen;
	float drawOffset;
	float measuredWidth;
};

// draws runs and advances baseline accordingly. If \a firstLine then we only increase baseline by ascender
void processLine( const vector<pair<vector<RunData>::iterator,SpanData>> &spans, std::vector<uint32_t> glyphIndices, std::vector<float> glyphAdvances,
						bool *firstLine, Alignment justification, int maxWidth, float *baseline, TypesetProcessor &fn, const TypesetOptions &options )
{
	if( spans.empty() )
		return;

	float maxAscent = 0, maxLineHeight = 0, maxDescent = 0, maxLineGap = 0;
	for( auto &span : spans ) {
		maxAscent = std::max( maxAscent, span.first->font->getAscender() );
		maxDescent = std::max( maxDescent, -span.first->font->getDescender() );
		maxLineGap = std::max( maxLineGap, span.first->font->getLineGap() );
		float baseHeight = options.getIgnoreLineMetrics() ? span.first->font->getSize() : span.first->font->getHeight();
		maxLineHeight = std::max( maxLineHeight, span.first->leading.getLineHeight( baseHeight ) );
		//maxLineHeight = std::max( maxLineHeight, run.font->getSize() );
	}

	if( *firstLine ) { // first line moves baseline down by max ascent
		*firstLine = false;
		*baseline = maxAscent + options.getTopLineOffset();
	}
	else { // other lines move down by max( prev max line height, 
		*baseline += maxLineHeight;
	}

	float measuredLineWidth = spans.back().second.drawOffset + spans.back().second.measuredWidth;	
	float justificationOffset = 0;
	if( maxWidth != std::numeric_limits<int32_t>::max() ) {
		if( justification == Alignment::RIGHT ) justificationOffset = maxWidth - measuredLineWidth;
		else if( justification == Alignment::CENTER ) justificationOffset = ( maxWidth - measuredLineWidth ) / 2.0f;
	}

	fn.addLine( justification, vec2( justificationOffset, *baseline ), maxAscent, maxDescent, maxLineGap, measuredLineWidth );
	for( auto &span : spans ) {
		if( span.first->len && span.second.glyphLen )
			fn.addRun( span.first->font, span.second.chStart, span.second.chLen, span.first->color, span.second.glyphLen, &glyphIndices[span.second.glyphStart], &glyphAdvances[span.second.glyphStart], span.second.drawOffset, span.second.measuredWidth );
	}
	fn.finishLine();
}

Channel8u renderString( const AttrString &attrString )
{
	std::vector<uint32_t> glyphIndices;
	std::vector<float> glyphAdvances;

	float resultWidthF, resultHeightF, baseline;
	measureString( attrString, &resultWidthF, &resultHeightF, &baseline );
	int resultWidth = (int)ceil( resultWidthF );
	int resultHeight = (int)ceil( resultHeightF );
	
	Channel8u result( resultWidth, resultHeight );
	ip::fill( &result, (uint8_t)0 );
	
	auto runIt = attrString.iterate( TypesetOptions().getDefaultFont() );
	float penX = 0, runWidth;
	while( runIt.nextRun() ) {
		runIt.shape( runIt.getShapingOptions( ShapingOptions() ), &glyphIndices, nullptr, nullptr, &glyphAdvances, nullptr, &runWidth );
		drawRun( runIt.getFont(), runIt.getLengthCh(), glyphIndices.data(), glyphAdvances.data(), penX, baseline, result );
		penX += runWidth;
	}
	return result;
}

void typeset( const AttrString &attrString, int32_t width, int32_t height, TypesetProcessor &processor, const TypesetOptions &options )
{
	std::vector<uint32_t> glyphIndices, clusters;
	std::vector<float> glyphAdvances, glyphMaxXs;

	if( width == 0 || height == 0 )
		return;

	if( width < 0 )
		width = std::numeric_limits<int32_t>::max();

	auto runIt = attrString.iterate( options.getDefaultFont() );
	if( ! runIt.nextRun() ) // empty string
		return;
	
	std::vector<char> breaks( attrString.size() );
	setLineBreaksUtf32( attrString.getStringUtf32().data(), attrString.size(), breaks.data() );

	vector<RunData> runData;
	size_t glyphStart = 0;
	do {
		size_t glyphLen = runIt.shape( runIt.getShapingOptions( options.getDefaultShapingOptions() ), &glyphIndices, &clusters, nullptr, &glyphAdvances, &glyphMaxXs, nullptr );
		runData.push_back( RunData{ runIt.getFont(), runIt.getAlignment( options.getDefaultAlignment() ), runIt.getLeading(), runIt.getColor( ColorAf::white() ), glyphStart, glyphLen, runIt.getStartCh() } );
		for( size_t g = glyphStart; g < clusters.size(); ++g )
			clusters[g] += (uint32_t)runIt.getStartCh();
		glyphStart += glyphLen;
	} while( runIt.nextRun() );

	vector<pair<vector<RunData>::iterator,SpanData>> linebreakedSpans;
	vector<RunData>::iterator curRunDataIt = runData.begin();
	size_t lineStartGlyph = 0, lineEndGlyph = 0; // inclusive range
	Alignment curAlignment = curRunDataIt->alignment;
	float baseline = 0;
	bool firstLine = true;
	while( lineEndGlyph < glyphIndices.size() && curRunDataIt != runData.end() ) {
		// calculate last glyph that will fit on the line (or hits a hard break). Does not account for implicit paragraph breaks due to alignment changes
		double lineWidthPx = glyphAdvances[lineStartGlyph];
		bool hitHardBreak = lineEndGlyph + 1 == glyphIndices.size() ? true : mustBreak( breaks.data(), clusters[lineEndGlyph + 1] );
		while( lineEndGlyph + 1 < glyphIndices.size() && lineWidthPx + glyphMaxXs[lineEndGlyph + 1] < width 
				/*&& lineWidthPx + glyphAdvances[lineEndGlyph + 1] < width*/ && ! hitHardBreak ) {
			++lineEndGlyph;
			lineWidthPx += glyphAdvances[lineEndGlyph];
			if( lineEndGlyph + 1 < glyphIndices.size() && mustBreak( breaks.data(), clusters[lineEndGlyph + 1] ) )
				hitHardBreak = true;
		}
		if( ! hitHardBreak ) { // no hard break means we ran over the end of the line; backtrack to see if we can find a place we can break
			if( ! findCanBreak( breaks.data(), clusters.data(), lineStartGlyph, &lineEndGlyph ) )
				processor.incrementNumForcedWordBreaks(); // failed to backtrack to a suitable break - note it for external fitting algorithms; this bookkeeping is not used here though
		}
		
		// gather the spans (either the first or last may be a partial Run)
		size_t curGlyph = lineStartGlyph;
		vector<pair<vector<RunData>::iterator,SpanData>> linebreakedSpans;
		lineWidthPx = 0;
		bool alignmentChange = false;
		do {
			size_t start = std::max( curGlyph, curRunDataIt->start ); // inclusive range
			size_t end = std::min( curRunDataIt->start + curRunDataIt->len - 1, lineEndGlyph ); // inclusive range
			CI_ASSERT( start <= end );
			uint32_t clusterStart = clusters[start]; // inclusive range
			uint32_t clusterEnd = clusters[end]; // inclusive range
			float spanWidthPx = 0;
			for( size_t g = start; g < end; ++g ) 
				spanWidthPx += glyphAdvances[g];
			spanWidthPx += glyphMaxXs[end];
			linebreakedSpans.emplace_back( curRunDataIt, SpanData{ start, end - start + 1, clusterStart, clusterEnd - clusterStart + 1, (float)lineWidthPx, spanWidthPx } );
			lineWidthPx += spanWidthPx + glyphAdvances[end] - glyphMaxXs[end];
			if( end == curRunDataIt->start + curRunDataIt->len - 1 ) { // finished this run
				++curRunDataIt;
				if( curRunDataIt != runData.end() && curRunDataIt->alignment != curAlignment ) { // alignment change implies new paragraph
					alignmentChange = true;
					lineEndGlyph = end;
					break;
				}
			}
			else
				break;
		} while( curRunDataIt != runData.end() && curRunDataIt->start <= lineEndGlyph );

		// process the spans
		if( ! linebreakedSpans.empty() )
			processLine( linebreakedSpans, glyphIndices, glyphAdvances, &firstLine, curAlignment, width, &baseline, processor, options );
		if( hitHardBreak && ! alignmentChange )
			++lineEndGlyph;
		++lineEndGlyph;

		if( alignmentChange )
			curAlignment = curRunDataIt->alignment;

		// process any trailing hard breaks
		while( lineEndGlyph < glyphIndices.size() && curRunDataIt != runData.end() && mustBreak( breaks.data(), clusters[lineEndGlyph] ) ) {
			processLine( { { curRunDataIt, SpanData{ lineEndGlyph, 0, 0, 0 } } }, glyphIndices, glyphAdvances, &firstLine, curAlignment, width, &baseline, processor, options );
			++lineEndGlyph;
			if( lineEndGlyph == curRunDataIt->start + curRunDataIt->len ) { // if we hit the end of the run, advance to the next
				++curRunDataIt;
				if( curRunDataIt != runData.end() )
					curAlignment = curRunDataIt->alignment;
			}
		}

		// skip any trailing whitespace
		if( ! alignmentChange ) {
			if( lineEndGlyph < glyphIndices.size() )
				findNonWhitespace( attrString.getStringUtf32().c_str(), clusters.data(), attrString.getStringUtf32().size(), &lineEndGlyph );
			// we may have terminated/skipped multiple runs due to whitespace skipping - get 'curRanData' in line with whatever glyph we're on
			while( curRunDataIt != runData.end() && lineEndGlyph >= curRunDataIt->start + curRunDataIt->len ) {
				++curRunDataIt;
				if( curRunDataIt != runData.end() )
					curAlignment = curRunDataIt->alignment;
			}
		}

		lineStartGlyph = lineEndGlyph;
	}

	processor.finish();
}

namespace {
std::once_flag once;
void initLineBreaks()
{
    std::call_once( once, [](){ init_linebreak(); });
}
}

void setLineBreaksUtf8( const char *str, size_t len, char *outBreaks )
{
	initLineBreaks();
	set_linebreaks_utf8( (utf8_t*)str, len, nullptr, outBreaks );
}

void setLineBreaksUtf32( const char32_t *str, size_t len, char *outBreaks )
{
	initLineBreaks();
	set_linebreaks_utf32( (utf32_t*)str, len, nullptr, outBreaks );
}

void setWordBreaksUtf8( const char *str, size_t len, char *outBreaks )
{
	set_wordbreaks_utf8( (utf8_t*)str, len, nullptr, outBreaks );
}

void setWordBreaksUtf32( const char32_t *str, size_t len, char *outBreaks )
{
	set_wordbreaks_utf32( (utf32_t*)str, len, nullptr, outBreaks );
}

////////////////////////////////////////////////////////////////////////////////////////////////
// TypesetOptions
TypesetOptions::TypesetOptions()
{
	mDefaultFont = loadFont( systemDefaultFace(), 12 );
}

////////////////////////////////////////////////////////////////////////////////////////////////
// GlyphLayout
void GlyphLayout::measure()
{
	mMeasuredWidth = 0;
	if( mLines.empty() ) {
		mMeasuredHeight = 0;
	}
	else {
		for( auto &line : mLines )
			mMeasuredWidth = std::max( mMeasuredWidth, line.getDrawOffset().x + line.getMeasuredWidth() );
		mMeasuredHeight = mLines.back().getDrawOffset().y + mLines.back().getDescender();
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Frame
class GlyphLayoutConstructorTypesetProcessor : public TypesetProcessor {
  public:
	  GlyphLayoutConstructorTypesetProcessor( GlyphLayout *glyphLayout ) : mGlyphLayout{ glyphLayout } {}

	void	addLine( Alignment justification, vec2 drawOffset, float ascender, float descender, float lineGap, float measuredWidth ) override {
		mGlyphLayout->getLines().push_back( Line( justification, drawOffset, ascender, descender, lineGap, measuredWidth ) );
	}
	void	addRun( const Font *font, size_t chStart, size_t chLen, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const float glyphAdvances[], float penX, float measuredWidth ) override {
		mGlyphLayout->getLines().back().getRuns().push_back( Run( len, font, chStart, chLen, color, glyphIndices, glyphAdvances, penX, measuredWidth ) );
	}
	void	finish() override {
		mGlyphLayout->measure();
	}

  private:
	GlyphLayout	*mGlyphLayout;
};

Frame::Frame( const AttrString &attrString, int32_t width, int32_t height, const TypesetOptions &options )
	: mAttrString( attrString), mWidth( width ), mHeight( height ), mTypesetOptions( options ), mDirty( true )
{
}

void Frame::updateGlyphLayout()
{
	mGlyphLayout.clear();
	GlyphLayoutConstructorTypesetProcessor processor{ &mGlyphLayout };

	typeset( mAttrString, mWidth, mHeight, processor, mTypesetOptions );

	mNumForcedWordBreaks = processor.mNumForcedWordBreaks;

	mDirty = false;
}

uint32_t Frame::getNumForcedWordBreaks() const
{
	if( mDirty )
		const_cast<Frame*>( this )->updateGlyphLayout();

	return mNumForcedWordBreaks;
}

const GlyphLayout& Frame::getGlyphLayout() const
{
	if( mDirty )
		const_cast<Frame*>( this )->updateGlyphLayout();
	
	return mGlyphLayout;
}

StaticGlyphLayout Frame::getStaticGlyphLayout() const
{
	return StaticGlyphLayout();
}

class SoftwareRenderFontData : public Font::Data {
	struct BitmapInfo {
		std::unique_ptr<Channel8u>		mChannel;
		std::unique_ptr<Surface8u>		mSurface;
		int16_t							mOffsetLeft, mOffsetTop;
		float							mScale;
	};

	SoftwareRenderFontData( text::Font *font );
	Channel8u			getGlyphChannel( uint32_t glyphIndex, int32_t *outOffsetLeft, int32_t *outOffsetTop );

	text::Font							*mFont;
	std::vector<BitmapInfo>		mBitmapCache;
};

SoftwareRenderFontData::SoftwareRenderFontData( text::Font *font )
	: mFont( font )
{
	// create a cache with slots for every glyph, but initially empty
	mBitmapCache = std::vector<SoftwareRenderFontData::BitmapInfo>( font->getFace()->getNumGlyphs() );
}

Channel8u SoftwareRenderFontData::getGlyphChannel( uint32_t glyphIndex, int32_t *outOffsetLeft, int32_t *outOffsetTop )
{
	auto &cached = mBitmapCache[glyphIndex];

	if( ! cached.mChannel ) {
		int32_t offsetLeft, offsetTop;
		cached.mChannel = mFont->getGlyphBitmap( glyphIndex, &offsetLeft, &offsetTop );
		mFont->lock();
		if( FT_Error err = FT_Load_Glyph( mFace->getFtFace(), glyphIndex, FT_LOAD_DEFAULT ) )
			throw text::FreeTypeExc( err );
		if( FT_Error err = FT_Render_Glyph( mFace->getFtFace()->glyph, FT_RENDER_MODE_NORMAL ) )
			throw text::FreeTypeExc( err );

		const FT_Bitmap &ftBitmap = mFace->getFtFace()->glyph->bitmap; 
		cached.mChannel = make_unique<Channel8u>( ftBitmap.width, ftBitmap.rows );
		cached.mChannel->copyFrom( ci::Channel8u( ftBitmap.width, ftBitmap.rows, ftBitmap.pitch, 1, ftBitmap.buffer ), Area( 0, 0, ftBitmap.width, ftBitmap.rows ) );
		cached.mOffsetLeft = mFace->getFtFace()->glyph->bitmap_left;
		cached.mOffsetTop = mFace->getFtFace()->glyph->bitmap_top;

		mFont->unlock();
	}

	if( outOffsetLeft )
		*outOffsetLeft = cached.mOffsetLeft;
	if( outOffsetTop )
		*outOffsetTop = cached.mOffsetTop;

	return ci::Channel8u( cached.mChannel->getWidth(), cached.mChannel->getHeight(), cached.mChannel->getRowBytes(), 1, cached.mChannel->getData() );
}

void render( const GlyphLayout &glyphLayout, Surface8u *surface, const ivec2 &offset )
{
	int32_t width = (int32_t)ceilf( glyphLayout.getMeasuredWidth() );
	int32_t height = (int32_t)ceilf( glyphLayout.getMeasuredHeight() );

	for( auto &line : glyphLayout.getLines() )
		for( auto &run : line.getRuns() ) {
			vec2 drawOffset = line.getDrawOffset() + run.getDrawOffset();
			drawRun( run.getFont(), run.getColor(), run.getLength(), run.getGlyphIndices(), run.getGlyphAdvances(), drawOffset.x, drawOffset.y, *surface );
		}
}

Surface8u renderSurface( const GlyphLayout &glyphLayout, const ivec2 &offset, const ColorA8u &bgColor )
{
	int32_t width = (int32_t)ceilf( glyphLayout.getMeasuredWidth() );
	int32_t height = (int32_t)ceilf( glyphLayout.getMeasuredHeight() );
	Surface8u result( width, height, true );
	result.setPremultiplied( true );
	ip::fill( &result, bgColor );

	for( auto &line : glyphLayout.getLines() )
		for( auto &run : line.getRuns() ) {
			vec2 drawOffset = line.getDrawOffset() + run.getDrawOffset();
			drawRun( run.getFont(), run.getColor(), run.getLength(), run.getGlyphIndices(), run.getGlyphAdvances(), drawOffset.x, drawOffset.y, result );
		}
	return result;
}

void render( const GlyphLayout &glyphLayout, Channel8u *channel, const ivec2 &offset )
{
	int32_t width = (int32_t)ceilf( glyphLayout.getMeasuredWidth() );
	int32_t height = (int32_t)ceilf( glyphLayout.getMeasuredHeight() );

	for( auto &line : glyphLayout.getLines() )
		for( auto &run : line.getRuns() ) {
			vec2 drawOffset = line.getDrawOffset() + run.getDrawOffset();
			drawRun( run.getFont(), run.getLength(), run.getGlyphIndices(), run.getGlyphAdvances(), drawOffset.x, drawOffset.y, *channel );
		}
}

Channel8u renderChannel( const GlyphLayout &glyphLayout, const ivec2 &offset )
{
	int32_t width = (int32_t)ceilf( glyphLayout.getMeasuredWidth() );
	int32_t height = (int32_t)ceilf( glyphLayout.getMeasuredHeight() );
	Channel8u result( width, height );
	ip::fill( &result, (uint8_t)0 );

	for( auto &line : glyphLayout.getLines() )
		for( auto &run : line.getRuns() ) {
			vec2 drawOffset = line.getDrawOffset() + run.getDrawOffset();
			drawRun( run.getFont(), run.getLength(), run.getGlyphIndices(), run.getGlyphAdvances(), drawOffset.x, drawOffset.y, result );
		}
	return result;
}

std::ostream& operator<<( std::ostream& os, const Run& r )
{
	os << "# glyphs: " << r.getLength() <<  " Font: " << *r.getFont() << " Offset: " << r.getDrawOffset();

	return os;
}

const char* FreeTypeExc::what() const noexcept
{
	sprintf( mBuffer, "FreeType error #%d", mErr );
	return mBuffer;
}

std::ostream& operator<<( std::ostream& os, const Line& l )
{
	os << "# runs: " << l.getRuns().size() << " Just: " << l.getAlignment() << " Base: " << l.getDrawOffset().y << " A: " << l.getAscender() << " D: " << l.getDescender() << " G: " << l.getLineGap();

	return os;
}

std::ostream& operator<<( std::ostream& os, const GlyphLayout& g )
{
	for( auto &line : g.getLines() ) {
		os << line << std::endl;
		for( auto &run : line.getRuns() )
			os << "   " << run << std::endl;
	}

	return os;
}

} } // namespace cinder::text
