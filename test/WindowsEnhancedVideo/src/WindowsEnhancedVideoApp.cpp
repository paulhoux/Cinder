#include "vld.h"

#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/VboMesh.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include "cinder/evr/RendererGlImpl.h"
#include "cinder/gl/Query.h"

using namespace ci;
using namespace ci::app;
using namespace ci::msw;
using namespace ci::msw::video;
using namespace std;

struct Circle {
	ci::vec2  size;
	int       index;
	int       pad0;

	ci::vec2  position;
	float     radius;
	float     pad1;

	float     thickness;
	float     length;
	float     offset;
	float     pad2;
};

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

	void close() { mMovies.clear(); }

private:
	static const int kMaxMovieCount = 1;

	std::vector<MovieGlRef>        mMovies;

	ci::gl::GlslProgRef            mShader;
	ci::gl::VboRef                 mData;
	ci::gl::VboMeshRef             mMesh;
	ci::gl::BatchRef               mBatch;
};

void WindowsEnhancedVideoApp::prepareSettings( Settings* settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1280, 720 );
}

void WindowsEnhancedVideoApp::setup()
{
	auto mgr = log::manager();
	mgr->setConsoleLoggingEnabled( true );

	auto path = getHomeDirectory() / "cinder-wmf.log";
	if( !fs::exists( path.stem() ) )
		fs::create_directories( path.stem() );

	mgr->setFileLoggingEnabled( true, path );

	CI_LOG_I( "Logging enabled for Cinder WMF: " << path.string() );

	//
	gl::enableVerticalSync( true );
	gl::clear();
	gl::color( 1, 1, 1 );

	//
	getWindow()->connectClose( &WindowsEnhancedVideoApp::close, this );
}

void WindowsEnhancedVideoApp::shutdown()
{
}

void WindowsEnhancedVideoApp::update()
{
	if( getElapsedFrames() % 30 == 0 ) {
		for( auto itr = mMovies.begin(); itr != mMovies.end(); ++itr ) {
			auto &movie = *itr;
			CI_LOG_V( "Playing:" << ( *itr )->isPlaying() << ", Paused:" << ( *itr )->isPaused() << ", Done:" << ( *itr )->isDone() );
		}
	}
}

void WindowsEnhancedVideoApp::draw()
{
	gl::clear();
	gl::setMatricesWindow( getWindowSize(), true );

	// Draw movies.
	for( size_t i = 0; i < mMovies.size(); ++i ) {
		auto movie = mMovies[i];
		if( !movie )
			continue;

		auto texture = movie->getTexture();
		if( !texture )
			continue;

		gl::draw( texture, Rectf( Area::proportionalFit( texture->getBounds(), getWindowBounds(), true, true ) ) );
	}
}

void WindowsEnhancedVideoApp::mouseDown( MouseEvent event )
{
}

void WindowsEnhancedVideoApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_ESCAPE:
		mMovies.clear();
		quit();
		break;
	case KeyEvent::KEY_DELETE:
		if( !mMovies.empty() )
			mMovies.erase( mMovies.begin() );
		break;
	case KeyEvent::KEY_o:
		playVideo( getOpenFilePath() );
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
	case KeyEvent::KEY_r:
		mMovies.clear();
		break;
	case KeyEvent::KEY_f:
		setFullScreen( !isFullScreen() );
		break;
	case KeyEvent::KEY_SPACE:
		if( !mMovies.empty() && mMovies.front() ) {
			auto &movie = mMovies.front();
			if( movie->isPlaying() )
				movie->pause();
			else //if( movie->isPaused() )
				movie->play();
			break;
		}
	case KeyEvent::KEY_RIGHT:
		if( !mMovies.empty() && mMovies.front() ) {
			auto &movie = mMovies.front();
			movie->seekToTime( 100.0f );
		}
		break;
	}
}

void WindowsEnhancedVideoApp::resize()
{
}

void WindowsEnhancedVideoApp::fileDrop( FileDropEvent event )
{
	for( size_t i = 0; i < event.getNumFiles(); ++i ) {
		const fs::path& path = event.getFile( i );
		playVideo( path );
	}
}

bool WindowsEnhancedVideoApp::playVideo( const fs::path &path )
{
	mMovies.clear();

	if( mMovies.size() < kMaxMovieCount && !path.empty() && fs::exists( path ) ) {
		auto movie = video::MovieGl::create( path );
		movie->play();

		mMovies.push_back( movie );

		return true;
	}

	return false;
}

CINDER_APP_NATIVE( WindowsEnhancedVideoApp, RendererGl )
