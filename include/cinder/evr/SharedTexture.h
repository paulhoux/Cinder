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

#pragma once

#include "cinder/Cinder.h"

#if defined( CINDER_MSW )

#include "cinder/gl/gl.h" // included to avoid error C2120 when including "wgl_all.h"
#include "cinder/gl/Texture.h"

#include "glload/wgl_all.h"

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/CinderMswCom.h"

//#include <mmsystem.h> // for timeBeginPeriod and timeEndPeriod

#include <unordered_map>
#include <deque>
#include <thread>

#include <d3d9.h>
#include <d3d9types.h>
//#include <dxva2api.h>
#include <shobjidl.h> 
#include <shlwapi.h>

// Media Foundation headers.
//#include <mfapi.h>
//#include <mfidl.h>
//#include <mferror.h>
//#include <evr.h>

// Include these libraries.
#pragma comment(lib, "winmm.lib")
#pragma comment(lib,"d3d9.lib")
//#pragma comment(lib,"dxva2.lib")
//#pragma comment (lib,"evr.lib")

namespace cinder {
	namespace msw {
		namespace video {

			typedef std::unordered_map<GLuint, class SharedTexture> SharedTextureList;
			typedef std::deque<GLuint> SharedTextureIDList;

			class SharedTexture {
			public:
				SharedTexture()
					: m_hD3DSurface( NULL ), m_pD3DSurface( NULL ), m_pD3DTexture( NULL ), locked( FALSE ) {}
				SharedTexture( IDirect3DDevice9Ex *pDevice, const D3DSURFACE_DESC &desc )
				{
					HRESULT hr = S_OK;

					do {
						BREAK_ON_NULL( pDevice, E_POINTER );

						hr = pDevice->CreateOffscreenPlainSurface( desc.Width, desc.Height, desc.Format, D3DPOOL_DEFAULT, &m_pD3DSurface, &m_hD3DSurface );
						BREAK_ON_FAIL( hr );
					} while( false );
				}

				SharedTexture( const SharedTexture &other )
					: SharedTexture()
				{
					name = other.name;
					value = other.value;
					locked = other.locked;

					m_hGLTexture = other.m_hGLTexture;
					m_pGLTexture = other.m_pGLTexture;

					m_hD3DSurface = other.m_hD3DSurface;
					CopyComPtr( m_pD3DSurface, other.m_pD3DSurface );
					CopyComPtr( m_pD3DTexture, other.m_pD3DTexture );
				}

				~SharedTexture()
				{
					SafeRelease( m_pD3DSurface );
					SafeRelease( m_pD3DTexture );
				}

				SharedTexture& operator=( SharedTexture &&other )
				{
					std::swap( name, other.name );
					std::swap( value, other.value );
					std::swap( locked, other.locked );

					std::swap( m_hGLTexture, other.m_hGLTexture );
					std::swap( m_pGLTexture, other.m_pGLTexture );

					std::swap( m_hD3DSurface, other.m_hD3DSurface );
					std::swap( m_pD3DSurface, other.m_pD3DSurface );
					std::swap( m_pD3DTexture, other.m_pD3DTexture );

					return *this;
				}

				explicit 

			private:
				GLuint name;
				DWORD  value;
				BOOL   locked;

				// OpenGL texture and handle
				HANDLE             m_hGLTexture;
				gl::Texture2dRef   m_pGLTexture;

				// DirectX texture and handle
				HANDLE             m_hD3DSurface;
				IDirect3DSurface9* m_pD3DSurface;
				IDirect3DTexture9* m_pD3DTexture;
			};

			class SharedTexturePool {
			public:
				~SharedTexturePool()
				{
					resetDevice();
				}

				static SharedTexturePool& instance()
				{
					static SharedTexturePool instance;
					return instance;
				}

				bool setDevice( IDirect3DDevice9Ex *pDevice )
				{
					assert( m_pDevice == NULL ); // Only allow the device to be set once.
					assert( pDevice != NULL ); // Make sure a valid pointer was passed.

					m_pDevice = pDevice;
					m_pDevice->AddRef();

					// Make sure OpenGL interop is initialized.
					m_pD3DDeviceHandle = wglDXOpenDeviceNV( m_pDevice );
					if( !m_pD3DDeviceHandle )
						return false;

					return true;
				}

				void resetDevice()
				{
					// Release all textures.
					mSharedTextureFreeIDs.clear();
					mSharedTextures.clear();

					// Close OpenGL interop.
					if( m_pD3DDeviceHandle ) {
						wglDXCloseDeviceNV( m_pD3DDeviceHandle );
						m_pD3DDeviceHandle = NULL;
					}

					SafeRelease( m_pDevice );
				}

				bool createTexture( int width, int height );

				SharedTexture& getOldestTexture( int width, int height )
				{
					// Make sure the D3D device was set & OpenGL interop is available.
					assert( m_pDevice );
					assert( m_pD3DDeviceHandle );

					ScopedCriticalSection lock( mSharedTexturesLock );

					// Check if there are free textures.
					if( !mSharedTextureFreeIDs.empty() ) {
						GLuint textureID = mSharedTextureFreeIDs.front();
						mSharedTextureFreeIDs.pop_front();
						return mSharedTextures[textureID];
					}

					// Create a new shared texture if there aren't.
					if( createTexture( width, height ) ) {
						GLuint textureID = mSharedTextureFreeIDs.front();
						mSharedTextureFreeIDs.pop_front();
						return mSharedTextures[textureID];
					}

					return SharedTexture();
				}

				void freeTexture( const SharedTexture& texture )
				{
					// Check if texture is part of this pool.
					if( mSharedTextures.find( texture.name ) != mSharedTextures.end() ) {
						mSharedTextureFreeIDs.push_back( texture.name );
					}
				}

			private:
				SharedTexturePool() : m_pDevice( NULL ), m_pD3DDeviceHandle( NULL )
				{
					// Keep track of main thread.
					mMainThreadID = std::this_thread::get_id();

					// Define the default texture format.
					mFormat.setTarget( GL_TEXTURE_RECTANGLE );
				}

			private:
				IDirect3DDevice9Ex    *m_pDevice;
				HANDLE                 m_pD3DDeviceHandle;     // Shared device handle for OpenGL interop.

				int                    mWidth;                 // Width of all shared textures.
				int                    mHeight;                // Height of all shared textures.

				gl::Texture2d::Format  mFormat;

				CriticalSection        mSharedTexturesLock;

				SharedTextureList      mSharedTextures;
				SharedTextureIDList    mSharedTextureFreeIDs;

				std::thread::id        mMainThreadID;
			};

			class ScopedSharedTexture {
			public:
				ScopedSharedTexture( int width, int height )
					: mTexture( SharedTexturePool::instance().getOldestTexture( width, height ) )
				{
				}

				~ScopedSharedTexture()
				{
					SharedTexturePool::instance().freeTexture( mTexture );
				}

				SharedTexture& operator()() { return mTexture; }

			private:
				SharedTexture& mTexture;
			};

			inline bool SharedTexturePool::createTexture( int width, int height )
			{
				// Only allow texture creation on the main thread.
				if( std::this_thread::get_id() != mMainThreadID )
					return false;

				// Sanity check.
				if( !mSharedTextures.empty() ) {
					// Texture size should match.
					assert( mWidth == width );
					assert( mHeight == height );
				}
				else {
					// Set texture size.
					mWidth = width;
					mHeight = height;
				}

				// Create OpenGL texture.
				SharedTexture shared;

				shared.pGLTexture = gl::Texture2d::create( mWidth, mHeight, mFormat );
				shared.name = shared.pGLTexture->getId();

				// Share it with DirectX.
				HANDLE pSharedHandle = NULL;
				HRESULT hr = m_pDevice->CreateTexture( width, height, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &( shared.pD3DTexture ), &pSharedHandle );

				ULONG rc = GetRefCount( shared.pD3DTexture );

				if( FAILED( hr ) || !pSharedHandle )
					return false;

				if( !wglDXSetResourceShareHandleNV( shared.pD3DTexture, pSharedHandle ) )
					return false;

				shared.handle = wglDXRegisterObjectNV( m_pD3DDeviceHandle, shared.pD3DTexture, shared.name, GL_TEXTURE_RECTANGLE, WGL_ACCESS_READ_ONLY_NV );
				if( !shared.handle ) {
					switch( GetLastError() ) {
					case ERROR_INVALID_HANDLE:
						//CI_LOG_E( "No GL context is made current to the calling thread." );
						break;
					case ERROR_INVALID_DATA:
						//CI_LOG_E( "Incorrect <name> <type> or <access> parameters." );
						break;
					case ERROR_OPEN_FAILED:
						//CI_LOG_E( "Opening the Direct3D resource failed." );
						break;
					default:
						//CI_LOG_E( "Failed to register objects: " << GetLastError() );
						break;
					}
					return false;
				}

				shared.pD3DTexture->GetSurfaceLevel( 0, &( shared.pD3DSurface ) );

				rc = GetRefCount( shared.pD3DTexture );
				rc = GetRefCount( shared.pD3DSurface );

				// Now lock it once, it's an expensive operation. We'll promise to make sure we're not writing to the texture while OpenGL is using it.
				if( wglDXLockObjectsNV( m_pD3DDeviceHandle, 1, &( shared.handle ) ) ) {
					// Store texture in map and make it available for use.
					mSharedTextures[shared.name] = shared;
					mSharedTextureFreeIDs.push_back( shared.name );

					return true;
				}

				return false;
			}

		} // namespace video
	} // namespace msw
} // namespace cinder

#endif // CINDER_MSW