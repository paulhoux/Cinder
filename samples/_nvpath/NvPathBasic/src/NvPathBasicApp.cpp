#include "cinder/Log.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/ip/Fill.h"
#include "cinder/nvpath/NvPath.h"

using namespace ci;
using namespace app;
using namespace std;

class NvPathBasicApp : public App {
  public:
	static void prepare( Settings *settings );

	void setup() override;
	void update() override;
	void draw() override;

	void resize() override;

	void keyDown( KeyEvent event ) override;

  private:
	nvpath::Canvas        mCanvas{ 32, 16, false };
	std::vector<fs::path> mFiles;
	gl::Texture2dRef      mTexture;
};

void NvPathBasicApp::prepare( Settings *settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1880, 1000 );
}

void NvPathBasicApp::setup()
{
	gl::enableVerticalSync();
}

void NvPathBasicApp::update()
{
	std::string name = app::getAppPath().stem().string();
	std::string file = mFiles.size() == 1 ? mFiles.back().string() : "";

	std::string title;
	title.resize( 255 );
	snprintf( title.data(), title.size(), "%s (%.0f FPS) %s", name.c_str(), static_cast<double>( getAverageFps() ), file.c_str() );

	getWindow()->setTitle( title );
}

void NvPathBasicApp::draw()
{
	gl::clear( Color::white() );

	// Render to our canvas in its own scope.
	{
		nvpath::ScopedCanvas scpCanvas( mCanvas );

		gl::ScopedBlendPremult scpBlend; // Important! Use either pre-multiplied alpha or additive blending.

		static const std::vector<std::function<void()>> sDispatch = {
			//
			[&]() {
				// Circle.
				nvpath::Path primitive( Path2d::circle( { 128, 128 }, 96 ) );
				primitive.fill( Color::black() );
				primitive.setStrokeWidth( 5 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
			[&]() {
				// Ellipse.
				nvpath::Path primitive( Path2d::ellipse( { 128, 128 }, 96, 64 ) );
				primitive.fill( Color::black() );
				primitive.setStrokeWidth( 5 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
			[&]() {
				// Line.
				nvpath::Path primitive( Path2d::line( { 64, 64 }, { 192, 192 } ) );
				primitive.setStrokeWidth( 5 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
			[&]() {
				// Rectangle.
				nvpath::Path primitive( Path2d::rectangle( { 32, 32, 192, 192 } ) );
				primitive.fill( Color::black() );
				primitive.setStrokeWidth( 5 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
			[&]() {
				// Rounded rectangle.
				nvpath::Path primitive( Path2d::roundedRectangle( 32, 32, 192, 192, 16 ) );
				primitive.fill( Color::black() );
				primitive.setStrokeWidth( 5 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
			[&]() {
				// Star.
				nvpath::Path primitive( Path2d::star( { 128, 128 }, 5, 96, 40 ) );
				primitive.fill( Color::black() );
				primitive.setStrokeWidth( 5 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
			[&]() {
				// Arrow.
				nvpath::Path primitive( Path2d::arrow( { 32, 128 }, { 224, 128 }, 16 ) );
				primitive.fill( Color::black() );
				primitive.setStrokeWidth( 5 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
			[&]() {
				// Spiral.
				nvpath::Path primitive( Path2d::spiral( { 128, 128 }, 0, 96, 16, -5 * getElapsedSeconds() ) );
				primitive.setStrokeWidth( 5 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
			[&]() {
				// Dash caps.
				nvpath::Path primitive( Path2d::roundedRectangle( 32, 32, 192, 192, 16 ) );
				primitive.setClientLength( 900.0f ); // Precisely fits 20 shapes along the path.
				primitive.setDashOffset( -10 * getElapsedSeconds() );
				primitive.setDashPattern( { 30.0f, 15.0f } );
				primitive.setDashCaps( nvpath::CapsStyle::ROUND, nvpath::CapsStyle::TRIANGULAR );
				primitive.setStrokeWidth( 10 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ) );
			},
		};

		// Render all scenes in a grid.
		const auto rows = glm::floor( glm::sqrt( sDispatch.size() ) );
		const auto cols = glm::ceil( double( sDispatch.size() ) / rows );
		const auto width = getWindowWidth() / cols;
		const auto height = getWindowHeight() / rows;

		int index = 0;
		for( const auto &func : sDispatch ) {
			constexpr int size = 256;
			const auto    scale = glm::min( float( width ) / size, float( height ) / size );
			const auto    offset = vec2( index % int( cols ) * width, index / int( cols ) * height ) + 0.5f * ( vec2( width, height ) - scale * vec2{ size } ) - scale * vec2{ 0 };

			gl::ScopedModelMatrix m;
			gl::translate( offset );
			gl::scale( scale, scale );

			func();

			++index;
		}
	}
	{
		// Our canvas has gone out of scope, so now we can render it to the main window.
		gl::ScopedBlendPremult scpBlend;

		mCanvas.draw();
	}
}

void NvPathBasicApp::resize()
{
	mCanvas.resize( getWindowSize() );
}

void NvPathBasicApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_ESCAPE:
		if( isFullScreen() )
			setFullScreen( false );
		else
			quit();
		break;
	case KeyEvent::KEY_f:
		setFullScreen( !isFullScreen() );
		break;
	case KeyEvent::KEY_v:
		gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
		break;
	default:
		return;
	}
}

CINDER_APP( NvPathBasicApp, RendererGl( RendererGl::Options().colorChannelDepth( 8 ) ), &NvPathBasicApp::prepare )
