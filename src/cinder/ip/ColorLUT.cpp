/*
 Copyright (c) 2015, The Cinder Project, All rights reserved.
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

#include "cinder/CinderAssert.h"
#include "cinder/ip/ColorLUT.h"

namespace cinder { namespace ip { 

Surface8u colorLUT( const uint8_t bitsPerChannel )
{
	// Sanity check.
	CI_ASSERT( bitsPerChannel > 2 && bitsPerChannel < 9 );

	const uint8_t kBitShift = 8 - bitsPerChannel;     // Skips least significant bits.
	const uint8_t kBitOffset = ~( ~0 << kBitShift );  // Sets least significant bits to 1.
	const uint16_t kSize = 1 << bitsPerChannel;       // Number of stops per channel.

	uint32_t width = kSize * kSize;
	uint32_t height = kSize;

	if( ! ( bitsPerChannel % 2 ) ) {
		width = height = ( 1 << ( bitsPerChannel * 3 / 2 ) );
	}

	Surface8u result = Surface8u( width, height, false, SurfaceChannelOrder::RGB );
	auto data = result.getData();

	for( size_t b = 0; b < kSize; ++b ) {
		for( size_t g = 0; g < kSize; ++g ) {
			for( size_t r = 0; r < kSize; ++r ) {
				*data++ = ( r << kBitShift ) | kBitOffset;
				*data++ = ( g << kBitShift ) | kBitOffset;
				*data++ = ( b << kBitShift ) | kBitOffset;
			}
		}
	}

	return result;
}

} } // namespace cinder::ip