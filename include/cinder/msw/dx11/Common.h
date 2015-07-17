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
//#include "cinder/msw/dx11/DX11VideoRenderer.h"
#include "cinder/msw/dx11/DoubleLinkedList.h"
#include "cinder/msw/dx11/StaticAsyncCallback.h"

// Include these libraries.
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")

#pragma comment(lib, "winmm.lib") // for timeBeginPeriod and timeEndPeriod
//#pragma comment(lib,"d3d9.lib")
//#pragma comment(lib,"dxva2.lib")
//#pragma comment (lib,"evr.lib")
//#pragma comment (lib,"dcomp.lib")
#pragma comment (lib,"uuid.lib")

#ifndef LOWORD
#define LOWORD(_dw)     ((WORD)(((DWORD_PTR)(_dw)) & 0xffff))
#endif // !LOWORD

#ifndef HIWORD
#define HIWORD(_dw)     ((WORD)((((DWORD_PTR)(_dw)) >> 16) & 0xffff))
#endif // !HIWORD

#ifndef LODWORD
#define LODWORD(_qw)    ((DWORD)(_qw))
#endif // !LODWORD

#ifndef HIDWORD
#define HIDWORD(_qw)    ((DWORD)(((_qw) >> 32) & 0xffffffff))
#endif // !HIDWORD

#ifndef BREAK_ON_FAIL
#define BREAK_ON_FAIL(value)          if( FAILED( value ) ) break;
//#define BREAK_ON_FAIL(value) if( FAILED(value) ) { CI_LOG_E("Fail:" << value); break; }
#endif // !BREAK_ON_FAIL

#ifndef BREAK_ON_NULL
#define BREAK_ON_NULL(value, result)  if( value == NULL ) { hr = result; break; }
#endif // !BREAK_ON_NULL

#ifndef BREAK_IF_FALSE
#define BREAK_IF_FALSE(test, result)  if( !(test) ) { hr = result; break; }
#endif // !BREAK_IF_FALSE

#undef COMPILE_ASSERT
#define COMPILE_ASSERT(expr, msg)     static_assert(expr, #msg)

DEFINE_GUID(CLSID_VideoProcessorMFT, 0x88753b26, 0x5b24, 0x49bd, 0xb2, 0xe7, 0xc, 0x44, 0x5c, 0x78, 0xc9, 0x82);

// MF_XVP_PLAYBACK_MODE
// Data type: UINT32 (treat as BOOL)
// If this attribute is TRUE, the XVP will run in playback mode where it will:
//      1) Allow caller to allocate D3D output samples
//      2) Not perform FRC
//      3) Allow last frame regeneration (repaint)
// This attribute should be set on the transform's attrbiute store prior to setting the input type.
DEFINE_GUID(MF_XVP_PLAYBACK_MODE, 0x3c5d293f, 0xad67, 0x4e29, 0xaf, 0x12, 0xcf, 0x3e, 0x23, 0x8a, 0xcc, 0xe9);

namespace cinder { namespace msw
{
    inline void SafeCloseHandle(HANDLE& h)
    {
        if (h != NULL)
        {
            CloseHandle(h);
            h = NULL;
        }
    }

    template <class T> inline void SafeDelete(T*& pT)
    {
        delete pT;
        pT = NULL;
    }

    template <class T> inline void SafeDeleteArray(T*& pT)
    {
        delete[] pT;
        pT = NULL;
    }

    template <class T> inline void SafeRelease(T*& pT)
    {
        if (pT != NULL)
        {
            pT->Release();
            pT = NULL;
        }
    }

    template <class T> inline double TicksToMilliseconds(const T& t)
    {
        return t / 10000.0;
    }

    template <class T> inline T MillisecondsToTicks(const T& t)
    {
        return t * 10000;
    }

    // returns the greatest common divisor of A and B
    inline int gcd(int A, int B)
    {
        int Temp;

        if (A < B)
        {
            Temp = A;
            A = B;
            B = Temp;
        }

        while (B != 0)
        {
            Temp = A % B;
            A = B;
            B = Temp;
        }

        return A;
    }

    // Convert a fixed-point to a float.
    inline float MFOffsetToFloat(const MFOffset& offset)
    {
        return (float)offset.value + ((float)offset.value / 65536.0f);
    }

    inline RECT MFVideoAreaToRect(const MFVideoArea area)
    {
        float left = MFOffsetToFloat(area.OffsetX);
        float top = MFOffsetToFloat(area.OffsetY);

        RECT rc =
        {
            int( left + 0.5f ),
            int( top + 0.5f ),
            int( left + area.Area.cx + 0.5f ),
            int( top + area.Area.cy + 0.5f )
        };

        return rc;
    }

    inline MFOffset MakeOffset(float v)
    {
        MFOffset offset;
        offset.value = short(v);
        offset.fract = WORD(65536 * (v-offset.value));
        return offset;
    }

    inline MFVideoArea MakeArea(float x, float y, DWORD width, DWORD height)
    {
        MFVideoArea area;
        area.OffsetX = MakeOffset(x);
        area.OffsetY = MakeOffset(y);
        area.Area.cx = width;
        area.Area.cy = height;
        return area;
    }

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

    class CBase
    {
    public:

        static long GetObjectCount(void)
        {
            return s_lObjectCount;
        }

    protected:

        CBase(void)
        {
            InterlockedIncrement(&s_lObjectCount);
        }

        ~CBase(void)
        {
            InterlockedDecrement(&s_lObjectCount);
        }

    private:

        static volatile long s_lObjectCount;
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
    class CAsyncCallback : public IMFAsyncCallback
    {
    public:

        typedef HRESULT (T::*InvokeFn)(IMFAsyncResult* pAsyncResult);

        CAsyncCallback(T* pParent, InvokeFn fn) :
            m_pParent(pParent),
            m_pInvokeFn(fn)
        {
        }

        // IUnknown
        STDMETHODIMP_(ULONG) AddRef(void)
        {
            // Delegate to parent class.
            return m_pParent->AddRef();
        }

        STDMETHODIMP_(ULONG) Release(void)
        {
            // Delegate to parent class.
            return m_pParent->Release();
        }

        STDMETHODIMP QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
        {
            if (!ppv)
            {
                return E_POINTER;
            }
            if (iid == __uuidof(IUnknown))
            {
                *ppv = static_cast<IUnknown*>(static_cast<IMFAsyncCallback*>(this));
            }
            else if (iid == __uuidof(IMFAsyncCallback))
            {
                *ppv = static_cast<IMFAsyncCallback*>(this);
            }
            else
            {
                *ppv = NULL;
                return E_NOINTERFACE;
            }
            AddRef();
            return S_OK;
        }

        // IMFAsyncCallback methods
        STDMETHODIMP GetParameters(__RPC__out DWORD* pdwFlags, __RPC__out DWORD* pdwQueue)
        {
            // Implementation of this method is optional.
            return E_NOTIMPL;
        }

        STDMETHODIMP Invoke(__RPC__in_opt IMFAsyncResult* pAsyncResult)
        {
            return (m_pParent->*m_pInvokeFn)(pAsyncResult);
        }

    private:

        T* m_pParent;
        InvokeFn m_pInvokeFn;
    };

    //-----------------------------------------------------------------------------
    // ThreadSafeQueue template
    // Thread-safe queue of COM interface pointers.
    //
    // T: COM interface type.
    //
    // This class is used by the scheduler.
    //
    // Note: This class uses a critical section to protect the state of the queue.
    // With a little work, the scheduler could probably use a lock-free queue.
    //-----------------------------------------------------------------------------

    template <class T>
    class ThreadSafeQueue
    {
    public:

        ThreadSafeQueue(void)
        {
            InitializeCriticalSection(&m_lock);
        }

        virtual ~ThreadSafeQueue(void)
        {
            DeleteCriticalSection(&m_lock);
        }

        HRESULT Queue(T* p)
        {
            EnterCriticalSection(&m_lock);
            HRESULT hr = m_list.InsertBack(p);
            LeaveCriticalSection(&m_lock);
            return hr;
        }

        HRESULT Dequeue(T** pp)
        {
            EnterCriticalSection(&m_lock);
            HRESULT hr = S_OK;
            if (m_list.IsEmpty())
            {
                *pp = NULL;
                hr = S_FALSE;
            }
            else
            {
                hr = m_list.RemoveFront(pp);
            }
            LeaveCriticalSection(&m_lock);
            return hr;
        }

        HRESULT PutBack(T* p)
        {
            EnterCriticalSection(&m_lock);
            HRESULT hr =  m_list.InsertFront(p);
            LeaveCriticalSection(&m_lock);
            return hr;
        }

        DWORD GetCount(void)
        {
            EnterCriticalSection(&m_lock);
            DWORD nCount =  m_list.GetCount();
            LeaveCriticalSection(&m_lock);
            return nCount;
        }

        void Clear(void)
        {
            EnterCriticalSection(&m_lock);
            m_list.Clear();
            LeaveCriticalSection(&m_lock);
        }

    private:

        CRITICAL_SECTION    m_lock;
        ComPtrListEx<T>     m_list;
    };

    class CCritSec
    {
    public:

        CCritSec(void) :
            m_cs()
        {
            InitializeCriticalSection(&m_cs);
        }

        ~CCritSec(void)
        {
            DeleteCriticalSection(&m_cs);
        }

        _Acquires_lock_(this->m_cs)
        void Lock(void)
        {
            EnterCriticalSection(&m_cs);
        }

        _Releases_lock_(this->m_cs)
        void Unlock(void)
        {
            LeaveCriticalSection(&m_cs);
        }

    private:

        CRITICAL_SECTION m_cs;
    };

    class CAutoLock
    {
    public:

        _Acquires_lock_(this->m_pLock->m_cs)
        CAutoLock(CCritSec* pLock) :
            m_pLock(pLock)
        {
            m_pLock->Lock();
        }

        _Releases_lock_(this->m_pLock->m_cs)
        ~CAutoLock(void)
        {
            m_pLock->Unlock();
        }

    private:

        CCritSec* m_pLock;
    };
} }
