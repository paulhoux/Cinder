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

#include "cinder/gl/gl.h"
#include "cinder/gl/Batch.h"
#include "cinder/gl/Context.h"
#include "cinder/AxisAlignedBox.h"
#include "cinder/Camera.h"
#include "cinder/Quaternion.h"
#include "cinder/Utilities.h"

namespace cinder {
namespace gl {

typedef std::shared_ptr<class Sketch> SketchRef;

class Sketch {
public:
	//! By default, Sketch will respect the current transform matrices, allowing you to mix 2D and 3D rendering and it 'just works'.
	//  All transformations are done on the CPU, which can be slow for large numbers of objects. By setting \a autoTransform to FALSE, 
	//  Sketch will not perform any transformations on the CPU. It's faster, but you yourself are responsible for properly setting up
	//  the model, view and projection matrices before you draw the sketch.
	Sketch( bool autoTransform = true )
		: mAutoTransform( autoTransform ), mOwnsBuffers( true ) {};
	~Sketch() {};

	static SketchRef create( bool autoTransform = true ) { return std::make_shared<Sketch>( autoTransform ); }

	void clear()
	{
		mVertices.clear();
		mColors.clear();
		mVbo.reset();
		mVao.reset();
	}

	void draw()
	{
		gl::pushMatrices();

		if( mAutoTransform ) {
			gl::setModelMatrix( mat4() );
			gl::setViewMatrix( mat4() );
			gl::setProjectionMatrix( mat4() );
		}

		// this pushes the VAO, which needs to be popped
		setupBuffers();
		ScopedVao vao( mVao );

		auto ctx = context();
		ctx->setDefaultShaderVars();
		ctx->drawArrays( GL_LINES, 0, mVertices.size() );

		// this was set by setupBuffers
		ctx->popVao();

		//
		gl::popMatrices();
	}

	//! Draws a vector from \a  a to \a b. 
	//! Optionally, you can specify the length of the arrow as a \a percentage of the vector's length.
	void vector( const vec3& a, const vec3& b, float percentage = 0.05f )
	{
		const float length = glm::length( b - a );
		implDrawLine( a, b );
		implDrawCylinder( b - percentage * ( b - a ), b, percentage * length / 4, 0 );
	}

	//! Draws the x-axis in red, the y-axis in green and the z-axis in blue with the specified \a size.
	//! Optionally, you can specify the length of the arrow as a \a percentage of the size.
	void coordinateFrame( size_t size, float percentage = 0.05f )
	{
		gl::ScopedColor color( 1, 0, 0 ); vector( vec3( 0 ), vec3( size, 0, 0 ), percentage );
		gl::color( 0, 1, 0 ); vector( vec3( 0 ), vec3( 0, size, 0 ), percentage );
		gl::color( 0, 0, 1 ); vector( vec3( 0 ), vec3( 0, 0, size ), percentage );
	}

	//! Draws a plane grid with sides of length \a size and a line at \a step intervals.
	void grid( size_t size, size_t step = 0, const vec3& normal = vec3( 0, 1, 0 ) )
	{ implDrawGrid( vec3( 0, 0, 0 ), normal, size, step ); }
	//! Draws a plane grid at \a center with sides of length \a size and a line at \a step intervals.
	void grid( const vec3& center, size_t size, size_t step = 0, const vec3& normal = vec3( 0, 1, 0 ) )
	{ implDrawGrid( center, normal, size, step ); }

	//! Draws a cylinder from \a base to \a top with the specified \a radius.
	void cylinder( const vec3& base, const vec3& top, float radius )
	{ implDrawCylinder( base, top, radius, radius ); }
	//! Draws a cylinder from \a base to \a top with a different \a baseRadius and \a topRadius.
	void cylinder( const vec3& base, const vec3& top, float baseRadius, float topRadius )
	{ implDrawCylinder( base, top, baseRadius, topRadius ); }

	//! Draws a cone from \a apex (top) to \a base (bottom). The \a ratio is the diameter of the base divided by the height.
	void cone( const vec3& apex, const vec3& base, float ratio )
	{ implDrawCylinder( base, apex, ratio * distance( apex, base ), 0.0f ); }

	//! Draws a capsule from \a base to \a top with the specified \a radius.
	void capsule( const vec3& base, const vec3& top, float radius )
	{ implDrawCapsule( base, top, radius ); }

	//! Draws a sphere of the specified \a radius at \a center.
	void sphere( const vec3& center, float radius )
	{ implDrawSphere( center, radius ); }

	//! Draws a hemisphere of the specified \a radius at \a center.
	void hemisphere( const vec3& center, float radius )
	{ implDrawHemisphere( center, radius, vec3( 0, 1, 0 ) ); implDrawCircle( center, radius, vec3( 0, 1, 0 ) ); }
	//! Draws a hemisphere of the specified \a radius at \a center, around \a axis.
	void hemisphere( const vec3& center, float radius, const vec3& axis )
	{ implDrawHemisphere( center, radius, axis ); implDrawCircle( center, radius, axis ); }

	//! Draws a cube of the specified \a size at \a center.
	void cube( const vec3& center, const vec3& size )
	{ implDrawCube( center, size ); }
	//! Draws the axis aligned bounding \a box as a cube.
	void cube( const AxisAlignedBox3f& box )
	{ implDrawCube( box.getCenter(), box.getSize() ); }

	//! Draws the viewing frustum of a \a camera.
	void frustum( const Camera& camera );

	//! Draws a line between \a a and \a b.
	void line( const vec2& a, const vec2& b )
	{ implDrawLine( vec3( a, 0 ), vec3( b, 0 ) ); }
	//! Draws a line between \a a and \a b.
	void line( const vec3& a, const vec3& b )
	{ implDrawLine( a, b ); }

	//! Draws a line strip by connecting all \a vertices.
	void lineStrip( const std::vector<vec2>& vertices )
	{ implDrawPoly( reinterpret_cast<const float*>( vertices.data() ), vertices.size() - 1, 2 ); }
	//! Draws a line strip by connecting all \a vertices.
	void lineStrip( const std::vector<vec3>& vertices )
	{ implDrawPoly( reinterpret_cast<const float*>( vertices.data() ), vertices.size() - 1, 3 ); }

	//! Draws a polygon by connecting all \a vertices.
	void polygon( const std::vector<vec2>& vertices )
	{ implDrawPoly( reinterpret_cast<const float*>( vertices.data() ), vertices.size(), 2 ); }
	//! Draws a polygon by connecting all \a vertices.
	void polygon( const std::vector<vec3>& vertices )
	{ implDrawPoly( reinterpret_cast<const float*>( vertices.data() ), vertices.size(), 3 ); }

	//! Draws a circle at the specified \a center with the specified \a radius in the XY-plane.
	void circle( const vec2& center, float radius )
	{ implDrawCircle( vec3( center, 0.0f ), radius, vec3( 0, 0, 1 ) ); }
	//! Draws a circle at the specified \a center with the specified \a radius around \a axis.
	void circle( const vec2& center, float radius, const vec3& axis )
	{ implDrawCircle( vec3( center, 0.0f ), radius, axis ); }
	//! Draws a circle at the specified \a center with the specified \a radius in the XY-plane.
	void circle( const vec3& center, float radius )
	{ implDrawCircle( center, radius, vec3( 0, 0, 1 ) ); }
	//! Draws a circle at the specified \a center with the specified \a radius radius around \a axis.
	void circle( const vec3& center, float radius, const vec3& axis )
	{ implDrawCircle( center, radius, axis ); }

	//! Draws a \a rectangle in the XY-plane.
	void rect( const Rectf& rectangle )
	{ implDrawRect( vec3( rectangle.getCenter(), 0.0f ), rectangle.getSize() ); }
	//! Draws a rectangle at the specified \a center with the specified \a size in the XY-plane.
	void rect( const vec2& center, const vec2& size )
	{ implDrawRect( vec3( center, 0.0f ), size ); }
	//! Draws a rectangle at the specified \a center with the specified \a size in the XY-plane.
	void rect( const vec3& center, const vec2& size )
	{ implDrawRect( center, size ); }

private:
	void implDrawGrid( const vec3& center, const vec3& normal, size_t size, size_t step )
	{
		const quat orientation( vec3( 0, 1, 0 ), normalize( normal ) );

		if( step <= 0 || step > size )
			step = size;

		const float fs = size * 0.5f;
		const float fss = ( size / step ) * 0.5f;
		for( size_t i = 0; i <= size / step; ++i ) {
			float s = ( i - fss ) * float( step );
			implDrawLine( center + orientation * vec3( s, 0.0f, -fs ), center + orientation * vec3( s, 0.0f, +fs ) );
			implDrawLine( center + orientation * vec3( -fs, 0.0f, s ), center + orientation * vec3( +fs, 0.0f, s ) );
		}
	}

	void implDrawLine( const vec3& a, const vec3& b )
	{
		const ColorA& clr = gl::context()->getCurrentColor();

		if( mAutoTransform ) {
			const mat4& transform = gl::getModelViewProjection();
			mVertices.push_back( transform * vec4( a, 1 ) );
			mVertices.push_back( transform * vec4( b, 1 ) );
		}
		else {
			mVertices.push_back( vec4( a, 1 ) );
			mVertices.push_back( vec4( b, 1 ) );
		}

		mColors.push_back( clr );
		mColors.push_back( clr );
	}

	void implDrawQuad( const vec3& a, const vec3& b, const vec3& c, const vec3& d )
	{
		implDrawLine( a, b );
		implDrawLine( b, c );
		implDrawLine( c, d );
		implDrawLine( d, a );
	}

	void implDrawPoly( const float* vertices, size_t size, size_t dimension )
	{
		assert( dimension > 1 && dimension < 4 );

		vec3 a, b;
		for( size_t i = 0; i < size; ++i ) {
			size_t j = ( i + 1 ) % size;
			a.x = vertices[i*dimension + 0];
			a.y = vertices[i*dimension + 1];
			a.z = ( dimension > 2 ) ? vertices[i*dimension + 2] : 0.0f;
			b.x = vertices[j*dimension + 0];
			b.y = vertices[j*dimension + 1];
			b.z = ( dimension > 2 ) ? vertices[j*dimension + 2] : 0.0f;
			implDrawLine( a, b );
		}
	}

	void implDrawCircle( const vec3& center, float radius, const vec3& axis )
	{
		static const int kCurves = 60;

		const float angle = toRadians( 360.0f / kCurves );
		const quat orientation( vec3( 0, 0, 1 ), normalize( axis ) );

		for( int i = 0; i < kCurves; ++i ) {
			float c1 = radius * math<float>::cos( i * angle );
			float s1 = radius * math<float>::sin( i * angle );
			float c2 = radius * math<float>::cos( ( i + 1 ) * angle );
			float s2 = radius * math<float>::sin( ( i + 1 ) * angle );
			implDrawLine( center + orientation * vec3( c1, s1, 0 ), center + orientation * vec3( c2, s2, 0 ) );
		}
	}

	void implDrawRect( const vec3& center, const vec2& size )
	{
		const vec3 half = vec3( 0.5f * size, 0.0f );
		const vec3 inv = vec3( -1.0f, 1.0f, 0.0f ) * half;

		implDrawQuad( center - half, center + inv, center + half, center - inv );
	}

	void implDrawCube( const vec3& center, const vec3& size )
	{
		vec3 a = center + 0.5f * vec3( -size.x, -size.y, -size.z );
		vec3 b = center + 0.5f * vec3( size.x, -size.y, -size.z );
		vec3 c = center + 0.5f * vec3( -size.x, size.y, -size.z );
		vec3 d = center + 0.5f * vec3( size.x, size.y, -size.z );
		vec3 e = center + 0.5f * vec3( -size.x, -size.y, size.z );
		vec3 f = center + 0.5f * vec3( size.x, -size.y, size.z );
		vec3 g = center + 0.5f * vec3( -size.x, size.y, size.z );
		vec3 h = center + 0.5f * vec3( size.x, size.y, size.z );

		implDrawLine( a, b );
		implDrawLine( c, d );
		implDrawLine( e, f );
		implDrawLine( g, h );

		implDrawLine( b, d );
		implDrawLine( f, h );
		implDrawLine( a, c );
		implDrawLine( e, g );

		implDrawLine( c, g );
		implDrawLine( d, h );
		implDrawLine( a, e );
		implDrawLine( b, f );
	}

	void implDrawCylinder( const vec3& base, const vec3& top, float radiusBase, float radiusTop )
	{
		static const int kSides = 6;

		const float angle = toRadians( 360.0f / kSides );
		const float height = distance( base, top );
		const vec3 axis = normalize( top - base );
		const quat orientation( vec3( 0, 0, 1 ), normalize( axis ) );

		if( height > 0.0f ) {
			for( int i = 0; i < kSides; ++i ) {
				float c = math<float>::cos( i * angle );
				float s = math<float>::sin( i * angle );
				vec3 p = vec3( c, s, 0 );
				implDrawLine( base + orientation * ( radiusBase * p ), top + orientation * ( radiusTop * p ) );
			}

			for( int i = 0; i <= 4; ++i )
				implDrawCircle( lerp( base, top, i * 0.25f ), lerp( radiusBase, radiusTop, i*0.25f ), axis );
		}
	}

	void implDrawHemisphere( const vec3& center, float radius, const vec3& axis )
	{
		static const int kSides = 6;
		static const int kCurves = 9;

		const quat orientation( vec3( 0, 0, 1 ), normalize( axis ) );

		const float phi = toRadians( 90.0f / kCurves );
		const float theta = toRadians( 360.0f / kSides );

		for( int i = 0; i < kSides; ++i ) {
			for( int j = 0; j < kCurves; ++j ) {
				vec3 a;
				a.x = math<float>::sin( j * phi ) * math<float>::cos( i * theta );
				a.z = math<float>::cos( j * phi );
				a.y = math<float>::sin( j * phi ) * math<float>::sin( i * theta );

				vec3 b;
				b.x = math<float>::sin( ( j + 1 ) * phi ) * math<float>::cos( i * theta );
				b.z = math<float>::cos( ( j + 1 ) * phi );
				b.y = math<float>::sin( ( j + 1 ) * phi ) * math<float>::sin( i * theta );
				implDrawLine( center + orientation * ( radius * a ), center + orientation * ( radius * b ) );
			}
		}

		// note: the base circle is omitted, since most calling functions already include it
		implDrawCircle( center + radius * math<float>::cos( kCurves * 1 / 3 * phi ) * normalize( axis ), radius * math<float>::sin( kCurves * 1 / 3 * phi ), axis );
		implDrawCircle( center + radius * math<float>::cos( kCurves * 2 / 3 * phi ) * normalize( axis ), radius * math<float>::sin( kCurves * 2 / 3 * phi ), axis );
	}

	void implDrawSphere( const vec3& center, float radius )
	{
		implDrawCircle( center, radius, vec3( 0, 1, 0 ) );
		implDrawHemisphere( center, radius, vec3( 0, 1, 0 ) );
		implDrawHemisphere( center, -radius, vec3( 0, 1, 0 ) );
	}

	void implDrawCapsule( const vec3& base, const vec3& top, float radius )
	{
		implDrawCylinder( base, top, radius, radius );

		const vec3 axis = top - base;
		implDrawHemisphere( base, -radius, axis );
		implDrawHemisphere( top, radius, axis );
	}

private:
	void					setupBuffers();

	bool					mAutoTransform;

	std::vector<vec4>		mVertices;
	std::vector<ColorAf>	mColors;

	bool					mOwnsBuffers;
	VaoRef					mVao;
	VboRef					mVbo;
};

}
} // namespace cinder::gl