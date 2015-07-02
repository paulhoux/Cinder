

#pragma once

#include "cinder/app/msw/AppImplMsw.h"
#include "cinder/gl/Texture.h"
#include "cinder/video/Video.h"

#if defined(CINDER_MSW)
#include "cinder/msw/MediaFoundation.h"
#endif

namespace cinder { namespace gl {

#if defined(CINDER_MSW)

typedef std::shared_ptr<class VideoTexture> VideoTextureRef;

//! Uses the Windows Media Foundation backend to render video directly to a texture using hardware support where available.
class VideoTexture : public video::VideoBase {
public:
	VideoTexture();
	VideoTexture( const fs::path &path ) : VideoTexture() {}
	~VideoTexture() {}

	static std::shared_ptr<VideoTexture> create( const fs::path &path )
	{
		return std::make_shared<VideoTexture>( path ); 
	}

	//! Begins video playback.
	void play() override;
	//! Stops video playback.
	void stop() override;

	const Texture2dRef getTexture() const;

private:
	msw::MFPlayerRef  mPlayer;
};

#else
#endif

} }