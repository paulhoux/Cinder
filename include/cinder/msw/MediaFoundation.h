

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Exception.h"

#include "cinder/msw/MediaFoundationFramework.h"

namespace cinder {
namespace msw {

struct ScopedMFInitializer {
	ScopedMFInitializer() : ScopedMFInitializer( MF_VERSION, MFSTARTUP_FULL ) {}
	ScopedMFInitializer( ULONG version, DWORD params )
	{
		if( FAILED( ::MFStartup( version, params ) ) )
			throw Exception( "Failed to initialize Windows Media Foundation." );
	}

	~ScopedMFInitializer()
	{
		::MFShutdown();
	}
};

//------------------------------------------------------------------------------------

class MFPlayer : public IMFAsyncCallback {
public:
	typedef enum {
		Closed = 0,     // No session.
		Ready,          // Session was created, ready to open a file. 
		OpenPending,    // Session is opening a file.
		Started,        // Session is playing a file.
		Paused,         // Session is paused.
		Stopped,        // Session is stopped (ready to play). 
		Closing         // Application has closed the session, but is waiting for MESessionClosed.
	} State;

public:
	MFPlayer();

	//! 
	HRESULT OpenURL( const WCHAR *url, const WCHAR *audioDeviceId = 0 );
	//!
	HRESULT Close() { return CloseSession(); }

	//! Returns the current state.
	State GetState() const { return mState; }

	// IUnknown methods
	STDMETHODIMP QueryInterface( REFIID iid, void** ppv );
	STDMETHODIMP_( ULONG ) AddRef();
	STDMETHODIMP_( ULONG ) Release();

	// IMFAsyncCallback methods
	STDMETHODIMP GetParameters( DWORD*, DWORD* ) { return E_NOTIMPL; }
	//!  Callback for the asynchronous BeginGetEvent method.
	STDMETHODIMP Invoke( IMFAsyncResult* pAsyncResult );

private:
	//! Destructor is private, caller should call Release().
	~MFPlayer();

	//! Handle events received from Media Foundation. See: Invoke.
	LRESULT HandleEvent( WPARAM wParam );
	//! Allow MFWndProc access to HandleEvent.
	friend LRESULT CALLBACK MFWndProc( HWND, UINT, WPARAM, LPARAM );

	HRESULT OnTopologyStatus( IMFMediaEvent *pEvent ) { return S_OK; }
	HRESULT OnPresentationEnded( IMFMediaEvent *pEvent ) { return S_OK; }
	HRESULT OnNewPresentation( IMFMediaEvent *pEvent ) { return S_OK; }
	HRESULT OnSessionEvent( IMFMediaEvent*, MediaEventType ) { return S_OK; }

	HRESULT CreateSession();
	HRESULT CloseSession();

	HRESULT CreatePartialTopology( IMFPresentationDescriptor *pDescriptor );
	HRESULT SetMediaInfo( IMFPresentationDescriptor *pDescriptor );

	//! Creates a (hidden) window used by Media Foundation.
	void CreateWnd();
	//! Destroys the (hidden) window.
	void DestroyWnd();

	static void RegisterWindowClass();

	static HRESULT CreateMediaSource( LPCWSTR pUrl, IMFMediaSource **ppSource );
	static HRESULT CreatePlaybackTopology( IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology, IMFVideoPresenter *pVideoPresenter );
	static HRESULT CreateMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate, IMFVideoPresenter *pVideoPresenter, IMFMediaSink **ppMediaSink );
	static HRESULT AddSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode );
	static HRESULT AddOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode );
	static HRESULT AddOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode );
	static HRESULT AddBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd, IMFVideoPresenter *pVideoPresenter );
	
private:
	//! Makes sure Media Foundation is initialized. 
	msw::ScopedMFInitializer   mInitializer;

	State   mState;

	ULONG   mRefCount;
	HWND    mWnd;

	UINT32  mWidth, mHeight;

	HANDLE  mCloseEvent;

	IMFMediaSession    *mSessionPtr;
	IMFMediaSource     *mSourcePtr;
	IMFVideoPresenter  *mPresenterPtr;
};

}
}