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
#include "cinder/svg/Svg.h"

namespace cinder {

// Forward declarations.
namespace text {
class Face;
class AttrString;
class Typesetter;
} // namespace text

namespace nvp {

// Forward declarations.
class Cache;
class Canvas;
class ClipRect;
class Face;
class Gradient;
class Gradients;
class Path;
class Shader;

using CanvasRef = std::shared_ptr<Canvas>;
using PathRef = std::shared_ptr<Path>;
using FaceRef = std::shared_ptr<Face>;
using GradientRef = std::shared_ptr<Gradient>;
using ShaderRef = std::shared_ptr<Shader>;

//!
CI_API enum class CapsStyle { FLAT = GL_FLAT, SQUARE = GL_SQUARE_NV, ROUND = GL_ROUND_NV, TRIANGULAR = GL_TRIANGULAR_NV, DEFAULT = GL_FLAT };
//!
CI_API enum class JoinStyle { ROUND = GL_ROUND_NV, BEVEL = GL_BEVEL_NV, MITER_REVERT = GL_MITER_REVERT_NV, MITER_TRUNCATE = GL_MITER_TRUNCATE_NV, DEFAULT = GL_MITER_REVERT_NV };
//!
CI_API enum class PathStyle { MOVETO_RESETS = GL_MOVE_TO_RESETS_NV, MOVETO_CONTINUES = GL_MOVE_TO_CONTINUES_NV, DEFAULT = GL_MOVE_TO_RESETS_NV };
//! Defines the coordinate system used by the gradient.
enum class GradientUnits { OBJECT_BOUNDING_BOX = GL_PATH_OBJECT_BOUNDING_BOX_NV, USER_SPACE_ON_USE = GL_OBJECT_LINEAR_NV, DEFAULT = GL_PATH_OBJECT_BOUNDING_BOX_NV };
//! Defines the spread method used by the gradient.
enum class GradientSpreadMethod { PAD = GL_CLAMP_TO_EDGE, REFLECT = GL_MIRRORED_REPEAT, REPEAT = GL_REPEAT, DEFAULT = PAD };

//!
CapsStyle toCapsStyle( ci::svg::LineCap lineCap );
//!
JoinStyle toJoinStyle( ci::svg::LineJoin lineJoin );

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
	//! Sets the path's end caps for strokes.
	void setEndCaps( CapsStyle caps ) const;
	//! Sets the join style for strokes.
	void setJoinStyle( JoinStyle joins ) const;
	//! Sets the stroke width.
	void setStrokeWidth( float width ) const;
	//! Sets the miter limit for stokes.
	void setMiterLimit( float limit ) const;

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
	virtual void fill( const ColorA &color ) const;
	//! Fills the path with a \a texture, automatically centered within the path's bounding box.
	virtual void fill( const gl::TextureRef &texture ) const { fill( texture, getBounds() ); }
	//! Fills the path with a \a texture, automatically centered within the specified \a bounding box.
	virtual void fill( const gl::TextureRef &texture, const Rectf &bounds ) const;
	//! Fills the path with a \a texture.
	virtual void fill( const gl::TextureRef &texture, const vec2 &upperLeftTexCoord, const vec2 &lowerRightTexCoord ) const;

	//! Fills the path instances with a solid \a color.
	virtual void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat3x2> &transforms, const ColorA &color ) const;
	//! Fills the path instances with a solid \a color.
	virtual void fillInstanced( const std::vector<GLuint> &paths, const std::vector<glm::mat4x3> &transforms, const ColorA &color ) const;

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

  protected:
	static GLubyte toPathCommand( Path2d::SegmentType type );

	GLuint mPathId{ 0 };
};

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

//!
class Gradient {
  public:
	class Stop {
		float    mOffset{ 0 }; // normalized 0-1
		ColorA8u mColor;

	  public:
		Stop() = default;
		Stop( float t, const ColorA8u &color, unsigned char opacity = 255 )
			: mOffset{ clamp( t, 0.0f, 1.0f ) }
			, mColor{ color }
		{
			mColor.a = mColor.a * opacity / 255;
		}
		Stop( float t, const Color8u &color, unsigned char opacity = 255 )
			: mOffset{ clamp( t, 0.0f, 1.0f ) }
			, mColor{ color, unsigned char( 255 * opacity ) }
		{
		}
		Stop( float t, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255 )
			: mOffset{ clamp( t, 0.0f, 1.0f ) }
			, mColor{ r, g, b, a }
		{
		}
		~Stop() = default;

