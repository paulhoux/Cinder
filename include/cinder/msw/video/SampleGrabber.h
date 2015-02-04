#pragma once

#include "cinder/Cinder.h"
#include "cinder/Log.h"

#if defined( CINDER_MSW )

#include <dshow.h>

// Due to a missing qedit.h in recent Platform SDKs, we've replicated the relevant contents here
// #include <qedit.h>
MIDL_INTERFACE( "0579154A-2B53-4994-B0D0-E773148EFF85" )
ISampleGrabberCB : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE SampleCB(
		double SampleTime,
		IMediaSample *pSample ) = 0;

	virtual HRESULT STDMETHODCALLTYPE BufferCB(
		double SampleTime,
		BYTE *pBuffer,
		long BufferLen ) = 0;

};

MIDL_INTERFACE( "6B652FFF-11FE-4fce-92AD-0266B5D7C78F" )
ISampleGrabber : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE SetOneShot(
		BOOL OneShot ) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetMediaType(
		const AM_MEDIA_TYPE *pType ) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(
		AM_MEDIA_TYPE *pType ) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(
		BOOL BufferThem ) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(
		/* [out][in] */ long *pBufferSize,
		/* [out] */ long *pBuffer ) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(
		/* [retval][out] */ IMediaSample **ppSample ) = 0;

	virtual HRESULT STDMETHODCALLTYPE SetCallback(
		ISampleGrabberCB *pCallback,
		long WhichMethodToCallback ) = 0;

};

EXTERN_C const CLSID CLSID_SampleGrabber;
EXTERN_C const IID IID_ISampleGrabber;
EXTERN_C const CLSID CLSID_NullRenderer;

//////////////////////////////  CALLBACK  ////////////////////////////////

//Callback class
class SampleGrabberCallback : public ISampleGrabberCB {
public:

	//------------------------------------------------
	SampleGrabberCallback()
		: mBuffer( NULL ), mBufferSize( 0 ), mFreezeCheck( 0 ), mNewFrame( false ), mLocked( false )
	{
		InitializeCriticalSection( &mLock );
		mEvent = CreateEvent( NULL, true, false, NULL );
	}


	//------------------------------------------------
	~SampleGrabberCallback()
	{
		if( mLocked ) {
			CI_LOG_E( "Pixels are still locked. Call UnlockPixels() prior to destructing the Sample Grabber callback!" );
			UnlockPixels();
		}

		CloseHandle( mEvent );
		DeleteCriticalSection( &mLock );

		if( mBuffer ) {
			delete[] mBuffer;
			mBuffer = NULL;
		}
	}

	BOOL HasNewFrame() { return mNewFrame; }

	ULONG FreezeCheck()
	{
		return InterlockedIncrement( &mFreezeCheck );
	}


	//------------------------------------------------
	bool SetupBuffer( int width, int height, int bytesPerPixel )
	{
		if( mBuffer ) {
			return false;
		}
		else {
			mWidth = width;
			mHeight = height;
			mBufferSize = width * height * bytesPerPixel;
			mBuffer = new unsigned char[mBufferSize];

			mNewFrame = false;
		}
		return true;
	}


	//------------------------------------------------
	STDMETHODIMP_( ULONG ) AddRef() { return 1; }
	STDMETHODIMP_( ULONG ) Release() { return 2; }


	//------------------------------------------------
	STDMETHODIMP QueryInterface( REFIID riid, void **ppvObject )
	{
		*ppvObject = static_cast<ISampleGrabberCB*>( this );
		return S_OK;
	}


	//This method is meant to have less overhead
	//------------------------------------------------
	STDMETHODIMP SampleCB( double Time, IMediaSample *pSample )
	{
		if( WaitForSingleObject( mEvent, 0 ) == WAIT_OBJECT_0 ) return S_OK;

		unsigned char *ptrBuffer = nullptr;
		HRESULT hr = pSample->GetPointer( &ptrBuffer );

		if( hr == S_OK ) {
			long bytes = pSample->GetActualDataLength();
			if( bytes == mBufferSize ) {
				EnterCriticalSection( &mLock );
				memcpy( mBuffer, ptrBuffer, bytes );
				mNewFrame = true;
				mFreezeCheck = 1;
				LeaveCriticalSection( &mLock );
				SetEvent( mEvent );
			}
			else {
				CI_LOG_E( "Buffer sizes do not match" );
			}
		}

		return S_OK;
	}


	//This method is meant to have more overhead
	STDMETHODIMP BufferCB( double Time, BYTE *pBuffer, long BufferLen )
	{
		return E_NOTIMPL;
	}

	BOOL LockPixels( unsigned char *pDest )
	{
		DWORD result = WaitForSingleObject( mEvent, 1000 );
		if( result != WAIT_OBJECT_0 ) return FALSE;

		// Buffer must exist unlocked.
		if( !mBuffer )
			return FALSE;

		if( mLocked )
			return FALSE;

		// Destination point should be NULL.
		if( pDest )
			return FALSE;

		EnterCriticalSection( &mLock );
		mLocked = true;

		pDest = mBuffer;

		return TRUE;
	}

	BOOL UnlockPixels()
	{
		if( mLocked ) {
			LeaveCriticalSection( &mLock );
			mNewFrame = false;
			mLocked = false;
			ResetEvent( mEvent );

			return TRUE;
		}

		return FALSE;
	}

	BOOL CopyPixels( unsigned char *pDest )
	{
		DWORD result = WaitForSingleObject( mEvent, 1000 );
		if( result != WAIT_OBJECT_0 ) return FALSE;

		// Buffer must exist unlocked.
		if( !mBuffer )
			return FALSE;

		if( mLocked )
			return FALSE;

		// Destination pointer should be non-NULL
		if( !pDest )
			return FALSE;

		EnterCriticalSection( &mLock );
		mLocked = true;

		memcpy( pDest, mBuffer, mBufferSize );

		LeaveCriticalSection( &mLock );
		mNewFrame = false;
		mLocked = false;
		ResetEvent( mEvent );

		return TRUE;
	}

private:
	ULONG mFreezeCheck;
	bool mNewFrame;
	bool mLocked;

	int mWidth;
	int mHeight;

	size_t   mBufferSize;
	uint8_t *mBuffer;

	CRITICAL_SECTION mLock;
	HANDLE mEvent;
};

#endif // CINDER_MSW