#pragma once

#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/detail/Presenter.h"
#include "cinder/msw/detail/Scheduler.h"
#include "cinder/msw/detail/StreamSink.h"

namespace cinder {
namespace msw {
namespace detail {

class MediaSink :
	public IMFMediaSink,
	public IMFClockStateSink,
	public IMFGetService,
	public IMFRateSupport,
	public IMFMediaSinkPreroll {
public:

	// Static method to create the object.
	static HRESULT CreateInstance( _In_ REFIID iid, _COM_Outptr_ void** ppSink );

	// IUnknown
	STDMETHODIMP_( ULONG ) AddRef( void );
	STDMETHODIMP QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv );
	STDMETHODIMP_( ULONG ) Release( void );

	// IMFMediaSink methods
	STDMETHODIMP AddStreamSink( DWORD dwStreamSinkIdentifier, __RPC__in_opt IMFMediaType* pMediaType, __RPC__deref_out_opt IMFStreamSink** ppStreamSink );
	STDMETHODIMP GetCharacteristics( __RPC__out DWORD* pdwCharacteristics );
	STDMETHODIMP GetPresentationClock( __RPC__deref_out_opt IMFPresentationClock** ppPresentationClock );
	STDMETHODIMP GetStreamSinkById( DWORD dwIdentifier, __RPC__deref_out_opt IMFStreamSink** ppStreamSink );
	STDMETHODIMP GetStreamSinkByIndex( DWORD dwIndex, __RPC__deref_out_opt IMFStreamSink** ppStreamSink );
	STDMETHODIMP GetStreamSinkCount( __RPC__out DWORD* pcStreamSinkCount );
	STDMETHODIMP RemoveStreamSink( DWORD dwStreamSinkIdentifier );
	STDMETHODIMP SetPresentationClock( __RPC__in_opt IMFPresentationClock* pPresentationClock );
	STDMETHODIMP Shutdown( void );

	// IMFClockStateSink methods
	STDMETHODIMP OnClockPause( MFTIME hnsSystemTime );
	STDMETHODIMP OnClockRestart( MFTIME hnsSystemTime );
	STDMETHODIMP OnClockSetRate( MFTIME hnsSystemTime, float flRate );
	STDMETHODIMP OnClockStart( MFTIME hnsSystemTime, LONGLONG llClockStartOffset );
	STDMETHODIMP OnClockStop( MFTIME hnsSystemTime );

	// IMFGetService
	STDMETHODIMP GetService( __RPC__in REFGUID guidService, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppvObject );

	// IMFRateSupport
	STDMETHODIMP GetFastestRate( MFRATE_DIRECTION eDirection, BOOL fThin, _Out_ float* pflRate );
	STDMETHODIMP GetSlowestRate( MFRATE_DIRECTION eDirection, BOOL fThin, _Out_ float* pflRate );
	STDMETHODIMP IsRateSupported( BOOL fThin, float flRate, __RPC__inout_opt float* pflNearestSupportedRate );

	// IMFMediaSinkPreroll
	STDMETHODIMP NotifyPreroll( MFTIME hnsUpcomingStartTime );

private:

	static CriticalSection s_csStreamSinkAndScheduler; // critical section for thread safety, used for CStreamSink and CScheduler

	MediaSink( void );
	virtual ~MediaSink( void );

	HRESULT CheckShutdown( void ) const;
	HRESULT Initialize( void );

	const DWORD             STREAM_ID;      // The stream ID of the one stream on the sink.
	long                    m_nRefCount;    // reference count
	CriticalSection         m_csMediaSink;  // critical section for thread safety, used for CMediaSink
	BOOL                    m_IsShutdown;   // Flag to indicate if Shutdown() method was called.
	StreamSink*             m_pStream;      // Byte stream
	IMFPresentationClock*   m_pClock;       // Presentation clock.
	Scheduler*              m_pScheduler;    // Manages scheduling of samples.
	Presenter*              m_pPresenter;
};

} // namespace detail
} // namespace msw
} // namespace cinder
