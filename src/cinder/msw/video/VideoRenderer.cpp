// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#include "cinder/msw/video/VideoRenderer.h"
#include "cinder/msw/video/EVRCustomPresenter.h"
#include "cinder/Log.h"

#if defined(CINDER_MSW)

#include <mfidl.h>

namespace cinder {
namespace msw {
namespace video {

/// VMR-7 Wrapper

RendererVMR7::RendererVMR7()
	: m_pWindowless( NULL )
{

}

RendererVMR7::~RendererVMR7()
{
	SafeRelease( m_pWindowless );
}

BOOL RendererVMR7::HasVideo() const
{
	return ( m_pWindowless != NULL );
}

HRESULT RendererVMR7::AddToGraph( IGraphBuilder *pGraph, HWND hwnd )
{
	ScopedComPtr<IBaseFilter> pVMR;
	HRESULT hr = AddFilterByCLSID( pGraph, CLSID_VideoMixingRenderer,
								   &pVMR, L"VMR-7" );

	if( SUCCEEDED( hr ) ) {
		// Set windowless mode on the VMR. This must be done before the VMR
		// is connected.
		hr = InitWindowlessVMR( pVMR, hwnd, &m_pWindowless );
	}

	return hr;
}

HRESULT RendererVMR7::FinalizeGraph( IGraphBuilder *pGraph )
{
	HRESULT hr = S_OK;

	if( m_pWindowless != NULL ) {
		do {
			ScopedComPtr<IBaseFilter> pFilter;
			hr = m_pWindowless->QueryInterface( IID_PPV_ARGS( &pFilter ) );
			BREAK_ON_FAIL( hr );

			BOOL bRemoved;
			hr = RemoveUnconnectedRenderer( pGraph, pFilter, &bRemoved );

			// If we removed the VMR, then we also need to release our 
			// pointer to the VMR's windowless control interface.
			if( bRemoved ) {
				SafeRelease( m_pWindowless );
			}
		} while( false );
	}

	return hr;
}

HRESULT RendererVMR7::UpdateVideoWindow( HWND hwnd, const LPRECT prc )
{
	if( m_pWindowless == NULL ) {
		return S_OK; // no-op
	}

	if( prc ) {
		return m_pWindowless->SetVideoPosition( NULL, prc );
	}
	else {
		RECT rc;
		GetClientRect( hwnd, &rc );
		return m_pWindowless->SetVideoPosition( NULL, &rc );
	}
}

HRESULT RendererVMR7::Repaint( HWND hwnd, HDC hdc )
{
	if( m_pWindowless ) {
		return m_pWindowless->RepaintVideo( hwnd, hdc );
	}
	else {
		return S_OK;
	}
}

HRESULT RendererVMR7::DisplayModeChanged()
{
	if( m_pWindowless ) {
		return m_pWindowless->DisplayModeChanged();
	}
	else {
		return S_OK;
	}
}

HRESULT RendererVMR7::GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const
{
	assert( lpWidth != nullptr );
	assert( lpHeight != nullptr );

	if( m_pWindowless ) {
		LONG ARWidth, ARHeight;
		return m_pWindowless->GetNativeVideoSize( lpWidth, lpHeight, &ARWidth, &ARHeight );
	}
	else {
		return E_POINTER;
	}
}


// Initialize the VMR-7 for windowless mode. 

HRESULT InitWindowlessVMR(
	IBaseFilter *pVMR,              // Pointer to the VMR
	HWND hwnd,                      // Clipping window
	IVMRWindowlessControl** ppWC    // Receives a pointer to the VMR.
	)
{
	HRESULT hr = S_OK;

	do {
		// Set the rendering mode.
		ScopedComPtr<IVMRFilterConfig> pConfig;
		hr = pVMR->QueryInterface( IID_PPV_ARGS( &pConfig ) );
		BREAK_ON_FAIL( hr );

		hr = pConfig->SetRenderingMode( VMRMode_Windowless );
		BREAK_ON_FAIL( hr );

		// Query for the windowless control interface.
		ScopedComPtr<IVMRWindowlessControl> pWC;
		hr = pVMR->QueryInterface( IID_PPV_ARGS( &pWC ) );
		BREAK_ON_FAIL( hr );

		// Set the clipping window.
		hr = pWC->SetVideoClippingWindow( hwnd );
		BREAK_ON_FAIL( hr );

		// Preserve aspect ratio by letter-boxing
		hr = pWC->SetAspectRatioMode( VMR_ARMODE_LETTER_BOX );
		BREAK_ON_FAIL( hr );

		// Return the IVMRWindowlessControl pointer to the caller.
		*ppWC = pWC.Detach();
	} while( false );

	return hr;
}


/// VMR-9 Wrapper

RendererVMR9::RendererVMR9() : m_pWindowless( NULL )
{

}

BOOL RendererVMR9::HasVideo() const
{
	return ( m_pWindowless != NULL );
}

RendererVMR9::~RendererVMR9()
{
	SafeRelease( m_pWindowless );
}

HRESULT RendererVMR9::AddToGraph( IGraphBuilder *pGraph, HWND hwnd )
{
	ScopedComPtr<IBaseFilter> pVMR;
	HRESULT hr = AddFilterByCLSID( pGraph, CLSID_VideoMixingRenderer9,
								   &pVMR, L"VMR-9" );
	if( SUCCEEDED( hr ) ) {
		// Set windowless mode on the VMR. This must be done before the VMR 
		// is connected.
		hr = InitWindowlessVMR9( pVMR, hwnd, &m_pWindowless );
	}

	return hr;
}

HRESULT RendererVMR9::FinalizeGraph( IGraphBuilder *pGraph )
{
	if( m_pWindowless == NULL ) {
		return S_OK;
	}

	HRESULT hr = S_OK;

	do {
		ScopedComPtr<IBaseFilter> pFilter;
		hr = m_pWindowless->QueryInterface( IID_PPV_ARGS( &pFilter ) );
		BREAK_ON_FAIL( hr );

		BOOL bRemoved;
		hr = RemoveUnconnectedRenderer( pGraph, pFilter, &bRemoved );

		// If we removed the VMR, then we also need to release our 
		// pointer to the VMR's windowless control interface.
		if( bRemoved ) {
			SafeRelease( m_pWindowless );
		}
	} while( false );

	return hr;
}


HRESULT RendererVMR9::UpdateVideoWindow( HWND hwnd, const LPRECT prc )
{
	if( m_pWindowless == NULL ) {
		return S_OK; // no-op
	}

	if( prc ) {
		return m_pWindowless->SetVideoPosition( NULL, prc );
	}
	else {

		RECT rc;
		GetClientRect( hwnd, &rc );
		return m_pWindowless->SetVideoPosition( NULL, &rc );
	}
}

HRESULT RendererVMR9::Repaint( HWND hwnd, HDC hdc )
{
	if( m_pWindowless ) {
		return m_pWindowless->RepaintVideo( hwnd, hdc );
	}
	else {
		return S_OK;
	}
}

HRESULT RendererVMR9::DisplayModeChanged()
{
	if( m_pWindowless ) {
		return m_pWindowless->DisplayModeChanged();
	}
	else {
		return S_OK;
	}
}

HRESULT RendererVMR9::GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const
{
	assert( lpWidth != nullptr );
	assert( lpHeight != nullptr );

	if( m_pWindowless ) {
		LONG ARWidth, ARHeight;
		return m_pWindowless->GetNativeVideoSize( lpWidth, lpHeight, &ARWidth, &ARHeight );
	}
	else {
		return E_POINTER;
	}
}


// Initialize the VMR-9 for windowless mode. 

HRESULT InitWindowlessVMR9(
	IBaseFilter *pVMR,              // Pointer to the VMR
	HWND hwnd,                      // Clipping window
	IVMRWindowlessControl9** ppWC   // Receives a pointer to the VMR.
	)
{
	HRESULT hr = S_OK;

	do {
		// Set the rendering mode.
		ScopedComPtr<IVMRFilterConfig9> pConfig;
		hr = pVMR->QueryInterface( IID_PPV_ARGS( &pConfig ) );
		BREAK_ON_FAIL( hr );

		hr = pConfig->SetRenderingMode( VMR9Mode_Windowless );
		BREAK_ON_FAIL( hr );

		// Query for the windowless control interface.
		ScopedComPtr<IVMRWindowlessControl9> pWC;
		hr = pVMR->QueryInterface( IID_PPV_ARGS( &pWC ) );
		BREAK_ON_FAIL( hr );

		// Set the clipping window.
		hr = pWC->SetVideoClippingWindow( hwnd );
		BREAK_ON_FAIL( hr );

		// Preserve aspect ratio by letter-boxing
		hr = pWC->SetAspectRatioMode( VMR9ARMode_LetterBox );
		BREAK_ON_FAIL( hr );

		// Return the IVMRWindowlessControl pointer to the caller.
		*ppWC = pWC.Detach();
	} while( false );

	return hr;
}


/// EVR Wrapper

RendererEVR::RendererEVR()
	: m_pEVR( NULL ), m_pVideoDisplay( NULL ), m_pPresenter( NULL )
{
	HRESULT hr = S_OK;

	// Create custom EVR presenter.
	m_pPresenter = new EVRCustomPresenter( hr );
	m_pPresenter->AddRef();
}

RendererEVR::~RendererEVR()
{
	SafeRelease( m_pVideoDisplay );
	SafeRelease( m_pEVR );
	SafeRelease( m_pPresenter );
}

BOOL RendererEVR::HasVideo() const
{
	return ( m_pVideoDisplay != NULL );
}

HRESULT RendererEVR::AddToGraph( IGraphBuilder *pGraph, HWND hwnd )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( m_pPresenter, E_POINTER );
		BREAK_IF_FALSE( m_pEVR == NULL, E_POINTER );

