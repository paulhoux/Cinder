
#include "cinder/msw/ScopedPtr.h"
#include "cinder/wmf/detail/MediaSink.h"
#include "cinder/wmf/detail/Presenter.h"

#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include <Mferror.h>
#include <VersionHelpers.h>

using namespace cinder::msw;

namespace cinder {
namespace wmf {

/////////////////////////////////////////////////////////////////////////////////////////////
//
// MediaSink class. - Implements the media sink.
//
// Notes:
// - Most public methods calls CheckShutdown. This method fails if the sink was shut down.
//
/////////////////////////////////////////////////////////////////////////////////////////////

CriticalSection MediaSink::s_csStreamSinkAndScheduler;

//-------------------------------------------------------------------
// Name: CreateInstance
// Description: Creates an instance of the DX11 Video Renderer sink object.
//-------------------------------------------------------------------

/* static */ HRESULT MediaSink::CreateInstance( _In_ REFIID iid, _COM_Outptr_ void** ppSink, const BOOL *quitFlag, const MFOptions &options )
{
	if( ppSink == NULL ) {
		return E_POINTER;
	}

	*ppSink = NULL;

	HRESULT hr = S_OK;
	MediaSink* pSink = new MediaSink( quitFlag, options ); // Created with ref count = 1.

	if( pSink == NULL ) {
		hr = E_OUTOFMEMORY;
	}

	if( SUCCEEDED( hr ) ) {
		hr = pSink->Initialize();
	}

	if( SUCCEEDED( hr ) ) {
		hr = pSink->QueryInterface( iid, ppSink );
	}

	SafeRelease( pSink );

	return hr;
}

// IUnknown methods

ULONG MediaSink::AddRef( void )
{
	return InterlockedIncrement( &m_nRefCount );
}

HRESULT MediaSink::QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv )
{
	if( !ppv ) {
		return E_POINTER;
	}
	if( iid == IID_IUnknown ) {
		*ppv = static_cast<IUnknown*>( static_cast<IMFMediaSink*>( this ) );
	}
	else if( iid == __uuidof( IMFMediaSink ) ) {
		*ppv = static_cast<IMFMediaSink*>( this );
	}
	else if( iid == __uuidof( IMFClockStateSink ) ) {
		*ppv = static_cast<IMFClockStateSink*>( this );
	}
	else if( iid == __uuidof( IMFGetService ) ) {
		*ppv = static_cast<IMFGetService*>( this );
	}
	else if( iid == IID_IMFRateSupport ) {
		*ppv = static_cast<IMFRateSupport*>( this );
	}
	else if( iid == IID_IMFMediaSinkPreroll ) {
		*ppv = static_cast<IMFMediaSinkPreroll*>( this );
	}
	else {
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	AddRef();
	return S_OK;
}

ULONG  MediaSink::Release( void )
{
	ULONG uCount = InterlockedDecrement( &m_nRefCount );
	if( uCount == 0 ) {
		delete this;
	}
	// For thread safety, return a temporary variable.
	return uCount;
}

///  IMFMediaSink methods.

//-------------------------------------------------------------------
// Name: AddStreamSink
// Description: Adds a new stream to the sink.
//
// Note: This sink has a fixed number of streams, so this method
//       always returns MF_E_STREAMSINKS_FIXED.
//-------------------------------------------------------------------

HRESULT MediaSink::AddStreamSink( DWORD dwStreamSinkIdentifier, __RPC__in_opt IMFMediaType* pMediaType, __RPC__deref_out_opt IMFStreamSink** ppStreamSink )
{
	return MF_E_STREAMSINKS_FIXED;
}

//-------------------------------------------------------------------
// Name: GetCharacteristics
// Description: Returns the characteristics flags.
//
// Note: This sink has a fixed number of streams.
//-------------------------------------------------------------------

HRESULT MediaSink::GetCharacteristics( __RPC__out DWORD* pdwCharacteristics )
{
	ScopedCriticalSection lock( m_csMediaSink );

	if( pdwCharacteristics == NULL ) {
		return E_POINTER;
	}

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		*pdwCharacteristics = MEDIASINK_FIXED_STREAMS | MEDIASINK_CAN_PREROLL;
	}

	return hr;
}

//-------------------------------------------------------------------
// Name: GetPresentationClock
// Description: Returns a pointer to the presentation clock.
//-------------------------------------------------------------------

HRESULT MediaSink::GetPresentationClock( __RPC__deref_out_opt IMFPresentationClock** ppPresentationClock )
{
	ScopedCriticalSection lock( m_csMediaSink );

	if( ppPresentationClock == NULL ) {
		return E_POINTER;
	}

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		if( m_pClock == NULL ) {
			hr = MF_E_NO_CLOCK; // There is no presentation clock.
		}
		else {
			// Return the pointer to the caller.
			*ppPresentationClock = m_pClock;
			( *ppPresentationClock )->AddRef();
		}
	}

	return hr;
}

//-------------------------------------------------------------------
// Name: GetStreamSinkById
// Description: Retrieves a stream by ID.
//-------------------------------------------------------------------

HRESULT MediaSink::GetStreamSinkById( DWORD dwStreamSinkIdentifier, __RPC__deref_out_opt IMFStreamSink** ppStreamSink )
{
	ScopedCriticalSection lock( m_csMediaSink );

	if( ppStreamSink == NULL ) {
		return E_POINTER;
	}

	// Fixed stream ID.
	if( dwStreamSinkIdentifier != STREAM_ID ) {
		return MF_E_INVALIDSTREAMNUMBER;
	}

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		*ppStreamSink = m_pStream;
		( *ppStreamSink )->AddRef();
	}

	return hr;
}

//-------------------------------------------------------------------
// Name: GetStreamSinkByIndex
// Description: Retrieves a stream by index.
//-------------------------------------------------------------------

HRESULT MediaSink::GetStreamSinkByIndex( DWORD dwIndex, __RPC__deref_out_opt IMFStreamSink** ppStreamSink )
{
	ScopedCriticalSection lock( m_csMediaSink );

	if( ppStreamSink == NULL ) {
		return E_POINTER;
	}

	// Fixed stream: Index 0.
	if( dwIndex > 0 ) {
		return MF_E_INVALIDINDEX;
	}

	HRESULT hr = CheckShutdown();
	if( SUCCEEDED( hr ) ) {
		*ppStreamSink = m_pStream;
		( *ppStreamSink )->AddRef();
	}

	return hr;
}

//-------------------------------------------------------------------
// Name: GetStreamSinkCount
// Description: Returns the number of streams.
//-------------------------------------------------------------------

HRESULT MediaSink::GetStreamSinkCount( __RPC__out DWORD* pcStreamSinkCount )
{
	ScopedCriticalSection lock( m_csMediaSink );

	if( pcStreamSinkCount == NULL ) {
		return E_POINTER;
	}

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		*pcStreamSinkCount = 1;  // Fixed number of streams.
	}

	return hr;

}

//-------------------------------------------------------------------
// Name: RemoveStreamSink
// Description: Removes a stream from the sink.
//
// Note: This sink has a fixed number of streams, so this method
//       always returns MF_E_STREAMSINKS_FIXED.
//-------------------------------------------------------------------

HRESULT MediaSink::RemoveStreamSink( DWORD dwStreamSinkIdentifier )
{
	return MF_E_STREAMSINKS_FIXED;
}

//-------------------------------------------------------------------
// Name: SetPresentationClock
// Description: Sets the presentation clock.
//
// pPresentationClock: Pointer to the clock. Can be NULL.
//-------------------------------------------------------------------

