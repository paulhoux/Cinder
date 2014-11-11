#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMswCom.h"

namespace cinder {
namespace msw {
namespace video {

//! Definitions
const UINT WM_PLAYER_EVENT = WM_APP + 1;

class IPlayer : public IUnknown {
public:
	IPlayer() {}
	virtual ~IPlayer() {}

	virtual HRESULT OpenFile( PCWSTR pszFileName ) = 0;

	virtual HRESULT Play() = 0;
	virtual HRESULT Pause() = 0;
	virtual HRESULT Stop() = 0;

	virtual HRESULT HandleEvent( UINT_PTR pEventPtr ) = 0;

	virtual UINT32  GetWidth() = 0;
	virtual UINT32  GetHeight() = 0;

	virtual bool CreateSharedTexture( int w, int h, int textureID ) = 0;
	virtual bool LockSharedTexture() = 0;
	virtual bool UnlockSharedTexture() = 0;
	virtual void ReleaseSharedTexture() = 0;
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif