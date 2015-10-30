
#include "cinder/Cinder.h"
#if ! defined( CINDER_WINRT ) && ( _WIN32_WINNT < _WIN32_WINNT_VISTA )
#error "Media Foundation only available on Windows Vista or newer"
#else

#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/detail/PresenterDX11.h"

namespace cinder {
namespace msw {
namespace detail {

const PresenterDX11::FormatEntry PresenterDX11::s_DXGIFormatMapping[] =
{
	{ MFVideoFormat_RGB32,      DXGI_FORMAT_B8G8R8X8_UNORM },
	{ MFVideoFormat_ARGB32,     DXGI_FORMAT_R8G8B8A8_UNORM },
	{ MFVideoFormat_AYUV,      DXGI_FORMAT_AYUV },
	{ MFVideoFormat_YUY2,      DXGI_FORMAT_YUY2 },
	{ MFVideoFormat_NV12,      DXGI_FORMAT_NV12 },
	{ MFVideoFormat_NV11,      DXGI_FORMAT_NV11 },
	{ MFVideoFormat_AI44,      DXGI_FORMAT_AI44 },
	{ MFVideoFormat_P010,      DXGI_FORMAT_P010 },
	{ MFVideoFormat_P016,      DXGI_FORMAT_P016 },
	{ MFVideoFormat_Y210,      DXGI_FORMAT_Y210 },
	{ MFVideoFormat_Y216,      DXGI_FORMAT_Y216 },
	{ MFVideoFormat_Y410,      DXGI_FORMAT_Y410 },
	{ MFVideoFormat_Y416,      DXGI_FORMAT_Y416 },
	{ MFVideoFormat_420O,      DXGI_FORMAT_420_OPAQUE }
};

GUID const* const PresenterDX11::s_pVideoFormats[] =
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

const DWORD PresenterDX11::s_dwNumVideoFormats = sizeof( PresenterDX11::s_pVideoFormats ) / sizeof( PresenterDX11::s_pVideoFormats[0] );


/////////////////////////////////////////////////////////////////////////////////////////////
//
// PresenterDX11 class. - Presents samples using DX11.
//
// Notes:
// - Most public methods calls CheckShutdown. This method fails if the presenter was shut down.
//
/////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------
// PresenterDX11 constructor.
//-------------------------------------------------------------------

PresenterDX11::PresenterDX11( void )
	: m_pDXGIFactory2( NULL )
	, m_pDXGIManager( NULL )
	, m_pDXGIOutput1( NULL )
	, m_pSampleAllocatorEx( NULL )
#if ( WINVER >= _WIN32_WINNT_WIN8 )
	, m_pDCompDevice( NULL )
	, m_pHwndTarget( NULL )
	, m_pRootVisual( NULL )
#endif
	, m_bSoftwareDXVADeviceInUse( FALSE )
	, m_DeviceResetToken( 0 )
	, m_DXSWSwitch( 0 )
	, m_useDCompVisual( 0 )
	, m_useDebugLayer( D3D11_CREATE_DEVICE_VIDEO_SUPPORT /*| D3D11_CREATE_DEVICE_DEBUG*/ )
	, m_pD3DDevice( NULL )
	, m_pVideoDevice( NULL )
	, m_pD3DImmediateContext( NULL )
	, m_pVideoProcessorEnum( NULL )
	, m_pVideoProcessor( NULL )
	, m_pPool( NULL )
	, m_pReady( NULL )
	, m_pSwapChain1( NULL )
	, m_bDeviceChanged( FALSE )
	, m_bResize( TRUE )
	, m_b3DVideo( FALSE )
	, m_bStereoEnabled( FALSE )
#if (WINVER >= _WIN32_WINNT_WIN8)
	, m_vp3DOutput( MFVideo3DSampleFormat_BaseView )
#endif
	, m_bFullScreenState( FALSE )
	, m_bCanProcessNextSample( TRUE )
	, m_dxgiFormat( DXGI_FORMAT_UNKNOWN )
#if (WINVER >= _WIN32_WINNT_WIN8) 
	, m_pXVP( NULL )
	, m_pXVPControl( NULL )
	, m_useXVP( TRUE )
#else
	, m_useXVP( FALSE )
#endif
	, m_D3D11Module( NULL )
	, _D3D11CreateDevice( NULL )
{
	ZeroMemory( &m_rcSrcApp, sizeof( m_rcSrcApp ) );
	ZeroMemory( &m_rcDstApp, sizeof( m_rcDstApp ) );
}

//-------------------------------------------------------------------
// PresenterDX11 destructor.
//-------------------------------------------------------------------

PresenterDX11::~PresenterDX11( void )
{
	// Unload D3D11.
	if( m_D3D11Module ) {
		FreeLibrary( m_D3D11Module );
		m_D3D11Module = NULL;
	}
}

// IUnknown
HRESULT PresenterDX11::QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv )
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
	else if( iid == __uuidof( PresenterDX11 ) ) {
		*ppv = static_cast<PresenterDX11*>( this );
	}
	else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

// IMFVideoDisplayControl
HRESULT PresenterDX11::GetFullscreen( __RPC__out BOOL* pfFullscreen )
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
HRESULT PresenterDX11::SetFullscreen( BOOL fFullscreen )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		m_bFullScreenState = fFullscreen;

		SafeRelease( m_pVideoDevice );
		SafeRelease( m_pVideoProcessorEnum );
		SafeRelease( m_pVideoProcessor );
	}

	return hr;
}

// IMFVideoDisplayControl
HRESULT PresenterDX11::SetVideoWindow( __RPC__in HWND hwndVideo )
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

		CheckDecodeSwitchRegKey();

		m_hwndVideo = hwndVideo;

		hr = CreateDXGIManagerAndDevice();
		BREAK_ON_FAIL( hr );

#if (WINVER >= _WIN32_WINNT_WIN8)
		if( m_useXVP ) {
			hr = CreateXVP();
			BREAK_ON_FAIL( hr );
		}
#endif
	} while( FALSE );

	return hr;
}

// IMFGetService
HRESULT PresenterDX11::GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject )
{
	HRESULT hr = S_OK;

	if( guidService == MR_VIDEO_ACCELERATION_SERVICE ) {
		if( riid == __uuidof( IMFDXGIDeviceManager ) ) {
			if( NULL != m_pDXGIManager ) {
				*ppvObject = ( void* ) static_cast<IUnknown*>( m_pDXGIManager );
				( (IUnknown*)*ppvObject )->AddRef();
			}
			else {
				hr = E_NOINTERFACE;
			}
		}
		else if( riid == __uuidof( IMFVideoSampleAllocatorEx ) ) {
			if( NULL == m_pSampleAllocatorEx ) {
				hr = MFCreateVideoSampleAllocatorEx( IID_IMFVideoSampleAllocatorEx, (LPVOID*)&m_pSampleAllocatorEx );
				if( SUCCEEDED( hr ) && NULL != m_pDXGIManager ) {
					hr = m_pSampleAllocatorEx->SetDirectXManager( m_pDXGIManager );
				}
			}
			if( SUCCEEDED( hr ) ) {
				hr = m_pSampleAllocatorEx->QueryInterface( riid, ppvObject );
			}
		}
		else {
			hr = E_NOINTERFACE;
		}
	}
	else if( guidService == MR_VIDEO_RENDER_SERVICE ) {
		hr = QueryInterface( riid, ppvObject );
	}
	else {
		hr = MF_E_UNSUPPORTED_SERVICE;
	}

	return hr;
}

HRESULT PresenterDX11::GetFrame( ID3D11Texture2D **ppFrame )
{
	HRESULT hr = S_OK;

	// Note: we don't need a critical section, as the
	//       queue is already thread-safe. This way,
	//       we don't have to block more than necessary.

	do {
		BREAK_ON_NULL( m_pReady, E_FAIL );

		ScopedComPtr<ID3D11Texture2D> pSurface;
		hr = m_pReady->RemoveFront( &pSurface );
		BREAK_ON_FAIL( hr );

		( *ppFrame ) = pSurface;
		( *ppFrame )->AddRef();
	} while( FALSE );

	return hr;
}

HRESULT PresenterDX11::ReturnFrame( ID3D11Texture2D **ppFrame )
{
	HRESULT hr = S_OK;

	// Note: we don't need a critical section, as the
	//       queue is already thread-safe. This way,
	//       we don't have to block more than necessary.

	do {
		BREAK_ON_NULL( m_pPool, E_FAIL );

		hr = m_pPool->InsertBack( *ppFrame );
		BREAK_ON_FAIL( hr );

		SafeRelease( *ppFrame );
	} while( FALSE );

	return hr;
}

// Presenter
HRESULT PresenterDX11::Initialize( void )
{
	// TEMP: Force DX9.
	//return E_FAIL;

	if( !m_D3D11Module ) {
		// Dynamically load D3D11 functions (to avoid static linkage with d3d11.lib)
		m_D3D11Module = LoadLibrary( TEXT( "d3d11.dll" ) );

		if( !m_D3D11Module )
			return E_FAIL;

		_D3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>( GetProcAddress( m_D3D11Module, "D3D11CreateDevice" ) );
		if( !_D3D11CreateDevice )
			return E_FAIL;
	}

	return S_OK;
}

// Presenter
HRESULT PresenterDX11::Flush( void )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = CheckShutdown();

#if (WINVER >= _WIN32_WINNT_WIN8)
	if( SUCCEEDED( hr ) && m_useXVP ) {
		hr = m_pXVP->ProcessMessage( MFT_MESSAGE_COMMAND_FLUSH, 0 );
	}
#endif

	m_bCanProcessNextSample = TRUE;

	return hr;
}

// Presenter
HRESULT PresenterDX11::GetMonitorRefreshRate( DWORD* pdwRefreshRate )
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
HRESULT PresenterDX11::IsMediaTypeSupported( IMFMediaType* pMediaType )
{
	HRESULT hr = S_OK;

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		BREAK_ON_NULL( pMediaType, E_POINTER );

		if( !m_pVideoDevice ) {
			hr = m_pD3DDevice->QueryInterface( __uuidof( ID3D11VideoDevice ), (void**)&m_pVideoDevice );
			BREAK_ON_FAIL( hr );
		}

		UINT32 uimageWidthInPixels, uimageHeightInPixels = 0;
		hr = MFGetAttributeSize( pMediaType, MF_MT_FRAME_SIZE, &uimageWidthInPixels, &uimageHeightInPixels );
		BREAK_ON_FAIL( hr );

		UINT32 uiNumerator = 30000, uiDenominator = 1001;
		MFGetAttributeRatio( pMediaType, MF_MT_FRAME_RATE, &uiNumerator, &uiDenominator );

		// Check if the format is supported
		D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
		ZeroMemory( &ContentDesc, sizeof( ContentDesc ) );
		ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
		ContentDesc.InputWidth = (DWORD)uimageWidthInPixels;
		ContentDesc.InputHeight = (DWORD)uimageHeightInPixels;
		ContentDesc.OutputWidth = (DWORD)uimageWidthInPixels;
		ContentDesc.OutputHeight = (DWORD)uimageHeightInPixels;
		ContentDesc.InputFrameRate.Numerator = uiNumerator;
		ContentDesc.InputFrameRate.Denominator = uiDenominator;
		ContentDesc.OutputFrameRate.Numerator = uiNumerator;
		ContentDesc.OutputFrameRate.Denominator = uiDenominator;
		ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

		SafeRelease( m_pVideoProcessorEnum );
		hr = m_pVideoDevice->CreateVideoProcessorEnumerator( &ContentDesc, &m_pVideoProcessorEnum );
		BREAK_ON_FAIL( hr );

		GUID subType = GUID_NULL;
		hr = pMediaType->GetGUID( MF_MT_SUBTYPE, &subType );
		BREAK_ON_FAIL( hr );

		DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN;
		for( DWORD i = 0; i < ARRAYSIZE( s_DXGIFormatMapping ); i++ ) {
			const FormatEntry& e = s_DXGIFormatMapping[i];
			if( e.Subtype == subType ) {
				dxgiFormat = e.DXGIFormat;
				break;
			}
		}

		UINT uiFlags;
		hr = m_pVideoProcessorEnum->CheckVideoProcessorFormat( dxgiFormat, &uiFlags );
		if( FAILED( hr ) || 0 == ( uiFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_INPUT ) ) {
			hr = MF_E_INVALIDMEDIATYPE;
			break;
		}

		m_dxgiFormat = dxgiFormat;

#if (WINVER >= _WIN32_WINNT_WIN8)
		if( m_useXVP ) {
			hr = m_pXVP->SetInputType( 0, pMediaType, MFT_SET_TYPE_TEST_ONLY );
			BREAK_ON_FAIL( hr );
		}
#endif
	} while( FALSE );

	return hr;
}

//+-------------------------------------------------------------------------
//
//  Member:     PresentFrame
//
//  Synopsis:   Present the current outstanding frame in the DX queue
//
//--------------------------------------------------------------------------

// Presenter
HRESULT PresenterDX11::PresentFrame( void )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		BREAK_ON_NULL( m_pSwapChain1, E_POINTER );

		RECT rcDest;
		ZeroMemory( &rcDest, sizeof( rcDest ) );
		if( CheckEmptyRect( &rcDest ) ) {
			hr = S_OK;
			break;
		}

		hr = m_pSwapChain1->Present( 0, 0 );
		BREAK_ON_FAIL( hr );

		m_bCanProcessNextSample = TRUE;
	} while( FALSE );

	return hr;
}

//-------------------------------------------------------------------
// Name: ProcessFrame
// Description: Present one media sample.
//-------------------------------------------------------------------

// Presenter
HRESULT PresenterDX11::ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample )
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
		ScopedComPtr<IMFMediaBuffer> pEVBuffer;
		if( 1 == cBuffers ) {
			hr = pSample->GetBufferByIndex( 0, &pBuffer );
		}
		else if( 2 == cBuffers && m_b3DVideo && FALSE /* 0 != m_vp3DOutput */ ) {
			hr = pSample->GetBufferByIndex( 0, &pBuffer );
			BREAK_ON_FAIL( hr );

			hr = pSample->GetBufferByIndex( 1, &pEVBuffer );
		}
		else {
			hr = pSample->ConvertToContiguousBuffer( &pBuffer );
		}
		BREAK_ON_FAIL( hr );

		// Check if device is still valid.
		hr = CheckDeviceState( pbDeviceChanged );
		BREAK_ON_FAIL( hr );

		if( *pbDeviceChanged ) {
			hr = CreateDXGIManagerAndDevice();
			BREAK_ON_FAIL( hr );
		}

		// Adjust output size.
		RECT rcDest;
		ZeroMemory( &rcDest, sizeof( rcDest ) );
		if( CheckEmptyRect( &rcDest ) ) {
			hr = S_OK;
			break;
		}

		// Check the per-sample attributes.
		MFVideoInterlaceMode unInterlaceMode = (MFVideoInterlaceMode)MFGetAttributeUINT32( pCurrentType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive );

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

		ScopedComPtr<IMFDXGIBuffer> pDXGIBuffer;
		ScopedComPtr<IMFDXGIBuffer> pEVDXGIBuffer;
		hr = pBuffer.QueryInterface( __uuidof( IMFDXGIBuffer ), (LPVOID*)&pDXGIBuffer );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<ID3D11Texture2D> pTexture2D;
		ScopedComPtr<ID3D11Texture2D> pEVTexture2D;
		hr = pDXGIBuffer->GetResource( __uuidof( ID3D11Texture2D ), (LPVOID*)&pTexture2D );
		BREAK_ON_FAIL( hr );

		UINT dwViewIndex = 0;
		UINT dwEVViewIndex = 0;
		hr = pDXGIBuffer->GetSubresourceIndex( &dwViewIndex );
		BREAK_ON_FAIL( hr );

#if (WINVER >= _WIN32_WINNT_WIN8)
		if( m_b3DVideo && 0 != m_vp3DOutput ) {
			if( pEVBuffer && MFVideo3DSampleFormat_MultiView == m_vp3DOutput ) {
				hr = pEVBuffer.QueryInterface( __uuidof( IMFDXGIBuffer ), (LPVOID*)&pEVDXGIBuffer );
				BREAK_ON_FAIL( hr );

				hr = pEVDXGIBuffer->GetResource( __uuidof( ID3D11Texture2D ), (LPVOID*)&pEVTexture2D );
				BREAK_ON_FAIL( hr );

				hr = pEVDXGIBuffer->GetSubresourceIndex( &dwEVViewIndex );
				BREAK_ON_FAIL( hr );
			}
		}
#endif

		ScopedComPtr<ID3D11Device> pDeviceInput;
		pTexture2D->GetDevice( &pDeviceInput );
		if( ( NULL == pDeviceInput ) || ( pDeviceInput != m_pD3DDevice ) ) {
			break;
		}

#if (WINVER >= _WIN32_WINNT_WIN8)
		if( m_useXVP ) {
			BOOL bInputFrameUsed = FALSE;

			hr = ProcessFrameUsingXVP( pCurrentType, pSample, pTexture2D, rcDest, ppOutputSample, &bInputFrameUsed );

			if( SUCCEEDED( hr ) && !bInputFrameUsed ) {
				*pbProcessAgain = TRUE;
			}
		}
		else
#endif
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
	} while( FALSE );

	return hr;
}

// Presenter
HRESULT PresenterDX11::SetCurrentMediaType( IMFMediaType* pMediaType )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IMFAttributes> pAttributes;
		hr = pMediaType->QueryInterface( IID_IMFAttributes, reinterpret_cast<void**>( &pAttributes ) );
		BREAK_ON_FAIL( hr );

		HRESULT hr1 = pAttributes->GetUINT32( MF_MT_VIDEO_3D, (UINT32*)&m_b3DVideo );
		if( SUCCEEDED( hr1 ) ) {
#if (WINVER >=_WIN32_WINNT_WIN8)
			hr = pAttributes->GetUINT32( MF_MT_VIDEO_3D_FORMAT, (UINT32*)&m_vp3DOutput );
			BREAK_ON_FAIL( hr );
#else
			// This is a 3D video and we only support it on Windows 8+.
			BREAK_IF_TRUE( m_b3DVideo, E_NOTIMPL );
#endif
		}

		//Now Determine Correct Display Resolution
		if( SUCCEEDED( hr ) ) {
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
		}

#if (WINVER >=_WIN32_WINNT_WIN8)
		if( SUCCEEDED( hr ) && m_useXVP ) {
			// set the input type on the XVP
			hr = m_pXVP->SetInputType( 0, pMediaType, 0 );
			BREAK_ON_FAIL( hr );
		}
#endif
	} while( FALSE );

	return hr;
}

//-------------------------------------------------------------------
// Name: Shutdown
// Description: Releases resources held by the presenter.
//-------------------------------------------------------------------

// Presenter
HRESULT PresenterDX11::Shutdown( void )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = MF_E_SHUTDOWN;

	m_IsShutdown = TRUE;

	SafeRelease( m_pPool );
	SafeRelease( m_pReady );
	SafeRelease( m_pDXGIManager );
	SafeRelease( m_pDXGIFactory2 );
	SafeRelease( m_pD3DDevice );
	SafeRelease( m_pD3DImmediateContext );
	SafeRelease( m_pDXGIOutput1 );
	SafeRelease( m_pSampleAllocatorEx );
#if (WINVER >= _WIN32_WINNT_WIN8) 
	SafeRelease( m_pDCompDevice );
	SafeRelease( m_pHwndTarget );
	SafeRelease( m_pRootVisual );
	SafeRelease( m_pXVPControl );
	SafeRelease( m_pXVP );
#endif
	SafeRelease( m_pVideoDevice );
	SafeRelease( m_pVideoProcessor );
	SafeRelease( m_pVideoProcessorEnum );
	SafeRelease( m_pSwapChain1 );

	return hr;
}

// Presenter
HRESULT PresenterDX11::GetMediaTypeByIndex( DWORD dwIndex, GUID *subType ) const
{
	if( dwIndex >= s_dwNumVideoFormats )
		return MF_E_NO_MORE_TYPES;

	*subType = *s_pVideoFormats[dwIndex];

	return S_OK;
}

/// Private methods

void PresenterDX11::CheckDecodeSwitchRegKey( void )
{
	const TCHAR* lpcszDXSW = TEXT( "DXSWSwitch" );
	const TCHAR* lpcszInVP = TEXT( "XVP" );
	const TCHAR* lpcszDComp = TEXT( "DComp" );
	const TCHAR* lpcszDebugLayer = TEXT( "Dbglayer" );
	const TCHAR* lpcszREGKEY = TEXT( "SOFTWARE\\Microsoft\\Scrunch\\CodecPack\\MSDVD" );
	HKEY hk = NULL;
	DWORD dwData;
	DWORD cbData = sizeof( DWORD );
	DWORD cbType;

	if( 0 == RegOpenKeyEx( HKEY_CURRENT_USER, lpcszREGKEY, 0, KEY_READ, &hk ) ) {
		if( 0 == RegQueryValueEx( hk, lpcszDXSW, 0, &cbType, (LPBYTE)&dwData, &cbData ) ) {
			m_DXSWSwitch = dwData;
		}

		dwData = 0;
		cbData = sizeof( DWORD );
		if( 0 == RegQueryValueEx( hk, lpcszInVP, 0, &cbType, (LPBYTE)&dwData, &cbData ) ) {
			m_useXVP = dwData;
		}
#if 0 // Disabled support for IDComposition for now
		dwData = 0;
		cbData = sizeof( DWORD );
		if( 0 == RegQueryValueEx( hk, lpcszDComp, 0, &cbType, (LPBYTE)&dwData, &cbData ) ) {
			m_useDCompVisual = dwData;
		}
#endif
		dwData = 0;
		cbData = sizeof( DWORD );
		if( 0 == RegQueryValueEx( hk, lpcszDebugLayer, 0, &cbType, (LPBYTE)&dwData, &cbData ) ) {
			m_useDebugLayer = dwData;
		}
	}

	if( NULL != hk ) {
		RegCloseKey( hk );
	}

	return;
}

HRESULT PresenterDX11::CheckDeviceState( BOOL* pbDeviceChanged )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( pbDeviceChanged, E_POINTER );

		hr = SetVideoMonitor( m_hwndVideo );
		BREAK_ON_FAIL( hr );

		if( S_FALSE == hr && m_pD3DDevice != NULL ) {
			// Lost/hung device. Destroy the device and create a new one.
			*pbDeviceChanged = TRUE;
		}
	} while( FALSE );

	return hr;
}

HRESULT PresenterDX11::CreateDCompDeviceAndVisual( void )
{
	HRESULT hr = S_OK;
	IDXGIDevice* pDXGIDevice = NULL;

	do {
		hr = m_pD3DDevice->QueryInterface( __uuidof( IDXGIDevice ), reinterpret_cast<void**>( &pDXGIDevice ) );
		BREAK_ON_FAIL( hr );

		hr = E_NOTIMPL; // DCompositionCreateDevice(pDXGIDevice, __uuidof(IDCompositionDevice), reinterpret_cast<void**>(&m_pDCompDevice));
		BREAK_ON_FAIL( hr );

		hr = E_NOTIMPL; // m_pDCompDevice->CreateTargetForHwnd(m_hwndVideo, TRUE, &m_pHwndTarget);
		BREAK_ON_FAIL( hr );

		hr = E_NOTIMPL; // m_pDCompDevice->CreateVisual(reinterpret_cast<IDCompositionVisual**>(&m_pRootVisual));
		BREAK_ON_FAIL( hr );

		hr = E_NOTIMPL; // m_pHwndTarget->SetRoot(m_pRootVisual);
		BREAK_ON_FAIL( hr );
	} while( FALSE );

	SafeRelease( pDXGIDevice );

	return hr;
}

//-------------------------------------------------------------------
// Name: CreateDXGIManagerAndDevice
// Description: Creates D3D11 device and manager.
//
// Note: This method is called once when SetVideoWindow is called using
//       IDX11VideoRenderer.
//-------------------------------------------------------------------

HRESULT PresenterDX11::CreateDXGIManagerAndDevice()
{
	HRESULT hr = S_OK;

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0, D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
	D3D_FEATURE_LEVEL featureLevel;

	do {
		SafeRelease( m_pD3DDevice );
		SafeRelease( m_pVideoDevice );
		SafeRelease( m_pVideoProcessorEnum );
		SafeRelease( m_pVideoProcessor );
		SafeRelease( m_pSwapChain1 );
		SafeRelease( m_pD3DImmediateContext );

		// Find and create D3DDevice with highest compatible feature level.
		for( DWORD dwCount = 0; dwCount < ARRAYSIZE( featureLevels ); dwCount++ ) {
			hr = (_D3D11CreateDevice)( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, m_useDebugLayer, &featureLevels[dwCount], 1, D3D11_SDK_VERSION, &m_pD3DDevice, &featureLevel, NULL );
			if( SUCCEEDED( hr ) ) {
				// Check if it supports a D3D11VideoDevice.
				ScopedComPtr<ID3D11VideoDevice> pDX11VideoDevice;
				hr = m_pD3DDevice->QueryInterface( __uuidof( ID3D11VideoDevice ), (void**)&pDX11VideoDevice );

				if( SUCCEEDED( hr ) ) {
					break;
				}

				// If not, try next device.
				SafeRelease( m_pD3DDevice );
			}
		}
		BREAK_ON_FAIL( hr );

		if( NULL == m_pDXGIManager ) {
			UINT resetToken;
			hr = MFCreateDXGIDeviceManager( &resetToken, &m_pDXGIManager );
			BREAK_ON_FAIL( hr );
			m_DeviceResetToken = resetToken;
		}

		hr = m_pDXGIManager->ResetDevice( m_pD3DDevice, m_DeviceResetToken );
		BREAK_ON_FAIL( hr );

		m_pD3DDevice->GetImmediateContext( &m_pD3DImmediateContext );

		// Need to explitly set the multithreaded mode for this device
		ScopedComPtr<ID3D10Multithread> pMultiThread;
		hr = m_pD3DImmediateContext->QueryInterface( __uuidof( ID3D10Multithread ), (void**)&pMultiThread );
		BREAK_ON_FAIL( hr );

		pMultiThread->SetMultithreadProtected( TRUE );

		ScopedComPtr<IDXGIDevice1> pDXGIDev;
		hr = m_pD3DDevice->QueryInterface( __uuidof( IDXGIDevice1 ), (LPVOID*)&pDXGIDev );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IDXGIAdapter> pTempAdapter;
		hr = pDXGIDev->GetAdapter( &pTempAdapter );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IDXGIAdapter1> pAdapter;
		hr = pTempAdapter.QueryInterface( __uuidof( IDXGIAdapter1 ), (LPVOID*)&pAdapter );
		BREAK_ON_FAIL( hr );

		SafeRelease( m_pDXGIFactory2 );
		hr = pAdapter->GetParent( __uuidof( IDXGIFactory2 ), (LPVOID*)&m_pDXGIFactory2 );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IDXGIOutput> pDXGIOutput;
		hr = pAdapter->EnumOutputs( 0, &pDXGIOutput );
		BREAK_ON_FAIL( hr );

		SafeRelease( m_pDXGIOutput1 );
		hr = pDXGIOutput.QueryInterface( __uuidof( IDXGIOutput1 ), (LPVOID*)&m_pDXGIOutput1 );
		BREAK_ON_FAIL( hr );

		if( m_useDCompVisual ) {
			hr = CreateDCompDeviceAndVisual();
			BREAK_ON_FAIL( hr );
		}

		SafeRelease( m_pPool );
		SafeRelease( m_pReady );
		m_pPool = new Queue<ID3D11Texture2D>(); // Created with ref count = 1.
		m_pReady = new Queue<ID3D11Texture2D>(); // Created with ref count = 1.
	} while( FALSE );

	return hr;
}

//-------------------------------------------------------------------
// Name: CreateXVP
// Description: Creates a new instance of the XVP MFT.
//-------------------------------------------------------------------

#if (WINVER >= _WIN32_WINNT_WIN8) 
HRESULT PresenterDX11::CreateXVP( void )
{
	HRESULT hr = S_OK;

	do {
		hr = CoCreateInstance( CLSID_VideoProcessorMFT, nullptr, CLSCTX_INPROC_SERVER, IID_IMFTransform, (void**)&m_pXVP );
		BREAK_ON_FAIL( hr );

		hr = m_pXVP->ProcessMessage( MFT_MESSAGE_SET_D3D_MANAGER, ULONG_PTR( m_pDXGIManager ) );
		BREAK_ON_FAIL( hr );

		// Tell the XVP that we are the swapchain allocator
		ScopedComPtr<IMFAttributes> pAttributes;
		hr = m_pXVP->GetAttributes( &pAttributes );
		BREAK_ON_FAIL( hr );

		hr = pAttributes->SetUINT32( MF_XVP_PLAYBACK_MODE, TRUE );
		BREAK_ON_FAIL( hr );

		hr = m_pXVP->QueryInterface( IID_PPV_ARGS( &m_pXVPControl ) );
		BREAK_ON_FAIL( hr );
	} while( FALSE );

	return hr;
}
#endif

//+-------------------------------------------------------------------------
//
//  Member:     FindBOBProcessorIndex
//
//  Synopsis:   Find the BOB video processor. BOB does not require any
//              reference frames and can be used with both Progressive
//              and interlaced video
//
//--------------------------------------------------------------------------

HRESULT PresenterDX11::FindBOBProcessorIndex( DWORD* pIndex )
{
	HRESULT hr = S_OK;
	D3D11_VIDEO_PROCESSOR_CAPS caps = {};
	D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS convCaps = {};

	*pIndex = 0;
	hr = m_pVideoProcessorEnum->GetVideoProcessorCaps( &caps );
	if( FAILED( hr ) ) {
		return hr;
	}
	for( DWORD i = 0; i < caps.RateConversionCapsCount; i++ ) {
		hr = m_pVideoProcessorEnum->GetVideoProcessorRateConversionCaps( i, &convCaps );
		if( FAILED( hr ) ) {
			return hr;
		}

		// Check the caps to see which deinterlacer is supported
		if( ( convCaps.ProcessorCaps & D3D11_VIDEO_PROCESSOR_PROCESSOR_CAPS_DEINTERLACE_BOB ) != 0 ) {
			*pIndex = i;
			return hr;
		}
	}

	return E_FAIL;
}

//-------------------------------------------------------------------
// Name: GetVideoDisplayArea
// Description: get the display area from the media type.
//-------------------------------------------------------------------

