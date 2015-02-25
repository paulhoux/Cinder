#pragma once

#include "cinder/Cinder.h"
#include "cinder/gl/Texture.h"

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
	virtual HRESULT Close() = 0;

	virtual HRESULT Play() = 0;
	virtual HRESULT Pause() = 0;
	virtual HRESULT Stop() = 0;

	virtual HRESULT HandleEvent( UINT_PTR pEventPtr ) = 0;

	virtual BOOL    IsPlaying() const = 0;
	virtual BOOL    IsPaused() const = 0;
	virtual BOOL    IsStopped() const = 0;
	virtual BOOL    IsLooping() const = 0;

	virtual UINT32  GetWidth() const = 0;
	virtual UINT32  GetHeight() const = 0;

	virtual float   GetTime() const = 0;
	virtual float   GetDuration() const = 0;
	virtual float   GetFrameRate() const = 0;
	virtual UINT32  GetNumFrames() const = 0;

	virtual void    SetLoop( BOOL loop = TRUE ) = 0;
	virtual void    SeekToTime( FLOAT seconds ) = 0;

	virtual ci::gl::Texture2dRef GetTexture() = 0;
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif