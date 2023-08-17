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

#include "cinder/Path2d.h"
#include "cinder/PolyLine.h"
#include "cinder/Shape2d.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/Fbo.h"

namespace cinder {

// Forward declarations.
namespace text {
class Face;
class AttrString;
class Typesetter;
} // namespace text

namespace nvp {

//!
CI_API enum class CapsStyle { FLAT = GL_FLAT, SQUARE = GL_SQUARE_NV, ROUND = GL_ROUND_NV, TRIANGULAR = GL_TRIANGULAR_NV, DEFAULT = GL_FLAT };
//!
CI_API enum class JoinStyle { ROUND = GL_ROUND_NV, BEVEL = GL_BEVEL_NV, MITER_REVERT = GL_MITER_REVERT_NV, MITER_TRUNCATE = GL_MITER_TRUNCATE_NV, DEFAULT = GL_MITER_REVERT_NV };
//!
CI_API enum class PathStyle { MOVETO_RESETS = GL_MOVE_TO_RESETS_NV, MOVETO_CONTINUES = GL_MOVE_TO_CONTINUES_NV, DEFAULT = GL_MOVE_TO_RESETS_NV };

using PathRef = std::shared_ptr<class Path>;

//! Path represents a vector shape stored efficiently on the GPU.
CI_API class Path {
  public:
	virtual ~Path();

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
	void setPath( const std::vector<GLubyte> &commands, const std::vector<GLfloat> &coords ) const; /* non-const */

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
	virtual void stroke( const ColorA &color, float strokeWidth = 1 ) const { stroke( color, CapsStyle::DEFAULT, JoinStyle::DEFAULT, strokeWidth ); }
	//! Strokes the path with a solid \a color and the specified \a caps style.
	virtual void stroke( const ColorA &color, CapsStyle caps, float strokeWidth = 1 ) const { stroke( color, caps, JoinStyle::DEFAULT, strokeWidth ); }
	//! Strokes the path with a solid \a color and the specified \a join style.
	virtual void stroke( const ColorA &color, JoinStyle join, float strokeWidth = 1 ) const { stroke( color, CapsStyle::DEFAULT, join, strokeWidth ); }
	//! Strokes the path with a solid \a color and the specified \a caps and \a join styles.
	virtual void stroke( const ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth = 1 ) const;

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

	//! Fills the path instances with a solid \a color.
	virtual void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color );
	//! Fills the path instances with a solid \a color.
	virtual void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color );

	//! Creates a new path by adding paths together.
	[[nodiscard]] Path operator+( const Path &other ) const;

	//! Adds the \a other path to our path.
	Path &operator+=( const Path &other );

	//! Transforms the coordinates of our path.
	void transform( const glm::mat3x2 &transform ) const;

	//! Returns the result of this path's transformation as a new path.
	[[nodiscard]] Path transformed( const glm::mat3x2 &transform ) const;

  protected:
	Path() = default;

	static GLubyte toPathCommand( Path2d::SegmentType type );

	GLuint mPathId{ 0 };
};

using FaceRef = std::shared_ptr<class Face>;

//! Represents a font face stored efficiently on the GPU as a list of glyph paths.
CI_API class Face {
  public:
	static FaceRef create( const text::Face *face ) { return std::make_shared<Face>( face ); }

	Face() = default;

	Face( const Face & ) = delete;
	Face( Face && ) = default;
	Face &operator=( const Face & ) = delete;
	Face &operator=( Face && ) = default;

	explicit Face( const text::Face *face );

	~Face();

	//! Returns the base path id.
	[[nodiscard]] GLuint getBaseId() const { return mBaseId; }
	//! Returns the base path id, pointing to the first glyph of the font. Returns zero if no id was assigned.
	[[nodiscard]] GLuint getId( uint32_t index = 0 ) const { return mBaseId > 0 ? mBaseId + index : 0; }
	//! Returns the number of glyphs defined for this face.
	[[nodiscard]] GLsizei getNumGlyphs() const { return mNumGlyphs; }

  private:
	//! Sets the stroke style for all glyphs.
	void setStrokeStyle( float width, JoinStyle joinStyle, CapsStyle capsStyle ) const;

	//!
	void createPaths() const;

	const text::Face *mFace = nullptr; //
	GLuint            mBaseId{ 0 };    //
	GLsizei           mNumGlyphs{ 0 }; //
};

//! Represents a rectangular region in 2D space outside of which no content should be drawn. Transformations are respected.
struct CI_API ClipRect {
	ClipRect() = default;

	ClipRect( int x1, int y1, int x2, int y2 )
		: mBounds( x1, y1, x2, y2 )
	{
	}

	explicit ClipRect( const Area &bounds )
		: mBounds( bounds )
	{
	}

	const Area &get() const { return mBounds; }

	void set( int x1, int y1, int x2, int y2 ) { mBounds = { x1, y1, x2, y2 }; }

	void set( const Area &bounds ) { mBounds = bounds; }

	bool push();

	bool pop();

  private:
	Area         mBounds;
	gl::Context *mCtx = nullptr;
};

//!
struct CI_API ScopedClipRect : private Noncopyable {
	ScopedClipRect( const ivec2 &position, const ivec2 &dimension )
		: mClipRect( position.x, position.y, dimension.x, dimension.y )
	{
		mClipRect.push();
	}

	ScopedClipRect( int x, int y, int width, int height )
		: mClipRect{ x, y, x + width, y + height }
	{
		mClipRect.push();
	}

	explicit ScopedClipRect( const Area &bounds )
		: mClipRect{ bounds }
	{
		mClipRect.push();
	}

	~ScopedClipRect() { mClipRect.pop(); }

  private:
	ClipRect mClipRect;
};

using ShaderRef = std::shared_ptr<struct Shader>;

//! Shader for solid colors or gradients to be applied to paths.
struct CI_API Shader {
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
	explicit ScopedShader( const ColorA &color )
		: ScopedShader( Shader::Type::SOLID_COLOR )
	{
		if( mShader )
			gl::programPathFragmentInputGenNV( mShader->mProgram, 0, GL_CONSTANT_NV, 4, color.premultiplied().ptr() );
	}

	~ScopedShader()
	{
		if( mShader )
			mShader->unbind();
	}

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

	//! Only works for solid color shaders! TODO
	void setColor( const ColorA &color ) const
	{
		if( mShader && mShader->mType == Shader::Type::SOLID_COLOR )
			gl::programPathFragmentInputGenNV( mShader->mProgram, 0, GL_CONSTANT_NV, 4, color.premultiplied().ptr() );
	}

	//! Only works for linear gradient, radial gradient and image shaders! TODO
	void setCoords( GLenum genMode = GL_PATH_OBJECT_BOUNDING_BOX_NV, const mat3 &transform = {} ) const
	{
		// Sanity check.
		static_assert( sizeof( mat3::value_type ) == sizeof( GLfloat ) );

		if( mShader && ( mShader->mType == Shader::Type::LINEAR_GRADIENT || mShader->mType == Shader::Type::RADIAL_GRADIENT || mShader->mType == Shader::Type::IMAGE ) ) {
			const auto t = transpose( inverse( transform ) );
			gl::programPathFragmentInputGenNV( mShader->mProgram, 0, genMode, 2, value_ptr( t ) );
		}
	}

	//! Only works for linear gradient, radial gradient and image shaders! TODO
	void setCoords( GLenum genMode, const std::vector<float> &data ) const
	{
		assert( data.size() == 6 );

		if( mShader && ( mShader->mType == Shader::Type::LINEAR_GRADIENT || mShader->mType == Shader::Type::RADIAL_GRADIENT || mShader->mType == Shader::Type::IMAGE ) ) {
			gl::programPathFragmentInputGenNV( mShader->mProgram, 0, genMode, 2, data.data() );
		}
	}
};

//! Stores font faces and shaders so they can be easily reused by other parts of your code.
class Cache {
	std::unordered_map<std::string, FaceRef>    mFaces;
	std::unordered_map<Shader::Type, ShaderRef> mShaders;

  public:
	static Cache &get()
	{
		thread_local static Cache instance;
		return instance;
	}

	static FaceRef loadFace( const text::Face *face );

	static ShaderRef loadShader( Shader::Type type );

	static void clean();

	static void clear();

  private:
	Cache() = default;
};

using CanvasRef = std::shared_ptr<class Canvas>;

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

//! Renders text using NV Path Rendering. Make sure a stencil buffer is available and cleared before calling, or alternatively use an nvp::Canvas to render to.
CI_API void renderText( const text::Typesetter &typesetter, const vec2 &offset = vec2() );

} // namespace nvp
} // namespace cinder