

#pragma once

#include "cinder/Signals.h"
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

	//! Draws the video at full resolution.
	void draw() override {}
	//! Draws the video using the specified \a bounds.
	void draw( const ci::Area &bounds ) override {}

	const Texture2dRef getTexture() const;

private:
	void close();

private:
	signals::ScopedConnection mConnCleanup;

#if defined(CINDER_MSW)
	msw::MFPlayer  *mPlayerPtr;
#endif
};

}
}