/*
Copyright (c) 2014, The Cinder Project, All rights reserved.

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

#if defined( CINDER_MSW )

#include "cinder/Surface.h"
#include "cinder/Display.h"
#include "cinder/Url.h"
#include "cinder/DataSource.h"
#include "cinder/Thread.h"
#include "cinder/CinderAssert.h"
#include "cinder/Log.h"
#include "cinder/msw/CinderMsw.h"
#include "cinder/evr/IPlayer.h"

// TEMP
#include "cinder/gl/Texture.h"

namespace cinder {
namespace msw {
namespace video {

class MovieBase {
public:
	typedef enum PlayerBackends { BE_MEDIA_FOUNDATION, BE_DIRECTSHOW, BE_COUNT };

public:
	virtual		~MovieBase();

	//! Returns whether the movie has loaded and buffered enough to playback without interruption
	bool		checkPlayable() { /*TODO*/ return false; }
	//! Returns whether the movie has loaded and buffered enough to playback without interruption
	bool		checkPlaythroughOk() { /*TODO*/ return false; }

	//! Returns the width of the movie in pixels
	uint32_t	getWidth() const { return mWidth; }
	//! Returns the height of the movie in pixels
	uint32_t	getHeight() const { return mHeight; }
	//! Returns the size of the movie in pixels
	ivec2		getSize() const { return ivec2( getWidth(), getHeight() ); }
	//! Returns the movie's aspect ratio, the ratio of its width to its height
	float		getAspectRatio() const { return static_cast<float>( mWidth ) / static_cast<float>( mHeight ); }
	//! the Area defining the Movie's bounds in pixels: [0,0]-[width,height]
	Area		getBounds() const { return Area( 0, 0, getWidth(), getHeight() ); }

	//! Returns the movie's pixel aspect ratio. Returns 1.0 if the movie does not contain an explicit pixel aspect ratio.
	float		getPixelAspectRatio() const { /*TODO*/ return 1.0f; }

	//! Returns whether the movie is in a loaded state, implying its structures are ready for reading but it may not be ready for playback
	bool		isLoaded() const { return mIsLoaded; }
	//! Returns whether the movie is playable, implying the movie is fully formed and can be played but media data is still downloading
	bool		isPlayable() const { return mIsPlayable; }
	//! Returns true if the content represented by the movie is protected by DRM
	bool		isProtected() const { return mIsProtected; }
	//! Returns the movie's length measured in seconds
	float		getDuration() const { return mDuration; }
	//! Returns the movie's framerate measured as frames per second
	float		getFramerate() const { return mFrameRate; }
	//! Returns the total number of frames (video samples) in the movie
	uint32_t	getNumFrames() const { /*TODO*/ return 0; }

	//! Returns whether a movie contains at least one visual track, defined as Video, MPEG, Sprite, QuickDraw3D, Text, or TimeCode tracks
	bool		hasVisuals() const { return mHasVideo; }
	//! Returns whether a movie contains at least one audio track, defined as Sound, Music, or MPEG tracks
	bool		hasAudio() const { return mHasAudio; }
	//! Returns whether the first video track in the movie contains an alpha channel. Returns false in the absence of visual media.
	virtual bool hasAlpha() const { return false; }

	//! Returns whether a movie has a new frame available
	bool		checkNewFrame() { /*TODO*/ return false; }

	//! Returns the current time of a movie in seconds
	float		getCurrentTime() const;
	//! Sets the movie to the time \a seconds
	void		seekToTime( float seconds );
	//! Sets the movie time to the start time of frame \a frame
	void		seekToFrame( int frame );
	//! Sets the movie time to its beginning
	void		seekToStart() { /*TODO*/ return; }
	//! Sets the movie time to its end
	void		seekToEnd() { /*TODO*/ return; }
	//! Limits the active portion of a movie to a subset beginning at \a startTime seconds and lasting for \a duration seconds. QuickTime will not process the movie outside the active segment.
	void		setActiveSegment( float startTime, float duration );
	//! Resets the active segment to be the entire movie
	void		resetActiveSegment() { /*TODO*/ return; }

	//! Sets whether the movie is set to loop during playback. If \a palindrome is true, the movie will "ping-pong" back and forth
	void		setLoop( bool loop = true, bool palindrome = false );
	//! Advances the movie by one frame (a single video sample). Ignores looping settings.
	void		stepForward() { /*TODO*/ return; }
	//! Steps backward by one frame (a single video sample). Ignores looping settings.
	void		stepBackward() { /*TODO*/ return; }
	/** Sets the playback rate, which begins playback immediately for nonzero values.
	* 1.0 represents normal speed. Negative values indicate reverse playback and \c 0 stops.
	*
	* Returns a boolean value indicating whether the rate value can be played (some media types cannot be played backwards)
	*/
	bool		setRate( float rate ) { /*TODO*/ }

	//! Sets the audio playback volume ranging from [0 - 1.0]
	void		setVolume( float volume ) { /*TODO*/ }
	//! Gets the audio playback volume ranging from [0 - 1.0]
	float		getVolume() const { /*TODO*/ return 0.0f; }
	//! Returns whether the movie is currently playing or is paused/stopped.
	bool		isPlaying() const { /*TODO*/ return false; }
	//! Returns whether the movie has completely finished playing
	bool		isDone() const { /*TODO*/ return false; }
	//! Begins movie playback.
	void		play() {
		if( mPlayer ) mPlayer->Play(); mIsPlaying = true; 
	}
	//! Stops movie playback.
	void		stop() { if( mPlayer ) mPlayer->Stop(); }

	//! TODO: implement getPlayerHandle();

	signals::signal<void()>&	getNewFrameSignal() { return mSignalNewFrame; }
	signals::signal<void()>&	getReadySignal() { return mSignalReady; }
	signals::signal<void()>&	getCancelledSignal() { return mSignalCancelled; }
	signals::signal<void()>&	getEndedSignal() { return mSignalEnded; }
	signals::signal<void()>&	getJumpedSignal() { return mSignalJumped; }
	signals::signal<void()>&	getOutputWasFlushedSignal() { return mSignalOutputWasFlushed; }

protected:
	MovieBase();

	virtual void  init( const std::wstring &url );
	virtual void  initFromUrl( const Url& url ) { init( toWideString( url.str() ) ); }
	virtual void  initFromPath( const fs::path& filePath ) { init( toWideString( filePath.string() ) ); }
	//virtual void  initFromLoader( const MovieLoader& loader );

	//void      loadAsset();
	//void      updateFrame();
	//uint32_t  countFrames() const;

	void      lock() { mMutex.lock(); }
	void      unlock() { mMutex.unlock(); }

	//void      removeObservers();
	//void      addObservers();

	//! Default window message handler.
	LRESULT WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

	//! Static functions
	static HWND        createWindow( MovieBase* movie );
	static void        destroyWindow( HWND hWnd );
	static MovieBase*  findMovie( HWND hWnd );

	static LRESULT CALLBACK WndProcDummy( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );

protected:
	IPlayer*					mPlayer;
	HWND						mHwnd;

	gl::Texture2dRef			mTexture; // TEMP

	uint32_t					mWidth, mHeight;
	uint32_t					mFrameCount;
	float						mFrameRate;
	float						mDuration;
	std::atomic<bool>			mAssetLoaded;
	bool						mIsLoaded, mPlayThroughOk, mIsPlayable, mIsProtected;
	bool						mPlayingForward, mLoop, mPalindrome;
	bool						mHasAudio, mHasVideo;
	bool						mIsPlaying;	// required to auto-start the movie
	bool						mIsInitialized;

	std::mutex					mMutex;

	signals::signal<void()>		mSignalNewFrame, mSignalReady, mSignalCancelled, mSignalEnded, mSignalJumped, mSignalOutputWasFlushed;

	//! Keeps track of each player's window.
	static std::map<HWND, MovieBase*> sMovieWindows;
};

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW