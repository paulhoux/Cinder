/*
Copyright (c) 2022, Paul Houx Creative Coding - All rights reserved.
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

#include <algorithm>
#include <string>

namespace cinder {
namespace nvp {

//!
static void ltrim( std::string &s )
{
	s.erase( s.begin(), std::find_if( s.begin(), s.end(), []( unsigned char ch ) { return !std::isspace( ch ); } ) );
}
//!
static void rtrim( std::string &s )
{
	s.erase( std::find_if( s.rbegin(), s.rend(), []( unsigned char ch ) { return !std::isspace( ch ); } ).base(), s.end() );
}
//!
static std::string trim( std::string s )
{
	ltrim( s );
	rtrim( s );
	return s;
}
//!
static std::string trimNumber( std::string s )
{
	s.erase( std::find_if( s.rbegin(), s.rend(), []( unsigned char ch ) { return ch != '0'; } ).base(), s.end() );
	s.erase( std::find_if( s.rbegin(), s.rend(), []( unsigned char ch ) { return ch != '.'; } ).base(), s.end() );
	return s;
}

//!
static std::string to_lower( std::string_view str )
{
	auto s = std::string( str );
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return std::tolower( c ); } );
	return s;
}
//!
static std::string to_upper( std::string_view str )
{
	auto s = std::string( str );
	std::transform( s.begin(), s.end(), s.begin(), []( unsigned char c ) { return std::toupper( c ); } );
	return s;
}
//!
static std::string filter( std::string_view str, char needle )
{
	std::string result;

	size_t cursor = 0;
	while( true ) {
		const size_t pos = str.substr( cursor ).find( needle );
		if( pos == std::string::npos ) {
			result += str.substr( cursor );
			return result;
		}
		result += str.substr( cursor, pos );
		cursor += pos + 1;
	}
}
//!
static std::string filter( std::string str, const char chars[] )
{
	for( size_t i = 0; i < strlen( chars ); ++i )
		str = filter( str, chars[i] );

	return str;
}

} // namespace nvp
} // namespace cinder