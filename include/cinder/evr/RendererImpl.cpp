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

#include "cinder/evr/RendererImpl.h"
#include "cinder/evr/MediaFoundationPlayer.h"
#include "cinder/evr/DirectShowPlayer.h"

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

template <class Q>
HRESULT GetEventObject( IMFMediaEvent *pEvent, Q **ppObject )
{
	*ppObject = NULL;

	PROPVARIANT var;
	HRESULT hr = pEvent->GetValue( &var );
	if( SUCCEEDED( hr ) ) {
		if( var.vt == VT_UNKNOWN ) {
			hr = var.punkVal->QueryInterface( ppObject );
		}
		else {
			hr = MF_E_INVALIDTYPE;
		}
		PropVariantClear( &var );
	}

	return hr;
}

//////////////////////////////////////////////////////////////////////////////////////////

MovieBase::MovieBase()
	: mPlayer( NULL ), mHwnd( NULL ), mWidth( 0 ), mHeight( 0 )
	, mIsLoaded( false ), mPlayThroughOk( false ), mIsPlayable( false ), mIsProtected( false ), mIsPlaying( false ), mIsInitialized( false )
	, mPlayingForward( true ), mLoop( false ), mPalindrome( false ), mHasAudio( false ), mHasVideo( false )
{
	mHwnd = createWindow( this );
}

MovieBase::~MovieBase()
{
	SafeRelease( mPlayer );

	destroyWindow( mHwnd );
	mHwnd = NULL;
}

void MovieBase::init( const std::wstring &url )
{
	HRESULT hr = S_OK;

	assert( mHwnd != NULL );

	for( int i = 0; i < BE_COUNT; ++i ) {
		SafeRelease( mPlayer );

		if( i == BE_DIRECTSHOW ) {
			// Try to play the movie using DirectShow.
			mPlayer = new DirectShowPlayer( hr, mHwnd );
			mPlayer->AddRef();
			if( SUCCEEDED( hr ) ) {
				hr = mPlayer->OpenFile( url.c_str() );
				if( SUCCEEDED( hr ) )
					break;
			}
		}
		else if( i == BE_MEDIA_FOUNDATION ) {
			// Try to play the movie using Media Foundation.
			mPlayer = new MediaFoundationPlayer( hr, mHwnd );
			//mPlayer->AddRef();
			if( SUCCEEDED( hr ) ) {
				hr = mPlayer->OpenFile( url.c_str() );
				if( SUCCEEDED( hr ) )
					break;
			}
		}
	}

	if( FAILED( hr ) ) {
		CI_LOG_V( "Failed to play movie using DirectShow." );
		SafeRelease( mPlayer );

		// log error
		CI_LOG_E( "Failed to play movie: " << url.c_str() );
	}
	else {
		mWidth = mPlayer->GetWidth();
		mHeight = mPlayer->GetHeight();
	}
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

////////////////////////////////////////////////////////////////////////////////////////////////

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

} // namespace video
} // namespace msw
} // namespace cinder

#endif // CINDER_MSW