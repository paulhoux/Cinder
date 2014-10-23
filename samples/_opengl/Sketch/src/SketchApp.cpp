#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Sketch.h"
#include "cinder/Camera.h"
#include "cinder/MayaCamUI.h"

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
	gl::Sketch   mSketch;

	CameraPersp  mCamera;
	CameraPersp  mCameraGhost;
	MayaCamUI    mMayaCam;
};

void SketchApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 960, 720 );
	settings->setResizable( false );
}

void SketchApp::setup()
{
	mCamera.setPerspective( 50, 1, 1, 1000 );
	mCamera.setEyePoint( vec3( 10, 10, 10 ) );
	mCamera.setCenterOfInterestPoint( vec3( 0 ) );

	mCameraGhost = mCamera;
	mCameraGhost.setNearClip( 1 );
	mCameraGhost.setFarClip( 10 );
}

void SketchApp::update()
{
	float time = (float) getElapsedSeconds();

	// Save the current matrices, so we can restore them.
	gl::ScopedMatrices matrices;

	// Clear the sketch buffer.
	mSketch.clear();

	// When sketching, the current coordinate space is respected,
	// so let's draw in 3D using our camera.
	gl::setMatrices( mCamera );

	// Set the draw color and draw a grid.
	gl::color( 0.25f, 0.25f, 0.25f );
	mSketch.grid( 10, 1 );

	// Drawing a sphere is just as easy.
	gl::color( 1.00f, 0.75f, 0.50f );
	mSketch.sphere( vec3( 0 ), 2 );

	// A cube is no problem, either.
	gl::color( 0.50f, 1.00f, 0.75f );
	mSketch.cube( vec3( 5, 0, 0 ), vec3( 2 ) );

	// What about a nice cone?
	gl::color( 0.75f, 0.50f, 1.00f );
	mSketch.cone( vec3( -5, 3, 0 ), vec3( -5, 0, 0 ), 0.5f );

	// We can also animate stuff. Let's draw a rotating capsule.
	{
		gl::ScopedModelMatrix model;

		gl::translate( 0, 0, 6 );
		gl::rotate( 2 * time, 0, 1, 0 );

		gl::color( 1.00f, 0.50f, 0.75f );
		mSketch.capsule( vec3( -2, 0, 0 ), vec3( 2, 0, 0 ), 1 );
	}

	// And a jumping cylinder.
	{
		gl::ScopedModelMatrix model;

		gl::translate( 0, 0, -6 );
		gl::rotate( time, 0, 1, 0 );

		vec3 base = vec3( 0, math<float>::max( 0, -3 * math<float>::cos( 3 * time + 1.5f ) ), 0 );
		vec3 top = vec3( 0, 5 + 2 * math<float>::sin( 3 * time ), 0 );
		float radius = math<float>::sqrt( 5 / glm::distance( base, top ) );

		gl::color( 0.75, 1.00, 0.50f );
		mSketch.cylinder( base, top, radius );
	}

	// It's easy to draw a camera frustum, too.
	gl::color( 0.5f, 0.5f, 0.5f );
	mSketch.frustum( mCameraGhost );

	// We can also draw in 2D. Simply change to window coordinates:
	gl::setMatricesWindow( getWindowSize() );

	gl::color( 0.50f, 0.75f, 1.00f );
	mSketch.circle( getWindowSize() / 2, 100 );

	// And with a bit of work, we can draw polygons.
	{
		Font font = Font( "Georgia", 144 );
		Shape2d shape = font.getGlyphShape( font.getGlyphChar( '*' ) );

		gl::ScopedModelMatrix model;

		gl::translate( 40, 100, 0 );
		gl::color( 1, 1, 0 );

		auto contours = shape.getContours();
		for( auto contour : contours ) {
			mSketch.lineStrip( contour.getPoints() );
		}
	}
}

void SketchApp::draw()
{
	gl::clear();
	gl::enableDepthRead();
	gl::enableDepthWrite();

	// Now draw everything using a single draw call.
	mSketch.draw();
}

void SketchApp::mouseDown( MouseEvent event )
{
	mCameraGhost = mCamera;
	mCameraGhost.setNearClip( 5 );
	mCameraGhost.setFarClip( 20 );

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
	mCameraGhost.setAspectRatio( getWindowAspectRatio() );
}

CINDER_APP_NATIVE( SketchApp, RendererGl )
