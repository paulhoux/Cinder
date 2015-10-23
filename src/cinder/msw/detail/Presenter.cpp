#pragma once

#include "cinder/msw/detail/Presenter.h"

namespace cinder {
namespace msw {
namespace detail {

BOOL Presenter::CheckEmptyRect( RECT* pDst )
{
	GetClientRect( m_hwndVideo, pDst );

	return IsRectEmpty( pDst );
}

HRESULT Presenter::CheckShutdown( void ) const
{
	if( m_IsShutdown ) {
		return MF_E_SHUTDOWN;
	}
	else {
		return S_OK;
	}
}

HRESULT Presenter::SetMonitor( UINT adapterID )
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

HRESULT Presenter::SetVideoMonitor( HWND hwndVideo )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( m_pMonitors, E_UNEXPECTED );

		HMONITOR hMon = MonitorFromWindow( hwndVideo, MONITOR_DEFAULTTONULL );
		BREAK_ON_NULL( hMon, E_POINTER );

		if( NULL != m_lpCurrMon && m_lpCurrMon->hMon == hMon )
			return S_OK;

		m_pMonitors->TerminateDisplaySystem();
		m_lpCurrMon = NULL;

		hr = m_pMonitors->InitializeDisplaySystem( hwndVideo );
		BREAK_ON_FAIL( hr );

		AMDDrawMonitorInfo* pMonInfo = m_pMonitors->FindMonitor( hMon );
		if( NULL != pMonInfo && pMonInfo->uDevID != m_ConnectionGUID ) {
			hr = SetMonitor( pMonInfo->uDevID );
			BREAK_ON_FAIL( hr );

			hr = S_FALSE; // Signal lost device.
		}
	} while( FALSE );

	return hr;
}

//+-------------------------------------------------------------------------
//
//  Function:   ReduceToLowestTerms
//
//  Synopsis:   reduces a numerator and denominator pair to their lowest terms
//
//--------------------------------------------------------------------------

void Presenter::ReduceToLowestTerms(
	int NumeratorIn,
	int DenominatorIn,
	int* pNumeratorOut,
	int* pDenominatorOut
	)
{
	int GCD = gcd( NumeratorIn, DenominatorIn );

	*pNumeratorOut = NumeratorIn / GCD;
	*pDenominatorOut = DenominatorIn / GCD;
}

//+-------------------------------------------------------------------------
//
//  Function:   LetterBoxDstRectPixelAspectToPictureAspect
//
//  Synopsis:
//
// Takes a src rectangle and constructs the largest possible destination
// rectangle within the specifed destination rectangle such that
// the video maintains its current shape.
//
// This function assumes that pels are the same shape within both the src
// and dst rectangles.
//
//--------------------------------------------------------------------------

void Presenter::LetterBoxDstRect(
	LPRECT lprcLBDst,     // output letterboxed rectangle
	const RECT& rcSrc,    // input source rectangle
	const RECT& rcDst     // input destination rectangle
	)
{
	// figure out src/dest scale ratios
	int iSrcWidth = rcSrc.right - rcSrc.left;
	int iSrcHeight = rcSrc.bottom - rcSrc.top;

	int iDstWidth = rcDst.right - rcDst.left;
	int iDstHeight = rcDst.bottom - rcDst.top;

	int iDstLBWidth = 0;
	int iDstLBHeight = 0;

	//
	// work out if we are Column or Row letter boxing
	//

	if( MulDiv( iSrcWidth, iDstHeight, iSrcHeight ) <= iDstWidth ) {
		//
		// column letter boxing - we add border color bars to the
		// left and right of the video image to fill the destination
		// rectangle.
		//
		iDstLBWidth = MulDiv( iDstHeight, iSrcWidth, iSrcHeight );
		iDstLBHeight = iDstHeight;
	}
	else {
		//
		// row letter boxing - we add border color bars to the top
		// and bottom of the video image to fill the destination
		// rectangle
		//
		iDstLBWidth = iDstWidth;
		iDstLBHeight = MulDiv( iDstWidth, iSrcHeight, iSrcWidth );
	}

	//
	// now create a centered LB rectangle within the current destination rect
	//
	lprcLBDst->left = rcDst.left + ( ( iDstWidth - iDstLBWidth ) / 2 );
	lprcLBDst->right = lprcLBDst->left + iDstLBWidth;

	lprcLBDst->top = rcDst.top + ( ( iDstHeight - iDstLBHeight ) / 2 );
	lprcLBDst->bottom = lprcLBDst->top + iDstLBHeight;
}

//+-------------------------------------------------------------------------
//
//  Function:   PixelAspectToPictureAspect
//
//  Synopsis:   Converts a pixel aspect ratio to a picture aspect ratio
//
//--------------------------------------------------------------------------

void Presenter::PixelAspectToPictureAspect(
	int Width,
	int Height,
	int PixelAspectX,
	int PixelAspectY,
	int* pPictureAspectX,
	int* pPictureAspectY
	)
{
	//
	// sanity check - if any inputs are 0, return 0
	//
	if( PixelAspectX == 0 || PixelAspectY == 0 || Width == 0 || Height == 0 ) {
		*pPictureAspectX = 0;
		*pPictureAspectY = 0;
		return;
	}

	//
	// start by reducing both ratios to lowest terms
	//
	ReduceToLowestTerms( Width, Height, &Width, &Height );
	ReduceToLowestTerms( PixelAspectX, PixelAspectY, &PixelAspectX, &PixelAspectY );

	//
	// Make sure that none of the values are larger than 2^16, so we don't
	// overflow on the last operation.   This reduces the accuracy somewhat,
	// but it's a "hail mary" for incredibly strange aspect ratios that don't
	// exist in practical usage.
	//
	while( Width > 0xFFFF || Height > 0xFFFF ) {
		Width >>= 1;
		Height >>= 1;
	}

	while( PixelAspectX > 0xFFFF || PixelAspectY > 0xFFFF ) {
		PixelAspectX >>= 1;
		PixelAspectY >>= 1;
	}

	ReduceToLowestTerms(
		PixelAspectX * Width,
		PixelAspectY * Height,
		pPictureAspectX,
		pPictureAspectY
		);
}

//+-------------------------------------------------------------------------
//
//  Function:   AspectRatioCorrectSize
//
//  Synopsis:   Corrects the supplied size structure so that it becomes the same shape
//              as the specified aspect ratio, the correction is always applied in the
//              horizontal axis
//
//--------------------------------------------------------------------------

void Presenter::AspectRatioCorrectSize(
	LPSIZE lpSizeImage,     // size to be aspect ratio corrected
	const SIZE& sizeAr,     // aspect ratio of image
	const SIZE& sizeOrig,   // original image size
	BOOL ScaleXorY          // axis to correct in
	)
{
	int cxAR = sizeAr.cx;
	int cyAR = sizeAr.cy;
	int cxOr = sizeOrig.cx;
	int cyOr = sizeOrig.cy;
	int sx = lpSizeImage->cx;
	int sy = lpSizeImage->cy;

	// MulDiv rounds correctly.
	lpSizeImage->cx = MulDiv( ( sx * cyOr ), cxAR, ( cyAR * cxOr ) );

	if( ScaleXorY && lpSizeImage->cx < cxOr ) {
		lpSizeImage->cx = cxOr;
		lpSizeImage->cy = MulDiv( ( sy * cxOr ), cyAR, ( cxAR * cyOr ) );
	}
}

//+-------------------------------------------------------------------------
//
//  Member:     UpdateRectangles
//
//  Synopsis:   Figures out the real source and destination rectangles
//              to use when drawing the video frame into the clients
//              destination location.  Takes into account pixel aspect
//              ration correction, letterboxing and source rectangles.
//
//--------------------------------------------------------------------------

void Presenter::UpdateRectangles( RECT* pDst, RECT* pSrc )
{
	// take the given src rect and reverse map it into the native video
	// image rectange.  For example, consider a video with a buffer size of
	// 720x480 and an active area of 704x480 - 8,0 with a picture aspect
	// ratio of 4:3.  The user sees the native video size as 640x480.
	//
	// If the user gave us a src rectangle of (180, 135, 540, 405)
	// then this gets reversed mapped to
	//
	// 8 + (180 * 704 / 640) = 206
	// 0 + (135 * 480 / 480) = 135
	// 8 + (540 * 704 / 640) = 602
	// 0 + (405 * 480 / 480) = 405

	RECT Src = *pSrc;

	pSrc->left = m_displayRect.left + MulDiv( pSrc->left, ( m_displayRect.right - m_displayRect.left ), m_uiRealDisplayWidth );
	pSrc->right = m_displayRect.left + MulDiv( pSrc->right, ( m_displayRect.right - m_displayRect.left ), m_uiRealDisplayWidth );

	pSrc->top = m_displayRect.top + MulDiv( pSrc->top, ( m_displayRect.bottom - m_displayRect.top ), m_uiRealDisplayHeight );
	pSrc->bottom = m_displayRect.top + MulDiv( pSrc->bottom, ( m_displayRect.bottom - m_displayRect.top ), m_uiRealDisplayHeight );

	LetterBoxDstRect( pDst, Src, m_rcDstApp );
}

}
}
} // namespace ci::msw::detail