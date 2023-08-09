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

#include "cinder/nvp/Values.h"

#include "cinder/Utilities.h"
#include "cinder/nvp/Algorithm.h"

namespace cinder {
namespace nvp {

Number::Number( const std::string &v )
{
	if( v.empty() || v == "none" || v == "inherit" )
		return;

	if( v == "auto" ) {
		mValue = 1;
		mIsPercentage = true;
		return;
	}

	const auto value = std::stof( filter( v, '%' ) );
	if( std::isfinite( value ) )
		mValue = value;

	if( v.find( '%' ) != std::string::npos ) {
		mValue /= 100.0f;
		mIsPercentage = true;
	}
	else if( v.find( "cm" ) != std::string::npos ) {
		mValue /= 0.028f; // Size of a pixel in cm at 90dpi, see https://www.w3.org/TR/2008/REC-CSS2-20080411/syndata.html#length-units
	}
	else if( v.find( "mm" ) != std::string::npos ) {
		mValue /= 0.28f; // Size of a pixel in mm at 90dpi, see https://www.w3.org/TR/2008/REC-CSS2-20080411/syndata.html#length-units
	}
	else if( v.find( "in" ) != std::string::npos ) {
		mValue *= 90.0f; // See https://www.w3.org/TR/2008/REC-CSS2-20080411/syndata.html#length-units
	}
}

std::vector<Number> Number::split( const std::string &s, const std::string &separators )
{
	std::vector<Number> result;

	const auto values = ci::split( s, separators );
	result.reserve( values.size() );

	for( const auto &v : values )
		result.emplace_back( v );

	return result;
}

std::string Number::toString() const
{
	if( mIsPercentage )
		return trimNumber( std::to_string( mValue * 100 ) ) + "%";

	return trimNumber( std::to_string( mValue ) );
}

} // namespace nvp
} // namespace cinder
