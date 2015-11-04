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

#include "cinder/wmf/Player.h"
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/wmf/Activate.h"
#include "cinder/wmf/PresenterDX9.h"
#include "cinder/wmf/PresenterDX11.h"

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

static const UINT WM_APP_PLAYER_EVENT = WM_APP + 1; // TODO: come up with better name

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

Player::Player( DirectXVersion dxVersion )
	: mRefCount( 1 )
	, mState( Closed )
	, mCommand( CmdNone )
	, mWnd( NULL )
	, mIsLoop( FALSE )
	, mIsPending( FALSE )
	, mCanScrub( FALSE )
	, mWidth( 0 )
	, mHeight( 0 )
	, mDuration( 0 )
	, mSeekTo( 0 )
	, mCapabilities( 0 )
	, mSessionPtr( NULL )
	, mSourcePtr( NULL )
	, mClockPtr( NULL )
	, mRateControlPtr( NULL )
	, mRateSupportPtr( NULL )
	, mDxVersion( dxVersion )
{
	mCloseEvent = ::CreateEventA( NULL, FALSE, FALSE, NULL );
	if( mCloseEvent == NULL )
		throw Exception( "Player: failed to create Close event." );

	CreateWnd();
}

Player::~Player()
{
	DestroyWnd();

	if( mCloseEvent ) {
		::CloseHandle( mCloseEvent );
		mCloseEvent = NULL;
	}
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
	else if( iid == __uuidof( PresenterDX9 ) ) {
		if( NULL == mSessionPtr ) {
			*ppv = NULL;
			return E_NOINTERFACE;
		}

		ScopedComPtr<IMFVideoDisplayControl> pVideoDisplayControl;
		if( FAILED( MFGetService( mSessionPtr, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pVideoDisplayControl ) ) ) ) {
			*ppv = NULL;
			return E_NOINTERFACE;
		}

		return pVideoDisplayControl.QueryInterface( __uuidof( PresenterDX9 ), ppv );
	}
	else if( iid == __uuidof( PresenterDX11 ) ) {
		if( NULL == mSessionPtr ) {
			*ppv = NULL;
			return E_NOINTERFACE;
		}

		ScopedComPtr<IMFVideoDisplayControl> pVideoDisplayControl;
		if( FAILED( MFGetService( mSessionPtr, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pVideoDisplayControl ) ) ) ) {
			*ppv = NULL;
			return E_NOINTERFACE;
		}

		return pVideoDisplayControl.QueryInterface( __uuidof( PresenterDX11 ), ppv );
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
	return ::InterlockedIncrement( &mRefCount );
}

// IUnknown
ULONG Player::Release()
{
	ULONG uCount = ::InterlockedDecrement( &mRefCount );
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

	*pbCanSeek = ( ( ( mCapabilities & MFSESSIONCAP_SEEK ) == MFSESSIONCAP_SEEK ) && ( mDuration > 0 ) && ( mClockPtr != NULL ) );
	return S_OK;
}

// Player
HRESULT Player::CanFastForward( BOOL *pbCanFF ) const
{
	if( pbCanFF == NULL ) {
		return E_POINTER;
	}

	*pbCanFF = ( ( mCapabilities & MFSESSIONCAP_RATE_FORWARD ) == MFSESSIONCAP_RATE_FORWARD );
	return S_OK;
}

// Player
HRESULT Player::CanRewind( BOOL *pbCanRewind ) const
{
	if( pbCanRewind == NULL ) {
		return E_POINTER;
	}

	*pbCanRewind = ( ( mCapabilities & MFSESSIONCAP_RATE_REVERSE ) == MFSESSIONCAP_RATE_REVERSE );
	return S_OK;
}

// Player
HRESULT Player::OpenURL( const WCHAR *url, const WCHAR *audioDeviceId )
{
	HRESULT hr = S_OK;

	// Sanity check.
	assert( mState == Closed );
	assert( url );

	do {
		// Create the media session.
		hr = CreateSession();
		BREAK_ON_FAIL( hr );

		// Create the media source.
		hr = CreateMediaSource( url, &mSourcePtr );
		BREAK_ON_FAIL( hr );

		// Create the presentation descriptor for the media source.
		ScopedComPtr<IMFPresentationDescriptor> pDescriptor;
		hr = mSourcePtr->CreatePresentationDescriptor( &pDescriptor );
		BREAK_ON_FAIL( hr );

		// Create a partial topology.
		hr = CreatePartialTopology( pDescriptor );
		BREAK_ON_FAIL( hr );

		// If SetTopology succeeds, the media session will queue an MESessionTopologySet event.
		mState = Opening;
		mIsPending = TRUE;
	} while( false );

	if( FAILED( hr ) )
		CloseSession();

	return hr;
}

// Player
HRESULT Player::Start()
{
	ScopedCriticalSection lock( mCritSec );

	if( mIsPending ) {
		mCommand = CmdStart;
		return S_OK;
	}
	else if( mState != Paused && mState != Stopped )
		return MF_E_INVALIDREQUEST;

	if( mSessionPtr == NULL || mSourcePtr == NULL )
		return E_POINTER;

	PROPVARIANT varStart;
	PropVariantInit( &varStart );

	// Seek to start if near the end (less than a second to go)
	MFTIME position = 0L;
	if( SUCCEEDED( GetPosition( &position ) ) ) {
		if( ( position + 10000000L ) >= mDuration ) {
			varStart.vt = VT_I8;
			varStart.intVal = 0L;
		}
	}

	HRESULT hr = mSessionPtr->Start( &GUID_NULL, &varStart );
	if( SUCCEEDED( hr ) ) {
		mState = Started;
		mIsPending = TRUE;
	}

	PropVariantClear( &varStart );

	return hr;
}

// Player
HRESULT Player::Pause()
{
	ScopedCriticalSection lock( mCritSec );

	if( mIsPending ) {
		mCommand = CmdPause;
		return S_OK;
	}
	else if( mState != Started )
		return MF_E_INVALIDREQUEST;

	if( mSessionPtr == NULL || mSourcePtr == NULL )
		return E_POINTER;

	HRESULT hr = mSessionPtr->Pause();
	if( SUCCEEDED( hr ) ) {
		mState = Paused;
		mIsPending = TRUE;
	}

	return hr;
}

// Player
HRESULT Player::Stop()
{
	ScopedCriticalSection lock( mCritSec );

	if( mIsPending ) {
		mCommand = CmdStop;
		return S_OK;
	}
	else if( mState != Started && mState != Paused )
		return MF_E_INVALIDREQUEST;

	if( mSessionPtr == NULL || mSourcePtr == NULL )
		return E_POINTER;

	HRESULT hr = mSessionPtr->Stop();
	if( SUCCEEDED( hr ) ) {
		mState = Stopped;
		mIsPending = TRUE;
	}

	return hr;
}

HRESULT Player::GetPosition( MFTIME *pPosition )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( mCritSec );
	
	do {
		BREAK_ON_NULL( pPosition, E_POINTER );
		BREAK_ON_NULL( mClockPtr, E_POINTER );

		BREAK_IF_TRUE( mState == Opening, MF_E_INVALIDREQUEST );

		// Return, in order:
		// 1. Cached seek request (nominal position).
		// 2. Pending seek operation (nominal position).
		// 3. Presentation time (actual position).
		if( mCommand == CmdSeek ) {
			*pPosition = mSeekTo;
		}
		else if( mIsPending && mState == Seeking ) {
			*pPosition = mSeekTo;
		}
		else {
			hr = mClockPtr->GetTime( pPosition );
		}
	} while( FALSE );

	return hr;
}

HRESULT Player::SetPosition( MFTIME position )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( mCritSec );

	do {
		if( mIsPending ) {
			// Currently seeking or changing rates, so cache this request.
			mCommand = CmdSeek;
			mSeekTo = position;
		}
		else {
			hr = SetPositionInternal( position );
		}
	} while( FALSE );

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
	UpdatePendingCommands( CmdStart );

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

	UpdatePendingCommands( CmdStop );

	return hr;
}

HRESULT Player::OnSessionPause( HRESULT hrStatus )
{
	HRESULT hr = S_OK;

	if( FAILED( hrStatus ) ) {
		return hrStatus;
	}

	hr = UpdatePendingCommands( CmdPause );

	return hr;
}

