#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/CinderMswCom.h"
#include "cinder/evr/IPlayer.h"
#include "cinder/evr/DirectShowVideo.h"

#include <dshow.h>

namespace cinder {
	namespace msw {
		namespace video {

			//! Definitions
			typedef void ( CALLBACK *GraphEventFN )( HWND hwnd, long eventCode, LONG_PTR param1, LONG_PTR param2 );

			//!
			class DirectShowPlayer : public IPlayer {
				typedef enum PlayerState {
					STATE_NO_GRAPH,
					STATE_RUNNING,
					STATE_PAUSED,
					STATE_STOPPED
				};

			public:
				DirectShowPlayer( HRESULT &hr, HWND hwnd );
				virtual ~DirectShowPlayer();

				// IUnknown methods
				STDMETHODIMP QueryInterface( REFIID iid, void** ppv ) override;
				STDMETHODIMP_( ULONG ) AddRef() override;
				STDMETHODIMP_( ULONG ) Release() override;

				// IPlayer methods
				HRESULT OpenFile( PCWSTR pszFileName ) override;
				HRESULT Close() override;

				HRESULT Play() override;
				HRESULT Pause() override;
				HRESULT Stop() override;

				HRESULT HandleEvent( UINT_PTR pEventPtr ) override;

				BOOL    IsPlaying() const override { return ( m_state == STATE_RUNNING ); }
				BOOL    IsPaused() const override { return ( m_state == STATE_PAUSED ); }
				BOOL    IsStopped() const override { return ( m_state == STATE_STOPPED ); }

				UINT32  GetWidth() const override { return (UINT32) m_Width; }
				UINT32  GetHeight() const override { return (UINT32) m_Height; }

				ci::gl::Texture2dRef GetTexture() override { assert( m_pVideo ); return m_pVideo->GetTexture(); }

				//
				PlayerState State() const { return m_state; }

				BOOL    HasVideo() const;
				HRESULT UpdateVideoWindow( const LPRECT prc );
				HRESULT Repaint( HDC hdc );
				HRESULT DisplayModeChanged();

				HRESULT HandleGraphEvent( GraphEventFN pfnOnGraphEvent );

			protected:
				HRESULT         InitializeGraph();
				void            TearDownGraph();
				virtual HRESULT CreateVideoRenderer();
				virtual HRESULT RenderStreams( IBaseFilter *pSource );

				PlayerState      m_state;

				long             m_Width;
				long             m_Height;

				HWND             m_hwnd; // Video window. This window also receives graph events.

				IGraphBuilder   *m_pGraph;
				IMediaControl   *m_pControl;
				IMediaEventEx   *m_pEvent;
				VideoRenderer   *m_pVideo;

			private:
				volatile long    mRefCount;
			};

		} // namespace video
	} // namespace msw
} // namespace cinder

#endif // CINDER_MSW

