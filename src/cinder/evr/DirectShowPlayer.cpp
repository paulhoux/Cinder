#include "cinder/evr/DirectShowPlayer.h"
#include "cinder/evr/DirectShowVideo.h"
#include "cinder/Log.h"

#if defined(CINDER_MSW)

namespace cinder {
namespace msw { 
namespace video {


DirectShowPlayer::DirectShowPlayer( HRESULT *hr, HWND hwnd )
	: m_state( STATE_NO_GRAPH ), m_hwnd( hwnd ), m_pGraph( NULL ), m_pControl( NULL ), m_pEvent( NULL ), m_pVideo( NULL )
{
	*hr = S_OK;
}

DirectShowPlayer::~DirectShowPlayer()
{
	TearDownGraph();
}

// Open a media file for playback.
HRESULT DirectShowPlayer::OpenFile( PCWSTR pszFileName )
{
	HRESULT hr = S_OK;

	do {
		// Create a new filter graph. (This also closes the old one, if any.)
		HRESULT hr = InitializeGraph();
		BREAK_ON_FAIL( hr );

		// Add the source filter to the graph.
		ScopedComPtr<IBaseFilter> pSource;
		hr = m_pGraph->AddSourceFilter( pszFileName, NULL, &pSource );
		BREAK_ON_FAIL( hr );

		// Try to render the streams.
		hr = RenderStreams( pSource );
	} while( false );

	if( FAILED( hr ) )
		TearDownGraph();

	return hr;
}

HRESULT DirectShowPlayer::HandleEvent( UINT_PTR pEventPtr )
{
	HRESULT hr = S_OK;

	do {
		// Get the event type.
		long evCode = 0;
		LONG_PTR param1 = 0, param2 = 0;

		// Get the event status. If the operation that triggered the event 
		// did not succeed, the status is a failure code.
		while( SUCCEEDED( m_pEvent->GetEvent( &evCode, &param1, &param2, 0 ) ) ) {
			// Invoke the callback.

			switch( evCode ) {
			case EC_COMPLETE:
			case EC_USERABORT:
				Stop();
				break;

			case EC_ERRORABORT:
				CI_LOG_E( "Playback error." );
				Stop();
				break;
			}

			// Free the event data.
			hr = m_pEvent->FreeEventParams( evCode, param1, param2 );
			BREAK_ON_FAIL( hr );
		}
	} while( false );

	return hr;
}

HRESULT DirectShowPlayer::Play()
{
	if( m_state != STATE_PAUSED && m_state != STATE_STOPPED ) {
		return VFW_E_WRONG_STATE;
	}

	HRESULT hr = m_pControl->Run();
	if( SUCCEEDED( hr ) ) {
		m_state = STATE_RUNNING;
	}
	return hr;
}

HRESULT DirectShowPlayer::Pause()
{
	if( m_state != STATE_RUNNING ) {
		return VFW_E_WRONG_STATE;
	}

	HRESULT hr = m_pControl->Pause();
	if( SUCCEEDED( hr ) ) {
		m_state = STATE_PAUSED;
	}
	return hr;
}

HRESULT DirectShowPlayer::Stop()
{
	if( m_state != STATE_RUNNING && m_state != STATE_PAUSED ) {
		return VFW_E_WRONG_STATE;
	}

	HRESULT hr = m_pControl->Stop();
	if( SUCCEEDED( hr ) ) {
		m_state = STATE_STOPPED;
	}
	return hr;
}

// EVR/VMR functionality

BOOL DirectShowPlayer::HasVideo() const
{
	return ( m_pVideo && m_pVideo->HasVideo() );
}

// Sets the destination rectangle for the video.
HRESULT DirectShowPlayer::UpdateVideoWindow( const LPRECT prc )
{
	if( m_pVideo ) {
		return m_pVideo->UpdateVideoWindow( m_hwnd, prc );
	}
	else {
		return S_OK;
	}
}

// Repaints the video. Call this method when the application receives WM_PAINT.
HRESULT DirectShowPlayer::Repaint( HDC hdc )
{
	if( m_pVideo ) {
		return m_pVideo->Repaint( m_hwnd, hdc );
	}
	else {
		return S_OK;
	}
}

// Notifies the video renderer that the display mode changed.
//
// Call this method when the application receives WM_DISPLAYCHANGE.

HRESULT DirectShowPlayer::DisplayModeChanged()
{
	if( m_pVideo ) {
		return m_pVideo->DisplayModeChanged();
	}
	else {
		return S_OK;
	}
}


// Graph building

// Create a new filter graph. 
HRESULT DirectShowPlayer::InitializeGraph()
{
	HRESULT hr = S_OK;

	TearDownGraph();

	do {
		// Create the Filter Graph Manager.
		HRESULT hr = CoCreateInstance( CLSID_FilterGraph, NULL,
									   CLSCTX_INPROC_SERVER, IID_PPV_ARGS( &m_pGraph ) );
		BREAK_ON_FAIL( hr );

		hr = m_pGraph->QueryInterface( IID_PPV_ARGS( &m_pControl ) );
		BREAK_ON_FAIL( hr );

		hr = m_pGraph->QueryInterface( IID_PPV_ARGS( &m_pEvent ) );
		BREAK_ON_FAIL( hr );

		// Set up event notification.
		hr = m_pEvent->SetNotifyWindow( (OAHWND) m_hwnd, WM_PLAYER_EVENT, NULL );
		BREAK_ON_FAIL( hr );

		m_state = STATE_STOPPED;
	} while( false );

	return hr;
}

void DirectShowPlayer::TearDownGraph()
{
	// Stop sending event messages
	if( m_pEvent ) {
		m_pEvent->SetNotifyWindow( (OAHWND) NULL, NULL, NULL );
	}

	SafeRelease( m_pGraph );
	SafeRelease( m_pControl );
	SafeRelease( m_pEvent );

	delete m_pVideo;
	m_pVideo = NULL;

	m_state = STATE_NO_GRAPH;
}


HRESULT DirectShowPlayer::CreateVideoRenderer()
{
	HRESULT hr = E_FAIL;

	enum { Try_EVR, Try_VMR9, Try_VMR7 };

	for( DWORD i = Try_EVR; i <= Try_VMR7; i++ ) {
		switch( i ) {
		case Try_EVR:
			m_pVideo = new ( std::nothrow ) CEVR();
			break;

		case Try_VMR9:
			m_pVideo = new ( std::nothrow ) CVMR9();
			break;

		case Try_VMR7:
			m_pVideo = new ( std::nothrow ) CVMR7();
			break;
		}

		if( m_pVideo == NULL ) {
			hr = E_OUTOFMEMORY;
			break;
		}

		hr = m_pVideo->AddToGraph( m_pGraph, m_hwnd );
		if( SUCCEEDED( hr ) ) {
			break;
		}

		delete m_pVideo;
		m_pVideo = NULL;
	}
	return hr;
}


// Render the streams from a source filter. 

HRESULT DirectShowPlayer::RenderStreams( IBaseFilter *pSource )
{
	HRESULT hr = S_OK;

	BOOL bRenderedAnyPin = FALSE;

	do {
		ScopedComPtr<IFilterGraph2> pGraph2;
		HRESULT hr = m_pGraph->QueryInterface( IID_PPV_ARGS( &pGraph2 ) );
		BREAK_ON_FAIL( hr );

		// Add the video renderer to the graph
		hr = CreateVideoRenderer();
		BREAK_ON_FAIL( hr );

		// Add the DSound Renderer to the graph.
		ScopedComPtr<IBaseFilter> pAudioRenderer;
		hr = AddFilterByCLSID( m_pGraph, CLSID_DSoundRender,
							   &pAudioRenderer, L"Audio Renderer" );
		BREAK_ON_FAIL( hr );

		// Enumerate the pins on the source filter.
		ScopedComPtr<IEnumPins> pEnum;
		hr = pSource->EnumPins( &pEnum );
		BREAK_ON_FAIL( hr );

		// Loop through all the pins
		IPin *pPin;
		while( S_OK == pEnum->Next( 1, &pPin, NULL ) ) {
			// Try to render this pin. 
			// It's OK if we fail some pins, if at least one pin renders.
			HRESULT hr2 = pGraph2->RenderEx( pPin, AM_RENDEREX_RENDERTOEXISTINGRENDERERS, NULL );

			pPin->Release();
			if( SUCCEEDED( hr2 ) ) {
				bRenderedAnyPin = TRUE;
			}
		}

		hr = m_pVideo->FinalizeGraph( m_pGraph );
		BREAK_ON_FAIL( hr );

		// Remove the audio renderer, if not used.
		BOOL bRemoved;
		hr = RemoveUnconnectedRenderer( m_pGraph, pAudioRenderer, &bRemoved );
	} while( false );

	// If we succeeded to this point, make sure we rendered at least one 
	// stream.
	if( SUCCEEDED( hr ) ) {
		if( !bRenderedAnyPin ) {
			hr = VFW_E_CANNOT_RENDER;
		}
	}

	return hr;
}

}
}
}

#endif // CINDER_MSW
