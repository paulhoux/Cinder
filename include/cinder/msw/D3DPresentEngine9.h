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

#include "cinder/msw/MediaFoundationFramework.h"

#include <d3d9.h>
#include <d3d9types.h>
#include <dxva2api.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dxva2.lib")

namespace cinder {
namespace msw {

// Pointer to a Direct3D swap chain.
static const GUID MFSamplePresenter_SampleSwapChain =
{ 0xad885bd1, 0x7def, 0x414a,{ 0xb5, 0xb0, 0xd3, 0xd2, 0x63, 0xd6, 0xe9, 0x6d } };

inline float MFOffsetToFloat( const MFOffset& offset ) { return offset.value + ( float( offset.fract ) / 65536 ); }

class D3DPresentEngine9 : public SchedulerCallback {
public:
	// State of the Direct3D device.
	typedef enum {
		DeviceOK,
		DeviceReset,    // The device was reset OR re-created.
		DeviceRemoved,  // The device was removed.
	} DeviceState;

public:
	D3DPresentEngine9();
	~D3DPresentEngine9();

   // GetService: Returns the IDirect3DDeviceManager9 interface.
   // (The signature is identical to IMFGetService::GetService but 
   // this object does not derive from IUnknown.)
	HRESULT GetService( REFGUID guidService, REFIID riid, void** ppv );
	HRESULT CheckFormat( D3DFORMAT format );

   // Video window / destination rectangle:
   // This object implements a sub-set of the functions defined by the 
   // IMFVideoDisplayControl interface. However, some of the method signatures 
   // are different. The presenter's implementation of IMFVideoDisplayControl 
   // calls these methods.
	HRESULT SetVideoWindow( HWND hwnd );
	HWND    GetVideoWindow() const { return mWnd; }
	HRESULT SetDestinationRect( const RECT& rcDest );
	RECT    GetDestinationRect() const { return mDestRect; };

	HRESULT CreateVideoSamples( IMFMediaType *pFormat, VideoSampleList& videoSampleQueue );
	void    ReleaseResources();

	HRESULT CheckDeviceState( DeviceState *pState );
	HRESULT PresentSample( IMFSample* pSample, LONGLONG llTarget );

	UINT    RefreshRate() const { return mDisplayMode.RefreshRate; }

	void    OnReleaseResources() {}

private:
	HRESULT InitializeD3D();
	HRESULT GetSwapChainPresentParameters( IMFMediaType *pType, D3DPRESENT_PARAMETERS* pPP );
	HRESULT CreateD3DDevice();
	HRESULT CreateTexturePool( IDirect3DDevice9Ex *pDevice );
	HRESULT CreateD3DSample( IDirect3DSwapChain9 *pSwapChain, IMFSample **ppVideoSample );
	HRESULT UpdateDestRect();

	// A derived class can override these handlers to allocate any additional D3D resources.
	HRESULT OnCreateVideoSamples( D3DPRESENT_PARAMETERS& pp ) { return S_OK; }

	HRESULT PresentSwapChain( IDirect3DSwapChain9* pSwapChain, IDirect3DSurface9* pSurface );
	void    PaintFrameWithGDI();

private:
	static const int PRESENTER_BUFFER_COUNT = 3;

	UINT                        mDeviceResetToken;   // Reset token for the D3D device manager.

	HWND                        mWnd;                // Application-provided destination window.
	RECT                        mDestRect;           // Destination rectangle.
	D3DDISPLAYMODE              mDisplayMode;        // Adapter's display mode.

	CriticalSection             mObjectLock;         // Thread lock for the D3D device.

	// COM interfaces
	IDirect3D9Ex                *mD3D9Ptr;
	IDirect3DDevice9Ex          *mDevicePtr;
	IDirect3DDeviceManager9     *mDeviceManagerPtr;  // Direct3D device manager.
	IDirect3DSurface9           *mSurfacePtr;        // Surface for repaint requests.

	UINT32                      mWidth;              // Width of all shared textures.
	UINT32                      mHeight;             // Height of all shared textures.
};

}
}