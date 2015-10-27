/*
Copyright (c) 2015, The Barbarian Group
All rights reserved.

Copyright (c) Microsoft Open Technologies, Inc. All rights reserved.

This code is intended for use with the Cinder C++ library: http://libcinder.org

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

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Signals.h"
#include "cinder/gl/gl.h"
#include "cinder/wmf/MediaFoundationImpl.h"

#include "cinder/msw/CinderMsw.h"
#include "cinder/msw/MediaFoundation.h"
#include "cinder/msw/ScopedPtr.h"
#include "cinder/msw/detail/MediaSink.h"
#include "cinder/msw/detail/PresenterDX9.h"
#include "cinder/msw/detail/PresenterDX11.h"

#include "glload/wgl_all.h"

namespace cinder {
namespace wmf {

typedef std::shared_ptr<class MovieGl> MovieGlRef;

//! Uses the Windows Media Foundation backend to render video directly to a texture using hardware support where available.
class MovieGl : public MovieBase {
public:
	~MovieGl();

	static MovieGlRef create( const fs::path &path ) { return std::shared_ptr<MovieGl>( new MovieGl( path ) ); }

	//! Enable or disable looping.
	void setLoop( bool enabled = true ) override;

	//! Begins video playback.
	void play() override;
	//! Stops video playback.
	void stop() override;

	//! Draws the video at full resolution.
	void draw() override {}
	//! Draws the video using the specified \a bounds.
	void draw( const ci::Area &bounds ) override {}

	//! Returns the gl::Texture representing the Movie's current frame, bound to the \c GL_TEXTURE_RECTANGLE_ARB target.
	const gl::TextureRef getTexture();

protected:
	MovieGl();
	MovieGl( const fs::path &path );

	void close();

protected:
	struct Obj : public MovieBase::Obj {
		Obj()
			: mPlayerPtr( NULL )
			, mDeviceHandle( NULL )
		{
			// Check availability of interop extensions.
			if( msw::detail::MediaSink::s_version > msw::MFPlayerDirectXVersion::DX_9 && !wglext_NV_DX_interop2 ) {
				CI_LOG_V( "NV_DX_interop2 extension not available, trying DX9..." );
				msw::detail::MediaSink::s_version = msw::MFPlayerDirectXVersion::DX_9;
			}

			if( !wglext_NV_DX_interop ) {
				throw std::exception( "NV_DX_interop extension not available!" );
			}

			// Create player.
			mPlayerPtr = new msw::MFPlayer(); // Created with ref count = 1.
			if( NULL == mPlayerPtr )
				throw std::exception( "Out of memory" );
		}
		~Obj()
		{
			SafeRelease( mPlayerPtr );
		}

		// Instance of the Media Foundation player backend.
		msw::MFPlayer*               mPlayerPtr;
		// DX/GL interop device handle.
		HANDLE                       mDeviceHandle;
		// GL textures, shared from DX.
		std::vector<gl::TextureRef>  mTextures;

		signals::ScopedConnection    mConnCleanup;
	};

	std::shared_ptr<Obj>	mObj;
	MovieBase::Obj*	getObj() const override { return mObj.get(); }

public:
	//@{
	//! Emulates shared_ptr-like behavior
	typedef std::shared_ptr<Obj> MovieGl::*unspecified_bool_type;
	operator unspecified_bool_type() const { return ( mObj.get() == 0 ) ? 0 : &MovieGl::mObj; }
	void reset() { mObj.reset(); }
	//@}  
};

}
}