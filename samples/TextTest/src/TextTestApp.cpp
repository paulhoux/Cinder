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

class TextTestApp : public App {
 public:
	void setup() override;
	void draw() override;
	void loadFonts( text::Face *face );
	void fileDrop( FileDropEvent event ) override;
	void keyDown( KeyEvent event ) override;
	void resize() override;
	void renderTexture( int width, int height );
	void updateExtremeTest();
	void animate( const text::Frame &frame );
	void updateAnimatedTest();

	text::Face		*mFace, *mEmojiFace;
	const text::Font		*mFontSmall, *mEmojiFont, *mFontMedium, *mFontLarge;
	
	gl::TextureRef 	mTex;
	Shape2d			mGlyphShape;
	unique_ptr<Surface8u>		mBackgroundSurface;
	Surface8u					mSurface;
	bool			mDrawLines = false;
	bool			mExtremeTesting = false;
	bool			mPreciseRendering = false;

	bool			mAnimatedTesting = false;
	double			mAnimatedTestStartTime;
	text::Frame		mAnimatedTestFrame;
};

void printFontNames()
{
	vector<string> names = text::Manager::get()->getSystemFaceNames();
	for( auto &name : names )
		console() << name << endl;
}

void printFaceInfo( const text::Face *face )
{
	console() << "Family: '" << face->getFamilyName() << "'  Style: '" << face->getStyleName() << "'  Total Glyphs: " << face->getNumGlyphs()
				<< "  Color: " << (face->hasColor()  ? "true" : "false");
	console() << "  Fixed sizes: { ";
	for( auto sz : face->getFixedSizes() )
		console() << sz << " ";
	console() << "}" << std::endl;
	console() << "  Multiple Masters: { " << std::endl;
	if( face->hasVariations() ) {
		console() << "   Axes: ";
		for( auto &axis : face->getVariationAxes() )
			console() << "[\"" << axis.getName() << "\" min: " << axis.getMinimum() << " default: " << axis.getDefault() << " maximum: " << axis.getMaximum() << "]";
		console() << std::endl << "   Named Styles: " << std::endl;
		for( auto &style : face->getVariationNamedStyles() ) {
			console() << "    \"" << style.getName() << "\"" << std::endl;
			console() << "      " << style.getVariation() << std::endl;
		}
	}
	else
		console() << "   None";
	console() << std::endl << "}" << std::endl;
//	console() << "  Ascender: " << face->getAscender() << "  Descender: " << face->getDescender() << "  Height: " << face->getHeight() << std::endl;
}

void printFontInfo( const text::Font *font )
{
	console() << "  Ascender: " << font->getAscender() << "  Descender: " << font->getDescender() << "  Height: " << font->getHeight() << " Line Gap: " << font->getLineGap() << std::endl;
}

void printLineBreaks( std::string s )
{
	std::vector<char> breaks( s.size() );
	text::setLineBreaksUtf8( s.c_str(), s.length(), breaks.data() );

	console() << "{" << s << "}" << std::endl;
	console() << "{";
	for( size_t c = 0; c < s.length(); ++c ) {
		// 0: must break, 1: allow break, 2: cannot break, 3: inside utf8/utf16 sequence, 4: indeterminate
		const char m[5] = { '|', ' ', '!', '_', '?' };
		console() << m[breaks[c]];
	}
	console() << "}" << std::endl;
}

void printWordBreaks( std::string s )
{
	std::vector<char> breaks( s.size() );
	text::setWordBreaksUtf8( s.c_str(), s.length(), breaks.data() );

	console() << "{" << s << "}" << std::endl;
	console() << "{";
	for( size_t c = 0; c < s.length(); ++c ) {
		// 0: can break, 1: allow break, 2: inside utf8/utf16 sequence
		const char m[4] = { '-', '!', '_' };
		console() << m[breaks[c]];
	}
	console() << "}" << std::endl;
}

void TextTestApp::setup()
{
	printFontNames();

	//printLineBreaks( "ツ😊Now is the time for all good men to come to the aid of their country" );
	//printLineBreaks( "Now is the time\nfor all good men\rto come to the aid of their country" );
	//printLineBreaks( "Hi World" );
	//printLineBreaks( "Hi World\n" );
	//printLineBreaks( "Hi World\n\r" );

#if 0 //defined( CINDER_MAC )
	mEmojiFace = text::loadFace( "/System/Library/Fonts/Apple Color Emoji.ttc" );
	mFace = text::loadFace( "/System/Library/Fonts/Noteworthy.ttc" );
#else
	//mEmojiFace = text::loadFace( "C:\\Windows\\Fonts\\Cambria.ttc", 0 );
	loadFonts( text::systemDefaultFace() );
	//loadFonts( "C:\\Windows\\Fonts\\Simsun.ttc" );
	////mFace = text::loadFace( "C:\\Users\\deploy\\Downloads\\test-fonts\\2017\\Domaine Display Condensed\\DomaineDispCond-BoldItalic.otf" );
	//mFace = text::loadFace( "C:\\Users\\deploy\\Downloads\\test-fonts\\2017\\SignPainter\\SignPainter-HouseCasual.otf" );
#endif

	//mATex = gl::Texture::create( mFont17->getGlyphBitmap( mFont17->getCharIndex( 'A' ) ) );
//	writeImage( getHomeDirectory() / "out.png", mEmojiFont->getGlyphBitmap( mEmojiFont->getCharIndex( U"😀"[0] ) ) );
//	mATex = gl::Texture::create( mEmojiFont->getGlyphBitmap( mEmojiFont->getCharIndex( U"😀"[0] ) ) );
	
	//mGlyphShape = mFont17->getGlyphShape( mFont17->getCharIndex( 'A' ) );
	
	//console() << "Width: " << mFont17->calcStringWidth( "Hello World" ) << std::endl;
	//vector<uint32_t> indices;
	//vector<float> positions;
	//mFont17->shapeString( "Héllo World", 0, &indices, &positions );
	//
	//for( size_t i = 0; i < indices.size(); ++i ) {
	//	Shape2d temp = mFont17->getGlyphShape( indices[i] );
	//	temp.translate( vec2( positions[i], 0 ) );
	//	mGlyphShape.append( temp );
	//}

	//auto str = text::AttrString() << mFontSmall << "He" << text::Leading::extra( 1.2f ) << text::RunBreak() << mFontMedium << "ll" << text::RunBreak() << "o";
	//auto f = text::Frame( str, 200, 200 );
	//app::console() << str.debugString() << std::endl;
}

void drawFrameLines( const text::GlyphLayout &layout, Surface8u *surface )
{
	bool drawLineMetrics = true;

	for( auto &line : layout.getLines() ) {
		if( drawLineMetrics ) {
			ip::fill( surface, Color8u( 255, 255, 0 ), Area( 0, line.getDrawOffset().y - line.getAscender(), surface->getWidth(), line.getDrawOffset().y - line.getAscender() + 1 ) );
			ip::fill( surface, Color8u( 0, 0, 255 ), Area( 0, line.getDrawOffset().y + line.getDescender(), surface->getWidth(), line.getDrawOffset().y + line.getDescender() + 1 ) );
			ip::fill( surface, Color8u( 0, 255, 0 ), Area( 0, line.getDrawOffset().y, surface->getWidth(), line.getDrawOffset().y + 1 ) );
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

void TextTestApp::renderTexture( int width, int height )
{
	if( ! mFace )
		return;

#if 0
	auto str = text::AttrString() << mFontLarge << "Hello Kind" << mFontSmall << "World";
#elif 0
	auto str = text::AttrString() << mFontSmall << "Hewwo Â, Ê, Î, Ô, Û, Ä, Ë, Ï, Ö, Ü, À, Æ, æ, Ç, É, È, Œ, œ, Ù" << mFontSmall << text::Tracking::pixels( 40 ) << text::Tracking() << " big " << text::Tracking::pixels( 20 ) << " test";
	console() << str.debugString() << std::endl;
#elif 0
	auto str = text::AttrString() << text::Leading::size( 160 ) << text::Tracking::em( -25 ) << mFontLarge << "Creating a Potential\n";
#elif 0
	auto str = text::AttrString() << text::Leading::size( 65 ) << text::Tracking::em( -2 ) << mFontSmall <<
	"Imagine being a researcher who is staring down not only a highly infectious virus, but one that's caused the first global pandemic in more than 100 years.\nMeet Johnson & Johnson's Hanneke Schuitemaker, Ph.D.";
#elif 0
	auto str = text::AttrString() << mFontSmall << u8"Left (then 2 blanks)\n" << text::Alignment::RIGHT << "\nright (then one blank)\n" << text::Alignment::CENTER << "and center.";
#elif 0
	auto face = text::loadSystemFace( "Arial Bold Italic" );
	auto font = text::loadFont( face, 24 );
	auto str = text::AttrString() << text::ShapingOptions().ignoreMissingGlyphs() << mFontSmall << u8"Missing ignored: {\u65E5}" << text::ShapingOptions().ignoreMissingGlyphs( false ) << mFontLarge << u8"Missing: {\u65E5}"; // 日本語
#elif 0 // missing font
	auto str = text::AttrString() << text::font( "Not here", 36 ) << "Hello Kind" << mFontSmall << "World";
#elif 1 // ligatures
	auto str = text::AttrString() << text::font( { {"Calibri", 36}, {"Times New Roman", 36} } ) << "Office furniture " << text::font( "Calibri Bold", 36 ) << "finally offered";
#elif 0 // dynamic ligatures
	auto str = text::AttrString() << text::font( { {"Calibri", 36}, {"Lucida Grande", 36} } ) << text::ShapingOptions().ligatures( true ) << "+Ligatures: Office furniture " << "finally offered " << "\n";
							str << text::ShapingOptions().ligatures( false ) << "-Ligatures: Office furniture " << "finally offered";
#elif 0 // repro trailing space bug
	auto str = text::AttrString() << text::font( "Calibri", 36 ) << "ABC " << text::font( "Calibri", 46 ) << "DEF";
	width = 15;
#elif 0
	auto str = text::AttrString() << mFontSmall << U"\u65E5\u672C\u8A9E"; // 日本語
#elif 0
	auto str = text::AttrString() << mFontSmall << text::Alignment::RIGHT << "Suuuper" << ColorAf::gray(0.5) << "\nTRAC";
#elif 0
	auto str = text::AttrString() << mFontLarge << "L"
		<< text::Alignment::CENTER << "NO\n";
#else
	auto str = text::AttrString() << mFontSmall << "4:15 PM “I believe what really happens in " << text::Alignment::DEFAULT << mFontLarge << "his" << text::RunBreak() << "tory" << text::Alignment::DEFAULT << mFontSmall << " is this: the old man is always wrong; and the young people are always wrong about what is wrong with him.\n"
	<< text::Alignment::RIGHT << ColorA8u( 255, 0, 0, 180 ) << "Supercalifragilisticexpialidocious" << Color8u::white()
	<< text::Tracking::em( -150 ) << text::Leading::mult( 0.9f ) << "\nNEGATIVE TRACKING, x0.9 LEADING" << text::Tracking() << text::Leading()
	<< text::Alignment::CENTER << "NORMAL TRACKING\n\n" << text::Tracking()
	<< "   The practical form it takes is this: that, while the old man may "
	<< "stand by some stupid custom, the young man always attacks it with some theory that turns out to be equally stupid.” – Illustrated London News, June 3, 1922 alphabet";
	//auto str = text::AttrString() << mFontSmall << text::Alignment::RIGHT << "Supercalifragilisticexpialidocious";
#endif

	auto frame = text::Frame( str, width, height, text::TypesetOptions().ignoreLineMetrics( false ).defaultAlignment( text::Alignment::LEFT ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );
	if( mAnimatedTesting )
		animate( frame );
	else {
		mSurface = Surface8u( width, height, true );
		mSurface.setPremultiplied( true );
		ip::fill( &mSurface, ColorA8u( 0, 0, 0, 0 ) );
		if( mBackgroundSurface )
			mSurface.copyFrom( *mBackgroundSurface, mBackgroundSurface->getBounds() );
		//text::renderToSurface( frame, &mSurface );
		mSurface = text::renderSurface( frame, vec2(0), ColorA8u::black(), mPreciseRendering );
		if( mDrawLines )
			drawFrameLines( frame.getGlyphLayout(), &mSurface );

		mTex = gl::Texture::create( mSurface );
	}
}

void TextTestApp::updateExtremeTest()
{
	static Rand rootRand = Rand{ (uint32_t)clock() };
	auto seed = rootRand.nextUint();
	Rand sRand = Rand{ seed };
	static std::vector<text::Font*> sFonts = { text::font( "Times New Roman", 14 ), text::font( "Arial", 18 ), text::font( "Arial Bold", 10 ), text::font( "Arial Bold Italic", 18 ) };
	static const int kFontChangePct = 10;
	static const int kNewlinePct = 3;
	static const int kTrackingPct = 3;
	static const int kRunBreakPct = 2;
	int32_t width = sRand.nextInt( 1, 1000 );
	int32_t height = sRand.nextInt( 1, 1000 );

	text::AttrString str;
	str <<  sFonts[sRand.nextInt( 0, sFonts.size() )];
	int32_t numWords = sRand.nextInt( 0, 400 );
	for( size_t word = 0; word < numWords; ++word ) {
		if( sRand.nextInt( 1, 100 ) <= kFontChangePct )
			str << sFonts[sRand.nextInt( 0, sFonts.size() )];
		int32_t wordLength = sRand.nextInt( 0, 20 );
		string wordStr = "";
		for( size_t c = 0; c < wordLength; ++c )
			wordStr += ( 'a' + sRand.nextInt( 0, 26 ) );

		if( sRand.nextInt( 1, 100 ) <= kTrackingPct ) {
			if( sRand.nextBool() )
				str << text::Tracking::em( sRand.nextFloat( 0, 2000 ) );
			else
				str << text::Tracking();
		}

		if( sRand.nextInt( 1, 100 ) <= kRunBreakPct ) {
			str << text::RunBreak();
		}

		str << wordStr;
		size_t numSpaces = sRand.nextInt( 0, 3 );
		for( size_t exSp = 0; exSp < numSpaces; ++exSp )
			str << " ";
		if( sRand.nextInt( 1, 100 ) <= kNewlinePct )
			str << "\n";
	}

	auto alignment = text::Alignment( (int)text::Alignment::LEFT + sRand.nextInt( 0, 3 ) );
	bool ligate = sRand.nextBool();
	bool ignoreMissing = sRand.nextBool();
	auto frame = text::Frame( str, width, height, text::TypesetOptions().defaultAlignment( alignment ).defaultShapingOptions( text::ShapingOptions().ligatures( ligate ).ignoreMissingGlyphs( ignoreMissing ) ) );
	if( mDrawLines ) {
		ip::fill( &mSurface, ColorA8u( 0, 0, 0, 255 ) );
		text::render( frame, &mSurface );
		drawFrameLines( frame.getGlyphLayout(), &mSurface );
		mTex = gl::Texture::create( mSurface );
	}
}

void TextTestApp::animate( const text::Frame &frame )
{
	mAnimatedTesting = true;
	mAnimatedTestFrame = frame;
	mAnimatedTestFrame;
	mAnimatedTestStartTime = getElapsedSeconds();
}

void TextTestApp::updateAnimatedTest()
{
	const double lineAnimationDuration = 1.0; // fade + char cascade
	const double perLineOffsetTime = 0.3; // line animation start = perLineOffsetTime * lineNum
	double t = getElapsedSeconds() - mAnimatedTestStartTime;
	const float glyphCascadeHeight = 12;
	text::GlyphLayout layout = mAnimatedTestFrame.getGlyphLayout();
	layout.breakGlyphsIntoRuns();
	if( t >= (perLineOffsetTime + lineAnimationDuration) * layout.getLines().size() )
		return;

	for( size_t lineIdx = 0; lineIdx < layout.getLines().size(); ++lineIdx ) {
		double lineStartTime = lineIdx * perLineOffsetTime;
		auto &line = layout.getLines()[lineIdx];
		if( t < lineStartTime )
			line.setOpacity( 0 );
		else {
			double lineRelativeT = (t - lineStartTime) / lineAnimationDuration; // 0-1
			for( size_t runIdx = 0; runIdx < line.getNumRuns(); ++runIdx ) {
				double glyphRelativeAnimDuration = 0.2; // in normalized 0-1 range, duration of a glyph fade+translate
				double glyphRelative = runIdx / (float)line.getNumGlyphs();
				auto &run = line.getRuns()[runIdx];
				float glyphRelativeT = ci::clamp<double>( (lineRelativeT - glyphRelative ) / glyphRelativeAnimDuration, 0.0, 1.0 );
				run.setDrawOffset( run.getDrawOffset() + vec2( 0, glyphCascadeHeight * ci::easeInCubic(1.0 - glyphRelativeT) ) );
				run.setOpacity( glyphRelativeT );
			}
		}
	}

	mSurface = Surface8u( getWindowWidth(), getWindowHeight(), true );
	mSurface.setPremultiplied( true );
	ip::fill( &mSurface, ColorA8u( 0, 0, 0, 0 ) );
	if( mBackgroundSurface )
		mSurface.copyFrom( *mBackgroundSurface, mBackgroundSurface->getBounds() );
	//text::renderToSurface( frame, &mSurface );
	mSurface = text::renderSurface( layout, vec2(0), ColorA8u::black(), mPreciseRendering );

	if( mDrawLines ) {
		for( auto& line : layout.getLines() ) {
			for( auto &run : line.getRuns() ) {
				for( size_t g = 0; g < run.getNumGlyphs(); ++g )
					ip::fill( &mSurface, Color8u( 200, 64, 0 ), Area( run.getGlyphBounds( g ) + run.getDrawOffset() + line.getDrawOffset() ) );
			}
		}
	}

	mTex = gl::Texture::create( mSurface );
}

void TextTestApp::resize()
{
	renderTexture( getWindowWidth(), getWindowHeight() );
}

void TextTestApp::loadFonts( text::Face *face )
{
	mFace = face;

	mFontSmall = text::loadFont( mFace, 44 );
	mFontMedium = text::loadFont( mFace, 77 );
	mFontLarge = text::loadFont( mFace, 99 );

	printFaceInfo( mFace );
	printFontInfo( mFontLarge );
}

void TextTestApp::fileDrop( FileDropEvent event )
{
	string ext = event.getFile( 0 ).extension().string();
	if( ext == ".png" || ext == ".jpg" )
		mBackgroundSurface = make_unique<Surface8u>( loadImage( event.getFile( 0 ) ) );
	else if( ext == ".ttf" || ext == ".otf" )
		loadFonts( text::loadFace( event.getFile( 0 ) ) );

	resize();
}

void TextTestApp::keyDown( KeyEvent event )
{
	if( event.getChar() == 's' ) {
		writeImage( getHomeDirectory() / "textTestOut.png", mSurface );
	}
	else if( event.getChar() == 'l' ) {
		mDrawLines = ! mDrawLines;
		renderTexture( getWindowWidth(), getWindowHeight() );
	}
	else if( event.getChar() == 'x' ) {
		mExtremeTesting = ! mExtremeTesting;
	}
	else if( event.getChar() == 'p' ) {
		mPreciseRendering = ! mPreciseRendering;
		CI_LOG_I( "Precise rendering: " << string(( mPreciseRendering ? "true" : "false" )) );
	}
	else if( event.getChar() == 'a' ) {
		mAnimatedTesting = ! mAnimatedTesting;
		renderTexture( getWindowWidth(), getWindowHeight() );
	}
}

void TextTestApp::draw()
{
	gl::setMatricesWindow( getWindowSize() );
	gl::clear( Color( 0.5f, 0.5f, 0.5f ) );
	//gl::enableAlphaBlending();
	gl::enableAlphaBlendingPremult();
	
	gl::color( Color::white() );
	if( mExtremeTesting )
		updateExtremeTest();
	else if( mAnimatedTesting )
		updateAnimatedTest();
	gl::draw( mTex );
	gl::color( Color8u( 255, 128, 64 ) );
}

CINDER_APP( TextTestApp, RendererGl )
