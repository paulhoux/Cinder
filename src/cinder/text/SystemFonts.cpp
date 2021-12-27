/*
 Copyright (c) 2020, The Cinder Project: http://libcinder.org
 All rights reserved.

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

#include "cinder/Cinder.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"
#include "cinder/text/SystemFonts.h"
#include "cinder/Unicode.h"
#if defined( CINDER_MSW )
	#include <windows.h>
	#include <strsafe.h>
	#include "cinder/msw/CinderMsw.h"
	#include <set>
#elif defined( CINDER_COCOA )
	#include "cinder/cocoa/CinderCocoa.h"
	#include <CoreText/CoreText.h>
#endif

namespace cinder { namespace text {

using namespace std;

#if defined( CINDER_MSW )
namespace {

struct CaseInsensitive { 
    bool operator() (const std::string& a, const std::string& b) const {
        return asciiCaseCmp(a.c_str(), b.c_str()) < 0;
    }
};

static std::set<std::string,CaseInsensitive>	sSystemFonts;
static HDC										sFontDc = nullptr;
static std::mutex								sFontDcMutex;

int __stdcall enumFontFamiliesExProc( ENUMLOGFONTEX *lpelfe, NEWTEXTMETRICEX *lpntme, int fontType, LPARAM lParam )
{
	if( fontType == TRUETYPE_FONTTYPE )
		reinterpret_cast<std::set<std::string>*>( lParam )->insert( toUtf8( (char16_t*)lpelfe->elfFullName ) );

	return 1;
}

void enumSystemFonts()
{
	lock_guard<mutex> lock( sFontDcMutex );
	if( ! sFontDc )
		sFontDc = ::CreateCompatibleDC( NULL );

	::LOGFONT lf;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfFaceName[0] = '\0';
	lf.lfPitchAndFamily = 0;
	static std::set<std::string,CaseInsensitive> tempFonts;
	::EnumFontFamiliesExW( sFontDc, &lf, (FONTENUMPROCW)enumFontFamiliesExProc, (LPARAM)&tempFonts, 0 );
	for( auto &t : tempFonts ) {
		std::wstring temp = msw::toWideString( t );
		::StringCchCopy( lf.lfFaceName, LF_FACESIZE, temp.c_str() );
		::EnumFontFamiliesExW( sFontDc, &lf, (FONTENUMPROCW)enumFontFamiliesExProc, (LPARAM)&sSystemFonts, 0 );
	}
}

std::once_flag enumSystemFontsFlag, loadSystemDefaultFaceDataFlag;
void initSystemFonts()
{
    std::call_once( enumSystemFontsFlag, [](){ enumSystemFonts(); });
}
}

std::unique_ptr<cinder::Buffer> loadSystemFaceData( const std::string &name )
{
	constexpr DWORD TTC_START = 0x66637474; // To retrieve the data from the beginning of the file for TrueType Collection files
	std::unique_ptr<cinder::Buffer> result;
	initSystemFonts();
	auto it = sSystemFonts.find( name.c_str() );
	if( it == sSystemFonts.end() )
		return nullptr;
	else {
		lock_guard<mutex> lock( sFontDcMutex );
		if( ! sFontDc )
			sFontDc = ::CreateCompatibleDC( NULL );

		::LOGFONT lf = { 12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE };
		::StringCchCopy( lf.lfFaceName, LF_FACESIZE, msw::toWideString( it->c_str() ).c_str() );
		HFONT hf = ::CreateFontIndirect( &lf );
		HFONT oldF = (HFONT)::SelectObject( sFontDc, hf );
		DWORD sz = ::GetFontData( sFontDc, TTC_START, 0, NULL, 0 );
		if( sz != GDI_ERROR ) { // if this didn't fail then we have a TTC rather than an individual font
			result = std::make_unique<cinder::Buffer>( sz );
			if( ::GetFontData( sFontDc, TTC_START, 0, result->getData(), sz ) == GDI_ERROR ) {
				::SelectObject( sFontDc, oldF );
				::DeleteObject( hf );
				return false;
			}
		}
		else {
			DWORD sz = ::GetFontData( sFontDc, 0, 0, NULL, 0 );
			if( sz == GDI_ERROR ) {
				::SelectObject( sFontDc, oldF );
				::DeleteObject( hf );
				return nullptr;
			}
			result = std::make_unique<cinder::Buffer>( sz );
			if( ::GetFontData( sFontDc, 0, 0, result->getData(), sz ) == GDI_ERROR ) {
				::SelectObject( sFontDc, oldF );
				::DeleteObject( hf );
				return false;
			}
		}
		::SelectObject( sFontDc, oldF );
		::DeleteObject( hf );
		return result;
	}
}

std::unique_ptr<cinder::Buffer> loadSystemDefaultFaceData()
{
	lock_guard<mutex> lock( sFontDcMutex );
	if( ! sFontDc )
		sFontDc = ::CreateCompatibleDC( NULL );

	NONCLIENTMETRICS metrics;
	metrics.cbSize = sizeof(NONCLIENTMETRICS);
	::SystemParametersInfo( SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &metrics, 0 );
	HFONT hf = ::CreateFontIndirect( &metrics.lfMessageFont );
	HFONT oldF = (HFONT)::SelectObject( sFontDc, hf );
	DWORD sz = ::GetFontData( sFontDc, 0, 0, NULL, 0 );
	if( sz == GDI_ERROR ) {
		::SelectObject( sFontDc, oldF );
		::DeleteObject( hf );
		return nullptr;
	}
	std::unique_ptr<cinder::Buffer> result = std::make_unique<cinder::Buffer>( sz );
	if( ::GetFontData( sFontDc, 0, 0, result->getData(), sz ) == GDI_ERROR ) {
		::SelectObject( sFontDc, oldF );
		::DeleteObject( hf );
		return false;
	}
	::SelectObject( sFontDc, oldF );
	::DeleteObject( hf );
	return result;
}

vector<string> copySystemFaceNames()
{
	initSystemFonts();

	vector<string> result;
	for( auto &s : sSystemFonts )
		result.push_back( s );
		
	return result;
}

#elif defined( CINDER_COCOA )

namespace {
struct CaseInsensitive { 
	bool operator() (const std::string& a, const std::string& b) const {
		return asciiCaseCmp(a.c_str(), b.c_str()) < 0;
	}
};

static std::map<std::string,ci::fs::path,CaseInsensitive>	sSystemFonts;

void enumSystemFonts()
{
	CTFontCollectionRef collection = ::CTFontCollectionCreateFromAvailableFonts( nullptr );
	CFArrayRef array = ::CTFontCollectionCreateMatchingFontDescriptors( collection );
	for( int i = 0, count = (int)::CFArrayGetCount(array); i < count; ++i ) {
		CTFontDescriptorRef font = reinterpret_cast<CTFontDescriptorRef>( ::CFArrayGetValueAtIndex( array, i ) );
		if( ! font )
			continue;

		CFURLRef urlCf = reinterpret_cast<CFURLRef>( ::CTFontDescriptorCopyAttribute( font, kCTFontURLAttribute ) );
		CFStringRef urlStrCf = ::CFURLCopyFileSystemPath( urlCf, kCFURLPOSIXPathStyle );
		CFStringRef nameCf = reinterpret_cast<CFStringRef>( ::CTFontDescriptorCopyAttribute( font, kCTFontDisplayNameAttribute ) );
		sSystemFonts[cocoa::convertCfString( nameCf )] = ci::fs::path( cocoa::convertCfString( urlStrCf ) );

		::CFRelease( nameCf );
		::CFRelease( urlStrCf );
		::CFRelease( urlCf );
	}
	::CFRelease( array );
	::CFRelease( collection );
}
} // anonymous namespace

std::once_flag enumSystemFontsFlag, loadSystemDefaultFaceDataFlag;
void initSystemFonts()
{
    std::call_once( enumSystemFontsFlag, [](){ enumSystemFonts(); });
}

std::unique_ptr<cinder::Buffer> loadSystemFaceData( const std::string &name )
{
	initSystemFonts();
	auto it = sSystemFonts.find( name );
	if( it != sSystemFonts.end() ) {
		Buffer buf( loadFile( it->second ) );
		std::unique_ptr<cinder::Buffer> result = std::make_unique<cinder::Buffer>( buf.getSize() );
		memcpy( result->getData(), buf.getData(), buf.getSize() );
		return result;
	}
	else
		return nullptr;
}

std::unique_ptr<cinder::Buffer> loadSystemDefaultFaceData()
{
	// queries the system for the default label font, and then records the name
	CTFontRef font = ::CTFontCreateUIFontForLanguage( kCTFontUIFontSystem, 0.0, nullptr );
	CTFontDescriptorRef descriptorRef = ::CTFontCopyFontDescriptor( font );
	CFURLRef urlCf = reinterpret_cast<CFURLRef>( ::CTFontDescriptorCopyAttribute( descriptorRef, kCTFontURLAttribute ) );
	CFStringRef urlStrCf = ::CFURLCopyFileSystemPath( urlCf, kCFURLPOSIXPathStyle );

	Buffer buf( loadFile( ci::fs::path( cocoa::convertCfString( urlStrCf ) ) ) );
	std::unique_ptr<cinder::Buffer> result = std::make_unique<cinder::Buffer>( buf.getSize() );
	memcpy( result->getData(), buf.getData(), buf.getSize() );

	::CFRelease( descriptorRef );
	::CFRelease( font );
	::CFRelease( urlStrCf );
	::CFRelease( urlCf );

	return result;
}

vector<string> copySystemFaceNames()
{
	initSystemFonts();

	vector<string> result;
	for( auto &s : sSystemFonts )
		result.push_back( s.first );
		
	return result;
}

#else
bool findSystemFont( const char *name, cinder::Buffer *outBuffer, cinder::fs::path *outPath )
{
	return false;
}

std::unique_ptr<cinder::Buffer> loadSystemFaceData( const std::string &name )
{
	return nullptr;
}

std::unique_ptr<cinder::Buffer> loadSystemDefaultFaceData()
{
	return nullptr;
}

vector<string> copySystemFaceNames()
{
	return vector<string>();
}
#endif

} } // namespace cinder::text
