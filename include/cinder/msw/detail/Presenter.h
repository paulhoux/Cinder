#pragma once

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/ScopedPtr.h"

#include <d3dcommon.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h> // for IDCompositionDevice et.al. (Windows 8+ only)
#include <evr.h> // IMFVideoDisplayControl
#include <mfidl.h> // for IMFVideoProcessorControl

namespace cinder {
namespace msw {
namespace detail {

//! Abstract base class.
class Presenter : public IMFVideoDisplayControl, public IMFGetService {
public:
	virtual ~Presenter() {}

	// IUnknown
	virtual STDMETHODIMP_( ULONG ) AddRef( void ) = 0;
	virtual STDMETHODIMP QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv ) = 0;
	virtual STDMETHODIMP_( ULONG ) Release( void ) = 0;

	//!
	virtual BOOL    CanProcessNextSample( void ) = 0;
	//!
	virtual HRESULT Flush( void ) = 0;
	//!
	virtual HRESULT GetMonitorRefreshRate( DWORD* pdwMonitorRefreshRate ) = 0;
	//!
	virtual HRESULT IsMediaTypeSupported( IMFMediaType* pMediaType, DXGI_FORMAT dxgiFormat = DXGI_FORMAT_UNKNOWN ) = 0;
	//!
	virtual HRESULT PresentFrame( void ) = 0;
	//!
	virtual HRESULT ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample = NULL ) = 0;
	//!
	virtual HRESULT SetCurrentMediaType( IMFMediaType* pMediaType ) = 0;
	//!
	virtual HRESULT Shutdown( void ) = 0;
};

} // namespace detail
} // namespace msw
} // namespace cinder
