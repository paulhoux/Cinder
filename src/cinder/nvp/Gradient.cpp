/*
Copyright (c) 2022, Paul Houx Creative Coding - All rights reserved.
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

#include "cinder/nvp/Gradient.h"

#include "cinder/Utilities.h"
#include "cinder/gl/Context.h"

namespace cinder {
namespace nvp {

GradientUnits toGradientUnits( std::string style )
{
	style = trim( style );
	if( style == "userSpaceOnUse" )
		return GradientUnits::USER_SPACE_ON_USE;
	return GradientUnits::DEFAULT;
}

GradientSpreadMethod toSpreadMethod( std::string style )
{
	style = trim( toLower( style ) );
	if( style == "reflect" )
		return GradientSpreadMethod::REFLECT;
	if( style == "repeat" )
		return GradientSpreadMethod::REPEAT;
	return GradientSpreadMethod::DEFAULT;
}

const GradientRef &Gradients::at( const std::string &id ) const
{
	if( mLookUp.count( id ) )
		return mGradients.at( mLookUp.at( id ) );

	static const GradientRef kEmpty;
	return kEmpty;
}

float Gradients::index( const std::string &id ) const
{
	if( !mTexture || !mLookUp.count( id ) )
		return 0.0f;

	return ( float( mLookUp.at( id ) ) + 0.5f ) / float( mTexture->getHeight() );
}

void Gradients::set( const GradientRef &gradient )
{
	if( !mLookUp.count( gradient->getId() ) ) {
		mLookUp.insert_or_assign( gradient->getId(), mGradients.size() );
		mDirty.insert( mGradients.size() );
		mGradients.emplace_back( gradient );
	}
	else if( auto index = mLookUp.at( gradient->getId() ); *mGradients.at( index ) != *gradient ) {
		mDirty.insert( index );
		mGradients.at( index ) = gradient;
	}
}

gl::Texture2dRef Gradients::getTexture() const
{
	if( !mTexture )
		mTexture = create( 128, 1 );

	if( !mDirty.empty() ) {
		// Resize texture if more space is needed.
		if( const auto size = int( mGradients.size() ); mTexture->getHeight() < size ) {
			const auto source = Surface8u( mTexture->createSource() );
			const auto texture = create( mTexture->getWidth(), int( isPowerOf2( size ) ? size : nextPowerOf2( size ) ) );
			texture->update( source );

			mTexture = texture;
		}

		// Update gradients.
		for( const auto index : mDirty ) {
			const auto data = mGradients.at( index )->data( 128, 1 );
			mTexture->update( data.get(), GL_RGBA, GL_UNSIGNED_BYTE, 0, 128, 1, { 0, index } );
		}

		// Done.
		mDirty.clear();
	}

	return mTexture;
}

gl::Texture2dRef Gradients::create( int width, int height ) const
{
	static const gl::Texture2d::Format FORMAT = gl::Texture2d::Format().internalFormat( GL_RGBA ).target( GL_TEXTURE_2D ).loadTopDown();

	gl::Texture2dRef texture = gl::Texture2d::create( width, height, FORMAT );
	return texture;
}

} // namespace nvp
} // namespace cinder