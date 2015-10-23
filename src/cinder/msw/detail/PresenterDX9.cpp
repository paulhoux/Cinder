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

#if MF_USE_DXVA2_DECODER
GUID const* const PresenterDX9::s_pVideoFormats[] =
{
	&MFVideoFormat_NV12,
	&MFVideoFormat_IYUV,
	&MFVideoFormat_YUY2,
	&MFVideoFormat_YV12,
	&MFVideoFormat_RGB32,
	&MFVideoFormat_ARGB32,
	&MFVideoFormat_RGB24,
	&MFVideoFormat_RGB555,
	&MFVideoFormat_RGB565,
	&MFVideoFormat_RGB8,
	&MFVideoFormat_AYUV,
	&MFVideoFormat_UYVY,
	&MFVideoFormat_YVYU,
	&MFVideoFormat_YVU9,
	//&MEDIASUBTYPE_V216,
	&MFVideoFormat_v410,
	&MFVideoFormat_I420,
	&MFVideoFormat_NV11,
	&MFVideoFormat_420O
};
#else
GUID const* const PresenterDX9::s_pVideoFormats[] =
{
	&MFVideoFormat_YUY2,
	&MFVideoFormat_ARGB32,
	&MFVideoFormat_RGB24,
	&MFVideoFormat_RGB32
};
#endif

const DWORD PresenterDX9::s_dwNumVideoFormats = sizeof( PresenterDX9::s_pVideoFormats ) / sizeof( PresenterDX9::s_pVideoFormats[0] );

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
	, m_pSwapChain( NULL )
	, m_pQueue( NULL )
	, m_bCanProcessNextSample( TRUE )
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
	else if( iid == __uuidof( PresenterDX9 ) ) {
		*ppv = static_cast<PresenterDX9*>( this );
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

HRESULT PresenterDX9::GetFrame( IDirect3DSurface9 **ppFrame )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	do {
		BREAK_ON_NULL( m_pQueue, E_FAIL );

		ScopedComPtr<IDirect3DSurface9> pSurface;
		hr = m_pQueue->RemoveFront( &pSurface );
		BREAK_ON_FAIL( hr );

		( *ppFrame ) = pSurface;
		( *ppFrame )->AddRef();
	} while( FALSE );

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

STDMETHODIMP PresenterDX9::Flush( void )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = CheckShutdown();

	m_bCanProcessNextSample = TRUE;

	return hr;
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

		ScopedComPtr<IDirect3DSurface9> pSurface;
		hr = m_pDecoderService->CreateSurface(
			desc.SampleWidth, desc.SampleHeight, 0, (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
			D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, &pSurface, NULL );
		BREAK_ON_FAIL( hr );

		IDirect3DSurface9* surfaces = pSurface.get();
		for( UINT i = 0; i < configCount; i++ ) {
			ScopedComPtr<IDirectXVideoDecoder> pDecoder;
			hr = m_pDecoderService->CreateVideoDecoder( m_DecoderGUID, &desc, &configs[i], &surfaces, 1, &pDecoder );
			if( SUCCEEDED( hr ) && pDecoder )
				break;
		}
#else
		break; // Allow for now.

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
		//MFVideoInterlaceMode interlaceMode;
		//hr = pMediaType->GetUINT32( MF_MT_INTERLACE_MODE, (UINT32*)&interlaceMode );
		//BREAK_ON_FAIL( hr );
		//BREAK_IF_FALSE( interlaceMode == MFVideoInterlace_Progressive, MF_E_INVALIDMEDIATYPE );
#endif
	} while( FALSE );

	if( FAILED( hr ) )
		hr = MF_E_INVALIDMEDIATYPE;

	CoTaskMemFree( configs );

	return hr;
}

STDMETHODIMP PresenterDX9::PresentFrame( void )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		BREAK_ON_NULL_MSG( m_pSwapChain, E_POINTER, "No swap chain available." );

		RECT rcDest;
		ZeroMemory( &rcDest, sizeof( rcDest ) );
		if( CheckEmptyRect( &rcDest ) ) {
			hr = S_OK;
			break;
		}

		//hr = m_pD3DDevice->Present( NULL, &rcDest, NULL, NULL );
		hr = m_pSwapChain->Present( NULL, &rcDest, /*m_hwndVideo*/ NULL, NULL, 0 );
		BREAK_ON_FAIL_MSG( hr, "Failed to present swap chain" );

		m_bCanProcessNextSample = TRUE;
	} while( FALSE );

	return hr;
}

// Presenter
HRESULT PresenterDX9::ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample )
{
	HRESULT hr = S_OK;

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

		ScopedComPtr<IMFMediaBuffer> pBuffer;
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

		if( *pbDeviceChanged ) {
			hr = CreateDXVA2ManagerAndDevice();
			BREAK_ON_FAIL( hr );
		}

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

		// Obtain surface from buffer.
		ScopedComPtr<IDirect3DSurface9> pSurface;
		hr = MFGetService( pBuffer, MR_BUFFER_SERVICE, __uuidof( IDirect3DSurface9 ), (void**)&pSurface );
		BREAK_ON_FAIL( hr );

		hr = ProcessFrameUsingD3D9( pSurface, 0, rcDest, *punInterlaceMode, ppOutputSample );

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

	} while( FALSE );

	return hr;
}

// Presenter
HRESULT PresenterDX9::SetCurrentMediaType( IMFMediaType *pMediaType )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IMFAttributes> pAttributes;
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

		//ScopedComPtr<IDirect3DSurface9> pSurface;
		//hr = m_pDecoderService->CreateSurface(
		//	desc.SampleWidth, desc.SampleHeight, 0, (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
		//	D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget, &pSurface, NULL );
		//BREAK_ON_FAIL( hr );

		//ScopedComPtr<IMFSample> pSample;
		//hr = MFCreateVideoSampleFromSurface( pSurface, &pSample );
		//BREAK_ON_FAIL( hr );
#endif
	} while( FALSE );

	return hr;
}

// Presenter
HRESULT PresenterDX9::Shutdown( void )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = MF_E_SHUTDOWN;

	m_IsShutdown = TRUE;

	SafeRelease( m_pQueue );
	SafeRelease( m_pSwapChain );
	SafeRelease( m_pDecoderService );
	SafeRelease( m_pSampleAllocator );
	SafeRelease( m_pDeviceManager );
	SafeRelease( m_pD3DDevice );
	SafeRelease( m_pD3D9 );

	return hr;
}

// Presenter
HRESULT PresenterDX9::GetMediaTypeByIndex( DWORD dwIndex, GUID *subType ) const
{
	if( dwIndex >= s_dwNumVideoFormats )
		return MF_E_NO_MORE_TYPES;

	*subType = *s_pVideoFormats[dwIndex];

	return S_OK;
}

/// Private methods

HRESULT PresenterDX9::CheckDeviceState( BOOL * pbDeviceChanged )
{
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
				}
			}
		}
#endif

		hr = SetVideoMonitor( m_hwndVideo );
		BREAK_ON_FAIL( hr );

		if( S_FALSE == hr && m_pD3DDevice != NULL ) {
			// Lost/hung device. Destroy the device and create a new one.
			*pbDeviceChanged = TRUE;
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

		// Ensure we can do the YCbCr->RGB conversion in StretchRect.
		hr = m_pD3D9->CheckDeviceFormatConversion( D3DADAPTER_DEFAULT,
												   D3DDEVTYPE_HAL,
												   (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
												   D3DFMT_X8R8G8B8 );
		BREAK_ON_FAIL_MSG( hr, "Conversion from YUV to RGB is not supported." );

		// Close existing interfaces.
		SafeRelease( m_pDecoderService );
		SafeRelease( m_pDeviceManager );
		SafeRelease( m_pQueue );

		m_DecoderGUID = GUID_NULL;

		// Create the device manager and initialize it.
		hr = DXVA2CreateDirect3DDeviceManager9( &m_DeviceResetToken, &m_pDeviceManager );
		BREAK_ON_FAIL( hr );

		// Get the device caps for this adapter.
		D3DCAPS9 ddCaps;
		ZeroMemory( &ddCaps, sizeof( ddCaps ) );

		hr = m_pD3D9->GetDeviceCaps( m_ConnectionGUID, D3DDEVTYPE_HAL, &ddCaps );
		BREAK_ON_FAIL_MSG( hr, "No hardware renderer available." );

		// Check if device supports hardware processing.
		BREAK_IF_FALSE( ddCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT, E_FAIL );

		// Note: The presenter creates additional swap chains to present the
		// video frames. Therefore, it does not use the device's implicit 
		// swap chain, so the size of the back buffer here is 1 x 1.

		D3DPRESENT_PARAMETERS pPresentParams;
		ZeroMemory( &pPresentParams, sizeof( pPresentParams ) );

		pPresentParams.BackBufferWidth = 1;
		pPresentParams.BackBufferHeight = 1;
		pPresentParams.BackBufferCount = 1;
		pPresentParams.Windowed = TRUE;
		pPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
		pPresentParams.BackBufferFormat = D3DFMT_UNKNOWN;
		pPresentParams.hDeviceWindow = NULL; //  m_hwndVideo;
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

		// Create (additional) swap chain.
		hr = UpdateDX9SwapChain();
		BREAK_ON_FAIL( hr );

		// Create shared surface queue.
		m_pQueue = new Queue<IDirect3DSurface9>(); // Created with ref count = 1.

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

	do {
		UINT32 uimageWidthInPixels = 0, uimageHeightInPixels = 0;
		hr = MFGetAttributeSize( pType, MF_MT_FRAME_SIZE, &uimageWidthInPixels, &uimageHeightInPixels );
		BREAK_ON_FAIL( hr );

		if( uimageWidthInPixels != m_imageWidthInPixels || uimageHeightInPixels != m_imageHeightInPixels ) {
			//SafeRelease( m_pVideoProcessorEnum );
			//SafeRelease( m_pVideoProcessor );
			//SafeRelease( m_pSwapChain1 );
		}

		m_imageWidthInPixels = uimageWidthInPixels;
		m_imageHeightInPixels = uimageHeightInPixels;

		BOOL bPanScan = FALSE;
		bPanScan = MFGetAttributeUINT32( pType, MF_MT_PAN_SCAN_ENABLED, FALSE );

		// In pan/scan mode, try to get the pan/scan region.
		if( bPanScan ) {
			hr = pType->GetBlob( MF_MT_PAN_SCAN_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );
		}

		// If not in pan/scan mode, or the pan/scan region is not set,
		// get the minimimum display aperture.
		if( !bPanScan || hr == MF_E_ATTRIBUTENOTFOUND ) {
			hr = pType->GetBlob( MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );

			if( hr == MF_E_ATTRIBUTENOTFOUND ) {
				// Minimum display aperture is not set.

				// For backward compatibility with some components,
				// check for a geometric aperture.

				hr = pType->GetBlob( MF_MT_GEOMETRIC_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );
			}

			// Default: Use the entire video area.
			if( hr == MF_E_ATTRIBUTENOTFOUND ) {
				*pArea = MFMakeArea( 0.0, 0.0, m_imageWidthInPixels, m_imageHeightInPixels );
				hr = S_OK;
			}
		}
	} while( FALSE );

	return hr;
}

HRESULT PresenterDX9::ProcessFrameUsingD3D9( IDirect3DSurface9* pSurface, UINT dwViewIndex, RECT rcDest, UINT32 unInterlaceMode, IMFSample** ppVideoOutFrame )
{
	HRESULT hr = S_OK;

	LARGE_INTEGER lpcStart, lpcEnd;

	do {
		// Remember the original rectangles.
		RECT TRectOld = m_rcDstApp;
		RECT SRectOld = m_rcSrcApp;
		UpdateRectangles( &TRectOld, &SRectOld );

		// Update destination rect with current client rect.
		m_rcDstApp = rcDest;

		m_rcSrcApp.left = 0;
		m_rcSrcApp.top = 0;
		m_rcSrcApp.right = m_uiRealDisplayWidth;
		m_rcSrcApp.bottom = m_uiRealDisplayHeight;

		// now create the input and output media types - these need to reflect
		// the src and destination rectangles that we have been given.
		RECT TRect = m_rcDstApp;
		RECT SRect = m_rcSrcApp;
		UpdateRectangles( &TRect, &SRect );

		const BOOL fDestRectChanged = !EqualRect( &TRect, &TRectOld );

		if( !m_pSwapChain || fDestRectChanged ) {
			hr = UpdateDX9SwapChain();
			BREAK_ON_FAIL( hr );
		}

		m_bCanProcessNextSample = FALSE;

		QueryPerformanceCounter( &lpcStart );

		// Get Backbuffer.
		ScopedComPtr<IDirect3DSurface9> pBackBuffer;
		//m_pD3DDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
		m_pSwapChain->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
		BREAK_ON_FAIL( hr );

		// We're required to create an output sample for the Scheduler. See: Scheduler::ProcessSamplesFromQueue().
		ScopedComPtr<IMFMediaBuffer> pBuffer;
		hr = MFCreateDXSurfaceBuffer( __uuidof( IDirect3DSurface9 ), pBackBuffer, FALSE, &pBuffer );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IMFSample> pRTSample;
		hr = MFCreateSample( &pRTSample );
		BREAK_ON_FAIL( hr );

		hr = pRTSample->AddBuffer( pBuffer );
		BREAK_ON_FAIL( hr );

		if( ppVideoOutFrame != NULL ) {
			*ppVideoOutFrame = pRTSample;
			( *ppVideoOutFrame )->AddRef();
		}

		// Fill backbuffer with black.
		hr = m_pD3DDevice->ColorFill( pBackBuffer, NULL, D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00 ) );

		// Draw the surface to the backbuffer, decompressing YUV to RGB if needed.
		hr = m_pD3DDevice->StretchRect( pSurface, NULL, pBackBuffer, &TRect, D3DTEXF_NONE );
		BREAK_ON_FAIL_MSG( hr, "Failed to render to back buffer." );

		QueryPerformanceCounter( &lpcEnd );

		// Blit video to shared texture.
		QueryPerformanceCounter( &lpcStart );
#if 1
		do {
			BREAK_ON_NULL( m_pQueue, E_POINTER );

			D3DSURFACE_DESC surfaceDesc;
			hr = pSurface->GetDesc( &surfaceDesc );
			BREAK_ON_FAIL( hr );

			ScopedComPtr<IDirect3DSurface9> pFrame;
			hr = m_pQueue->RemoveBack( &pFrame );

			// Check texture compatibility.
			if( SUCCEEDED( hr ) ) {
				D3DSURFACE_DESC desc;
				hr = pSurface->GetDesc( &desc );

				if( FAILED( hr ) || desc.Width != surfaceDesc.Width || desc.Height != surfaceDesc.Height ) {
					// Do not return it to the pool.
					pFrame.Release();
					hr = E_FAIL;
				}
			}
			
			// If no existing texture is available, create new one.
			if( FAILED( hr ) ) {
				D3DSURFACE_DESC desc = surfaceDesc;
				desc.Format = D3DFMT_A8R8G8B8;
				desc.Usage = D3DUSAGE_RENDERTARGET;

				HANDLE sharedHandle = NULL;
				hr = m_pD3DDevice->CreateRenderTarget( desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE, &pFrame, &sharedHandle );
				BREAK_ON_FAIL( hr );

				// Set private data.
				//hr = pFrame->SetPrivateData( GUID_SharedHandle, sharedHandle, sizeof( HANDLE ), 0 );
			}
			BREAK_ON_FAIL( hr );

			// TODO: blit to texture.

			// Add to front of queue.
			hr = m_pQueue->InsertFront( pFrame );
		} while( FALSE );
		// TEMP: end
#endif
		QueryPerformanceCounter( &lpcEnd );
	} while( FALSE );

	return hr;
}

//+-------------------------------------------------------------------------
//
//  Member:     UpdateDX9SwapChain
//
//  Synopsis:   Creates SwapChain for HWND or Resizes buffers in case of resolution change
//
//--------------------------------------------------------------------------

_Post_satisfies_( this->m_pSwapChain != NULL )
HRESULT PresenterDX9::UpdateDX9SwapChain( void )
{
	HRESULT hr = S_OK;

	D3DPRESENT_PARAMETERS pPresentParams;
	ZeroMemory( &pPresentParams, sizeof( pPresentParams ) );

	pPresentParams.BackBufferWidth = 0;
	pPresentParams.BackBufferHeight = 0;
	pPresentParams.BackBufferCount = 1;
	pPresentParams.Windowed = !m_bFullScreenState;
	pPresentParams.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pPresentParams.BackBufferFormat = D3DFMT_UNKNOWN;
	pPresentParams.hDeviceWindow = m_hwndVideo;
	pPresentParams.Flags = D3DPRESENTFLAG_VIDEO;
	pPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

	do {
		SafeRelease( m_pSwapChain );

		hr = m_pD3DDevice->CreateAdditionalSwapChain( &pPresentParams, &m_pSwapChain );
		BREAK_ON_FAIL( hr );
	} while( FALSE );

	return hr;
}

	} // namespace detail
} // namespace msw
} // namespace cinder