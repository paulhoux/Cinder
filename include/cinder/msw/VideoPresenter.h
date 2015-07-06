
#pragma once

#include "cinder/msw/MediaFoundationFramework.h"

namespace cinder {
namespace msw {

class VideoPresenter : public IMFVideoDisplayControl, public IMFGetService {
	virtual ~VideoPresenter() {}

	// IUnknown
	STDMETHODIMP_( ULONG ) AddRef( void ) override;
	STDMETHODIMP_( ULONG ) Release( void ) override;
	virtual STDMETHODIMP QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv ) override;

	// IMFVideoDisplayControl
	virtual STDMETHODIMP GetAspectRatioMode( __RPC__out DWORD* pdwAspectRatioMode ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP GetBorderColor( __RPC__out COLORREF* pClr ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP GetCurrentImage( __RPC__inout BITMAPINFOHEADER* pBih, __RPC__deref_out_ecount_full_opt( *pcbDib ) BYTE** pDib, __RPC__out DWORD* pcbDib, __RPC__inout_opt LONGLONG* pTimestamp ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP GetFullscreen( __RPC__out BOOL* pfFullscreen ) override { return E_NOTIMPL; } // 
	virtual STDMETHODIMP GetIdealVideoSize( __RPC__inout_opt SIZE* pszMin, __RPC__inout_opt SIZE* pszMax ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP GetNativeVideoSize( __RPC__inout_opt SIZE* pszVideo, __RPC__inout_opt SIZE* pszARVideo ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP GetRenderingPrefs( __RPC__out DWORD* pdwRenderFlags ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP GetVideoPosition( __RPC__out MFVideoNormalizedRect* pnrcSource, __RPC__out LPRECT prcDest ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP GetVideoWindow( __RPC__deref_out_opt HWND* phwndVideo ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP RepaintVideo( void ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP SetAspectRatioMode( DWORD dwAspectRatioMode ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP SetBorderColor( COLORREF Clr ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP SetFullscreen( BOOL fFullscreen ) override { return E_NOTIMPL; } //
	virtual STDMETHODIMP SetRenderingPrefs( DWORD dwRenderingPrefs ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP SetVideoPosition( __RPC__in_opt const MFVideoNormalizedRect* pnrcSource, __RPC__in_opt const LPRECT prcDest ) override { return E_NOTIMPL; }
	virtual STDMETHODIMP SetVideoWindow( __RPC__in HWND hwndVideo ) override { return E_NOTIMPL; } //

	// IMFGetService
	virtual STDMETHODIMP GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject ) override { return E_NOTIMPL; } //

	//
	BOOL CanProcessNextSample( void );
	STDMETHODIMP Flush( void );
	STDMETHODIMP GetMonitorRefreshRate( DWORD* pdwMonitorRefreshRate );
	STDMETHODIMP IsMediaTypeSupported( IMFMediaType* pMediaType, DXGI_FORMAT dxgiFormat );
	STDMETHODIMP PresentFrame( void );
	STDMETHODIMP ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample = NULL );
	STDMETHODIMP SetCurrentMediaType( IMFMediaType* pMediaType );
	STDMETHODIMP Shutdown( void );

private:
	ULONG            mRefCount = 1;
};

inline ULONG VideoPresenter::AddRef( void )
{
	return InterlockedIncrement( &mRefCount );
}

inline ULONG VideoPresenter::Release( void )
{
	ULONG uCount = InterlockedDecrement( &mRefCount );
	if( uCount == 0 ) {
		delete this;
	}
	// For thread safety, return a temporary variable.
	return uCount;
}

inline HRESULT VideoPresenter::QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv )
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

}
}