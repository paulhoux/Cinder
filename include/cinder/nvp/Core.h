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

#pragma once

#include "cinder/gl/Context.h"

namespace cinder {

// Forward declaration.
namespace text {
class Typesetter;
}

namespace nvp {

CI_API enum class CapsStyle { FLAT = GL_FLAT, SQUARE = GL_SQUARE_NV, ROUND = GL_ROUND_NV, TRIANGULAR = GL_TRIANGULAR_NV, DEFAULT = GL_FLAT };

CI_API enum class JoinStyle { ROUND = GL_ROUND_NV, BEVEL = GL_BEVEL_NV, MITER_REVERT = GL_MITER_REVERT_NV, MITER_TRUNCATE = GL_MITER_TRUNCATE_NV, DEFAULT = GL_MITER_REVERT_NV };

CI_API enum class PathStyle { MOVETO_RESETS = GL_MOVE_TO_RESETS_NV, MOVETO_CONTINUES = GL_MOVE_TO_CONTINUES_NV, DEFAULT = GL_MOVE_TO_RESETS_NV };

CI_API inline bool hasNvPathRendering()
{
	assert( ci::gl::context() ); // We must have an active OpenGL context first!

	return bool( GLAD_GL_NV_path_rendering );
}

CI_API inline void color3fv( const ci::Color &color )
{
	glColor3fv( &color.r );
}
CI_API inline void color4fv( const ci::ColorA &color )
{
	glColor4fv( &color.r );
}
CI_API inline void color3ubv( const ci::Color8u &color )
{
	glColor3ubv( &color.r );
}
CI_API inline void color4ubv( const ci::ColorA8u &color )
{
	glColor4ubv( &color.r );
}

//!
CI_API inline float areaOfPolygon( const ci::vec2 *points, size_t count )
{
	float area = 0.0f;
	for( size_t i = 0, j = count - 1; i < count; ++i, j = i - 1 ) {
		area += points[j].x * points[i].y - points[i].x * points[j].y;
	}
	return 0.5f * area;
}

//!
CI_API void renderText( const text::Typesetter &typesetter, const vec2 &offset = vec2() );

//!
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

	void set( int x1, int y1, int x2, int y2 )
	{
		mBounds.x1 = x1;
		mBounds.y1 = y1;
		mBounds.x2 = x2;
		mBounds.y2 = y2;
	}

	void set( const Area &bounds ) { mBounds = bounds; }

	bool push();

	bool pop();

  private:
	Area             mBounds;
	ci::gl::Context *mCtx = nullptr;
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

//! Debug marker which can be inserted into the render queue.
struct CI_API Marker : private Noncopyable {
	static void push( const char *message ) { glPushDebugGroup( GL_DEBUG_SOURCE_APPLICATION, mId++, -1, message ); }
	static void pop() { glPopDebugGroup(); }

	__declspec( thread ) inline static unsigned int mId = 0;
};

//! Debug marker which can be inserted into the render queue.
struct CI_API ScopedMarker : private Noncopyable {
	explicit ScopedMarker( const char *message ) { Marker::push( message ); }
	explicit ScopedMarker( const std::string &message ) { Marker::push( message.c_str() ); }
	~ScopedMarker() { Marker::pop(); }
};

using ShaderRef = std::shared_ptr<struct Shader>;

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

	void bind() const;
	void unbind() const;

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

} // namespace nvp
} // namespace cinder