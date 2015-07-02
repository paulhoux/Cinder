
#include "cinder/app/App.h"
#include "cinder/video/VideoTexture.h"

namespace cinder {
namespace gl {

VideoTexture::VideoTexture()
{
	// Create player window.
	mPlayer = msw::MFPlayer::create();
}

void VideoTexture::play()
{
}

void VideoTexture::stop()
{
}

const Texture2dRef VideoTexture::getTexture() const
{
	return Texture2dRef();
}

}
}

