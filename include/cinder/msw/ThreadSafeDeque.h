#pragma once

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/LinkList.h"

namespace cinder {
namespace msw {

//! 
template<typename T, typename = typename std::enable_if< std::is_base_of< IUnknown, T >::value >::type >
class ThreadSafeDeque : public ComObject {
public:
	ThreadSafeDeque()
	{
	}

	virtual ~ThreadSafeDeque()
	{
		// List will clear itself.
	}

	//! Remove and release all items in the queue.
	STDMETHODIMP Clear()
	{
		ScopedCriticalSection lock( m_critSec );

		m_queue.Clear();

		return S_OK;
	}

	//! Returns the number of items in the queue.
	STDMETHODIMP_( DWORD ) GetCount()
	{
		return m_queue.GetCount();
	}

	//! Returns \c TRUE if the queue is empty.
	STDMETHODIMP_( BOOL ) IsEmpty()
	{
		return m_queue.IsEmpty();
	}

	//! Atomically swaps the item that is currently at the back (if any) with the item pointed to by \a ppItem.
	STDMETHODIMP SwapBack( T** ppItem )
	{
		HRESULT hr = S_OK;

		ScopedCriticalSection lock( m_critSec );

		do {
			if( NULL == ppItem )
				return E_POINTER;

			ScopedComPtr<T> pTemp;
			hr = m_queue.RemoveBack( &pTemp );

			if( NULL != *ppItem ) {
				m_queue.InsertBack( *ppItem );
			}

			SafeRelease( *ppItem );

			if( SUCCEEDED( hr ) ) {
				( *ppItem ) = pTemp;
				( *ppItem )->AddRef();
			}
		} while( FALSE );

		return hr;
	}

	//! Removes the item that is currently at the back (if any) and stores it in \a ppItem. Caller is responsible for releasing the item.
	STDMETHODIMP RemoveBack( T** ppItem )
	{
		HRESULT hr = S_OK;

		ScopedCriticalSection lock( m_critSec );

		do {
			if( ppItem == NULL )
				return E_POINTER;

			hr = m_queue.RemoveBack( ppItem );
			BREAK_ON_FAIL( hr );
		} while( FALSE );

		return hr;
	}

	//! Inserts \a pItem at the back of the queue. Calls AddRef internally.
	STDMETHODIMP InsertBack( T* pItem )
	{
		HRESULT hr = S_OK;

		ScopedCriticalSection lock( m_critSec );

		do {
			if( pItem == NULL )
				return E_POINTER;

			hr = m_queue.InsertBack( pItem );
			BREAK_ON_FAIL( hr );

		} while( FALSE );

		return hr;
	}

	//! Atomically swaps the item that is currently at the front (if any) with the item pointed to by \a ppItem.
	STDMETHODIMP SwapFront( T** ppItem )
	{
		HRESULT hr = S_OK;

		ScopedCriticalSection lock( m_critSec );

		do {
			BREAK_ON_NULL_MSG( ppItem, E_POINTER, "Invalid pointer." );

			ScopedComPtr<T> pTemp;
			hr = m_queue.RemoveFront( &pTemp );

			if( NULL != *ppItem ) {
				if( FAILED( m_queue.InsertFront( *ppItem ) ) )
					CI_LOG_E( "Failed to insert item in front of list." );
			}

			SafeRelease( *ppItem );

			if( SUCCEEDED( hr ) ) {
				( *ppItem ) = pTemp;
				( *ppItem )->AddRef();
			}
		} while( FALSE );

		return hr;
	}

	//! Removes the item that is currently at the front (if any) and stores it in \a ppItem. Caller is responsible for releasing the item.
	STDMETHODIMP RemoveFront( T** ppItem )
	{
		HRESULT hr = S_OK;

		ScopedCriticalSection lock( m_critSec );

		do {
			if( ppItem == NULL )
				return E_POINTER;

			hr = m_queue.RemoveFront( ppItem );
			BREAK_ON_FAIL( hr );
		} while( FALSE );

		return hr;
	}

	//! Inserts \a pItem at the front of the queue. Calls AddRef internally.
	STDMETHODIMP InsertFront( T* pItem )
	{
		HRESULT hr = S_OK;

		ScopedCriticalSection lock( m_critSec );

		do {
			if( pItem == NULL )
				return E_POINTER;

			hr = m_queue.InsertFront( pItem );
			BREAK_ON_FAIL( hr );

		} while( FALSE );

		return hr;
	}

private:
	CriticalSection  m_critSec;  // critical section for thread safety
	ComPtrListEx<T>  m_queue;
};

} // namespace msw
} // namespace cinder
