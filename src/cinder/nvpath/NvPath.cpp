/*
Copyright (c) 2023, Paul Houx Creative Coding - All rights reserved.
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

#include "cinder/nvpath/NvPath.h"

#include "cinder/Log.h"
#include "cinder/Utilities.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/scoped.h"

namespace cinder {
namespace nvpath {

CapsStyle toCapsStyle( svg::LineCap lineCap )
{
	switch( lineCap ) {
	case svg::LineCap::LINE_CAP_BUTT:
		return CapsStyle::FLAT;
	case svg::LineCap::LINE_CAP_ROUND:
		return CapsStyle::ROUND;
	case svg::LineCap::LINE_CAP_SQUARE:
		return CapsStyle::SQUARE;
	}

	return CapsStyle::DEFAULT;
}

JoinStyle toJoinStyle( svg::LineJoin lineJoin )
{
	switch( lineJoin ) {
	case svg::LineJoin::LINE_JOIN_BEVEL:
		return JoinStyle::BEVEL;
	case svg::LineJoin::LINE_JOIN_MITER:
		return JoinStyle::MITER_REVERT;
	case svg::LineJoin::LINE_JOIN_ROUND:
		return JoinStyle::ROUND;
	}

	return JoinStyle::DEFAULT;
}

CoordinateSpace toGradientUnits( std::string style )
{
	style = trim( style );
	if( style == "userSpaceOnUse" )
		return CoordinateSpace::USER_SPACE_ON_USE;
	return CoordinateSpace::DEFAULT;
}

SpreadMethod toSpreadMethod( std::string style )
{
	style = trim( style );
	if( !asciiCaseCmp( style.c_str(), "reflect" ) )
		return SpreadMethod::REFLECT;
	if( !asciiCaseCmp( style.c_str(), "repeat" ) )
		return SpreadMethod::REPEAT;
	return SpreadMethod::DEFAULT;
}

Path::~Path()
{
	if( mPathId > 0 )
		gl::deletePathsNV( mPathId, 1 );
}

Path::Path( const Path &other )
{
	if( other.mPathId > 0 ) {
		mPathId = gl::genPathsNV( 1 );
		gl::copyPathNV( mPathId, other.mPathId );
	}
}

Path::Path( Path &&other ) noexcept
{
	if( mPathId > 0 )
		gl::deletePathsNV( mPathId, 1 );
	mPathId = other.mPathId;
	other.mPathId = 0;
}

Path &Path::operator=( const Path &other )
{
	if( other.mPathId > 0 && this != &other ) {
		if( mPathId == 0 )
			mPathId = gl::genPathsNV( 1 );
		gl::copyPathNV( mPathId, other.mPathId );
	}
	return *this;
}

Path &Path::operator=( Path &&other ) noexcept
{
	if( this != &other ) {
		if( mPathId > 0 )
			gl::deletePathsNV( mPathId, 1 );
		mPathId = other.mPathId;
		other.mPathId = 0;
	}
	return *this;
}

Path::Path( const Path2d &path )
{
	std::vector<GLubyte> commands;

	commands.reserve( path.getNumSegments() + 1 /* implicit MOVE_TO at start */ );
	commands.push_back( GL_MOVE_TO_NV );

	for( size_t i = 0; i < path.getNumSegments(); ++i )
		commands.push_back( toPathCommand( path.getSegmentType( i ) ) );

	const auto &points = path.getPoints();

	mPathId = gl::genPathsNV( 1 );
	gl::pathCommandsNV( mPathId, static_cast<GLsizei>( commands.size() ), commands.data(), static_cast<GLsizei>( points.size() * 2 /* each vec2 contains 2 floats */ ), GL_FLOAT, points.data() );
}

Path::Path( const Shape2d &shape )
{
	std::vector<GLubyte> commands;
	std::vector<vec2>    coords;

	const auto &contours = shape.getContours();
	for( const auto &contour : contours ) {
		const auto &points = contour.getPoints();
		coords.insert( coords.end(), points.begin(), points.end() );

		const auto numCommands = static_cast<GLsizei>( contour.getNumSegments() );

		commands.reserve( commands.size() + numCommands + 1 /* implicit MOVE_TO at start */ );
		commands.push_back( GL_MOVE_TO_NV );

		for( GLsizei i = 0; i < numCommands; ++i )
			commands.push_back( toPathCommand( contour.getSegmentType( i ) ) );
	}

	mPathId = gl::genPathsNV( 1 );
	gl::pathCommandsNV( mPathId, static_cast<GLsizei>( commands.size() ), commands.data(), static_cast<GLsizei>( coords.size() * 2 /* each vec2 contains 2 floats */ ), GL_FLOAT, coords.data() );
}

Path::Path( const PolyLine2 &polyLine )
{
	const auto &points = polyLine.getPoints();

	std::vector<GLubyte> commands;

	const auto numCommands = points.size() + size_t( polyLine.isClosed() );
	commands.reserve( numCommands );
	commands.push_back( GL_MOVE_TO_NV );

	for( size_t i = 0; i < numCommands; ++i )
		commands.push_back( GL_LINE_TO_NV );

	if( polyLine.isClosed() )
		commands.push_back( GL_CLOSE_PATH_NV );

	mPathId = gl::genPathsNV( 1 );
	gl::pathCommandsNV( mPathId, static_cast<GLsizei>( commands.size() ), commands.data(), static_cast<GLsizei>( points.size() * 2 /* each vec2 contains 2 floats */ ), GL_FLOAT, points.data() );
}

Path::Path( const std::string &svg )
{
	mPathId = gl::genPathsNV( 1 );
	gl::pathStringNV( mPathId, GL_PATH_FORMAT_SVG_NV, GLsizei( svg.length() ), svg.c_str() );
}

float Path::getLength() const
{
	float length{ 0 };

	if( mPathId > 0 )
		length = gl::getPathLengthNV( mPathId, 0, getNumSegments() );

	return length;
}

Rectf Path::getFillBounds() const
{
	Rectf bounds;

	if( mPathId > 0 )
		gl::getPathParameterfvNV( mPathId, GL_PATH_FILL_BOUNDING_BOX_NV, reinterpret_cast<GLfloat *>( &bounds ) );

	return bounds;
}

Rectf Path::getStrokeBounds() const
{
	Rectf bounds;

	if( mPathId > 0 )
		gl::getPathParameterfvNV( mPathId, GL_PATH_STROKE_BOUNDING_BOX_NV, reinterpret_cast<GLfloat *>( &bounds ) );

	return bounds;
}

int Path::getNumSegments() const
{
	int numSegments = 0;

	if( mPathId > 0 )
		gl::getPathParameterivNV( mPathId, GL_PATH_COMMAND_COUNT_NV, &numSegments );

	return numSegments;
}

void Path::resetDashPattern() const
{
	if( mPathId > 0 ) {
		gl::pathDashArrayNV( mPathId, 0, nullptr );
		gl::pathParameteriNV( mPathId, GL_PATH_DASH_CAPS_NV, GLint( CapsStyle::FLAT ) );
		gl::pathParameteriNV( mPathId, GL_PATH_INITIAL_DASH_CAP_NV, GLint( CapsStyle::FLAT ) );
		gl::pathParameteriNV( mPathId, GL_PATH_TERMINAL_DASH_CAP_NV, GLint( CapsStyle::FLAT ) );
	}
}

void Path::setDashPattern( const std::vector<float> &pattern ) const
{
	if( mPathId > 0 ) {
		gl::pathDashArrayNV( mPathId, static_cast<GLsizei>( pattern.size() ), pattern.data() );
	}
}

void Path::setDashOffset( float offset, PathStyle style ) const
{
	if( mPathId > 0 ) {
		gl::pathParameterfNV( mPathId, GL_PATH_DASH_OFFSET_NV, offset );
		gl::pathParameteriNV( mPathId, GL_PATH_DASH_OFFSET_RESET_NV, GLint( style ) );
	}
}

void Path::setDashCaps( CapsStyle caps ) const
{
	setDashCaps( caps, caps );
}

void Path::setDashCaps( CapsStyle initialCap, CapsStyle terminalCap ) const
{
	if( mPathId > 0 ) {
		gl::pathParameteriNV( mPathId, GL_PATH_INITIAL_DASH_CAP_NV, GLint( initialCap ) );
		gl::pathParameteriNV( mPathId, GL_PATH_TERMINAL_DASH_CAP_NV, GLint( terminalCap ) );
	}
}

void Path::setEndCaps( CapsStyle caps ) const
{
	setEndCaps( caps, caps );
}

void Path::setEndCaps( CapsStyle initialCap, CapsStyle terminalCap ) const
{
	if( mPathId > 0 ) {
		gl::pathParameteriNV( mPathId, GL_PATH_INITIAL_END_CAP_NV, GLint( initialCap ) );
		gl::pathParameteriNV( mPathId, GL_PATH_TERMINAL_END_CAP_NV, GLint( terminalCap ) );
	}
}

void Path::setJoinStyle( JoinStyle joins ) const
{
	if( mPathId > 0 ) {
		gl::pathParameteriNV( mPathId, GL_PATH_JOIN_STYLE_NV, GLint( joins ) );
	}
}

void Path::setStrokeWidth( float width ) const
{
	if( mPathId > 0 ) {
		gl::pathParameterfNV( mPathId, GL_PATH_STROKE_WIDTH_NV, GLfloat( width ) );
	}
}

void Path::setMiterLimit( float limit ) const
{
	if( mPathId > 0 ) {
		gl::pathParameterfNV( mPathId, GL_PATH_MITER_LIMIT_NV, GLfloat( limit ) );
	}
}

void Path::setStroke( CapsStyle caps, JoinStyle join, float strokeWidth ) const
{
	if( mPathId > 0 ) {
		gl::pathParameterfNV( mPathId, GL_PATH_STROKE_WIDTH_NV, strokeWidth );
		gl::pathParameteriNV( mPathId, GL_PATH_END_CAPS_NV, GLint( caps ) );
		gl::pathParameteriNV( mPathId, GL_PATH_JOIN_STYLE_NV, GLint( join ) );
	}
}

void Path::stencilStroke( GLuint stencilMask )
{
	if( mPathId > 0 ) {
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilStrokePathNV( mPathId, 0, stencilMask );
	}
}

void Path::stencilFill( GLuint stencilMask, GLenum fillMode )
{
	if( mPathId > 0 ) {
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilFillPathNV( mPathId, fillMode, stencilMask );
	}
}

void Path::cover( const ColorA &color, bool clearStencil )
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::coverFillPathNV( mPathId, GL_BOUNDING_BOX_NV );
	}
}

void Path::cover( const ColorA &color, const Rectf &bounds, bool clearStencil )
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::drawSolidRect( bounds );
	}
}

void Path::cover( const gl::Texture2dRef &texture, const Rectf &bounds, bool clearStencil )
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::draw( texture, bounds );
	}
}

void Path::stroke( const ColorA &color, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverStrokePathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
	}
}

void Path::strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color, bool clearStencil )
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverStrokePathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
	}
}

void Path::strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, bool clearStencil )
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverStrokePathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_3D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
	}
}

void Path::fill( const ColorA &color, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
	}
}

void Path::fill( const gl::TextureRef &texture, const Rectf &bounds, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		const auto pathBounds = getFillBounds();
		const auto textureBounds = Rectf( texture->getBounds() );
		const auto fit = bounds.getCenteredFit( textureBounds, true ).scaled( 1.0f / textureBounds.getSize() );

		auto normalized = Rectf( pathBounds.getUpperLeft() - bounds.getUpperLeft(), pathBounds.getLowerRight() - bounds.getUpperLeft() ).scaled( fit.getSize() / bounds.getSize() );
		normalized.offset( fit.getUpperLeft() );

		const auto upperLeftTexCoord = vec2{ normalized.x1, normalized.y1 };
		const auto lowerRightTexCoord = vec2{ normalized.x2, normalized.y2 };
		fill( texture, upperLeftTexCoord, lowerRightTexCoord, clearStencil );
	}
}

void Path::fill( const gl::TextureRef &texture, const vec2 &upperLeftTexCoord, const vec2 &lowerRightTexCoord, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		gl::ScopedTextureBind scpTex( texture, 0 );

		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( Shader::Type::IMAGE );
		scpShader.setColor( ColorA::white() );
		scpShader.uniform( "image", 0 );
		scpShader.uniform( "opacity", 1 );

		const vec2 s{ lowerRightTexCoord.x - upperLeftTexCoord.x, upperLeftTexCoord.y - lowerRightTexCoord.y }; // Flip y.
		const mat3 m{ 1.0f / s.x, 0, 0, 0, 1.0f / s.y, 0, -upperLeftTexCoord.x, lowerRightTexCoord.y, 1 };      // Flip y.
		scpShader.setCoords( GL_PATH_OBJECT_BOUNDING_BOX_NV, m );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
	}
}

void Path::fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverFillPathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
	}
}

void Path::fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverFillPathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_3D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
	}
}

Path Path::operator+( const Path &other ) const
{
	if( mPathId > 0 ) {
		Path path( *this );

		GLint ourNumCommands = 0;
		GLint ourNumCoords = 0;
		gl::getPathParameterivNV( path.mPathId, GL_PATH_COMMAND_COUNT_NV, &ourNumCommands );
		gl::getPathParameterivNV( path.mPathId, GL_PATH_COORD_COUNT_NV, &ourNumCoords );

		GLint theirNumCommands = 0;
		GLint theirNumCoords = 0;
		gl::getPathParameterivNV( other.mPathId, GL_PATH_COMMAND_COUNT_NV, &theirNumCommands );
		gl::getPathParameterivNV( other.mPathId, GL_PATH_COORD_COUNT_NV, &theirNumCoords );

		std::vector<GLubyte> commands;
		commands.resize( theirNumCommands );
		gl::getPathCommandsNV( other.mPathId, commands.data() );

		std::vector<GLfloat> coords;
		coords.resize( theirNumCoords );
		gl::getPathCoordsNV( other.mPathId, coords.data() );

		gl::pathSubCommandsNV( path.mPathId, ourNumCommands, 0, theirNumCommands, commands.data(), theirNumCoords, GL_FLOAT, coords.data() );

		return path;
	}

	return other;
}

Path &Path::operator+=( const Path &other )
{
	if( mPathId > 0 ) {
		GLint ourNumCommands = 0;
		GLint ourNumCoords = 0;
		gl::getPathParameterivNV( mPathId, GL_PATH_COMMAND_COUNT_NV, &ourNumCommands );
		gl::getPathParameterivNV( mPathId, GL_PATH_COORD_COUNT_NV, &ourNumCoords );

		GLint theirNumCommands = 0;
		GLint theirNumCoords = 0;
		gl::getPathParameterivNV( other.mPathId, GL_PATH_COMMAND_COUNT_NV, &theirNumCommands );
		gl::getPathParameterivNV( other.mPathId, GL_PATH_COORD_COUNT_NV, &theirNumCoords );

		std::vector<GLubyte> commands;
		commands.resize( theirNumCommands );
		gl::getPathCommandsNV( other.mPathId, commands.data() );

		std::vector<GLfloat> coords;
		coords.resize( theirNumCoords );
		gl::getPathCoordsNV( other.mPathId, coords.data() );

		gl::pathSubCommandsNV( mPathId, ourNumCommands, 0, theirNumCommands, commands.data(), theirNumCoords, GL_FLOAT, coords.data() );
	}
	else
		*this = Path( other );

	return *this;
}

void Path::transform( const glm::mat3x2 &transform ) const
{
	if( mPathId > 0 )
		gl::transformPathNV( mPathId, mPathId, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( &transform ) );
}

Path Path::transformed( const glm::mat3x2 &transform ) const
{
	Path path( *this );
	path.transform( transform );
	return path;
}

void Path::reverse() const
{
	if( mPathId > 0 ) {
		GLint numCommands = 0;
		GLint numCoords = 0;
		gl::getPathParameterivNV( mPathId, GL_PATH_COMMAND_COUNT_NV, &numCommands );
		gl::getPathParameterivNV( mPathId, GL_PATH_COORD_COUNT_NV, &numCoords );

		std::vector<GLubyte> commands;
		commands.resize( numCommands );
		gl::getPathCommandsNV( mPathId, commands.data() );

		std::vector<GLfloat> coords;
		coords.resize( numCoords );
		gl::getPathCoordsNV( mPathId, coords.data() );

		// Reverse all coords.
		std::reverse( coords.begin(), coords.end() );

		// Reverse all commands, but change move-to's into close and close into move-to's.
		std::reverse( commands.begin(), commands.end() );
		for( auto &command : commands ) {
			switch( command ) {
			case GL_MOVE_TO_NV:
			case GL_RELATIVE_MOVE_TO_NV:
				command = GL_CLOSE_PATH_NV;
				break;
			case GL_CLOSE_PATH_NV:
				command = GL_MOVE_TO_NV;
				break;
			default: // do nothing
				break;
			}
		}

		gl::getPathCoordsNV( mPathId, coords.data() );
		gl::getPathCommandsNV( mPathId, commands.data() );
	}
}

GLubyte Path::toPathCommand( Path2d::SegmentType type )
{
	switch( type ) {
	case Path2d::SegmentType::MOVETO:
		return GL_MOVE_TO_NV;
	case Path2d::SegmentType::LINETO:
		return GL_LINE_TO_NV;
	case Path2d::SegmentType::QUADTO:
		return GL_QUADRATIC_CURVE_TO_NV;
	case Path2d::SegmentType::CUBICTO:
		return GL_CUBIC_CURVE_TO_NV;
	case Path2d::SegmentType::CLOSE:
		return GL_CLOSE_PATH_NV;
	}

	return 0;
}

void Path::getPath( std::vector<GLubyte> &commands, std::vector<GLfloat> &coords ) const
{
	if( mPathId > 0 ) {
		GLint numCommands;
		gl::getPathParameterivNV( mPathId, GL_PATH_COMMAND_COUNT_NV, &numCommands );
		GLint numCoords;
		gl::getPathParameterivNV( mPathId, GL_PATH_COORD_COUNT_NV, &numCoords );

		commands.resize( numCommands );
		gl::getPathCommandsNV( mPathId, commands.data() );

		coords.resize( numCoords );
		gl::getPathCoordsNV( mPathId, coords.data() );
	}
}

void Path::setPath( const std::vector<GLubyte> &commands, const std::vector<GLfloat> &coords )
{
	if( !mPathId )
		mPathId = gl::genPathsNV( 1 );

	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

void Path::setPath( const std::string &svg )
{
	if( !mPathId )
		mPathId = gl::genPathsNV( 1 );

	gl::pathStringNV( mPathId, GL_PATH_FORMAT_SVG_NV, GLsizei( svg.length() ), svg.c_str() );
}

Shader::Shader( Type type )
	: mType( type )
{
	switch( type ) {
	case Type::SOLID_COLOR: {
		static const char *glsl
			= "#version 330 core\n"
			  "#extension GL_ARB_separate_shader_objects : enable\n"
			  "precision highp float;"
			  "layout(location = 0) in vec4 color;"
			  "uniform float opacity = 1.0;"
			  "out vec4 fragColor;"
			  "void main() {"
			  "  fragColor = color * opacity;"
			  "}";

		mProgram = glCreateShaderProgramv( GL_FRAGMENT_SHADER, 1, &glsl );
		break;
	}
	case Type::LINEAR_GRADIENT: {
		static const char *glsl
			= "#version 330 core\n"
			  "#extension GL_ARB_separate_shader_objects : enable\n"
			  "precision highp float;"
			  "layout(location = 0) in vec4 color;"
			  "layout(location = 1) in vec2 uv;"
			  "uniform float index = 0.5;"
			  "uniform float opacity = 1.0;"
			  "uniform sampler2D gradTab;"
			  "uniform vec2 gradStart;"
			  "uniform vec2 gradEnd;"
			  "out vec4 fragColor;"
			  "void main() {"
			  "    vec2 gradVec = gradEnd - gradStart;"
			  "    float gradTabIndex = dot(gradVec, uv - gradStart) / (gradVec.x * gradVec.x + gradVec.y * gradVec.y);"
			  "    fragColor = texture(gradTab, vec2(gradTabIndex, index));"
			  "    fragColor.a *= opacity;"
			  "    fragColor.rgb *= fragColor.a;"
			  "}";

		mProgram = glCreateShaderProgramv( GL_FRAGMENT_SHADER, 1, &glsl );
		break;
	}
	case Type::RADIAL_GRADIENT: {
		static const char *glsl
			= "#version 330 core\n"
			  "#extension GL_ARB_separate_shader_objects : enable\n"
			  "precision highp float;"
			  "uniform sampler2D gradTab;"
			  "uniform float index = 0.5;"
			  "uniform float opacity = 1.0;"
			  "uniform vec2 focalToCenter;"
			  "uniform float centerRadius;"
			  "uniform float focalRadius;"
			  "uniform vec2 translationPoint;"
			  "layout(location = 0) in vec4 color;"
			  "layout(location = 1) in vec2 uv;"
			  "out vec4 fragColor;"
			  "void main() {"
			  "    vec2 coord = uv - translationPoint;"
			  "    float rd = centerRadius - focalRadius;"
			  "    float b = 2.0 * (rd * focalRadius + dot(coord, focalToCenter));"
			  "    float fmp2_m_radius2 = -focalToCenter.x * focalToCenter.x - focalToCenter.y * focalToCenter.y + rd * rd;"
			  "    float inverse_2_fmp2_m_radius2 = 1.0 / (2.0 * fmp2_m_radius2);"
			  "    float det = b * b - 4.0 * fmp2_m_radius2 * ((focalRadius * focalRadius) - dot(coord, coord));"
			  "    fragColor = vec4(0.0);"
			  "    if (det >= 0.0) {"
			  "        float detSqrt = sqrt(det);"
			  "        float w = max((-b - detSqrt) * inverse_2_fmp2_m_radius2, (-b + detSqrt) * inverse_2_fmp2_m_radius2);"
			  "        if (focalRadius + w * (centerRadius - focalRadius) >= 0.0)"
			  "            fragColor = texture(gradTab, vec2(w, index));"
			  "    }"
			  "    fragColor.a *= opacity;"
			  "    fragColor.rgb *= fragColor.a;"
			  "}";

		mProgram = glCreateShaderProgramv( GL_FRAGMENT_SHADER, 1, &glsl );
		break;
	}
	case Type::CONICAL_GRADIENT: { // UNTESTED
		static const char *glsl
			= "#version 330 core\n"
			  "#extension GL_ARB_separate_shader_objects : enable\n"
			  "precision highp float;"
			  "#define INVERSE_2PI 0.1591549430918953358"
			  "uniform sampler2D gradTab;"
			  "uniform float index = 0.5;"
			  "uniform float opacity = 1.0;"
			  "uniform float angle;"
			  "uniform vec2 translationPoint;"
			  "layout(location = 0) in vec4 color;"
			  "layout(location = 1) in vec2 uv;"
			  "out vec4 fragColor;"
			  "void main() {"
			  "    vec2 coord = uv - translationPoint;"
			  "    float t;"
			  "    if (abs(coord.y) == abs(coord.x))"
			  "        t = (atan(-coord.y + 0.002, coord.x) + angle) * INVERSE_2PI;"
			  "    else"
			  "        t = (atan(-coord.y, coord.x) + angle) * INVERSE_2PI;"
			  "    fragColor = texture(gradTab, vec2(t - floor(t), index));"
			  "    fragColor.a *= opacity;"
			  "    fragColor.rgb *= fragColor.a;"
			  "}";

		mProgram = glCreateShaderProgramv( GL_FRAGMENT_SHADER, 1, &glsl );
		break;
	}
	case Type::IMAGE: {
		static const char *glsl
			= "#version 330 core\n"
			  "#extension GL_ARB_separate_shader_objects : enable\n"
			  "precision highp float;"
			  "uniform sampler2D image;"
			  "uniform float opacity = 1;"
			  "layout(location = 0) in vec4 color;"
			  "layout(location = 1) in vec2 uv;"
			  "out vec4 fragColor;"
			  "void main() {"
			  "    fragColor = texture(image, vec2( uv.x, 1.0 - uv.y ) );" // Flip texture vertically.
			  "    if( uv.x < 0 || uv.y < 0 || uv.x > 1 || uv.y > 1 ) {"
			  "        fragColor.a = 0;"
			  "    }"
			  "    fragColor.a *= opacity;"
			  "    fragColor.rgb *= fragColor.a;"
			  "}";

		mProgram = glCreateShaderProgramv( GL_FRAGMENT_SHADER, 1, &glsl );
		break;
	}
	default:
		mProgram = 0;
		break;
	}

	GLint status = 0;
	glGetProgramiv( mProgram, GL_LINK_STATUS, &status );
	if( !status ) {
		// error!
		GLchar  buffer[2048];
		GLsizei length = 0;
		glGetProgramInfoLog( mProgram, 2048, &length, buffer );
		CI_LOG_E( std::string( buffer, length ) );

		return;
	}

	glGenProgramPipelines( 1, &mPipeline );
	glUseProgramStages( mPipeline, GL_FRAGMENT_SHADER_BIT, mProgram );
	glActiveShaderProgram( mPipeline, mProgram );
	glValidateProgramPipeline( mPipeline );

	status = 0;
	glGetProgramPipelineiv( mPipeline, GL_VALIDATE_STATUS, &status );
	if( !status ) {
		// error!
		GLchar  buffer[2048];
		GLsizei length = 0;
		glGetProgramInfoLog( mProgram, 2048, &length, buffer );
		CI_LOG_E( std::string( buffer, length ) );

		return;
	}

	switch( type ) {
	case Type::LINEAR_GRADIENT:
	case Type::RADIAL_GRADIENT:
	case Type::CONICAL_GRADIENT:
	case Type::IMAGE: {
		const GLfloat data[6] = { 1, 0, 0, 0, 1, 0 };
		gl::programPathFragmentInputGenNV( mProgram, 0, GL_PATH_OBJECT_BOUNDING_BOX_NV, 2, &data[0] );
		break;
	}
	case Type::SOLID_COLOR: {
		break;
	}
	default:
		break;
	}
}

Shader::~Shader()
{
	if( mPipeline )
		glDeleteProgramPipelines( 1, &mPipeline );
	if( mProgram )
		glDeleteProgram( mProgram );

	mProgram = 0;
	mPipeline = 0;
}

void Shader::bind() const
{
	if( mPipeline )
		glBindProgramPipeline( mPipeline );
}

void Shader::unbind()
{
	glBindProgramPipeline( 0 );
}

void Shader::uniform( const std::string &name, GLint value ) const
{
	if( mPipeline && mProgram ) {
		if( GLint loc = glGetProgramResourceLocation( mProgram, GL_UNIFORM, name.c_str() ); loc >= 0 )
			glProgramUniform1i( mProgram, loc, value );
	}
}

void Shader::uniform( const std::string &name, GLfloat value ) const
{
	if( mPipeline && mProgram ) {
		if( GLint loc = glGetProgramResourceLocation( mProgram, GL_UNIFORM, name.c_str() ); loc >= 0 )
			glProgramUniform1f( mProgram, loc, value );
	}
}

void Shader::uniform( const std::string &name, const vec2 &value ) const
{
	if( mPipeline && mProgram ) {
		if( GLint loc = glGetProgramResourceLocation( mProgram, GL_UNIFORM, name.c_str() ); loc >= 0 )
			glProgramUniform2fv( mProgram, loc, 1, value_ptr( value ) );
	}
}

void Shader::uniform( const std::string &name, const vec3 &value ) const
{
	if( mPipeline && mProgram ) {
		if( GLint loc = glGetProgramResourceLocation( mProgram, GL_UNIFORM, name.c_str() ); loc >= 0 )
			glProgramUniform3fv( mProgram, loc, 1, value_ptr( value ) );
	}
}

void Shader::uniform( const std::string &name, const vec4 &value ) const
{
	if( mPipeline && mProgram ) {
		if( GLint loc = glGetProgramResourceLocation( mProgram, GL_UNIFORM, name.c_str() ); loc >= 0 )
			glProgramUniform4fv( mProgram, loc, 1, value_ptr( value ) );
	}
}

void Shader::uniform( const std::string &name, const mat3 &value ) const
{
	if( mPipeline && mProgram ) {
		if( GLint loc = glGetProgramResourceLocation( mProgram, GL_UNIFORM, name.c_str() ); loc >= 0 )
			glProgramUniformMatrix3fv( mProgram, loc, 1, false, value_ptr( value ) );
	}
}

void Shader::uniform( const std::string &name, const glm::mat3x2 &value ) const
{
	if( mPipeline && mProgram ) {
		if( GLint loc = glGetProgramResourceLocation( mProgram, GL_UNIFORM, name.c_str() ); loc >= 0 )
			glProgramUniformMatrix3x2fv( mProgram, loc, 1, false, value_ptr( value ) );
	}
}

void Shader::uniform( const std::string &name, const mat4 &value ) const
{
	if( mPipeline && mProgram ) {
		if( GLint loc = glGetProgramResourceLocation( mProgram, GL_UNIFORM, name.c_str() ); loc >= 0 )
			glProgramUniformMatrix4fv( mProgram, loc, 1, false, value_ptr( value ) );
	}
}

void Shader::setColor( const ci::ColorA &color ) const
{
	gl::programPathFragmentInputGenNV( mProgram, 0, GL_CONSTANT_NV, 4, color.premultiplied().ptr() );
}

ScopedShader::ScopedShader( Shader::Type type )
	: mCtx( gl::context() )
{
	mShader = Cache::loadShader( type );
	mShader->bind();
}

ScopedShader::ScopedShader( const ColorA &color )
	: ScopedShader( Shader::Type::SOLID_COLOR )
{
	if( mShader )
		mShader->setColor( color );
}

ScopedShader::~ScopedShader()
{
	if( mShader )
		mShader->unbind();
}

ShaderRef Cache::loadShader( Shader::Type type )
{
	Cache &self = get();

	if( self.mShaders.count( type ) && static_cast<bool>( self.mShaders.at( type ) ) ) {
		return self.mShaders.at( type );
	}

	CI_LOG_V( "Creating shader (" << std::this_thread::get_id() << ")" );
	auto shader = Shader::create( type );
	self.mShaders.insert_or_assign( type, shader );

	return shader;
}

void Cache::clean()
{
	auto &shaders = get().mShaders;
	for( auto itr = shaders.begin(); itr != shaders.end(); ) {
		const auto &item = *itr;
		if( item.second.use_count() < 2 ) {
			CI_LOG_V( "Removing shader (" << std::this_thread::get_id() << ")" );
			itr = shaders.erase( itr );
		}
		else
			++itr;
	}
}

void Cache::clear()
{
	CI_LOG_V( "Removing all shaders (" << std::this_thread::get_id() << ")" );
	get().mShaders.clear();
}

Canvas::Canvas( int samples, int coverageSamples, bool useFloats )
	: mSamples( samples )
	, mCoverageSamples( coverageSamples )
	, mUseFloats( useFloats )
{
}

Canvas::Canvas( int width, int height, int samples, int coverageSamples, bool useFloats )
	: mWidth( width )
	, mHeight( height )
	, mSamples( samples )
	, mCoverageSamples( coverageSamples )
	, mUseFloats( useFloats )
{
}

void Canvas::bind() const
{
	if( !mIsBound ) {
		if( !mFbo ) {
			mCtx = gl::context();
			mFbo = gl::Fbo::create( mWidth, mHeight, getFboFormat() );
		}
		if( mCtx == gl::context() ) {
			mCtx->pushFramebuffer( mFbo );
			mCtx->pushViewport( std::make_pair( ivec2( 0 ), mFbo->getSize() ) );
			mCtx->pushGlslProg( nullptr );
			gl::pushMatrices();
			gl::pushModelMatrix();
			gl::setMatricesWindow( mWidth, mHeight );
			gl::popModelMatrix();
			gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
			gl::matrixLoadfEXT( GL_PROJECTION, value_ptr( gl::getProjectionMatrix() ) );
			gl::clearColor( ColorA( 0, 0, 0, 0 ) );
			gl::clear( GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
			gl::clearStencil( 0 );
			gl::stencilMask( ~0 );
			mIsBound = true;
		}
	}
}

void Canvas::unbind() const
{
	if( mIsBound && mCtx == gl::context() ) {
		mIsBound = false;
		gl::popMatrices();
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::matrixLoadfEXT( GL_PROJECTION, value_ptr( gl::getProjectionMatrix() ) );
		mCtx->popGlslProg( true );
		mCtx->popViewport();
		mCtx->popFramebuffer();
	}
}

void Canvas::draw() const
{
	if( mFbo && !mIsBound ) {
		// Use pre-multiplied alpha!
		gl::ScopedBlendPremult scpBlend;
		gl::draw( mFbo->getColorTexture() );
	}
}

void Canvas::draw( const Rectf &bounds ) const
{
	if( mFbo && !mIsBound ) {
		gl::draw( mFbo->getColorTexture(), bounds );
	}
}

gl::Texture2dRef Canvas::getTexture( GLenum attachment ) const
{
	if( mFbo )
		return mFbo->getTexture2d( attachment );

	return nullptr;
}

gl::Fbo::Format Canvas::getFboFormat() const
{
	if( mUseFloats ) {
		// Using GL_NEAREST, we try to avoid additional blurring of the final image.
		const auto format = gl::Texture2d::Format().internalFormat( GL_RGBA16F ).dataType( GL_FLOAT ).minFilter( GL_NEAREST ).magFilter( GL_NEAREST );
		return gl::Fbo::Format().samples( mSamples ).coverageSamples( mCoverageSamples ).stencilBuffer().disableDepth().colorTexture( format );
	}

	// Using GL_NEAREST, we try to avoid additional blurring of the final image.
	const auto format = gl::Texture2d::Format().minFilter( GL_NEAREST ).magFilter( GL_NEAREST );
	return gl::Fbo::Format().samples( mSamples ).coverageSamples( mCoverageSamples ).stencilBuffer().disableDepth().colorTexture( format );
}

void Canvas::resize( const ivec2 &size )
{
	if( size.x > 0 && size.y > 0 && ( mWidth != size.x || mHeight != size.y ) ) {
		mWidth = size.x;
		mHeight = size.y;

		const bool isBound = mIsBound;
		if( isBound )
			unbind();

		mFbo.reset();

		if( isBound )
			bind();
	}
}

} // namespace nvpath
} // namespace cinder