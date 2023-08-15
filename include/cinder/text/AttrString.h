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

#include <string>
#include <vector>
#include <utility>

#include "cinder/Color.h"
#include "cinder/text/Font.h"
#include "cinder/IntervalMap.h"


namespace cinder { namespace text {

struct CI_API Tracking {
	enum class Type { DEFAULT, PIXELS, EM };

	Tracking() : mType( Type::DEFAULT ), mValue( 0 ) {}
	//! Thousandths of an em, thus font size-relative
	static Tracking em( float tracking ) { return Tracking( Type::EM, tracking ); }
	//! Pixels
	static Tracking pixels( float tracking ) { return Tracking( Type::PIXELS, tracking ); }
	Tracking( const Tracking &rhs ) = default;

	float		getValuePx( float fontSize ) const {
		if( mType == Type::DEFAULT ) return 0;
		else if( mType == Type::PIXELS ) return mValue;
		else /*( mType == Type::EM )*/ return fontSize * mValue / 1000.0f;
	}
	bool		isDefault() const { return mType == Type::DEFAULT; }

  private:
	Tracking( Type type, float value ) : mType( type ), mValue( value ) {}

	Type		mType;
	float		mValue;
};

struct CI_API BaselineOffset {
	enum class Type { DEFAULT, PIXELS, EM };

	BaselineOffset() : mType( Type::DEFAULT ), mValue( 0 ) {}
	//! Thousandths of an em, thus font size-relative
	static BaselineOffset em( float offset ) { return BaselineOffset( Type::EM, offset ); }
	//! Pixels
	static BaselineOffset pixels( float offset ) { return BaselineOffset( Type::PIXELS, offset ); }
	BaselineOffset( const BaselineOffset &rhs ) = default;

	float		getValuePx( float fontSize ) const {
		if( mType == Type::DEFAULT ) return 0;
		else if( mType == Type::PIXELS ) return mValue;
		else /*( mType == Type::EM )*/ return fontSize * mValue / 1000.0f;
	}
	bool		isDefault() const { return mType == Type::DEFAULT; }

  private:
	BaselineOffset( Type type, float value ) : mType( type ), mValue( value ) {}

	Type		mType;
	float		mValue;
};

struct CI_API Leading {
	enum class Type { DEFAULT, MULT, EXTRA, SIZE };

	//! Multiplies \a factor by font size if TypesetOptions::ignoreLineMetrics, else font height
	static Leading mult( float factor ) { return Leading( Type::MULT, factor ); }
	//! Adds \a px pixels to font size if TypesetOptions::ignoreLineMetrics, else font height
	static Leading extra( float px ) { return Leading( Type::EXTRA, px ); }
	//! Sets leading to \a ptSize regardless of TypesetOptions
	static Leading size( float ptSize ) { return Leading( Type::SIZE, ptSize ); }

	Leading() : mType( Type::DEFAULT ), mAmount( 0 ) {}

	float	getLineHeight( float height ) const {
		if( mType == Type::DEFAULT ) return height;
		else if( mType == Type::MULT ) return mAmount * height;
		else if( mType == Type::SIZE ) return mAmount;
		else /*EXTRA*/ return height + mAmount; 
	}
	bool	isDefault() const { return mType == Type::DEFAULT; }
	bool	isMult() const { return mType == Type::MULT; }
	float	getAmount() const { return mAmount; }

  private:
	Leading( Type type, float amount ) : mType( type ), mAmount( amount ) {}

	Type		mType;
	float		mAmount;
};

CI_API uint32_t constexpr feature( char name[4] ) { return (((uint32_t)(name[0])&0xFF)<<24)|(((uint32_t)(name[1])&0xFF)<<16)|(((uint32_t)(name[2])&0xFF)<<8)|((uint32_t)(name[3])&0xFF); }
CI_API uint32_t constexpr feature( char c1, char c2, char c3, char c4 ) { return (((uint32_t)(c1)&0xFF)<<24)|(((uint32_t)(c2)&0xFF)<<16)|(((uint32_t)(c3)&0xFF)<<8)|((uint32_t)(c4)&0xFF); }


struct CI_API ShapingOptions {
	//! Causes missing glyphs to be ignored rather than substituted with a default missing character. Default is \c false
	ShapingOptions&		ignoreMissingGlyphs( bool enable = true ) { mIgnoreMissingGlyphs = enable; mIgnoreMissingGlyphsNondefault = true; return *this; }

	bool				getIgnoreMissingGlyphs() const { return mIgnoreMissingGlyphs; }
	bool				isIgnoreMissingGlyphsNondefault() const { return mIgnoreMissingGlyphsNondefault; }
	void				setIgnoreMissingGlyphsDefault() { mIgnoreMissingGlyphsNondefault = false; }

	bool				isDefault() const;

	ShapingOptions&					feature( uint32_t feature, bool enabled ) { mFeatures[feature] = enabled; return *this; }
	//! Enables/disables 'liga', 'clig', and 'calt' features
	ShapingOptions&					ligatures( bool enabled = true );
	ShapingOptions&					superscript( bool enabled = true );
	ShapingOptions&					subscript( bool enabled = true );
	void							setFeatureDefault( uint32_t feature ) { mFeatures.erase( feature ); }
	const std::map<uint32_t,bool>&	getFeatures() const { return mFeatures; }

  protected:
	bool						mIgnoreMissingGlyphs = false, mIgnoreMissingGlyphsNondefault = false;
	std::map<uint32_t,bool>		mFeatures;
};

enum class Alignment { LEFT, CENTER, RIGHT, JUSTIFIED, DEFAULT };

struct CI_API RunBreak {
};

struct CI_API Placeholder {
	Placeholder();
	Placeholder( ci::vec2 size, const std::string& equivalentUtf8 = " ", void *data = nullptr );

	ci::vec2				getSize() const { return mSize; }
	float					getWidth() const { return mSize.x; }
	float					getHeight() const { return mSize.y; }
	void*					getData() const { return mData; }
	void					setData( void *data ) { mData = data; }
	const std::u32string&	getEquivalentStringU32() const { return mEquivalentStr; }
	void					setEquivalentString( const std::string& equivalentUtf8 );

	ci::vec2		mSize;
	void*			mData;
	std::u32string	mEquivalentStr;
};

class AttrStringIter;

class CI_API AttrString
{
  public:
	template<typename T>
	struct CI_API Span {
	   	Span( size_t start, size_t end, T value )
			   	: start( start ), end( end ), value( value ) {}
	   	Span( size_t start )
			   	: start( start ), end( start ) {}
	   	Span() {}
	
	   	bool    isOpen() const { return start == end; }
	
	   	size_t start, end;  // [start,end) range
	   	
	   	T value;
	   	
	   	bool operator<(const Span &span) const { return start < span.start; }
	   	bool operator==(const Span &span) const { return start == span.start && end == span.end && value == span.value; }
	   	bool operator!=(const Span &span) const { return start != span.start || end != span.end || value != span.value; }
	};
	typedef Span<const Font*>		FontSpan;

	friend AttrStringIter;

	AttrString();
	AttrString( const std::string &utf8Str );

	AttrString& operator<<( const std::string &utf8Str );
	AttrString& operator<<( const char *utf8Str );
	AttrString& operator<<( const char32_t *utf32Str );
	AttrString& operator<<( const Font *font );
	AttrString& operator<<( const std::pair<std::string,float> &fontNameSize );
	AttrString& operator<<( Tracking tracking );
	AttrString& operator<<( BaselineOffset baselineOffset );
	AttrString& operator<<( Alignment alignment );
	AttrString& operator<<( Leading leading );
	AttrString& operator<<( const ColorA8u &color );
	AttrString& operator<<( const Color8u &color );
	AttrString& operator<<( const ColorAf &color );
	AttrString& operator<<( const Colorf &color );
	AttrString& operator<<( RunBreak runBreak );
	AttrString& operator<<( const Placeholder &placeholder );
	AttrString& operator<<( ShapingOptions shapingOptions );
//	AttrString& operator<<( const std::pair<const char*,float> &font );

