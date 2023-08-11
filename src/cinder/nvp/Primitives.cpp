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

#include "cinder/nvp/Primitives.h"

namespace cinder {
namespace nvp {

void Shape::compile() const
{
	GLsizei numCommands;
	gl::getPathParameterivNV( mPathId, GL_PATH_COMMAND_COUNT_NV, &numCommands );
	GLsizei numCoords;
	gl::getPathParameterivNV( mPathId, GL_PATH_COORD_COUNT_NV, &numCoords );

	if( numCommands != static_cast<GLsizei>( mCommands.size() ) || numCoords != static_cast<GLsizei>( mCoords.size() ) )
		gl::pathCommandsNV( mPathId, static_cast<GLsizei>( mCommands.size() ), mCommands.data(), static_cast<GLsizei>( mCoords.size() ), GL_FLOAT, mCoords.data() );
}

void Shape::clear()
{
	mCommands.clear();
	mCoords.clear();
}

void Shape::moveTo( float x, float y )
{
	mCommands.emplace_back( GL_MOVE_TO_NV );
	mCoords.emplace_back( x );
	mCoords.emplace_back( y );
}

void Shape::relativeMoveTo( float x, float y )
{
	mCommands.emplace_back( GL_RELATIVE_MOVE_TO_NV );
	mCoords.emplace_back( x );
	mCoords.emplace_back( y );
}

void Shape::lineTo( float x, float y )
{
	mCommands.emplace_back( GL_LINE_TO_NV );
	mCoords.emplace_back( x );
	mCoords.emplace_back( y );
}

void Shape::relativeLineTo( float x, float y )
{
	mCommands.emplace_back( GL_RELATIVE_LINE_TO_NV );
	mCoords.emplace_back( x );
	mCoords.emplace_back( y );
}

void Shape::horizontalLineTo( float x )
{
	mCommands.emplace_back( GL_HORIZONTAL_LINE_TO_NV );
	mCoords.emplace_back( x );
}

void Shape::relativeHorizontalLineTo( float x )
{
	mCommands.emplace_back( GL_RELATIVE_HORIZONTAL_LINE_TO_NV );
	mCoords.emplace_back( x );
}

void Shape::verticalLineTo( float y )
{
	mCommands.emplace_back( GL_VERTICAL_LINE_TO_NV );
	mCoords.emplace_back( y );
}

void Shape::relativeVerticalLineTo( float y )
{
	mCommands.emplace_back( GL_RELATIVE_VERTICAL_LINE_TO_NV );
	mCoords.emplace_back( y );
}

void Shape::quadTo( float x1, float y1, float x2, float y2 )
{
	mCommands.emplace_back( GL_QUADRATIC_CURVE_TO_NV );
	mCoords.emplace_back( x1 );
	mCoords.emplace_back( y1 );
	mCoords.emplace_back( x2 );
	mCoords.emplace_back( y2 );
}

void Shape::relativeQuadTo( float x1, float y1, float x2, float y2 )
{
	mCommands.emplace_back( GL_RELATIVE_QUADRATIC_CURVE_TO_NV );
	mCoords.emplace_back( x1 );
	mCoords.emplace_back( y1 );
	mCoords.emplace_back( x2 );
	mCoords.emplace_back( y2 );
}

void Shape::smoothQuadTo( float x2, float y2 )
{
	mCommands.emplace_back( GL_SMOOTH_QUADRATIC_CURVE_TO_NV );
	mCoords.emplace_back( x2 );
	mCoords.emplace_back( y2 );
}

void Shape::relativeSmoothQuadTo( float x2, float y2 )
{
	mCommands.emplace_back( GL_RELATIVE_SMOOTH_QUADRATIC_CURVE_TO_NV );
	mCoords.emplace_back( x2 );
	mCoords.emplace_back( y2 );
}

void Shape::cubicTo( float x1, float y1, float x2, float y2, float x3, float y3 )
{
	mCommands.emplace_back( GL_CUBIC_CURVE_TO_NV );
	mCoords.emplace_back( x1 );
	mCoords.emplace_back( y1 );
	mCoords.emplace_back( x2 );
	mCoords.emplace_back( y2 );
	mCoords.emplace_back( x3 );
	mCoords.emplace_back( y3 );
}

void Shape::relativeCubicTo( float x1, float y1, float x2, float y2, float x3, float y3 )
{
	mCommands.emplace_back( GL_RELATIVE_CUBIC_CURVE_TO_NV );
	mCoords.emplace_back( x1 );
	mCoords.emplace_back( y1 );
	mCoords.emplace_back( x2 );
	mCoords.emplace_back( y2 );
	mCoords.emplace_back( x3 );
	mCoords.emplace_back( y3 );
}

void Shape::smoothCubicTo( float x2, float y2, float x3, float y3 )
{
	mCommands.emplace_back( GL_SMOOTH_CUBIC_CURVE_TO_NV );
	mCoords.emplace_back( x2 );
	mCoords.emplace_back( y2 );
	mCoords.emplace_back( x3 );
	mCoords.emplace_back( y3 );
}

void Shape::relativeSmoothCubicTo( float x2, float y2, float x3, float y3 )
{
	mCommands.emplace_back( GL_RELATIVE_SMOOTH_CUBIC_CURVE_TO_NV );
	mCoords.emplace_back( x2 );
	mCoords.emplace_back( y2 );
	mCoords.emplace_back( x3 );
	mCoords.emplace_back( y3 );
}

void Shape::arcTo( float rx, float ry, float phi, bool largeArcFlag, bool sweepFlag, float px, float py )
{
	mCommands.emplace_back( GL_ARC_TO_NV );
	mCoords.emplace_back( rx );
	mCoords.emplace_back( ry );
	mCoords.emplace_back( phi );
	mCoords.emplace_back( largeArcFlag );
	mCoords.emplace_back( sweepFlag );
	mCoords.emplace_back( px );
	mCoords.emplace_back( py );
}

void Shape::relativeArcTo( float rx, float ry, float phi, bool largeArcFlag, bool sweepFlag, float px, float py )
{
	mCommands.emplace_back( GL_RELATIVE_ARC_TO_NV );
	mCoords.emplace_back( rx );
	mCoords.emplace_back( ry );
	mCoords.emplace_back( phi );
	mCoords.emplace_back( largeArcFlag );
	mCoords.emplace_back( sweepFlag );
	mCoords.emplace_back( px );
	mCoords.emplace_back( py );
}

void Shape::close()
{
	mCommands.emplace_back( GL_CLOSE_PATH_NV );
}

void Arc::create() const
{
	bool isSweep = mEnd > mStart;
	bool isLarge = glm::fract( ( mEnd - mStart ) / 360.0f ) > 0.5f;

	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	commands.push_back( GL_MOVE_TO_NV );
	coords.push_back( mCenter.x + mRadius * glm::sin( mStart ) );
	coords.push_back( mCenter.y - mRadius * glm::cos( mStart ) );
	commands.push_back( GL_ARC_TO_NV );
	coords.push_back( mRadius );
	coords.push_back( mRadius );
	coords.push_back( mOffset );
	coords.push_back( isLarge );
	coords.push_back( isSweep );
	coords.push_back( mCenter.x + mRadius * glm::sin( mEnd ) );
	coords.push_back( mCenter.y - mRadius * glm::cos( mEnd ) );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Circle::create() const
{
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	commands.push_back( GL_MOVE_TO_NV );
	coords.push_back( mCenter.x + mRadius );
	coords.push_back( mCenter.y );
	commands.push_back( GL_RELATIVE_ARC_TO_NV );
	coords.push_back( mRadius );
	coords.push_back( mRadius );
	coords.push_back( 0 );
	coords.push_back( false );
	coords.push_back( true );
	coords.push_back( -( mRadius + mRadius ) );
	coords.push_back( 0 );
	commands.push_back( GL_RELATIVE_ARC_TO_NV );
	coords.push_back( mRadius );
	coords.push_back( mRadius );
	coords.push_back( 0 );
	coords.push_back( false );
	coords.push_back( true );
	coords.push_back( mRadius + mRadius );
	coords.push_back( 0 );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Ellipse::create() const
{
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	commands.push_back( GL_MOVE_TO_NV );
	coords.push_back( mCenter.x + mRadiusX );
	coords.push_back( mCenter.y );
	commands.push_back( GL_RELATIVE_ARC_TO_NV );
	coords.push_back( mRadiusX );
	coords.push_back( mRadiusY );
	coords.push_back( 0 );
	coords.push_back( false );
	coords.push_back( true );
	coords.push_back( -( mRadiusX + mRadiusX ) );
	coords.push_back( 0 );
	commands.push_back( GL_RELATIVE_ARC_TO_NV );
	coords.push_back( mRadiusX );
	coords.push_back( mRadiusY );
	coords.push_back( 0 );
	coords.push_back( false );
	coords.push_back( true );
	coords.push_back( mRadiusX + mRadiusX );
	coords.push_back( 0 );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Line::create() const
{
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	commands.push_back( GL_MOVE_TO_NV );
	coords.push_back( mP0.x );
	coords.push_back( mP0.y );
	commands.push_back( GL_LINE_TO_NV );
	coords.push_back( mP1.x );
	coords.push_back( mP1.y );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Polygon::create( bool closed ) const
{
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	commands.push_back( GL_MOVE_TO_NV );
	for( const auto &point : mPoints ) {
		coords.push_back( point.x );
		coords.push_back( point.y );
		commands.push_back( GL_LINE_TO_NV );
	}

	if( closed )
		commands.back() = GL_CLOSE_PATH_NV;
	else
		commands.pop_back();

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Rectangle::create() const
{
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	commands.push_back( GL_RECT_NV );
	coords.push_back( mBounds.x1 );
	coords.push_back( mBounds.y1 );
	coords.push_back( mBounds.x2 - mBounds.x1 );
	coords.push_back( mBounds.y2 - mBounds.y1 );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void RoundedRectangle::create() const
{
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	commands.push_back( approxEqual( mCornerRadiusX, mCornerRadiusY ) ? GL_ROUNDED_RECT_NV : GL_ROUNDED_RECT2_NV );
	coords.push_back( mBounds.x1 );
	coords.push_back( mBounds.y1 );
	coords.push_back( mBounds.getWidth() );
	coords.push_back( mBounds.getHeight() );
	coords.push_back( mCornerRadiusX );
	if( !approxEqual( mCornerRadiusX, mCornerRadiusY ) )
		coords.push_back( mCornerRadiusY );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Star::create() const
{
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	const float step = glm::radians( 180.0f / float( mPoints ) );

	for( int i = 0; i < 2 * mPoints; i += 2 ) {
		commands.push_back( i == 0 ? GL_MOVE_TO_NV : GL_LINE_TO_NV );
		coords.push_back( mCenter.x + mRadiusLarge * glm::sin( mRotation + float( i + 0 ) * step ) );
		coords.push_back( mCenter.y - mRadiusLarge * glm::cos( mRotation + float( i + 0 ) * step ) );
		commands.push_back( GL_LINE_TO_NV );
		coords.push_back( mCenter.x + mRadiusSmall * glm::sin( mRotation + float( i + 1 ) * step ) );
		coords.push_back( mCenter.y - mRadiusSmall * glm::cos( mRotation + float( i + 1 ) * step ) );
	}
	commands.push_back( GL_CLOSE_PATH_NV );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Arrow::create() const
{
	const float length = glm::distance( mP1, mP0 );
	const vec2  direction = ( mP1 - mP0 ) / length;
	const vec2  normal{ 0.5f * mThickness * direction.y, -0.5f * mThickness * direction.x };

	vec2 base = mP0 + direction * glm::max( 0.0f, length - mThickness * mLength );

	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	commands.push_back( GL_MOVE_TO_NV );
	coords.push_back( mP0.x - normal.x );
	coords.push_back( mP0.y - normal.y );
	commands.push_back( GL_LINE_TO_NV );
	coords.push_back( base.x - normal.x + direction.x * mThickness * mLength * mConcavity );
	coords.push_back( base.y - normal.y + direction.y * mThickness * mLength * mConcavity );
	commands.push_back( GL_LINE_TO_NV );
	coords.push_back( base.x - normal.x * mWidth );
	coords.push_back( base.y - normal.y * mWidth );
	commands.push_back( GL_LINE_TO_NV );
	coords.push_back( mP1.x );
	coords.push_back( mP1.y );
	commands.push_back( GL_LINE_TO_NV );
	coords.push_back( base.x + normal.x * mWidth );
	coords.push_back( base.y + normal.y * mWidth );
	commands.push_back( GL_LINE_TO_NV );
	coords.push_back( base.x + normal.x + direction.x * mThickness * mLength * mConcavity );
	coords.push_back( base.y + normal.y + direction.y * mThickness * mLength * mConcavity );
	commands.push_back( GL_LINE_TO_NV );
	coords.push_back( mP0.x + normal.x );
	coords.push_back( mP0.y + normal.y );
	commands.push_back( GL_CLOSE_PATH_NV );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Spiral::create() const
{
	const auto spacing = mSpacing / ( 2.0f * glm::pi<float>() );
	const auto radiansStart = getInnerRadians();
	const auto radiansEnd = getOuterRadians();

	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	Point p0( radiansStart, mOffset - radiansStart );

	commands.push_back( GL_MOVE_TO_NV );
	coords.push_back( mCenter.x + p0.x * spacing );
	coords.push_back( mCenter.y + p0.y * spacing );

	float radians = radiansStart + glm::radians( clamp( radiansStart * spacing, 3.0f, 90.0f ) ); // Adaptive step size.
	while( radians < radiansEnd ) {
		const auto p3 = Point( radians, mOffset - radiansStart );
		const auto controls = p3.generate( p0 );

		commands.push_back( GL_CUBIC_CURVE_TO_NV );
		coords.push_back( mCenter.x + controls.first.x * spacing );
		coords.push_back( mCenter.y + controls.first.y * spacing );
		coords.push_back( mCenter.x + controls.second.x * spacing );
		coords.push_back( mCenter.y + controls.second.y * spacing );
		coords.push_back( mCenter.x + p3.x * spacing );
		coords.push_back( mCenter.y + p3.y * spacing );

		p0 = p3;

		radians += glm::radians( clamp( radians * spacing, 3.0f, 90.0f ) ); // Adaptive step size.
	}

	const auto p3 = Point( radiansEnd, mOffset - radiansStart );
	const auto controls = p3.generate( p0 );

	commands.push_back( GL_CUBIC_CURVE_TO_NV );
	coords.push_back( mCenter.x + controls.first.x * spacing );
	coords.push_back( mCenter.y + controls.first.y * spacing );
	coords.push_back( mCenter.x + controls.second.x * spacing );
	coords.push_back( mCenter.y + controls.second.y * spacing );
	coords.push_back( mCenter.x + p3.x * spacing );
	coords.push_back( mCenter.y + p3.y * spacing );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

Spiral::Point::Point( float theta, float offset )
	: theta( theta )
{
	float c = glm::cos( theta + offset );
	float s = glm::sin( theta + offset );
	x = theta * c;
	y = theta * s;
	tangent = glm::atan( s + x, c - y );
}

std::pair<vec2, vec2> Spiral::Point::generate( const Point &previous ) const
{
	const auto offset = 4 * glm::tan( ( theta - previous.theta ) / 4 ) / 3;
	const auto p1 = vec2( glm::cos( previous.tangent ) * offset * previous.theta + previous.x, glm::sin( previous.tangent ) * offset * previous.theta + previous.y );
	const auto p2 = vec2( glm::cos( tangent - glm::pi<float>() ) * offset * theta + x, glm::sin( tangent - glm::pi<float>() ) * offset * theta + y );
	return std::make_pair( p1, p2 );
}

} // namespace nvp
} // namespace cinder
