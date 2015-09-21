/*
Copyright (c) 2015, The Cinder Project, All rights reserved.

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

#pragma once

#include "cinder/msw/CinderMsw.h"

#include <mmsystem.h> // for timeBeginPeriod and timeEndPeriod

//#include <d3d9.h>
//#include <d3d9types.h>
//#include <dxva2api.h>
#include <shobjidl.h> 
#include <shlwapi.h>

// Media Foundation headers.
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <evr.h>

// Include these libraries.
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

#pragma comment(lib, "winmm.lib") // for timeBeginPeriod and timeEndPeriod
//#pragma comment(lib,"d3d9.lib")
//#pragma comment(lib,"dxva2.lib")
//#pragma comment (lib,"evr.lib")

namespace cinder {
namespace msw {

template <class Q>
HRESULT GetEventObject( IMFMediaEvent *pEvent, Q **ppObject )
{
	*ppObject = NULL;

	PROPVARIANT var;
	HRESULT hr = pEvent->GetValue( &var );
	if( SUCCEEDED( hr ) ) {
		if( var.vt == VT_UNKNOWN ) {
			hr = var.punkVal->QueryInterface( ppObject );
		}
		else {
			hr = MF_E_INVALIDTYPE;
		}
		PropVariantClear( &var );
	}

	return hr;
}

//-----------------------------------------------------------------------------

inline LONG MFTimeToMsec( const LONGLONG& time )
{
	return (LONG)( time / ( 10000000 / 1000 ) );
}

//-----------------------------------------------------------------------------

template <class T>
struct NoOp {
	void operator()( T& t )
	{
	}
};

//-----------------------------------------------------------------------------

template <class T>
class List {
protected:
	// Nodes in the linked list
	struct Node {
		Node *prev;
		Node *next;
		T    item;

		Node() : prev( NULL ), next( NULL )
		{
		}

		Node( T item ) : prev( NULL ), next( NULL )
		{
			this->item = item;
		}

		T Item() const { return item; }
	};

public:
	// Object for enumerating the list.
	class POSITION {
		friend class List < T >;

	public:
		POSITION() : pNode( NULL )
		{
		}

		bool operator==( const POSITION &p ) const
		{
			return pNode == p.pNode;
		}

		bool operator!=( const POSITION &p ) const
		{
			return pNode != p.pNode;
		}

	private:
		const Node *pNode;

		POSITION( Node *p ) : pNode( p )
		{
		}
	};

protected:
	Node    m_anchor;  // Anchor node for the linked list.
	DWORD   m_count;   // Number of items in the list.

	Node* Front() const
	{
		return m_anchor.next;
	}

	Node* Back() const
	{
		return m_anchor.prev;
	}

	virtual HRESULT InsertAfter( T item, Node *pBefore )
	{
		if( pBefore == NULL ) {
			return E_POINTER;
		}

		Node *pNode = new Node( item );
		if( pNode == NULL ) {
			return E_OUTOFMEMORY;
		}

		Node *pAfter = pBefore->next;

		pBefore->next = pNode;
		pAfter->prev = pNode;

		pNode->prev = pBefore;
		pNode->next = pAfter;

		m_count++;

		return S_OK;
	}

	virtual HRESULT GetItem( const Node *pNode, T* ppItem )
	{
		if( pNode == NULL || ppItem == NULL ) {
			return E_POINTER;
		}

		*ppItem = pNode->item;
		return S_OK;
	}

	// RemoveItem:
	// Removes a node and optionally returns the item.
	// ppItem can be NULL.
	virtual HRESULT RemoveItem( Node *pNode, T *ppItem )
	{
		if( pNode == NULL ) {
			return E_POINTER;
		}

		assert( pNode != &m_anchor ); // We should never try to remove the anchor node.
		if( pNode == &m_anchor ) {
			return E_INVALIDARG;
		}


		T item;

		// The next node's previous is this node's previous.
		pNode->next->prev = pNode->prev;

		// The previous node's next is this node's next.
		pNode->prev->next = pNode->next;

		item = pNode->item;
		delete pNode;

		m_count--;

		if( ppItem ) {
			*ppItem = item;
		}

		return S_OK;
	}

public:
	List()
	{
		m_anchor.next = &m_anchor;
		m_anchor.prev = &m_anchor;

		m_count = 0;
	}

	virtual ~List()
	{
		Clear();
	}

	// Insertion functions
	HRESULT InsertBack( T item )
	{
		return InsertAfter( item, m_anchor.prev );
	}

	HRESULT InsertFront( T item )
	{
		return InsertAfter( item, &m_anchor );
	}

	// RemoveBack: Removes the tail of the list and returns the value.
	// ppItem can be NULL if you don't want the item back. (But the method does not release the item.)
	HRESULT RemoveBack( T *ppItem )
	{
		if( IsEmpty() ) {
			return E_FAIL;
		}
		else {
			return RemoveItem( Back(), ppItem );
		}
	}

	// RemoveFront: Removes the head of the list and returns the value.
	// ppItem can be NULL if you don't want the item back. (But the method does not release the item.)
	HRESULT RemoveFront( T *ppItem )
	{
		if( IsEmpty() ) {
			return E_FAIL;
		}
		else {
			return RemoveItem( Front(), ppItem );
		}
	}

	// GetBack: Gets the tail item.
	HRESULT GetBack( T *ppItem )
	{
		if( IsEmpty() ) {
			return E_FAIL;
		}
		else {
			return GetItem( Back(), ppItem );
		}
	}

	// GetFront: Gets the front item.
	HRESULT GetFront( T *ppItem )
	{
		if( IsEmpty() ) {
			return E_FAIL;
		}
		else {
			return GetItem( Front(), ppItem );
		}
	}

	// GetCount: Returns the number of items in the list.
	DWORD GetCount() const { return m_count; }

	bool IsEmpty() const
	{
		return ( GetCount() == 0 );
	}

	// Clear: Takes a functor object whose operator()
	// frees the object on the list.
	template <class FN>
	void Clear( FN& clear_fn )
	{
		Node *n = m_anchor.next;

		// Delete the nodes
		while( n != &m_anchor ) {
			clear_fn( n->item );

			Node *tmp = n->next;
			delete n;
			n = tmp;
		}

		// Reset the anchor to point at itself
		m_anchor.next = &m_anchor;
		m_anchor.prev = &m_anchor;

		m_count = 0;
	}

	// Clear: Clears the list. (Does not delete or release the list items.)
	virtual void Clear()
	{
		Clear<NoOp<T>>( NoOp<T>() );
	}

	// Enumerator functions

	POSITION FrontPosition()
	{
		if( IsEmpty() ) {
			return POSITION( NULL );
		}
		else {
			return POSITION( Front() );
		}
	}

	POSITION EndPosition() const
	{
		return POSITION();
	}

	HRESULT GetItemPos( POSITION pos, T *ppItem )
	{
		if( pos.pNode ) {
			return GetItem( pos.pNode, ppItem );
		}
		else {
			return E_FAIL;
		}
	}

	POSITION Next( const POSITION pos )
	{
		if( pos.pNode && ( pos.pNode->next != &m_anchor ) ) {
			return POSITION( pos.pNode->next );
		}
		else {
			return POSITION( NULL );
		}
	}

	// Remove an item at a position. 
	// The item is returns in ppItem, unless ppItem is NULL.
	// NOTE: This method invalidates the POSITION object.
	HRESULT Remove( POSITION& pos, T *ppItem )
	{
		if( pos.pNode ) {
			// Remove const-ness temporarily...
			Node *pNode = const_cast<Node*>( pos.pNode );

			pos = POSITION();

			return RemoveItem( pNode, ppItem );
		}
		else {
			return E_INVALIDARG;
		}
	}
};

//-----------------------------------------------------------------------------

class ComAutoRelease {
public:
	void operator()( IUnknown *p )
	{
		if( p ) {
			p->Release();
		}
	}
};

class MemDelete {
public:
	void operator()( void *p )
	{
		if( p ) {
			delete p;
		}
	}
};

//-----------------------------------------------------------------------------

template <class T, bool NULLABLE = FALSE>
class ComPtrList : public List < T* > {
public:

	typedef T* Ptr;

	void Clear()
	{
		List<Ptr>::Clear( ComAutoRelease() );
	}

	~ComPtrList()
	{
		Clear();
	}

protected:
	HRESULT InsertAfter( Ptr item, Node *pBefore )
	{
		// Do not allow NULL item pointers unless NULLABLE is true.
		if( !item && !NULLABLE ) {
			return E_POINTER;
		}

		if( item ) {
			item->AddRef();
		}

		HRESULT hr = List<Ptr>::InsertAfter( item, pBefore );
		if( FAILED( hr ) ) {
			if( item ) {
				item->Release();
			}
		}
		return hr;
	}

	HRESULT GetItem( const Node *pNode, Ptr* ppItem )
	{
		Ptr pItem = NULL;

		// The base class gives us the pointer without AddRef'ing it.
		// If we return the pointer to the caller, we must AddRef().
		HRESULT hr = List<Ptr>::GetItem( pNode, &pItem );
		if( SUCCEEDED( hr ) ) {
			assert( pItem || NULLABLE );
			if( pItem ) {
				*ppItem = pItem;
				( *ppItem )->AddRef();
			}
		}
		return hr;
	}

	HRESULT RemoveItem( Node *pNode, Ptr *ppItem )
	{
		// ppItem can be NULL, but we need to get the
		// item so that we can release it. 

		// If ppItem is not NULL, we will AddRef it on the way out.

		Ptr pItem = NULL;

		HRESULT hr = List<Ptr>::RemoveItem( pNode, &pItem );

		if( SUCCEEDED( hr ) ) {
			assert( pItem || NULLABLE );
			if( ppItem && pItem ) {
				*ppItem = pItem;
				( *ppItem )->AddRef();
			}

			if( pItem ) {
				pItem->Release();
			}
		}

		return hr;
	}
};

typedef ComPtrList<IMFSample> VideoSampleList;

//-----------------------------------------------------------------------------

class SamplePool {
public:
	SamplePool() : m_bInitialized( FALSE ), m_cPending( 0 ) {}
	virtual ~SamplePool() {}

	HRESULT Initialize( VideoSampleList& samples )
	{
		ScopedCriticalSection lock( m_lock );

		if( m_bInitialized ) {
			return MF_E_INVALIDREQUEST;
		}

		HRESULT hr = S_OK;

		// Move these samples into our allocated queue.
		VideoSampleList::POSITION pos = samples.FrontPosition();
		while( pos != samples.EndPosition() ) {
			ScopedComPtr<IMFSample> pSample;
			if( SUCCEEDED( hr = samples.GetItemPos( pos, &pSample ) ) ) {
				if( SUCCEEDED( hr = m_VideoSampleQueue.InsertBack( pSample ) ) ) {
					pos = samples.Next( pos );
				}
				else
					break;
			}
			else
				break;
		}

		if( SUCCEEDED( hr ) )
			m_bInitialized = TRUE;

		samples.Clear();

		return hr;
	}

	HRESULT Clear()
	{
		HRESULT hr = S_OK;

		ScopedCriticalSection lock( m_lock );

		m_VideoSampleQueue.Clear();
		m_bInitialized = FALSE;
		m_cPending = 0;
		return S_OK;
	}

	HRESULT GetSample( IMFSample **ppSample )    // Does not block.
	{
		ScopedCriticalSection lock( m_lock );

		if( !m_bInitialized ) {
			return MF_E_NOT_INITIALIZED;
		}

		if( m_VideoSampleQueue.IsEmpty() ) {
			return MF_E_SAMPLEALLOCATOR_EMPTY;
		}

		HRESULT hr = S_OK;

		// Get a sample from the allocated queue.

		// It doesn't matter if we pull them from the head or tail of the list,
		// but when we get it back, we want to re-insert it onto the opposite end.
		// (see ReturnSample)

		ScopedComPtr<IMFSample> pSample;
		hr = m_VideoSampleQueue.RemoveFront( &pSample );
		if( FAILED( hr ) )
			return hr;

		m_cPending++;

		// Give the sample to the caller.
		*ppSample = pSample;
		( *ppSample )->AddRef();

		return hr;
	}

	HRESULT ReturnSample( IMFSample *pSample )
	{
		ScopedCriticalSection lock( m_lock );

		if( !m_bInitialized ) {
			return MF_E_NOT_INITIALIZED;
		}

		HRESULT hr = m_VideoSampleQueue.InsertBack( pSample );
		if( FAILED( hr ) )
			return hr;

		m_cPending--;

		return hr;
	}

	BOOL AreSamplesPending()
	{
		ScopedCriticalSection lock( m_lock );

		if( !m_bInitialized ) {
			return FALSE;
		}

		return ( m_cPending > 0 );
	}

private:
	CriticalSection        m_lock;

	VideoSampleList        m_VideoSampleQueue;         // Available queue

	BOOL                   m_bInitialized;
	DWORD                  m_cPending;
};

//-----------------------------------------------------------------------------

template <class T>
class ThreadSafeQueue {
public:
	HRESULT Queue( T *p )
	{
		ScopedCriticalSection lock( m_lock );
		return m_list.InsertBack( p );
	}

	HRESULT Dequeue( T **pp )
	{
		ScopedCriticalSection lock( m_lock );

		if( m_list.IsEmpty() ) {
			*pp = NULL;
			return S_FALSE;
		}

		return m_list.RemoveFront( pp );
	}

	HRESULT PutBack( T *p )
	{
		ScopedCriticalSection lock( m_lock );
		return m_list.InsertFront( p );
	}

	void Clear()
	{
		ScopedCriticalSection lock( m_lock );
		m_list.Clear();
	}


private:
	CriticalSection         m_lock;
	ComPtrList<T>   m_list;
};

//-----------------------------------------------------------------------------

struct SchedulerCallback {
	virtual HRESULT PresentSample( IMFSample *pSample, LONGLONG llTarget ) = 0;
};

//-----------------------------------------------------------------------------

class Scheduler {
	enum ScheduleEvent {
		eTerminate = WM_USER,
		eSchedule = WM_USER + 1,
		eFlush = WM_USER + 2
	};

	const DWORD SCHEDULER_TIMEOUT = 5000;
public:
	Scheduler()
		: m_pCB( NULL ), mClockPtr( NULL ), m_dwThreadID( 0 ), mPlaybackRate( 1.0f ), m_LastSampleTime( 0 ), m_PerFrameInterval( 0 ), m_PerFrame_1_4th( 0 )
		, m_hSchedulerThread( NULL, ::CloseHandle ), m_hThreadReadyEvent( NULL, ::CloseHandle ), m_hFlushEvent( NULL, ::CloseHandle )
	{
	}
	virtual ~Scheduler()
	{
		SafeRelease( mClockPtr );
	}

	void SetCallback( SchedulerCallback *pCB )
	{
		m_pCB = pCB;
	}

	void SetFrameRate( const MFRatio& fps )
	{
		UINT64 AvgTimePerFrame = 0;

		// Convert to a duration.
		MFFrameRateToAverageTimePerFrame( fps.Numerator, fps.Denominator, &AvgTimePerFrame );

		m_PerFrameInterval = (MFTIME)AvgTimePerFrame;

		// Calculate 1/4th of this value, because we use it frequently.
		m_PerFrame_1_4th = m_PerFrameInterval / 4;
	}

	void SetClockRate( float fRate ) { mPlaybackRate = fRate; }

	const LONGLONG& LastSampleTime() const { return m_LastSampleTime; }
	const LONGLONG& FrameDuration() const { return m_PerFrameInterval; }

	HRESULT StartScheduler( IMFClock *pClock )
	{
		if( m_hSchedulerThread != NULL ) {
			return E_UNEXPECTED;
		}

		HRESULT hr = S_OK;

		CopyComPtr( mClockPtr, pClock );
		mClockPtr->AddRef();

		// Set a high the timer resolution (ie, short timer period).
		timeBeginPeriod( 1 );

		// Create an event to wait for the thread to start.
		HANDLE handle = CreateEvent( NULL, FALSE, FALSE, NULL );
		if( handle == NULL ) {
			return HRESULT_FROM_WIN32( GetLastError() );
		}
		m_hThreadReadyEvent.reset( handle );

		// Create an event to wait for flush commands to complete.
		handle = CreateEvent( NULL, FALSE, FALSE, NULL );
		if( handle == NULL ) {
			return HRESULT_FROM_WIN32( GetLastError() );
		}
		m_hFlushEvent.reset( handle );

		// Create the scheduler thread.
		DWORD dwID = 0;
		handle = CreateThread( NULL, 0, SchedulerThreadProc, ( LPVOID )this, 0, &dwID );
		if( handle == NULL ) {
			return HRESULT_FROM_WIN32( GetLastError() );
		}
		SetThreadPriority( handle, THREAD_PRIORITY_TIME_CRITICAL );
		m_hSchedulerThread.reset( handle );

		HANDLE hObjects[] = { m_hThreadReadyEvent.get(), m_hSchedulerThread.get() };
		DWORD dwWait = 0;

		// Wait for the thread to signal the "thread ready" event.
		dwWait = WaitForMultipleObjects( 2, hObjects, FALSE, INFINITE );  // Wait for EITHER of these handles.
		if( WAIT_OBJECT_0 != dwWait ) {
			// The thread terminated early for some reason. This is an error condition.
			m_hSchedulerThread.reset();
			return E_UNEXPECTED;
		}

		m_dwThreadID = dwID;

		return hr;
	}

	HRESULT StopScheduler()
	{
		if( m_hSchedulerThread.get() == NULL ) {
			return S_OK;
		}

		// Ask the scheduler thread to exit.
		PostThreadMessage( m_dwThreadID, eTerminate, 0, 0 );

		// Wait for the thread to exit.
		WaitForSingleObject( m_hSchedulerThread.get(), INFINITE );

		// Close handles.
		m_hSchedulerThread.reset();
		m_hFlushEvent.reset();

		// Discard samples.
		m_ScheduledSamples.Clear();

		// Restore the timer resolution.
		timeEndPeriod( 1 );

		return S_OK;
	}

	HRESULT ScheduleSample( IMFSample *pSample, BOOL bPresentNow )
	{
		if( m_pCB == NULL ) {
			return MF_E_NOT_INITIALIZED;
		}

		if( m_hSchedulerThread.get() == NULL ) {
			return MF_E_NOT_INITIALIZED;
		}

		HRESULT hr = S_OK;
		DWORD dwExitCode = 0;

		GetExitCodeThread( m_hSchedulerThread.get(), &dwExitCode );
		if( dwExitCode != STILL_ACTIVE ) {
			return E_FAIL;
		}

		if( bPresentNow || ( mClockPtr == NULL ) ) {
			// Present the sample immediately.
			hr = m_pCB->PresentSample( pSample, 0 );
		}
		else {
			// Queue the sample and ask the scheduler thread to wake up.
			hr = m_ScheduledSamples.Queue( pSample );

			if( SUCCEEDED( hr ) ) {
				PostThreadMessage( m_dwThreadID, eSchedule, 0, 0 );
			}
		}

		return hr;
	}

	HRESULT ProcessSamplesInQueue( LONG *plNextSleep )
	{
		HRESULT hr = S_OK;
		LONG lWait = 0;

		// Process samples until the queue is empty or until the wait time > 0.

		// Note: Dequeue returns S_FALSE when the queue is empty.
		ScopedComPtr<IMFSample> pSample;
		while( m_ScheduledSamples.Dequeue( &pSample ) == S_OK ) {
			// Process the next sample in the queue. If the sample is not ready
			// for presentation. the value returned in lWait is > 0, which
			// means the scheduler should sleep for that amount of time.

			hr = ProcessSample( pSample, &lWait );
			pSample.Release();

			if( FAILED( hr ) ) {
				break;
			}
			if( lWait > 0 ) {
				break;
			}
		}

		// If the wait time is zero, it means we stopped because the queue is
		// empty (or an error occurred). Set the wait time to infinite; this will
		// make the scheduler thread sleep until it gets another thread message.
		if( lWait == 0 ) {
			lWait = INFINITE;
		}

		*plNextSleep = lWait;
		return hr;
	}

	HRESULT ProcessSample( IMFSample *pSample, LONG *plNextSleep )
	{
		HRESULT hr = S_OK;

		LONGLONG hnsPresentationTime = 0;
		LONGLONG hnsTimeNow = 0;
		MFTIME   hnsSystemTime = 0;

		BOOL bPresentNow = TRUE;
		LONG lNextSleep = 0;

		if( mClockPtr ) {
			// Get the sample's time stamp. It is valid for a sample to
			// have no time stamp.
			hr = pSample->GetSampleTime( &hnsPresentationTime );

			// Get the clock time. (But if the sample does not have a time stamp, 
			// we don't need the clock time.)
			if( SUCCEEDED( hr ) ) {
				hr = mClockPtr->GetCorrelatedTime( 0, &hnsTimeNow, &hnsSystemTime );
			}

			// Calculate the time until the sample's presentation time. 
			// A negative value means the sample is late.
			LONGLONG hnsDelta = hnsPresentationTime - hnsTimeNow;
			if( mPlaybackRate < 0 ) {
				// For reverse playback, the clock runs backward. Therefore the delta is reversed.
				hnsDelta = -hnsDelta;
			}

			if( hnsDelta < -m_PerFrame_1_4th ) {
				// This sample is late. 
				bPresentNow = TRUE;
			}
			else if( hnsDelta >( 3 * m_PerFrame_1_4th ) ) {
				// This sample is still too early. Go to sleep.
				lNextSleep = MFTimeToMsec( hnsDelta - ( 3 * m_PerFrame_1_4th ) );

				// Adjust the sleep time for the clock rate. (The presentation clock runs
				// at mPlaybackRate, but sleeping uses the system clock.)
				lNextSleep = (LONG)( lNextSleep / fabsf( mPlaybackRate ) );

				// Don't present yet.
				bPresentNow = FALSE;
			}
		}

		if( bPresentNow ) {
			hr = m_pCB->PresentSample( pSample, hnsPresentationTime );
		}
		else {
			// The sample is not ready yet. Return it to the queue.
			hr = m_ScheduledSamples.PutBack( pSample );
		}

		*plNextSleep = lNextSleep;

		return hr;
	}

	HRESULT Flush()
	{
		if( m_hSchedulerThread.get() == NULL ) {
			return MF_E_NOT_INITIALIZED;
		}

		// Ask the scheduler thread to flush.
		PostThreadMessage( m_dwThreadID, eFlush, 0, 0 );

		// Wait for the scheduler thread to signal the flush event,
		// OR for the thread to terminate.
		HANDLE objects[] = { m_hFlushEvent.get(), m_hSchedulerThread.get() };
		WaitForMultipleObjects( 2, objects, FALSE, SCHEDULER_TIMEOUT );

		return S_OK;
	}

	// ThreadProc for the scheduler thread.
	static DWORD WINAPI SchedulerThreadProc( LPVOID lpParameter )
	{
		Scheduler* pScheduler = reinterpret_cast<Scheduler*>( lpParameter );
		if( pScheduler == NULL ) {
			return -1;
		}
		return pScheduler->SchedulerThreadProcPrivate();
	}

private:
	// non-static version of SchedulerThreadProc.
	DWORD SchedulerThreadProcPrivate()
	{
		HRESULT hr = S_OK;
		MSG     msg;
		LONG    lWait = INFINITE;
		BOOL    bExitThread = FALSE;

		// Force the system to create a message queue for this thread.
		// (See MSDN documentation for PostThreadMessage.)
		PeekMessage( &msg, NULL, WM_USER, WM_USER, PM_NOREMOVE );

		// Signal to the scheduler that the thread is ready.
		SetEvent( m_hThreadReadyEvent.get() );

		while( !bExitThread ) {
			// Wait for a thread message OR until the wait time expires.
			DWORD dwResult = MsgWaitForMultipleObjects( 0, NULL, FALSE, lWait, QS_POSTMESSAGE );

			if( dwResult == WAIT_TIMEOUT ) {
				// If we timed out, then process the samples in the queue
				hr = ProcessSamplesInQueue( &lWait );
				if( FAILED( hr ) ) {
					bExitThread = TRUE;
				}
			}

			while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) ) {
				BOOL bProcessSamples = TRUE;

				switch( msg.message ) {
					case eTerminate:
						bExitThread = TRUE;
						break;

					case eFlush:
						// Flushing: Clear the sample queue and set the event.
						m_ScheduledSamples.Clear();
						lWait = INFINITE;
						SetEvent( m_hFlushEvent.get() );
						break;

					case eSchedule:
						// Process as many samples as we can.
						if( bProcessSamples ) {
							hr = ProcessSamplesInQueue( &lWait );
							if( FAILED( hr ) ) {
								bExitThread = TRUE;
							}
							bProcessSamples = ( lWait != INFINITE );
						}
						break;
				} // switch  

			} // while PeekMessage

		}  // while (!bExitThread)

		return ( SUCCEEDED( hr ) ? 0 : 1 );
	}

private:
	ThreadSafeQueue<IMFSample>  m_ScheduledSamples;     // Samples waiting to be presented.

	IMFClock            *mClockPtr;  // Presentation clock. Can be NULL.
	SchedulerCallback   *m_pCB;     // Weak reference; do not delete.

	DWORD               m_dwThreadID;
	//HANDLE              m_hSchedulerThread;
	//HANDLE              m_hThreadReadyEvent;
	//HANDLE              m_hFlushEvent;
	std::unique_ptr<void, decltype( &::CloseHandle )> m_hSchedulerThread;
	std::unique_ptr<void, decltype( &::CloseHandle )> m_hThreadReadyEvent;
	std::unique_ptr<void, decltype( &::CloseHandle )> m_hFlushEvent;

	float               mPlaybackRate;                // Playback rate.
	MFTIME              m_PerFrameInterval;     // Duration of each frame.
	LONGLONG            m_PerFrame_1_4th;       // 1/4th of the frame duration.
	MFTIME              m_LastSampleTime;       // Most recent sample time.
};

//-----------------------------------------------------------------------------

inline MFOffset MakeOffset( float v )
{
	MFOffset offset;
	offset.value = short( v );
	offset.fract = WORD( 65536 * ( v - offset.value ) );
	return offset;
}

inline MFVideoArea MakeArea( float x, float y, DWORD width, DWORD height )
{
	MFVideoArea area;
	area.OffsetX = MakeOffset( x );
	area.OffsetY = MakeOffset( y );
	area.Area.cx = width;
	area.Area.cy = height;
	return area;
}

// Get the frame rate from a video media type.
inline HRESULT GetFrameRate( IMFMediaType *pType, MFRatio *pRatio )
{
	return MFGetAttributeRatio( pType, MF_MT_FRAME_RATE, (UINT32*)&pRatio->Numerator, (UINT32*)&pRatio->Denominator );
}

// Get the correct display area from a video media type.
inline HRESULT GetVideoDisplayArea( IMFMediaType *pType, MFVideoArea *pArea )
{
	HRESULT hr = S_OK;
	BOOL bPanScan = FALSE;
	UINT32 width = 0, height = 0;

	bPanScan = MFGetAttributeUINT32( pType, MF_MT_PAN_SCAN_ENABLED, FALSE );

	// In pan/scan mode, try to get the pan/scan region.
	if( bPanScan ) {
		hr = pType->GetBlob( MF_MT_PAN_SCAN_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );
	}

	// If not in pan/scan mode, or the pan/scan region is not set, get the minimimum display aperture.

	if( !bPanScan || hr == MF_E_ATTRIBUTENOTFOUND ) {
		hr = pType->GetBlob( MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );

		if( hr == MF_E_ATTRIBUTENOTFOUND ) {
			// Minimum display aperture is not set.

			// For backward compatibility with some components, check for a geometric aperture. 
			hr = pType->GetBlob( MF_MT_GEOMETRIC_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );
		}

		// Default: Use the entire video area.

		if( hr == MF_E_ATTRIBUTENOTFOUND ) {
			hr = MFGetAttributeSize( pType, MF_MT_FRAME_SIZE, &width, &height );
			if( SUCCEEDED( hr ) ) {
				*pArea = MakeArea( 0.0, 0.0, width, height );
			}
		}
	}

	return hr;
}


inline HRESULT GetDefaultStride( IMFMediaType *pType, LONG *plStride )
{
	LONG lStride = 0;

	// Try to get the default stride from the media type.
	HRESULT hr = pType->GetUINT32( MF_MT_DEFAULT_STRIDE, (UINT32*)&lStride );
	if( FAILED( hr ) ) {
		// Attribute not set. Try to calculate the default stride.
		GUID subtype = GUID_NULL;

		UINT32 width = 0;
		UINT32 height = 0;

		// Get the subtype and the image size.
		hr = pType->GetGUID( MF_MT_SUBTYPE, &subtype );
		if( SUCCEEDED( hr ) ) {
			hr = MFGetAttributeSize( pType, MF_MT_FRAME_SIZE, &width, &height );
		}
		if( SUCCEEDED( hr ) ) {
			hr = MFGetStrideForBitmapInfoHeader( subtype.Data1, width, &lStride );
		}

		// Set the attribute for later reference.
		if( SUCCEEDED( hr ) ) {
			(void)pType->SetUINT32( MF_MT_DEFAULT_STRIDE, UINT32( lStride ) );
		}
	}

	if( SUCCEEDED( hr ) ) {
		*plStride = lStride;
	}
	return hr;
}

//-----------------------------------------------------------------------------

class MediaType {
protected:
	IMFMediaType* m_pType;

protected:
	BOOL IsValid()
	{
		return m_pType != NULL;
	}

	IMFMediaType* GetMediaType()
	{
		assert( IsValid() );
		return m_pType;
	}

public:
	// Construct from an existing media type.
	MediaType( IMFMediaType* pType = NULL ) : m_pType( NULL )
	{
		if( pType ) {
			m_pType = pType;
			m_pType->AddRef();
		}
	}

	// Copy ctor
	MediaType( const MediaType&  mt )
	{
		m_pType = mt.m_pType;
		if( m_pType ) {
			m_pType->AddRef();
		}
	}

	virtual ~MediaType()
	{
		SafeRelease( m_pType );
	}


	// Assignment
	MediaType& operator=( const MediaType& mt )
	{
		// If we are assigned to ourselves, do nothing.
		if( !AreComObjectsEqual( m_pType, mt.m_pType ) ) {
			if( m_pType ) {
				m_pType->Release();
			}

			m_pType = mt.m_pType;
			if( m_pType ) {
				m_pType->AddRef();
			}
		}
		return *this;
	}

	// address-of operator
	IMFMediaType** operator&()
	{
		return &m_pType;
	}

	// coerce to underlying pointer type.
	operator IMFMediaType*( )
	{
		return m_pType;
	}

	virtual HRESULT CreateEmptyType()
	{
		SafeRelease( m_pType );

		HRESULT hr = S_OK;

		hr = MFCreateMediaType( &m_pType );

		return hr;
	}

	// Detach the interface (does not Release)
	IMFMediaType* Detach()
	{
		IMFMediaType* p = m_pType;
		m_pType = NULL;
		return p;
	}

	// Direct wrappers of IMFMediaType methods.
	// (For these methods, we leave parameter validation to the IMFMediaType implementation.)

	// Retrieves the major type GUID.
	HRESULT GetMajorType( GUID *pGuid )
	{
		return GetMediaType()->GetMajorType( pGuid );
	}

	// Specifies whether the media data is compressed
	HRESULT IsCompressedFormat( BOOL *pbCompressed )
	{
		return GetMediaType()->IsCompressedFormat( pbCompressed );
	}

	// Compares two media types and determines whether they are identical.
	HRESULT IsEqual( IMFMediaType *pType, DWORD *pdwFlags )
	{
		return GetMediaType()->IsEqual( pType, pdwFlags );
	}

	// Retrieves an alternative representation of the media type.
	HRESULT GetRepresentation( GUID guidRepresentation, LPVOID *ppvRepresentation )
	{
		return GetMediaType()->GetRepresentation( guidRepresentation, ppvRepresentation );
	}

	// Frees memory that was allocated by the GetRepresentation method.
	HRESULT FreeRepresentation( GUID guidRepresentation, LPVOID pvRepresentation )
	{
		return GetMediaType()->FreeRepresentation( guidRepresentation, pvRepresentation );
	}


	// Helper methods

	// CopyFrom: Copy all of the attributes from another media type into this type.
	HRESULT CopyFrom( MediaType *pType )
	{
		if( !pType->IsValid() ) {
			return E_UNEXPECTED;
		}
		if( pType == NULL ) {
			return E_POINTER;
		}
		return CopyFrom( pType->m_pType );
	}

	HRESULT CopyFrom( IMFMediaType *pType )
	{
		if( pType == NULL ) {
			return E_POINTER;
		}
		return pType->CopyAllItems( m_pType );
	}

	// Returns the underlying IMFMediaType pointer. 
	HRESULT GetMediaType( IMFMediaType** ppType )
	{
		assert( IsValid() );
		if( ppType == NULL ) return E_POINTER;
		*ppType = m_pType;
		( *ppType )->AddRef();
		return S_OK;
	}

	// Sets the major type GUID.
	HRESULT SetMajorType( GUID guid )
	{
		return GetMediaType()->SetGUID( MF_MT_MAJOR_TYPE, guid );
	}

	// Retrieves the subtype GUID.
	HRESULT GetSubType( GUID* pGuid )
	{
		if( pGuid == NULL ) return E_POINTER;
		return GetMediaType()->GetGUID( MF_MT_SUBTYPE, pGuid );
	}

	// Sets the subtype GUID.
	HRESULT SetSubType( GUID guid )
	{
		return GetMediaType()->SetGUID( MF_MT_SUBTYPE, guid );
	}

	// Extracts the FOURCC code from the subtype.
	// Not all subtypes follow this pattern.
	HRESULT GetFourCC( DWORD *pFourCC )
	{
		assert( IsValid() );
		if( pFourCC == NULL ) return E_POINTER;
		HRESULT hr = S_OK;
		GUID guidSubType = GUID_NULL;

		if( SUCCEEDED( hr ) ) {
			hr = GetSubType( &guidSubType );
		}

		if( SUCCEEDED( hr ) ) {
			*pFourCC = guidSubType.Data1;
		}
		return hr;
	}

	//  Queries whether each sample is independent of the other samples in the stream.
	HRESULT GetAllSamplesIndependent( BOOL* pbIndependent )
	{
		if( pbIndependent == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_ALL_SAMPLES_INDEPENDENT, (UINT32*)pbIndependent );
	}

	//  Specifies whether each sample is independent of the other samples in the stream.
	HRESULT SetAllSamplesIndependent( BOOL bIndependent )
	{
		return GetMediaType()->SetUINT32( MF_MT_ALL_SAMPLES_INDEPENDENT, (UINT32)bIndependent );
	}

	// Queries whether the samples have a fixed size.
	HRESULT GetFixedSizeSamples( BOOL *pbFixed )
	{
		if( pbFixed == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_FIXED_SIZE_SAMPLES, (UINT32*)pbFixed );
	}

	// Specifies whether the samples have a fixed size.
	HRESULT SetFixedSizeSamples( BOOL bFixed )
	{
		return GetMediaType()->SetUINT32( MF_MT_FIXED_SIZE_SAMPLES, (UINT32)bFixed );
	}

	// Retrieves the size of each sample, in bytes. 
	HRESULT GetSampleSize( UINT32 *pnSize )
	{
		if( pnSize == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_SAMPLE_SIZE, pnSize );
	}

	// Sets the size of each sample, in bytes. 
	HRESULT SetSampleSize( UINT32 nSize )
	{
		return GetMediaType()->SetUINT32( MF_MT_SAMPLE_SIZE, nSize );
	}

	// Retrieves a media type that was wrapped by the MFWrapMediaType function.
	HRESULT Unwrap( IMFMediaType **ppOriginal )
	{
		if( ppOriginal == NULL ) return E_POINTER;
		return ::MFUnwrapMediaType( GetMediaType(), ppOriginal );
	}

	// The following versions return reasonable defaults if the relevant attribute is not present (zero/FALSE).
	// This is useful for making quick comparisons betweeen media types. 

	BOOL AllSamplesIndependent()
	{
		return (BOOL)MFGetAttributeUINT32( GetMediaType(), MF_MT_ALL_SAMPLES_INDEPENDENT, FALSE );
	}

	BOOL FixedSizeSamples()
	{
		return (BOOL)MFGetAttributeUINT32( GetMediaType(), MF_MT_FIXED_SIZE_SAMPLES, FALSE );
	}

	UINT32 SampleSize()
	{
		return MFGetAttributeUINT32( GetMediaType(), MF_MT_SAMPLE_SIZE, 0 );
	}
};

//-----------------------------------------------------------------------------

class VideoType : public MediaType {
	friend class MediaType;

public:

	VideoType( IMFMediaType* pType = NULL ) : MediaType( pType )
	{
	}

	virtual HRESULT CreateEmptyType()
	{
		SafeRelease( m_pType );

		HRESULT hr = S_OK;

		hr = MediaType::CreateEmptyType();
		if( SUCCEEDED( hr ) ) {
			hr = SetMajorType( MFMediaType_Video );
		}
		return hr;
	}


	// Retrieves a description of how the frames are interlaced.
	HRESULT GetInterlaceMode( MFVideoInterlaceMode *pmode )
	{
		if( pmode == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_INTERLACE_MODE, (UINT32*)pmode );
	}

	// Sets a description of how the frames are interlaced.
	HRESULT SetInterlaceMode( MFVideoInterlaceMode mode )
	{
		return GetMediaType()->SetUINT32( MF_MT_INTERLACE_MODE, (UINT32)mode );
	}

	// This returns the default or attempts to compute it, in its absence.
	HRESULT GetDefaultStride( LONG *plStride )
	{
		return ci::msw::GetDefaultStride( GetMediaType(), plStride );
	}

	// Sets the default stride. Only appropriate for uncompressed data formats.
	HRESULT SetDefaultStride( LONG nStride )
	{
		return GetMediaType()->SetUINT32( MF_MT_DEFAULT_STRIDE, (UINT32)nStride );
	}

	// Retrieves the width and height of the video frame.
	HRESULT GetFrameDimensions( UINT32 *pdwWidthInPixels, UINT32 *pdwHeightInPixels )
	{
		return MFGetAttributeSize( GetMediaType(), MF_MT_FRAME_SIZE, pdwWidthInPixels, pdwHeightInPixels );
	}

	// Sets the width and height of the video frame.
	HRESULT SetFrameDimensions( UINT32 dwWidthInPixels, UINT32 dwHeightInPixels )
	{
		return MFSetAttributeSize( GetMediaType(), MF_MT_FRAME_SIZE, dwWidthInPixels, dwHeightInPixels );
	}

	// Retrieves the data error rate in bit errors per second
	HRESULT GetDataBitErrorRate( UINT32 *pRate )
	{
		if( pRate == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AVG_BIT_ERROR_RATE, pRate );
	}

	// Sets the data error rate in bit errors per second
	HRESULT SetDataBitErrorRate( UINT32 rate )
	{
		return GetMediaType()->SetUINT32( MF_MT_AVG_BIT_ERROR_RATE, rate );
	}

	// Retrieves the approximate data rate of the video stream.
	HRESULT GetAverageBitRate( UINT32 *pRate )
	{
		if( pRate == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AVG_BITRATE, pRate );
	}

	// Sets the approximate data rate of the video stream.
	HRESULT SetAvgerageBitRate( UINT32 rate )
	{
		return GetMediaType()->SetUINT32( MF_MT_AVG_BITRATE, rate );
	}

	// Retrieves custom color primaries.
	HRESULT GetCustomVideoPrimaries( MT_CUSTOM_VIDEO_PRIMARIES *pPrimaries )
	{
		if( pPrimaries == NULL ) return E_POINTER;
		return GetMediaType()->GetBlob( MF_MT_CUSTOM_VIDEO_PRIMARIES, (UINT8*)pPrimaries, sizeof( MT_CUSTOM_VIDEO_PRIMARIES ), NULL );
	}

	// Sets custom color primaries.
	HRESULT SetCustomVideoPrimaries( const MT_CUSTOM_VIDEO_PRIMARIES& primary )
	{
		return GetMediaType()->SetBlob( MF_MT_CUSTOM_VIDEO_PRIMARIES, (const UINT8*)&primary, sizeof( MT_CUSTOM_VIDEO_PRIMARIES ) );
	}

	// Gets the number of frames per second.
	HRESULT GetFrameRate( UINT32 *pnNumerator, UINT32 *pnDenominator )
	{
		if( pnNumerator == NULL ) return E_POINTER;
		if( pnDenominator == NULL ) return E_POINTER;
		return MFGetAttributeRatio( GetMediaType(), MF_MT_FRAME_RATE, pnNumerator, pnDenominator );
	}

	// Gets the frames per second as a ratio.
	HRESULT GetFrameRate( MFRatio *pRatio )
	{
		if( pRatio == NULL ) return E_POINTER;
		return GetFrameRate( (UINT32*)&pRatio->Numerator, (UINT32*)&pRatio->Denominator );
	}

	// Sets the number of frames per second.
	HRESULT SetFrameRate( UINT32 nNumerator, UINT32 nDenominator )
	{
		return MFSetAttributeRatio( GetMediaType(), MF_MT_FRAME_RATE, nNumerator, nDenominator );
	}

	// Sets the number of frames per second, as a ratio.
	HRESULT SetFrameRate( const MFRatio& ratio )
	{
		return MFSetAttributeRatio( GetMediaType(), MF_MT_FRAME_RATE, ratio.Numerator, ratio.Denominator );
	}

	// Queries the geometric aperture.
	HRESULT GetGeometricAperture( MFVideoArea *pArea )
	{
		if( pArea == NULL ) return E_POINTER;
		return GetMediaType()->GetBlob( MF_MT_GEOMETRIC_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );
	}

	// Sets the geometric aperture.
	HRESULT SetGeometricAperture( const MFVideoArea& area )
	{
		return GetMediaType()->SetBlob( MF_MT_GEOMETRIC_APERTURE, (UINT8*)&area, sizeof( MFVideoArea ) );
	}

	// Retrieves the maximum number of frames from one key frame to the next.
	HRESULT GetMaxKeyframeSpacing( UINT32 *pnSpacing )
	{
		if( pnSpacing == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_MAX_KEYFRAME_SPACING, pnSpacing );
	}

	// Sets the maximum number of frames from one key frame to the next.
	HRESULT SetMaxKeyframeSpacing( UINT32 nSpacing )
	{
		return GetMediaType()->SetUINT32( MF_MT_MAX_KEYFRAME_SPACING, nSpacing );
	}

	// Retrieves the region that contains the valid portion of the signal.
	HRESULT GetMinDisplayAperture( MFVideoArea *pArea )
	{
		if( pArea == NULL ) return E_POINTER;
		return GetMediaType()->GetBlob( MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );
	}

	// Sets the the region that contains the valid portion of the signal.
	HRESULT SetMinDisplayAperture( const MFVideoArea& area )
	{
		return GetMediaType()->SetBlob( MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&area, sizeof( MFVideoArea ) );
	}


	// Retrieves the aspect ratio of the output rectangle for a video media type. 
	HRESULT GetPadControlFlags( MFVideoPadFlags *pFlags )
	{
		if( pFlags == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_PAD_CONTROL_FLAGS, (UINT32*)pFlags );
	}

	// Sets the aspect ratio of the output rectangle for a video media type. 
	HRESULT SetPadControlFlags( MFVideoPadFlags flags )
	{
		return GetMediaType()->SetUINT32( MF_MT_PAD_CONTROL_FLAGS, flags );
	}

	// Retrieves an array of palette entries for a video media type. 
	HRESULT GetPaletteEntries( MFPaletteEntry *paEntries, UINT32 nEntries )
	{
		if( paEntries == NULL ) return E_POINTER;
		return GetMediaType()->GetBlob( MF_MT_PALETTE, (UINT8*)paEntries, sizeof( MFPaletteEntry ) * nEntries, NULL );
	}

	// Sets an array of palette entries for a video media type. 
	HRESULT SetPaletteEntries( MFPaletteEntry *paEntries, UINT32 nEntries )
	{
		if( paEntries == NULL ) return E_POINTER;
		return GetMediaType()->SetBlob( MF_MT_PALETTE, (UINT8*)paEntries, sizeof( MFPaletteEntry ) * nEntries );
	}

	// Retrieves the number of palette entries.
	HRESULT GetNumPaletteEntries( UINT32 *pnEntries )
	{
		if( pnEntries == NULL ) return E_POINTER;
		UINT32 nBytes = 0;
		HRESULT hr = S_OK;
		hr = GetMediaType()->GetBlobSize( MF_MT_PALETTE, &nBytes );
		if( SUCCEEDED( hr ) ) {
			if( nBytes % sizeof( MFPaletteEntry ) != 0 ) {
				hr = E_UNEXPECTED;
			}
		}
		if( SUCCEEDED( hr ) ) {
			*pnEntries = nBytes / sizeof( MFPaletteEntry );
		}
		return hr;
	}

	// Queries the 4�3 region of video that should be displayed in pan/scan mode.
	HRESULT GetPanScanAperture( MFVideoArea *pArea )
	{
		if( pArea == NULL ) return E_POINTER;
		return GetMediaType()->GetBlob( MF_MT_PAN_SCAN_APERTURE, (UINT8*)pArea, sizeof( MFVideoArea ), NULL );
	}

	// Sets the 4�3 region of video that should be displayed in pan/scan mode.
	HRESULT SetPanScanAperture( const MFVideoArea& area )
	{
		return GetMediaType()->SetBlob( MF_MT_PAN_SCAN_APERTURE, (UINT8*)&area, sizeof( MFVideoArea ) );
	}

	// Queries whether pan/scan mode is enabled.
	HRESULT IsPanScanEnabled( BOOL *pBool )
	{
		if( pBool == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_PAN_SCAN_ENABLED, (UINT32*)pBool );
	}

	// Sets whether pan/scan mode is enabled.
	HRESULT SetPanScanEnabled( BOOL bEnabled )
	{
		return GetMediaType()->SetUINT32( MF_MT_PAN_SCAN_ENABLED, (UINT32)bEnabled );
	}

	// Queries the pixel aspect ratio
	HRESULT GetPixelAspectRatio( UINT32 *pnNumerator, UINT32 *pnDenominator )
	{
		if( pnNumerator == NULL ) return E_POINTER;
		if( pnDenominator == NULL ) return E_POINTER;
		return MFGetAttributeRatio( GetMediaType(), MF_MT_PIXEL_ASPECT_RATIO, pnNumerator, pnDenominator );
	}

	// Sets the pixel aspect ratio
	HRESULT SetPixelAspectRatio( UINT32 nNumerator, UINT32 nDenominator )
	{
		return MFSetAttributeRatio( GetMediaType(), MF_MT_PIXEL_ASPECT_RATIO, nNumerator, nDenominator );
	}

	HRESULT SetPixelAspectRatio( const MFRatio& ratio )
	{
		return MFSetAttributeRatio( GetMediaType(), MF_MT_PIXEL_ASPECT_RATIO, ratio.Numerator, ratio.Denominator );
	}

	// Queries the intended aspect ratio.
	HRESULT GetSourceContentHint( MFVideoSrcContentHintFlags *pFlags )
	{
		if( pFlags == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_SOURCE_CONTENT_HINT, (UINT32*)pFlags );
	}

	// Sets the intended aspect ratio.
	HRESULT SetSourceContentHint( MFVideoSrcContentHintFlags nFlags )
	{
		return GetMediaType()->SetUINT32( MF_MT_SOURCE_CONTENT_HINT, (UINT32)nFlags );
	}

	// Queries an enumeration which represents the conversion function from RGB to R'G'B'.
	HRESULT GetTransferFunction( MFVideoTransferFunction *pnFxn )
	{
		if( pnFxn == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_TRANSFER_FUNCTION, (UINT32*)pnFxn );
	}

	// Set an enumeration which represents the conversion function from RGB to R'G'B'.
	HRESULT SetTransferFunction( MFVideoTransferFunction nFxn )
	{
		return GetMediaType()->SetUINT32( MF_MT_TRANSFER_FUNCTION, (UINT32)nFxn );
	}

	// Queries how chroma was sampled for a Y'Cb'Cr' video media type.
	HRESULT GetChromaSiting( MFVideoChromaSubsampling *pSampling )
	{
		if( pSampling == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_VIDEO_CHROMA_SITING, (UINT32*)pSampling );
	}

	// Sets how chroma was sampled for a Y'Cb'Cr' video media type.
	HRESULT SetChromaSiting( MFVideoChromaSubsampling nSampling )
	{
		return GetMediaType()->SetUINT32( MF_MT_VIDEO_CHROMA_SITING, (UINT32)nSampling );
	}

	// Queries the optimal lighting conditions for viewing.
	HRESULT GetVideoLighting( MFVideoLighting *pLighting )
	{
		if( pLighting == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_VIDEO_LIGHTING, (UINT32*)pLighting );
	}

	// Sets the optimal lighting conditions for viewing.
	HRESULT SetVideoLighting( MFVideoLighting nLighting )
	{
		return GetMediaType()->SetUINT32( MF_MT_VIDEO_LIGHTING, (UINT32)nLighting );
	}

	// Queries the nominal range of the color information in a video media type. 
	HRESULT GetVideoNominalRange( MFNominalRange *pRange )
	{
		if( pRange == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_VIDEO_NOMINAL_RANGE, (UINT32*)pRange );
	}

	// Sets the nominal range of the color information in a video media type. 
	HRESULT SetVideoNominalRange( MFNominalRange nRange )
	{
		return GetMediaType()->SetUINT32( MF_MT_VIDEO_NOMINAL_RANGE, (UINT32)nRange );
	}

	// Queries the color primaries for a video media type.
	HRESULT GetVideoPrimaries( MFVideoPrimaries *pPrimaries )
	{
		if( pPrimaries == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_VIDEO_PRIMARIES, (UINT32*)pPrimaries );
	}

	// Sets the color primaries for a video media type.
	HRESULT SetVideoPrimaries( MFVideoPrimaries nPrimaries )
	{
		return GetMediaType()->SetUINT32( MF_MT_VIDEO_PRIMARIES, (UINT32)nPrimaries );
	}

	// Gets a enumeration representing the conversion matrix from the 
	// Y'Cb'Cr' color space to the R'G'B' color space.
	HRESULT GetYUVMatrix( MFVideoTransferMatrix *pMatrix )
	{
		if( pMatrix == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_YUV_MATRIX, (UINT32*)pMatrix );
	}

	// Sets an enumeration representing the conversion matrix from the 
	// Y'Cb'Cr' color space to the R'G'B' color space.
	HRESULT SetYUVMatrix( MFVideoTransferMatrix nMatrix )
	{
		return GetMediaType()->SetUINT32( MF_MT_YUV_MATRIX, (UINT32)nMatrix );
	}

	// 
	// The following versions return reasonable defaults if the relevant attribute is not present (zero/FALSE).
	// This is useful for making quick comparisons betweeen media types. 
	//

	MFRatio GetPixelAspectRatio() // Defaults to 1:1 (square pixels)
	{
		MFRatio PAR = { 0, 0 };
		HRESULT hr = S_OK;

		hr = MFGetAttributeRatio( GetMediaType(), MF_MT_PIXEL_ASPECT_RATIO, (UINT32*)&PAR.Numerator, (UINT32*)&PAR.Denominator );
		if( FAILED( hr ) ) {
			PAR.Numerator = 1;
			PAR.Denominator = 1;
		}
		return PAR;
	}

	BOOL IsPanScanEnabled() // Defaults to FALSE
	{
		return (BOOL)MFGetAttributeUINT32( GetMediaType(), MF_MT_PAN_SCAN_ENABLED, FALSE );
	}

	// Returns (in this order) 
	// 1. The pan/scan region, only if pan/scan mode is enabled.
	// 2. The geometric aperture.
	// 3. The entire video area.
	HRESULT GetVideoDisplayArea( MFVideoArea *pArea )
	{
		if( pArea == NULL ) return E_POINTER;

		return ci::msw::GetVideoDisplayArea( GetMediaType(), pArea );
	}
};

//-----------------------------------------------------------------------------

class AudioType : public MediaType {
	friend class MediaType;

public:


	AudioType( IMFMediaType* pType = NULL ) : MediaType( pType )
	{
	}

	virtual HRESULT CreateEmptyType()
	{
		SafeRelease( m_pType );

		HRESULT hr = S_OK;

		hr = MediaType::CreateEmptyType();

		if( SUCCEEDED( hr ) ) {
			hr = SetMajorType( MFMediaType_Audio );
		}
		return hr;
	}

	// Query the average number of bytes per second.
	HRESULT GetAvgerageBytesPerSecond( UINT32 *pBytes )
	{
		if( pBytes == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, pBytes );
	}

	// Sets the average number of bytes per second.
	HRESULT SetAvgerageBytesPerSecond( UINT32 nBytes )
	{
		return GetMediaType()->SetUINT32( MF_MT_AUDIO_AVG_BYTES_PER_SECOND, nBytes );
	}

	// Query the bits per audio sample
	HRESULT GetBitsPerSample( UINT32 *pBits )
	{
		if( pBits == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, pBits );
	}

	// Sets the bits per audio sample
	HRESULT SetBitsPerSample( UINT32 nBits )
	{
		return GetMediaType()->SetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, nBits );
	}

	// Query the block aignment in bytes
	HRESULT GetBlockAlignment( UINT32 *pBytes )
	{
		if( pBytes == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AUDIO_BLOCK_ALIGNMENT, pBytes );
	}

	// Sets the block aignment in bytes
	HRESULT SetBlockAlignment( UINT32 nBytes )
	{
		return GetMediaType()->SetUINT32( MF_MT_AUDIO_BLOCK_ALIGNMENT, nBytes );
	}

	// Query the assignment of audio channels to speaker positions.
	HRESULT GetChannelMask( UINT32 *pMask )
	{
		if( pMask == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AUDIO_CHANNEL_MASK, pMask );
	}

	// Sets the assignment of audio channels to speaker positions.
	HRESULT SetChannelMask( UINT32 nMask )
	{
		return GetMediaType()->SetUINT32( MF_MT_AUDIO_CHANNEL_MASK, nMask );
	}

	// Query the number of audio samples per second (floating-point value).
	HRESULT GetFloatSamplesPerSecond( double *pfSampleRate )
	{
		if( pfSampleRate == NULL ) return E_POINTER;
		return GetMediaType()->GetDouble( MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND, pfSampleRate );
	}

	// Sets the number of audio samples per second (floating-point value).
	HRESULT SetFloatSamplesPerSecond( double fSampleRate )
	{
		return GetMediaType()->SetDouble( MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND, fSampleRate );
	}

	// Queries the number of audio channels
	HRESULT GetNumChannels( UINT32 *pnChannels )
	{
		if( pnChannels == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AUDIO_NUM_CHANNELS, pnChannels );
	}

	// Sets the number of audio channels
	HRESULT SetNumChannels( UINT32 nChannels )
	{
		return GetMediaType()->SetUINT32( MF_MT_AUDIO_NUM_CHANNELS, nChannels );
	}

	// Query the number of audio samples contained in one compressed block of audio data.
	HRESULT GetSamplesPerBlock( UINT32 *pnSamples )
	{
		if( pnSamples == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AUDIO_SAMPLES_PER_BLOCK, pnSamples );
	}

	// Sets the number of audio samples contained in one compressed block of audio data.
	HRESULT SetSamplesPerBlock( UINT32 nSamples )
	{
		return GetMediaType()->SetUINT32( MF_MT_AUDIO_SAMPLES_PER_BLOCK, nSamples );
	}

	// Query the number of audio samples per second as an integer value.
	HRESULT GetSamplesPerSecond( UINT32 *pnSampleRate )
	{
		if( pnSampleRate == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND, pnSampleRate );
	}

	// Set the number of audio samples per second as an integer value.
	HRESULT SetSamplesPerSecond( UINT32 nSampleRate )
	{
		return GetMediaType()->SetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND, nSampleRate );
	}

	// Query number of valid bits of audio data in each sample.
	HRESULT GetValidBitsPerSample( UINT32 *pnBits )
	{
		if( pnBits == NULL ) return E_POINTER;
		HRESULT hr = GetMediaType()->GetUINT32( MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, pnBits );
		if( !SUCCEEDED( hr ) ) {
			hr = GetBitsPerSample( pnBits );
		}
		return hr;
	}

	// Set the number of valid bits of audio data in each sample.
	HRESULT SetValidBitsPerSample( UINT32 nBits )
	{
		return GetMediaType()->SetUINT32( MF_MT_AUDIO_VALID_BITS_PER_SAMPLE, nBits );
	}


	// The following versions return zero if the relevant attribute is not present. 
	// This is useful for making quick comparisons betweeen media types. 

	UINT32 AvgerageBytesPerSecond()
	{
		return MFGetAttributeUINT32( GetMediaType(), MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 0 );
	}

	UINT32 BitsPerSample()
	{
		return MFGetAttributeUINT32( GetMediaType(), MF_MT_AUDIO_BITS_PER_SAMPLE, 0 );
	}

	UINT32 GetBlockAlignment()
	{
		return MFGetAttributeUINT32( GetMediaType(), MF_MT_AUDIO_BLOCK_ALIGNMENT, 0 );
	}

	double FloatSamplesPerSecond()
	{
		return MFGetAttributeDouble( GetMediaType(), MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND, 0.0 );
	}

	UINT32 NumChannels()
	{
		return MFGetAttributeUINT32( GetMediaType(), MF_MT_AUDIO_NUM_CHANNELS, 0 );
	}

	UINT32 SamplesPerSecond()
	{
		return MFGetAttributeUINT32( GetMediaType(), MF_MT_AUDIO_SAMPLES_PER_SECOND, 0 );
	}

};

//-----------------------------------------------------------------------------

class MPEGVideoType : public VideoType {
	friend class MediaType;

public:

	MPEGVideoType( IMFMediaType* pType = NULL ) : VideoType( pType )
	{
	}

	// Retrieves the MPEG sequence header for a video media type.
	HRESULT GetMpegSeqHeader( BYTE *pData, UINT32 cbSize )
	{
		if( pData == NULL ) return E_POINTER;
		return GetMediaType()->GetBlob( MF_MT_MPEG_SEQUENCE_HEADER, pData, cbSize, NULL );
	}

	// Sets the MPEG sequence header for a video media type.
	HRESULT SetMpegSeqHeader( const BYTE *pData, UINT32 cbSize )
	{
		if( pData == NULL ) return E_POINTER;
		return GetMediaType()->SetBlob( MF_MT_MPEG_SEQUENCE_HEADER, pData, cbSize );
	}

	// Retrieves the size of the MPEG sequence header.
	HRESULT GetMpegSeqHeaderSize( UINT32 *pcbSize )
	{
		if( pcbSize == NULL ) return E_POINTER;

		*pcbSize = 0;

		HRESULT hr = GetMediaType()->GetBlobSize( MF_MT_MPEG_SEQUENCE_HEADER, pcbSize );

		if( hr == MF_E_ATTRIBUTENOTFOUND ) {
			hr = S_OK;
		}
		return hr;
	}

	// Retrieve the group-of-pictures (GOP) start time code, for an MPEG-1 or MPEG-2 video media type.
	HRESULT GetStartTimeCode( UINT32 *pnTime )
	{
		if( pnTime == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_MPEG_START_TIME_CODE, pnTime );
	}

	// Sets the group-of-pictures (GOP) start time code, for an MPEG-1 or MPEG-2 video media type.
	HRESULT SetStartTimeCode( UINT32 nTime )
	{
		return GetMediaType()->SetUINT32( MF_MT_MPEG_START_TIME_CODE, nTime );
	}

	// Retrieves assorted flags for MPEG-2 video media type
	HRESULT GetMPEG2Flags( UINT32 *pnFlags )
	{
		if( pnFlags == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_MPEG2_FLAGS, pnFlags );
	}

	// Sets assorted flags for MPEG-2 video media type
	HRESULT SetMPEG2Flags( UINT32 nFlags )
	{
		return GetMediaType()->SetUINT32( MF_MT_MPEG2_FLAGS, nFlags );
	}

	// Retrieves the MPEG-2 level in a video media type.
	HRESULT GetMPEG2Level( UINT32 *pLevel )
	{
		if( pLevel == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_MPEG2_LEVEL, pLevel );
	}

	// Sets the MPEG-2 level in a video media type.
	HRESULT SetMPEG2Level( UINT32 nLevel )
	{
		return GetMediaType()->SetUINT32( MF_MT_MPEG2_LEVEL, nLevel );
	}

	// Retrieves the MPEG-2 profile in a video media type
	HRESULT GetMPEG2Profile( UINT32 *pProfile )
	{
		if( pProfile == NULL ) return E_POINTER;
		return GetMediaType()->GetUINT32( MF_MT_MPEG2_PROFILE, pProfile );
	}

	// Sets the MPEG-2 profile in a video media type
	HRESULT SetMPEG2Profile( UINT32 nProfile )
	{
		return GetMediaType()->SetUINT32( MF_MT_MPEG2_PROFILE, nProfile );
	}
};

//-----------------------------------------------------------------------------

template<class T>
class AsyncCallback : public IMFAsyncCallback {
public:
	typedef HRESULT( T::*InvokeFn )( IMFAsyncResult *pAsyncResult );

	AsyncCallback( T *pParent, InvokeFn fn ) : m_pParent( pParent ), m_pInvokeFn( fn )
	{
	}

	// IUnknown
	STDMETHODIMP QueryInterface( REFIID iid, void** ppv )
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
	STDMETHODIMP_( ULONG ) AddRef()
	{
		// Delegate to parent class.
		return m_pParent->AddRef();
	}
	STDMETHODIMP_( ULONG ) Release()
	{
		// Delegate to parent class.
		return m_pParent->Release();
	}


	// IMFAsyncCallback methods
	STDMETHODIMP GetParameters( DWORD*, DWORD* )
	{
		// Implementation of this method is optional.
		return E_NOTIMPL;
	}

	STDMETHODIMP Invoke( IMFAsyncResult* pAsyncResult )
	{
		return ( m_pParent->*m_pInvokeFn )( pAsyncResult );
	}

	T *m_pParent;
	InvokeFn m_pInvokeFn;
};

} // namespace msw
} // namespace cinder