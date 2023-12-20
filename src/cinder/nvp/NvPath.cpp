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

#include "cinder/nvp/NvPath.h"

#include "cinder/Log.h"
#include "cinder/Utilities.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/scoped.h"

namespace cinder {
namespace nvp {

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

Path::~Path()
{
	if( mPathId > 0 )
		gl::deletePathsNV( mPathId, 1 );
}

Path::Path()
{
	mPathId = gl::genPathsNV( 1 );
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

void Path::stroke( const svg::Paint &paint, float opacity, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( preparePaint( paint, opacity, true ) );

		bindGradients( gl::context(), 0 );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverStrokePathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );

		unbindGradients( gl::context() );
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

void Path::fill( const svg::Paint &paint, float opacity, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( preparePaint( paint, opacity, true ) );

		bindGradients( gl::context(), 0 );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );

		unbindGradients( gl::context() );
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
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

void Path::fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, bool clearStencil ) const
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_3D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

Path Path::operator+( const Path &other ) const
{
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

Path &Path::operator+=( const Path &other )
{
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

	return *this;
}

void Path::transform( const glm::mat3x2 &transform ) const
{
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

Shader::Type Path::preparePaint( const svg::Paint &paint, float opacity, bool prepareShader )
{
	if( paint.isLinearGradient() )
		return prepareLinearGradient( paint, opacity, prepareShader );

	if( paint.isRadialGradient() )
		return prepareRadialGradient( paint, opacity, prepareShader );

	return Shader::Type::SOLID_COLOR;
}

Shader::Type Path::prepareLinearGradient( const svg::Paint &paint, float opacity, bool prepareShader )
{
	assert( paint.isLinearGradient() );

	sGradients.set( paint );
	sGradients.setSpreadMethod( paint.getSpreadMethod() );

	if( prepareShader ) {
		ScopedShader scpShader( Shader::Type::LINEAR_GRADIENT );
		scpShader.setCoords( paint.useObjectBoundingBox() ? GL_PATH_OBJECT_BOUNDING_BOX_NV : GL_OBJECT_LINEAR_NV, paint.getTransform() );
		scpShader.uniform( "index", sGradients.index( paint.getId() ) );
		scpShader.uniform( "gradTab", 0 );
		scpShader.uniform( "gradStart", paint.getCoords0() );
		scpShader.uniform( "gradEnd", paint.getCoords1() );
		scpShader.uniform( "opacity", opacity );
	}

	return Shader::Type::LINEAR_GRADIENT;
}

Shader::Type Path::prepareRadialGradient( const svg::Paint &paint, float opacity, bool prepareShader )
{
	assert( paint.isRadialGradient() );

	sGradients.set( paint );
	sGradients.setSpreadMethod( paint.getSpreadMethod() );

	if( prepareShader ) {
		ScopedShader scpShader( Shader::Type::RADIAL_GRADIENT );
		scpShader.setCoords( paint.useObjectBoundingBox() ? GL_PATH_OBJECT_BOUNDING_BOX_NV : GL_OBJECT_LINEAR_NV, paint.getTransform() );
		scpShader.uniform( "index", sGradients.index( paint.getId() ) );
		scpShader.uniform( "gradTab", 0 );
		scpShader.uniform( "focalToCenter", paint.getCoords0() - paint.getCoords1() );
		scpShader.uniform( "centerRadius", paint.getRadius0() );
		scpShader.uniform( "focalRadius", paint.getRadius1() );
		scpShader.uniform( "translationPoint", paint.getCoords1() );
		scpShader.uniform( "opacity", opacity );
	}

	return Shader::Type::RADIAL_GRADIENT;
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
	GLint numCommands;
	gl::getPathParameterivNV( mPathId, GL_PATH_COMMAND_COUNT_NV, &numCommands );
	GLint numCoords;
	gl::getPathParameterivNV( mPathId, GL_PATH_COORD_COUNT_NV, &numCoords );

	commands.resize( numCommands );
	gl::getPathCommandsNV( mPathId, commands.data() );

	coords.resize( numCoords );
	gl::getPathCoordsNV( mPathId, coords.data() );
}

void Path::setPath( const std::vector<GLubyte> &commands, const std::vector<GLfloat> &coords ) const
{
	gl::pathCommandsNV( mPathId, GLsizei( commands.size() ), commands.data(), GLsizei( coords.size() ), GL_FLOAT, coords.data() );
}

Face::Face( const Font &font )
	: mFont( font )
{
	mNumGlyphs = mFont.getNumGlyphs();
	mBaseId = gl::genPathsNV( mNumGlyphs );

	createPaths();
}

Face::~Face()
{
	if( mBaseId > 0 )
		gl::deletePathsNV( mBaseId, GLsizei( mNumGlyphs ) );
}

void Face::setStrokeStyle( float width, JoinStyle joinStyle, CapsStyle capsStyle ) const
{
	for( GLsizei i = 0; i < mNumGlyphs; ++i ) {
		gl::pathParameterfNV( mBaseId + i, GL_PATH_STROKE_WIDTH_NV, width );
		gl::pathParameteriNV( mBaseId + i, GL_PATH_JOIN_STYLE_NV, GLint( joinStyle ) );
		gl::pathParameteriNV( mBaseId + i, GL_PATH_END_CAPS_NV, GLint( capsStyle ) );
	}
}

namespace {
struct Glyph {
	GLuint               id{ 0 };
	GLfloat              scale{ 1 };
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;
};

int moveTo( const ivec2 *to, void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );

	// Always close glyph paths. This improves stroking.
	if( !glyph.commands.empty() && glyph.commands.back() != GL_CLOSE_PATH_NV )
		glyph.commands.push_back( GL_CLOSE_PATH_NV );

	glyph.commands.push_back( GL_MOVE_TO_NV );
	glyph.coords.push_back( to->x * glyph.scale );
	glyph.coords.push_back( to->y * glyph.scale );
	return 0;
}

int lineTo( const ivec2 *to, void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );
	glyph.commands.push_back( GL_LINE_TO_NV );
	glyph.coords.push_back( to->x * glyph.scale );
	glyph.coords.push_back( to->y * glyph.scale );
	return 0;
}

int quadTo( const ivec2 *control, const ivec2 *to, void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );
	glyph.commands.push_back( GL_QUADRATIC_CURVE_TO_NV );
	glyph.coords.push_back( control->x * glyph.scale );
	glyph.coords.push_back( control->y * glyph.scale );
	glyph.coords.push_back( to->x * glyph.scale );
	glyph.coords.push_back( to->y * glyph.scale );
	return 0;
}

int cubicTo( const ivec2 *control1, const ivec2 *control2, const ivec2 *to, void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );
	glyph.commands.push_back( GL_CUBIC_CURVE_TO_NV );
	glyph.coords.push_back( control1->x * glyph.scale );
	glyph.coords.push_back( control1->y * glyph.scale );
	glyph.coords.push_back( control2->x * glyph.scale );
	glyph.coords.push_back( control2->y * glyph.scale );
	glyph.coords.push_back( to->x * glyph.scale );
	glyph.coords.push_back( to->y * glyph.scale );
	return 0;
}

void restart( void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );

	if( !( glyph.commands.empty() || glyph.coords.empty() ) ) {
		// Always close glyph paths. This improves stroking.
		if( glyph.commands.back() != GL_CLOSE_PATH_NV )
			glyph.commands.push_back( GL_CLOSE_PATH_NV );

		gl::pathCommandsNV( glyph.id, GLsizei( glyph.commands.size() ), glyph.commands.data(), GLsizei( glyph.coords.size() ), GL_FLOAT, glyph.coords.data() );
	}

	glyph.id++;
	glyph.commands.clear();
	glyph.coords.clear();
}

} // namespace

void Face::createPaths() const
{
	// text::Face::OutlineFunctions functions;
	// functions.moveTo = moveTo;
	// functions.lineTo = lineTo;
	// functions.quadTo = quadTo;
	// functions.cubicTo = cubicTo;
	// functions.restart = restart;

	// Glyph glyph;
	// glyph.id = mBaseId;
	// glyph.scale = DEFAULT_SIZE / float( mFace->getUnitsPerEm() );

	// mFace->getGlyphOutlines( 0, mNumGlyphs, functions, &glyph );
}

bool ClipRect::push()
{
	if( mCtx == nullptr ) {
		mCtx = gl::context();

		// Calculate scissor coordinates.
		ivec4 lowerLeft = gl::getModelView() * vec4( mBounds.x1, mBounds.y2, 0, 1 );
		ivec4 upperRight = gl::getModelView() * vec4( mBounds.x2, mBounds.y1, 0, 1 );

		const auto &[origin, size] = gl::getViewport();
		ivec2 position{ lowerLeft.x, size.y - lowerLeft.y };
		ivec2 dimensions{ upperRight.x - lowerLeft.x, lowerLeft.y - upperRight.y };

		// Clip by current scissor.
		if( mCtx->getBoolState( GL_SCISSOR_TEST ) ) {
			const auto scissor = mCtx->getScissor();

			Area ours( position, position + dimensions );
			Area theirs( scissor.first, scissor.first + scissor.second );
			ours.clipBy( theirs );

			position = ours.getUL();
			dimensions = ours.getSize();
		}

		mCtx->pushBoolState( GL_SCISSOR_TEST, GL_TRUE );
		mCtx->pushScissor( std::make_pair( position, dimensions ) );

		return true;
	}

	return false;
}

bool ClipRect::pop()
{
	if( mCtx == gl::context() ) {
		mCtx->popBoolState( GL_SCISSOR_TEST );
		mCtx->popScissor();

		mCtx = nullptr;

		return true;
	}

	return false;
}

bool Gradients::contains( const std::string &id ) const
{
	return mLookUp.count( id ) > 0;
}

float Gradients::index( const std::string &id ) const
{
	if( !mTexture || !mLookUp.count( id ) )
		return 0.0f;

	return ( float( mLookUp.at( id ) ) + 0.5f ) / float( mTexture->getHeight() );
}

void Gradients::set( const svg::Gradient &gradient )
{
	if( !mLookUp.count( gradient.getId() ) ) {
		store( mIndex, gradient.asPaint() );

		mLookUp.insert_or_assign( gradient.getId(), mIndex++ );
	}
	else {
		// store( mLookUp.at( gradient.getId() ), gradient.asPaint() );
	}
}

void Gradients::set( const svg::Paint &paint )
{
	if( !mLookUp.count( paint.getId() ) ) {
		store( mIndex, paint );

		mLookUp.insert_or_assign( paint.getId(), mIndex++ );
	}
	else {
		// store( mLookUp.at( paint.getId() ), paint );
	}
}

void Gradients::setSpreadMethod( svg::SpreadMethod method ) const
{
	if( mTexture ) {
		mTexture->setWrapS(method == svg::SpreadMethod::SPREAD_METHOD_REFLECT ? GL_MIRRORED_REPEAT : //							
							method == svg::SpreadMethod::SPREAD_METHOD_REPEAT ? GL_REPEAT : //																				   
																				   GL_CLAMP_TO_EDGE );
	}
}

void Gradients::bind( gl::Context *ctx, uint8_t textureUnit )
{
	assert( nullptr == mCtx );

	mTextureUnit = textureUnit;
	mCtx = ctx;

	if( mTexture )
		mCtx->pushTextureBinding( mTexture->getTarget(), mTexture->getId(), mTextureUnit );
}

void Gradients::unbind( gl::Context *ctx )
{
	assert( ctx == mCtx );

	if( mTexture )
		mCtx->popTextureBinding( mTexture->getTarget(), mTextureUnit );

	mCtx = nullptr;
}

gl::Texture2dRef Gradients::create( int width, int height ) const
{
	static const gl::Texture2d::Format FORMAT = gl::Texture2d::Format().wrap( GL_REPEAT ).internalFormat( GL_RGBA ).target( GL_TEXTURE_2D ).loadTopDown();

	gl::Texture2dRef texture = gl::Texture2d::create( width, height, FORMAT );
	return texture;
}

void Gradients::store( size_t index, const svg::Paint &paint ) const
{
	// Resize texture if more space is needed.
	if( const auto size = glm::max( 128, int( index ) ); !mTexture || mTexture->getHeight() < size ) {
		const auto texture = create( 128, int( isPowerOf2( size ) ? size : nextPowerOf2( size ) ) );

		if( mTexture ) {
			if( glad_glCopyImageSubData ) {
				glCopyImageSubData(                                       //
					mTexture->getId(), mTexture->getTarget(), 0, 0, 0, 0, // Src
					texture->getId(), texture->getTarget(), 0, 0, 0, 0,   // Dst
					mTexture->getWidth(), mTexture->getHeight(), 1 );     // Size
			}
			else {
				GLint previous[2];
				glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &previous[0] );
				glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &previous[1] );

				GLuint fbo[2];
				glGenFramebuffers( 2, fbo );
				glNamedFramebufferTexture2DEXT( fbo[0], GL_COLOR_ATTACHMENT0, mTexture->getTarget(), mTexture->getId(), 0 );
				glNamedFramebufferTexture2DEXT( fbo[1], GL_COLOR_ATTACHMENT0, texture->getTarget(), texture->getId(), 0 );
				glBindFramebuffer( GL_READ_FRAMEBUFFER, fbo[0] );
				glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo[1] );
				glBlitFramebufferEXT( 0, 0, mTexture->getWidth(), mTexture->getHeight(), 0, 0, mTexture->getWidth(), mTexture->getHeight(), GL_COLOR_BUFFER_BIT, GL_LINEAR );
				glBindFramebuffer( GL_READ_FRAMEBUFFER, previous[0] );
				glBindFramebuffer( GL_DRAW_FRAMEBUFFER, previous[1] );
				glDeleteFramebuffers( 2, fbo );
			}
		}

		if( mCtx && mTexture )
			mCtx->popTextureBinding( mTexture->getTarget(), mTextureUnit );

		mTexture = texture;

		if( mCtx )
			mCtx->pushTextureBinding( mTexture->getTarget(), mTexture->getId(), mTextureUnit );
	}

	const auto data = paint.data( 128, 1, false );
	mTexture->update( data.get(), GL_RGBA, GL_UNSIGNED_BYTE, 0, 128, 1, { 0, index } );
}

Svg::Svg( const DataSourceRef &src )
	: Svg( svg::Doc::create( src ) )
{
}

Svg::Svg( const svg::DocRef &svg )
	: mDoc( svg )
	, mBounds( svg->getBounds() )
{
}

Shader::Type Svg::prepareLinearGradient( const svg::Paint &paint, float opacity, bool prepareShader )
{
	assert( paint.isLinearGradient() );

	mGradients.set( paint );
	mGradients.setSpreadMethod( paint.getSpreadMethod() );

	if( prepareShader ) {
		ScopedShader scpShader( Shader::Type::LINEAR_GRADIENT );
		scpShader.setCoords( paint.useObjectBoundingBox() ? GL_PATH_OBJECT_BOUNDING_BOX_NV : GL_OBJECT_LINEAR_NV, paint.getTransform() );
		scpShader.uniform( "index", mGradients.index( paint.getId() ) );
		scpShader.uniform( "gradTab", 0 );
		scpShader.uniform( "gradStart", paint.getCoords0() );
		scpShader.uniform( "gradEnd", paint.getCoords1() );
		scpShader.uniform( "opacity", opacity );
	}

	return Shader::Type::LINEAR_GRADIENT;
}

Shader::Type Svg::prepareRadialGradient( const svg::Paint &paint, float opacity, bool prepareShader )
{
	assert( paint.isRadialGradient() );

	mGradients.set( paint );
	mGradients.setSpreadMethod( paint.getSpreadMethod() );

	if( prepareShader ) {
		ScopedShader scpShader( Shader::Type::RADIAL_GRADIENT );
		scpShader.setCoords( paint.useObjectBoundingBox() ? GL_PATH_OBJECT_BOUNDING_BOX_NV : GL_OBJECT_LINEAR_NV, paint.getTransform() );
		scpShader.uniform( "index", mGradients.index( paint.getId() ) );
		scpShader.uniform( "gradTab", 0 );
		scpShader.uniform( "focalToCenter", paint.getCoords0() - paint.getCoords1() );
		scpShader.uniform( "centerRadius", paint.getRadius0() );
		scpShader.uniform( "focalRadius", paint.getRadius1() );
		scpShader.uniform( "translationPoint", paint.getCoords1() );
		scpShader.uniform( "opacity", opacity );
	}

	return Shader::Type::RADIAL_GRADIENT;
}

Shader::Type Svg::preparePaint( const svg::Paint &paint, float opacity, bool prepareShader )
{
	if( paint.isLinearGradient() )
		return prepareLinearGradient( paint, opacity, prepareShader );

	if( paint.isRadialGradient() )
		return prepareRadialGradient( paint, opacity, prepareShader );

	return Shader::Type::SOLID_COLOR;
}

void Svg::addCommand( Cmd cmd, GLuint pathId, const glm::mat3x2 &transform )
{
	mCommands.push_back( cmd );
	mCommands.push_back( pathId );

	size_t index = mCommands.size();
	mCommands.resize( mCommands.size() + sizeof( transform ) / sizeof( uint32_t ) );
	memcpy( mCommands.data() + index, glm::value_ptr( transform ), sizeof( transform ) );
}

void Svg::addPushClipCommand( GLuint pathId, const glm::mat3x2 &transform, GLuint clipMask, GLuint fillRule )
{
	addCommand( PUSH_CLIP, pathId, transform );
	mCommands.push_back( clipMask );
	mCommands.push_back( fillRule );
}

void Svg::addPopClipCommand( GLuint pathId, const glm::mat3x2 &transform, GLuint clipMask )
{
	addCommand( POP_CLIP, pathId, transform );
	mCommands.push_back( clipMask );
}

void Svg::addPreparePaintCommand( const svg::Paint &paint, float opacity )
{
	mCommands.push_back( PREPARE_PAINT );

	size_t paintId = insertPaint( paint );
	mCommands.push_back( paintId );

	size_t index = mCommands.size();
	mCommands.resize( mCommands.size() + sizeof( opacity ) / sizeof( uint32_t ) );
	memcpy( mCommands.data() + index, &opacity, sizeof( opacity ) );
}

void Svg::addFillCommand( GLuint pathId, const glm::mat3x2 &transform, GLuint clipMask, GLuint fillRule )
{
	addCommand( FILL_PATH, pathId, transform );
	mCommands.push_back( clipMask );
	mCommands.push_back( fillRule );
}

void Svg::addStrokeCommand( GLuint pathId, const glm::mat3x2 &transform, GLuint clipMask )
{
	addCommand( STROKE_PATH, pathId, transform );
	mCommands.push_back( clipMask );
}

const Path *Svg::findPath( size_t uuid ) const
{
	if( mPaths.count( uuid ) )
		return &mPaths.at( uuid );

	return nullptr;
}

const Path *Svg::insertPath( size_t uuid, const Path2d &path, bool isClipPath )
{
	if( !findPath( uuid ) ) {
		mPaths.insert_or_assign( uuid, Path( path ) );

		// Set path parameters.
		if( !isClipPath ) {
			auto &path = mPaths.at( uuid );
			path.setMiterLimit( mStacks.miterLimit.back() );
			path.setDashPattern( mStacks.dashArray.back() );
			path.setDashOffset( mStacks.dashOffset.back() );
			path.setEndCaps( toCapsStyle( mStacks.lineCap.back() ) );
			path.setDashCaps( toCapsStyle( mStacks.lineCap.back() ), toCapsStyle( mStacks.lineCap.back() ) );
			path.setJoinStyle( toJoinStyle( mStacks.lineJoin.back() ) );
			path.setStrokeWidth( mStacks.strokeWidth.back() );
		}
	}

	return &mPaths.at( uuid );
}

const Path *Svg::insertPath( size_t uuid, const Shape2d &shape, bool isClipPath )
{
	if( !findPath( uuid ) ) {
		mPaths.insert_or_assign( uuid, Path( shape ) );

		// Set path parameters.
		if( !isClipPath ) {
			auto &path = mPaths.at( uuid );
			path.setMiterLimit( mStacks.miterLimit.back() );
			path.setDashPattern( mStacks.dashArray.back() );
			path.setDashOffset( mStacks.dashOffset.back() );
			path.setEndCaps( toCapsStyle( mStacks.lineCap.back() ) );
			path.setDashCaps( toCapsStyle( mStacks.lineCap.back() ) );
			path.setJoinStyle( toJoinStyle( mStacks.lineJoin.back() ) );
			path.setStrokeWidth( mStacks.strokeWidth.back() );
		}
	}

	return &mPaths.at( uuid );
}

