#pragma once

#include "cinder/wmf/MediaFoundation.h"
#include "cinder/wmf/MonitorArray.h"
#include "cinder/msw/ThreadSafeDeque.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <initguid.h>

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

// MF_MT_VIDEO_3D is only defined for Windows 8+, but we need it to check if a video is 3D.
#if (WINVER < _WIN32_WINNT_WIN8)
DEFINE_GUID( MF_MT_VIDEO_3D, 0xcb5e88cf, 0x7b5b, 0x476b, 0x85, 0xaa, 0x1c, 0xa5, 0xae, 0x18, 0x75, 0x55 );
#endif

//DEFINE_GUID( CLSID_VideoProcessorMFT, 0x88753b26, 0x5b24, 0x49bd, 0xb2, 0xe7, 0xc, 0x44, 0x5c, 0x78, 0xc9, 0x82 );

// We store the shared texture handle in a private data field of the texture using this custom GUID.
DEFINE_GUID( GUID_SharedHandle, 0xad243275, 0xbd3a, 0x473d, 0x8c, 0xa5, 0xb9, 0x7a, 0x9c, 0x4e, 0x92, 0x7d );

namespace cinder {
namespace wmf {

//! Abstract base class.
class __declspec( uuid( "F57105CF-F608-4107-ADD5-B0CF01CBFD0D" ) ) Presenter : public IMFVideoDisplayControl, public IMFGetService {
public:
	Presenter()
		: m_nRefCount( 1 )
		, m_critSec() // default ctor
		, m_hwndVideo( NULL )
		, m_pMonitors( NULL )
		, m_lpCurrMon( NULL )
		, m_displayRect() // default ctor
		, m_imageWidthInPixels( 0 )
		, m_imageHeightInPixels( 0 )
		, m_uiRealDisplayWidth( 0 )
		, m_uiRealDisplayHeight( 0 )
		, m_rcSrcApp() // default ctor
		, m_rcDstApp() // default ctor
		, m_IsShutdown( FALSE )
	{
	}
	virtual ~Presenter()
	{
		msw::SafeDelete( m_pMonitors );
	}

	// IUnknown
	virtual STDMETHODIMP QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv ) = 0;

	virtual STDMETHODIMP_( ULONG ) AddRef( void )
	{
		return InterlockedIncrement( &m_nRefCount );
	}

	STDMETHODIMP_( ULONG ) Release( void )
	{
		ULONG uCount = InterlockedDecrement( &m_nRefCount );
		if( uCount == 0 ) {
			delete this;
		}
		// For thread safety, return a temporary variable.
		return uCount;
	}

	// Presenter
	virtual STDMETHODIMP           Initialize( void ) = 0;
	virtual STDMETHODIMP_( BOOL )  CanProcessNextSample( void ) = 0;
	virtual STDMETHODIMP           Flush( void ) = 0;
	virtual STDMETHODIMP           GetMonitorRefreshRate( DWORD* pdwMonitorRefreshRate ) = 0;
	virtual STDMETHODIMP           IsMediaTypeSupported( IMFMediaType* pMediaType ) = 0;
	virtual STDMETHODIMP           PresentFrame( void ) = 0;
	virtual STDMETHODIMP           ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample = NULL ) = 0;
	virtual STDMETHODIMP           SetCurrentMediaType( IMFMediaType* pMediaType ) = 0;
	virtual STDMETHODIMP           Shutdown( void ) = 0;
	virtual STDMETHODIMP           GetMediaTypeByIndex( DWORD dwIndex, GUID *subType ) const = 0;
	virtual STDMETHODIMP_( DWORD ) GetMediaTypeCount() const = 0;

protected:
	STDMETHODIMP_( BOOL )          CheckEmptyRect( RECT* pDst );
	STDMETHODIMP                   CheckShutdown( void ) const;
	STDMETHODIMP                   SetMonitor( UINT adapterID );
	STDMETHODIMP                   SetVideoMonitor( HWND hwndVideo );

	STDMETHODIMP_( VOID )          ReduceToLowestTerms( int NumeratorIn, int DenominatorIn, int * pNumeratorOut, int * pDenominatorOut );
	STDMETHODIMP_( VOID )          LetterBoxDstRect( LPRECT lprcLBDst, const RECT & rcSrc, const RECT & rcDst );
	STDMETHODIMP_( VOID )          PixelAspectToPictureAspect( int Width, int Height, int PixelAspectX, int PixelAspectY, int * pPictureAspectX, int * pPictureAspectY );
	STDMETHODIMP_( VOID )          AspectRatioCorrectSize( LPSIZE lpSizeImage, const SIZE & sizeAr, const SIZE & sizeOrig, BOOL ScaleXorY );
	STDMETHODIMP_( VOID )          UpdateRectangles( RECT* pDst, RECT* pSrc );

	//! Returns the greatest common divisor of A and B.
	inline int gcd( int a, int b )
	{
		if( a < b )
			std::swap( a, b );

		int temp;
		while( b != 0 ) {
			temp = a % b;
			a = b;
			b = temp;
		}

		return a;
	}

protected:
	msw::CriticalSection            m_critSec;                  // critical section for thread safety

	HWND                            m_hwndVideo;
	MonitorArray*                   m_pMonitors;
	AMDDrawMonitorInfo*             m_lpCurrMon;
	UINT                            m_ConnectionGUID;
	RECT                            m_displayRect;
	UINT32                          m_imageWidthInPixels;
	UINT32                          m_imageHeightInPixels;
	UINT32                          m_uiRealDisplayWidth;
	UINT32                          m_uiRealDisplayHeight;
	RECT                            m_rcSrcApp;
	RECT                            m_rcDstApp;

	BOOL                            m_IsShutdown;               // Flag to indicate if Shutdown() method was called.

private:
	long                            m_nRefCount;                // reference count
};

// -----------------------------------------------------------------------------------------------------------------------------------

class __declspec( uuid( "011760FC-FF4C-4B8B-B0CE-823842C812E9" ) ) PresenterDX9 : public Presenter {
public:
	PresenterDX9( void );
	~PresenterDX9( void );

	// IUnknown
	STDMETHODIMP             QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv );

	// IMFVideoDisplayControl
	STDMETHODIMP             GetAspectRatioMode( __RPC__out DWORD* pdwAspectRatioMode ) { return E_NOTIMPL; }
	STDMETHODIMP             GetBorderColor( __RPC__out COLORREF* pClr ) { return E_NOTIMPL; }
	STDMETHODIMP             GetCurrentImage( __RPC__inout BITMAPINFOHEADER* pBih, __RPC__deref_out_ecount_full_opt( *pcbDib ) BYTE** pDib, __RPC__out DWORD* pcbDib, __RPC__inout_opt LONGLONG* pTimestamp ) { return E_NOTIMPL; }
	STDMETHODIMP             GetFullscreen( __RPC__out BOOL* pfFullscreen );
	STDMETHODIMP             GetIdealVideoSize( __RPC__inout_opt SIZE* pszMin, __RPC__inout_opt SIZE* pszMax ) { return E_NOTIMPL; }
	STDMETHODIMP             GetNativeVideoSize( __RPC__inout_opt SIZE* pszVideo, __RPC__inout_opt SIZE* pszARVideo ) { return E_NOTIMPL; }
	STDMETHODIMP             GetRenderingPrefs( __RPC__out DWORD* pdwRenderFlags ) { return E_NOTIMPL; }
	STDMETHODIMP             GetVideoPosition( __RPC__out MFVideoNormalizedRect* pnrcSource, __RPC__out LPRECT prcDest ) { return E_NOTIMPL; }	STDMETHODIMP GetVideoWindow( __RPC__deref_out_opt HWND* phwndVideo ) { return E_NOTIMPL; }
	STDMETHODIMP             RepaintVideo( void ) { return E_NOTIMPL; }
	STDMETHODIMP             SetAspectRatioMode( DWORD dwAspectRatioMode ) { return E_NOTIMPL; }
	STDMETHODIMP             SetBorderColor( COLORREF Clr ) { return E_NOTIMPL; }
	STDMETHODIMP             SetFullscreen( BOOL fFullscreen );
	STDMETHODIMP             SetRenderingPrefs( DWORD dwRenderingPrefs ) { return E_NOTIMPL; }
	STDMETHODIMP             SetVideoPosition( __RPC__in_opt const MFVideoNormalizedRect* pnrcSource, __RPC__in_opt const LPRECT prcDest ) { return E_NOTIMPL; }
	STDMETHODIMP             SetVideoWindow( __RPC__in HWND hwndVideo );

	// IMFGetService
	STDMETHODIMP             GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject );

	// Presenter
	STDMETHODIMP             Initialize( void );
	STDMETHODIMP_( BOOL )    CanProcessNextSample( void ) { return m_bCanProcessNextSample; }
	STDMETHODIMP             Flush( void );
	STDMETHODIMP             GetMonitorRefreshRate( DWORD* pdwRefreshRate );
	STDMETHODIMP             IsMediaTypeSupported( IMFMediaType* pMediaType );
	STDMETHODIMP             PresentFrame( void );
	STDMETHODIMP             ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample = NULL );
	STDMETHODIMP             SetCurrentMediaType( IMFMediaType* pMediaType );
	STDMETHODIMP             Shutdown( void );
	STDMETHODIMP             GetMediaTypeByIndex( DWORD dwIndex, GUID *subType ) const;
	STDMETHODIMP_( DWORD )   GetMediaTypeCount() const { return s_dwNumVideoFormats; }
	STDMETHODIMP             GetFrame( IDirect3DSurface9 **ppFrame );
	STDMETHODIMP             ReturnFrame( IDirect3DSurface9 **ppFrame );

private:
	STDMETHODIMP             CheckDeviceState( BOOL* pbDeviceChanged );
	STDMETHODIMP             CreateDXVA2ManagerAndDevice( D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_HARDWARE );
	STDMETHODIMP             GetVideoDisplayArea( IMFMediaType* pType, MFVideoArea* pArea );
	STDMETHODIMP             ProcessFrameUsingD3D9( IDirect3DSurface9* pTexture2D, UINT dwViewIndex, RECT rcDest, UINT32 unInterlaceMode, IMFSample** ppVideoOutFrame );
	STDMETHODIMP             BlitToShared( IDirect3DSurface9* pSurface );

	_Post_satisfies_( this->m_pSwapChain != NULL )
		STDMETHODIMP             UpdateDX9SwapChain( void );

	UINT                            m_DeviceResetToken;
	D3DDISPLAYMODE                  m_DisplayMode;              // Adapter's display mode.

	IDirect3D9Ex*                   m_pD3D9;
	IDirect3DDevice9Ex*             m_pD3DDevice;
	IDirect3DDeviceManager9*        m_pDeviceManager;
	IMFVideoSampleAllocator*        m_pSampleAllocator;

	IDirectXVideoDecoderService*    m_pDecoderService;
	GUID                            m_DecoderGUID;

	IDirect3DSwapChain9*            m_pSwapChain;

	msw::ThreadSafeDeque<IDirect3DSurface9>*  m_pPool;
	msw::ThreadSafeDeque<IDirect3DSurface9>*  m_pReady;

	// Dynamically link to DirectX.
	HMODULE                         m_D3D9Module;

	BOOL                            m_b3DVideo;
	BOOL                            m_bFullScreenState;
	BOOL                            m_bCanProcessNextSample;

	// Define a function pointer to the Direct3DCreate9Ex function.
	typedef HRESULT( WINAPI *LPDIRECT3DCREATE9EX )( UINT, IDirect3D9Ex ** );

	LPDIRECT3DCREATE9EX             _Direct3DCreate9Ex;

	// Supported video formats.
	static GUID const* const s_pVideoFormats[];
	static const DWORD s_dwNumVideoFormats;
};

// -----------------------------------------------------------------------------------------------------------------------------------

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

	msw::ThreadSafeDeque<ID3D11Texture2D>*    m_pPool;
	msw::ThreadSafeDeque<ID3D11Texture2D>*    m_pReady;

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
