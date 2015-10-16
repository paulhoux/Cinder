#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/detail/PresenterDX9.h"

#pragma comment(lib, "dxva2.lib")
#pragma comment(lib, "evr.lib")

static const GUID DXVA2_ModeH264_E = {
	0x1b81be68, 0xa0c7, 0x11d3,{ 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5 }
};

static const GUID DXVA2_Intel_ModeH264_E = {
	0x604F8E68, 0x4951, 0x4c54,{ 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6 }
};

namespace cinder {
namespace msw {
namespace detail {

/*//
IMFSamplePool::IMFSamplePool( IDirectXVideoDecoderService* pDecoder, IMFMediaType* pMediaType )
	: m_nRefCount( 1 )
	, m_pDecoderService( pDecoder )
	, m_Count( 0 )
{
	ZeroMemory( m_aSurfaces, sizeof( m_aSurfaces ) );
	ZeroMemory( m_aSamples, sizeof( m_aSamples ) );

	assert( m_pDecoderService != NULL );
	assert( pMediaType != NULL );

	HRESULT hr = ConvertToDXVAType( pMediaType, &m_Desc );
	//BREAK_ON_FAIL( hr );

	m_pDecoderService->AddRef();
}

IMFSamplePool::~IMFSamplePool()
{
	for( DWORD i = 0; i < 8; ++i ) {
		SafeRelease( m_aSamples[i] );
		SafeRelease( m_aSurfaces[i] );
	}

	SafeRelease( m_pDecoderService );
}

// IUnknown
HRESULT IMFSamplePool::QueryInterface( REFIID iid, void ** ppv )
{
	if( !ppv ) {
		return E_POINTER;
	}
	if( iid == IID_IUnknown ) {
		*ppv = static_cast<IUnknown*>( this );
	}
	//else if( iid == __uuidof( IMFVideoDisplayControl ) ) {
	//*ppv = static_cast<IMFVideoDisplayControl*>( this );
	//}
	//else if( iid == __uuidof( IMFGetService ) ) {
	//*ppv = static_cast<IMFGetService*>( this );
	//}
	else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

HRESULT IMFSamplePool::SetMaxPoolSize( DWORD size )
{
	return E_NOTIMPL;
}

HRESULT IMFSamplePool::GetFreeSample( IMFSample **pSample )
{
	HRESULT hr = S_OK;

	do {
		// TODO: Check if we have a free sample available.


		// If not, check if there is room for one more.
		BREAK_IF_FALSE( m_Count < 8, E_FAIL );

		// If maximum pool size has  not been reached, create new sample.
		ScopedPtr<IDirect3DSurface9> pSurface;
		hr = m_pDecoderService->CreateSurface(
			m_Desc.SampleWidth, m_Desc.SampleHeight, 0, (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
			D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, &pSurface, NULL );
		BREAK_ON_FAIL( hr );

		ScopedPtr<IMFSample> pTemp;
		hr = MFCreateVideoSampleFromSurface( pSurface, &pTemp );
		BREAK_ON_FAIL( hr );

		// Store surface and sample.
		m_aSurfaces[0] = pSurface;
		m_aSurfaces[0]->AddRef();

		m_aSamples[0] = pTemp;
		m_aSamples[0]->AddRef();

		// TODO: Mark sample as 'in use' and set callback(?).

		( *pSample ) = pTemp;
		( *pSample )->AddRef();
	} while( FALSE );

	return hr;
}//*/

// ------------------------------------------------------------------------------

PresenterDX9::PresenterDX9( void )
	: m_bFullScreenState( FALSE )
	, m_DeviceResetToken( 0 )
	, m_pD3D9( NULL )
	, m_pD3DDevice( NULL )
	, m_pDeviceManager( NULL )
	, m_pSampleAllocator( NULL )
	, m_D3D9Module( NULL )
	, m_pDecoderService( NULL )
	, m_DecoderGUID( GUID_NULL )
	, m_b3DVideo( FALSE )
	, _Direct3DCreate9Ex( NULL )
{
}

PresenterDX9::~PresenterDX9( void )
{
	// Unload D3D9.
	if( m_D3D9Module ) {
		FreeLibrary( m_D3D9Module );
		m_D3D9Module = NULL;
	}
}

// IUnknown
HRESULT PresenterDX9::QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv )
{
	if( !ppv ) {
		return E_POINTER;
	}
	if( iid == IID_IUnknown ) {
		*ppv = static_cast<IUnknown*>( static_cast<IMFVideoDisplayControl*>( this ) );
	}
	else if( iid == __uuidof( IMFVideoDisplayControl ) ) {
		*ppv = static_cast<IMFVideoDisplayControl*>( this );
	}
	else if( iid == __uuidof( IMFGetService ) ) {
		*ppv = static_cast<IMFGetService*>( this );
	}
	else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

// IMFVideoDisplayControl
HRESULT PresenterDX9::GetFullscreen( __RPC__out BOOL* pfFullscreen )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = CheckShutdown();
	if( FAILED( hr ) ) {
		return hr;
	}

	if( pfFullscreen == NULL ) {
		return E_POINTER;
	}

	*pfFullscreen = m_bFullScreenState;

	return S_OK;
}

// IMFVideoDisplayControl
HRESULT PresenterDX9::SetFullscreen( BOOL fFullscreen )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		m_bFullScreenState = fFullscreen;

