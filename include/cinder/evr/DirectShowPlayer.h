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
	~DirectShowPlayer();

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

	UINT32  GetWidth() const override { return (UINT32) m_Width; }
	UINT32  GetHeight() const override { return (UINT32) m_Height; }

	BOOL    CheckNewFrame() const override { return m_pVideo->CheckNewFrame(); }

	bool CreateSharedTexture( int w, int h, int textureID ) override { return m_pVideo->CreateSharedTexture( w, h, textureID ); }
	void ReleaseSharedTexture( int textureID ) override { m_pVideo->ReleaseSharedTexture( textureID ); }
	bool LockSharedTexture( int *pTextureID ) override { return m_pVideo->LockSharedTexture( pTextureID ); }
	bool UnlockSharedTexture( int textureID ) override { return m_pVideo->UnlockSharedTexture( textureID ); }

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

	long             m_Width;
	long             m_Height;

	HWND             m_hwnd; // Video window. This window also receives graph events.

	IGraphBuilder   *m_pGraph;
	IMediaControl   *m_pControl;
	IMediaEventEx   *m_pEvent;
	VideoRenderer   *m_pVideo;

	volatile long    mRefCount;
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW

