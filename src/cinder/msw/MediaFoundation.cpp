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

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/ScopedPtr.h"

#include "cinder/msw/detail/Activate.h"

#include <string>
#include <Shlwapi.h>

// Include these libraries.
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

#pragma comment(lib, "winmm.lib") // for timeBeginPeriod and timeEndPeriod
#pragma comment (lib,"uuid.lib")

//#pragma comment(lib,"dxva2.lib")
//#pragma comment (lib,"evr.lib")
//#pragma comment (lib,"dcomp.lib")

namespace cinder {
namespace msw {

static const wchar_t *MF_WINDOW_CLASS_NAME = TEXT( "MFPlayer" );

static const UINT WM_APP_PLAYER_EVENT = WM_APP + 1; // TODO: come up with better name

LRESULT CALLBACK MFWndProc( HWND wnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	MFPlayer *impl = NULL;

	// If the message is WM_NCCREATE we need to hide 'this' in the window long.
	if( uMsg == WM_NCCREATE ) {
		impl = reinterpret_cast<MFPlayer*>( ( (LPCREATESTRUCT)lParam )->lpCreateParams );
		::SetWindowLongPtr( wnd, GWLP_USERDATA, (__int3264)(LONG_PTR)impl );
	}
	else
		impl = reinterpret_cast<MFPlayer*>( ::GetWindowLongPtr( wnd, GWLP_USERDATA ) );
	
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

MFPlayer::MFPlayer()
	: mRefCount( 1 ), mState( Closed ), mWnd( NULL )
	, mPlayWhenReady( FALSE ), mIsLooping( FALSE ), mWidth( 0 ), mHeight( 0 )
	, mSessionPtr( NULL ), mSourcePtr( NULL ), mPresenterPtr( NULL ) /* mSinkPtr( NULL ) */, mVideoDisplayPtr( NULL )
{
	mCloseEvent = ::CreateEventA( NULL, FALSE, FALSE, NULL );
	if( mCloseEvent == NULL )
		throw Exception( "MFPlayer: failed to create Close event." );

	CreateWnd();

	// Create custom EVR presenter.
	/*mPresenterPtr = new EVRCustomPresenter();

	HRESULT hr = static_cast<EVRCustomPresenter*>( mPresenterPtr )->SetVideoWindow( mWnd );
	if( FAILED( hr ) )
		throw Exception( "MFPlayer: failed to set video window." );*/
}

MFPlayer::~MFPlayer()
{
	// Destroy EVR custom presenter.
	SafeRelease( mPresenterPtr );

	DestroyWnd();

	if( mCloseEvent ) {
		::CloseHandle( mCloseEvent );
		mCloseEvent = NULL;
	}
}

HRESULT MFPlayer::OpenURL( const WCHAR *url, const WCHAR *audioDeviceId )
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
		ScopedComPtr<IMFTopology> pTopology;
		hr = CreatePlaybackTopology( mSourcePtr, pDescriptor, mWnd, &pTopology, mPresenterPtr );
		BREAK_ON_FAIL( hr );

		SetMediaInfo( pDescriptor );

		// Set the topology on the media session.
		hr = mSessionPtr->SetTopology( 0, pTopology );
		BREAK_ON_FAIL( hr );

		// If SetTopology succeeds, the media session will queue an MESessionTopologySet event.
		mState = OpenPending;

	} while( false );

	return hr;
}

HRESULT MFPlayer::Play()
{
	if( mState == OpenPending ) {
		mPlayWhenReady = TRUE;
		return S_OK;
	}
	else if( mState != Paused && mState != Stopped )
		return MF_E_INVALIDREQUEST;

	if( mSessionPtr == NULL || mSourcePtr == NULL )
		return E_POINTER;

	PROPVARIANT varStart;
	PropVariantInit( &varStart );

	// Note: Start is an asynchronous operation. However, we
	// can treat our state as being already started. If Start
	// fails later, we'll get an MESessionStarted event with
	// an error code, and we will update our state then.

	HRESULT hr = mSessionPtr->Start( &GUID_NULL, &varStart );
	if( SUCCEEDED( hr ) )
		mState = Started;

	PropVariantClear( &varStart );
	return hr;
}

HRESULT MFPlayer::Pause()
{
	if( mState != Started )
		return MF_E_INVALIDREQUEST;

	if( mSessionPtr == NULL || mSourcePtr == NULL )
		return E_POINTER;

	HRESULT hr = mSessionPtr->Pause();
	if( SUCCEEDED( hr ) )
		mState = Paused;

	return hr;
}

HRESULT MFPlayer::SetPosition( MFTIME position )
{
	if( !mSessionPtr )
		return E_POINTER;

	if( mState == OpenPending )
		return MF_E_INVALIDREQUEST;

	BOOL bWasPlaying = ( mState == Started );
	Pause();

	PROPVARIANT varStart;
	PropVariantInit( &varStart );
	varStart.vt = VT_I8;
	varStart.hVal.QuadPart = position;

	HRESULT hr = mSessionPtr->Start( &GUID_NULL, &varStart );
	if( SUCCEEDED( hr ) ) {
		mState = Started;

		if( !bWasPlaying )
			Pause();
	}

	PropVariantClear( &varStart );

	return hr;
}

HRESULT MFPlayer::OnTopologyStatus( IMFMediaEvent *pEvent )
{
	UINT32 status;

	HRESULT hr = pEvent->GetUINT32( MF_EVENT_TOPOLOGY_STATUS, &status );
	if( SUCCEEDED( hr ) && ( status == MF_TOPOSTATUS_READY ) ) {
		SafeRelease( mVideoDisplayPtr );

		// Get the IMFVideoDisplayControl interface from EVR. This call is
		// expected to fail if the media file does not have a video stream.
		(void) MFGetService( mSessionPtr, MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS( &mVideoDisplayPtr ) );

		mState = Paused;

		if( mPlayWhenReady )
			hr = Play();
	}
	return hr;
}

HRESULT MFPlayer::OnPresentationEnded( IMFMediaEvent *pEvent )
{
	HRESULT hr = S_OK;

	if( mIsLooping ) {
		// Seek to the beginning.
		hr = SetPosition( 0 );
	}
	else {
		// Pause instead of Stop, so we can easily restart the video later.
		hr = Pause();
	}

	return hr;
}

HRESULT MFPlayer::OnNewPresentation( IMFMediaEvent *pEvent )
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

		mState = OpenPending;
	} while( false );

	return hr;
}

HRESULT MFPlayer::CreateSession()
{
	HRESULT hr = S_OK;

	do {
		// Close the old session.
		hr = CloseSession();
		BREAK_ON_FAIL( hr );

		// Sanity check.
		assert( mState == Closed );

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

HRESULT MFPlayer::CloseSession()
{
	HRESULT hr = S_OK;

	//  The IMFMediaSession::Close method is asynchronous, but the 
	//  MFPlayer::CloseSession method waits on the MESessionClosed event.
	//  
	//  MESessionClosed is guaranteed to be the last event that the media session fires.

	SafeRelease( mVideoDisplayPtr );

	// First close the media session.
	if( mSessionPtr ) {
		DWORD dwWaitResult = 0;

		mState = Closing;

		hr = mSessionPtr->Close();

		// Wait for the close operation to complete
		if( SUCCEEDED( hr ) ) {
			dwWaitResult = WaitForSingleObject( mCloseEvent, 5000 );
			if( dwWaitResult == WAIT_TIMEOUT ) {
				assert( 0 );
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

	SafeRelease( mSourcePtr );
	SafeRelease( mSessionPtr );

	mState = Closed;

	return hr;
}

HRESULT MFPlayer::Repaint()
{
	HRESULT hr = S_OK;

	PAINTSTRUCT ps;
	::BeginPaint( mWnd, &ps );

	if( mVideoDisplayPtr ) 
		hr = mVideoDisplayPtr->RepaintVideo();

	::EndPaint( mWnd, &ps );
	
	return hr;
}

HRESULT MFPlayer::ResizeVideo( WORD width, WORD height )
{
	if( mVideoDisplayPtr ) {
		// Set the destination rectangle.
		// Leave the default source rectangle (0,0,1,1).

		RECT rcDest = { 0, 0, width, height };

		return mVideoDisplayPtr->SetVideoPosition( NULL, &rcDest );
	}
	else {
		return S_OK;
	}
}

HRESULT MFPlayer::CreatePartialTopology( IMFPresentationDescriptor *pDescriptor )
{
	HRESULT hr = S_OK;

	do {
		ScopedComPtr<IMFTopology> pTopology;
		hr = CreatePlaybackTopology( mSourcePtr, pDescriptor, mWnd, &pTopology, mPresenterPtr );
		BREAK_ON_FAIL( hr );

		hr = SetMediaInfo( pDescriptor );
		BREAK_ON_FAIL( hr );

		// Set the topology on the media session.
		hr = mSessionPtr->SetTopology( 0, pTopology );
		BREAK_ON_FAIL( hr );

		// If SetTopology succeeds, the media session will queue an MESessionTopologySet event.

	} while( false );

	return hr;
}

HRESULT MFPlayer::SetMediaInfo( IMFPresentationDescriptor *pDescriptor )
{
	HRESULT hr = S_OK;

	// Sanity check.
	assert( pDescriptor != NULL );

	do {
		DWORD count;
		hr = pDescriptor->GetStreamDescriptorCount( &count );
		BREAK_ON_FAIL( hr );

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

// IUnknown methods

HRESULT MFPlayer::QueryInterface( REFIID riid, void** ppv )
{
	static const QITAB qit[] = { QITABENT( MFPlayer, IMFAsyncCallback ), { 0 } };
	return QISearch( this, qit, riid, ppv );
}

ULONG MFPlayer::AddRef()
{
	return ::InterlockedIncrement( &mRefCount );
}

ULONG MFPlayer::Release()
{
	ULONG uCount = ::InterlockedDecrement( &mRefCount );
	if( uCount == 0 ) {
		delete this;
	}
	return uCount;
}

// IMFAsyncCallback methods

HRESULT MFPlayer::Invoke( IMFAsyncResult *pResult )
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

LRESULT MFPlayer::HandleEvent( WPARAM wParam )
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
				break;

			default:
				hr = OnSessionEvent( pEvent, eventType );
				break;
		}
	} while( false );

	SafeRelease( pEvent );

	return 0;
}

void MFPlayer::CreateWnd()
{
	const std::wstring unicodeTitle = L"";

	if( !mWnd ) {
		RegisterWindowClass();

		mWnd = ::CreateWindowEx( WS_EX_APPWINDOW, MF_WINDOW_CLASS_NAME, unicodeTitle.c_str(), WS_OVERLAPPEDWINDOW,
								 CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, ::GetModuleHandle( NULL ), reinterpret_cast<LPVOID>( this ) );
		if( !mWnd )
			throw Exception( "MFPlayer: failed to create window." );

		::ShowWindow( mWnd, SW_SHOW );
		::UpdateWindow( mWnd );
	}
}

void MFPlayer::DestroyWnd()
{
	if( mWnd )
		::DestroyWindow( mWnd );
}

void MFPlayer::RegisterWindowClass()
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
	wc.hbrBackground = (HBRUSH)( BLACK_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = MF_WINDOW_CLASS_NAME;

	if( !::RegisterClass( &wc ) ) {
		DWORD err = ::GetLastError();
		return;
	}

	sRegistered = true;
}

//  Create a media source from a URL.
HRESULT MFPlayer::CreateMediaSource( LPCWSTR pUrl, IMFMediaSource **ppSource )
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
HRESULT MFPlayer::CreatePlaybackTopology( IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology, IMFVideoPresenter *pVideoPresenter )
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
			hr = AddBranchToPartialTopology( pTopology, pSource, pPD, i, hVideoWnd, pVideoPresenter );
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
HRESULT MFPlayer::CreateMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate, IMFVideoPresenter *pVideoPresenter, IMFMediaSink **ppMediaSink )
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
			if( pVideoPresenter != NULL ) {
				// Create the video renderer.
				ScopedComPtr<IMFMediaSink> pSink;
				hr = ::MFCreateVideoRenderer( __uuidof( IMFMediaSink ), (void**)&pSink );
				BREAK_ON_FAIL( hr );

				ScopedComPtr<IMFVideoRenderer> pVideoRenderer;
				hr = pSink.QueryInterface( __uuidof( IMFVideoRenderer ), (void**)&pVideoRenderer );
				BREAK_ON_FAIL( hr );

				hr = pVideoRenderer->InitializeRenderer( NULL, pVideoPresenter );
				BREAK_ON_FAIL( hr );

				// Return IMFMediaSink pointer to caller.
				*ppMediaSink = pSink;
				( *ppMediaSink )->AddRef();
			}
#if 0
			else {
				// Use default EVR.
				ScopedComPtr<IMFActivate> pActivate;
				hr = ::MFCreateVideoRendererActivate( hVideoWindow, &pActivate );
				BREAK_ON_FAIL( hr );

				// Return IMFActivate pointer to caller.
				*ppActivate = pActivate;
				( *ppActivate )->AddRef();
			}
#else
			else {
				// Use custom EVR.
				ScopedComPtr<IMFActivate> pActivate;
				hr = detail::Activate::CreateInstance( hVideoWindow, &pActivate );
				BREAK_ON_FAIL( hr );

				// Return IMFActivate pointer to caller.
				*ppActivate = pActivate;
				( *ppActivate )->AddRef();
			}
#endif
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
HRESULT MFPlayer::AddSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode )
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

HRESULT MFPlayer::AddOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode )
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
HRESULT MFPlayer::AddOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode )
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

HRESULT MFPlayer::AddBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd, IMFVideoPresenter *pVideoPresenter )
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
			ScopedComPtr<IMFMediaSink> pMediaSink;
			hr = CreateMediaSinkActivate( pSD, hVideoWnd, &pSinkActivate, pVideoPresenter, &pMediaSink );
			BREAK_ON_FAIL( hr );

			// Add a source node for this stream.
			ScopedComPtr<IMFTopologyNode> pSourceNode;
			hr = AddSourceNode( pTopology, pSource, pPD, pSD, &pSourceNode );
			BREAK_ON_FAIL( hr );

			// Create the output node for the renderer.
			ScopedComPtr<IMFTopologyNode> pOutputNode;
			if( pSinkActivate ) {
				hr = AddOutputNode( pTopology, pSinkActivate, 0, &pOutputNode );
			}
			else if( pMediaSink ) {
				IMFStreamSink* pStreamSink = NULL;
				DWORD streamCount;

				pMediaSink->GetStreamSinkCount( &streamCount );
				pMediaSink->GetStreamSinkByIndex( 0, &pStreamSink );

				hr = AddOutputNode( pTopology, pStreamSink, &pOutputNode );
			}
			BREAK_ON_FAIL( hr );

			// Connect the source node to the output node.
			hr = pSourceNode->ConnectOutput( 0, pOutputNode, 0 );
			BREAK_ON_FAIL( hr );
		}
	} while( false );

	return hr;
}

}
}
