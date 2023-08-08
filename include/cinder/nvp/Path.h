/*
Copyright (c) 2021, Paul Houx Creative Coding - All rights reserved.
This code is intended for use with the Cinder C++ library: http://libcinder.org

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

#include "cinder/PolyLine.h"
#include "cinder/gl/Texture.h"
#include "cinder/gl/wrapper.h"
#include "cinder/nvp/Core.h"
#include "cinder/nvp/Gradient.h"

namespace cinder {
namespace nvp {

using PathRef = std::shared_ptr<class Path>;

class Path {
  public:
	virtual ~Path();

	Path( const Path &other )
	{
		if( other.mPathId > 0 ) {
			mPathId = gl::genPathsNV( 1 );
			gl::copyPathNV( mPathId, other.mPathId );
		}
	}
	Path( Path &&other ) noexcept
	{
		if( mPathId > 0 )
			gl::deletePathsNV( mPathId, 1 );
		mPathId = other.mPathId;
		other.mPathId = 0;
	}
	Path &operator=( const Path &other )
	{
		if( other.mPathId > 0 && this != &other ) {
			if( mPathId == 0 )
				mPathId = gl::genPathsNV( 1 );
			gl::copyPathNV( mPathId, other.mPathId );
		}
		return *this;
	}
	Path &operator=( Path &&other ) noexcept
	{
		if( this != &other ) {
			if( mPathId > 0 )
				gl::deletePathsNV( mPathId, 1 );
			mPathId = other.mPathId;
			other.mPathId = 0;
		}
		return *this;
	}

	//! Construct a path from a Path2d. Note the correct winding order: points should be defined in counter clockwise order.
	explicit Path( const Path2d &path );
	//! Construct a path from a Shape2d. Note the correct winding order: holes should be defined in clockwise order.
	explicit Path( const Shape2d &shape );
	//! Construct a path from a PolyLine2. Note the correct winding order: points should be defined in counter clockwise order.
	explicit Path( const PolyLine2 &polyLine );

	// ReSharper disable once CppHiddenFunction
	//! Creates a shallow clone of this path. Use with care.
	[[nodiscard]] PathRef clone() const { return std::make_shared<Path>( *this ); }

	//! Returns the path's unique id number.
	GLuint getId() const { return mPathId; }

	//! Returns the total length of the path.
	[[nodiscard]] virtual float getLength() const;
	//! Returns the path's bounding box, calculated from the actual shape and adjusted for stroke width.
	[[nodiscard]] virtual Rectf getBounds() const;	

	//! Obtains the path's commands and coords.
	void getPath( std::vector<GLubyte> &commands, std::vector<GLfloat> &coords ) const;
	//! Set the path's commands and coords. This will overwrite any existing commands and coords.
	void setPath( const std::vector<GLubyte> &commands, const std::vector<GLfloat> &coords ); /* non-const */

	//! Returns the number of segments defined for this path.
	int getNumSegments() const;

	//! Resets the dash pattern.
	void resetDashPattern() const;
	//! Sets the dash pattern.
	void setDashPattern( const std::vector<float> &pattern ) const;
	//! Sets the dash pattern offset.
	void setDashOffset( float offset, PathStyle style = PathStyle::DEFAULT ) const;
	//! Sets the caps for dashed strokes.
	void setDashCaps( CapsStyle caps ) const;
	//! Sets the caps for dashed strokes.
	void setDashCaps( CapsStyle initialCap, CapsStyle terminalCap ) const;

	//! Renders the path to the stencil buffer but does not cover the path.
	//! Use the `stroke()` methods to stencil and cover the path in a single step.
	virtual void stencilStroke( float strokeWidth ) { stencilStroke( CapsStyle::DEFAULT, JoinStyle::DEFAULT, strokeWidth ); }
	//! Renders the path to the stencil buffer but does not cover the path.
	//! Use the `stroke()` methods to stencil and cover the path in a single step.
	virtual void stencilStroke( CapsStyle caps, float strokeWidth ) { stencilStroke( caps, JoinStyle::DEFAULT, strokeWidth ); }
	//! Renders the path to the stencil buffer but does not cover the path.
	//! Use the `stroke()` methods to stencil and cover the path in a single step.
	virtual void stencilStroke( JoinStyle join, float strokeWidth ) { stencilStroke( CapsStyle::DEFAULT, join, strokeWidth ); }
	//! Renders the path to the stencil buffer but does not cover the path.
	//! Use the `stroke()` methods to stencil and cover the path in a single step.
	virtual void stencilStroke( CapsStyle caps, JoinStyle join, float strokeWidth );
	//! Renders the path to the stencil buffer but does not cover the path.
	//! Use the `fill()` methods to stencil and cover the path in a single step.
	virtual void stencilFill();

	//! Covers the paths that have already been rendered to the stencil buffer using a solid \a color. Clears the affected region of the stencil buffer by default, but this can be overridden.
	//! Use the `fill()` or `stroke()` methods to stencil and cover the path in a single step.
	virtual void cover( const ColorA &color, bool clearStencil = true );
	//! Covers the paths that have already been rendered to the stencil buffer using a solid \a color. Clears the affected region of the stencil buffer by default, but this can be overridden.
	virtual void cover( const ColorA &color, const Rectf &bounds, bool clearStencil = true );
	//! Covers the paths that have already been rendered to the stencil buffer using the provided \a texture. Clears the affected region of the stencil buffer by default, but this can be overridden.
	virtual void cover( const gl::Texture2dRef &texture, const Rectf &bounds, bool clearStencil = true );

	//! Strokes the path with a solid \a color.
	virtual void stroke( const ColorA &color, float strokeWidth = 1 ) { stroke( color, CapsStyle::DEFAULT, JoinStyle::DEFAULT, strokeWidth ); }
	//! Strokes the path with a solid \a color and the specified \a caps style.
	virtual void stroke( const ColorA &color, CapsStyle caps, float strokeWidth = 1 ) { stroke( color, caps, JoinStyle::DEFAULT, strokeWidth ); }
	//! Strokes the path with a solid \a color and the specified \a join style.
	virtual void stroke( const ColorA &color, JoinStyle join, float strokeWidth = 1 ) { stroke( color, CapsStyle::DEFAULT, join, strokeWidth ); }
	//! Strokes the path with a solid \a color and the specified \a caps and \a join styles.
	virtual void stroke( const ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth = 1 );

	//! Strokes the path instances with a solid \a color and the specified \a caps and \a join styles.
	virtual void strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth = 1 );
	//! Strokes the path instances with a solid \a color and the specified \a caps and \a join styles.
	virtual void strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth = 1 );

	//! Fills the path with a solid \a color.
	virtual void fill( const ColorA &color );
	//! Fills the path with a \a texture, automatically centered within the path's bounding box.
	virtual void fill( const gl::TextureRef &texture ) { fill( texture, getBounds() ); }
	//! Fills the path with a \a texture, automatically centered within the specified \a bounding box.
	virtual void fill( const gl::TextureRef &texture, const Rectf &bounds );
	//! Fills the path with a \a texture.
	virtual void fill( const gl::TextureRef &texture, const vec2 &upperLeftTexCoord, const vec2 &lowerRightTexCoord );
	//! Fills the path with a gradient.
	virtual void fill( const Gradients &gradients, const std::string &id, float opacity = 1 );

	//! Fills the path instances with a solid \a color.
	virtual void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color );
	//! Fills the path instances with a solid \a color.
	virtual void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color );

	//! Creates a new path by adding paths together.
	[[nodiscard]] Path operator+( const Path &other ) const
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

	//! Adds the \a other path to our path.
	Path &operator+=( const Path &other )
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

	void transform( const glm::mat3x2 &transform ) const { gl::transformPathNV( mPathId, mPathId, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( &transform ) ); }

	[[nodiscard]] Path transformed( const glm::mat3x2 &transform ) const
	{
		Path path( *this );
		path.transform( transform );
		return path;
	}

	static std::string toSvgString( GLuint pathId );

  protected:
	Path() = default;

	static GLubyte toPathCommand( Path2d::SegmentType type )
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

	GLuint mPathId{ 0 };
};

} // namespace nvp
} // namespace cinder