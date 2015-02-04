#pragma once

#include "cinder/Cinder.h"

#if defined(CINDER_MSW)

#include "cinder/msw/CinderMswCom.h"

namespace cinder {
namespace msw {
namespace video {

class IRenderer {
public:
	IRenderer() {}
	virtual ~IRenderer() {}
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif