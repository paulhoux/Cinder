#pragma once

#include "cinder/wmf/MediaFoundation.h"
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/wmf/MFAttributesImpl.h"
#include "cinder/wmf/MediaSink.h"

namespace cinder {
namespace wmf {

class __declspec( uuid( "30212DC8-9CE6-4CEF-BF26-4CA034A3476F" ) ) Activate : public MFAttributesImpl<IMFActivate>, public IPersistStream {
public:
	static HRESULT CreateInstance( HWND hwnd, IMFActivate** ppActivate, DirectXVersion dxVersion = DX_11 );

	// IUnknown
	STDMETHODIMP_( ULONG ) AddRef( void );
	STDMETHODIMP QueryInterface( REFIID riid, __RPC__deref_out _Result_nullonfailure_ void** ppvObject );
	STDMETHODIMP_( ULONG ) Release( void );

	// IMFActivate
	STDMETHODIMP ActivateObject( __RPC__in REFIID riid, __RPC__deref_out_opt void** ppvObject );
	STDMETHODIMP DetachObject( void );
	STDMETHODIMP ShutdownObject( void );

	// IPersistStream
	STDMETHODIMP GetSizeMax( __RPC__out ULARGE_INTEGER* pcbSize );
	STDMETHODIMP IsDirty( void );
	STDMETHODIMP Load( __RPC__in_opt IStream* pStream );
	STDMETHODIMP Save( __RPC__in_opt IStream* pStream, BOOL bClearDirty );

	// IPersist (from IPersistStream)
	STDMETHODIMP GetClassID( __RPC__out CLSID* pClassID );

private:
	Activate( DirectXVersion dxVersion );
	~Activate( void );

	long           m_lRefCount;
	IMFMediaSink*  m_pMediaSink;
	HWND           m_hwnd;

	DirectXVersion m_dxVersion = DX_11;
};

} // namespace wmf
} // namespace cinder

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )
