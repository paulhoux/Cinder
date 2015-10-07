#pragma once

#include "cinder/msw/detail/Presenter.h"

#include <d3d9.h>
#include <dxva2api.h>

DEFINE_GUID( CLSID_VideoProcessorMFT, 0x88753b26, 0x5b24, 0x49bd, 0xb2, 0xe7, 0xc, 0x44, 0x5c, 0x78, 0xc9, 0x82 );

namespace cinder {
namespace msw {
namespace detail {

class PresenterDX9 : public Presenter {
public:
	PresenterDX9( void );
	virtual ~PresenterDX9( void );

	// IUnknown
	STDMETHODIMP QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv );

	// IMFVideoDisplayControl
	STDMETHODIMP GetAspectRatioMode( __RPC__out DWORD* pdwAspectRatioMode ) { return E_NOTIMPL; }
	STDMETHODIMP GetBorderColor( __RPC__out COLORREF* pClr ) { return E_NOTIMPL; }
	STDMETHODIMP GetCurrentImage( __RPC__inout BITMAPINFOHEADER* pBih, __RPC__deref_out_ecount_full_opt( *pcbDib ) BYTE** pDib, __RPC__out DWORD* pcbDib, __RPC__inout_opt LONGLONG* pTimestamp ) { return E_NOTIMPL; }
	STDMETHODIMP GetFullscreen( __RPC__out BOOL* pfFullscreen ) { return E_NOTIMPL; /* TODO */ }
	STDMETHODIMP GetIdealVideoSize( __RPC__inout_opt SIZE* pszMin, __RPC__inout_opt SIZE* pszMax ) { return E_NOTIMPL; }
	STDMETHODIMP GetNativeVideoSize( __RPC__inout_opt SIZE* pszVideo, __RPC__inout_opt SIZE* pszARVideo ) { return E_NOTIMPL; }
	STDMETHODIMP GetRenderingPrefs( __RPC__out DWORD* pdwRenderFlags ) { return E_NOTIMPL; }
	STDMETHODIMP GetVideoPosition( __RPC__out MFVideoNormalizedRect* pnrcSource, __RPC__out LPRECT prcDest ) { return E_NOTIMPL; }
	STDMETHODIMP GetVideoWindow( __RPC__deref_out_opt HWND* phwndVideo ) { return E_NOTIMPL; }
	STDMETHODIMP RepaintVideo( void ) { return E_NOTIMPL; }
	STDMETHODIMP SetAspectRatioMode( DWORD dwAspectRatioMode ) { return E_NOTIMPL; }
	STDMETHODIMP SetBorderColor( COLORREF Clr ) { return E_NOTIMPL; }
	STDMETHODIMP SetFullscreen( BOOL fFullscreen ) { return E_NOTIMPL; /* TODO */ }
	STDMETHODIMP SetRenderingPrefs( DWORD dwRenderingPrefs ) { return E_NOTIMPL; }
	STDMETHODIMP SetVideoPosition( __RPC__in_opt const MFVideoNormalizedRect* pnrcSource, __RPC__in_opt const LPRECT prcDest ) { return E_NOTIMPL; }
	STDMETHODIMP SetVideoWindow( __RPC__in HWND hwndVideo );

	// IMFGetService
	STDMETHODIMP GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject );

	// Presenter
	STDMETHODIMP Initialize( void );
	BOOL         CanProcessNextSample( void ) { return FALSE; /* TODO */ }
	STDMETHODIMP Flush( void ) { return E_NOTIMPL; /* TODO */ }
	STDMETHODIMP GetMonitorRefreshRate( DWORD* pdwMonitorRefreshRate ) { return E_NOTIMPL; /* TODO */ }
	STDMETHODIMP IsMediaTypeSupported( IMFMediaType* pMediaType );
	STDMETHODIMP PresentFrame( void ) { return E_NOTIMPL; /* TODO */ }
	STDMETHODIMP ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample = NULL );
	STDMETHODIMP SetCurrentMediaType( IMFMediaType* pMediaType ) { return S_OK; /* TODO */ }
	STDMETHODIMP Shutdown( void );
	BOOL         IsDX9() const { return TRUE; }
	BOOL         IsDX11() const { return FALSE; }

private:
	HRESULT CheckShutdown( void ) const;
	HRESULT CreateDXVA2ManagerAndDevice( D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_HARDWARE );

	HRESULT ConvertToDXVAType( IMFMediaType* pMediaType, DXVA2_VideoDesc* pDesc ) const;
	HRESULT GetDXVA2ExtendedFormat( IMFMediaType* pMediaType, DXVA2_ExtendedFormat* pFormat ) const;

	BOOL                            m_IsShutdown;               // Flag to indicate if Shutdown() method was called.

	UINT                            m_DeviceResetToken;
	D3DDISPLAYMODE                  m_DisplayMode;              // Adapter's display mode.

	IDirect3D9Ex*                   m_pD3D9;
	IDirect3DDevice9Ex*             m_pD3DDevice;
	IDirect3DDeviceManager9*        m_pDeviceManager;

	IDirectXVideoDecoderService*    m_pDecoderService;
	GUID                            m_DecoderGUID;

	// Dynamically link to DirectX.
	HMODULE                         m_D3D9Module;

	// Define a function pointer to the Direct3DCreate9Ex function.
	typedef HRESULT( WINAPI *LPDIRECT3DCREATE9EX )( UINT, IDirect3D9Ex ** );

	LPDIRECT3DCREATE9EX             _Direct3DCreate9Ex;
};

} // namespace detail
} // namespace msw
} // namespace cinder
