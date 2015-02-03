//#include "vld.h"

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
	video::MovieGlRef              mMovieRef;
	glm::mat4                      mTransform;

	std::vector<gl::Texture2dRef>  mCapturedFrames;

	gl::QueryTimeSwappedRef        mQuery;
};

void WindowsEnhancedVideoApp::prepareSettings( Settings* settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1920, 1080 );
}

void WindowsEnhancedVideoApp::setup()
{
	auto p = getAssetPath( "test.txt" );

	fs::path path = getOpenFilePath();
	playVideo( path );

	gl::enableVerticalSync( true );
	gl::clear();
	gl::color( 1, 1, 1 );

	mQuery = gl::QueryTimeSwapped::create();
}

void WindowsEnhancedVideoApp::shutdown()
{
	mCapturedFrames.clear();
	mMovieRef.reset();
}

void WindowsEnhancedVideoApp::update()
{
}

void WindowsEnhancedVideoApp::draw()
{
	gl::clear();

	if( mMovieRef ) {

		// Draw movie.
		mQuery->begin();

		gl::Texture2dRef tex = mMovieRef->getTexture();
		if( tex )
			gl::draw( tex, (Rectf) Area::proportionalFit( tex->getBounds(), getWindowBounds(), true, true ) );

		mQuery->end();

		// Draw captured frames.
		float x = 0.0f, y = 0.0f;
		for( auto &frame : mCapturedFrames ) {
			float w = frame->getWidth() * 0.25f;
			float h = frame->getHeight() * 0.25f;
			gl::draw( frame, Rectf( x, y, x + w, y + h ) );

			x += w;
			if( ( x + w ) > float( getWindowWidth() ) ) {
				x = 0.0f;
				y += h;
			}
		}
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
	case KeyEvent::KEY_SPACE:
		if( mMovieRef ) {
			mCapturedFrames.push_back( mMovieRef->getTexture() );
		}
		break;
	case KeyEvent::KEY_DELETE:
		if( !mCapturedFrames.empty() )
			mCapturedFrames.pop_back();
		break;
	case KeyEvent::KEY_o:
		playVideo( getOpenFilePath() );
		break;
	case KeyEvent::KEY_r:
		mMovieRef.reset();
		break;
	case KeyEvent::KEY_v:
		gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
		break;
	case KeyEvent::KEY_l:
		if( isFrameRateEnabled() )
			disableFrameRate();
		else
			setFrameRate( 60.0f );
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
		// 
		mCapturedFrames.clear();

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
