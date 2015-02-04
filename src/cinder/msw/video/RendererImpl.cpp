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

#include "cinder/msw/video/RendererImpl.h"
#include "cinder/msw/video/MediaFoundationPlayer.h"
#include "cinder/msw/video/DirectShowPlayer.h"

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "shlwapi.lib")

namespace cinder {
namespace msw {
namespace video {

std::map<HWND, MovieBase*> MovieBase::sMovieWindows;

//////////////////////////////////////////////////////////////////////////////////////////

MovieBase::MovieBase()
	: mPlayer( NULL ), mRenderer( NULL ), mHwnd( NULL ), mWidth( 0 ), mHeight( 0 )
	, mIsLoaded( false ), mPlayThroughOk( false ), mIsPlayable( false ), mIsProtected( false ), mIsPlaying( false ), mIsInitialized( false )
	, mPlayingForward( true ), mLoop( false ), mPalindrome( false ), mHasAudio( false ), mHasVideo( false ), mCurrentBackend( BE_UNKNOWN )
{
	mHwnd = createWindow( this );
}

MovieBase::~MovieBase()
{
	if( mPlayer )
		mPlayer->Close();

	SafeRelease( mPlayer );
	SafeDelete( mRenderer );

	destroyWindow( mHwnd );
	mHwnd = NULL;
}

LRESULT MovieBase::WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	switch( message ) {
	case WM_DESTROY:
		PostQuitMessage( 0 );
		break;
	case WM_PLAYER_EVENT:
		if( mPlayer )
			mPlayer->HandleEvent( wParam );
	default:
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	return 0L;
}

LRESULT CALLBACK MovieBase::WndProcDummy( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	MovieBase* movie = NULL;

	switch( message ) {
	case WM_CREATE:
		// Let DefWindowProc handle it.
		break;
	default:
		movie = findMovie( hWnd );
		if( movie )
			return movie->WndProc( hWnd, message, wParam, lParam );
		break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

HWND MovieBase::createWindow( MovieBase* movie )
{
	static const PCWSTR szWindowClass = L"MFPLAYBACK";

	// Register the window class.
	WNDCLASSEX wcex;
	ZeroMemory( &wcex, sizeof( WNDCLASSEX ) );
	wcex.cbSize = sizeof( WNDCLASSEX );
	wcex.style = CS_HREDRAW | CS_VREDRAW;

	wcex.lpfnWndProc = MovieBase::WndProcDummy;
	wcex.hbrBackground = (HBRUSH) NULL; // no bkg brush, the view and the bars should always fill the whole client area
	wcex.lpszClassName = szWindowClass;

	RegisterClassEx( &wcex );

	// Create the window.
	HWND hWnd;
	hWnd = CreateWindow( szWindowClass, L"", WS_OVERLAPPEDWINDOW,
						 CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, NULL, NULL );

	if( hWnd != NULL ) {
		// Keep track of window.
		sMovieWindows[hWnd] = movie;
		CI_LOG_V( "Created a hidden window. Total number of hidden windows is now: " << sMovieWindows.size() );

		// Set window attributes.
		LONG style2 = ::GetWindowLong( hWnd, GWL_STYLE );
		style2 &= ~WS_DLGFRAME;
		style2 &= ~WS_CAPTION;
		style2 &= ~WS_BORDER;
		style2 &= WS_POPUP;

		LONG exstyle2 = ::GetWindowLong( hWnd, GWL_EXSTYLE );
		exstyle2 &= ~WS_EX_DLGMODALFRAME;
		::SetWindowLong( hWnd, GWL_STYLE, style2 );
		::SetWindowLong( hWnd, GWL_EXSTYLE, exstyle2 );

		UpdateWindow( hWnd );

#if _DEBUG
		//ShowWindow( hWnd, SW_SHOWNORMAL ); // TEMP
#endif
	}

	return hWnd;
}

void MovieBase::destroyWindow( HWND hWnd )
{
	auto itr = sMovieWindows.find( hWnd );
	if( itr != sMovieWindows.end() )
		sMovieWindows.erase( itr );

	if( !DestroyWindow( hWnd ) ) {
		// Something went wrong. Use GetLastError() to find out what.
		CI_LOG_E( "Failed to destroy hidden window." );
	}
	else
		CI_LOG_V( "Destroyed a hidden window. Total number of hidden windows is now: " << sMovieWindows.size() );
}

MovieBase* MovieBase::findMovie( HWND hWnd )
{
	auto itr = sMovieWindows.find( hWnd );
	if( itr != sMovieWindows.end() )
		return itr->second;

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////

MovieSurface::MovieSurface()
	: MovieBase()
{
}

MovieSurface::~MovieSurface()
{
}

MovieSurface::MovieSurface( const Url& url )
{
	init( toWideString( url.c_str() ) );
}

MovieSurface::MovieSurface( const fs::path& filePath )
{
	init( filePath.generic_wstring() );
}

void MovieSurface::init( const std::wstring& url )
{
	HRESULT hr = S_OK;

	assert( mHwnd != NULL );

	// Create the renderer.
	//enum { Try_EVR, Try_VMR9, Try_VMR7 };

	//for( DWORD i = Try_EVR; i <= Try_VMR7; i++ ) {
	SafeDelete( mRenderer );

	//	switch( i ) {
	//	case Try_EVR:
	//		CI_LOG_V( "Trying EVR..." );
	mRenderer = new ( std::nothrow ) RendererSampleGrabber();
	//		break;

	//	case Try_VMR9:
	//		CI_LOG_V( "Trying VMR9..." );
	//		mRenderer = new ( std::nothrow ) RendererVMR9();
	//		break;

	//	case Try_VMR7:
	//		CI_LOG_V( "Trying VMR7..." );
	//		mRenderer = new ( std::nothrow ) RendererVMR7();
	//		break;
	//	}

	// Create the player.
	for( int i = 0; i < BE_COUNT; ++i ) {
		SafeRelease( mPlayer );

		if( i == BE_MEDIA_FOUNDATION ) {
			// Try to play the movie using Media Foundation.
			mPlayer = new MediaFoundationPlayer( hr, mHwnd );
			mPlayer->AddRef();
			if( SUCCEEDED( hr ) ) {
				hr = mPlayer->SetVideoRenderer( mRenderer );
				if( SUCCEEDED( hr ) ) {
					hr = mPlayer->OpenFile( url.c_str() );
					if( SUCCEEDED( hr ) ) {
						mCurrentBackend = (PlayerBackends) i;
						break;
					}
				}
			}
		}
		else if( i == BE_DIRECTSHOW ) {
			// Try to play the movie using DirectShow.
			mPlayer = new DirectShowPlayer( hr, mHwnd );
			mPlayer->AddRef();
			if( SUCCEEDED( hr ) ) {
				hr = mPlayer->SetVideoRenderer( mRenderer );
				if( SUCCEEDED( hr ) ) {
					hr = mPlayer->OpenFile( url.c_str() );
					if( SUCCEEDED( hr ) ) {
						mCurrentBackend = (PlayerBackends) i;
						break;
					}
				}
			}
		}
	}

	//	//
	//	if( SUCCEEDED( hr ) )
	//		break;
	//}

	if( FAILED( hr ) ) {
		mCurrentBackend = BE_UNKNOWN;

		SafeRelease( mPlayer );
		SafeDelete( mRenderer );
		CI_LOG_E( "Failed to play movie: " << url.c_str() );

		return;
	}

	// Get width and height of the video.
	mWidth = mPlayer->GetWidth();
	mHeight = mPlayer->GetHeight();
}

const Surface& MovieSurface::getSurface() const
{
	RendererSampleGrabber* pGrabber = dynamic_cast<RendererSampleGrabber*>( mRenderer );

	if( pGrabber && pGrabber->CheckNewFrame() ) {
		mSurface = Surface( mWidth, mHeight, false, SurfaceChannelOrder::BGR );

		if( pGrabber->GetCallback()->CopyPixels( mSurface.getData() ) ) {
		}
	}

	return mSurface;
}

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW