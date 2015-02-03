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

#define USE_OFFSCREEN_PLAIN_SURFACE 1

namespace cinder {
	namespace msw {
		namespace video {

			class SharedTexture {
			public:
				SharedTexture() : m_uTextureId( ~0 ), m_hGLHandle( NULL ), m_hD3DHandle( NULL ), m_pD3DSurface( NULL ), m_pD3DTexture( NULL )
				{
					myUuid = ++uuid;
					++count;
					CI_LOG_V( "Created SharedTexture " << myUuid << " (" << count << " in total)" );
				}

				SharedTexture( const SharedTexture &other ) : SharedTexture()
				{
					CI_LOG_V( "Copy constructor called from " << other.myUuid << " to " << myUuid );

					m_uTextureId = other.m_uTextureId;
					m_hGLHandle = other.m_hGLHandle;
					m_pGLTexture = other.m_pGLTexture;

					m_hD3DHandle = other.m_hD3DHandle;
					msw::CopyComPtr( m_pD3DSurface, other.m_pD3DSurface );
					msw::CopyComPtr( m_pD3DTexture, other.m_pD3DTexture );
				}

				SharedTexture( SharedTexture &&other ) : SharedTexture()
				{
					CI_LOG_V( "Move constructor called from " << other.myUuid << " to " << myUuid );

					std::swap( m_uTextureId, other.m_uTextureId );
					std::swap( m_hGLHandle, other.m_hGLHandle );
					std::swap( m_pGLTexture, other.m_pGLTexture );

					std::swap( m_hD3DHandle, other.m_hD3DHandle );
					std::swap( m_pD3DSurface, other.m_pD3DSurface );
					std::swap( m_pD3DTexture, other.m_pD3DTexture );
				}

				~SharedTexture()
				{
					--count;

					CI_LOG_V( "Destroying SharedTexture " << myUuid << " (" << count << " in total)" );
					SafeRelease( m_pD3DSurface );
					SafeRelease( m_pD3DTexture );
				}

				SharedTexture& operator=( SharedTexture &other ) throw( )
				{
					CI_LOG_V( "Copy operator called from " << other.myUuid << " to " << myUuid );

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
					CI_LOG_V( "Move operator called from " << other.myUuid << " to " << myUuid );

					std::swap( m_uTextureId, other.m_uTextureId );
					std::swap( m_hGLHandle, other.m_hGLHandle );
					std::swap( m_pGLTexture, other.m_pGLTexture );

					std::swap( m_hD3DHandle, other.m_hD3DHandle );
					std::swap( m_pD3DSurface, other.m_pD3DSurface );
					std::swap( m_pD3DTexture, other.m_pD3DTexture );

					return *this;
				}

				bool operator==( GLuint textureId )
				{
					return ( m_uTextureId == textureId );
				}

				bool operator!=( GLuint textureId )
				{
					return !( *this == textureId );
				}

				// OpenGL texture and handle
				GLuint                        m_uTextureId;
				HANDLE                        m_hGLHandle;
				std::weak_ptr<gl::Texture2d>  m_pGLTexture;

				// DirectX texture and handle
				HANDLE                        m_hD3DHandle;
				IDirect3DSurface9*            m_pD3DSurface;
				IDirect3DTexture9*            m_pD3DTexture;

				static int uuid, count;
				int myUuid;
			};

			class SharedTexturePool : public std::enable_shared_from_this < SharedTexturePool > {
			public:
				SharedTexturePool( IDirect3DDevice9Ex *pDevice )
					: m_pDevice( pDevice ), m_pD3DDeviceHandle( NULL )
				{
					assert( m_pDevice != NULL );

					//
					m_pDevice->AddRef();

					// Make sure OpenGL interop is initialized.
					if( !wglext_NV_DX_interop ) {
						throw std::runtime_error( "WGL_NV_DX_interop extension not supported. Upgrade your graphics drivers and try again." );
					}
					else {
						CI_LOG_V( "Opening DX/GL interop" );
						m_pD3DDeviceHandle = wglDXOpenDeviceNV( m_pDevice );
						if( m_pD3DDeviceHandle == NULL ) {
							DWORD err = GetLastError(); // ERROR_OPEN_FAILED or ERROR_NOT_SUPPORTED
							throw std::runtime_error( "WGL_NV_DX_interop extension not supported. Upgrade your graphics drivers and try again." );
						}
					}
				}

				~SharedTexturePool()
				{
					std::lock_guard<std::mutex> lock1( mFreeLock );
					std::lock_guard<std::mutex> lock2( mAvailableLock );
					std::lock_guard<std::mutex> lock3( mUsedLock );

					// Release all textures and surfaces. ( now is the time to actually call wglDXUnregisterObjectNV() & glDeleteTextures() )
					for( auto &shared : mFree ) {
						if( shared.m_hGLHandle != NULL ) {
							CI_LOG_V( "Unregistering surface " << shared.m_hGLHandle );
							BOOL success = wglDXUnregisterObjectNV( m_pD3DDeviceHandle, shared.m_hGLHandle );
						}
						if( glIsTexture( shared.m_uTextureId ) ) {
							glDeleteTextures( 1, &( shared.m_uTextureId ) );
						}
					}

					for( auto &shared : mAvailable ) {
						if( shared.m_hGLHandle != NULL ) {
							CI_LOG_V( "Unregistering texture " << shared.m_hGLHandle );
							BOOL success = wglDXUnregisterObjectNV( m_pD3DDeviceHandle, shared.m_hGLHandle );
						}
						if( glIsTexture( shared.m_uTextureId ) ) {
							glDeleteTextures( 1, &( shared.m_uTextureId ) );
						}
					}

					// The following textures are still in use, so don't delete them, just unlock and unregister them (?)
					for( auto &shared : mUsed ) {
						if( shared.m_hGLHandle != NULL ) {
							CI_LOG_V( "Unlocking texture " << shared.m_hGLHandle );
							BOOL success = wglDXUnlockObjectsNV( m_pD3DDeviceHandle, 1, &( shared.m_hGLHandle ) );
						}
						if( shared.m_hGLHandle != NULL ) {
							CI_LOG_V( "Unregistering texture " << shared.m_hGLHandle );
							BOOL success = wglDXUnregisterObjectNV( m_pD3DDeviceHandle, shared.m_hGLHandle );
						}
					}

					mFree.clear();
					mAvailable.clear();
					mUsed.clear();

					// Close OpenGL interop.
					if( m_pD3DDeviceHandle ) {
						try {
							CI_LOG_V( "Closing DX/GL interop..." );
							BOOL success = wglDXCloseDeviceNV( m_pD3DDeviceHandle );
							CI_LOG_V( "Succeeded." );
						}
						catch( const std::exception &e ) {
							CI_LOG_E( "Failed: " << e.what() );
						}
						m_pD3DDeviceHandle = NULL;
					}

					SafeRelease( m_pDevice );
				}

				bool IsCompatibleWith( const D3DSURFACE_DESC &desc )
				{
					return true; //  ( desc.Width == mWidth && desc.Height == mHeight );
				}

				bool IsCompatibleWith( int width, int height )
				{
					return true; //  ( width == mWidth && height == mHeight );
				}

				//! Will be called from a separate thread controlled by the presenter engine.
				HRESULT GetFreeSurface( const D3DSURFACE_DESC &desc, SharedTexture *pShared )
				{
					HRESULT hr = S_OK;

					do {
						// Check if we have a surface that we can reuse, if not then create one.
						std::lock_guard<std::mutex> lock( mFreeLock );

						do {
							// First, try to reuse an available surface if there are more than 1.
							std::lock_guard<std::mutex> lock( mAvailableLock );

							while( mAvailable.size() > 1 ) {
								SharedTexture s = mAvailable[0];

								mAvailable.erase( mAvailable.begin() );
								mFree.push_back( s );
							}
						} while( false );

						if( mFree.empty() ) {
							CI_LOG_V( "Creating new surface to render to." );

							SharedTexture s;
#if USE_OFFSCREEN_PLAIN_SURFACE
							// Create D3DSurface only.
							hr = m_pDevice->CreateOffscreenPlainSurface( desc.Width, desc.Height, /*D3DFMT_A8R8G8B8*/ desc.Format, D3DPOOL_DEFAULT, &( s.m_pD3DSurface ), &( s.m_hD3DHandle ) );
							BREAK_ON_FAIL( hr );
#else
							// Create both a D3DSurface and D3DTexture.
							hr = m_pDevice->CreateTexture( desc.Width, desc.Height, 1, D3DUSAGE_RENDERTARGET, /*D3DFMT_A8R8G8B8*/ desc.Format, D3DPOOL_DEFAULT, &( s.m_pD3DTexture ), &( s.m_hD3DHandle ) );
							BREAK_ON_FAIL( hr );

							hr = s.m_pD3DTexture->GetSurfaceLevel( 0, &( s.m_pD3DSurface ) );
							BREAK_ON_FAIL( hr );
#endif
							D3DSURFACE_DESC desc;
							hr = s.m_pD3DSurface->GetDesc( &desc );
							BREAK_ON_FAIL( hr );

							*pShared = s;
						}
						else {
							CI_LOG_V( "reusing existing surface to render frame." );
							*pShared = mFree.back();
							mFree.pop_back();
						}
					} while( false );

					return hr;
				}

				HRESULT AddFreeSurface( const SharedTexture &shared )
				{
					std::lock_guard<std::mutex> lock( mFreeLock );
					mFree.push_back( shared );

					return S_OK;
				}

				HRESULT AddAvailableSurface( const SharedTexture &shared )
				{
					std::lock_guard<std::mutex> lock( mAvailableLock );
					mAvailable.push_back( shared );

					return S_OK;
				}

				//! Call from the main thread only, or from a thread that shares an OpenGL context with the main thread.
				ci::gl::Texture2dRef GetTexture()
				{
					HRESULT hr = S_OK;

					SharedTexture s;

					do {
						std::lock_guard<std::mutex> lock( mAvailableLock );

						if( mAvailable.empty() )
							return ci::gl::Texture2dRef();

						CI_LOG_V( "Getting available surface..." );

						s = mAvailable.back();
						mAvailable.pop_back();
					} while( false );

					do {
						D3DSURFACE_DESC desc;
						hr = s.m_pD3DSurface->GetDesc( &desc );
						BREAK_ON_FAIL( hr );

						auto tex = s.m_pGLTexture.lock();

						if( !tex ) {
							CI_LOG_V( "Creating OpenGL texture for this frame." );

							if( !glIsTexture( s.m_uTextureId ) )
								glGenTextures( 1, &( s.m_uTextureId ) );

							tex = gl::Texture2d::create( GL_TEXTURE_RECTANGLE, s.m_uTextureId, desc.Width, desc.Height, false, [&]( ci::gl::Texture *tex ) {
								CI_LOG_V( "Texture destroyed" );

								std::lock_guard<std::mutex> lock( mUsedLock );

								// Find corresponding texture in mUsed.
								auto itr = std::find( mUsed.begin(), mUsed.end(), tex->getId() );
								if( itr != mUsed.end() ) {
									SharedTexture &s = *itr;

									CI_LOG_V( "Unlocking texture " << s.m_hGLHandle );
									if( !wglDXUnlockObjectsNV( m_pD3DDeviceHandle, 1, &( s.m_hGLHandle ) ) ) {
										CI_LOG_E( "Failed to unlock object." );
									}
									else
										CI_LOG_V( "Succeeded." );

									//CI_LOG_V( "Unregistering object " << s.m_hGLHandle );
									//if( !wglDXUnregisterObjectNV( m_pD3DDeviceHandle, s.m_hGLHandle ) ) {
									//	CI_LOG_E( "Failed to unregister object." );
									//}
									//else
									//	CI_LOG_V( "Succeeded." );

									// Return texture to free surfaces.
									std::lock_guard<std::mutex> surlock( mFreeLock );
									mFree.push_back( s );

									// Warning: 's' no longer exists after this call!
									mUsed.erase( itr );
								}

								// Don't actually delete the texture, we're gonna reuse it later. But we do need to notify the gl::context() we're done using it.
								auto ctx = gl::context();
								if( ctx )
									ctx->textureDeleted( tex );
							} );

							// Make sure the texture is not rendered upside down.
							tex->setTopDown( true );

							// Store a weak pointer to the texture.
							s.m_pGLTexture = std::weak_ptr<ci::gl::Texture2d>( tex );

							// Share the 3D surface with OpenGL.
							if( !s.m_hGLHandle ) {
								CI_LOG_V( "Setting resource share handle..." );
								if( !wglDXSetResourceShareHandleNV( s.m_pD3DSurface, s.m_hD3DHandle ) ) {
									CI_LOG_E( "Failed." );
									hr = E_FAIL;
									break;
								}
								CI_LOG_V( "Succeeded." );

								CI_LOG_V( "Registering object..." );
								s.m_hGLHandle = wglDXRegisterObjectNV( m_pD3DDeviceHandle, s.m_pD3DSurface, s.m_uTextureId, GL_TEXTURE_RECTANGLE, WGL_ACCESS_READ_ONLY_NV );
								if( !s.m_hGLHandle ) {
									DWORD err = GetLastError();
									switch( err ) {
									case ERROR_INVALID_HANDLE:
										CI_LOG_E( "Failed to register D3DSurface with OpenGL: Invalid Handle." );
										break;
									case ERROR_INVALID_DATA:
										CI_LOG_E( "Failed to register D3DSurface with OpenGL: Invalid Data." );
										break;
									case ERROR_OPEN_FAILED:
										CI_LOG_E( "Failed to register D3DSurface with OpenGL: Open Failed." );
										break;
									default:
										CI_LOG_E( "Failed to register D3DSurface with OpenGL: Unknown Error." );
										break;
									}

									hr = E_FAIL;
									break;
								}
								CI_LOG_V( "Succeeded." );
							}

							CI_LOG_V( "Texture created." );

							CI_LOG_V( "Locking texture " << s.m_hGLHandle );
							if( !wglDXLockObjectsNV( m_pD3DDeviceHandle, 1, &( s.m_hGLHandle ) ) ) {
								CI_LOG_E( "Failed." );
								hr = E_FAIL;
								break;
							}
							CI_LOG_V( "Succeeded." );
						}
						else {
							// Should never happen!
							__debugbreak();
						}

						std::lock_guard<std::mutex> lock( mUsedLock );
						mUsed.push_back( s );

						return tex;
					} while( false );

					return ci::gl::Texture2dRef();
				}

			private:
				IDirect3DDevice9Ex         *m_pDevice;
				HANDLE                      m_pD3DDeviceHandle;     // Shared device handle for OpenGL interop.

				std::thread::id             mMainThreadID;

				std::mutex                  mFreeLock;
				std::vector<SharedTexture>  mFree;					// Free surfaces ready to be rendered to.

				std::mutex                  mAvailableLock;
				std::vector<SharedTexture>  mAvailable;				// Surfaces containing rendered frames.

				std::mutex                  mUsedLock;
				std::vector<SharedTexture>  mUsed;					// Surfaces in use as textures.

			};
		} // namespace video
	} // namespace msw
} // namespace cinder

#endif // CINDER_MSW