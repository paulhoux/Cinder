#pragma once

#include "cinder/msw/detail/Presenter.h"

namespace cinder {
namespace msw {
namespace detail {

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

}
}
} // namespace ci::msw::detail