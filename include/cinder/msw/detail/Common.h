#pragma once

#include <windows.h>
//#include <float.h> // for FLT_MAX
//#include <tchar.h>
//#include <math.h>
//#include <strsafe.h>
//#include <mmsystem.h>
//#include <mfapi.h>
//#include <mfidl.h>
//#include <evr.h>
//#include <mferror.h>
//#include <d3d9.h>
//#include <d3d11.h>
//#include <dxgi1_2.h>
//#include <dcomp.h>
//#include <wmcodecdsp.h> // for MEDIASUBTYPE_V216

//// Include these libraries.
//#pragma comment(lib, "mf.lib")
//#pragma comment(lib, "mfplat.lib")
//#pragma comment(lib, "mfuuid.lib")
//#pragma comment(lib, "d3d11.lib")
//
//#pragma comment(lib, "winmm.lib") // for timeBeginPeriod and timeEndPeriod
////#pragma comment(lib,"d3d9.lib")
////#pragma comment(lib,"dxva2.lib")
////#pragma comment (lib,"evr.lib")
////#pragma comment (lib,"dcomp.lib")
//#pragma comment (lib,"uuid.lib")

namespace cinder {
namespace msw {
namespace detail {

/*
class CBase {
public:

	static long GetObjectCount( void )
	{
		return s_lObjectCount;
	}

protected:

	CBase( void )
	{
		InterlockedIncrement( &s_lObjectCount );
	}

	~CBase( void )
	{
		InterlockedDecrement( &s_lObjectCount );
	}

private:

	static volatile long s_lObjectCount;
};
*/

/*
class CCritSec {
public:

	CCritSec( void ) :
		m_cs()
	{
		InitializeCriticalSection( &m_cs );
	}

	~CCritSec( void )
	{
		DeleteCriticalSection( &m_cs );
	}

	_Acquires_lock_( this->m_cs )
		void Lock( void )
	{
		EnterCriticalSection( &m_cs );
	}

	_Releases_lock_( this->m_cs )
		void Unlock( void )
	{
		LeaveCriticalSection( &m_cs );
	}

private:

	CRITICAL_SECTION m_cs;
};

class CAutoLock {
public:
	_Acquires_lock_( this->m_pLock->m_cs )
		CAutoLock( CCritSec* pLock ) :
		m_pLock( pLock )
	{
		m_pLock->Lock();
	}

	_Releases_lock_( this->m_pLock->m_cs )
		~CAutoLock( void )
	{
		m_pLock->Unlock();
	}

private:

	CCritSec* m_pLock;
};
*/

} // namespace detail
} // namespace msw
} // namespace cinder