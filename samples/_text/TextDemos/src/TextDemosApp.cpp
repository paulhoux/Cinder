#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/text/Text.h"
#include "cinder/text/AttrString.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"
#include "cinder/GeomIo.h"
#include "cinder/Log.h"
#include "cinder/ip/Fill.h"
#include "cinder/Rand.h"
#include "cinder/text/SystemFonts.h"
#include "cinder/Easing.h"
#include "Resources.h"

using namespace ci;
using namespace ci::app;
using namespace std;

text::Face	*gFace;
const text::Font	*gFontSmall, *gFontMedium, *gFontLarge;

class Demo {
  public:
	virtual ~Demo() {}
	virtual void update() {}
	virtual void render( Surface8u *surface ) { }
	virtual void resize( ivec2 size ) {}
};

class TextDemosApp : public App {
 public:
	void setup() override;
	void draw() override;
	void loadGlobalFonts( text::Face *face );
	void fileDrop( FileDropEvent event ) override;
	void keyDown( KeyEvent event ) override;
	void resize() override;
	
	vector<unique_ptr<Demo>>	mDemos;
	size_t						mCurrentDemoIdx = 0;

	gl::TextureRef 	mTex;
	unique_ptr<Surface8u>		mBackgroundSurface;
	Surface8u					mSurface;
	bool			mDrawLines = false;
	int							mZoom = 1;
};

/// Basic Rendering Demo
class BasicRenderingDemo : public Demo {
	void render( Surface8u *surface ) override {
		auto str = text::AttrString() << gFontLarge << "Hello World";
		auto frame = text::Frame( str, surface->getWidth(), surface->getHeight(), text::TypesetOptions().ignoreLineMetrics( false ).defaultAlignment( text::Alignment::LEFT ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
		text::render( frame, surface, vec2{0}, false );
	}
};

class PrecisionRenderingDemo : public Demo {
	void render( Surface8u *surface ) override {
		vector<const text::Font*> fonts;
		text::AttrString str, strPrecise;
		for( float size = 7.0f; size < 21.0f; size += 0.5f ) {
			fonts.push_back( text::loadFont( gFace, size ) );
			str << fonts.back() << "Waltz, bad nymph, for quick jigs vex! 0123456789\n";
			strPrecise << fonts.back() << "Waltz, bad nymph, for quick jigs vex! 0123456789\n";
		}

		auto frame = text::Frame( str, text::Frame::GROW, surface->getHeight(), text::TypesetOptions().ignoreLineMetrics( false ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
		auto frameLayout = frame.getGlyphLayout();
		text::render( frame, surface, vec2{0}, false );
		auto framePrecise = text::Frame( str, text::Frame::GROW, surface->getHeight(), text::TypesetOptions().ignoreLineMetrics( false ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
		text::render( framePrecise, surface, vec2{ frameLayout.getMeasuredWidth() + 30, 0 }, true );
	}
};

class SrgbRenderingDemo : public Demo {
	void render( Surface8u *surface ) override {
		ip::fill( surface, ColorA8u( 255, 0, 0, 255 ) );
		vector<const text::Font*> fonts;
		text::AttrString str, strPrecise;
		for( float size = 7.0f; size < 21.0f; size += 0.5f ) {
			fonts.push_back( text::loadFont( gFace, size ) );
			str << Color8u( 0, 255, 0 );
			str << fonts.back() << "Waltz, bad nymph, for quick jigs vex! 0123456789\n";
			strPrecise << fonts.back() << "Waltz, bad nymph, for quick jigs vex! 0123456789\n";
		}

		auto frame = text::Frame( str, text::Frame::GROW, surface->getHeight(), text::TypesetOptions().ignoreLineMetrics( false ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
		auto frameLayout = frame.getGlyphLayout();
		text::render( frame, surface, vec2{0}, true, false );
		auto framePrecise = text::Frame( str, text::Frame::GROW, surface->getHeight(), text::TypesetOptions().ignoreLineMetrics( false ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
		text::render( frame, surface, vec2{ frameLayout.getMeasuredWidth() + 30, 0 }, true, true );
	}
};

class SpacersDemo : public Demo {
	void render( Surface8u *surface ) override {
		// create placeholder image
		Surface8u placeholderImage( 200, 300, false );
		ip::fill( &placeholderImage, ColorA8u( 255, 128, 64, 255 ) );
		ip::fill( surface, ColorA8u( 32, 32, 32, 255 ) );
		text::AttrString str;
//		str << text::loadFont( gFace, 30.0f ) << Color8u( 255, 0, 255 ) << "A placeholder:" << Color8u( 255, 255, 0 ) << text::Spacer( {22, 0} ) << "done";
		str << text::loadFont( gFace, 100.0f ) << Color8u( 255, 0, 255 ) << "A placeholder:" << text::Placeholder( {placeholderImage.getWidth(), placeholderImage.getHeight()}, "hello" ) << " done";
		//str << text::loadFont( gFace, 160.0f ) << Color8u( 255, 0, 255 ) << "One two three four five";
		auto frame = text::Frame( str, surface->getWidth(), surface->getHeight(), text::TypesetOptions().ignoreLineMetrics( false ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
		auto frameLayout = frame.getGlyphLayout();
		text::render( frameLayout, surface, vec2{0}, true, true );
	}
};

void TextDemosApp::setup()
{
	loadGlobalFonts( text::systemDefaultFace() );
	//loadGlobalFonts( text::loadFace( "C:\\Windows\\Fonts\\Candarali.ttf" ) );

	mDemos.emplace_back( new BasicRenderingDemo() );
	mDemos.emplace_back( new PrecisionRenderingDemo() );
	mDemos.emplace_back( new SrgbRenderingDemo() );
	mDemos.emplace_back( new SpacersDemo() );

	mCurrentDemoIdx = mDemos.size() - 1;
}

void drawFrameLines( const text::GlyphLayout &layout, Surface8u *surface )
{
	bool drawLineMetrics = true;

	for( auto &line : layout.getLines() ) {
		if( drawLineMetrics ) {
			ip::fill( surface, Color8u( 255, 255, 0 ), Area( 0, (int32_t)(line.getDrawOffset().y - line.getAscender()), (int32_t)(surface->getWidth()), (int32_t)(line.getDrawOffset().y - line.getAscender() + 1) ) );
			ip::fill( surface, Color8u( 0, 0, 255 ), Area( 0, (int32_t)(line.getDrawOffset().y + line.getDescender()), (int32_t)(surface->getWidth()), (int32_t)(line.getDrawOffset().y + line.getDescender() + 1) ) );
			ip::fill( surface, Color8u( 0, 255, 0 ), Area( 0, (int32_t)(line.getDrawOffset().y), surface->getWidth(), (int32_t)(line.getDrawOffset().y + 1) ) );
		}

		for( auto &run : line.getRuns() ) {
			float drawOffsetX = line.getDrawOffset().x + run.getDrawOffset().x;
			ip::fill( surface, Color8u( 255, 128, 64 ), Area( drawOffsetX + 1, line.getDrawOffset().y, drawOffsetX + 2, line.getDrawOffset().y - line.getAscender() ) );
			ip::fill( surface, Color8u( 128, 64, 32 ), Area( (int32_t)ceilf(drawOffsetX + run.getMeasuredWidth()), line.getDrawOffset().y, (int32_t)ceilf( drawOffsetX + run.getMeasuredWidth() ) + 1, line.getDrawOffset().y - line.getAscender() ) );
		}
	}

	ip::fill( surface, Color8u( 200, 64, 0 ), Area( 0, layout.getMeasuredHeight(), layout.getMeasuredWidth(), layout.getMeasuredHeight() + 1 ) );
	ip::fill( surface, Color8u( 200, 64, 0 ), Area( layout.getMeasuredWidth(), 0, layout.getMeasuredWidth() + 1, layout.getMeasuredHeight() ) );
}

void TextDemosApp::resize()
{
	mDemos[mCurrentDemoIdx]->resize( getWindowSize() );
	mSurface = Surface8u( getWindowWidth(), getWindowHeight(), true );
	mSurface.setPremultiplied( true );
	ip::fill( &mSurface, ColorA8u( 0, 0, 0, 0 ) );
	mDemos[mCurrentDemoIdx]->render( &mSurface );
	
	mTex = gl::Texture::create( mSurface, gl::Texture::Format().magFilter( GL_NEAREST ) );
}

void TextDemosApp::loadGlobalFonts( text::Face *face )
{
	gFace = face;

	gFontSmall = text::loadFont( gFace, 12 );
	gFontMedium = text::loadFont( gFace, 77 );
	gFontLarge = text::loadFont( gFace, 99 );
}

void TextDemosApp::fileDrop( FileDropEvent event )
{
	string ext = event.getFile( 0 ).extension().string();
	if( ext == ".png" || ext == ".jpg" )
		mBackgroundSurface = make_unique<Surface8u>( loadImage( event.getFile( 0 ) ) );
	else if( ext == ".ttf" || ext == ".otf" )
		loadGlobalFonts( text::loadFace( event.getFile( 0 ) ) );

	resize();
}

void TextDemosApp::keyDown( KeyEvent event )
{
	if( event.getChar() == 's' ) {
		writeImage( getHomeDirectory() / "textDemosOut.png", mSurface );
	}
	else if( event.getChar() == '-' )  {
		mZoom = max<int>( 1, mZoom -1 );
	}
	else if( event.getChar() == '+' || event.getChar() == '=' ) {
		mZoom += 1;
	}
}

void TextDemosApp::draw()
{
	gl::setMatricesWindow( getWindowSize() );
	gl::clear( Color( 0.5f, 0.5f, 0.5f ) );
	//gl::enableAlphaBlending();
	gl::enableAlphaBlendingPremult();
	
	gl::color( Color::white() );
	Rectf r( 0, 0, mTex->getWidth() * mZoom, mTex->getHeight() * mZoom );
	gl::draw( mTex, r );
}

CINDER_APP( TextDemosApp, RendererGl )
