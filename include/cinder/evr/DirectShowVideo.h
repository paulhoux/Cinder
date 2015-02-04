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
	// Forward declare the GL texture.
	namespace gl {
		typedef std::shared_ptr<class Texture2d> Texture2dRef;
	}
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
	virtual HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const = 0;
	virtual ci::gl::Texture2dRef GetTexture() = 0;
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
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const override;
	ci::gl::Texture2dRef GetTexture() override;
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
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const override;
	ci::gl::Texture2dRef GetTexture() override;
};


// Manages the EVR video renderer filter.
class RendererEVR : public VideoRenderer {
	IBaseFilter*             m_pEVR;
	IMFVideoDisplayControl*  m_pVideoDisplay;
	EVRCustomPresenter*      m_pPresenter;

public:
	RendererEVR();
	~RendererEVR();
	BOOL    HasVideo() const override;
	HRESULT AddToGraph( IGraphBuilder *pGraph, HWND hwnd ) override;
	HRESULT FinalizeGraph( IGraphBuilder *pGraph ) override;
	HRESULT UpdateVideoWindow( HWND hwnd, const LPRECT prc ) override;
	HRESULT Repaint( HWND hwnd, HDC hdc ) override;
	HRESULT DisplayModeChanged() override;
	HRESULT GetNativeVideoSize( LONG *lpWidth, LONG *lpHeight ) const override;
	ci::gl::Texture2dRef GetTexture() override;
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


//-----------------------------------------------------------------------------------
// See: https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/ac877e2d-80a7-47b6-b315-5e3160b8b219/alternative-for-isamplegrabber?forum=windowsdirectshowdevelopment

interface ISampleGrabberCB : public IUnknown {
	virtual STDMETHODIMP SampleCB( double SampleTime, IMediaSample *pSample ) = 0;
	virtual STDMETHODIMP BufferCB( double SampleTime, BYTE *pBuffer, long BufferLen ) = 0;
};

interface ISampleGrabber : public IUnknown {
	virtual HRESULT STDMETHODCALLTYPE SetOneShot( BOOL OneShot ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetMediaType( const AM_MEDIA_TYPE *pType ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType( AM_MEDIA_TYPE *pType ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetBufferSamples( BOOL BufferThem ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer( long *pBufferSize, long *pBuffer ) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentSample( IMediaSample **ppSample ) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCallback( ISampleGrabberCB *pCallback, long WhichMethodToCallback ) = 0;
};

static const IID IID_ISampleGrabber = { 0x6B652FFF, 0x11FE, 0x4fce, { 0x92, 0xAD, 0x02, 0x66, 0xB5, 0xD7, 0xC7, 0x8F } };
static const IID IID_ISampleGrabberCB = { 0x0579154A, 0x2B53, 0x4994, { 0xB0, 0xD0, 0xE7, 0x73, 0x14, 0x8E, 0xFF, 0x85 } };
static const CLSID CLSID_SampleGrabber = { 0xC1F400A0, 0x3F08, 0x11d3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

#endif // CINDER_MSW
