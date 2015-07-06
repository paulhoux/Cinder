/*
 Copyright (c) 2010, The Barbarian Group
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

#include "cinder/Cinder.h"
#include "cinder/Vector.h"
#include "cinder/Surface.h"
#include "cinder/Stream.h"

#include "cinder/Log.h"

#include <string>
#include <windows.h>
#include <Objidl.h>
#undef min
#undef max

#define LOWORD(_dw)     ((WORD)(((DWORD_PTR)(_dw)) & 0xffff))
#define HIWORD(_dw)     ((WORD)((((DWORD_PTR)(_dw)) >> 16) & 0xffff))
#define LODWORD(_qw)    ((DWORD)(_qw))
#define HIDWORD(_qw)    ((DWORD)(((_qw) >> 32) & 0xffffffff))

//#define BREAK_ON_FAIL(value)          if( FAILED( value ) ) break;
#define BREAK_ON_FAIL(value) if( FAILED(value) ) { CI_LOG_E("Fail:" << value); break; }
#define BREAK_ON_NULL(value, result)  if( value == NULL ) { hr = result; break; }
#define BREAK_IF_FALSE(test, result)  if( !(test) ) { hr = result; break; }

#undef COMPILE_ASSERT
#define COMPILE_ASSERT(expr, msg)     static_assert(expr, #msg)

namespace cinder {
namespace msw {

/** Converts a Win32 HBITMAP to a cinder::Surface8u
	\note Currently always copies the alpha channel **/
Surface8uRef convertHBitmap( HBITMAP hbitmap );

//! Converts a UTF-8 string into a wide string (wstring). Note that wstring is not a good cross-platform choice and this is here for interop with Windows APIs.
std::wstring toWideString( const std::string &utf8String );
//! Converts a wide string to a UTF-8 string. Note that wstring is not a good cross-platform choice and this is here for interop with Windows APIs.
std::string toUtf8String( const std::wstring &wideString );

//! Converts a Win32 POINTFX fixed point point to a cinder::vec2
#if !defined ( CINDER_WINRT )
inline vec2 toVec2( const ::POINTFX &p )
{ return vec2( ( ( p.x.value << 16 ) | p.x.fract ) / 65535.0f, ( ( p.y.value << 16 ) | p.y.fract ) / 65535.0f ); }
#endif

//! Releases a COM pointer if the pointer is not NULL, and sets the pointer to NULL.
template <class T> inline void SafeRelease( T*& p )
{
	if( p ) {
		p->Release();
		p = NULL;
	}
}

//! Deletes a pointer allocated with new.
template <class T> inline void SafeDelete( T*& p )
{
	if( p ) {
		delete p;
		p = NULL;
	}
}

//! A free function designed to interact with makeComShared, calls Release() on a com-managed object
void ComDelete( void *p );

//! Functor version that calls Release() on a com-managed object
struct ComDeleter {
	template <typename T>
	void operator()( T *p ) { if( p ) p->Release(); }
};

template<typename T>
using ManagedComRef = std::shared_ptr<T>;

//! Creates a shared_ptr whose deleter will properly decrement the reference count of a COM object
template<typename T>
ManagedComRef<T> makeComShared( T *p ) { return ManagedComRef<T>( p, &ComDelete ); }

template<typename T>
using ManagedComPtr = std::unique_ptr<T, ComDeleter>;

//! Creates a unique_ptr whose deleter will properly decrement the reference count of a COM object
template<typename T>
ManagedComPtr<T> makeComUnique( T *p ) { return ManagedComPtr<T>( p ); }

//! Wraps a cinder::OStream with a COM ::IStream
class ComOStream : public ::IStream {
public:
	ComOStream( cinder::OStreamRef aOStream ) : mOStream( aOStream ), _refcount( 1 ) {}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void ** ppvObject );
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	// ISequentialStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE Read( void* pv, ULONG cb, ULONG* pcbRead ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Write( void const* pv, ULONG cb, ULONG* pcbWritten );
	// IStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE SetSize( ULARGE_INTEGER ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE CopyTo( ::IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER* ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Commit( DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Revert() { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE LockRegion( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE UnlockRegion( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Clone( IStream ** ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Seek( LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, ULARGE_INTEGER* lpNewFilePointer );
	virtual HRESULT STDMETHODCALLTYPE Stat( STATSTG* pStatstg, DWORD grfStatFlag ) { return E_NOTIMPL; }

private:
	cinder::OStreamRef	mOStream;
	LONG			_refcount;
};

//! Wraps a cinder::IStream with a COM ::IStream
class ComIStream : public ::IStream {
public:
	ComIStream( cinder::IStreamRef aIStream ) : mIStream( aIStream ), _refcount( 1 ) {}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface( REFIID iid, void ** ppvObject );
	virtual ULONG STDMETHODCALLTYPE AddRef();
	virtual ULONG STDMETHODCALLTYPE Release();

	// ISequentialStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE Read( void* pv, ULONG cb, ULONG* pcbRead );
	virtual HRESULT STDMETHODCALLTYPE Write( void const* pv, ULONG cb, ULONG* pcbWritten ) { return E_NOTIMPL; }
	// IStream Interface
public:
	virtual HRESULT STDMETHODCALLTYPE SetSize( ULARGE_INTEGER ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE CopyTo( ::IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER* ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Commit( DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Revert() { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE LockRegion( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE UnlockRegion( ULARGE_INTEGER, ULARGE_INTEGER, DWORD ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Clone( IStream ** ) { return E_NOTIMPL; }
	virtual HRESULT STDMETHODCALLTYPE Seek( LARGE_INTEGER liDistanceToMove, DWORD dwOrigin, ULARGE_INTEGER* lpNewFilePointer );
	virtual HRESULT STDMETHODCALLTYPE Stat( STATSTG* pStatstg, DWORD grfStatFlag );

private:
	cinder::IStreamRef	mIStream;
	LONG			_refcount;
};

//! Initializes COM on this thread. Uses thread local storage to prevent multiple initializations per thread
void initializeCom( DWORD params = COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE );

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
//     foo = nullptr;  // explicitly releases |foo|
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
//     // now, |b| references the MyFoo object, and |a| references nullptr.
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

	ScopedPtr() : ptr_( nullptr )
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
	T** operator&() throw( )
	{
		assert( ptr_ == nullptr );
		return &ptr_;
	}

	T& operator*() const
	{
		assert( ptr_ != nullptr );
		return *ptr_;
	}

	T* operator->() const
	{
		assert( ptr_ != nullptr );
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
class ScopedComPtr : public ScopedPtr < Interface > {
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
		if( ptr_ != nullptr ) {
			ptr_->Release();
			ptr_ = nullptr;
		}
	}

	// Sets the internal pointer to nullptr and returns the held object without
	// releasing the reference.
	Interface* Detach()
	{
		Interface* p = ptr_;
		ptr_ = nullptr;
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
	// The function asserts on the current value being nullptr.
	// Usage: Foo(p.Receive());
	Interface** Receive()
	{
		assert( !ptr_ ) << "Object leak. Pointer must be nullptr.";
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
		assert( p != nullptr );
		assert( ptr_ != nullptr );
		// IUnknown already has a template version of QueryInterface
		// so the iid parameter is implicit here. The only thing this
		// function adds are the asserts.
		return ptr_->QueryInterface( p );
	}

	// QI for times when the IID is not associated with the type.
	HRESULT QueryInterface( const IID& iid, void** obj )
	{
		assert( obj != nullptr );
		assert( ptr_ != nullptr );
		return ptr_->QueryInterface( iid, obj );
	}

	// Queries |other| for the interface this object wraps and returns the
	// error code from the other->QueryInterface operation.
	HRESULT QueryFrom( IUnknown* object )
	{
		assert( object != nullptr );
		return object->QueryInterface( Receive() );
	}

	// Convenience wrapper around CoCreateInstance
	HRESULT CreateInstance( const CLSID& clsid, IUnknown* outer = nullptr, DWORD context = CLSCTX_ALL )
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

	//! Returns the current reference count. Use for debugging purposes only.
	ULONG GetRefCount()
	{
		if( !ptr_ )
			return 0;

		ULONG rc = ptr_->AddRef();
		ptr_->Release();

		return ( rc - 1 );
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
	BlockIUnknownMethods* operator->() const
	{
		assert( ptr_ != nullptr );
		return reinterpret_cast<BlockIUnknownMethods*>( ptr_ );
	}

	// Pull in operator=() from the parent class.
	using ScopedPtr<Interface>::operator=;

	// Pull in operator&() from the parent class.
	using ScopedPtr<Interface>::operator&;

	static const IID& iid() { return *interface_id; }
};

template <class T>
void CopyComPtr( T*& dest, T* src )
{
	if( dest ) {
		dest->Release();
	}
	dest = src;
	if( dest ) {
		dest->AddRef();
	}
}

template <class T1, class T2>
bool AreComObjectsEqual( T1 *p1, T2 *p2 )
{
	if( p1 == NULL && p2 == NULL ) {
		// Both are NULL
		return true;
	}
	else if( p1 == NULL || p2 == NULL ) {
		// One is NULL and one is not
		return false;
	}
	else {
		// Both are not NULL. Compare IUnknowns.
		ScopedComPtr<IUnknown> pUnk1, pUnk2;
		if( SUCCEEDED( p1->QueryInterface( IID_IUnknown, (void**) &pUnk1 ) ) ) {
			if( SUCCEEDED( p2->QueryInterface( IID_IUnknown, (void**) &pUnk2 ) ) ) {
				return ( pUnk1 == pUnk2 );
			}
		}
	}

	return false;
}

//! Warps a critical section.
class CriticalSection {
private:
	CRITICAL_SECTION mCriticalSection;
public:
	CriticalSection()
	{
		::InitializeCriticalSection( &mCriticalSection );
	}

	~CriticalSection()
	{
		assert( mCriticalSection.LockCount == -1 );
		::DeleteCriticalSection( &mCriticalSection );
	}

	void Lock()
	{
		::EnterCriticalSection( &mCriticalSection );
	}

	void Unlock()
	{
		assert( mCriticalSection.LockCount < -1 );
		::LeaveCriticalSection( &mCriticalSection );
	}
};

//! Provides automatic locking and unlocking of a critical section.
class ScopedCriticalSection {
private:
	CriticalSection *mCriticalSectionPtr;
public:
	ScopedCriticalSection( CriticalSection& section )
	{
		mCriticalSectionPtr = &section;
		mCriticalSectionPtr->Lock();
	}
	~ScopedCriticalSection()
	{
		assert( mCriticalSectionPtr != nullptr );
		mCriticalSectionPtr->Unlock();
	}
};

}
} // namespace cinder::msw
