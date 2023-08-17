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
#include "cinder/text/Face.h"
#include "cinder/CinderAssert.h"

#include <string>

#include <freetype/ft2build.h>
#include <freetype/freetype.h>
#include <freetype/ftmm.h>
#include <freetype/ftsnames.h>
#include <freetype/ftcolor.h>
#include <freetype/ftoutln.h>
#include <hb.h>
#include <hb-ft.h>

using namespace std;

namespace cinder { namespace text {

Face::Face( FT_Face face )
	: mFtFace( face )
{
	mHbFace = hb_ft_face_create_referenced( face );
	if( ! mHbFace )
		throw HarfBuzzExc();

	if( hasVariations() ) {
		FT_Get_MM_Var( mFtFace, &mFtMmVar );
		std::vector<FT_Fixed> defaultValues; // FT_Fixed
		for( size_t v = 0; v < mFtMmVar->num_axis; ++v ) {
			mVariationAxes.push_back( { string( mFtMmVar->axis[v].name ), (int32_t)mFtMmVar->axis[v].minimum, (int32_t)mFtMmVar->axis[v].def, (int32_t)mFtMmVar->axis[v].maximum, (uint32_t)mFtMmVar->axis[v].tag } );
			defaultValues.push_back( (int32_t)mFtMmVar->axis[v].def );
		}

		// construct default variation
		mDefaultVariation = Variation( this, defaultValues );

		for( size_t v = 0; v < mFtMmVar->num_namedstyles; ++v ) {
			std::vector<FT_Fixed> axisValues;
			for( size_t a = 0; a < mVariationAxes.size(); ++a )
				axisValues.push_back( (int32_t)mFtMmVar->namedstyle[v].coords[a] );
			mVariationNamedStyles.push_back( { findName( mFtMmVar->namedstyle[v].strid ), Variation( this, axisValues ) } );
		}
	}
	else
		mDefaultVariation = Variation( this );
}

Face::~Face()
{
	if( mHbFace )
		hb_face_destroy( mHbFace );
	if( mFtMmVar )
		FT_Done_MM_Var( text::Manager::get()->getFtLibrary(), mFtMmVar );
	FT_Done_Face( mFtFace );
}

// TODO: improve this
Face* Face::clone() const
{
	FT_Face ftFace;
	FT_Error error = FT_New_Face( Manager::get()->getFtLibrary(), mFilePath.string().c_str(), mFaceIndex, &ftFace );
	if( error )
	 	throw FreeTypeExc( error );
	
	return new Face( ftFace );
}

std::string Face::findName( uint16_t nameId ) const
{
    FT_UInt nameCount = FT_Get_Sfnt_Name_Count( mFtFace );
    FT_Int candidate = -1;

    for( FT_UInt i = 0; i < nameCount; i++ ) {
        FT_SfntName sfntName;
        FT_Get_Sfnt_Name( mFtFace, i, &sfntName );
        if( sfntName.name_id == nameId )
            return std::string( (const char*)sfntName.string, sfntName.string_len );
    }

    return std::string();
}

const Face::Variation* Face::findVariationByName( const std::string &name ) const
{
	for( auto &style : mVariationNamedStyles )
		if( style.getName() == name )
			return &style.getVariation();

	return nullptr;
}

int32_t Face::getNumGlyphs() const
{
	return (int32_t)mFtFace->num_glyphs;
}

uint32_t Face::getCharIndex( uint32_t utf32Char ) const
{
	return FT_Get_Char_Index( mFtFace, utf32Char );
}

std::string Face::getFamilyName() const
{
	return std::string( mFtFace->family_name );
}

std::string	Face::getStyleName() const
{
	return std::string( mFtFace->style_name );
}

bool Face::hasColor() const
{
	return (mFtFace->face_flags & FT_FACE_FLAG_COLOR) != 0;
}

bool Face::hasVariations() const
{
	return (mFtFace->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS) != 0;
}

vector<int32_t> Face::getFixedSizes() const
{
	vector<int32_t> result;
	for( FT_Int i = 0; i < mFtFace->num_fixed_sizes; ++i ) {
		FT_Bitmap_Size *f = &mFtFace->available_sizes[i]; 
		result.push_back( f->height );
	}
	
	return result;
}

namespace {

struct FtShape2dData {
	cinder::Shape2d		mShape;
	float				mUnitScale;
};

int ftShape2dMoveTo( const FT_Vector *to, void *user )
{
	Shape2d *shape = &static_cast<FtShape2dData *>( user )->mShape;
	float    unitScale = static_cast<FtShape2dData *>( user )->mUnitScale;
	shape->moveTo( float( to->x ) * unitScale, float( to->y ) * unitScale );
	return 0;
}

int ftShape2dLineTo( const FT_Vector *to, void *user )
{
	Shape2d *shape = &static_cast<FtShape2dData *>( user )->mShape;
	float    unitScale = static_cast<FtShape2dData *>( user )->mUnitScale;
	shape->lineTo( float( to->x ) * unitScale, float( to->y ) * unitScale );
	return 0;
}

int ftShape2dConicTo( const FT_Vector *control, const FT_Vector *to, void *user )
{
	Shape2d *shape = &static_cast<FtShape2dData *>( user )->mShape;
	float    unitScale = static_cast<FtShape2dData *>( user )->mUnitScale;
	shape->quadTo( float( control->x ) * unitScale, float( control->y ) * unitScale, float( to->x ) * unitScale, float( to->y ) * unitScale );
	return 0;
}

int ftShape2dCubicTo( const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user )
{
	Shape2d *shape = &static_cast<FtShape2dData *>( user )->mShape;
	float    unitScale = static_cast<FtShape2dData *>( user )->mUnitScale;
	shape->curveTo( float( control1->x ) * unitScale, float( control1->y ) * unitScale, float( control2->x ) * unitScale, float( control2->y ) * unitScale, float( to->x ) * unitScale, float( to->y ) * unitScale );
	return 0;
}
} // namespace

cinder::Shape2d Face::getGlyphShape( uint32_t glyphIndex ) const
{
	lock();

	FT_Load_Glyph( mFtFace, glyphIndex, FT_LOAD_NO_HINTING | FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP );
	FT_Outline       outline = mFtFace->glyph->outline;
	FT_Outline_Funcs funcs;
	funcs.move_to = ftShape2dMoveTo;
	funcs.line_to = ftShape2dLineTo;
	funcs.conic_to = ftShape2dConicTo;
	funcs.cubic_to = ftShape2dCubicTo;
	funcs.shift = 0;
	funcs.delta = 0;

	FtShape2dData userData;
	userData.mUnitScale = 1.0f / float( mFtFace->units_per_EM );
	FT_Outline_Decompose( &outline, &funcs, &userData );
	if( userData.mShape.getNumContours() )
		userData.mShape.close();
	userData.mShape.scale( vec2( 1, -1 ) );

	unlock();

	return userData.mShape;
}

void Face::setRendererData( uint16_t rendererId, Face::Data *data ) const
{
	for( auto &r : mRendererData )
		if( r.first == rendererId ) {
			r.second = std::shared_ptr<Face::Data>( data );
			return;
		}

	mRendererData.push_back( std::make_pair( rendererId, std::shared_ptr<Face::Data>( data ) ) );
}

Face::Data* Face::getRendererData( uint16_t rendererId ) const
{
	for( auto &r : mRendererData )
		if( r.first == rendererId )
			return r.second.get();

	return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Face::Variation

Face::Variation::Variation( Face *face )
	: mFace( face ), mFixedValues( face->getDefaultVariation().getFixedValues() )
{
}

Face::Variation::Variation( Face *face, const std::vector<float> floatAxisValues )
	: mFace( face )
{
	CI_ASSERT( floatAxisValues.size() == face->getVariationAxes().size() );
	for( size_t f = 0; f < floatAxisValues.size(); ++f )
		mFixedValues.push_back( (int32_t)(floatAxisValues[f] * 65536) );
}

Face::Variation::Variation( Face *face, const std::vector<signed long> fixedAxisValues )
	: mFace( face ), mFixedValues( fixedAxisValues )
{
	CI_ASSERT( fixedAxisValues.size() == face->getVariationAxes().size() );
}

std::vector<float> Face::Variation::getFloatValues() const
{
	std::vector<float> result;
	result.reserve( mFixedValues.size() );
	for( size_t f = 0; f < mFixedValues.size(); ++f )
		result.push_back( mFixedValues[f] / 65536.0f );
	return result;
}

size_t Face::Variation::getAxisIndex( const std::string &axisName ) const
{
	auto &axes = mFace->getVariationAxes();
	for( size_t index = 0; index < axes.size(); ++index )
		if( axes[index].getName() == axisName )
			return index;

	throw InvalidVariationAxisName();
}

bool Face::Variation::operator==( const Variation &rhs ) const
{
	if( mFace != rhs.mFace ) return false;
	if( ! mFace && ! rhs.mFace ) return true;
	if( mFace != rhs.mFace ) return false;
	return mFixedValues == rhs.mFixedValues;
}

std::ostream& operator<<( std::ostream& os, const Face::Variation& v )
{
	for( size_t axis = 0; axis < v.getFace()->getVariationAxes().size(); ++axis )
		os << "[\"" << v.getFace()->getVariationAxes()[axis].getName() << "\" " << v.getAxisValue( axis ) << "]";
	return os;
}

} } // namespace cinder::text