HRESULT PresenterDX11::GetVideoDisplayArea( IMFMediaType* pType, MFVideoArea* pArea )
{
	HRESULT hr = S_OK;
	BOOL bPanScan = FALSE;
	UINT32 uimageWidthInPixels = 0, uimageHeightInPixels = 0;

	hr = MFGetAttributeSize( pType, MF_MT_FRAME_SIZE, &uimageWidthInPixels, &uimageHeightInPixels );
	if( FAILED( hr ) ) {
		return hr;
	}

	if( uimageWidthInPixels != m_imageWidthInPixels || uimageHeightInPixels != m_imageHeightInPixels ) {
		SafeRelease( m_pVideoProcessorEnum );
		SafeRelease( m_pVideoProcessor );
		SafeRelease( m_pSwapChain1 );
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

HRESULT PresenterDX11::ProcessFrameUsingD3D11( ID3D11Texture2D* pLeftTexture2D, ID3D11Texture2D* pRightTexture2D, UINT dwLeftViewIndex, UINT dwRightViewIndex, RECT rcDest, UINT32 unInterlaceMode, IMFSample** ppVideoOutFrame )
{
	HRESULT hr = S_OK;

	LARGE_INTEGER lpcStart, lpcEnd;

	do {
		if( !m_pVideoDevice ) {
			hr = m_pD3DDevice->QueryInterface( __uuidof( ID3D11VideoDevice ), (void**)&m_pVideoDevice );
			BREAK_ON_FAIL( hr );
		}

		ScopedComPtr<ID3D11VideoContext> pVideoContext;
		hr = m_pD3DImmediateContext->QueryInterface( __uuidof( ID3D11VideoContext ), (void**)&pVideoContext );
		BREAK_ON_FAIL( hr );

		// Remember the original rectangles, so we can check if the client rect has changed.
		RECT TRectOld = m_rcDstApp;
		RECT SRectOld = m_rcSrcApp;
		UpdateRectangles( &TRectOld, &SRectOld );

		// Update destination rect with current client rect.
		m_rcDstApp = rcDest;

		D3D11_TEXTURE2D_DESC surfaceDesc;
		pLeftTexture2D->GetDesc( &surfaceDesc );

		if( !m_pVideoProcessorEnum || !m_pVideoProcessor || m_imageWidthInPixels != surfaceDesc.Width || m_imageHeightInPixels != surfaceDesc.Height ) {
			SafeRelease( m_pVideoProcessorEnum );
			SafeRelease( m_pVideoProcessor );

			m_imageWidthInPixels = surfaceDesc.Width;
			m_imageHeightInPixels = surfaceDesc.Height;

			D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
			ZeroMemory( &ContentDesc, sizeof( ContentDesc ) );
			ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
			ContentDesc.InputWidth = surfaceDesc.Width;
			ContentDesc.InputHeight = surfaceDesc.Height;
			ContentDesc.OutputWidth = surfaceDesc.Width;
			ContentDesc.OutputHeight = surfaceDesc.Height;
			ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

			hr = m_pVideoDevice->CreateVideoProcessorEnumerator( &ContentDesc, &m_pVideoProcessorEnum );
			BREAK_ON_FAIL( hr );

			UINT uiFlags;
			DXGI_FORMAT VP_Output_Format = DXGI_FORMAT_B8G8R8A8_UNORM;

			hr = m_pVideoProcessorEnum->CheckVideoProcessorFormat( VP_Output_Format, &uiFlags );
			if( FAILED( hr ) || 0 == ( uiFlags & D3D11_VIDEO_PROCESSOR_FORMAT_SUPPORT_OUTPUT ) ) {
				hr = MF_E_UNSUPPORTED_D3D_TYPE;
				break;
			}

			m_rcSrcApp.left = 0;
			m_rcSrcApp.top = 0;
			m_rcSrcApp.right = m_uiRealDisplayWidth;
			m_rcSrcApp.bottom = m_uiRealDisplayHeight;

			DWORD index;
			hr = FindBOBProcessorIndex( &index );
			BREAK_ON_FAIL( hr );

			hr = m_pVideoDevice->CreateVideoProcessor( m_pVideoProcessorEnum, index, &m_pVideoProcessor );
			BREAK_ON_FAIL( hr );

			if( m_b3DVideo ) {
				D3D11_VIDEO_PROCESSOR_CAPS vpCaps = { 0 };
				hr = m_pVideoProcessorEnum->GetVideoProcessorCaps( &vpCaps );
				BREAK_ON_FAIL( hr );

				if( vpCaps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_STEREO ) {
					m_bStereoEnabled = TRUE;
				}

				DXGI_MODE_DESC1 modeFilter = { 0 };
				modeFilter.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				modeFilter.Width = surfaceDesc.Width;
				modeFilter.Height = surfaceDesc.Height;
				modeFilter.Stereo = m_bStereoEnabled;

				DXGI_MODE_DESC1 matchedMode;
				if( m_bFullScreenState ) {
					hr = m_pDXGIOutput1->FindClosestMatchingMode1( &modeFilter, &matchedMode, m_pD3DDevice );
					BREAK_ON_FAIL( hr );
				}
			}
		}

		// Make sure the swap chain is initialized and has the correct size.
		RECT TRect = m_rcDstApp;
		RECT SRect = m_rcSrcApp;
		UpdateRectangles( &TRect, &SRect );

		const BOOL fDestRectChanged = !EqualRect( &TRect, &TRectOld );

		if( !m_pSwapChain1 || fDestRectChanged ) {
			hr = UpdateDXGISwapChain();
			BREAK_ON_FAIL( hr );
		}

		m_bCanProcessNextSample = FALSE;

		// Get Backbuffer.
		QueryPerformanceCounter( &lpcStart );

		ScopedComPtr<ID3D11Texture2D> pDXGIBackBuffer;
		hr = m_pSwapChain1->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&pDXGIBackBuffer );
		BREAK_ON_FAIL( hr );

		// Create the output media sample and buffers.
		ScopedComPtr<IMFMediaBuffer> pBuffer;
		hr = MFCreateDXGISurfaceBuffer( __uuidof( ID3D11Texture2D ), pDXGIBackBuffer, 0, FALSE, &pBuffer );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IMFSample> pRTSample;
		hr = MFCreateSample( &pRTSample );
		BREAK_ON_FAIL( hr );

		hr = pRTSample->AddBuffer( pBuffer );
		BREAK_ON_FAIL( hr );

		if( m_b3DVideo && FALSE /* 0 != m_vp3DOutput */ ) {
			pBuffer.Release();

			hr = MFCreateDXGISurfaceBuffer( __uuidof( ID3D11Texture2D ), pDXGIBackBuffer, 1, FALSE, &pBuffer );
			BREAK_ON_FAIL( hr );

			hr = pRTSample->AddBuffer( pBuffer );
			BREAK_ON_FAIL( hr );
		}

		if( ppVideoOutFrame != NULL ) {
			*ppVideoOutFrame = pRTSample;
			( *ppVideoOutFrame )->AddRef();
		}

		QueryPerformanceCounter( &lpcEnd );

		// Create Output View of Output Surfaces.
		QueryPerformanceCounter( &lpcStart );

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
		if( m_b3DVideo && FALSE /* 0 != m_vp3DOutput */ ) {
			OutputViewDesc.Texture2DArray.ArraySize = 2; // STEREO
		}

		ScopedComPtr<ID3D11VideoProcessorOutputView> pOutputView;
		hr = m_pVideoDevice->CreateVideoProcessorOutputView( pDXGIBackBuffer, m_pVideoProcessorEnum, &OutputViewDesc, &pOutputView );
		BREAK_ON_FAIL( hr );

		QueryPerformanceCounter( &lpcEnd );

		// Create Input Views.
		QueryPerformanceCounter( &lpcStart );

		D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputLeftViewDesc;
		ZeroMemory( &InputLeftViewDesc, sizeof( InputLeftViewDesc ) );
		InputLeftViewDesc.FourCC = 0;
		InputLeftViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
		InputLeftViewDesc.Texture2D.MipSlice = 0;
		InputLeftViewDesc.Texture2D.ArraySlice = dwLeftViewIndex;

		ScopedComPtr<ID3D11VideoProcessorInputView> pLeftInputView;
		hr = m_pVideoDevice->CreateVideoProcessorInputView( pLeftTexture2D, m_pVideoProcessorEnum, &InputLeftViewDesc, &pLeftInputView );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<ID3D11VideoProcessorInputView> pRightInputView;
		if( m_b3DVideo && FALSE /* MFVideo3DSampleFormat_MultiView == m_vp3DOutput */ && pRightTexture2D ) {
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

		// Set video context parameters.
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

		// Blit video to backbuffer.
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
		BREAK_ON_FAIL_MSG( hr, "Failed to blit to back buffer." );

		QueryPerformanceCounter( &lpcEnd );

		hr = BlitToShared( &StreamData, unInterlaceMode );
	} while( FALSE );

	return hr;
}

#if (WINVER >= _WIN32_WINNT_WIN8)
HRESULT PresenterDX11::ProcessFrameUsingXVP( IMFMediaType* pCurrentType, IMFSample* pVideoFrame, ID3D11Texture2D* pTexture2D, RECT rcDest, IMFSample** ppVideoOutFrame, BOOL* pbInputFrameUsed )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( m_pXVPControl, E_POINTER );
		BREAK_ON_NULL( m_pD3DImmediateContext, E_POINTER );

		if( !m_pVideoDevice ) {
			hr = m_pD3DDevice->QueryInterface( __uuidof( ID3D11VideoDevice ), (void**)&m_pVideoDevice );
			BREAK_ON_FAIL( hr );
		}

		ScopedComPtr<ID3D11VideoContext> pVideoContext;
		hr = m_pD3DImmediateContext->QueryInterface( __uuidof( ID3D11VideoContext ), (void**)&pVideoContext );
		BREAK_ON_FAIL( hr );

		// remember the original rectangles
		RECT TRectOld = m_rcDstApp;
		RECT SRectOld = m_rcSrcApp;
		UpdateRectangles( &TRectOld, &SRectOld );

		//Update destination rect with current client rect
		m_rcDstApp = rcDest;

		D3D11_TEXTURE2D_DESC surfaceDesc;
		pTexture2D->GetDesc( &surfaceDesc );

		BOOL fTypeChanged = FALSE;
		if( !m_pVideoProcessorEnum || !m_pSwapChain1 || m_imageWidthInPixels != surfaceDesc.Width || m_imageHeightInPixels != surfaceDesc.Height ) {
			SafeRelease( m_pVideoProcessorEnum );
			SafeRelease( m_pSwapChain1 );

			m_imageWidthInPixels = surfaceDesc.Width;
			m_imageHeightInPixels = surfaceDesc.Height;
			fTypeChanged = TRUE;

			D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
			ZeroMemory( &ContentDesc, sizeof( ContentDesc ) );
			ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
			ContentDesc.InputWidth = surfaceDesc.Width;
			ContentDesc.InputHeight = surfaceDesc.Height;
			ContentDesc.OutputWidth = surfaceDesc.Width;
			ContentDesc.OutputHeight = surfaceDesc.Height;
			ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;

			hr = m_pVideoDevice->CreateVideoProcessorEnumerator( &ContentDesc, &m_pVideoProcessorEnum );
			BREAK_ON_FAIL( hr );

			m_rcSrcApp.left = 0;
			m_rcSrcApp.top = 0;
			m_rcSrcApp.right = m_uiRealDisplayWidth;
			m_rcSrcApp.bottom = m_uiRealDisplayHeight;

			if( m_b3DVideo ) {
				D3D11_VIDEO_PROCESSOR_CAPS vpCaps = { 0 };
				hr = m_pVideoProcessorEnum->GetVideoProcessorCaps( &vpCaps );
				BREAK_ON_FAIL( hr );

				if( vpCaps.FeatureCaps & D3D11_VIDEO_PROCESSOR_FEATURE_CAPS_STEREO ) {
					m_bStereoEnabled = TRUE;
				}

				DXGI_MODE_DESC1 modeFilter = { 0 };
				modeFilter.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				modeFilter.Width = surfaceDesc.Width;
				modeFilter.Height = surfaceDesc.Height;
				modeFilter.Stereo = m_bStereoEnabled;

				DXGI_MODE_DESC1 matchedMode;
				if( m_bFullScreenState ) {
					hr = m_pDXGIOutput1->FindClosestMatchingMode1( &modeFilter, &matchedMode, m_pD3DDevice );
					BREAK_ON_FAIL( hr );
				}

				ScopedComPtr<IMFAttributes>  pAttributes;
				hr = m_pXVP->GetAttributes( &pAttributes );
				BREAK_ON_FAIL( hr );

				hr = pAttributes->SetUINT32( MF_ENABLE_3DVIDEO_OUTPUT, ( 0 != m_vp3DOutput ) ? MF3DVideoOutputType_Stereo : MF3DVideoOutputType_BaseView );
				BREAK_ON_FAIL( hr );
			}
		}

		// now create the input and output media types - these need to reflect
		// the src and destination rectangles that we have been given.
		RECT TRect = m_rcDstApp;
		RECT SRect = m_rcSrcApp;
		UpdateRectangles( &TRect, &SRect );

		const BOOL fDestRectChanged = !EqualRect( &TRect, &TRectOld );
		const BOOL fSrcRectChanged = !EqualRect( &SRect, &SRectOld );

		if( !m_pSwapChain1 || fDestRectChanged ) {
			hr = UpdateDXGISwapChain();
			BREAK_ON_FAIL( hr );
		}

		if( fTypeChanged || fSrcRectChanged || fDestRectChanged ) {
			// stop streaming to avoid multiple start\stop calls internally in XVP
			hr = m_pXVP->ProcessMessage( MFT_MESSAGE_NOTIFY_END_STREAMING, 0 );
			if( FAILED( hr ) ) {
				break;
			}

			if( fTypeChanged ) {
				hr = SetXVPOutputMediaType( pCurrentType, DXGI_FORMAT_B8G8R8A8_UNORM );
				BREAK_ON_FAIL( hr );
			}

			if( fDestRectChanged ) {
				hr = m_pXVPControl->SetDestinationRectangle( &m_rcDstApp );
				BREAK_ON_FAIL( hr );
			}

			if( fSrcRectChanged ) {
				hr = m_pXVPControl->SetSourceRectangle( &SRect );
				BREAK_ON_FAIL( hr );
			}

			hr = m_pXVP->ProcessMessage( MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0 );
			BREAK_ON_FAIL( hr );
		}

		m_bCanProcessNextSample = FALSE;

		// Get Backbuffer
		ScopedComPtr<ID3D11Texture2D> pDXGIBackBuffer;
		hr = m_pSwapChain1->GetBuffer( 0, __uuidof( ID3D11Texture2D ), (void**)&pDXGIBackBuffer );
		BREAK_ON_FAIL( hr );

		// create the output media sample
		ScopedComPtr<IMFSample> pRTSample;
		hr = MFCreateSample( &pRTSample );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IMFMediaBuffer> pBuffer;
		hr = MFCreateDXGISurfaceBuffer( __uuidof( ID3D11Texture2D ), pDXGIBackBuffer, 0, FALSE, &pBuffer );
		BREAK_ON_FAIL( hr );

		hr = pRTSample->AddBuffer( pBuffer );
		BREAK_ON_FAIL( hr );

		if( m_b3DVideo && 0 != m_vp3DOutput ) {
			pBuffer.Release();

			hr = MFCreateDXGISurfaceBuffer( __uuidof( ID3D11Texture2D ), pDXGIBackBuffer, 1, FALSE, &pBuffer );
			BREAK_ON_FAIL( hr );

			hr = pRTSample->AddBuffer( pBuffer );
			BREAK_ON_FAIL( hr );
		}

		if( ppVideoOutFrame != NULL ) {
			*ppVideoOutFrame = pRTSample;
			( *ppVideoOutFrame )->AddRef();
		}

		DWORD dwStatus = 0;
		MFT_OUTPUT_DATA_BUFFER outputDataBuffer = {};
		outputDataBuffer.pSample = pRTSample;
		hr = m_pXVP->ProcessOutput( 0, 1, &outputDataBuffer, &dwStatus );
		if( hr == MF_E_TRANSFORM_NEED_MORE_INPUT ) {
			//call process input on the MFT to deliver the YUV video sample
			// and the call process output to extract of newly processed frame
			hr = m_pXVP->ProcessInput( 0, pVideoFrame, 0 );
			BREAK_ON_FAIL( hr );

			*pbInputFrameUsed = TRUE;

			hr = m_pXVP->ProcessOutput( 0, 1, &outputDataBuffer, &dwStatus );
			BREAK_ON_FAIL( hr );
		}
		else {
			*pbInputFrameUsed = FALSE;
		}

		// TODO: Blit video to shared texture.
	} while( FALSE );

	return hr;
}
#endif

//+-------------------------------------------------------------------------
//
//  Member:     SetVideoContextParameters
//
//  Synopsis:   Updates the various parameters used for VpBlt call
//
//--------------------------------------------------------------------------

void PresenterDX11::SetVideoContextParameters( ID3D11VideoContext* pVideoContext, const RECT* pSRect, const RECT* pTRect, UINT32 unInterlaceMode )
{
	D3D11_VIDEO_FRAME_FORMAT FrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
	if( MFVideoInterlace_FieldInterleavedUpperFirst == unInterlaceMode || MFVideoInterlace_FieldSingleUpper == unInterlaceMode || MFVideoInterlace_MixedInterlaceOrProgressive == unInterlaceMode ) {
		FrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_TOP_FIELD_FIRST;
	}
	else if( MFVideoInterlace_FieldInterleavedLowerFirst == unInterlaceMode || MFVideoInterlace_FieldSingleLower == unInterlaceMode ) {
		FrameFormat = D3D11_VIDEO_FRAME_FORMAT_INTERLACED_BOTTOM_FIELD_FIRST;
	}

	// input format
	pVideoContext->VideoProcessorSetStreamFrameFormat( m_pVideoProcessor, 0, FrameFormat );

	// Output rate (repeat frames)
	pVideoContext->VideoProcessorSetStreamOutputRate( m_pVideoProcessor, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, TRUE, NULL );

	// Source rect
	pVideoContext->VideoProcessorSetStreamSourceRect( m_pVideoProcessor, 0, ( NULL == pSRect ) ? FALSE : TRUE, pSRect );

	// Stream dest rect
	pVideoContext->VideoProcessorSetStreamDestRect( m_pVideoProcessor, 0, ( NULL == pTRect ) ? FALSE : TRUE, pTRect );

	pVideoContext->VideoProcessorSetOutputTargetRect( m_pVideoProcessor, FALSE, NULL );

	// Stream color space
	D3D11_VIDEO_PROCESSOR_COLOR_SPACE colorSpace = {};
	colorSpace.YCbCr_xvYCC = 1;
	pVideoContext->VideoProcessorSetStreamColorSpace( m_pVideoProcessor, 0, &colorSpace );

	// Output color space
	pVideoContext->VideoProcessorSetOutputColorSpace( m_pVideoProcessor, &colorSpace );

	// Output background color (black)
	D3D11_VIDEO_COLOR backgroundColor = {};
	backgroundColor.RGBA.A = 1.0F;
	backgroundColor.RGBA.R = 1.0F * static_cast<float>( GetRValue( 0 ) ) / 255.0F;
	backgroundColor.RGBA.G = 1.0F * static_cast<float>( GetGValue( 0 ) ) / 255.0F;
	backgroundColor.RGBA.B = 1.0F * static_cast<float>( GetBValue( 0 ) ) / 255.0F;

	pVideoContext->VideoProcessorSetOutputBackgroundColor( m_pVideoProcessor, FALSE, &backgroundColor );
}

//+-------------------------------------------------------------------------
//
//  Member:     BlitToShared
//
//  Synopsis:   Blits a sample to a texture or surface for sharing.
//
//--------------------------------------------------------------------------

HRESULT PresenterDX11::BlitToShared( const D3D11_VIDEO_PROCESSOR_STREAM *pStream, UINT32 unInterlaceMode )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( pStream, E_POINTER );

		BREAK_ON_NULL( m_pPool, E_POINTER );
		BREAK_ON_NULL( m_pReady, E_POINTER );
		BREAK_ON_NULL( m_pVideoProcessorEnum, E_POINTER );
		BREAK_ON_NULL( m_pVideoProcessor, E_POINTER );

		D3D11_TEXTURE2D_DESC desc;
		desc.Width = m_imageWidthInPixels;
		desc.Height = m_imageHeightInPixels;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET;
		desc.CPUAccessFlags = 0;
#if (WINVER >= _WIN32_WINNT_WIN8)
		#error Untested!
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
#else
		desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
#endif

		// Get free texture.
		ScopedComPtr<ID3D11Texture2D> pFrame;
		hr = m_pPool->RemoveFront( &pFrame );

		// Check texture compatibility.
		if( SUCCEEDED( hr ) ) {
			D3D11_TEXTURE2D_DESC frameDesc;
			pFrame->GetDesc( &frameDesc );

			if( frameDesc.Width != desc.Width || frameDesc.Height != desc.Height ) {
				// Do not return it to the pool.
				pFrame.Release();
				hr = E_FAIL;
			}
		}

		// If no existing texture is available, create new one.
		if( FAILED( hr ) ) {
			hr = m_pD3DDevice->CreateTexture2D( &desc, NULL, &pFrame );
		}
		BREAK_ON_FAIL( hr );

		D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC OutputViewDesc;
		ZeroMemory( &OutputViewDesc, sizeof( OutputViewDesc ) );
		OutputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;

		ScopedComPtr<ID3D11VideoProcessorOutputView> pOutputView;
		hr = m_pVideoDevice->CreateVideoProcessorOutputView( pFrame, m_pVideoProcessorEnum, &OutputViewDesc, &pOutputView );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<ID3D11VideoContext> pVideoContext;
		hr = m_pD3DImmediateContext->QueryInterface( __uuidof( ID3D11VideoContext ), (void**)&pVideoContext );
		BREAK_ON_FAIL( hr );

		SetVideoContextParameters( pVideoContext, NULL, NULL, unInterlaceMode );

		hr = pVideoContext->VideoProcessorBlt( m_pVideoProcessor, pOutputView, 0, 1, pStream );
		BREAK_ON_FAIL_MSG( hr, "Failed to blit to shared texture." );

		// Make it available. If another frame is already available,
		// swap it out and return it to the pool. Ideally this is an
		// atomic operation, but it proved hard to maintain proper
		// refcounts, so we do it in two steps for now.
		ScopedComPtr<ID3D11Texture2D> pTemp;
		if( SUCCEEDED( m_pReady->RemoveFront( &pTemp ) ) )
			m_pPool->InsertBack( pTemp );

		m_pReady->InsertFront( pFrame );
	} while( FALSE );

	return hr;
}

//-------------------------------------------------------------------
// Name: SetXVPOutputMediaType
// Description: Tells the XVP about the size of the destination surface
// and where within the surface we should be writing.
//-------------------------------------------------------------------

#if (WINVER >=_WIN32_WINNT_WIN8)
HRESULT PresenterDX11::SetXVPOutputMediaType( IMFMediaType* pType, DXGI_FORMAT vpOutputFormat )
{
	HRESULT hr = S_OK;
	IMFVideoMediaType* pMTOutput = NULL;
	MFVIDEOFORMAT mfvf = {};

	if( SUCCEEDED( hr ) ) {
		hr = MFInitVideoFormat_RGB(
			&mfvf,
			m_rcDstApp.right,
			m_rcDstApp.bottom,
			MFMapDXGIFormatToDX9Format( vpOutputFormat ) );
	}

	if( SUCCEEDED( hr ) ) {
		hr = MFCreateVideoMediaType( &mfvf, &pMTOutput );
	}

	if( SUCCEEDED( hr ) ) {
		hr = m_pXVP->SetOutputType( 0, pMTOutput, 0 );
	}

	SafeRelease( pMTOutput );

	return hr;
}
#endif

//+-------------------------------------------------------------------------
//
//  Member:     UpdateDXGISwapChain
//
//  Synopsis:   Creates SwapChain for HWND or DComp or Resizes buffers in case of resolution change
//
//--------------------------------------------------------------------------

_Post_satisfies_( this->m_pSwapChain1 != NULL )
HRESULT PresenterDX11::UpdateDXGISwapChain( void )
{
	HRESULT hr = S_OK;

	// Get the DXGISwapChain1
	DXGI_SWAP_CHAIN_DESC1 scd;
	ZeroMemory( &scd, sizeof( scd ) );
	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	scd.Scaling = DXGI_SCALING_STRETCH;
	scd.Width = m_rcDstApp.right;
	scd.Height = m_rcDstApp.bottom;
	scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scd.Stereo = m_bStereoEnabled;
	scd.BufferUsage = DXGI_USAGE_BACK_BUFFER | DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.Flags = m_bStereoEnabled ? DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : 0; //opt in to do direct flip;
	scd.BufferCount = 4;

	do {
		if( m_pSwapChain1 ) {
			// Resize our back buffers for the desired format.
			hr = m_pSwapChain1->ResizeBuffers
				(
					4,
					m_rcDstApp.right,
					m_rcDstApp.bottom,
					scd.Format,
					scd.Flags
					);

			break;
		}

		if( !m_useDCompVisual ) {
			hr = m_pDXGIFactory2->CreateSwapChainForHwnd( m_pD3DDevice, m_hwndVideo, &scd, NULL, NULL, &m_pSwapChain1 );
			BREAK_ON_FAIL( hr );

			if( m_bFullScreenState ) {
				hr = m_pSwapChain1->SetFullscreenState( TRUE, NULL );
				BREAK_ON_FAIL( hr );
			}
			else {
				hr = m_pSwapChain1->SetFullscreenState( FALSE, NULL );
				BREAK_ON_FAIL( hr );
			}
		}
		else {
#if (WINVER >=_WIN32_WINNT_WIN8)
			// Create a swap chain for composition
			hr = m_pDXGIFactory2->CreateSwapChainForComposition( m_pD3DDevice, &scd, NULL, &m_pSwapChain1 );
			BREAK_ON_FAIL( hr );

			hr = m_pRootVisual->SetContent( m_pSwapChain1 );
			BREAK_ON_FAIL( hr );

			hr = m_pDCompDevice->Commit();
			BREAK_ON_FAIL( hr );
#else
			// This should never happen, as we only support composition on Windows 8+.
			hr = E_NOTIMPL;
			BREAK_ON_FAIL( hr );
#endif
		}
	} while( FALSE );

	return hr;
}



} // namespace detail
} // namespace msw
} // namespace cinder

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )