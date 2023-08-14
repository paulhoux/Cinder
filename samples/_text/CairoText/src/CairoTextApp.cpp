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

class CairoTextApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;
	void renderTextOverlay( cairo::Context& ctx ) const;
	void renderTextOnPath( cairo::Context& ctx ) const;
	void renderScene( cairo::Context& ctx );
	void draw() override;
};

void CairoTextApp::setup()
{
	//gFace = text::loadFace( "C:\\Windows\\Fonts\\Candarali.ttf" );
	gFace = text::loadFace( "C:\\Windows\\Fonts\\Gabriola.ttf" );
}

void CairoTextApp::mouseDown( MouseEvent event )
{	
}

void CairoTextApp::keyDown( KeyEvent event )
{
	if( event.getChar() == 's' ) {
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

void CairoTextApp::renderTextOverlay( cairo::Context& ctx ) const
{
	text::Frame frame( text::AttrString() << text::font( gFace, 384.0f ) << "Cinder" );
	float xOffset = (getWindowWidth() - frame.getMeasuredWidth()) / 2;
	ctx.glyphPath( frame, vec2{ xOffset, getWindowCenter().y / 2} );

	// Fill the text path with the gradient
	cairo::GradientLinear gradient{ {xOffset, 0}, { xOffset + frame.getMeasuredWidth(), 0} };
	gradient.addColorStop( 0.0, ColorA( 1.0f, 0.5f, 0.25f, 0.2f ) );
	gradient.addColorStop( 1.0, ColorA( 1.0f, 0.9f, 0.1f, 0.4f ) );
	ctx.setSource( gradient );
	ctx.fillPreserve();
	ctx.setSource( ColorA( 0.8f, 0.1f, 0.1f, 0.8f ) );
	ctx.stroke();
}

void CairoTextApp::renderTextOnPath( cairo::Context& ctx ) const
{
	int32_t canvasWidth = getWindowWidth();
	int32_t canvasHeight = getWindowHeight();
	vec2 drawOffset{ 50, canvasHeight / 2.0f };

	ci::Path2d path;
	path.moveTo( vec2( 0, 0 ) );
	//path.lineTo( canvasWidth, canvasHeight );
	path.curveTo( vec2( 200, -200 ), vec2( canvasWidth / 2, 200 ), vec2( canvasWidth, 20 ) );
	path.curveTo( vec2( canvasWidth - 50, -200 ), vec2( 300, 200 ), vec2( canvasWidth - 50, 120 ) );
	text::AttrString str;
	str << text::font( gFace, 48.0f ) << Color( 1.0f, 0.25f, 1.0f ) << "Jos\xc3\xa9 and Zo\xc3\xab enjoyed caf\xc3\xa9 cr\xc3\xa8me and"
		<< text::font( gFace, 64.0f ) << Color( 0.5f, 0.25f, 1.0f ) << " affogatos in Malm\xc3\xb6.";

	// draw the path itself
	ctx.setSource( Color( 0, 0, 1.0f ) );
	ctx.save();
	ctx.translate( drawOffset );
	ctx.appendPath( path );
	ctx.stroke();
	ctx.restore();

	ctx.showText( text::TextOnPath( str, path, text::TypesetOptions(), getElapsedSeconds() * 0, -getElapsedSeconds() * 0 ), drawOffset );
	//ctx.showText( text::Frame( str ), mLoc );	
}

void CairoTextApp::renderScene( cairo::Context &ctx )
{
	// clear the context with our radial gradient
	cairo::GradientRadial radialGrad( getWindowCenter(), 0, getWindowCenter(), getWindowWidth() );
	radialGrad.addColorStop( 0, Color( 1, 1, 1 ) );
	radialGrad.addColorStop( 1, Color( 0.6, 0.6, 0.6 ) );	
	ctx.setSource( radialGrad );	
	ctx.paint();

	renderTextOnPath( ctx );
	renderTextOverlay( ctx );
}

void CairoTextApp::draw()
{
	// render the scene straight to the window
	cairo::Context ctx( cairo::createWindowSurface() );	
	renderScene( ctx );
}

CINDER_APP( CairoTextApp, Renderer2d )
