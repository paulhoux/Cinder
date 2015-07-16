#include "cinder/msw/dx11/Marker.h"

namespace cinder {
namespace msw {

//////////////////////
// CMarker class
// Holds information from IMFStreamSink::PlaceMarker
//

CMarker::CMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType) :
    m_nRefCount(1),
    m_eMarkerType(eMarkerType)
{
    PropVariantInit(&m_varMarkerValue);
    PropVariantInit(&m_varContextValue);
}

CMarker::~CMarker(void)
{
    assert(m_nRefCount == 0);

    PropVariantClear(&m_varMarkerValue);
    PropVariantClear(&m_varContextValue);
}

/* static */
HRESULT CMarker::Create(
    MFSTREAMSINK_MARKER_TYPE eMarkerType,
    const PROPVARIANT* pvarMarkerValue,     // Can be NULL.
    const PROPVARIANT* pvarContextValue,    // Can be NULL.
    IMarker** ppMarker
    )
{
    if (ppMarker == NULL)
    {
        return E_POINTER;
    }

    HRESULT hr = S_OK;
    CMarker* pMarker = new CMarker(eMarkerType);

    if (pMarker == NULL)
    {
        hr = E_OUTOFMEMORY;
    }

    // Copy the marker data.
    if (SUCCEEDED(hr))
    {
        if (pvarMarkerValue)
        {
            hr = PropVariantCopy(&pMarker->m_varMarkerValue, pvarMarkerValue);
        }
    }

    if (SUCCEEDED(hr))
    {
        if (pvarContextValue)
        {
            hr = PropVariantCopy(&pMarker->m_varContextValue, pvarContextValue);
        }
    }

    if (SUCCEEDED(hr))
    {
        *ppMarker = pMarker;
        (*ppMarker)->AddRef();
    }

    SafeRelease(pMarker);

    return hr;
}

// IUnknown methods.

ULONG CMarker::AddRef(void)
{
    return InterlockedIncrement(&m_nRefCount);
}

ULONG CMarker::Release(void)
{
    ULONG uCount = InterlockedDecrement(&m_nRefCount);
    if (uCount == 0)
    {
        delete this;
    }
    // For thread safety, return a temporary variable.
    return uCount;
}

HRESULT CMarker::QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
{
    if (!ppv)
    {
        return E_POINTER;
    }
    if (iid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(this);
    }
    else if (iid == __uuidof(IMarker))
    {
        *ppv = static_cast<IMarker*>(this);
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

// IMarker methods
HRESULT CMarker::GetMarkerType(MFSTREAMSINK_MARKER_TYPE* pType)
{
    if (pType == NULL)
    {
        return E_POINTER;
    }

    *pType = m_eMarkerType;
    return S_OK;
}

HRESULT CMarker::GetMarkerValue(PROPVARIANT* pvar)
{
    if (pvar == NULL)
    {
        return E_POINTER;
    }
    return PropVariantCopy(pvar, &m_varMarkerValue);

}
HRESULT CMarker::GetContext(PROPVARIANT* pvar)
{
    if (pvar == NULL)
    {
        return E_POINTER;
    }
    return PropVariantCopy(pvar, &m_varContextValue);
}

}}
