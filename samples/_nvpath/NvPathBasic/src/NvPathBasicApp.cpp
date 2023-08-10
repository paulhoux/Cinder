#include "cinder/CanvasUi.h"
#include "cinder/Log.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/ip/Fill.h"
#include "cinder/nvp/Canvas.h"
#include "cinder/nvp/Primitives.h"
#include "cinder/text/Text.h"

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
	CanvasUi              mCanvasUi;
	nvp::Canvas           mCanvas{ 32, 16, false };
	nvp::Gradients        mGradients;
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

	mCanvasUi.connect( getWindow() );

	// Find and load texture from the sample data folder.
	const auto dataDir = Platform::get()->findFolders( "data" );
	for( const auto &dir : dataDir ) {
		if( fs::exists( dir / "CinderApp_ios.png" ) ) {
			mTexture = gl::Texture2d::create( loadImage( dir / "CinderApp_ios.png" ) );
			break;
		}
	}

	// Create linear gradient.
	auto linear = nvp::LinearGradient::create( "my_gradient" );
	linear->insert( 0, ColorA( 1, 0, 0, 1 ) );
	linear->insert( 0.25f, ColorA( 1, 1, 0, 1 ) );
	linear->insert( 0.5f, ColorA( 0, 1, 0, 1 ) );
	linear->insert( 0.75f, ColorA( 0, 1, 1, 1 ) );
	linear->insert( 1, ColorA( 0, 0, 1, 1 ) );
	linear->from( 0.5f, 0 );
	linear->to( 0.5f, 1 );
	linear->units( nvp::GradientUnits::OBJECT_BOUNDING_BOX );

	// Store gradients.
	mGradients.set( linear );
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
		nvp::ScopedCanvas     scpCanvas( mCanvas );
		gl::ScopedModelMatrix scpModel( mCanvasUi.getModelMatrix() );

		gl::ScopedBlendPremult scpBlend; // Important! Use either pre-multiplied alpha or additive blending.

		static const std::vector<std::function<void()>> sDispatch = {
			//
			[&]() {
				// Arc.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				auto  t = 0.5f + 0.5f * glm::sin( getElapsedSeconds() );
				float start = glm::mix( 0.0f, 85.0f, t );
				float end = glm::mix( 95.0f, 180.0f, t * 1.5f );

				nvp::Arc primitive( { 128, 128 }, 96, start, end );
				primitive.fill( Color::black() );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Circle.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::Circle primitive( { 128, 128 }, 96 );
				primitive.fill( Color::black() );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Ellipse.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::Ellipse primitive( { 128, 128 }, 96, 64 );
				primitive.fill( Color::black() );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Line.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::Line primitive( { 64, 64 }, { 192, 192 } );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Rectangle.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::Rectangle primitive( { 32, 32, 192, 192 } );
				primitive.fill( Color::black() );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Rounded rectangle.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::RoundedRectangle primitive( 32, 32, 192, 192, 16 );
				primitive.fill( Color::black() );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Star.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::Star primitive( 128, 128, 5, 96, 40 );
				primitive.fill( Color::black() );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Arrow.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::Arrow primitive( 32, 128, 224, 128, 16 );
				primitive.fill( Color::black() );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Spiral.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::Spiral primitive( { 128, 128 }, 0, 96, 16 );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 5 );
			},
			[&]() {
				// Dash caps.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				nvp::RoundedRectangle primitive( -64, 64, 256, 256, 16 );
				primitive.setDashOffset( -10 * getElapsedSeconds() );
				primitive.setDashPattern( { 30.0f, 15.0f } );
				primitive.setDashCaps( nvp::CapsStyle::ROUND, nvp::CapsStyle::TRIANGULAR );
				primitive.stroke( Color( 0.2f, 0.4f, 1.0f ), 10 );
			},
			[&]() {
				// Stencil demo.
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				// Render a circle to the stencil buffer.
				nvp::Circle primitive( { 128, 128 }, 540.0f / glm::radians( 360.0f ) );
				primitive.stencilFill();

				// Calculate the size and position of the texture from the circle's bounds.
				auto bounds = primitive.getBounds();
				auto width = mTexture->getAspectRatio() * bounds.getHeight();
				bounds.inflate( { width - bounds.getWidth(), 0 } );

				// Cover the circle with our texture.
				primitive.cover( mTexture, bounds );
			},
			[&]() {
				// Shape2d to nvp::Path
				nvp::ScopedClipRect scpClipRect( 0, 0, 256, 256 );

				// Create a Shape2d from a glyph.
				auto    face = text::loadSystemFace( "Arial" );
				auto    font = text::loadFont( face, 384 );
				Shape2d glyph = font->getGlyphShape( font->getCharIndex( u'a' ) );

				// Render the glyph.
				nvp::Path primitive( glyph );
				vec2      offset = vec2( 128 ) - primitive.getBounds().getCenter();

				gl::ScopedModelMatrix scpModelMatrix;
				gl::translate( offset );

				primitive.fill( mGradients, "my_gradient" );
				primitive.stroke( Color::black(), 5 );
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
	mCanvasUi.reset();
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
