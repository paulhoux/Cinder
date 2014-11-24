/*
Copyright (c) 2014, The Barbarian Group
All rights reserved.

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

#include "cinder/gl/gl.h" // included to avoid error C2120 when including "wgl_all.h"
#include "glload/wgl_all.h"

#if defined( CINDER_MSW )

#include "cinder/evr/RendererGlImpl.h"
#include "cinder/evr/MediaFoundationPlayer.h"

namespace cinder {
namespace msw {
namespace video {

MovieGl::MovieGl()
	: MovieBase()
{
	if( !wglext_NV_DX_interop ) {
		throw std::runtime_error( "WGL_NV_DX_interop extension not supported. Upgrade your graphics drivers and try again." );
	}
}

MovieGl::~MovieGl()
{
}

MovieGl::MovieGl( const Url& url )
	: MovieGl()
{
	MovieBase::initFromUrl( url );
}

MovieGl::MovieGl( const fs::path& filePath )
	: MovieGl()
{
	MovieBase::initFromPath( filePath );
}

void MovieGl::initFromUrl( const Url& url )
{
	MovieBase::initFromUrl( url );
}

void MovieGl::initFromPath( const fs::path& filePath )
{
	MovieBase::initFromPath( filePath );
}

void MovieGl::draw( int x, int y, int w, int h )
{
	if( mTexture[mTextureIndex] ){
		if( mPlayer && mPlayer->CheckNewFrame() ) {
			if( mPlayer->LockSharedTexture( mTexture[mTextureIndex]->getId() ) ) {
				gl::draw( mTexture[mTextureIndex], Rectf( x, y, x + w, y + h ) );
				mPlayer->UnlockSharedTexture( mTexture[mTextureIndex]->getId() );
				//mTextureIndex = ( mTextureIndex + 1) % 2;
			}
		}
	}
}

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW