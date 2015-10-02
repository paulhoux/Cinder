#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/detail/PresenterDX9.h"

#pragma comment(lib, "dxva2.lib")

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
HRESULT PresenterDX9::Shutdown( void )
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = MF_E_SHUTDOWN;

	m_IsShutdown = TRUE;

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
		ScopedComPtr<IDirect3DDevice9Ex> pDevice;
		hr = m_pD3D9->CreateDeviceEx( m_ConnectionGUID, D3DDEVTYPE_HAL, pPresentParams.hDeviceWindow,
									  D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_NOWINDOWCHANGES | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE,
									  &pPresentParams, NULL, &pDevice );
		BREAK_ON_FAIL( hr );

		// Get the adapter display mode.
		hr = m_pD3D9->GetAdapterDisplayMode( m_ConnectionGUID, &m_DisplayMode );
		BREAK_ON_FAIL( hr );

		// Reset the D3DDeviceManager with the new device.
		hr = m_pDeviceManager->ResetDevice( pDevice, m_DeviceResetToken );
		BREAK_ON_FAIL( hr );
	} while( false );

	return hr;
}

} // namespace detail
} // namespace msw
} // namespace cinder