		Stop( const Stop & ) = default;
		Stop( Stop && ) noexcept = default;
		Stop &operator=( const Stop & ) = default;
		Stop &operator=( Stop && ) noexcept = default;

		float offset() const { return mOffset; }

		const ColorA8u &color() const { return mColor; }

		bool operator<( const Stop &other ) const { return offset() < other.offset(); }
		bool operator==( const Stop &other ) const { return approxEqual( offset(), other.offset() ) && mColor == other.mColor; }
		bool operator!=( const Stop &other ) const { return !( *this == other ); }
	};

  public:
	Gradient() = default;
	virtual ~Gradient() = default;

	Gradient( const Gradient & ) = default;
	Gradient( Gradient && ) = default;
	Gradient &operator=( const Gradient & ) = default;
	Gradient &operator=( Gradient && ) = default;

	bool operator==( const Gradient &other ) const { return mStops == other.mStops; }
	bool operator!=( const Gradient &other ) const { return !( *this == other ); }

	//!
	virtual const std::string &getId() const = 0;
	//!
	const mat3 &getTransform() const { return mTransform; }
	//!
	const auto &getUnits() const { return mUnits; }
	//!
	const auto &getSpread() const { return mSpread; }

	//! Returns the (interpolated) color at position \a t. Does not pre-multiply the RGB values.
	ColorA8u at( float t ) const;

	//! Returns the nearest stop lower than position \a t.
	const Stop &floor( float t ) const;

	//! Returns the nearest stop higher than position \a t.
	const Stop &ceil( float t ) const;

	//! Inserts a \a color at position \a t.
	Gradient &stop( float t, const ColorA8u &color );
	//! Inserts a \a color at position \a t.
	Gradient &stop( float t, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255 );
	//! Inserts a \a color at position \a t.
	Gradient &stop( float t, const ColorA &color ) { return stop( t, ColorA8u( color ) ); }
	//! Inserts a \a color at position \a t.
	Gradient &stop( float t, float r, float g, float b, float a = 1 );

	//! Inserts a \a color at offset \a t. If an additional \a opacity is given, it will be multiplied with the color's alpha value.
	void insert( float t, const ColorA8u &color, unsigned char opacity = 255 ) { insert( Stop{ t, color, opacity } ); }
	//! Inserts a \a color at offset \a t.
	void insert( float t, const Color8u &color, unsigned char opacity = 255 ) { insert( Stop{ t, color, opacity } ); }
	//! Inserts a \a color at offset \a t. If an additional \a opacity is given, it will be multiplied with the color's alpha value.
	void insert( float t, const ColorA &color, float opacity = 1 ) { insert( Stop{ t, ColorA8u( color ), static_cast<unsigned char>( 255 * opacity ) } ); }
	//! Inserts a \a color at offset \a t.
	void insert( float t, const Color &color, float opacity = 1 ) { insert( Stop{ t, Color8u( color ), static_cast<unsigned char>( 255 * opacity ) } ); }
	//! Inserts a stop.
	void insert( const Stop &stop );
	//! Removes all stops.
	void clear() { mStops.clear(); }
	//! Adds all stops of the \a other gradient without discarding existing stops.
	void add( const Gradient &other );
	//! Replaces all stops with those of the \a other gradient.
	void replace( const Gradient &other ) { mStops = other.mStops; }

	bool   empty() const { return mStops.empty(); }
	size_t size() const { return mStops.size(); }

	auto begin() const { return mStops.begin(); }
	auto end() const { return mStops.end(); }

	auto rbegin() const { return mStops.rbegin(); }
	auto rend() const { return mStops.rend(); }

	//! Returns raw 8-bit RGBA data. You can use this to construct a Surface.
	std::unique_ptr<uint8_t[]> data( int32_t width, int32_t height, float from = 0.0f, float to = 1.0f ) const;

  protected:
	mat3                 mTransform;
	GradientUnits        mUnits{ GradientUnits::DEFAULT };
	GradientSpreadMethod mSpread{ GradientSpreadMethod::DEFAULT };

  private:
	std::vector<Stop> mStops;
};

using LinearGradientRef = std::shared_ptr<class LinearGradient>;

class LinearGradient : public Gradient {
  public:
	static LinearGradientRef create( const char *id ) { return std::make_shared<LinearGradient>( id ); }

	explicit LinearGradient( std::string id )
		: mId{ std::move( id ) }
	{
	}
	explicit LinearGradient( const char *id )
		: mId{ id }
	{
	}
	explicit LinearGradient( const GradientRef &other );

	const std::string &getId() const override { return mId; }

	const auto &getX1() const { return mX1; }
	const auto &getY1() const { return mY1; }
	const auto &getX2() const { return mX2; }
	const auto &getY2() const { return mY2; }

	LinearGradientRef clone() { return std::make_shared<LinearGradient>( *this ); }

	LinearGradient &id( const std::string &id )
	{
		mId = id;
		return *this;
	}
	LinearGradient &transform( const mat3 &transform )
	{
		mTransform = transform;
		return *this;
	}
	LinearGradient &units( GradientUnits units )
	{
		mUnits = units;
		return *this;
	}
	LinearGradient &spread( GradientSpreadMethod spread )
	{
		mSpread = spread;
		return *this;
	}
	LinearGradient &from( float x1, float y1 )
	{
		mX1 = x1;
		mY1 = y1;
		return *this;
	}
	LinearGradient &from( const vec2 &pt )
	{
		mX1 = pt.x;
		mY1 = pt.y;
		return *this;
	}
	LinearGradient &to( float x2, const float y2 )
	{
		mX2 = x2;
		mY2 = y2;
		return *this;
	}
	LinearGradient &to( const vec2 &pt )
	{
		mX2 = pt.x;
		mY2 = pt.y;
		return *this;
	}

	LinearGradient &operator<<( const LinearGradient &other );

  private:
	std::string mId;
	float       mX1{ 0 };
	float       mY1{ 0 };
	float       mX2{ 1 };
	float       mY2{ 0 };
};

using RadialGradientRef = std::shared_ptr<class RadialGradient>;

class RadialGradient : public Gradient {
  public:
	static RadialGradientRef create( const char *id ) { return std::make_shared<RadialGradient>( id ); }

	explicit RadialGradient( std::string id )
		: mId{ std::move( id ) }
	{
	}
	explicit RadialGradient( const char *id )
		: mId{ id }
	{
	}
	explicit RadialGradient( const GradientRef &other );

	const std::string &getId() const override { return mId; }

	const auto &getR() const { return mR; }
	const auto &getCx() const { return mCx; }
	const auto &getCy() const { return mCy; }
	const auto &getFr() const { return mFr; }
	const auto &getFx() const { return mFx; }
	const auto &getFy() const { return mFy; }

	RadialGradientRef clone() { return std::make_shared<RadialGradient>( *this ); }

	RadialGradient &id( const std::string &id )
	{
		mId = id;
		return *this;
	}
	RadialGradient &transform( const mat3 &transform )
	{
		mTransform = transform;
		return *this;
	}
	RadialGradient &units( GradientUnits units )
	{
		mUnits = units;
		return *this;
	}
	RadialGradient &spread( GradientSpreadMethod spread )
	{
		mSpread = spread;
		return *this;
	}
	RadialGradient &radius( float r )
	{
		mR = r;
		return *this;
	}
	RadialGradient &center( float cx, float cy )
	{
		mCx = cx;
		mCy = cy;
		return *this;
	}
	RadialGradient &center( const vec2 &center )
	{
		mCx = center.x;
		mCy = center.y;
		return *this;
	}
	RadialGradient &focal( float fx, float fy, float fr = 0 )
	{
		mFx = fx;
		mFy = fy;
		mFr = fr;
		return *this;
	}
	RadialGradient &focal( const vec2 &focal, float fr = 0 )
	{
		mFx = focal.x;
		mFy = focal.y;
		mFr = fr;
		return *this;
	}

	RadialGradient &operator<<( const RadialGradient &other );

  private:
	std::string mId;
	float       mR{ 0.5f };  // Defaults to 50%.
	float       mCx{ 0.5f }; // Defaults to 50%.
	float       mCy{ 0.5f }; // Defaults to 50%.
	float       mFr{ 0 };    // Defaults to 0%.
	float       mFx{ mCx };
	float       mFy{ mCy };
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
		mGradients.clear();
		mLookUp.clear();
		mDirty.clear();
		mTexture.reset();
	}
	//!
	size_t size() const { return mLookUp.size(); }

	//! Returns a pointer to the gradient, if it exists. Returns nullptr otherwise.
	const GradientRef &at( const std::string &id ) const;
	//! Returns the coordinate of the gradient in the texture.
	float index( const std::string &id ) const;

	//! Sets or updates the gradient.
	void set( const GradientRef &gradient );

	//! Returns the texture containing all gradients. If no texture was created or if any of the gradients have been updated,
	//! the texture will be (re)created in this call.
	gl::Texture2dRef getTexture() const;

  private:
	gl::Texture2dRef create( int width, int height ) const;

	std::vector<GradientRef>                mGradients{};
	std::unordered_map<std::string, size_t> mLookUp{};
	mutable std::set<size_t>                mDirty{};
	mutable gl::Texture2dRef                mTexture{};
};

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
			gl::programPathFragmentInputGenNV( mShader->mProgram, 0, GL_CONSTANT_NV, 4, color.premultiplied().ptr() );
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

//!
class CI_API Svg {
  public:
	Svg() = default;

	explicit Svg( const DataSourceRef &src );
	explicit Svg( const svg::DocRef &svg );

	float getWidth() const { return mDoc ? mDoc->getWidth() : 0.0f; }
	float getHeight() const { return mDoc ? mDoc->getHeight() : 0.0f; }
	vec2  getSize() const { return mDoc ? mDoc->getSize() : vec2{}; }
	Rectf getBounds() const { return mDoc ? mDoc->getBounds() : Rectf{}; }

	//! Returns whether any paths are defined.
	bool empty() const { return mPaths.empty(); }
	//! Returns the number of paths.
	size_t size() const { return mPaths.size(); }

	void draw();

  private:
	//!
	nvp::Shader::Type prepareLinearGradient( const svg::Paint &paint, float opacity );
	//!
	nvp::Shader::Type prepareRadialGradient( const svg::Paint &paint, float opacity );
	//!
	nvp::Shader::Type preparePaint( const svg::Paint &paint, float opacity );

	class Renderer : public svg::Renderer {
	  public:
		Renderer( Svg *svg );

		void start() override {}
		void finish() override {}
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
		void drawTextSpan( const svg::TextSpan & ) override {}
		void pushMatrix( const mat3 & ) override;
		void popMatrix() override;
		// void pushStyle( const svg::Style & ) override;
		// void popStyle() override;
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
		void pushTextPen( const vec2 & ) override {}
		void popTextPen() override {}
		void pushTextRotation( float ) override {}
		void popTextRotation() override {}

	  private:
		//!
		svg::Style getCurrentStyle() const;
		//!
		bool shouldRender() const { return !( mFillStack.back().isNone() && mStrokeStack.back().isNone() ); }
		//!
		void render( const Shape2d &shape ) const;

		Svg *                           mSvg = nullptr;
		std::vector<mat3>               mMatrixStack;
		std::vector<svg::Paint>         mFillStack, mStrokeStack;
		std::vector<float>              mFillOpacityStack, mStrokeOpacityStack;
		std::vector<float>              mGroupOpacityStack;
		std::vector<float>              mStrokeWidthStack;
		std::vector<svg::FillRule>      mFillRuleStack;
		std::vector<svg::LineCap>       mLineCapStack;
		std::vector<svg::LineJoin>      mLineJoinStack;
		std::vector<float>              mMiterLimitStack;
		std::vector<std::vector<float>> mDashArrayStack;
		std::vector<float>              mDashOffsetStack;
		std::vector<svg::ClipPath>      mClipPathStack;
	};

	struct DrawCall {
		GLuint           pathId{ 0 };
		svg::Paint       fill;
		svg::Paint       stroke;
		gl::Texture2dRef image;
		float            fillOpacity{ 1 };
		float            strokeOpacity{ 1 };
		GLuint           fillRule{ 0xFF };
		mat3             matrix;
	};

	svg::DocRef           mDoc;
	Renderer              mRenderer{ this };
	Gradients             mGradients;
	std::vector<Path>     mPaths;
	std::vector<DrawCall> mDrawCalls;

	friend class Renderer;
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
CI_API void renderText( const text::Typesetter &typesetter, const vec2 &offset = vec2() );

//!
CI_API static GradientUnits toGradientUnits( std::string style );
//!
CI_API static GradientSpreadMethod toSpreadMethod( std::string style );

//!
CI_API inline glm::mat3x2 toMat3x2( const glm::mat3x3 &m )
{
	return glm::mat3x2{ m[0][0], m[0][1], m[1][0], m[1][1], m[2][0], m[2][1] };
}

} // namespace nvp
} // namespace cinder