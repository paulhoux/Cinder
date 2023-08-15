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
		return font( face, size );
	else
		return font( systemDefaultFace(), size );
}

Font* font( const std::vector<std::pair<std::string,float>> &fonts )
{
	for( auto &tryFont : fonts ) {
		Face *face = Manager::get()->findFace( tryFont.first ); // try to find among loaded
		if( ! face )
			face = loadSystemFace( tryFont.first );
		if( face )
			return font( face, tryFont.second );
	}

	if( fonts.empty() )
		return font( systemDefaultFace(), 12 );
	else
		return font( systemDefaultFace(), fonts.front().second );
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
	bool isPlaceholder = false;
	Placeholder placeholder;
};

struct SpanData {
	size_t glyphStart, glyphLen;
	size_t chStart, chLen;
	float drawOffset;
	float measuredWidth;
};

namespace {
size_t countGlyphs( const vector<pair<vector<RunData>::iterator,SpanData>> &spans )
{
	size_t result = 0;
	for( const auto &span : spans )
		result += span.second.glyphLen;
	return result;
}
} // anonymous

// draws runs and advances baseline accordingly. If \a firstLine then we only increase baseline by ascender
bool processLine( const AttrString &attrString, const std::vector<uint32_t> &clusters, const vector<pair<vector<RunData>::iterator,SpanData>> &spans, const std::vector<uint32_t> &glyphIndices, const std::vector<float> &glyphXAdvances, std::vector<vec2> *glyphPositions,
						bool *firstLine, bool lastLine, Alignment justification, int maxWidth, float *baseline, TypesetProcessor &fn, const TypesetOptions &options )
{
	if( spans.empty() )
		return true;

	// measure line height metrics
	float maxAscent = 0, maxLineHeight = 0, maxDescent = 0, maxLineGap = 0;
	for( auto &span : spans ) {
		if( span.first->isPlaceholder ) {
			maxAscent = std::max( maxAscent, span.first->placeholder.getHeight() );
			maxLineHeight = std::max( maxLineHeight, span.first->placeholder.getHeight() );
		}
		else {
			maxAscent = std::max( maxAscent, span.first->font->getAscender() );
			maxDescent = std::max( maxDescent, -span.first->font->getDescender() );
			maxLineGap = std::max( maxLineGap, span.first->font->getLineGap() );
			float baseHeight = options.getIgnoreLineMetrics() ? span.first->font->getSize() : span.first->font->getHeight();
			maxLineHeight = std::max( maxLineHeight, span.first->leading.getLineHeight( baseHeight ) );
		}
	}

	if( *firstLine ) { // first line moves baseline down by max ascent
		*firstLine = false;
		*baseline = maxAscent + options.getTopLineOffset();
	}
	else { // other lines move down by max( prev max line height, 
		*baseline += maxLineHeight;
	}

	// local lambda

	// prep offsets based on line alignment
	float measuredLineWidth = spans.back().second.drawOffset + spans.back().second.measuredWidth;	
	float justificationOffset = 0;
	double trackingPerGlyph = 0;
	if( maxWidth != std::numeric_limits<int32_t>::max() ) {
		if( justification == Alignment::RIGHT ) justificationOffset = maxWidth - measuredLineWidth;
		else if( justification == Alignment::CENTER ) justificationOffset = ( maxWidth - measuredLineWidth ) / 2.0f;
		else if( justification == Alignment::JUSTIFIED && ! lastLine ) {
			size_t numGlyphs = countGlyphs( spans );
			if( numGlyphs > 1 )
				trackingPerGlyph = ( maxWidth - measuredLineWidth ) / (double)(numGlyphs - 1);
		}
	}

	bool exited = false;
	if( ! fn.addLine( justification, vec2( justificationOffset, *baseline ), maxAscent, maxDescent, maxLineGap, measuredLineWidth ) )
		exited = true;
	double trackingSum = 0; // for tracking offset for JUSTIFIED across Runs
	if( ! exited ) { // iterate the Spans
		for( auto &span : spans ) {
			double penX = trackingSum;
			if( span.first->len && span.second.glyphLen ) { // not an empty Span or RunData
				std::vector<uint32_t> localClusters; // relativize clusters to the Run, rather than the master AttrString
				localClusters.reserve( span.second.glyphLen );
				for( size_t g = 0; g < span.second.glyphLen; g++ ) {
					localClusters.push_back( (uint32_t)(clusters[span.second.glyphStart + g] - span.second.chStart) );
					(*glyphPositions)[span.second.glyphStart + g].x = (float)penX; // relativize x-values for position to the Run, but keep y values
					penX += glyphXAdvances[span.second.glyphStart + g] + trackingPerGlyph; // move pen by xAdvance, as well as tracking used for JUSTIFIED alignment
					trackingSum += trackingPerGlyph;
				}
				if( span.first->isPlaceholder ) { // special handling for a Placeholder
					PlaceholderInfo placeholderInfo;
					placeholderInfo.mData = span.first->placeholder.getData();
					float width = span.second.measuredWidth;
					float placeholderX = (*glyphPositions)[span.second.glyphStart].x;
					placeholderInfo.mBounds = Rectf( placeholderX, -span.first->placeholder.getHeight(), placeholderX + span.second.measuredWidth, 0 ) + vec2( span.second.drawOffset, *baseline );
					if( ! fn.addRun( span.first->font, &(attrString.getStringUtf32().c_str()[span.second.chStart]), span.second.chLen, localClusters, span.first->color, span.second.glyphLen, &glyphIndices[span.second.glyphStart], &(*glyphPositions)[span.second.glyphStart], span.second.drawOffset, span.second.measuredWidth, &placeholderInfo ) ) {
						exited = true;
						break;
					}
				}
				else { // normal Run
					if( ! fn.addRun( span.first->font, &(attrString.getStringUtf32().c_str()[span.second.chStart]), span.second.chLen, localClusters, span.first->color, span.second.glyphLen, &glyphIndices[span.second.glyphStart], &(*glyphPositions)[span.second.glyphStart], span.second.drawOffset, span.second.measuredWidth, nullptr ) ) {
						exited = true;
						break;
					}
				}
			}
		}
	}
	fn.finishLine();
	return ! exited;
}

