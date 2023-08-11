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

#include <cassert>
#include <utility>

namespace cinder {
namespace nvp {

template <typename T>
class Attribute {
	T    mValue{};
	T    mDefault{};
	bool mIsSet{ false };

	friend class Attribute; // Allows access from all templated attributes.

  public:
	/* non-explicit */ explicit Attribute( const T &defaultValue )
		: mValue( defaultValue )
		, mDefault( defaultValue )
	{
	}
	~Attribute() = default;

	Attribute( const Attribute &other )
		: mValue( other.mValue )
		, mDefault( other.mDefault )
		, mIsSet( other.mIsSet )
	{
	}
	Attribute( Attribute &&other ) noexcept
		: mValue( std::move( other.mValue ) )
		, mDefault( std::move( other.mDefault ) )
		, mIsSet( other.mIsSet )
	{
	}
	Attribute &operator=( const Attribute<T> &other ) noexcept
	{
		if( !mIsSet || other.mIsSet ) {
			mValue = other.mValue;
			mDefault = other.mDefault;
			mIsSet = other.mIsSet;
		}

		return *this;
	}
	Attribute &operator=( Attribute<T> &&other ) noexcept
	{
		swap( *this, other );
		return *this;
	}

	const T &default() const { return mDefault; }

	//! Returns a non-const reference to the value.
	T &ref()
	{
		mIsSet = true;
		return mValue;
	}

	//! Returns a const reference to the value.
	const T &value() const { return mValue; }

	bool isSet() const { return mIsSet; }

	void reset()
	{
		mValue = mDefault;
		mIsSet = false;
	}

	constexpr void swap( Attribute<T> &first, Attribute<T> &second ) noexcept
	{
		std::swap( first.mValue, second.mValue );
		std::swap( first.mDefault, second.mDefault );
		std::swap( first.mIsSet, second.mIsSet );
	}

	/* non-explicit */ operator T() const { return mValue; }

	Attribute &operator=( T value )
	{
		mValue = std::move( value );
		mIsSet = true;
		return *this;
	}

	bool operator<( const Attribute<T> &other ) const { return mValue < other.mValue; }
	bool operator<=( const Attribute<T> &other ) const { return mValue <= other.mValue; }
	bool operator>( const Attribute<T> &other ) const { return mValue > other.mValue; }
	bool operator>=( const Attribute<T> &other ) const { return mValue >= other.mValue; }

	bool operator==( const Attribute<T> &other ) const { return mValue == other.mValue; }
	bool operator!=( const Attribute<T> &other ) const { return !( *this == other ); }

	bool operator<( const T &other ) const { return mValue < other; }
	bool operator<=( const T &other ) const { return mValue <= other; }
	bool operator>( const T &other ) const { return mValue > other; }
	bool operator>=( const T &other ) const { return mValue >= other; }

	bool operator==( const T &other ) const { return mValue == other; }
	bool operator!=( const T &other ) const { return !( *this == other ); }

	template <typename N, class = decltype( std::declval<T>() + std::declval<N>() )>
	Attribute &operator+=( const Attribute<N> &other )
	{
		// if( other.mIsSet ) {
		mValue += other.mValue;
		mIsSet = true;
		//}
		return *this;
	}
	template <typename N, class = decltype( std::declval<T>() - std::declval<N>() )>
	Attribute &operator-=( const Attribute<N> &other )
	{
		// if( other.mIsSet ) {
		mValue -= other.mValue;
		mIsSet = true;
		//}
		return *this;
	}
	template <typename N, class = decltype( std::declval<T>() * std::declval<N>() )>
	Attribute &operator*=( const Attribute<N> &other )
	{
		// if( other.mIsSet ) {
		mValue *= other.mValue;
		mIsSet = true;
		//}
		return *this;
	}
	template <typename N, class = decltype( std::declval<T>() / std::declval<N>() )>
	Attribute &operator/=( const Attribute<N> &other )
	{
		// if( other.mIsSet ) {
		mValue /= other.mValue;
		mIsSet = true;
		//}
		return *this;
	}

	template <typename N, class = decltype( std::declval<T>() + std::declval<N>() )>
	Attribute &operator+=( const N &other )
	{
		mValue += other;
		mIsSet = true;
		return *this;
	}
	template <typename N, class = decltype( std::declval<T>() - std::declval<N>() )>
	Attribute &operator-=( const N &other )
	{
		mValue -= other;
		mIsSet = true;
		return *this;
	}
	template <typename N, class = decltype( std::declval<T>() * std::declval<N>() )>
	Attribute &operator*=( const N &other )
	{
		mValue *= other;
		mIsSet = true;
		return *this;
	}
	template <typename N, class = decltype( std::declval<T>() / std::declval<N>() )>
	Attribute &operator/=( const N &other )
	{
		mValue /= other;
		mIsSet = true;
		return *this;
	}

	template <typename N, class = decltype( std::declval<T>() + std::declval<N>() )>
	friend Attribute operator+( Attribute<T> lhs, const Attribute<N> &rhs )
	{
		lhs += rhs;
		return lhs;
	}

	template <typename N, class = decltype( std::declval<T>() - std::declval<N>() )>
	friend Attribute operator-( Attribute<T> lhs, const Attribute<N> &rhs )
	{
		lhs -= rhs;
		return lhs;
	}

	template <typename N, class = decltype( std::declval<T>() * std::declval<N>() )>
	friend Attribute operator*( Attribute<T> lhs, const Attribute<N> &rhs )
	{
		lhs *= rhs;
		return lhs;
	}

	template <typename N, class = decltype( std::declval<T>() / std::declval<N>() )>
	friend Attribute operator/( Attribute<T> lhs, const Attribute<N> &rhs )
	{
		lhs /= rhs;
		return lhs;
	}

	template <typename N, class = decltype( std::declval<T>() + std::declval<N>() )>
	friend Attribute operator+( Attribute<T> lhs, const N &rhs )
	{
		lhs += rhs;
		return lhs;
	}

	template <typename N, class = decltype( std::declval<T>() - std::declval<N>() )>
	friend Attribute operator-( Attribute<T> lhs, const N &rhs )
	{
		lhs -= rhs;
		return lhs;
	}

	template <typename N, class = decltype( std::declval<T>() * std::declval<N>() )>
	friend Attribute operator*( Attribute<T> lhs, const N &rhs )
	{
		lhs *= rhs;
		return lhs;
	}

	template <typename N, class = decltype( std::declval<T>() / std::declval<N>() )>
	friend Attribute operator/( Attribute<T> lhs, const N &rhs )
	{
		lhs /= rhs;
		return lhs;
	}
};

template <typename T, typename U>
bool operator==( const Attribute<T> &lhs, const Attribute<U> &rhs )
{
	return false; // Attributes of different types are never the same.
}
template <typename T, typename U>
bool operator!=( const Attribute<T> &lhs, const Attribute<U> &rhs )
{
	return true; // Attributes of different types are never the same.
}

} // namespace nvp
} // namespace cinder

#pragma warning( push )
#pragma warning( disable: 4244 )
inline void testAttribute()
{
	cinder::nvp::Attribute              a( 5 ); // Default to int.
	const cinder::nvp::Attribute<float> b( 5 );

	assert( a != b && "Should be different types" );
	assert( !a.isSet() && "Should be default value" );
	assert( !b.isSet() && "Should be default value" );

	assert( a.value() == b.value() && "Should be same values" );

	assert( !a.isSet() && "Should still be default value" ); // Checks if value() is non-const.
	assert( !b.isSet() && "Should still be default value" );

	a = b; // Implicit conversion from b to float.

	assert( a.isSet() && "Should not still be default value" );
	assert( !b.isSet() && "Should still be default value" );
	assert( a.value() == b.value() && "Should be same values" );

	a += b;

	assert( a.value() == 10 && "Should be 5 + 5 = 10" );
	assert( !b.isSet() && "Should still be default value" );

	a += 5.0f; // adding float value to integer attribute is allowed.

	assert( a.value() == 15 && "Should be 10 + 5 = 15" );
	assert( a.default() == 5 && "Default value should still be 5" );

	a = a + b;

	assert( a.value() == 20 && "Should be 15 + 5 = 20" );
	assert( a.default() == 5 && "Default value should still be 5" );

	a *= b;

	assert( a.value() == 100 && "Should be 20 * 5 = 100" );

	a -= b;

	assert( a.value() == 95 && "Should be 100 - 5 = 95" );

	a /= b;

	assert( a.value() == 19 && "Should be 95 / 5 = 19" );
	assert( !b.isSet() && "Should still be default value" );
	assert( a.default() == 5 && "Default value should still be 5" );

	a = cinder::nvp::Attribute<int>( 10 );

	assert( !a.isSet() && "Should be default value" );
	assert( a.default() == 10 && "Default value should be 10" );

	auto sum = a + b;

	assert( sum.value() == 15 && "Should be 10 + 5 = 15" );
	assert( sum.default() == 10 && "Should be the default of the first operand (a)" );

	auto difference = a - b;

	assert( difference.value() == 5 && "Should be 10 - 5 = 5" );
	assert( difference.default() == 10 && "Should be the default of the first operand (a)" );

	auto product = a * b;

	assert( product.value() == 50 && "Should be 10 * 5 = 50" );
	assert( product.default() == 10 && "Should be the default of the first operand (a)" );

	auto fraction = a / b;

	assert( fraction.value() == 2 && "Should be 10 / 5 = 2" );
	assert( fraction.default() == 10 && "Should be the default of the first operand (a)" );
}
#pragma warning( pop )