HRESULT MediaSink::SetPresentationClock( __RPC__in_opt IMFPresentationClock* pPresentationClock )
{
	ScopedCriticalSection lock( m_csMediaSink );

	HRESULT hr = CheckShutdown();

	// If we already have a clock, remove ourselves from that clock's
	// state notifications.
	if( SUCCEEDED( hr ) ) {
		if( m_pClock ) {
			hr = m_pClock->RemoveClockStateSink( this );
		}
	}

	// Register ourselves to get state notifications from the new clock.
	if( SUCCEEDED( hr ) ) {
		if( pPresentationClock ) {
			hr = pPresentationClock->AddClockStateSink( this );
		}
	}

	if( SUCCEEDED( hr ) ) {
		// Release the pointer to the old clock.
		// Store the pointer to the new clock.

		SafeRelease( m_pClock );
		m_pClock = pPresentationClock;
		if( m_pClock ) {
			m_pClock->AddRef();
		}
	}

	return hr;
}

//-------------------------------------------------------------------
// Name: Shutdown
// Description: Releases resources held by the media sink.
//-------------------------------------------------------------------

HRESULT MediaSink::Shutdown( void )
{
	ScopedCriticalSection lock( m_csMediaSink );

	HRESULT hr = MF_E_SHUTDOWN;

	m_IsShutdown = TRUE;

	if( m_pStream != NULL ) {
		m_pStream->Shutdown();
	}

	if( m_pPresenter != NULL ) {
		m_pPresenter->Shutdown();
	}

	SafeRelease( m_pClock );
	SafeRelease( m_pStream );
	SafeRelease( m_pPresenter );

	if( m_pScheduler != NULL ) {
		hr = m_pScheduler->StopScheduler();
	}

	SafeRelease( m_pScheduler );

	return hr;
}

//-------------------------------------------------------------------
// Name: OnClockPause
// Description: Called when the presentation clock paused.
//
// Note: For an archive sink, the paused state is equivalent to the
//       running (started) state. We still accept data and archive it.
//-------------------------------------------------------------------

HRESULT MediaSink::OnClockPause(
	/* [in] */ MFTIME hnsSystemTime )
{
	ScopedCriticalSection lock( m_csMediaSink );

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		hr = m_pStream->Pause();
	}

	return hr;
}

//-------------------------------------------------------------------
// Name: OnClockRestart
// Description: Called when the presentation clock restarts.
//-------------------------------------------------------------------

HRESULT MediaSink::OnClockRestart(
	/* [in] */ MFTIME hnsSystemTime )
{
	ScopedCriticalSection lock( m_csMediaSink );

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		hr = m_pStream->Restart();
	}

	return hr;
}

//-------------------------------------------------------------------
// Name: OnClockSetRate
// Description: Called when the presentation clock's rate changes.
//
// Note: For a rateless sink, the clock rate is not important.
//-------------------------------------------------------------------

HRESULT MediaSink::OnClockSetRate(
	/* [in] */ MFTIME hnsSystemTime,
	/* [in] */ float flRate )
{
	if( m_pScheduler != NULL ) {
		// Tell the scheduler about the new rate.
		m_pScheduler->SetClockRate( flRate );
	}

	return S_OK;
}

//-------------------------------------------------------------------
// Name: OnClockStart
// Description: Called when the presentation clock starts.
//
// hnsSystemTime: System time when the clock started.
// llClockStartOffset: Starting presentatation time.
//
// Note: For an archive sink, we don't care about the system time.
//       But we need to cache the value of llClockStartOffset. This
//       gives us the earliest time stamp that we archive. If any
//       input samples have an earlier time stamp, we discard them.
//-------------------------------------------------------------------

HRESULT MediaSink::OnClockStart(
	/* [in] */ MFTIME hnsSystemTime,
	/* [in] */ LONGLONG llClockStartOffset )
{
	ScopedCriticalSection lock( m_csMediaSink );

	HRESULT hr = CheckShutdown();
	if( FAILED( hr ) ) {
		return hr;
	}

	// Check if the clock is already active (not stopped).
	// And if the clock position changes while the clock is active, it
	// is a seek request. We need to flush all pending samples.
	if( m_pStream->IsActive() && llClockStartOffset != PRESENTATION_CURRENT_POSITION ) {
		// This call blocks until the scheduler threads discards all scheduled samples.
		hr = m_pStream->Flush();
	}
	else {
		if( m_pScheduler != NULL ) {
			// Start the scheduler thread.
			hr = m_pScheduler->StartScheduler( m_pClock );
		}
	}

	if( SUCCEEDED( hr ) ) {
		hr = m_pStream->Start( llClockStartOffset );
	}

	return hr;
}

//-------------------------------------------------------------------
// Name: OnClockStop
// Description: Called when the presentation clock stops.
//
// Note: After this method is called, we stop accepting new data.
//-------------------------------------------------------------------

HRESULT MediaSink::OnClockStop(
	/* [in] */ MFTIME hnsSystemTime )
{
	ScopedCriticalSection lock( m_csMediaSink );

	HRESULT hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		hr = m_pStream->Stop();
	}

	if( SUCCEEDED( hr ) ) {
		if( m_pScheduler != NULL ) {
			// Stop the scheduler thread.
			hr = m_pScheduler->StopScheduler();
		}
	}

	return hr;
}

//-------------------------------------------------------------------------
// Name: GetService
// Description: IMFGetService
//-------------------------------------------------------------------------

HRESULT MediaSink::GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject )
{
	HRESULT hr = S_OK;

	if( guidService == MF_RATE_CONTROL_SERVICE ) {
		hr = QueryInterface( riid, ppvObject );
	}
	else if( guidService == MR_VIDEO_RENDER_SERVICE ) {
		hr = m_pPresenter->QueryInterface( riid, ppvObject );
	}
	else if( guidService == MR_VIDEO_ACCELERATION_SERVICE ) {
		hr = m_pPresenter->GetService( guidService, riid, ppvObject );
	}
	else {
		hr = MF_E_UNSUPPORTED_SERVICE;
	}

	return hr;
}

STDMETHODIMP MediaSink::GetFastestRate(
	MFRATE_DIRECTION eDirection,
	BOOL fThin,
	_Out_ float *pflRate
	)
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_csMediaSink );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		if( NULL == pflRate ) {
			hr = E_POINTER;
			break;
		}

		float rate;

		hr = m_pStream->GetMaxRate( fThin, &rate );
		BREAK_ON_FAIL( hr );

		if( MFRATE_FORWARD == eDirection ) {
			*pflRate = rate;
		}
		else {
			*pflRate = -rate;
		}
	} while( FALSE );

	return hr;
}

//-------------------------------------------------------------------------
// Name: GetSlowestRate
// Description: IMFRateSupport
//-------------------------------------------------------------------------

STDMETHODIMP MediaSink::GetSlowestRate(
	MFRATE_DIRECTION eDirection,
	BOOL fThin,
	_Out_ float* pflRate
	)
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_csMediaSink );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		if( NULL == pflRate ) {
			hr = E_POINTER;
			break;
		}

		if( SUCCEEDED( hr ) ) {
			//
			// We go as slow as you want!
			//
			*pflRate = 0;
		}
	} while( FALSE );

	return hr;
}

STDMETHODIMP MediaSink::IsRateSupported( BOOL fThin, float flRate, __RPC__inout_opt float* pflNearestSupportedRate )
{
	HRESULT hr = S_OK;
	float flNearestSupportedRate = flRate;

	ScopedCriticalSection lock( m_csMediaSink );

	do {
		hr = CheckShutdown();
		BREAK_ON_FAIL( hr );

		//
		// Only support rates up to the refresh rate of the monitor.
		// This check makes sense only if we're going to be receiving
		// all frames
		//
		if( !fThin ) {
			float rate;

			hr = m_pStream->GetMaxRate( fThin, &rate );
			if( FAILED( hr ) ) {
				break;
			}

			if( ( flRate > 0 && flRate > (float)rate ) ||
				( flRate < 0 && flRate < -(float)rate ) ) {
				hr = MF_E_UNSUPPORTED_RATE;
				flNearestSupportedRate = ( flRate >= 0.0f ) ? rate : -rate;

				break;
			}
		}
	} while( FALSE );

	if( NULL != pflNearestSupportedRate ) {
		*pflNearestSupportedRate = flNearestSupportedRate;
	}

	return hr;
}

STDMETHODIMP MediaSink::NotifyPreroll( MFTIME hnsUpcomingStartTime )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( m_csMediaSink );

	hr = CheckShutdown();

	if( SUCCEEDED( hr ) ) {
		hr = m_pStream->Preroll();
	}

	return hr;
}

/// Private methods

//-------------------------------------------------------------------
// MediaSink constructor.
//-------------------------------------------------------------------

MediaSink::MediaSink( const BOOL *quitFlag, const MFOptions &options )
	: STREAM_ID( 0 )
	, m_nRefCount( 1 )
	, m_csMediaSink() // default ctor
	, m_IsShutdown( FALSE )
	, m_pStream( NULL )
	, m_pClock( NULL )
	, m_pScheduler( NULL )
	, m_pPresenter( NULL )
	, m_quitFlag( quitFlag )
	, m_options( options )
{
}

//-------------------------------------------------------------------
// MediaSink destructor.
//-------------------------------------------------------------------

MediaSink::~MediaSink( void )
{
}

HRESULT MediaSink::CheckShutdown( void ) const
{
	if( m_IsShutdown ) {
		return MF_E_SHUTDOWN;
	}
	else {
		return S_OK;
	}
}

//-------------------------------------------------------------------
// Name: Initialize
// Description: Initializes the media sink.
//
// Note: This method is called once when the media sink is first
//       initialized.
//-------------------------------------------------------------------

HRESULT MediaSink::Initialize( void )
{
	HRESULT hr = S_OK;

	do {
		m_pScheduler = new Scheduler( m_quitFlag, s_csStreamSinkAndScheduler );
		BREAK_ON_NULL( m_pScheduler, E_OUTOFMEMORY );

		m_pStream = new StreamSink( STREAM_ID, s_csStreamSinkAndScheduler, m_pScheduler );
		BREAK_ON_NULL( m_pStream, E_OUTOFMEMORY );

		// Try initializing the DX11 pipeline.
		if( NULL == m_pPresenter && m_options.getDXVersion() >= MFOptions::DX_11 ) {
			m_pPresenter = new PresenterDX11(); // Created with ref count = 1.
			BREAK_ON_NULL_MSG( m_pPresenter, E_OUTOFMEMORY, "Failed to create DX11 presenter." );

			hr = m_pPresenter->Initialize();
			if( FAILED( hr ) ) {
				CI_LOG_V( "Failed to create DX11 presenter." );
				SafeRelease( m_pPresenter );
				m_options.setDXVersion( MFOptions::DX_9 );
			}
		}

		// Try initializing the DX9 pipeline.
		if( NULL == m_pPresenter && m_options.getDXVersion() >= MFOptions::DX_9 ) {
			m_pPresenter = new PresenterDX9(); // Created with ref count = 1.
			BREAK_ON_NULL_MSG( m_pPresenter, E_OUTOFMEMORY, "Failed to create DX9 presenter." );

			hr = m_pPresenter->Initialize();
			if( FAILED( hr ) ){
				CI_LOG_V( "Failed to create DX9 presenter." );
				SafeRelease( m_pPresenter );
			}
		}

		BREAK_ON_FAIL( hr );

		ScopedComPtr<IMFMediaSink> pSink;
		hr = QueryInterface( IID_PPV_ARGS( &pSink ) );
		BREAK_ON_FAIL( hr );

		hr = m_pStream->Initialize( pSink, m_pPresenter );
		BREAK_ON_FAIL( hr );

		m_pScheduler->SetCallback( static_cast<SchedulerCallback*>( m_pStream ) );
	} while( FALSE );

	if( FAILED( hr ) ) {
		Shutdown();
	}

	return hr;
}

} // namespace wmf
} // namespace cinder

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )