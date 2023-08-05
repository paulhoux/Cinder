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

#include "cinder/nvp/Core.h"

namespace cinder {
namespace nvp {

class Transform {
	mat3 mTransform{};

  public:
	Transform() = default;
	~Transform() = default;

	explicit Transform( const std::string &param );

	Transform( const Transform & ) = default;
	Transform( Transform && ) = default;
	Transform &operator=( const Transform & ) = default;
	Transform &operator=( Transform && ) = default;

	Transform &operator*=( const Transform &other )
	{
		mTransform *= other.mTransform;
		return *this;
	}

	Transform &operator*=( const mat3 &other )
	{
		mTransform *= other;
		return *this;
	}

	Transform operator*( const Transform &other ) const
	{
		Transform result = *this;
		result *= other;
		return result;
	}

	vec2 operator*( const vec2 &other ) const
	{
		vec3 v{ other, 1 };
		v = mTransform * v;
		return v;
	}

	Transform &translate( float x, float y )
	{
		mTransform *= glm::translate( mTransform, { x, y } );
		return *this;
	}
	Transform &translate( const vec2 &v )
	{
		mTransform *= glm::translate( mTransform, v );
		return *this;
	}

	Transform &rotate( float degrees )
	{
		mTransform *= glm::rotate( mTransform, glm::radians( degrees ) );
		return *this;
	}
	Transform &rotate( float degrees, float x, float y )
	{
		mTransform = glm::translate( mTransform, { x, y } );
		mTransform = glm::rotate( mTransform, glm::radians( degrees ) );
		mTransform = glm::translate( mTransform, { -x, -y } );
		return *this;
	}
	Transform &rotate( float degrees, const vec2 &v )
	{
		mTransform = glm::translate( mTransform, v );
		mTransform = glm::rotate( mTransform, glm::radians( degrees ) );
		mTransform = glm::translate( mTransform, -v );
		return *this;
	}

	Transform &scale( float s )
	{
		mTransform *= glm::scale( mTransform, { s, s } );
		return *this;
	}
	Transform &scale( float x, float y )
	{
		mTransform *= glm::scale( mTransform, { x, y } );
		return *this;
	}
	Transform &scale( const vec2 &s )
	{
		mTransform *= glm::scale( mTransform, s );
		return *this;
	}

	explicit operator glm::mat3() const { return mTransform; }
	explicit operator glm::mat3x2() const { return { mTransform[0][0], mTransform[0][1], mTransform[1][0], mTransform[1][1], mTransform[2][0], mTransform[2][1] }; }
	explicit operator glm::mat4() const { return { mTransform[0][0], mTransform[0][1], 0, mTransform[0][2], mTransform[1][0], mTransform[1][1], 0, mTransform[1][2], 0, 0, 1, 0, mTransform[2][0], mTransform[2][1], 0, mTransform[2][2] }; }

	std::string toString() const;
};

} // namespace nvp
} // namespace cinder