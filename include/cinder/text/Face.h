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
#include "cinder/Buffer.h"
#include "cinder/Filesystem.h"
#include "cinder/Exception.h"
#include "cinder/Shape2d.h"

#include <vector>

typedef struct FT_FaceRec_		*FT_Face;
typedef struct FT_MM_Var_		FT_MM_Var;
typedef struct hb_face_t 		hb_face_t;

namespace cinder { namespace text {

class Manager;

class CI_API Face {
  public:
	struct Variation;

  	~Face();
  	
	int32_t			getNumGlyphs() const;
	std::string		getFamilyName() const;
	std::string		getStyleName() const;

	bool			hasColor() const;
	bool			hasVariations() const;

	//! Returns a font-relative index for UTF-32 codepoint \a utf32Char. Returns \c 0 if the font cannot represent \a utf32Char
	uint32_t		getCharIndex( uint32_t utf32Char ) const;
	
	FT_Face			getFtFace() const { return mFtFace; }
	hb_face_t*		getHbFace() { return mHbFace; }
	//! returns \c true if the Face was loaded from system fonts rather than an external source
	bool			getSystemFace() const { return mSystemFace; }

	//! Returns the available fixed pixel sizes. Only relevant in bitmap fonts.
	std::vector<int32_t>	getFixedSizes() const;

	//! Returns the unit size of the face. Useful when scaling glyphs.
	unsigned short getUnitsPerEm() const;

	void			lock() const {}
	void			unlock() const {}
	
	//! This be empty if the Face was constructed from memory rather than disk
	ci::fs::path	getFilePath() const{ return mFilePath; }
	int				getFaceIndex() const { return mFaceIndex; }

	Variation			getDefaultVariation() const { return mDefaultVariation; }
	const Variation*	findVariationByName( const std::string &name ) const;

	static Face*		getSystemDefault();

	struct CI_API VariationAxis {
		std::string		getName() const { return mName; }
		float			getMinimum() const { return mMin / 65536.0f; }
		float			getDefault() const { return mDefault / 65536.0f; }
		float			getMaximum() const { return mMax / 65536.0f; }
		uint32_t		getTag() const { return mTag; }

		std::string		mName;
		int32_t			mMin, mDefault, mMax; // 16.16 fixed point
		uint32_t		mTag;
	};

	struct CI_API Variation {
		Variation() : mFace( nullptr ) {}
		Variation( Face *face );
		Variation( Face *face, const std::vector<float> floatAxisValues );
		Variation( Face *face, const std::vector<signed long> fixedAxisValues );

		float			getAxisValue( size_t axis ) const { return mFixedValues[axis] / 65536.0f; }
		void			setAxisValue( size_t axis, float value ) { mFixedValues[axis] = (int32_t)(value * 65536); }
		float			getAxisValue( const VariationAxis &axis ) const { return getAxisValue( getAxisIndex( axis.getName() ) ); }
		void			setAxisValue( const VariationAxis &axis, float value ) { setAxisValue( getAxisIndex( axis.getName() ), value ); }
		const Face*		getFace() const { return mFace; }

		size_t			getAxisIndex( const std::string &axisName ) const;

		std::vector<signed long>	getFixedValues() const { return mFixedValues; } // FT_Fixed is signed long
		std::vector<float>			getFloatValues() const;

		bool operator==( const Variation &rhs ) const;

	  private:
		const Face					*mFace;
		std::vector<signed long>	mFixedValues;
	};

	struct CI_API VariationNamedStyle {
		std::string		getName() const { return mName; }

		const Variation&	getVariation() const { return mVariation; }

		std::string		mName;
		Variation		mVariation;
	};

	const std::vector<VariationAxis>&			getVariationAxes() const { return mVariationAxes; }
	const std::vector<VariationNamedStyle>&		getVariationNamedStyles() const { return mVariationNamedStyles; }

	//! Returns a Shape2d containing the outline for glyph \a glyphIndex, in font units. Note that this index is not a Unicode codepoint, and can be obtained with \a getCharIndex().
	cinder::Shape2d			getGlyphShape( uint32_t glyphIndex ) const;
	
	//! Function called by the getGlyphOutline and getGlyphOutlines methods. See nvp::Face::createPaths() for an example of how to use these.
	using MoveToFn = int ( * )( const ivec2 *, void * );
	using LineToFn = int ( * )( const ivec2 *, void * );
	using QuadToFn = int ( * )( const ivec2 *, const ivec2 *, void * );
	using CubicToFn = int ( * )( const ivec2 *, const ivec2 *, const ivec2 *, void * );
	using RestartFn = void ( * )( void * );
	struct OutlineFunctions {
		MoveToFn  moveTo;
		LineToFn  lineTo;
		QuadToFn  quadTo;
		CubicToFn cubicTo;
		RestartFn restart;
	};
	//! Obtain the outline of the glyph with the specified \a glyphIndex. See nvp::Face::createPaths() for an example of how to use this method.
	void getGlyphOutline( uint32_t glyphIndex, const OutlineFunctions &functions, void *user ) const;
	//! Obtain the outline of \a count glyphs, starting at \a startIndex. See nvp::Face::createPaths() for an example of how to use this method.
	void getGlyphOutlines( uint32_t startIndex, uint32_t count, const OutlineFunctions &functions, void *user ) const;

	struct CI_API Data {
		virtual ~Data() {}
	};

	void		setRendererData( uint16_t rendererId, Data *data ) const;
	Data*		getRendererData( uint16_t rendererId ) const;

	//! returns a BufferRef containing the raw Face data
	BufferRef	getDataBuffer() const { return mDataBuffer; }

	Face*		clone() const;

  private:
  	Face( FT_Face face );

	std::string		findName( uint16_t nameId ) const;

	void		setFilePath( const fs::path &path, int faceIndex ) { mFilePath = path; mFaceIndex = faceIndex; }

	FT_Face			mFtFace;
	hb_face_t		*mHbFace = nullptr;

	FT_MM_Var							*mFtMmVar = nullptr;
	std::vector<VariationAxis>			mVariationAxes;
	std::vector<VariationNamedStyle>	mVariationNamedStyles;
	Variation							mDefaultVariation;

	fs::path	mFilePath;
	int			mFaceIndex;
	BufferRef	mDataBuffer;
	bool		mSystemFace = false;

	mutable std::vector<std::pair<uint16_t,std::shared_ptr<Data>>>	mRendererData;

	friend Manager;
};

class CI_API InvalidVariationAxisName : public cinder::Exception {
};

std::ostream& operator<<( std::ostream &os, const Face::Variation &v );

} } // namespace cinder::text
