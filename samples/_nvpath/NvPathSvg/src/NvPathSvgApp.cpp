#include "cinder/CanvasUi.h"
#include "cinder/Log.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/nvp/NvPath.h"
#include "cinder/svg/Svg.h"
#include "cinder/svg/SvgGl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class NvPathSvgApp : public App {
  public:
	using Engine = enum { NVP, GL };

	void setup() override;
	void update() override;
	void draw() override;
	void resize() override;
	void keyDown( KeyEvent event ) override;
	void fileDrop( FileDropEvent event ) override;

	bool loadSvgFile( const fs::path &file );
	void loadPreviousSvgFile();
	void loadNextSvgFile();

  private:
	Engine      mEngine = NVP;
	CanvasUi    mCanvasUi;
	nvp::Canvas mCanvas;
	nvp::Svg    mSvg;
	svg::DocRef mDoc;
	fs::path    mFilePath;
	bool        mCaptureScreenshot = false;
};

void NvPathSvgApp::setup()
{
	disableFrameRate();

	mCanvasUi.connect( getWindow() );
}

void NvPathSvgApp::update()
{
	// Show application name and frame rate in the window title bar.
	std::string name = app::getAppPath().stem().string();

	// Use filename instead if available.
	if( !mFilePath.empty() )
		name = mFilePath.stem().string();

	std::string title;
	title.resize( 255 );
	snprintf( title.data(), title.size(), "%s (%.0f FPS)", name.c_str(), static_cast<double>( getAverageFps() ) );

	getWindow()->setTitle( title );
}

void NvPathSvgApp::draw()
{
	gl::FboRef fbo;
	if( mCaptureScreenshot ) {
		fbo = gl::Fbo::create( getWindowWidth(), getWindowHeight(), gl::Fbo::Format().disableDepth().stencilBuffer( false ) );
		fbo->bindFramebuffer();
	}

	gl::clear( Color::white() );

	if( mDoc && mEngine == GL ) {
		gl::ScopedModelMatrix scpModel( mCanvasUi.getModelMatrix() );

		// Scale SVG to window.
		if( mDoc->getWidth() > 0 && mDoc->getHeight() > 0 ) {
			auto bounds = Area::proportionalFit( Area( mDoc->getBounds() ), getWindowBounds(), true, true );
			auto scale = vec2( bounds.getSize() ) / vec2( mDoc->getSize() );
			auto offset = bounds.getUL();

			gl::translate( offset );
			gl::scale( scale );
		}

		try {
			mDoc->render( SvgRendererGl() );
		}
		catch( ... ) {
		}
	}
	else if( mEngine == NVP ) {
		{
			nvp::ScopedCanvas scpCanvas( mCanvas );

			gl::ScopedModelMatrix scpModel( mCanvasUi.getModelMatrix() );

			// Scale SVG to canvas.
			if( mSvg.getWidth() > 0 && mSvg.getHeight() > 0 ) {
				auto bounds = Area::proportionalFit( Area( mSvg.getBounds() ), mCanvas.getBounds(), true, true );
				auto scale = vec2( bounds.getSize() ) / vec2( mSvg.getSize() );
				auto offset = bounds.getUL();

				gl::translate( offset );
				gl::scale( scale );
			}

			mSvg.draw();
		}

		// Use pre-multiplied alpha!
		gl::ScopedBlendPremult scpBlend;
		gl::ScopedColor        scpColor( 1, 1, 1 );

		mCanvas.draw();
	}

	if( mCaptureScreenshot ) {
		fbo->unbindFramebuffer();

		auto surface = fbo->readPixels8u( fbo->getBounds(), GL_COLOR_ATTACHMENT0 );
		auto now = std::chrono::system_clock::to_time_t( std::chrono::system_clock::now() );

		std::string filename( 64, '\0' );
		std::strftime( filename.data(), filename.size(), "%Y-%m-%d %H.%M.%S.png", std::localtime( &now ) );

		ci::fs::path folder = app::getAppPath();
		writeImage( cinder::writeFile( folder / filename ), surface, ImageTarget::Options(), "png" );

		mCaptureScreenshot = false;
	}
}

void NvPathSvgApp::resize()
{
	mCanvasUi.resize( getWindowSize() );
	mCanvas.resize( getWindowSize() );
}

void NvPathSvgApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_F1:
		mEngine = NVP;
		break;
	case KeyEvent::KEY_F2:
		mEngine = GL;
		break;
	case KeyEvent::KEY_f:
		setFullScreen( !isFullScreen() );
		break;
	case KeyEvent::KEY_v:
		gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
		break;
	case KeyEvent::KEY_LEFT:
		loadPreviousSvgFile();
		break;
	case KeyEvent::KEY_RIGHT:
		loadNextSvgFile();
		break;
	case KeyEvent::KEY_UP:
	case KeyEvent::KEY_DOWN:
		if( !mFilePath.empty() )
			loadSvgFile( mFilePath );
		break;
	case KeyEvent::KEY_ESCAPE:
		if( isFullScreen() )
			setFullScreen( false );
		else
			quit();
		break;
	case KeyEvent::KEY_KP_PLUS:
		mCaptureScreenshot = !mCaptureScreenshot;
		break;
	default:
		App::keyDown( event );
		break;
	}
}

void NvPathSvgApp::fileDrop( FileDropEvent event )
{
	mFilePath.clear();

	for( const auto &file : event.getFiles() ) {
		if( loadSvgFile( file ) )
			break;
	}
}

bool NvPathSvgApp::loadSvgFile( const fs::path &file )
{
	Timer t( true );

	if( file.extension() == ".svgz" )
		mDoc = svg::Doc::createFromSvgz( loadFile( file ) );
	else if( file.extension() == ".svg" )
		mDoc = svg::Doc::create( loadFile( file ) );
	else
		return false;

	CI_LOG_I( "Parsing SVG took " << t.getSeconds() << "s." );

	mFilePath = file;

	mCanvasUi.reset();

	t.start();
	mSvg = nvp::Svg( mDoc );
	CI_LOG_I( "Preparing SVG for path rendering took " << t.getSeconds() << "s." );

	return true;
}

void NvPathSvgApp::loadPreviousSvgFile()
{
	if( mFilePath.empty() )
		return;

	const auto            parent = mFilePath.parent_path();
	std::vector<fs::path> files;

	fs::directory_iterator it( parent );
	while( it != fs::directory_iterator() ) {
		files.push_back( it->path() );
		++it;
	}

	auto itr = std::find_if( files.begin(), files.end(), [this]( const fs::path &file ) { return file == mFilePath; } );
	while( itr != files.begin() && itr != files.end() ) {
		--itr;
		if( loadSvgFile( *itr ) )
			break;
	}
}

void NvPathSvgApp::loadNextSvgFile()
{
	if( mFilePath.empty() )
		return;

	const auto            parent = mFilePath.parent_path();
	std::vector<fs::path> files;

	fs::directory_iterator it( parent );
	while( it != fs::directory_iterator() ) {
		files.push_back( it->path() );
		++it;
	}

	auto itr = std::find_if( files.begin(), files.end(), [this]( const fs::path &file ) { return file == mFilePath; } );
	while( ++itr != files.end() ) {
		if( loadSvgFile( *itr ) )
			break;
	}
}

CINDER_APP( NvPathSvgApp, RendererGl( RendererGl::Options() ) )
