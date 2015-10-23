
#include "cinder/video/VideoTexture.h"
#include "cinder/msw/CinderMsw.h"
#include "cinder/app/App.h"

// OpenGL interop stuff
#include "cinder/gl/Context.h"
#include "glload/wgl_all.h"

namespace cinder {

#if defined(CINDER_MSW)
using namespace msw;
using namespace msw::detail;
#endif

namespace gl {

VideoTexture::VideoTexture()
#if defined(CINDER_MSW)
	: m_pFrame( NULL )
	, m_hDevice( NULL )
	, m_hObject( NULL )
	, m_texId( 0 )
#endif
{
	// Create player.
#if defined(CINDER_MSW)
	mPlayerPtr = new msw::MFPlayer();
#endif

	// Make sure this instance is properly destroyed before the application quits.
	mConnCleanup = app::App::get()->getSignalCleanup().connect( [&]() { close(); } );
}

VideoTexture::VideoTexture( const fs::path & path )
	: VideoTexture()
{
#if defined(CINDER_MSW)
	if( mPlayerPtr ) {
		mPlayerPtr->OpenURL( msw::toWideString( path.string() ).c_str() );
	}
#endif
}

VideoTexture::~VideoTexture()
{
	close();
}

void VideoTexture::setLoop( bool enabled )
{
#if defined(CINDER_MSW)
	mPlayerPtr->SetLoop( enabled );
#endif
}

void VideoTexture::play()
{
#if defined(CINDER_MSW)
	if( mPlayerPtr )
		mPlayerPtr->Play();
#endif
}

void VideoTexture::stop()
{
#if defined(CINDER_MSW)
	if( mPlayerPtr )
		mPlayerPtr->Pause();
#endif
}

const Texture2dRef VideoTexture::getTexture() 
{
	ci::gl::Texture2dRef result;

#if defined(CINDER_MSW)
	HRESULT hr = S_OK;

	do {
		BREAK_ON_NULL( mPlayerPtr, E_POINTER );

		ScopedComPtr<PresenterDX9> pPresenterDX9;
		hr = mPlayerPtr->QueryInterface( __uuidof( PresenterDX9 ), (void**)&pPresenterDX9 );
		if( SUCCEEDED( hr ) ) {
			ScopedComPtr<IDirect3DSurface9> pFrame;
			hr = pPresenterDX9->GetFrame( &pFrame );
			BREAK_ON_FAIL( hr );

			// TODO
		}
		else {
			ScopedComPtr<PresenterDX11> pPresenterDX11;
			hr = mPlayerPtr->QueryInterface( __uuidof( PresenterDX11 ), (void**)&pPresenterDX11 );
			if( SUCCEEDED( hr ) ) {
				// Get latest video frame.
				ScopedComPtr<ID3D11Texture2D> pFrame;
				hr = pPresenterDX11->GetFrame( &pFrame );
				BREAK_ON_FAIL( hr );

				if( m_pFrame ) {
					BOOL unlocked = ::wglDXUnlockObjectsNV( m_hDevice, 1, &m_hObject );

					BOOL unregistered = ::wglDXUnregisterObjectNV( m_hDevice, m_hObject );

					::glDeleteTextures( 1, &m_texId );
					m_texId = 0;

					//hr = pPresenterDX11->ReturnFrame( static_cast<ID3D11Texture2D*>( m_pFrame ) );
					SafeRelease( m_pFrame );
				}

				m_pFrame = pFrame;
				m_pFrame->AddRef();

				// Open Interop device.
				if( NULL == m_hDevice ) {
					ScopedComPtr<ID3D11Device> pDevice;
					pFrame->GetDevice( &pDevice );
					BREAK_ON_NULL( pDevice, E_POINTER );

					m_hDevice = ::wglDXOpenDeviceNV( pDevice );
					BREAK_ON_NULL_MSG( m_hDevice, E_POINTER, "Failed to open DX/GL interop device." );
				}

				// Generate texture name.
				if( 0 == m_texId ) {
					::glGenTextures( 1, &m_texId );
					BREAK_IF_TRUE( m_texId == 0, E_FAIL );
				}

				// Register with OpenGL.
				m_hObject = ::wglDXRegisterObjectNV( m_hDevice, (void*)pFrame, m_texId, GL_TEXTURE_RECTANGLE, WGL_ACCESS_READ_ONLY_NV );
				if( NULL == m_hObject ) {
					DWORD err = ::GetLastError();
					switch( err ) {
						case ERROR_INVALID_HANDLE:
							CI_LOG_E( "ERROR_INVALID_HANDLE" );
							break;
						case ERROR_INVALID_DATA:
							CI_LOG_E( "ERROR_INVALID_DATA" );
							break;
						case ERROR_OPEN_FAILED:
							CI_LOG_E( "ERROR_OPEN_FAILED" );
							break;
						default:
							CI_LOG_E( "Unknown error" );
							break;
					}
					BREAK_ON_NULL( hr, E_FAIL );
				}

				// Lock texture.
				BOOL locked = ::wglDXLockObjectsNV( m_hDevice, 1, &m_hObject );
				BREAK_IF_FALSE( locked, E_FAIL );

				D3D11_TEXTURE2D_DESC desc;
				pFrame->GetDesc( &desc );

				result = ci::gl::Texture2d::create( GL_TEXTURE_RECTANGLE, m_texId, desc.Width, desc.Height, true, []( ci::gl::Texture2d* ) {} );
				result->setTopDown( true );
			}
		}
	} while( FALSE );






#endif

	return result;
}

void VideoTexture::close()
{
#if defined(CINDER_MSW)
	if( m_pFrame ) {
		BOOL unlocked = ::wglDXUnlockObjectsNV( m_hDevice, 1, &m_hObject );
		BOOL unregistered = ::wglDXUnregisterObjectNV( m_hDevice, m_hObject );
		m_hObject = NULL;

		::glDeleteTextures( 1, &m_texId );
		m_texId = 0;

		BOOL closed = ::wglDXCloseDeviceNV( m_hDevice );
		m_hDevice = NULL;

		// TODO: copy existing textures?

		SafeRelease( m_pFrame );
	}

	if( mPlayerPtr )
		mPlayerPtr->Close();

	SafeRelease( mPlayerPtr );
#endif
}

}
}

