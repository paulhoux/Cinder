/*
Copyright (c) 2015, The Barbarian Group
All rights reserved.

Copyright (c) Microsoft Open Technologies, Inc. All rights reserved.

This code is intended for use with the Cinder C++ library: http://libcinder.org

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "cinder/Exception.h"
#include "cinder/app/Window.h" // ci::app::Window::Format
#include "cinder/msw/CinderMsw.h"

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <d3d9.h>
#include <dxva2api.h>
#include <evr.h>

#define MF_USE_DXVA2_DECODER 1

namespace cinder {
namespace msw {

struct ScopedMFInitializer {
	ScopedMFInitializer() : ScopedMFInitializer( MF_VERSION, MFSTARTUP_FULL ) {}
	ScopedMFInitializer( ULONG version, DWORD params )
	{
		if( FAILED( ::MFStartup( version, params ) ) )
			throw Exception( "ScopedMFInitializer: failed to initialize Windows Media Foundation." );
	}

	~ScopedMFInitializer()
	{
		::MFShutdown();
	}
};

/*//
struct ScopedLockDevice {
	ScopedLockDevice( IDirect3DDeviceManager9 *pDeviceManager, BOOL fBlock );
	~ScopedLockDevice();

	STDMETHODIMP GetDevice( IDirect3DDevice9** pDevice );

private:
	IDirect3DDeviceManager9*  m_pDeviceManager;
	IDirect3DDevice9*         m_pDevice;
	HANDLE                    m_pHandle;
};//*/

//------------------------------------------------------------------------------------

//////////////////////////////////////////////////////////////////////////
//  MFAsyncCallback [template]
//
//  Description:
//  Helper class that routes IMFAsyncCallback::Invoke calls to a class
//  method on the parent class.
//
//  Usage:
//  Add this class as a member variable. In the parent class constructor,
//  initialize the MFAsyncCallback class like this:
//      m_cb(this, &CYourClass::OnInvoke)
//  where
//      m_cb       = MFAsyncCallback object
//      CYourClass = parent class
//      OnInvoke   = Method in the parent class to receive Invoke calls.
//
//  The parent's OnInvoke method (you can name it anything you like) must
//  have a signature that matches the InvokeFn typedef below.
//////////////////////////////////////////////////////////////////////////

// T: Type of the parent object
template<class T>
class MFAsyncCallback : public IMFAsyncCallback {
public:

	typedef HRESULT( T::*InvokeFn )( IMFAsyncResult* pAsyncResult );

	MFAsyncCallback( T* pParent, InvokeFn fn ) :
		m_pParent( pParent ),
		m_pInvokeFn( fn )
	{
	}

	// IUnknown
	STDMETHODIMP_( ULONG ) AddRef( void )
	{
		// Delegate to parent class.
		return m_pParent->AddRef();
	}

	STDMETHODIMP_( ULONG ) Release( void )
	{
		// Delegate to parent class.
		return m_pParent->Release();
	}

	STDMETHODIMP QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv )
	{
		if( !ppv ) {
			return E_POINTER;
		}
		if( iid == __uuidof( IUnknown ) ) {
			*ppv = static_cast<IUnknown*>( static_cast<IMFAsyncCallback*>( this ) );
		}
		else if( iid == __uuidof( IMFAsyncCallback ) ) {
			*ppv = static_cast<IMFAsyncCallback*>( this );
		}
		else {
			*ppv = NULL;
			return E_NOINTERFACE;
		}
		AddRef();
		return S_OK;
	}

	// IMFAsyncCallback methods
	STDMETHODIMP GetParameters( __RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue )
	{
		// Implementation of this method is optional.
		return E_NOTIMPL;
	}

	STDMETHODIMP Invoke( __RPC__in_opt IMFAsyncResult* pAsyncResult )
	{
		return ( m_pParent->*m_pInvokeFn )( pAsyncResult );
	}

private:

	T* m_pParent;
	InvokeFn m_pInvokeFn;
};

//------------------------------------------------------------------------------------

//! Converts a fixed-point to a float.
inline float MFOffsetToFloat( const MFOffset& offset )
{
	return (float)offset.value + ( (float)offset.value / 65536.0f );
}

inline RECT MFVideoAreaToRect( const MFVideoArea area )
{
	float left = MFOffsetToFloat( area.OffsetX );
	float top = MFOffsetToFloat( area.OffsetY );

	RECT rc =
	{
		int( left + 0.5f ),
		int( top + 0.5f ),
		int( left + area.Area.cx + 0.5f ),
		int( top + area.Area.cy + 0.5f )
	};

	return rc;
}

inline MFOffset MFMakeOffset( float v )
{
	MFOffset offset;
	offset.value = short( v );
	offset.fract = WORD( 65536 * ( v - offset.value ) );
	return offset;
}

inline MFVideoArea MFMakeArea( float x, float y, DWORD width, DWORD height )
{
	MFVideoArea area;
	area.OffsetX = MFMakeOffset( x );
	area.OffsetY = MFMakeOffset( y );
	area.Area.cx = width;
	area.Area.cy = height;
	return area;
}

//------------------------------------------------------------------------------------

template <class Q>
HRESULT MFGetEventObject( IMFMediaEvent *pEvent, Q **ppObject )
{
	*ppObject = NULL;

	PROPVARIANT var;
	HRESULT hr = pEvent->GetValue( &var );
	if( SUCCEEDED( hr ) ) {
		if( var.vt == VT_UNKNOWN ) {
			hr = var.punkVal->QueryInterface( ppObject );
		}
		else {
			hr = MF_E_INVALIDTYPE;
		}
		PropVariantClear( &var );
	}

	return hr;
}

//------------------------------------------------------------------------------------

inline HRESULT GetDXVA2ExtendedFormat( IMFMediaType* pMediaType, DXVA2_ExtendedFormat* pFormat )
{
	if( pFormat == NULL )
		return E_POINTER;

	if( pMediaType == NULL )
		return E_POINTER;

	HRESULT hr = S_OK;

	do {
		MFVideoInterlaceMode interlace = (MFVideoInterlaceMode)MFGetAttributeUINT32( pMediaType, MF_MT_INTERLACE_MODE, MFVideoInterlace_Unknown );

		if( interlace == MFVideoInterlace_MixedInterlaceOrProgressive ) {
			pFormat->SampleFormat = DXVA2_SampleFieldInterleavedEvenFirst;
		}
		else {
			pFormat->SampleFormat = (UINT)interlace;
		}

		pFormat->VideoChromaSubsampling =
			MFGetAttributeUINT32( pMediaType, MF_MT_VIDEO_CHROMA_SITING, MFVideoChromaSubsampling_Unknown );
		pFormat->NominalRange =
			MFGetAttributeUINT32( pMediaType, MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_Unknown );
		pFormat->VideoTransferMatrix =
			MFGetAttributeUINT32( pMediaType, MF_MT_YUV_MATRIX, MFVideoTransferMatrix_Unknown );
		pFormat->VideoLighting =
			MFGetAttributeUINT32( pMediaType, MF_MT_VIDEO_LIGHTING, MFVideoLighting_Unknown );
		pFormat->VideoPrimaries =
			MFGetAttributeUINT32( pMediaType, MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_Unknown );
		pFormat->VideoTransferFunction =
			MFGetAttributeUINT32( pMediaType, MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_Unknown );
	} while( FALSE );

	return hr;
}

inline HRESULT ConvertToDXVAType( IMFMediaType* pMediaType, DXVA2_VideoDesc* pDesc ) {
	if( pDesc == NULL )
		return E_POINTER;

	if( pMediaType == NULL )
		return E_POINTER;

	HRESULT hr = S_OK;

	ZeroMemory( pDesc, sizeof( *pDesc ) );

	do {
		GUID guidSubType = GUID_NULL;
		hr = pMediaType->GetGUID( MF_MT_SUBTYPE, &guidSubType );
		BREAK_ON_FAIL( hr );

		UINT32 width = 0, height = 0;
		hr = MFGetAttributeSize( pMediaType, MF_MT_FRAME_SIZE, &width, &height );
		BREAK_ON_FAIL( hr );

		UINT32 fpsNumerator = 0, fpsDenominator = 0;
		hr = MFGetAttributeRatio( pMediaType, MF_MT_FRAME_RATE, &fpsNumerator, &fpsDenominator );
		BREAK_ON_FAIL( hr );

		pDesc->Format = (D3DFORMAT)guidSubType.Data1;
		pDesc->SampleWidth = width;
		pDesc->SampleHeight = height;
		pDesc->InputSampleFreq.Numerator = fpsNumerator;
		pDesc->InputSampleFreq.Denominator = fpsDenominator;

		hr = GetDXVA2ExtendedFormat( pMediaType, &pDesc->SampleFormat );
		BREAK_ON_FAIL( hr );

		pDesc->OutputFrameFreq = pDesc->InputSampleFreq;
		if( ( pDesc->SampleFormat.SampleFormat == DXVA2_SampleFieldInterleavedEvenFirst ) ||
			( pDesc->SampleFormat.SampleFormat == DXVA2_SampleFieldInterleavedOddFirst ) ) {
			pDesc->OutputFrameFreq.Numerator *= 2;
		}
	} while( FALSE );

	return hr;
}

//------------------------------------------------------------------------------------

class __declspec( uuid( "5D1B744C-7145-431D-B62C-6BF08BB9E17C" ) ) MFPlayer : public IMFAsyncCallback {
public:
	typedef enum State {
		Closed = 0,     // No session.
		Ready,          // Session was created, ready to open a file. 
		OpenPending,    // Session is opening a file.
		Started,        // Session is playing a file.
		Paused,         // Session is paused.
		Stopped,        // Session is stopped (ready to play). 
		Closing         // Application has closed the session, but is waiting for MESessionClosed.
	};

public:
	MFPlayer();

	//! 
	HRESULT OpenURL( const WCHAR *url, const WCHAR *audioDeviceId = 0 );
	//!
	HRESULT Close() { return CloseSession(); }
	//!
	HRESULT Play();
	//!
	HRESULT Pause();
	//! Seeks to \a position, which is expressed in 100-nano-second units.
	HRESULT SetPosition( MFTIME position );
	//!
	HRESULT SetLoop( BOOL loop ) { mIsLooping = loop; return S_OK; }

	//! Returns the current state.
	State GetState() const { return mState; }

	// IUnknown methods
	STDMETHODIMP QueryInterface( REFIID iid, void** ppv );
	STDMETHODIMP_( ULONG ) AddRef();
	STDMETHODIMP_( ULONG ) Release();

	// IMFAsyncCallback methods
	STDMETHODIMP GetParameters( DWORD*, DWORD* ) { return E_NOTIMPL; }
	//!  Callback for the asynchronous BeginGetEvent method.
	STDMETHODIMP Invoke( IMFAsyncResult* pAsyncResult );

private:
	//! Destructor is private, caller should call Release().
	~MFPlayer();

	//! Handle events received from Media Foundation. See: Invoke.
	LRESULT HandleEvent( WPARAM wParam );
	//! Allow MFWndProc access to HandleEvent.
	friend LRESULT CALLBACK MFWndProc( HWND, UINT, WPARAM, LPARAM );

	HRESULT OnTopologyStatus( IMFMediaEvent *pEvent );
	HRESULT OnPresentationEnded( IMFMediaEvent *pEvent );
	HRESULT OnNewPresentation( IMFMediaEvent *pEvent );
	HRESULT OnSessionEvent( IMFMediaEvent*, MediaEventType ) { return S_OK; }

	HRESULT CreateSession();
	HRESULT CloseSession();

	HRESULT Repaint();
	HRESULT ResizeVideo( WORD width, WORD height );

	HRESULT CreatePartialTopology( IMFPresentationDescriptor *pDescriptor );
	HRESULT SetMediaInfo( IMFPresentationDescriptor *pDescriptor );

	//! Creates a (hidden) window used by Media Foundation.
	void CreateWnd();
	//! Destroys the (hidden) window.
	void DestroyWnd();

	static void RegisterWindowClass();

	static HRESULT CreateMediaSource( LPCWSTR pUrl, IMFMediaSource **ppSource );
	static HRESULT CreatePlaybackTopology( IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology, IMFVideoPresenter *pVideoPresenter );
	static HRESULT CreateMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate, IMFVideoPresenter *pVideoPresenter, IMFMediaSink **ppMediaSink );
	static HRESULT AddSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode );
	static HRESULT AddOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode );
	static HRESULT AddOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode );
	static HRESULT AddBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd, IMFVideoPresenter *pVideoPresenter );

private:
	//! Makes sure Media Foundation is initialized. 
	ScopedMFInitializer  mInitializer;

	State   mState;

	ULONG   mRefCount;
	HWND    mWnd;

	BOOL    mPlayWhenReady;
	BOOL    mIsLooping;
	UINT32  mWidth, mHeight;

	HANDLE  mCloseEvent;

	IMFMediaSession         *mSessionPtr;
	IMFMediaSource          *mSourcePtr;
	
	IMFVideoPresenter       *mPresenterPtr;
	//IMFMediaSink            *mSinkPtr;

	IMFVideoDisplayControl  *mVideoDisplayPtr;

	//! Allows control over the created window.
	ci::app::Window::Format  mWindowFormat;
};

}
}