#include <vector>
using std::vector;

#include "cinder/app/App.h"
#include "cinder/Path2d.h"
#include "cinder/cairo/Cairo.h"
#include "cinder/ip/Fill.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/text/Text.h"
#include "cinder/Path2d.h"

#include <cairo.h>

using namespace ci;
using namespace ci::app;

text::Face *gFace;

class TextFragment {
  public:
	TextFragment( vec2 loc, ColorA color )
		: mLoc( loc ), mColor( color )
	{}
	
	void draw( cairo::Context &ctx ) const
	{
		int32_t canvasWidth = getWindowWidth();
		int32_t canvasHeight = getWindowHeight();

		ci::Path2d path;
		path.moveTo( vec2( 0, 0 ) );
		path.lineTo( canvasWidth, 0 );
		//path.curveTo( vec2( 200, -200 ), vec2( 300, 200 ), vec2( canvasWidth, 20 ) );
		//path.curveTo( vec2( 200, -200 ), vec2( 300, 200 ), vec2( canvasWidth - 20, 120 ) );
		text::AttrString str;
		str << text::loadFont( gFace, 32.0f ) << Color( 1.0f, 0.25f, 1.0f ) << "Jos\xc3\xa9 and Zo\xc3\xab enjoyed caf\xc3\xa9 cr\xc3\xa8me and"
			<< text::loadFont( gFace, 48.0f ) << Color( 0.5f, 0.25f, 1.0f ) << " affogatos in Malm\xc3\xb6.";

		//ctx.showText( text::TextOnPath( str, path ), mLoc );
		ctx.showText( text::Frame( str ), mLoc );
		
		ctx.setSource( Color( 0, 0, 1.0f ) );
		ctx.save();
		ctx.translate( mLoc );
		ctx.appendPath( path );
		ctx.stroke();
		ctx.restore();
		//ctx.showText( text::Frame( str ), mLoc );
	};
	
  private:
	vec2		mLoc;
	ColorA		mColor;
};

class CairoTextApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;
	void renderScene( cairo::Context &ctx );
	void draw() override;
	
	vector<TextFragment>		mTextFragments;
};

void CairoTextApp::setup()
{
	//gFace = text::loadFace( "C:\\Windows\\Fonts\\Candarali.ttf" );
	gFace = text::loadFace( "C:\\Windows\\Fonts\\Gabriola.ttf" );
}

void CairoTextApp::mouseDown( MouseEvent event )
{	
	// create a new TextFragment
	mTextFragments.push_back( TextFragment( event.getPos(), ColorA( CM_HSV, randFloat(), 1, 1, 0.65f ) ) );
}

void CairoTextApp::keyDown( KeyEvent event )
{
	if( event.getCode() == KeyEvent::KEY_f ) {
		setFullScreen( ! isFullScreen() );
	}
	else if( event.getChar() == 'x' ) {
		mTextFragments.clear();
	}
	else if( event.getChar() == 's' ) {
		cairo::Context ctx( cairo::SurfaceSvg( getHomeDirectory() / "CairoBasicShot.svg", getWindowWidth(), getWindowHeight() ) );
		renderScene( ctx );
	}
	else if( event.getChar() == 'e' ) {
		cairo::Context ctx( cairo::SurfaceEps( getHomeDirectory() / "CairoBasicShot.eps", getWindowWidth(), getWindowHeight() ) );
		renderScene( ctx );
	}
	else if( event.getChar() == 'p' ) {
		cairo::Context ctx( cairo::SurfacePs( getHomeDirectory() / "CairoBasicShot.ps", getWindowWidth(), getWindowHeight() ) );
		renderScene( ctx );
	}	
	else if( event.getChar() == 'd' ) {
		cairo::Context ctx( cairo::SurfacePdf( getHomeDirectory() / "CairoBasicShot.pdf", getWindowWidth(), getWindowHeight() ) );
		renderScene( ctx );
	}	
}

void CairoTextApp::renderScene( cairo::Context &ctx )
{
	// clear the context with our radial gradient
	cairo::GradientRadial radialGrad( getWindowCenter(), 0, getWindowCenter(), getWindowWidth() );
	radialGrad.addColorStop( 0, Color( 1, 1, 1 ) );
	radialGrad.addColorStop( 1, Color( 0.6, 0.6, 0.6 ) );	
	ctx.setSource( radialGrad );	
	ctx.paint();
	
	ctx.setSource( Colorf( 1.0f, 0.5f, 0.25f ) );
	ctx.setFont( loadFont( gFace, 24 ) );
	ctx.showText( "12345" );

	for( vector<TextFragment>::const_iterator flIt = mTextFragments.begin(); flIt != mTextFragments.end(); ++flIt )
		flIt->draw( ctx );
}

void CairoTextApp::draw()
{
	// render the scene straight to the window
	cairo::Context ctx( cairo::createWindowSurface() );	
	renderScene( ctx );
}

CINDER_APP( CairoTextApp, Renderer2d )
