#include "cinder/Log.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/msw/CinderMsw.h"
#include "cinder/wmf/MediaFoundationGl.h"

#include "Helpers.h"

// Make our application NVIDIA Optimus aware.
#if defined(CINDER_MSW)
extern "C" {
	_declspec( dllexport ) DWORD NvOptimusEnablement = 0x0000001;
}
#endif

using namespace ci;
using namespace ci::app;
using namespace ci::wmf;
using namespace std;

class VideoTestApp : public App {
public:
	static void prepare( Settings *settings );

	void setup() override;
	void cleanup() override;
	void update() override;
	void draw() override;

	void mouseMove( MouseEvent event ) override;
	void mouseDown( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;
	void mouseUp( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;

	void fileDrop( FileDropEvent event ) override;

	void resize() override;

	void close();
	void rewind( bool enable = true );
	void fastForward( bool enable = true );
	void seekTo( float seconds );
	void pause();
	void play();
	void eject();
	void toggleLoop();
	void test();
	void measure();

	void createUi();

private:
	gl::Texture2dRef              mLabel;
	std::vector<gl::Texture2dRef> mSnapshots;
	std::vector<MovieGlRef>       mVideos;
	size_t                        mVideoSelected;

	// Performance
	std::vector<LONGLONG> mPerformance;
	std::vector<size_t>   mTimings;
	size_t                mIndex;

	// Ui
	UiNodeRef   mUi;
	UiButtonRef mBtnPause, mBtnPlay, mBtnVS, mBtnFR60, mBtnDX9, mBtnDX11, mBtnLoop;

	std::vector<signals::Connection> mConnections;

	bool mLoop = true;
	bool mEnableTest = false;
};

void VideoTestApp::prepare( Settings *settings )
{
	// settings->setResizable( false );
	settings->setWindowSize( 1280, 720 );
}

void VideoTestApp::setup()
{
	// Setup file logging.
	auto folder = getAppPath() / "logs";
	log::LogManager::instance()->addLogger(
		std::make_shared<log::LoggerFileRotating>( folder, "%Y-%m-%d.log", true ) );

	// Disable vertical sync. You can enable it, but it will have an effect on the
	// performance measurements that skew the results (blitting frames will take
	// longer if we're waiting on the GPU flush). The results are valid, but most
	// of the time is spent spin-waiting on the vertical sync, not on the blit.
	gl::enableVerticalSync( false );

	// Enable frame rate limiter. You can disable it, but on fast machines this
	// will
	// only show a few seconds of performance measurements because of the high
	// frame rate.
	setFrameRate( 60.0f );

	// Close the video player if the window is closed. This is required because
	// the player depends on an OpenGL context.
	// mConnClose = getWindow()->getSignalClose().connect( [&]() { close(); } );

	// Init performance.
	mIndex = 0;
	mPerformance.resize( getWindowWidth() );

	// Create user interface.
	createUi();
}

void VideoTestApp::cleanup()
{
	for( auto &conn : mConnections )
		conn.disconnect();

	mConnections.clear();
}

void VideoTestApp::update()
{
	// Update user interface.
	if( !mVideos.empty() ) {
		auto &video = mVideos[mVideoSelected];

		// Resize performance counter if necessary.
		if( getWindow()->getWidth() > 0 ) {
			mPerformance.resize( getWindow()->getWidth() );
			mIndex = mIndex % mPerformance.size();
		}

		// Update user interface.
		if( video->isPlaying() ) {
			mBtnPause->toggle( false );
			mBtnPlay->toggle( true );
		}
		else {
			mBtnPause->toggle( true );
			mBtnPlay->toggle( false );
		}

		mBtnDX9->toggle( video->isDX9() );
		mBtnDX11->toggle( !video->isDX9() );
	}
	else {
		// Update user interface.
		mBtnPause->toggle( false );
		mBtnPlay->toggle( false );

		mBtnDX9->toggle( false );
		mBtnDX11->toggle( false );
	}

	mBtnVS->toggle( gl::isVerticalSyncEnabled() );
	mBtnFR60->toggle( isFrameRateEnabled() );
	mBtnLoop->toggle( mLoop );
}

void VideoTestApp::draw()
{
	// Perform test events.
	if( mEnableTest )
		test();

	gl::clear();
	gl::color( 1, 1, 1 );

	// start performance measurement.
	LARGE_INTEGER start, stop;
	::QueryPerformanceCounter( &start );

	// Obtain the latest video frame texture. May be empty if no frame is available.
	auto texture = mVideos.empty() ? nullptr : mVideos[mVideoSelected]->getTexture();

	// Stop measurement and add to results.
	::QueryPerformanceCounter( &stop );

	mPerformance[mIndex] = stop.QuadPart - start.QuadPart;
	mIndex = ( mIndex + 1 ) % mPerformance.size();

	size_t index = ( mIndex + 20 ) % mPerformance.size();
	if( index < mPerformance.size() )
		mPerformance[index] = 0;

	// Draw video.
	if( texture ) {
		auto bounds = Area( 0, 0, getWindowWidth(), getWindowHeight() - 110 - mUi->getHeight() );
		auto centered = Area::proportionalFit( texture->getBounds(), bounds, true, true );
		gl::draw( texture, texture->getBounds(), centered );
	}

	// Draw progress.
	do {
		float p = mVideos.empty() ? 0.0f : mVideos[mVideoSelected]->getCurrentTime() / mVideos[mVideoSelected]->getDuration();

		float w = getWindowWidth();
		float h = 10;
		float x = 0;
		float y = float( getWindowHeight() - 110 );

		gl::draw( gl::VboMesh::create(
			geom::Rect()
			.colors( Color::hex( 0x3A444C ), Color::hex( 0x3A444C ), Color::hex( 0x1D2226 ), Color::hex( 0x1D2226 ) )
			.rect( Rectf( x, y, x + w, y + h ) ) ) );
		gl::draw( gl::VboMesh::create(
			geom::Rect()
			.colors( Color::hex( 0xF7A149 ), Color::hex( 0xF7A149 ), Color::hex( 0xC4683E ), Color::hex( 0xC4683E ) )
			.rect( Rectf( x, y, x + p * w, y + h ) ) ) );
	} while( false );

	// Draw captured thumbnails, if any.
	vec2 position;
	for( auto &snapshot : mSnapshots ) {
		gl::draw( snapshot,
				  Rectf( position, position + 0.2f * vec2( snapshot->getSize() ) ) );
		float w = 0.2f * snapshot->getWidth();
		position.x += w;
		if( position.x > getWindowWidth() - w ) {
			position.x = 0;
			position.y += 0.2f * snapshot->getHeight();
		}
	}

	// Measure and draw performance.
	measure();

	// Draw user interface.
	do {
		float w = getWindowWidth();
		float h = mUi->getHeight();
		float x = 0;
		float y = float( getWindowHeight() - 110 - mUi->getHeight() );

		gl::draw( gl::VboMesh::create( geom::Rect().colors( Color::hex( 0x1D2226 ), Color::hex( 0x1D2226 ), Color::hex( 0x13171A ), Color::hex( 0x13171A ) ).rect( Rectf( x, y, x + w, y + h ) ) ) );

		mUi->recursePreorder( &UiNode::draw );
	} while( false );
}

void VideoTestApp::mouseMove( MouseEvent event )
{
	mUi->recursePostorder<MouseEvent &>( &UiNode::mouseMove, event );
}

void VideoTestApp::mouseDown( MouseEvent event )
{
	Area scrubRegion = Area( 0, getWindowHeight() - 110, getWindowWidth(), getWindowHeight() - 100 );

	if( !mVideos.empty() && scrubRegion.contains( event.getPos() ) ) {
		mVideos[mVideoSelected]->setScrubbing( true );
		mVideos[mVideoSelected]->seekToTime( mVideos[mVideoSelected]->getDuration() * event.getPos().x / float( getWindowWidth() ) );
	}
	else {
		mUi->recursePostorder<MouseEvent &>( &UiNode::mouseDown, event );
	}
}

void VideoTestApp::mouseDrag( MouseEvent event )
{
	if( !mVideos.empty() && mVideos[mVideoSelected]->isScrubbing() ) {
		mVideos[mVideoSelected]->seekToTime( mVideos[mVideoSelected]->getDuration() * event.getPos().x / float( getWindowWidth() ) );
	}
	else
		mUi->recursePostorder<MouseEvent &>( &UiNode::mouseDrag, event );
}

void VideoTestApp::mouseUp( MouseEvent event )
{
	if( !mVideos.empty() && mVideos[mVideoSelected]->isScrubbing() ) {
		mVideos[mVideoSelected]->setScrubbing( false );
	}
	else
		mUi->recursePostorder<MouseEvent &>( &UiNode::mouseUp, event );
}

void VideoTestApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
		case KeyEvent::KEY_ESCAPE:
			if( isFullScreen() )
				setFullScreen( false );
			else
				quit();
			return;
		case KeyEvent::KEY_BACKSPACE:
		case KeyEvent::KEY_DELETE:
			if( !mSnapshots.empty() ) {
				mSnapshots.pop_back();
			}
			return;
		case KeyEvent::KEY_f:
			setFullScreen( !isFullScreen() );
			return;
		case KeyEvent::KEY_r:
			if( isFrameRateEnabled() )
				disableFrameRate();
			else
				setFrameRate( 60 );
			return;
		case KeyEvent::KEY_v:
			gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
			return;
		case KeyEvent::KEY_t:
			mEnableTest = !mEnableTest;
			break;
	}

	if( mVideos.empty() )
		return;

	auto &video = mVideos[mVideoSelected];

	switch( event.getCode() ) {
		case KeyEvent::KEY_SPACE:
			// Create snapshot.
			if( mSnapshots.size() < 25 ) {
				if( auto texture = video->getTexture() )
					mSnapshots.push_back( texture );
			}
			break;
		case KeyEvent::KEY_p:
			if( video->isPlaying() )
				video->stop();
			else
				video->play();
			break;
		case KeyEvent::KEY_LEFTBRACKET:
			// video->seekToTime( 10.0f );
			break;
		case KeyEvent::KEY_RIGHTBRACKET:
			// video->seekToTime( 110.0f );
			break;

		case KeyEvent::KEY_PLUS:
		case KeyEvent::KEY_KP_PLUS:
			video->setVolume( 1.0f );
			CI_LOG_V( "Audio level set to: " << video->getVolume() );
			break;

		case KeyEvent::KEY_MINUS:
		case KeyEvent::KEY_KP_MINUS:
			video->setVolume( 0.25f );
			CI_LOG_V( "Audio level set to: " << video->getVolume() );
			break;
	}
}

void VideoTestApp::fileDrop( FileDropEvent event )
{
	// Close current movie.
	mVideos.clear();

	// Try to open each file.
	size_t count = event.getNumFiles();
	while( count-- > 0 ) {
		try {
			auto video = MovieGl::create( event.getFile( count ) );

			video->setLoop( mLoop );
			video->play();

			mVideos.push_back( video );
			mVideoSelected = mVideos.size() - 1;

			return;
		}
		catch( const std::exception &exc ) {
			console() << exc.what() << std::endl;
		}
	}
}

void VideoTestApp::resize()
{
	mUi->setPosition( ( getWindowWidth() - mUi->getWidth() ) / 2,
					  ( getWindowHeight() - 105 - mUi->getHeight() ) );
}

void VideoTestApp::close()
{
	mSnapshots.clear();
}

void VideoTestApp::rewind( bool enable )
{
	if( mVideos.empty() )
		return;

	auto &video = mVideos[mVideoSelected];

	// video->rewind( enable );
}

void VideoTestApp::fastForward( bool enable )
{
	if( mVideos.empty() )
		return;

	auto &video = mVideos[mVideoSelected];

	// video->fastForward( enable );
}

void VideoTestApp::pause()
{
	if( mVideos.empty() )
		return;

	auto &video = mVideos[mVideoSelected];

	if( video->isPlaying() )
		video->stop();
}

void VideoTestApp::play()
{
	if( mVideos.empty() )
		return;

	auto &video = mVideos[mVideoSelected];

	if( !video->isPlaying() )
		video->play();
}

void VideoTestApp::seekTo( float seconds )
{
	if( mVideos.empty() )
		return;

	auto &video = mVideos[mVideoSelected];

	// video->seekToTime( seconds );
}

void VideoTestApp::eject()
{
	static fs::path initialPath = getAssetPath( "" );

	if( mVideos.empty() ) {
		auto path = ci::app::getOpenFilePath( initialPath, { "mp4" } );
		if( !path.empty() ) {
			try {
				auto video = MovieGl::create( path );

				video->setLoop( mLoop );
				video->play();

				mVideos.push_back( video );
				mVideoSelected = mVideos.size() - 1;

				initialPath = path.parent_path();
			}
			catch( const std::exception &exc ) {
				console() << exc.what() << std::endl;
			}
		}
	}
	else {
		mVideos.clear();
	}
}

void VideoTestApp::toggleLoop()
{
	if( mVideos.empty() ) {
		mLoop = !mLoop;
		return;
	}

	auto &video = mVideos[mVideoSelected];

	mLoop = !video->isLooping();
	video->setLoop( mLoop );
}

void VideoTestApp::test()
{
	LARGE_INTEGER seed;
	::QueryPerformanceCounter( &seed );

	Rand::randSeed( (uint32_t)seed.QuadPart );
	static uint32_t rnd = 0;

	if( rnd < 10000 ) {
		// Close current and open another movie.
		mVideos.clear();

		// Create a list of available movies.
		std::vector<fs::path> videos;
		fs::path path = getAssetPath( "" );
		fs::directory_iterator end_itr; // default construction yields past-the-end
		for( fs::directory_iterator itr( path ); itr != end_itr; ++itr ) {
			if( fs::is_regular_file( *itr ) ) {
				if( itr->path().extension() == ".mp4" )
					videos.push_back( *itr );
			}
		}

		if( !videos.empty() ) {
			// Select a random video and play it.
			auto index = Rand::randUint( videos.size() );
			auto video = MovieGl::create( videos.at( index ) );

			video->setLoop( mLoop );
			video->play();

			mVideos.push_back( video );
			mVideoSelected = mVideos.size() - 1;
		}
	}
	//else if( rnd < 50 ) {
	//	// Pause if playing.
	//	if( !mVideos.empty() ) {
	//		auto &video = mVideos[mVideoSelected];

	//		if( video->isPlaying() )
	//			video->stop();
	//	}
	//}
	//else if( rnd < 250 ) {
	//	// Play if paused.
	//	if( !mVideos.empty() ) {
	//		auto &video = mVideos[mVideoSelected];

	//		if( !video->isPlaying() )
	//			video->play();
	//	}
	//}
	else if( rnd < 100 ) {
		// Jump to random position.
		if( !mVideos.empty() ) {
			auto &video = mVideos[mVideoSelected];

			video->seekToTime( Rand::randFloat() * video->getDuration() );
		}
	}

	rnd = Rand::randUint( 50000 );
}

void VideoTestApp::measure()
{
	const double kTimePerBar = 1.0;

	// Update frame timings.
	mTimings.erase( std::remove( mTimings.begin(), mTimings.end(), mIndex ),
					mTimings.end() );

	static double last = getElapsedSeconds();
	double        time = getElapsedSeconds();
	if( time - last >= kTimePerBar ) {
		last += kTimePerBar;
		mTimings.push_back( mIndex );
	}
	while( time - last >= kTimePerBar ) {
		last += kTimePerBar;
	}

	// Draw visualizer.
	float x = 0;
	float y = 0;
	float w = float( getWindowWidth() );
	float h = float( getWindowHeight() );

	auto batch = gl::VertBatch::create( GL_LINES );
	batch->clear();

	batch->begin( GL_LINES );
	// gray lines, 10 pixels = 1 ms
	for( int i = 0; i <= 10; ++i ) {
		y = h - float( i * 10 );
		batch->color( 0.2f, 0.2f, 0.2f );
		batch->vertex( vec2( 0, y ) );
		batch->color( 0.2f, 0.2f, 0.2f );
		batch->vertex( vec2( w, y ) );
	}
	// gray line at top of interface
	batch->color( 0.2f, 0.2f, 0.2f );
	batch->vertex( vec2( 0, h - 110 - mUi->getHeight() ) );
	batch->color( 0.2f, 0.2f, 0.2f );
	batch->vertex( vec2( w, h - 110 - mUi->getHeight() ) );
	// gray bar every 'kTimePerBar' secs
	for( auto &timing : mTimings ) {
		x = float( timing );
		batch->color( 0.2f, 0.2f, 0.2f );
		batch->vertex( vec2( x, h ) );
		batch->color( 0.2f, 0.2f, 0.2f );
		batch->vertex( vec2( x, h - 100 ) );
	}
	// ms per frame in orange
	x = 0;
	for( auto &perf : mPerformance ) {
		batch->color( Color::hex( 0xC4683E ) );
		batch->vertex( vec2( x, h ) );
		batch->color( Color::hex( 0xF7A149 ) );
		batch->vertex( vec2( x, h - float( perf * 10 / 10000 ) ) );
		x += 1.0f;
	}
	// current position in white
	x = float( mIndex );
	batch->color( 1, 1, 1 );
	batch->vertex( vec2( x, h ) );
	batch->color( 1, 1, 1 );
	batch->vertex( vec2( x, h - 100 ) );
	batch->end();

	gl::color( 1, 1, 1 );
	batch->draw();

	if( mLabel ) {
		gl::ScopedBlendAdditive blend;
		gl::draw( mLabel, vec2( ( w - mLabel->getWidth() ) * 0.5f, h - 100 + 0.5f * ( 100 - mLabel->getHeight() ) ) );
	}
}

void VideoTestApp::createUi()
{
	// Load sprite textures.
	auto big = std::make_shared<SpriteSheet>( loadAsset( "buttons.png" ), ivec2( 72, 80 ) );
	auto sml = std::make_shared<SpriteSheet>( loadAsset( "sm_buttons.png" ), ivec2( 72, 36 ) );

	// Create Ui container
	mUi = UiNode::create();

	// Add buttons.
	auto groupBig = UiNode::create();
	groupBig->setSize( 3 * big->getCellSize().x, big->getCellSize().y );
	groupBig->setParent( mUi );

	auto btnShutdown = UiButton::create( "Shutdown" );
	btnShutdown->setSprites( big, 24, 25, 26 );
	btnShutdown->setPosition( 0 * big->getCellSize().x, 0 );
	// btnShutdown->setParent( groupBig );

	auto btnSeekToStart = UiButton::create( "SeekToStart" );
	btnSeekToStart->setSprites( big, 21, 22, 23 );
	btnSeekToStart->setPosition( 1 * big->getCellSize().x, 0 );
	// btnSeekToStart->setParent( groupBig );

	auto btnRewind = UiButton::create( "Rewind" );
	btnRewind->setSprites( big, 15, 16, 17 );
	btnRewind->setPosition( 2 * big->getCellSize().x, 0 );
	// btnRewind->setParent( groupBig );

	mBtnPlay = UiButton::create( "Play" );
	mBtnPlay->setSprites( big, 12, 13, 14 );
	mBtnPlay->setPosition( 0 * big->getCellSize().x, 0 );
	mBtnPlay->setParent( groupBig );

	mBtnPause = UiButton::create( "Pause" );
	mBtnPause->setSprites( big, 9, 10, 11 );
	mBtnPause->setPosition( 1 * big->getCellSize().x, 0 );
	mBtnPause->setParent( groupBig );

	auto btnFastForward = UiButton::create( "FastForward" );
	btnFastForward->setSprites( big, 3, 4, 5 );
	btnFastForward->setPosition( 5 * big->getCellSize().x, 0 );
	// btnFastForward->setParent( groupBig );

	auto btnSeekToEnd = UiButton::create( "SeekToEnd" );
	btnSeekToEnd->setSprites( big, 18, 19, 20 );
	btnSeekToEnd->setPosition( 6 * big->getCellSize().x, 0 );
	// btnSeekToEnd->setParent( groupBig );

	auto btnEject = UiButton::create( "Eject" );
	btnEject->setSprites( big, 0, 1, 2 );
	btnEject->setPosition( 2 * big->getCellSize().x, 0 );
	btnEject->setParent( groupBig );

	auto btnMute = UiButton::create( "Mute" );
	btnMute->setSprites( big, 6, 7, 8 );
	btnMute->setToggle( true );
	btnMute->setPosition( 8 * big->getCellSize().x, 0 );
	// btnMute->setParent( groupBig );

	auto groupLeft = UiNode::create();
	groupLeft->setSize( 2 * sml->getCellSize().x, sml->getCellSize().y );
	groupLeft->setParent( mUi );

	mBtnDX9 = UiButton::create( "DX9" );
	mBtnDX9->setSprites( sml, 3, 4, 5 );
	mBtnDX9->setPosition( 0 * sml->getCellSize().x, 0 );
	mBtnDX9->setClickable( false );
	mBtnDX9->setParent( groupLeft );

	mBtnDX11 = UiButton::create( "DX11" );
	mBtnDX11->setSprites( sml, 0, 1, 2 );
	mBtnDX11->setPosition( 1 * sml->getCellSize().x, 0 );
	mBtnDX11->setClickable( false );
	mBtnDX11->setParent( groupLeft );

	auto groupRight = UiNode::create();
	groupRight->setSize( 3 * sml->getCellSize().x, sml->getCellSize().y );
	groupRight->setParent( mUi );

	mBtnFR60 = UiButton::create( "FR60" );
	mBtnFR60->setSprites( sml, 6, 7, 8 );
	mBtnFR60->setPosition( 0 * sml->getCellSize().x, 0 );
	mBtnFR60->setParent( groupRight );

	mBtnVS = UiButton::create( "VS" );
	mBtnVS->setSprites( sml, 12, 13, 14 );
	mBtnVS->setPosition( 1 * sml->getCellSize().x, 0 );
	mBtnVS->setParent( groupRight );

	mBtnLoop = UiButton::create( "LOOP" );
	mBtnLoop->setSprites( sml, 9, 10, 11 );
	mBtnLoop->setPosition( 2 * sml->getCellSize().x, 0 );
	mBtnLoop->setParent( groupRight );

	// Manual layout in absence of layout helpers.
	mUi->setSize( groupBig->getWidth() + groupLeft->getWidth() + groupRight->getWidth(), groupBig->getHeight() );
	groupLeft->setPosition( 0, ( groupBig->getHeight() - groupLeft->getHeight() ) / 2 );
	groupBig->setPosition( groupLeft->getWidth(), 0 );
	groupRight->setPosition( groupLeft->getWidth() + groupBig->getWidth(), ( groupBig->getHeight() - groupRight->getHeight() ) / 2 );

	// Connect to buttons.
	mConnections.push_back( mBtnPause->pressed( [&]( const UiButtonRef & ) { pause(); } ) );
	mConnections.push_back( mBtnPlay->pressed( [&]( const UiButtonRef & ) { play(); } ) );
	mConnections.push_back( btnEject->released( [&]( const UiButtonRef & ) { eject(); } ) );
	// mConnections.push_back( btnShutdown->released( [&]( const UiButtonRef& ) {
	// quit(); } ) );
	mConnections.push_back( mBtnVS->released( [&]( const UiButtonRef & ) {
		gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
	} ) );
	mConnections.push_back( mBtnFR60->released( [&]( const UiButtonRef & ) {
		if( isFrameRateEnabled() )
			disableFrameRate();
		else
			setFrameRate( 60.0f );
	} ) );
	mConnections.push_back( mBtnLoop->released( [&]( const UiButtonRef & ) { toggleLoop(); } ) );
	// mConnections.push_back( btnRewind->pressed( [&]( const UiButtonRef& ) {
	// rewind( true ); } ) );
	// mConnections.push_back( btnRewind->released( [&]( const UiButtonRef& ) {
	// rewind( false ); } ) );
	// mConnections.push_back( btnFastForward->pressed( [&]( const UiButtonRef& )
	// { fastForward( true ); } ) );
	// mConnections.push_back( btnFastForward->released( [&]( const UiButtonRef& )
	// { fastForward( false ); } ) );
	// mConnections.push_back( btnSeekToStart->released( [&]( const UiButtonRef& )
	// { seekTo( 0 ); } ) );
	// mConnections.push_back( btnSeekToEnd->released( [&]( const UiButtonRef& ) {
	// seekTo( FLT_MAX ); } ) );

	// Load label.
	mLabel = TextureCache::instance().load( loadAsset( "label.png" ) );
}

CINDER_APP( VideoTestApp, RendererGl, &VideoTestApp::prepare )