	void 	setCurrentFont( const Font *font );
	void	setCurrentTracking( Tracking tracking );
	void	setCurrentBaselineOffset( BaselineOffset baselineOffset );
	void	setCurrentAlignment( Alignment alignment );
	void	setCurrentLeading( Leading leading );
	void 	setCurrentColor( const ColorAf &c );
	void 	setCurrentShapingOptions( const ShapingOptions &options );
	void 	setFont( size_t start, size_t end, const Font *font );
	void	setTracking( size_t start, size_t end, Tracking tracking );
	void	setBaselineOffset( size_t start, size_t end, BaselineOffset baselineOffset );
	void	setAlignment( size_t start, size_t end, Alignment alignment );
	void	setShapingOptions( size_t start, size_t end, ShapingOptions options );

	void	append( const std::string &utf8Str );
	void	append( const char *utf8Str );
	void	append( const char32_t *utf32Str );
	void	appendRunBreak( RunBreak runBreak );
	void	append( const Placeholder& placeholder );
	void	append( const ShapingOptions &shapingOptions );
//	void	append( const ColorA8u &color );

	size_t	size() const { return mString.size(); }
	bool	empty() const { return mString.empty(); }
	void	clear();

	AttrStringIter		iterate( const Font *defaultFont ) const;

	const std::u32string&		getStringUtf32() const { return mString; }

	std::vector<FontSpan>		getFontSpans() const;
	void						setFontSpans( const std::vector<FontSpan> &spans );
	
	std::string					debugString();
  protected:
	void						extendCurrentLimits();
	template<typename T>
	void						setCurrentAttr( T value, bool valueIsDefault, bool *currentActive, IntervalMap<T> *intervals );


	IntervalMap<const Font*> 		mFonts;
	IntervalMap<ColorA> 			mColorAs;
	IntervalMap<Tracking>			mTrackings;
	IntervalMap<BaselineOffset>		mBaselineOffsets;
	IntervalMap<Alignment>			mAlignments;
	IntervalMap<Leading>			mLeadings;
	IntervalMap<ColorAf>			mColors;
	IntervalMap<ShapingOptions>		mShapingOptions;

	//! Encodes either a RunBreak of a Placeholder
	struct RunBreakInfo {
		RunBreakInfo() : mIsPlaceholder( false ) {}
		RunBreakInfo( const Placeholder &placeholder ) : mIsPlaceholder( true ), mPlaceholder( placeholder ) {}

		bool		isPlaceholder() const { return mIsPlaceholder; }
		Placeholder	getPlaceholder() const { return mPlaceholder; }

		bool		mIsPlaceholder;
		Placeholder	mPlaceholder;
	};
	std::vector<std::pair<size_t, RunBreakInfo>>	mRunBreaks; // sorted vector of string offsets
	
	bool			mCurrentFontActive = false;
	bool			mCurrentTrackingActive = false;
	bool			mCurrentBaselineOffsetActive = false;
	bool			mCurrentAlignmentActive = false;
	bool			mCurrentLeadingActive = false;
	bool			mCurrentColorActive = false;
	bool			mCurrentShapingOptionsActive = false;
	
	std::u32string 		mString;
};
	
class CI_API AttrStringIter {
	friend AttrString;

  public:
	bool	nextRun();
	
