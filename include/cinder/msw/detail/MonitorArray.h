/******************************Module*Header*******************************\
* Module Name: display.h
*
*
*
*
* Created: Mon 01/24/2000
* Author:  Stephen Estrop [StEstrop]
*
* Copyright (c) Microsoft Corporation
\**************************************************************************/

#pragma once

#include <minwindef.h>
#include <winerror.h>

namespace cinder {
namespace msw {
namespace detail {

#define AMDDRAWMONITORINFO_PRIMARY_MONITOR          0x0001

struct AMDDrawMonitorInfo {
	UINT uDevID;
	HMONITOR hMon;
	TCHAR szDevice[32];
	LARGE_INTEGER liDriverVersion;
	DWORD dwVendorId;
	DWORD dwDeviceId;
	DWORD dwSubSysId;
	DWORD dwRevision;
	SIZE physMonDim;
	DWORD dwRefreshRate;
	IUnknown *pDD;
};

#define EVR_MAX_MONITORS 16

class MonitorArray {
public:
	MonitorArray();
	virtual ~MonitorArray();

	virtual HRESULT         InitializeDisplaySystem( _In_ HWND hwnd );

	virtual HRESULT         InitializeXclModeDisplaySystem( IUnknown* lpDD, UINT* pAdapterID ) { return E_NOTIMPL; }

	virtual void            TerminateDisplaySystem();
	AMDDrawMonitorInfo*     FindMonitor( _In_ HMONITOR hMon );
	HRESULT                 MatchGUID( UINT uDevID, _Out_ DWORD* pdwMatchID );


	AMDDrawMonitorInfo&    operator[]( int i )
	{
		return m_DDMon[i];
	}
	DWORD                   Count() const
	{
		return m_dwNumMonitors;
	}

	static BOOL CALLBACK    MonitorEnumProc( _In_ HMONITOR hMon, _In_opt_ HDC hDC, _In_ LPRECT pRect, LPARAM dwData );

	virtual BOOL            InitMonitor( _In_ HMONITOR hMon, BOOL fXclMode );
protected:
	BOOL                    GetAMDDrawMonitorInfo( UINT uDevID, _Out_ AMDDrawMonitorInfo* lpmi, _In_ HMONITOR hm );

	virtual void            TermDDrawMonitorInfo( _Inout_ AMDDrawMonitorInfo* pmi );

	DWORD                   m_dwNumMonitors;
	AMDDrawMonitorInfo     m_DDMon[EVR_MAX_MONITORS];
};

typedef struct {
	HWND hwnd;
	MonitorArray* pMonArray;
} MonitorEnumProcInfo;

} // namespace detail
} // namespace msw
} // namespace cinder



