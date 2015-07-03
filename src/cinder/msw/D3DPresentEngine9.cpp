
#include "cinder/msw/D3DPresentEngine9.h"

namespace cinder { namespace msw {

HRESULT FindAdapter( IDirect3D9 *pD3D9, HMONITOR hMonitor, UINT *puAdapterID )
{
	HRESULT hr = E_FAIL;

	UINT cAdapters = 0;
	UINT uAdapterID = (UINT)-1;

	cAdapters = pD3D9->GetAdapterCount();
	for(UINT i = 0; i < cAdapters; i++) {
		HMONITOR hMonitorTmp = pD3D9->GetAdapterMonitor( i );
		BREAK_ON_NULL( hMonitorTmp, E_FAIL );

		if(hMonitorTmp == hMonitor) {
			*puAdapterID = i;
			return S_OK;
		}
	}

	return hr;
}

// ----------------------------------------------------------------------------------------------------------

D3DPresentEngine9::D3DPresentEngine9()
	: mWnd( NULL ), mDeviceResetToken( 0 ), mD3D9Ptr( NULL ), mDevicePtr( NULL ), mDeviceManagerPtr( NULL )
	, mSurfacePtr( NULL )/*, m_pTexturePool( NULL )*/
{
	HRESULT hr = S_OK;

	do {
		SetRectEmpty( &mDestRect );

		ZeroMemory( &mDisplayMode, sizeof( mDisplayMode ) );

		hr = InitializeD3D();
		BREAK_ON_FAIL( hr );

		hr = CreateD3DDevice();
		BREAK_ON_FAIL( hr );
	} while( false );

	if(FAILED( hr ))
		throw Exception( "D3DPresentEngine9: failed to create device." );
}

D3DPresentEngine9::~D3DPresentEngine9()
{
	//m_pTexturePool.reset();

	SafeRelease( mDevicePtr );
	SafeRelease( mSurfacePtr );
	SafeRelease( mDeviceManagerPtr );
	SafeRelease( mD3D9Ptr );
}

HRESULT D3DPresentEngine9::GetService( REFGUID guidService, REFIID riid, void** ppv )
{
	assert( ppv != NULL );

	if( riid == __uuidof( IDirect3DDeviceManager9 ) ) {
		if( mDeviceManagerPtr == NULL ) {
			return MF_E_UNSUPPORTED_SERVICE;
		}

		*ppv = mDeviceManagerPtr;
		mDeviceManagerPtr->AddRef();
	}
	else
		return MF_E_UNSUPPORTED_SERVICE;

	return S_OK;
}

HRESULT D3DPresentEngine9::CheckFormat( D3DFORMAT format )
{
	HRESULT hr = S_OK;

	do {
		UINT uAdapter = D3DADAPTER_DEFAULT;
		D3DDEVTYPE type = D3DDEVTYPE_HAL;

		if( mDevicePtr ) {
			D3DDEVICE_CREATION_PARAMETERS params;
			hr = mDevicePtr->GetCreationParameters( &params );
			BREAK_ON_FAIL( hr );

			uAdapter = params.AdapterOrdinal;
			type = params.DeviceType;
		}

		D3DDISPLAYMODE mode;
		hr = mD3D9Ptr->GetAdapterDisplayMode( uAdapter, &mode );
		BREAK_ON_FAIL( hr );

		hr = mD3D9Ptr->CheckDeviceType( uAdapter, type, mode.Format, format, TRUE );
		BREAK_ON_FAIL( hr );
	} while( false );

	return hr;
}

HRESULT D3DPresentEngine9::SetVideoWindow( HWND hwnd )
{
	assert( IsWindow( hwnd ) );
	assert( hwnd != mWnd );

	HRESULT hr = S_OK;

	ScopedCriticalSection lock( mObjectLock );

	mWnd = hwnd;

	UpdateDestRect();

	// Recreate the device.
	hr = CreateD3DDevice();

	return hr;
}

HRESULT D3DPresentEngine9::SetDestinationRect( const RECT& rcDest )
{
	if( EqualRect( &rcDest, &mDestRect ) ) {
		return S_OK; // No change.
	}

	HRESULT hr = S_OK;

	ScopedCriticalSection lock( mObjectLock );

	mDestRect = rcDest;

	UpdateDestRect();

	return hr;
}

HRESULT D3DPresentEngine9::CreateVideoSamples( IMFMediaType *pFormat, VideoSampleList& videoSampleQueue )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( mObjectLock );

	do {
		BREAK_ON_NULL( mWnd, MF_E_INVALIDREQUEST );
		BREAK_ON_NULL( pFormat, MF_E_UNEXPECTED );

		ReleaseResources();

		// Get the swap chain parameters from the media type.
		D3DPRESENT_PARAMETERS pPresentParams;
		hr = GetSwapChainPresentParameters( pFormat, &pPresentParams );
		BREAK_ON_FAIL( hr );

		UpdateDestRect();

		// Create the video samples.
		for( int i = 0; i < PRESENTER_BUFFER_COUNT; i++ ) {
			ScopedComPtr<IDirect3DSwapChain9> pSwapChain;
			ScopedComPtr<IMFSample> pVideoSample;

			// Create a new swap chain.
			hr = mDevicePtr->CreateAdditionalSwapChain( &pPresentParams, &pSwapChain );
			BREAK_ON_FAIL( hr );

			// Create the video sample from the swap chain.
			hr = CreateD3DSample( pSwapChain, &pVideoSample );
			BREAK_ON_FAIL( hr );

			// Add it to the list.
			hr = videoSampleQueue.InsertBack( pVideoSample );
			BREAK_ON_FAIL( hr );

			// Set the swap chain pointer as a custom attribute on the sample. This keeps
			// a reference count on the swap chain, so that the swap chain is kept alive
			// for the duration of the sample's lifetime.
			hr = pVideoSample->SetUnknown( MFSamplePresenter_SampleSwapChain, pSwapChain );
			BREAK_ON_FAIL( hr );
		}
		BREAK_ON_FAIL( hr );

		// Let the derived class create any additional D3D resources that it needs.
		hr = OnCreateVideoSamples( pPresentParams );
		BREAK_ON_FAIL( hr );
	} while( false );

	//if( FAILED( hr ) )
	//	CI_LOG_E( "Failed to CreateVideoSamples: " << hr );

	if( FAILED( hr ) )
		ReleaseResources();

	return hr;
}

void D3DPresentEngine9::ReleaseResources()
{
	// Let the derived class release any resources it created.
	OnReleaseResources();

	SafeRelease( mSurfacePtr );
}

HRESULT D3DPresentEngine9::CheckDeviceState( DeviceState *pState )
{
	HRESULT hr = S_OK;

	ScopedCriticalSection lock( mObjectLock );

	// Check the device state. Not every failure code is a critical failure.
	hr = mDevicePtr->CheckDeviceState( mWnd );

	*pState = DeviceOK;

	switch( hr ) {
	case D3DERR_DEVICELOST:
	case D3DERR_DEVICEHUNG:
		// Lost/hung device. Destroy the device and create a new one.
		hr = CreateD3DDevice();
		if( SUCCEEDED( hr ) )
			*pState = DeviceReset;
		return hr;
	case D3DERR_DEVICEREMOVED:
		// This is a fatal error.
		*pState = DeviceRemoved;
		return hr;
	default:
		return S_OK;
	}
}

HRESULT D3DPresentEngine9::PresentSample( IMFSample* pSample, LONGLONG llTarget )
{
	HRESULT hr = S_OK;

	do {
		ScopedComPtr<IDirect3DSurface9> pSurface;

		if( pSample ) {
			// Get the buffer from the sample.
			ScopedComPtr<IMFMediaBuffer> pBuffer;
			hr = pSample->GetBufferByIndex( 0, &pBuffer );
			BREAK_ON_FAIL( hr );

			// Get the surface from the buffer.
			hr = ::MFGetService( pBuffer, MR_BUFFER_SERVICE, __uuidof( IDirect3DSurface9 ), (void**) &pSurface );
			BREAK_ON_FAIL( hr );
		}
		else if( mSurfacePtr ) {
			// Redraw from the last surface.
			mSurfacePtr->AddRef();
			pSurface.Attach( mSurfacePtr );
		}

		if( pSurface ) {
			// Get the swap chain from the surface.
			ScopedComPtr<IDirect3DSwapChain9> pSwapChain;
			hr = pSurface->GetContainer( __uuidof( IDirect3DSwapChain9 ), (LPVOID*) &pSwapChain );
			BREAK_ON_FAIL( hr );

			// Present the swap chain.
			hr = PresentSwapChain( pSwapChain, pSurface );
			BREAK_ON_FAIL( hr );

			// Store this pointer in case we need to repaint the surface.
			CopyComPtr( mSurfacePtr, pSurface.get() );
		}
		else {
			// No surface. All we can do is paint a black rectangle.
			PaintFrameWithGDI();
		}
	} while( false );

	//if( FAILED( hr ) )
	//	CI_LOG_E( "Failed to PresentSample: " << hr );

	if( FAILED( hr ) ) {
		if( hr == D3DERR_DEVICELOST || hr == D3DERR_DEVICENOTRESET || hr == D3DERR_DEVICEHUNG ) {
			// We failed because the device was lost. Fill the destination rectangle.
			PaintFrameWithGDI();

			// Ignore. We need to reset or re-create the device, but this method
			// is probably being called from the scheduler thread, which is not the
			// same thread that created the device. The Reset(Ex) method must be
			// called from the thread that created the device.

			// The presenter will detect the state when it calls CheckDeviceState() 
			// on the next sample.
			hr = S_OK;
		}
	}

	return hr;
}
/*
ci::gl::Texture2dRef D3DPresentEngine9::GetTexture()
{
	assert( m_pTexturePool );
	return m_pTexturePool->GetTexture();
}
*/
HRESULT D3DPresentEngine9::InitializeD3D()
{
	HRESULT hr = S_OK;

	assert( mD3D9Ptr == NULL );
	assert( mDeviceManagerPtr == NULL );

	do {
		// Create Direct3D
		hr = Direct3DCreate9Ex( D3D_SDK_VERSION, &mD3D9Ptr );
		BREAK_ON_FAIL( hr );

		// Create the device manager
		hr = DXVA2CreateDirect3DDeviceManager9( &mDeviceResetToken, &mDeviceManagerPtr );
		BREAK_ON_FAIL( hr );
	} while( false );

	//if( FAILED( hr ) )
	//	CI_LOG_E( "Failed to InitializeD3D: " << hr );

	return hr;
}

HRESULT D3DPresentEngine9::CreateD3DDevice()
{
	HRESULT hr = S_OK;

	do {
		// Hold the lock because we might be discarding an exisiting device.
		ScopedCriticalSection lock( mObjectLock );

		BREAK_ON_NULL( mD3D9Ptr, MF_E_NOT_INITIALIZED );
		BREAK_ON_NULL( mDeviceManagerPtr, MF_E_NOT_INITIALIZED );

		// Find the monitor for this window.
		UINT uAdapterID = D3DADAPTER_DEFAULT;
		if( mWnd ) {
			HMONITOR hMonitor = MonitorFromWindow( mWnd, MONITOR_DEFAULTTONEAREST );

			// Find the corresponding adapter.
			hr = FindAdapter( mD3D9Ptr, hMonitor, &uAdapterID );
			BREAK_ON_FAIL( hr );
		}

		// Get the device caps for this adapter.
		D3DCAPS9 ddCaps;
		ZeroMemory( &ddCaps, sizeof( ddCaps ) );

		hr = mD3D9Ptr->GetDeviceCaps( uAdapterID, D3DDEVTYPE_HAL, &ddCaps );
		BREAK_ON_FAIL( hr );

		DWORD vp = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
		if( ddCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT ) {
			vp = D3DCREATE_HARDWARE_VERTEXPROCESSING;
		}
		else {
			//CI_LOG_W( "Failed to enable hardware rendering. Using software rendering instead." );
		}

		// Note: The presenter creates additional swap chains to present the
		// video frames. Therefore, it does not use the device's implicit 
		// swap chain, so the size of the back buffer here is 1 x 1.

		D3DPRESENT_PARAMETERS pPresentParams;
		ZeroMemory( &pPresentParams, sizeof( pPresentParams ) );

		pPresentParams.BackBufferWidth = 1;
		pPresentParams.BackBufferHeight = 1;
		pPresentParams.Windowed = TRUE;
		pPresentParams.SwapEffect = D3DSWAPEFFECT_COPY;
		pPresentParams.BackBufferFormat = D3DFMT_UNKNOWN;
		pPresentParams.hDeviceWindow = GetDesktopWindow();
		pPresentParams.Flags = D3DPRESENTFLAG_VIDEO;
		pPresentParams.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

		// Create the device. The interop definition states D3DCREATE_MULTITHREADED is required, but it may vary by vendor.
		ScopedComPtr<IDirect3DDevice9Ex> pDevice;
		hr = mD3D9Ptr->CreateDeviceEx( uAdapterID, D3DDEVTYPE_HAL, pPresentParams.hDeviceWindow,
									  vp | D3DCREATE_NOWINDOWCHANGES | D3DCREATE_MULTITHREADED | D3DCREATE_FPU_PRESERVE, &pPresentParams, NULL, &pDevice );
		BREAK_ON_FAIL( hr );

		// Get the adapter display mode.
		hr = mD3D9Ptr->GetAdapterDisplayMode( uAdapterID, &mDisplayMode );
		BREAK_ON_FAIL( hr );

		// Reset the D3DDeviceManager with the new device 
		hr = mDeviceManagerPtr->ResetDevice( pDevice, mDeviceResetToken );
		BREAK_ON_FAIL( hr );

		// Create the texture pool.
		hr = CreateTexturePool(pDevice);
		BREAK_ON_FAIL( hr );

		CopyComPtr( mDevicePtr, pDevice.get() );
	} while( false );

	//if( FAILED( hr ) )
	//	CI_LOG_E( "Failed to CreateD3DDevice: " << hr );

	return hr;
}

HRESULT D3DPresentEngine9::CreateTexturePool( IDirect3DDevice9Ex *pDevice )
{
	assert( pDevice != NULL );

	//m_pTexturePool = std::make_shared<SharedTexturePool>( pDevice );

	return S_OK;
}


HRESULT D3DPresentEngine9::CreateD3DSample( IDirect3DSwapChain9 *pSwapChain, IMFSample **ppVideoSample )
{
	// Caller holds the object lock.

	HRESULT hr = S_OK;

	do {
		// Get the back buffer surface.
		ScopedComPtr<IDirect3DSurface9> pSurface;
		hr = pSwapChain->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pSurface );
		BREAK_ON_FAIL( hr );

		// Fill it with black.
		const D3DCOLOR clrBlack = D3DCOLOR_ARGB( 0xFF, 0x00, 0x00, 0x00 );
		hr = mDevicePtr->ColorFill( pSurface, NULL, clrBlack );
		BREAK_ON_FAIL( hr );

		// Create the sample.
		ScopedComPtr<IMFSample> pSample;
		hr = ::MFCreateVideoSampleFromSurface( pSurface, &pSample );
		BREAK_ON_FAIL( hr );

		// Return the pointer to the caller.
		*ppVideoSample = pSample;
		( *ppVideoSample )->AddRef();
	} while( false );

	//if( FAILED( hr ) )
	//	CI_LOG_E( "Failed to CreateD3DSample: " << hr );

	return hr;
}


HRESULT D3DPresentEngine9::PresentSwapChain( IDirect3DSwapChain9* pSwapChain, IDirect3DSurface9* pSurface )
{
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( mWnd, MF_E_INVALIDREQUEST );

		// Grab back buffer.
		ScopedComPtr<IDirect3DSurface9> pSurface;
		hr = pSwapChain->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pSurface );
		BREAK_ON_FAIL( hr );

		D3DSURFACE_DESC desc;
		hr = pSurface->GetDesc( &desc );
		BREAK_ON_FAIL( hr );

		// Obtain free surface from the texture pool.
		//assert( m_pTexturePool );

		/*//SharedTexture shared;
		hr = m_pTexturePool->GetFreeSurface( desc, &shared );
		BREAK_ON_FAIL( hr );

#if _DEBUG
		D3DSURFACE_DESC shared_desc;
		hr = shared.m_pD3DSurface->GetDesc( &shared_desc );
		if( SUCCEEDED( hr ) )
			assert( desc.Width == shared_desc.Width && desc.Height == shared_desc.Height );
#endif

		// Blit texture.
		hr = mDevicePtr->StretchRect( pSurface, NULL, shared.m_pD3DSurface, NULL, D3DTEXF_NONE );
		if( SUCCEEDED( hr ) ) {
			m_pTexturePool->AddAvailableSurface( shared );
		}
		else {
			assert( hr == D3DERR_INVALIDCALL );
			//m_pTexturePool->AddFreeSurface( shared );
		}
		//*/

		//
		hr = pSwapChain->Present( NULL, &mDestRect, mWnd, NULL, 0 ); // <-- Is this required if we can't see the window anyway?
		BREAK_ON_FAIL( hr );
	} while( false );

	//if( FAILED( hr ) )
	//	CI_LOG_E( "Failed to PresentSwapChain: " << hr );

	return hr;
}

void D3DPresentEngine9::PaintFrameWithGDI()
{
	HDC hdc = GetDC( mWnd );

	if( hdc ) {
		HBRUSH hBrush = CreateSolidBrush( RGB( 0, 0, 0 ) );

		if( hBrush ) {
			FillRect( hdc, &mDestRect, hBrush );
			DeleteObject( hBrush );
		}

		ReleaseDC( mWnd, hdc );
	}
}

HRESULT D3DPresentEngine9::GetSwapChainPresentParameters( IMFMediaType *pType, D3DPRESENT_PARAMETERS* pPP )
{
	// Caller holds the object lock.

	HRESULT hr = S_OK;

	UINT32 width = 0, height = 0;
	DWORD d3dFormat = 0;

	// Helper object for reading the proposed type.
	VideoType videoType( pType );

	if( mWnd == NULL ) {
		return MF_E_INVALIDREQUEST;
	}

	ZeroMemory( pPP, sizeof( D3DPRESENT_PARAMETERS ) );

	// Get some information about the video format.
	hr = videoType.GetFrameDimensions( &width, &height );
	hr = videoType.GetFourCC( &d3dFormat );

	ZeroMemory( pPP, sizeof( D3DPRESENT_PARAMETERS ) );
	pPP->BackBufferWidth = width;
	pPP->BackBufferHeight = height;
	pPP->Windowed = TRUE;
	pPP->SwapEffect = D3DSWAPEFFECT_COPY;
	pPP->BackBufferFormat = (D3DFORMAT) d3dFormat;
	pPP->hDeviceWindow = mWnd;
	pPP->Flags = D3DPRESENTFLAG_VIDEO;
	pPP->PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

	D3DDEVICE_CREATION_PARAMETERS params;
	hr = mDevicePtr->GetCreationParameters( &params );

	if( params.DeviceType != D3DDEVTYPE_HAL ) {
		pPP->Flags |= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	}

	return S_OK;
}

HRESULT D3DPresentEngine9::UpdateDestRect()
{
	if( mWnd == NULL ) {
		return MF_E_INVALIDREQUEST;
	}

	RECT rcView;
	GetClientRect( mWnd, &rcView );

	// Clip the destination rectangle to the window's client area.
	if( mDestRect.right > rcView.right ) {
		mDestRect.right = rcView.right;
	}

	if( mDestRect.bottom > rcView.bottom ) {
		mDestRect.bottom = rcView.bottom;
	}

	return S_OK;
}

}}
