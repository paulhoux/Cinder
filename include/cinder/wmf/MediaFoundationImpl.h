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

#include "cinder/wmf/MediaFoundation.h"
#if ( _WIN32_WINNT >= _WIN32_WINNT_VISTA ) // Requires Windows Vista

#include "cinder/Area.h"
#include "cinder/Vector.h"

namespace cinder {
namespace wmf {

class MovieBase {
public:
	MovieBase() {}
	virtual ~MovieBase() {}

	//! Returns the width of the movie in pixels.
	int32_t			getWidth() const { return getObj()->mWidth; }
	//! Returns the height of the movie in pixels.
	int32_t			getHeight() const { return getObj()->mHeight; }
	//! Returns the size of the movie in pixels.
	ivec2			getSize() const { return ivec2( getWidth(), getHeight() ); }
	//! Returns the movie's aspect ratio, the ratio of its width to its height.
	float			getAspectRatio() const { return getObj()->mWidth / (float)getObj()->mHeight; }
	//! the Area defining the Movie's bounds in pixels: [0,0]-[width,height].
	Area			getBounds() const { return Area( 0, 0, getWidth(), getHeight() ); }
	//! Returns the video's duration measured in seconds.
	float			getDuration() const { return getObj()->mDuration; }

	//! Returns the video's pixel aspect ratio. Returns 1.0 if the video does not contain an explicit pixel aspect ratio.
	virtual float	getPixelAspectRatio() const { return 1.0f; }
	//! Returns the video's framerate measured as frames per second.
	virtual float	getFramerate() const { return 0; }
	//! Returns the total number of frames (video samples) in the video.
	virtual int32_t	getNumFrames() const { return 0; }

	//! Returns whether the first video track in the video contains an alpha channel. Returns false in the absence of visual media.
	virtual bool	hasAlpha() const { return false; }
	//! Returns whether a video contains at least one visual track.
	virtual bool	hasVisuals() const { return false; }
	//! Returns whether a video contains at least one audio track.
	virtual bool	hasAudio() const { return false; }

	//! Returns whether a video has a new frame available.
	virtual bool	checkNewFrame() { return false; }

	//! Returns the current time of a video in seconds.
	virtual float	getCurrentTime() const { return 0.0f; }
	//! Sets the video to the time \a seconds.
	virtual void	seekToTime( float seconds ) {}
	//! Sets the video time to the start time of frame \a frame.
	virtual void	seekToFrame( int frame ) {}
	//! Sets the video time to its beginning.
	virtual void	seekToStart() {}
	//! Sets the video time to its end.
	virtual void	seekToEnd() {}

	//! Sets whether the video is set to loop during playback.
	virtual void	setLoop( bool loop = true ) {}
	//! Sets the playback rate, which begins playback immediately for nonzero values. 1.0 represents normal speed. Negative values indicate reverse playback and \c 0 stops.
	virtual void	setRate( float rate ) {}

	//! Sets the audio playback volume ranging from [0 - 1.0].
	virtual void	setVolume( float volume ) {}
	//! Gets the audio playback volume ranging from [0 - 1.0].
	virtual float	getVolume() const { return 1.0f; }

	//! Returns whether the video is currently playing or is paused/stopped.
	virtual bool	isPlaying() const = 0;
	//! Returns whether the video has completely finished playing.
	virtual bool	isDone() const = 0;
	//! Begins video playback.
	virtual void	play() = 0;
	//! Stops video playback.
	virtual void	stop() = 0;

	//! Draws the video at full resolution.
	virtual void	draw() = 0;
	//! Draws the video using the specified \a bounds.
	virtual void	draw( const Area &bounds ) = 0;

protected:
	//! Obtains the latest frame from the video player and creates a texture from it.
	virtual void updateFrame() = 0;

protected:
	struct Obj {
		Obj() : mWidth( 0 ), mHeight( 0 ), mDuration( 0 ) {}
		virtual ~Obj() {}

		int32_t  mWidth, mHeight;
		float    mDuration;
	};

	virtual Obj*	getObj() const = 0;
};

} } // namespace ci::wmf

#endif // ( _WIN32_WINNT >= _WIN32_WINNT_VISTA )