		hr = m_pPresenter->SetVideoWindow( hwnd );
		BREAK_ON_FAIL( hr );

		// Note: Because IMFVideoDisplayControl is a service interface,
		// you cannot QI the pointer to get back the IBaseFilter pointer.
		// Therefore, we need to cache the IBaseFilter pointer.

		ScopedComPtr<IBaseFilter> pEVR;
		hr = AddFilterByCLSID( pGraph, CLSID_EnhancedVideoRenderer,
							   &pEVR, L"EVR" );
		BREAK_ON_FAIL( hr );

		hr = InitializeEVR( pEVR, hwnd, m_pPresenter, &m_pVideoDisplay );
		BREAK_ON_FAIL( hr );

		m_pEVR = pEVR.Detach();
	} while( false );

	if( FAILED( hr ) ) {
		SafeRelease( m_pVideoDisplay );
		SafeRelease( m_pEVR );
	}

	return hr;
}

HRESULT RendererEVR::FinalizeGraph( IGraphBuilder *pGraph )
{
	if( m_pEVR == NULL ) {
		return S_OK;
	}

	BOOL bRemoved;
	HRESULT hr = RemoveUnconnectedRenderer( pGraph, m_pEVR, &bRemoved );
	if( bRemoved ) {
		SafeRelease( m_pVideoDisplay );
		SafeRelease( m_pEVR );
	}
	return hr;
}

HRESULT RendererEVR::UpdateVideoWindow( HWND hwnd, const LPRECT prc )
{
	if( m_pVideoDisplay == NULL ) {
		return S_OK; // no-op
	}

	if( prc ) {
		return m_pVideoDisplay->SetVideoPosition( NULL, prc );
	}
	else {

		RECT rc;
		GetClientRect( hwnd, &rc );
		return m_pVideoDisplay->SetVideoPosition( NULL, &rc );
	}
}

HRESULT RendererEVR::Repaint( HWND hwnd, HDC hdc )
{
	if( m_pVideoDisplay ) {
		return m_pVideoDisplay->RepaintVideo();
	}
	else {
		return S_OK;
	}
}

HRESULT RendererEVR::DisplayModeChanged()
{
	// The EVR does not require any action in response to WM_DISPLAYCHANGE.
	return S_OK;
}

HRESULT RendererEVR::GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const
{
	assert( lpWidth != nullptr );
	assert( lpHeight != nullptr );

	HRESULT hr = S_OK;

	if( m_pPresenter ) {
		SIZE szVideo, szARVideo;
		hr = m_pPresenter->GetNativeVideoSize( &szVideo, &szARVideo );

		if( SUCCEEDED( hr ) ) {
			*lpWidth = szVideo.cx;
			*lpHeight = szVideo.cy;
		}

		return hr;
	}
	else {
		return E_POINTER;
	}
}


/// RendererSampleGrabber

RendererSampleGrabber::RendererSampleGrabber()
	: m_pGrabberFilter( NULL ), m_pGrabber( NULL ), m_pNullRenderer( NULL )
{
	m_pCallBack = new SampleGrabberCallback();
}

RendererSampleGrabber::~RendererSampleGrabber()
{
	SafeRelease( m_pNullRenderer );
	SafeRelease( m_pGrabber );
	SafeRelease( m_pGrabberFilter );

	SafeDelete( m_pCallBack );
}

BOOL RendererSampleGrabber::HasVideo() const
{
	return ( m_pGrabber != NULL );
}

HRESULT RendererSampleGrabber::AddToGraph( IGraphBuilder *pGraph, HWND hwnd )
{
	HRESULT hr = S_OK;

	SafeRelease( m_pNullRenderer );
	SafeRelease( m_pGrabber );
	SafeRelease( m_pGrabberFilter );

	do {
		ScopedComPtr<IBaseFilter> pGrabberFilter;
		hr = AddFilterByCLSID( pGraph, CLSID_SampleGrabber,
							   &pGrabberFilter, L"SampleGrabber" );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<ISampleGrabber> pGrabber;
		hr = pGrabberFilter.QueryInterface( IID_ISampleGrabber, (void**) &pGrabber );
		BREAK_ON_FAIL( hr );

		// Set media type.
		AM_MEDIA_TYPE mt;
		ZeroMemory( &mt, sizeof( AM_MEDIA_TYPE ) );

		mt.majortype = MEDIATYPE_Video;
		mt.subtype = MEDIASUBTYPE_RGB24;
		mt.formattype = FORMAT_VideoInfo;

		hr = pGrabber->SetMediaType( &mt );
		BREAK_ON_FAIL( hr );

		// Setup the grabber.
		hr = pGrabber->SetOneShot( FALSE );
		hr = pGrabber->SetBufferSamples( TRUE );

		hr = pGrabber->SetCallback( m_pCallBack, 0 );
		BREAK_ON_FAIL( hr );

		// Add null renderer
		ScopedComPtr<IBaseFilter> pNullRenderer;
		hr = AddFilterByCLSID( pGraph, CLSID_NullRenderer,
							   &pNullRenderer, L"NullRenderer" );
		BREAK_ON_FAIL( hr );

		// If everything worked out, we're ready to take ownership of the pointers.
		m_pNullRenderer = pNullRenderer.Detach();
		m_pGrabberFilter = pGrabberFilter.Detach();
		m_pGrabber = pGrabber.Detach();
	} while( false );

	return hr;
}

HRESULT RendererSampleGrabber::FinalizeGraph( IGraphBuilder *pGraph )
{
	if( m_pGrabber == NULL ) {
		return S_OK;
	}

	BOOL bRemoved;
	HRESULT hr = RemoveUnconnectedRenderer( pGraph, m_pNullRenderer, &bRemoved );
	if( bRemoved ) {
		SafeRelease( m_pNullRenderer );
	}

	hr = RemoveUnconnectedRenderer( pGraph, m_pGrabberFilter, &bRemoved );
	if( bRemoved ) {
		SafeRelease( m_pGrabber );
		SafeRelease( m_pGrabberFilter );
	}

	return hr;
}

HRESULT RendererSampleGrabber::ConnectFilters( IGraphBuilder *pGraph, IPin *pPin )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( m_pGrabberFilter, E_POINTER );
		BREAK_ON_NULL( m_pNullRenderer, E_POINTER );

		// Connect source to Sample Grabber
		hr = video::ConnectFilters( pGraph, pPin, m_pGrabberFilter );
		BREAK_ON_FAIL( hr );

		// Connect sample grabber to null renderer
		hr = video::ConnectFilters( pGraph, m_pGrabberFilter, m_pNullRenderer );
		BREAK_ON_FAIL( hr );
	} while( false );

	return hr;
}

HRESULT RendererSampleGrabber::UpdateVideoWindow( HWND hwnd, const LPRECT prc )
{
	return S_OK;
}

HRESULT RendererSampleGrabber::Repaint( HWND hwnd, HDC hdc )
{
	return S_OK;
}

HRESULT RendererSampleGrabber::DisplayModeChanged()
{
	return S_OK;
}

HRESULT RendererSampleGrabber::GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const
{
	assert( lpWidth != nullptr );
	assert( lpHeight != nullptr );

	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( m_pCallBack, E_POINTER );
		BREAK_ON_NULL( m_pGrabber, E_POINTER );

		AM_MEDIA_TYPE mediaType;
		hr = m_pGrabber->GetConnectedMediaType( &mediaType );
		BREAK_ON_FAIL( hr );

		if( mediaType.formattype == FORMAT_VideoInfo ) {
			if( mediaType.cbFormat >= sizeof( VIDEOINFOHEADER ) ) {
				VIDEOINFOHEADER *pHeader = reinterpret_cast<VIDEOINFOHEADER*>( mediaType.pbFormat );
				*lpWidth = pHeader->bmiHeader.biWidth;
				*lpHeight = pHeader->bmiHeader.biHeight;

				// Use this opportunity to setup the grabber buffer.
				m_pCallBack->SetupBuffer( *lpWidth, *lpHeight, 3 ); // RGB
				break;
			}
		}

		hr = E_FAIL;
	} while( false );

	return hr;
}


// Initialize the EVR filter. 

HRESULT InitializeEVR(
	IBaseFilter *pEVR,              // Pointer to the EVR
	HWND hwnd,                      // Clipping window
	IMFVideoPresenter* pRenderer,   // Custom presenter, or NULL for default
	IMFVideoDisplayControl** ppDisplayControl
	)
{
	HRESULT hr = S_OK;

	do {
		ScopedComPtr<IMFGetService> pGS;
		hr = pEVR->QueryInterface( IID_PPV_ARGS( &pGS ) );
		BREAK_ON_FAIL( hr );

		ScopedComPtr<IMFVideoDisplayControl> pDisplay;
		hr = pGS->GetService( MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pDisplay ) );
		BREAK_ON_FAIL( hr );

		// Set the clipping window.
		hr = pDisplay->SetVideoWindow( hwnd );
		BREAK_ON_FAIL( hr );

		// Preserve aspect ratio by letter-boxing
		hr = pDisplay->SetAspectRatioMode( MFVideoARMode_PreservePicture );
		BREAK_ON_FAIL( hr );

		if( pRenderer != NULL ) {
			ScopedComPtr<IMFVideoRenderer> pVideoRenderer;
			hr = pEVR->QueryInterface( __uuidof( IMFVideoRenderer ), (void**) &pVideoRenderer );
			BREAK_ON_FAIL( hr );

			hr = pVideoRenderer->InitializeRenderer( NULL, pRenderer );
			BREAK_ON_FAIL( hr );
		}

		// Return the IMFVideoDisplayControl pointer to the caller.
		*ppDisplayControl = pDisplay.Detach();
	} while( false );

	return hr;
}

// Helper functions.

HRESULT RemoveUnconnectedRenderer( IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved )
{
	*pbRemoved = FALSE;

	// Look for a connected input pin on the renderer.

	ScopedComPtr<IPin> pPin;
	HRESULT hr = FindConnectedPin( pRenderer, PINDIR_INPUT, &pPin );

	// If this function succeeds, the renderer is connected, so we don't remove it.
	// If it fails, it means the renderer is not connected to anything, so
	// we remove it.

	if( FAILED( hr ) ) {
		hr = pGraph->RemoveFilter( pRenderer );
		*pbRemoved = TRUE;
	}

	return hr;
}

HRESULT IsPinConnected( IPin *pPin, BOOL *pResult )
{
	ScopedComPtr<IPin> pTmp;
	HRESULT hr = pPin->ConnectedTo( &pTmp );
	if( SUCCEEDED( hr ) ) {
		*pResult = TRUE;
	}
	else if( hr == VFW_E_NOT_CONNECTED ) {
		// The pin is not connected. This is not an error for our purposes.
		*pResult = FALSE;
		hr = S_OK;
	}

	return hr;
}

HRESULT IsPinDirection( IPin *pPin, PIN_DIRECTION dir, BOOL *pResult )
{
	PIN_DIRECTION pinDir;
	HRESULT hr = pPin->QueryDirection( &pinDir );
	if( SUCCEEDED( hr ) ) {
		*pResult = ( pinDir == dir );
	}
	return hr;
}

HRESULT FindUnconnectedPin( IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin )
{
	HRESULT hr = S_OK;
	BOOL bFound = FALSE;

	do {
		ScopedComPtr<IEnumPins> pEnum;
		ScopedComPtr<IPin> pPin;

		hr = pFilter->EnumPins( &pEnum );
		BREAK_ON_FAIL( hr );

		while( S_OK == pEnum->Next( 1, &pPin, NULL ) ) {
			hr = MatchPin( pPin, PinDir, FALSE, &bFound );
			BREAK_ON_FAIL( hr );

			if( bFound ) {
				*ppPin = pPin.Detach();
				break;
			}

			// Re-use pPin on next iteration.
			pPin.Release();
		}
	} while( false );

	if( !bFound ) {
		hr = VFW_E_NOT_FOUND;
	}

	return hr;
}

HRESULT FindConnectedPin( IBaseFilter *pFilter, PIN_DIRECTION PinDir,
						  IPin **ppPin )
{
	HRESULT hr = S_OK;
	BOOL bFound = FALSE;

	*ppPin = NULL;

	do {
		ScopedComPtr<IEnumPins> pEnum;
		ScopedComPtr<IPin> pPin;

		hr = pFilter->EnumPins( &pEnum );
		BREAK_ON_FAIL( hr );

		while( S_OK == pEnum->Next( 1, &pPin, NULL ) ) {
			BOOL bIsConnected;
			hr = IsPinConnected( pPin, &bIsConnected );
			if( SUCCEEDED( hr ) ) {
				if( bIsConnected ) {
					hr = IsPinDirection( pPin, PinDir, &bFound );
				}
			}

			BREAK_ON_FAIL( hr );

			if( bFound ) {
				*ppPin = pPin.Detach();
				break;
			}

			// Re-use pPin on next iteration.
			pPin.Release();
		}
	} while( false );

	if( !bFound ) {
		hr = VFW_E_NOT_FOUND;
	}

	return hr;
}

HRESULT AddFilterByCLSID( IGraphBuilder *pGraph, REFGUID clsid,
						  IBaseFilter **ppF, LPCWSTR wszName )
{
	*ppF = 0;

	HRESULT hr = S_OK;

	do {
		ScopedComPtr<IBaseFilter> pFilter;
		hr = CoCreateInstance( clsid, NULL, CLSCTX_INPROC_SERVER,
							   IID_PPV_ARGS( &pFilter ) );
		BREAK_ON_FAIL( hr );

		hr = pGraph->AddFilter( pFilter, wszName );
		BREAK_ON_FAIL( hr );

		std::wstring wstr( wszName );
		CI_LOG_V( "Added filter: " << toUtf8String( wstr ).c_str() );

		*ppF = pFilter.Detach();
	} while( false );

	return hr;
}

HRESULT MatchPin( IPin *pPin, PIN_DIRECTION direction, BOOL bShouldBeConnected, BOOL *pResult )
{
	assert( pResult != NULL );

	BOOL bMatch = FALSE;
	BOOL bIsConnected = FALSE;

	HRESULT hr = IsPinConnected( pPin, &bIsConnected );
	if( SUCCEEDED( hr ) ) {
		if( bIsConnected == bShouldBeConnected ) {
			hr = IsPinDirection( pPin, direction, &bMatch );
		}
	}

	if( SUCCEEDED( hr ) ) {
		*pResult = bMatch;
	}

	return hr;
}

HRESULT ConnectFilters( IGraphBuilder *pGraph, IPin *pOut, IBaseFilter *pDest )
{
	IPin *pIn = NULL;

	// Find an input pin on the downstream filter.
	HRESULT hr = FindUnconnectedPin( pDest, PINDIR_INPUT, &pIn );
	if( SUCCEEDED( hr ) ) {
		// Try to connect them.
		hr = pGraph->Connect( pOut, pIn );
		pIn->Release();
	}

	return hr;
}

HRESULT ConnectFilters( IGraphBuilder *pGraph, IBaseFilter *pSrc, IPin *pIn )
{
	IPin *pOut = NULL;

	// Find an output pin on the upstream filter.
	HRESULT hr = FindUnconnectedPin( pSrc, PINDIR_OUTPUT, &pOut );
	if( SUCCEEDED( hr ) ) {
		// Try to connect them.
		hr = pGraph->Connect( pOut, pIn );
		pOut->Release();
	}

	return hr;
}

HRESULT ConnectFilters( IGraphBuilder *pGraph, IBaseFilter *pSrc, IBaseFilter *pDest )
{
	IPin *pOut = NULL;

	// Find an output pin on the first filter.
	HRESULT hr = FindUnconnectedPin( pSrc, PINDIR_OUTPUT, &pOut );
	if( SUCCEEDED( hr ) ) {
		hr = ConnectFilters( pGraph, pOut, pDest );
		pOut->Release();
	}

	return hr;
}

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW