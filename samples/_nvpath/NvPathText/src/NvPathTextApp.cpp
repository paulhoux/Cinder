#include "cinder/CanvasUi.h"
#include "cinder/Log.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/ip/Fill.h"
#include "cinder/nvp/Canvas.h"
#include "cinder/nvp/NvpFace.h"
#include "cinder/nvp/NvpFont.h"
#include "cinder/nvp/Primitives.h"
#include "cinder/text/Text.h"

using namespace ci;
using namespace app;
using namespace std;

class NvPathTextApp : public App {
  public:
	static void prepare( Settings *settings );

	void setup() override;
	void update() override;
	void draw() override;

	void resize() override;

	void keyDown( KeyEvent event ) override;

  private:
	CanvasUi         mCanvasUi;                // Pan & zoom the contents of the window.
	nvp::Canvas      mCanvas{ 32, 16, false }; // Creates a frame buffer with stencil buffer, required by NVP.
	text::AttrString mText;                    // Our sample text.
	vec2             mTextSize;                // The measured size of our text.
	gl::Texture2dRef mReference;               // Text will be rasterized to this texture.
	Path2d           mSpiral;                  // A spiral shape.
};

void NvPathTextApp::prepare( Settings *settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1880, 1000 );
}

void NvPathTextApp::setup()
{
	gl::enableVerticalSync();

	// Setup canvas Ui.
	mCanvasUi.connect( getWindow() );

	// Load font face.
	const auto body = text::loadSystemFace( "Curlz MT" );

	// Create text.
	mText.clear();
	mText << text::loadFont( body, 32 ) << text::Alignment::CENTER << text::Leading::mult( 0.8f );
	mText << Color( 0.8f, 0, 0 );
	mText << "Upon vectors' wings, text comes alive,\n";
	mText << "Lines and curves, a dance to strive.\n";
	mText << Color( 0.6f, 0, 0 );
	mText << "From abstract forms to words so clear,\n";
	mText << "Rendering tales for all to hear.\n";
	mText << Color( 0.4f, 0, 0 );
	mText << "Intricate patterns, meticulously designed,\n";
	mText << "Every character, a story confined.\n";
	mText << Color( 0.2f, 0, 0 );
	mText << "With precision and art, a visual feat,\n";
	mText << "Text transformed, in vectors' heartbeat.";
}

void NvPathTextApp::update()
{
	// Show application name and frame rate in the window title bar.
	std::string name = app::getAppPath().stem().string();

	std::string title;
	title.resize( 255 );
	snprintf( title.data(), title.size(), "%s (%.0f FPS)", name.c_str(), static_cast<double>( getAverageFps() ) );

	getWindow()->setTitle( title );
}

void NvPathTextApp::draw()
{
	gl::clear( Color::white() );

	// Render to our canvas in its own scope.
	{
		nvp::ScopedCanvas     scpCanvas( mCanvas );
		gl::ScopedModelMatrix scpModel( mCanvasUi.getModelMatrix() );

		gl::ScopedBlendPremult scpBlend; // Important! Use either pre-multiplied alpha or additive blending.

		// Render text.
		nvp::NvpTextProcessor text;
		text::typeset( mText, 0.5f * getWindowWidth(), 0.5f * getWindowHeight(), text, text::TypesetOptions().topLineOffset( ( 0.5f * getWindowHeight() - mTextSize.y ) * 0.5f ) );

		nvp::NvpTextOnPath textOnPath( mSpiral, 0 );
		text::typeset( mText, -1, -1, textOnPath, text::TypesetOptions() );

		// Render spiral for reference.
		auto spiral = nvp::Path( mSpiral );
		spiral.setDashPattern( { 3, 6 } );
		spiral.stroke( Color( 0, 0, 0 ), 1 );
	}

	{
		// Our canvas has gone out of scope, so now we can render it to the main window.
		gl::ScopedBlendPremult scpBlend;

		mCanvas.draw();
	}

	// To check if NVP renders text correctly, render to the right half of the window using the default rasterizer.
	gl::ScopedModelMatrix scpModel;
	gl::translate( 0.5f * getWindowWidth(), 0 );

	gl::ScopedColor color( 1, 1, 1 );
	gl::draw( mReference );

	// Render spiral for reference.
	gl::ScopedBlendAlpha scpBlend;
	gl::color( 0, 0, 0, 0.25f );
	gl::draw( mSpiral );
}

void NvPathTextApp::resize()
{
	mCanvas.resize( getWindowSize() );
	mCanvasUi.reset();

	// Measure text for later.
	text::Frame typesetter( mText, 0.5f * getWindowWidth(), 0.5f * getWindowHeight() );
	mTextSize.x = typesetter.getGlyphLayout().getMeasuredWidth();
	mTextSize.y = typesetter.getGlyphLayout().getMeasuredHeight();

	// Update spiral shape.
	const auto size = glm::min( 0.5f * getWindowWidth(), 0.5f * getWindowHeight() );
	mSpiral = Path2d::spiral( vec2( 0.25f, 0.75f ) * vec2( getWindowSize() ), 0.10f * size, 0.45f * size, 35 /* line gap */ );

	// Render reference texture.
	Surface surface( 0.5f * getWindowWidth(), getWindowHeight(), true );
	ip::fill( &surface, ColorA( 0, 0, 0, 0.15f ) );

	text::Frame text( mText, 0.5f * getWindowWidth(), 0.5f * getWindowHeight(), text::TypesetOptions().topLineOffset( ( 0.5f * getWindowHeight() - mTextSize.y ) * 0.5f ) );
	text::render( text, &surface, vec2{}, false, true, true );

	text::TextOnPath textOnPath( mText, mSpiral );
	text::render( textOnPath, &surface, vec2{}, false, true, false );

	mReference = gl::Texture2d::create( surface );
}

void NvPathTextApp::keyDown( KeyEvent event )
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

CINDER_APP( NvPathTextApp, RendererGl( RendererGl::Options().colorChannelDepth( 8 ) ), &NvPathTextApp::prepare )
