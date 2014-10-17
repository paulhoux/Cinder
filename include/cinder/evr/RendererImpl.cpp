/*
Copyright (c) 2014, The Barbarian Group
All rights reserved.

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

#include "cinder/Cinder.h"

#if defined( CINDER_MSW )

#include "cinder/evr/RendererImpl.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "shlwapi.lib")

namespace cinder {
namespace evr {

std::map<HWND, MovieBase*> MovieBase::sMovieWindows;

//////////////////////////////////////////////////////////////////////////////////////////

template <class Q>
HRESULT GetEventObject( IMFMediaEvent *pEvent, Q **ppObject )
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

//////////////////////////////////////////////////////////////////////////////////////////

MovieBase::MovieBase()
	: mRefCount( 0 ), mHwnd( NULL ), mState( CLOSED ), mWidth( 0 ), mHeight( 0 ), mCurrentVolume( 1 )
	, mLoaded( false ), mPlayThroughOk( false ), mPlayable( false ), mProtected( false ), mPlaying( false )
	, mPlayingForward( true ), mLoop( false ), mPalindrome( false ), mHasAudio( false ), mHasVideo( false )
{
	static bool isInitialized = false;

	if( !isInitialized ) {
		HRESULT hr = MFStartup( MF_VERSION );
		if( !SUCCEEDED( hr ) ) {
			CI_LOG_E( "Error while loading MF" );
			return;
		}

		isInitialized = true;
	}
}

void MovieBase::init()
{
	mHwnd = createWindow( this );
	// TODO: error checking.
}

void MovieBase::initFromUrl( const Url& url )
{
	init();

	// Create the media session.
	HRESULT hr = createSession();
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create session." );
		return;
	}

	// Create the media source.
	std::wstring wstr = msw::toWideString( url.str() );
	hr = createMediaSource( wstr.c_str(), &mMediaSourcePtr );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create media source." );
		closeSession();
		return;
	}

	// Create the presentation descriptor for the media source.
	msw::ScopedComPtr<IMFPresentationDescriptor> pPD;
	hr = mMediaSourcePtr->CreatePresentationDescriptor( &pPD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create presentation descriptor." );
		closeSession();
		return;
	}

	hr = createPartialTopology( pPD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create partial topology." );
		closeSession();
		return;
	}

	mState = OPEN_PENDING;
}

void MovieBase::initFromPath( const fs::path& filePath )
{
	init();

	// Create the media session.
	HRESULT hr = createSession();
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create session." );
		return;
	}

	// Create the media source.
	std::wstring wstr = msw::toWideString( filePath.generic_string() );
	hr = createMediaSource( wstr.c_str(), &mMediaSourcePtr );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create media source." );
		closeSession();
		return;
	}

	// Create the presentation descriptor for the media source.
	msw::ScopedComPtr<IMFPresentationDescriptor> pPD;
	hr = mMediaSourcePtr->CreatePresentationDescriptor( &pPD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create presentation descriptor." );
		closeSession();
		return;
	}

	hr = createPartialTopology( pPD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create partial topology." );
		closeSession();
		return;
	}

	mState = OPEN_PENDING;
}

//  Create a new instance of the media session.
HRESULT MovieBase::createSession()
{
	// Close the old session.
	HRESULT hr = closeSession();
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to close session." );
		return hr;
	}

	CI_ASSERT( mState == CLOSED );

	// Create the media session.
	hr = MFCreateMediaSession( NULL, &mMediaSessionPtr );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create media session." );
		return hr;
	}

	CI_LOG_V( "Starting media session..." );

	// Start pulling events from the media session.
	hr = mMediaSessionPtr->BeginGetEvent( this, NULL );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to begin getting events." );
		return hr;
	}

	CI_LOG_V( "Media session started." );

	mState = READY;

	return hr;
}

//  Close the media session. 
HRESULT MovieBase::closeSession()
{
	//  The IMFMediaSession::Close method is asynchronous, but the 
	//  EvrPlayer::CloseSession method waits on the MESessionClosed event.
	//  
	//  MESessionClosed is guaranteed to be the last event that the 
	//  media session fires.

	HRESULT hr = S_OK;

	mVideoDisplayControlPtr = NULL;
	mAudioStreamVolumePtr = NULL;

	// First close the media session.
	if( mMediaSessionPtr ) {
		CI_LOG_V( "Closing media session..." );

		mState = CLOSING;
		hr = mMediaSessionPtr->Close();

		// Wait for the close operation to complete
		if( SUCCEEDED( hr ) ) {
			DWORD dwWaitResult = WaitForSingleObject( mCloseEventHandle, 5000 );
			if( dwWaitResult == WAIT_TIMEOUT ) {
				assert( FALSE );
			}
			// Now there will be no more events from this session.
		}
	}

	// Complete shutdown operations.
	if( SUCCEEDED( hr ) ) {
		// Shut down the media source. (Synchronous operation, no events.)
		if( mMediaSourcePtr )
			mMediaSourcePtr->Shutdown();

		// Shut down the media session. (Synchronous operation, no events.)
		if( mMediaSessionPtr ) {
			mMediaSessionPtr->Shutdown();
			CI_LOG_V( "Media session closed." );
		}
	}

	mMediaSourcePtr = NULL;
	mMediaSessionPtr = NULL;

	mState = CLOSED;

	return hr;
}

HRESULT MovieBase::setMediaInfo( IMFPresentationDescriptor *pPD )
{
	mWidth = 0;
	mHeight = 0;

	assert( pPD != NULL );

	HRESULT hr = S_OK;

	DWORD count;
	pPD->GetStreamDescriptorCount( &count );

	for( DWORD i = 0; i < count; i++ ) {
		BOOL selected;

		msw::ScopedComPtr<IMFStreamDescriptor> spStreamDesc;
		hr = pPD->GetStreamDescriptorByIndex( i, &selected, &spStreamDesc );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to get stream descriptor by index." );
			return hr;
		}

		if( selected ) {
			msw::ScopedComPtr<IMFMediaTypeHandler> pHandler;
			hr = spStreamDesc->GetMediaTypeHandler( &pHandler );
			if( FAILED( hr ) ) {
				CI_LOG_E( "Failed to get media type handler." );
				return hr;
			}

			GUID guidMajorType = GUID_NULL;
			hr = pHandler->GetMajorType( &guidMajorType );
			if( FAILED( hr ) ) {
				CI_LOG_E( "Failed to get major type." );
				return hr;
			}

			if( MFMediaType_Video == guidMajorType ) {
				// first get the source video size and allocate a new texture
				msw::ScopedComPtr<IMFMediaType> pMediaType;
				hr = pHandler->GetCurrentMediaType( &pMediaType );
				if( FAILED( hr ) ) {
					CI_LOG_E( "Failed to get current media type." );
					return hr;
				}

				hr = MFGetAttributeSize( pMediaType, MF_MT_FRAME_SIZE, &mWidth, &mHeight );
				if( FAILED( hr ) ) {
					CI_LOG_E( "Failed to get attribute size." );
					return hr;
				}

				if( mWidth % 2 != 0 || mHeight % 2 != 0 ) {
					CI_LOG_E( "Video resolution not divisible by 2." );
					return E_UNEXPECTED;
				}
			}
		}
	}

	return hr;
}

LRESULT MovieBase::WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch( message ) {
	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;
	case WM_APP_MOVIE_EVENT:
		handleEvent( wParam );
	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0L;
}

HRESULT MovieBase::handleEvent( UINT_PTR pEventPtr )
{
	HRESULT hr = S_OK;

	msw::ScopedComPtr<IMFMediaEvent> pEvent( (IMFMediaEvent*) pEventPtr );
	if( pEvent == NULL )
		return E_POINTER;

	// Get the event type.
	MediaEventType eventType = MEUnknown;
	hr = pEvent->GetType( &eventType );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to get event type." );
		return hr;
	}

	// Get the event status. If the operation that triggered the event 
	// did not succeed, the status is a failure code.
	HRESULT hrStatus = S_OK;
	hr = pEvent->GetStatus( &hrStatus );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to get status for event." );
		return hr;
	}

	// Check if the async operation succeeded.
	if( SUCCEEDED( hr ) && FAILED( hrStatus ) ) {
		CI_LOG_E( "GetStatus() returned a failed status: " << hrStatus );
		hr = hrStatus;
	}

	if( FAILED( hr ) )
		return hr;

	switch( eventType ) {
	case MESessionTopologySet:
		hr = handleSessionTopologySetEvent( pEvent );
		break;
	case MESessionTopologyStatus:
		hr = handleSessionTopologyStatusEvent( pEvent );
		break;
	case MEEndOfPresentation:
		hr = handleEndOfPresentationEvent( pEvent );
		break;
	case MENewPresentation:
		hr = handleNewPresentationEvent( pEvent );
		break;
	case MESessionStarted:
		// Do nothing.
		break;
	default:
		hr = handleSessionEvent( pEvent, eventType );
		break;
	}

	return hr;
}

HRESULT MovieBase::handleSessionTopologySetEvent( IMFMediaEvent *pEvent )
{
#if _DEBUG
	msw::ScopedComPtr<IMFTopology> pTopology;
	GetEventObject<IMFTopology>( pEvent, &pTopology );

	if( !pTopology )
		return E_POINTER;

	WORD nodeCount;
	pTopology->GetNodeCount( &nodeCount );

	CI_LOG_V( "Topo set and we have " << nodeCount << " nodes" );
#endif

	return S_OK;
}

HRESULT MovieBase::handleSessionTopologyStatusEvent( IMFMediaEvent *pEvent )
{
	UINT32 status;

	HRESULT hr = pEvent->GetUINT32( MF_EVENT_TOPOLOGY_STATUS, &status );
	if( SUCCEEDED( hr ) && ( status == MF_TOPOSTATUS_READY ) ) {
		mVideoDisplayControlPtr.Release();

		//hr = StartPlayback();
		//hr = Pause();
	}

	return hr;
}

HRESULT MovieBase::handleEndOfPresentationEvent( IMFMediaEvent *pEvent )
{
	mMediaSessionPtr->Pause();
	mState = PAUSED;

	// Seek to the beginning.
	PROPVARIANT varStart;
	PropVariantInit( &varStart );
	varStart.vt = VT_I8;
	varStart.hVal.QuadPart = 0;

	HRESULT hr = S_OK;
	hr = mMediaSessionPtr->Start( &GUID_NULL, &varStart );

	if( !mLoop )
		mMediaSessionPtr->Pause();
	else
		mState = STARTED;

	PropVariantClear( &varStart );

	// The session puts itself into the stopped state automatically.

	return S_OK;
}

HRESULT MovieBase::handleNewPresentationEvent( IMFMediaEvent *pEvent )
{
	// Get the presentation descriptor from the event.
	msw::ScopedComPtr<IMFPresentationDescriptor> pPD;
	HRESULT hr = GetEventObject( pEvent, &pPD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to get event object." );
		return hr;
	}

	// Create a partial topology.
	hr = createPartialTopology( pPD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to set topology." );
		return hr;
	}

	mState = OPEN_PENDING;

	return S_OK;
}

HRESULT MovieBase::handleSessionEvent( IMFMediaEvent *pEvent, MediaEventType eventType )
{
	return S_OK;
}

// IUnknown methods

HRESULT MovieBase::QueryInterface( REFIID riid, void** ppv )
{
	static const QITAB qit[] = {
		QITABENT( MovieBase, IMFAsyncCallback ),
		{ 0 }
	};
	return QISearch( this, qit, riid, ppv );
}

ULONG MovieBase::AddRef()
{
	return InterlockedIncrement( &mRefCount );
}

ULONG MovieBase::Release()
{
	ULONG uCount = InterlockedDecrement( &mRefCount );
	if( uCount == 0 ) {
		delete this;
	}
	return uCount;
}

// IMFAsyncCallback methods
HRESULT MovieBase::Invoke( IMFAsyncResult *pResult )
{
	// Sometimes Invoke is called but mMediaSessionPtr is closed.
	if( !mMediaSessionPtr )
		return E_POINTER;

	// Get the event from the event queue.
	msw::ScopedComPtr<IMFMediaEvent> pEvent;
	HRESULT hr = mMediaSessionPtr->EndGetEvent( pResult, &pEvent );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to end get event." );
		return hr;
	}

	// Get the event type. 
	MediaEventType eventType = MEUnknown;
	hr = pEvent->GetType( &eventType );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to get event type." );
		return hr;
	}

	if( eventType == MESessionClosed ) {
		// The session was closed. 
		// The application is waiting on the mCloseEventHandle.
		SetEvent( mCloseEventHandle );
	}
	else {
		// For all other events, get the next event in the queue.
		hr = mMediaSessionPtr->BeginGetEvent( this, NULL );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to begin get event." );
			return hr;
		}
	}

	// Check the application state. 

	// If a call to IMFMediaSession::Close is pending, it means the 
	// application is waiting on the mCloseEventHandle and
	// the application's message loop is blocked. 

	// Otherwise, post a private window message to the application. 

	if( mState != CLOSING ) {
		// Leave a reference count on the event.
		pEvent.get()->AddRef();

		PostMessage( mHwnd, WM_APP_MOVIE_EVENT, (WPARAM) pEvent.get(), (LPARAM) eventType );
	}
	return S_OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK MovieBase::WndProcDummy( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	MovieBase* movie = NULL;

	switch( message ) {
	case WM_CREATE:
		// Let DefWindowProc handle it.
		break;
	default:
		movie = findMovie( hWnd );
		if( movie )
			return movie->WndProc( hWnd, message, wParam, lParam );
		break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

HWND MovieBase::createWindow( MovieBase* movie )
{
	static const PCWSTR szWindowClass = L"MFPLAYBACK";

	// Register the window class.
	WNDCLASSEX wcex;
	ZeroMemory( &wcex, sizeof( WNDCLASSEX ) );
	wcex.cbSize = sizeof( WNDCLASSEX );
	wcex.style = CS_HREDRAW | CS_VREDRAW;

	wcex.lpfnWndProc = MovieBase::WndProcDummy;
	wcex.hbrBackground = (HBRUSH) ( BLACK_BRUSH );
	wcex.lpszClassName = szWindowClass;

	RegisterClassEx( &wcex );

	// Create the window.
	HWND hWnd;
	hWnd = CreateWindow( szWindowClass, L"", WS_OVERLAPPEDWINDOW,
						 CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, NULL, NULL );

	if( hWnd != NULL ) {
		// Keep track of window.
		sMovieWindows[hWnd] = movie;
		CI_LOG_V( "Created a hidden window. Total number of hidden windows is now: " << sMovieWindows.size() );

		// Set window attributes.
		LONG style2 = ::GetWindowLong( hWnd, GWL_STYLE );
		style2 &= ~WS_DLGFRAME;
		style2 &= ~WS_CAPTION;
		style2 &= ~WS_BORDER;
		style2 &= WS_POPUP;

		LONG exstyle2 = ::GetWindowLong( hWnd, GWL_EXSTYLE );
		exstyle2 &= ~WS_EX_DLGMODALFRAME;
		::SetWindowLong( hWnd, GWL_STYLE, style2 );
		::SetWindowLong( hWnd, GWL_EXSTYLE, exstyle2 );

		UpdateWindow( hWnd );
	}

	return hWnd;
}

void MovieBase::destroyWindow( HWND hWnd )
{
	auto itr = sMovieWindows.find( hWnd );
	if( itr != sMovieWindows.end() )
		sMovieWindows.erase( itr );

	if( !DestroyWindow( hWnd ) ) {
		// Something went wrong. Use GetLastError() to find out what.
		CI_LOG_E( "Failed to destroy hidden window." );
	}
	else
		CI_LOG_V( "Destroyed a hidden window. Total number of hidden windows is now: " << sMovieWindows.size() );
}

MovieBase* MovieBase::findMovie( HWND hWnd )
{
	auto itr = sMovieWindows.find( hWnd );
	if( itr != sMovieWindows.end() )
		return itr->second;

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////

//  Create a media source from a URL.
HRESULT MovieBase::createMediaSource( LPCWSTR pUrl, IMFMediaSource **ppSource )
{
	// Create the source resolver.
	msw::ScopedComPtr<IMFSourceResolver> pSourceResolver;
	HRESULT hr = MFCreateSourceResolver( &pSourceResolver );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create source resolver." );
		return hr;
	}

	// Use the source resolver to create the media source.

	// Note: For simplicity this sample uses the synchronous method to create 
	// the media source. However, creating a media source can take a noticeable
	// amount of time, especially for a network source. For a more responsive 
	// UI, use the asynchronous BeginCreateObjectFromURL method.

	DWORD dwFlags = MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE;
	MF_OBJECT_TYPE objectType = MF_OBJECT_INVALID;
	msw::ScopedComPtr<IUnknown> pSource;
	hr = pSourceResolver->CreateObjectFromURL( pUrl, dwFlags, NULL, &objectType, &pSource );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create object from URL: " << pUrl );
		return hr;
	}

	// Get the IMFMediaSource interface from the media source.
	return pSource.QueryInterface( __uuidof( **ppSource ), (void**) ppSource );
}

//  Create a playback topology from a media source.
HRESULT MovieBase::createPlaybackTopology( IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology, IMFVideoPresenter *pVideoPresenter )
{
	// Create a new topology.
	msw::ScopedComPtr<IMFTopology> pTopology;
	HRESULT hr = MFCreateTopology( &pTopology );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create topology." );
		return hr;
	}

	// Get the number of streams in the media source.
	DWORD dwSourceStreams = 0;
	hr = pPD->GetStreamDescriptorCount( &dwSourceStreams );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to get the number of stream descriptors." );
		return hr;
	}

	// For each stream, create the topology nodes and add them to the topology.
	for( DWORD i = 0; i < dwSourceStreams; i++ ) {
		hr = addBranchToPartialTopology( pTopology, pSource, pPD, i, hVideoWnd, pVideoPresenter );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to add branch to partial topology." );
			return hr;
		}
	}

	// Return the IMFTopology pointer to the caller.
	*ppTopology = pTopology;
	( *ppTopology )->AddRef();

	return hr;
}

//  Create an activation object for a renderer, based on the stream media type.
HRESULT MovieBase::createMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate, IMFVideoPresenter *pVideoPresenter, IMFMediaSink **ppMediaSink )
{
	// Get the media type handler for the stream.
	msw::ScopedComPtr<IMFMediaTypeHandler> pHandler;
	HRESULT hr = pSourceSD->GetMediaTypeHandler( &pHandler );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to get media type handler." );
		return hr;
	}

	// Get the major media type.
	GUID guidMajorType;
	hr = pHandler->GetMajorType( &guidMajorType );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to get major type." );
		return hr;
	}

	// Create an IMFActivate object for the renderer, based on the media type.
	if( MFMediaType_Audio == guidMajorType ) {
		// Create the audio renderer.
		msw::ScopedComPtr<IMFActivate> pActivate;
		hr = MFCreateAudioRendererActivate( &pActivate );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to create audio renderer / activate it." );
			return hr;
		}

		// Return IMFActivate pointer to caller.
		*ppActivate = pActivate;
		( *ppActivate )->AddRef();
	}
	else if( MFMediaType_Video == guidMajorType ) {
		// Create the video renderer.
		msw::ScopedComPtr<IMFMediaSink> pSink;
		hr = MFCreateVideoRenderer( __uuidof( IMFMediaSink ), (void**) &pSink );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to create video renderer." );
			return hr;
		}

		msw::ScopedComPtr<IMFVideoRenderer> pVideoRenderer;
		hr = pSink.QueryInterface( __uuidof( IMFVideoRenderer ), (void**) &pVideoRenderer );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to query IMFVideoRenderer interface." );
			return hr;
		}

		hr = pVideoRenderer->InitializeRenderer( NULL, pVideoPresenter );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to intialize renderer." );
			return hr;
		}

		// Return IMFMediaSink pointer to caller.
		*ppMediaSink = pSink;
		( *ppMediaSink )->AddRef();
	}
	else {
		// Unknown stream type.
		CI_LOG_E( "Unknown stream type." );
		hr = E_FAIL;
		// Optionally, you could deselect this stream instead of failing.
	}

	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create topology node." );
		return hr;
	}

	return hr;
}

// Add a source node to a topology.
HRESULT MovieBase::addSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode )
{
	// Create the node.
	msw::ScopedComPtr<IMFTopologyNode> pNode;
	HRESULT hr = MFCreateTopologyNode( MF_TOPOLOGY_SOURCESTREAM_NODE, &pNode );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create topology node." );
		return hr;
	}

	// Set the attributes.
	hr = pNode->SetUnknown( MF_TOPONODE_SOURCE, pSource );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to set topology attribute." );
		return hr;
	}

	hr = pNode->SetUnknown( MF_TOPONODE_PRESENTATION_DESCRIPTOR, pPD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to set topology attribute." );
		return hr;
	}

	hr = pNode->SetUnknown( MF_TOPONODE_STREAM_DESCRIPTOR, pSD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to set topology attribute." );
		return hr;
	}

	// Add the node to the topology.
	hr = pTopology->AddNode( pNode );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to add node to topology." );
		return hr;
	}

	// Return the pointer to the caller.
	*ppNode = pNode;
	( *ppNode )->AddRef();

	return hr;
}

HRESULT MovieBase::addOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode )
{
	HRESULT hr = S_OK;

	// Create the node.
	msw::ScopedComPtr<IMFTopologyNode> pNode;
	hr = MFCreateTopologyNode( MF_TOPOLOGY_OUTPUT_NODE, &pNode );

	// Set the object pointer.
	if( SUCCEEDED( hr ) ) {
		hr = pNode->SetObject( pStreamSink );
	}

	// Add the node to the topology.
	if( SUCCEEDED( hr ) ) {
		hr = pTopology->AddNode( pNode );
	}

	if( SUCCEEDED( hr ) ) {
		hr = pNode->SetUINT32( MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, TRUE );
	}

	// Return the pointer to the caller.
	if( SUCCEEDED( hr ) ) {
		*ppNode = pNode;
		( *ppNode )->AddRef();
	}

	return hr;
}

// Add an output node to a topology.
HRESULT MovieBase::addOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode )
{
	msw::ScopedComPtr<IMFTopologyNode> pNode;

	// Create the node.
	HRESULT hr = MFCreateTopologyNode( MF_TOPOLOGY_OUTPUT_NODE, &pNode );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to create topology node." );
		return hr;
	}

	// Set the object pointer.
	hr = pNode->SetObject( pActivate );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to set object." );
		return hr;
	}

	// Set the stream sink ID attribute.
	hr = pNode->SetUINT32( MF_TOPONODE_STREAMID, dwId );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to set stream sink ID attribute." );
		return hr;
	}

	hr = pNode->SetUINT32( MF_TOPONODE_NOSHUTDOWN_ON_REMOVE, FALSE );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to set stream NoShutdown attribute." );
		return hr;
	}

	// Add the node to the topology.
	hr = pTopology->AddNode( pNode );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to add node to topology." );
		return hr;
	}

	// Return the pointer to the caller.
	*ppNode = pNode;
	( *ppNode )->AddRef();

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

HRESULT MovieBase::addBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd, IMFVideoPresenter *pVideoPresenter )
{
	msw::ScopedComPtr<IMFStreamDescriptor> pSD;
	msw::ScopedComPtr<IMFActivate> pSinkActivate;
	msw::ScopedComPtr<IMFTopologyNode> pSourceNode;
	msw::ScopedComPtr<IMFTopologyNode> pOutputNode;
	msw::ScopedComPtr<IMFMediaSink> pMediaSink;

	assert( pPD != NULL );

	BOOL fSelected = FALSE;
	HRESULT hr = pPD->GetStreamDescriptorByIndex( iStream, &fSelected, &pSD );
	if( FAILED( hr ) ) {
		CI_LOG_E( "Failed to get stream descriptor by index." );
		return hr;
	}

	if( fSelected ) {
		// Create the media sink activation object.
		hr = createMediaSinkActivate( pSD, hVideoWnd, &pSinkActivate, pVideoPresenter, &pMediaSink );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to create and activate media sink." );
			return hr;
		}

		// Add a source node for this stream.
		hr = addSourceNode( pTopology, pSource, pPD, pSD, &pSourceNode );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to add source node." );
			return hr;
		}

		// Create the output node for the renderer.
		if( pSinkActivate ) {
			hr = addOutputNode( pTopology, pSinkActivate, 0, &pOutputNode );
		}
		else if( pMediaSink ) {
			IMFStreamSink  * pStreamSink = NULL;
			DWORD streamCount;

			pMediaSink->GetStreamSinkCount( &streamCount );
			pMediaSink->GetStreamSinkByIndex( 0, &pStreamSink );

			hr = addOutputNode( pTopology, pStreamSink, &pOutputNode );
		}

		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to add output node." );
			return hr;
		}

		// Connect the source node to the output node.
		hr = pSourceNode->ConnectOutput( 0, pOutputNode, 0 );
		if( FAILED( hr ) ) {
			CI_LOG_E( "Failed to connect output." );
			return hr;
		}
	}

	return hr;
}

}
} // namespace cinder::evr

#endif // CINDER_MSW