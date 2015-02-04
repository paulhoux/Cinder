
#include "cinder/evr/MediaFoundationVideo.h"
#include "cinder/evr/SharedTexture.h"
#include "cinder/Log.h"

#if defined(CINDER_MSW)

#include <dshow.h> // for EC_DISPLAY_CHANGED event

namespace cinder {
	namespace msw {
		namespace video {

			RECT CorrectAspectRatio( const RECT& src, const MFRatio& srcPAR, const MFRatio& destPAR )
			{
				// Start with a rectangle the same size as src, but offset to the origin (0,0).
				RECT rc = { 0, 0, src.right - src.left, src.bottom - src.top };

				// If the source and destination have the same PAR, there is nothing to do.
				// Otherwise, adjust the image size, in two steps:
				//  1. Transform from source PAR to 1:1
				//  2. Transform from 1:1 to destination PAR.

				if( ( srcPAR.Numerator != destPAR.Numerator ) || ( srcPAR.Denominator != destPAR.Denominator ) ) {
					// Correct for the source's PAR.

					if( srcPAR.Numerator > srcPAR.Denominator ) {
						// The source has "wide" pixels, so stretch the width.
						rc.right = MulDiv( rc.right, srcPAR.Numerator, srcPAR.Denominator );
					}
					else if( srcPAR.Numerator < srcPAR.Denominator ) {
						// The source has "tall" pixels, so stretch the height.
						rc.bottom = MulDiv( rc.bottom, srcPAR.Denominator, srcPAR.Numerator );
					}
					// else: PAR is 1:1, which is a no-op.


					// Next, correct for the target's PAR. This is the inverse operation of the previous.

					if( destPAR.Numerator > destPAR.Denominator ) {
						// The destination has "wide" pixels, so stretch the height.
						rc.bottom = MulDiv( rc.bottom, destPAR.Numerator, destPAR.Denominator );
					}
					else if( destPAR.Numerator < destPAR.Denominator ) {
						// The destination has "tall" pixels, so stretch the width.
						rc.right = MulDiv( rc.right, destPAR.Denominator, destPAR.Numerator );
					}
					// else: PAR is 1:1, which is a no-op.
				}

				return rc;
			}

			BOOL AreMediaTypesEqual( IMFMediaType *pType1, IMFMediaType *pType2 )
			{
				if( ( pType1 == NULL ) && ( pType2 == NULL ) ) {
					return TRUE; // Both are NULL.
				}
				else if( ( pType1 == NULL ) || ( pType2 == NULL ) ) {
					return FALSE; // One is NULL.
				}

				DWORD dwFlags = 0;
				HRESULT hr = pType1->IsEqual( pType2, &dwFlags );

				return ( hr == S_OK );
			}

			HRESULT ValidateVideoArea( const MFVideoArea& area, UINT32 width, UINT32 height )
			{
				float fOffsetX = MFOffsetToFloat( area.OffsetX );
				float fOffsetY = MFOffsetToFloat( area.OffsetY );

				if( ( (LONG) fOffsetX + area.Area.cx > (LONG) width ) ||
					( (LONG) fOffsetY + area.Area.cy > (LONG) height ) ) {
					return MF_E_INVALIDMEDIATYPE;
				}
				else {
					return S_OK;
				}
			}

			HRESULT SetDesiredSampleTime( IMFSample *pSample, const LONGLONG& hnsSampleTime, const LONGLONG& hnsDuration )
			{
				if( pSample == NULL )
					return E_POINTER;

				ScopedComPtr<IMFDesiredSample> pDesired;
				HRESULT hr = pSample->QueryInterface( __uuidof( IMFDesiredSample ), (void**) &pDesired );
				if( SUCCEEDED( hr ) ) {
					// This method has no return value.
					(void) pDesired->SetDesiredSampleTimeAndDuration( hnsSampleTime, hnsDuration );
				}

				return hr;
			}

			HRESULT ClearDesiredSampleTime( IMFSample *pSample )
			{
				if( pSample == NULL )
					return E_POINTER;

				// We store some custom attributes on the sample, so we need to cache them
				// and reset them.
				//
				// This works around the fact that IMFDesiredSample::Clear() removes all of the
				// attributes from the sample. 

				UINT32 counter = MFGetAttributeUINT32( pSample, MFSamplePresenter_SampleCounter, (UINT32) -1 );

				ScopedComPtr<IUnknown> pUnkSwapChain;
				(void) pSample->GetUnknown( MFSamplePresenter_SampleSwapChain, IID_IUnknown, (void**) &pUnkSwapChain );

				ScopedComPtr<IMFDesiredSample> pDesired;
				HRESULT hr = pSample->QueryInterface( __uuidof( IMFDesiredSample ), (void**) &pDesired );
				if( SUCCEEDED( hr ) ) {
					// This method has no return value.
					(void) pDesired->Clear();

					hr = pSample->SetUINT32( MFSamplePresenter_SampleCounter, counter );
					if( FAILED( hr ) )
						return hr;

					if( pUnkSwapChain ) {
						hr = pSample->SetUnknown( MFSamplePresenter_SampleSwapChain, pUnkSwapChain );
						if( FAILED( hr ) )
							return hr;
					}
				}

				return hr;
			}

			BOOL IsSampleTimePassed( IMFClock *pClock, IMFSample *pSample )
			{
				assert( pClock != NULL );
				assert( pSample != NULL );

				if( pSample == NULL || pClock == NULL )
					return E_POINTER;

				HRESULT hr = S_OK;

				MFTIME hnsTimeNow = 0;
				MFTIME hnsSystemTime = 0;
				MFTIME hnsSampleStart = 0;
				MFTIME hnsSampleDuration = 0;

				// The sample might lack a time-stamp or a duration, and the
				// clock might not report a time.

				hr = pClock->GetCorrelatedTime( 0, &hnsTimeNow, &hnsSystemTime );

				if( SUCCEEDED( hr ) ) {
					hr = pSample->GetSampleTime( &hnsSampleStart );
				}
				if( SUCCEEDED( hr ) ) {
					hr = pSample->GetSampleDuration( &hnsSampleDuration );
				}

				if( SUCCEEDED( hr ) ) {
					if( hnsSampleStart + hnsSampleDuration < hnsTimeNow ) {
						return TRUE;
					}
				}

				return FALSE;
			}

			HRESULT SetMixerSourceRect( IMFTransform *pMixer, const MFVideoNormalizedRect& nrcSource )
			{
				if( pMixer == NULL )
					return E_POINTER;

				ScopedComPtr<IMFAttributes> pAttributes;
				HRESULT hr = pMixer->GetAttributes( &pAttributes );
				if( FAILED( hr ) )
					return hr;

				return pAttributes->SetBlob( VIDEO_ZOOM_RECT, (const UINT8*) &nrcSource, sizeof( nrcSource ) );
			}


			//-----------------------------------------------------------------------------

			HRESULT FindAdapter( IDirect3D9 *pD3D9, HMONITOR hMonitor, UINT *puAdapterID )
			{
				HRESULT hr = E_FAIL;
				UINT cAdapters = 0;
				UINT uAdapterID = (UINT) -1;

				cAdapters = pD3D9->GetAdapterCount();
				for( UINT i = 0; i < cAdapters; i++ ) {
					HMONITOR hMonitorTmp = pD3D9->GetAdapterMonitor( i );

					if( hMonitorTmp == NULL ) {
						break;
					}
					if( hMonitorTmp == hMonitor ) {
						uAdapterID = i;
						break;
					}
				}

				if( uAdapterID != (UINT) -1 ) {
					*puAdapterID = uAdapterID;
					hr = S_OK;
				}
				return hr;
			}

			D3DPresentEngine::D3DPresentEngine( HRESULT& hr )
				: m_hwnd( NULL ), m_DeviceResetToken( 0 ), m_pD3D9( NULL ), m_pDevice( NULL ), m_pDeviceManager( NULL )
				, m_pSurfaceRepaint( NULL ), m_pTexturePool( NULL )
			{
				hr = S_OK;

				CI_LOG_V( "Created D3DPresentEngine." );

				do {
					SetRectEmpty( &m_rcDestRect );

					ZeroMemory( &m_DisplayMode, sizeof( m_DisplayMode ) );

					hr = InitializeD3D();
					BREAK_ON_FAIL( hr );

					hr = CreateD3DDevice();
					BREAK_ON_FAIL( hr );
				} while( false );
			}

			D3DPresentEngine::~D3DPresentEngine()
			{
				m_pTexturePool.reset();

				SafeRelease( m_pDevice );
				SafeRelease( m_pSurfaceRepaint );
				SafeRelease( m_pDeviceManager );
				SafeRelease( m_pD3D9 );

				CI_LOG_V( "Destroyed D3DPresentEngine." );
			}

			HRESULT D3DPresentEngine::GetService( REFGUID guidService, REFIID riid, void** ppv )
			{
				assert( ppv != NULL );

				if( riid == __uuidof( IDirect3DDeviceManager9 ) ) {
					if( m_pDeviceManager == NULL ) {
						return MF_E_UNSUPPORTED_SERVICE;
					}

					*ppv = m_pDeviceManager;
					m_pDeviceManager->AddRef();
				}
				else
					return MF_E_UNSUPPORTED_SERVICE;


				return S_OK;
			}

			HRESULT D3DPresentEngine::CheckFormat( D3DFORMAT format )
			{
				HRESULT hr = S_OK;

				do {
					UINT uAdapter = D3DADAPTER_DEFAULT;
					D3DDEVTYPE type = D3DDEVTYPE_HAL;

					if( m_pDevice ) {
						D3DDEVICE_CREATION_PARAMETERS params;
						hr = m_pDevice->GetCreationParameters( &params );
						BREAK_ON_FAIL( hr );

						uAdapter = params.AdapterOrdinal;
						type = params.DeviceType;
					}

					D3DDISPLAYMODE mode;
					hr = m_pD3D9->GetAdapterDisplayMode( uAdapter, &mode );
					BREAK_ON_FAIL( hr );

					hr = m_pD3D9->CheckDeviceType( uAdapter, type, mode.Format, format, TRUE );
					BREAK_ON_FAIL( hr );
				} while( false );

				return hr;
			}

			HRESULT D3DPresentEngine::SetVideoWindow( HWND hwnd )
			{
				// Assertions: EVRCustomPresenter checks these cases.
				assert( IsWindow( hwnd ) );
				assert( hwnd != m_hwnd );

				HRESULT hr = S_OK;

				ScopedCriticalSection lock( m_ObjectLock );

				m_hwnd = hwnd;

				UpdateDestRect();

				// Recreate the device.
				hr = CreateD3DDevice();

				return hr;
			}

			HRESULT D3DPresentEngine::SetDestinationRect( const RECT& rcDest )
			{
				if( EqualRect( &rcDest, &m_rcDestRect ) ) {
					return S_OK; // No change.
				}

				HRESULT hr = S_OK;

				ScopedCriticalSection lock( m_ObjectLock );

				m_rcDestRect = rcDest;

				UpdateDestRect();

				return hr;
			}

			HRESULT D3DPresentEngine::CreateVideoSamples( IMFMediaType *pFormat, VideoSampleList& videoSampleQueue )
			{
				HRESULT hr = S_OK;

				ScopedCriticalSection lock( m_ObjectLock );

				do {
					BREAK_ON_NULL( m_hwnd, MF_E_INVALIDREQUEST );
					BREAK_ON_NULL( pFormat, MF_E_UNEXPECTED );

					ReleaseResources();

					// Get the swap chain parameters from the media type.
					D3DPRESENT_PARAMETERS pPresentParams;
					hr = GetSwapChainPresentParameters( pFormat, &pPresentParams );
					BREAK_ON_FAIL( hr );

					UpdateDestRect();

					// Create the video samples.
					for( int i = 0; i < PRESENTER_BUFFER_COUNT; i++ ) {
						ScopedComPtr<IDirect3DSwapChain9> pSwapChain;
						ScopedComPtr<IMFSample> pVideoSample;

						// Create a new swap chain.
						hr = m_pDevice->CreateAdditionalSwapChain( &pPresentParams, &pSwapChain );
						BREAK_ON_FAIL( hr );

						// Create the video sample from the swap chain.
						hr = CreateD3DSample( pSwapChain, &pVideoSample );
						BREAK_ON_FAIL( hr );

						// Add it to the list.
						hr = videoSampleQueue.InsertBack( pVideoSample );
						BREAK_ON_FAIL( hr );

						// Set the swap chain pointer as a custom attribute on the sample. This keeps
						// a reference count on the swap chain, so that the swap chain is kept alive
						// for the duration of the sample's lifetime.
						hr = pVideoSample->SetUnknown( MFSamplePresenter_SampleSwapChain, pSwapChain );
						BREAK_ON_FAIL( hr );
					}
					BREAK_ON_FAIL( hr );

					// Let the derived class create any additional D3D resources that it needs.
					hr = OnCreateVideoSamples( pPresentParams );
					BREAK_ON_FAIL( hr );
				} while( false );

				if( FAILED( hr ) )
					CI_LOG_E( "Failed to CreateVideoSamples: " << hr );

				if( FAILED( hr ) )
					ReleaseResources();

				return hr;
			}

			void D3DPresentEngine::ReleaseResources()
			{
				// Let the derived class release any resources it created.
				OnReleaseResources();

				SafeRelease( m_pSurfaceRepaint );
			}

			HRESULT D3DPresentEngine::CheckDeviceState( DeviceState *pState )
			{
				HRESULT hr = S_OK;

				ScopedCriticalSection lock( m_ObjectLock );

				// Check the device state. Not every failure code is a critical failure.
				hr = m_pDevice->CheckDeviceState( m_hwnd );

				*pState = DeviceOK;

				switch( hr ) {
				case D3DERR_DEVICELOST:
				case D3DERR_DEVICEHUNG:
					// Lost/hung device. Destroy the device and create a new one.
					hr = CreateD3DDevice();
					if( SUCCEEDED( hr ) )
						*pState = DeviceReset;
					return hr;
				case D3DERR_DEVICEREMOVED:
					// This is a fatal error.
					*pState = DeviceRemoved;
					return hr;
				default:
					return S_OK;
				}
			}

			HRESULT D3DPresentEngine::PresentSample( IMFSample* pSample, LONGLONG llTarget )
			{
				HRESULT hr = S_OK;

				do {
					ScopedComPtr<IDirect3DSurface9> pSurface;

					if( pSample ) {
						// Get the buffer from the sample.
						ScopedComPtr<IMFMediaBuffer> pBuffer;
						hr = pSample->GetBufferByIndex( 0, &pBuffer );
						BREAK_ON_FAIL( hr );

						// Get the surface from the buffer.
						hr = MFGetService( pBuffer, MR_BUFFER_SERVICE, __uuidof( IDirect3DSurface9 ), (void**) &pSurface );
						BREAK_ON_FAIL( hr );
					}
					else if( m_pSurfaceRepaint ) {
						// Redraw from the last surface.
						m_pSurfaceRepaint->AddRef();
						pSurface.Attach( m_pSurfaceRepaint );
					}

					if( pSurface ) {
						// Get the swap chain from the surface.
						ScopedComPtr<IDirect3DSwapChain9> pSwapChain;
						hr = pSurface->GetContainer( __uuidof( IDirect3DSwapChain9 ), (LPVOID*) &pSwapChain );
						BREAK_ON_FAIL( hr );

						// Present the swap chain.
						hr = PresentSwapChain( pSwapChain, pSurface );
						BREAK_ON_FAIL( hr );

						// Store this pointer in case we need to repaint the surface.
						CopyComPtr( m_pSurfaceRepaint, pSurface.get() );
					}
					else {
						// No surface. All we can do is paint a black rectangle.
						PaintFrameWithGDI();
					}
				} while( false );

				if( FAILED( hr ) )
					CI_LOG_E( "Failed to PresentSample: " << hr );

				if( FAILED( hr ) ) {
					if( hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET || hr == D3DERR_DEVICEHUNG ) {
						// We failed because the device was lost. Fill the destination rectangle.
						PaintFrameWithGDI();

						// Ignore. We need to reset or re-create the device, but this method
						// is probably being called from the scheduler thread, which is not the
						// same thread that created the device. The Reset(Ex) method must be
						// called from the thread that created the device.

						// The presenter will detect the state when it calls CheckDeviceState() 
						// on the next sample.
						hr = S_OK;
					}
				}

				return hr;
			}

			ci::gl::Texture2dRef D3DPresentEngine::GetTexture()
			{
				assert( m_pTexturePool );
				return m_pTexturePool->GetTexture();
			}

			HRESULT D3DPresentEngine::InitializeD3D()
			{
				HRESULT hr = S_OK;

				assert( m_pD3D9 == NULL );
				assert( m_pDeviceManager == NULL );

				do {
					// Create Direct3D
					hr = Direct3DCreate9Ex( D3D_SDK_VERSION, &m_pD3D9 );
					BREAK_ON_FAIL( hr );

					// Create the device manager
					hr = DXVA2CreateDirect3DDeviceManager9( &m_DeviceResetToken, &m_pDeviceManager );
					BREAK_ON_FAIL( hr );
				} while( false );

				if( FAILED( hr ) )
					CI_LOG_E( "Failed to InitializeD3D: " << hr );

				return hr;
			}

			HRESULT D3DPresentEngine::CreateD3DDevice()
			{
				HRESULT hr = S_OK;

				do {
					// Hold the lock because we might be discarding an exisiting device.
					ScopedCriticalSection lock( m_ObjectLock );

					BREAK_ON_NULL( m_pD3D9, MF_E_NOT_INITIALIZED );
					BREAK_ON_NULL( m_pDeviceManager, MF_E_NOT_INITIALIZED );

					// Find the monitor for this window.
					UINT uAdapterID = D3DADAPTER_DEFAULT;
					if( m_hwnd ) {
						HMONITOR hMonitor = MonitorFromWindow( m_hwnd, MONITOR_DEFAULTTONEAREST );

						// Find the corresponding adapter.
						hr = FindAdapter( m_pD3D9, hMonitor, &uAdapterID );
						BREAK_ON_FAIL( hr );
					}

					// Get the device caps for this adapter.
					D3DCAPS9 ddCaps;
					ZeroMemory( &ddCaps, sizeof( ddCaps ) );

					hr = m_pD3D9->GetDeviceCaps( uAdapterID, D3DDEVTYPE_HAL, &ddCaps );
					BREAK_ON_FAIL( hr );

					DWORD vp = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
					if( ddCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) {
						vp = D3DCREATE_HARDWARE_VERTEXPROCESSING;
					}
					else {
						CI_LOG_W( "Failed to enable hardware rendering. Using software rendering instead." );
					}

					// Note: The presenter creates additional swap chains to present the
					// video frames. Therefore, it does not use the device's implicit 
					// swap chain, so the size of the back buffer here is 1 x 1.

					D3DPRESENT_PARAMETERS pPresentParams;
					ZeroMemory( &pPresentParams, sizeof( pPresentParams ) );

					pPresentParams.BackBufferWidth = 1;
					pPresentParams.BackBufferHeight = 1;
					pPresentParams.Windowed = TRUE;
					pPresentParams.SwapEffect = D3DSWAPEFFECT_COPY;
					pPresentParams.BackBufferFormat = D3DFMT_UNKNOWN;
					pPresentParams.hDeviceWindow = GetDesktopWindow();
					pPresentParams.Flags = D3DPRESENTFLAG_VIDEO;
					pPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

					// Create the device. The interop definition states D3DCREATE_MULTITHREADED is required, but it may vary by vendor.
					ScopedComPtr<IDirect3DDevice9Ex> pDevice;
					hr = m_pD3D9->CreateDeviceEx( uAdapterID, D3DDEVTYPE_HAL, pPresentParams.hDeviceWindow,
												  vp | D3DCREATE_NOWINDOWCHANGES | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, &pPresentParams, NULL, &pDevice );
					BREAK_ON_FAIL( hr );

					// Get the adapter display mode.
					hr = m_pD3D9->GetAdapterDisplayMode( uAdapterID, &m_DisplayMode );
					BREAK_ON_FAIL( hr );

					// Reset the D3DDeviceManager with the new device 
					hr = m_pDeviceManager->ResetDevice( pDevice, m_DeviceResetToken );
					BREAK_ON_FAIL( hr );

					// Create the texture pool.
					hr = CreateTexturePool(pDevice);
					BREAK_ON_FAIL( hr );

					CopyComPtr( m_pDevice, pDevice.get() );
				} while( false );

				if( FAILED( hr ) )
					CI_LOG_E( "Failed to CreateD3DDevice: " << hr );

				return hr;
			}

			HRESULT D3DPresentEngine::CreateTexturePool( IDirect3DDevice9Ex *pDevice )
			{
				assert( pDevice != NULL );

				m_pTexturePool = std::make_shared<SharedTexturePool>( pDevice );

				return S_OK;
			}


			HRESULT D3DPresentEngine::CreateD3DSample( IDirect3DSwapChain9 *pSwapChain, IMFSample **ppVideoSample )
			{
				// Caller holds the object lock.

				HRESULT hr = S_OK;

				do {
					// Get the back buffer surface.
					ScopedComPtr<IDirect3DSurface9> pSurface;
					hr = pSwapChain->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pSurface );
					BREAK_ON_FAIL( hr );

					// Fill it with black.
					const D3DCOLOR clrBlack = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00 );
					hr = m_pDevice->ColorFill( pSurface, NULL, clrBlack );
					BREAK_ON_FAIL( hr );

					// Create the sample.
					ScopedComPtr<IMFSample> pSample;
					hr = MFCreateVideoSampleFromSurface( pSurface, &pSample );
					BREAK_ON_FAIL( hr );

					// Return the pointer to the caller.
					*ppVideoSample = pSample;
					( *ppVideoSample )->AddRef();
				} while( false );

				if( FAILED( hr ) )
					CI_LOG_E( "Failed to CreateD3DSample: " << hr );

				return hr;
			}


			HRESULT D3DPresentEngine::PresentSwapChain( IDirect3DSwapChain9* pSwapChain, IDirect3DSurface9* pSurface )
			{
				HRESULT hr = S_OK;

				do {
					BREAK_ON_NULL( m_hwnd, MF_E_INVALIDREQUEST );

					// Grab back buffer.
					ScopedComPtr<IDirect3DSurface9> pSurface;
					hr = pSwapChain->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pSurface );
					BREAK_ON_FAIL( hr );

					D3DSURFACE_DESC desc;
					hr = pSurface->GetDesc( &desc );
					BREAK_ON_FAIL( hr );

					// Obtain free surface from the texture pool.
					assert( m_pTexturePool );

					SharedTextureRef shared;
					hr = m_pTexturePool->GetFreeSurface( desc, shared );
					BREAK_ON_FAIL( hr );

					// Blit texture.
					hr = shared->BlitTo( m_pDevice, pSurface );

					//
					hr = pSwapChain->Present( NULL, &m_rcDestRect, m_hwnd, NULL, 0 ); // <-- Is this required if we can't see the window anyway?
					BREAK_ON_FAIL( hr );
				} while( false );

				if( FAILED( hr ) )
					CI_LOG_E( "Failed to PresentSwapChain: " << hr );

