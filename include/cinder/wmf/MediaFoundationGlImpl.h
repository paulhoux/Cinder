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
#include "cinder/msw/CinderMsw.h"
#include "cinder/Signals.h"
#include "cinder/gl/gl.h"

namespace cinder {
namespace wmf {

//! Forward declarations
class Player;

typedef std::shared_ptr<class MovieGl> MovieGlRef;

//! Uses the Windows Media Foundation backend to render video directly to a texture using hardware support where available.
class MovieGl : public MovieBase {
  public:
	~MovieGl();

	static MovieGlRef create( const fs::path& path ) { return std::shared_ptr<MovieGl>( new MovieGl( path ) ); }

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
	//! Returns whether the video is currently playing or is set to loop.
	bool isLooping() const;
	//! Returns whether the video has completely finished playing.
	bool isDone() const;
	//! Returns whether scrubbing the video is enabled.
	bool isScrubbing() const;
	//! Returns \c true if the video is being rendered using DX9, \c false if using DX11.
	bool isDX9() const;

	//! Begins video playback.
	void play() override;
	//! Stops video playback.
	void stop() override;
	//! Enables/disables scrubbing.
	void setScrubbing( bool enable = true );

	//! Draws the video at full resolution.
	void draw() override;
	//! Draws the video using the specified \a bounds.
	void draw( const ci::Area& bounds ) override;

	//! Returns the gl::Texture representing the Movie's current frame, bound to the \c GL_TEXTURE_RECTANGLE_ARB target.
	const gl::TextureRef& getTexture();

  protected:
	MovieGl();
	MovieGl( const fs::path& path );

	void close();

	//! Obtains the latest frame from the video player and creates a texture from it.
	void updateFrame() override;

  protected:
	struct Obj : public MovieBase::Obj {
		Obj();
		~Obj();

		// Instance of the Media Foundation player backend.
		Player* mPlayerPtr;
		// DX/GL interop device handle.
		HANDLE mDeviceHandle;
	};

	std::shared_ptr<Obj> mObj;
	MovieBase::Obj*      getObj() const override { return mObj.get(); }

	// GL texture, shared from DX.
	gl::Texture2dRef mTexture;

	signals::ScopedConnection mConnClose;

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