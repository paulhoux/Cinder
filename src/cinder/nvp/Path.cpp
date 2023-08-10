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

#include "cinder/nvp/Path.h"

#include "cinder/gl/draw.h"
#include "cinder/gl/scoped.h"
#include "cinder/nvp/Canvas.h"
#include "cinder/nvp/Core.h"

#include <variant>

namespace cinder {
namespace nvp {

Path::~Path()
{
	if( mPathId > 0 )
		gl::deletePathsNV( mPathId, 1 );
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

void Path::stencilStroke( CapsStyle caps, JoinStyle join, float strokeWidth )
{
	gl::pathParameterfNV( mPathId, GL_PATH_STROKE_WIDTH_NV, strokeWidth );
	gl::pathParameteriNV( mPathId, GL_PATH_END_CAPS_NV, GLint( caps ) );
	gl::pathParameteriNV( mPathId, GL_PATH_JOIN_STYLE_NV, GLint( join ) );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilStrokePathNV( mPathId, 0, 0xFF );
}

void Path::stencilFill()
{
	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF );
}

void Path::cover( const ColorA &color, bool clearStencil )
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::coverFillPathNV( mPathId, GL_BOUNDING_BOX_NV );
}

void Path::cover( const ColorA &color, const Rectf &bounds, bool clearStencil )
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::drawSolidRect( bounds );
}

void Path::cover( const gl::Texture2dRef &texture, const Rectf &bounds, bool clearStencil )
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, clearStencil ? GL_ZERO : GL_KEEP );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	draw( texture, bounds );
}

void Path::stroke( const ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth ) const
{
	gl::pathParameterfNV( mPathId, GL_PATH_STROKE_WIDTH_NV, strokeWidth );
	gl::pathParameteriNV( mPathId, GL_PATH_END_CAPS_NV, GLint( caps ) );
	gl::pathParameteriNV( mPathId, GL_PATH_JOIN_STYLE_NV, GLint( join ) );

	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverStrokePathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
}

void Path::strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth )
{
	gl::pathParameterfNV( mPathId, GL_PATH_STROKE_WIDTH_NV, strokeWidth );
	gl::pathParameteriNV( mPathId, GL_PATH_END_CAPS_NV, GLint( caps ) );
	gl::pathParameteriNV( mPathId, GL_PATH_JOIN_STYLE_NV, GLint( join ) );

	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverStrokePathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

void Path::strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth )
{
	gl::pathParameterfNV( mPathId, GL_PATH_STROKE_WIDTH_NV, strokeWidth );
	gl::pathParameteriNV( mPathId, GL_PATH_END_CAPS_NV, GLint( caps ) );
	gl::pathParameteriNV( mPathId, GL_PATH_JOIN_STYLE_NV, GLint( join ) );

	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverStrokePathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_3D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

void Path::fill( const ColorA &color )
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
}

void Path::fill( const gl::TextureRef &texture, const Rectf &bounds )
{
	const auto textureBounds = Rectf( texture->getBounds() );
	const auto fit = bounds.getCenteredFit( textureBounds, true ).scaled( 1.0f / textureBounds.getSize() );

	const auto pathBounds = getBounds();

	auto normalized = Rectf( pathBounds.getUpperLeft() - bounds.getUpperLeft(), pathBounds.getLowerRight() - bounds.getUpperLeft() ).scaled( fit.getSize() / bounds.getSize() );
	normalized.offset( fit.getUpperLeft() );

	const auto topDown = texture->isTopDown();
	const auto upperLeftTexCoord = vec2{ normalized.x1, topDown ? normalized.y1 : 1.0f - normalized.y1 };
	const auto lowerRightTexCoord = vec2{ normalized.x2, topDown ? normalized.y2 : 1.0f - normalized.y2 };

	fill( texture, upperLeftTexCoord, lowerRightTexCoord );
}

void Path::fill( const gl::TextureRef &texture, const vec2 &upperLeftTexCoord, const vec2 &lowerRightTexCoord )
{
	const GLfloat data[6] = { lowerRightTexCoord.x - upperLeftTexCoord.x, 0, upperLeftTexCoord.x, 0, lowerRightTexCoord.y - upperLeftTexCoord.y, upperLeftTexCoord.y };

	gl::ScopedTextureBind scpTex( texture, 0 );
	gl::ScopedState       scpState( GL_TEXTURE_2D, GL_TRUE );
	gl::pathTexGenNV( GL_TEXTURE0, GL_PATH_OBJECT_BOUNDING_BOX_NV, 2, &data[0] );

	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	ScopedShader scpShader( gl::context()->getCurrentColor().premultiplied() ); // TODO: texture shader

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
}

void Path::fill( const Gradients &gradients, const std::string &id, float opacity )
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	// Bind gradient texture.
	const auto &          texture = gradients.getTexture();
	gl::ScopedTextureBind scpTex( texture, 0 );

	if( auto linear = std::dynamic_pointer_cast<const LinearGradient>( gradients.at( id ) ); linear ) {
		// Use the correct gradient spread.
		texture->setWrapS( GLenum( linear->getSpread() ) );

		// Setup shader.
		ScopedShader scpShader( Shader::Type::LINEAR_GRADIENT );
		scpShader.setCoords( GLenum( linear->getUnits() ), mat3{ linear->getTransform() } );
		scpShader.uniform( "index", gradients.index( id ) );                        //
		scpShader.uniform( "gradTab", 0 );                                          //
		scpShader.uniform( "gradStart", vec2{ linear->getX1(), linear->getY1() } ); // TODO: percentages.
		scpShader.uniform( "gradEnd", vec2{ linear->getX2(), linear->getY2() } );   // TODO: percentages.
		scpShader.uniform( "opacity", opacity );

		// Render.
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
	}
	else if( auto radial = std::dynamic_pointer_cast<const RadialGradient>( gradients.at( id ) ); radial ) {
		// Use the correct gradient spread.
		texture->setWrapS( GLenum( radial->getSpread() ) );

		// Setup shader.
		ScopedShader scpShader( Shader::Type::RADIAL_GRADIENT );
		scpShader.setCoords( GLenum( radial->getUnits() ), mat3{ radial->getTransform() } );
		scpShader.uniform( "index", gradients.index( id ) );                                                                //
		scpShader.uniform( "gradTab", 0 );                                                                                  //
		scpShader.uniform( "focalToCenter", vec2{ radial->getCx() - radial->getFx(), radial->getCy() - radial->getFy() } ); // TODO: percentages.
		scpShader.uniform( "centerRadius", radial->getR() );                                                                //
		scpShader.uniform( "focalRadius", radial->getFr() );                                                                //
		scpShader.uniform( "translationPoint", vec2{ radial->getFx(), radial->getFy() } );                                  // TODO: percentages.
		scpShader.uniform( "opacity", opacity );

		// Render.
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverFillPathNV( mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
	}
}

void Path::fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color )
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

void Path::fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color )
{
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	ScopedShader scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathInstancedNV( GLsizei( paths.size() ), GL_UNSIGNED_INT, paths.data(), mPathId, GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV, GL_AFFINE_3D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

std::string Path::toSvgString( GLuint pathId )
{
	using Element = std::variant<GLfloat, std::string>;
	static const auto PATH = []( std::string &svg, const std::initializer_list<Element> &elements ) {
		if( !svg.empty() && std::isdigit( svg[svg.size() - 1] ) )
			svg += " ";

		bool useSeparator{ false };
		for( const auto &element : elements ) {
			const auto *value = std::get_if<GLfloat>( &element );
			if( value ) {
				if( useSeparator )
					svg += ",";
				svg += /*trimNumber*/ ( std::to_string( *value ) ); // TODO trim number
				useSeparator = true;
			}
			else {
				if( useSeparator )
					svg += " ";
				svg += std::get<std::string>( element );
				useSeparator = std::isdigit( svg[svg.size() - 1] );
			}
		}
	};

	std::string svg;

	if( pathId > 0 ) {
		GLint numCommands;
		GLint numCoords;

		gl::getPathParameterivNV( pathId, GL_PATH_COMMAND_COUNT_NV, &numCommands );
		gl::getPathParameterivNV( pathId, GL_PATH_COORD_COUNT_NV, &numCoords );

		if( numCommands > 0 && numCoords > 0 ) {
			std::vector<GLubyte> commands( numCommands );
			std::vector<GLfloat> coords( numCoords );

			gl::getPathCommandsNV( pathId, commands.data() );
			gl::getPathCoordsNV( pathId, coords.data() );

			auto previous = GLubyte( GL_CLOSE_PATH_NV );
			auto command = commands.begin();
			auto coord = coords.begin();
			while( command != commands.end() ) {
				switch( *command ) {
				case GL_MOVE_TO_NV:
					PATH( svg, { "M", *coord++, *coord++ } );
					break;
				case GL_RELATIVE_MOVE_TO_NV:
					PATH( svg, { "m", *coord++, *coord++ } );
					break;
				case GL_LINE_TO_NV:
					if( previous != *command && previous != GL_MOVE_TO_NV )
						PATH( svg, { "L" } );
					PATH( svg, { *coord++, *coord++ } );
					break;
				case GL_RELATIVE_LINE_TO_NV:
					if( previous != *command && previous != GL_RELATIVE_MOVE_TO_NV )
						PATH( svg, { "l" } );
					PATH( svg, { *coord++, *coord++ } );
					break;
				case GL_HORIZONTAL_LINE_TO_NV:
					if( previous != *command )
						PATH( svg, { "H" } );
					PATH( svg, { *coord++ } );
					break;
				case GL_RELATIVE_HORIZONTAL_LINE_TO_NV:
					if( previous != *command )
						PATH( svg, { "h" } );
					PATH( svg, { *coord++ } );
					break;
				case GL_VERTICAL_LINE_TO_NV:
					if( previous != *command )
						PATH( svg, { "V" } );
					PATH( svg, { *coord++ } );
					break;
				case GL_RELATIVE_VERTICAL_LINE_TO_NV:
					if( previous != *command )
						PATH( svg, { "v" } );
					PATH( svg, { *coord++ } );
					break;
				case GL_CUBIC_CURVE_TO_NV:
					if( previous != *command )
						PATH( svg, { "C" } );
					PATH( svg, { *coord++, *coord++, "", *coord++, *coord++, "", *coord++, *coord++ } );
					break;
				case GL_RELATIVE_CUBIC_CURVE_TO_NV:
					if( previous != *command )
						PATH( svg, { "c" } );
					PATH( svg, { *coord++, *coord++, "", *coord++, *coord++, "", *coord++, *coord++ } );
					break;
				case GL_SMOOTH_CUBIC_CURVE_TO_NV:
					if( previous != *command )
						PATH( svg, { "S" } );
					PATH( svg, { *coord++, *coord++, "", *coord++, *coord++ } );
					break;
				case GL_RELATIVE_SMOOTH_CUBIC_CURVE_TO_NV:
					if( previous != *command )
						PATH( svg, { "s" } );
					PATH( svg, { *coord++, *coord++, "", *coord++, *coord++ } );
					break;
				case GL_QUADRATIC_CURVE_TO_NV:
					if( previous != *command )
						PATH( svg, { "Q" } );
					PATH( svg, { *coord++, *coord++, "", *coord++, *coord++ } );
					break;
				case GL_RELATIVE_QUADRATIC_CURVE_TO_NV:
					if( previous != *command )
						PATH( svg, { "q" } );
					PATH( svg, { *coord++, *coord++, "", *coord++, *coord++ } );
					break;
				case GL_SMOOTH_QUADRATIC_CURVE_TO_NV:
					if( previous != *command )
						PATH( svg, { "T" } );
					PATH( svg, { *coord++, *coord++ } );
					break;
				case GL_RELATIVE_SMOOTH_QUADRATIC_CURVE_TO_NV:
					if( previous != *command )
						PATH( svg, { "t" } );
					PATH( svg, { *coord++, *coord++ } );
					break;
				case GL_ARC_TO_NV:
					if( previous != *command )
						PATH( svg, { "A" } );
					PATH( svg, { *coord++, *coord++, "", *coord++, *coord++, *coord++, "", *coord++, *coord++ } );
					break;
				case GL_RELATIVE_ARC_TO_NV:
					if( previous != *command )
						PATH( svg, { "a" } );
					PATH( svg, { *coord++, *coord++, "", *coord++, *coord++, *coord++, "", *coord++, *coord++ } );
					break;
				case GL_CLOSE_PATH_NV:
					PATH( svg, { "Z" } );
					break;
				case GL_RECT_NV: {
					const auto &x = *coord++;
					const auto &y = *coord++;
					const auto &w = *coord++;
					const auto &h = *coord++;
					PATH( svg, { "M", x, y, "h", w, "v", h, "h", -w, "Z" } );
				} break;
				case GL_ROUNDED_RECT_NV: {
					const auto &x = *coord++;
					const auto &y = *coord++;
					auto        w = *coord++;
					auto        h = *coord++;
					const auto &r = *coord++;
					w = glm::max( 0.0f, w - r - r );
					h = glm::max( 0.0f, h - r - r );
					PATH( svg,
						{ "M", x + r, y,                                 //
							"h", w, "a", r, r, "", 0, 0, 1, "", r, r,    //
							"v", h, "a", r, r, "", 0, 0, 1, "", -r, r,   //
							"h", -w, "a", r, r, "", 0, 0, 1, "", -r, -r, //
							"v", -h, "a", r, r, "", 0, 0, 1, "", r, -r,  //
							"Z" } );

				} break;
				case GL_ROUNDED_RECT2_NV: {
					const auto &x = *coord++;
					const auto &y = *coord++;
					auto        w = *coord++;
					auto        h = *coord++;
					const auto &rx = *coord++;
					const auto &ry = *coord++;
					w = glm::max( 0.0f, w - rx - rx );
					h = glm::max( 0.0f, h - ry - ry );
					PATH( svg,
						{ "M", x + rx, y,                                    //
							"h", w, "a", rx, ry, "", 0, 0, 1, "", rx, ry,    //
							"v", h, "a", rx, ry, "", 0, 0, 1, "", -rx, ry,   //
							"h", -w, "a", rx, ry, "", 0, 0, 1, "", -rx, -ry, //
							"v", -h, "a", rx, ry, "", 0, 0, 1, "", rx, -ry,  //
							"Z" } );

				} break;
				default:
					__debugbreak();
					break;
				}
				previous = *command++;
			}
		}
	}

	return svg;
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

void Path::setPath( const std::vector<GLubyte> &commands, const std::vector<GLfloat> &coords )
{
	gl::pathCommandsNV( mPathId, commands.size(), commands.data(), coords.size(), GL_FLOAT, coords.data() );
}

} // namespace nvp
} // namespace cinder
