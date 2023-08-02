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

#include "cinder/text/AttrString.h"
#include "cinder/Unicode.h"
#include "cinder/CinderAssert.h"
#include "cinder/text/Text.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include <utility>

using namespace std;

namespace {
const char * sAlignmentNames[] = { "LEFT", "CENTER", "RIGHT", "DEFAULT" };
}

namespace cinder { namespace text {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ShapingOptions
ShapingOptions& ShapingOptions::ligatures( bool enabled )
{
	mFeatures[text::feature( "liga" )] = enabled;
	mFeatures[text::feature( "clig" )] = enabled;
	mFeatures[text::feature( "calt" )] = enabled;
	return *this;
}

bool ShapingOptions::isDefault() const
{
	return ! mIgnoreMissingGlyphsNondefault && mFeatures.empty();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Placeholder
Placeholder::Placeholder()
	: mSize{ 0 }, mData{ 0 }, mEquivalentStr{ toUtf32( " " ) }
{}

Placeholder::Placeholder( ci::vec2 size, const std::string& equivalentUtf8, void* data )
	: mSize( size ), mData( data ), mEquivalentStr( toUtf32( equivalentUtf8 ) )
{
}

void Placeholder::setEquivalentString( const std::string& equivalentUtf8 )
{
	mEquivalentStr = toUtf32( equivalentUtf8 );
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AttrString
AttrString::AttrString() {
}
	
AttrString::AttrString( const string &utf8Str )
{
	mString = ci::toUtf32( utf8Str );
}

AttrString& AttrString::operator<<( const std::string &utf8Str )
{
	append( utf8Str );
	return *this;
}

AttrString& AttrString::operator<<( const char *utf8Str )
{
	append( utf8Str );
	return *this;
}

AttrString& AttrString::operator<<( const char32_t *utf32Str )
{
	append( utf32Str );
	return *this;
}

AttrString& AttrString::operator<<( const Font *font )
{
	setCurrentFont( font );
	return *this;
}

AttrString& AttrString::operator<<( const std::pair<std::string,float> &fontNameSize )
{
	Face *face = text::loadFace( fontNameSize.first );
	if( face ) {
		Font *font = text::loadFont( face, fontNameSize.second );
		setCurrentFont( font );
	}
	return *this;
}

AttrString& AttrString::operator<<( Tracking tracking )
{
	setCurrentTracking( tracking );
	return *this;
}

AttrString& AttrString::operator<<( Alignment alignment )
{
	setCurrentAlignment( alignment );
	return *this;
}

AttrString& AttrString::operator<<( Leading leading )
{
	setCurrentLeading( leading );
	return *this;
}

AttrString& AttrString::operator<<( const ColorA8u &color )
{
	setCurrentColor( ColorAf( color ) );
	return *this;
}

AttrString& AttrString::operator<<( const Color8u &color )
{
	setCurrentColor( ColorAf( color ) );
	return *this;
}

AttrString& AttrString::operator<<( const ColorAf &color )
{
	setCurrentColor( ColorAf( color ) );
	return *this;
}

AttrString& AttrString::operator<<( const Colorf &color )
{
	setCurrentColor( ColorAf( color ) );
	return *this;
}

AttrString& AttrString::operator<<( RunBreak runBreak )
{
	appendRunBreak( runBreak );
	return *this;
}

AttrString& AttrString::operator<<( const Placeholder &placeholder )
{
	append( placeholder );
	return *this;
}

AttrString& AttrString::operator<<( ShapingOptions shapingOptions )
{
	setCurrentShapingOptions( shapingOptions );
	return *this;
}

void AttrString::appendRunBreak( RunBreak runBreak )
{
	mRunBreaks.push_back( make_pair( (size_t)mString.length(), RunBreakInfo() ) );
}

void AttrString::append( const Placeholder& placeholder )
{
	mRunBreaks.push_back( make_pair( (size_t)mString.length(), RunBreakInfo( placeholder ) ) );
	mString.append( placeholder.getEquivalentStringU32() );
}

/*AttrString& AttrString::operator<<( const std::pair<const char*,float> &font )
{
	setCurrentFont( Manager::get()->loadFont( font.first, font.second ) );
	return *this;
}*/

AttrStringIter AttrString::iterate( const Font *defaultFont ) const
{
	return AttrStringIter( this, defaultFont );
}

void AttrString::append( const string &utf8Str )
{
	mString.append( ci::toUtf32( utf8Str ) );
	extendCurrentLimits();
}

void AttrString::append( const char *utf8Str )
{
	mString.append( ci::toUtf32( utf8Str ) );
	extendCurrentLimits();
}

void AttrString::append( const char32_t *utf32Str )
{
	mString.append( utf32Str );
	extendCurrentLimits();
}

void AttrString::clear()
{
	mFonts.clear();
	mColorAs.clear();
	mTrackings.clear();
	mAlignments.clear();
	mLeadings.clear();
	mColors.clear();
	mShapingOptions.clear();
	mRunBreaks.clear();

	mCurrentFontActive = false;
	mCurrentTrackingActive = false;
	mCurrentAlignmentActive = false;
	mCurrentLeadingActive = false;
	mCurrentColorActive = false;
	mCurrentShapingOptionsActive = false;

	mString.clear();
}

void AttrString::extendCurrentLimits()
{
	if( mCurrentFontActive )
		mFonts.setEndLimit( mString.size() );
	if( mCurrentTrackingActive )
		mTrackings.setEndLimit( mString.size() );
	if( mCurrentAlignmentActive )
		mAlignments.setEndLimit( mString.size() );
	if( mCurrentLeadingActive )
		mLeadings.setEndLimit( mString.size() );
	if( mCurrentColorActive )
		mColors.setEndLimit( mString.size() );
	if( mCurrentShapingOptionsActive )
		mShapingOptions.setEndLimit( mString.size() );
}

template<typename T>
void AttrString::setCurrentAttr( T value, bool valueIsDefault, bool *currentActive, IntervalMap<T> *intervals )
{
	// are we replacing the existing current
	if( *currentActive && intervals->endIsZeroLength() ) {
		if( valueIsDefault )
			intervals->clearInterval( mString.size(), mString.size() );
		else
			intervals->setEndValue( value );
		*currentActive = ! valueIsDefault;
	}
	else {
		if( ! valueIsDefault ) {
			intervals->set( mString.size(), mString.size(), value );
			*currentActive = true;
		}
		else
			*currentActive = false;
	}	
}

void AttrString::setCurrentFont( const Font *font )
{
	setCurrentAttr( font, font == nullptr, &mCurrentFontActive, &mFonts );
}

void AttrString::setCurrentTracking( Tracking tracking )
{
	setCurrentAttr( tracking, tracking.isDefault(), &mCurrentTrackingActive, &mTrackings );
}

void AttrString::setCurrentAlignment( Alignment alignment )
{
	setCurrentAttr( alignment, alignment == Alignment::DEFAULT, &mCurrentAlignmentActive, &mAlignments );
}

void AttrString::setCurrentLeading( Leading leading )
{
	setCurrentAttr( leading, leading.isDefault(), &mCurrentLeadingActive, &mLeadings );
}

void AttrString::setCurrentColor( const ColorAf &color )
{
	setCurrentAttr( color, color.a < 0, &mCurrentColorActive, &mColors );
}

void AttrString::setCurrentShapingOptions( const ShapingOptions &options )
{
	setCurrentAttr( options, options.isDefault(), &mCurrentShapingOptionsActive, &mShapingOptions );
}

void AttrString::setFont( size_t start, size_t end, const Font* font )
{
	if( ! font )
		mFonts.clearInterval( start, end );
	else
		mFonts.set( start, end, font );
}

void AttrString::setTracking( size_t start, size_t end, Tracking tracking )
{
	if( tracking.isDefault() )
		mTrackings.clearInterval( start, end );
	else
		mTrackings.set( start, end, tracking ); 
}

void AttrString::setAlignment( size_t start, size_t end, Alignment alignment )
{
	if( alignment == Alignment::DEFAULT )
		mAlignments.clearInterval( start, end );
	else
		mAlignments.set( start, end, alignment ); 
}

void AttrString::setShapingOptions( size_t start, size_t end, ShapingOptions options )
{
	if( options.isDefault() )
		mShapingOptions.clearInterval( start, end );
	else
		mShapingOptions.set( start, end, options );
}

std::vector<AttrString::FontSpan> AttrString::getFontSpans() const
{
	std::vector<AttrString::FontSpan> result;
	
	for( auto &spanIt : mFonts.getIntervals() )
		result.push_back( FontSpan( spanIt.first, spanIt.second.limit, spanIt.second.value ) );
	
	return result;
}

void AttrString::setFontSpans( const std::vector<FontSpan> &spans )
{
	mFonts.clear();
	for( auto &s : spans )
	 	mFonts.set( s.start, s.end, s.value );
}

std::string AttrString::debugString()
{
	ostringstream os;

	auto runIt = iterate( nullptr );
	while( runIt.nextRun() ) {
		float tracking = runIt.getTracking();
		bool colorIsDefault = runIt.isColorDefault();
		ColorAf color = runIt.getColor( ColorAf::white() );
		os << "> '" << runIt.getStrUtf8() << "' Font:" << *runIt.getFont() << " Tracking: " << tracking
				<< " Alignment: " << runIt.getAlignment( Alignment::DEFAULT )
//				<< " Color: " << ( colorIsDefault ? "Default" : toString( runIt.getColor( nullptr ).r ).c_str() )
				<< " Leading: " << toString( runIt.getLeading() )
				<< std::endl;
	}
	return os.str();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

AttrStringIter::AttrStringIter( const AttrString *attrStr, const Font *defaultFont )
	: mAttrStr( attrStr ), mStrStartOffset( 0 ), mStrEndOffset( 0 ), mStrLength( attrStr->size() ), mDefaultFont( defaultFont )
{
}

std::string AttrStringIter::getStrUtf8() const
{
	return ci::toUtf8( &mAttrStr->mString[mStrStartOffset], ( mStrEndOffset - mStrStartOffset ) * 4 );
}

ShapingOptions AttrStringIter::getShapingOptions( const ShapingOptions &defaultOptions )
{
	ShapingOptions result = defaultOptions;
	if( mShapingOptions.isIgnoreMissingGlyphsNondefault() )
		result.ignoreMissingGlyphs( mShapingOptions.getIgnoreMissingGlyphs() );
	for( auto feature : mShapingOptions.getFeatures() )
		result.feature( feature.first, feature.second );
	return result;
}

template<typename T>
size_t AttrStringIter::firstRunAttr( const IntervalMap<T> &attrMap, T *attrValue, const T defaultValue, bool *attrDone, typename IntervalMap<T>::ConstMapIter *attrIter )
{
	size_t end;
	if( attrMap.empty() ) { // empty attr map means default
		*attrValue = defaultValue;
		end = mStrLength;
		*attrDone = true;
	}
	else {
		*attrIter = attrMap.begin();
		if( (*attrIter)->first > 0 ) { // first attr span starts after start of string
			*attrValue = defaultValue;
			end = (*attrIter)->first;
		}
		else {
			*attrValue = (*attrIter)->second.value;
			end = (*attrIter)->second.limit;
		}
	}

	return end;
}

void AttrStringIter::firstRun()
{
	mIsPlaceholder = false;
	mStrEndOffset = mStrLength;

	mStrEndOffset = std::min( firstRunAttr<const Font*>( mAttrStr->mFonts, &mFont, nullptr, &mFontsDone, &mFontIter ), mStrEndOffset );
	mStrEndOffset = std::min( firstRunAttr<Tracking>( mAttrStr->mTrackings, &mTracking, Tracking(), &mTrackingsDone, &mTrackingIter ), mStrEndOffset );
	mStrEndOffset = std::min( firstRunAttr<Alignment>( mAttrStr->mAlignments, &mAlignment, Alignment::DEFAULT, &mAlignmentsDone, &mAlignmentIter ), mStrEndOffset );
	mStrEndOffset = std::min( firstRunAttr<Leading>( mAttrStr->mLeadings, &mLeading, Leading(), &mLeadingsDone, &mLeadingIter ), mStrEndOffset );
	mStrEndOffset = std::min( firstRunAttr<ColorAf>( mAttrStr->mColors, &mColor, ColorAf( 0, 0, 0, -1 ), &mColorsDone, &mColorIter ), mStrEndOffset );
	mStrEndOffset = std::min( firstRunAttr<ShapingOptions>( mAttrStr->mShapingOptions, &mShapingOptions, ShapingOptions{}, &mShapingOptionsDone, &mShapingOptionsIter ), mStrEndOffset );

	if( ! mFont )
		mFont = mDefaultFont;

	mRunBreaksIter = mAttrStr->mRunBreaks.begin();
	if( mRunBreaksIter != mAttrStr->mRunBreaks.end() ) {
		if( mAttrStr->mRunBreaks.begin()->first == 0 && mAttrStr->mRunBreaks.begin()->second.isPlaceholder() ) { // special case of RunBreak exactly at the start which is a Placeholder
			mIsPlaceholder = true;
			mCurrentPlaceholder = mAttrStr->mRunBreaks.begin()->second.getPlaceholder();
			mStrEndOffset = mStrStartOffset + mRunBreaksIter->second.getPlaceholder().getEquivalentStringU32().size();
			++mRunBreaksIter;			
		}
		else if( mRunBreaksIter->first <= mStrEndOffset ) {
			mStrEndOffset = mRunBreaksIter->first;
			if( ! mRunBreaksIter->second.isPlaceholder() ) // if this is not a Placeholder, just move on
				++mRunBreaksIter;
		}
	}
}

template<typename T>
void AttrStringIter::advanceAttr( const IntervalMap<T> &attrMap, T *attrValue, const T defaultValue, bool *attrDone, typename IntervalMap<T>::ConstMapIter *attrIter, size_t *newStrEnd )
{
	if( ! *attrDone ) {
		size_t spanStart, spanEnd;
		T spanValue = defaultValue;
		
		if( mStrStartOffset >= (*attrIter)->second.limit ) // time to increment
			++(*attrIter);

		if( (*attrIter) == attrMap.end() ) { // hit the end of the attr interval map; see if we have an active attr on the AttrString
			*attrDone = true;
			spanStart = mStrLength; // will fail range test
		}
		else {
			spanStart = (*attrIter)->first;
			spanEnd = (*attrIter)->second.limit;
			spanValue = (*attrIter)->second.value;
		}

		if( mStrStartOffset < spanStart ) { // still before this span
			*attrValue = defaultValue;
			*newStrEnd = std::min( *newStrEnd, spanStart );
		}
		else {
			*attrValue = spanValue;
			*newStrEnd = std::min( *newStrEnd, spanEnd );
		}
	}
}

// move all iterators forward so that their starts >= mStrStartOffset
void AttrStringIter::advance()
{
	size_t newStrEnd = mStrLength;
	mStrStartOffset = mStrEndOffset;

	// if we broke on a Placeholder, we need to process that as Run unto itself
	if( mRunBreaksIter != mAttrStr->mRunBreaks.end() && mStrEndOffset == mRunBreaksIter->first ) {
		mIsPlaceholder = true;
		mCurrentPlaceholder = mRunBreaksIter->second.getPlaceholder();
		mStrEndOffset = mStrStartOffset + mRunBreaksIter->second.getPlaceholder().getEquivalentStringU32().size();
		++mRunBreaksIter;
		return;
	}

	mIsPlaceholder = false;

	advanceAttr<const Font*>( mAttrStr->mFonts, &mFont, nullptr, &mFontsDone, &mFontIter, &newStrEnd );
	advanceAttr<Tracking>( mAttrStr->mTrackings, &mTracking, Tracking(), &mTrackingsDone, &mTrackingIter, &newStrEnd );
	advanceAttr<Alignment>( mAttrStr->mAlignments, &mAlignment, Alignment::DEFAULT, &mAlignmentsDone, &mAlignmentIter, &newStrEnd );
	advanceAttr<Leading>( mAttrStr->mLeadings, &mLeading, Leading(), &mLeadingsDone, &mLeadingIter, &newStrEnd );
	advanceAttr<ColorAf>( mAttrStr->mColors, &mColor, ColorAf( 0, 0, 0, -1 ), &mColorsDone, &mColorIter, &newStrEnd );
	advanceAttr<ShapingOptions>( mAttrStr->mShapingOptions, &mShapingOptions, ShapingOptions{}, &mShapingOptionsDone, &mShapingOptionsIter, &newStrEnd );

	if( ! mFont )
		mFont = mDefaultFont;

	if( mRunBreaksIter != mAttrStr->mRunBreaks.end() && newStrEnd >= mRunBreaksIter->first ) {
		newStrEnd = mRunBreaksIter->first;
		if( ! mRunBreaksIter->second.isPlaceholder() ) // if this is not a Placeholder, just move on
			++mRunBreaksIter;
	}
	
	mStrEndOffset = newStrEnd;
}

bool AttrStringIter::nextRun()
{
	if( mFirstRun ) {
		firstRun();
		mFirstRun = false;
		return mStrEndOffset > 0;
	}

	if( mStrEndOffset == mStrLength )
		return false;
	
	advance();
	
	return true;
}

size_t AttrStringIter::shape( const ShapingOptions &shapingOptions, vector<uint32_t> *outGlyphIndices, vector<uint32_t> *outClusters, vector<vec2> *outGlyphPositions, vector<float> *outGlyphXAdvances, vector<float> *outGlyphMaxXs, float *outPixelWidth ) const
{
	const Font *font = getFont();
	if( font ) {
		font->lock();
		size_t len;
		if( isPlaceholder() ) { // if this is a placeholder, manipulate the out* vectors so that the last glyph represents the width of the spacer (and the preceding, if they exist, are zero width)
			size_t startGlyphIdx = outClusters->size();
			len = mFont->shapeString( shapingOptions, &mAttrStr->mString[mStrStartOffset], mStrEndOffset - mStrStartOffset, getTracking(), outGlyphIndices, outClusters, outGlyphPositions, outGlyphXAdvances, outGlyphMaxXs, outPixelWidth );
			if( len ) {
				if( outGlyphPositions )
					for( size_t i = startGlyphIdx; i < outGlyphPositions->size(); ++i )
						(*outGlyphPositions)[i] = mCurrentPlaceholder.getSize();
				if( outGlyphXAdvances ) {
					for( size_t i = startGlyphIdx; i < outGlyphXAdvances->size() - 1; ++i )
						(*outGlyphXAdvances)[i] = 0;
					(*outGlyphXAdvances).back() = mCurrentPlaceholder.getWidth();
				}
				if( outGlyphMaxXs ) {
					for( size_t i = startGlyphIdx; i < outGlyphMaxXs->size() - 1; ++i )
						(*outGlyphMaxXs)[i] = 0;
					(*outGlyphMaxXs).back() = mCurrentPlaceholder.getWidth();
				}
			}
		}
		else
			len = mFont->shapeString( shapingOptions, &mAttrStr->mString[mStrStartOffset], mStrEndOffset - mStrStartOffset, getTracking(), outGlyphIndices, outClusters, outGlyphPositions, outGlyphXAdvances, outGlyphMaxXs, outPixelWidth );
		font->unlock();
		return len;
	}
	else {
		if( outPixelWidth )
			*outPixelWidth = 0;
		return 0;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

ostream& operator<<( ostream& os, const AttrString& a )
{
	os << ci::toUtf8( a.getStringUtf32() );
	return os;
}

std::ostream& operator<<( std::ostream& os, const Leading& l )
{
	if( l.isDefault() )
		os << "DEFAULT";
	else if( l.isMult() )
		os << "x " << l.getAmount();
	else
		os << "+ " << l.getAmount() << "px";
	return os;
}

std::ostream& operator<<( std::ostream& os, const Alignment& j )
{
	os << sAlignmentNames[(size_t)j];
	return os;
}

} } // namespace cinder::text
