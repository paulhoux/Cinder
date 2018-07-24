#include "cinder/Rand.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class CameraProjectionTestApp : public App {
  public:
	void setup() override;
	void update() override;
	void draw() override;
};

void CameraProjectionTestApp::setup()
{
	// Due to the poor precision of matrix calculations, use a relatively big epsilon.
	constexpr float kEpsilon = 1.0e-6f;

	// Run tests.
	auto cam = CameraPersp();

	assert( !cam.isDepthReversedEnabled() );
	assert( !cam.isClipZeroToOne() );
	assert( !cam.isInfiniteFarClip() );

	vec4 clip;
	vec4 view;

	for( int i = 0; i < 100; ++i ) {
		// Apply random lens shift.
		cam.setLensShift( Rand::randFloat( -1.0f, 1.0f ), Rand::randFloat( -1.0f, 1.0f ) );

		// Create a random point in view space.
		const vec4 pt = vec4( Rand::randInt( -10, 11 ), Rand::randInt( -10, 11 ), Rand::randInt( -10, -1 ), 1 );

		cam.enableDepthReversed( false );
		cam.setClipZeroToOne( false );
		cam.setInfiniteFarClip( false );

		// Test projection and reprojection, making sure the inverse projection matrix matches the projection matrix.
		clip = cam.getProjectionMatrix() * pt;
		view = cam.getInverseProjectionMatrix() * clip;
		assert( fabs( view.x - pt.x ) < kEpsilon );
		assert( fabs( view.y - pt.y ) < kEpsilon );
		assert( fabs( view.z - pt.z ) < kEpsilon );

		// Compare the actual near and far clip distance with the desired one.
		vec4 comp = pt;
		comp.z = -cam.getNearClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - -1.0f ) < kEpsilon );
		comp.z = -cam.getFarClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - +1.0f ) < kEpsilon );

		// Repeat for all variants.
		cam.setClipZeroToOne( true );
		cam.setInfiniteFarClip( false );
		clip = cam.getProjectionMatrix() * pt;
		view = cam.getInverseProjectionMatrix() * clip;
		assert( fabs( view.x - pt.x ) < kEpsilon );
		assert( fabs( view.y - pt.y ) < kEpsilon );
		assert( fabs( view.z - pt.z ) < kEpsilon );

		comp.z = -cam.getNearClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - 0.0f ) < kEpsilon );
		comp.z = -cam.getFarClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - 1.0f ) < kEpsilon );

		cam.setClipZeroToOne( false );
		cam.setInfiniteFarClip( true );
		clip = cam.getProjectionMatrix() * pt;
		view = cam.getInverseProjectionMatrix() * clip;
		assert( fabs( view.x - pt.x ) < kEpsilon );
		assert( fabs( view.y - pt.y ) < kEpsilon );
		assert( fabs( view.z - pt.z ) < kEpsilon );

		comp.z = -cam.getNearClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - -1.0f ) < kEpsilon );

		cam.setClipZeroToOne( true );
		cam.setInfiniteFarClip( true );
		clip = cam.getProjectionMatrix() * pt;
		view = cam.getInverseProjectionMatrix() * clip;
		assert( fabs( view.x - pt.x ) < kEpsilon );
		assert( fabs( view.y - pt.y ) < kEpsilon );
		assert( fabs( view.z - pt.z ) < kEpsilon );

		comp.z = -cam.getNearClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - 0.0f ) < kEpsilon );

		cam.enableDepthReversed( true );

		cam.setClipZeroToOne( false );
		cam.setInfiniteFarClip( false );
		clip = cam.getProjectionMatrix() * pt;
		view = cam.getInverseProjectionMatrix() * clip;
		assert( fabs( view.x - pt.x ) < kEpsilon );
		assert( fabs( view.y - pt.y ) < kEpsilon );
		assert( fabs( view.z - pt.z ) < kEpsilon );

		comp.z = -cam.getNearClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - +1.0f ) < kEpsilon );
		comp.z = -cam.getFarClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - -1.0f ) < kEpsilon );

		cam.setClipZeroToOne( true );
		cam.setInfiniteFarClip( false );
		clip = cam.getProjectionMatrix() * pt;
		view = cam.getInverseProjectionMatrix() * clip;
		assert( fabs( view.x - pt.x ) < kEpsilon );
		assert( fabs( view.y - pt.y ) < kEpsilon );
		assert( fabs( view.z - pt.z ) < kEpsilon );

		comp.z = -cam.getNearClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - 1.0f ) < kEpsilon );
		comp.z = -cam.getFarClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - 0.0f ) < kEpsilon );

		cam.setClipZeroToOne( false );
		cam.setInfiniteFarClip( true );
		clip = cam.getProjectionMatrix() * pt;
		view = cam.getInverseProjectionMatrix() * clip;
		assert( fabs( view.x - pt.x ) < kEpsilon );
		assert( fabs( view.y - pt.y ) < kEpsilon );
		assert( fabs( view.z - pt.z ) < kEpsilon );

		comp.z = -cam.getNearClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - +1.0f ) < kEpsilon );

		cam.setClipZeroToOne( true );
		cam.setInfiniteFarClip( true );
		clip = cam.getProjectionMatrix() * pt;
		view = cam.getInverseProjectionMatrix() * clip;
		assert( fabs( view.x - pt.x ) < kEpsilon );
		assert( fabs( view.y - pt.y ) < kEpsilon );
		assert( fabs( view.z - pt.z ) < kEpsilon );

		comp.z = -cam.getNearClip();
		clip = cam.getProjectionMatrix() * comp;
		assert( fabs( clip.z / clip.w - 1.0f ) < kEpsilon );
	}
}

void CameraProjectionTestApp::update() {}

void CameraProjectionTestApp::draw()
{
	gl::clear();
}

CINDER_APP( CameraProjectionTestApp, RendererGl )
