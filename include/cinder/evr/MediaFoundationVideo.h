/*
Copyright (c) 2014, The Cinder Project, All rights reserved.

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

#include <map>

#include "cinder/Cinder.h"

#include "cinder/gl/gl.h" // included to avoid error C2120 when including "wgl_all.h"
#include "glload/wgl_all.h"

#if defined( CINDER_MSW )

#include "IRenderer.h"
#include "MediaFoundationFramework.h"
#include "SharedTexture.h"

namespace cinder {
	namespace msw {
		namespace video {

			inline float MFOffsetToFloat( const MFOffset& offset ) { return offset.value + ( float( offset.fract ) / 65536 ); }

			class D3DPresentEngine : public SchedulerCallback {
			public:
				// State of the Direct3D device.
				typedef enum DeviceState {
					DeviceOK,
					DeviceReset,    // The device was reset OR re-created.
					DeviceRemoved,  // The device was removed.
				};

			public:
				static const int PRESENTER_BUFFER_COUNT = 3;
				//static const int SHARED_TEXTURE_COUNT = 3;

				D3DPresentEngine( HRESULT& hr );
				virtual ~D3DPresentEngine();

				// GetService: Returns the IDirect3DDeviceManager9 interface.
				// (The signature is identical to IMFGetService::GetService but 
				// this object does not derive from IUnknown.)
				virtual HRESULT GetService( REFGUID guidService, REFIID riid, void** ppv );
				virtual HRESULT CheckFormat( D3DFORMAT format );

				// Video window / destination rectangle:
				// This object implements a sub-set of the functions defined by the 
				// IMFVideoDisplayControl interface. However, some of the method signatures 
				// are different. The presenter's implementation of IMFVideoDisplayControl 
				// calls these methods.
				HRESULT SetVideoWindow( HWND hwnd );
				HWND    GetVideoWindow() const { return m_hwnd; }
				HRESULT SetDestinationRect( const RECT& rcDest );
				RECT    GetDestinationRect() const { return m_rcDestRect; };

				HRESULT CreateVideoSamples( IMFMediaType *pFormat, VideoSampleList& videoSampleQueue );
				void    ReleaseResources();

				HRESULT CheckDeviceState( DeviceState *pState );
				HRESULT PresentSample( IMFSample* pSample, LONGLONG llTarget );

				UINT    RefreshRate() const { return m_DisplayMode.RefreshRate; }

				//! Returns the latest frame as an OpenGL texture, if available. Returns an empty texture if no (new) frame is available.
				ci::gl::Texture2dRef GetTexture();

			protected:
				HRESULT InitializeD3D();
				HRESULT GetSwapChainPresentParameters( IMFMediaType *pType, D3DPRESENT_PARAMETERS* pPP );
				HRESULT CreateD3DDevice();
				HRESULT CreateTexturePool( IDirect3DDevice9Ex *pDevice );
				HRESULT CreateD3DSample( IDirect3DSwapChain9 *pSwapChain, IMFSample **ppVideoSample );
				HRESULT UpdateDestRect();

				// A derived class can override these handlers to allocate any additional D3D resources.
				virtual HRESULT OnCreateVideoSamples( D3DPRESENT_PARAMETERS& pp ) { return S_OK; }

				virtual HRESULT PresentSwapChain( IDirect3DSwapChain9* pSwapChain, IDirect3DSurface9* pSurface );
				virtual void    PaintFrameWithGDI();

			protected:
				UINT                        m_DeviceResetToken;     // Reset token for the D3D device manager.

				HWND                        m_hwnd;                 // Application-provided destination window.
				RECT                        m_rcDestRect;           // Destination rectangle.
				D3DDISPLAYMODE              m_DisplayMode;          // Adapter's display mode.

				CriticalSection             m_ObjectLock;           // Thread lock for the D3D device.

				// COM interfaces
				IDirect3D9Ex                *m_pD3D9;
				IDirect3DDevice9Ex          *m_pDevice;
				IDirect3DDeviceManager9     *m_pDeviceManager;      // Direct3D device manager.
				IDirect3DSurface9           *m_pSurfaceRepaint;     // Surface for repaint requests.

			protected:
				int                          mWidth;                // Width of all shared textures.
				int                          mHeight;               // Height of all shared textures.

				std::shared_ptr<SharedTexturePool>  m_pTexturePool;

			public:
				virtual void OnReleaseResources() {}
			};

			// MFSamplePresenter_SampleCounter
			// Data type: UINT32
			//
			// Version number for the video samples. When the presenter increments the version
			// number, all samples with the previous version number are stale and should be
			// discarded.
			static const GUID MFSamplePresenter_SampleCounter =
			{ 0xb0bb83cc, 0xf10f, 0x4e2e, { 0xaa, 0x2b, 0x29, 0xea, 0x5e, 0x92, 0xef, 0x85 } };

			// MFSamplePresenter_SampleSwapChain
			// Data type: IUNKNOWN
			// 
			// Pointer to a Direct3D swap chain.
			static const GUID MFSamplePresenter_SampleSwapChain =
			{ 0xad885bd1, 0x7def, 0x414a, { 0xb5, 0xb0, 0xd3, 0xd2, 0x63, 0xd6, 0xe9, 0x6d } };

			class __declspec( uuid( "9A6E430D-27EE-4DBB-9A7F-7782EA4036A0" ) ) EVRCustomPresenter :
				public IRenderer,
				public IMFVideoDeviceID,
				public IMFVideoPresenter, // Inherits IMFClockStateSink
				public IMFRateSupport,
				//public IMFRateControl,
				public IMFGetService,
				public IMFTopologyServiceLookupClient,
				public IMFVideoDisplayControl {
			public:
				typedef enum RenderState {
					Started = 1,
					Stopped,
					Paused,
					Shutdown
				};

				typedef enum FramestepState {
					None = 0,
					WaitingStart,
					Pending,
					Scheduled,
					Complete
				};

			public:
				//static HRESULT CreateInstance( IUnknown *pUnkOuter, REFIID iid, void **ppv );
				EVRCustomPresenter( HRESULT& hr );
				virtual ~EVRCustomPresenter();

				// IUnknown methods
				STDMETHOD( QueryInterface )( REFIID riid, void ** ppv ) override;
				STDMETHOD_( ULONG, AddRef )( ) override;
				STDMETHOD_( ULONG, Release )( ) override;

				// IRenderer methods


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

				//! Returns the latest frame as an OpenGL texture, if available. Returns an empty texture if no (new) frame is available.
				ci::gl::Texture2dRef GetTexture() { assert( m_pD3DPresentEngine ); return m_pD3DPresentEngine->GetTexture(); };

			protected:

				// CheckShutdown: 
				//     Returns MF_E_SHUTDOWN if the presenter is shutdown.
				//     Call this at the start of any methods that should fail after shutdown.
				inline HRESULT CheckShutdown() const
				{
					if( m_RenderState == Shutdown )
						return MF_E_SHUTDOWN;

					return S_OK;

				}

				// IsActive: The "active" state is started or paused.
				inline BOOL IsActive() const
				{
					return ( ( m_RenderState == Started ) || ( m_RenderState == Paused ) );
				}

				// IsScrubbing: Scrubbing occurs when the frame rate is 0.
				inline BOOL IsScrubbing() const { return m_fRate == 0.0f; }

				// NotifyEvent: Send an event to the EVR through its IMediaEventSink interface.
				void NotifyEvent( long EventCode, LONG_PTR Param1, LONG_PTR Param2 )
				{
					if( m_pMediaEventSink ) {
						m_pMediaEventSink->Notify( EventCode, Param1, Param2 );
					}
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
				AsyncCallback<EVRCustomPresenter>   m_SampleFreeCB;

			protected:

				// FrameStep: Holds information related to frame-stepping. 
				// Note: The purpose of this structure is simply to organize the data in one variable.
				struct FrameStep {
					FrameStep() : state( None ), steps( 0 ), pSampleNoRef( NULL )
					{
					}

					FramestepState     state;          // Current frame-step state.
					VideoSampleList     samples;        // List of pending samples for frame-stepping.
					DWORD               steps;          // Number of steps left.
					DWORD_PTR           pSampleNoRef;   // Identifies the frame-step sample.
				};


			protected:
				static const MFRatio EVRCustomPresenter::sDefaultFrameRate;

				RenderState                 m_RenderState;          // Rendering state.
				FrameStep                   m_FrameStep;            // Frame-stepping information.

				CriticalSection             m_ObjectLock;           // Serializes our public methods.  

				// Samples and scheduling
				Scheduler                   m_scheduler;            // Manages scheduling of samples.
				SamplePool                  m_SamplePool;           // Pool of allocated samples.
				DWORD                       m_TokenCounter;         // Counter. Incremented whenever we create new samples.

				// Rendering state
				BOOL                        m_bSampleNotify;        // Did the mixer signal it has an input sample?
				BOOL                        m_bRepaint;             // Do we need to repaint the last sample?
				BOOL                        m_bPrerolled;           // Have we presented at least one sample?
				BOOL                        m_bEndStreaming;        // Did we reach the end of the stream (EOS)?

				MFVideoNormalizedRect       m_nrcSource;            // Source rectangle.
				float                       m_fRate;                // Playback rate.

				SIZE                        m_VideoSize;            // Size (width and height) of the video in pixels.
				SIZE                        m_VideoARSize;          // Aspect ratio (nominator and denominator) of the video.

				// Deletable objects.
				D3DPresentEngine            *m_pD3DPresentEngine;    // Rendering engine. (Never null if the constructor succeeds.)

				// COM interfaces.
				IMFClock                    *m_pClock;               // The EVR's clock.
				IMFTransform                *m_pMixer;               // The mixer.
				IMediaEventSink             *m_pMediaEventSink;      // The EVR's event-sink interface.
				IMFMediaType                *m_pMediaType;           // Output media type

				volatile long               mRefCount;
			public:
				HANDLE GetSharedDeviceHandle();
			};

		} // namespace video
	} // namespace msw
} // namespace cinder

#endif // CINDER_MSW

