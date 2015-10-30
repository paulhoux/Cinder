#pragma once

#include "cinder/msw/LinkList.h"

// TODO: include Synchapi.h on Windows 10 instead
#include <windows.h>

namespace cinder {
namespace msw {

template <class T>
class ThreadSafeQueue {
public:

	ThreadSafeQueue( void )
	{
		InitializeCriticalSection( &m_lock );
	}

	virtual ~ThreadSafeQueue( void )
	{
		DeleteCriticalSection( &m_lock );
	}

	HRESULT Queue( T* p )
	{
		EnterCriticalSection( &m_lock );
		HRESULT hr = m_list.InsertBack( p );
		LeaveCriticalSection( &m_lock );
		return hr;
	}

	HRESULT Dequeue( T** pp )
	{
		HRESULT hr = S_OK;

		EnterCriticalSection( &m_lock );
		if( m_list.IsEmpty() ) {
			*pp = NULL;
			hr = S_FALSE;
		}
		else {
			hr = m_list.RemoveFront( pp );
		}
		LeaveCriticalSection( &m_lock );
		return hr;
	}

	HRESULT PutBack( T* p )
	{
		EnterCriticalSection( &m_lock );
		HRESULT hr = m_list.InsertFront( p );
		LeaveCriticalSection( &m_lock );
		return hr;
	}

	DWORD GetCount( void )
	{
		EnterCriticalSection( &m_lock );
		DWORD nCount = m_list.GetCount();
		LeaveCriticalSection( &m_lock );
		return nCount;
	}

	void Clear( void )
	{
		EnterCriticalSection( &m_lock );
		m_list.Clear();
		LeaveCriticalSection( &m_lock );
	}

private:

	CRITICAL_SECTION    m_lock;
	ComPtrListEx<T>     m_list;
};

} // namespace msw
} // namespace cinder