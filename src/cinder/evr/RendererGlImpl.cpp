/*
Copyright (c) 2014, The Barbarian Group
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that
the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this list of conditions and
the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include "cinder/Cinder.h"

#if defined( CINDER_MSW )

#include "cinder/evr/RendererGlImpl.h"

namespace cinder {
namespace msw {
namespace video {

MovieGl::MovieGl()
	: MovieBase(), mEVRPresenter( NULL )
{
	if( !wglext_NV_DX_interop ) {
		throw std::runtime_error( "WGL_NV_DX_interop extension not supported. Upgrade your graphics drivers and try again." );
	}
}

MovieGl::~MovieGl()
{
}

MovieGl::MovieGl( const Url& url )
	: MovieGl()
{
	HRESULT hr = S_OK;
	/*
	if( !mEVRPresenter ) {
	mEVRPresenter = new EVRCustomPresenter( hr );
	hr = mEVRPresenter->SetVideoWindow( mHwnd );
	}
	*/
	MovieBase::initFromUrl( url );
	/*
		// TODO: move to method of its own
		if( mTexture && ( mWidth != mTexture->getWidth() || mHeight != mTexture->getHeight() ) ) {
		mEVRPresenter->releaseSharedTexture();
		mTexture.reset();
		}

		if( !mTexture ) {
		gl::Texture2d::Format fmt;
		fmt.setTarget( GL_TEXTURE_RECTANGLE );
		fmt.loadTopDown( true );

		mTexture = gl::Texture2d::create( mWidth, mHeight, fmt );
		mEVRPresenter->createSharedTexture( mWidth, mHeight, mTexture->getId() );
		}
		*/
}

MovieGl::MovieGl( const fs::path& filePath )
	: MovieGl()
{
	HRESULT hr = S_OK;
	/*
	if( !mEVRPresenter ) {
	mEVRPresenter = new EVRCustomPresenter( hr );
	hr = mEVRPresenter->SetVideoWindow( mHwnd );
	}
	*/
	MovieBase::initFromPath( filePath );
	/*
		// TODO: move to method of its own
		if( mTexture && ( mWidth != mTexture->getWidth() || mHeight != mTexture->getHeight() ) ) {
		mEVRPresenter->releaseSharedTexture();
		mTexture.reset();
		}

		if( !mTexture ) {
		gl::Texture2d::Format fmt;
		fmt.setTarget( GL_TEXTURE_RECTANGLE );
		fmt.loadTopDown( true );

		mTexture = gl::Texture2d::create( mWidth, mHeight, fmt );
		mEVRPresenter->createSharedTexture( mWidth, mHeight, mTexture->getId() );
		}
		*/
}

/*
HRESULT MovieGl::createPartialTopology( IMFPresentationDescriptor *pPD )
{
HRESULT hr = S_OK;

msw::ScopedComPtr<IMFTopology> pTopology;
hr = createPlaybackTopology( mMediaSourcePtr, pPD, mHwnd, &pTopology, mEVRPresenter );
if( FAILED( hr ) ) {
CI_LOG_E( "Failed to create playback topology." );
return hr;
}

hr = setMediaInfo( pPD );
if( FAILED( hr ) ) {
CI_LOG_E( "Failed to set media info." );
return hr;
}

// Set the topology on the media session.
hr = mMediaSessionPtr->SetTopology( 0, pTopology );
if( FAILED( hr ) ) {
CI_LOG_E( "Failed to set topology." );
return hr;
}

// If SetTopology succeeds, the media session will queue an MESessionTopologySet event.

return hr;
}
*/
} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW