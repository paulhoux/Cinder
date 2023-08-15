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
#include "cinder/ip/Blend.h"
#include "cinder/Rand.h"
#include "cinder/text/SystemFonts.h"
#include "cinder/Easing.h"
#include "Resources.h"

using namespace ci;
using namespace ci::app;
using namespace std;

text::Face	*gFace;
const text::Font	*gFontSmall, *gFontMedium, *gFontLarge;

struct Demo {
  public:
	virtual ~Demo() {}
	virtual void update() {}
	virtual void render( Surface8u *surface ) { }
	virtual void resize( ivec2 size ) {}
	virtual bool animated() const { return false; }
};

class TextDemosApp : public App {
 public:
	void setup() override;
	void update() override;
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
struct BasicRenderingDemo : public Demo {
	void render( Surface8u *surface ) override {
		auto str = text::AttrString() << gFontLarge << "Hello World";
		auto frame = text::Frame( str, surface->getWidth(), surface->getHeight(), text::TypesetOptions().ignoreLineMetrics( false ).defaultAlignment( text::Alignment::LEFT ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
		text::render( frame, surface, vec2{0}, false );
	}
};

struct PrecisionRenderingDemo : public Demo {
	void render( Surface8u *surface ) override {
		vector<const text::Font*> fonts;
		text::AttrString str, strPrecise;
		for( float size = 7.0f; size < 21.0f; size += 0.5f ) {
			fonts.push_back( text::font( gFace, size ) );
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

struct SrgbRenderingDemo : public Demo {
	void render( Surface8u *surface ) override {
		ip::fill( surface, ColorA8u( 255, 0, 0, 255 ) );
		vector<const text::Font*> fonts;
		text::AttrString str, strPrecise;
		for( float size = 7.0f; size < 21.0f; size += 0.5f ) {
			fonts.push_back( text::font( gFace, size ) );
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

struct PlaceholdersDemo : public Demo {
	Surface8u				mHeadshot;
	std::vector<Surface8u>	mFireFrames;
	std::vector<Surface8u>	mLightbulbFrames;
	std::vector<Surface8u>	mHeartFrames;

	PlaceholdersDemo::PlaceholdersDemo()
	{
		mHeadshot = loadImage( loadAsset( "paulhoux.png" ) );
		for( size_t i = 1; i <= 8; ++i )
			mFireFrames.push_back( loadImage( loadAsset( fs::path("fire") / ("frame." + to_string(i) + ".png") ) ) );
		for( size_t i = 1; i <= 40; ++i )
			mHeartFrames.push_back( loadImage( loadAsset( fs::path("heart") / ("frame." + to_string(i) + ".png") ) ) );
		for( size_t i = 1; i <= 16; ++i )
			mLightbulbFrames.push_back( loadImage( loadAsset( fs::path("lightbulb") / ("frame." + to_string(i) + ".png") ) ) );
	}

	void render( Surface8u *surface ) override {
		ip::fill( surface, ColorA8u( 32, 32, 32, 255 ) );
		const Surface8u* lightbulbFrame = &mLightbulbFrames[getElapsedFrames() / 3 % mLightbulbFrames.size()];
		const Surface8u* fireFrame = &mFireFrames[getElapsedFrames() / 3 % mFireFrames.size()];
		const Surface8u* heartFrame = &mHeartFrames[getElapsedFrames() / 3 % mHeartFrames.size()];

		text::AttrString str;
		str << text::font( gFace, 32.0f )
			<<	"In a realm where pixels dance and graphics soar,\n"
				"Paul Houx " << text::Placeholder( mHeadshot.getSize(), "!", &mHeadshot ) << "stands tall, with tales of lore.\n"
				"Eye-popping designs, jaw-dropping feats,\n"
				"His craft leaves onlookers glued to their seats.\n"
				"\n"
				"From games to displays, his creations unfold,\n"
				"With narratives bright, and stories untold.\n"
				"An artisan of code, C++ his quill,\n"
				"Transforming abstracts with unmatched skill.\n"
				"\n"
				"Complex ideas take life, stories ignite,\n"
				"With intuitive elegance, he brings them to light" << text::Placeholder( lightbulbFrame->getSize(), "!", (void*)lightbulbFrame ) << ".\n"
				"Visuals of data, or simulations that play,\n"
				"He captures the heart " <<  text::Placeholder( heartFrame->getSize(), " ", (void*)heartFrame ) << " in a striking display.\n"
				"\n"
				"Beyond just the visuals, a deeper connection,\n"
				"His works spark curiosity, a closer inspection.\n"
				"For with every project, a tale he does weave,\n"
				"Of dreams, of visions, that one must believe.\n"
				"\n"
				"Boundless in creativity, with horizons so wide,\n"
				"Paul seeks new challenges, with passion and pride.\n"
				"As you venture through his gallery, each frame,\n"
				"You'll sense the heartbeat, the soul, and the flame " << text::Placeholder( fireFrame->getSize(), "!", (void*)fireFrame ) << ".\n";

		auto frame = text::Frame( str, surface->getWidth(), surface->getHeight(), text::TypesetOptions().ignoreLineMetrics( false ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
		text::GlyphLayout layout = frame.getGlyphLayout();
		text::render( layout, surface, vec2{0}, true, true );
		// query the GlyphLayout for placeholders and render them
		for( text::PlaceholderInfo& placeholder : layout.getPlaceholders() ) {
			Surface8u *placeholderSurface = (Surface8u*)placeholder.getData();
			ip::blend( surface, *placeholderSurface, placeholderSurface->getBounds(), ivec2( placeholder.getBounds().getUpperLeft() ) );
		}
	}

	bool animated() const override { return true; }
};

struct TextOnPathDemo : public Demo {
	void render( Surface8u *surface ) override {
		ip::fill( surface, ColorA8u( 32, 32, 32, 255 ) );

		Path2d path;
		vec2 center = vec2( surface->getSize() ) / 2.0f; 
		path.moveTo( vec2( 20, center.y ) );
		path.curveTo( vec2( 200, center.y - 200 ), vec2( 300, center.y + 200 ), vec2( surface->getWidth(), center.y + 20 ) );
		path.curveTo( vec2( 200, center.y - 200 ), vec2( 300, center.y + 200 ), vec2( surface->getHeight() - 20, center.y + 120 ) );
		text::AttrString str;
		str << text::font( gFace, 32.0f )
			<<	"In a realm where pixels dance and graphics soar, "
				"Paul Houx stands tall, with tales of lore. ";
		str << text::font( gFace, 24.0f ) <<
				"Eye-popping designs, jaw-dropping feats, "
				"His craft leaves onlookers glued to their seats."
				"From games to displays, his creations unfold, "
				"With narratives bright, and stories untold.";
		str << text::font( gFace, 18.0f ) <<
				"An artisan of code, C++ his quill,"
				"Transforming abstracts with unmatched skill.";
		text::TextOnPath typesetting( str, path );
		text::render( typesetting, surface, vec2{0}, true, true );
	}
};

struct AlignmentDemo : public Demo {
	void render( Surface8u *surface ) override {
		ip::fill( surface, ColorA8u( 32, 32, 32, 255 ) );

		text::AttrString str;
		str << text::Alignment::JUSTIFIED;
		str << text::font( gFace, 32.0f )
			<<	"In a realm where pixels dance and graphics soar, "
				"Paul Houx stands tall, with tales of lore. ";
		str << text::font( gFace, 24.0f ) <<
				"Eye-popping designs, " << Color( 1.0f, 0.5f, 0.25f ) << "jaw-dropping feats, " << Color( 1.0f, 1.0f, 1.0f ) <<
				"His craft leaves onlookers glued to their seats."
				"From games to displays, his creations unfold, "
				"With narratives bright, and stories untold.";
		str << text::font( gFace, 18.0f ) <<
				"An artisan of code, C++ his quill,"
				"Transforming abstracts with unmatched skill.";

		text::Frame typesetting( str, surface->getWidth(), -1 );
		text::render( typesetting, surface, vec2{0}, true, true );
	}
};

struct SuperSubscriptDemo : public Demo {
	void render( Surface8u *surface ) override {
		ip::fill( surface, ColorA8u( 32, 32, 32, 255 ) );

		text::ShapingOptions shapeDefault;
		text::ShapingOptions super = text::ShapingOptions().superscript();
		text::ShapingOptions sub = text::ShapingOptions().subscript();
		text::AttrString str;
		str << text::font( "Cambria", 48.0f ) << "O" << sub << "3\n";
		str << "a" << super << "2" << shapeDefault << " + b" << super << "2" << shapeDefault << " = c" << super << "2";

		text::render( text::Frame( str ), surface, vec2{0} );
	}
};

struct BaselineOffsetDemo : public Demo {
	void render( Surface8u *surface ) override {
		ip::fill( surface, ColorA8u( 32, 32, 32, 255 ) );

		text::AttrString str;
		text::Font* normal = text::font( "Cambria", 48.0f );
		text::Font* small = text::font( "Cambria", 24.0f );
		str << normal << "Normal" << small << text::BaselineOffset::pixels( 20 ) << "Super" << text::BaselineOffset() << normal << " Normal" << small << text::BaselineOffset::em( -512 ) << "Sub";
		str << normal << "\nAnother string beneath these.";

		text::render( text::Frame( str ), surface, vec2{0} );
	}
};

void TextDemosApp::setup()
{
	//loadGlobalFonts( text::systemDefaultFace() );
	loadGlobalFonts( text::loadFace( "C:\\Windows\\Fonts\\Candarali.ttf" ) );

	mDemos.emplace_back( new BasicRenderingDemo() );
	mDemos.emplace_back( new PrecisionRenderingDemo() );
	mDemos.emplace_back( new SrgbRenderingDemo() );
	mDemos.emplace_back( new PlaceholdersDemo() );
	mDemos.emplace_back( new TextOnPathDemo() );
	mDemos.emplace_back( new AlignmentDemo() );
	mDemos.emplace_back( new SuperSubscriptDemo() );
	mDemos.emplace_back( new BaselineOffsetDemo() );
	
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

	mDemos[mCurrentDemoIdx]->render( &mSurface );	
	mTex = gl::Texture::create( mSurface, gl::Texture::Format().magFilter( GL_NEAREST ) );
}

void TextDemosApp::loadGlobalFonts( text::Face *face )
{
	gFace = face;

	gFontSmall = text::font( gFace, 12 );
	gFontMedium = text::font( gFace, 77 );
	gFontLarge = text::font( gFace, 99 );
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

void TextDemosApp::update()
{
	if( mDemos[mCurrentDemoIdx]->animated() ) {
		mDemos[mCurrentDemoIdx]->render( &mSurface );
		mTex->update( mSurface );
	}
}

void TextDemosApp::draw()
{
	gl::setMatricesWindow( getWindowSize() );
	gl::clear( Color( 0.25f, 0.5f, 0.5f ) );
	//gl::enableAlphaBlending();
	gl::enableAlphaBlendingPremult();
	
	gl::color( Color::white() );
	Rectf r( 0, 0, mTex->getWidth() * mZoom, mTex->getHeight() * mZoom );
	gl::draw( mTex, r );
}

CINDER_APP( TextDemosApp, RendererGl )
