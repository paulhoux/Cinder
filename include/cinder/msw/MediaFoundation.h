

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Exception.h"

// Media Foundation headers
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <evr.h>

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

typedef std::shared_ptr<class MFPlayer> MFPlayerRef;

class MFPlayer : public IMFAsyncCallback {
public:
	MFPlayer();
	~MFPlayer();

	static std::shared_ptr<MFPlayer> create() { return std::make_shared<MFPlayer>(); }

	// IUnknown methods
	STDMETHODIMP QueryInterface( REFIID iid, void** ppv );
	STDMETHODIMP_( ULONG ) AddRef();
	STDMETHODIMP_( ULONG ) Release();

	// IMFAsyncCallback methods
	STDMETHODIMP GetParameters( DWORD*, DWORD* ) { return E_NOTIMPL; }
	//!  Callback for the asynchronous BeginGetEvent method.
	STDMETHODIMP Invoke( IMFAsyncResult* pAsyncResult );

	//
	LRESULT HandleEvent( WPARAM wParam );

protected:
	//! Creates a (hidden) window used by Media Foundation.
	void CreateWnd();
	//! Destroys the (hidden) window.
	void DestroyWnd();

	void Shutdown();

	static void RegisterWindowClass();

private:
	//! Makes sure Media Foundation is initialized. 
	msw::ScopedMFInitializer   mInitializer;

	ULONG  mRefCount;
	HWND   mWnd;
};

}
}