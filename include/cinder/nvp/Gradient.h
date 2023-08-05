/*
Copyright (c) 2016-2021, Paul Houx Creative Coding - All rights reserved.
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

#include "cinder/nvp/Attribute.h"
#include "cinder/nvp/Transform.h"
#include "cinder/nvp/Values.h"

#include <cinder/Color.h>
#include <cinder/gl/Texture.h>
#include <set>

namespace cinder {
namespace nvp {

//! Defines the coordinate system used by the gradient.
enum class GradientUnits { OBJECT_BOUNDING_BOX = GL_PATH_OBJECT_BOUNDING_BOX_NV, USER_SPACE_ON_USE = GL_OBJECT_LINEAR_NV, DEFAULT = GL_PATH_OBJECT_BOUNDING_BOX_NV };
//!
CI_API static GradientUnits toGradientUnits( std::string style );
//! Defines the spread method used by the gradient.
enum class GradientSpreadMethod { PAD = GL_CLAMP_TO_EDGE, REFLECT = GL_MIRRORED_REPEAT, REPEAT = GL_REPEAT, DEFAULT = PAD };
//!
CI_API static GradientSpreadMethod toSpreadMethod( std::string style );

using GradientRef = std::shared_ptr<struct Gradient>;

CI_API struct Gradient {
	CI_API struct Stop {
		Stop() = default;
		Stop( float t, const ColorAf &color, float opacity = 1 )
			: mOffset{ clamp( t, 0.0f, 1.0f ) }
			, mColor{ color }
		{
			mColor.a *= opacity;
		}
		Stop( float t, const Colorf &color, float opacity = 1 )
			: mOffset{ clamp( t, 0.0f, 1.0f ) }
			, mColor{ color, opacity }
		{
		}
		Stop( float t, float r, float g, float b, float a = 1 )
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

		const ColorAf &color() const { return mColor; }

		bool operator<( const Stop &other ) const { return offset() < other.offset(); }
		bool operator==( const Stop &other ) const { return offset() == other.offset() && mColor == other.mColor; }
		bool operator!=( const Stop &other ) const { return !( *this == other ); }

	  private:
		float   mOffset{ 0 }; // normalized 0-1
		ColorAf mColor;
	};

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
	virtual const Transform &getTransform() const = 0;

	//! Returns the color at position \a t. Does not pre-multiply the RGB values.
	ColorAf at( float t ) const
	{
		auto &lo = floor( t );
		auto &hi = ceil( t );

		if( lo == hi )
			return lo.color();

		float f = clamp( ( t - lo.offset() ) / ( hi.offset() - lo.offset() ), 0.0f, 1.0f );
		return lo.color().lerp( f, hi.color() );
	}

	//! Returns the nearest stop lower than position \a t.
	const Stop &floor( float t ) const
	{
		if( mStops.empty() ) {
			static Stop kEmpty{ 0.0f, ColorA( 0, 0, 0, 0 ) };
			return kEmpty;
		}

		for( auto itr = mStops.rbegin(); itr != mStops.rend(); ++itr ) {
			if( itr->offset() <= t )
				return *itr;
		}

		return mStops.front();
	}

	//! Returns the nearest stop higher than position \a t.
	const Stop &ceil( float t ) const
	{
		if( mStops.empty() ) {
			static Stop kEmpty{ 0.0f, ColorA( 0, 0, 0, 0 ) };
			return kEmpty;
		}

		for( auto itr = mStops.begin(); itr != mStops.end(); ++itr ) {
			if( itr->offset() > t )
				return *itr;
		}

		return mStops.back();
	}

	//! Inserts a \a color at position \a t.
	Gradient &stop( float t, const ColorA &color )
	{
		insert( t, color );
		return *this;
	}
	//! Inserts a \a color at position \a t.
	Gradient &stop( float t, float r, float g, float b, float a = 1 )
	{
		insert( t, { r, g, b, a } );
		return *this;
	}

	//! Inserts a \a color at offset \a t. If an additional \a opacity is given, it will be multiplied with the color's alpha value.
	void insert( float t, const ColorA &color, float opacity = 1 ) { insert( Stop{ t, color, opacity } ); }
	//! Inserts a \a color at offset \a t.
	void insert( float t, const Color &color, float opacity = 1 ) { insert( Stop{ t, color, opacity } ); }
	//! Inserts a stop.
	void insert( const Stop &stop )
	{
		// Keep sorted.
		mStops.insert( std::upper_bound( mStops.begin(), mStops.end(), stop ), stop );
	}
	//! Removes all stops.
	void clear() { mStops.clear(); }
	//! Adds all stops of the \a other gradient without discarding existing stops.
	void add( const Gradient &other )
	{
		for( const auto &stop : other.mStops )
			insert( stop );
	}
	//! Replaces all stops with those of the \a other gradient.
	void replace( const Gradient &other ) { mStops = other.mStops; }

	bool   empty() const { return mStops.empty(); }
	size_t size() const { return mStops.size(); }

	auto begin() const { return mStops.begin(); }
	auto end() const { return mStops.end(); }

	auto rbegin() const { return mStops.rbegin(); }
	auto rend() const { return mStops.rend(); }

	//! Returns raw 8-bit RGBA data. You can use this to construct a Surface.
	std::unique_ptr<uint8_t[]> data( int32_t width, int32_t height, float from = 0.0f, float to = 1.0f ) const
	{
		auto result = std::make_unique<uint8_t[]>( size_t( width ) * size_t( height ) * sizeof( ColorA8u ) );

		for( int y = 0; y < height; ++y ) {
			for( int x = 0; x < width; ++x ) {
				float t = mix( from, to, float( x ) / float( width - 1 ) );

				const auto    c = ColorA8u( at( t ) );
				const int64_t i = ( int64_t( x ) + int64_t( y ) * int64_t( width ) ) * sizeof( ColorA8u );
				result[size_t( i ) + 0] = uint8_t( c.r );
				result[size_t( i ) + 1] = uint8_t( c.g );
				result[size_t( i ) + 2] = uint8_t( c.b );
				result[size_t( i ) + 3] = uint8_t( c.a );
			}
		}

		return result;
	}

  private:
	std::vector<Stop> mStops;
};

using LinearGradientRef = std::shared_ptr<struct LinearGradient>;

CI_API struct LinearGradient : public Gradient {
	static LinearGradientRef create( const char *id ) { return std::make_shared<LinearGradient>( id ); }

	explicit LinearGradient( std::string id )
		: mId{ std::move( id ) }
	{
	}
	explicit LinearGradient( const char *id )
		: mId{ id }
	{
	}
	explicit LinearGradient( const GradientRef &other )
	{
		if( other ) {
			if( const auto linear = std::dynamic_pointer_cast<LinearGradient>( other ) )
				*this << *linear; // Copy all attributes.
			else
				replace( *other ); // Only copy stops.
		}
	}

	// explicit LinearGradient( const LinearGradientRef &other )
	//{
	//	if( other )
	//		replace( *other );
	// }

	const std::string &getId() const override { return mId; }
	const Transform   &getTransform() const override { return mTransform; }

	const auto &getUnits() const { return mUnits.value(); }
	const auto &getSpread() const { return mSpread.value(); }
	const auto &getX1() const { return mX1.value(); }
	const auto &getY1() const { return mY1.value(); }
	const auto &getX2() const { return mX2.value(); }
	const auto &getY2() const { return mY2.value(); }

	LinearGradientRef clone() { return std::make_shared<LinearGradient>( *this ); }

	LinearGradient &id( const std::string &id )
	{
		mId = id;
		return *this;
	}
	LinearGradient &transform( const Transform &transform )
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
	LinearGradient &from( const Number &x1, const Number &y1 )
	{
		mX1 = x1;
		mY1 = y1;
		return *this;
	}
	LinearGradient &to( const Number &x2, const Number &y2 )
	{
		mX2 = x2;
		mY2 = y2;
		return *this;
	}

	LinearGradient &operator<<( const LinearGradient &other )
	{
		add( other );
		mUnits = other.mUnits;
		mSpread = other.mSpread;
		mX1 = other.mX1;
		mY1 = other.mY1;
		mX2 = other.mX2;
		mY2 = other.mY2;
		return *this;
	}

  private:
	std::string                     mId;
	Transform                       mTransform;
	Attribute<GradientUnits>        mUnits{ GradientUnits::DEFAULT };
	Attribute<GradientSpreadMethod> mSpread{ GradientSpreadMethod::DEFAULT };
	Attribute<Number>               mX1{ 0 };
	Attribute<Number>               mY1{ 0 };
	Attribute<Number>               mX2{ 1 };
	Attribute<Number>               mY2{ 0 };
};

using RadialGradientRef = std::shared_ptr<struct RadialGradient>;

CI_API struct RadialGradient : public Gradient {
	static RadialGradientRef create( const char *id ) { return std::make_shared<RadialGradient>( id ); }

	explicit RadialGradient( std::string id )
		: mId{ std::move( id ) }
	{
	}
	explicit RadialGradient( const char *id )
		: mId{ id }
	{
	}
	explicit RadialGradient( const GradientRef &other )
	{
		if( other ) {
			if( const auto radial = std::dynamic_pointer_cast<RadialGradient>( other ) )
				*this << *radial; // Copy all attributes.
			else
				replace( *other ); // Only copy stops.
		}
	}

	const std::string &getId() const override { return mId; }
	const Transform   &getTransform() const override { return mTransform; }

	const auto &getUnits() const { return mUnits.value(); }
	const auto &getSpread() const { return mSpread.value(); }
	const auto &getR() const { return mR.value(); }
	const auto &getCx() const { return mCx.value(); }
	const auto &getCy() const { return mCy.value(); }
	const auto &getFr() const { return mFr.value(); }
	const auto &getFx() const { return mFx.value(); }
	const auto &getFy() const { return mFy.value(); }

	RadialGradientRef clone() { return std::make_shared<RadialGradient>( *this ); }

	RadialGradient &id( const std::string &id )
	{
		mId = id;
		return *this;
	}
	RadialGradient &transform( const Transform &transform )
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
	RadialGradient &radius( const Number &r )
	{
		mR = r;
		return *this;
	}
	RadialGradient &center( const Number &cx, const Number &cy )
	{
		mCx = cx;
		mCy = cy;
		return *this;
	}
	RadialGradient &focal( const Number &fx, const Number &fy, const Number &fr = 0 )
	{
		mFx = fx;
		mFy = fy;
		mFr = fr;
		return *this;
	}

	RadialGradient &operator<<( const RadialGradient &other )
	{
		add( other );
		mUnits = other.mUnits;
		mSpread = other.mSpread;
		mR = other.mR;
		mCx = other.mCx;
		mCy = other.mCy;
		mFr = other.mFr;
		mFx = other.mFx;
		mFy = other.mFy;
		return *this;
	}

  private:
	std::string                     mId;
	Transform                       mTransform;
	Attribute<GradientUnits>        mUnits{ GradientUnits::DEFAULT };
	Attribute<GradientSpreadMethod> mSpread{ GradientSpreadMethod::DEFAULT };
	Attribute<Number>               mR{ { "50%" } };  // Defaults to 50%.
	Attribute<Number>               mCx{ { "50%" } }; // Defaults to 50%.
	Attribute<Number>               mCy{ { "50%" } }; // Defaults to 50%.
	Attribute<Number>               mFr{ 0 };
	Attribute<Number>               mFx{ mCx };
	Attribute<Number>               mFy{ mCy };
};

//! Stores multiple gradients in a single texture for performance.
CI_API struct Gradients {
	Gradients() = default;
	~Gradients() = default;

	Gradients( const Gradients & ) = delete;
	Gradients( Gradients && ) = default;
	Gradients &operator=( const Gradients & ) = delete;
	Gradients &operator=( Gradients && ) = default;

	//!
	bool empty() const { return mLookUp.empty(); }
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

} // namespace nvp
} // namespace cinder
