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

#pragma once

#include "cinder/Font.h"
#include "cinder/Path2d.h"
#include "cinder/PolyLine.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/Fbo.h"
#include "cinder/svg/Svg.h"

namespace cinder {
namespace nvpath {

// Forward declarations.
class Cache;
class Canvas;
class ClipRect;
class Path;
class Shader;

using CanvasRef = std::shared_ptr<Canvas>;
using PathRef = std::shared_ptr<Path>;
using ShaderRef = std::shared_ptr<Shader>;

//!
CI_API enum class CapsStyle { FLAT = GL_FLAT, SQUARE = GL_SQUARE_NV, ROUND = GL_ROUND_NV, TRIANGULAR = GL_TRIANGULAR_NV, DEFAULT = GL_FLAT };
//!
CI_API enum class JoinStyle { ROUND = GL_ROUND_NV, BEVEL = GL_BEVEL_NV, MITER_REVERT = GL_MITER_REVERT_NV, MITER_TRUNCATE = GL_MITER_TRUNCATE_NV, DEFAULT = GL_MITER_REVERT_NV };
//!
CI_API enum class PathStyle { MOVETO_RESETS = GL_MOVE_TO_RESETS_NV, MOVETO_CONTINUES = GL_MOVE_TO_CONTINUES_NV, DEFAULT = GL_MOVE_TO_RESETS_NV };

//! Defines the coordinate system used by gradients and images.
enum class CoordinateSpace { OBJECT_BOUNDING_BOX = GL_PATH_OBJECT_BOUNDING_BOX_NV, USER_SPACE_ON_USE = GL_OBJECT_LINEAR_NV, DEFAULT = GL_PATH_OBJECT_BOUNDING_BOX_NV };
//! Defines the spread method used by gradient and images.
enum class SpreadMethod { PAD = GL_CLAMP_TO_EDGE, REFLECT = GL_MIRRORED_REPEAT, REPEAT = GL_REPEAT, DEFAULT = PAD };

//!
CI_API static CapsStyle toCapsStyle( ci::svg::LineCap lineCap );
//!
CI_API static JoinStyle toJoinStyle( ci::svg::LineJoin lineJoin );
//!
CI_API static CoordinateSpace toGradientUnits( std::string style );
//!
CI_API static SpreadMethod toSpreadMethod( std::string style );

//!
CI_API inline glm::mat3x2 toMat3x2( const glm::mat3x3 &m )
{
	return glm::mat3x2{ m[0][0], m[0][1], m[1][0], m[1][1], m[2][0], m[2][1] };
}
//!
CI_API inline glm::mat3x2 toMat3x2( const glm::mat4x4 &m )
{
	return glm::mat3x2{ m[0][0], m[0][1], m[1][0], m[1][1], m[3][0], m[3][1] };
}

//! Shader for solid colors or gradients to be applied to paths.
class CI_API Shader {
  public:
	enum class Type { SOLID_COLOR, LINEAR_GRADIENT, RADIAL_GRADIENT, CONICAL_GRADIENT, IMAGE, UNDEFINED };

	static ShaderRef create( Type type ) { return std::make_shared<Shader>( type ); }

	Shader() = default;
	explicit Shader( Type type );
	virtual ~Shader();

	Shader( const Shader & ) = delete;
	Shader( Shader && ) = default;
	Shader &operator=( const Shader & ) = delete;
	Shader &operator=( Shader && ) = default;

	void        bind() const;
	static void unbind();

	void uniform( const std::string &name, GLint value ) const;
	void uniform( const std::string &name, GLfloat value ) const;
	void uniform( const std::string &name, const vec2 &value ) const;
	void uniform( const std::string &name, const vec3 &value ) const;
	void uniform( const std::string &name, const vec4 &value ) const;
	void uniform( const std::string &name, const mat3 &value ) const;
	void uniform( const std::string &name, const glm::mat3x2 &value ) const;
	void uniform( const std::string &name, const mat4 &value ) const;

	void setColor( const ci::ColorA &color ) const;

  protected:
	GLuint mProgram{ 0 };
	GLuint mPipeline{ 0 };
	Type   mType{ Type::UNDEFINED };

	friend class ScopedShader;
};

class CI_API ScopedShader : public Noncopyable {
	gl::Context *mCtx = nullptr;
	ShaderRef    mShader;

  public:
	//!
	explicit ScopedShader( Shader::Type type );
	//! Activates the solid color shader and sets the current color.
	explicit ScopedShader( const ColorA &color );

	~ScopedShader();

	ScopedShader( const ScopedShader & ) = delete;
	ScopedShader( ScopedShader && ) = delete;
	ScopedShader &operator=( const ScopedShader & ) = delete;
	ScopedShader &operator=( ScopedShader && ) = delete;

	template <typename T>
	void uniform( const std::string &name, const T &value )
	{
		if( mShader )
			mShader->uniform( name, value );
	}

	void setColor( const ColorA &color ) const
	{
		if( mShader )
			mShader->setColor( color );
	}

	//! Only works for linear gradient, radial gradient and image shaders! TODO
	void setCoords( GLenum genMode = GL_PATH_OBJECT_BOUNDING_BOX_NV, const mat3 &transform = {} ) const
	{
		// Sanity check.
		static_assert( sizeof( mat3::value_type ) == sizeof( GLfloat ) );

		if( mShader && ( mShader->mType == Shader::Type::LINEAR_GRADIENT || mShader->mType == Shader::Type::RADIAL_GRADIENT || mShader->mType == Shader::Type::IMAGE ) ) {
			const auto t = transpose( inverse( transform ) );
			gl::programPathFragmentInputGenNV( mShader->mProgram, 1, genMode, 2, value_ptr( t ) );
		}
	}

	//! Only works for linear gradient, radial gradient and image shaders! TODO
	void setCoords( GLenum genMode, const std::vector<float> &data ) const
	{
		assert( data.size() == 6 );

		if( mShader && ( mShader->mType == Shader::Type::LINEAR_GRADIENT || mShader->mType == Shader::Type::RADIAL_GRADIENT || mShader->mType == Shader::Type::IMAGE ) ) {
			gl::programPathFragmentInputGenNV( mShader->mProgram, 1, genMode, 2, data.data() );
		}
	}
};

//! Path represents a vector shape stored efficiently on the GPU.
CI_API class Path {
  public:
	~Path();

	Path() = default;
	Path( const Path &other );
	Path( Path &&other ) noexcept;
	Path &operator=( const Path &other );
	Path &operator=( Path &&other ) noexcept;

	//! Construct a path from a Path2d. Note the correct winding order: points should be defined in counter clockwise order.
	explicit Path( const Path2d &path );
	//! Construct a path from a Shape2d. Note the correct winding order: holes should be defined in clockwise order.
	explicit Path( const Shape2d &shape );
	//! Construct a path from a PolyLine2. Note the correct winding order: points should be defined in counter clockwise order.
	explicit Path( const PolyLine2 &polyLine );
	//! Construct a path from an \a svg string.
	explicit Path( const std::string &svg );

	//! Creates a shallow clone of this path. Use with care.
	[[nodiscard]] PathRef clone() const { return std::make_shared<Path>( *this ); }

	//! Returns the path's unique id number.
	GLuint getId() const { return mPathId; }

	//! Returns the total length of the path.
	[[nodiscard]] float getLength() const;
	//! Returns the path's bounding box, calculated from the actual shape.
	[[nodiscard]] Rectf getFillBounds() const;
	//! Returns the path's bounding box, calculated from the actual shape and adjusted for stroke width.
	[[nodiscard]] Rectf getStrokeBounds() const;

	//! Obtains the path's commands and coords.
	void getPath( std::vector<GLubyte> &commands, std::vector<GLfloat> &coords ) const;
	//! Set the path's commands and coords. This will overwrite any existing commands and coords.
	void setPath( const std::vector<GLubyte> &commands, const std::vector<GLfloat> &coords );
	//! Set the path's commands and coords using the provided \a svg string. This will overwrite any existing commands and coords.
	void setPath( const std::string &svg );

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
	//! Sets the path's end caps for strokes.
	void setEndCaps( CapsStyle caps ) const;
	//! Sets the path's end caps for strokes.
	void setEndCaps( CapsStyle initialCap, CapsStyle terminalCap ) const;
	//! Sets the join style for strokes.
	void setJoinStyle( JoinStyle joins ) const;
	//! Sets the stroke width.
	void setStrokeWidth( float width ) const;
	//! Sets the miter limit for stokes.
	void setMiterLimit( float limit ) const;

	//! Sets multiple stroke parameters at once.
	void setStroke( CapsStyle caps, float strokeWidth ) const { setStroke( caps, JoinStyle::DEFAULT, strokeWidth ); }
	//! Sets multiple stroke parameters at once.
	void setStroke( JoinStyle join, float strokeWidth ) const { setStroke( CapsStyle::DEFAULT, join, strokeWidth ); }
	//! Sets multiple stroke parameters at once.
	void setStroke( CapsStyle caps, JoinStyle join, float strokeWidth ) const;

	//! Renders the path to the stencil buffer but does not cover the path.
	//! Use the `stroke()` methods to stencil and cover the path in a single step.
	void stencilStroke( GLuint stencilMask = 0xFF );
	//! Renders the path to the stencil buffer but does not cover the path.
	//! Use the `fill()` methods to stencil and cover the path in a single step.
	void stencilFill( GLuint stencilMask = 0xFF, GLenum fillMode = GL_COUNT_UP_NV );

	//! Covers the paths that have already been rendered to the stencil buffer using a solid \a color. Clears the affected region of the stencil buffer by default, but this can be overridden.
	//! Use the `fill()` or `stroke()` methods to stencil and cover the path in a single step.
	void cover( const ColorA &color, bool clearStencil = true );
	//! Covers the paths that have already been rendered to the stencil buffer using a solid \a color. Clears the affected region of the stencil buffer by default, but this can be overridden.
	void cover( const ColorA &color, const Rectf &bounds, bool clearStencil = true );
	//! Covers the paths that have already been rendered to the stencil buffer using the provided \a texture. Clears the affected region of the stencil buffer by default, but this can be overridden.
	void cover( const gl::Texture2dRef &texture, const Rectf &bounds, bool clearStencil = true );

	//! Strokes the path with a solid \a color.
	void stroke( const ColorA &color, bool clearStencil = true ) const;

	//! Strokes the path instances with a solid \a color and the specified \a caps and \a join styles.
	void strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color, bool clearStencil = true );
	//! Strokes the path instances with a solid \a color and the specified \a caps and \a join styles.
	void strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, bool clearStencil = true );

	//! Fills the path with a solid \a color.
	void fill( const ColorA &color, bool clearStencil = true ) const;
	//! Fills the path with a \a texture, automatically centered within the path's bounding box.
	void fill( const gl::TextureRef &texture, bool clearStencil = true ) const { fill( texture, getFillBounds(), clearStencil ); }
	//! Fills the path with a \a texture, automatically centered within the specified \a bounding box.
	void fill( const gl::TextureRef &texture, const Rectf &bounds, bool clearStencil = true ) const;
	//! Fills the path with a \a texture.
	void fill( const gl::TextureRef &texture, const vec2 &upperLeftTexCoord, const vec2 &lowerRightTexCoord, bool clearStencil = true ) const;

	//! Fills the path instances with a solid \a color.
	void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color, bool clearStencil = true ) const;
	//! Fills the path instances with a solid \a color.
	void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, bool clearStencil = true ) const;

	//! Creates a new path by adding paths together.
	[[nodiscard]] Path operator+( const Path &other ) const;

	//! Adds the \a other path to our path.
	Path &operator+=( const Path &other );

	//! Transforms the coordinates of our path.
	void transform( const glm::mat3x2 &transform ) const;

	//! Returns the result of this path's transformation as a new path.
	[[nodiscard]] Path transformed( const glm::mat3x2 &transform ) const;
	//! Returns the result of this path's transformation as a new path.
	[[nodiscard]] Path transformed( const glm::mat3x3 &transform ) const { return transformed( toMat3x2( transform ) ); }
	//! Returns the result of this path's transformation as a new path.
	[[nodiscard]] Path transformed( const glm::mat4x4 &transform ) const { return transformed( toMat3x2( transform ) ); }

	//! Reverses the order of the points and segments, effectively changing the winding. NOT THOROUGHLY TESTED, USE WITH CARE!
	void reverse() const;

  protected:
	static GLubyte toPathCommand( Path2d::SegmentType type );

	GLuint mPathId{ 0 };
};

//! Stores font faces and shaders so they can be easily reused by other parts of your code.
class Cache {
	std::unordered_map<Shader::Type, ShaderRef> mShaders;

  public:
	static Cache &get()
	{
		thread_local static Cache instance;
		return instance;
	}

	static ShaderRef loadShader( Shader::Type type );

	static void clean();

	static void clear();

  private:
	Cache() = default;
};

//! Sets up a canvas to render NvPath graphics to.
class Canvas {
  public:
	Canvas( int samples = 8, int coverageSamples = 16, bool useFloats = false );
	Canvas( int width, int height, int samples = 8, int coverageSamples = 16, bool useFloats = false );

	explicit Canvas( const ivec2 &size, int samples = 8, int coverageSamples = 16, bool useFloats = false )
		: Canvas( size.x, size.y, samples, useFloats )
	{
	}

	//! Binds the canvas. Please consider using ScopedCanvas instead.
	void bind() const;
	//! Unbinds the canvas. Please consider using ScopedCanvas instead.
	void unbind() const;
	//! Returns whether the canvas is currently bound.
	bool isBound() const { return mIsBound; }

	//! Draws the canvas.
	void draw() const;
	//! Draws the canvas.
	void draw( const Rectf &bounds ) const;

	//! Returns the width of the canvas in pixels.
	int32_t getWidth() const { return mWidth; }
	//! Returns the height of the canvas in pixels.
	int32_t getHeight() const { return mHeight; }
	//! Returns the size of the canvas in pixels.
	ivec2 getSize() const { return { mWidth, mHeight }; }
	//! Returns the bounds of the canvas in pixels.
	Area getBounds() const { return { 0, 0, mWidth, mHeight }; }

	//! Returns the number of samples used per pixel.
	int getSamples() const { return getFboFormat().getSamples(); }

	//! Returns the canvas texture if it exists, otherwise returns nullptr.
	gl::Texture2dRef getTexture( GLenum attachment = GL_COLOR_ATTACHMENT0 ) const;
	//! Returns the frame buffer format for this canvas.
	gl::Fbo::Format getFboFormat() const;

	//! Sets the size of the canvas in pixels.
	void resize( const ivec2 &size );

  private:
	int32_t              mWidth{ 640 };          //
	int32_t              mHeight{ 480 };         //
	int                  mSamples{ 8 };          //
	int                  mCoverageSamples{ 16 }; //
	bool                 mUseFloats{ false };    //
	mutable gl::Context *mCtx = nullptr;         //
	mutable gl::FboRef   mFbo;                   //
	mutable bool         mIsBound = false;       //
};

class ScopedCanvas {
	const Canvas &mCanvas;

  public:
	explicit ScopedCanvas( const Canvas &canvas )
		: mCanvas( canvas )
	{
		mCanvas.bind();
	}
	~ScopedCanvas() { mCanvas.unbind(); }

	ScopedCanvas( const ScopedCanvas & ) = delete;
	ScopedCanvas( ScopedCanvas && ) = delete;
	ScopedCanvas &operator=( const ScopedCanvas & ) = delete;
	ScopedCanvas &operator=( ScopedCanvas && ) = delete;
};

//! Returns whether NV Path Rendering is available on this system.
CI_API inline bool hasNvPathRendering()
{
	assert( ci::gl::context() ); // We must have an active OpenGL context first!
	return bool( GLAD_GL_NV_path_rendering );
}

} // namespace nvpath
} // namespace cinder