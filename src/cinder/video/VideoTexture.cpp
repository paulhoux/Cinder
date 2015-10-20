
#include "cinder/video/VideoTexture.h"
#include "cinder/msw/CinderMsw.h"
#include "cinder/app/App.h"

namespace cinder {
namespace gl {

VideoTexture::VideoTexture()
{
	// Create player.
#if defined(CINDER_MSW)
	mPlayerPtr = new msw::MFPlayer();
#endif

	// Make sure this instance is properly destroyed before the application quits.
	mConnCleanup = app::App::get()->getSignalCleanup().connect( [&]() { close(); } );
}

VideoTexture::VideoTexture( const fs::path & path )
	: VideoTexture()
{
#if defined(CINDER_MSW)
	if( mPlayerPtr ) {
		mPlayerPtr->OpenURL( msw::toWideString( path.string() ).c_str() );
	}
#endif
}

VideoTexture::~VideoTexture()
{
	close();
}

void VideoTexture::setLoop( bool enabled )
{
#if defined(CINDER_MSW)
	mPlayerPtr->SetLoop( enabled );
#endif
}

void VideoTexture::play()
{
#if defined(CINDER_MSW)
	if( mPlayerPtr )
		mPlayerPtr->Play();
#endif
}

void VideoTexture::stop()
{
#if defined(CINDER_MSW)
	if( mPlayerPtr )
		mPlayerPtr->Pause();
#endif
}

const Texture2dRef VideoTexture::getTexture() const
{
	return Texture2dRef();
}

void VideoTexture::close()
{
#if defined(CINDER_MSW)
	if( mPlayerPtr )
		mPlayerPtr->Close();

	SafeRelease( mPlayerPtr );
#endif
}

}
}

