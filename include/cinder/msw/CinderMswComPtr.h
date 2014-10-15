/*
Copyright (c) 2014, The Barbarian Group
All rights reserved.

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

#include <assert.h>
#include <windows.h>
#undef min
#undef max

namespace cinder {
namespace msw {

IUnknown* __stdcall ComPtrAssign( IUnknown** pp, IUnknown* lp );
IUnknown* __stdcall ComQIPtrAssign( IUnknown** pp, IUnknown* lp, REFIID riid );

inline IUnknown* __stdcall ComPtrAssign( IUnknown** pp, IUnknown* lp )
{
	if( lp != NULL )
		lp->AddRef();
	if( *pp )
		( *pp )->Release();
	*pp = lp;
	return lp;
}

inline IUnknown* __stdcall ComQIPtrAssign( IUnknown** pp, IUnknown* lp, REFIID riid )
{
	IUnknown* pTemp = *pp;
	*pp = NULL;
	if( lp != NULL )
		lp->QueryInterface( riid, (void**) pp );
	if( pTemp )
		pTemp->Release();
	return *pp;
}

template <class T>
class _NoAddRefReleaseOnComPtr : public T {
private:
	STDMETHOD_( ULONG, AddRef )( ) = 0;
	STDMETHOD_( ULONG, Release )( ) = 0;
};

//ComPtrBase provides the basis for all other smart pointers
//The other smartpointers add their own constructors and operators
template <class T> class ComPtrBase {
protected:
	ComPtrBase() throw( )
	{
		p = NULL;
	}
	ComPtrBase( int nNull ) throw( )
	{
		assert( nNull == 0 );
		(void) nNull;
		p = NULL;
	}
	ComPtrBase( T* lp ) throw( )
	{
		p = lp;
		if( p != NULL )
			p->AddRef();
	}
public:
	typedef T _PtrClass;
	~ComPtrBase() throw( )
	{
		if( p )
			p->Release();
	}
	operator T*( ) const throw( )
	{
		return p;
	}
	T& operator*( ) const throw( )
	{
		assert( p != NULL );
		return *p;
	}
	//The assert on operator& usually indicates a bug.  If this is really
	//what is needed, however, take the address of the p member explicitly.
	T** operator&( ) throw( )
	{
		assert( p == NULL );
		return &p;
	}
	_NoAddRefReleaseOnComPtr<T>* operator->( ) const throw( )
	{
		assert( p != NULL );
		return ( _NoAddRefReleaseOnComPtr<T>* )p;
	}
	bool operator!( ) const throw( )
	{
		return ( p == NULL );
	}
	bool operator<( T* pT ) const throw( )
	{
		return p < pT;
	}
	bool operator==( T* pT ) const throw( )
	{
		return p == pT;
	}

	// Release the interface and set to NULL
	void Release() throw( )
	{
		T* pTemp = p;
		if( pTemp ) {
			p = NULL;
			pTemp->Release();
		}
	}
	// Compare two objects for equivalence
	bool IsEqualObject( IUnknown* pOther ) throw( )
	{
		if( p == pOther )
			return true;

		if( p == NULL || pOther == NULL )
			return false; // One is NULL the other is not

		ComPtr<IUnknown> punk1;
		ComPtr<IUnknown> punk2;
		p->QueryInterface( __uuidof( IUnknown ), (void**) &punk1 );
		pOther->QueryInterface( __uuidof( IUnknown ), (void**) &punk2 );
		return punk1 == punk2;
	}
	// Attach to an existing interface (does not AddRef)
	void Attach( T* p2 ) throw( )
	{
		if( p )
			p->Release();
		p = p2;
	}
	// Detach the interface (does not Release)
	T* Detach() throw( )
	{
		T* pt = p;
		p = NULL;
		return pt;
	}
	HRESULT CopyTo( T** ppT ) throw( )
	{
		assert( ppT != NULL );
		if( ppT == NULL )
			return E_POINTER;
		*ppT = p;
		if( p )
			p->AddRef();
		return S_OK;
	}

	HRESULT CoCreateInstance( REFCLSID rclsid, LPUNKNOWN pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL ) throw( )
	{
		assert( p == NULL );
		return ::CoCreateInstance( rclsid, pUnkOuter, dwClsContext, __uuidof( T ), (void**) &p );
	}
	HRESULT CoCreateInstance( LPCOLESTR szProgID, LPUNKNOWN pUnkOuter = NULL, DWORD dwClsContext = CLSCTX_ALL ) throw( )
	{
		CLSID clsid;
		HRESULT hr = CLSIDFromProgID( szProgID, &clsid );
		assert( p == NULL );
		if( SUCCEEDED( hr ) )
			hr = ::CoCreateInstance( clsid, pUnkOuter, dwClsContext, __uuidof( T ), (void**) &p );
		return hr;
	}
	template <class Q>
	HRESULT QueryInterface( Q** pp ) const throw( )
	{
		assert( pp != NULL );
		return p->QueryInterface( __uuidof( Q ), (void**) pp );
	}
	T* p;
};

template <class T>
class ComPtr : public ComPtrBase < T > {
public:
	ComPtr() throw( )
	{
	}
	ComPtr( int nNull ) throw( ) :
		ComPtrBase<T>( nNull )
	{
	}
	ComPtr( T* lp ) throw( ) :
		ComPtrBase<T>( lp )

	{
	}
	ComPtr( const ComPtr<T>& lp ) throw( ) :
		ComPtrBase<T>( lp.p )
	{
	}
	T* operator=( T* lp ) throw( )
	{
		return static_cast<T*>( ComPtrAssign( (IUnknown**) &p, lp ) );
	}
	template <typename Q>
	T* operator=( const ComPtr<Q>& lp ) throw( )
	{
		return static_cast<T*>( ComQIPtrAssign( (IUnknown**) &p, lp, __uuidof( T ) ) );
	}

	T* operator=( const ComPtr<T>& lp ) throw( )
	{
		return static_cast<T*>( ComPtrAssign( (IUnknown**) &p, lp ) );
	}
};

//specialization for IDispatch
template <>
class ComPtr<IDispatch> : public ComPtrBase < IDispatch >
{
public:
	ComPtr() throw( )
	{
	}
	ComPtr( IDispatch* lp ) throw( ) :
		ComPtrBase<IDispatch>( lp )
	{
	}
	ComPtr( const ComPtr<IDispatch>& lp ) throw( ) :
		ComPtrBase<IDispatch>( lp.p )
	{
	}
	IDispatch* operator=( IDispatch* lp ) throw( )
	{
		return static_cast<IDispatch*>( ComPtrAssign( (IUnknown**) &p, lp ) );
	}
	IDispatch* operator=( const ComPtr<IDispatch>& lp ) throw( )
	{
		return static_cast<IDispatch*>( ComPtrAssign( (IUnknown**) &p, lp.p ) );
	}

	// IDispatch specific stuff
	HRESULT GetPropertyByName( LPCOLESTR lpsz, VARIANT* pVar ) throw( )
	{
		assert( p );
		assert( pVar );
		DISPID dwDispID;
		HRESULT hr = GetIDOfName( lpsz, &dwDispID );
		if( SUCCEEDED( hr ) )
			hr = GetProperty( dwDispID, pVar );
		return hr;
	}
	HRESULT GetProperty( DISPID dwDispID, VARIANT* pVar ) throw( )
	{
		return GetProperty( p, dwDispID, pVar );
	}
	HRESULT PutPropertyByName( LPCOLESTR lpsz, VARIANT* pVar ) throw( )
	{
		assert( p );
		assert( pVar );
		DISPID dwDispID;
		HRESULT hr = GetIDOfName( lpsz, &dwDispID );
		if( SUCCEEDED( hr ) )
			hr = PutProperty( dwDispID, pVar );
		return hr;
	}
	HRESULT PutProperty( DISPID dwDispID, VARIANT* pVar ) throw( )
	{
		return PutProperty( p, dwDispID, pVar );
	}
	HRESULT GetIDOfName( LPCOLESTR lpsz, DISPID* pdispid ) throw( )
	{
		return p->GetIDsOfNames( IID_NULL, const_cast<LPOLESTR*>( &lpsz ), 1, LOCALE_USER_DEFAULT, pdispid );
	}
	// Invoke a method by DISPID with no parameters
	HRESULT Invoke0( DISPID dispid, VARIANT* pvarRet = NULL ) throw( )
	{
		DISPPARAMS dispparams = { NULL, NULL, 0, 0 };
		return p->Invoke( dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dispparams, pvarRet, NULL, NULL );
	}
	// Invoke a method by name with no parameters
	HRESULT Invoke0( LPCOLESTR lpszName, VARIANT* pvarRet = NULL ) throw( )
	{
		HRESULT hr;
		DISPID dispid;
		hr = GetIDOfName( lpszName, &dispid );
		if( SUCCEEDED( hr ) )
			hr = Invoke0( dispid, pvarRet );
		return hr;
	}
	// Invoke a method by DISPID with a single parameter
	HRESULT Invoke1( DISPID dispid, VARIANT* pvarParam1, VARIANT* pvarRet = NULL ) throw( )
	{
		DISPPARAMS dispparams = { pvarParam1, NULL, 1, 0 };
		return p->Invoke( dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dispparams, pvarRet, NULL, NULL );
	}
	// Invoke a method by name with a single parameter
	HRESULT Invoke1( LPCOLESTR lpszName, VARIANT* pvarParam1, VARIANT* pvarRet = NULL ) throw( )
	{
		HRESULT hr;
		DISPID dispid;
		hr = GetIDOfName( lpszName, &dispid );
		if( SUCCEEDED( hr ) )
			hr = Invoke1( dispid, pvarParam1, pvarRet );
		return hr;
	}
	// Invoke a method by DISPID with two parameters
	HRESULT Invoke2( DISPID dispid, VARIANT* pvarParam1, VARIANT* pvarParam2, VARIANT* pvarRet = NULL ) throw( );
	// Invoke a method by name with two parameters
	HRESULT Invoke2( LPCOLESTR lpszName, VARIANT* pvarParam1, VARIANT* pvarParam2, VARIANT* pvarRet = NULL ) throw( )
	{
		HRESULT hr;
		DISPID dispid;
		hr = GetIDOfName( lpszName, &dispid );
		if( SUCCEEDED( hr ) )
			hr = Invoke2( dispid, pvarParam1, pvarParam2, pvarRet );
		return hr;
	}
	// Invoke a method by DISPID with N parameters
	HRESULT InvokeN( DISPID dispid, VARIANT* pvarParams, int nParams, VARIANT* pvarRet = NULL ) throw( )
	{
		DISPPARAMS dispparams = { pvarParams, NULL, nParams, 0 };
		return p->Invoke( dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &dispparams, pvarRet, NULL, NULL );
	}
	// Invoke a method by name with Nparameters
	HRESULT InvokeN( LPCOLESTR lpszName, VARIANT* pvarParams, int nParams, VARIANT* pvarRet = NULL ) throw( )
	{
		HRESULT hr;
		DISPID dispid;
		hr = GetIDOfName( lpszName, &dispid );
		if( SUCCEEDED( hr ) )
			hr = InvokeN( dispid, pvarParams, nParams, pvarRet );
		return hr;
	}
	static HRESULT PutProperty( IDispatch* p, DISPID dwDispID, VARIANT* pVar ) throw( )
	{
		assert( p );
		DISPPARAMS dispparams = { NULL, NULL, 1, 1 };
		dispparams.rgvarg = pVar;
		DISPID dispidPut = DISPID_PROPERTYPUT;
		dispparams.rgdispidNamedArgs = &dispidPut;

		if( pVar->vt == VT_UNKNOWN || pVar->vt == VT_DISPATCH ||
			( pVar->vt & VT_ARRAY ) || ( pVar->vt & VT_BYREF ) ) {
			HRESULT hr = p->Invoke( dwDispID, IID_NULL,
									LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUTREF,
									&dispparams, NULL, NULL, NULL );
			if( SUCCEEDED( hr ) )
				return hr;
		}
		return p->Invoke( dwDispID, IID_NULL,
						  LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT,
						  &dispparams, NULL, NULL, NULL );
	}
	static HRESULT GetProperty( IDispatch* p, DISPID dwDispID, VARIANT* pVar ) throw( )
	{
		assert( p );
		DISPPARAMS dispparamsNoArgs = { NULL, NULL, 0, 0 };
		return p->Invoke( dwDispID, IID_NULL,
						  LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET,
						  &dispparamsNoArgs, pVar, NULL, NULL );
	}
};

template <class T, const IID* piid = &__uuidof( T )>
class CComQIPtr : public ComPtr < T > {
public:
	CComQIPtr() throw( )
	{
	}
	CComQIPtr( T* lp ) throw( ) :
		ComPtr<T>( lp )
	{
	}
	CComQIPtr( const CComQIPtr<T, piid>& lp ) throw( ) :
		ComPtr<T>( lp.p )
	{
	}
	CComQIPtr( IUnknown* lp ) throw( )
	{
		if( lp != NULL )
			lp->QueryInterface( *piid, (void **) &p );
	}
	T* operator=( T* lp ) throw( )
	{
		return static_cast<T*>( ComPtrAssign( (IUnknown**) &p, lp ) );
	}
	T* operator=( const CComQIPtr<T, piid>& lp ) throw( )
	{
		return static_cast<T*>( ComPtrAssign( (IUnknown**) &p, lp.p ) );
	}
	T* operator=( IUnknown* lp ) throw( )
	{
		return static_cast<T*>( ComQIPtrAssign( (IUnknown**) &p, lp, *piid ) );
	}
};

//Specialization to make it work
template<>
class CComQIPtr<IUnknown, &IID_IUnknown> : public ComPtr < IUnknown >
{
public:
	CComQIPtr() throw( )
	{
	}
	CComQIPtr( IUnknown* lp ) throw( )
	{
		//Actually do a QI to get identity
		if( lp != NULL )
			lp->QueryInterface( __uuidof( IUnknown ), (void **) &p );
	}
	CComQIPtr( const CComQIPtr<IUnknown, &IID_IUnknown>& lp ) throw( ) :
		ComPtr<IUnknown>( lp.p )
	{
	}
	IUnknown* operator=( IUnknown* lp ) throw( )
	{
		//Actually do a QI to get identity
		return ComQIPtrAssign( (IUnknown**) &p, lp, __uuidof( IUnknown ) );
	}
	IUnknown* operator=( const CComQIPtr<IUnknown, &IID_IUnknown>& lp ) throw( )
	{
		return ComPtrAssign( (IUnknown**) &p, lp.p );
	}
};

typedef CComQIPtr<IDispatch, &__uuidof( IDispatch )> CComDispatchDriver;

} // namespace msw
} // namespace cinder