#include "cinder/msw/detail/Activate.h"
#include "cinder/msw/ScopedPtr.h"

#if (WINVER < _WIN32_WINNT_WIN7)
#error "The minimum system required to compile this file is Windows 7."
#endif

namespace cinder {
namespace msw {
namespace detail {

HRESULT Activate::CreateInstance( HWND hwnd, IMFActivate** ppActivate )
{
	if( ppActivate == NULL ) {
		return E_POINTER;
	}

	Activate* pActivate = new Activate();
	if( pActivate == NULL ) {
		return E_OUTOFMEMORY;
	}

	pActivate->AddRef();

	HRESULT hr = S_OK;

	do {
		hr = pActivate->Initialize();
		BREAK_ON_FAIL( hr );

		hr = pActivate->QueryInterface( IID_PPV_ARGS( ppActivate ) );
		BREAK_ON_FAIL( hr );

		pActivate->m_hwnd = hwnd;
	} while( FALSE );

	SafeRelease( pActivate );

	return hr;
}

// IUnknown
ULONG Activate::AddRef( void )
{
	return InterlockedIncrement( &m_lRefCount );
}

// IUnknown
HRESULT Activate::QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv )
{
	if( !ppv ) {
		return E_POINTER;
	}
	if( iid == IID_IUnknown ) {
		*ppv = static_cast<IUnknown*>( static_cast<IMFActivate*>( this ) );
	}
	else if( iid == __uuidof( IMFActivate ) ) {
		*ppv = static_cast<IMFActivate*>( this );
	}
	else if( iid == __uuidof( IPersistStream ) ) {
		*ppv = static_cast<IPersistStream*>( this );
	}
	else if( iid == __uuidof( IPersist ) ) {
		*ppv = static_cast<IPersist*>( this );
	}
	else if( iid == __uuidof( IMFAttributes ) ) {
		*ppv = static_cast<IMFAttributes*>( this );
	}
	else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

// IUnknown
ULONG Activate::Release( void )
{
	ULONG lRefCount = InterlockedDecrement( &m_lRefCount );
	if( lRefCount == 0 ) {
		delete this;
	}
	return lRefCount;
}

// IMFActivate
HRESULT Activate::ActivateObject( __RPC__in REFIID riid, __RPC__deref_out_opt void** ppvObject )
{
	HRESULT hr = S_OK;

	do {
		if( m_pMediaSink == NULL ) {
			hr = MediaSink::CreateInstance( IID_PPV_ARGS( &m_pMediaSink ) );
			BREAK_ON_FAIL( hr );

			ScopedPtr<IMFGetService> pSinkGetService;
			hr = m_pMediaSink->QueryInterface( IID_PPV_ARGS( &pSinkGetService ) );
			BREAK_ON_FAIL( hr );

			ScopedPtr<IMFVideoDisplayControl> pSinkVideoDisplayControl;
			hr = pSinkGetService->GetService( MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pSinkVideoDisplayControl ) );
			BREAK_ON_FAIL( hr );

			hr = pSinkVideoDisplayControl->SetVideoWindow( m_hwnd );
			BREAK_ON_FAIL( hr );
		}

		hr = m_pMediaSink->QueryInterface( riid, ppvObject );
		BREAK_ON_FAIL( hr );
	} while( FALSE );

	return hr;
}

// IMFActivate
HRESULT Activate::DetachObject( void )
{
	SafeRelease( m_pMediaSink );

	return S_OK;
}

// IMFActivate
HRESULT Activate::ShutdownObject( void )
{
	if( m_pMediaSink != NULL ) {
		m_pMediaSink->Shutdown();
		SafeRelease( m_pMediaSink );
	}

	return S_OK;
}

// IPersistStream
HRESULT Activate::GetSizeMax( __RPC__out ULARGE_INTEGER* pcbSize )
{
	return E_NOTIMPL;
}

// IPersistStream
HRESULT Activate::IsDirty( void )
{
	return E_NOTIMPL;
}

// IPersistStream
HRESULT Activate::Load( __RPC__in_opt IStream* pStream )
{
	return E_NOTIMPL;
}

// IPersistStream
HRESULT Activate::Save( __RPC__in_opt IStream* pStream, BOOL bClearDirty )
{
	return E_NOTIMPL;
}

// IPersist
HRESULT Activate::GetClassID( __RPC__out CLSID* pClassID )
{
	if( pClassID == NULL ) {
		return E_POINTER;
	}
	//*pClassID = CLSID_DX11VideoRendererActivate;
	//return S_OK;
	return E_FAIL;
}

// ctor
Activate::Activate( void ) :
	m_lRefCount( 0 ),
	m_pMediaSink( NULL ),
	m_hwnd( NULL )
{
}

// dtor
Activate::~Activate( void )
{
	SafeRelease( m_pMediaSink );
}

} // namespace detail
} // namespace msw
} // namespace cinder
