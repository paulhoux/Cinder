#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/detail/PresenterDX9.h"

namespace cinder {
namespace msw {
namespace detail {

PresenterDX9::PresenterDX9( void )
	: m_nRefCount( 1 )
	, m_critSec() // default ctor
	, m_IsShutdown( FALSE )
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
ULONG PresenterDX9::AddRef( void )
{
	return InterlockedIncrement( &m_nRefCount );
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

// IUnknown
ULONG  PresenterDX9::Release( void )
{
	ULONG uCount = InterlockedDecrement( &m_nRefCount );
	if( uCount == 0 ) {
		delete this;
	}
	// For thread safety, return a temporary variable.
	return uCount;
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

} // namespace detail
} // namespace msw
} // namespace cinder