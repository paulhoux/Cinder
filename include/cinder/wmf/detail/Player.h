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
#include "cinder/Log.h"
#include "cinder/app/Window.h" // ci::app::Window::Format
#include "cinder/msw/CinderMsw.h"
#include "cinder/wmf/detail/MFOptions.h"

#include <deque>
#include <evr.h>
#include <mfapi.h>

#if defined( _DEBUG )
#define CI_LOG_PLAYER( args ) CI_LOG_V( args )
#else
#define CI_LOG_PLAYER( args ) ( void )( 0 )
#endif

namespace cinder {
namespace wmf {

//!
struct ScopedMFInitializer {
	ScopedMFInitializer()
	    : ScopedMFInitializer( MF_VERSION, MFSTARTUP_FULL )
	{
	}
	ScopedMFInitializer( ULONG version, DWORD params )
	{
		if( FAILED(::MFStartup( version, params ) ) )
			throw Exception( "Failed to initialize Windows Media Foundation." );
	}

	~ScopedMFInitializer()
	{
		::MFShutdown();
	}
};

// ------------------------------------------------------------------------------------------------

class __declspec( uuid( "5D1B744C-7145-431D-B62C-6BF08BB9E17C" ) ) Player : public IMFAsyncCallback {
  private:
	typedef enum Command {
		CmdNone = 0,
		CmdStop,
		CmdStart,
		CmdPause,
		CmdSeek,
		CmdOpen,
		CmdClose,
		CmdRate
	};

	typedef struct State {
		State() {}
		State( const State& other )
		    : command( other.command )
		    , hnsStart( other.hnsStart )
		    , rate( other.rate )
		    , thin( other.thin )
		{
		}
		State( Command cmd )
		    : command( cmd )
		{
		}
		State( const State& other, Command cmd )
		    : command( cmd )
		    , hnsStart( other.hnsStart )
		    , rate( other.rate )
		    , thin( other.thin )
		{
		}

		std::string toString() const
		{
			static const char* cmds[] = { "None", "Stop", "Start", "Pause", "Seek", "Open", "Close", "Rate" };
			return std::string( cmds[command] );
		}
		friend std::ostream& operator<<( std::ostream& s, const State& o ) { return s << "[" << o.toString() << "]"; }

		Command command = CmdNone;
		MFTIME  hnsStart;     // Start position
		FLOAT   rate = 1.0f;  // Playback rate
		BOOL    thin = false; // Thinned playback?
	};

  public:
	Player( const MFOptions& options = MFOptions() );

	//!
	STDMETHODIMP CanScrub( BOOL* pbCanScrub ) const;
	STDMETHODIMP_( BOOL ) CanScrub() const { return m_bCanScrub; }
	//!
	STDMETHODIMP CanSeek( BOOL* pbCanSeek ) const;
	STDMETHODIMP_( BOOL ) CanSeek() const
	{
		BOOL bResult = FALSE;
		CanSeek( &bResult );
		return bResult;
	}
	//!
	STDMETHODIMP CanFastForward( BOOL* pbCanFF ) const;
	STDMETHODIMP_( BOOL ) CanFastForward() const
	{
		BOOL bResult = FALSE;
		CanFastForward( &bResult );
		return bResult;
	}
	//!
	STDMETHODIMP CanRewind( BOOL* pbCanRewind ) const;
	STDMETHODIMP_( BOOL ) CanRewind() const
	{
		BOOL bResult = FALSE;
		CanRewind( &bResult );
		return bResult;
	}
	//!
	STDMETHODIMP OpenURL( const WCHAR* url, const WCHAR* audioDeviceId = 0 );
	//!
	STDMETHODIMP Close();
	//!
	STDMETHODIMP Start();
	//!
	STDMETHODIMP Start( MFTIME position );
	//!
	STDMETHODIMP Pause();
	//!
	STDMETHODIMP Stop();
	//
	STDMETHODIMP Rewind();
	//
	STDMETHODIMP FastForward();
	//!
	STDMETHODIMP GetState( State* state ) const;
	//! Stores the current position in 100-nano-second units in \a pPosition. Returns result code.
	STDMETHODIMP GetPosition( MFTIME* pPosition ) const;
	//! Seeks to \a position, which is expressed in 100-nano-second units.
	STDMETHODIMP SetPosition( MFTIME position );
	//! 
	STDMETHODIMP_( FLOAT ) GetVolume() const;
	//!
	STDMETHODIMP SetVolume( float volume );
	//!
	STDMETHODIMP_( UINT64 ) GetDuration() const { return m_hnsDuration; }
	//!
	STDMETHODIMP Scrub( BOOL bScrub );
	//!
	STDMETHODIMP SetRate( FLOAT fRate );
	//!
	STDMETHODIMP SetLoop( BOOL loop )
	{
		m_bIsLoop = loop;
		return S_OK;
	}
	//!
	STDMETHODIMP_( BOOL ) IsLooping() const { return m_bIsLoop; }
	//!
	STDMETHODIMP_( BOOL ) IsPlaying() const;
	//!
	STDMETHODIMP_( BOOL ) IsPaused() const;
	//!
	STDMETHODIMP_( BOOL ) IsStopped() const;
	//!
	STDMETHODIMP_( BOOL ) IsScrubbing() const { return m_bIsScrubbing; }
	//!
	STDMETHODIMP_( BOOL ) IsDX9() const { return m_options.getDXVersion() == MFOptions::DX_9; }

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

	STDMETHODIMP Clear();

	//! Handle events received from Media Foundation. See: Invoke.
	STDMETHODIMP_( LRESULT ) HandleEvent( WPARAM wParam );
	//! Allow MFWndProc access to HandleEvent.
	friend LRESULT CALLBACK MFWndProc( HWND, UINT, WPARAM, LPARAM );

	STDMETHODIMP OnSessionStart( HRESULT hrStatus );
	STDMETHODIMP OnSessionStop( HRESULT hrStatus );
	STDMETHODIMP OnSessionPause( HRESULT hrStatus );
	STDMETHODIMP OnSessionEnded( HRESULT hrStatus );
	STDMETHODIMP OnSessionRateChanged( IMFMediaEvent* pEvent, HRESULT hrStatus );
	STDMETHODIMP OnSessionScrubSampleComplete( HRESULT hrStatus );

	STDMETHODIMP OnTopologyStatus( IMFMediaEvent* pEvent );
	STDMETHODIMP OnPresentationEnded( IMFMediaEvent* pEvent );
	STDMETHODIMP OnNewPresentation( IMFMediaEvent* pEvent );
	STDMETHODIMP OnSessionEvent( IMFMediaEvent*, MediaEventType ) { return S_OK; }

	STDMETHODIMP CreateSession();
	STDMETHODIMP CloseSession();

	STDMETHODIMP Repaint();
	STDMETHODIMP ResizeVideo( WORD width, WORD height );

	STDMETHODIMP CreatePartialTopology( IMFPresentationDescriptor* pDescriptor );
	STDMETHODIMP SetMediaInfo( IMFPresentationDescriptor* pDescriptor );

	STDMETHODIMP CommitRateChange( FLOAT fRate, BOOL bThin );
	STDMETHODIMP_( FLOAT ) Player::GetNominalRate()
	{
		//return m_requests.empty() ? m_state.rate : m_requests.front().rate;
		return m_state.rate;
	}
	//STDMETHODIMP SetPositionInternal( MFTIME position );

	STDMETHODIMP QueueCommand( const State& cmd );
	STDMETHODIMP ExecuteCommand();
	STDMETHODIMP UpdatePendingCommands( Command cmd );

	//! Creates a (hidden) window used by Media Foundation.
	STDMETHODIMP CreateWnd();
	//! Destroys the (hidden) window.
	STDMETHODIMP DestroyWnd();

	static void RegisterWindowClass();

	STDMETHODIMP CreateMediaSource( LPCWSTR pUrl, IMFMediaSource** ppSource ) const;
	STDMETHODIMP CreatePlaybackTopology( IMFMediaSource* pSource, IMFPresentationDescriptor* pPD, HWND hVideoWnd, IMFTopology** ppTopology ) const;
	STDMETHODIMP CreateMediaSinkActivate( IMFStreamDescriptor* pSourceSD, HWND hVideoWindow, IMFActivate** ppActivate ) const;
	STDMETHODIMP AddSourceNode( IMFTopology* pTopology, IMFMediaSource* pSource, IMFPresentationDescriptor* pPD, IMFStreamDescriptor* pSD, IMFTopologyNode** ppNode ) const;
	STDMETHODIMP AddOutputNode( IMFTopology* pTopology, IMFStreamSink* pStreamSink, IMFTopologyNode** ppNode ) const;
	STDMETHODIMP AddOutputNode( IMFTopology* pTopology, IMFActivate* pActivate, DWORD dwId, IMFTopologyNode** ppNode ) const;
	STDMETHODIMP AddBranchToPartialTopology( IMFTopology* pTopology, IMFMediaSource* pSource, IMFPresentationDescriptor* pPD, DWORD iStream, HWND hVideoWnd ) const;

  private:
	//! Makes sure Media Foundation is initialized.
	ScopedMFInitializer m_initializer;

	mutable msw::CriticalSection m_critSec;

	State             m_state;
	State             m_restore;
	State             m_pending;
	std::deque<State> m_requests;

	ULONG m_nRefCount;
	HWND  m_hWnd;

	BOOL m_bIsLoop;
	BOOL m_bCanScrub;
	BOOL m_bIsScrubbing;

	DWORD  m_caps;
	UINT32 m_uiWidth, m_uiHeight;
	MFTIME m_hnsDuration;

	HANDLE m_hCloseEvent;

	IMFMediaSession*      m_pSession;
	IMFMediaSource*       m_pSource;
	IMFPresentationClock* m_pClock;
	IMFRateControl*       m_pRateControl;
	IMFRateSupport*       m_pRateSupport;
	IMFAudioStreamVolume* m_pVolume;

	IMFVideoDisplayControl* m_pVideoDisplayControl;

	//! Allows control over the created window.
	ci::app::Window::Format m_windowFormat;

	MFOptions m_options;
};
}
} // namespace ci::wmf