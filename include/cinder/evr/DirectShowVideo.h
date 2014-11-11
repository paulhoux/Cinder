#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/CinderMswCom.h"
#include "cinder/evr/MediaFoundationVideo.h"

#include <dshow.h>
#include <d3d9.h>
#include <vmr9.h>
#include <evr.h>

namespace cinder {
namespace msw {
namespace video {

//! Forward declarations
HRESULT RemoveUnconnectedRenderer( IGraphBuilder *pGraph, IBaseFilter *pRenderer, BOOL *pbRemoved );
HRESULT AddFilterByCLSID( IGraphBuilder *pGraph, REFGUID clsid, IBaseFilter **ppF, LPCWSTR wszName );

// Abstract class to manage the video renderer filter.
// Specific implementations handle the VMR-7, VMR-9, or EVR filter.
class VideoRenderer {
public:
	virtual ~VideoRenderer() {};
	virtual BOOL    HasVideo() const = 0;
	virtual HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) = 0;
	virtual HRESULT FinalizeGraph( IGraphBuilder *pGraph ) = 0;
	virtual HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) = 0;
	virtual HRESULT Repaint( HWND hwnd, HDC hdc ) = 0;
	virtual HRESULT DisplayModeChanged() = 0;
	virtual HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) = 0;

	virtual bool CreateSharedTexture( int w, int h, int textureID ) = 0;
	virtual bool LockSharedTexture() = 0;
	virtual bool UnlockSharedTexture() = 0;
	virtual void ReleaseSharedTexture() = 0;
};

// Manages the VMR-7 video renderer filter.
class RendererVMR7 : public VideoRenderer {
	IVMRWindowlessControl   *m_pWindowless;

public:
	RendererVMR7();
	~RendererVMR7();
	BOOL    HasVideo() const override;
	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) override;
	HRESULT FinalizeGraph( IGraphBuilder *pGraph ) override;
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) override;
	HRESULT Repaint( HWND hwnd, HDC hdc ) override;
	HRESULT DisplayModeChanged() override;
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) override;

	bool CreateSharedTexture( int w, int h, int textureID ) override { throw std::runtime_error( "Not implemented" ); }
	bool LockSharedTexture() override { throw std::runtime_error( "Not implemented" ); }
	bool UnlockSharedTexture() override { throw std::runtime_error( "Not implemented" ); }
	void ReleaseSharedTexture() override { throw std::runtime_error( "Not implemented" ); }
};


// Manages the VMR-9 video renderer filter.
class RendererVMR9 : public VideoRenderer {
	IVMRWindowlessControl9 *m_pWindowless;

public:
	RendererVMR9();
	~RendererVMR9();
	BOOL    HasVideo() const override;
	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) override;
	HRESULT FinalizeGraph( IGraphBuilder *pGraph ) override;
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) override;
	HRESULT Repaint( HWND hwnd, HDC hdc ) override;
	HRESULT DisplayModeChanged() override;
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) override;

	bool CreateSharedTexture( int w, int h, int textureID ) override { throw std::runtime_error( "Not implemented" ); }
	bool LockSharedTexture() override { throw std::runtime_error( "Not implemented" ); }
	bool UnlockSharedTexture() override { throw std::runtime_error( "Not implemented" ); }
	void ReleaseSharedTexture() override { throw std::runtime_error( "Not implemented" ); }
};


// Manages the EVR video renderer filter.
class RendererEVR : public VideoRenderer {
	IBaseFilter            *m_pEVR;
	IMFVideoDisplayControl *m_pVideoDisplay;

	EVRCustomPresenter     *m_pPresenter;

public:
	RendererEVR();
	~RendererEVR();
	BOOL    HasVideo() const override;
	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) override;
	HRESULT FinalizeGraph( IGraphBuilder *pGraph ) override;
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) override;
	HRESULT Repaint( HWND hwnd, HDC hdc ) override;
	HRESULT DisplayModeChanged() override;
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) override;

	bool CreateSharedTexture( int w, int h, int textureID ) override { assert( m_pPresenter != NULL ); return m_pPresenter->createSharedTexture( w, h, textureID ); }
	bool LockSharedTexture() override { assert( m_pPresenter != NULL ); return m_pPresenter->lockSharedTexture(); }
	bool UnlockSharedTexture() override { assert( m_pPresenter != NULL ); return m_pPresenter->unlockSharedTexture(); }
	void ReleaseSharedTexture() override { assert( m_pPresenter != NULL ); m_pPresenter->releaseSharedTexture(); }
};


HRESULT InitializeEVR( IBaseFilter *pEVR, HWND hwnd, IMFVideoPresenter *pPresenter, IMFVideoDisplayControl **ppWc );
HRESULT InitWindowlessVMR9( IBaseFilter *pVMR, HWND hwnd, IVMRWindowlessControl9 **ppWc );
HRESULT InitWindowlessVMR( IBaseFilter *pVMR, HWND hwnd, IVMRWindowlessControl **ppWc );
HRESULT FindConnectedPin( IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin );
HRESULT IsPinConnected( IPin *pPin, BOOL *pResult );
HRESULT IsPinDirection( IPin *pPin, PIN_DIRECTION dir, BOOL *pResult );

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW
