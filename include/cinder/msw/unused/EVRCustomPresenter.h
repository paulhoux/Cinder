/*
Copyright (c) 2015, The Cinder Project, All rights reserved.

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

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/D3DPresentEngine9.h"
#include "cinder/msw/MediaFoundationFramework.h"

#pragma comment(lib, "evr.lib")

//#include <map>

//#include "cinder/gl/gl.h" // included to avoid error C2120 when including "wgl_all.h"
//#include "glload/wgl_all.h"

//#include "IRenderer.h"
//#include "SharedTexture.h"

namespace cinder {
namespace msw {

// Version number for the video samples. When the presenter increments the version
// number, all samples with the previous version number are stale and should be
// discarded.
static const GUID MFSamplePresenter_SampleCounter =
{ 0xb0bb83cc, 0xf10f, 0x4e2e, { 0xaa, 0x2b, 0x29, 0xea, 0x5e, 0x92, 0xef, 0x85 } };

class __declspec( uuid( "9A6E430D-27EE-4DBB-9A7F-7782EA4036A0" ) ) EVRCustomPresenter :
	public IMFVideoDeviceID,
	public IMFVideoPresenter, // Inherits IMFClockStateSink
	public IMFRateSupport,
	//public IMFRateControl,
	public IMFGetService,
	public IMFTopologyServiceLookupClient,
	public IMFVideoDisplayControl {
public:
	typedef enum {
		Started = 1,
		Stopped,
		Paused,
		Shutdown
	} RenderState;

	typedef enum {
		None = 0,
		WaitingStart,
		Pending,
		Scheduled,
		Complete
	} FramestepState;

public:
	EVRCustomPresenter();
	~EVRCustomPresenter();

	// IUnknown methods
	STDMETHOD( QueryInterface )( REFIID riid, void ** ppv ) override;
	STDMETHOD_( ULONG, AddRef )( ) override;
	STDMETHOD_( ULONG, Release )( ) override;

	// IMFGetService methods
	STDMETHOD( GetService )( REFGUID guidService, REFIID riid, LPVOID *ppvObject ) override;

	// IMFVideoPresenter methods
	STDMETHOD( ProcessMessage )( MFVP_MESSAGE_TYPE eMessage, ULONG_PTR ulParam ) override;
	STDMETHOD( GetCurrentMediaType )( IMFVideoMediaType** ppMediaType ) override;

	// IMFClockStateSink methods
	STDMETHOD( OnClockStart )( MFTIME hnsSystemTime, LONGLONG llClockStartOffset ) override;
	STDMETHOD( OnClockStop )( MFTIME hnsSystemTime ) override;
	STDMETHOD( OnClockPause )( MFTIME hnsSystemTime ) override;
	STDMETHOD( OnClockRestart )( MFTIME hnsSystemTime ) override;
	STDMETHOD( OnClockSetRate )( MFTIME hnsSystemTime, float flRate ) override;

	// IMFRateSupport methods
	STDMETHOD( GetSlowestRate )( MFRATE_DIRECTION eDirection, BOOL bThin, float *pflRate ) override;
	STDMETHOD( GetFastestRate )( MFRATE_DIRECTION eDirection, BOOL bThin, float *pflRate ) override;
	STDMETHOD( IsRateSupported )( BOOL bThin, float flRate, float *pflNearestSupportedRate ) override;

	// IMFRateControl methods
	//STDMETHOD( GetRate )( BOOL *pfThin, float *pflRate ) override;
	//STDMETHOD( SetRate )( BOOL fThin, float flRate ) override;

	// IMFVideoDeviceID methods
	STDMETHOD( GetDeviceID )( IID* pDeviceID ) override;

	// IMFTopologyServiceLookupClient methods
	STDMETHOD( InitServicePointers )( IMFTopologyServiceLookup *pLookup ) override;
	STDMETHOD( ReleaseServicePointers )( ) override;

	// IMFVideoDisplayControl methods
	STDMETHOD( GetNativeVideoSize )( SIZE* pszVideo, SIZE* pszARVideo ) override;
	STDMETHOD( GetIdealVideoSize )( SIZE* pszMin, SIZE* pszMax ) override;
	STDMETHOD( SetVideoPosition )( const MFVideoNormalizedRect* pnrcSource, const LPRECT prcDest ) override;
	STDMETHOD( GetVideoPosition )( MFVideoNormalizedRect* pnrcSource, LPRECT prcDest ) override;
	STDMETHOD( SetAspectRatioMode )( DWORD dwAspectRatioMode ) override { return E_NOTIMPL; }
	STDMETHOD( GetAspectRatioMode )( DWORD* pdwAspectRatioMode ) override { return E_NOTIMPL; }
	STDMETHOD( SetVideoWindow )( HWND hwndVideo ) override;
	STDMETHOD( GetVideoWindow )( HWND* phwndVideo ) override;
	STDMETHOD( RepaintVideo )( ) override;
	STDMETHOD( GetCurrentImage )( BITMAPINFOHEADER* pBih, BYTE** pDib, DWORD* pcbDib, LONGLONG* pTimeStamp ) override { return E_NOTIMPL; }
	STDMETHOD( SetBorderColor )( COLORREF Clr ) override { return E_NOTIMPL; }
	STDMETHOD( GetBorderColor )( COLORREF* pClr ) override { return E_NOTIMPL; }
	STDMETHOD( SetRenderingPrefs )( DWORD dwRenderFlags ) override { return E_NOTIMPL; }
	STDMETHOD( GetRenderingPrefs )( DWORD* pdwRenderFlags ) override { return E_NOTIMPL; }
	STDMETHOD( SetFullscreen )( BOOL bFullscreen ) override { return E_NOTIMPL; }
	STDMETHOD( GetFullscreen )( BOOL* pbFullscreen ) override { return E_NOTIMPL; }

	HANDLE GetSharedDeviceHandle();

private:
	// CheckShutdown: 
	//     Returns MF_E_SHUTDOWN if the presenter is shutdown.
	//     Call this at the start of any methods that should fail after shutdown.
	inline HRESULT CheckShutdown() const
	{
		if( mRenderState == Shutdown )
			return MF_E_SHUTDOWN;

		return S_OK;
	}

	// IsActive: The "active" state is started or paused.
	inline BOOL IsActive() const { return ( ( mRenderState == Started ) || ( mRenderState == Paused ) ); }

	// IsScrubbing: Scrubbing occurs when the frame rate is 0.
	inline BOOL IsScrubbing() const { return mPlaybackRate == 0.0f; }

	// NotifyEvent: Send an event to the EVR through its IMediaEventSink interface.
	void NotifyEvent( long EventCode, LONG_PTR Param1, LONG_PTR Param2 )
	{
		if( mMediaEventSinkPtr )
			mMediaEventSinkPtr->Notify( EventCode, Param1, Param2 );
	}

	float GetMaxRate( BOOL bThin );

	// Mixer operations
	HRESULT ConfigureMixer( IMFTransform *pMixer );

	// Formats
	HRESULT CreateOptimalVideoType( IMFMediaType* pProposed, IMFMediaType **ppOptimal );
	HRESULT CalculateOutputRectangle( IMFMediaType *pProposed, RECT *prcOutput );
	HRESULT SetMediaType( IMFMediaType *pMediaType );
	HRESULT IsMediaTypeSupported( IMFMediaType *pMediaType );

	// Message handlers
	HRESULT Flush();
	HRESULT RenegotiateMediaType();
	HRESULT ProcessInputNotify();
	HRESULT BeginStreaming();
	HRESULT EndStreaming();
	HRESULT CheckEndOfStream();

	// Managing samples
	void    ProcessOutputLoop();
	HRESULT ProcessOutput();
	HRESULT DeliverSample( IMFSample *pSample, BOOL bRepaint );
	HRESULT TrackSample( IMFSample *pSample );
	void    ReleaseResources();

	// Frame-stepping
	HRESULT PrepareFrameStep( DWORD cSteps );
	HRESULT StartFrameStep();
	HRESULT DeliverFrameStepSample( IMFSample *pSample );
	HRESULT CompleteFrameStep( IMFSample *pSample );
	HRESULT CancelFrameStep();

	// Callbacks

	// Callback when a video sample is released.
	HRESULT OnSampleFree( IMFAsyncResult *pResult );

private:
	// FrameStep: Holds information related to frame-stepping. 
	// Note: The purpose of this structure is simply to organize the data in one variable.
	struct FrameStep {
		FrameStep() : state( None ), steps( 0 ), pSampleNoRef( NULL ) {}

		FramestepState   state;          // Current frame-step state.
		VideoSampleList  samples;        // List of pending samples for frame-stepping.
		DWORD            steps;          // Number of steps left.
		DWORD_PTR        pSampleNoRef;   // Identifies the frame-step sample.
	};

private:
	static const MFRatio EVRCustomPresenter::sDefaultFrameRate;

	AsyncCallback<EVRCustomPresenter>   m_SampleFreeCB;

	RenderState                 mRenderState;          // Rendering state.
	FrameStep                   mFrameStep;            // Frame-stepping information.

	CriticalSection             mObjectLock;           // Serializes our public methods.  

	// Samples and scheduling
	Scheduler                   mScheduler;            // Manages scheduling of samples.
	SamplePool                  mSamplePool;           // Pool of allocated samples.
	DWORD                       mTokenCounter;         // Counter. Incremented whenever we create new samples.

	// Rendering state
	BOOL                        m_bSampleNotify;        // Did the mixer signal it has an input sample?
	BOOL                        m_bRepaint;             // Do we need to repaint the last sample?
	BOOL                        m_bPrerolled;           // Have we presented at least one sample?
	BOOL                        m_bEndStreaming;        // Did we reach the end of the stream (EOS)?

	MFVideoNormalizedRect       mNormalizedRect;        // Source rectangle.
	float                       mPlaybackRate;          // Playback rate.

	SIZE                        mVideoSize;             // Size (width and height) of the video in pixels.
	SIZE                        mVideoAspectRatio;      // Aspect ratio (nominator and denominator) of the video.

	// Deletable objects.
	D3DPresentEngine9           *mD3DPresentEnginePtr;  // Rendering engine. (Never null if the constructor succeeds.)

	// COM interfaces.
	IMFClock                    *mClockPtr;             // The EVR's clock.
	IMFTransform                *mTransformPtr;         // The mixer.
	IMediaEventSink             *mMediaEventSinkPtr;    // The EVR's event-sink interface.
	IMFMediaType                *mMediaTypePtr;         // Output media type

	ULONG                       mRefCount;
};

} // namespace msw
} // namespace cinder
