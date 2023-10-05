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
#include "cinder/text/Text.h"

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

Rectf Path::getBounds() const
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
	if( mPathId > 0 ) {
		gl::pathParameteriNV( mPathId, GL_PATH_DASH_CAPS_NV, GLint( caps ) );
	}
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
	if( mPathId > 0 ) {
		gl::pathParameteriNV( mPathId, GL_PATH_END_CAPS_NV, GLint( caps ) );
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
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverStrokePathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

void Path::strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, bool clearStencil )
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverStrokePathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_3D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

void Path::fill( const ColorA &color, bool clearStencil ) const
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
}

void Path::fill( const gl::TextureRef &texture, const Rectf &bounds, bool clearStencil ) const
{
	const auto textureBounds = Rectf( texture->getBounds() );
	const auto fit = bounds.getCenteredFit( textureBounds, true ).scaled( 1.0f / textureBounds.getSize() );

	const auto pathBounds = getBounds();

	auto normalized = Rectf( pathBounds.getUpperLeft() - bounds.getUpperLeft(), pathBounds.getLowerRight() - bounds.getUpperLeft() ).scaled( fit.getSize() / bounds.getSize() );
	normalized.offset( fit.getUpperLeft() );

	const auto topDown = texture->isTopDown();
	const auto upperLeftTexCoord = vec2{ normalized.x1, topDown ? normalized.y1 : 1.0f - normalized.y1 };
	const auto lowerRightTexCoord = vec2{ normalized.x2, topDown ? normalized.y2 : 1.0f - normalized.y2 };

	fill( texture, upperLeftTexCoord, lowerRightTexCoord, clearStencil );
}

void Path::fill( const gl::TextureRef &texture, const vec2 &upperLeftTexCoord, const vec2 &lowerRightTexCoord, bool clearStencil ) const
{
	if( mPathId > 0 ) {
		const GLfloat data[6] = { lowerRightTexCoord.x - upperLeftTexCoord.x, 0, upperLeftTexCoord.x, 0, lowerRightTexCoord.y - upperLeftTexCoord.y, upperLeftTexCoord.y };

		gl::ScopedTextureBind scpTex( texture, 0 );
		gl::ScopedState       scpState( GL_TEXTURE_2D, GL_TRUE );
		gl::pathTexGenNV( GL_TEXTURE0, GL_PATH_OBJECT_BOUNDING_BOX_NV, 2, &data[0] );

		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

		ScopedShader scpShader( gl::context()->getCurrentColor().premultiplied() ); // TODO: texture shader

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

Face::Face( const text::Face *face )
	: mFace( face )
{
	mNumGlyphs = mFace->getNumGlyphs();
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

	if( !( glyph.commands.empty() || glyph.coords.empty() ) )
		gl::pathCommandsNV( glyph.id, GLsizei( glyph.commands.size() ), glyph.commands.data(), GLsizei( glyph.coords.size() ), GL_FLOAT, glyph.coords.data() );

	glyph.id++;
	glyph.commands.clear();
	glyph.coords.clear();
}

} // namespace

void Face::createPaths() const
{
	text::Face::OutlineFunctions functions;
	functions.moveTo = moveTo;
	functions.lineTo = lineTo;
	functions.quadTo = quadTo;
	functions.cubicTo = cubicTo;
	functions.restart = restart;

	Glyph glyph;
	glyph.id = mBaseId;
	glyph.scale = 1.0f / float( mFace->getUnitsPerEm() );

	mFace->getGlyphOutlines( 0, mNumGlyphs, functions, &glyph );
}

ColorA8u Gradient::at( float t ) const
{
	auto &lo = floor( t );
	auto &hi = ceil( t );

	if( lo == hi )
		return lo.color();

	float f = clamp( ( t - lo.offset() ) / ( hi.offset() - lo.offset() ), 0.0f, 1.0f );
	return lo.color().lerp( static_cast<unsigned char>( f * 255 ), hi.color() );
}

const Gradient::Stop &Gradient::floor( float t ) const
{
	if( mStops.empty() ) {
		static Stop kEmpty{ 0.0f, ColorA8u( 0, 0, 0, 0 ) };
		return kEmpty;
	}

	for( auto itr = mStops.rbegin(); itr != mStops.rend(); ++itr ) {
		if( itr->offset() <= t )
			return *itr;
	}

	return mStops.front();
}

const Gradient::Stop &Gradient::ceil( float t ) const
{
	if( mStops.empty() ) {
		static Stop kEmpty{ 0.0f, ColorA8u( 0, 0, 0, 0 ) };
		return kEmpty;
	}

	for( auto itr = mStops.begin(); itr != mStops.end(); ++itr ) {
		if( itr->offset() > t )
			return *itr;
	}

	return mStops.back();
}

Gradient &Gradient::stop( float t, const ColorA8u &color )
{
	insert( t, color );
	return *this;
}

Gradient &Gradient::stop( float t, unsigned char r, unsigned char g, unsigned char b, unsigned char a )
{
	insert( t, ColorA8u{ r, g, b, a } );
	return *this;
}

Gradient &Gradient::stop( float t, float r, float g, float b, float a )
{
	insert( t, ColorA8u{ static_cast<unsigned char>( r * 255 ), static_cast<unsigned char>( g * 255 ), static_cast<unsigned char>( b * 255 ), static_cast<unsigned char>( a * 255 ) } );
	return *this;
}

void Gradient::insert( const Stop &stop )
{
	// Keep sorted.
	mStops.insert( std::upper_bound( mStops.begin(), mStops.end(), stop ), stop );
}

void Gradient::add( const Gradient &other )
{
	for( const auto &stop : other.mStops )
		insert( stop );
}

std::unique_ptr<uint8_t[]> Gradient::data( int32_t width, int32_t height, float from, float to ) const
{
	auto result = std::make_unique<uint8_t[]>( size_t( width ) * size_t( height ) * sizeof( ColorA8u ) );

	for( int y = 0; y < height; ++y ) {
		for( int x = 0; x < width; ++x ) {
			float t = mix( from, to, float( x ) / float( width - 1 ) );

			const auto    c = at( t );
			const int64_t i = ( int64_t( x ) + int64_t( y ) * int64_t( width ) ) * sizeof( ColorA8u );
			result[size_t( i ) + 0] = uint8_t( c.r );
			result[size_t( i ) + 1] = uint8_t( c.g );
			result[size_t( i ) + 2] = uint8_t( c.b );
			result[size_t( i ) + 3] = uint8_t( c.a );
		}
	}

	return result;
}

LinearGradient::LinearGradient( const GradientRef &other )
{
	if( other ) {
		if( const auto linear = std::dynamic_pointer_cast<LinearGradient>( other ) )
			*this << *linear; // Copy all attributes.
		else
			replace( *other ); // Only copy stops.
	}
}

LinearGradient &LinearGradient::operator<<( const LinearGradient &other )
{
	add( other );
	mUnits = other.mUnits;
	mSpread = other.mSpread;
	mX1 = other.mX1;
	mY1 = other.mY1;
	mX2 = other.mX2;
	mY2 = other.mY2;
	return *this;
}

RadialGradient::RadialGradient( const GradientRef &other )
{
	if( other ) {
		if( const auto radial = std::dynamic_pointer_cast<RadialGradient>( other ) )
			*this << *radial; // Copy all attributes.
		else
			replace( *other ); // Only copy stops.
	}
}

RadialGradient &RadialGradient::operator<<( const RadialGradient &other )
{
	add( other );
	mUnits = other.mUnits;
	mSpread = other.mSpread;
	mR = other.mR;
	mCx = other.mCx;
	mCy = other.mCy;
	mFr = other.mFr;
	mFx = other.mFx;
	mFy = other.mFy;
	return *this;
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

const GradientRef &Gradients::at( const std::string &id ) const
{
	if( mLookUp.count( id ) )
		return mGradients.at( mLookUp.at( id ) );

	static const GradientRef kEmpty;
	return kEmpty;
}

float Gradients::index( const std::string &id ) const
{
	if( !mTexture || !mLookUp.count( id ) )
		return 0.0f;

	return ( float( mLookUp.at( id ) ) + 0.5f ) / float( mTexture->getHeight() );
}

void Gradients::set( const GradientRef &gradient )
{
	if( !mLookUp.count( gradient->getId() ) ) {
		mLookUp.insert_or_assign( gradient->getId(), mGradients.size() );
		mDirty.insert( mGradients.size() );
		mGradients.emplace_back( gradient );
	}
	else if( auto index = mLookUp.at( gradient->getId() ); *mGradients.at( index ) != *gradient ) {
		mDirty.insert( index );
		mGradients.at( index ) = gradient;
	}
}

gl::Texture2dRef Gradients::getTexture() const
{
	if( !mTexture || !mDirty.empty() ) {
		// Resize texture if more space is needed.
		if( const auto size = glm::max( 128, int( mGradients.size() ) ); !mTexture || mTexture->getHeight() < size ) {
			const auto texture = create( 128, int( isPowerOf2( size ) ? size : nextPowerOf2( size ) ) );

			if( mTexture ) {
				if( glad_glCopyImageSubData ) {
					glCopyImageSubData(                                       //
						mTexture->getId(), mTexture->getTarget(), 0, 0, 0, 0, // Src
						texture->getId(), texture->getTarget(), 0, 0, 0, 0,   // Dst
						mTexture->getWidth(), mTexture->getHeight(), 0 );     // Size
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

			mTexture = texture;
		}

		// Update gradients.
		for( const auto index : mDirty ) {
			const auto data = mGradients.at( index )->data( 128, 1 );
			mTexture->update( data.get(), GL_RGBA, GL_UNSIGNED_BYTE, 0, 128, 1, { 0, index } );
		}

		// Done.
		mDirty.clear();
	}

	return mTexture;
}

gl::Texture2dRef Gradients::create( int width, int height ) const
{
	static const gl::Texture2d::Format FORMAT = gl::Texture2d::Format().internalFormat( GL_RGBA ).target( GL_TEXTURE_2D ).loadTopDown();

	gl::Texture2dRef texture = gl::Texture2d::create( width, height, FORMAT );
	return texture;
}

Svg::Svg( const DataSourceRef &src )
	: Svg( svg::Doc::create( src ) )
{
}

Svg::Svg( const svg::DocRef &svg )
	: mBounds( svg->getBounds() )
{
	svg->render( *this );
}

Shader::Type Svg::prepareLinearGradient( const svg::Paint &paint, float opacity, bool prepareShader )
{
	auto gradient = std::dynamic_pointer_cast<LinearGradient>( mGradients.at( paint.getId() ) );
	if( !gradient ) {
		gradient = LinearGradient::create( paint.getId().c_str() );
		gradient->units( paint.mUseObjectBoundingBox ? CoordinateSpace::OBJECT_BOUNDING_BOX : CoordinateSpace::USER_SPACE_ON_USE ); //
		gradient->transform( paint.getTransform() );
		gradient->from( paint.getCoords0() );
		gradient->to( paint.getCoords1() );
		for( size_t i = 0; i < paint.getNumColors(); ++i )
			gradient->stop( paint.getOffset( i ), paint.getColor( i ) );

		mGradients.set( gradient );
	}

	if( prepareShader ) {
		ScopedShader scpShader( Shader::Type::LINEAR_GRADIENT );
		scpShader.setCoords( GLenum( gradient->getUnits() ), mat3{ gradient->getTransform() } );
		scpShader.uniform( "index", mGradients.index( paint.getId() ) );
		scpShader.uniform( "gradTab", 0 );
		scpShader.uniform( "gradStart", vec2( gradient->getX1(), gradient->getY1() ) );
		scpShader.uniform( "gradEnd", vec2( gradient->getX2(), gradient->getY2() ) );
		scpShader.uniform( "opacity", opacity );
	}

	return Shader::Type::LINEAR_GRADIENT;
}

Shader::Type Svg::prepareRadialGradient( const svg::Paint &paint, float opacity, bool prepareShader )
{
	auto gradient = std::dynamic_pointer_cast<RadialGradient>( mGradients.at( paint.getId() ) );
	if( !gradient ) {
		gradient = RadialGradient::create( paint.getId().c_str() );
		gradient->units( paint.useObjectBoundingBox() ? CoordinateSpace::OBJECT_BOUNDING_BOX : CoordinateSpace::USER_SPACE_ON_USE ); //
		gradient->transform( paint.getTransform() );
		gradient->center( paint.getCoords0() );
		gradient->radius( paint.getRadius0() );
		gradient->focal( paint.getCoords1(), paint.getRadius1() );
		for( size_t i = 0; i < paint.getNumColors(); ++i )
			gradient->stop( paint.getOffset( i ), paint.getColor( i ) );

		mGradients.set( gradient );
	}

	if( prepareShader ) {
		ScopedShader scpShader( Shader::Type::RADIAL_GRADIENT );
		scpShader.setCoords( GLenum( gradient->getUnits() ), mat3{ gradient->getTransform() } );
		scpShader.uniform( "index", mGradients.index( paint.getId() ) );
		scpShader.uniform( "gradTab", 0 );
		scpShader.uniform( "focalToCenter", vec2( gradient->getCx() - gradient->getFx(), gradient->getCy() - gradient->getFy() ) );
		scpShader.uniform( "centerRadius", gradient->getR() );
		scpShader.uniform( "focalRadius", gradient->getFr() );
		scpShader.uniform( "translationPoint", vec2( gradient->getFx(), gradient->getFy() ) );
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

bool Svg::findPath( size_t uuid, size_t &index ) const
{
	if( mPathsLookup.count( uuid ) ) {
		index = mPathsLookup.at( uuid );
		return true;
	}

	return false;
}

size_t Svg::insertOrReplacePath( size_t uuid, Path &&path )
{
	size_t index = 0;
	if( findPath( uuid, index ) ) {
		mPaths.at( index ) = std::move( path );
	}
	else {
		index = mPaths.size();
		mPathsLookup[uuid] = index;
		mPaths.push_back( std::move( path ) );
	}

	return index;
}

void Svg::draw()
{
	auto ctx = gl::context();

	// Enable stencil buffer testing.
	ctx->pushBoolState( GL_STENCIL_TEST, GL_TRUE );

	// Enable premultiplied alpha.
	ctx->pushBoolState( GL_BLEND, GL_TRUE );
	ctx->pushBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

	// Enable sRGB correct rendering.
	ctx->pushBoolState( GL_FRAMEBUFFER_SRGB, GL_TRUE );

	// Bind gradient texture.
	const auto &tex = mGradients.getTexture();
	ctx->pushTextureBinding( tex->getTarget(), tex->getId(), 0 );

	gl::pushModelView();
	for( const auto &call : mDrawCalls ) {
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::matrixMult3x2fNV( GL_MODELVIEW, value_ptr( call.transform ) );

		/// <summary>
		/// Clip paths are handled as follows:
		///	  1. The path or group of paths are rendered to the stencil buffer using either non-zero or even-odd fill rule.
		///	  2. The lowest bits of the stencil are now set for all pixels that need to be covered.
		///	  3. We then cover the pixels without writing to the color buffer, replacing the stencil value with the highest bit (0x80) of the test is passed.
		///	  4. Repeat this for each nested clip path, but use the next highest bit (0x40, 0x20, etc.) instead.
		///	  5. When rendering the actual clipped content, render normally but only draw pixels if all clip bits are set.
		///	  6. We then reset the lowest clip bit by doing a cover with the appropriate stencil functions set.
		///	  7. At the end of each clip path, reset the corresponding clip bit.
		/// </summary>

		if( call.clipMask && call.fill.isNone() && call.stroke.isNone() ) { // (steps 1-4 and 7).
			// Render shape to stencil buffer to use it as a clip-path.
			gl::ScopedColorMask   scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.
			gl::ScopedStencilMask scpStencilMask( call.coverMask | call.clipMask );       // Don't write to previous clip bits.

			gl::stencilOp( GL_KEEP, GL_KEEP, call.stencilOp );                           // stencilOp = GL_REPLACE to set clip, GL_ZERO to erase clip.
			gl::stencilFunc( call.stencilFunc, GLint( call.clipMask ), call.coverMask ); // stencilFunc = GL_NOTEQUAL to set clip, GL_ALWAYS to erase clip.

			if( call.stencilOp == GL_REPLACE )
				gl::stencilFillPathNV( call.path, GL_COUNT_UP_NV, call.coverMask & call.fillRule ); // On set: write path to LSB portion (step 1).

			gl::coverFillPathNV( call.path, GL_BOUNDING_BOX_NV ); // On set: convert LSB portion to clip bit (step 3), on reset: clear stencil buffer bits (step 7).
		}

		if( call.image ) {
			ScopedShader scpShader( Shader::Type::IMAGE );
			scpShader.setColor( ColorA::white() );
			scpShader.setCoords( GLenum( /*call.fill.mUseObjectBoundingBox ?*/ CoordinateSpace::OBJECT_BOUNDING_BOX /*: CoordinateSpace::USER_SPACE_ON_USE*/ ), call.fill.getTransform() );
			scpShader.uniform( "image", 2 );

			gl::ScopedTextureBind scpImage( call.image, 2 );

			if( call.clipMask ) {
				gl::ScopedStencilMask scpStencilMask( call.coverMask | call.clipMask ); // Don't write to previous clip bits.

				GLuint mask = call.coverMask << 1 | 0x01;
				gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
				gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

				gl::stencilThenCoverFillPathNV( call.path, GL_COUNT_UP_NV, call.fillRule & call.coverMask, GL_BOUNDING_BOX_NV ); // (step 5).

				// Remove shape from stencil buffer (step 6).
				gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

				gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
				gl::stencilFunc( GL_ALWAYS, GLint( call.clipMask ), call.coverMask );

				gl::coverFillPathNV( call.path, GL_BOUNDING_BOX_NV );
			}
			else {
				gl::stencilFunc( GL_NOTEQUAL, 0, call.fillRule );
				gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

				gl::stencilThenCoverFillPathNV( call.path, GL_COUNT_UP_NV, call.fillRule, GL_BOUNDING_BOX_NV );
			}
		}
		else {
			if( !call.fill.isNone() ) {
				ColorA solidColor = call.fill.getColor();
				solidColor.a *= call.fillOpacity;

				const auto type = preparePaint( call.fill, call.fillOpacity, true );

				ScopedShader scpShader( type );
				scpShader.setColor( solidColor );

				if( call.clipMask ) {
					gl::ScopedStencilMask scpStencilMask( call.coverMask | call.clipMask ); // Don't write to previous clip bits.

					GLuint mask = call.coverMask << 1 | 0x01;
					gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
					gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

					gl::stencilThenCoverFillPathNV( call.path, GL_COUNT_UP_NV, call.fillRule & call.coverMask, GL_BOUNDING_BOX_NV ); // (step 5).

					// Remove shape from stencil buffer (step 6).
					gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

					gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
					gl::stencilFunc( GL_ALWAYS, GLint( call.clipMask ), call.coverMask );

					gl::coverFillPathNV( call.path, GL_BOUNDING_BOX_NV );
				}
				else {
					gl::stencilFunc( GL_NOTEQUAL, 0, call.fillRule );
					gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

					gl::stencilThenCoverFillPathNV( call.path, GL_COUNT_UP_NV, call.fillRule, GL_BOUNDING_BOX_NV );
				}
			}

			if( !call.stroke.isNone() ) {
				ColorA solidColor = call.stroke.getColor();
				solidColor.a *= call.strokeOpacity;

				const auto type = preparePaint( call.stroke, call.strokeOpacity, true );

				ScopedShader scpShader( type );
				scpShader.setColor( solidColor );

				if( call.clipMask ) {
					gl::ScopedStencilMask scpStencilMask( call.coverMask | call.clipMask ); // Don't write to previous clip bits.

					GLuint mask = call.coverMask << 1 | 0x01;
					gl::stencilFunc( GL_LESS, GLint( ~mask & 0xFF ), 0xFF ); // (step 5).
					gl::stencilOp( GL_KEEP, GL_KEEP, GL_KEEP );

					gl::stencilThenCoverStrokePathNV( call.path, GL_COUNT_UP_NV, call.coverMask, GL_BOUNDING_BOX_NV ); // (step 5).

					// Remove shape from stencil buffer (step 6).
					gl::ScopedColorMask scpColorMask( GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE ); // Don't write to color buffer.

					gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );
					gl::stencilFunc( GL_ALWAYS, GLint( call.clipMask ), call.coverMask );

					gl::coverStrokePathNV( call.path, GL_BOUNDING_BOX_NV );
				}
				else {
					gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
					gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

					gl::stencilThenCoverStrokePathNV( call.path, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
				}
			}
		}
	}
	gl::popModelView();

	ctx->popTextureBinding( tex->getTarget(), 0 );

	ctx->popBoolState( GL_FRAMEBUFFER_SRGB );

	ctx->popBlendFuncSeparate();
	ctx->popBoolState( GL_BLEND );

	ctx->popBoolState( GL_STENCIL_TEST );
}

void Svg::start()
{
	mStacks.defaults();
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
	if( mStacks.clipPath.size() > 5 )
		CI_LOG_W( "Maximum number of nested clip-paths reached! Results are undefined." );

	// Generate draw call to set the clip-path.
	size_t index = 0;
	if( !findPath( clippath.getUuid(), index ) ) {
		// Obtain shape from clip-path.
		const auto shape = clippath.getShape();
		if( shape.empty() )
			return;

		Path path( shape );
		index = insertOrReplacePath( clippath.getUuid(), std::move( path ) );
	}

	mStacks.clipPath.push_back( &clippath );

	startClipPath( GLuint( mStacks.clipPath.size() ), getPathAt( index ).getId(), toMat3x2( mStacks.matrix.back() ) );
}

void Svg::popClipPath()
{
	assert( !mClipPathStack.empty() );

	// Generate draw call to reset the clip-path.
	size_t index = 0;
	if( !findPath( mStacks.clipPath.back()->getUuid(), index ) ) {
		__debugbreak(); // Path should already be cached!
	}

	finishClipPath( GLuint( mStacks.clipPath.size() ), getPathAt( index ).getId(), toMat3x2( mStacks.matrix.back() ) );

	mStacks.clipPath.pop_back();
}

void Svg::drawPath( const svg::Path &path )
{
	size_t index = 0;
	if( !findPath( path.getUuid(), index ) ) {
		Path p( path.getShape2d() );
		index = insertOrReplacePath( path.getUuid(), std::move( p ) );
	}

	render( getPathAt( index ) );
}

void Svg::drawPolyline( const svg::Polyline &polyline )
{
	size_t index = 0;
	if( !findPath( polyline.getUuid(), index ) ) {
		Path p( polyline.getShape() );
		index = insertOrReplacePath( polyline.getUuid(), std::move( p ) );
	}

	render( getPathAt( index ) );
}

void Svg::drawPolygon( const svg::Polygon &polygon )
{
	size_t index = 0;
	if( !findPath( polygon.getUuid(), index ) ) {
		Path p( polygon.getShape() );
		index = insertOrReplacePath( polygon.getUuid(), std::move( p ) );
	}

	render( getPathAt( index ) );
}

void Svg::drawLine( const svg::Line &line )
{
	size_t index = 0;
	if( !findPath( line.getUuid(), index ) ) {
		Path p( line.getShape() );
		index = insertOrReplacePath( line.getUuid(), std::move( p ) );
	}

	render( getPathAt( index ) );
}

void Svg::drawRect( const svg::Rect &rect )
{
	size_t index = 0;
	if( !findPath( rect.getUuid(), index ) ) {
		Path p( rect.getShape() );
		index = insertOrReplacePath( rect.getUuid(), std::move( p ) );
	}

	render( getPathAt( index ) );
}

void Svg::drawCircle( const svg::Circle &circle )
{
	size_t index = 0;
	if( !findPath( circle.getUuid(), index ) ) {
		Path p( circle.getShape() );
		index = insertOrReplacePath( circle.getUuid(), std::move( p ) );
	}

	render( getPathAt( index ) );
}

void Svg::drawEllipse( const svg::Ellipse &ellipse )
{
	size_t index = 0;
	if( !findPath( ellipse.getUuid(), index ) ) {
		Path p( ellipse.getShape() );
		index = insertOrReplacePath( ellipse.getUuid(), std::move( p ) );
	}

	render( getPathAt( index ) );
}

void Svg::drawImage( const svg::Image &image )
{
	if( !shouldRender() )
		return;

	size_t index = 0;
	if( !findPath( image.getUuid(), index ) ) {
		Path p( Path2d::rectangle( image.getRect() ) );
		index = insertOrReplacePath( image.getUuid(), std::move( p ) );
	}

	addDrawCall( GLuint( mStacks.clipPath.size() ),                 //
		getPathAt( index ).getId(),                                 //
		toMat3x2( mStacks.matrix.back() ),                          //
		mStacks.fill.back(),                                        //
		mStacks.stroke.back(),                                      //
		mStacks.fillOpacity.back() * mStacks.groupOpacity.back(),   //
		mStacks.strokeOpacity.back() * mStacks.groupOpacity.back(), //
		mStacks.fillRule.back(),                                    //
		image );
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
	preparePaint( paint );
}

void Svg::popFill()
{
	mStacks.fill.pop_back();
}

void Svg::pushStroke( const svg::Paint &paint )
{
	mStacks.stroke.push_back( paint );
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

void Svg::render( const Path &path )
{
	if( !shouldRender() )
		return;

	// Set path parameters.
	path.setMiterLimit( mStacks.miterLimit.back() );
	path.setDashPattern( mStacks.dashArray.back() );
	path.setDashOffset( mStacks.dashOffset.back() );
	path.setEndCaps( toCapsStyle( mStacks.lineCap.back() ) );
	path.setDashCaps( toCapsStyle( mStacks.lineCap.back() ), toCapsStyle( mStacks.lineCap.back() ) );
	path.setJoinStyle( toJoinStyle( mStacks.lineJoin.back() ) );
	path.setStrokeWidth( mStacks.strokeWidth.back() );

	// Generate draw call.
	addDrawCall( GLuint( mStacks.clipPath.size() ), path.getId(), toMat3x2( mStacks.matrix.back() ), //
		mStacks.fill.back(), mStacks.stroke.back(),                                                  //
		mStacks.fillOpacity.back() * mStacks.groupOpacity.back(), mStacks.strokeOpacity.back() * mStacks.groupOpacity.back(), mStacks.fillRule.back() );
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
			  "    fragColor.rgb = mix( color.rgb, fragColor.rgb, fragColor.a );"
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
			  "    fragColor.rgb = mix( color.rgb, fragColor.rgb, fragColor.a );"
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
			  "    fragColor.rgb = mix( color.rgb, fragColor.rgb, fragColor.a );"
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
			  "    fragColor = texture(image, uv);"
			  "    if( uv.x < 0 || uv.y < 0 || uv.x > 1 || uv.y > 1 ) {"
			  "        fragColor.a = 0;"
			  "    }"
			  "    fragColor.a *= opacity;"
			  "    fragColor.rgb = mix( color.rgb, fragColor.rgb, fragColor.a );"
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
		gl::programPathFragmentInputGenNV( mShader->mProgram, 0, GL_CONSTANT_NV, 4, color.premultiplied().ptr() );
}

ScopedShader::~ScopedShader()
{
	if( mShader )
		mShader->unbind();
}

FaceRef Cache::loadFace( const text::Face *face )
{
	Cache &self = get();

	const auto &name = face->getFamilyName();

	if( self.mFaces.count( name ) && static_cast<bool>( self.mFaces.at( name ) ) ) {
		// CI_LOG_V( "Using cached face for " << name << " (" << std::this_thread::get_id() << ")" );
		return self.mFaces.at( name );
	}

	CI_LOG_V( "Loading face for " << name << " (" << std::this_thread::get_id() << ")" );

	auto cached = Face::create( face );
	self.mFaces.insert_or_assign( name, cached );

	return cached;
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
	auto &fonts = get().mFaces;
	for( auto itr = fonts.begin(); itr != fonts.end(); ) {
		const auto &item = *itr;
		if( item.second.use_count() < 2 ) {
			CI_LOG_V( "Removing face " << item.first << " (" << std::this_thread::get_id() << ")" );
			itr = fonts.erase( itr );
		}
		else
			++itr;
	}

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
	CI_LOG_V( "Removing all faces (" << std::this_thread::get_id() << ")" );
	get().mFaces.clear();
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

void renderText( const text::Typesetter &typesetter, const vec2 &offset )
{
	gl::ScopedBlendPremult scpBlend;
	gl::ScopedColor        scpColor;
	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );

	std::vector<glm::mat3x2> transforms;

	text::Typesetter::Iterator iter = typesetter.getIterator();
	const text::Run           *runPtr;
	vec2                       lineDrawOffset;
	while( typesetter.nextRun( iter, &runPtr, &lineDrawOffset ) ) {
		if( runPtr->isPlaceholder() )
			continue;

		// Cache the font face.
		auto face = Cache::loadFace( runPtr->getFont()->getFace() );

		// Create transforms.
		transforms.clear();
		transforms.reserve( runPtr->getNumGlyphs() );

		const auto origin = offset + lineDrawOffset + runPtr->getDrawOffset();
		const auto scale = runPtr->getFont()->getSize();
		const auto positions = runPtr->getGlyphPositions();
		const auto orientations = runPtr->getGlyphOrientations();
		const auto indices = runPtr->getGlyphIndices();

		for( size_t i = 0; i < runPtr->getNumGlyphs(); ++i ) {
			const auto position = origin + positions[i];
			const auto normal = orientations ? scale * orientations[i] : vec2( 0, -scale );
			transforms.emplace_back( -normal.y, normal.x, normal.x, normal.y, position.x, position.y );
		}

		// Draw immediately.
		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		ScopedShader scpShader( runPtr->getColor().premultiplied() );
		gl::stencilThenCoverFillPathInstancedNV(
			static_cast<GLsizei>( transforms.size() ), GL_UNSIGNED_INT, indices, face->getBaseId(), GL_PATH_FILL_MODE_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
	}
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
	style = trim( toLower( style ) );
	if( style == "reflect" )
		return SpreadMethod::REFLECT;
	if( style == "repeat" )
		return SpreadMethod::REPEAT;
	return SpreadMethod::DEFAULT;
}

} // namespace nvp
} // namespace cinder