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

#include "cinder/nvp/Transform.h"

#include "cinder/Utilities.h"
#include "cinder/nvp/Algorithm.h"
#include "cinder/nvp/Values.h"

namespace cinder {
namespace nvp {

Transform::Transform( const std::string &param )
{
	if( param.empty() )
		return;

	auto parts = cinder::split( filter( param, "\t\n\r" ), '(' );
	for( auto itr = parts.begin(); itr != parts.end(); ) {
		++itr;
		const auto s = split( *itr, ')' );
		*itr = s.front();
		++itr;
		if( !s.back().empty() )
			itr = parts.insert( itr, s.back() );
	}
	for( auto itr = parts.begin(); itr != parts.end(); ) {
		const auto command = trim( filter( *itr++, ',' ) );
		const auto params = trim( *itr++ );
		const auto numbers = split( params, " ," );

		if( command == "matrix" ) {
			if( numbers.size() == 6 ) {
				mTransform = glm::mat3x2( Number( numbers.at( 0 ) ), Number( numbers.at( 1 ) ), Number( numbers.at( 2 ) ), Number( numbers.at( 3 ) ), Number( numbers.at( 4 ) ), Number( numbers.at( 5 ) ) );
			}
		}
		else if( command == "translate" ) {
			const float x = Number( numbers.at( 0 ) ); // TODO support both px and % units.
			const float y = numbers.size() > 1 ? Number( numbers.at( 1 ) ) : 0;
			mTransform = glm::translate( mTransform, vec2( x, y ) );
		}
		else if( command == "translateX" ) {
			const float x = Number( numbers.at( 0 ) ); // TODO support both px and % units.
			mTransform = glm::translate( mTransform, vec2( x, 0 ) );
		}
		else if( command == "translateY" ) {
			const float y = Number( numbers.at( 0 ) ); // TODO support both px and % units.
			mTransform = glm::translate( mTransform, vec2( y, 0 ) );
		}
		else if( command == "scale" ) {
			const float sx = Number( numbers.at( 0 ) );
			const float sy = numbers.size() > 1 ? Number( numbers.at( 1 ) ) : sx;
			mTransform = glm::scale( mTransform, vec2( sx, sy ) );
		}
		else if( command == "scaleX" ) {
			const float s = Number( numbers.at( 0 ) );
			mTransform = glm::scale( mTransform, vec2( s, 1 ) );
		}
		else if( command == "scaleY" ) {
			const float s = Number( numbers.at( 0 ) );
			mTransform = glm::scale( mTransform, vec2( 1, s ) );
		}
		else if( command == "rotate" ) {
			const float a = Number( numbers.at( 0 ) );
			const float x = numbers.size() > 1 ? Number( numbers.at( 1 ) ) : 0;
			const float y = numbers.size() > 2 ? Number( numbers.at( 2 ) ) : 0;
			mTransform = glm::translate( mTransform, vec2( x, y ) );
			mTransform = glm::rotate( mTransform, glm::radians( a ) );
			mTransform = glm::translate( mTransform, vec2( -x, -y ) );
		}
		else if( command == "rotateX" ) {
			const float a = Number( numbers.at( 0 ) );                                       // TODO support both rad, deg and turn
			mTransform = glm::scale( mTransform, vec2( 1, glm::cos( glm::radians( a ) ) ) ); // TODO proper 3D rotation
		}
		else if( command == "rotateY" ) {
			const float a = Number( numbers.at( 0 ) );                                       // TODO support both rad, deg and turn
			mTransform = glm::scale( mTransform, vec2( glm::cos( glm::radians( a ) ), 1 ) ); // TODO proper 3D rotation
		}
		else if( command == "skew" ) {
			const float ax = Number( numbers.at( 0 ) ); // TODO support both rad, deg and turn
			const float ay = numbers.size() > 1 ? Number( numbers.at( 1 ) ) : ax;
			mTransform = shearY( mTransform,
				glm::tan( glm::radians( ax ) ) ); // Note: glm uses shear instead of skew, which must be applied to the other axis.
			mTransform = shearX( mTransform, glm::tan( glm::radians( ay ) ) );
		}
		else if( command == "skewX" ) {
			const float a = Number( numbers.at( 0 ) ); // TODO support both rad, deg and turn

			mTransform = shearY( mTransform,
				glm::tan( glm::radians( a ) ) ); // Note: glm uses shear instead of skew, which must be applied to the other axis.
		}
		else if( command == "skewY" ) {
			const float a = Number( numbers.at( 0 ) ); // TODO support both rad, deg and turn

			mTransform = shearX( mTransform,
				glm::tan( glm::radians( a ) ) ); // Note: glm uses shear instead of skew, which must be applied to the other axis.
		}
	}
}

std::string Transform::toString() const
{
	std::string result;

	if( mTransform == mat3{} )
		return result;

	result += "matrix(";
	result += trimNumber( std::to_string( mTransform[0][0] ) );
	result += ",";
	result += trimNumber( std::to_string( mTransform[1][0] ) );
	result += ",";
	result += trimNumber( std::to_string( mTransform[0][1] ) );
	result += ",";
	result += trimNumber( std::to_string( mTransform[1][1] ) );
	result += ",";
	result += trimNumber( std::to_string( mTransform[2][0] ) );
	result += ",";
	result += trimNumber( std::to_string( mTransform[2][1] ) );
	result += ")";

	return result;
}

} // namespace nvp
} // namespace cinder