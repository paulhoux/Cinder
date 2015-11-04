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

#include "cinder/wmf/MediaFoundation.h"
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/Exception.h"
#include "cinder/app/Window.h" // ci::app::Window::Format

#define MF_USE_DXVA2_DECODER 0

namespace cinder {
namespace wmf {

class __declspec( uuid( "5D1B744C-7145-431D-B62C-6BF08BB9E17C" ) ) Player : public IMFAsyncCallback {
public:
	typedef enum State {
		Closed = 0,     // No session.
		Stopped,        // Session is stopped (ready to play).
		Started,        // Session is playing a file.
		Paused,         // Session is paused.
		Seeking,        // Session is seeking.
		Ready,          // Session was created, ready to open a file.
		Opening,        // Session is opening a file.
		Closing         // Application has closed the session, but is waiting for MESessionClosed.
	};

	typedef enum Command {
		CmdNone = 0,
		CmdStop = State::Stopped,
		CmdStart = State::Started,
		CmdPause = State::Paused,
		CmdSeek = State::Seeking
	};

public:
	Player( DirectXVersion dxVersion = DX_11 );

	//!
	STDMETHODIMP CanSeek( BOOL *pbCanSeek ) const;
	//!
	STDMETHODIMP CanFastForward( BOOL *pbCanFF ) const;
	//!
	STDMETHODIMP CanRewind( BOOL *pbCanRewind ) const;
	//! 
	STDMETHODIMP OpenURL( const WCHAR *url, const WCHAR *audioDeviceId = 0 );
	//!
	STDMETHODIMP Close() { return CloseSession(); }
	//!
	STDMETHODIMP Start();
	//!
	STDMETHODIMP Pause();
	//!
	STDMETHODIMP Stop();
	//! Stores the current position in 100-nano-second units in \a pPosition. Returns result code.
	STDMETHODIMP GetPosition( MFTIME *pPosition );
	//! Seeks to \a position, which is expressed in 100-nano-second units.
	STDMETHODIMP SetPosition( MFTIME position );
	//!
	STDMETHODIMP_( UINT64 ) GetDuration() const { return mDuration; }
	//!
	STDMETHODIMP SetLoop( BOOL loop ) { mIsLoop = loop; return S_OK; }
	//! Returns the current state.
	STDMETHODIMP_( State ) GetState() const { return mState; }

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
	~Player();

	//! Handle events received from Media Foundation. See: Invoke.
	STDMETHODIMP_( LRESULT ) HandleEvent( WPARAM wParam );
	//! Allow MFWndProc access to HandleEvent.
	friend LRESULT CALLBACK MFWndProc( HWND, UINT, WPARAM, LPARAM );

	STDMETHODIMP OnSessionStart( HRESULT hrStatus );
	STDMETHODIMP OnSessionStop( HRESULT hrStatus );
	STDMETHODIMP OnSessionPause( HRESULT hrStatus );
	STDMETHODIMP OnSessionEnded( HRESULT hrStatus );

	STDMETHODIMP OnTopologyStatus( IMFMediaEvent *pEvent );
	STDMETHODIMP OnPresentationEnded( IMFMediaEvent *pEvent );
	STDMETHODIMP OnNewPresentation( IMFMediaEvent *pEvent );
	STDMETHODIMP OnSessionEvent( IMFMediaEvent*, MediaEventType ) { return S_OK; }

	STDMETHODIMP UpdatePendingCommands( Command cmd );

	STDMETHODIMP CreateSession();
	STDMETHODIMP CloseSession();

	STDMETHODIMP Repaint();
	STDMETHODIMP ResizeVideo( WORD width, WORD height );

	STDMETHODIMP CreatePartialTopology( IMFPresentationDescriptor *pDescriptor );
	STDMETHODIMP SetMediaInfo( IMFPresentationDescriptor *pDescriptor );

	STDMETHODIMP SetPositionInternal( MFTIME position );

	//! Creates a (hidden) window used by Media Foundation.
	STDMETHODIMP_( void ) CreateWnd();
	//! Destroys the (hidden) window.
	STDMETHODIMP_( void ) DestroyWnd();

	static void RegisterWindowClass();

	STDMETHODIMP CreateMediaSource( LPCWSTR pUrl, IMFMediaSource **ppSource ) const;
	STDMETHODIMP CreatePlaybackTopology( IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology ) const;
	STDMETHODIMP CreateMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate ) const;
	STDMETHODIMP AddSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode ) const;
	STDMETHODIMP AddOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode ) const;
	STDMETHODIMP AddOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode ) const;
	STDMETHODIMP AddBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd ) const;

private:
	//! Makes sure Media Foundation is initialized.
	ScopedMFInitializer   mInitializer;

	msw::CriticalSection  mCritSec;

	State    mState;
	Command  mCommand;

	ULONG    mRefCount;
	HWND     mWnd;

	BOOL     mIsLoop;
	BOOL     mIsPending;
	BOOL     mCanScrub;

	UINT32   mWidth, mHeight;
	MFTIME   mDuration, mSeekTo;
	DWORD    mCapabilities;

	HANDLE   mCloseEvent;

	IMFMediaSession         *mSessionPtr;
	IMFMediaSource          *mSourcePtr;
	IMFPresentationClock    *mClockPtr;
	IMFRateControl          *mRateControlPtr;
	IMFRateSupport          *mRateSupportPtr;

	//! Allows control over the created window.
	ci::app::Window::Format  mWindowFormat;

	DirectXVersion           mDxVersion;
};

}
} // namespace ci::wmf

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )