#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/CinderMswCom.h"

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

class __declspec( uuid( "9A6E430D-27EE-4DBB-9A7F-7782EA4036A0" ) ) EVRCustomPresenter;

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
};

// Manages the VMR-7 video renderer filter.
class RendererVMR7 : public VideoRenderer {
	IVMRWindowlessControl   *m_pWindowless;

public:
	RendererVMR7();
	~RendererVMR7();
	BOOL    HasVideo() const;
	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd );
	HRESULT FinalizeGraph( IGraphBuilder *pGraph );
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc );
	HRESULT Repaint( HWND hwnd, HDC hdc );
	HRESULT DisplayModeChanged();
};


// Manages the VMR-9 video renderer filter.
class RendererVMR9 : public VideoRenderer {
	IVMRWindowlessControl9 *m_pWindowless;

public:
	RendererVMR9();
	~RendererVMR9();
	BOOL    HasVideo() const;
	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd );
	HRESULT FinalizeGraph( IGraphBuilder *pGraph );
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc );
	HRESULT Repaint( HWND hwnd, HDC hdc );
	HRESULT DisplayModeChanged();
};


// Manages the EVR video renderer filter.
class RendererEVR : public VideoRenderer {
	IBaseFilter            *m_pEVR;
	IMFVideoDisplayControl *m_pVideoDisplay;

	EVRCustomPresenter     *m_pPresenter;

public:
	RendererEVR();
	~RendererEVR();
	BOOL    HasVideo() const;
	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd );
	HRESULT FinalizeGraph( IGraphBuilder *pGraph );
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc );
	HRESULT Repaint( HWND hwnd, HDC hdc );
	HRESULT DisplayModeChanged();
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW
