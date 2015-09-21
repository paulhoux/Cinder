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

	virtual HRESULT         InitializeDisplaySystem( HWND hwnd );

	virtual HRESULT         InitializeXclModeDisplaySystem( IUnknown* lpDD, UINT* pAdapterID ) { return E_NOTIMPL; }

	virtual void            TerminateDisplaySystem();
	AMDDrawMonitorInfo*    FindMonitor( HMONITOR hMon );
	HRESULT                 MatchGUID( UINT uDevID, DWORD* pdwMatchID );


	AMDDrawMonitorInfo&    operator[]( int i )
	{
		return m_DDMon[i];
	}
	DWORD                   Count() const
	{
		return m_dwNumMonitors;
	}

	static BOOL CALLBACK    MonitorEnumProc( HMONITOR hMon, HDC hDC, LPRECT pRect, LPARAM dwData );

	virtual BOOL            InitMonitor( HMONITOR hMon, BOOL fXclMode );
protected:
	BOOL                    GetAMDDrawMonitorInfo( UINT uDevID, AMDDrawMonitorInfo* lpmi, HMONITOR hm );

	virtual void            TermDDrawMonitorInfo( AMDDrawMonitorInfo* pmi );

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



