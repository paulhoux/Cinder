#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMswCom.h"

namespace cinder {
namespace msw {
namespace video {

class IRenderer : public IUnknown {
public:
	IRenderer() {}
	virtual ~IRenderer() {}
	/*
	virtual HRESULT OpenFile( PCWSTR pszFileName ) = 0;
	virtual HRESULT Close() = 0;

	virtual HRESULT Play() = 0;
	virtual HRESULT Pause() = 0;
	virtual HRESULT Stop() = 0;

	virtual HRESULT HandleEvent( UINT_PTR pEventPtr ) = 0;

	virtual UINT32  GetWidth() const = 0;
	virtual UINT32  GetHeight() const = 0;
	*/
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif