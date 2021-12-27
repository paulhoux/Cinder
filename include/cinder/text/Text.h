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
#include "cinder/Filesystem.h"
#include "cinder/text/Face.h"
#include "cinder/text/Font.h"
#include "cinder/text/AttrString.h"
#include "cinder/Exception.h"
#include "cinder/Vector.h"
#include "cinder/Channel.h"
#include "cinder/Surface.h"
#include "cinder/DataSource.h"

#include <mutex>
#include <vector>

// FreeType forward declarations
typedef struct FT_LibraryRec_  	*FT_Library;
typedef int  					FT_Error;

namespace cinder { namespace text {

struct TypesetOptions;

class CI_API Manager {
  public:
	static Manager*		get();

	Face*				loadFace( const ci::fs::path &path, int faceIndex = 0 );
	Face*				loadFace( const DataSourceRef &dataSource, int faceIndex = 0 );
	Face*				loadFace( const void *data, size_t dataSize, int faceIndex = 0 );
	Face*				loadSystemFace( const std::string &name );
	Face*				systemDefaultFace();
	Font*				loadFont( Face *face, float size );
	Font*				loadFont( Face *face, float size, const Face::Variation &variation );
	
	//! Searches loaded fonts by name. Returns \c nullptr if no match is found
	Face*				findFace( const std::string &name ) const;
	//! returns \c nullptr if Font has not been loaded
	Font*				findFont( const Face *face, float size ) const;
	//! returns \c nullptr if Font has not been loaded
	Font*				findFont( const Face *face, float size, const Face::Variation &variation ) const;

	FT_Library&			getFtLibrary() { return *mLibraryPtr; }

	//! Returns a list of face names installed in the system which can be used in loadSystemFace() 
	std::vector<std::string>		getSystemFaceNames();

  private:
	Manager();
	~Manager();
	
	mutable std::mutex					mFaceMutex, mSystemDefaultFaceMutex;
	std::vector<std::unique_ptr<Face>>	mFaces;
	std::vector<std::unique_ptr<Font>>	mFonts;
	std::unique_ptr<FT_Library>			mLibraryPtr;
	Face*								mSystemDefaultFace = nullptr;
};

class CI_API Run {
  public:
	Run( size_t len, const Font* font, size_t startChar, size_t lengthChar, const ColorAf &color, const uint32_t *glyphIndices, const float *glyphAdvances, float drawOffsetX, float measuredWidth )
		: mFont( font ), mColor( color ), mStartChar( startChar ), mLengthChar( lengthChar ), 
			mGlyphIndices( glyphIndices, glyphIndices + len ), mGlyphAdvances( glyphAdvances, glyphAdvances + len ), mDrawOffset( drawOffsetX, 0 ), mMeasuredWidth( measuredWidth )
	{}

	//! Length in glyphs
	size_t							getLength() const { return mGlyphIndices.size(); }
	const uint32_t*					getGlyphIndices() const { return mGlyphIndices.data(); }
	const float*					getGlyphAdvances() const { return mGlyphAdvances.data(); }
	const Font*						getFont() const { return mFont; }
	//! Baseline-relative
	cinder::vec2					getDrawOffset() const { return mDrawOffset; }
	ColorAf							getColor() const { return mColor; }
	float							getMeasuredWidth() const { return mMeasuredWidth; }

	//! Index into the original AttrString
	size_t							getStartChar() const  { return mStartChar; }
	//! Length in characters
	size_t							getLengthChar() const { return mLengthChar; }

	void							setColor( const ColorAf &color ) { mColor = color; }
	void							setDrawOffset( const cinder::vec2 &drawOffset ) { mDrawOffset = drawOffset; }

  private:
	const Font*				mFont;
	size_t					mStartChar, mLengthChar;
	std::vector<uint32_t>	mGlyphIndices;
	std::vector<float>		mGlyphAdvances;
	cinder::vec2			mDrawOffset;
	float					mMeasuredWidth;
	ColorAf					mColor; // alpha < 0 -> default color

	friend class FrameConstructorTypesetProcessor;
};

class CI_API Line {
   public:
	Line( Alignment justification, float baseline, float ascender, float descender, float lineGap, float measuredWidth )
		: mAlignment( justification ), mBaseline( baseline ), mAscender( ascender ), mDescender( descender ), mLineGap( lineGap ), mMeasuredWidth( measuredWidth ), mDrawOffset( 0 )
	{}

	std::vector<Run>&			getRuns() { return mRuns; }
	const std::vector<Run>&		getRuns() const { return mRuns; }
	cinder::vec2				getDrawOffset() const { return mDrawOffset; }

	Alignment					getAlignment() const { return mAlignment; }
	float						getBaseline() const { return mBaseline; }
	float						getAscender() const { return mAscender; }
	float						getDescender() const { return mDescender; }
	float						getLineGap() const { return mLineGap; }
	float						getMeasuredWidth() const { return mMeasuredWidth; }

	void						setBaseline( float baseline ) { mBaseline = baseline; }

  private:
	std::vector<Run>		mRuns;
	cinder::vec2			mDrawOffset;
	float					mMeasuredWidth;

	Alignment			mAlignment;
	float				mBaseline, mAscender, mDescender, mLineGap;

	friend class FrameConstructorTypesetProcessor;
};

struct CI_API TypesetOptions {
	TypesetOptions();

	TypesetOptions&		defaultAlignment( Alignment alignment ) { mDefaultAlignment = alignment; return *this; }
	TypesetOptions&		defaultShapingOptions( const ShapingOptions &shapingOptions ) { mDefaultShapingOptions = shapingOptions; return *this; }
	TypesetOptions&		ignoreLineMetrics( bool ignore = true ) { mIgnoreLineMetrics = ignore; return *this; }
	//! Number of pixels to offset the topmost line's baseline relative to its normal position
	TypesetOptions&		topLineOffset( float px ) { mTopLineOffsetPx = px; return *this; }
	//! Defaults to system font in 12 pt
	TypesetOptions&		defaultFont( const Font *font ) { mDefaultFont = font; return *this; }

	Alignment			getDefaultAlignment() const { return mDefaultAlignment; }
	ShapingOptions		getDefaultShapingOptions() const { return mDefaultShapingOptions; }
	bool				getIgnoreLineMetrics() const { return mIgnoreLineMetrics; }
	float				getTopLineOffset() const { return mTopLineOffsetPx; }
	const Font*			getDefaultFont() const { return mDefaultFont; }

  private:
	Alignment		mDefaultAlignment = Alignment::LEFT;
	ShapingOptions	mDefaultShapingOptions;
	bool			mIgnoreLineMetrics = false;
	float			mTopLineOffsetPx = 0;
	const Font*		mDefaultFont; // = loadFont( systemDefaultFace(), 12 );
};

class CI_API Frame {
  public:
	static constexpr int GROW = -1;

	Frame( const AttrString &attrString, int32_t width, int32_t height = GROW, const TypesetOptions &options = TypesetOptions() );

	float						getFirstBaseline() const { return mLines.empty() ? 0 : mLines[0].getBaseline(); }
	std::vector<Line>&			getLines() { return mLines; }
	const std::vector<Line>&    getLines() const { return mLines; }

	Channel8u				renderToChannel() const;
	Surface8u				renderToSurface( bool alpha = true, const ColorA &background = ColorA( 0, 0, 0, 0 ) ) const;

	void					render( Surface8u *surface, const vec2 &drawOffset );

	float					getMeasuredWidth() const { return mMeasuredWidth; }
	float					getMeasuredHeight() const { return mMeasuredHeight; }

	//! Returns number of times a word was forced to break because it was too long for the line width
	uint32_t				getNumForcedWordBreaks() const { return mNumForcedWordBreaks; }

  private:
	AttrString				mAttrString;
	int32_t					mWidth, mHeight;
	float					mMeasuredWidth, mMeasuredHeight;
	uint32_t				mNumForcedWordBreaks = 0;
	std::vector<Line>       mLines;

	friend class FrameConstructorTypesetProcessor;
};

CI_API std::ostream& operator<<( std::ostream& os, const Run& r );
CI_API std::ostream& operator<<( std::ostream& os, const Line& l );
CI_API std::ostream& operator<<( std::ostream& os, const Frame& f );

//! Loads a system font based on its \a name. Returns \c nullptr if no suitable match is found
CI_API inline Face*		loadSystemFace( const std::string &name ) { return Manager::get()->loadSystemFace( name ); }
CI_API inline Face*		systemDefaultFace() { return Manager::get()->systemDefaultFace(); }
CI_API inline Face*		loadFace( const ci::fs::path &path, int faceIndex = 0 ) { return Manager::get()->loadFace( path, faceIndex ); }
CI_API inline Face*		loadFace( const DataSourceRef &dataSource, int faceIndex = 0 ) { return Manager::get()->loadFace( dataSource, faceIndex ); }
CI_API inline Face*		loadFace( const void *data, size_t dataSize, int faceIndex = 0 ) { return Manager::get()->loadFace( data, dataSize, faceIndex ); }
CI_API inline Font*		loadFont( Face *face, float size ) { return Manager::get()->loadFont( face, size ); }
CI_API inline Font*		loadFont( Face *face, float size, const Face::Variation &variation ) { return Manager::get()->loadFont( face, size, variation ); }
//! Load a system font named \a name, of point size \a size
CI_API Font*				font( const std::string &name, float size );
//! Load a system font, searching in-order in \a fonts
CI_API Font*				font( const std::vector<std::pair<std::string,float>> &fonts );


CI_API void measureString( const AttrString& attrString, float *resultWidth, float *resultHeight = nullptr, float *resultBaseline = nullptr );

CI_API Channel8u	renderString( const Font *font, const char *utf8String, float tracking = 0 );
CI_API Channel8u	renderString( const AttrString &attrString ); // renders on one line
CI_API Channel8u	renderString( const AttrString &attrString, int32_t width, int32_t height, const TypesetOptions &options = TypesetOptions(), float *outBaseline = nullptr );


//! \a outBreaks size must be >= \a len, contains 0: must break, 1: allow break, 2: cannot break, 3: inside utf8/utf16 sequence, 4: indeterminate
CI_API void setLineBreaksUtf8( const char *str, size_t len, char *outBreaks );
//! \a outBreaks size must be >= \a len, contains 0: must break, 1: allow break, 2: cannot break, 4: indeterminate
CI_API void setLineBreaksUtf32( const char32_t *str, size_t len, char *outBreaks );

//! \a outBreaks size must be >= \a len, contains 0: must break, 1: cannot break, 3: inside utf8/utf16 sequence
CI_API void setWordBreaksUtf8( const char *str, size_t len, char *outBreaks );
//! \a outBreaks size must be >= \a len, contains 0: must break, 1: cannot break
CI_API void setWordBreaksUtf32( const char32_t *str, size_t len, char *outBreaks );

class CI_API TypesetProcessor {
  public:
	virtual ~TypesetProcessor() {}

	virtual void	addLine( Alignment justification, float baseline, float ascender, float descender, float lineGap, float measuredWidth ) {}
	virtual void	addRun( const Font *font, size_t chStart, size_t chLen, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const float glyphAdvances[], float penX, float measuredWidth ) {}
	virtual void	finishLine() {}

	virtual void	incrementNumForcedWordBreaks() { ++mNumForcedWordBreaks; }

	uint32_t		mNumForcedWordBreaks = 0;
};

CI_API void typeset( const AttrString &attrString, int32_t width, int32_t height, TypesetProcessor &processor, const TypesetOptions &options );

class CI_API FreeTypeExc : public Exception {
  public:
	FreeTypeExc() : mErr( 0 ) {}
	FreeTypeExc( FT_Error err ) : mErr( err ) {}

	const char* what() const noexcept override;

	mutable char	mBuffer[1024];
	FT_Error		mErr;
};

class CI_API HarfBuzzExc : public Exception {
  public:
	HarfBuzzExc() {}
};

//! Loaded a Font with a Variation of the wrong Face
class CI_API InvalidVariationFace : public Exception {
};

} } // namespace cinder::text
