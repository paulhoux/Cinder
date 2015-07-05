

#pragma once

#include "cinder/gl/Texture.h"
#include "cinder/video/Video.h"

#if defined(CINDER_MSW)
#include "cinder/msw/MediaFoundation.h"
#endif

namespace cinder {
namespace gl {

typedef std::shared_ptr<class VideoTexture> VideoTextureRef;

//! Uses the Windows Media Foundation backend to render video directly to a texture using hardware support where available.
class VideoTexture : public video::VideoBase {
public:
	VideoTexture();
	VideoTexture( const fs::path &path );
	~VideoTexture();

	static std::shared_ptr<VideoTexture> create( const fs::path &path )
	{
		return std::make_shared<VideoTexture>( path );
	}

	//! Enable or disable looping.
	void setLoop( bool enabled = true ) override;

	//! Begins video playback.
	void play() override;
	//! Stops video playback.
	void stop() override;

	const Texture2dRef getTexture() const;

private:
#if defined(CINDER_MSW)
	msw::MFPlayer  *mPlayerPtr;
#endif
};

}
}