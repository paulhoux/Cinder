/*
Copyright (c) 2015, The Barbarian Group
All rights reserved.

Copyright (c) Microsoft Open Technologies, Inc. All rights reserved.

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

//////////////////////////////////////////////////////////////////////////
//  MFAsyncCallback [template]
//
//  Description:
//  Helper class that routes IMFAsyncCallback::Invoke calls to a class
//  method on the parent class.
//
//  Usage:
//  Add this class as a member variable. In the parent class constructor,
//  initialize the MFAsyncCallback class like this:
//      m_cb(this, &CYourClass::OnInvoke)
//  where
//      m_cb       = MFAsyncCallback object
//      CYourClass = parent class
//      OnInvoke   = Method in the parent class to receive Invoke calls.
//
//  The parent's OnInvoke method (you can name it anything you like) must
//  have a signature that matches the InvokeFn typedef below.
//////////////////////////////////////////////////////////////////////////

#pragma once

#include <mfobjects.h>

namespace cinder {
namespace wmf {

// T: Type of the parent object
template <class T>
class MFAsyncCallback : public IMFAsyncCallback {
  public:
	typedef HRESULT ( T::*InvokeFn )( IMFAsyncResult* pAsyncResult );

	MFAsyncCallback( T* pParent, InvokeFn fn )
	    : m_pParent( pParent )
	    , m_pInvokeFn( fn )
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
	T*       m_pParent;
	InvokeFn m_pInvokeFn;
};
}
} // namespace cinder::wmf