void typeset( const AttrString &attrString, int32_t width, int32_t height, TypesetProcessor &processor, const TypesetOptions &options )
{
	std::vector<uint32_t> glyphIndices, clusters;
	std::vector<float> glyphMaxXs, glyphXAdvances;
	std::vector<vec2> glyphPositions; // we pass to processLine() to retrieve the y values, but we ovewrite the x-values based on x-advances

	if( width == 0 || height == 0 )
		return;

	if( width < 0 )
		width = std::numeric_limits<int32_t>::max();

	AttrStringIter runIt = attrString.iterate( options.getDefaultFont() );
	if( ! runIt.nextRun() ) // empty string
		return;
	
	std::vector<char> breaks( attrString.size() );
	setLineBreaksUtf32( attrString.getStringUtf32().data(), attrString.size(), breaks.data() );

	// construct 'RunData' vector by extracting runs from the AttrString
	vector<RunData> runData;
	size_t glyphStart = 0;
	do {
		size_t glyphLen = runIt.shape( runIt.getShapingOptions( options.getDefaultShapingOptions() ), &glyphIndices, &clusters, &glyphPositions, &glyphXAdvances, &glyphMaxXs, nullptr );
		runData.push_back( RunData{ runIt.getFont(), runIt.getAlignment( options.getDefaultAlignment() ), runIt.getLeading(), runIt.getColor( ColorAf::white() ), glyphStart, glyphLen, runIt.getStartCh() } );
		if( runIt.isPlaceholder() ) {
			runData.back().isPlaceholder = true;
			runData.back().placeholder = runIt.getPlaceholder();
		}
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
		double lineWidthPx;
		lineWidthPx = glyphXAdvances[lineStartGlyph];
		bool hitHardBreak = lineEndGlyph + 1 == glyphIndices.size() ? true : mustBreak( breaks.data(), clusters[lineEndGlyph + 1] );
		while( lineEndGlyph + 1 < glyphIndices.size() && lineWidthPx + glyphMaxXs[lineEndGlyph + 1] < width 
				/*&& lineWidthPx + glyphAdvances[lineEndGlyph + 1] < width*/ && ! hitHardBreak ) {
			++lineEndGlyph;
			lineWidthPx += glyphXAdvances[lineEndGlyph];
			if( lineEndGlyph + 1 < glyphIndices.size() && mustBreak( breaks.data(), clusters[lineEndGlyph + 1] ) )
				hitHardBreak = true;
		}
		if( ! hitHardBreak ) { // no hard break means we ran over the end of the line; backtrack to see if we can find a place we can break
			if( ! findCanBreak( breaks.data(), clusters.data(), lineStartGlyph, &lineEndGlyph ) )
				processor.incrementNumForcedWordBreaks(); // failed to backtrack to a suitable break - note it for external fitting algorithms; this bookkeeping is not used here though
		}
		
		// gather the spans for this line (the first or last may be a partial Runs)
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
				spanWidthPx += glyphXAdvances[g];
			spanWidthPx += glyphMaxXs[end];
			linebreakedSpans.emplace_back( curRunDataIt, SpanData{ start, end - start + 1, clusterStart, clusterEnd - clusterStart + 1, (float)lineWidthPx, spanWidthPx } );
			lineWidthPx += spanWidthPx + glyphXAdvances[end] - glyphMaxXs[end];
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
			if( ! processLine( attrString, clusters, linebreakedSpans, glyphIndices, glyphXAdvances, &glyphPositions, &firstLine, lineEndGlyph >= glyphIndices.size() - 1, curAlignment, width, &baseline, processor, options ) )
				goto exit;
		if( hitHardBreak && ! alignmentChange )
			++lineEndGlyph;
		++lineEndGlyph;

		if( alignmentChange )
			curAlignment = curRunDataIt->alignment;

		// process any trailing hard breaks
		while( lineEndGlyph < glyphIndices.size() && curRunDataIt != runData.end() && mustBreak( breaks.data(), clusters[lineEndGlyph] ) ) {
			if( ! processLine( attrString, clusters, { { curRunDataIt, SpanData{ lineEndGlyph, 0, 0, 0 } } }, glyphIndices, glyphXAdvances, &glyphPositions, &firstLine, false, curAlignment, width, &baseline, processor, options ) )
				goto exit;
			++lineEndGlyph;
			if( lineEndGlyph == curRunDataIt->start + curRunDataIt->len ) { // if we hit the end of the run, advance to the next
				++curRunDataIt;
				if( curRunDataIt != runData.end() )
					curAlignment = curRunDataIt->alignment;
			}
		}

		// skip any trailing whitespace
		if( ! alignmentChange ) {
			if( lineEndGlyph < glyphIndices.size() && ! curRunDataIt->isPlaceholder )
				findNonWhitespace( attrString.getStringUtf32().c_str(), clusters.data(), attrString.getStringUtf32().size(), &lineEndGlyph );
			// we may have terminated/skipped multiple runs due to whitespace skipping - get 'curRunData' in line with whatever glyph we're on
			while( curRunDataIt != runData.end() && lineEndGlyph >= curRunDataIt->start + curRunDataIt->len ) {
				++curRunDataIt;
				if( curRunDataIt != runData.end() )
					curAlignment = curRunDataIt->alignment;
			}
		}

		lineStartGlyph = lineEndGlyph;
	}

	exit:
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
	mDefaultFont = font( systemDefaultFace(), 12 );
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Line
void Line::breakGlyphsIntoRuns()
{
	std::vector<Run> runs;
	
	for( auto &run : mRuns ) {
		double penX = run.getDrawOffset().x;
		auto &clusters = run.getClusters();
		std::vector<uint32_t> localClusters{ 0 };
		for( size_t glyphIdx = 0; glyphIdx < run.getNumGlyphs(); ++glyphIdx ) {
			//Run( const Font* font, const char32_t *utf32Text, size_t textLength, const ColorAf &color, uint32_t glyph, float drawOffsetX, float measuredWidth )
			const vec2 *orientation = run.getGlyphOrientations() ? &run.getGlyphOrientations()[glyphIdx] : nullptr;
			runs.emplace_back( run.getFont(), &run.getTextUtf32()[clusters[glyphIdx]], 1, localClusters, run.getColor(), run.getGlyphIndices()[glyphIdx],
								run.getGlyphPositions()[glyphIdx], orientation, (float)penX, 0.0f );
		}
	}

	mRuns = runs;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Run
Rectf Run::getGlyphBounds( size_t g ) const
{
	CI_ASSERT( g < getNumGlyphs() );
	GlyphMetrics metrics = mFont->getGlyphMetrics( mGlyphIndices[g] );
	return Rectf( (float)(mGlyphPositions[g].x + metrics.horizontalBearingX), mGlyphPositions[g].y - metrics.horizontalBearingY,
			(float)(mGlyphPositions[g].x + metrics.horizontalBearingX + metrics.width), mGlyphPositions[g].y - metrics.horizontalBearingY + metrics.height );
}

////////////////////////////////////////////////////////////////////////////////////////////////
// GlyphLayoutConstructorTypesetProcessor
class GlyphLayoutConstructorTypesetProcessor : public TypesetProcessor {
  public:
	  GlyphLayoutConstructorTypesetProcessor( GlyphLayout *glyphLayout ) : mGlyphLayout{ glyphLayout } {}

	bool	addLine( Alignment justification, vec2 drawOffset, float ascender, float descender, float lineGap, float measuredWidth ) override {
		mGlyphLayout->getLines().push_back( Line( justification, drawOffset, ascender, descender, lineGap, measuredWidth ) );
		return true;
	}

	bool	addRun( const Font *font, const char32_t *utf32Str, size_t chLen, const std::vector<uint32_t> &clusters, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float measuredWidth, PlaceholderInfo *placeholderInfo ) override {
		if( placeholderInfo ) {
			mGlyphLayout->getPlaceholders().push_back( *placeholderInfo );
			mGlyphLayout->getLines().back().getRuns().push_back( Run( len, font, utf32Str, chLen, clusters, color, glyphIndices, glyphPositions, nullptr, penX, measuredWidth, placeholderInfo ) );
		}
		else
			mGlyphLayout->getLines().back().getRuns().push_back( Run( len, font, utf32Str, chLen, clusters, color, glyphIndices, glyphPositions, nullptr, penX, measuredWidth, nullptr ) );
		return true;
	}
	
	void	finish() override {
		mGlyphLayout->measure();
	}

  private:
	GlyphLayout	*mGlyphLayout;
};

////////////////////////////////////////////////////////////////////////////////////////////////
// GlyphLayout
vec2 GlyphLayout::calcSize() const
{
	float width = 0, height = 0;
	for( auto &line : mLines ) {
		for( auto &run : line.getRuns() ) {
			width = std::max( width, run.getDrawOffset().x + run.getMeasuredWidth() );
			height = std::max( height, run.getDrawOffset().y + run.getDescender() );
		}
	}

	return { width, height };
}

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

bool GlyphLayout::nextRun( Iterator &iter, const Run** run, vec2 *lineDrawOffset ) const
{
	if( iter.refcon0 >= mLines.size() )
		return false;

	while( iter.refcon1 >= mLines[iter.refcon0].getNumRuns() ) {
		iter.refcon0++;
		iter.refcon1 = 0;
		if( iter.refcon0 >= mLines.size() )
			return false;
	}

	*run = &mLines[iter.refcon0].getRuns()[iter.refcon1];
	*lineDrawOffset = mLines[iter.refcon0].getDrawOffset();
	iter.refcon1++;
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Frame
Frame::Frame( const AttrString &attrString, int32_t width, int32_t height, const TypesetOptions &options )
	: mAttrString( attrString), mWidth( width ), mHeight( height ), mTypesetOptions( options ), mDirty( true )
{
}

void Frame::updateGlyphLayout() const
{
	if( mDirty )
		const_cast<Frame*>( this )->updateGlyphLayoutImpl();
}

void Frame::updateGlyphLayoutImpl()
{
	mGlyphLayout.clear();
	GlyphLayoutConstructorTypesetProcessor processor{ &mGlyphLayout };

	typeset( mAttrString, mWidth, mHeight, processor, mTypesetOptions );

	mNumForcedWordBreaks = processor.mNumForcedWordBreaks;

	mDirty = false;
}

uint32_t Frame::getNumForcedWordBreaks() const
{
	updateGlyphLayout();
	return mNumForcedWordBreaks;
}

const GlyphLayout& Frame::getGlyphLayout() const
{
	updateGlyphLayout();
	return mGlyphLayout;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TextOnPath
class TextOnPathTypesetProcessor : public text::TypesetProcessor {
public:
	TextOnPathTypesetProcessor( const ci::Path2d &path, float initialMargin, float baselineOffset, GlyphLayout *glyphLayout )
		: mPath( path ), mPathCalcCache( path ), mInitialMargin( initialMargin ), mBaselineOffset( baselineOffset ), mGlyphLayout( glyphLayout ), mDone( false ), mLineVerticalOffset( 0 )
	{}

	bool	addLine( Alignment justification, vec2 drawOffset, float ascender, float descender, float lineGap, float measuredWidth ) {
		if( mDone )
			return false;

		mLineVerticalOffset = -ascender;
		mGlyphLayout->getLines().push_back( Line( justification, drawOffset, ascender, descender, lineGap, measuredWidth ) );
		return true;
	}

	bool	addRun( const Font *font, const char32_t *utf32Str, size_t chLen, const std::vector<uint32_t> &clusters, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float measuredWidth, PlaceholderInfo *placeholderInfo ) override
	{
		if( mDone )
			return false;

		penX += mInitialMargin;

		if( placeholderInfo ) {
			mGlyphLayout->getPlaceholders().push_back( *placeholderInfo );
			mGlyphLayout->getLines().back().getRuns().push_back( Run( len, font, utf32Str, chLen, clusters, color, glyphIndices, glyphPositions, nullptr, penX, measuredWidth, placeholderInfo ) );
		}
		else {
			size_t glyphIdx = 0;
			std::vector<vec2> positions( len );
			std::vector<vec2> orientations( len );
			do {
				float positionTime = mPathCalcCache.calcTimeForDistance( penX + glyphPositions[glyphIdx].x, false );
				float tangentTime = mPathCalcCache.calcTimeForDistance( penX + glyphPositions[glyphIdx].x + font->getGlyphMetrics( glyphIndices[glyphIdx] ).width / 2.0f, false );
				vec2 tangent = glm::normalize( mPath.getTangent( tangentTime ) );
				orientations[glyphIdx] = vec2( tangent.y, -tangent.x );
				positions[glyphIdx] = mPath.getPosition( positionTime ) + vec2( 0, mLineVerticalOffset ) + orientations[glyphIdx] * mBaselineOffset;
				++glyphIdx;
			} while( penX + glyphPositions[glyphIdx].x < mPathCalcCache.getLength() && glyphIdx < len );
			
			if( glyphIdx < len ) // if we terminated because we ran out of path instead of glyphs, we're done
				mDone = true;
			mGlyphLayout->getLines().back().getRuns().push_back( Run( glyphIdx, font, utf32Str, chLen, clusters, color, glyphIndices, positions.data(), orientations.data(), 0, measuredWidth, nullptr ) );
		}

		return ! mDone;
	}

	void	finish() override {
		mGlyphLayout->measure();
	}

	cinder::Path2d				mPath;
	cinder::Path2dCalcCache		mPathCalcCache;
	GlyphLayout*				mGlyphLayout;
	float						mLineVerticalOffset, mInitialMargin, mBaselineOffset;
	bool						mDone;
};

TextOnPath::TextOnPath( const AttrString &attrString, const Path2d &path, const TypesetOptions &options, float initialMargin, float baselineOffset )
	: mDirty( true ), mAttrString( attrString ), mPath( path ), mTypesetOptions( options ), mInitialMargin( initialMargin ), mBaselineOffset( baselineOffset )
{
}

void TextOnPath::updateGlyphLayout() const
{
	if( mDirty )
		const_cast<TextOnPath*>( this )->updateGlyphLayoutImpl();
}

void TextOnPath::updateGlyphLayoutImpl()
{
	mGlyphLayout.clear();
	TextOnPathTypesetProcessor processor{ mPath, mInitialMargin, mBaselineOffset, &mGlyphLayout };

	// We don't need to limit width or height
	typeset( mAttrString, -1, -1, processor, mTypesetOptions );

	mDirty = false;
}

const GlyphLayout& TextOnPath::getGlyphLayout() const
{
	updateGlyphLayout();
	return mGlyphLayout;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// render implementation
Channel8u renderString( const AttrString &attrString )
{
	std::vector<uint32_t> glyphIndices;
	std::vector<vec2> glyphPositions;

	float resultWidthF, resultHeightF, baseline;
	measureString( attrString, &resultWidthF, &resultHeightF, &baseline );
	int resultWidth = (int)ceil( resultWidthF );
	int resultHeight = (int)ceil( resultHeightF );
	
	Channel8u result( resultWidth, resultHeight );
	ip::fill( &result, (uint8_t)0 );
	
	auto runIt = attrString.iterate( TypesetOptions().getDefaultFont() );
	float penX = 0, runWidth;
	while( runIt.nextRun() ) {
		if( runIt.isPlaceholder() )
			continue;
		runIt.shape( runIt.getShapingOptions( ShapingOptions() ), &glyphIndices, nullptr, &glyphPositions, nullptr, nullptr, &runWidth );
		runIt.getFont()->drawGlyphs( runIt.getLengthCh(), glyphIndices.data(), glyphPositions.data(), penX, baseline, result ); 
	}
	return result;
}

void render( const Typesetter &typesetter, Surface8u *surface, const vec2 &offset, bool precise, bool srgb )
{
	Typesetter::Iterator iter = typesetter.getIterator();
	const Run* runPtr;
	vec2 lineDrawOffset;
	while( typesetter.nextRun( iter, &runPtr, &lineDrawOffset ) ) {
		if( runPtr->isPlaceholder() )
			continue;
		vec2 drawOffset = lineDrawOffset + runPtr->getDrawOffset();
		if( precise || runPtr->getGlyphOrientations() )
			runPtr->getFont()->drawGlyphsPrecise( runPtr->getColor(), runPtr->getNumGlyphs(), runPtr->getGlyphIndices(), runPtr->getGlyphPositions(), runPtr->getGlyphOrientations(), offset.x + drawOffset.x, offset.y + drawOffset.y, *surface, srgb );
		else
			runPtr->getFont()->drawGlyphs( runPtr->getColor(), runPtr->getNumGlyphs(), runPtr->getGlyphIndices(), runPtr->getGlyphPositions(), offset.x + drawOffset.x, offset.y + drawOffset.y, *surface, srgb );		
	}
}

Surface8u renderSurface( const Typesetter &typesetter, const vec2 &offset, const ColorA8u &bgColor, bool precise, bool srgb )
{
	vec2 size = typesetter.calcSize();
	int32_t width = (int32_t)ceilf( size.x + offset.x );
	int32_t height = (int32_t)ceilf( size.y + offset.y );
	Surface8u result( width, height, true );
	result.setPremultiplied( true );
	ip::fill( &result, bgColor );

	Typesetter::Iterator iter = typesetter.getIterator();
	const Run* runPtr;
	vec2 lineDrawOffset;
	while( typesetter.nextRun( iter, &runPtr, &lineDrawOffset ) ) {
		if( runPtr->isPlaceholder() )
			continue;
		vec2 drawOffset = lineDrawOffset + runPtr->getDrawOffset();
		if( precise || runPtr->getGlyphOrientations() )
			runPtr->getFont()->drawGlyphsPrecise( runPtr->getColor(), runPtr->getNumGlyphs(), runPtr->getGlyphIndices(), runPtr->getGlyphPositions(), runPtr->getGlyphOrientations(), offset.x + drawOffset.x, offset.y + drawOffset.y, result, srgb );
		else
			runPtr->getFont()->drawGlyphs( runPtr->getColor(), runPtr->getNumGlyphs(), runPtr->getGlyphIndices(), runPtr->getGlyphPositions(), offset.x + drawOffset.x, offset.y + drawOffset.y, result, srgb );
	}

	return result;
}

void render( const Typesetter &typesetter, Channel8u *channel, const vec2 &offset, bool precise, bool srgb )
{
	Typesetter::Iterator iter = typesetter.getIterator();
	const Run* runPtr;
	vec2 lineDrawOffset;
	while( typesetter.nextRun( iter, &runPtr, &lineDrawOffset ) ) {
		if( runPtr->isPlaceholder() )
			continue;
		vec2 drawOffset = lineDrawOffset + runPtr->getDrawOffset();
		if( precise || runPtr->getGlyphOrientations() )
			runPtr->getFont()->drawGlyphsPrecise( runPtr->getNumGlyphs(), runPtr->getGlyphIndices(), runPtr->getGlyphPositions(), runPtr->getGlyphOrientations(), offset.x + drawOffset.x, offset.y + drawOffset.y, *channel );
		else
			runPtr->getFont()->drawGlyphs( runPtr->getNumGlyphs(), runPtr->getGlyphIndices(), runPtr->getGlyphPositions(), offset.x + drawOffset.x, offset.y + drawOffset.y, *channel );
	}
}

Channel8u renderChannel( const Typesetter &typesetter, const vec2 &offset, bool precise, bool srgb )
{
	vec2 size = typesetter.calcSize();
	int32_t width = (int32_t)ceilf( size.x + offset.x );
	int32_t height = (int32_t)ceilf( size.y + offset.y );
	Channel8u result( width, height );
	ip::fill( &result, (uint8_t)0 );

	Typesetter::Iterator iter = typesetter.getIterator();
	const Run* runPtr;
	vec2 lineDrawOffset;
	while( typesetter.nextRun( iter, &runPtr, &lineDrawOffset ) ) {
		if( runPtr->isPlaceholder() )
			continue;
		vec2 drawOffset = lineDrawOffset + runPtr->getDrawOffset();
		if( precise || runPtr->getGlyphOrientations() )
			runPtr->getFont()->drawGlyphsPrecise( runPtr->getNumGlyphs(), runPtr->getGlyphIndices(), runPtr->getGlyphPositions(), runPtr->getGlyphOrientations(), offset.x + drawOffset.x, offset.y + drawOffset.y, result, srgb );
		else
			runPtr->getFont()->drawGlyphs( runPtr->getNumGlyphs(), runPtr->getGlyphIndices(), runPtr->getGlyphPositions(), offset.x + drawOffset.x, offset.y + drawOffset.y, result );
	}

	return result;
}

std::ostream& operator<<( std::ostream& os, const Run& r )
{
	os << "# glyphs: " << r.getNumGlyphs() <<  " Font: " << *r.getFont() << " Offset: " << r.getDrawOffset();

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
