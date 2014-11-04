// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
// PARTICULAR PURPOSE.
//
// Copyright (c) Microsoft Corporation. All rights reserved.

#include "cinder/evr/DirectShowVideo.h"

#if defined(CINDER_MSW)

#include <mfidl.h>

namespace cinder {
namespace msw {
namespace video {

HRESULT InitializeEVR( IBaseFilter *pEVR, HWND hwnd, IMFVideoDisplayControl ** ppWc );
HRESULT InitWindowlessVMR9( IBaseFilter *pVMR, HWND hwnd, IVMRWindowlessControl9 ** ppWc );
HRESULT InitWindowlessVMR( IBaseFilter *pVMR, HWND hwnd, IVMRWindowlessControl** ppWc );
HRESULT FindConnectedPin( IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin );

/// VMR-7 Wrapper

RendererVMR7::RendererVMR7() : m_pWindowless( NULL )
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
		*ppWC = pWC;
		( *ppWC )->AddRef();
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
		*ppWC = pWC;
		( *ppWC )->AddRef();
	} while( false );

	return hr;
}


/// EVR Wrapper

RendererEVR::RendererEVR() : m_pEVR( NULL ), m_pVideoDisplay( NULL )
{

}

RendererEVR::~RendererEVR()
{
	SafeRelease( m_pEVR );
	SafeRelease( m_pVideoDisplay );
}

BOOL RendererEVR::HasVideo() const
{
	return ( m_pVideoDisplay != NULL );
}

HRESULT RendererEVR::AddToGraph( IGraphBuilder *pGraph, HWND hwnd )
{
	HRESULT hr = S_OK;

	do {
		ScopedComPtr<IBaseFilter> pEVR;
		hr = AddFilterByCLSID( pGraph, CLSID_EnhancedVideoRenderer,
							   &pEVR, L"EVR" );
		BREAK_ON_FAIL( hr );

		hr = InitializeEVR( pEVR, hwnd, &m_pVideoDisplay );
		BREAK_ON_FAIL( hr );

		// Note: Because IMFVideoDisplayControl is a service interface,
		// you cannot QI the pointer to get back the IBaseFilter pointer.
		// Therefore, we need to cache the IBaseFilter pointer.

		m_pEVR = pEVR;
		m_pEVR->AddRef();
	} while( false );

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
		SafeRelease( m_pEVR );
		SafeRelease( m_pVideoDisplay );
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


// Initialize the EVR filter. 

HRESULT InitializeEVR(
	IBaseFilter *pEVR,              // Pointer to the EVR
	HWND hwnd,                      // Clipping window
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

		// Return the IMFVideoDisplayControl pointer to the caller.
		*ppDisplayControl = pDisplay;
		( *ppDisplayControl )->AddRef();
	} while( false );

	return hr;
}

// Helper functions.

HRESULT RemoveUnconnectedRenderer( IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved )
{
	*pbRemoved = FALSE;

	// Look for a connected input pin on the renderer.

	IPin *pPin = NULL;
	HRESULT hr = FindConnectedPin( pRenderer, PINDIR_INPUT, &pPin );
	SafeRelease( pPin );

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
	IPin *pTmp = NULL;
	HRESULT hr = pPin->ConnectedTo( &pTmp );
	if( SUCCEEDED( hr ) ) {
		*pResult = TRUE;
	}
	else if( hr == VFW_E_NOT_CONNECTED ) {
		// The pin is not connected. This is not an error for our purposes.
		*pResult = FALSE;
		hr = S_OK;
	}

	SafeRelease( pTmp );
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

HRESULT FindConnectedPin( IBaseFilter *pFilter, PIN_DIRECTION PinDir,
						  IPin **ppPin )
{
	*ppPin = NULL;

	IEnumPins *pEnum = NULL;
	IPin *pPin = NULL;

	HRESULT hr = pFilter->EnumPins( &pEnum );
	if( FAILED( hr ) ) {
		return hr;
	}

	BOOL bFound = FALSE;
	while( S_OK == pEnum->Next( 1, &pPin, NULL ) ) {
		BOOL bIsConnected;
		hr = IsPinConnected( pPin, &bIsConnected );
		if( SUCCEEDED( hr ) ) {
			if( bIsConnected ) {
				hr = IsPinDirection( pPin, PinDir, &bFound );
			}
		}

		if( FAILED( hr ) ) {
			pPin->Release();
			break;
		}
		if( bFound ) {
			*ppPin = pPin;
			break;
		}
		pPin->Release();
	}

	pEnum->Release();

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

		*ppF = pFilter;
		( *ppF )->AddRef();
	} while( false );

	return hr;
}

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW