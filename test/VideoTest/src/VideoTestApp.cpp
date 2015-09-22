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

private:
	gl::VideoTextureRef  mVideo, mVideo4k;
};

void VideoTestApp::setup()
{
#if _DEBUG
	//VLDEnable();
#endif

	gl::enableVerticalSync( true );
	disableFrameRate();

	const char* video4k = "test4k.mp4";

	/*try {
		mVideo4k = gl::VideoTexture::create( getAssetPath( video4k ) );
		mVideo4k->setLoop( true );
		mVideo4k->play();
	}
	catch( const std::exception &exc ) {
		console() << exc.what() << std::endl;
	}*/
}

void VideoTestApp::mouseDown( MouseEvent event )
{
	const char* video = "test.mp4";

	if( mVideo ) {
		mVideo.reset();
	}
	else {
		try {
			mVideo = gl::VideoTexture::create( getAssetPath( video ) );
			mVideo->setLoop( true );
			mVideo->play();
		}
		catch( const std::exception &exc ) {
			console() << exc.what() << std::endl;
		}
	}

	//mVideo4k.reset();
}

void VideoTestApp::update()
{
}

void VideoTestApp::draw()
{
	gl::clear();

	if( mVideo )
		gl::draw( mVideo->getTexture() );
}

CINDER_APP( VideoTestApp, RendererGl )
