#pragma once

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/ScopedPtr.h"
#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/detail/MonitorArray.h"

#include <d3dcommon.h>

// MF_MT_VIDEO_3D is only defined for Windows 8+, but we need it to check if a video is 3D.
#if (WINVER < _WIN32_WINNT_WIN8)
// {CB5E88CF-7B5B-476b-85AA-1CA5AE187555}        MF_MT_VIDEO_3D                 {UINT32 (BOOL)}
DEFINE_GUID( MF_MT_VIDEO_3D, 0xcb5e88cf, 0x7b5b, 0x476b, 0x85, 0xaa, 0x1c, 0xa5, 0xae, 0x18, 0x75, 0x55 );
#endif

DEFINE_GUID( CLSID_VideoProcessorMFT, 0x88753b26, 0x5b24, 0x49bd, 0xb2, 0xe7, 0xc, 0x44, 0x5c, 0x78, 0xc9, 0x82 );

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
namespace msw {
namespace detail {

//! Abstract base class.
class Presenter : public IMFVideoDisplayControl, public IMFGetService {
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
		SafeDelete( m_pMonitors );
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
	virtual STDMETHODIMP          Initialize( void ) = 0;
	virtual STDMETHODIMP_( BOOL ) CanProcessNextSample( void ) = 0;
	virtual STDMETHODIMP          Flush( void ) = 0;
	virtual STDMETHODIMP          GetMonitorRefreshRate( DWORD* pdwMonitorRefreshRate ) = 0;
	virtual STDMETHODIMP          IsMediaTypeSupported( IMFMediaType* pMediaType ) = 0;
	virtual STDMETHODIMP          PresentFrame( void ) = 0;
	virtual STDMETHODIMP          ProcessFrame( IMFMediaType* pCurrentType, IMFSample* pSample, UINT32* punInterlaceMode, BOOL* pbDeviceChanged, BOOL* pbProcessAgain, IMFSample** ppOutputSample = NULL ) = 0;
	virtual STDMETHODIMP          SetCurrentMediaType( IMFMediaType* pMediaType ) = 0;
	virtual STDMETHODIMP          Shutdown( void ) = 0;
	virtual STDMETHODIMP_( BOOL ) IsDX9() const = 0;
	virtual STDMETHODIMP_( BOOL ) IsDX11() const = 0;

protected:
	STDMETHODIMP_( BOOL )         CheckEmptyRect( RECT* pDst );
	STDMETHODIMP                  CheckShutdown( void ) const;

	STDMETHODIMP SetMonitor( UINT adapterID )
	{
		HRESULT hr = S_OK;
		DWORD dwMatchID = 0;

		ScopedCriticalSection lock( m_critSec );

		do {
			hr = m_pMonitors->MatchGUID( adapterID, &dwMatchID );
			BREAK_ON_FAIL( hr );

			if( hr == S_FALSE ) {
				hr = E_INVALIDARG;
				break;
			}

			m_lpCurrMon = &( *m_pMonitors )[dwMatchID];
			m_ConnectionGUID = adapterID;
		} while( FALSE );

		return hr;
	}

	STDMETHODIMP SetVideoMonitor( HWND hwndVideo )
	{
		HRESULT hr = S_OK;
		AMDDrawMonitorInfo* pMonInfo = NULL;
		HMONITOR hMon = NULL;

		if( !m_pMonitors ) {
			return E_UNEXPECTED;
		}

		hMon = MonitorFromWindow( hwndVideo, MONITOR_DEFAULTTONULL );

		do {
			if( NULL != hMon ) {
				m_pMonitors->TerminateDisplaySystem();
				m_lpCurrMon = NULL;

				hr = m_pMonitors->InitializeDisplaySystem( hwndVideo );
				BREAK_ON_FAIL( hr );

				pMonInfo = m_pMonitors->FindMonitor( hMon );
				if( NULL != pMonInfo && pMonInfo->uDevID != m_ConnectionGUID ) {
					hr = SetMonitor( pMonInfo->uDevID );
					BREAK_ON_FAIL( hr );

					hr = S_FALSE; // Signal lost device.
				}
			}
			else {
				hr = E_POINTER;
				BREAK_ON_FAIL( hr );
			}
		} while( FALSE );

		return hr;
	}

	STDMETHODIMP_( VOID ) ReduceToLowestTerms( int NumeratorIn, int DenominatorIn, int * pNumeratorOut, int * pDenominatorOut );
	STDMETHODIMP_( VOID ) LetterBoxDstRect( LPRECT lprcLBDst, const RECT & rcSrc, const RECT & rcDst );
	STDMETHODIMP_( VOID ) PixelAspectToPictureAspect( int Width, int Height, int PixelAspectX, int PixelAspectY, int * pPictureAspectX, int * pPictureAspectY );
	STDMETHODIMP_( VOID ) AspectRatioCorrectSize( LPSIZE lpSizeImage, const SIZE & sizeAr, const SIZE & sizeOrig, BOOL ScaleXorY );
	STDMETHODIMP_( VOID ) UpdateRectangles( RECT* pDst, RECT* pSrc );

protected:
	CriticalSection                 m_critSec;                  // critical section for thread safety

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

} // namespace detail
} // namespace msw
} // namespace cinder
