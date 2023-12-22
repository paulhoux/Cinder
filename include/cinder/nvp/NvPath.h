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
#include "cinder/Shape2d.h"
#include "cinder/gl/Context.h"
#include "cinder/gl/Fbo.h"
#include "cinder/svg/Svg.h"

namespace cinder {

namespace nvp {

// Forward declarations.
class Cache;
class Canvas;
class ClipRect;
class Face;
class Gradients;
class Path;
class Shader;
class Svg;

using CanvasRef = std::shared_ptr<Canvas>;
using PathRef = std::shared_ptr<Path>;
using FaceRef = std::shared_ptr<Face>;
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
CapsStyle toCapsStyle( ci::svg::LineCap lineCap );
//!
JoinStyle toJoinStyle( ci::svg::LineJoin lineJoin );

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

//! Stores multiple gradients in a single texture for performance.
class Gradients {
  public:
	Gradients() = default;
	~Gradients() = default;

	Gradients( const Gradients & ) = delete;
	Gradients( Gradients && ) = default;
	Gradients &operator=( const Gradients & ) = delete;
	Gradients &operator=( Gradients && ) = default;

	//!
	bool empty() const { return mLookUp.empty(); }
	//!
	void clear()
	{
		mIndex = 0;
		mLookUp.clear();
		mTexture.reset();
	}
	//!
	size_t size() const { return mLookUp.size(); }

	//! Returns whether the gradient is known.
	bool contains( const std::string &id ) const;
	//! Returns the coordinate of the gradient in the texture.
	float index( const std::string &id ) const;

	//! Sets or updates the gradient.
	void set( const svg::Gradient &gradient );
	//! Sets or updates the gradient.
	void set( const svg::Paint &paint );

	//!
	void setSpreadMethod( svg::SpreadMethod method ) const;

	//!
	void bind( gl::Context *ctx, uint8_t textureUnit = 0 );
	//!
	void unbind( gl::Context *ctx );

  private:
	//!
	gl::Texture2dRef create( int width, int height ) const;
	//!
	void store( size_t index, const svg::Paint &paint ) const;

	gl::Context                            *mCtx = nullptr;
	size_t                                  mIndex{ 0 };
	std::unordered_map<std::string, size_t> mLookUp{};
	mutable gl::Texture2dRef                mTexture{};
	uint8_t                                 mTextureUnit{ 0 };
};

//! Path represents a vector shape stored efficiently on the GPU.
CI_API class Path {
  public:
	virtual ~Path();

	Path();
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
	//! Returns the path's bounding box, calculated from the actual shape.
	[[nodiscard]] virtual Rectf getFillBounds() const;
	//! Returns the path's bounding box, calculated from the actual shape and adjusted for stroke width.
	[[nodiscard]] virtual Rectf getStrokeBounds() const;

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
	virtual void stencilStroke( GLuint stencilMask = 0xFF );
	//! Renders the path to the stencil buffer but does not cover the path.
	//! Use the `fill()` methods to stencil and cover the path in a single step.
	virtual void stencilFill( GLuint stencilMask = 0xFF, GLenum fillMode = GL_COUNT_UP_NV );

	//! Covers the paths that have already been rendered to the stencil buffer using a solid \a color. Clears the affected region of the stencil buffer by default, but this can be overridden.
	//! Use the `fill()` or `stroke()` methods to stencil and cover the path in a single step.
	virtual void cover( const ColorA &color, bool clearStencil = true );
	//! Covers the paths that have already been rendered to the stencil buffer using a solid \a color. Clears the affected region of the stencil buffer by default, but this can be overridden.
	virtual void cover( const ColorA &color, const Rectf &bounds, bool clearStencil = true );
	//! Covers the paths that have already been rendered to the stencil buffer using the provided \a texture. Clears the affected region of the stencil buffer by default, but this can be overridden.
	virtual void cover( const gl::Texture2dRef &texture, const Rectf &bounds, bool clearStencil = true );

	//! Strokes the path with a solid \a color.
	virtual void stroke( const ColorA &color, bool clearStencil = true ) const;
	//! Strokes the path with the specified \a paint and \a opacity.
	virtual void stroke( const svg::Paint &paint, float opacity = 1, bool clearStencil = true ) const;

	//! Strokes the path instances with a solid \a color and the specified \a caps and \a join styles.
	virtual void strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color, bool clearStencil = true );
	//! Strokes the path instances with a solid \a color and the specified \a caps and \a join styles.
	virtual void strokeInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, bool clearStencil = true );

	//! Fills the path with a solid \a color.
	virtual void fill( const ColorA &color, bool clearStencil = true ) const;
	//! Fills the path with the specified \a paint and \a opacity.
	virtual void fill( const svg::Paint &paint, float opacity = 1, bool clearStencil = true ) const;
	//! Fills the path with a \a texture, automatically centered within the path's bounding box.
	virtual void fill( const gl::TextureRef &texture, bool clearStencil = true ) const { fill( texture, getFillBounds(), clearStencil ); }
	//! Fills the path with a \a texture, automatically centered within the specified \a bounding box.
	virtual void fill( const gl::TextureRef &texture, const Rectf &bounds, bool clearStencil = true ) const;
	//! Fills the path with a \a texture.
	virtual void fill( const gl::TextureRef &texture, const vec2 &upperLeftTexCoord, const vec2 &lowerRightTexCoord, bool clearStencil = true ) const;

	//! Fills the path instances with a solid \a color.
	virtual void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color, bool clearStencil = true ) const;
	//! Fills the path instances with a solid \a color.
	virtual void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color, bool clearStencil = true ) const;

	//! Creates a new path by adding paths together.
	[[nodiscard]] Path operator+( const Path &other ) const;

	//! Adds the \a other path to our path.
	Path &operator+=( const Path &other );

	//! Transforms the coordinates of our path.
	void transform( const glm::mat3x2 &transform ) const;

	//! Returns the result of this path's transformation as a new path.
	[[nodiscard]] Path transformed( const glm::mat3x2 &transform ) const;

	//! Reverses the order of the points and segments, effectively changing the winding. NOT THOROUGHLY TESTED, USE WITH CARE!
	void reverse() const;

	//!
	static void bindGradients( gl::Context *ctx, uint8_t textureUnit = 0 ) { sGradients.bind( ctx, textureUnit ); }
	//!
	static void unbindGradients( gl::Context *ctx ) { sGradients.unbind( ctx ); }

  protected:
	static Shader::Type preparePaint( const svg::Paint &paint, float opacity = 1, bool prepareShader = false );
	static Shader::Type prepareLinearGradient( const svg::Paint &paint, float opacity = 1, bool prepareShader = false );
	static Shader::Type prepareRadialGradient( const svg::Paint &paint, float opacity = 1, bool prepareShader = false );

	static GLubyte toPathCommand( Path2d::SegmentType type );

	GLuint mPathId{ 0 };

	//! Stores gradients into a single texture per thread.
	inline static thread_local Gradients sGradients{};
};

//! Represents a font face stored efficiently on the GPU as a list of glyph paths.
CI_API class Face {
  public:
	static FaceRef create( const Font &font ) { return std::make_shared<Face>( font ); }

	Face() = default;

	Face( const Face & ) = delete;
	Face( Face && ) = default;
	Face &operator=( const Face & ) = delete;
	Face &operator=( Face && ) = default;

	explicit Face( const Font &font );

	~Face();

	//! Returns the base path id.
	[[nodiscard]] GLuint getBaseId() const { return mBaseId; }
	//! Returns the base path id, pointing to the first glyph of the font. Returns zero if no id was assigned.
	[[nodiscard]] GLuint getId( uint32_t index = 0 ) const { return mBaseId > 0 ? mBaseId + index : 0; }
	//! Returns the number of glyphs defined for this face.
	[[nodiscard]] GLsizei getNumGlyphs() const { return mNumGlyphs; }

	static constexpr float DEFAULT_SIZE = 1;

  private:
	//! Sets the stroke style for all glyphs.
	void setStrokeStyle( float width, JoinStyle joinStyle, CapsStyle capsStyle ) const;

	//!
	void createPaths() const;

	const Font mFont;           //
	GLuint     mBaseId{ 0 };    //
	GLsizei    mNumGlyphs{ 0 }; //
};

class CI_API Svg : public svg::Renderer {
  public:
	Svg() = default;

	explicit Svg( const DataSourceRef &src );
	explicit Svg( const svg::DocRef &svg );

	float        getWidth() const { return mBounds.getWidth(); }
	float        getHeight() const { return mBounds.getHeight(); }
	vec2         getSize() const { return mBounds.getSize(); }
	const Rectf &getBounds() const { return mBounds; }

	//! Clears rendering cache.
	void clear() override;

  private:
	// svg::Renderer callbacks.

	void start() override;
	void finish() override;
	void pushGroup( const svg::Group &group, float opacity ) override;
	void popGroup() override;
	void pushClipPath( const svg::ClipPath &clippath ) override;
	void popClipPath() override;
	void drawPath( const svg::Path & ) override;
	void drawPolyline( const svg::Polyline & ) override;
	void drawPolygon( const svg::Polygon & ) override;
	void drawLine( const svg::Line & ) override;
	void drawRect( const svg::Rect & ) override;
	void drawCircle( const svg::Circle & ) override;
	void drawEllipse( const svg::Ellipse & ) override;
	void drawImage( const svg::Image & ) override;
	void drawTextSpan( const svg::TextSpan & ) override { /* currently not implemented */ }
	void pushMatrix( const mat3 & ) override;
	void popMatrix() override;
	void pushFill( const svg::Paint & ) override;
	void popFill() override;
	void pushStroke( const svg::Paint & ) override;
	void popStroke() override;
	void pushFillOpacity( float ) override;
	void popFillOpacity() override;
	void pushStrokeOpacity( float ) override;
	void popStrokeOpacity() override;
	void pushStrokeWidth( float ) override;
	void popStrokeWidth() override;
	void pushFillRule( svg::FillRule ) override;
	void popFillRule() override;
	void pushLineCap( svg::LineCap ) override;
	void popLineCap() override;
	void pushLineJoin( svg::LineJoin ) override;
	void popLineJoin() override;
	void pushMiterLimit( float miterLimit ) override;
	void popMiterLimit() override;
	void pushDashArray( const std::vector<float> &dashArray ) override;
	void popDashArray() override;
	void pushDashOffset( float dashOffset ) override;
	void popDashOffset() override;
	void pushTextPen( const vec2 & ) override;
	void popTextPen() override;
	void pushTextRotation( float ) override;
	void popTextRotation() override;

	//! Returns whether the current style has fill or stroke enabled.
	bool shouldRender() const { return !( mStacks.fill.back().isNone() && mStacks.stroke.back().isNone() ); }
	//! Generates a draw call for visible paths.
	void render( const Path &path );
	//! Fills the path with the specified \a paint and \a opacity.
	void fill( GLuint pathId, const svg::Paint &paint, float opacity );
	//! Strokes the path with the specified \a paint and \a opacity.
	void stroke( GLuint pathId, const svg::Paint &paint, float opacity );
	//! Strokes the instances with the specified \a paint and \a opacity.
	void fillInstanced( GLuint baseId, GLsizei count, const glm::mat3x2 *transforms, const uint32_t *indices, const svg::Paint &paint, float opacity );
	//! Strokes the instances with the specified \a paint and \a opacity.
	void strokeInstanced( GLuint baseId, GLsizei count, const glm::mat3x2 *transforms, const uint32_t *indices, const svg::Paint &paint, float opacity );

	//! Returns whether the path with the specified \a uuid exists and returns a pointer to the path if it exists.
	const Path *findPath( size_t uuid ) const;
	//! Caches a path using the specified \a uuid and \a path. Returns a pointer to the new path.
	const Path *insertPath( size_t uuid, const Path2d &path, bool isClipPath = false );
	//! Caches a path using the specified \a uuid and \a shape. Returns a pointer to the new path.
	const Path *insertPath( size_t uuid, const Shape2d &shape, bool isClipPath = false );

	//! Returns whether the paint is cached and set \a index to the position of the paint in the cache.
	bool findPaint( const svg::Paint &paint, size_t &index ) const;
	//! Stores the paint in the cache and returns the index to the position of the paint in the cache.
	size_t insertPaint( const svg::Paint &paint );

	//!  Creates or activates a linear gradient. Optionally prepares the correct shader as well.
	Shader::Type prepareLinearGradient( const svg::Paint &paint, float opacity = 1, bool prepareShader = false );
	//!  Creates or activates a radial gradient. Optionally prepares the correct shader as well.
	Shader::Type prepareRadialGradient( const svg::Paint &paint, float opacity = 1, bool prepareShader = false );
	//! Creates or activates a gradient. Optionally prepares the correct shader as well.
	Shader::Type preparePaint( const svg::Paint &paint, float opacity = 1, bool prepareShader = false );

	//! Returns the stencil mask based on the current fill rule.
	GLuint getStencilMask() const { return mStacks.fillRule.back() == svg::FILL_RULE_EVEN_ODD ? 0x01 : 0xFF; }

	svg::DocRef                                  mDoc;
	gl::Context                                 *mCtx = nullptr;
	Stacks                                       mStacks;
	Rectf                                        mBounds;
	Gradients                                    mGradients;
	std::unordered_map<GLuint, gl::Texture2dRef> mTextures;
	std::unordered_map<GLuint, Path>             mPaths;
	std::vector<svg::Paint>                      mPaints;
};

//! Stores font faces and shaders so they can be easily reused by other parts of your code.
class Cache {
	// std::unordered_map<std::string, FaceRef>    mFaces;
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

//! Represents a rectangular region in 2D space outside of which no content should be drawn. Transformations are respected.
class CI_API ClipRect {
  public:
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

//! Returns whether NV Path Rendering is available on this system.
CI_API inline bool hasNvPathRendering()
{
	assert( ci::gl::context() ); // We must have an active OpenGL context first!
	return bool( GLAD_GL_NV_path_rendering );
}

//! Renders text using NV Path Rendering. Make sure a stencil buffer is available and cleared before calling, or alternatively use an nvp::Canvas to render to.
// CI_API void renderText( const text::Typesetter &typesetter, const vec2 &offset = vec2() );

//!
CI_API static CoordinateSpace toGradientUnits( std::string style );
//!
CI_API static SpreadMethod toSpreadMethod( std::string style );

//!
CI_API inline glm::mat3x2 toMat3x2( const glm::mat3x3 &m )
{
	return glm::mat3x2{ m[0][0], m[0][1], m[1][0], m[1][1], m[2][0], m[2][1] };
}

} // namespace nvp
} // namespace cinder