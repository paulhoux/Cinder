#include "cinder/Camera.h"
#include "cinder/ImageIo.h"
#include "cinder/MayaCamUI.h"
#include "cinder/TriMesh.h"
#include "cinder/app/AppBasic.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Vbo.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class SubdivideAndSpherizeTestApp : public AppBasic {
public:
	void setup();
	void update();
	void draw();

	void mouseDown( MouseEvent event );
	void mouseDrag( MouseEvent event );

	void keyDown( KeyEvent event );

	void resize( ResizeEvent event );
protected:
	void render();
	void createIcosahedron();
private:
	TriMesh			mTriMesh;
	gl::VboMesh		mVboMesh;

	MayaCamUI		mMayaCam;
};

void SubdivideAndSpherizeTestApp::setup()
{
	CameraPersp cam;
	cam.setEyePoint( Vec3f(0.0, 0.0f, 10.0f) );
	cam.setCenterOfInterestPoint( Vec3f::zero() );
	cam.setFov( 20.0f );
	mMayaCam.setCurrentCam( cam );

	createIcosahedron();
}

void SubdivideAndSpherizeTestApp::update()
{
}

void SubdivideAndSpherizeTestApp::draw()
{
	static const Font font("Tahoma", 16);

	gl::clear(); 
	gl::enableDepthRead();
	gl::enableDepthWrite();

	gl::pushMatrices();
	gl::setMatrices( mMayaCam.getCamera() );
	render();
	gl::popMatrices();

	gl::disableDepthWrite();
	gl::disableDepthRead();

	gl::enableAlphaBlending();
	gl::drawString("Press SPACE to reset icosahedron\nPress PLUS to subdivide once\nPress ENTER to spherize", Vec2f(10, 10), Color::white(), font);
	gl::disableAlphaBlending();
}

void SubdivideAndSpherizeTestApp::mouseDown( MouseEvent event )
{
	mMayaCam.mouseDown( event.getPos() );
}

void SubdivideAndSpherizeTestApp::mouseDrag( MouseEvent event )
{
	mMayaCam.mouseDrag( event.getPos(), event.isLeftDown(), false, event.isRightDown() );
}

void SubdivideAndSpherizeTestApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	case KeyEvent::KEY_f:
		setFullScreen( !isFullScreen() );
		break;
	case KeyEvent::KEY_PLUS:
	case KeyEvent::KEY_KP_PLUS:
		mTriMesh.subdivide();
		// invalidate vbo
		mVboMesh = gl::VboMesh();
		break;
	case KeyEvent::KEY_RETURN:
	case KeyEvent::KEY_KP_ENTER:
		mTriMesh.spherize();
		// invalidate vbo
		mVboMesh = gl::VboMesh();
		break;
	case KeyEvent::KEY_SPACE:
		createIcosahedron();
		break;
	}
}

void SubdivideAndSpherizeTestApp::resize( ResizeEvent event )
{
	CameraPersp cam = mMayaCam.getCamera();
	cam.setAspectRatio( event.getAspectRatio() );
	mMayaCam.setCurrentCam( cam );
}

void SubdivideAndSpherizeTestApp::render()
{
	if(!mVboMesh) mVboMesh = gl::VboMesh( mTriMesh );

	gl::pushModelView();
	gl::rotate( (float) getElapsedSeconds() * 10.0f * Vec3f::yAxis() );

	gl::enableWireframe();

	gl::enable( GL_CULL_FACE );

	glCullFace( GL_BACK );
	gl::color( Color::white() );
	gl::draw( mVboMesh );

	gl::disable( GL_CULL_FACE );

	gl::disableWireframe();

	/*//
	gl::color( Color(0.1f, 0.1f, 0.1f) );
	for(int i=-10;i<=10;++i) {
		gl::drawLine( Vec3f((float)i, 0, -10), Vec3f((float)i, 0, 10) ); 
		gl::drawLine( Vec3f(-10, 0, (float)i), Vec3f(10, 0, (float)i) );
	}
	//*/

	gl::popModelView();
}