HRESULT Player::OnSessionEnded( HRESULT hr )
{
	// After the session ends, playback starts from position zero. But if the
	// current playback rate is reversed, playback would end immediately 
	// (reversing from position 0). Therefore, reset the rate to 1x.

	/*if( GetNominalRate() < 0.0f ) {
		m_state.command = CmdStop;

		hr = CommitRateChange( 1.0f, FALSE );
	}*/

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
			// Get the IMFVideoDisplayControl interface from EVR. This call is
			// expected to fail if the media file does not have a video stream.
			ScopedComPtr<IMFVideoDisplayControl> pVideoDisplayControl;
			hr = MFGetService( mSessionPtr, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pVideoDisplayControl ) );
			if( FAILED( hr ) ) {
				CI_LOG_E( "Failed to get IMFVideoDisplayControl interface from EVR. " << HRESULT_STRING( hr ) );
				__debugbreak();
			}

			mState = Paused;

			UpdatePendingCommands( CmdPause );
		}
	} while( FALSE );

	return hr;
}

HRESULT Player::OnPresentationEnded( IMFMediaEvent *pEvent )
{
	HRESULT hr = S_OK;

	if( mIsLoop ) {
		// Seek to the beginning.
		hr = SetPosition( 0 );
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

		mState = Opening;
	} while( false );

	return hr;
}

HRESULT Player::UpdatePendingCommands( Command cmd )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( mCritSec );

	PROPVARIANT varStart;
	PropVariantInit( &varStart );

	do {
		if( mIsPending && mState == (State)cmd ) {
			mIsPending = FALSE;

			// The current pending command has completed.

			//// First look for rate changes.
			//if( m_request.fRate != m_state.fRate ) {
			//	hr = CommitRateChange( m_request.fRate, m_request.bThin );
			//	if( FAILED( hr ) ) {
			//		goto done;
			//	}
			//}

			// Now look for seek requests.
			if( !mIsPending ) {
				switch( mCommand ) {
					case CmdNone:
						// Nothing to do.
						break;

					case CmdStart:
						Start();
						break;

					case CmdPause:
						Pause();
						break;

					case CmdStop:
						Stop();
						break;

					case CmdSeek:
						SetPositionInternal( mSeekTo );
						break;
				}

				mCommand = CmdNone;
			}
		}
	} while( FALSE );

	return hr;
}

HRESULT Player::CreateSession()
{
	HRESULT hr = S_OK;

	do {
		// Close the old session.
		hr = CloseSession();
		BREAK_ON_FAIL( hr );

		// Sanity check.
		assert( mState == Closed );
		assert( mSessionPtr == NULL );

		// Create the media session.
		hr = ::MFCreateMediaSession( NULL, &mSessionPtr );
		BREAK_ON_FAIL( hr );

		// Start pulling events from the media session
		hr = mSessionPtr->BeginGetEvent( ( IMFAsyncCallback* ) this, NULL );
		BREAK_ON_FAIL( hr );

		mState = Ready;
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
	if( mSessionPtr ) {
		DWORD dwWaitResult = 0;

		mState = Closing;

		hr = mSessionPtr->Close();

		// Wait for the close operation to complete
		if( SUCCEEDED( hr ) ) {
			dwWaitResult = WaitForSingleObject( mCloseEvent, 5000 );
			if( dwWaitResult == WAIT_TIMEOUT ) {
				CI_LOG_W( "Timeout closing session." );
			}
			// Now there will be no more events from this session.
		}
	}

	// Complete shutdown operations.
	if( SUCCEEDED( hr ) ) {
		// Shut down the media source. (Synchronous operation, no events.)
		if( mSourcePtr ) {
			(void)mSourcePtr->Shutdown();
		}

		// Shut down the media session. (Synchronous operation, no events.)
		if( mSessionPtr ) {
			(void)mSessionPtr->Shutdown();
		}
	}

	SafeRelease( mRateSupportPtr );
	SafeRelease( mRateControlPtr );
	SafeRelease( mClockPtr );
	SafeRelease( mSourcePtr );
	SafeRelease( mSessionPtr );

	mState = Closed;

	return hr;
}

HRESULT Player::Repaint()
{
	HRESULT hr = S_OK;

	PAINTSTRUCT ps;
	::BeginPaint( mWnd, &ps );

	do {
		BREAK_ON_NULL( mSessionPtr, E_POINTER );

		// Obtain reference to video display.
		ScopedComPtr<IMFVideoDisplayControl> pVideoDisplayControl;
		hr = MFGetService( mSessionPtr, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pVideoDisplayControl ) );
		BREAK_ON_FAIL( hr );

		pVideoDisplayControl->RepaintVideo();
	} while( FALSE );

	::EndPaint( mWnd, &ps );

	return hr;
}

HRESULT Player::ResizeVideo( WORD width, WORD height )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( mSessionPtr, E_POINTER );

		// Obtain reference to video display.
		ScopedComPtr<IMFVideoDisplayControl> pVideoDisplayControl;
		hr = MFGetService( mSessionPtr, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &pVideoDisplayControl ) );
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
		ScopedComPtr<IMFTopology> pTopology;
		hr = CreatePlaybackTopology( mSourcePtr, pDescriptor, mWnd, &pTopology );
		BREAK_ON_FAIL( hr );

		hr = SetMediaInfo( pDescriptor );
		BREAK_ON_FAIL( hr );

		// Set the topology on the media session.
		hr = mSessionPtr->SetTopology( 0, pTopology );
		BREAK_ON_FAIL( hr );

		// If SetTopology succeeds, the media session will queue an MESessionTopologySet event.

		// Get the presentation clock interface.
		ScopedComPtr<IMFClock> pClock;
		hr = mSessionPtr->GetClock( &pClock );
		BREAK_ON_FAIL( hr );

		hr = pClock.QueryInterface( IID_PPV_ARGS( &mClockPtr ) );
		BREAK_ON_FAIL( hr );

		// Get the rate control interface.
		hr = MFGetService( mSessionPtr, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS( &mRateControlPtr ) );
		BREAK_ON_FAIL( hr );

		// Get the rate support interface.
		hr = MFGetService( mSessionPtr, MF_RATE_CONTROL_SERVICE, IID_PPV_ARGS( &mRateSupportPtr ) );
		BREAK_ON_FAIL( hr );

		// Check if rate 0 (scrubbing) is supported.
		hr = mRateSupportPtr->IsRateSupported( TRUE, 0, NULL );
		BREAK_ON_FAIL( hr );

		mCanScrub = TRUE;
	} while( false );

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
		mDuration = 0L;
		hr = pDescriptor->GetUINT64( MF_PD_DURATION, (UINT64*)&mDuration );

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

					hr = MFGetAttributeSize( pMediaType, MF_MT_FRAME_SIZE, &mWidth, &mHeight );
					BREAK_ON_FAIL( hr );
				}
			}
		}
	} while( false );

	return hr;
}

HRESULT Player::SetPositionInternal( MFTIME position )
{
	assert( !mIsPending );

	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( mSessionPtr, E_POINTER );

		BREAK_IF_TRUE( mState == Opening, MF_E_INVALIDREQUEST );

		BOOL bPaused = ( mState == Paused );
		if( !bPaused ) {
			hr = mSessionPtr->Pause();
			BREAK_ON_FAIL( hr );
		}

		PROPVARIANT varStart;
		PropVariantInit( &varStart );
		varStart.vt = VT_I8;
		varStart.hVal.QuadPart = position;

		hr = mSessionPtr->Start( &GUID_NULL, &varStart );
		if( SUCCEEDED( hr ) ) {
			mState = Started;
			mIsPending = TRUE;
		}

		PropVariantClear( &varStart );

		if( bPaused ) {
			hr = mSessionPtr->Pause();
			mState = Paused;
			mIsPending = TRUE;
		}
	} while( FALSE );

	return hr;
}

// IMFAsyncCallback
HRESULT Player::Invoke( IMFAsyncResult *pResult )
{
	HRESULT hr = S_OK;
	IMFMediaEvent* pEvent = NULL;

	do {
		// Sometimes Invoke is called when mSessionPtr is closed.
		BREAK_ON_NULL( mSessionPtr, S_OK );

		// Get the event from the event queue.
		hr = mSessionPtr->EndGetEvent( pResult, &pEvent );
		BREAK_ON_FAIL( hr );

		// Get the event type.
		MediaEventType eventType = MEUnknown;
		hr = pEvent->GetType( &eventType );
		BREAK_ON_FAIL( hr );

		if( eventType == MESessionClosed ) {
			// The session was closed. The application is waiting on the mCloseEvent event handle. 
			SetEvent( mCloseEvent );
		}
		else {
			// For all other events, get the next event in the queue.
			hr = mSessionPtr->BeginGetEvent( this, NULL );
			BREAK_ON_FAIL( hr );
		}

		// If a call to IMFMediaSession::Close is pending, it means the 
		// application is waiting on the mCloseEvent event and
		// the application's message loop is blocked.
		if( mState != Closing ) {
			// Leave a reference count on the event.
			pEvent->AddRef();

			// Post a private window message to the application.
			::PostMessage( mWnd, WM_APP_PLAYER_EVENT, (WPARAM)pEvent, (LPARAM)eventType );
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
		BREAK_ON_NULL( pEvent, E_POINTER );

		// Get the event type.
		MediaEventType eventType = MEUnknown;
		hr = pEvent->GetType( &eventType );
		BREAK_ON_FAIL( hr );

		// Get the event status. If the operation that triggered the event 
		// did not succeed, the status is a failure code.
		HRESULT eventStatus = S_OK;
		hr = pEvent->GetStatus( &eventStatus );
		BREAK_ON_FAIL( hr );

		// Check if the async operation succeeded.
		hr = eventStatus;
		BREAK_ON_FAIL( hr );

		// Handle event.
		switch( eventType ) {
			case MESessionTopologyStatus:
				hr = OnTopologyStatus( pEvent );
				break;

			case MEEndOfPresentation:
				hr = OnPresentationEnded( pEvent );
				break;

			case MENewPresentation:
				hr = OnNewPresentation( pEvent );
				break;

			case MESessionTopologySet:
				break;

			case MESessionStarted:
				hr = OnSessionStart( hr );
				break;

			case MESessionStopped:
				hr = OnSessionStop( hr );
				break;

			case MESessionPaused:
				hr = OnSessionPause( hr );
				break;

			case MESessionRateChanged:
				//hr = OnSessionRateChanged( pEvent );
				break;

			case MESessionEnded:
				hr = OnSessionEnded( hr );
				break;

			case MESessionCapabilitiesChanged:
				// The session capabilities changed. Get the updated capabilities.
				mCapabilities = MFGetAttributeUINT32( pEvent, MF_EVENT_SESSIONCAPS, mCapabilities );
				break;

			default:
				hr = OnSessionEvent( pEvent, eventType );
				break;
		}
	} while( false );

	SafeRelease( pEvent );

	return 0;
}

void Player::CreateWnd()
{
	if( !mWnd ) {
		RegisterWindowClass();

		RECT windowRect;
		windowRect.left = 0;
		windowRect.top = 0;
		windowRect.right = mWindowFormat.getSize().x;
		windowRect.bottom = mWindowFormat.getSize().y;

		// Adjust Window To True Requested Size
		::AdjustWindowRectEx( &windowRect, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_APPWINDOW );

		std::wstring unicodeTitle = ci::msw::toWideString( mWindowFormat.getTitle() );
		mWnd = ::CreateWindowEx( WS_EX_APPWINDOW, MF_WINDOW_CLASS_NAME, unicodeTitle.c_str(), WS_OVERLAPPEDWINDOW,
								 CW_USEDEFAULT, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
								 NULL, NULL, ::GetModuleHandle( NULL ), reinterpret_cast<LPVOID>( this ) );
		if( !mWnd )
			throw Exception( "Player: failed to create window." );

		//::ShowWindow( mWnd, SW_SHOW );
		//::UpdateWindow( mWnd );
	}
}

void Player::DestroyWnd()
{
	if( mWnd )
		::DestroyWindow( mWnd );
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
			hr = Activate::CreateInstance( hVideoWindow, &pActivate, mDxVersion );

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
