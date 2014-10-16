/*
Copyright (c) 2014, The Barbarian Group
Copyright (c) 2014, The Chromium Authors
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.
* Neither the name of Google Inc. nor the names of its contributors may be used to endorse or promote
products derived from this software without specific prior written permission.

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

// The COMPILE_ASSERT macro can be used to verify that a compile time
// expression is true. For example, you could use it to verify the
// size of a static array:
//
//   COMPILE_ASSERT(arraysize(content_type_names) == CONTENT_NUM_TYPES,
//                  content_type_names_incorrect_size);
//
// or to make sure a struct is smaller than a certain size:
//
//   COMPILE_ASSERT(sizeof(foo) < 128, foo_too_large);
//
// The second argument to the macro is the name of the variable. If
// the expression is false, most compilers will issue a warning/error
// containing the name of the variable.

#undef COMPILE_ASSERT
#define COMPILE_ASSERT(expr, msg) static_assert(expr, #msg)

namespace cinder {
namespace msw {

// A smart pointer class for reference counted objects.  Use this class instead
// of calling AddRef and Release manually on a reference counted object to
// avoid common memory leaks caused by forgetting to Release an object
// reference.  Sample usage:
//
//   class MyFoo : public RefCounted<MyFoo> {
//    ...
//   };
//
//   void some_function() {
//     ScopedPtr<MyFoo> foo = new MyFoo();
//     foo->Method(param);
//     // |foo| is released when this function returns
//   }
//
//   void some_other_function() {
//     ScopedPtr<MyFoo> foo = new MyFoo();
//     ...
//     foo = NULL;  // explicitly releases |foo|
//     ...
//     if (foo)
//       foo->Method(param);
//   }
//
// The above examples show how ScopedPtr<T> acts like a pointer to T.
// Given two ScopedPtr<T> classes, it is also possible to exchange
// references between the two objects, like so:
//
//   {
//     ScopedPtr<MyFoo> a = new MyFoo();
//     ScopedPtr<MyFoo> b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references NULL.
//   }
//
// To make both |a| and |b| in the above example reference the same MyFoo
// object, simply use the assignment operator:
//
//   {
//     ScopedPtr<MyFoo> a = new MyFoo();
//     ScopedPtr<MyFoo> b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//   }
//
template <class T>
class ScopedPtr {
public:
	typedef T element_type;

	ScopedPtr() : ptr_( NULL )
	{
	}

	ScopedPtr( T* p ) : ptr_( p )
	{
		if( ptr_ )
			AddRef( ptr_ );
	}

	ScopedPtr( const ScopedPtr<T>& r ) : ptr_( r.ptr_ )
	{
		if( ptr_ )
			AddRef( ptr_ );
	}

	template <typename U>
	ScopedPtr( const ScopedPtr<U>& r ) : ptr_( r.get() )
	{
		if( ptr_ )
			AddRef( ptr_ );
	}

	~ScopedPtr()
	{
		if( ptr_ )
			Release( ptr_ );
	}

	T* get() const { return ptr_; }

	// Allow ScopedPtr<C> to be used in boolean expression
	// and comparison operations.
	operator T*( ) const { return ptr_; }
	
	//The assert on operator& usually indicates a bug.  If this is really
	//what is needed, however, take the address of the ptr_ member explicitly.
	T** operator&( ) throw( )
	{
		assert( ptr_ == NULL );
		return &ptr_;
	}

	T& operator*( ) const
	{
		assert( ptr_ != NULL );
		return *ptr_;
	}

	T* operator->( ) const
	{
		assert( ptr_ != NULL );
		return ptr_;
	}

	ScopedPtr<T>& operator=( T* p )
	{
		// AddRef first so that self assignment should work
		if( p )
			AddRef( p );
		T* old_ptr = ptr_;
		ptr_ = p;
		if( old_ptr )
			Release( old_ptr );
		return *this;
	}

	ScopedPtr<T>& operator=( const ScopedPtr<T>& r )
	{
		return *this = r.ptr_;
	}

	template <typename U>
	ScopedPtr<T>& operator=( const ScopedPtr<U>& r )
	{
		return *this = r.get();
	}

	void swap( T** pp )
	{
		T* p = ptr_;
		ptr_ = *pp;
		*pp = p;
	}

	void swap( ScopedPtr<T>& r )
	{
		swap( &r.ptr_ );
	}

protected:
	T* ptr_;

private:
	// Non-inline helpers to allow:
	//     class Opaque;
	//     extern template class ScopedPtr<Opaque>;
	// Otherwise the compiler will complain that Opaque is an incomplete type.
	static void AddRef( T* ptr );
	static void Release( T* ptr );
};

template <typename T>
void ScopedPtr<T>::AddRef( T* ptr )
{
	ptr->AddRef();
}

template <typename T>
void ScopedPtr<T>::Release( T* ptr )
{
	ptr->Release();
}

// Handy utility for creating a ScopedPtr<T> out of a T* explicitly without
// having to retype all the template arguments
template <typename T>
ScopedPtr<T> makeScopedPtr( T* t )
{
	return ScopedPtr<T>( t );
}

//////////////////////////////////////////////////////////////////////////////////////

// A fairly minimalistic smart class for COM interface pointers.
// Uses ScopedPtr for the basic smart pointer functionality
// and adds a few IUnknown specific services.
template <class Interface, const IID* interface_id = &__uuidof( Interface )>
class ScopedComPtr : public ScopedPtr<Interface> {
public:
	// Utility template to prevent users of ScopedComPtr from calling AddRef
	// and/or Release() without going through the ScopedComPtr class.
	class BlockIUnknownMethods : public Interface {
	private:
		STDMETHOD( QueryInterface )( REFIID iid, void** object ) = 0;
		STDMETHOD_( ULONG, AddRef )( ) = 0;
		STDMETHOD_( ULONG, Release )( ) = 0;
	};

	typedef ScopedPtr<Interface> ParentClass;

	ScopedComPtr()
	{
	}

	explicit ScopedComPtr( Interface* p ) : ParentClass( p )
	{
	}

	ScopedComPtr( const ScopedComPtr<Interface, interface_id>& p )
		: ParentClass( p )
	{
	}

	~ScopedComPtr()
	{
		// We don't want the smart pointer class to be bigger than the pointer
		// it wraps.
		COMPILE_ASSERT( sizeof( ScopedComPtr<Interface, interface_id> ) ==
						sizeof( Interface* ), ScopedComPtrSize );
	}

	// Explicit Release() of the held object.  Useful for reuse of the
	// ScopedComPtr instance.
	// Note that this function equates to IUnknown::Release and should not
	// be confused with e.g. scoped_ptr::release().
	void Release()
	{
		if( ptr_ != NULL ) {
			ptr_->Release();
			ptr_ = NULL;
		}
	}

	// Sets the internal pointer to NULL and returns the held object without
	// releasing the reference.
	Interface* Detach()
	{
		Interface* p = ptr_;
		ptr_ = NULL;
		return p;
	}

	// Accepts an interface pointer that has already been addref-ed.
	void Attach( Interface* p )
	{
		assert( !ptr_ );
		ptr_ = p;
	}

	// Retrieves the pointer address.
	// Used to receive object pointers as out arguments (and take ownership).
	// The function asserts on the current value being NULL.
	// Usage: Foo(p.Receive());
	Interface** Receive()
	{
		assert( !ptr_ ) << "Object leak. Pointer must be NULL";
		return &ptr_;
	}

	// A convenience for whenever a void pointer is needed as an out argument.
	void** ReceiveVoid()
	{
		return reinterpret_cast<void**>( Receive() );
	}

	template <class Query>
	HRESULT QueryInterface( Query** p )
	{
		assert( p != NULL );
		assert( ptr_ != NULL );
		// IUnknown already has a template version of QueryInterface
		// so the iid parameter is implicit here. The only thing this
		// function adds are the asserts.
		return ptr_->QueryInterface( p );
	}

	// QI for times when the IID is not associated with the type.
	HRESULT QueryInterface( const IID& iid, void** obj )
	{
		assert( obj != NULL );
		assert( ptr_ != NULL );
		return ptr_->QueryInterface( iid, obj );
	}

	// Queries |other| for the interface this object wraps and returns the
	// error code from the other->QueryInterface operation.
	HRESULT QueryFrom( IUnknown* object )
	{
		assert( object != NULL );
		return object->QueryInterface( Receive() );
	}

	// Convenience wrapper around CoCreateInstance
	HRESULT CreateInstance( const CLSID& clsid, IUnknown* outer = NULL,
							DWORD context = CLSCTX_ALL )
	{
		assert( !ptr_ );
		HRESULT hr = ::CoCreateInstance( clsid, outer, context, *interface_id,
										 reinterpret_cast<void**>( &ptr_ ) );
		return hr;
	}

	// Checks if the identity of |other| and this object is the same.
	bool IsSameObject( IUnknown* other )
	{
		if( !other && !ptr_ )
			return true;

		if( !other || !ptr_ )
			return false;

		ScopedComPtr<IUnknown> my_identity;
		QueryInterface( my_identity.Receive() );

		ScopedComPtr<IUnknown> other_identity;
		other->QueryInterface( other_identity.Receive() );

		return static_cast<IUnknown*>( my_identity ) ==
			static_cast<IUnknown*>( other_identity );
	}

	// Provides direct access to the interface.
	// Here we use a well known trick to make sure we block access to
	// IUnknown methods so that something bad like this doesn't happen:
	//    ScopedComPtr<IUnknown> p(Foo());
	//    p->Release();
	//    ... later the destructor runs, which will Release() again.
	// and to get the benefit of the asserts we add to QueryInterface.
	// There's still a way to call these methods if you absolutely must
	// by statically casting the ScopedComPtr instance to the wrapped interface
	// and then making the call... but generally that shouldn't be necessary.
	BlockIUnknownMethods* operator->( ) const
	{
		assert( ptr_ != NULL );
		return reinterpret_cast<BlockIUnknownMethods*>( ptr_ );
	}

	// Pull in operator=() from the parent class.
	using ScopedPtr<Interface>::operator=;

	// Pull in operator&() from the parent class.
	using ScopedPtr<Interface>::operator&;

	// static methods

	static const IID& iid()
	{
		return *interface_id;
	}
};


/*// ATL VERSION STARTS HERE

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

//*/ 

} // namespace msw
} // namespace cinder