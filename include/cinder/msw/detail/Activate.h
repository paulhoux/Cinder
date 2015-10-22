#pragma once

#include "cinder/msw/detail/MFAttributesImpl.h"
#include "cinder/msw/detail/MediaSink.h"

namespace cinder {
namespace msw {
namespace detail {

class __declspec( uuid( "30212DC8-9CE6-4CEF-BF26-4CA034A3476F" ) ) Activate : public MFAttributesImpl<IMFActivate>, public IPersistStream {
public:
	static HRESULT CreateInstance( HWND hwnd, IMFActivate** ppActivate );

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
	Activate( void );
	~Activate( void );

	long           m_lRefCount;
	IMFMediaSink*  m_pMediaSink;
	HWND           m_hwnd;
};

} // namespace detail
} // namespace msw
} // namespace cinder
