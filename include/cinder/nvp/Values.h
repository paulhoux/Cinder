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

#include <string>
#include <vector>

#include <glm/common.hpp>

namespace cinder {
namespace nvp {

class Number {
  public:
	Number() = default;

	/* non-explicit */ Number( float v )
		: mValue( v )
	{
	}

	/* non-explicit */ Number( const std::string &v );

	operator float() const { return mValue; }

	bool isPercentage() const { return mIsPercentage; }

	bool operator==( const Number &other ) const { return mValue == other.mValue; }
	bool operator!=( const Number &other ) const { return !( *this == other ); }

	Number &operator+=( float value )
	{
		mValue += value;
		mIsPercentage = false;
		return *this;
	}
	Number &operator-=( float value )
	{
		mValue -= value;
		mIsPercentage = false;
		return *this;
	}
	Number &operator*=( float value )
	{
		mValue *= value;
		mIsPercentage = false;
		return *this;
	}
	Number &operator/=( float value )
	{
		mValue /= value;
		mIsPercentage = false;
		return *this;
	}

	//! Splits a string into a list of numbers.
	static std::vector<Number> split( const std::string &s, const std::string &separators = " ," );

	std::string toString() const;

  protected:
	float mValue{ 0 };
	bool  mIsPercentage{ false };
};

class AlphaValue : public Number {
  public:
	AlphaValue() = default;

	/* non-explicit */ AlphaValue( float v )
		: Number( glm::clamp( v, 0.0f, 1.0f ) )
	{
		mIsPercentage = false;
	}

	/* non-explicit */ AlphaValue( const std::string &v )
		: Number( v )
	{
		mValue = glm::clamp( mValue, 0.0f, 1.0f );
		mIsPercentage = false;
	}

	bool operator==( const AlphaValue &other ) const { return mValue == other.mValue; }
	bool operator!=( const AlphaValue &other ) const { return !( *this == other ); }

	bool operator==( const Number &other ) const { return mValue == float( other ); }
	bool operator!=( const Number &other ) const { return !( *this == other ); }

	AlphaValue &operator*=( const AlphaValue &value )
	{
		mValue *= value.mValue;
		return *this;
	}

	AlphaValue &operator+=( const Number &value )
	{
		mValue = glm::clamp( mValue + float( value ), 0.0f, 1.0f );
		return *this;
	}
	AlphaValue &operator-=( const Number &value )
	{
		mValue = glm::clamp( mValue - float( value ), 0.0f, 1.0f );
		return *this;
	}
	AlphaValue &operator*=( const Number &value )
	{
		mValue = glm::clamp( mValue * float( value ), 0.0f, 1.0f );
		return *this;
	}
	AlphaValue &operator/=( const Number &value )
	{
		mValue = glm::clamp( mValue / float( value ), 0.0f, 1.0f );
		return *this;
	}

	AlphaValue &operator+=( float value )
	{
		mValue = glm::clamp( mValue + value, 0.0f, 1.0f );
		return *this;
	}
	AlphaValue &operator-=( float value )
	{
		mValue = glm::clamp( mValue - value, 0.0f, 1.0f );
		return *this;
	}
	AlphaValue &operator*=( float value )
	{
		mValue = glm::clamp( mValue * value, 0.0f, 1.0f );
		return *this;
	}
	AlphaValue &operator/=( float value )
	{
		mValue = glm::clamp( mValue / value, 0.0f, 1.0f );
		return *this;
	}
};

} // namespace nvp
} // namespace cinder