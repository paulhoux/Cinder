#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/detail/PresenterDX9.h"

#pragma comment(lib, "dxva2.lib")

static const GUID DXVA2_ModeH264_E = {
	0x1b81be68, 0xa0c7, 0x11d3,{ 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5 }
};

static const GUID DXVA2_Intel_ModeH264_E = {
	0x604F8E68, 0x4951, 0x4c54,{ 0x88, 0xFE, 0xAB, 0xD2, 0x5C, 0x15, 0xB3, 0xD6 }
};

namespace cinder {
namespace msw {
namespace detail {

PresenterDX9::PresenterDX9( void )
	: m_IsShutdown( FALSE )
	, m_DeviceResetToken( 0 )
	, m_pD3D9( NULL )
	, m_pD3DDevice( NULL )
	, m_pDeviceManager( NULL )
	, m_D3D9Module( NULL )
	, m_pDecoderService( NULL )
	, m_DecoderGUID( GUID_NULL )
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
HRESULT PresenterDX9::SetVideoWindow( HWND hwndVideo )
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

// Presenter
HRESULT PresenterDX9::IsMediaTypeSupported( IMFMediaType *pMediaType )
{
	DXVA2_ConfigPictureDecode* configs = NULL;

	HRESULT hr = S_OK;

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		BREAK_ON_NULL( pMediaType, E_POINTER );
		BREAK_ON_NULL( m_pDecoderService, E_POINTER );

		// Check device.
		if( m_pD3DDevice ) {}

		BOOL bIsCompressed;
		hr = pMediaType->IsCompressedFormat( &bIsCompressed );
		BREAK_ON_FAIL( hr );

		BREAK_IF_TRUE( bIsCompressed, MF_E_UNSUPPORTED_D3D_TYPE );

		GUID subType = GUID_NULL;
		hr = pMediaType->GetGUID( MF_MT_SUBTYPE, &subType );
		BREAK_ON_FAIL( hr );

		D3DFORMAT format = D3DFMT_UNKNOWN;
		format = D3DFORMAT( subType.Data1 );

		// Check if we can decode this format in hardware.
		DXVA2_VideoDesc desc;
		hr = ConvertToDXVAType( pMediaType, &desc );
		BREAK_ON_FAIL( hr );

		UINT configCount = 0;
		hr = m_pDecoderService->GetDecoderConfigurations( m_DecoderGUID, &desc, NULL, &configCount, &configs );
		BREAK_ON_FAIL( hr );

		ScopedPtr<IDirect3DSurface9> pSurface;
		hr = m_pDecoderService->CreateSurface(
			desc.SampleWidth, desc.SampleHeight, 0, (D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' ),
			D3DPOOL_DEFAULT, 0, DXVA2_VideoDecoderRenderTarget,
			&pSurface, NULL );
		BREAK_ON_FAIL( hr );

		IDirect3DSurface9* surfaces = pSurface.get();
		for( UINT i = 0; i < configCount; i++ ) {
			ScopedPtr<IDirectXVideoDecoder> pDecoder;
			hr = m_pDecoderService->CreateVideoDecoder( m_DecoderGUID, &desc, &configs[i], &surfaces, 1, &pDecoder );
			if( SUCCEEDED( hr ) && pDecoder )
				break;
		}

		/*D3DDISPLAYMODE mode;
		hr = m_pD3D9->GetAdapterDisplayMode( m_ConnectionGUID, &mode );
		BREAK_ON_FAIL( hr );

		hr = m_pD3D9->CheckDeviceType( m_ConnectionGUID, D3DDEVTYPE_HAL, mode.Format, format, TRUE );
		BREAK_ON_FAIL( hr );*/
	} while( FALSE );

	if( FAILED( hr ) )
		hr = MF_E_UNSUPPORTED_D3D_TYPE;

	CoTaskMemFree( configs );

	return hr;
}

// Presenter
HRESULT PresenterDX9::Shutdown( void )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = MF_E_SHUTDOWN;

	m_IsShutdown = TRUE;

	SafeRelease( m_pDecoderService );
	SafeRelease( m_pD3DDevice );
	SafeRelease( m_pDeviceManager );
	SafeRelease( m_pD3D9 );

	return hr;
}

/// Private methods

HRESULT PresenterDX9::CheckShutdown( void ) const
{
	if( m_IsShutdown ) {
		return MF_E_SHUTDOWN;
	}
	else {
		return S_OK;
	}
}

HRESULT PresenterDX9::CreateDXVA2ManagerAndDevice( D3D_DRIVER_TYPE DriverType )
{
	GUID *decoderDevices = NULL;

	HRESULT hr = S_OK;

	assert( m_pD3D9 == NULL );
	assert( m_pDeviceManager == NULL );

	do {
		// Create Direct3D
		hr = (_Direct3DCreate9Ex)( D3D_SDK_VERSION, &m_pD3D9 );
		BREAK_ON_FAIL( hr );

		// Create the device manager
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

		pPresentParams.BackBufferWidth = 1;
		pPresentParams.BackBufferHeight = 1;
		pPresentParams.Windowed = TRUE;
		pPresentParams.SwapEffect = D3DSWAPEFFECT_COPY;
		pPresentParams.BackBufferFormat = D3DFMT_UNKNOWN;
		pPresentParams.hDeviceWindow = GetDesktopWindow();
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

		// Create the video decoder service.
		HANDLE hDevice = NULL;
		hr = m_pDeviceManager->OpenDeviceHandle( &hDevice );
		BREAK_ON_FAIL( hr );

		hr = m_pDeviceManager->GetVideoService( hDevice, __uuidof( IDirectXVideoDecoderService ), (LPVOID*)&m_pDecoderService );
		m_pDeviceManager->CloseDeviceHandle( hDevice );
		BREAK_ON_FAIL( hr );

		UINT deviceCount = 0;
		hr = m_pDecoderService->GetDecoderDeviceGuids( &deviceCount, &decoderDevices );
		BREAK_ON_FAIL( hr );

		BOOL bFound = FALSE;
		for( UINT i = 0; i < deviceCount; ++i ) {
			if( decoderDevices[i] == DXVA2_ModeH264_E ||
				decoderDevices[i] == DXVA2_Intel_ModeH264_E ) {
				m_DecoderGUID = decoderDevices[i];
				bFound = TRUE;
				break;
			}
		}

		BREAK_IF_FALSE( bFound, E_FAIL );
	} while( false );

	CoTaskMemFree( decoderDevices );

	return hr;
}

HRESULT PresenterDX9::ConvertToDXVAType( IMFMediaType* pMediaType, DXVA2_VideoDesc* pDesc )
{
	if( pDesc == NULL )
		return E_POINTER;

	if( pMediaType == NULL )
		return E_POINTER;

	HRESULT hr = S_OK;

	ZeroMemory( pDesc, sizeof( *pDesc ) );

	do {
		GUID guidSubType = GUID_NULL;
		hr = pMediaType->GetGUID( MF_MT_SUBTYPE, &guidSubType );
		BREAK_ON_FAIL( hr );

		UINT32 width = 0, height = 0;
		hr = MFGetAttributeSize( pMediaType, MF_MT_FRAME_SIZE, &width, &height );
		BREAK_ON_FAIL( hr );

		UINT32 fpsNumerator = 0, fpsDenominator = 0;
		hr = MFGetAttributeRatio( pMediaType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator );
		BREAK_ON_FAIL( hr );

		pDesc->Format = (D3DFORMAT)guidSubType.Data1;
		pDesc->SampleWidth = width;
		pDesc->SampleHeight = height;
		pDesc->InputSampleFreq.Numerator = fpsNumerator;
		pDesc->InputSampleFreq.Denominator = fpsDenominator;

		hr = GetDXVA2ExtendedFormat( pMediaType, &pDesc->SampleFormat );
		BREAK_ON_FAIL( hr );

		pDesc->OutputFrameFreq = pDesc->InputSampleFreq;
		if( ( pDesc->SampleFormat.SampleFormat == DXVA2_SampleFieldInterleavedEvenFirst ) ||
			( pDesc->SampleFormat.SampleFormat == DXVA2_SampleFieldInterleavedOddFirst ) ) {
			pDesc->OutputFrameFreq.Numerator *= 2;
		}
	} while( FALSE );

	return hr;
}

HRESULT PresenterDX9::GetDXVA2ExtendedFormat( IMFMediaType* pMediaType, DXVA2_ExtendedFormat* pFormat )
{
	if( pFormat == NULL )
		return E_POINTER;

	if( pMediaType == NULL )
		return E_POINTER;

	HRESULT hr = S_OK;

	do {
		MFVideoInterlaceMode interlace = (MFVideoInterlaceMode)MFGetAttributeUINT32( pMediaType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown );

		if( interlace == MFVideoInterlace_MixedInterlaceOrProgressive ) {
			pFormat->SampleFormat = DXVA2_SampleFieldInterleavedEvenFirst;
		}
		else {
			pFormat->SampleFormat = (UINT)interlace;
		}

		pFormat->VideoChromaSubsampling =
			MFGetAttributeUINT32( pMediaType, MF_MT_VIDEO_CHROMA_SITING, MFVideoChromaSubsampling_Unknown );
		pFormat->NominalRange =
			MFGetAttributeUINT32( pMediaType, MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_Unknown );
		pFormat->VideoTransferMatrix =
			MFGetAttributeUINT32( pMediaType, MF_MT_YUV_MATRIX, MFVideoTransferMatrix_Unknown );
		pFormat->VideoLighting =
			MFGetAttributeUINT32( pMediaType, MF_MT_VIDEO_LIGHTING, MFVideoLighting_Unknown );
		pFormat->VideoPrimaries =
			MFGetAttributeUINT32( pMediaType, MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_Unknown );
		pFormat->VideoTransferFunction =
			MFGetAttributeUINT32( pMediaType, MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_Unknown );
	} while( FALSE );

	return hr;
}

} // namespace detail
} // namespace msw
} // namespace cinder