bool Svg::findPaint( const svg::Paint &paint, size_t &index ) const
{
	for( size_t i = 0; i < mPaints.size(); ++i ) {
		if( paint == mPaints.at( i ) ) {
			index = i;
			return true;
		}
	}

	return false;
}

size_t Svg::insertPaint( const svg::Paint &paint )
{
	size_t index;

	if( !findPaint( paint, index ) ) {
		index = mPaints.size();
		mPaints.push_back( paint );
	}

	return index;
}

void Svg::start()
{
	assert( nullptr == mCtx );

	// Clear commands.
	mCommands.clear();

	// Clear stacks.
	mStacks.defaults();

	// Keep track of OpenGL context.
	mCtx = gl::context();

	// Disable any shader.
	mCtx->pushGlslProg( nullptr );

	// Enable stencil buffer testing.
	mCtx->pushBoolState( GL_STENCIL_TEST, GL_TRUE );

	// Enable pre-multiplied alpha blending.
	mCtx->pushBoolState( GL_BLEND, GL_TRUE );
	mCtx->pushBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

	// Disable sRGB correct rendering.
	mCtx->pushBoolState( GL_FRAMEBUFFER_SRGB, GL_FALSE );

	// Bind gradient texture.
	mGradients.bind( mCtx, 0 );

	// Store current transformations.
	gl::pushModelView();
}

void Svg::finish()
{
	assert( gl::context() == mCtx );

	// Restore current transformations.
	gl::popModelView();

	// Unbind gradient texture.
	mGradients.unbind( mCtx );

	// Restore sRGB correct rendering.
	mCtx->popBoolState( GL_FRAMEBUFFER_SRGB );

	// Restore blending.
	mCtx->popBlendFuncSeparate();
	mCtx->popBoolState( GL_BLEND );

	// Restore stencil buffer testing.
	mCtx->popBoolState( GL_STENCIL_TEST );

	// Restore shader.
	mCtx->popGlslProg();

	// Done.
	mCtx = nullptr;
}

void Svg::clear()
{
	mGradients.clear();
	mTextures.clear();
	mPaths.clear();
	mPaints.clear();
	mCommands.clear();
}

