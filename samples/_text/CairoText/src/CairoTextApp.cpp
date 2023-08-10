#include <vector>
using std::vector;

#include "cinder/app/App.h"
#include "cinder/Path2d.h"
#include "cinder/cairo/Cairo.h"
#include "cinder/ip/Fill.h"
#include "cinder/Rand.h"
#include "cinder/Utilities.h"
#include "cinder/text/Text.h"

#include <cairo.h>

using namespace ci;
using namespace ci::app;

text::Face *gFace;

class Flower {
  public:
	Flower( vec2 loc, float radius, float petalOutsideRadius, float petalInsideRadius, int numPetals, ColorA color )
		: mLoc( loc ), mRadius( radius ), mPetalOutsideRadius( petalOutsideRadius ), mPetalInsideRadius( petalInsideRadius ), mNumPetals( numPetals ), mColor( color )
	{}

	void makePath( cairo::Context &ctx ) const
	{
		for( int petal = 0; petal < mNumPetals; ++petal ) {
			ctx.newSubPath();
			float petalAngle = ( petal / (float)mNumPetals ) * 2 * M_PI;
			vec2 outsideCircleCenter = mLoc + vec2( 1, 0 ) * (float)cos( petalAngle ) * mRadius + vec2( 0, 1 ) * (float)sin( petalAngle ) * mRadius;
			vec2 insideCircleCenter = mLoc + vec2( 1, 0 ) * (float)cos( petalAngle ) * mPetalInsideRadius + vec2( 0, 1 ) * (float)sin( petalAngle ) * mPetalInsideRadius;
			ctx.arc( outsideCircleCenter, mPetalOutsideRadius, petalAngle + M_PI / 2 + M_PI, petalAngle + M_PI / 2 );
			ctx.arc( insideCircleCenter, mPetalInsideRadius, petalAngle + M_PI / 2, petalAngle + M_PI / 2 + M_PI );
			ctx.closePath();
		}		
	}
	
	void draw( cairo::Context &ctx ) const
	{
		// draw the solid petals
		ctx.setSource( mColor );
//		makePath( ctx );
//		ctx.fill();
		
		// draw the petal outlines
		ctx.setSource( mColor * 0.8f );
		//makePath( ctx );
		ctx.setFont( text::loadFont( gFace, 24 ) );
		ctx.moveTo( mLoc );
		ctx.showText( "12345" );

		text::AttrString str;
		str << Color( 1.0f, 0.25f, 1.0f ) << text::loadFont( gFace, 40.0f ) << "Born today in 1908, Mary G. Ross was the first known Native American female engineer, and the first female engineer in the history of Lockheed, remembered for her work on aerospace design and design concepts for interplanetary space travel";
		ctx.showText( text::Frame( str, 300, 500 ), mLoc );
		//ctx.stroke();

	};
	
  private:
	vec2		mLoc;
	float		mRadius, mPetalOutsideRadius, mPetalInsideRadius;
	int			mNumPetals;
	ColorA		mColor;
};

class CairoTextApp : public App {
  public:
	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;
	void renderScene( cairo::Context &ctx );
	void draw() override;
	
	vector<Flower>		mFlowers;
};

void CairoTextApp::setup()
{
	gFace = text::loadFace( "C:\\Windows\\Fonts\\Candarali.ttf" );
}

void CairoTextApp::mouseDown( MouseEvent event )
{	
	// create a new flower
	float radius = randFloat( 60, 90 );
	int numPetals = randInt( 6, 50 );
	float outerRadius = ( 2 * M_PI * radius ) / numPetals / 2 * randFloat( 0.9f, 1.0f );
	float innerRadius = outerRadius * randFloat( 0.2f, 0.4f );
	mFlowers.push_back( Flower( event.getPos(), radius, outerRadius, innerRadius, numPetals, ColorA( CM_HSV, randFloat(), 1, 1, 0.65f ) ) );
}

void CairoTextApp::keyDown( KeyEvent event )
{
	if( event.getCode() == KeyEvent::KEY_f ) {
		setFullScreen( ! isFullScreen() );
	}
	else if( event.getChar() == 'x' ) {
		mFlowers.clear();
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

	for( vector<Flower>::const_iterator flIt = mFlowers.begin(); flIt != mFlowers.end(); ++flIt )
		flIt->draw( ctx );
}

void CairoTextApp::draw()
{
	// render the scene straight to the window
	cairo::Context ctx( cairo::createWindowSurface() );	
	renderScene( ctx );
}

CINDER_APP( CairoTextApp, Renderer2d )
