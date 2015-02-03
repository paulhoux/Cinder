//#include "vld.h"

#include "cinder/app/AppNative.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/gl/VboMesh.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"

#include "cinder/evr/RendererGlImpl.h"
#include "cinder/gl/Query.h"

using namespace ci;
using namespace ci::app;
using namespace ci::msw;
using namespace ci::msw::video;
using namespace std;

struct Circle {
	ci::vec4  color;

	ci::vec2  position;
	float     radius;
	float     pad0;

	float     thickness;
	float     length;
	float     offset;
	float     pad2;
};

class WindowsEnhancedVideoApp : public AppNative {
public:
	void prepareSettings( Settings* settings ) override;

	void setup() override;
	void shutdown() override;
	void update() override;
	void draw() override;

	void mouseDown( MouseEvent event ) override;
	void keyDown( KeyEvent event ) override;

	void resize() override;
	void fileDrop( FileDropEvent event ) override;

	bool playVideo( const fs::path &path );

private:
	static const int kMaxMovieCount = 16;

	std::vector<MovieGlRef>        mMovies;

	ci::gl::GlslProgRef            mShader;
	ci::gl::VboRef                 mData;
	ci::gl::VboMeshRef             mMesh;
	ci::gl::BatchRef               mBatch;
};

void WindowsEnhancedVideoApp::prepareSettings( Settings* settings )
{
	settings->disableFrameRate();
	settings->setWindowSize( 1280, 720 );
}

void WindowsEnhancedVideoApp::setup()
{
	gl::enableVerticalSync( true );
	gl::clear();
	gl::color( 1, 1, 1 );

	// create an array of initial per-instance data
	std::vector<Circle> data( kMaxMovieCount );

	// create the VBO which will contain per-instance (rather than per-vertex) data
	mData = gl::Vbo::create( GL_ARRAY_BUFFER, data.size() * sizeof( Circle ), data.data(), GL_DYNAMIC_DRAW );

	geom::BufferLayout instanceDataLayout;
	instanceDataLayout.append( geom::Attrib::CUSTOM_0, 4, sizeof( Circle ), offsetof( Circle, color ), 1 /* per instance */ );
	instanceDataLayout.append( geom::Attrib::CUSTOM_1, 4, sizeof( Circle ), offsetof( Circle, position ), 1 /* per instance */ );
	instanceDataLayout.append( geom::Attrib::CUSTOM_2, 4, sizeof( Circle ), offsetof( Circle, thickness ), 1 /* per instance */ );

	// 
	mMesh = gl::VboMesh::create( geom::Rect( Rectf( -1, -1, 1, 1 ) ) );
	mMesh->appendVbo( instanceDataLayout, mData );

	//
	mShader = gl::GlslProg::create( loadAsset( "circle.vert" ), loadAsset( "circle.frag" ) );

	//
	mBatch = gl::Batch::create( mMesh, mShader, { { geom::Attrib::CUSTOM_0, "vInstanceColor" }, { geom::Attrib::CUSTOM_1, "vInstancePosition" }, { geom::Attrib::CUSTOM_2, "vInstanceAttribs" } } );

}

void WindowsEnhancedVideoApp::shutdown()
{
}

void WindowsEnhancedVideoApp::update()
{
	for( auto itr = mMovies.begin(); itr != mMovies.end(); ) {
		auto &movie = *itr;
		if( !movie || movie->isDone() ) {
			itr = mMovies.erase( itr );
		}
		else {
			++itr;
		}
	}
}

void WindowsEnhancedVideoApp::draw()
{
	gl::clear();

	// Update instance data.
	double time = getElapsedSeconds();
	Circle *ptr = (Circle*) mData->mapWriteOnly( true );
	size_t base = (size_t) ptr;
	for( size_t i = 0; i < mMovies.size(); ++i ) {
		ptr->color = vec4( 1 );
		//ptr->color.a = 0.45f + 0.4f * sinf( float( 2.0 * M_PI ) * ( i / float( mMovies.size() ) ) - (float) time * 8.0f );
		ptr->position = vec2( getWindowSize() / 2 );
		ptr->radius = 0.8f * math<float>::max( getWindowWidth(), getWindowHeight() );
		ptr->thickness = 0.5f;
		ptr->length = 1.0f / mMovies.size();
		ptr->offset = float( time * 0.05 ) + float( i ) / mMovies.size();
		++ptr;
	}
	mData->unmap();

	// Bind textures.
	for( int i = 0; i < (int) mMovies.size(); ++i ) {
		auto movie = mMovies[i];
		if( movie ) {
			auto texture = movie->getTexture();
			if( texture ) {
				texture->bind( i );
			}
		}
	}

	// Draw movies.
	mBatch->drawInstanced( mMovies.size() );
}

void WindowsEnhancedVideoApp::mouseDown( MouseEvent event )
{
}

void WindowsEnhancedVideoApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_ESCAPE:
		quit();
		break;
	case KeyEvent::KEY_o:
		playVideo( getOpenFilePath() );
		break;
	case KeyEvent::KEY_v:
		gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
		break;
	case KeyEvent::KEY_l:
		if( isFrameRateEnabled() )
			disableFrameRate();
		else
			setFrameRate( 60.0f );
		break;
	}
}

void WindowsEnhancedVideoApp::resize()
{
}

void WindowsEnhancedVideoApp::fileDrop( FileDropEvent event )
{
	const fs::path& path = event.getFile( 0 );
	playVideo( path );
}

bool WindowsEnhancedVideoApp::playVideo( const fs::path &path )
{
	if( mMovies.size() < kMaxMovieCount && !path.empty() && fs::exists( path ) ) {
		// TODO: make sure the movie can play
		auto movie = video::MovieGl::create( path );
		movie->play();

		mMovies.push_back( movie );

		return true;
	}

	return false;
}

CINDER_APP_NATIVE( WindowsEnhancedVideoApp, RendererGl )
