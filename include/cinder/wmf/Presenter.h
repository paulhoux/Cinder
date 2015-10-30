#pragma once

#include "cinder/wmf/MediaFoundation.h"
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/wmf/MonitorArray.h"

// MF_MT_VIDEO_3D is only defined for Windows 8+, but we need it to check if a video is 3D.
#if (WINVER < _WIN32_WINNT_WIN8)
// {CB5E88CF-7B5B-476b-85AA-1CA5AE187555}        MF_MT_VIDEO_3D                 {UINT32 (BOOL)}
DEFINE_GUID( MF_MT_VIDEO_3D, 0xcb5e88cf, 0x7b5b, 0x476b, 0x85, 0xaa, 0x1c, 0xa5, 0xae, 0x18, 0x75, 0x55 );
#endif

DEFINE_GUID( CLSID_VideoProcessorMFT, 0x88753b26, 0x5b24, 0x49bd, 0xb2, 0xe7, 0xc, 0x44, 0x5c, 0x78, 0xc9, 0x82 );

// We store the shared texture handle in a private data field of the texture using this GUID.
// {AD243275-BD3A-473D-8CA5-B97A9C4E927D}
static const GUID GUID_SharedHandle = { 0xad243275, 0xbd3a, 0x473d,{ 0x8c, 0xa5, 0xb9, 0x7a, 0x9c, 0x4e, 0x92, 0x7d } };

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

} // namespace wmf
} // namespace cinder

#endif ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )
