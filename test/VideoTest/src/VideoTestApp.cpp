//#include "vld.h"

//#pragma comment(lib, "vld.lib")

#include "cinder/wmf/MediaFoundationGlImpl.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class VideoTestApp : public App {
public:
	static void prepare( Settings *settings );

	void setup() override;
	void update() override;
	void draw() override;

	void mouseDown( MouseEvent event ) override;
	void fileDrop( FileDropEvent event ) override;

	void resize() override;

private:
	Rectf                         mClearRegion;
	std::vector<ci::wmf::MovieGlRef>  mVideos;

	ci::gl::Texture2dRef          mTexture;
};

void VideoTestApp::prepare( Settings *settings )
{
	settings->setResizable( false );
}

void VideoTestApp::setup()
{
#if _DEBUG
	//VLDEnable();
#endif

	gl::enableVerticalSync( false );
	disableFrameRate();
}

void VideoTestApp::update()
{
	if( !mVideos.empty() ) {
		if( getWindowSize() != mVideos.front()->getSize() && mVideos.front()->getSize().x > 0 && mVideos.front()->getSize().y > 0 ) {
			getWindow()->setSize( mVideos.front()->getSize() );
		}
	}
}

void VideoTestApp::draw()
{
	gl::clear();

	gl::ScopedColor color( 1, 0, 0 );
	gl::drawSolidRoundedRect( mClearRegion, 10.0f );

	if( !mVideos.empty() ) {
		//auto texture = mVideos.front()->getTexture();

		//if( texture )
		//	mTexture = texture;

		//gl::ScopedColor color( 1, 1, 1 );
		//gl::draw( mTexture );
		mVideos.front()->draw();
	}
}

void VideoTestApp::mouseDown( MouseEvent event )
{
	if( mClearRegion.contains( event.getPos() ) ) {
		mTexture.reset(); // TEMP

		if( !mVideos.empty() )
			mVideos.pop_back();
	}
}

void VideoTestApp::fileDrop( FileDropEvent event )
{
	size_t count = event.getNumFiles();
	for( size_t i = 0; i < count; ++i ) {
		try {
			auto video = wmf::MovieGl::create( event.getFile( i ) );

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

CINDER_APP( VideoTestApp, RendererGl, &VideoTestApp::prepare )
