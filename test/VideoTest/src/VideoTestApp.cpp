#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/video/VideoTexture.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class VideoTestApp : public App {
public:
	void setup() override;
	void update() override;
	void draw() override;

	void mouseDown( MouseEvent event ) override;

private:
	gl::VideoTextureRef  mVideo;
};

void VideoTestApp::setup()
{
	mVideo = gl::VideoTexture::create( getAssetPath( "test.mp4" ) );
	//mVideo->play();

	mVideo.reset();
}

void VideoTestApp::mouseDown( MouseEvent event )
{
}

void VideoTestApp::update()
{
}

void VideoTestApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
	//gl::draw( mVideo->getTexture() );
}

CINDER_APP( VideoTestApp, RendererGl )
