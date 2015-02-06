#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Sketch.h"
#include "cinder/Camera.h"
#include "cinder/MayaCamUI.h"

#include "cinder/MatrixAlgo.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class SketchApp : public AppNative {
public:
	void prepareSettings( Settings *settings ) override;

	void setup() override;
	void update() override;
	void draw() override;

	void mouseDown( MouseEvent event ) override;
	void mouseDrag( MouseEvent event ) override;

	void resize() override;
public:
	gl::SketchRef   mSketchStatic;
	gl::SketchRef   mSketchDynamic;

	CameraPersp     mCamera;
	MayaCamUI       mMayaCam;
};

void SketchApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 960, 720 );
	settings->setResizable( false );

	// test MatrixAlgo
	mat4 m;
	vec3 s, h, r, t;

	ci::extractSHRT( m, s, h, r, t );

	m = glm::translate( m, vec3( 10, 20, 30 ) );

	ci::extractSHRT( m, s, h, r, t );

	m = glm::rotate( m, glm::radians( 45.0f ), vec3( 0, 1, 0 ) );

	ci::extractSHRT( m, s, h, r, t );

	m = glm::scale( m, vec3( 15, 2, 1 ) );

	ci::extractSHRT( m, s, h, r, t );
}

void SketchApp::setup()
{
	mCamera.setPerspective( 50, 1, 1, 1000 );
	mCamera.setEyePoint( vec3( 10, 10, 10 ) );
	mCamera.setCenterOfInterestPoint( vec3( 0 ) );

	// Let's create a static Sketch buffer that we can use over and over again.
	// Make sure it records our primitives untransformed, so we can apply GPU transforms later.
	// Just remember that we can draw either in 3D or 2D, but not both, because they will all
	// share the same coordinate space.
	mSketchStatic = gl::Sketch::create( false );

	// Let's also create a dynamic Sketch buffer that we use to draw animated stuff with.
	// This time, we'd like to mix 3D with 2D, so we enable auto transformations and do all
	// transformations on the CPU. This is easier and more flexible, but less efficient.
	mSketchDynamic = gl::Sketch::create( true );

	// Clear the static Sketch buffer.
	mSketchStatic->clear();

	// Set the draw color and draw a grid.
	gl::color( 0.25f, 0.25f, 0.25f );
	mSketchStatic->grid( 20, 1 );

	// Drawing a sphere is just as easy.
	gl::color( 1.00f, 0.75f, 0.50f );
	mSketchStatic->sphere( vec3( 0 ), 2 );

	// A cube is no problem, either.
	gl::color( 0.50f, 1.00f, 0.75f );
	mSketchStatic->cube( vec3( 5, 0, 0 ), vec3( 2 ) );

	// What about a nice cone?
	gl::color( 0.75f, 0.50f, 1.00f );
	mSketchStatic->cone( vec3( -5, 3, 0 ), vec3( -5, 0, 0 ), 0.5f );

}

void SketchApp::update()
{
	float time = (float) getElapsedSeconds();

	// Save the current matrices, so we can restore them.
	gl::ScopedMatrices matrices;

	// Clear the dynamic Sketch buffer.
	mSketchDynamic->clear();

	// When sketching with auto transforms, the current coordinate space is respected,
	// so let's draw in 3D using our camera.
	gl::setMatrices( mCamera );

	// Let's draw a rotating capsule.
	{
		gl::ScopedModelMatrix model;

		gl::translate( 0, 0, 6 );
		gl::rotate( 2 * time, 0, 1, 0 );

		gl::color( 1.00f, 0.50f, 0.75f );
		mSketchDynamic->capsule( vec3( -2, 0, 0 ), vec3( 2, 0, 0 ), 1 );
	}

	// And a jumping cylinder.
	{
		gl::ScopedModelMatrix model;

		gl::translate( 0, 0, -6 );
		gl::rotate( time, 0, 1, 0 );

		vec3 base = vec3( 0, math<float>::max( 0, -3 * math<float>::cos( 3 * time + 1.5f ) ), 0 );
		vec3 top = vec3( 0, 5 + 2 * math<float>::sin( 3 * time ), 0 );
		float radius = math<float>::sqrt( 3 / glm::distance( base, top ) );

		gl::color( 0.75, 1.00, 0.50f );
		mSketchDynamic->cylinder( base, top, radius );
	}

	// We can also draw in 2D. Simply change to window coordinates.
	gl::setMatricesWindow( getWindowSize() );

	// Draw a circle.
	gl::color( 0.50f, 0.75f, 1.00f );
	mSketchDynamic->circle( getWindowSize() / 2, 100 );

	// And with a bit of work, we can draw polygons.
	{
		Font font = Font( "Georgia", 144 );
		Shape2d shape = font.getGlyphShape( font.getGlyphChar( '*' ) );

		gl::ScopedModelMatrix model;

		gl::translate( 40, 100, 0 );
		gl::color( 1, 1, 0 );

		auto contours = shape.getContours();
		for( auto contour : contours ) {
			mSketchDynamic->lineStrip( contour.getPoints() );
		}
	}
}

void SketchApp::draw()
{
	gl::clear();

	// Save the current matrices, so we can restore them.
	gl::ScopedMatrices matrices;

	// For our static Sketch buffer, we need to setup the correct matrices,
	// because it does not apply them automatically.
	gl::setMatrices( mCamera );

	// Now draw everything using a single draw call per Sketch buffer.
	mSketchStatic->draw();

	// The dynamic buffer will ignore the current matrices, using the 
	// matrices that were active at the time of recording instead.
	mSketchDynamic->draw();
}

void SketchApp::mouseDown( MouseEvent event )
{
	mMayaCam.setCurrentCam( mCamera );
	mMayaCam.mouseDown( event.getPos() );
}

void SketchApp::mouseDrag( MouseEvent event )
{
	mMayaCam.mouseDrag( event.getPos(), event.isLeftDown(), false, event.isRightDown() );
	mCamera = mMayaCam.getCamera();
}

void SketchApp::resize()
{
	mCamera.setAspectRatio( getWindowAspectRatio() );
}

CINDER_APP_NATIVE( SketchApp, RendererGl )
