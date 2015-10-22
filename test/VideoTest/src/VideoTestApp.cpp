//#include "vld.h"

//#pragma comment(lib, "vld.lib")

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/video/VideoTexture.h"
#include "cinder/Log.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class VideoTestApp : public App {
public:
	void setup() override;
	void update() override;
	void draw() override;

	void mouseDown( MouseEvent event ) override;
	void fileDrop( FileDropEvent event ) override;

	void resize() override;

private:
	Rectf                             mClearRegion;
	std::vector<gl::VideoTextureRef>  mVideos;
};

void VideoTestApp::setup()
{
#if _DEBUG
	//VLDEnable();
#endif

	gl::enableVerticalSync( true );
	disableFrameRate();
}

void VideoTestApp::update()
{
	for( auto &video : mVideos ) {
		// TEMP
		video->getTexture();
	}
}

void VideoTestApp::draw()
{
	gl::clear();

	gl::ScopedColor color( 1, 0, 0 );
	gl::drawSolidRoundedRect( mClearRegion, 10.0f );
}

void VideoTestApp::mouseDown( MouseEvent event )
{
	if( mClearRegion.contains( event.getPos() ) ) {
		if( !mVideos.empty() )
			mVideos.pop_back();
	}
}

void VideoTestApp::fileDrop( FileDropEvent event )
{
	size_t count = event.getNumFiles();
	for( size_t i = 0; i < count; ++i ) {
		try {
			auto video = gl::VideoTexture::create( event.getFile( i ) );

			video->setLoop( true );
			video->play();

			mVideos.push_back( video );
		}
		catch( const std::exception &exc ) {
			console() << exc.what() << std::endl;
		}
	}
}

void VideoTestApp::resize()
{
	mClearRegion = Rectf( getWindowBounds() ).inflated( vec2( -128 ) );
}

CINDER_APP( VideoTestApp, RendererGl )
