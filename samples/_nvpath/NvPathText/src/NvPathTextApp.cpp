#include "cinder/CanvasUi.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/ip/Fill.h"
#include "cinder/nvp/NvPath.h"
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
	text::AttrString mTitleLeft;               //
	text::AttrString mTitleRight;              //
	text::AttrString mText;                    // Our sample text.
	text::AttrString mTextNoLineBreaks;        // Our sample text without line breaks.
	text::Frame      mTextFrame;               //
	text::TextOnPath mTextOnPath;              //
	ivec2            mColumnSize;              //
	vec2             mTextSize;                // The measured size of our text.
	gl::Texture2dRef mReference;               // Text will be rasterized to this texture.
	Path2d           mSpiral;                  // A spiral shape.
	float            mPosition{ 0.0f };        //
	float            mTarget{ 0.0f };          //
};

void NvPathTextApp::prepare( Settings *settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1200, 900 );
}

void NvPathTextApp::setup()
{
	if( !nvp::hasNvPathRendering() ) {
		console() << "This GPU does not support for NV path rendering. Try running this application on systems equipped with an NVIDIA GPU." << std::endl;
		return quit();
	}

	gl::enableVerticalSync();

	// Setup canvas Ui.
	mCanvasUi.connect( getWindow() );

	// Load font face.
	const auto title = text::loadSystemFace( "Georgia Italic" );
	const auto body = text::loadSystemFace( "Curlz MT" );

	// Create titles.
	mTitleLeft.clear();
	mTitleLeft << text::font( title, 14 ) << text::Alignment::CENTER;
	mTitleLeft << Color( 0, 0, 0 );
	mTitleLeft << "Software Rasterized using FreeType";

	mTitleRight.clear();
	mTitleRight << text::font( title, 14 ) << text::Alignment::CENTER;
	mTitleRight << Color( 0, 0, 0 );
	mTitleRight << "Real-time rendered using Path Rendering";

	// Create text.
	text::ShapingOptions sub = text::ShapingOptions().subscript();
	text::ShapingOptions super = text::ShapingOptions().superscript();
	text::ShapingOptions standard = text::ShapingOptions();

	mText.clear();
	mText << text::font( body, 32 ) << text::Alignment::CENTER << text::Leading::mult( 0.8f );
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

	mTextNoLineBreaks.clear();
	mTextNoLineBreaks << text::font( body, 32 ) << text::Alignment::CENTER << text::Leading::mult( 0.9f );
	mTextNoLineBreaks << Color( 0.8f, 0, 0 );
	mTextNoLineBreaks << "Upon vectors' wings, text comes alive, ";
	mTextNoLineBreaks << "Lines and curves, a dance to strive. ";
	mTextNoLineBreaks << Color( 0.6f, 0, 0 );
	mTextNoLineBreaks << "From abstract forms to words so clear, ";
	mTextNoLineBreaks << "Rendering tales for all to hear. ";
	mTextNoLineBreaks << Color( 0.4f, 0, 0 );
	mTextNoLineBreaks << "Intricate patterns, meticulously designed, ";
	mTextNoLineBreaks << "Every character, a story confined. ";
	mTextNoLineBreaks << Color( 0.2f, 0, 0 );
	mTextNoLineBreaks << "With precision and art, a visual feat, ";
	mTextNoLineBreaks << "Text transformed, in vectors' heartbeat.";
}

void NvPathTextApp::update()
{
	// Update position.
	mPosition += 0.1f * ( mTarget - mPosition );
	if( approxEqual( mPosition, mTarget ) )
		mPosition = mTarget;

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
		nvp::ScopedCanvas scpCanvas( mCanvas );

		// Important! Use either pre-multiplied alpha or additive blending.
		gl::ScopedBlendPremult scpBlend;

		gl::ScopedModelMatrix scpModel( mCanvasUi.getModelMatrix() );

		// Render text using path rendering.
		gl::translate( 0.5f * getWindowWidth(), 0 );
		text::Frame title( mTitleRight, mColumnSize.x, -1, text::TypesetOptions().topLineOffset( 10 ) );
		nvp::renderText( title );

		gl::translate( mPosition * getWindowWidth(), 0 );
		nvp::renderText( mTextFrame );
		nvp::renderText( mTextOnPath );
	}

	// Our canvas has gone out of scope, so now we can render it to the main window.
	gl::ScopedBlendPremult scpBlend;
	mCanvas.draw();

	// Render reference text on the left.
	gl::ScopedModelMatrix scpModel( mCanvasUi.getModelMatrix() );
	gl::ScopedColor       scpColor( 1, 1, 1 );
	gl::enableAlphaBlending();
	gl::draw( mReference );
}

void NvPathTextApp::resize()
{
	mCanvas.resize( getWindowSize() );
	mCanvasUi.reset();

	//
	mColumnSize.x = getWindowWidth() / 2;
	mColumnSize.y = getWindowHeight();

	// Measure text for later.
	text::Frame typesetter( mText, mColumnSize.x, mColumnSize.y );
	mTextSize.x = typesetter.getGlyphLayout().getMeasuredWidth();
	mTextSize.y = typesetter.getGlyphLayout().getMeasuredHeight();

	// Update spiral shape.
	constexpr auto leading = 35;
	const auto     radius = 0.45f * float( glm::min( mColumnSize.x, mColumnSize.y * 6 / 10 - 2 * leading ) );
	mSpiral = Path2d::spiral( vec2( 0.25f, 0.7f ) * vec2( getWindowSize() ), 0.3f * radius, radius, leading );

	// Render reference texture using FreeType software rasterizer.
	Surface surface( mColumnSize.x, mColumnSize.y, true );
	ip::fill( &surface, ColorA( 0, 0, 0, 0.15f ).premultiplied() );

	text::Frame title( mTitleLeft, mColumnSize.x, -1, text::TypesetOptions().topLineOffset( 10 ) );
	text::render( title, &surface, {}, true, true );

	mTextFrame = text::Frame( mText, mColumnSize.x, mColumnSize.y * 4 / 10, text::TypesetOptions().topLineOffset( ( 0.4f * float( mColumnSize.y ) - mTextSize.y ) * 0.5f ) );
	text::render( mTextFrame, &surface, vec2{}, true, true );

	mTextOnPath = text::TextOnPath( mTextNoLineBreaks, mSpiral );
	text::render( mTextOnPath, &surface, vec2{}, true, true );

	mReference = gl::Texture2d::create( surface, gl::Texture2d::Format().minFilter( GL_LINEAR ).magFilter( GL_NEAREST ) );
}

void NvPathTextApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_SPACE:
		mTarget = mTarget < 0 ? 0 : -0.5f;
		break;
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