				return hr;
			}

			void D3DPresentEngine::PaintFrameWithGDI()
			{
				HDC hdc = GetDC( m_hwnd );

				if( hdc ) {
					HBRUSH hBrush = CreateSolidBrush( RGB( 0, 0, 0 ) );

					if( hBrush ) {
						FillRect( hdc, &m_rcDestRect, hBrush );
						DeleteObject( hBrush );
					}

					ReleaseDC( m_hwnd, hdc );
				}
			}

			HRESULT D3DPresentEngine::GetSwapChainPresentParameters( IMFMediaType *pType, D3DPRESENT_PARAMETERS* pPP )
			{
				// Caller holds the object lock.

				HRESULT hr = S_OK;

				UINT32 width = 0, height = 0;
				DWORD d3dFormat = 0;

				// Helper object for reading the proposed type.
				VideoType videoType( pType );

				if( m_hwnd == NULL ) {
					return MF_E_INVALIDREQUEST;
				}

				ZeroMemory( pPP, sizeof( D3DPRESENT_PARAMETERS ) );

				// Get some information about the video format.
				hr = videoType.GetFrameDimensions( &width, &height );
				hr = videoType.GetFourCC( &d3dFormat );

				ZeroMemory( pPP, sizeof( D3DPRESENT_PARAMETERS ) );
				pPP->BackBufferWidth = width;
				pPP->BackBufferHeight = height;
				pPP->Windowed = TRUE;
				pPP->SwapEffect = D3DSWAPEFFECT_COPY;
				pPP->BackBufferFormat = (D3DFORMAT) d3dFormat;
				pPP->hDeviceWindow = m_hwnd;
				pPP->Flags = D3DPRESENTFLAG_VIDEO;
				pPP->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

				D3DDEVICE_CREATION_PARAMETERS params;
				hr = m_pDevice->GetCreationParameters( &params );

				if( params.DeviceType != D3DDEVTYPE_HAL ) {
					pPP->Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
				}

				return S_OK;
			}

			HRESULT D3DPresentEngine::UpdateDestRect()
			{
				if( m_hwnd == NULL ) {
					return MF_E_INVALIDREQUEST;
				}

				RECT rcView;
				GetClientRect( m_hwnd, &rcView );

				// Clip the destination rectangle to the window's client area.
				if( m_rcDestRect.right > rcView.right ) {
					m_rcDestRect.right = rcView.right;
				}

				if( m_rcDestRect.bottom > rcView.bottom ) {
					m_rcDestRect.bottom = rcView.bottom;
				}

				return S_OK;
			}

			//-----------------------------------------------------------------------------

			const MFRatio EVRCustomPresenter::sDefaultFrameRate = { 30, 1 };

			HRESULT EVRCustomPresenter::QueryInterface( REFIID riid, void ** ppv )
			{
				if( ppv == NULL )
					return E_POINTER;

				if( riid == __uuidof( IUnknown ) ) {
					*ppv = static_cast<IUnknown*>( static_cast<IMFVideoPresenter*>( this ) );
				}
				else if( riid == __uuidof( IMFVideoDeviceID ) ) {
					*ppv = static_cast<IMFVideoDeviceID*>( this );
				}
				else if( riid == __uuidof( IMFVideoPresenter ) ) {
					*ppv = static_cast<IMFVideoPresenter*>( this );
				}
				else if( riid == __uuidof( IMFClockStateSink ) )    // Inherited from IMFVideoPresenter
				{
					*ppv = static_cast<IMFClockStateSink*>( this );
				}
				else if( riid == __uuidof( IMFRateSupport ) ) {
					*ppv = static_cast<IMFRateSupport*>( this );
				}
				//else if( riid == __uuidof( IMFRateControl ) ) {
				//	*ppv = static_cast<IMFRateControl*>( this );
				//}
				else if( riid == __uuidof( IMFGetService ) ) {
					*ppv = static_cast<IMFGetService*>( this );
				}
				else if( riid == __uuidof( IMFTopologyServiceLookupClient ) ) {
					*ppv = static_cast<IMFTopologyServiceLookupClient*>( this );
				}
				else if( riid == __uuidof( IMFVideoDisplayControl ) ) {
					*ppv = static_cast<IMFVideoDisplayControl*>( this );
				}
				else {
					*ppv = NULL;
					return E_NOINTERFACE;
				}

				AddRef();
				return S_OK;
			}

			ULONG EVRCustomPresenter::AddRef()
			{
				return InterlockedIncrement( &mRefCount );
			}

			ULONG EVRCustomPresenter::Release()
			{
				assert( mRefCount > 0 );

				ULONG uCount = InterlockedDecrement( &mRefCount );
				if( uCount == 0 ) {
					mRefCount = DESTRUCTOR_REF_COUNT;
					delete this;
				}

				return uCount;
			}

			HRESULT EVRCustomPresenter::GetService( REFGUID guidService, REFIID riid, LPVOID *ppvObject )
			{
				HRESULT hr = S_OK;

				if( ppvObject == NULL )
					return E_POINTER;

				// The only service GUID that we support is MR_VIDEO_RENDER_SERVICE.
				if( guidService != MR_VIDEO_RENDER_SERVICE ) {
					return MF_E_UNSUPPORTED_SERVICE;
				}

				// First try to get the service interface from the D3DPresentEngine object.
				hr = m_pD3DPresentEngine->GetService( guidService, riid, ppvObject );
				if( FAILED( hr ) ) {
					// Next, QI to check if this object supports the interface.
					hr = QueryInterface( riid, ppvObject );
				}

				return hr;
			}

			/*! The standard mixer and presenter both use Direct3D 9, with the device GUID equal to IID_IDirect3DDevice9.
				If you intend to use your custom presenter with the standard mixer, the presenter's device GUID must be IID_IDirect3DDevice9.
				If you replace both components, you could define a new device GUID.
				*/
			HRESULT EVRCustomPresenter::GetDeviceID( IID* pDeviceID )
			{
				if( pDeviceID == NULL )
					return E_POINTER;

				*pDeviceID = __uuidof( IDirect3DDevice9 );
				return S_OK;
			}

			HRESULT EVRCustomPresenter::InitServicePointers( IMFTopologyServiceLookup *pLookup )
			{
				HRESULT hr = S_OK;

				do {
					BREAK_ON_NULL( pLookup, E_POINTER );

					ScopedCriticalSection lock( m_ObjectLock );

					// Do not allow initializing when playing or paused.
					if( IsActive() )
						return MF_E_INVALIDREQUEST;

					SafeRelease( m_pClock );
					SafeRelease( m_pMixer );
					SafeRelease( m_pMediaEventSink );

					// Ask for the clock. Optional, because the EVR might not have a clock.
					DWORD dwObjectCount = 1;
					HRESULT hr = pLookup->LookupService( MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE,
														 __uuidof( IMFClock ), (void**) &m_pClock, &dwObjectCount );

					// Ask for the mixer. (Required.)
					dwObjectCount = 1;
					hr = pLookup->LookupService( MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_MIXER_SERVICE,
												 __uuidof( IMFTransform ), (void**) &m_pMixer, &dwObjectCount );
					BREAK_ON_FAIL( hr );

					// Make sure that we can work with this mixer.
					hr = ConfigureMixer( m_pMixer );
					BREAK_ON_FAIL( hr );

					// Ask for the EVR's event-sink interface. (Required.)
					dwObjectCount = 1;
					hr = pLookup->LookupService( MF_SERVICE_LOOKUP_GLOBAL, 0, MR_VIDEO_RENDER_SERVICE,
												 __uuidof( IMediaEventSink ), (void**) &m_pMediaEventSink, &dwObjectCount );
					BREAK_ON_FAIL( hr );

					// Successfully initialized. Set the state to "stopped."
					m_RenderState = Stopped;
				} while( false );

				return hr;
			}

			/*! The EVR calls ReleaseServicePointers for various reasons, including:
				* Disconnecting or reconnecting pins (DirectShow), or adding or removing stream sinks (Media Foundation).
				* Changing format.
				* Setting a new clock.
				* Final shutdown of the EVR.

				During the lifetime of the presenter, the EVR might call InitServicePointers and ReleaseServicePointers several times.
				*/
			HRESULT EVRCustomPresenter::ReleaseServicePointers()
			{
				HRESULT hr = S_OK;

				// Enter the shut-down state.
				{
					ScopedCriticalSection lock( m_ObjectLock );
					m_RenderState = Shutdown;
				}

				// Flush any samples that were scheduled.
				Flush();

				// Clear the media type and release related resources (surfaces, etc).
				SetMediaType( NULL );

				// Release all services that were acquired from InitServicePointers.
				SafeRelease( m_pClock );
				SafeRelease( m_pMixer );
				SafeRelease( m_pMediaEventSink );

				return hr;
			}

			HRESULT EVRCustomPresenter::GetNativeVideoSize( SIZE* pszVideo, SIZE* pszARVideo )
			{
				assert( pszVideo != nullptr );
				assert( pszARVideo != nullptr );

				*pszVideo = m_VideoSize;
				*pszARVideo = m_VideoARSize;

				return S_OK;
			}

			HRESULT EVRCustomPresenter::GetIdealVideoSize( SIZE* pszMin, SIZE* pszMax )
			{
				return E_NOTIMPL;
			}

			HRESULT EVRCustomPresenter::ProcessMessage( MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam )
			{
				HRESULT hr = S_OK;

				ScopedCriticalSection lock( m_ObjectLock );

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				switch( eMessage ) {
				case MFVP_MESSAGE_FLUSH:
					CI_LOG_V( "MFVP_MESSAGE_FLUSH" );
					// Flush all pending samples.
					hr = Flush();
					break;
				case MFVP_MESSAGE_INVALIDATEMEDIATYPE:
					CI_LOG_V( "MFVP_MESSAGE_INVALIDATEMEDIATYPE. Renegotiating..." );
					// Renegotiate the media type with the mixer.
					hr = RenegotiateMediaType();
					break;
				case MFVP_MESSAGE_PROCESSINPUTNOTIFY:
					//CI_LOG_V( "MFVP_MESSAGE_PROCESSINPUTNOTIFY" );
					// The mixer received a new input sample. 
					hr = ProcessInputNotify();
					break;
				case MFVP_MESSAGE_BEGINSTREAMING:
					CI_LOG_V( "MFVP_MESSAGE_BEGINSTREAMING" );
					// Streaming is about to start.
					hr = BeginStreaming();
					break;
				case MFVP_MESSAGE_ENDSTREAMING:
					CI_LOG_V( "MFVP_MESSAGE_ENDSTREAMING" );
					// Streaming has ended. (The EVR has stopped.)
					hr = EndStreaming();
					break;
				case MFVP_MESSAGE_ENDOFSTREAM:
					CI_LOG_V( "MFVP_MESSAGE_ENDOFSTREAM" );
					// All input streams have ended.
					// Set the EOS flag. 
					m_bEndStreaming = TRUE;
					// Check if it's time to send the EC_COMPLETE event to the EVR.
					hr = CheckEndOfStream();
					break;
				case MFVP_MESSAGE_STEP:
					CI_LOG_V( "MFVP_MESSAGE_STEP" );
					// Frame-stepping is starting.
					hr = PrepareFrameStep( LODWORD( ulParam ) );
					break;
				case MFVP_MESSAGE_CANCELSTEP:
					CI_LOG_V( "MFVP_MESSAGE_CANCELSTEP" );
					// Cancels frame-stepping.
					hr = CancelFrameStep();
					break;
				default:
					// Unknown message. (This case should never occur.)
					hr = E_INVALIDARG;
					break;
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::GetCurrentMediaType( IMFVideoMediaType** ppMediaType )
			{
				HRESULT hr = S_OK;

				ScopedCriticalSection lock( m_ObjectLock );

				if( ppMediaType == NULL )
					return E_POINTER;

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				if( m_pMediaType == NULL )
					return MF_E_NOT_INITIALIZED;

				// The function returns an IMFVideoMediaType pointer, and we store our media
				// type as an IMFMediaType pointer, so we need to QI.
				hr = m_pMediaType->QueryInterface( __uuidof( IMFVideoMediaType ), (void**) &ppMediaType );

				return hr;
			}

			HRESULT EVRCustomPresenter::OnClockStart( MFTIME hnsSystemTime, LONGLONG llClockStartOffset )
			{
				HRESULT hr = S_OK;

				ScopedCriticalSection lock( m_ObjectLock );

				// We cannot start after shutdown.
				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				// Check if the clock is already active (not stopped). 
				if( IsActive() ) {
					m_RenderState = Started;

					// If the clock position changes while the clock is active, it 
					// is a seek request. We need to flush all pending samples.
					if( llClockStartOffset != PRESENTATION_CURRENT_POSITION )
						Flush();
				}
				else {
					m_RenderState = Started;

					// The clock has started from the stopped state. 

					// Possibly we are in the middle of frame-stepping OR have samples waiting 
					// in the frame-step queue. Deal with these two cases first:
					hr = StartFrameStep();
					if( FAILED( hr ) )
						return hr;
				}

				// Now try to get new output samples from the mixer.
				ProcessOutputLoop();

				return hr;
			}

			HRESULT EVRCustomPresenter::OnClockRestart( MFTIME hnsSystemTime )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				HRESULT hr = S_OK;

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				// The EVR calls OnClockRestart only while paused.
				assert( m_RenderState == Paused );

				m_RenderState = Started;

				// Possibly we are in the middle of frame-stepping OR we have samples waiting 
				// in the frame-step queue. Deal with these two cases first:
				hr = StartFrameStep();
				if( FAILED( hr ) )
					return hr;

				// Now resume the presentation loop.
				ProcessOutputLoop();

				return hr;
			}

			HRESULT EVRCustomPresenter::OnClockStop( MFTIME hnsSystemTime )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				HRESULT hr = S_OK;

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				if( m_RenderState != Stopped ) {
					m_RenderState = Stopped;
					Flush();

					// If we are in the middle of frame-stepping, cancel it now.
					if( m_FrameStep.state != None )
						CancelFrameStep();
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::OnClockPause( MFTIME hnsSystemTime )
			{
				HRESULT hr = S_OK;

				ScopedCriticalSection lock( m_ObjectLock );

				// We cannot pause the clock after shutdown.
				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				// Set the state. (No other actions are necessary.)
				m_RenderState = Paused;

				return hr;
			}

			HRESULT EVRCustomPresenter::OnClockSetRate( MFTIME hnsSystemTime, float fRate )
			{
				// Note: 
				// The presenter reports its maximum rate through the IMFRateSupport interface.
				// Here, we assume that the EVR honors the maximum rate.

				ScopedCriticalSection lock( m_ObjectLock );

				HRESULT hr = S_OK;

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				// If the rate is changing from zero (scrubbing) to non-zero, cancel the 
				// frame-step operation.
				if( ( m_fRate == 0.0f ) && ( fRate != 0.0f ) ) {
					CancelFrameStep();
					m_FrameStep.samples.Clear();
				}

				m_fRate = fRate;

				// Tell the scheduler about the new rate.
				m_scheduler.SetClockRate( fRate );

				return hr;
			}

			HRESULT EVRCustomPresenter::GetSlowestRate( MFRATE_DIRECTION eDirection, BOOL bThin, float *pfRate )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				HRESULT hr = S_OK;

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				if( pfRate == NULL )
					return E_POINTER;

				// There is no minimum playback rate, so the minimum is zero.
				*pfRate = 0;

				return S_OK;
			}

			HRESULT EVRCustomPresenter::GetFastestRate( MFRATE_DIRECTION eDirection, BOOL bThin, float *pfRate )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				HRESULT hr = S_OK;

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				if( pfRate == NULL )
					return E_POINTER;

				// Get the maximum *forward* rate.
				float fMaxRate = GetMaxRate( bThin );

				// For reverse playback, it's the negative of fMaxRate.
				if( eDirection == MFRATE_REVERSE ) {
					fMaxRate = -fMaxRate;
				}

				*pfRate = fMaxRate;

				return hr;
			}

			HRESULT EVRCustomPresenter::IsRateSupported( BOOL bThin, float fRate, float *pfNearestSupportedRate )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				HRESULT hr = S_OK;

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				// Find the maximum forward rate.
				// Note: We have no minimum rate (ie, we support anything down to 0).
				float fMaxRate = GetMaxRate( bThin );
				float fNearestRate = fRate;   // If we support fRate, then fRate *is* the nearest.

				if( fabsf( fRate ) > fMaxRate ) {
					// The (absolute) requested rate exceeds the maximum rate.
					hr = MF_E_UNSUPPORTED_RATE;

					// The nearest supported rate is fMaxRate.
					fNearestRate = fMaxRate;
					if( fRate < 0 ) {
						// Negative for reverse playback.
						fNearestRate = -fNearestRate;
					}
				}

				// Return the nearest supported rate.
				if( pfNearestSupportedRate != NULL ) {
					*pfNearestSupportedRate = fNearestRate;
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::SetVideoWindow( HWND hwndVideo )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				if( !IsWindow( hwndVideo ) )
					return E_INVALIDARG;

				HRESULT hr = S_OK;

				// If the window has changed, notify the D3DPresentEngine object.
				// This will cause a new Direct3D device to be created.
				HWND oldHwnd = m_pD3DPresentEngine->GetVideoWindow();
				if( oldHwnd != hwndVideo ) {
					hr = m_pD3DPresentEngine->SetVideoWindow( hwndVideo );

					// Tell the EVR that the device has changed.
					NotifyEvent( EC_DISPLAY_CHANGED, 0, 0 );
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::GetVideoWindow( HWND* phwndVideo )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				if( phwndVideo == NULL )
					return E_POINTER;

				// The D3DPresentEngine object stores the handle.
				*phwndVideo = m_pD3DPresentEngine->GetVideoWindow();

				return S_OK;
			}

			HRESULT EVRCustomPresenter::SetVideoPosition( const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				// Validate parameters.

				// One parameter can be NULL, but not both.
				if( pnrcSource == NULL && prcDest == NULL )
					return E_POINTER;

				// Validate the rectangles.
				if( pnrcSource ) {
					// The source rectangle cannot be flipped.
					if( ( pnrcSource->left > pnrcSource->right ) ||
						( pnrcSource->top > pnrcSource->bottom ) ) {
						return E_INVALIDARG;
					}

					// The source rectangle has range (0..1)
					if( ( pnrcSource->left < 0 ) || ( pnrcSource->right > 1 ) ||
						( pnrcSource->top < 0 ) || ( pnrcSource->bottom > 1 ) ) {
						return E_INVALIDARG;
					}
				}

				if( prcDest ) {
					// The destination rectangle cannot be flipped.
					if( ( prcDest->left > prcDest->right ) ||
						( prcDest->top > prcDest->bottom ) ) {
						return E_INVALIDARG;
					}
				}

				HRESULT hr = S_OK;

				// Update the source rectangle. Source clipping is performed by the mixer.
				if( pnrcSource ) {
					m_nrcSource = *pnrcSource;

					if( m_pMixer ) {
						hr = SetMixerSourceRect( m_pMixer, m_nrcSource );
						if( FAILED( hr ) )
							return hr;
					}
				}

				// Update the destination rectangle.
				if( prcDest ) {
					RECT rcOldDest = m_pD3DPresentEngine->GetDestinationRect();

					// Check if the destination rectangle changed.
					if( !EqualRect( &rcOldDest, prcDest ) ) {
						hr = m_pD3DPresentEngine->SetDestinationRect( *prcDest );
						if( FAILED( hr ) )
							return hr;

						// Set a new media type on the mixer.
						if( m_pMixer ) {
							//hr = RenegotiateMediaType();
							if( hr == MF_E_TRANSFORM_TYPE_NOT_SET ) {
								// This error means that the mixer is not ready for the media type.
								// Not a failure case -- the EVR will notify us when we need to set
								// the type on the mixer.
								hr = S_OK;
							}
							else {
								if( FAILED( hr ) )
									return hr;

								// The media type changed. Request a repaint of the current frame.
								m_bRepaint = TRUE;
								(void) ProcessOutput(); // Ignore errors, the mixer might not have a video frame.
							}
						}
					}
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::GetVideoPosition( MFVideoNormalizedRect* pnrcSource, LPRECT prcDest )
			{
				ScopedCriticalSection lock( m_ObjectLock );

				if( pnrcSource == NULL || prcDest == NULL ) {
					return E_POINTER;
				}

				*pnrcSource = m_nrcSource;
				*prcDest = m_pD3DPresentEngine->GetDestinationRect();

				return S_OK;
			}

			HRESULT EVRCustomPresenter::RepaintVideo()
			{
				ScopedCriticalSection lock( m_ObjectLock );

				HRESULT hr = S_OK;

				hr = CheckShutdown();
				if( FAILED( hr ) )
					return hr;

				// Ignore the request if we have not presented any samples yet.
				if( m_bPrerolled ) {
					m_bRepaint = TRUE;
					(void) ProcessOutput();
				}

				return hr;
			}

			//-----------------------------------------------------------------------------

			EVRCustomPresenter::EVRCustomPresenter( HRESULT& hr )
				: mRefCount( 0 ), m_RenderState( Shutdown ), m_pD3DPresentEngine( NULL ), m_pClock( NULL ), m_pMixer( NULL )
				, m_pMediaEventSink( NULL ), m_pMediaType( NULL ), m_bSampleNotify( FALSE ), m_bRepaint( FALSE )
				, m_bEndStreaming( FALSE ), m_bPrerolled( FALSE ), m_fRate( 1.0f ), m_TokenCounter( 0 )
				, m_SampleFreeCB( this, &EVRCustomPresenter::OnSampleFree )
			{
				hr = S_OK;

				CI_LOG_V( "Created EVRCustomPresenter." );

				// Initial source rectangle = (0,0,1,1)
				m_nrcSource.top = 0;
				m_nrcSource.left = 0;
				m_nrcSource.bottom = 1;
				m_nrcSource.right = 1;

				m_pD3DPresentEngine = new D3DPresentEngine( hr );
				if( m_pD3DPresentEngine == NULL )
					hr = E_OUTOFMEMORY;

				if( SUCCEEDED( hr ) ) {
					m_scheduler.SetCallback( m_pD3DPresentEngine );
				}
			}

			EVRCustomPresenter::~EVRCustomPresenter()
			{
				// COM interfaces
				SafeRelease( m_pClock );
				SafeRelease( m_pMixer );
				SafeRelease( m_pMediaEventSink );
				SafeRelease( m_pMediaType );

				// Deletable objects
				SafeDelete( m_pD3DPresentEngine );

				CI_LOG_V( "Destroyed EVRCustomPresenter." );
			}

			HRESULT EVRCustomPresenter::ConfigureMixer( IMFTransform *pMixer )
			{
				HRESULT hr = S_OK;
				IID deviceID = GUID_NULL;

				ScopedComPtr<IMFVideoDeviceID> pDeviceID;

				// Make sure that the mixer has the same device ID as ourselves.
				hr = pMixer->QueryInterface( __uuidof( IMFVideoDeviceID ), (void**) &pDeviceID );
				if( FAILED( hr ) )
					return hr;

				hr = pDeviceID->GetDeviceID( &deviceID );
				if( FAILED( hr ) )
					return hr;

				if( !IsEqualGUID( deviceID, __uuidof( IDirect3DDevice9 ) ) )
					return MF_E_INVALIDREQUEST;

				// Set the zoom rectangle (ie, the source clipping rectangle).
				SetMixerSourceRect( pMixer, m_nrcSource );

				return hr;
			}

			HRESULT EVRCustomPresenter::RenegotiateMediaType()
			{
				HRESULT hr = S_OK;

				do {
					BREAK_ON_NULL( m_pMixer, MF_E_INVALIDREQUEST );

					// Loop through all of the mixer's proposed output types.
					DWORD iTypeIndex = 0;
					BOOL bFoundMediaType = FALSE;
					while( !bFoundMediaType && ( hr != MF_E_NO_MORE_TYPES ) ) {
						ScopedComPtr<IMFMediaType> pMixerType;
						ScopedComPtr<IMFMediaType> pOptimalType;

						// Step 1. Get the next media type supported by mixer.
						hr = m_pMixer->GetOutputAvailableType( 0, iTypeIndex++, &pMixerType );
						BREAK_ON_FAIL( hr );

						// From now on, if anything in this loop fails, try the next type,
						// until we succeed or the mixer runs out of types.

						// Step 2. Check if we support this media type. 
						if( SUCCEEDED( hr ) ) {
							// Note: None of the modifications that we make later in CreateOptimalVideoType
							// will affect the suitability of the type, at least for us. (Possibly for the mixer.)
							hr = IsMediaTypeSupported( pMixerType );
						}

						// Step 3. Adjust the mixer's type to match our requirements.
						if( SUCCEEDED( hr ) ) {
							hr = CreateOptimalVideoType( pMixerType, &pOptimalType );
						}

						// Step 4. Check if the mixer will accept this media type.
						if( SUCCEEDED( hr ) ) {
							hr = m_pMixer->SetOutputType( 0, pOptimalType, MFT_SET_TYPE_TEST_ONLY );
						}

						// Step 5. Try to set the media type on ourselves.
						if( SUCCEEDED( hr ) ) {
							hr = SetMediaType( pOptimalType );
						}

						// Step 6. Set output media type on mixer.
						if( SUCCEEDED( hr ) ) {
							hr = m_pMixer->SetOutputType( 0, pOptimalType, 0 );

							assert( SUCCEEDED( hr ) ); // This should succeed unless the MFT lied in the previous call.

							// If something went wrong, clear the media type.
							if( FAILED( hr ) ) {
								SetMediaType( NULL );
							}
						}

						if( SUCCEEDED( hr ) ) {
							bFoundMediaType = TRUE;
						}
					}
				} while( false );

				return hr;
			}

			HRESULT EVRCustomPresenter::Flush()
			{
				m_bPrerolled = FALSE;

				// The scheduler might have samples that are waiting for
				// their presentation time. Tell the scheduler to flush.

				// This call blocks until the scheduler threads discards all scheduled samples.
				m_scheduler.Flush();

				// Flush the frame-step queue.
				m_FrameStep.samples.Clear();

				if( m_RenderState == Stopped ) {
					// Repaint with black.
					//(void) m_pD3DPresentEngine->PresentSample( NULL, 0 );
				}

				return S_OK;
			}

			HRESULT EVRCustomPresenter::ProcessInputNotify()
			{
				HRESULT hr = S_OK;

				// Set the flag that says the mixer has a new sample.
				m_bSampleNotify = TRUE;

				if( m_pMediaType == NULL ) {
					// We don't have a valid media type yet.
					hr = MF_E_TRANSFORM_TYPE_NOT_SET;
				}
				else {
					// Try to process an output sample.
					ProcessOutputLoop();
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::BeginStreaming()
			{
				HRESULT hr = S_OK;

				// Start the scheduler thread.
				hr = m_scheduler.StartScheduler( m_pClock );

				return hr;
			}

			HRESULT EVRCustomPresenter::EndStreaming()
			{
				HRESULT hr = S_OK;

				// Stop the scheduler thread.
				hr = m_scheduler.StopScheduler();

				return hr;
			}

			HRESULT EVRCustomPresenter::CheckEndOfStream()
			{
				if( !m_bEndStreaming ) {
					// The EVR did not send the MFVP_MESSAGE_ENDOFSTREAM message.
					return S_OK;
				}

				if( m_bSampleNotify ) {
					// The mixer still has input.
					return S_OK;
				}

				if( m_SamplePool.AreSamplesPending() ) {
					// Samples are still scheduled for rendering.
					return S_OK;
				}

				// Everything is complete. Now we can tell the EVR that we are done.
				NotifyEvent( EC_COMPLETE, (LONG_PTR) S_OK, 0 );
				m_bEndStreaming = FALSE;

				return S_OK;
			}

			HRESULT EVRCustomPresenter::PrepareFrameStep( DWORD cSteps )
			{
				HRESULT hr = S_OK;

				// Cache the step count.
				m_FrameStep.steps += cSteps;

				// Set the frame-step state. 
				m_FrameStep.state = WaitingStart;

				// If the clock is are already running, we can start frame-stepping now.
				// Otherwise, we will start when the clock starts.
				if( m_RenderState == Started ) {
					hr = StartFrameStep();
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::StartFrameStep()
			{
				assert( m_RenderState == Started );

				HRESULT hr = S_OK;

				if( m_FrameStep.state == WaitingStart ) {
					// We have a frame-step request, and are waiting for the clock to start.
					// Set the state to "pending," which means we are waiting for samples.
					m_FrameStep.state = Pending;

					// If the frame-step queue already has samples, process them now.
					while( !m_FrameStep.samples.IsEmpty() && ( m_FrameStep.state == Pending ) ) {
						ScopedComPtr<IMFSample> pSample;
						hr = m_FrameStep.samples.RemoveFront( &pSample );
						if( FAILED( hr ) )
							return hr;

						hr = DeliverFrameStepSample( pSample );
						if( FAILED( hr ) )
							return hr;

						// We break from this loop when:
						//   (a) the frame-step queue is empty, or
						//   (b) the frame-step operation is complete.
					}
				}
				else if( m_FrameStep.state == None ) {
					// We are not frame stepping. Therefore, if the frame-step queue has samples, 
					// we need to process them normally.
					while( !m_FrameStep.samples.IsEmpty() ) {
						ScopedComPtr<IMFSample> pSample;
						hr = m_FrameStep.samples.RemoveFront( &pSample );
						if( FAILED( hr ) )
							return hr;

						hr = DeliverSample( pSample, FALSE );
						if( FAILED( hr ) )
							return hr;
					}
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::CompleteFrameStep( IMFSample *pSample )
			{
				HRESULT hr = S_OK;
				MFTIME hnsSampleTime = 0;
				MFTIME hnsSystemTime = 0;

				// Update our state.
				m_FrameStep.state = Complete;
				m_FrameStep.pSampleNoRef = NULL;

				// Notify the EVR that the frame-step is complete.
				NotifyEvent( EC_STEP_COMPLETE, FALSE, 0 ); // FALSE = completed (not cancelled)

				// If we are scrubbing (rate == 0), also send the "scrub time" event.
				if( IsScrubbing() ) {
					// Get the time stamp from the sample.
					hr = pSample->GetSampleTime( &hnsSampleTime );
					if( FAILED( hr ) ) {
						// No time stamp. Use the current presentation time.
						if( m_pClock ) {
							(void) m_pClock->GetCorrelatedTime( 0, &hnsSampleTime, &hnsSystemTime );
						}
						hr = S_OK; // (Not an error condition.)
					}

					NotifyEvent( EC_SCRUB_TIME, LODWORD( hnsSampleTime ), HIDWORD( hnsSampleTime ) );
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::CancelFrameStep()
			{
				FramestepState oldState = m_FrameStep.state;

				m_FrameStep.state = None;
				m_FrameStep.steps = 0;
				m_FrameStep.pSampleNoRef = NULL;
				// Don't clear the frame-step queue yet, because we might frame step again.

				if( oldState > None && oldState < Complete ) {
					// We were in the middle of frame-stepping when it was cancelled.
					// Notify the EVR.
					NotifyEvent( EC_STEP_COMPLETE, TRUE, 0 ); // TRUE = cancelled
				}

				return S_OK;
			}

			HRESULT EVRCustomPresenter::CreateOptimalVideoType( IMFMediaType* pProposedType, IMFMediaType **ppOptimalType )
			{
				HRESULT hr = S_OK;

				RECT rcOutput;
				ZeroMemory( &rcOutput, sizeof( rcOutput ) );

				MFVideoArea displayArea;
				ZeroMemory( &displayArea, sizeof( displayArea ) );

				// Helper object to manipulate the optimal type.
				VideoType mtOptimal;
				mtOptimal.CreateEmptyType();

				do {
					// Clone the proposed type.
					hr = mtOptimal.CopyFrom( pProposedType );
					BREAK_ON_FAIL( hr );


					// Determine the size (width & height) of the video.
					AM_MEDIA_TYPE* pAMMedia = nullptr;
					/*LARGE_INTEGER  i64Size;*/
					MFVIDEOFORMAT* VideoFormat;

					hr = pProposedType->GetRepresentation( FORMAT_MFVideoFormat, (void**) &pAMMedia );
					BREAK_ON_FAIL( hr );

					VideoFormat = (MFVIDEOFORMAT*) pAMMedia->pbFormat;
					m_VideoSize.cx = VideoFormat->videoInfo.dwWidth;
					m_VideoSize.cy = VideoFormat->videoInfo.dwHeight;
					m_VideoARSize.cx = VideoFormat->videoInfo.PixelAspectRatio.Numerator;
					m_VideoARSize.cy = VideoFormat->videoInfo.PixelAspectRatio.Denominator;

					pProposedType->FreeRepresentation( FORMAT_MFVideoFormat, (void*) pAMMedia );


					// Modify the new type.

					// For purposes of this SDK sample, we assume 
					// 1) The monitor's pixels are square.
					// 2) The presenter always preserves the pixel aspect ratio.

					// Set the pixel aspect ratio (PAR) to 1:1 (see assumption #1, above)
					hr = mtOptimal.SetPixelAspectRatio( 1, 1 );
					BREAK_ON_FAIL( hr );

					// Get the output rectangle.
					rcOutput = m_pD3DPresentEngine->GetDestinationRect();
					if( IsRectEmpty( &rcOutput ) ) {
						// Calculate the output rectangle based on the media type.
						hr = CalculateOutputRectangle( pProposedType, &rcOutput );
						BREAK_ON_FAIL( hr );
					}

					// Set the extended color information: Use BT.709
					hr = mtOptimal.SetYUVMatrix( MFVideoTransferMatrix_BT709 );
					BREAK_ON_FAIL( hr );
					hr = mtOptimal.SetTransferFunction( MFVideoTransFunc_709 );
					BREAK_ON_FAIL( hr );
					hr = mtOptimal.SetVideoPrimaries( MFVideoPrimaries_BT709 );
					BREAK_ON_FAIL( hr );
					hr = mtOptimal.SetVideoNominalRange( MFNominalRange_16_235 );
					BREAK_ON_FAIL( hr );
					hr = mtOptimal.SetVideoLighting( MFVideoLighting_dim );
					BREAK_ON_FAIL( hr );

					// Set the target rect dimensions. 
					hr = mtOptimal.SetFrameDimensions( rcOutput.right, rcOutput.bottom );
					BREAK_ON_FAIL( hr );

					// Set the geometric aperture, and disable pan/scan.
					displayArea = MakeArea( 0, 0, rcOutput.right, rcOutput.bottom );

					hr = mtOptimal.SetPanScanEnabled( FALSE );
					BREAK_ON_FAIL( hr );

					hr = mtOptimal.SetGeometricAperture( displayArea );
					BREAK_ON_FAIL( hr );

					// Set the pan/scan aperture and the minimum display aperture. We don't care
					// about them per se, but the mixer will reject the type if these exceed the 
					// frame dimentions.
					hr = mtOptimal.SetPanScanAperture( displayArea );
					BREAK_ON_FAIL( hr );

					hr = mtOptimal.SetMinDisplayAperture( displayArea );
					BREAK_ON_FAIL( hr );

					// Return the pointer to the caller.
					*ppOptimalType = mtOptimal.Detach();
				} while( false );

				return hr;
			}

			HRESULT EVRCustomPresenter::CalculateOutputRectangle( IMFMediaType *pProposedType, RECT *prcOutput )
			{
				HRESULT hr = S_OK;
				UINT32  srcWidth = 0, srcHeight = 0;

				MFRatio inputPAR = { 0, 0 };
				MFRatio outputPAR = { 0, 0 };
				RECT    rcOutput = { 0, 0, 0, 0 };

				MFVideoArea displayArea;
				ZeroMemory( &displayArea, sizeof( displayArea ) );

				// Helper object to read the media type.
				VideoType mtProposed( pProposedType );

				// Get the source's frame dimensions.
				hr = mtProposed.GetFrameDimensions( &srcWidth, &srcHeight );
				if( FAILED( hr ) )
					return hr;

				// Get the source's display area. 
				hr = mtProposed.GetVideoDisplayArea( &displayArea );
				if( FAILED( hr ) )
					return hr;

				// Calculate the x,y offsets of the display area.
				LONG offsetX = (LONG) MFOffsetToFloat( displayArea.OffsetX );
				LONG offsetY = (LONG) MFOffsetToFloat( displayArea.OffsetY );

				// Use the display area if valid. Otherwise, use the entire frame.
				if( displayArea.Area.cx != 0 &&
					displayArea.Area.cy != 0 &&
					offsetX + displayArea.Area.cx <= (LONG) ( srcWidth ) &&
					offsetY + displayArea.Area.cy <= (LONG) ( srcHeight ) ) {
					rcOutput.left = offsetX;
					rcOutput.right = offsetX + displayArea.Area.cx;
					rcOutput.top = offsetY;
					rcOutput.bottom = offsetY + displayArea.Area.cy;
				}
				else {
					rcOutput.left = 0;
					rcOutput.top = 0;
					rcOutput.right = srcWidth;
					rcOutput.bottom = srcHeight;
				}

				// rcOutput is now either a sub-rectangle of the video frame, or the entire frame.

				// If the pixel aspect ratio of the proposed media type is different from the monitor's, 
				// letterbox the video. We stretch the image rather than shrink it.

				inputPAR = mtProposed.GetPixelAspectRatio();    // Defaults to 1:1

				outputPAR.Denominator = outputPAR.Numerator = 1; // This is an assumption of the sample.

				// Adjust to get the correct picture aspect ratio.
				*prcOutput = CorrectAspectRatio( rcOutput, inputPAR, outputPAR );

				return hr;
			}

			HRESULT EVRCustomPresenter::SetMediaType( IMFMediaType *pMediaType )
			{
				// Note: pMediaType can be NULL (to clear the type)

				// Clearing the media type is allowed in any state (including shutdown).
				if( pMediaType == NULL ) {
					SafeRelease( m_pMediaType );
					ReleaseResources();
					return S_OK;
				}

				HRESULT hr = S_OK;

				do {
					// Cannot set the media type after shutdown.
					hr = CheckShutdown();
					BREAK_ON_FAIL( hr );

					// Check if the new type is actually different.
					// Note: This function safely handles NULL input parameters.
					if( AreMediaTypesEqual( m_pMediaType, pMediaType ) ) {
						return S_OK; // Nothing more to do.
					}

					// We're really changing the type. First get rid of the old type.
					SafeRelease( m_pMediaType );
					ReleaseResources();

					// Initialize the presenter engine with the new media type.
					// The presenter engine allocates the samples. 
					VideoSampleList sampleQueue;
					hr = m_pD3DPresentEngine->CreateVideoSamples( pMediaType, sampleQueue );
					BREAK_ON_FAIL( hr );

					// Mark each sample with our token counter. If this batch of samples becomes
					// invalid, we increment the counter, so that we know they should be discarded. 
					for( auto pos = sampleQueue.FrontPosition(); pos != sampleQueue.EndPosition(); pos = sampleQueue.Next( pos ) ) {
						ScopedComPtr<IMFSample> pSample;
						hr = sampleQueue.GetItemPos( pos, &pSample );
						BREAK_ON_FAIL( hr );

						hr = pSample->SetUINT32( MFSamplePresenter_SampleCounter, m_TokenCounter );
						BREAK_ON_FAIL( hr );
					}

					// Add the samples to the sample pool.
					hr = m_SamplePool.Initialize( sampleQueue );
					BREAK_ON_FAIL( hr );

					// Set the frame rate on the scheduler. 
					MFRatio fps = { 0, 0 };
					if( SUCCEEDED( GetFrameRate( pMediaType, &fps ) ) && ( fps.Numerator != 0 ) && ( fps.Denominator != 0 ) ) {
						m_scheduler.SetFrameRate( fps );
					}
					else {
						// NOTE: The mixer's proposed type might not have a frame rate, in which case 
						// we'll use an arbitary default. (Although it's unlikely the video source
						// does not have a frame rate.)
						m_scheduler.SetFrameRate( sDefaultFrameRate );
					}

					// Store the media type.
					assert( pMediaType != NULL );

					m_pMediaType = pMediaType;
					m_pMediaType->AddRef();
				} while( false );

				if( FAILED( hr ) )
					ReleaseResources();

				return hr;
			}

			HRESULT EVRCustomPresenter::IsMediaTypeSupported( IMFMediaType *pMediaType )
			{
				HRESULT                 hr = S_OK;

				D3DFORMAT               d3dFormat = D3DFMT_UNKNOWN;
				BOOL                    bCompressed = FALSE;
				MFVideoInterlaceMode    InterlaceMode = MFVideoInterlace_Unknown;
				MFVideoArea             VideoCropArea;
				UINT32                  width = 0, height = 0;

				// Helper object for reading the proposed type.
				VideoType               mtProposed( pMediaType );

				// Reject compressed media types.
				hr = mtProposed.IsCompressedFormat( &bCompressed );
				if( FAILED( hr ) ) {
					return hr;
				}

				if( bCompressed ) {
					return MF_E_INVALIDMEDIATYPE;
				}

				// Validate the format.
				hr = mtProposed.GetFourCC( (DWORD*) &d3dFormat );
				if( FAILED( hr ) ) {
					return hr;
				}

				// The D3DPresentEngine checks whether the format can be used as
				// the back-buffer format for the swap chains.
				hr = m_pD3DPresentEngine->CheckFormat( d3dFormat );
				if( FAILED( hr ) ) {
					return hr;
				}

				// Reject interlaced formats.
				hr = mtProposed.GetInterlaceMode( &InterlaceMode );
				if( FAILED( hr ) ) {
					return hr;
				}

				if( InterlaceMode != MFVideoInterlace_Progressive ) {
					return MF_E_INVALIDMEDIATYPE;
				}

				hr = mtProposed.GetFrameDimensions( &width, &height );
				if( FAILED( hr ) ) {
					return hr;
				}

				// Validate the various apertures (cropping regions) against the frame size.
				// Any of these apertures may be unspecified in the media type, in which case 
				// we ignore it. We just want to reject invalid apertures.
				if( SUCCEEDED( mtProposed.GetPanScanAperture( &VideoCropArea ) ) ) {
					ValidateVideoArea( VideoCropArea, width, height );
				}
				if( SUCCEEDED( mtProposed.GetGeometricAperture( &VideoCropArea ) ) ) {
					ValidateVideoArea( VideoCropArea, width, height );
				}
				if( SUCCEEDED( mtProposed.GetMinDisplayAperture( &VideoCropArea ) ) ) {
					ValidateVideoArea( VideoCropArea, width, height );
				}

				return hr;
			}

			void EVRCustomPresenter::ProcessOutputLoop()
			{
				HRESULT hr = S_OK;

				// Process as many samples as possible.
				while( hr == S_OK ) {
					// If the mixer doesn't have a new input sample, break from the loop.
					if( !m_bSampleNotify ) {
						hr = MF_E_TRANSFORM_NEED_MORE_INPUT;
						break;
					}

					// Try to process a sample.
					hr = ProcessOutput();

					// NOTE: ProcessOutput can return S_FALSE to indicate it did not process a sample.
					// If so, we break out of the loop.
				}

				if( hr == MF_E_TRANSFORM_NEED_MORE_INPUT ) {
					// The mixer has run out of input data. Check if we're at the end of the stream.
					CheckEndOfStream();
				}
			}

			//-----------------------------------------------------------------------------
			// ProcessOutput
			//
			// Attempts to get a new output sample from the mixer.
			//
			// Called in two situations:
			// (1) ProcessOutputLoop, if the mixer has a new input sample.
			// (2) Repainting the last frame.
			//-----------------------------------------------------------------------------
			HRESULT EVRCustomPresenter::ProcessOutput()
			{
				assert( m_bSampleNotify || m_bRepaint ); // See note above.

				HRESULT     hr = S_OK;
				DWORD       dwStatus = 0;
				LONGLONG    mixerStartTime = 0, mixerEndTime = 0;
				MFTIME      systemTime = 0;
				BOOL        bRepaint = m_bRepaint; // Temporarily store this state flag.

				// If the clock is not running, we present the first sample,
				// and then don't present any more until the clock starts.
				if( ( m_RenderState != Started ) && !m_bRepaint && m_bPrerolled ) {
					return S_FALSE;
				}

				// Make sure we have a pointer to the mixer.
				if( m_pMixer == NULL ) {
					return MF_E_INVALIDREQUEST;
				}

				// Try to get a free sample from the video sample pool.
				ScopedComPtr<IMFSample> pSample;
				hr = m_SamplePool.GetSample( &pSample );
				if( hr == MF_E_SAMPLEALLOCATOR_EMPTY ) {
					return S_FALSE; // No free samples. We'll try again when a sample is released.
				}
				if( FAILED( hr ) ) {
					return hr;
				}

				// From now on, we have a valid video sample pointer, where the mixer will
				// write the video data.
				assert( pSample != NULL );

				// (If the following assertion fires, it means we are not managing the sample pool correctly.)
				assert( MFGetAttributeUINT32( pSample, MFSamplePresenter_SampleCounter, (UINT32) -1 ) == m_TokenCounter );

				if( m_bRepaint ) {
					// Repaint request. Ask the mixer for the most recent sample.
					SetDesiredSampleTime( pSample, m_scheduler.LastSampleTime(), m_scheduler.FrameDuration() );
					m_bRepaint = FALSE; // OK to clear this flag now.
				}
				else {
					// Not a repaint request. Clear the desired sample time; the mixer will
					// give us the next frame in the stream.
					ClearDesiredSampleTime( pSample );

					if( m_pClock ) {
						// Latency: Record the starting time for the ProcessOutput operation. 
						(void) m_pClock->GetCorrelatedTime( 0, &mixerStartTime, &systemTime );
					}
				}

				// Now we are ready to get an output sample from the mixer.
				MFT_OUTPUT_DATA_BUFFER dataBuffer;
				ZeroMemory( &dataBuffer, sizeof( dataBuffer ) );

				dataBuffer.dwStreamID = 0;
				dataBuffer.pSample = pSample;
				dataBuffer.dwStatus = 0;

				hr = m_pMixer->ProcessOutput( 0, 1, &dataBuffer, &dwStatus );

				if( FAILED( hr ) ) {
					// Return the sample to the pool.
					HRESULT hr2 = m_SamplePool.ReturnSample( pSample );
					if( FAILED( hr2 ) ) {
						hr = hr2;
						if( FAILED( hr ) ) {
							SafeRelease( dataBuffer.pEvents );
							return hr;
						}
					}

					// Handle some known error codes from ProcessOutput.
					if( hr == MF_E_TRANSFORM_TYPE_NOT_SET ) {
						// The mixer's format is not set. Negotiate a new format.
						hr = RenegotiateMediaType();
					}
					else if( hr == MF_E_TRANSFORM_STREAM_CHANGE ) {
						// There was a dynamic media type change. Clear our media type.
						SetMediaType( NULL );
					}
					else if( hr == MF_E_TRANSFORM_NEED_MORE_INPUT ) {
						// The mixer needs more input. 
						// We have to wait for the mixer to get more input.
						m_bSampleNotify = FALSE;
					}
				}
				else {
					// We got an output sample from the mixer.
					if( m_pClock && !bRepaint ) {
						// Latency: Record the ending time for the ProcessOutput operation,
						// and notify the EVR of the latency. 

						(void) m_pClock->GetCorrelatedTime( 0, &mixerEndTime, &systemTime );

						LONGLONG latencyTime = mixerEndTime - mixerStartTime;
						NotifyEvent( EC_PROCESSING_LATENCY, (LONG_PTR) &latencyTime, 0 );
					}

					// Set up notification for when the sample is released.
					hr = TrackSample( pSample );
					if( FAILED( hr ) ) {
						SafeRelease( dataBuffer.pEvents );
						return hr;
					}

					// Schedule the sample.
					if( ( m_FrameStep.state == None ) || bRepaint ) {
						hr = DeliverSample( pSample, bRepaint );
						if( FAILED( hr ) ) {
							SafeRelease( dataBuffer.pEvents );
							return hr;
						}
					}
					else {
						// We are frame-stepping (and this is not a repaint request).
						hr = DeliverFrameStepSample( pSample );
						if( FAILED( hr ) ) {
							SafeRelease( dataBuffer.pEvents );
							return hr;
						}
					}
					m_bPrerolled = TRUE; // We have presented at least one sample now.
				}

				// Release any events that were returned from the ProcessOutput method. 
				// (We don't expect any events from the mixer, but this is a good practice.)
				SafeRelease( dataBuffer.pEvents );

				return hr;
			}

			HRESULT EVRCustomPresenter::DeliverSample( IMFSample *pSample, BOOL bRepaint )
			{
				assert( pSample != NULL );

				// If we are not actively playing, OR we are scrubbing (rate = 0) OR this is a 
				// repaint request, then we need to present the sample immediately. Otherwise, 
				// schedule it normally.
				BOOL bPresentNow = ( ( m_RenderState != Started ) || IsScrubbing() || bRepaint );

				// Check the D3D device state.
				D3DPresentEngine::DeviceState state = D3DPresentEngine::DeviceOK;
				HRESULT hr = m_pD3DPresentEngine->CheckDeviceState( &state );

				if( SUCCEEDED( hr ) ) {
					hr = m_scheduler.ScheduleSample( pSample, bPresentNow );
				}

				if( FAILED( hr ) ) {
					// Notify the EVR that we have failed during streaming. The EVR will notify the 
					// pipeline (ie, it will notify the Filter Graph Manager in DirectShow or the 
					// Media Session in Media Foundation).
					NotifyEvent( EC_ERRORABORT, hr, 0 );
				}
				else if( state == D3DPresentEngine::DeviceReset ) {
					// The Direct3D device was re-set. Notify the EVR.
					NotifyEvent( EC_DISPLAY_CHANGED, S_OK, 0 );
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::DeliverFrameStepSample( IMFSample *pSample )
			{
				HRESULT hr = S_OK;

				// For rate 0, discard any sample that ends earlier than the clock time.
				if( IsScrubbing() && m_pClock && IsSampleTimePassed( m_pClock, pSample ) ) {
					// Discard this sample.
				}
				else if( m_FrameStep.state >= Scheduled ) {
					// A frame was already submitted. Put this sample on the frame-step queue, 
					// in case we are asked to step to the next frame. If frame-stepping is
					// cancelled, this sample will be processed normally.
					hr = m_FrameStep.samples.InsertBack( pSample );
					if( FAILED( hr ) ) {
						return hr;
					}
				}
				else {
					// We're ready to frame-step.

					// Decrement the number of steps.
					if( m_FrameStep.steps > 0 ) {
						m_FrameStep.steps--;
					}

					if( m_FrameStep.steps > 0 ) {
						// This is not the last step. Discard this sample.
					}
					else if( m_FrameStep.state == WaitingStart ) {
						// This is the right frame, but the clock hasn't started yet. Put the
						// sample on the frame-step queue. When the clock starts, the sample
						// will be processed.
						hr = m_FrameStep.samples.InsertBack( pSample );
						if( FAILED( hr ) ) {
							return hr;
						}
					}
					else {
						// This is the right frame *and* the clock has started. Deliver this sample.
						hr = DeliverSample( pSample, FALSE );
						if( FAILED( hr ) ) {
							return hr;
						}

						// QI for IUnknown so that we can identify the sample later.
						// (Per COM rules, an object alwayss return the same pointer when QI'ed for IUnknown.)
						ScopedComPtr<IUnknown> pUnk;
						hr = pSample->QueryInterface( __uuidof( IUnknown ), (void**) &pUnk );
						if( FAILED( hr ) ) {
							return hr;
						}

						// Save this value.
						m_FrameStep.pSampleNoRef = (DWORD_PTR) pUnk.get(); // No add-ref. 

						// NOTE: We do not AddRef the IUnknown pointer, because that would prevent the 
						// sample from invoking the OnSampleFree callback after the sample is presented. 
						// We use this IUnknown pointer purely to identify the sample later; we never
						// attempt to dereference the pointer.

						// Update our state.
						m_FrameStep.state = Scheduled;
					}
				}

				return hr;
			}

			HRESULT EVRCustomPresenter::TrackSample( IMFSample *pSample )
			{
				ScopedComPtr<IMFTrackedSample> pTracked;

				HRESULT hr = pSample->QueryInterface( __uuidof( IMFTrackedSample ), (void**) &pTracked );
				if( FAILED( hr ) ) {
					return hr;
				}

				hr = pTracked->SetAllocator( &m_SampleFreeCB, NULL );
				if( FAILED( hr ) ) {
					return hr;
				}

				return hr;
			}

			void EVRCustomPresenter::ReleaseResources()
			{
				// Increment the token counter to indicate that all existing video samples
				// are "stale." As these samples get released, we'll dispose of them. 
				//
				// Note: The token counter is required because the samples are shared
				// between more than one thread, and they are returned to the presenter 
				// through an asynchronous callback (OnSampleFree). Without the token, we
				// might accidentally re-use a stale sample after the ReleaseResources
				// method returns.

				m_TokenCounter++;

				Flush();

				m_SamplePool.Clear();

				m_pD3DPresentEngine->ReleaseResources();
			}

			HRESULT EVRCustomPresenter::OnSampleFree( IMFAsyncResult *pResult )
			{
				HRESULT hr = S_OK;

				do {
					// Get the sample from the async result object.
					ScopedComPtr<IUnknown> pObject;
					hr = pResult->GetObject( &pObject );
					BREAK_ON_FAIL( hr );

					ScopedComPtr<IMFSample> pSample;
					hr = pObject.QueryInterface( __uuidof( IMFSample ), (void**) &pSample );
					BREAK_ON_FAIL( hr );

					// If this sample was submitted for a frame-step, then the frame step is complete.
					if( m_FrameStep.state == Scheduled ) {
						// QI the sample for IUnknown and compare it to our cached value.
						ScopedComPtr<IUnknown> pUnk;
						hr = pSample.QueryInterface( __uuidof( IMFSample ), (void**) &pUnk );
						BREAK_ON_FAIL( hr );

						if( m_FrameStep.pSampleNoRef == (DWORD_PTR) pUnk.get() ) {
							// Notify the EVR. 
							hr = CompleteFrameStep( pSample );
							BREAK_ON_FAIL( hr );
						}

						// Note: Although pObject is also an IUnknown pointer, it's not guaranteed
						// to be the exact pointer value returned via QueryInterface, hence the 
						// need for the second QI.
					}

					ScopedCriticalSection lock( m_ObjectLock );

					if( MFGetAttributeUINT32( pSample, MFSamplePresenter_SampleCounter, (UINT32) -1 ) == m_TokenCounter ) {
						// Return the sample to the sample pool.
						hr = m_SamplePool.ReturnSample( pSample );
						BREAK_ON_FAIL( hr );

						// Now that a free sample is available, process more data if possible.
						(void) ProcessOutputLoop();
					}
				} while( false );

				if( FAILED( hr ) ) {
					NotifyEvent( EC_ERRORABORT, hr, 0 );
				}

				return hr;
			}

			float EVRCustomPresenter::GetMaxRate( BOOL bThin )
			{
				// Non-thinned:
				// If we have a valid frame rate and a monitor refresh rate, the maximum 
				// playback rate is equal to the refresh rate. Otherwise, the maximum rate 
				// is unbounded (FLT_MAX).

				// Thinned: The maximum rate is unbounded.

				float   fMaxRate = FLT_MAX;

				if( !bThin && ( m_pMediaType != NULL ) ) {
					MFRatio fps = { 0, 0 };
					GetFrameRate( m_pMediaType, &fps );

					UINT MonitorRateHz = m_pD3DPresentEngine->RefreshRate();
					if( fps.Denominator && fps.Numerator && MonitorRateHz ) {
						// Max Rate = Refresh Rate / Frame Rate
						fMaxRate = (float) MulDiv( MonitorRateHz, fps.Denominator, fps.Numerator );
					}
				}

				return fMaxRate;
			}

		} // namespace video
	} // namespace msw
} // namespace cinder

#endif // CINDER_MSW