	bool					isPlaceholder() const { return mIsPlaceholder; }
	Placeholder				getPlaceholder() const { return mCurrentPlaceholder; }
	size_t					getStartCh() const { return mStrStartOffset; }
	size_t					getLengthCh() const { return mStrEndOffset - mStrStartOffset; }
	const char32_t*			getStrPtr() const { return &mAttrStr->mString[mStrStartOffset]; }
	std::string				getStrUtf8() const;
	const Font*				getFont() const { return mFont; }
	float					getTracking() const { return mTracking.getValuePx( mFont->getSize() ); }
	float					getBaselineOffset() const { return mBaselineOffset.getValuePx( mFont->getSize() ); }
	Alignment				getAlignment( Alignment defaultValue ) const { return mAlignment == Alignment::DEFAULT ? defaultValue : mAlignment; }
	Leading					getLeading() const { return mLeading; }
	ColorAf					getColor( ColorAf defaultColor ) const { return mColor.a < 0 ? defaultColor : mColor; }
	bool					isColorDefault() const { return mColor.a < 0; }
	ShapingOptions			getShapingOptions( const ShapingOptions &defaultOptions );

	//! Appends to vectors. Returns number of glyphs added
	size_t					shape( const ShapingOptions &options, std::vector<uint32_t> *outGlyphIndices, std::vector<uint32_t> *outClusters, std::vector<vec2> *outGlyphPositions, std::vector<float> *outGlyphXAdvances, std::vector<float> *outGlyphMaxXs, float *outPixelWidth ) const;
//  	bool			getRunTrackingIsConstant() const { return mRunTrackingIsConstant; }
//  	void			getRunTrackingValue() const { return mRunTrackingValue; }
  	
  private:
  	AttrStringIter( const AttrString *attrStr, const Font *defaultFont );
	template<typename T>
	size_t			firstRunAttr( const IntervalMap<T> &attrMap, T *attrValue, const T defaultValue, bool *attrDone, typename IntervalMap<T>::ConstMapIter *attrIter );
	void			firstRun();

	void			advance();
	template<typename T>
	void			advanceAttr( const IntervalMap<T> &attrMap, T *attrValue, const T defaultValue, bool *attrDone, typename IntervalMap<T>::ConstMapIter *attrIter, size_t *newStrEnd );


	const AttrString	*mAttrStr;

	bool				mFirstRun = true;
	size_t				mStrStartOffset, mStrEndOffset;
	const size_t		mStrLength;
	const Font*			mDefaultFont;

	IntervalMap<const Font*>::ConstMapIter		mFontIter;
	const Font*									mFont;
	bool										mFontsDone = false;

	IntervalMap<Tracking>::ConstMapIter	mTrackingIter;
	Tracking							mTracking;
	bool								mTrackingsDone = false;

	IntervalMap<BaselineOffset>::ConstMapIter	mBaselineOffsetIter;
	BaselineOffset								mBaselineOffset;
	bool										mBaselineOffsetsDone = false;

	IntervalMap<Alignment>::ConstMapIter	mAlignmentIter;
	Alignment								mAlignment = Alignment::DEFAULT;
	bool									mAlignmentsDone = false;

	IntervalMap<Leading>::ConstMapIter			mLeadingIter;
	Leading										mLeading;
	bool										mLeadingsDone = false;
	
	IntervalMap<ColorAf>::ConstMapIter	mColorIter;
	ColorAf								mColor;
	bool								mColorsDone = false;

	IntervalMap<ShapingOptions>::ConstMapIter	mShapingOptionsIter;
	ShapingOptions								mShapingOptions;
	bool										mShapingOptionsDone = false;

	std::vector<std::pair<size_t, AttrString::RunBreakInfo>>::const_iterator	mRunBreaksIter;
	bool																		mIsPlaceholder = false;
	Placeholder																	mCurrentPlaceholder;
};

std::ostream& operator<<( std::ostream& os, const AttrString& dt );
std::ostream& operator<<( std::ostream& os, const Alignment& j );
std::ostream& operator<<( std::ostream& os, const Leading& l );

} } // namespace cinder::text