void Svg::draw()
{
	if( mCommands.empty() )
		return;

	start();

	ShaderRef shader;

	size_t index = 0;
	while( index < mCommands.size() ) {
		const auto cmd = Cmd( mCommands.at( index++ ) );

		if( cmd == PREPARE_PAINT ) {
			const auto  paintId = mCommands.at( index++ );
			const float opacity = *reinterpret_cast<float *>( mCommands.data() + index );
			index += sizeof( float ) / sizeof( uint32_t );

			if( shader )
				shader->unbind();

			const auto &paint = mPaints.at( paintId );
			shader = Cache::loadShader( preparePaint( paint, opacity, true ) );
			shader->bind();

			ColorA solidColor = paint.getColor();
			solidColor.a *= opacity;
			shader->setColor( solidColor );
		}
		else {
			const auto        pathId = mCommands.at( index++ );
			const glm::mat3x2 transform = *reinterpret_cast<glm::mat3x2 *>( mCommands.data() + index );
			index += sizeof( glm::mat3x2 ) / sizeof( uint32_t );

			const auto clipMask = mCommands.at( index++ );
			GLuint     coverMask = clipMask - 1;

			gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
			gl::matrixMult3x2fNV( GL_MODELVIEW, value_ptr( transform ) );

			switch( cmd ) {
			case Cmd::PUSH_CLIP: {
				// Render shape to stencil buffer to use it as a clip-path.
				gl::ScopedColorMask   scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.
				gl::ScopedStencilMask scpStencilMask( coverMask | clipMask );                 // Don't write to previous clip bits.

				gl::stencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
				gl::stencilFunc( GL_NOTEQUAL, GLint( clipMask ), coverMask );

				const auto fillRule = mCommands.at( index++ );
				gl::stencilFillPathNV( pathId, GL_COUNT_UP_NV, coverMask & fillRule ); // Write path to LSB portion (step 1).

				gl::coverFillPathNV( pathId, GL_BOUNDING_BOX_NV ); // Convert LSB portion to clip bit (step 3).
			} break;
			case Cmd::POP_CLIP: {
				// Render shape to stencil buffer to use it as a clip-path.
				gl::ScopedColorMask   scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.
				gl::ScopedStencilMask scpStencilMask( coverMask | clipMask );                 // Don't write to previous clip bits.

				gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
				gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

				gl::coverFillPathNV( pathId, GL_BOUNDING_BOX_NV ); // Clear stencil buffer bits (step 7).
			} break;
			case Cmd::FILL_PATH: {
				const auto fillRule = mCommands.at( index++ );

				if( clipMask ) {
					// Render clipped path.
					gl::ScopedStencilMask scpStencilMask( coverMask | clipMask ); // Don't write to previous clip bits.

					GLuint mask = coverMask << 1 | 0x01;
					gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
					gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

					gl::stencilThenCoverFillPathNV( pathId, GL_COUNT_UP_NV, fillRule & coverMask, GL_BOUNDING_BOX_NV ); // (step 5).

					// Remove path from stencil buffer (step 6).
					gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

					gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
					gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

					gl::coverFillPathNV( pathId, GL_BOUNDING_BOX_NV );
				}
				else {
					gl::stencilFunc( GL_NOTEQUAL, 0, fillRule );
					gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

					gl::stencilThenCoverFillPathNV( pathId, GL_COUNT_UP_NV, fillRule, GL_BOUNDING_BOX_NV );
				}
			} break;
			case STROKE_PATH: {
				if( clipMask ) {
					// Render clipped path.
					gl::ScopedStencilMask scpStencilMask( coverMask | clipMask ); // Don't write to previous clip bits.

					GLuint mask = coverMask << 1 | 0x01;
					gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
					gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

					gl::stencilThenCoverStrokePathNV( pathId, GL_COUNT_UP_NV, coverMask, GL_BOUNDING_BOX_NV ); // (step 5).

					// Remove path from stencil buffer (step 6).
					gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

					gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
					gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

					gl::coverStrokePathNV( pathId, GL_BOUNDING_BOX_NV );
				}
				else {
					gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
					gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

					gl::stencilThenCoverStrokePathNV( pathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
				}
			} break;
			}
		}
	}

	if( shader )
		shader->unbind();

	finish();
}

void Svg::pushGroup( const svg::Group &group, float opacity )
{
	mStacks.groupOpacity.push_back( opacity );
}

void Svg::popGroup()
{
	mStacks.groupOpacity.pop_back();
}

void Svg::pushClipPath( const svg::ClipPath &clippath )
{
	assert( gl::context() == mCtx );

	if( mStacks.clipPath.size() > 5 )
		CI_LOG_W( "Maximum number of nested clip-paths reached! Results are undefined." );

	// Only render clip path if visible.
	if( clippath.isVisible() && !clippath.isDisplayNone() ) {
		// Store clip-path in cache.
		const Path *path = findPath( clippath.getUuid() );
		if( !path ) {
			// Obtain shape from clip-path, which is a merge of all shapes contained within.
			path = insertPath( clippath.getUuid(), clippath.getShape(), true );
		}

		/// <summary>
		/// Clip paths are handled as follows:
		///	  1. The path or group of paths are rendered to the stencil buffer using either non-zero or even-odd fill rule.
		///	  2. The lowest bits of the stencil are now set for all pixels that need to be covered.
		///	  3. We then cover the pixels without writing to the color buffer, replacing the stencil value with the highest bit (0x80) if the test is passed.
		///	  4. Repeat this for each nested clip path, but use the next highest bit (0x40, 0x20, etc.) instead.
		///	  5. When rendering the actual clipped content, render normally but only draw pixels if all clip bits are set.
		///	  6. We then reset the lowest clip bits by doing a cover with the appropriate stencil functions set.
		///	  7. At the end of each clip path, reset the corresponding clip bit.
		/// </summary>
		GLuint pathId = path->getId();
		GLuint clipMask = 0x80 >> mStacks.clipPath.size();
		GLuint coverMask = clipMask - 1;
		GLuint fillRule = mStacks.fillRule.back() == svg::FILL_RULE_EVEN_ODD ? 0x01 : 0xFF;

		// Render shape to stencil buffer to use it as a clip-path.
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::matrixMult3x3fNV( GL_MODELVIEW, value_ptr( mStacks.matrix.back() ) );

		gl::ScopedColorMask   scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.
		gl::ScopedStencilMask scpStencilMask( coverMask | clipMask );                 // Don't write to previous clip bits.

		gl::stencilOp( GL_KEEP, GL_KEEP, GL_REPLACE );
		gl::stencilFunc( GL_NOTEQUAL, GLint( clipMask ), coverMask );

		gl::stencilFillPathNV( pathId, GL_COUNT_UP_NV, coverMask & fillRule ); // Write path to LSB portion (step 1).

		gl::coverFillPathNV( pathId, GL_BOUNDING_BOX_NV ); // Convert LSB portion to clip bit (step 3).

		// Store in command buffer.
		addPushClipCommand( pathId, mStacks.matrix.back(), clipMask, fillRule );
	}

	//
	mStacks.clipPath.push_back( &clippath );
}

void Svg::popClipPath()
{
	assert( gl::context() == mCtx );
	assert( !mStacks.clipPath.empty() );

	// Generate draw call to reset the clip-path.
	const auto &clippath = *mStacks.clipPath.back();
	if( clippath.isVisible() && !clippath.isDisplayNone() ) {
		const Path *path = findPath( clippath.getUuid() );
		if( !path ) {
			__debugbreak(); // Path should already be cached!
		}

		//
		GLuint pathId = path->getId();
		GLuint clipMask = 0x80 >> ( mStacks.clipPath.size() - 1 );
		GLuint coverMask = clipMask - 1;

		// Render shape to stencil buffer to use it as a clip-path.
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::matrixMult3x3fNV( GL_MODELVIEW, value_ptr( mStacks.matrix.back() ) );

		gl::ScopedColorMask   scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.
		gl::ScopedStencilMask scpStencilMask( coverMask | clipMask );                 // Don't write to previous clip bits.

		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
		gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

		gl::coverFillPathNV( pathId, GL_BOUNDING_BOX_NV ); // Clear stencil buffer bits (step 7).

		// Store in command buffer.
		addPopClipCommand( pathId, mStacks.matrix.back(), clipMask );
	}

	//
	mStacks.clipPath.pop_back();
}

void Svg::drawPath( const svg::Path &path )
{
	assert( gl::context() == mCtx );

	const Path *ptr = findPath( path.getUuid() );
	if( !ptr ) {
		ptr = insertPath( path.getUuid(), path.getShape() );
	}

	render( *ptr );
}

void Svg::drawPolyline( const svg::Polyline &polyline )
{
	assert( gl::context() == mCtx );

	const Path *ptr = findPath( polyline.getUuid() );
	if( !ptr ) {
		ptr = insertPath( polyline.getUuid(), polyline.getShape() );
	}

	render( *ptr );
}

void Svg::drawPolygon( const svg::Polygon &polygon )
{
	assert( gl::context() == mCtx );

	const Path *ptr = findPath( polygon.getUuid() );
	if( !ptr ) {
		ptr = insertPath( polygon.getUuid(), polygon.getShape() );
	}

	render( *ptr );
}

void Svg::drawLine( const svg::Line &line )
{
	assert( gl::context() == mCtx );

	const Path *ptr = findPath( line.getUuid() );
	if( !ptr ) {
		ptr = insertPath( line.getUuid(), line.getShape() );
	}

	render( *ptr );
}

void Svg::drawRect( const svg::Rect &rect )
{
	assert( gl::context() == mCtx );

	const Path *ptr = findPath( rect.getUuid() );
	if( !ptr ) {
		ptr = insertPath( rect.getUuid(), rect.getShape() );
	}

	render( *ptr );
}

void Svg::drawCircle( const svg::Circle &circle )
{
	assert( gl::context() == mCtx );

	const Path *ptr = findPath( circle.getUuid() );
	if( !ptr ) {
		ptr = insertPath( circle.getUuid(), circle.getShape() );
	}

	render( *ptr );
}

void Svg::drawEllipse( const svg::Ellipse &ellipse )
{
	assert( gl::context() == mCtx );

	const Path *ptr = findPath( ellipse.getUuid() );
	if( !ptr ) {
		ptr = insertPath( ellipse.getUuid(), ellipse.getShape() );
	}

	render( *ptr );
}

void Svg::drawImage( const svg::Image &image )
{
	assert( gl::context() == mCtx );

	if( !shouldRender() )
		return;

	const Path *ptr = findPath( image.getUuid() );
	if( !ptr ) {
		ptr = insertPath( image.getUuid(), Path2d::rectangle( image.getRect() ) );
	}

	//
	GLuint pathId = ptr->getId();
	GLuint fillRule = mStacks.fillRule.back() == svg::FILL_RULE_EVEN_ODD ? 0x01 : 0xFF;

	// Obtain image texture.
	if( !mTextures.count( image.getUuid() ) ) {
		auto svg = image.getSvg();
		if( svg ) {
			// Render embedded SVG to texture.
			Svg renderer;

			gl::pushModelMatrix();
			gl::setModelMatrix( mat4() );

			Canvas canvas( int( svg->getWidth() ), int( svg->getHeight() ), 8, 16 );
			canvas.bind();
			svg->render( renderer );
			canvas.unbind();

			gl::popModelMatrix();

			auto texture = canvas.getTexture();
			if( texture )
				mTextures.insert_or_assign( image.getUuid(), texture );
		}
		else {
			auto surface = image.getSurface();
			if( surface )
				mTextures.insert_or_assign( image.getUuid(), gl::Texture2d::create( *surface, gl::Texture2d::Format().loadTopDown( false ) ) );
		}
	}

	const auto &texture = mTextures.at( image.getUuid() );

	// Render image.
	gl::ScopedTextureBind scpImage( texture, 2 );
	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::matrixMult3x3fNV( GL_MODELVIEW, value_ptr( mStacks.matrix.back() ) );

	ScopedShader scpShader( Shader::Type::IMAGE );
	scpShader.setColor( ColorA::white() );
	scpShader.setCoords( GL_PATH_OBJECT_BOUNDING_BOX_NV /* TODO support userSpaceOnUse */, image.getTextureMatrix() );
	scpShader.uniform( "image", 2 );
	scpShader.uniform( "opacity", image.getOpacity() );

	if( !mStacks.clipPath.empty() ) {
		// Render clipped image.
		GLuint clipMask = 0x80 >> mStacks.clipPath.size();
		GLuint coverMask = clipMask - 1;

		gl::ScopedStencilMask scpStencilMask( coverMask | clipMask ); // Don't write to previous clip bits.

		GLuint mask = coverMask << 1 | 0x01;
		gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

		gl::stencilThenCoverFillPathNV( pathId, GL_COUNT_UP_NV, fillRule & coverMask, GL_BOUNDING_BOX_NV ); // (step 5).

		// Remove shape from stencil buffer (step 6).
		gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
		gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

		gl::coverFillPathNV( pathId, GL_BOUNDING_BOX_NV );
	}
	else {
		gl::stencilFunc( GL_NOTEQUAL, 0, fillRule );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		gl::stencilThenCoverFillPathNV( pathId, GL_COUNT_UP_NV, fillRule, GL_BOUNDING_BOX_NV );
	}
}

void Svg::drawTextSpan( const svg::TextSpan &textSpan )
{
	// assert( gl::context() == mCtx );

	// const auto font = textSpan.getFont();
	// if( font ) {
	//	// Construct text.
	//	text::AttrString text;
	//	text << font << textSpan.getString();

	//	text::Frame                layout( text );
	//	text::Typesetter::Iterator iter = layout.getIterator();

	//	// Calculate origin.
	//	vec2 offset{ 0, -font->getAscender() };
	//	measureString( text, &offset.x );

	//	switch( textSpan.getTextAnchor() ) {
	//	case svg::TEXT_ANCHOR_START:
	//		offset.x *= 0.0f;
	//		break;
	//	case svg::TEXT_ANCHOR_MIDDLE:
	//		offset.x *= -0.5f;
	//		break;
	//	case svg::TEXT_ANCHOR_END:
	//		offset.x *= -1.0f;
	//		break;
	//	}

	//	offset += mStacks.textPen.back();

	//	// Render text as instanced paths.
	//	std::vector<glm::mat3x2> transforms;

	//	vec2 lineDrawOffset;

	//	const text::Run *runPtr;
	//	while( layout.nextRun( iter, &runPtr, &lineDrawOffset ) ) {
	//		if( runPtr->isPlaceholder() )
	//			continue;

	//		// Cache the font face.
	//		auto face = Cache::loadFace( runPtr->getFont()->getFace() );

	//		// Create transforms.
	//		transforms.clear();
	//		transforms.reserve( runPtr->getNumGlyphs() );

	//		const auto origin = offset + lineDrawOffset + runPtr->getDrawOffset();
	//		const auto scale = runPtr->getFont()->getSize() / Face::DEFAULT_SIZE;
	//		const auto positions = runPtr->getGlyphPositions();
	//		const auto orientations = runPtr->getGlyphOrientations();
	//		const auto indices = runPtr->getGlyphIndices();

	//		for( size_t i = 0; i < runPtr->getNumGlyphs(); ++i ) {
	//			const auto position = origin + positions[i];
	//			const auto normal = orientations ? scale * orientations[i] : vec2( 0, -scale );
	//			transforms.emplace_back( -normal.y, normal.x, normal.x, normal.y, position.x, position.y );

	//			if( !mStacks.stroke.back().isNone() ) {
	//				GLuint pathId = face->getBaseId() + indices[i];
	//				//  gl::pathDashArrayNV( pathId, static_cast<GLsizei>( pattern.size() ), pattern.data() );
	//				//  gl::pathParameterfNV( pathId, GL_PATH_DASH_OFFSET_NV, mStacks.dashOffset.back() );
	//				//  gl::pathParameteriNV( pathId, GL_PATH_DASH_CAPS_NV, GLint( mStacks.lineCap.back() ) );
	//				//  gl::pathParameteriNV( pathId, GL_PATH_INITIAL_DASH_CAP_NV, GLint( mStacks.lineCap.back() ) );
	//				//  gl::pathParameteriNV( pathId, GL_PATH_TERMINAL_DASH_CAP_NV, GLint( mStacks.lineCap.back() ) );
	//				gl::pathParameteriNV( pathId, GL_PATH_END_CAPS_NV, GLint( toCapsStyle( mStacks.lineCap.back() ) ) );
	//				gl::pathParameteriNV( pathId, GL_PATH_JOIN_STYLE_NV, GLint( toJoinStyle( mStacks.lineJoin.back() ) ) );
	//				gl::pathParameterfNV( pathId, GL_PATH_STROKE_WIDTH_NV, GLfloat( mStacks.strokeWidth.back() * Face::DEFAULT_SIZE / runPtr->getFont()->getSize() ) );
	//				gl::pathParameterfNV( pathId, GL_PATH_MITER_LIMIT_NV, GLfloat( mStacks.miterLimit.back() ) );
	//			}
	//		}

	//		if( !mStacks.fill.back().isNone() )
	//			fillText( face->getBaseId(), transforms.size(), transforms.data(), indices, mStacks.fill.back(), mStacks.fillOpacity.back() * mStacks.groupOpacity.back() );
	//		if( !mStacks.stroke.back().isNone() )
	//			strokeText( face->getBaseId(), transforms.size(), transforms.data(), indices, mStacks.stroke.back(), mStacks.strokeOpacity.back() * mStacks.groupOpacity.back() );
	//	}
	//}
}

void Svg::pushMatrix( const mat3 &m )
{
	mStacks.matrix.push_back( mStacks.matrix.back() * m );
}

void Svg::popMatrix()
{
	mStacks.matrix.pop_back();
}

void Svg::pushFill( const svg::Paint &paint )
{
	mStacks.fill.push_back( paint );

	// Render gradients to texture.
	preparePaint( paint );
}

void Svg::popFill()
{
	mStacks.fill.pop_back();
}

void Svg::pushStroke( const svg::Paint &paint )
{
	mStacks.stroke.push_back( paint );

	// Render gradients to texture.
	preparePaint( paint );
}

void Svg::popStroke()
{
	mStacks.stroke.pop_back();
}

void Svg::pushFillOpacity( float opacity )
{
	mStacks.fillOpacity.push_back( opacity );
}

void Svg::popFillOpacity()
{
	mStacks.fillOpacity.pop_back();
}

void Svg::pushStrokeOpacity( float opacity )
{
	mStacks.strokeOpacity.push_back( opacity );
}

void Svg::popStrokeOpacity()
{
	mStacks.strokeOpacity.pop_back();
}

void Svg::pushStrokeWidth( float x )
{
	mStacks.strokeWidth.push_back( x );
}

void Svg::popStrokeWidth()
{
	mStacks.strokeWidth.pop_back();
}

void Svg::pushFillRule( svg::FillRule fillRule )
{
	mStacks.fillRule.push_back( fillRule );
}

void Svg::popFillRule()
{
	mStacks.fillRule.pop_back();
}

void Svg::pushLineCap( svg::LineCap lineCap )
{
	mStacks.lineCap.push_back( lineCap );
}

void Svg::popLineCap()
{
	mStacks.lineCap.pop_back();
}

void Svg::pushLineJoin( svg::LineJoin lineJoin )
{
	mStacks.lineJoin.push_back( lineJoin );
}

void Svg::popLineJoin()
{
	mStacks.lineJoin.pop_back();
}

void Svg::pushMiterLimit( float miterLimit )
{
	mStacks.miterLimit.push_back( miterLimit );
}

void Svg::popMiterLimit()
{
	mStacks.miterLimit.pop_back();
}

void Svg::pushDashArray( const std::vector<float> &dashArray )
{
	mStacks.dashArray.push_back( dashArray );
}

void Svg::popDashArray()
{
	mStacks.dashArray.pop_back();
}

void Svg::pushDashOffset( float dashOffset )
{
	mStacks.dashOffset.push_back( dashOffset );
}

void Svg::popDashOffset()
{
	mStacks.dashOffset.pop_back();
}

void Svg::pushTextPen( const vec2 &vec2 )
{
	mStacks.textPen.push_back( vec2 );
}

void Svg::popTextPen()
{
	mStacks.textPen.pop_back();
}

void Svg::pushTextRotation( float x )
{
	mStacks.textRotation.push_back( x );
}

void Svg::popTextRotation()
{
	mStacks.textRotation.pop_back();
}

void Svg::render( const Path &path )
{
	if( !shouldRender() )
		return;

	if( !mStacks.fill.back().isNone() ) {
		// Vertical and horizontal lines don't have a bounding box, since they are one-dimensional,
		// even though the stroke-width makes it look like they should have a bounding box with non-zero width and height.
		if( mStacks.fill.back().fallback() && approxZero( path.getFillBounds().calcArea() ) ) {
			if( !mStacks.fill.back().fallback()->isNone() )
				fill( path.getId(), *mStacks.fill.back().fallback(), mStacks.fillOpacity.back() * mStacks.groupOpacity.back() );
		}
		else
			fill( path.getId(), mStacks.fill.back(), mStacks.fillOpacity.back() * mStacks.groupOpacity.back() );
	}
	if( !mStacks.stroke.back().isNone() ) {
		// Vertical and horizontal lines don't have a bounding box, since they are one-dimensional,
		// even though the stroke-width makes it look like they should have a bounding box with non-zero width and height.
		if( mStacks.stroke.back().fallback() && approxZero( path.getFillBounds().calcArea() ) ) {
			if( !mStacks.stroke.back().fallback()->isNone() )
				stroke( path.getId(), *mStacks.stroke.back().fallback(), mStacks.strokeOpacity.back() * mStacks.groupOpacity.back() );
		}
		else
			stroke( path.getId(), mStacks.stroke.back(), mStacks.strokeOpacity.back() * mStacks.groupOpacity.back() );
	}
}

void Svg::fill( GLuint pathId, const svg::Paint &paint, float opacity )
{
	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::matrixMult3x3fNV( GL_MODELVIEW, value_ptr( mStacks.matrix.back() ) );

	GLuint fillRule = mStacks.fillRule.back() == svg::FILL_RULE_EVEN_ODD ? 0x01 : 0xFF;

	//
	ScopedShader scpShader( preparePaint( paint, opacity, true ) );

	ColorA solidColor = paint.getColor();
	solidColor.a *= opacity;
	scpShader.setColor( solidColor );

	//
	if( !mStacks.clipPath.empty() ) {
		// Render clipped path.
		GLuint clipMask = 0x80 >> mStacks.clipPath.size();
		GLuint coverMask = clipMask - 1;

		gl::ScopedStencilMask scpStencilMask( coverMask | clipMask ); // Don't write to previous clip bits.

		GLuint mask = coverMask << 1 | 0x01;
		gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

		gl::stencilThenCoverFillPathNV( pathId, GL_COUNT_UP_NV, fillRule & coverMask, GL_BOUNDING_BOX_NV ); // (step 5).

		// Remove path from stencil buffer (step 6).
		gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
		gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

		gl::coverFillPathNV( pathId, GL_BOUNDING_BOX_NV );

		//
		addPreparePaintCommand( paint, opacity );
		addFillCommand( pathId, mStacks.matrix.back(), clipMask, fillRule );
	}
	else {
		gl::stencilFunc( GL_NOTEQUAL, 0, fillRule );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		gl::stencilThenCoverFillPathNV( pathId, GL_COUNT_UP_NV, fillRule, GL_BOUNDING_BOX_NV );

		//
		addPreparePaintCommand( paint, opacity );
		addFillCommand( pathId, mStacks.matrix.back(), 0x00, fillRule );
	}
}

void Svg::stroke( GLuint pathId, const svg::Paint &paint, float opacity )
{
	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::matrixMult3x3fNV( GL_MODELVIEW, value_ptr( mStacks.matrix.back() ) );

	//
	ScopedShader scpShader( preparePaint( paint, opacity, true ) );

	ColorA solidColor = paint.getColor();
	solidColor.a *= opacity;
	scpShader.setColor( solidColor );

	if( !mStacks.clipPath.empty() ) {
		// Render clipped path.
		GLuint clipMask = 0x80 >> mStacks.clipPath.size();
		GLuint coverMask = clipMask - 1;

		gl::ScopedStencilMask scpStencilMask( coverMask | clipMask ); // Don't write to previous clip bits.

		GLuint mask = coverMask << 1 | 0x01;
		gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

		gl::stencilThenCoverStrokePathNV( pathId, GL_COUNT_UP_NV, coverMask, GL_BOUNDING_BOX_NV ); // (step 5).

		// Remove path from stencil buffer (step 6).
		gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
		gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

		gl::coverStrokePathNV( pathId, GL_BOUNDING_BOX_NV );

		//
		addPreparePaintCommand( paint, opacity );
		addStrokeCommand( pathId, mStacks.matrix.back(), clipMask );
	}
	else {
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		gl::stencilThenCoverStrokePathNV( pathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );

		//
		addPreparePaintCommand( paint, opacity );
		addStrokeCommand( pathId, mStacks.matrix.back(), 0x00 );
	}
}

void Svg::fillText( GLuint baseId, GLsizei count, const glm::mat3x2 *transforms, const uint32_t *indices, const svg::Paint &paint, float opacity )
{
	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::matrixMult3x3fNV( GL_MODELVIEW, value_ptr( mStacks.matrix.back() ) );

	//
	GLuint fillRule = mStacks.fillRule.back() == svg::FILL_RULE_EVEN_ODD ? 0x01 : 0xFF;

	//
	ScopedShader scpShader( preparePaint( paint, opacity, true ) );

	ColorA solidColor = paint.getColor();
	solidColor.a *= opacity;
	scpShader.setColor( solidColor );

	//
	if( !mStacks.clipPath.empty() ) {
		// Render clipped text.
		GLuint clipMask = 0x80 >> mStacks.clipPath.size();
		GLuint coverMask = clipMask - 1;

		gl::ScopedStencilMask scpStencilMask( coverMask | clipMask ); // Don't write to previous clip bits.

		GLuint mask = coverMask << 1 | 0x01;
		gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

		// (step 5).
		gl::stencilThenCoverFillPathInstancedNV( count, GL_UNSIGNED_INT, indices, baseId, GL_PATH_FILL_MODE_NV, fillRule & coverMask, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms ) );

		// Remove text from stencil buffer (step 6).
		gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
		gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

		gl::stencilThenCoverFillPathInstancedNV( count, GL_UNSIGNED_INT, indices, baseId, GL_PATH_FILL_MODE_NV, fillRule & coverMask, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms ) );
	}
	else {
		gl::stencilFunc( GL_NOTEQUAL, 0, fillRule );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		gl::stencilThenCoverFillPathInstancedNV( count, GL_UNSIGNED_INT, indices, baseId, GL_PATH_FILL_MODE_NV, fillRule, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms ) );
	}
}

void Svg::strokeText( GLuint baseId, GLsizei count, const glm::mat3x2 *transforms, const uint32_t *indices, const svg::Paint &paint, float opacity )
{
	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::matrixMult3x3fNV( GL_MODELVIEW, value_ptr( mStacks.matrix.back() ) );

	//
	ScopedShader scpShader( preparePaint( paint, opacity, true ) );

	ColorA solidColor = paint.getColor();
	solidColor.a *= opacity;
	scpShader.setColor( solidColor );

	//
	if( !mStacks.clipPath.empty() ) {
		// Render clipped text.
		GLuint clipMask = 0x80 >> mStacks.clipPath.size();
		GLuint coverMask = clipMask - 1;

		gl::ScopedStencilMask scpStencilMask( coverMask | clipMask ); // Don't write to previous clip bits.

		GLuint mask = coverMask << 1 | 0x01;
		gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

		// (step 5).
		gl::stencilThenCoverStrokePathInstancedNV( count, GL_UNSIGNED_INT, indices, baseId, GL_PATH_FILL_MODE_NV, coverMask, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms ) );

		// Remove text from stencil buffer (step 6).
		gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
		gl::stencilFunc( GL_ALWAYS, GLint( clipMask ), coverMask );

		gl::stencilThenCoverStrokePathInstancedNV( count, GL_UNSIGNED_INT, indices, baseId, GL_PATH_FILL_MODE_NV, coverMask, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms ) );
	}
	else {
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		gl::stencilThenCoverStrokePathInstancedNV( count, GL_UNSIGNED_INT, indices, baseId, GL_PATH_FILL_MODE_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms ) );
	}
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
			  //"    fragColor.rgb = mix( color.rgb, fragColor.rgb, fragColor.a );"
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
			  //"    fragColor.rgb = mix( color.rgb, fragColor.rgb, fragColor.a );"
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
			  //"    fragColor.rgb = mix( color.rgb, fragColor.rgb, fragColor.a );"
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
			  //"    fragColor.rgb = mix( color.rgb, fragColor.rgb, fragColor.a );"
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

