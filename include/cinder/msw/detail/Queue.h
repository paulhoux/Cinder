#pragma once

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/detail/LinkList.h"

namespace cinder {
namespace msw {
namespace detail {

//! 
template<typename T, typename = typename std::enable_if< std::is_base_of< IUnknown, T >::value >::type >
class Queue : public ComObject {
public:
	Queue()
	{
	}

	virtual ~Queue()
	{
		// List will clear itself.
	}

	// Queue

	STDMETHODIMP Clear()
	{
		ScopedCriticalSection lock( m_critSec );

		m_queue.Clear();

		return S_OK;
	}

	STDMETHODIMP_( DWORD ) GetCount()
	{
		return m_queue.GetCount();
	}

	//! Caller is responsible for releasing the item.
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

	//! 
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

	//! 
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

	//! 
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
	CriticalSection  m_critSec;                  // critical section for thread safety

	ComPtrListEx<T>  m_queue;
};

} // namespace detail
} // namespace msw
} // namespace cinder