void SubdivideAndSpherizeTestApp::createIcosahedron()
{
	const float t = 0.5f + 0.5f * math<float>::sqrt(5.0f);
	const float one = 1.0f / math<float>::sqrt(1.0f+t*t);
	const float tau = t * one;
	const float pi = (float) M_PI;

	vector<Vec3f>		positions;
	vector<Vec3f>		normals;
	vector<Vec2f>		texcoords;
	vector<uint32_t>	indices;

	normals.push_back( Vec3f(one, 0, tau) );	
	normals.push_back( Vec3f(one, 0, -tau) );	
	normals.push_back( Vec3f(-one, 0, -tau) );		
	normals.push_back( Vec3f(-one, 0, tau) );		

	normals.push_back( Vec3f(tau, one, 0) );
	normals.push_back( Vec3f(-tau, one, 0) );	
	normals.push_back( Vec3f(-tau, -one, 0) );
	normals.push_back( Vec3f(tau, -one, 0) );

	normals.push_back( Vec3f(0, tau, one) );
	normals.push_back( Vec3f(0, -tau, one) );	
	normals.push_back( Vec3f(0, -tau, -one) );		
	normals.push_back( Vec3f(0, tau, -one) );		

	for(size_t i=0;i<normals.size();++i) 
		positions.push_back( normals[i] );

	indices.push_back(0); indices.push_back(8); indices.push_back(3);
	indices.push_back(0); indices.push_back(3); indices.push_back(9);
	indices.push_back(1); indices.push_back(2); indices.push_back(11);
	indices.push_back(1); indices.push_back(10); indices.push_back(2);
	
	indices.push_back(4); indices.push_back(0); indices.push_back(7);
	indices.push_back(4); indices.push_back(7); indices.push_back(1);
	indices.push_back(6); indices.push_back(3); indices.push_back(5);
	indices.push_back(6); indices.push_back(5); indices.push_back(2);
	
	indices.push_back(8); indices.push_back(4); indices.push_back(11);
	indices.push_back(8); indices.push_back(11); indices.push_back(5);
	indices.push_back(9); indices.push_back(10); indices.push_back(7);
	indices.push_back(9); indices.push_back(6); indices.push_back(10);
	
	indices.push_back(8); indices.push_back(0); indices.push_back(4);
	indices.push_back(11); indices.push_back(4); indices.push_back(1);
	indices.push_back(0); indices.push_back(9); indices.push_back(7);
	indices.push_back(1); indices.push_back(7); indices.push_back(10);
	
	indices.push_back(3); indices.push_back(8); indices.push_back(5);
	indices.push_back(2); indices.push_back(5); indices.push_back(11);
	indices.push_back(3); indices.push_back(6); indices.push_back(9);
	indices.push_back(2); indices.push_back(10); indices.push_back(6);

	// approximate texture coordinates by converting to spherical coordinates	
	vector<Vec3f>::const_iterator itr;
	for(itr=normals.begin();itr!=normals.end();++itr) {
		float u = 0.5f + 0.5f * math<float>::atan2( itr->x, itr->z ) / pi;
		float v = 0.5f - math<float>::asin( itr->y ) / pi;

		texcoords.push_back( Vec2f(u, v) );
	}

	// create mesh
	mTriMesh.clear();
	mTriMesh.appendVertices( &positions.front(), positions.size() );
	mTriMesh.appendNormals( &normals.front(), normals.size() );
	mTriMesh.appendIndices( &indices.front(), indices.size() );
	mTriMesh.appendTexCoords( &texcoords.front(), texcoords.size() );

	// invalidate vbo
	mVboMesh = gl::VboMesh();
}

CINDER_APP_BASIC( SubdivideAndSpherizeTestApp, RendererGl )