// FaceRef Cache::loadFace( const text::Face *face )
//{
//	Cache &self = get();
//
//	const auto &name = face->getFamilyName();
//
//	if( self.mFaces.count( name ) && static_cast<bool>( self.mFaces.at( name ) ) ) {
//		// CI_LOG_V( "Using cached face for " << name << " (" << std::this_thread::get_id() << ")" );
//		return self.mFaces.at( name );
//	}
//
//	CI_LOG_V( "Loading face for " << name << " (" << std::this_thread::get_id() << ")" );
//
//	auto cached = Face::create( face );
//	self.mFaces.insert_or_assign( name, cached );
//
//	return cached;
// }

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
	// auto &fonts = get().mFaces;
	// for( auto itr = fonts.begin(); itr != fonts.end(); ) {
	//	const auto &item = *itr;
	//	if( item.second.use_count() < 2 ) {
	//		CI_LOG_V( "Removing face " << item.first << " (" << std::this_thread::get_id() << ")" );
	//		itr = fonts.erase( itr );
	//	}
	//	else
	//		++itr;
	// }

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
	// CI_LOG_V( "Removing all faces (" << std::this_thread::get_id() << ")" );
	// get().mFaces.clear();
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

// void renderText( const text::Typesetter &typesetter, const vec2 &offset )
//{
//	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
//
//	std::vector<glm::mat3x2> transforms;
//
//	text::Typesetter::Iterator iter = typesetter.getIterator();
//	const text::Run           *runPtr;
//	vec2                       lineDrawOffset;
//	while( typesetter.nextRun( iter, &runPtr, &lineDrawOffset ) ) {
//		if( runPtr->isPlaceholder() )
//			continue;
//
//		// Cache the font face.
//		auto face = Cache::loadFace( runPtr->getFont()->getFace() );
//
//		// Create transforms.
//		transforms.clear();
//		transforms.reserve( runPtr->getNumGlyphs() );
//
//		const auto origin = offset + lineDrawOffset + runPtr->getDrawOffset();
//		const auto scale = runPtr->getFont()->getSize();
//		const auto positions = runPtr->getGlyphPositions();
//		const auto orientations = runPtr->getGlyphOrientations();
//		const auto indices = runPtr->getGlyphIndices();
//
//		for( size_t i = 0; i < runPtr->getNumGlyphs(); ++i ) {
//			const auto position = origin + positions[i];
//			const auto normal = orientations ? scale * orientations[i] : vec2( 0, -scale );
//			transforms.emplace_back( -normal.y, normal.x, normal.x, normal.y, position.x, position.y );
//		}
//
//		// Draw immediately.
//		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
//		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
//		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
//
//		ScopedShader scpShader( runPtr->getColor().premultiplied() );
//		gl::stencilThenCoverFillPathInstancedNV(
//			static_cast<GLsizei>( transforms.size() ), GL_UNSIGNED_INT, indices, face->getBaseId(), GL_PATH_FILL_MODE_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
//	}
// }

CoordinateSpace toGradientUnits( std::string style )
{
	style = trim( style );
	if( style == "userSpaceOnUse" )
		return CoordinateSpace::USER_SPACE_ON_USE;
	return CoordinateSpace::DEFAULT;
}

SpreadMethod toSpreadMethod( std::string style )
{
	style = trim( toLower( style ) );
	if( style == "reflect" )
		return SpreadMethod::REFLECT;
	if( style == "repeat" )
		return SpreadMethod::REPEAT;
	return SpreadMethod::DEFAULT;
}

} // namespace nvp
} // namespace cinder