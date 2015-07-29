#pragma once

#include <windows.h>
#include <float.h> // for FLT_MAX
#include <tchar.h>
#include <math.h>
#include <strsafe.h>
#include <mmsystem.h>
#include <mfapi.h>
#include <mfidl.h>
#include <evr.h>
#include <mferror.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <wmcodecdsp.h> // for MEDIASUBTYPE_V216

//// Include these libraries.
//#pragma comment(lib, "mf.lib")
//#pragma comment(lib, "mfplat.lib")
//#pragma comment(lib, "mfuuid.lib")
//#pragma comment(lib, "d3d11.lib")
//
//#pragma comment(lib, "winmm.lib") // for timeBeginPeriod and timeEndPeriod
////#pragma comment(lib,"d3d9.lib")
////#pragma comment(lib,"dxva2.lib")
////#pragma comment (lib,"evr.lib")
////#pragma comment (lib,"dcomp.lib")
//#pragma comment (lib,"uuid.lib")

namespace cinder {
namespace msw {

class CBase {
public:

	static long GetObjectCount( void )
	{
		return s_lObjectCount;
	}

protected:

	CBase( void )
	{
		InterlockedIncrement( &s_lObjectCount );
	}

	~CBase( void )
	{
		InterlockedDecrement( &s_lObjectCount );
	}

private:

	static volatile long s_lObjectCount;
};

class CCritSec {
public:

	CCritSec( void ) :
		m_cs()
	{
		InitializeCriticalSection( &m_cs );
	}

	~CCritSec( void )
	{
		DeleteCriticalSection( &m_cs );
	}

	_Acquires_lock_( this->m_cs )
		void Lock( void )
	{
		EnterCriticalSection( &m_cs );
	}

	_Releases_lock_( this->m_cs )
		void Unlock( void )
	{
		LeaveCriticalSection( &m_cs );
	}

private:

	CRITICAL_SECTION m_cs;
};

class CAutoLock {
public:
	_Acquires_lock_( this->m_pLock->m_cs )
		CAutoLock( CCritSec* pLock ) :
		m_pLock( pLock )
	{
		m_pLock->Lock();
	}

	_Releases_lock_( this->m_pLock->m_cs )
		~CAutoLock( void )
	{
		m_pLock->Unlock();
	}

private:

	CCritSec* m_pLock;
};

//////////////////////////////////////////////////////////////////////////
//  CAsyncCallback [template]
//
//  Description:
//  Helper class that routes IMFAsyncCallback::Invoke calls to a class
//  method on the parent class.
//
//  Usage:
//  Add this class as a member variable. In the parent class constructor,
//  initialize the CAsyncCallback class like this:
//      m_cb(this, &CYourClass::OnInvoke)
//  where
//      m_cb       = CAsyncCallback object
//      CYourClass = parent class
//      OnInvoke   = Method in the parent class to receive Invoke calls.
//
//  The parent's OnInvoke method (you can name it anything you like) must
//  have a signature that matches the InvokeFn typedef below.
//////////////////////////////////////////////////////////////////////////

// T: Type of the parent object
template<class T>
class CAsyncCallback : public IMFAsyncCallback {
public:

	typedef HRESULT( T::*InvokeFn )( IMFAsyncResult* pAsyncResult );

	CAsyncCallback( T* pParent, InvokeFn fn ) :
		m_pParent( pParent ),
		m_pInvokeFn( fn )
	{
	}

	// IUnknown
	STDMETHODIMP_( ULONG ) AddRef( void )
	{
		// Delegate to parent class.
		return m_pParent->AddRef();
	}

	STDMETHODIMP_( ULONG ) Release( void )
	{
		// Delegate to parent class.
		return m_pParent->Release();
	}

	STDMETHODIMP QueryInterface( REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv )
	{
		if( !ppv ) {
			return E_POINTER;
		}
		if( iid == __uuidof( IUnknown ) ) {
			*ppv = static_cast<IUnknown*>( static_cast<IMFAsyncCallback*>( this ) );
		}
		else if( iid == __uuidof( IMFAsyncCallback ) ) {
			*ppv = static_cast<IMFAsyncCallback*>( this );
		}
		else {
			*ppv = NULL;
			return E_NOINTERFACE;
		}
		AddRef();
		return S_OK;
	}

	// IMFAsyncCallback methods
	STDMETHODIMP GetParameters( __RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue )
	{
		// Implementation of this method is optional.
		return E_NOTIMPL;
	}

	STDMETHODIMP Invoke( __RPC__in_opt IMFAsyncResult* pAsyncResult )
	{
		return ( m_pParent->*m_pInvokeFn )( pAsyncResult );
	}

private:

	T* m_pParent;
	InvokeFn m_pInvokeFn;
};


}
}
