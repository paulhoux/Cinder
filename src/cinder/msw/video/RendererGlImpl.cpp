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

#if defined( CINDER_MSW )

#include "cinder/gl/gl.h" // included to avoid error C2120 when including "wgl_all.h"
#include "glload/wgl_all.h"

#include "cinder/msw/video/RendererGlImpl.h"
#include "cinder/msw/video/MediaFoundationPlayer.h"
#include "cinder/msw/video/DirectShowPlayer.h"

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
{
	init( toWideString( url.c_str() ) );
}

MovieGl::MovieGl( const fs::path& filePath )
{
	init( filePath.generic_wstring() );
}

void MovieGl::init( const std::wstring& url )
{
	HRESULT hr = S_OK;

	assert( mHwnd != NULL );

	// Create the renderer.
	//	enum { Try_EVR, Try_VMR9, Try_VMR7 };

	//for( DWORD i = Try_EVR; i <= Try_VMR7; i++ ) {
	SafeDelete( mRenderer );

	//	switch( i ) {
	//	case Try_EVR:
	//		CI_LOG_V( "Trying EVR..." );
	mRenderer = new ( std::nothrow ) RendererEVR();
	//		break;

	//	case Try_VMR9:
	//		CI_LOG_V( "Trying VMR9..." );
	//		mRenderer = new ( std::nothrow ) RendererVMR9();
	//		break;

	//	case Try_VMR7:
	//		CI_LOG_V( "Trying VMR7..." );
	//		mRenderer = new ( std::nothrow ) RendererVMR7();
	//		break;
	//	}

	// Create the player.
	for( int i = 0; i < BE_COUNT; ++i ) {
		SafeRelease( mPlayer );

		if( i == BE_MEDIA_FOUNDATION ) {
			// Try to play the movie using Media Foundation.
			mPlayer = new MediaFoundationPlayer( hr, mHwnd );
			mPlayer->AddRef();
			if( SUCCEEDED( hr ) ) {
				hr = mPlayer->SetVideoRenderer( mRenderer );
				if( SUCCEEDED( hr ) ) {
					hr = mPlayer->OpenFile( url.c_str() );
					if( SUCCEEDED( hr ) ) {
						mCurrentBackend = (PlayerBackends) i;
						break;
					}
				}
			}
		}
		else if( i == BE_DIRECTSHOW ) {
			// Try to play the movie using DirectShow.
			mPlayer = new DirectShowPlayer( hr, mHwnd );
			mPlayer->AddRef();
			if( SUCCEEDED( hr ) ) {
				hr = mPlayer->SetVideoRenderer( mRenderer );
				if( SUCCEEDED( hr ) ) {
					hr = mPlayer->OpenFile( url.c_str() );
					if( SUCCEEDED( hr ) ) {
						mCurrentBackend = (PlayerBackends) i;
						break;
					}
				}
			}
		}
	}

	//
	//	if( SUCCEEDED( hr ) )
	//		break;
	//}

	if( FAILED( hr ) ) {
		mCurrentBackend = BE_UNKNOWN;

		SafeRelease( mPlayer );
		SafeDelete( mRenderer );
		CI_LOG_E( "Failed to play movie: " << url.c_str() );

		return;
	}

	// Get width and height of the video.
	mWidth = mPlayer->GetWidth();
	mHeight = mPlayer->GetHeight();

	// Delete existing shared textures if their size is different.
	for( auto itr = mTextures.rbegin(); itr != mTextures.rend(); ++itr ) {
		gl::Texture2dRef texture = itr->second;
		if( texture && ( mWidth != texture->getWidth() || mHeight != texture->getHeight() ) ) {
			mPlayer->ReleaseSharedTexture( texture->getId() );
			mTextures.erase( texture->getId() );
		}
	}

	// Create shared textures.
	for( size_t i = 0; i < 3 - mTextures.size(); ++i ) {
		gl::Texture2d::Format fmt;
		fmt.setTarget( GL_TEXTURE_RECTANGLE );
		fmt.loadTopDown( true );

		gl::Texture2dRef texture = gl::Texture2d::create( mWidth, mHeight, fmt );
		if( mPlayer->CreateSharedTexture( mWidth, mHeight, texture->getId() ) )
			mTextures[texture->getId()] = texture;
	}
}

void MovieGl::draw( float x, float y, float w, float h )
{
	int textureID;

	if( mPlayer && mPlayer->CheckNewFrame() ) {
		if( mPlayer->LockSharedTexture( &textureID ) ) {
			gl::draw( mTextures[textureID], Rectf( x, y, x + w, y + h ) );
			mPlayer->UnlockSharedTexture( textureID );
		}
	}
}

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW