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

#if defined( CINDER_MSW )

#include "cinder/evr/EnhancedVideoRendererImpl.h"

#include "cinder/gl/gl.h"
#include "cinder/gl/Texture.h"

namespace cinder {
namespace evr {

typedef std::shared_ptr<class MovieGl>	MovieGlRef;
/** \brief Movie playback as OpenGL textures through the WGL_NV_DX_interop extension.
*	Textures are always bound to the \c GL_TEXTURE_RECTANGLE_ARB target
**/
class MovieGl : public MovieBase {
public:
	virtual ~MovieGl();

	static MovieGlRef create( const Url& url ) { return msw::makeComShared<MovieGl>( new MovieGl( url ) ); }
	static MovieGlRef create( const fs::path& path ) { return msw::makeComShared<MovieGl>( new MovieGl( path ) ); }
	//static MovieGlRef create( const MovieLoaderRef &loader ) { return msw::makeComShared<MovieGl>( new MovieGl( *loader ) ); }

	//! \inherit
	virtual bool hasAlpha() const { /*TODO*/ return false; }

	//! Returns the gl::Texture representing the Movie's current frame, bound to the \c GL_TEXTURE_RECTANGLE_ARB target
	gl::TextureRef	getTexture();

protected:
	MovieGl();
	MovieGl( const Url& url );
	MovieGl( const fs::path& path );
	//MovieGl( const MovieLoader& loader );

protected:
	gl::TextureRef           mTexture;
};

}
} // namespace cinder::evr

#endif // CINDER_MSW