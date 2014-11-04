/*
Copyright (c) 2014, The Cinder Project, All rights reserved.

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

// This file is only meant to be included by EnhancedVideoRenderer.h
#pragma once

#include "cinder/Cinder.h"
#include "cinder/Timer.h"

#if defined( CINDER_MSW )

#include "cinder/evr/RendererImpl.h"

#include "cinder/gl/gl.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/Texture.h"

namespace cinder {
namespace msw {
namespace video {

typedef std::shared_ptr<class MovieGl>	MovieGlRef;
/** \brief Movie playback as OpenGL textures through the WGL_NV_DX_interop extension.
*	Textures are always bound to the \c GL_TEXTURE_RECTANGLE_ARB target
**/
class MovieGl : public MovieBase {
public:
	virtual ~MovieGl();

	static MovieGlRef create( const Url& url ) { return std::shared_ptr<MovieGl>( new MovieGl( url ) ); }
	static MovieGlRef create( const fs::path& path ) { return std::shared_ptr<MovieGl>( new MovieGl( path ) ); }
	//static MovieGlRef create( const MovieLoaderRef &loader ) { return std::shared_ptr<MovieGl>( new MovieGl( *loader ) ); }

	//!
	virtual bool hasAlpha() const override { /*TODO*/ return false; }

	//! Returns the gl::Texture representing the Movie's current frame, bound to the \c GL_TEXTURE_RECTANGLE_ARB target
	//gl::TextureRef	getTexture();

	void draw( int x = 0, int y = 0 )
	{
		draw( x, y, mWidth, mHeight );
	}

	void draw( int x, int y, int w, int h )
	{
		if( !mTexture )
			return;

		static ci::Timer timer( true );

		// TEMP
		if( !mShader ) {
			gl::GlslProg::Format fmt;
			fmt.vertex(
				"#version 150\n"
				""
				"uniform mat4 ciModelViewProjection;\n"
				"in vec4 ciPosition;\n"
				"in vec2 ciTexCoord0;\n"
				"out vec2 vTexCoord0;\n"
				""
				"void main(void) {\n"
				"	gl_Position = ciModelViewProjection * ciPosition;\n"
				"	vTexCoord0 = ciTexCoord0;\n"
				"}"
				);
			fmt.fragment(
				"#version 150\n"
				""
				"uniform sampler2DRect uTex0;\n"
				"uniform float uTime;\n"
				"in vec2 vTexCoord0;\n"
				"out vec4 oColor;\n"
				""
				"void main(void) {\n"
				"	vec2 uv = vTexCoord0;\n"
				"	uv.x += 10 * sin(uTime * 10.0 + 0.1 * uv.y);\n"
				"	oColor = texture( uTex0, uv );\n"
				"}"
				);

			try {
				mShader = gl::GlslProg::create( fmt );
			}
			catch( const std::exception &e ) {
				CI_LOG_V( e.what() );
			}
		}
		/*
		if( mEVRPresenter && mEVRPresenter->lockSharedTexture() ) {
			//gl::draw( mTexture, Rectf( x, y, x+w, y+h ) );

			gl::ScopedTextureBind tex0( mTexture );
			gl::ScopedGlslProg shader( mShader );
			mShader->uniform( "uTex0", 0 );
			mShader->uniform( "uTime", float( timer.getSeconds() ) );
			gl::drawSolidRect( Rectf( x, y, x + w, y + h ), vec2( 0, 0 ), vec2( mTexture->getWidth(), mTexture->getHeight() ) );

			mEVRPresenter->unlockSharedTexture();
		}*/
	}

protected:
	MovieGl();
	MovieGl( const Url& url );
	MovieGl( const fs::path& path );

	void  initFromUrl( const Url& url ) override;
	void  initFromPath( const fs::path& filePath ) override;
	//MovieGl( const MovieLoader& loader );

	//virtual HRESULT createPartialTopology( IMFPresentationDescriptor *pPD ) override;

protected:
	gl::TextureRef           mTexture;
	gl::GlslProgRef          mShader;
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW