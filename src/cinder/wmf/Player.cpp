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

#include "cinder/wmf/detail/Player.h"

#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/msw/ScopedPtr.h"
#include "cinder/wmf/detail/Activate.h"
#include "cinder/wmf/detail/MFUtils.h"
#include "cinder/wmf/detail/Presenter.h"

#include <Mferror.h>

// Include these libraries.
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

#pragma comment(lib, "winmm.lib") // for timeBeginPeriod and timeEndPeriod
#pragma comment (lib,"uuid.lib")

using namespace cinder::msw;

namespace cinder {
namespace wmf {

static const wchar_t *MF_WINDOW_CLASS_NAME = TEXT( "Player" );

static const UINT WM_APP_PLAYER_EVENT = WM_APP + 1;

LRESULT CALLBACK MFWndProc( HWND wnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	Player *impl = NULL;

	// If the message is WM_NCCREATE we need to hide 'this' in the window long.
	if( uMsg == WM_NCCREATE ) {
		impl = reinterpret_cast<Player*>( ( (LPCREATESTRUCT)lParam )->lpCreateParams );
		::SetWindowLongPtr( wnd, GWLP_USERDATA, (__int3264)(LONG_PTR)impl );
	}
	else
		impl = reinterpret_cast<Player*>( ::GetWindowLongPtr( wnd, GWLP_USERDATA ) );

	switch( uMsg ) {
		case WM_CLOSE:
			if( impl )
				return impl->Close();
			break;
		case WM_DESTROY:
			PostQuitMessage( 0 );
			return S_OK;
		case WM_PAINT:
			if( impl )
				return impl->Repaint();
			break;
		case WM_SIZE:
			if( impl )
				return impl->ResizeVideo( LOWORD( lParam ), HIWORD( lParam ) );
			break;
		case WM_APP_PLAYER_EVENT:
			if( impl )
				return impl->HandleEvent( wParam );
			break;
	}

	return DefWindowProc( wnd, uMsg, wParam, lParam );
}

// ----------------------------------------------------------------------------

Player::Player( const MFOptions &options )
	: m_nRefCount( 1 )
	, m_hWnd( NULL )
	, m_bIsLoop( FALSE )
	, m_bCanScrub( FALSE )
	, m_bIsScrubbing( FALSE )
	, m_state( CmdNone )
	, m_pending( CmdNone )
	, m_uiWidth( 0 )
	, m_uiHeight( 0 )
	, m_hnsDuration( 0 )
	, m_caps( 0 )
	, m_pSession( NULL )
	, m_pSource( NULL )
	, m_pClock( NULL )
	, m_pRateControl( NULL )
	, m_pRateSupport( NULL )
	, m_pVideoDisplayControl( NULL )
	, m_pVolume( NULL )
	, m_options( options )
{
	m_hCloseEvent = ::CreateEventA( NULL, FALSE, FALSE, NULL );
	if( m_hCloseEvent == NULL )
		throw Exception( "Failed to create Close event." );
}

Player::~Player()
{
	Close();

	if( m_hCloseEvent ) {
		::CloseHandle( m_hCloseEvent );
		m_hCloseEvent = NULL;
	}
}

// Clears any resources for the current topology.
HRESULT Player::Clear()
{
	m_caps = 0;
	m_bCanScrub = FALSE;

	m_hnsDuration = 0;

	SafeRelease( m_pClock );
	SafeRelease( m_pRateControl );
	SafeRelease( m_pRateSupport );
	SafeRelease( m_pVolume );

	ZeroMemory( &m_state, sizeof( m_state ) );
	m_state.command = CmdNone;
	m_state.rate = 1.0f;

	m_pending = m_restore = m_state;

	return S_OK;
}

// IUnknown
HRESULT Player::QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv )
{
	if( !ppv ) {
		return E_POINTER;
	}
	if( iid == IID_IUnknown ) {
		*ppv = static_cast<IUnknown*>( this );
	}
	else if( iid == __uuidof( Player ) ) {
		*ppv = static_cast<Player*>( this );
	}
	else if( iid == __uuidof( PresenterDX9 ) || iid == __uuidof( PresenterDX11 ) ) {
		if( NULL == m_pVideoDisplayControl ) {
			*ppv = NULL;
			return E_NOINTERFACE;
		}

		return m_pVideoDisplayControl->QueryInterface( iid, ppv );
	}
	else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

// IUnknown
ULONG Player::AddRef()
{
	return ::InterlockedIncrement( &m_nRefCount );
}

// IUnknown
ULONG Player::Release()
{
	ULONG uCount = ::InterlockedDecrement( &m_nRefCount );
	if( uCount == 0 ) {
		delete this;
	}
	return uCount;
}

// Player
HRESULT Player::CanSeek( BOOL *pbCanSeek ) const
{
	if( pbCanSeek == NULL ) {
		return E_POINTER;
	}

	// Note: The MFSESSIONCAP_SEEK flag is sufficient for seeking. However, to
	// implement a seek bar, an application also needs the duration (to get 
	// the valid range) and a presentation clock (to get the current position).

	*pbCanSeek = ( ( ( m_caps & MFSESSIONCAP_SEEK ) == MFSESSIONCAP_SEEK ) && ( m_hnsDuration > 0 ) && ( m_pClock != NULL ) );
	return S_OK;
}

// Queries whether the current session supports scrubbing.
HRESULT Player::CanScrub( BOOL *pbCanScrub ) const
{
	if( pbCanScrub == NULL ) {
		return E_POINTER;
	}

	*pbCanScrub = m_bCanScrub;
	return S_OK;
}

// Player
HRESULT Player::CanFastForward( BOOL *pbCanFF ) const
{
	if( pbCanFF == NULL ) {
		return E_POINTER;
	}

	*pbCanFF = ( ( m_caps & MFSESSIONCAP_RATE_FORWARD ) == MFSESSIONCAP_RATE_FORWARD );
	return S_OK;
}

// Player
HRESULT Player::CanRewind( BOOL *pbCanRewind ) const
{
	if( pbCanRewind == NULL ) {
		return E_POINTER;
	}

	*pbCanRewind = ( ( m_caps & MFSESSIONCAP_RATE_REVERSE ) == MFSESSIONCAP_RATE_REVERSE );
	return S_OK;
}

// Player
HRESULT Player::OpenURL( const WCHAR *url, const WCHAR *audioDeviceId )
{
	HRESULT hr = S_OK;

	// Sanity check.
	assert( m_state.command == CmdClose || m_state.command == CmdNone );
	assert( m_pending.command == CmdNone );
	assert( url );

	do {
		hr = CreateWnd();
		BREAK_ON_FAIL_MSG( hr, "Failed to create hidden window." );

		// Create the media session.
		hr = CreateSession();
		BREAK_ON_FAIL( hr );

		// Create the media source.
		hr = CreateMediaSource( url, &m_pSource );
		BREAK_ON_FAIL( hr );

		// Create the presentation descriptor for the media source.
		ScopedComPtr<IMFPresentationDescriptor> pDescriptor;
		hr = m_pSource->CreatePresentationDescriptor( &pDescriptor );
		BREAK_ON_FAIL( hr );

		// Create a partial topology.
		hr = CreatePartialTopology( pDescriptor );
		BREAK_ON_FAIL( hr );

		// If SetTopology succeeds, the media session will queue an MESessionTopologySet event.
		//m_state.command = CmdOpen;
		m_pending.command = CmdOpen;
	} while( false );

	if( FAILED( hr ) )
		CloseSession();

	return hr;
}

// Player
HRESULT Player::Close()
{
	HRESULT hr = S_OK;

	do {
		// Close the media session.
		hr = CloseSession();
		BREAK_ON_FAIL( hr );

		// Destroy the hidden window.
		hr = DestroyWnd();
		BREAK_ON_FAIL( hr );
	} while( FALSE );

	return hr;
}

// Player
HRESULT Player::Start()
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	auto cmd = State( m_state, CmdStart );
	hr = GetPosition( &cmd.hnsStart );
	hr = QueueCommand( cmd );

	return hr;
}

// Player
HRESULT Player::Start( MFTIME position )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	auto cmd = State( m_state, CmdStart );
	cmd.hnsStart = position;
	hr = QueueCommand( cmd );

	return hr;
}

// Player
HRESULT Player::Pause()
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	auto cmd = State( m_state, CmdPause );
	hr = QueueCommand( cmd );

	return hr;
}

// Player
HRESULT Player::Stop()
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	auto cmd = State( m_state, CmdStop );
	hr = QueueCommand( cmd );

	return hr;
}

// Player
HRESULT Player::Rewind()
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = S_OK;

	return hr;
}

// Player
HRESULT Player::FastForward()
{
	ScopedCriticalSection lock( m_critSec );

	HRESULT hr = S_OK;

	return hr;
}

HRESULT Player::GetState( State *state ) const
{
	// Return, in order:
	// 1. Cached state.
	// 2. Pending state.
	// 3. Current state.
	auto itr = std::find_if( m_requests.rbegin(), m_requests.rend(), []( const State &cmd ) {
		return cmd.command == CmdStart || cmd.command == CmdPause || cmd.command == CmdStop;
	} );

	if( itr != m_requests.rend() ) {
		*state = *itr;
	}
	else if( m_pending.command == CmdStart || m_pending.command == CmdPause || m_pending.command == CmdStop ) {
		*state = m_pending;
	}
	else {
		*state = m_state;
	}

	return S_OK;
}

// Enables or disables scrubbing.
HRESULT Player::Scrub( BOOL bScrub )
{
	// Scrubbing is implemented as rate = 0.
	HRESULT hr = S_OK;

	if( m_bIsScrubbing == bScrub )
		return hr;

	if( !m_bCanScrub && bScrub )
		return E_FAIL;

	m_bIsScrubbing = bScrub;

	if( bScrub ) {
		m_restore = m_state;

		hr = SetRate( 0 );
	}
	else {
		hr = SetRate( m_restore.rate );
		Start( m_restore.hnsStart );

		if( m_restore.command == CmdPause )
			Pause();

		m_restore.command = CmdNone;
	}

	return hr;
}

HRESULT Player::GetPosition( MFTIME *pPosition ) const
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	do {
		BREAK_ON_NULL( pPosition, E_POINTER );
		BREAK_ON_NULL( m_pClock, E_POINTER );

		// Return, in order:
		// 1. Cached seek request (nominal position).
		// 2. Pending seek operation (nominal position).
		// 3. Active scrub operation (nominal position).
		// 4. Presentation time (actual position).
		auto itr = std::find_if( m_requests.rbegin(), m_requests.rend(), []( const State &cmd ) { return cmd.command == CmdSeek || cmd.command == CmdStart; } );
		if( itr != m_requests.rend() ) {
			*pPosition = itr->hnsStart;
		}
		else if( m_pending.command == CmdSeek || m_pending.command == CmdStart ) {
			*pPosition = m_pending.hnsStart;
		}
		else if( m_restore.command != CmdNone ) {
			*pPosition = m_restore.hnsStart;
		}
		else {
			hr = m_pClock->GetTime( pPosition );
		}
	} while( FALSE );

	return hr;
}

HRESULT Player::SetPosition( MFTIME position )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	m_restore.hnsStart = position;

	auto cmd = State( CmdSeek );
	cmd.hnsStart = position;
	hr = QueueCommand( cmd );

	// If current state is paused, make sure to pause again after seeking.
	if( m_state.command == CmdPause ) {
		cmd = State( CmdPause );
		cmd.hnsStart = position;
		hr = QueueCommand( cmd );
	}

	return hr;
}

FLOAT Player::GetVolume() const
{
	HRESULT hr = S_OK;

	float averageVolume = 0.0f;

	do {
		BREAK_ON_NULL_MSG( m_pSession, E_POINTER, "Session not available." );

		if( NULL == m_pVolume ) {
			hr = ::MFGetService( m_pSession, MR_STREAM_VOLUME_SERVICE, __uuidof( IMFAudioStreamVolume ), (void**)&m_pVolume );
			BREAK_ON_FAIL( hr );
		}

		UINT32 nChannels;
		m_pVolume->GetChannelCount( &nChannels );

		if( nChannels > 0 ) {
			for( UINT32 i = 0; i < nChannels; ++i ) {
				float volume;
				if( SUCCEEDED( m_pVolume->GetChannelVolume( i, &volume ) ) )
					averageVolume += volume;
			}

			averageVolume /= nChannels;
		}
	} while( false );

	return averageVolume;
}

HRESULT Player::SetVolume( float volume )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL_MSG( m_pSession, E_POINTER, "Session not available." );

		if( NULL == m_pVolume ) {
			hr = ::MFGetService( m_pSession, MR_STREAM_VOLUME_SERVICE, __uuidof( IMFAudioStreamVolume ), (void**)&m_pVolume );
			BREAK_ON_FAIL( hr );
		}

		UINT32 nChannels;
		m_pVolume->GetChannelCount( &nChannels );

		for( UINT32 i = 0; i < nChannels; ++i ) {
			m_pVolume->SetChannelVolume( i, volume );
		}
	} while( false );

	return hr;
}

HRESULT Player::OnSessionStart( HRESULT hrStatus )
{
	HRESULT hr = S_OK;

	if( FAILED( hrStatus ) ) {
		return hrStatus;
	}

	// The Media Session completed a start/seek operation. Check if there
	// is another seek request pending.
	if( m_pending.command == CmdStart || m_pending.command == CmdSeek )
		UpdatePendingCommands( m_pending.command );

	return hr;
}

HRESULT Player::OnSessionStop( HRESULT hrStatus )
{
	HRESULT hr = S_OK;

	if( FAILED( hrStatus ) ) {
		return hrStatus;
	}

	// The Media Session completed a transition to stopped. This might occur
	// because we are changing playback direction (forward/rewind). Check if
	// there is a pending rate-change request.
	if( m_pending.command == CmdStop )
		UpdatePendingCommands( CmdStop );

	return hr;
}

HRESULT Player::OnSessionPause( HRESULT hrStatus )
{
	HRESULT hr = S_OK;

	if( FAILED( hrStatus ) ) {
		return hrStatus;
	}

	if( m_pending.command == CmdPause )
		UpdatePendingCommands( CmdPause );

	return hr;
}

HRESULT Player::OnSessionEnded( HRESULT hr )
{
	// After the session ends, playback starts from position zero. But if the
	// current playback rate is reversed, playback would end immediately 
	// (reversing from position 0). Therefore, reset the rate to 1x.

	if( GetNominalRate() < 0.0f ) {
		hr = CommitRateChange( 1.0f, FALSE );
	}

	return hr;
}

HRESULT Player::OnSessionRateChanged( IMFMediaEvent *pEvent, HRESULT hrStatus )
{
	// If the rate change succeeded, we've already got the rate
	// cached. If it failed, try to get the actual rate.
	if( FAILED( hrStatus ) ) {
		PROPVARIANT var;
		PropVariantInit( &var );

		HRESULT hr = pEvent->GetValue( &var );
		if( SUCCEEDED( hr ) && ( var.vt == VT_R4 ) ) {
			m_state.rate = var.fltVal;
		}
	}

	if( m_pending.command == CmdRate )
		UpdatePendingCommands( CmdRate );

	return S_OK;
}

HRESULT Player::OnSessionScrubSampleComplete( HRESULT hrStatus )
{
	HRESULT hr = S_OK;

	if( FAILED( hrStatus ) ) {
		return hrStatus;
	}

	if( m_pending.command == CmdSeek )
		UpdatePendingCommands( CmdSeek );

	return hr;
}

HRESULT Player::OnTopologyStatus( IMFMediaEvent *pEvent )
{
	HRESULT hr = S_OK;

	do {
		UINT32 status;
		hr = pEvent->GetUINT32( MF_EVENT_TOPOLOGY_STATUS, &status );
		BREAK_ON_FAIL( hr );

		if( status == MF_TOPOSTATUS_READY ) {
			// Get the IMFVideoDisplayControl interface from session. This call is
			// expected to fail if the media file does not have a video stream.
			ScopedComPtr<IMFVideoDisplayControl> pVideoDisplayControl;
			hr = MFGetService( m_pSession, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pVideoDisplayControl ) );
			if( FAILED( hr ) ) {
				CI_LOG_E( "Failed to get IMFVideoDisplayControl interface from session. " << HRESULT_STRING( hr ) );
				__debugbreak();
			}

			// Keep track of the presenter.
			SafeRelease( m_pVideoDisplayControl );
			m_pVideoDisplayControl = pVideoDisplayControl.Detach();

			m_pending.command = CmdPause;
			UpdatePendingCommands( CmdPause );
		}
	} while( FALSE );

	return hr;
}

HRESULT Player::OnPresentationEnded( IMFMediaEvent *pEvent )
{
	SYSTEMTIME t;
	::GetLocalTime( &t );

	HRESULT hr = S_OK;

	if( m_bIsLoop ) {
		// Seek to the beginning.
		hr = Start( 0 );
	}
	else {
		// Pause instead of Stop, so we can easily restart the video later.
		hr = Pause();
	}

	return hr;
}

HRESULT Player::OnNewPresentation( IMFMediaEvent *pEvent )
{
	HRESULT hr = S_OK;

	do {
		// Get the presentation descriptor from the event.
		ScopedComPtr<IMFPresentationDescriptor> pPD;
		hr = MFGetEventObject( pEvent, &pPD );
		BREAK_ON_FAIL( hr );

		// Create a partial topology.
		hr = CreatePartialTopology( pPD );
		BREAK_ON_FAIL( hr );

		m_state.command = CmdOpen;
	} while( false );

	return hr;
}

HRESULT Player::QueueCommand( const State &cmd )
{
	HRESULT hr = S_OK;

	// Caller holds the lock.

	if( m_requests.empty() ) {
		CI_LOG_PLAYER( "Adding " << cmd << " to empty queue." );
		m_requests.push_back( cmd );
	}
	else if( m_requests.back().command == cmd.command ) {
		CI_LOG_PLAYER( "Replacing last command in queue with " << cmd );
		m_requests.back() = cmd;
	}
	else {
		CI_LOG_PLAYER( "Adding " << cmd << " to queue." );
		m_requests.push_back( cmd );
	}

	// Kick off the command.
	if( m_pending.command == CmdNone ) {
		hr = ExecuteCommand();
	}

	return hr;
}

HRESULT Player::ExecuteCommand()
{
	HRESULT hr = S_OK;

	assert( m_pending.command == CmdNone );

	// Caller holds the lock.

	if( !m_requests.empty() ) {
		if( m_pSession == NULL || m_pRateControl == NULL )
			return E_POINTER;

		State request = m_requests.front();
		m_requests.pop_front();

		PROPVARIANT varStart;
		PropVariantInit( &varStart );
		varStart.vt = VT_I8;
		varStart.hVal.QuadPart = request.hnsStart;

		CI_LOG_PLAYER( "Executing command: " << request );

		switch( request.command ) {
			case CmdNone:
				// Nothing to do.
				break;

			case CmdStart:
				hr = m_pSession->Start( &GUID_NULL, &varStart );
				if( SUCCEEDED( hr ) ) {
					m_pending = request;
				}
				else {
					CI_LOG_E( "Failed to start session. " << HRESULT_STRING( hr ) );
				}
				break;

			case CmdPause:
				hr = m_pSession->Pause();
				if( SUCCEEDED( hr ) ) {
					m_pending = request;
				}
				else {
					CI_LOG_E( "Failed to pause session. " << HRESULT_STRING( hr ) );
				}
				break;

			case CmdStop:
				hr = m_pSession->Stop();
				if( SUCCEEDED( hr ) ) {
					m_pending = request;
				}
				else {
					CI_LOG_E( "Failed to stop session. " << HRESULT_STRING( hr ) );
				}
				break;

			case CmdSeek:
				hr = m_pSession->Start( &GUID_NULL, &varStart );
				if( SUCCEEDED( hr ) ) {
					m_pending = request;
				}
				else {
					CI_LOG_E( "Failed to seek session. " << HRESULT_STRING( hr ) );
				}
				break;

			case CmdRate:
				hr = m_pRateControl->SetRate( request.thin, request.rate );
				if( SUCCEEDED( hr ) ) {
					m_pending = request;
				}
				else {
					CI_LOG_E( "Failed to set rate. " << HRESULT_STRING( hr ) );
				}
				break;

			default:
				CI_LOG_E( "Invalid request." );
				break;
		}

		PropVariantClear( &varStart );
	}
	else
		CI_LOG_PLAYER( "All commands have been executed." );

	return hr;
}

HRESULT Player::UpdatePendingCommands( Command cmd )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	PROPVARIANT varStart;
	PropVariantInit( &varStart );

	do {
		if( m_pending.command == cmd ) {
			CI_LOG_PLAYER( "Command executed: " << State( cmd ) );

			// 
			switch( m_pending.command ) {
				case CmdStart:
				case CmdPause:
				case CmdStop:
					m_state.command = m_pending.command;
					m_state.hnsStart = m_pending.hnsStart;
					break;
				case CmdSeek:
					m_state.hnsStart = m_pending.hnsStart;
					break;
				default:
					m_state.rate = m_pending.rate;
					m_state.hnsStart = m_pending.hnsStart;
					break;
			}

			m_pending.command = CmdNone;

			// Now execute next command, if queued.
			ExecuteCommand();
		}
	} while( FALSE );

	PropVariantClear( &varStart );

	return hr;
}

HRESULT Player::CreateSession()
{
	HRESULT hr = S_OK;

	do {
		// Close the old session.
		hr = CloseSession();
		BREAK_ON_FAIL_MSG( hr, "Failed to close media session." );

		// Create the media session.
		hr = ::MFCreateMediaSession( NULL, &m_pSession );
		BREAK_ON_FAIL_MSG( hr, "Failed to create media session." );

		// Start pulling events from the media session
		hr = m_pSession->BeginGetEvent( ( IMFAsyncCallback* ) this, NULL );
		BREAK_ON_FAIL_MSG( hr, "Failed to set media session callback." );

		m_state.command = CmdNone; // Ready
	} while( false );

	return hr;
}

HRESULT Player::CloseSession()
{
	HRESULT hr = S_OK;

	//  The IMFMediaSession::Close method is asynchronous, but the 
	//  Player::CloseSession method waits on the MESessionClosed event.
	//  
	//  MESessionClosed is guaranteed to be the last event that the media session fires.

	// First close the media session.
	if( m_pSession ) {
		m_state.command = CmdClose;

		hr = m_pSession->Close();

		// Wait for the close operation to complete
		if( SUCCEEDED( hr ) ) {
			DWORD dwWaitResult = 0;
			dwWaitResult = WaitForSingleObject( m_hCloseEvent, 5000 );
			if( dwWaitResult == WAIT_TIMEOUT ) {
				CI_LOG_W( "Timeout closing media session." );
			}
			// Now there will be no more events from this session.
		}
	}

	// Complete shutdown operations.
	if( SUCCEEDED( hr ) ) {
		// Shut down the media source. (Synchronous operation, no events.)
		if( m_pSource ) {
			(void)m_pSource->Shutdown();
		}

		// Shut down the media session. (Synchronous operation, no events.)
		if( m_pSession ) {
			(void)m_pSession->Shutdown();
		}
	}

	SafeRelease( m_pVideoDisplayControl );
	SafeRelease( m_pRateSupport );
	SafeRelease( m_pRateControl );
	SafeRelease( m_pVolume );
	SafeRelease( m_pClock );
	SafeRelease( m_pSource );
	SafeRelease( m_pSession );

	m_state.command = CmdNone; // Closed

	return hr;
}

HRESULT Player::Repaint()
{
	HRESULT hr = S_OK;

	PAINTSTRUCT ps;
	::BeginPaint( m_hWnd, &ps );

	do {
		BREAK_ON_NULL( m_pSession, E_POINTER );

		// Obtain reference to video display.
		ScopedComPtr<IMFVideoDisplayControl> pVideoDisplayControl;
		hr = MFGetService( m_pSession, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pVideoDisplayControl ) );
		BREAK_ON_FAIL( hr );

		pVideoDisplayControl->RepaintVideo();
	} while( FALSE );

	::EndPaint( m_hWnd, &ps );

	return hr;
}

HRESULT Player::ResizeVideo( WORD width, WORD height )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( m_pSession, E_POINTER );

		// Obtain reference to video display.
		ScopedComPtr<IMFVideoDisplayControl> pVideoDisplayControl;
		hr = MFGetService( m_pSession, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pVideoDisplayControl ) );
		BREAK_ON_FAIL( hr );

		RECT rcDest = { 0, 0, width, height };
		hr = pVideoDisplayControl->SetVideoPosition( NULL, &rcDest );
	} while( FALSE );

	return hr;
}

HRESULT Player::CreatePartialTopology( IMFPresentationDescriptor *pDescriptor )
{
	HRESULT hr = S_OK;

	do {
		Clear();

		ScopedComPtr<IMFTopology> pTopology;
		hr = CreatePlaybackTopology( m_pSource, pDescriptor, m_hWnd, &pTopology );
		BREAK_ON_FAIL( hr );

		hr = SetMediaInfo( pDescriptor );
		BREAK_ON_FAIL( hr );

		// Set the topology on the media session.
		hr = m_pSession->SetTopology( 0, pTopology );
		BREAK_ON_FAIL( hr );

		// If SetTopology succeeds, the media session will queue an MESessionTopologySet event.

		// Get the presentation clock interface.
		ScopedComPtr<IMFClock> pClock;
		hr = m_pSession->GetClock( &pClock );
		BREAK_ON_FAIL( hr );

		hr = pClock.QueryInterface( IID_PPV_ARGS( &m_pClock ) );
		BREAK_ON_FAIL( hr );

		// Get the rate control interface.
		hr = MFGetService( m_pSession, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS( &m_pRateControl ) );
		BREAK_ON_FAIL( hr );

		// Get the rate support interface.
		hr = MFGetService( m_pSession, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS( &m_pRateSupport ) );
		BREAK_ON_FAIL( hr );

		// Check if rate 0 (scrubbing) is supported.
		hr = m_pRateSupport->IsRateSupported( TRUE, 0, NULL );
		BREAK_ON_FAIL( hr );

		m_bCanScrub = TRUE;
	} while( false );

	// If m_pRateControl is NULL, m_bCanScrub must be FALSE.
	assert( m_pRateControl || !m_bCanScrub );

	return hr;
}

HRESULT Player::SetMediaInfo( IMFPresentationDescriptor *pDescriptor )
{
	HRESULT hr = S_OK;

	// Sanity check.
	assert( pDescriptor != NULL );

	do {
		DWORD count;
		hr = pDescriptor->GetStreamDescriptorCount( &count );
		BREAK_ON_FAIL( hr );

		// Get duration in 100-nano-second units.
		m_hnsDuration = 0L;
		hr = pDescriptor->GetUINT64( MF_PD_DURATION, (UINT64*)&m_hnsDuration );

		for( DWORD i = 0; i < count; i++ ) {
			BOOL selected;

			ScopedComPtr<IMFStreamDescriptor> pStreamDesc;
			hr = pDescriptor->GetStreamDescriptorByIndex( i, &selected, &pStreamDesc );
			BREAK_ON_FAIL( hr );

			if( selected ) {
				ScopedComPtr<IMFMediaTypeHandler> pHandler;
				hr = pStreamDesc->GetMediaTypeHandler( &pHandler );
				BREAK_ON_FAIL( hr );

				GUID guidMajorType = GUID_NULL;
				hr = pHandler->GetMajorType( &guidMajorType );
				BREAK_ON_FAIL( hr );

				if( MFMediaType_Video == guidMajorType ) {
					ScopedComPtr<IMFMediaType> pMediaType;
					hr = pHandler->GetCurrentMediaType( &pMediaType );
					BREAK_ON_FAIL( hr );

					hr = MFGetAttributeSize( pMediaType, MF_MT_FRAME_SIZE, &m_uiWidth, &m_uiHeight );
					BREAK_ON_FAIL( hr );
				}
			}
		}
	} while( false );

	return hr;
}

// Sets the playback rate.
HRESULT Player::SetRate( FLOAT fRate )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_critSec );

	do {
		BOOL bThin = FALSE;

		BREAK_IF_TRUE( fRate == GetNominalRate(), S_OK );
		BREAK_ON_NULL( m_pRateSupport, MF_E_INVALIDREQUEST );

		// Check if this rate is supported. Try non-thinned playback first,
		// then fall back to thinned playback.
		hr = m_pRateSupport->IsRateSupported( FALSE, fRate, NULL );
		if( FAILED( hr ) ) {
			bThin = TRUE;
			hr = m_pRateSupport->IsRateSupported( TRUE, fRate, NULL );
		}
		BREAK_ON_FAIL_MSG( hr, "Failed to check if rate is supported." );

		// Commit the new rate.
		hr = CommitRateChange( fRate, bThin );
	} while( FALSE );

	return hr;
}

BOOL Player::IsPlaying() const
{
	if( m_restore.command == CmdNone ) {
		State result;
		GetState( &result );
		return result.command == CmdStart;
	}
	else {
		return m_restore.command == CmdStart;
	}
}

BOOL Player::IsPaused() const
{
	if( m_restore.command == CmdNone ) {
		State result;
		GetState( &result );
		return result.command == CmdPause;
	}
	else {
		return m_restore.command == CmdPause;
	}
}

BOOL Player::IsStopped() const
{
	if( m_restore.command == CmdNone ) {
		State result;
		GetState( &result );
		return result.command == CmdStop;
	}
	else {
		return m_restore.command == CmdStop;
	}
}

// Sets the playback rate.
HRESULT Player::CommitRateChange( FLOAT fRate, BOOL bThin )
{
	HRESULT hr = S_OK;

	// Caller holds the lock.

	do {
		BREAK_ON_NULL_MSG( m_pRateControl, E_POINTER, "Invalid pointer." );

		// Allowed rate transitions:

		// Positive <-> negative:   Stopped
		// Negative <-> zero:       Stopped
		// Postive <-> zero:        Paused or stopped

		if( m_state.command != CmdStop && ( fRate > 0 && m_state.rate <= 0 ) || ( fRate < 0 && m_state.rate >= 0 ) ) {
			// Transition to stopped.
			hr = Stop();
			BREAK_ON_FAIL_MSG( hr, "Failed to stop." );
		}
		else if( m_state.command != CmdPause && fRate == 0 && m_state.rate != 0 ) {
			// Transition to paused.
			hr = Pause();
			BREAK_ON_FAIL_MSG( hr, "Failed to pause." );
		}

		// Set the rate.
		auto cmd = State( m_state, CmdRate );
		cmd.rate = fRate;
		cmd.thin = bThin;
		hr = QueueCommand( cmd );
	} while( FALSE );

	return hr;
}

// IMFAsyncCallback
HRESULT Player::Invoke( IMFAsyncResult *pResult )
{
	HRESULT hr = S_OK;
	IMFMediaEvent* pEvent = NULL;

	do {
		// Sometimes Invoke is called when m_pSession is closed.
		BREAK_ON_NULL( m_pSession, S_OK );

		// Get the event from the event queue.
		hr = m_pSession->EndGetEvent( pResult, &pEvent );
		BREAK_ON_FAIL_MSG( hr, "EndGetEvent failed." );

		// Get the event type.
		MediaEventType eventType = MEUnknown;
		hr = pEvent->GetType( &eventType );
		BREAK_ON_FAIL_MSG( hr, "GetType failed." );

		if( eventType == MESessionClosed ) {
			// The session was closed. The application is waiting on the m_hCloseEvent event handle. 
			SetEvent( m_hCloseEvent );
		}
		else {
			// For all other events, get the next event in the queue.
			hr = m_pSession->BeginGetEvent( this, NULL );
			BREAK_ON_FAIL_MSG( hr, "BeginGetEvent failed." );
		}

		// If a call to IMFMediaSession::Close is pending, it means the 
		// application is waiting on the m_hCloseEvent event and
		// the application's message loop is blocked.
		if( m_state.command != CmdClose ) {
			// Leave a reference count on the event.
			pEvent->AddRef();

			// Post a private window message to the application.
			::PostMessage( m_hWnd, WM_APP_PLAYER_EVENT, (WPARAM)pEvent, (LPARAM)eventType );
		}
	} while( false );

	SafeRelease( pEvent );

	return hr;
}

//

LRESULT Player::HandleEvent( WPARAM wParam )
{
	HRESULT hr = S_OK;
	IMFMediaEvent *pEvent = (IMFMediaEvent*)wParam;

	do {
		BREAK_ON_NULL_MSG( pEvent, E_POINTER, "Invalid pointer." );

		// Get the event type.
		MediaEventType eventType = MEUnknown;
		hr = pEvent->GetType( &eventType );
		BREAK_ON_FAIL_MSG( hr, "Failed to query event type." );

		// Get the event status. If the operation that triggered the event 
		// did not succeed, the status is a failure code.
		HRESULT eventStatus = S_OK;
		hr = pEvent->GetStatus( &eventStatus );
		BREAK_ON_FAIL_MSG( hr, "Failed to query status." << eventType );

		// Check if the async operation succeeded.
		hr = eventStatus;
		BREAK_ON_FAIL_MSG( hr, "Failed async operation." << eventType );

		// Handle event.
		switch( eventType ) {
			case MESessionTopologyStatus:
				CI_LOG_PLAYER( "MESessionTopologyStatus." );
				hr = OnTopologyStatus( pEvent );
				break;

			case MEEndOfPresentation:
				CI_LOG_PLAYER( "MEEndOfPresentation." );
				hr = OnPresentationEnded( pEvent );
				break;

			case MENewPresentation:
				CI_LOG_PLAYER( "MENewPresentation." );
				hr = OnNewPresentation( pEvent );
				break;

			case MESessionTopologySet:
				CI_LOG_PLAYER( "MESessionTopologySet." );
				break;

			case MESessionStarted:
				CI_LOG_PLAYER( "MESessionStarted." );
				hr = OnSessionStart( hr );
				break;

			case MESessionStopped:
				CI_LOG_PLAYER( "MESessionStopped." );
				hr = OnSessionStop( hr );
				break;

			case MESessionPaused:
				CI_LOG_PLAYER( "MESessionPaused." );
				hr = OnSessionPause( hr );
				break;

			case MESessionRateChanged:
				CI_LOG_PLAYER( "MESessionRateChanged." );
				hr = OnSessionRateChanged( pEvent, hr );
				break;

			case MESessionEnded:
				CI_LOG_PLAYER( "MESessionEnded." );
				hr = OnSessionEnded( hr );
				break;

			case MESessionScrubSampleComplete:
				CI_LOG_PLAYER( "MESessionScrubSampleComplete." );
				hr = OnSessionScrubSampleComplete( hr );
				break;

			case MESessionCapabilitiesChanged:
				CI_LOG_PLAYER( "MESessionCapabilitiesChanged." );
				// The session capabilities changed. Get the updated capabilities.
				m_caps = MFGetAttributeUINT32( pEvent, MF_EVENT_SESSIONCAPS, m_caps );
				break;

			case MESessionStreamSinkFormatChanged:
				CI_LOG_PLAYER( "MESessionStreamSinkFormatChanged." );
				break;

			case MESessionNotifyPresentationTime:
				CI_LOG_PLAYER( "MESessionNotifyPresentationTime." );
				break;

			case MEError:
				CI_LOG_E( "MEError." );
				break;

			default:
				CI_LOG_PLAYER( "OnSessionEvent " << eventType );
				hr = OnSessionEvent( pEvent, eventType );
				break;
		}
	} while( false );

	SafeRelease( pEvent );

	return hr;
}

HRESULT Player::CreateWnd()
{
	if( !m_hWnd ) {
		RegisterWindowClass();

		RECT windowRect;
		windowRect.left = 0;
		windowRect.top = 0;
		windowRect.right = m_windowFormat.getSize().x;
		windowRect.bottom = m_windowFormat.getSize().y;

		// Adjust Window To True Requested Size
		::AdjustWindowRectEx( &windowRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW );

		std::wstring unicodeTitle = ci::msw::toWideString( m_windowFormat.getTitle() );
		m_hWnd = ::CreateWindowEx( WS_EX_APPWINDOW, MF_WINDOW_CLASS_NAME, unicodeTitle.c_str(), WS_OVERLAPPEDWINDOW,
								   CW_USEDEFAULT, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
								   NULL, NULL, ::GetModuleHandle( NULL ), reinterpret_cast<LPVOID>( this ) );

		if( m_hWnd ) {
			//::ShowWindow( m_hWnd, SW_SHOW );
			//::UpdateWindow( m_hWnd );
		}
	}

	if( m_hWnd )
		return S_OK;
	else
		return E_FAIL;
}

HRESULT Player::DestroyWnd()
{
	if( m_hWnd )
		::DestroyWindow( m_hWnd );

	m_hWnd = NULL;

	return S_OK;
}

void Player::RegisterWindowClass()
{
	static bool sRegistered = false;

	if( sRegistered )
		return;

	WNDCLASS	wc;
	HMODULE instance = ::GetModuleHandle( NULL );
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MFWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = instance;
	wc.hIcon = ::LoadIcon( NULL, IDI_WINLOGO );
	wc.hCursor = ::LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = NULL; // Don't specify brush to eliminate flicker.
	wc.lpszMenuName = NULL;
	wc.lpszClassName = MF_WINDOW_CLASS_NAME;

	if( !::RegisterClass( &wc ) ) {
		DWORD err = ::GetLastError();
		return;
	}

	sRegistered = true;
}

//  Create a media source from a URL.
HRESULT Player::CreateMediaSource( LPCWSTR pUrl, IMFMediaSource **ppSource ) const
{
	HRESULT hr = S_OK;

	do {
		// Create the source resolver.
		ScopedComPtr<IMFSourceResolver> pSourceResolver;
		hr = MFCreateSourceResolver( &pSourceResolver );
		BREAK_ON_FAIL( hr );

		// Use the source resolver to create the media source.

		// Note: For simplicity this sample uses the synchronous method to create 
		// the media source. However, creating a media source can take a noticeable
		// amount of time, especially for a network source. For a more responsive 
		// UI, use the asynchronous BeginCreateObjectFromURL method.

		const DWORD dwFlags = MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE;
		MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
		ScopedComPtr<IUnknown> pSource;
		hr = pSourceResolver->CreateObjectFromURL( pUrl, dwFlags, NULL, &objectType, &pSource );
		BREAK_ON_FAIL( hr );

		// Get the IMFMediaSource interface from the media source.
		hr = pSource.QueryInterface( __uuidof( **ppSource ), (void**)ppSource );
	} while( false );

	return hr;
}

//  Create a playback topology from a media source.
HRESULT Player::CreatePlaybackTopology( IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology ) const
{
	HRESULT hr = S_OK;

	do {
		// Create a new topology.
		ScopedComPtr<IMFTopology> pTopology;
		hr = MFCreateTopology( &pTopology );
		BREAK_ON_FAIL( hr );

		// Get the number of streams in the media source.
		DWORD dwSourceStreams = 0;
		hr = pPD->GetStreamDescriptorCount( &dwSourceStreams );
		BREAK_ON_FAIL( hr );

		// For each stream, create the topology nodes and add them to the topology.
		for( DWORD i = 0; i < dwSourceStreams; i++ ) {
			hr = AddBranchToPartialTopology( pTopology, pSource, pPD, i, hVideoWnd );
			BREAK_ON_FAIL( hr );
		}
		BREAK_ON_FAIL( hr );

		// Return the IMFTopology pointer to the caller.
		*ppTopology = pTopology;
		( *ppTopology )->AddRef();
	} while( false );

	return hr;
}

//  Create an activation object for a renderer, based on the stream media type.
HRESULT Player::CreateMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate ) const
{
	HRESULT hr = S_OK;

	do {
		// Get the media type handler for the stream.
		ScopedComPtr<IMFMediaTypeHandler> pHandler;
		hr = pSourceSD->GetMediaTypeHandler( &pHandler );
		BREAK_ON_FAIL( hr );

		// Get the major media type.
		GUID guidMajorType;
		hr = pHandler->GetMajorType( &guidMajorType );
		BREAK_ON_FAIL( hr );

		// Create an IMFActivate object for the renderer, based on the media type.
		if( MFMediaType_Audio == guidMajorType ) {
			// Create the audio renderer.
			ScopedComPtr<IMFActivate> pActivate;
			hr = MFCreateAudioRendererActivate( &pActivate );
			BREAK_ON_FAIL( hr );

			// Return IMFActivate pointer to caller.
			*ppActivate = pActivate;
			( *ppActivate )->AddRef();
		}
		else if( MFMediaType_Video == guidMajorType ) {
			// Create our own Presenter.
			ScopedComPtr<IMFActivate> pActivate;
			hr = Activate::CreateInstance( hVideoWindow, &pActivate, m_options );

			if( FAILED( hr ) ) {
				CI_LOG_E( "Failed to create custom Activate, falling back to default." );

				// Use default presenter (EVR).
				hr = MFCreateVideoRendererActivate( hVideoWindow, &pActivate );
				BREAK_ON_FAIL( hr );
			}

			// Return IMFActivate pointer to caller.
			*ppActivate = pActivate;
			( *ppActivate )->AddRef();
		}
		else {
			// Unknown stream type.
			hr = E_FAIL;
			// Optionally, you could deselect this stream instead of failing.
		}
	} while( false );

	return hr;
}

// Add a source node to a topology.
HRESULT Player::AddSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode ) const
{
	HRESULT hr = S_OK;

	do {
		// Create the node.
		ScopedComPtr<IMFTopologyNode> pNode;
		hr = MFCreateTopologyNode( MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode );
		BREAK_ON_FAIL( hr );

		// Set the attributes.
		hr = pNode->SetUnknown( MF_TOPONODE_SOURCE, pSource );
		BREAK_ON_FAIL( hr );

		hr = pNode->SetUnknown( MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD );
		BREAK_ON_FAIL( hr );

		hr = pNode->SetUnknown( MF_TOPONODE_STREAM_DESCRIPTOR, pSD );
		BREAK_ON_FAIL( hr );

		// Add the node to the topology.
		hr = pTopology->AddNode( pNode );
		BREAK_ON_FAIL( hr );

		// Return the pointer to the caller.
		*ppNode = pNode;
		( *ppNode )->AddRef();
	} while( false );

	return hr;
}

HRESULT Player::AddOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode ) const
{
	HRESULT hr = S_OK;

	do {
		// Create the node.
		ScopedComPtr<IMFTopologyNode> pNode;
		hr = MFCreateTopologyNode( MF_TOPOLOGY_OUTPUT_NODE, &pNode );
		BREAK_ON_FAIL( hr );

		// Set the object pointer.
		hr = pNode->SetObject( pStreamSink );
		BREAK_ON_FAIL( hr );

		// Add the node to the topology.
		hr = pTopology->AddNode( pNode );
		BREAK_ON_FAIL( hr );

		hr = pNode->SetUINT32( MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, TRUE );
		BREAK_ON_FAIL( hr );

		// Return the pointer to the caller.
		*ppNode = pNode;
		( *ppNode )->AddRef();
	} while( false );

	return hr;
}

// Add an output node to a topology.
HRESULT Player::AddOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode ) const
{
	HRESULT hr = S_OK;

	do {
		ScopedComPtr<IMFTopologyNode> pNode;

		// Create the node.
		hr = MFCreateTopologyNode( MF_TOPOLOGY_OUTPUT_NODE, &pNode );
		BREAK_ON_FAIL( hr );

		// Set the object pointer.
		hr = pNode->SetObject( pActivate );
		BREAK_ON_FAIL( hr );

		// Set the stream sink ID attribute.
		hr = pNode->SetUINT32( MF_TOPONODE_STREAMID, dwId );
		BREAK_ON_FAIL( hr );

		hr = pNode->SetUINT32( MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE );
		BREAK_ON_FAIL( hr );

		// Add the node to the topology.
		hr = pTopology->AddNode( pNode );
		BREAK_ON_FAIL( hr );

		// Return the pointer to the caller.
		*ppNode = pNode;
		( *ppNode )->AddRef();
	} while( false );

	return hr;
}

//  Add a topology branch for one stream.
//
//  For each stream, this function does the following:
//
//    1. Creates a source node associated with the stream. 
//    2. Creates an output node for the renderer. 
//    3. Connects the two nodes.
//
//  The media session will add any decoders that are needed.

HRESULT Player::AddBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd ) const
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( pPD, E_POINTER );

		BOOL fSelected = FALSE;

		ScopedComPtr<IMFStreamDescriptor> pSD;
		hr = pPD->GetStreamDescriptorByIndex( iStream, &fSelected, &pSD );
		BREAK_ON_FAIL( hr );

		if( fSelected ) {
			// Create the media sink activation object.
			ScopedComPtr<IMFActivate> pSinkActivate;
			hr = CreateMediaSinkActivate( pSD, hVideoWnd, &pSinkActivate );
			BREAK_ON_FAIL( hr );

			// Add a source node for this stream.
			ScopedComPtr<IMFTopologyNode> pSourceNode;
			hr = AddSourceNode( pTopology, pSource, pPD, pSD, &pSourceNode );
			BREAK_ON_FAIL( hr );

			// Create the output node for the renderer.
			ScopedComPtr<IMFTopologyNode> pOutputNode;
			hr = AddOutputNode( pTopology, pSinkActivate, 0, &pOutputNode );
			BREAK_ON_FAIL( hr );

			// Connect the source node to the output node.
			hr = pSourceNode->ConnectOutput( 0, pOutputNode, 0 );
			BREAK_ON_FAIL( hr );
		}
	} while( false );

	return hr;
}

} // namespace msw
} // namespace cinder

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )
