/**
 * High Dynamic Range Display sample. Windows only.
 *
 * This sample shows how you can detect whether a display supports HDR and how to
 * enable HDR mode for that display. It then creates a window with a 16-bit floating
 * point main buffer. Windows then automatically assumes we are using the scRGB
 * color space and will take care of the required color conversions to display it
 * correctly on both HDR and SDR displays.
 *
 * Finally, it renders an HDR cube map. Use your mouse to control the camera.
 * The image will be divided in two halves. Everything on the left will be clamped
 * to SDR, everything on the right will be in HDR. You will only see a difference
 * if you are running the application on an HDR-capable display with HDR enabled. On
 * a standard SDR display, the left and right half should be indistinguishable.
 *
 * To properly render SDR content on an HDR display, you should also query the
 * SDR white level set by the user in the Windows HD Color Settings panel. The value
 * returned by Cinder is the white level in nits. To calculate the multiplier,
 * divide the value by 80:
 *
 * outputColor.rgb = inputColor.rgb * getSdrWhiteLevel() / 80
 *
 * If done correctly, your SDR white should be the same as the window's title bar.
 *
 */

#include "cinder/CameraUi.h"
#include "cinder/Log.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

using namespace ci;
using namespace app;

std::unordered_map<std::string, bool> gState; // HDR state of the display on startup.

class HighDynamicRangeDisplayApp : public App {
  public:
	static void prepare( Settings *settings );

	void setup() override;
	void cleanup() override;

	void draw() override;
	void resize() override;

	void mouseMove( MouseEvent event ) override;

  private:
	gl::Texture2dRef      mIndicator;         // Texture holding the SDR|HDR indicator.
	gl::TextureCubeMapRef mEnvironmentMap;    // Texture holding the environment cube map.
	gl::GlslProgRef       mEnvironmentShader; // Shader used to render the environment.
	CameraPersp           mCamera;            // Our 3D camera.
	CameraUi              mCameraUi;          // Our 3D camera controls.
	int                   mDivider{ 320 };    //
};

void HighDynamicRangeDisplayApp::prepare( Settings *settings )
{
	// Enable HDR on supported displays.
	const auto &displays = Platform::get()->getDisplays();
	for( const auto &display : displays ) {
		CI_LOG_I( "Display detected: " << display->getName() << ( display->supportsHdr() ? " (HDR)" : " (SDR)" ) );

		// Keep track whether HDR was enabled.
		gState.insert_or_assign( display->getName(), display->isHdrEnabled() );

		// Enable HDR and move our window to the first HDR display we find.
		if( display->supportsHdr() ) {
			display->enableHdr( true );

			if( !settings->getDisplay() )
				settings->setDisplay( display );
		}
	}
}

void HighDynamicRangeDisplayApp::setup()
{
	// Load our indicator image.
	mIndicator = gl::Texture2d::create( loadImage( loadAsset( "divider.png" ) ), gl::Texture2d::Format().swizzleMask( GL_ONE, GL_ONE, GL_ONE, GL_RED ) );

	// Load our HDR environment map.
	ImageSourceRef images[] = {
		// Our environment is a cube map with 6 faces:
		loadImage( getAssetPath( "px.hdr" ) ), // +X
		loadImage( getAssetPath( "nx.hdr" ) ), // -X
		loadImage( getAssetPath( "py.hdr" ) ), // +Y
		loadImage( getAssetPath( "ny.hdr" ) ), // -Y
		loadImage( getAssetPath( "pz.hdr" ) ), // +Z
		loadImage( getAssetPath( "nz.hdr" ) )  // -Z
	};
	mEnvironmentMap = gl::TextureCubeMap::create( images, gl::TextureCubeMap::Format().internalFormat( GL_RGB32F ) );

	// Create our environment shader.
	const auto fmt = gl::GlslProg::Format().vertex( loadAsset( "environment.vert" ) ).fragment( loadAsset( "environment.frag" ) );
	mEnvironmentShader = gl::GlslProg::create( fmt );
	mEnvironmentShader->uniform( "uMap", 0 );

	// Setup our camera.
	mCamera.setPerspective( 60.0f, 1.0f, 0.5f, 500.0f );
	mCamera.lookAt( vec3( -35.0f, -27.0f, 8.0f ), vec3( 0, 0, 0 ) );
	mCameraUi.setCamera( &mCamera );
	mCameraUi.connect( getWindow() );
}

void HighDynamicRangeDisplayApp::cleanup()
{
	// Restore HDR mode if necessary.
	const auto &displays = Platform::get()->getDisplays();
	for( const auto &display : displays )
		display->enableHdr( gState[display->getName()] );
}

void HighDynamicRangeDisplayApp::draw()
{
	auto eye = mCamera.getEyePoint();
	auto lookat = mCamera.getPivotPoint();

	// Clear color and depth buffers.
	gl::clear();

	// Enable our camera.
	gl::setMatrices( mCamera );

	// Bind our environment map.
	gl::ScopedTextureBind scpTex0( mEnvironmentMap, 0 );

	// Enable our shader.
	gl::ScopedGlslProg scpGlsl( mEnvironmentShader );
	mEnvironmentShader->uniform( "uDivider", mDivider );
	mEnvironmentShader->uniform( "uWhiteLevel", getDisplay()->getSdrWhiteLevel() );

	// Render geometry.
	// See for implementation details: https://gamedev.stackexchange.com/questions/60313/implementing-a-skybox-with-glsl-version-330/60377#60377
	gl::begin( GL_TRIANGLE_STRIP );
	gl::vertex( -1, -1, 1 );
	gl::vertex( 1, -1, 1 );
	gl::vertex( -1, 1, 1 );
	gl::vertex( 1, 1, 1 );
	gl::end();

	// Render 2D content using the correct white level.
	gl::ScopedColor scpColor( Color::gray( getDisplay()->getSdrWhiteLevel() / 80.0f ) );

	gl::setMatricesWindow( getWindowSize() );
	gl::draw( mIndicator, vec2( mDivider, 20 ) - 0.5f * vec2( mIndicator->getSize() ) );
}

void HighDynamicRangeDisplayApp::resize()
{
	mCamera.setAspectRatio( getWindowAspectRatio() );
}

void HighDynamicRangeDisplayApp::mouseMove( MouseEvent event )
{
	mDivider = event.getX();
}

// If we create a 16-bit floating point back buffer, our output is automatically assumed to be in the HDR-compatible scRGB color space.
CINDER_APP( HighDynamicRangeDisplayApp, RendererGl( RendererGl::Options().colorChannelDepth( 16 ) ), &HighDynamicRangeDisplayApp::prepare )
