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

#include <utility>

#include "cinder/nvp/Core.h"
#include "cinder/nvp/Path.h"

namespace cinder {
namespace nvp {

using ArcRef = std::shared_ptr<class Arc>;

class Arc final : public Path {
	glm::vec2 mCenter{ 0 };             //
	float     mRadius{ 100 };           //
	float     mStart{ 0 };              // In radians.
	float     mEnd{ glm::pi<float>() }; // In radians.
	float     mOffset{ 0 };             // In radians.

  public:
	static ArcRef create( const glm::vec2 &center, float radius, float start = 0, float end = 180, float offset = 0 ) { return std::make_shared<Arc>( center, radius, start, end, offset ); }

	//! Creates an arc with the specified \a center and \a radius, running clockwise from \a start (in degrees) at to \a end (in degrees).
	//! The result can then optionally be rotated clockwise by specifying an \a offset in degrees.
	Arc( const glm::vec2 &center, float radius, float start = 0, float end = 180, float offset = 0 )
		: mCenter{ center }
		, mRadius{ radius }
		, mStart{ glm::radians( start ) }
		, mEnd{ glm::radians( end ) }
		, mOffset{ glm::radians( offset ) }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}

	Arc( const Arc &other ) = default;
	Arc( Arc &&other ) noexcept = default;
	Arc &operator=( const Arc &other ) = default;
	Arc &operator=( Arc &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return { mCenter.x - mRadius, mCenter.y - mRadius, mCenter.x + mRadius, mCenter.y + mRadius }; }

	Arc transformed( const glm::mat3x2 &transform ) const
	{
		Arc arc( *this );
		arc.transform( transform );
		return arc;
	}

  private:
	void create() const;
};

using CircleRef = std::shared_ptr<class Circle>;

class Circle final : public Path {
	glm::vec2 mCenter{ 0 };   //
	float     mRadius{ 100 }; //

  public:
	static CircleRef create( const glm::vec2 &center, float radius ) { return std::make_shared<Circle>( center, radius ); }

	//! Creates a full circle with the specified \a center and \a radius.
	Circle( const glm::vec2 &center, float radius )
		: mCenter{ center }
		, mRadius{ radius }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}

	Circle( const Circle &other ) = default;
	Circle( Circle &&other ) noexcept = default;
	Circle &operator=( const Circle &other ) = default;
	Circle &operator=( Circle &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return { mCenter.x - mRadius, mCenter.y - mRadius, mCenter.x + mRadius, mCenter.y + mRadius }; }

	Circle transformed( const glm::mat3x2 &transform ) const
	{
		Circle circle( *this );
		circle.transform( transform );
		return circle;
	}

  private:
	void create() const;
};

using EllipseRef = std::shared_ptr<class Ellipse>;

class Ellipse final : public Path {
	glm::vec2 mCenter{ 0 };    //
	float     mRadiusX{ 100 }; //
	float     mRadiusY{ 100 }; //

  public:
	static EllipseRef create( const glm::vec2 &center, float radiusA, float radiusB ) { return std::make_shared<Ellipse>( center, radiusA, radiusB ); }

	//! Creates a full ellipse with the specified \a center and \a radiusA and \a radiusB.
	Ellipse( const glm::vec2 &center, float radiusA, float radiusB )
		: mCenter{ center }
		, mRadiusX{ radiusA }
		, mRadiusY{ radiusB }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}
	//!
	explicit Ellipse( const ci::Rectf &bounds )
		: Ellipse{ bounds.getCenter(), 0.5f * bounds.getWidth(), 0.5f * bounds.getHeight() }
	{
	}

	Ellipse( const Ellipse &other ) = default;
	Ellipse( Ellipse &&other ) noexcept = default;
	Ellipse &operator=( const Ellipse &other ) = default;
	Ellipse &operator=( Ellipse &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return { mCenter.x - mRadiusX, mCenter.y - mRadiusY, mCenter.x + mRadiusX, mCenter.y + mRadiusY }; }

	Ellipse transformed( const glm::mat3x2 &transform ) const
	{
		Ellipse ellipse( *this );
		ellipse.transform( transform );
		return ellipse;
	}

  private:
	void create() const;
};

// using LineRef = std::shared_ptr<class Line>; // TODO: conflict with Text and Svg classes!

class Line final : public Path {
	glm::vec2 mP0{ 0 }; //
	glm::vec2 mP1{ 0 }; //

  public:
	static std::shared_ptr<Line> create( const glm::vec2 &p0, const glm::vec2 &p1 ) { return std::make_shared<Line>( p0, p1 ); }

	Line( const glm::vec2 &p0, const glm::vec2 &p1 )
		: mP0{ p0 }
		, mP1{ p1 }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}

	Line( const Line &other ) = default;
	Line( Line &&other ) noexcept = default;
	Line &operator=( const Line &other ) = default;
	Line &operator=( Line &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return { mP0, mP1 }; }

	Line transformed( const glm::mat3x2 &transform ) const
	{
		Line line( *this );
		line.transform( transform );
		return line;
	}

  private:
	void create() const;
};

using PolygonRef = std::shared_ptr<class Polygon>;

class Polygon final : public Path {
	std::vector<glm::vec2> mPoints;

  public:
	static PolygonRef create( const std::vector<glm::vec2> &points, bool closed = true ) { return std::make_shared<Polygon>( points, closed ); }
	static PolygonRef create( const std::initializer_list<glm::vec2> &points, bool closed = true ) { return std::make_shared<Polygon>( points, closed ); }

	explicit Polygon( std::vector<glm::vec2> points, bool closed = true )
		: mPoints( std::move( points ) )
	{
		mPathId = gl::genPathsNV( 1 );
		create( closed );
	}
	explicit Polygon( const std::initializer_list<glm::vec2> &points, bool closed = true )
		: mPoints( points )
	{
		mPathId = gl::genPathsNV( 1 );
		create( closed );
	}

	Polygon( const Polygon &other ) = default;
	Polygon( Polygon &&other ) noexcept = default;
	Polygon &operator=( const Polygon &other ) = default;
	Polygon &operator=( Polygon &&other ) noexcept = default;

	Polygon transformed( const glm::mat3x2 &transform ) const
	{
		Polygon polygon( *this );
		polygon.transform( transform );
		return polygon;
	}

  private:
	void create( bool closed = true ) const;
};

using RectangleRef = std::shared_ptr<class Rectangle>;

class Rectangle final : public Path {
	ci::Rectf mBounds; //

  public:
	static RectangleRef create( const ci::Rectf &bounds ) { return std::make_shared<Rectangle>( bounds ); }
	static RectangleRef create( float x, float y, float width, float height ) { return std::make_shared<Rectangle>( x, y, width, height ); }

	explicit Rectangle( const ci::Rectf &bounds )
		: mBounds{ bounds }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}
	Rectangle( float x, float y, float width, float height )
		: Rectangle{ { x, y, x + width, y + height } }
	{
	}

	Rectangle( const Rectangle &other ) = default;
	Rectangle( Rectangle &&other ) noexcept = default;
	Rectangle &operator=( const Rectangle &other ) = default;
	Rectangle &operator=( Rectangle &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return mBounds; }

	Rectangle transformed( const glm::mat3x2 &transform ) const
	{
		Rectangle rectangle( *this );
		rectangle.transform( transform );
		return rectangle;
	}

  private:
	void create() const;
};

using RoundedRectangleRef = std::shared_ptr<class RoundedRectangle>;

class RoundedRectangle final : public Path {
	ci::Rectf mBounds;             //
	float     mCornerRadiusX{ 0 }; //
	float     mCornerRadiusY{ 0 }; //

  public:
	static RoundedRectangleRef create( float x, float y, float width, float height, float r ) { return std::make_shared<RoundedRectangle>( x, y, width, height, r ); }
	static RoundedRectangleRef create( float x, float y, float width, float height, float rx, float ry ) { return std::make_shared<RoundedRectangle>( x, y, width, height, rx, ry ); }

	RoundedRectangle( float x, float y, float width, float height, float r )
		: mBounds{ x, y, x + width, y + height }
		, mCornerRadiusX{ r }
		, mCornerRadiusY{ r }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}
	RoundedRectangle( float x, float y, float width, float height, float rx, float ry )
		: mBounds{ x, y, x + width, y + height }
		, mCornerRadiusX{ rx }
		, mCornerRadiusY{ ry }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}

	RoundedRectangle( const RoundedRectangle &other ) = default;
	RoundedRectangle( RoundedRectangle &&other ) noexcept = default;
	RoundedRectangle &operator=( const RoundedRectangle &other ) = default;
	RoundedRectangle &operator=( RoundedRectangle &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return mBounds; }

	RoundedRectangle transformed( const glm::mat3x2 &transform ) const
	{
		RoundedRectangle rectangle( *this );
		rectangle.transform( transform );
		return rectangle;
	}

  private:
	void create() const;
};

using StarRef = std::shared_ptr<class Star>;

class Star final : public Path {
	glm::vec2 mCenter;           //
	float     mRadiusLarge{ 0 }; //
	float     mRadiusSmall{ 0 }; //
	float     mRotation{ 0 };    //
	int       mPoints{ 5 };      //

  public:
	static StarRef create( const glm::vec2 &center, int points, float largeRadius, float smallRadius, float rotation = 0 ) { return std::make_shared<Star>( center, points, largeRadius, smallRadius, rotation ); }
	static StarRef create( float x, float y, int points, float largeRadius, float smallRadius, float rotation = 0 ) { return std::make_shared<Star>( x, y, points, largeRadius, smallRadius, rotation ); }

	Star( const glm::vec2 &center, int points, float largeRadius, float smallRadius, float rotation = 0 )
		: Star( center.x, center.y, points, largeRadius, smallRadius, rotation )
	{
	}
	Star( float x, float y, int points, float largeRadius, float smallRadius, float rotation = 0 )
		: mCenter{ x, y }
		, mRadiusLarge{ largeRadius }
		, mRadiusSmall{ smallRadius }
		, mRotation{ glm::radians( rotation ) }
		, mPoints{ points }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}

	Star( const Star &other ) = default;
	Star( Star &&other ) noexcept = default;
	Star &operator=( const Star &other ) = default;
	Star &operator=( Star &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return { mCenter.x - mRadiusLarge, mCenter.y - mRadiusLarge, mCenter.x + mRadiusLarge, mCenter.y + mRadiusLarge }; }

	Star transformed( const glm::mat3x2 &transform ) const
	{
		Star star( *this );
		star.transform( transform );
		return star;
	}

  private:
	void create() const;
};

using ArrowRef = std::shared_ptr<class Arrow>;

class Arrow final : public Path {
	glm::vec2 mP0;
	glm::vec2 mP1;
	float     mThickness{ 1 };
	float     mWidth{ 4 };     // As a percentage of thickness.
	float     mLength{ 4 };    // As a percentage of thickness.
	float     mConcavity{ 0 }; //
  public:
	static ArrowRef create( float x0, float y0, float x1, float y1, float thickness, float width = 4, float length = 4, float concavity = 0 ) { return std::make_shared<Arrow>( x0, y0, x1, y1, thickness, width, length, concavity ); }

	Arrow( float x0, float y0, float x1, float y1, float thickness, float width = 4, float length = 4, float concavity = 0 )
		: mP0{ x0, y0 }
		, mP1{ x1, y1 }
		, mThickness{ thickness }
		, mWidth{ width }
		, mLength{ length }
		, mConcavity{ concavity }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}

	Arrow( const Arrow &other ) = default;
	Arrow( Arrow &&other ) noexcept = default;
	Arrow &operator=( const Arrow &other ) = default;
	Arrow &operator=( Arrow &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return { /* TODO */ }; }

	Arrow transformed( const glm::mat3x2 &transform ) const
	{
		Arrow arrow( *this );
		arrow.transform( transform );
		return arrow;
	}

	void stroke( const ci::ColorA &color, float strokeWidth ) override { stroke( color, CapsStyle::DEFAULT, JoinStyle::MITER_REVERT, strokeWidth ); }
	void stroke( const ci::ColorA &color, CapsStyle caps, float strokeWidth ) override { stroke( color, caps, JoinStyle::MITER_REVERT, strokeWidth ); }
	void stroke( const ci::ColorA &color, JoinStyle join, float strokeWidth ) override { stroke( color, CapsStyle::DEFAULT, join, strokeWidth ); }
	void stroke( const ci::ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth ) override { Path::stroke( color, caps, join, strokeWidth ); }

  private:
	void create() const;
};

using SpiralRef = std::shared_ptr<class Spiral>;

//! Creates an Archimedean spiral.
class Spiral final : public Path {
	glm::vec2 mCenter;      //
	float     mInnerRadius; //
	float     mOuterRadius; //
	float     mSpacing;     // Distance between each winding.

  public:
	static SpiralRef create( const glm::vec2 &center, float innerRadius, float outerRadius, float spacing ) { return std::make_shared<Spiral>( center, innerRadius, outerRadius, spacing ); }

	//! Creates an Archimedean spiral at \a center, with the specified \a innerRadius, \a outerRadius and \a spacing between each winding.
	Spiral( const glm::vec2 &center, float innerRadius, float outerRadius, float spacing )
		: mCenter{ center }
		, mInnerRadius{ innerRadius }
		, mOuterRadius{ outerRadius }
		, mSpacing{ spacing }
	{
		mPathId = gl::genPathsNV( 1 );
		create();
	}

	Spiral( const Spiral &other ) = default;
	Spiral( Spiral &&other ) noexcept = default;
	Spiral &operator=( const Spiral &other ) = default;
	Spiral &operator=( Spiral &&other ) noexcept = default;

	[[nodiscard]] ci::Rectf getBounds() const override { return { /* TODO */ }; }

	Spiral transformed( const glm::mat3x2 &transform ) const
	{
		Spiral spiral( *this );
		spiral.transform( transform );
		return spiral;
	}

	void stroke( const ci::ColorA &color, float strokeWidth ) override { stroke( color, CapsStyle::DEFAULT, JoinStyle::MITER_REVERT, strokeWidth ); }
	void stroke( const ci::ColorA &color, CapsStyle caps, float strokeWidth ) override { stroke( color, caps, JoinStyle::MITER_REVERT, strokeWidth ); }
	void stroke( const ci::ColorA &color, JoinStyle join, float strokeWidth ) override { stroke( color, CapsStyle::DEFAULT, join, strokeWidth ); }
	void stroke( const ci::ColorA &color, CapsStyle caps, JoinStyle join, float strokeWidth ) override { Path::stroke( color, caps, join, strokeWidth ); }

	//! Returns the coordinates of the center of the spiral.
	const glm::vec2 &getCenter() const { return mCenter; }
	//! Returns the inner radius of the spiral.
	float getInnerRadius() const { return mInnerRadius; }
	//! Returns the outer radius of the spiral.
	float getOuterRadius() const { return mOuterRadius; }
	//! Returns the distance between each winding.
	float getSpacing() const { return mSpacing; }
	//! Returns the angle in radians between the positive x-axis and the line from the origin of the spiral to the point at the inner radius.
	float getInnerRadians() const { return glm::radians( 360 * mInnerRadius / mSpacing ); }
	//! Returns the angle in radians between the positive x-axis and the line from the origin of the spiral to the point at the outer radius.
	float getOuterRadians() const { return glm::radians( 360 * mOuterRadius / mSpacing ); }
	//! Returns the angle in radians between the positive x-axis and the line from the origin of the spiral to the point at \a radius distance from the origin.
	float getRadians( float radius, float spacing ) const { return glm::radians( 360 * radius / spacing ); }

  private:
	void create() const;

	struct Point {
		float x;
		float y;
		float theta;
		float tangent;

		explicit Point( float theta );

		std::pair<glm::vec2, glm::vec2> generate( const Point &previous ) const;
	};
};

} // namespace nvp
} // namespace cinder
