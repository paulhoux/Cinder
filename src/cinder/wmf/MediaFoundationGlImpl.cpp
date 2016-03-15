/*
Copyright (c) 2015, The Barbarian Group
All rights reserved.

Copyright (c) Microsoft Open Technologies, Inc. All rights reserved.

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
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/app/App.h"
#include "cinder/msw/ScopedPtr.h"
#include "cinder/wmf/MediaFoundationGlImpl.h"
#include "cinder/wmf/detail/MediaSink.h"
#include "cinder/wmf/detail/MFOptions.h"
#include "cinder/wmf/detail/Player.h"
#include "cinder/wmf/detail/Presenter.h"

#include "glload/wgl_all.h"

using namespace cinder::msw;

namespace cinder {
namespace wmf {

MovieGl::MovieGl()
	: MovieBase(), mObj( new Obj() )
{
	// Make sure this instance is properly destroyed before the window closes.
	mConnClose = app::App::get()->getWindow()->getSignalClose().connect( [&]() { close(); } );
}

MovieGl::MovieGl( const fs::path & path )
	: MovieGl()
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );

	CI_LOG_V( "Opening movie: " << path.string() );

	mObj->mPath = path;
	mObj->mPlayerPtr->OpenURL( msw::toWideString( path.string() ).c_str() );
	mObj->mDuration = mObj->mPlayerPtr->GetDuration() * 1.0e-7f;
}

MovieGl::~MovieGl()
{
	close();
}

float MovieGl::getCurrentTime() const
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );

	MFTIME t = 0L;

	HRESULT hr = mObj->mPlayerPtr->GetPosition( &t );
	if( SUCCEEDED( hr ) ) {
		return t * 1.0e-7f;
	}

	return 0.0f;
}

void MovieGl::seekToTime( float seconds )
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );

	seconds = ci::math<float>::clamp( seconds, 0.0f, mObj->mDuration );
	MFTIME t = MFTIME( seconds * 10000000L );

	mObj->mPlayerPtr->SetPosition( t );
}

void MovieGl::seekToFrame( int frame )
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
}

void MovieGl::seekToStart()
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );

	MFTIME t = MFTIME( 0L );
	mObj->mPlayerPtr->SetPosition( t );
}

void MovieGl::seekToEnd()
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );

	MFTIME t = MFTIME( mObj->mDuration * 10000000L );
	mObj->mPlayerPtr->SetPosition( t );
}

void MovieGl::setScrubbing( bool enabled )
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	mObj->mPlayerPtr->Scrub( (BOOL)enabled );
}

void MovieGl::setLoop( bool enabled )
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	mObj->mPlayerPtr->SetLoop( enabled );
}

bool MovieGl::isLooping() const
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	return ( mObj->mPlayerPtr->IsLooping() == TRUE );
}

bool MovieGl::isPlaying() const
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	return ( mObj->mPlayerPtr->IsPlaying() == TRUE );
}

bool MovieGl::isDone() const
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	return ( mObj->mPlayerPtr->IsStopped() == TRUE );
}

bool MovieGl::isScrubbing() const
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	return ( mObj->mPlayerPtr->IsScrubbing() == TRUE );
}

bool MovieGl::isDX9() const
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	return ( mObj->mPlayerPtr->IsDX9() == TRUE );
}

void MovieGl::play()
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	mObj->mPlayerPtr->Start();
}

void MovieGl::stop()
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );
	mObj->mPlayerPtr->Pause();
}

void MovieGl::draw()
{
	updateFrame();

	if( mTexture ) {
		gl::ScopedColor color( 1, 1, 1 );
		gl::draw( mTexture );
	}
}

void MovieGl::draw( const ci::Area &bounds )
{
	updateFrame();

	if( mTexture ) {
		gl::ScopedColor color( 1, 1, 1 );
		gl::draw( mTexture, bounds );
	}
}

const gl::Texture2dRef& MovieGl::getTexture()
{
	updateFrame();

	return mTexture;
}

void MovieGl::updateFrame()
{
	CI_ASSERT( mObj && mObj->mPlayerPtr );

	HRESULT hr = S_OK;

	// Generate GL texture ID.
	GLuint textureID = 0;
	::glGenTextures( 1, &textureID );

	do {
		BREAK_IF_TRUE( textureID == 0, E_FAIL );

		ScopedComPtr<PresenterDX9> pPresenterDX9;
		hr = mObj->mPlayerPtr->QueryInterface( __uuidof( PresenterDX9 ), (void**)&pPresenterDX9 );
		if( SUCCEEDED( hr ) ) {
			// Get video size in pixels.
			mObj->mWidth = pPresenterDX9->GetImageWidth();
			mObj->mHeight = pPresenterDX9->GetImageHeight();

			// Get latest video frame.
			ScopedComPtr<IDirect3DSurface9> pFrame;
			hr = pPresenterDX9->GetFrame( &pFrame );
			BREAK_ON_FAIL( hr );

			// Open Interop device.
			if( NULL == mObj->mDeviceHandle ) {
				ScopedComPtr<IDirect3DDevice9> pDevice;
				pFrame->GetDevice( &pDevice );
				BREAK_ON_NULL( pDevice, E_POINTER );

				mObj->mDeviceHandle = ::wglDXOpenDeviceNV( pDevice );
				BREAK_ON_NULL_MSG( mObj->mDeviceHandle, E_FAIL, "Failed to open DX/GL interop device." );
			}

			// Set share handle.
			HANDLE sharedHandle = NULL;
			DWORD sizeOfData = sizeof( sharedHandle );
			hr = pFrame->GetPrivateData( GUID_SharedHandle, &sharedHandle, &sizeOfData );
			BREAK_ON_FAIL_MSG( hr, "Failed to query shared handle." );

			BOOL shared = ::wglDXSetResourceShareHandleNV( (void*)pFrame, sharedHandle );
			//BREAK_IF_FALSE( shared, E_FAIL );

			// Register with OpenGL.
			HANDLE objectHandle = ::wglDXRegisterObjectNV( mObj->mDeviceHandle, (void*)pFrame, textureID, GL_TEXTURE_RECTANGLE, WGL_ACCESS_READ_ONLY_NV );
			if( NULL == objectHandle ) {
				BREAK_ON_NULL_MSG( objectHandle, E_FAIL, "Failed to register object. " << ERROR_STRING( ::GetLastError() ) );
			}

			// Lock object.
			BOOL locked = ::wglDXLockObjectsNV( mObj->mDeviceHandle, 1, &objectHandle );
			BREAK_IF_FALSE_MSG( locked, E_FAIL, "Failed to lock object." );

			// Create GL texture:
			//  1) obtain a pointer to the DX texture.
			auto deviceHandle = mObj->mDeviceHandle;
			IDirect3DSurface9* framePtr = pFrame.get();
			framePtr->AddRef();
			//  2) keep a reference to the player. That way, the player will be around until the last texture has been destroyed.
			auto obj = mObj;
			//  3) create a texture wrapper.
			D3DSURFACE_DESC desc;
			pFrame->GetDesc( &desc );

			ci::gl::Texture2dRef texture = ci::gl::Texture2d::create( GL_TEXTURE_RECTANGLE, textureID, desc.Width, desc.Height, false, [obj, pPresenterDX9, deviceHandle, objectHandle, framePtr]( gl::Texture* tex ) {
				BOOL unlocked = ::wglDXUnlockObjectsNV( deviceHandle, 1, const_cast<HANDLE*>( &objectHandle ) );
				BOOL unregistered = ::wglDXUnregisterObjectNV( deviceHandle, objectHandle );

				if( framePtr ) {
					HRESULT hr = pPresenterDX9->ReturnFrame( const_cast<IDirect3DSurface9**>( &framePtr ) );
				}

				delete tex;
			} );
			texture->setTopDown( true );
			//  4) assign texture to our member variable.
			mTexture = texture;
		}
		else {
			ScopedComPtr<PresenterDX11> pPresenterDX11;
			hr = mObj->mPlayerPtr->QueryInterface( __uuidof( PresenterDX11 ), (void**)&pPresenterDX11 );
			if( SUCCEEDED( hr ) ) {
				// Get video size in pixels.
				mObj->mWidth = pPresenterDX11->GetImageWidth();
				mObj->mHeight = pPresenterDX11->GetImageHeight();

				// Get latest video frame.
				ScopedComPtr<ID3D11Texture2D> pFrame;
				hr = pPresenterDX11->GetFrame( &pFrame );
				BREAK_ON_FAIL( hr );

				// Open Interop device.
				if( NULL == mObj->mDeviceHandle ) {
					ScopedComPtr<ID3D11Device> pDevice;
					pFrame->GetDevice( &pDevice );
					BREAK_ON_NULL( pDevice, E_POINTER );

					mObj->mDeviceHandle = ::wglDXOpenDeviceNV( pDevice );
					BREAK_ON_NULL_MSG( mObj->mDeviceHandle, E_FAIL, "Failed to open DX/GL interop device." );
				}

				// Register with OpenGL.
				HANDLE objectHandle = ::wglDXRegisterObjectNV( mObj->mDeviceHandle, (void*)pFrame, textureID, GL_TEXTURE_RECTANGLE, WGL_ACCESS_READ_ONLY_NV );
				if( NULL == objectHandle ) {
					BREAK_ON_NULL_MSG( objectHandle, E_FAIL, "Failed to register object. " << ERROR_STRING( ::GetLastError() ) );
				}

				// Lock object.
				BOOL locked = ::wglDXLockObjectsNV( mObj->mDeviceHandle, 1, &objectHandle );
				BREAK_IF_FALSE_MSG( locked, E_FAIL, "Failed to lock object." );

				// Create GL texture:
				//  1) obtain a pointer to the DX texture.
				auto deviceHandle = mObj->mDeviceHandle;
				ID3D11Texture2D* framePtr = pFrame.get();
				framePtr->AddRef();
				//  2) keep a reference to the player. That way, the player will be around until the last texture has been destroyed.
				auto obj = mObj;
				//  3) create a texture wrapper.
				D3D11_TEXTURE2D_DESC desc;
				pFrame->GetDesc( &desc );

				ci::gl::Texture2dRef texture = ci::gl::Texture2d::create( GL_TEXTURE_RECTANGLE, textureID, desc.Width, desc.Height, false, [obj, pPresenterDX11, deviceHandle, objectHandle, framePtr]( gl::Texture* tex ) {
					BOOL unlocked = ::wglDXUnlockObjectsNV( deviceHandle, 1, const_cast<HANDLE*>( &objectHandle ) );
					BOOL unregistered = ::wglDXUnregisterObjectNV( deviceHandle, objectHandle );

					if( framePtr ) {
						HRESULT hr = pPresenterDX11->ReturnFrame( const_cast<ID3D11Texture2D**>( &framePtr ) );
					}

					delete tex;
				} );
				texture->setTopDown( true );
				//  4) assign texture to our member variable.
				assert( texture.get() != mTexture.get() );
				mTexture = texture;
			}
		}
	} while( FALSE );

	if( FAILED( hr ) ) {
		// Close interop device. We'll open it again on the next successful frame.
		if( mObj->mDeviceHandle ) {
			BOOL closed = ::wglDXCloseDeviceNV( mObj->mDeviceHandle );
			mObj->mDeviceHandle = NULL;
		}

		// Delete any generated texture ID.
		if( textureID != 0 )
			::glDeleteTextures( 1, &textureID );
	}
}

void MovieGl::close()
{
	if( !( mObj && mObj->mPlayerPtr ) )
		return;

	// Pause the player.
	stop();

	CI_LOG_V( "Closing movie: " << mObj->mPath.string() );

	// Reset texture.
	mTexture.reset();

	mObj->mPlayerPtr->Close();

	// Reset player object.
	if( !mObj.unique() )
		CI_LOG_W( "All references to video textures should be released before the GL context is destroyed." );

	mObj.reset();
}

MovieGl::Obj::Obj()
	: mPlayerPtr( NULL )
	, mDeviceHandle( NULL )
{
	MFOptions options;

	// Check availability of interop extensions.
	if( !wglext_NV_DX_interop2 ) {
		CI_LOG_V( "NV_DX_interop2 extension not available, trying DX9..." );
		options.setDXVersion( MFOptions::DX_9 );
	}

	if( !wglext_NV_DX_interop ) {
		throw std::exception( "NV_DX_interop extension not available!" );
	}

	// Create player.
	mPlayerPtr = new Player( options ); // Created with ref count = 1.
	if( NULL == mPlayerPtr )
		throw std::exception( "Out of memory" );

	// Note: interop device handle will be created when
	//       we receive the first video frame.
}

MovieGl::Obj::~Obj()
{
	// Close interop device.
	if( mDeviceHandle ) {
		BOOL closed = ::wglDXCloseDeviceNV( mDeviceHandle );
		mDeviceHandle = NULL;
	}

	// Close player.
	if( mPlayerPtr )
		mPlayerPtr->Close();

	// Release player.
	msw::SafeRelease( mPlayerPtr );
}

}
}

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )

