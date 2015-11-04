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

#include "cinder/wmf/MediaFoundationGlImpl.h"
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/app/App.h"

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
	if( mObj && mObj->mPlayerPtr ) {
		mObj->mPlayerPtr->OpenURL( msw::toWideString( path.string() ).c_str() );
		mObj->mDuration = mObj->mPlayerPtr->GetDuration() * 1.0e-7f;
	}
}

MovieGl::~MovieGl()
{
	close();
}

float MovieGl::getCurrentTime() const
{
	MFTIME t = 0L;

	if( mObj && mObj->mPlayerPtr ) {
		HRESULT hr = mObj->mPlayerPtr->GetPosition( &t );
		if( SUCCEEDED( hr ) )
			return t * 1.0e-7f;
	}

	return 0.0f;
}

void MovieGl::seekToTime( float seconds )
{
	if( !( mObj && mObj->mPlayerPtr ) )
		return;

	seconds = ci::math<float>::clamp( seconds, 0.0f, mObj->mDuration );

	MFTIME t = MFTIME( seconds * 10000000L );
	mObj->mPlayerPtr->SetPosition( t );
}

void MovieGl::seekToFrame( int frame )
{
	//if( mObj && mObj->mPlayerPtr )
	//	mObj->mPlayerPtr->
}

void MovieGl::seekToStart()
{
	if( !( mObj && mObj->mPlayerPtr ) )
		return;

	MFTIME t = MFTIME( 0L );
	mObj->mPlayerPtr->SetPosition( t );
}

void MovieGl::seekToEnd()
{
	if( !( mObj && mObj->mPlayerPtr ) )
		return;

	MFTIME t = MFTIME( mObj->mDuration * 10000000L );
	mObj->mPlayerPtr->SetPosition( t );
}

void MovieGl::setLoop( bool enabled )
{
	if( mObj && mObj->mPlayerPtr )
		mObj->mPlayerPtr->SetLoop( enabled );
}

bool MovieGl::isPlaying() const
{
	if( mObj && mObj->mPlayerPtr )
		return ( mObj->mPlayerPtr->GetState() == Player::Started );
	else
		return false;
}

bool MovieGl::isDone() const
{
	if( mObj && mObj->mPlayerPtr )
		return ( mObj->mPlayerPtr->GetState() == Player::Stopped );
	else
		return true;
}

void MovieGl::play()
{
	if( mObj && mObj->mPlayerPtr )
		mObj->mPlayerPtr->Start();
}

void MovieGl::stop()
{
	if( mObj && mObj->mPlayerPtr )
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
	HRESULT hr = S_OK;

	// Generate GL texture ID.
	GLuint textureID = 0;
	::glGenTextures( 1, &textureID );

	do {
		BREAK_ON_NULL( mObj, E_POINTER );
		BREAK_ON_NULL( mObj->mPlayerPtr, E_POINTER );

		BREAK_IF_TRUE( textureID == 0, E_FAIL );

		ScopedComPtr<PresenterDX9> pPresenterDX9;
		hr = mObj->mPlayerPtr->QueryInterface( __uuidof( PresenterDX9 ), (void**)&pPresenterDX9 );
		if( SUCCEEDED( hr ) ) {
			// Get latest video frame.
			ScopedComPtr<IDirect3DSurface9> pFrame;
			hr = pPresenterDX9->GetFrame( &pFrame );
			BREAK_ON_FAIL( hr );

			D3DSURFACE_DESC desc;
			pFrame->GetDesc( &desc );

			mObj->mWidth = desc.Width;
			mObj->mHeight = desc.Height;

			// Open Interop device.
			if( NULL == mObj->mDeviceHandle ) {
				ScopedComPtr<IDirect3DDevice9> pDevice;
				pFrame->GetDevice( &pDevice );
				BREAK_ON_NULL( pDevice, E_POINTER );

				mObj->mDeviceHandle = ::wglDXOpenDeviceNV( pDevice );
				BREAK_ON_NULL_MSG( mObj->mDeviceHandle, E_POINTER, "Failed to open DX/GL interop device." );
			}

			// Set share handle.
			HANDLE sharedHandle = NULL;
			DWORD sizeOfData = sizeof( sharedHandle );
			hr = pFrame->GetPrivateData( GUID_SharedHandle, &sharedHandle, &sizeOfData );
			BREAK_ON_FAIL( hr );

			BOOL shared = ::wglDXSetResourceShareHandleNV( (void*)pFrame, sharedHandle );
			//BREAK_IF_FALSE( shared, E_FAIL );

			// Register with OpenGL.
			HANDLE objectHandle = ::wglDXRegisterObjectNV( mObj->mDeviceHandle, (void*)pFrame, textureID, GL_TEXTURE_RECTANGLE, WGL_ACCESS_READ_ONLY_NV );
			if( NULL == objectHandle ) {
				DWORD err = ::GetLastError();
				switch( err ) {
					case ERROR_INVALID_HANDLE:
						CI_LOG_E( "ERROR_INVALID_HANDLE" );
						break;
					case ERROR_INVALID_DATA:
						CI_LOG_E( "ERROR_INVALID_DATA" );
						break;
					case ERROR_OPEN_FAILED:
						CI_LOG_E( "ERROR_OPEN_FAILED" );
						break;
					default:
						CI_LOG_E( "Unknown error" );
						break;
				}
				BREAK_ON_NULL( hr, E_FAIL );
			}

			// Lock object.
			BOOL locked = ::wglDXLockObjectsNV( mObj->mDeviceHandle, 1, &objectHandle );
			BREAK_IF_FALSE( locked, E_FAIL );

			// Create GL texture:
			//  1) obtain a pointer to the DX texture.
			auto deviceHandle = mObj->mDeviceHandle;
			IDirect3DSurface9* framePtr = pFrame.get();
			framePtr->AddRef();
			//  2) keep a reference to the player. That way, the player will be around until the last texture has been destroyed.
			auto obj = mObj;
			//  3) create a texture wrapper.
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
				// Get latest video frame.
				ScopedComPtr<ID3D11Texture2D> pFrame;
				hr = pPresenterDX11->GetFrame( &pFrame );
				BREAK_ON_FAIL( hr );

				D3D11_TEXTURE2D_DESC desc;
				pFrame->GetDesc( &desc );

				mObj->mWidth = desc.Width;
				mObj->mHeight = desc.Height;

				// Open Interop device.
				if( NULL == mObj->mDeviceHandle ) {
					ScopedComPtr<ID3D11Device> pDevice;
					pFrame->GetDevice( &pDevice );
					BREAK_ON_NULL( pDevice, E_POINTER );

					mObj->mDeviceHandle = ::wglDXOpenDeviceNV( pDevice );
					BREAK_ON_NULL_MSG( mObj->mDeviceHandle, E_POINTER, "Failed to open DX/GL interop device." );
				}

				// Register with OpenGL.
				HANDLE objectHandle = ::wglDXRegisterObjectNV( mObj->mDeviceHandle, (void*)pFrame, textureID, GL_TEXTURE_RECTANGLE, WGL_ACCESS_READ_ONLY_NV );
				if( NULL == objectHandle ) {
					DWORD err = ::GetLastError();
					switch( err ) {
						case ERROR_INVALID_HANDLE:
							CI_LOG_E( "ERROR_INVALID_HANDLE" );
							break;
						case ERROR_INVALID_DATA:
							CI_LOG_E( "ERROR_INVALID_DATA" );
							break;
						case ERROR_OPEN_FAILED:
							CI_LOG_E( "ERROR_OPEN_FAILED" );
							break;
						default:
							CI_LOG_E( "Unknown error" );
							break;
					}
					BREAK_ON_NULL( hr, E_FAIL );
				}

				// Lock object.
				BOOL locked = ::wglDXLockObjectsNV( mObj->mDeviceHandle, 1, &objectHandle );
				BREAK_IF_FALSE( locked, E_FAIL );

				// Create GL texture:
				//  1) obtain a pointer to the DX texture.
				auto deviceHandle = mObj->mDeviceHandle;
				ID3D11Texture2D* framePtr = pFrame.get();
				framePtr->AddRef();
				//  2) keep a reference to the player. That way, the player will be around until the last texture has been destroyed.
				auto obj = mObj;
				//  3) create a texture wrapper.
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
				mTexture = texture;
			}
		}
	} while( FALSE );

	if( FAILED( hr ) ) {
		// Delete any generated texture ID.
		if( textureID != 0 )
			::glDeleteTextures( 1, &textureID );
	}
}

void MovieGl::close()
{
	// Pause the player.
	stop();

	// Reset texture.
	mTexture.reset();

	// Reset player object.
	if( mObj && !mObj.unique() )
		CI_LOG_W( "All references to video textures should be released before the GL context is destroyed." );

	mObj.reset();
}

}
}

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )

