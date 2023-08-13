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
#include "cinder/Unicode.h"

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


//! Records info about Placeholder after it has been typeset
struct PlaceholderInfo {
	Rectf		getBounds() const { return mBounds; }
	void*		getData() const { return mData; }

	Rectf		mBounds;
	void*		mData;
};

class CI_API Run {
  public:
	Run( size_t len, const Font* font, const char32_t *utf32Text, size_t textLength, const std::vector<uint32_t> &clusters, const ColorAf &color, const uint32_t *glyphIndices,
			const vec2* glyphPositions, const vec2* glyphOrientations, float drawOffsetX, float measuredWidth, PlaceholderInfo *placeholderInfo )
		: mFont( font ), mColor( color ), mText( utf32Text, textLength ), mClusters( clusters ),
			mGlyphIndices( glyphIndices, glyphIndices + len ), mGlyphPositions( glyphPositions, glyphPositions + len ), mDrawOffset( drawOffsetX, 0 ), mMeasuredWidth( measuredWidth )
	{
		if( glyphOrientations )
			mGlyphOrientations = std::vector<vec2>( glyphOrientations, glyphOrientations + len );
		if( placeholderInfo ) {
			mIsPlaceholder = true;
			mPlaceholderInfo = *placeholderInfo;
		}
		else
			mIsPlaceholder = false;
	}
	//! Single-glyph Run
	Run( const Font* font, const char32_t *utf32Text, size_t textLength, const std::vector<uint32_t> &clusters, const ColorAf &color, uint32_t glyph, const vec2 &glyphPosition,
			const vec2 *glyphOrientation, float drawOffsetX, float measuredWidth )
		: mFont( font ), mColor( color ), mText( utf32Text, textLength ), mClusters( clusters ),
		mGlyphIndices( &glyph, &glyph + 1 ), mGlyphPositions( { glyphPosition } ), mDrawOffset( drawOffsetX, 0 ), mMeasuredWidth( measuredWidth ), mIsPlaceholder( false )
	{
		if( glyphOrientation )
			mGlyphOrientations.push_back( *glyphOrientation );
	}

	//! Length in glyphs
	size_t							getNumGlyphs() const { return mGlyphIndices.size(); }
	const uint32_t*					getGlyphIndices() const { return mGlyphIndices.data(); }
	const vec2*						getGlyphPositions() const { return mGlyphPositions.data(); }
	//! Will return \c nullptr if the Run has no glyph orientations
	const vec2*						getGlyphOrientations() const { return mGlyphOrientations.empty() ? nullptr : mGlyphOrientations.data(); }
	const Font*						getFont() const { return mFont; }
	float							getDescender() const { return -mFont->getDescender(); }
	//! Line-relative
	cinder::vec2					getDrawOffset() const { return mDrawOffset; }
	ColorAf							getColor() const { return mColor; }
	void							setColor( const ColorAf &color ) { mColor = color; }
	void							setOpacity( const float opacity ) { mColor.a = opacity; }
	float							getMeasuredWidth() const { return mMeasuredWidth; }

	bool							isPlaceholder() const { return mIsPlaceholder; }
	const PlaceholderInfo&			getPlaceholderInfo() const { return mPlaceholderInfo; }
	PlaceholderInfo&				getPlaceholderInfo() { return mPlaceholderInfo; }

	//! Returns whether this run encodes an orientation per-glyph, which will be a 2D vector it's meant to be aligned to. By default, implicitly this vec2( 0, 1 );
	bool							hasOrientations() const { return ! mGlyphOrientations.empty(); }

	//! Returns Run-relative glyph bounds for glyph index \a g. Must be in the range [0, getNumGlyphs())
	Rectf							getGlyphBounds( size_t g ) const;

	//! UTF-32 string represented by the Run. Doesn't require conversion
	std::u32string					getTextUtf32() const { return mText; }
	//! UTF-8 string represented by the Run. Requires conversion from UTF-32
	std::string						getTextUtf8() const { return ci::toUtf8( mText ); }
	const std::vector<uint32_t>&	getClusters() const { return mClusters; }

	void							setDrawOffset( const cinder::vec2 &drawOffset ) { mDrawOffset = drawOffset; }

  private:
	const Font*				mFont;
	std::u32string			mText;
	std::vector<uint32_t>	mClusters;
	std::vector<uint32_t>	mGlyphIndices;
	std::vector<vec2>		mGlyphPositions;
	std::vector<vec2>		mGlyphOrientations;
	cinder::vec2			mDrawOffset;
	float					mMeasuredWidth;
	ColorAf					mColor; // alpha < 0 -> default color
	bool					mIsPlaceholder;
	PlaceholderInfo			mPlaceholderInfo;

	friend class FrameConstructorTypesetProcessor;
};

class CI_API Line {
   public:
	Line( Alignment justification, vec2 drawOffset, float ascender, float descender, float lineGap, float measuredWidth )
		: mAlignment( justification ), mDrawOffset( drawOffset ), mAscender( ascender ), mDescender( descender ), mLineGap( lineGap ), mMeasuredWidth( measuredWidth )
	{}

	std::vector<Run>&			getRuns() { return mRuns; }
	const std::vector<Run>&		getRuns() const { return mRuns; }
	size_t						getNumRuns() const { return mRuns.size(); }
	//! x-offset for Alignment and y-offset for baseline
	cinder::vec2				getDrawOffset() const { return mDrawOffset; }
	//! x-offset for Alignment and y-offset for baseline
	void						setDrawOffset( const vec2& drawOffset ) { mDrawOffset = drawOffset; }

	//! Convenience to set color of all Runs in the Line
	void						setColor( const ColorAf &color ) { for( auto &run : mRuns ) run.setColor( color ); }
	//! Convenience to set opacity of all Runs in the Line
	void						setOpacity( float opacity ) { for( auto &run : mRuns ) run.setOpacity( opacity ); }

	Alignment					getAlignment() const { return mAlignment; }
	//! Potentially invalidated by changes to Runs
	float						getAscender() const { return mAscender; }
	//! Potentially invalidated by changes to Runs
	float						getDescender() const { return mDescender; }
	//! Potentially invalidated by changes to Runs
	float						getLineGap() const { return mLineGap; }
	//! Potentially invalidated by changes to Runs. Excludes draw offset for Alignment
	float						getMeasuredWidth() const { return mMeasuredWidth; }

	//! Convenience, sum of all child Runs' glyph counts
	size_t						getNumGlyphs() const { size_t total = 0; for( auto &run : mRuns ) total += run.getNumGlyphs(); return total; }
	//! Breaks all Runs into a single Run per glyph (for animation or other custom rendering purposes)
	void						breakGlyphsIntoRuns();

  private:
	std::vector<Run>		mRuns;
	cinder::vec2			mDrawOffset;
	float					mMeasuredWidth;

	Alignment			mAlignment;
	float				mAscender, mDescender, mLineGap;

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

class CI_API Typesetter {
  public:
	struct Iterator {
		size_t	refcon0, refcon1; // opaque storage for an Iterator to store state
	};

	virtual ~Typesetter() {}
	virtual Iterator	getIterator() const = 0;
	virtual bool		nextRun( Iterator &iter, const Run** run, vec2 *lineDrawOffset ) const = 0;
	virtual vec2		calcSize() const = 0;
};

//! 
class CI_API GlyphLayout : public Typesetter {
  public:
	float			getMeasuredWidth() const { return mMeasuredWidth; }
	float			getMeasuredHeight() const { return mMeasuredHeight; }
	float			getFirstBaseline() const { return mLines.empty() ? 0 : mLines[0].getDrawOffset().y; }

	//! Modification of Lines requires a call to measure()
	std::vector<Line>&			getLines() { return mLines; }
	const std::vector<Line>&	getLines() const { return mLines; }
	size_t						getNumLines() const { return mLines.size(); }

	//! Recalculates measureWidth, measuredHeight, and the position of the first baseline if Lines are modified 
	void						measure();

	void						clear() { mLines.clear(); }

	//! Breaks all Runs into a single Run per glyph (for animation purposes)
	void						breakGlyphsIntoRuns() { for( auto &line : mLines ) line.breakGlyphsIntoRuns(); }


	Iterator		getIterator() const override { return Iterator{ 0, 0 }; }
	bool			nextRun( Iterator &iter, const Run** run, vec2 *lineDrawOffset ) const override;
	vec2			calcSize() const override;

	const std::vector<PlaceholderInfo>&		getPlaceholders() const { return mPlaceholders; }
	std::vector<PlaceholderInfo>&			getPlaceholders() { return mPlaceholders; }

  protected:
	float							mMeasuredWidth = -1, mMeasuredHeight = -1;
	std::vector<Line>				mLines;
	std::vector<PlaceholderInfo>	mPlaceholders;
};


class CI_API Frame : public Typesetter {
  public:
	static constexpr int GROW = -1;

	Frame() : mWidth( 0 ), mHeight( 0 ), mDirty( false ) {}
	Frame( const AttrString &attrString, int32_t width = GROW, int32_t height = GROW, const TypesetOptions &options = TypesetOptions() );

	//! Returns pre-typesetting width. A measured width requires the generation of a GlyphLayout. May return \c -1, meaning \c GROW
	int32_t				getWidth() const { return mWidth; }
	//! Returns pre-typesetting width. A measured width requires the generation of a GlyphLayout. May return \c -1, meaning \c GROW
	int32_t				getHeight() const { return mHeight; }

	TypesetOptions		getTypesetOptions() const { return mTypesetOptions; }

