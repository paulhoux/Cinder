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

#include "cinder/wmf/MediaFoundationImpl.h"
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/wmf/Player.h"
#include "cinder/wmf/MediaSink.h"
#include "cinder/wmf/Presenter.h"

#include "cinder/Signals.h"
#include "cinder/gl/gl.h"
#include "glload/wgl_all.h"

namespace cinder {
namespace wmf {

typedef std::shared_ptr<class MovieGl> MovieGlRef;

//! Uses the Windows Media Foundation backend to render video directly to a texture using hardware support where available.
class MovieGl : public MovieBase {
public:
	~MovieGl();

	static MovieGlRef create( const fs::path &path ) { return std::shared_ptr<MovieGl>( new MovieGl( path ) ); }

	//! Returns the current time of a video in seconds.
	float getCurrentTime() const;
	//! Sets the video to the time \a seconds.
	void seekToTime( float seconds );
	//! Sets the video time to the start time of frame \a frame.
	void seekToFrame( int frame );
	//! Sets the video time to its beginning.
	void seekToStart();
	//! Sets the video time to its end.
	void seekToEnd();

	//! Enable or disable looping.
	void setLoop( bool enabled = true ) override;
	
	//! Returns whether the video is currently playing or is paused/stopped.
	bool isPlaying() const;
	//! Returns whether the video has completely finished playing.
	bool isDone() const;
	//! Begins video playback.
	void play() override;
	//! Stops video playback.
	void stop() override;

	//! Draws the video at full resolution.
	void draw() override;
	//! Draws the video using the specified \a bounds.
	void draw( const ci::Area &bounds ) override;

	//! Returns the gl::Texture representing the Movie's current frame, bound to the \c GL_TEXTURE_RECTANGLE_ARB target.
	const gl::TextureRef& getTexture();

protected:
	MovieGl();
	MovieGl( const fs::path &path );

	void close();

	//! Obtains the latest frame from the video player and creates a texture from it.
	void updateFrame() override;

protected:
	struct Obj : public MovieBase::Obj {
		Obj()
			: mPlayerPtr( NULL )
			, mDeviceHandle( NULL )
		{
			DirectXVersion dxVersion = DX_11;

			// Check availability of interop extensions.
			if( !wglext_NV_DX_interop2 ) {
				CI_LOG_V( "NV_DX_interop2 extension not available, trying DX9..." );
				dxVersion = DX_9;
			}

			if( !wglext_NV_DX_interop ) {
				throw std::exception( "NV_DX_interop extension not available!" );
			}

			// Create player.
			mPlayerPtr = new Player( dxVersion ); // Created with ref count = 1.
			if( NULL == mPlayerPtr )
				throw std::exception( "Out of memory" );

			// Note: interop device handle will be created when
			//       we receive the first video frame.
		}
		~Obj()
		{
			CI_LOG_I( "Destroying MovieGl::Obj" );

			// Close interop device.
			if( mDeviceHandle ) {
				BOOL closed = ::wglDXCloseDeviceNV( mDeviceHandle );
				mDeviceHandle = NULL;
			}

			// Close player.
			if( mPlayerPtr )
				mPlayerPtr->Close();

			// Release player.
			msw::SafeRelease( mPlayerPtr );
		}

		// Instance of the Media Foundation player backend.
		Player*                      mPlayerPtr;
		// DX/GL interop device handle.
		HANDLE                       mDeviceHandle;
	};

	std::shared_ptr<Obj>	mObj;
	MovieBase::Obj*	getObj() const override { return mObj.get(); }

	// GL texture, shared from DX.
	gl::Texture2dRef             mTexture;

	signals::ScopedConnection    mConnClose;

public:
	//@{
	//! Emulates shared_ptr-like behavior
	typedef std::shared_ptr<Obj> MovieGl::*unspecified_bool_type;
	operator unspecified_bool_type() const { return ( mObj.get() == 0 ) ? 0 : &MovieGl::mObj; }
	void reset() { mObj.reset(); }
	//@}  
};

}
} // namespace ci::wmf

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )