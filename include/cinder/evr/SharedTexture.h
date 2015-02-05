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
#include "cinder/Log.h"

#if defined( CINDER_MSW )

#include "cinder/gl/gl.h" // included to avoid error C2120 when including "wgl_all.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/Texture.h"

#include "glload/wgl_all.h"

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/CinderMswCom.h"

#include <map>
#include <thread>

#include <d3d9.h>
#include <d3d9types.h>
#include <shobjidl.h> 
#include <shlwapi.h>

// Include these libraries.
#pragma comment(lib, "winmm.lib")
#pragma comment(lib,"d3d9.lib")
//#pragma comment(lib,"dxva2.lib")
//#pragma comment (lib,"evr.lib")

namespace cinder {
	namespace msw {
		namespace video {

			typedef std::shared_ptr<class SharedTexture> SharedTextureRef;
			typedef std::shared_ptr<class SharedTexturePool> SharedTexturePoolRef;

			class SharedTexture : public std::enable_shared_from_this < SharedTexture > {
			public:
				SharedTexture( const SharedTexturePoolRef &pool, const D3DSURFACE_DESC &desc );

				SharedTexture( const SharedTexture &other )
				{
					m_pPool = other.m_pPool;

					m_uTextureId = other.m_uTextureId;
					m_hGLHandle = other.m_hGLHandle;
					m_pGLTexture = other.m_pGLTexture;

					m_hD3DHandle = other.m_hD3DHandle;
					msw::CopyComPtr( m_pD3DSurface, other.m_pD3DSurface );
					msw::CopyComPtr( m_pD3DTexture, other.m_pD3DTexture );
				}

				SharedTexture( SharedTexture &&other )
				{
					m_pPool.swap( other.m_pPool );

					std::swap( m_uTextureId, other.m_uTextureId );
					std::swap( m_hGLHandle, other.m_hGLHandle );
					m_pGLTexture.swap( other.m_pGLTexture );

					std::swap( m_hD3DHandle, other.m_hD3DHandle );
					std::swap( m_pD3DSurface, other.m_pD3DSurface );
					std::swap( m_pD3DTexture, other.m_pD3DTexture );
				}

				~SharedTexture()
				{
					Unlock();
					Unshare();

					SafeRelease( m_pD3DSurface );
					SafeRelease( m_pD3DTexture );
				}

				SharedTexture& operator=( SharedTexture &other ) throw( )
				{
					m_pPool = other.m_pPool;

					m_uTextureId = other.m_uTextureId;
					m_hGLHandle = other.m_hGLHandle;
					m_pGLTexture = other.m_pGLTexture;

					m_hD3DHandle = other.m_hD3DHandle;
					msw::CopyComPtr( m_pD3DSurface, other.m_pD3DSurface );
					msw::CopyComPtr( m_pD3DTexture, other.m_pD3DTexture );

					return *this;
				}

				SharedTexture& operator=( SharedTexture &&other ) throw( )
				{
					m_pPool.swap( other.m_pPool );

					std::swap( m_uTextureId, other.m_uTextureId );
					std::swap( m_hGLHandle, other.m_hGLHandle );
					m_pGLTexture.swap( other.m_pGLTexture );

					std::swap( m_hD3DHandle, other.m_hD3DHandle );
					std::swap( m_pD3DSurface, other.m_pD3DSurface );
					std::swap( m_pD3DTexture, other.m_pD3DTexture );

					return *this;
				}

				//! Returns TRUE if the texture is shared with OpenGL.
				bool IsShared() const { return ( m_hGLHandle != NULL ); }

				bool Share();
				bool Unshare();

				//! Returns TRUE if the texture is in use by OpenGL.
				bool IsLocked() const { return m_bLocked == TRUE; }

				gl::Texture2dRef Lock();
				void Unlock();

				HRESULT BlitTo( IDirect3DDevice9Ex *pDevice, IDirect3DSurface9 *pSurface );

			private:
				std::weak_ptr<SharedTexturePool>  m_pPool;

				// OpenGL texture and handle
				GLuint                            m_uTextureId;
				HANDLE                            m_hGLHandle;
				std::weak_ptr<gl::Texture2d>      m_pGLTexture;

				// DirectX texture and handle
				HANDLE                            m_hD3DHandle;
				IDirect3DSurface9*                m_pD3DSurface;
				IDirect3DTexture9*                m_pD3DTexture;

				// State
				BOOL                              m_bLocked;
			};

			class SharedTexturePool : public std::enable_shared_from_this < SharedTexturePool > {
			public:
				SharedTexturePool( IDirect3DDevice9Ex *pDevice )
					: m_pDevice( pDevice ), m_pD3DDeviceHandle( NULL )
				{
					assert( m_pDevice != NULL );

					// We're gonna use this device.
					m_pDevice->AddRef();

					// Make sure OpenGL interop is initialized.
					if( wglext_NV_DX_interop ) {
						CI_LOG_V( "Opening DX/GL interop for device " << m_pDevice );
						m_pD3DDeviceHandle = wglDXOpenDeviceNV( m_pDevice );
					}

					if( m_pD3DDeviceHandle == NULL ) {
						throw std::runtime_error( "WGL_NV_DX_interop extension not supported. Upgrade your graphics drivers and try again." );
					}
				}

				~SharedTexturePool()
				{
					std::lock_guard<std::mutex> lock1( mFreeLock );
					std::lock_guard<std::mutex> lock2( mAvailableLock );

					// Release all textures and surfaces. ( now is the time to actually call wglDXUnregisterObjectNV() & glDeleteTextures() )
					mFree.reset();
					mAvailable.reset();

					// The following textures are still in use, so don't delete them, just unlock and unregister them.
					mUsed.clear();

					// Close OpenGL interop.
					if( m_pD3DDeviceHandle ) {
						CI_LOG_V( "Closing DX/GL interop for device " << m_pDevice << "..." );

						BOOL success = wglDXCloseDeviceNV( m_pD3DDeviceHandle );
						if( success )
							CI_LOG_V( "Succeeded." );
						else
							CI_LOG_V( "Failed." );

						m_pD3DDeviceHandle = NULL;
					}

					SafeRelease( m_pDevice );
				}

				IDirect3DDevice9Ex* GetDevice() const { assert( m_pDevice != NULL ); return m_pDevice; }
				HANDLE GetDeviceHandle() const { assert( m_pD3DDeviceHandle != NULL ); return m_pD3DDeviceHandle; }

				//! Will be called from a separate thread controlled by the presenter engine.
				HRESULT GetFreeSurface( const D3DSURFACE_DESC &desc, SharedTextureRef &shared )
				{
					HRESULT hr = S_OK;

					// Check if we have a surface that we can reuse, if not then create one.
					do {
						std::lock_guard<std::mutex> lock( mAvailableLock );

						if( mAvailable ) {
							shared = mAvailable;
							mAvailable.reset();

							return hr;
						}
					} while( false );

					do {
						std::lock_guard<std::mutex> lock( mFreeLock );
						if( !mFree ) {
							shared = std::make_shared<SharedTexture>( shared_from_this(), desc );
						}
						else {
							shared = mFree;
							mFree.reset();
						}
					} while( false );

					return hr;
				}

				HRESULT AddAvailableSurface( const SharedTextureRef &shared )
				{
					std::lock_guard<std::mutex> lock( mAvailableLock );
					mAvailable = shared;

					return S_OK;
				}

				//! Call from the main thread only, or from a thread that shares an OpenGL context with the main thread.
				gl::Texture2dRef GetTexture()
				{
					SharedTextureRef shared;

					do {
						std::lock_guard<std::mutex> lock( mAvailableLock );

						if( !mAvailable )
							return gl::Texture2dRef();

						shared = mAvailable;
						mAvailable.reset();
					} while( false );

					auto texture = shared->Lock();
					if( texture )
						mUsed[texture->getId()] = shared;

					return texture;
				}

				void FreeUsedTexture( gl::Texture2d *pTexture )
				{
					// Find corresponding texture in mUsed.
					auto itr = mUsed.find( pTexture->getId() );
					if( itr != mUsed.end() ) {
						SharedTextureRef shared = itr->second;
						mUsed.erase( itr );

						shared->Unlock();

						std::lock_guard<std::mutex> surlock( mFreeLock );
						if( mFree ) {
							// Destroy texture.
							pTexture->setDoNotDispose( false );
							delete pTexture;
						}
						else {
							// Reuse texture.
							mFree = shared;

							// Don't actually delete the texture id, we're gonna reuse it later. But we do need to notify the gl::context() we're done using the Texture2d wrapper.
							pTexture->setDoNotDispose( true );

							auto ctx = gl::context();
							if( ctx )
								ctx->textureDeleted( pTexture );

							delete pTexture;
						}
					}
				}

			private:
				IDirect3DDevice9Ex            *m_pDevice;
				HANDLE                         m_pD3DDeviceHandle;     // Shared device handle for OpenGL interop.

				std::thread::id                mMainThreadID;

				std::mutex                     mFreeLock;
				SharedTextureRef               mFree;

				std::mutex                     mAvailableLock;
				SharedTextureRef               mAvailable;

				std::map<GLuint, SharedTextureRef>  mUsed;				// Surfaces in use as textures. Used on the main thread only.
			};
		} // namespace video
	} // namespace msw
} // namespace cinder

#endif // CINDER_MSW