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

#include "cinder/gl/Sketch.h"

namespace cinder {
namespace gl {

void Sketch::frustum( const Camera& cam )
{
	vec3 nearTopLeft, nearTopRight, nearBottomLeft, nearBottomRight;
	cam.getNearClipCoordinates( &nearTopLeft, &nearTopRight, &nearBottomLeft, &nearBottomRight );

	vec3 farTopLeft, farTopRight, farBottomLeft, farBottomRight;
	cam.getFarClipCoordinates( &farTopLeft, &farTopRight, &farBottomLeft, &farBottomRight );

	// extract camera position from view matrix, so that it will work with CameraStereo as well
	//  see: http://www.gamedev.net/topic/397751-how-to-get-camera-position/page__p__3638207#entry3638207
	mat4 view = cam.getViewMatrix();
	vec3 eye;
	eye.x = -( view[0][0] * view[3][0] + view[0][1] * view[3][1] + view[0][2] * view[3][2] );
	eye.y = -( view[1][0] * view[3][0] + view[1][1] * view[3][1] + view[1][2] * view[3][2] );
	eye.z = -( view[2][0] * view[3][0] + view[2][1] * view[3][1] + view[2][2] * view[3][2] );

	implDrawLine( farTopLeft, nearTopLeft );
	implDrawLine( farTopRight, nearTopRight );
	implDrawLine( farBottomRight, nearBottomRight );
	implDrawLine( farBottomLeft, nearBottomLeft );

	implDrawQuad( nearTopLeft, nearTopRight, nearBottomRight, nearBottomLeft );
	implDrawQuad( farTopLeft, farTopRight, farBottomRight, farBottomLeft );

	gl::ScopedColor color( 0.25f * gl::context()->getCurrentColor() );
	implDrawLine( eye, nearTopLeft );
	implDrawLine( eye, nearTopRight );
	implDrawLine( eye, nearBottomRight );
	implDrawLine( eye, nearBottomLeft );
}

}
} // namespace cinder::gl