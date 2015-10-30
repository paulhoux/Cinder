#pragma once

#include "cinder/wmf/MediaFoundation.h"
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/wmf/Presenter.h"
#include "cinder/msw/Queue.h"

#include <d3d11.h>
#include <dxgi1_2.h>

#if (WINVER >= _WIN32_WINNT_WIN8)
#include <dcomp.h> // for IDCompositionDevice et.al. (Windows 8+ only)
#pragma message("WARNING! Uses experimental Windows 8+ code that has not been tested for OpenGL compatibility!")
#endif

// MF_XVP_PLAYBACK_MODE
// Data type: UINT32 (treat as BOOL)
// If this attribute is TRUE, the XVP will run in playback mode where it will:
//      1) Allow caller to allocate D3D output samples
//      2) Not perform FRC
//      3) Allow last frame regeneration (repaint)
// This attribute should be set on the transform's attrbiute store prior to setting the input type.
DEFINE_GUID( MF_XVP_PLAYBACK_MODE, 0x3c5d293f, 0xad67, 0x4e29, 0xaf, 0x12, 0xcf, 0x3e, 0x23, 0x8a, 0xcc, 0xe9 );

namespace cinder {
namespace wmf {

class __declspec( uuid( "B85BC91A-0513-4015-9AE6-C10FEB1D00E9" ) ) PresenterDX11 : public Presenter {
public:
	PresenterDX11( void );
	~PresenterDX11( void );

	// IUnknown
	STDMETHODIMP            QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv );

	// IMFVideoDisplayControl
	STDMETHODIMP            GetAspectRatioMode( __RPC__out DWORD* pdwAspectRatioMode ) { return E_NOTIMPL; }
	STDMETHODIMP            GetBorderColor( __RPC__out COLORREF* pClr ) { return E_NOTIMPL; }
	STDMETHODIMP            GetCurrentImage( __RPC__inout BITMAPINFOHEADER* pBih, __RPC__deref_out_ecount_full_opt( *pcbDib ) BYTE** pDib, __RPC__out DWORD* pcbDib, __RPC__inout_opt LONGLONG* pTimestamp ) { return E_NOTIMPL; }
	STDMETHODIMP            GetFullscreen( __RPC__out BOOL* pfFullscreen );
	STDMETHODIMP            GetIdealVideoSize( __RPC__inout_opt SIZE* pszMin, __RPC__inout_opt SIZE* pszMax ) { return E_NOTIMPL; }
	STDMETHODIMP            GetNativeVideoSize( __RPC__inout_opt SIZE* pszVideo, __RPC__inout_opt SIZE* pszARVideo ) { return E_NOTIMPL; }
	STDMETHODIMP            GetRenderingPrefs( __RPC__out DWORD* pdwRenderFlags ) { return E_NOTIMPL; }
	STDMETHODIMP            GetVideoPosition( __RPC__out MFVideoNormalizedRect* pnrcSource, __RPC__out LPRECT prcDest ) { return E_NOTIMPL; }
	STDMETHODIMP            GetVideoWindow( __RPC__deref_out_opt HWND* phwndVideo ) { return E_NOTIMPL; }
	STDMETHODIMP            RepaintVideo( void ) { return E_NOTIMPL; }
	STDMETHODIMP            SetAspectRatioMode( DWORD dwAspectRatioMode ) { return E_NOTIMPL; }
	STDMETHODIMP            SetBorderColor( COLORREF Clr ) { return E_NOTIMPL; }
	STDMETHODIMP            SetFullscreen( BOOL fFullscreen );
	STDMETHODIMP            SetRenderingPrefs( DWORD dwRenderingPrefs ) { return E_NOTIMPL; }
	STDMETHODIMP            SetVideoPosition( __RPC__in_opt const MFVideoNormalizedRect* pnrcSource, __RPC__in_opt const LPRECT prcDest ) { return E_NOTIMPL; }
	STDMETHODIMP            SetVideoWindow( __RPC__in HWND hwndVideo );

	// IMFGetService
	STDMETHODIMP            GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject );


	// Presenter
	STDMETHODIMP            Initialize( void );
	STDMETHODIMP_( BOOL )   CanProcessNextSample( void ) { return m_bCanProcessNextSample; }
	STDMETHODIMP            Flush( void );
	STDMETHODIMP            GetMonitorRefreshRate( DWORD* pdwMonitorRefreshRate );
	STDMETHODIMP            IsMediaTypeSupported( IMFMediaType* pMediaType );
	STDMETHODIMP            PresentFrame( void );
	STDMETHODIMP            ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample = NULL );
	STDMETHODIMP            SetCurrentMediaType( IMFMediaType* pMediaType );
	STDMETHODIMP            Shutdown( void );
	STDMETHODIMP            GetMediaTypeByIndex( DWORD dwIndex, GUID *subType ) const;
	STDMETHODIMP_( DWORD )  GetMediaTypeCount() const { return s_dwNumVideoFormats; }
	STDMETHODIMP            GetFrame( ID3D11Texture2D **ppFrame );
	STDMETHODIMP            ReturnFrame( ID3D11Texture2D **ppFrame );

private:
	STDMETHODIMP_( VOID )   CheckDecodeSwitchRegKey( void );
	STDMETHODIMP            CheckDeviceState( BOOL* pbDeviceChanged );
	STDMETHODIMP            CreateDCompDeviceAndVisual( void );
	STDMETHODIMP            CreateDXGIManagerAndDevice();
	STDMETHODIMP            FindBOBProcessorIndex( DWORD* pIndex );
	STDMETHODIMP            GetVideoDisplayArea( IMFMediaType* pType, MFVideoArea* pArea );
	STDMETHODIMP            ProcessFrameUsingD3D11( ID3D11Texture2D* pLeftTexture2D, ID3D11Texture2D* pRightTexture2D, UINT dwLeftViewIndex, UINT dwRightViewIndex, RECT rcDest, UINT32 unInterlaceMode, IMFSample** ppVideoOutFrame );
	STDMETHODIMP_( VOID )   SetVideoContextParameters( ID3D11VideoContext* pVideoContext, const RECT* pSRect, const RECT* pTRect, UINT32 unInterlaceMode );
	STDMETHODIMP            BlitToShared( const D3D11_VIDEO_PROCESSOR_STREAM *pStream, UINT32 unInterlaceMode );

#if (WINVER >=_WIN32_WINNT_WIN8)
	STDMETHODIMP            CreateXVP( void );
	STDMETHODIMP            ProcessFrameUsingXVP( IMFMediaType* pCurrentType, IMFSample* pVideoFrame, ID3D11Texture2D* pTexture2D, RECT rcDest, IMFSample** ppVideoOutFrame, BOOL* pbInputFrameUsed );
	STDMETHODIMP            SetXVPOutputMediaType( IMFMediaType* pType, DXGI_FORMAT vpOutputFormat );
#endif

	_Post_satisfies_( this->m_pSwapChain1 != NULL )
	STDMETHODIMP            UpdateDXGISwapChain( void );

	static const struct FormatEntry {
		GUID            Subtype;
		DXGI_FORMAT     DXGIFormat;
	} s_DXGIFormatMapping[];

	IDXGIFactory2*                  m_pDXGIFactory2;
	IMFDXGIDeviceManager*           m_pDXGIManager;
	IDXGIOutput1*                   m_pDXGIOutput1;
	IMFVideoSampleAllocatorEx*      m_pSampleAllocatorEx;
	BOOL                            m_bSoftwareDXVADeviceInUse;
	UINT                            m_DeviceResetToken;
	UINT                            m_DXSWSwitch;
	UINT                            m_useXVP;
	UINT                            m_useDCompVisual;
	UINT                            m_useDebugLayer;
	IDXGISwapChain1*                m_pSwapChain1;
	BOOL                            m_bDeviceChanged;
	BOOL                            m_bResize;
	BOOL                            m_b3DVideo;
	BOOL                            m_bStereoEnabled;
	BOOL                            m_bFullScreenState;
	BOOL                            m_bCanProcessNextSample;

	DXGI_FORMAT                     m_dxgiFormat;

	ID3D11Device*                   m_pD3DDevice;
	ID3D11DeviceContext*            m_pD3DImmediateContext;
	ID3D11VideoDevice*              m_pVideoDevice;
	ID3D11VideoProcessorEnumerator* m_pVideoProcessorEnum;
	ID3D11VideoProcessor*           m_pVideoProcessor;

	msw::Queue<ID3D11Texture2D>*    m_pPool;
	msw::Queue<ID3D11Texture2D>*    m_pReady;

#if (WINVER >= _WIN32_WINNT_WIN8)
	IDCompositionDevice*            m_pDCompDevice;
	IDCompositionTarget*            m_pHwndTarget;
	IDCompositionVisual*            m_pRootVisual;
	MFVideo3DFormat                 m_vp3DOutput;
	IMFTransform*                   m_pXVP;
	IMFVideoProcessorControl*       m_pXVPControl;
#endif

	// Dynamically link to DirectX.
	HMODULE                         m_D3D11Module;

	PFN_D3D11_CREATE_DEVICE         _D3D11CreateDevice;

	// Supported video formats.
	static GUID const* const s_pVideoFormats[];
	static const DWORD s_dwNumVideoFormats;
};

} // namespace wmf
} // namespace cinder

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )
