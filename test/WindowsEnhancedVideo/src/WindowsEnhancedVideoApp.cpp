#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include "cinder/evr/RendererGlImpl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class WindowsEnhancedVideoApp : public AppNative {
public:
	void prepareSettings( Settings* settings ) override;

	void setup() override;
	void shutdown() override;
	void update() override;
	void draw() override;

	void mouseDown( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;

	void resize() override;
	void fileDrop( FileDropEvent event ) override;
private:
	evr::MovieGlRef  mMovieRef;

	glm::mat4        mTransform;
	double           mTime;
};

void WindowsEnhancedVideoApp::prepareSettings( Settings* settings )
{
	settings->setFrameRate(60);
	settings->setWindowSize( 1920, 1080 );
}

void WindowsEnhancedVideoApp::setup()
{
	fs::path path = getOpenFilePath();

	if( !path.empty() && fs::exists( path ) ) {
		mMovieRef = evr::MovieGl::create( path );
		mMovieRef->play();
	}

	mTime = getElapsedSeconds();

	gl::enableVerticalSync( false );
}

void WindowsEnhancedVideoApp::shutdown()
{
	//mMovieRef->close();
	mMovieRef.reset();
}

void WindowsEnhancedVideoApp::update()
{
	double elapsed = getElapsedSeconds() - mTime;
	getWindow()->setTitle( toString( 1.0 / math<double>::max( 0.001, elapsed ) ) );
	mTime += elapsed;

	//mMovieRef->update();
}

void WindowsEnhancedVideoApp::draw()
{
	gl::clear();

	gl::pushModelMatrix();
	gl::setModelMatrix( mTransform );

	gl::color( 1, 1, 1 );
	if(mMovieRef)
		mMovieRef->draw( 0, 0 );

	gl::popModelMatrix();
}

void WindowsEnhancedVideoApp::mouseDown( MouseEvent event )
{
}

void WindowsEnhancedVideoApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	case KeyEvent::KEY_DELETE:
		mMovieRef.reset();
		break;
	/*case KeyEvent::KEY_l:
		if( mMovieRef ) {
			mMovieRef->setLoop( !mMovieRef->isLooping() );
			CI_LOG_I( "Looping set to: " << mPlayerRef->isLooping() );
		}
		break;*/
	}
}

void WindowsEnhancedVideoApp::resize()
{
	if( mMovieRef ) {
		Area bounds = mMovieRef->getBounds();
		Area scaled = Area::proportionalFit( bounds, getWindowBounds(), true, true );
		mTransform = glm::translate( vec3( scaled.getUL() - bounds.getUL(), 0 ) ) * glm::scale( vec3( vec2( scaled.getSize() ) / vec2( bounds.getSize() ), 1 ) );
	}
}

void WindowsEnhancedVideoApp::fileDrop( FileDropEvent event )
{
	const fs::path& path = event.getFile( 0 );
	if( !path.empty() && fs::exists( path ) ) {
		mMovieRef = evr::MovieGl::create( path );
		mMovieRef->play();

		resize();
	}
}

CINDER_APP_NATIVE( WindowsEnhancedVideoApp, RendererGl )
