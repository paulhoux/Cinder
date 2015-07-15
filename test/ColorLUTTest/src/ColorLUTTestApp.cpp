#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/ip/ColorLUT.h"
#include "cinder/Utilities.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class ColorLUTTestApp : public App {
public:
	void setup() override;
	void draw() override;

private:
	gl::Texture2dRef mTexSrc;
	gl::Texture3dRef mTexLUT;
	gl::GlslProgRef  mShader;
};

void ColorLUTTestApp::setup()
{
	std::string filename = "LUT";

	// Create all possible sizes of the LUT.
	for( size_t sz = 3; sz < 9; ++sz ) {
		auto lut = ip::colorLUT( sz );

		fs::path file = getAssetPath( "" );
		file /= ( filename + "_" + toString( sz ) + "bits_" + toString( lut.getWidth() ) + "x" + toString( lut.getHeight() ) + ".png" );
		writeImage( file, lut );
	}

	// Now load one of the LUT's and invert it.
	auto lut = Surface( loadImage( loadAsset( "LUT_5bits_1024x32.png" ) ) );
	assert( lut.getPixelBytes() == 3 );

	size_t numBytes = lut.getRowBytes() * lut.getHeight();
	size_t numTexels = numBytes / 3;
	size_t size = (size_t)std::cbrt( (double)numTexels );
	assert( isPowerOf2( size ) );

	auto data = lut.getData();
	for( size_t i = 0; i < numBytes; ++i )
		data[i] = ~data[i];

	// Create a texture3D from it.
	mTexLUT = gl::Texture3d::create( lut.getData(), GL_RGB, size, size, size );

	// Load a source texture (to be color graded).
	fs::path srcFile = getAssetPath( "" );
	srcFile /= "../../../samples/data/photo_8.jpg";
	mTexSrc = gl::Texture2d::create( loadImage( srcFile ) );

	// Load color grading shader.
	mShader = gl::GlslProg::create( loadAsset( "color_grading.vert" ), loadAsset( "color_grading.frag" ) );
	mShader->uniform( "uTexSrc", 0 );
	mShader->uniform( "uTexLUT", 1 );
}

void ColorLUTTestApp::draw()
{
	gl::clear();

	auto bounds = Area::proportionalFit( mTexSrc->getBounds(), getWindowBounds(), true, true );

	// Draw original image on the left.
	{
		gl::ScopedTextureBind tex0( mTexSrc );
		gl::ScopedGlslProg shader( gl::getStockShader( gl::ShaderDef().texture() ) );
		gl::drawSolidRect( Rectf( bounds.x1, bounds.y1, bounds.x1 + bounds.getWidth() / 2, bounds.y2 ), vec2( 0, 1 ), vec2( 0.5f, 0 ) );
	}

	// Draw color graded image on the right.
	{
		gl::ScopedTextureBind tex0( mTexSrc, 0 );
		gl::ScopedTextureBind tex1( mTexLUT, 1 );
		gl::ScopedGlslProg shader( mShader );
		gl::drawSolidRect( Rectf( bounds.x1 + bounds.getWidth() / 2, bounds.y1, bounds.x2, bounds.y2 ), vec2( 0.5f, 1 ), vec2( 1, 0 ) );
	}
}

CINDER_APP( ColorLUTTestApp, RendererGl )
