#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/CinderMswCom.h"
#include "cinder/evr/IPlayer.h"
#include "MediaFoundationFramework.h"

namespace cinder {
namespace msw {
namespace video {

//! Forward declarations.
class __declspec( uuid( "9A6E430D-27EE-4DBB-9A7F-7782EA4036A0" ) ) EVRCustomPresenter;

class MediaFoundationPlayer : public IMFAsyncCallback, public IPlayer {
public:
	typedef enum PlayerState {
		Closed = 0,
		Ready,
		OpenPending,
		Started,
		Paused,
		Stopped,
		Closing
	};

public:
	MediaFoundationPlayer( HRESULT &hr, HWND hwnd );
	~MediaFoundationPlayer();

protected:
	// IUnknown methods
	STDMETHODIMP QueryInterface( REFIID iid, void** ppv ) override;
	STDMETHODIMP_( ULONG ) AddRef() override;
	STDMETHODIMP_( ULONG ) Release() override;

	// IMFAsyncCallback methods
	STDMETHODIMP  GetParameters( DWORD*, DWORD* ) override { return E_NOTIMPL; } // Implementation of this method is optional.
	STDMETHODIMP  Invoke( IMFAsyncResult* pAsyncResult ) override;

	// IPlayer methods
	HRESULT OpenFile( PCWSTR pszFileName ) override;

	HRESULT Play() override;
	HRESULT Pause() override;
	HRESULT Stop() override { return S_OK; }

	HRESULT HandleEvent( UINT_PTR pEventPtr ) override;

	UINT32  GetWidth() override { return mWidth; }
	UINT32  GetHeight() override { return mHeight; }

	//
	HRESULT CreateSession();
	HRESULT CloseSession();
	HRESULT CreatePartialTopology( IMFPresentationDescriptor *pPD );// { return E_NOTIMPL; }
	HRESULT SetMediaInfo( IMFPresentationDescriptor *pPD );
	
	HRESULT HandleSessionTopologySetEvent( IMFMediaEvent *pEvent );
	HRESULT HandleSessionTopologyStatusEvent( IMFMediaEvent *pEvent );
	HRESULT HandleEndOfPresentationEvent( IMFMediaEvent *pEvent );
	HRESULT HandleNewPresentationEvent( IMFMediaEvent *pEvent );
	HRESULT HandleSessionEvent( IMFMediaEvent *pEvent, MediaEventType eventType );

	static HRESULT CreateMediaSource( LPCWSTR pUrl, IMFMediaSource **ppSource );
	static HRESULT CreatePlaybackTopology( IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, HWND hVideoWnd, IMFTopology **ppTopology, IMFVideoPresenter *pVideoPresenter );
	static HRESULT CreateMediaSinkActivate( IMFStreamDescriptor *pSourceSD, HWND hVideoWindow, IMFActivate **ppActivate, IMFVideoPresenter *pVideoPresenter, IMFMediaSink **ppMediaSink );
	static HRESULT AddSourceNode( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, IMFStreamDescriptor *pSD, IMFTopologyNode **ppNode );
	static HRESULT AddOutputNode( IMFTopology *pTopology, IMFStreamSink *pStreamSink, IMFTopologyNode **ppNode );
	static HRESULT AddOutputNode( IMFTopology *pTopology, IMFActivate *pActivate, DWORD dwId, IMFTopologyNode **ppNode );
	static HRESULT AddBranchToPartialTopology( IMFTopology *pTopology, IMFMediaSource *pSource, IMFPresentationDescriptor *pPD, DWORD iStream, HWND hVideoWnd, IMFVideoPresenter *pVideoPresenter );

protected:
	bool                     mIsInitialized;

	uint32_t                 mWidth, mHeight;
	float                    mCurrentVolume;

	IMFMediaSession*         mMediaSessionPtr;
	IMFSequencerSource*      mSequencerSourcePtr;
	IMFMediaSource*          mMediaSourcePtr;
	IMFVideoDisplayControl*  mVideoDisplayControlPtr;
	IMFAudioStreamVolume*    mAudioStreamVolumePtr;

	EVRCustomPresenter*      mPresenterPtr;

	//MFSequencerElementId     mPreviousTopologyId;
	
	HWND                     mHwnd;
	PlayerState              mState;
	HANDLE                   mOpenEventHandle;
	HANDLE                   mCloseEventHandle;

	volatile long            mRefCount;
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW