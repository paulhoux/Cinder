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

#include "cinder/nvp/Core.h"

#include "cinder/Log.h"
#include "cinder/gl/gl.h"
#include "cinder/nvp/Cache.h"

namespace cinder {
namespace nvp {

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
			  "uniform float opacity = 1;"
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
			  "layout(location = 0) in vec2 uv;"
			  "uniform float index = 0.5;"
			  "uniform float opacity;"
			  "uniform sampler2D gradTab;"
			  "uniform vec2 gradStart;"
			  "uniform vec2 gradEnd;"
			  "out vec4 fragColor;"
			  "void main() {"
			  "  vec2 gradVec = gradEnd - gradStart;"
			  "  float gradTabIndex = dot(gradVec, uv - gradStart) / (gradVec.x * gradVec.x + gradVec.y * gradVec.y);"
			  "  fragColor = texture(gradTab, vec2(gradTabIndex, index));"
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
			  "uniform float opacity;"
			  "uniform vec2 focalToCenter;"
			  "uniform float centerRadius;"
			  "uniform float focalRadius;"
			  "uniform vec2 translationPoint;"
			  "layout(location = 0) in vec2 uv;"
			  "out vec4 fragColor;"
			  "void main() {"
			  "    vec2 coord = uv - translationPoint;"
			  "    float rd = centerRadius - focalRadius;"
			  "    float b = 2.0 * (rd * focalRadius + dot(coord, focalToCenter));"
			  "    float fmp2_m_radius2 = -focalToCenter.x * focalToCenter.x - focalToCenter.y * focalToCenter.y + rd * rd;"
			  "    float inverse_2_fmp2_m_radius2 = 1.0 / (2.0 * fmp2_m_radius2);"
			  "    float det = b * b - 4.0 * fmp2_m_radius2 * ((focalRadius * focalRadius) - dot(coord, coord));"
			  "    vec4 result = vec4(0.0);"
			  "    if (det >= 0.0) {"
			  "        float detSqrt = sqrt(det);"
			  "        float w = max((-b - detSqrt) * inverse_2_fmp2_m_radius2, (-b + detSqrt) * inverse_2_fmp2_m_radius2);"
			  "        if (focalRadius + w * (centerRadius - focalRadius) >= 0.0)"
			  "            result = texture(gradTab, vec2(w, index));"
			  "    }"
			  "    fragColor.a = result.a * opacity;"
			  "    fragColor.rgb = result.rgb * fragColor.a;"
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
			  "uniform float opacity;"
			  "uniform float angle;"
			  "uniform vec2 translationPoint;"
			  "layout(location = 0) in vec2 uv;"
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
			  "layout(location = 0) in vec2 uv;"
			  "uniform sampler2D image;"
			  "uniform float opacity = 1;"
			  "out vec4 fragColor;"
			  "void main() {"
			  "  fragColor = texture(image, uv);"
			  "  if( uv.x < 0 || uv.y < 0 || uv.x > 1 || uv.y > 1 ) {"
			  "    fragColor.a = 0;"
			  "  }"
			  "  fragColor.a *= opacity;"
			  "  fragColor.rgb *= fragColor.a;"
			  "}";

		mProgram = glCreateShaderProgramv( GL_FRAGMENT_SHADER, 1, &glsl );
		break;
	}
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

void Shader::unbind() const
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

} // namespace nvp
} // namespace cinder