	const GlyphLayout&		getGlyphLayout() const;

	//! Returns number of times a word was forced to break because it was too long for the line width
	uint32_t				getNumForcedWordBreaks() const;

	Iterator		getIterator() const override { updateGlyphLayout(); return mGlyphLayout.getIterator(); }
	bool			nextRun( Iterator &iter, const Run** run, vec2 *lineDrawOffset ) const override { return mGlyphLayout.nextRun( iter, run, lineDrawOffset ); }
	vec2			calcSize() const override { return mGlyphLayout.calcSize(); }

  protected:
	void					updateGlyphLayout() const;
	void					updateGlyphLayoutImpl();

	bool					mDirty;
	GlyphLayout				mGlyphLayout;
	AttrString				mAttrString;
	TypesetOptions			mTypesetOptions;
	int32_t					mWidth, mHeight;
	uint32_t				mNumForcedWordBreaks = 0;

	friend class FrameConstructorTypesetProcessor;
};

class CI_API TextOnPath : public Typesetter {
  public:
	TextOnPath() : mDirty( false ) {}
	TextOnPath( const AttrString &attrString, const Path2d &path, const TypesetOptions &options = TypesetOptions(), float initialMargin = 0, float baselineOffset = 0 );

	TypesetOptions		getTypesetOptions() const { return mTypesetOptions; }

	const GlyphLayout&		getGlyphLayout() const;

	Iterator		getIterator() const override { updateGlyphLayout(); return mGlyphLayout.getIterator(); }
	bool			nextRun( Iterator &iter, const Run** run, vec2 *lineDrawOffset ) const override { return mGlyphLayout.nextRun( iter, run, lineDrawOffset ); }
	vec2			calcSize() const override { return mGlyphLayout.calcSize(); }

  protected:
	void					updateGlyphLayout() const;
	void					updateGlyphLayoutImpl();

	bool					mDirty;
	GlyphLayout				mGlyphLayout;
	AttrString				mAttrString;
	TypesetOptions			mTypesetOptions;
	float					mInitialMargin, mBaselineOffset;
	Path2d					mPath;
};

CI_API std::ostream& operator<<( std::ostream& os, const Run& r );
CI_API std::ostream& operator<<( std::ostream& os, const Line& l );
CI_API std::ostream& operator<<( std::ostream& os, const GlyphLayout& f );

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

CI_API void		render( const Typesetter &typesetter, Surface8u *surface, const vec2 &offset = vec2(0), bool precise = false, bool srgb = true );
CI_API void		render( const Typesetter &typesetter, Channel8u *channel, const vec2 &offset = vec2(0), bool precise = false, bool srgb = true );
//! Creates a premultiplied Surface8u
CI_API Surface8u	renderSurface( const Typesetter &typesetter, const vec2 &offset = vec2(0), const ColorA8u &bgColor = ColorA8u(0, 0, 0, 0), bool precise = false, bool srgb = true );
CI_API Channel8u	renderChannel( const Typesetter &typesetter, const vec2 &offset = vec2(0), bool precise = false, bool srgb = true );

/*CI_API Channel8u	renderString( const Font *font, const char *utf8String, float tracking = 0 );
CI_API Channel8u	renderString( const AttrString &attrString ); // renders on one line
CI_API Channel8u	renderString( const AttrString &attrString, int32_t width, int32_t height, const TypesetOptions &options = TypesetOptions(), float *outBaseline = nullptr );*/


//! \a outBreaks size must be >= \a len, contains 0: must break, 1: allow break, 2: cannot break, 3: inside utf8/utf16 sequence, 4: indeterminate
CI_API void setLineBreaksUtf8( const char *str, size_t len, char *outBreaks );
//! \a outBreaks size must be >= \a len, contains 0: must break, 1: allow break, 2: cannot break, 4: indeterminate
CI_API void setLineBreaksUtf32( const char32_t *str, size_t len, char *outBreaks );

//! \a outBreaks size must be >= \a len, contains 0: must break, 1: cannot break, 3: inside utf8/utf16 sequence
CI_API void setWordBreaksUtf8( const char *str, size_t len, char *outBreaks );
//! \a outBreaks size must be >= \a len, contains 0: must break, 1: cannot break
CI_API void setWordBreaksUtf32( const char32_t *str, size_t len, char *outBreaks );

//! Used by function typeset()
class CI_API TypesetProcessor {
  public:
	virtual ~TypesetProcessor() {}

	virtual bool	addLine( Alignment justification, vec2 drawOffset, float ascender, float descender, float lineGap, float measuredWidth ) { return true; }
	virtual bool	addRun( const Font *font, const char32_t *utf32Str, size_t chLen, const std::vector<uint32_t> &clusters, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX, float measuredWidth, struct PlaceholderInfo *info ) { return true; }
	virtual void	finishLine() {}
	virtual void	finish() {}

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
