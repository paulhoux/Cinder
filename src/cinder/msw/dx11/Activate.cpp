#include "cinder/msw/dx11/Activate.h"
#include "cinder/msw/dx11/DX11VideoRenderer.h"

namespace cinder {
namespace msw {

volatile long CBase::s_lObjectCount = 0;

HRESULT CActivate::CreateInstance(HWND hwnd, IMFActivate** ppActivate)
{
    if (ppActivate == NULL)
    {
        return E_POINTER;
    }

    CActivate* pActivate = new CActivate();
    if (pActivate == NULL)
    {
        return E_OUTOFMEMORY;
    }

    pActivate->AddRef();

    HRESULT hr = S_OK;

    do
    {
        hr = pActivate->Initialize();
        if (FAILED(hr))
        {
            break;
        }

        hr = pActivate->QueryInterface(IID_PPV_ARGS(ppActivate));
        if (FAILED(hr))
        {
            break;
        }

        pActivate->m_hwnd = hwnd;
    }
    while (FALSE);

    SafeRelease(pActivate);

    return hr;
}

// IUnknown
ULONG CActivate::AddRef(void)
{
    return InterlockedIncrement(&m_lRefCount);
}

// IUnknown
HRESULT CActivate::QueryInterface(REFIID iid, __RPC__deref_out _Result_nullonfailure_ void** ppv)
{
    if (!ppv)
    {
        return E_POINTER;
    }
    if (iid == IID_IUnknown)
    {
        *ppv = static_cast<IUnknown*>(static_cast<IMFActivate*>(this));
    }
    else if (iid == __uuidof(IMFActivate))
    {
        *ppv = static_cast<IMFActivate*>(this);
    }
    else if (iid == __uuidof(IPersistStream))
    {
        *ppv = static_cast<IPersistStream*>(this);
    }
    else if (iid == __uuidof(IPersist))
    {
        *ppv = static_cast<IPersist*>(this);
    }
    else if (iid == __uuidof(IMFAttributes))
    {
        *ppv = static_cast<IMFAttributes*>(this);
    }
    else
    {
        *ppv = NULL;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

// IUnknown
ULONG CActivate::Release(void)
{
    ULONG lRefCount = InterlockedDecrement(&m_lRefCount);
    if (lRefCount == 0)
    {
        delete this;
    }
    return lRefCount;
}

// IMFActivate
HRESULT CActivate::ActivateObject(__RPC__in REFIID riid, __RPC__deref_out_opt void** ppvObject)
{
    HRESULT hr = S_OK;
    IMFGetService* pSinkGetService = NULL;
    IMFVideoDisplayControl* pSinkVideoDisplayControl = NULL;

    do
    {
        if (m_pMediaSink == NULL)
        {
            hr = CMediaSink::CreateInstance(IID_PPV_ARGS(&m_pMediaSink));
            if (FAILED(hr))
            {
                break;
            }

            hr = m_pMediaSink->QueryInterface(IID_PPV_ARGS(&pSinkGetService));
            if (FAILED(hr))
            {
                break;
            }

            hr = pSinkGetService->GetService(MR_VIDEO_RENDER_SERVICE, IID_PPV_ARGS(&pSinkVideoDisplayControl));
            if (FAILED(hr))
            {
                break;
            }

            hr = pSinkVideoDisplayControl->SetVideoWindow(m_hwnd);
            if (FAILED(hr))
            {
                break;
            }
        }

        hr = m_pMediaSink->QueryInterface(riid, ppvObject);
        if (FAILED(hr))
        {
            break;
        }
    }
    while (FALSE);

    SafeRelease(pSinkGetService);
    SafeRelease(pSinkVideoDisplayControl);

    return hr;
}

// IMFActivate
HRESULT CActivate::DetachObject(void)
{
    SafeRelease(m_pMediaSink);

    return S_OK;
}

// IMFActivate
HRESULT CActivate::ShutdownObject(void)
{
    if (m_pMediaSink != NULL)
    {
        m_pMediaSink->Shutdown();
        SafeRelease(m_pMediaSink);
    }

    return S_OK;
}

// IPersistStream
HRESULT CActivate::GetSizeMax(__RPC__out ULARGE_INTEGER* pcbSize)
{
    return E_NOTIMPL;
}

// IPersistStream
HRESULT CActivate::IsDirty(void)
{
    return E_NOTIMPL;
}

// IPersistStream
HRESULT CActivate::Load(__RPC__in_opt IStream* pStream)
{
    return E_NOTIMPL;
}

// IPersistStream
HRESULT CActivate::Save(__RPC__in_opt IStream* pStream, BOOL bClearDirty)
{
    return E_NOTIMPL;
}

// IPersist
HRESULT CActivate::GetClassID(__RPC__out CLSID* pClassID)
{
    if (pClassID == NULL)
    {
        return E_POINTER;
    }
    //*pClassID = CLSID_DX11VideoRendererActivate;
    //return S_OK;
	return E_FAIL;
}

// ctor
CActivate::CActivate(void) :
    m_lRefCount(0),
    m_pMediaSink(NULL),
    m_hwnd(NULL)
{
}

// dtor
CActivate::~CActivate(void)
{
    SafeRelease(m_pMediaSink);
}

}}