		//SafeRelease( m_pVideoDevice );
		//SafeRelease( m_pVideoProcessorEnum );
		//SafeRelease( m_pVideoProcessor );
	}

	return hr;
}

// IMFVideoDisplayControl
HRESULT PresenterDX9::SetVideoWindow( __RPC__in HWND hwndVideo )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		BREAK_IF_FALSE( IsWindow( hwndVideo ), E_INVALIDARG );

		m_pMonitors = new MonitorArray();
		if( !m_pMonitors ) {
			hr = E_OUTOFMEMORY;
			break;
		}

		hr = SetVideoMonitor( hwndVideo );
		BREAK_ON_FAIL( hr );

		m_hwndVideo = hwndVideo;

		hr = CreateDXVA2ManagerAndDevice();
		BREAK_ON_FAIL( hr );
	} while( FALSE );

	return hr;
}

// IMFGetService
HRESULT PresenterDX9::GetService( REFGUID guidService, REFIID riid, LPVOID * ppvObject )
{
	HRESULT hr = S_OK;

	if( guidService == MR_VIDEO_ACCELERATION_SERVICE ) {
		if( riid == __uuidof( IDirect3DDeviceManager9 ) ) {
			if( NULL != m_pDeviceManager ) {
				*ppvObject = ( void* ) static_cast<IUnknown*>( m_pDeviceManager );
				( (IUnknown*)*ppvObject )->AddRef();
			}
			else {
				hr = E_NOINTERFACE;
			}
		}
		/*//
		else if( riid == __uuidof( IMFVideoSampleAllocator ) ) {
			if( NULL == m_pSampleAllocator ) {
				hr = MFCreateVideoSampleAllocator( IID_IMFVideoSampleAllocator, (LPVOID*)&m_pSampleAllocator );
				if( SUCCEEDED( hr ) && NULL != m_pDeviceManager ) {
					hr = m_pSampleAllocator->SetDirectXManager( m_pDeviceManager );
				}
			}
			if( SUCCEEDED( hr ) ) {
				hr = m_pSampleAllocator->QueryInterface( riid, ppvObject );
			}
		}
		//*/
		else {
			hr = E_NOINTERFACE;
		}
	}
	else {
		hr = MF_E_UNSUPPORTED_SERVICE;
	}

	return hr;
}

// Presenter
HRESULT PresenterDX9::Initialize( void )
{
	if( !m_D3D9Module ) {
		// Dynamically load D3D9 functions (to avoid static linkage with d3d9.lib)
		m_D3D9Module = LoadLibrary( TEXT( "d3d9.dll" ) );

		if( !m_D3D9Module )
			return E_FAIL;

		_Direct3DCreate9Ex = reinterpret_cast<LPDIRECT3DCREATE9EX>( GetProcAddress( m_D3D9Module, "Direct3DCreate9Ex" ) );
		if( !_Direct3DCreate9Ex )
			return E_FAIL;
	}

	return S_OK;
}

STDMETHODIMP PresenterDX9::GetMonitorRefreshRate( DWORD * pdwRefreshRate )
{
	if( pdwRefreshRate == NULL ) {
		return E_POINTER;
	}

	if( m_lpCurrMon == NULL ) {
		return MF_E_INVALIDREQUEST;
	}

	*pdwRefreshRate = m_lpCurrMon->dwRefreshRate;

	return S_OK;
}

// Presenter
HRESULT PresenterDX9::IsMediaTypeSupported( IMFMediaType *pMediaType )
{
	DXVA2_ConfigPictureDecode* configs = NULL;

	HRESULT hr = S_OK;

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		BREAK_ON_NULL( pMediaType, E_POINTER );

		// Check device.
		if( m_pD3DDevice ) {}

		// Reject compressed formats.
		BOOL bIsCompressed;
		hr = pMediaType->IsCompressedFormat( &bIsCompressed );
		BREAK_ON_FAIL( hr );
		BREAK_IF_TRUE( bIsCompressed, MF_E_INVALIDMEDIATYPE );

#if MF_USE_DXVA2_DECODER
		// Check if we can decode this format in hardware.
		BREAK_ON_NULL( m_pDecoderService, E_POINTER );

		DXVA2_VideoDesc desc;
		hr = ConvertToDXVAType( pMediaType, &desc );
		BREAK_ON_FAIL( hr );

		UINT configCount = 0;
		hr = m_pDecoderService->GetDecoderConfigurations( m_DecoderGUID, &desc, NULL, &configCount, &configs );
		BREAK_ON_FAIL( hr );

		ScopedPtr<IDirect3DSurface9> pSurface;
		hr = m_pDecoderService->CreateSurface(
			desc.SampleWidth, desc.SampleHeight, 0, (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
			D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, &pSurface, NULL );
		BREAK_ON_FAIL( hr );

		IDirect3DSurface9* surfaces = pSurface.get();
		for( UINT i = 0; i < configCount; i++ ) {
			ScopedPtr<IDirectXVideoDecoder> pDecoder;
			hr = m_pDecoderService->CreateVideoDecoder( m_DecoderGUID, &desc, &configs[i], &surfaces, 1, &pDecoder );
			if( SUCCEEDED( hr ) && pDecoder )
				break;
		}
#else
		// Check if format can be used as the back-buffer format for the swap chains.
		GUID subType = GUID_NULL;
		hr = pMediaType->GetGUID( MF_MT_SUBTYPE, &subType );
		BREAK_ON_FAIL( hr );

		D3DFORMAT format = D3DFORMAT( subType.Data1 );

		D3DDISPLAYMODE mode;
		hr = m_pD3D9->GetAdapterDisplayMode( m_ConnectionGUID, &mode );
		BREAK_ON_FAIL( hr );

		hr = m_pD3D9->CheckDeviceType( m_ConnectionGUID, D3DDEVTYPE_HAL, mode.Format, format, !m_bFullScreenState );
		BREAK_ON_FAIL( hr );

		// Reject interlaced formats.
		MFVideoInterlaceMode interlaceMode;
		hr = pMediaType->GetUINT32( MF_MT_INTERLACE_MODE, (UINT32*)&interlaceMode );
		BREAK_ON_FAIL( hr );
		//BREAK_IF_FALSE( interlaceMode == MFVideoInterlace_Progressive, MF_E_INVALIDMEDIATYPE );
#endif
	} while( FALSE );

	if( FAILED( hr ) )
		hr = MF_E_INVALIDMEDIATYPE;

	CoTaskMemFree( configs );

	return hr;
}

// Presenter
HRESULT PresenterDX9::ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample )
{
	HRESULT hr = S_OK;
	//BYTE* pData = NULL;
	//DWORD dwSampleSize = 0;
	//IMFMediaBuffer* pEVBuffer = NULL;
	//IDirect3DDevice9* pDeviceInput = NULL;
	//ID3D11Texture2D* pTexture2D = NULL;
	//IMFDXGIBuffer* pDXGIBuffer = NULL;
	//ID3D11Texture2D* pEVTexture2D = NULL;
	//IMFDXGIBuffer* pEVDXGIBuffer = NULL;
	//ID3D11Device* pDeviceInput = NULL;
	UINT dwViewIndex = 0;
	UINT dwEVViewIndex = 0;

	ScopedCriticalSection lock( m_critSec );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		if( punInterlaceMode == NULL || pCurrentType == NULL || pSample == NULL || pbDeviceChanged == NULL || pbProcessAgain == NULL ) {
			hr = E_POINTER;
			break;
		}

		*pbProcessAgain = FALSE;
		*pbDeviceChanged = FALSE;

		DWORD cBuffers = 0;
		hr = pSample->GetBufferCount( &cBuffers );
		BREAK_ON_FAIL( hr );

		ScopedPtr<IMFMediaBuffer> pBuffer;
		if( 1 == cBuffers ) {
			hr = pSample->GetBufferByIndex( 0, &pBuffer );
		}
		else {
			hr = pSample->ConvertToContiguousBuffer( &pBuffer );
		}

		BREAK_ON_FAIL( hr );

		// Check if device is still valid.
		hr = CheckDeviceState( pbDeviceChanged );
		BREAK_ON_FAIL( hr );

		// Adjust output size.
		RECT rcDest;
		ZeroMemory( &rcDest, sizeof( rcDest ) );
		if( CheckEmptyRect( &rcDest ) ) {
			hr = S_OK;
			break;
		}

		MFVideoInterlaceMode unInterlaceMode = (MFVideoInterlaceMode)MFGetAttributeUINT32( pCurrentType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );

		// Check the per-sample attributes.
		if( MFVideoInterlace_MixedInterlaceOrProgressive == unInterlaceMode ) {
			BOOL fInterlaced = MFGetAttributeUINT32( pSample, MFSampleExtension_Interlaced, FALSE );
			if( !fInterlaced ) {
				// Progressive sample
				*punInterlaceMode = MFVideoInterlace_Progressive;
			}
			else {
				BOOL fBottomFirst = MFGetAttributeUINT32( pSample, MFSampleExtension_BottomFieldFirst, FALSE );
				if( fBottomFirst ) {
					*punInterlaceMode = MFVideoInterlace_FieldInterleavedLowerFirst;
				}
				else {
					*punInterlaceMode = MFVideoInterlace_FieldInterleavedUpperFirst;
				}
			}
		}

		//////////////

		// Allocate uncompressed buffers.
		DXVA2_VideoDesc desc;
		hr = ConvertToDXVAType( pCurrentType, &desc );
		BREAK_ON_FAIL( hr );

		//ScopedPtr<IDirect3DSurface9> pSurface;
		//hr = m_pDecoderService->CreateSurface(
		//	desc.SampleWidth, desc.SampleHeight, 0, (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
		//	D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, &pSurface, NULL );
		//BREAK_ON_FAIL( hr );

		//ScopedPtr<IMFSample> pSample;
		//hr = MFCreateVideoSampleFromSurface( pSurface, &pSample );
		//BREAK_ON_FAIL( hr );

		//
		ScopedPtr<IDirect3DSurface9> pSurface;
		hr = MFGetService( pBuffer, MR_BUFFER_SERVICE, __uuidof( IDirect3DSurface9 ), (void**)&pSurface );
		BREAK_ON_FAIL( hr );

		// Get the swap chain from the surface.
		//ScopedPtr<IDirect3DSwapChain9> pSwapChain;
		//hr = pSurface->GetContainer( __uuidof( IDirect3DSwapChain9 ), (LPVOID*)&pSwapChain );
		//BREAK_ON_FAIL( hr );
		
		// Present the swap chain.
		ScopedPtr<IDirect3DSurface9> pBackBuffer;
		//pSwapChain->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
		hr = m_pD3DDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
		BREAK_ON_FAIL( hr );

		// TEMP!

		// remember the original rectangles
		RECT TRectOld = m_rcDstApp;
		RECT SRectOld = m_rcSrcApp;
		UpdateRectangles( &TRectOld, &SRectOld );

		//Update destination rect with current client rect
		m_rcDstApp = rcDest;

		m_rcSrcApp.left = 0;
		m_rcSrcApp.top = 0;
		m_rcSrcApp.right = m_uiRealDisplayWidth;
		m_rcSrcApp.bottom = m_uiRealDisplayHeight;

		UpdateRectangles( &m_rcDstApp, &m_rcSrcApp );

		// END TEMP!

		//
		hr = m_pD3DDevice->StretchRect( pSurface, NULL, pBackBuffer, NULL, D3DTEXF_NONE );
		BREAK_ON_FAIL( hr );

		//hr = pSwapChain->Present( NULL, &rcDest, m_hwndVideo, NULL, 0 );
		hr = m_pD3DDevice->Present( NULL, &m_rcDstApp, m_hwndVideo, NULL );
		BREAK_ON_FAIL( hr );
		//*/

		/*//
		hr = pBuffer->QueryInterface( __uuidof( IMFDXGIBuffer ), (LPVOID*)&pDXGIBuffer );
		BREAK_ON_FAIL( hr );

		hr = pDXGIBuffer->GetResource( __uuidof( ID3D11Texture2D ), (LPVOID*)&pTexture2D );
		BREAK_ON_FAIL( hr );

		hr = pDXGIBuffer->GetSubresourceIndex( &dwViewIndex );
		BREAK_ON_FAIL( hr );

		pTexture2D->GetDevice( &pDeviceInput );
		if( ( NULL == pDeviceInput ) || ( pDeviceInput != m_pD3DDevice ) ) {
			break;
		}
		//*/

		/*//
		{
			hr = ProcessFrameUsingD3D11( pTexture2D, pEVTexture2D, dwViewIndex, dwEVViewIndex, rcDest, *punInterlaceMode, ppOutputSample );

			LONGLONG hnsDuration = 0;
			LONGLONG hnsTime = 0;
			DWORD dwSampleFlags = 0;

			if( ppOutputSample != NULL && *ppOutputSample != NULL ) {
				if( SUCCEEDED( pSample->GetSampleDuration( &hnsDuration ) ) ) {
					( *ppOutputSample )->SetSampleDuration( hnsDuration );
				}

				if( SUCCEEDED( pSample->GetSampleTime( &hnsTime ) ) ) {
					( *ppOutputSample )->SetSampleTime( hnsTime );
				}

				if( SUCCEEDED( pSample->GetSampleFlags( &dwSampleFlags ) ) ) {
					( *ppOutputSample )->SetSampleFlags( dwSampleFlags );
				}
			}
		}
		//*/
	} while( FALSE );

	return hr;
}

// Presenter
HRESULT PresenterDX9::SetCurrentMediaType( IMFMediaType *pMediaType )
{
	HRESULT hr = S_OK;
	IMFAttributes* pAttributes = NULL;

	ScopedCriticalSection lock( m_critSec );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		hr = pMediaType->QueryInterface( IID_IMFAttributes, reinterpret_cast<void**>( &pAttributes ) );
		BREAK_ON_FAIL( hr );

		hr = pAttributes->GetUINT32( MF_MT_VIDEO_3D, (UINT32*)&m_b3DVideo );
		if( SUCCEEDED( hr ) ) {
			// This is a 3D video and we only support it on Windows 8+.
			BREAK_IF_TRUE( m_b3DVideo, E_NOTIMPL );
		}

		//Now Determine Correct Display Resolution
		UINT32 parX = 0, parY = 0;
		int PARWidth = 0, PARHeight = 0;
		MFVideoArea videoArea = { 0 };
		ZeroMemory( &m_displayRect, sizeof( RECT ) );

		if( FAILED( MFGetAttributeSize( pMediaType, MF_MT_PIXEL_ASPECT_RATIO, &parX, &parY ) ) ) {
			parX = 1;
			parY = 1;
		}

		hr = GetVideoDisplayArea( pMediaType, &videoArea );
		BREAK_ON_FAIL( hr );

		m_displayRect = MFVideoAreaToRect( videoArea );

		PixelAspectToPictureAspect(
			videoArea.Area.cx,
			videoArea.Area.cy,
			parX,
			parY,
			&PARWidth,
			&PARHeight );

		SIZE szVideo = videoArea.Area;
		SIZE szPARVideo = { PARWidth, PARHeight };
		AspectRatioCorrectSize( &szVideo, szPARVideo, videoArea.Area, FALSE );
		m_uiRealDisplayWidth = szVideo.cx;
		m_uiRealDisplayHeight = szVideo.cy;

#if MF_USE_DXVA2_DECODER
		//// Allocate uncompressed buffers.
		//DXVA2_VideoDesc desc;
		//hr = ConvertToDXVAType( pMediaType, &desc );
		//BREAK_ON_FAIL( hr );

		//ScopedPtr<IDirect3DSurface9> pSurface;
		//hr = m_pDecoderService->CreateSurface(
		//	desc.SampleWidth, desc.SampleHeight, 0, (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
		//	D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, &pSurface, NULL );
		//BREAK_ON_FAIL( hr );

		//ScopedPtr<IMFSample> pSample;
		//hr = MFCreateVideoSampleFromSurface( pSurface, &pSample );
		//BREAK_ON_FAIL( hr );
#endif
	} while( FALSE );

	SafeRelease( pAttributes );

	return hr;
}

// Presenter
HRESULT PresenterDX9::Shutdown( void )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = MF_E_SHUTDOWN;

	m_IsShutdown = TRUE;

	SafeRelease( m_pDecoderService );
	SafeRelease( m_pSampleAllocator );
	SafeRelease( m_pD3DDevice );
	SafeRelease( m_pDeviceManager );
	SafeRelease( m_pD3D9 );

	return hr;
}

/// Private methods

HRESULT PresenterDX9::CheckDeviceState( BOOL * pbDeviceChanged )
{
	static int deviceStateChecks = 0;
	static D3D_DRIVER_TYPE driverType = D3D_DRIVER_TYPE_HARDWARE;

	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( pbDeviceChanged, E_POINTER );

#if MF_USE_DXVA2_DECODER
		// Test if video decoder is still valid.
		if( m_pDeviceManager != NULL ) {
			HANDLE hDevice = NULL;

			hr = m_pDeviceManager->OpenDeviceHandle( &hDevice );
			if( SUCCEEDED( hr ) ) {
				hr = m_pDeviceManager->TestDevice( hDevice );
				m_pDeviceManager->CloseDeviceHandle( hDevice );

				if( hr == DXVA2_E_NEW_VIDEO_DEVICE ) {
					*pbDeviceChanged = TRUE;

					// Create a new video decoder.
					hr = CreateDXVA2ManagerAndDevice();
					BREAK_ON_FAIL( hr );
				}
			}
		}
#endif

		hr = SetVideoMonitor( m_hwndVideo );
		BREAK_ON_FAIL( hr );

		if( m_pD3DDevice != NULL ) {
			// Lost/hung device. Destroy the device and create a new one.
			if( S_FALSE == hr ) {
				*pbDeviceChanged = TRUE;

				hr = CreateDXVA2ManagerAndDevice( driverType );
				BREAK_ON_FAIL( hr );

				/*SafeRelease( m_pVideoDevice );
				SafeRelease( m_pVideoProcessorEnum );
				SafeRelease( m_pVideoProcessor );
				SafeRelease( m_pSwapChain1 );*/

				deviceStateChecks = 0;
			}
			deviceStateChecks++;
		}

	} while( FALSE );

	return hr;
}

HRESULT PresenterDX9::CreateDXVA2ManagerAndDevice( D3D_DRIVER_TYPE DriverType )
{
	HRESULT hr = S_OK;

	do {
		if( m_pD3D9 == NULL ) {
			// Create Direct3D
			hr = (_Direct3DCreate9Ex)( D3D_SDK_VERSION, &m_pD3D9 );
			BREAK_ON_FAIL( hr );
		}

#if MF_USE_DXVA2_DECODER
		// Ensure we can do the YCbCr->RGB conversion in StretchRect.
		hr = m_pD3D9->CheckDeviceFormatConversion( D3DADAPTER_DEFAULT,
												   D3DDEVTYPE_HAL,
												   (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
												   D3DFMT_X8R8G8B8 );
		BREAK_ON_FAIL( hr );
#endif

		// Close existing video decoder and device manager.
		SafeRelease( m_pDecoderService );
		SafeRelease( m_pDeviceManager );

		// Create the device manager and initialize it.
		hr = DXVA2CreateDirect3DDeviceManager9( &m_DeviceResetToken, &m_pDeviceManager );
		BREAK_ON_FAIL( hr );

		// Get the device caps for this adapter.
		D3DCAPS9 ddCaps;
		ZeroMemory( &ddCaps, sizeof( ddCaps ) );

		hr = m_pD3D9->GetDeviceCaps( m_ConnectionGUID, D3DDEVTYPE_HAL, &ddCaps );
		BREAK_ON_FAIL( hr );

		// Check if device supports hardware processing.
		BREAK_IF_FALSE( ddCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT, E_FAIL );

		// Note: The presenter creates additional swap chains to present the
		// video frames. Therefore, it does not use the device's implicit 
		// swap chain, so the size of the back buffer here is 1 x 1.

		D3DPRESENT_PARAMETERS pPresentParams;
		ZeroMemory( &pPresentParams, sizeof( pPresentParams ) );

		pPresentParams.BackBufferWidth = 0;
		pPresentParams.BackBufferHeight = 0;
		pPresentParams.BackBufferCount = 1;
		pPresentParams.Windowed = TRUE;
		pPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD; // D3DSWAPEFFECT_COPY;
		pPresentParams.BackBufferFormat = D3DFMT_UNKNOWN;
		pPresentParams.hDeviceWindow = m_hwndVideo; // GetDesktopWindow();
		pPresentParams.Flags = D3DPRESENTFLAG_VIDEO;
		pPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

		// Create the device. The interop definition states D3DCREATE_MULTITHREADED is required, but it may vary by vendor.
		hr = m_pD3D9->CreateDeviceEx( m_ConnectionGUID, D3DDEVTYPE_HAL, pPresentParams.hDeviceWindow,
									  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
									  &pPresentParams, NULL, &m_pD3DDevice );
		BREAK_ON_FAIL( hr );

		// Get the adapter display mode.
		hr = m_pD3D9->GetAdapterDisplayMode( m_ConnectionGUID, &m_DisplayMode );
		BREAK_ON_FAIL( hr );

		// Reset the D3DDeviceManager with the new device.
		hr = m_pDeviceManager->ResetDevice( m_pD3DDevice, m_DeviceResetToken );
		BREAK_ON_FAIL( hr );

#if MF_USE_DXVA2_DECODER
		// Create the video decoder service.
		HANDLE hDevice = NULL;
		hr = m_pDeviceManager->OpenDeviceHandle( &hDevice );
		BREAK_ON_FAIL( hr );

		hr = m_pDeviceManager->GetVideoService( hDevice, __uuidof( IDirectXVideoDecoderService ), (LPVOID*)&m_pDecoderService );
		m_pDeviceManager->CloseDeviceHandle( hDevice );
		BREAK_ON_FAIL( hr );

		// Find compatible decoder. At this moment we only support H264...
		GUID *decoderDevices = NULL;
		UINT deviceCount = 0;
		hr = m_pDecoderService->GetDecoderDeviceGuids( &deviceCount, &decoderDevices );
		BREAK_ON_FAIL( hr );

		BOOL bFound = FALSE;
		if( SUCCEEDED( hr ) ) {
			for( UINT i = 0; i < deviceCount; ++i ) {
				if( decoderDevices[i] == DXVA2_ModeH264_E ||
					decoderDevices[i] == DXVA2_Intel_ModeH264_E ) {
					m_DecoderGUID = decoderDevices[i];
					bFound = TRUE;
					break;
				}
			}
		}
		CoTaskMemFree( decoderDevices );
		BREAK_IF_FALSE( bFound, E_FAIL );
#endif
	} while( false );

	return hr;
}

HRESULT PresenterDX9::GetVideoDisplayArea( IMFMediaType* pType, MFVideoArea* pArea )
{
	HRESULT hr = S_OK;
	BOOL bPanScan = FALSE;
	UINT32 uimageWidthInPixels = 0, uimageHeightInPixels = 0;

	hr = MFGetAttributeSize( pType, MF_MT_FRAME_SIZE, &uimageWidthInPixels, &uimageHeightInPixels );
	if( FAILED( hr ) ) {
		return hr;
	}

	if( uimageWidthInPixels != m_imageWidthInPixels || uimageHeightInPixels != m_imageHeightInPixels ) {
		//SafeRelease( m_pVideoProcessorEnum );
		//SafeRelease( m_pVideoProcessor );
		//SafeRelease( m_pSwapChain1 );
	}

	m_imageWidthInPixels = uimageWidthInPixels;
	m_imageHeightInPixels = uimageHeightInPixels;

	bPanScan = MFGetAttributeUINT32( pType, MF_MT_PAN_SCAN_ENABLED, FALSE );

	// In pan/scan mode, try to get the pan/scan region.
	if( bPanScan ) {
		hr = pType->GetBlob(
			MF_MT_PAN_SCAN_APERTURE,
			(UINT8*)pArea,
			sizeof( MFVideoArea ),
			NULL
			);
	}

	// If not in pan/scan mode, or the pan/scan region is not set,
	// get the minimimum display aperture.

	if( !bPanScan || hr == MF_E_ATTRIBUTENOTFOUND ) {
		hr = pType->GetBlob(
			MF_MT_MINIMUM_DISPLAY_APERTURE,
			(UINT8*)pArea,
			sizeof( MFVideoArea ),
			NULL
			);

		if( hr == MF_E_ATTRIBUTENOTFOUND ) {
			// Minimum display aperture is not set.

			// For backward compatibility with some components,
			// check for a geometric aperture.

			hr = pType->GetBlob(
				MF_MT_GEOMETRIC_APERTURE,
				(UINT8*)pArea,
				sizeof( MFVideoArea ),
				NULL
				);
		}

		// Default: Use the entire video area.

		if( hr == MF_E_ATTRIBUTENOTFOUND ) {
			*pArea = MFMakeArea( 0.0, 0.0, m_imageWidthInPixels, m_imageHeightInPixels );
			hr = S_OK;
		}
	}

	return hr;
}

HRESULT PresenterDX9::ProcessFrameUsingD3D9( IDirect3DTexture9* pLeftTexture2D, IDirect3DTexture9* pRightTexture2D, UINT dwLeftViewIndex, UINT dwRightViewIndex, RECT rcDest, UINT32 unInterlaceMode, IMFSample** ppVideoOutFrame )
{
	HRESULT hr = S_OK;
	/*//
	//ID3D11VideoContext* pVideoContext = NULL;
	//ID3D11VideoProcessorInputView* pLeftInputView = NULL;
	//ID3D11VideoProcessorInputView* pRightInputView = NULL;
	//ID3D11VideoProcessorOutputView* pOutputView = NULL;
	//ID3D11Texture2D* pDXGIBackBuffer = NULL;
	//ID3D11RenderTargetView* pRTView = NULL;
	IMFSample* pRTSample = NULL;
	IMFMediaBuffer* pBuffer = NULL;
	//D3D11_VIDEO_PROCESSOR_CAPS vpCaps = { 0 };
	LARGE_INTEGER lpcStart, lpcEnd;

	do {
		//if( !m_pVideoDevice ) {
		//	hr = m_pD3DDevice->QueryInterface( __uuidof( ID3D11VideoDevice ), (void**)&m_pVideoDevice );
		//	BREAK_ON_FAIL( hr );
		//}

		//hr = m_pD3DImmediateContext->QueryInterface( __uuidof( ID3D11VideoContext ), (void**)&pVideoContext );
		//BREAK_ON_FAIL( hr );

		// remember the original rectangles
		RECT TRectOld = m_rcDstApp;
		RECT SRectOld = m_rcSrcApp;
		UpdateRectangles( &TRectOld, &SRectOld );

		//Update destination rect with current client rect
		m_rcDstApp = rcDest;

		//D3D11_TEXTURE2D_DESC surfaceDesc;
		//pLeftTexture2D->GetDesc( &surfaceDesc );

		//if( !m_pVideoProcessorEnum || !m_pVideoProcessor || m_imageWidthInPixels != surfaceDesc.Width || m_imageHeightInPixels != surfaceDesc.Height ) {
		//	SafeRelease( m_pVideoProcessorEnum );
		//	SafeRelease( m_pVideoProcessor );

		//	m_imageWidthInPixels = surfaceDesc.Width;
		//	m_imageHeightInPixels = surfaceDesc.Height;

		//	D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
		//	ZeroMemory( &ContentDesc, sizeof( ContentDesc ) );
		//	ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
		//	ContentDesc.InputWidth = surfaceDesc.Width;
		//	ContentDesc.InputHeight = surfaceDesc.Height;
		//	ContentDesc.OutputWidth = surfaceDesc.Width;
		//	ContentDesc.OutputHeight = surfaceDesc.Height;
		//	ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

		//	hr = m_pVideoDevice->CreateVideoProcessorEnumerator( &ContentDesc, &m_pVideoProcessorEnum );
		//	BREAK_ON_FAIL( hr );

		//	UINT uiFlags;
		//	DXGI_FORMAT VP_Output_Format = DXGI_FORMAT_B8G8R8A8_UNORM;

		//	hr = m_pVideoProcessorEnum->CheckVideoProcessorFormat( VP_Output_Format, &uiFlags );
		//	if( FAILED( hr ) || 0 == ( uiFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT ) ) {
		//		hr = MF_E_UNSUPPORTED_D3D_TYPE;
		//		break;
		//	}

		//	m_rcSrcApp.left = 0;
		//	m_rcSrcApp.top = 0;
		//	m_rcSrcApp.right = m_uiRealDisplayWidth;
		//	m_rcSrcApp.bottom = m_uiRealDisplayHeight;

		//	DWORD index;
		//	hr = FindBOBProcessorIndex( &index );
		//	BREAK_ON_FAIL( hr );

		//	hr = m_pVideoDevice->CreateVideoProcessor( m_pVideoProcessorEnum, index, &m_pVideoProcessor );
		//	BREAK_ON_FAIL( hr );

		//	if( m_b3DVideo ) {
		//		hr = m_pVideoProcessorEnum->GetVideoProcessorCaps( &vpCaps );
		//		BREAK_ON_FAIL( hr );

		//		if( vpCaps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_STEREO ) {
		//			m_bStereoEnabled = TRUE;
		//		}

		//		DXGI_MODE_DESC1 modeFilter = { 0 };
		//		modeFilter.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		//		modeFilter.Width = surfaceDesc.Width;
		//		modeFilter.Height = surfaceDesc.Height;
		//		modeFilter.Stereo = m_bStereoEnabled;

		//		DXGI_MODE_DESC1 matchedMode;
		//		if( m_bFullScreenState ) {
		//			hr = m_pDXGIOutput1->FindClosestMatchingMode1( &modeFilter, &matchedMode, m_pD3DDevice );
		//			BREAK_ON_FAIL( hr );
		//		}
		//	}
		//}

		// now create the input and output media types - these need to reflect
		// the src and destination rectangles that we have been given.
		RECT TRect = m_rcDstApp;
		RECT SRect = m_rcSrcApp;
		UpdateRectangles( &TRect, &SRect );

		const BOOL fDestRectChanged = !EqualRect( &TRect, &TRectOld );

		if( !m_pSwapChain1 || fDestRectChanged ) {
			hr = UpdateDXGISwapChain();
			BREAK_ON_FAIL( hr );
		}

		m_bCanProcessNextSample = FALSE;

		// Get Backbuffer
		hr = m_pSwapChain1->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&pDXGIBackBuffer );
		BREAK_ON_FAIL( hr );

		// create the output media sample
		hr = MFCreateSample( &pRTSample );
		BREAK_ON_FAIL( hr );

		hr = MFCreateDXGISurfaceBuffer( __uuidof( ID3D11Texture2D ), pDXGIBackBuffer, 0, FALSE, &pBuffer );
		BREAK_ON_FAIL( hr );

		hr = pRTSample->AddBuffer( pBuffer );
		BREAK_ON_FAIL( hr );

		if( m_b3DVideo && FALSE  ) { // 0 != m_vp3DOutput 
			SafeRelease( pBuffer );

			hr = MFCreateDXGISurfaceBuffer( __uuidof( ID3D11Texture2D ), pDXGIBackBuffer, 1, FALSE, &pBuffer );
			BREAK_ON_FAIL( hr );

			hr = pRTSample->AddBuffer( pBuffer );
			BREAK_ON_FAIL( hr );
		}

		QueryPerformanceCounter( &lpcStart );

		QueryPerformanceCounter( &lpcEnd );

		//
		// Create Output View of Output Surfaces.
		//
		D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
		ZeroMemory( &OutputViewDesc, sizeof( OutputViewDesc ) );
		if( m_b3DVideo && m_bStereoEnabled ) {
			OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2DARRAY;
		}
		else {
			OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
		}
		OutputViewDesc.Texture2D.MipSlice = 0;
		OutputViewDesc.Texture2DArray.MipSlice = 0;
		OutputViewDesc.Texture2DArray.FirstArraySlice = 0;
		if( m_b3DVideo && FALSE  ) { // 0 != m_vp3DOutput
			OutputViewDesc.Texture2DArray.ArraySize = 2; // STEREO
		}

		QueryPerformanceCounter( &lpcStart );

		hr = m_pVideoDevice->CreateVideoProcessorOutputView( pDXGIBackBuffer, m_pVideoProcessorEnum, &OutputViewDesc, &pOutputView );
		BREAK_ON_FAIL( hr );

		D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputLeftViewDesc;
		ZeroMemory( &InputLeftViewDesc, sizeof( InputLeftViewDesc ) );
		InputLeftViewDesc.FourCC = 0;
		InputLeftViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
		InputLeftViewDesc.Texture2D.MipSlice = 0;
		InputLeftViewDesc.Texture2D.ArraySlice = dwLeftViewIndex;

		hr = m_pVideoDevice->CreateVideoProcessorInputView( pLeftTexture2D, m_pVideoProcessorEnum, &InputLeftViewDesc, &pLeftInputView );
		BREAK_ON_FAIL( hr );

		if( m_b3DVideo && FALSE  && pRightTexture2D ) { // MFVideo3DSampleFormat_MultiView == m_vp3DOutput
			D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputRightViewDesc;
			ZeroMemory( &InputRightViewDesc, sizeof( InputRightViewDesc ) );
			InputRightViewDesc.FourCC = 0;
			InputRightViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
			InputRightViewDesc.Texture2D.MipSlice = 0;
			InputRightViewDesc.Texture2D.ArraySlice = dwRightViewIndex;

			hr = m_pVideoDevice->CreateVideoProcessorInputView( pRightTexture2D, m_pVideoProcessorEnum, &InputRightViewDesc, &pRightInputView );
			BREAK_ON_FAIL( hr );
		}
		QueryPerformanceCounter( &lpcEnd );

		QueryPerformanceCounter( &lpcStart );

		SetVideoContextParameters( pVideoContext, &SRect, &TRect, unInterlaceMode );

#if (WINVER >= _WIN32_WINNT_WIN8)
		// Enable/Disable Stereo
		if( m_b3DVideo ) {
			pVideoContext->VideoProcessorSetOutputStereoMode( m_pVideoProcessor, m_bStereoEnabled );

			D3D11_VIDEO_PROCESSOR_STEREO_FORMAT vpStereoFormat = D3D11_VIDEO_PROCESSOR_STEREO_FORMAT_SEPARATE;
			if( MFVideo3DSampleFormat_Packed_LeftRight == m_vp3DOutput ) {
				vpStereoFormat = D3D11_VIDEO_PROCESSOR_STEREO_FORMAT_HORIZONTAL;
			}
			else if( MFVideo3DSampleFormat_Packed_TopBottom == m_vp3DOutput ) {
				vpStereoFormat = D3D11_VIDEO_PROCESSOR_STEREO_FORMAT_VERTICAL;
			}

			pVideoContext->VideoProcessorSetStreamStereoFormat( m_pVideoProcessor,
																0, m_bStereoEnabled, vpStereoFormat, TRUE, TRUE, D3D11_VIDEO_PROCESSOR_STEREO_FLIP_NONE, 0 );
		}
#endif

		QueryPerformanceCounter( &lpcEnd );

		QueryPerformanceCounter( &lpcStart );

		D3D11_VIDEO_PROCESSOR_STREAM StreamData;
		ZeroMemory( &StreamData, sizeof( StreamData ) );
		StreamData.Enable = TRUE;
		StreamData.OutputIndex = 0;
		StreamData.InputFrameOrField = 0;
		StreamData.PastFrames = 0;
		StreamData.FutureFrames = 0;
		StreamData.ppPastSurfaces = NULL;
		StreamData.ppFutureSurfaces = NULL;
		StreamData.pInputSurface = pLeftInputView;
		StreamData.ppPastSurfacesRight = NULL;
		StreamData.ppFutureSurfacesRight = NULL;

#if (WINVER >= _WIN32_WINNT_WIN8)
		if( m_b3DVideo && MFVideo3DSampleFormat_MultiView == m_vp3DOutput && pRightTexture2D ) {
			StreamData.pInputSurfaceRight = pRightInputView;
		}
#endif

		hr = pVideoContext->VideoProcessorBlt( m_pVideoProcessor, pOutputView, 0, 1, &StreamData );
		BREAK_ON_FAIL( hr );

		QueryPerformanceCounter( &lpcEnd );

		if( ppVideoOutFrame != NULL ) {
			*ppVideoOutFrame = pRTSample;
			( *ppVideoOutFrame )->AddRef();
		}
	} while( FALSE );

	SafeRelease( pBuffer );
	SafeRelease( pRTSample );
	SafeRelease( pDXGIBackBuffer );
	SafeRelease( pOutputView );
	SafeRelease( pLeftInputView );
	SafeRelease( pRightInputView );
	SafeRelease( pVideoContext );
//*/
	return hr;
}

} // namespace detail
} // namespace msw
} // namespace cinder