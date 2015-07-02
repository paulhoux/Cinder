

#pragma once

#include "cinder/Area.h"
#include "cinder/Vector.h"

namespace cinder {
namespace video {

class VideoBase {
public:
	VideoBase() : mWidth( 0 ), mHeight( 0 ), mDuration( 0 ) {}
	virtual ~VideoBase() {}

	//! Returns the width of the video in pixels
	int32_t		getWidth() const { return mWidth; }
	//! Returns the height of the video in pixels
	int32_t		getHeight() const { return mHeight; }
	//! Returns the size of the video in pixels
	ivec2		getSize() const { return ivec2( getWidth(), getHeight() ); }
	//! Returns the video's aspect ratio, the ratio of its width to its height
	float		getAspectRatio() const { return mWidth / (float) mHeight; }
	//! the Area defining the Movie's bounds in pixels: [0,0]-[width,height]
	Area		getBounds() const { return Area( 0, 0, getWidth(), getHeight() ); }
	//! Returns the video's length measured in seconds
	float		getDuration() const { return mDuration; }

	//! Returns the video's pixel aspect ratio. Returns 1.0 if the video does not contain an explicit pixel aspect ratio.
	virtual float	getPixelAspectRatio() const { return 1.0f; }
	//! Returns the video's framerate measured as frames per second
	virtual float	getFramerate() const { return 0.0f; }
	//! Returns the total number of frames (video samples) in the video
	virtual int32_t	getNumFrames() const { return 0; }
	//! Returns whether the first video track in the video contains an alpha channel. Returns false in the absence of visual media.
	virtual bool	hasAlpha() const { return false; }

	//! Returns whether a video contains at least one visual track
	virtual bool	hasVisuals() const { return false; }
	//! Returns whether a video contains at least one audio track
	virtual bool	hasAudio() const { return false; }

	//! Returns whether a video has a new frame available
	virtual bool	checkNewFrame() { return false; }

	//! Returns the current time of a video in seconds
	virtual float	getCurrentTime() const { return 0.0f; }
	//! Sets the video to the time \a seconds
	virtual void	seekToTime( float seconds ) {}
	//! Sets the video time to the start time of frame \a frame
	virtual void	seekToFrame( int frame ) {}
	//! Sets the video time to its beginning
	virtual void	seekToStart() {}
	//! Sets the video time to its end
	virtual void	seekToEnd() {}

	//! Sets whether the video is set to loop during playback.
	virtual void	setLoop( bool loop = true ) {}
	//! Sets the playback rate, which begins playback immediately for nonzero values. 1.0 represents normal speed. Negative values indicate reverse playback and \c 0 stops.
	virtual void	setRate( float rate ) {}

	//! Sets the audio playback volume ranging from [0 - 1.0]
	virtual void	setVolume( float volume ) {}
	//! Gets the audio playback volume ranging from [0 - 1.0]
	virtual float	getVolume() const { return 1.0f; }

	//! Returns whether the video is currently playing or is paused/stopped.
	virtual bool	isPlaying() const { return true; }
	//! Returns whether the video has completely finished playing
	virtual bool	isDone() const { return true; }
	//! Begins video playback.
	virtual void	play() = 0;
	//! Stops video playback.
	virtual void	stop() = 0;

protected:
	uint32_t  mWidth, mHeight;
	float     mDuration;
};

}
}