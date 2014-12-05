#include "vld.h"

#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include "cinder/evr/RendererGlImpl.h"
#include "cinder/gl/Query.h"

using namespace ci;
using namespace ci::app;
using namespace ci::msw;
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

	bool playVideo( const fs::path &path );
private:
	video::MovieGlRef        mMovieRef;
	glm::mat4                mTransform;

	gl::QueryTimeSwappedRef  mQuery;
};

void WindowsEnhancedVideoApp::prepareSettings( Settings* settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1920, 1080 );
}

void WindowsEnhancedVideoApp::setup()
{
	fs::path path = getOpenFilePath();
	playVideo( path );

	gl::enableVerticalSync( true );
	gl::clear();
	gl::color( 1, 1, 1 );

	mQuery = gl::QueryTimeSwapped::create();
}

void WindowsEnhancedVideoApp::shutdown()
{
	mMovieRef.reset();
}

void WindowsEnhancedVideoApp::update()
{
}

void WindowsEnhancedVideoApp::draw()
{
	if( mMovieRef && mMovieRef->checkNewFrame() ) {
		gl::clear();

		mQuery->begin();
		mMovieRef->draw( 0, 0 );
		mQuery->end();
	}
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
	case KeyEvent::KEY_SPACE:
		if( mMovieRef->isPlaying() )
			mMovieRef->stop();
		else mMovieRef->play();
		break;
	}
}

void WindowsEnhancedVideoApp::resize()
{
	if( mMovieRef ) {
		Area bounds = mMovieRef->getBounds();
		Area scaled = Area::proportionalFit( bounds, getWindowBounds(), true, true );
		mTransform = glm::translate( vec3( vec2( scaled.getUL() - bounds.getUL() ) + vec2( 0.5 ), 0 ) ) * glm::scale( vec3( vec2( scaled.getSize() ) / vec2( bounds.getSize() ), 1 ) );
		gl::setModelMatrix( mTransform );
	}
}

void WindowsEnhancedVideoApp::fileDrop( FileDropEvent event )
{
	const fs::path& path = event.getFile( 0 );
	playVideo( path );
}

bool WindowsEnhancedVideoApp::playVideo( const fs::path &path )
{
	if( !path.empty() && fs::exists( path ) ) {
		// TODO: make sure the movie can play
		mMovieRef = video::MovieGl::create( path );
		mMovieRef->play();

		Area bounds = Area::proportionalFit( mMovieRef->getBounds(), getDisplay()->getBounds(), true, false );
		getWindow()->setSize( bounds.getSize() );
		getWindow()->setPos( bounds.getUL() );

		std::string title = "WindowsEnhancedVideo";
		if( mMovieRef->isUsingDirectShow() )
			title += " (DirectShow)";
		else if( mMovieRef->isUsingMediaFoundation() )
			title += " (Media Foundation)";

		getWindow()->setTitle( title );

		return true;
	}

	return false;
}

CINDER_APP_NATIVE( WindowsEnhancedVideoApp, RendererGl )
