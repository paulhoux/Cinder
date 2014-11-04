#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/CinderMswCom.h"
#include "cinder/evr/IPlayer.h"

#include <dshow.h>

namespace cinder {
namespace msw {
namespace video {

//! Forward declarations
class VideoRenderer;

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
	~DirectShowPlayer();

	// IUnknown methods
	STDMETHODIMP QueryInterface( REFIID iid, void** ppv ) override;
	STDMETHODIMP_( ULONG ) AddRef() override;
	STDMETHODIMP_( ULONG ) Release() override;

	// IPlayer methods
	HRESULT OpenFile( PCWSTR pszFileName ) override;

	HRESULT Play() override;
	HRESULT Pause() override;
	HRESULT Stop() override;

	HRESULT HandleEvent( UINT_PTR pEventPtr ) override;

	UINT32  GetWidth() override { return 1; }
	UINT32  GetHeight() override { return 1; }

	//
	PlayerState State() const { return m_state; }

	BOOL    HasVideo() const;
	HRESULT UpdateVideoWindow( const LPRECT prc );
	HRESULT Repaint( HDC hdc );
	HRESULT DisplayModeChanged();

	HRESULT HandleGraphEvent( GraphEventFN pfnOnGraphEvent );

private:
	HRESULT InitializeGraph();
	void    TearDownGraph();
	HRESULT CreateVideoRenderer();
	HRESULT RenderStreams( IBaseFilter *pSource );

	PlayerState      m_state;

	HWND             m_hwnd; // Video window. This window also receives graph events.

	IGraphBuilder   *m_pGraph;
	IMediaControl   *m_pControl;
	IMediaEventEx   *m_pEvent;
	VideoRenderer  *m_pVideo;

	volatile long    mRefCount;
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW

