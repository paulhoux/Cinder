/*
 Copyright (c) 2010-2020, The Cinder Project: http://libcinder.org
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

#pragma once

#include "AxisAlignedBox.h"
#include "cinder/Matrix.h"
#include "cinder/Ray.h"
#include "cinder/Sphere.h"
#include "cinder/Vector.h"

namespace cinder {

class CI_API OrientedBox {
public:
	OrientedBox() : mCenter( 0 ), mExtentsX( 0 ), mExtentsY( 0 ), mExtentsZ( 0 ), mSize( 0 ) {}
	OrientedBox( const vec3 &min, const vec3 &max ) { set( min, max ); }
	OrientedBox( const mat4 &m ) : mCenter( m[3] ), mExtentsX( m[0] ), mExtentsY( m[1] ), mExtentsZ( m[2] )
	{
		mSize = 2.0f * vec3( glm::length( mExtentsX ), glm::length( mExtentsY ), glm::length( mExtentsZ ) );
	}
	explicit OrientedBox( const AxisAlignedBox &aabb )
	{
		mCenter = aabb.getCenter();
		mExtentsX = vec3( aabb.getExtents().x, 0, 0 );
		mExtentsY = vec3( 0, aabb.getExtents().y, 0 );
		mExtentsZ = vec3( 0, 0, aabb.getExtents().z );
		mSize = 2.0f * vec3( glm::length( mExtentsX ), glm::length( mExtentsY ), glm::length( mExtentsZ ) );
	}

	//! Returns the center of the axis-aligned box.
	const vec3& getCenter() const { return mCenter; }

	//! Returns the X-axis extents of the box.
	const vec3& getExtentsX() const { return mExtentsX; }
	//! Returns the Y-axis extents of the box.
	const vec3& getExtentsY() const { return mExtentsY; }
	//! Returns the Z-axis extents of the box.
	const vec3& getExtentsZ() const { return mExtentsZ; }

	//! Returns the size of the box.
	const vec3& getSize() const { return mSize; }

	//! Returns the corner of the oriented box with the smallest x, y and z coordinates.
	vec3 getMin() const { return mCenter - mExtentsX - mExtentsY - mExtentsZ; }

	//! Returns the corner of the oriented box with the largest x, y and z coordinates.
	vec3 getMax() const { return mCenter + mExtentsX + mExtentsY + mExtentsZ; }

	//! Construct an oriented box by specifying two opposite corners.
	void set( const vec3 &min, const vec3 &max )
	{
		const vec3 e = 0.5f * ( max - min );
		mCenter = 0.5f * ( min + max );
		mExtentsX = vec3( glm::max( e.x, FLT_MIN ), 0, 0 );
		mExtentsY = vec3( 0, glm::max( e.y, FLT_MIN ), 0 );
		mExtentsZ = vec3( 0, 0, glm::max( e.z, FLT_MIN ) );
		mSize = 2.0f * vec3( glm::length( mExtentsX ), glm::length( mExtentsY ), glm::length( mExtentsZ ) );
	}
	//! Construct an oriented box from the given matrix.
	void set( const mat4 &m )
	{
		mCenter = vec3( m[3] );
		mExtentsX = vec3( m[0] );
		mExtentsY = vec3( m[1] );
		mExtentsZ = vec3( m[2] );
		mSize = 2.0f * vec3( glm::length( mExtentsX ), glm::length( mExtentsY ), glm::length( mExtentsZ ) );
	}

	//! Expands the box so that it contains \a point.
	void include( const vec3 &point )
	{
		vec3 min = glm::min( getMin(), point );
		vec3 max = glm::max( getMax(), point );
		set( min, max );
	}

	//! Expands the box so that it contains \a box.
	void include( const AxisAlignedBox &box )
	{
		vec3 min = glm::min( getMin(), box.getMin() );
		vec3 max = glm::max( getMax(), box.getMax() );
		set( min, max );
	}

	//! Expands the box so that it contains \a box.
	void include( const OrientedBox &box )
	{
		vec3 min = glm::min( getMin(), box.getMin() );
		vec3 max = glm::max( getMax(), box.getMax() );
		set( min, max );
	}

	//! Expands the box so that it contains \a sphere.
	void include( const Sphere &sphere )
	{
		const vec3 extents = sphere.getRadius() * glm::normalize( glm::abs( mExtentsX + mExtentsY + mExtentsZ ) );
		const vec3 min = glm::min( getMin(), sphere.getCenter() - extents );
		const vec3 max = glm::max( getMax(), sphere.getCenter() + extents );
		set( min, max );
	}

	//! Returns the minimum squared distance from the box to the \a sphere.
	float distance2( const ci::Sphere &sphere ) const
	{
		float dmin = 0;

		auto center = sphere.getCenter();
		auto bmin = getMin();
		auto bmax = getMax();

		if( center.x < bmin.x ) {
			float d = center.x - bmin.x;
			dmin += d * d;
		}
		else if( center.x > bmax.x ) {
			float d = center.x - bmax.x;
			dmin += d * d;
		}

		if( center.y < bmin.y ) {
			float d = center.y - bmin.y;
			dmin += d * d;
		}
		else if( center.y > bmax.y ) {
			float d = center.y - bmax.y;
			dmin += d * d;
		}

		if( center.z < bmin.z ) {
			float d = center.z - bmin.z;
			dmin += d * d;
		}
		else if( center.z > bmax.z ) {
			float d = center.z - bmax.z;
			dmin += d * d;
		}

		return glm::max( dmin - sphere.getRadius() * sphere.getRadius(), 0.0f );
	}
	
	//! Returns the minimum distance from the box to the \a sphere.
	float distance( const ci::Sphere &sphere ) const { return glm::sqrt( distance2( sphere ) ); }

	//! Returns \c true if the axis-aligned box contains \a point.
	bool contains( const vec3 &point ) const
	{
		const vec3 pos = point - mCenter;

		vec3 dst;
		dst.x = glm::abs( glm::dot( pos, mExtentsX ) );
		dst.y = glm::abs( glm::dot( pos, mExtentsY ) );
		dst.z = glm::abs( glm::dot( pos, mExtentsZ ) );

		return glm::all( glm::lessThanEqual( dst, vec3( 1 ) ) );
	}

	//! Returns \c true if the boxes intersect.
	bool intersects( const AxisAlignedBox &box ) const
	{
		return intersects( OrientedBox( box ) );
	}

	//! Returns \c true if the boxes intersect.
	bool intersects( const OrientedBox &box ) const
	{
		const auto xSq = glm::length2( box.getExtentsX() );
		const auto ySq = glm::length2( box.getExtentsY() );
		const auto zSq = glm::length2( box.getExtentsZ() );

		const auto pos = mCenter - box.getCenter();
		const auto x = glm::clamp( glm::dot( pos, box.getExtentsX() ), -xSq, xSq );
		const auto y = glm::clamp( glm::dot( pos, box.getExtentsY() ), -ySq, ySq );
		const auto z = glm::clamp( glm::dot( pos, box.getExtentsZ() ), -zSq, zSq );

		const auto closest = ( x / glm::max( xSq, FLT_MIN ) ) * box.getExtentsX() + ( y / glm::max( ySq, FLT_MIN ) ) * box.getExtentsY() + ( z / glm::max( zSq, FLT_MIN ) ) * box.getExtentsZ();

		const auto distanceSq = glm::length2( pos - closest );
		const auto aRadiusSq = glm::length2( mExtentsX + mExtentsY + mExtentsZ );
		return distanceSq < aRadiusSq;
	}

	//! Returns \c true if the axis-aligned box intersects \a sphere.
	bool intersects( const ci::Sphere &sphere ) const
	{
		return distance2( sphere ) < FLT_EPSILON;
	}

	//! Returns \c true if the ray intersects the axis-aligned box.
	bool intersects( const Ray &ray ) const
	{
		if( glm::length2( ray.getDirection() ) < FLT_EPSILON )
			return false;

		vec3 min = ( getMin() - ray.getOrigin() ) / ray.getDirection();
		vec3 max = ( getMax() - ray.getOrigin() ) / ray.getDirection();

		float fmin = glm::max( glm::max( glm::min( min.x, max.x ), glm::min( min.y, max.y ) ), glm::min( min.z, max.z ) );
		float fmax = glm::min( glm::min( glm::max( min.x, max.x ), glm::max( min.y, max.y ) ), glm::max( min.z, max.z ) );

		return ( fmax >= fmin );
	}

	//! Performs ray intersections and returns the number of intersections (0, 1 or 2). Returns \a min and \a max distance from the ray origin.
	int intersect( const Ray &ray, float *min, float *max ) const
	{
		if( glm::length2( ray.getDirection() ) < FLT_EPSILON )
			return 0;

		vec3 _min = ( getMin() - ray.getOrigin() ) / ray.getDirection();
		vec3 _max = ( getMax() - ray.getOrigin() ) / ray.getDirection();

		float fmin = glm::max( glm::max( glm::min( _min.x, _max.x ), glm::min( _min.y, _max.y ) ), glm::min( _min.z, _max.z ) );
		float fmax = glm::min( glm::min( glm::max( _min.x, _max.x ), glm::max( _min.y, _max.y ) ), glm::max( _min.z, _max.z ) );

		if( fmax >= fmin ) {
			*min = fmin;
			*max = fmax;

			if( fmax > fmin )
				return 2;
			else
				return 1;
		}

		return 0;
	}

	//! Given a plane through the origin with \a normal, returns the minimum and maximum distance to the axis-aligned box.
	void project( const vec3 &normal, float *min, float *max ) const
	{
		const float c = glm::dot( mCenter, normal );
		const float x = glm::abs( glm::dot( mExtentsX, normal ) ) + glm::abs( glm::dot( mExtentsY, normal ) ) + glm::abs( glm::dot( mExtentsZ, normal ) );
		*min = c - x;
		*max = c + x;
	}

	//! Given a plane through the origin with \a normal, returns the corner closest to the plane.
	vec3 getNegative( const vec3 &normal ) const
	{
		vec3 result = getMin();
		vec3 size = getSize();

		if( normal.x < 0 )
			result.x += size.x;

		if( normal.y < 0 )
			result.y += size.y;

		if( normal.z < 0 )
			result.z += size.z;

		return result;
	}

	//! Given a plane through the origin with \a normal, returns the corner farthest from the plane.
	vec3 getPositive( const vec3 &normal ) const
	{
		vec3 result = getMin();
		vec3 size = getSize();

		if( normal.x > 0 )
			result.x += size.x;

		if( normal.y > 0 )
			result.y += size.y;

		if( normal.z > 0 )
			result.z += size.z;

		return result;
	}

	//! Converts oriented box to another coordinate space.
	void transform( const mat4 &transform )
	{
		mCenter = vec3( transform * vec4( mCenter, 1 ) );
		const mat3 m{ transform };
		mExtentsX = m * mExtentsX;
		mExtentsY = m * mExtentsY;
		mExtentsZ = m * mExtentsZ;
		mSize = 2.0f * vec3( glm::length( mExtentsX ), glm::length( mExtentsY ), glm::length( mExtentsZ ) );
	}

	//! Converts oriented box to another coordinate space.
	OrientedBox transformed( const mat4 &transform ) const
	{
		OrientedBox result;
		result.mCenter = vec3( transform * vec4( mCenter, 1 ) );
		const mat3 m{ transform };
		result.mExtentsX = m * mExtentsX;
		result.mExtentsY = m * mExtentsY;
		result.mExtentsZ = m * mExtentsZ;
		result.mSize = 2.0f * vec3( glm::length( result.mExtentsX ), glm::length( result.mExtentsY ), glm::length( result.mExtentsZ ) );
		return result;
	}

protected:
	vec3  mCenter;
	vec3  mExtentsX;
	vec3  mExtentsY;
	vec3  mExtentsZ;
	vec3  mSize;
};

} // namespace cinder
