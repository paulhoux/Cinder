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
#include "cinder/evr/DirectShowPlayer.h"
#include "cinder/evr/SharedTexture.h"

#include "cinder/app/AppBasic.h"

namespace cinder {
	namespace msw {
		namespace video {

			MovieGl::MovieGl()
				: MovieBase(), mPreferredBackend( BE_MEDIA_FOUNDATION )
			{
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

			void MovieGl::init( const std::wstring &url )
			{
				HRESULT hr = E_FAIL;

				// Try preferred backend first.
				if( mPreferredBackend != BE_UNKNOWN ) {
					if( initPlayer( mPreferredBackend ) )
						hr = mPlayer->OpenFile( url.c_str() );
				}

				// Try all other backends next.
				if( FAILED( hr ) ) {
					for( int i = 0; i < BE_COUNT; ++i ) {
						if( i != mPreferredBackend && initPlayer( (Backend) i ) ) {
							hr = mPlayer->OpenFile( url.c_str() );
							if( SUCCEEDED( hr ) )
								break;
						}
					}
				}

				if( FAILED( hr ) ) {
					CI_LOG_E( "Failed to open movie: " << url.c_str() );
					return;
				}

				// Get width and height of the video.
				mWidth = mPlayer->GetWidth();
				mHeight = mPlayer->GetHeight();
			}

			bool MovieGl::initPlayer( Backend backend )
			{
				if( mPlayer && mCurrentBackend == backend )
					return true;

				HRESULT hr = S_OK;

				SafeRelease( mPlayer );

				assert( mHwnd != NULL );

				switch( backend ) {
				case BE_MEDIA_FOUNDATION:
					// Try to play the movie using Media Foundation.
					mPlayer = new MediaFoundationPlayer( hr, mHwnd );
					mPlayer->AddRef();
					break;
				case BE_DIRECTSHOW:
					// Try to play the movie using Media Foundation.
					mPlayer = new DirectShowPlayer( hr, mHwnd );
					mPlayer->AddRef();
					break;
				}

				if( FAILED( hr ) ) {
					mCurrentBackend = BE_UNKNOWN;
					SafeRelease( mPlayer );
					return false;
				}
				else {
					mCurrentBackend = backend;
					return true;
				}
			}

			gl::Texture2dRef MovieGl::getTexture()
			{
				// Update texture.
				if( mPlayer ) {
					auto texture = mPlayer->GetTexture();
					if( texture )
						mTexture = texture;
				}

				return mTexture;
			}

		} // namespace video
	} // namespace msw
} // namespace cinder

#endif // CINDER_MSW