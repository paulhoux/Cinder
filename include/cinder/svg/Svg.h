/*
 Copyright (c) 2012, The Cinder Project
 All rights reserved.

 This code is designed for use with the Cinder C++ library, http://libcinder.org

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

#include "cinder/Cinder.h"
#include "cinder/Xml.h"
#include "cinder/Vector.h"
#include "cinder/Matrix.h"
#include "cinder/Color.h"
#include "cinder/Shape2d.h"
#include "cinder/PolyLine.h"
#include "cinder/Exception.h"
#include "cinder/Surface.h"
#include "cinder/text/Font.h"
#include "cinder/Noncopyable.h"

#include <functional>
#include <map>

namespace cinder { namespace svg {

using FillRule = enum { FILL_RULE_NONZERO, FILL_RULE_EVENODD };
using LineCap = enum { LINE_CAP_BUTT, LINE_CAP_ROUND, LINE_CAP_SQUARE };
using LineJoin = enum { LINE_JOIN_MITER, LINE_JOIN_ROUND, LINE_JOIN_BEVEL };
using FontWeight = enum { WEIGHT_100, WEIGHT_200, WEIGHT_300, WEIGHT_400, WEIGHT_NORMAL = WEIGHT_400, WEIGHT_500, WEIGHT_600, WEIGHT_700, WEIGHT_BOLD = WEIGHT_700, WEIGHT_800, WEIGHT_900 };

using Align = enum { NONE, X_MIN_Y_MIN, X_MID_Y_MIN, X_MAX_Y_MIN, X_MIN_Y_MID, X_MID_Y_MID, X_MAX_Y_MID, X_MIN_Y_MAX, X_MID_Y_MAX, X_MAX_Y_MAX };
using MeetOrSlice = enum { MEET, SLICE };

//! Defines the coordinate system used by gradients and images.
using CoordinateSpace = enum { USER_SPACE_ON_USE, OBJECT_BOUNDING_BOX };

class Circle;
class ClipPath;
class Defs;
class Ellipse;
class ExcChildNotFound;
class Group;
class Image;
class Line;
class Node;
class Path;
class Polygon;
class Polyline;
class PreserveAspectRatio;
class Rect;
class Style;
class Styles;
class TextSpan;

using RenderVisitor = std::function<bool ( const Node &, svg::Style * )>;

//! Base class from which Renderers are derived.
class CI_API Renderer {
  public:
	virtual ~Renderer() = default;

	void setVisitor( const std::function<bool( const Node &, svg::Style * )> &visitor );

	virtual void start() {}
	virtual void finish() {}

	virtual void pushGroup( const Group & /*group*/, float /*opacity*/ ) {}
	virtual void popGroup() {}
	virtual void pushClipPath( const ClipPath & /* clippath */ ) {}
	virtual void popClipPath() {}
	virtual void drawPath( const svg::Path & /*path*/ ) {}
	virtual void drawPolyline( const svg::Polyline & /*polyline*/ ) {}
	virtual void drawPolygon( const svg::Polygon & /*polygon*/ ) {}
	virtual void drawLine( const svg::Line & /*line*/ ) {}
	virtual void drawRect( const svg::Rect & /*rect*/ ) {}
	virtual void drawCircle( const svg::Circle & /*circle*/ ) {}
	virtual void drawEllipse( const svg::Ellipse & /*ellipse*/ ) {}
	virtual void drawImage( const svg::Image & /*image*/ ) {}
	virtual void drawTextSpan( const svg::TextSpan & /*span*/ ) {}

	virtual void pushMatrix( const mat3 & /*m*/ ) {}
	virtual void popMatrix() {}
	virtual void pushStyle( const svg::Style & /*style*/ ) {}
	virtual void popStyle() {}
	virtual void pushFill( const class Paint & /*paint*/ ) {}
	virtual void popFill() {}
	virtual void pushStroke( const class Paint & /*paint*/ ) {}
	virtual void popStroke() {}
	virtual void pushFillOpacity( float /*opacity*/ ) {}
	virtual void popFillOpacity() {}
	virtual void pushStrokeOpacity( float /*opacity*/ ) {}
	virtual void popStrokeOpacity() {}
	virtual void pushStrokeWidth( float /*width*/ ) {}
	virtual void popStrokeWidth() {}
	virtual void pushFillRule( FillRule /*rule*/ ) {}
	virtual void popFillRule() {}
	virtual void pushLineCap( LineCap /*lineCap*/ ) {}
	virtual void popLineCap() {}
	virtual void pushLineJoin( LineJoin /*lineJoin*/ ) {}
	virtual void popLineJoin() {}
	virtual void pushMiterLimit( float /*miterLimit*/ ) {}
	virtual void popMiterLimit() {}
	virtual void pushDashArray( const std::vector<float> & /*dashArray*/ ) {}
	virtual void popDashArray() {}
	virtual void pushDashOffset( float /*dashOffset*/ ) {}
	virtual void popDashOffset() {}
	virtual void pushTextPen( const vec2 & /*penPos*/ ) {}
	virtual void popTextPen() {}
	virtual void pushTextRotation( float /*rotation*/ ) {}
	virtual void popTextRotation() {}

	bool		visit( const Node &node, svg::Style *style ) const {
		if( mVisitor )
			return ( *mVisitor )( node, style );
		else
			return true;
	}

  protected:
	// this is a shared_ptr to work around a bug in Clang 4.0
	std::shared_ptr<std::function<bool( const Node &, svg::Style * )>> mVisitor;

	friend class svg::Node;
};

//! SVG Value/Unit pair
class CI_API Value {
  public:
	enum Unit { USER, PX, PERCENT, PT, PC, MM, CM, INCH, EM, EX };

	Value() : mUnit( USER ), mValue( 0 ) {}
	Value( float value, Unit unit = USER ) : mUnit( unit ), mValue( value ) {}

	float asUser( float percentOf = 100, float dpi = 72, float fontSize = 12, float fontXHeight = 7 ) const;

	bool isUser() const { return mUnit == USER; }
	bool isPercent() const { return mUnit == PERCENT; }
	bool isPixels() const { return mUnit == PX; }

	static Value parse( const char **sInOut );
	static Value parse( const std::string &s );

	Unit  mUnit;
	float mValue;
};

//! SVG Paint specification for fill or stroke, including solids and gradients
class CI_API Paint {
  public:
	enum { NONE, COLOR, LINEAR_GRADIENT, RADIAL_GRADIENT };

	Paint();
	Paint( uint8_t type );
	Paint( const ColorA8u &color );
	Paint( const std::string &url ); // Marks Paint as "needs resolve".

	static Paint parse( const char *value, bool *specified, const Node *parentNode );

	bool            isNone() const { return mType == NONE; }
	bool            isLinearGradient() const { return mType == LINEAR_GRADIENT; }
	bool            isRadialGradient() const { return mType == RADIAL_GRADIENT; }
	bool            isTransparent() const;
	const ColorA8u &getColor( size_t idx = 0 ) const { return mStops[idx].second; }
	float           getOffset( size_t idx ) const { return mStops[idx].first; }
	size_t          getNumColors() const { return mStops.size(); }

	// only apply to gradients
	vec2    getCoords0() const { return mCoords0; } // (x1,y1) on linear, (cx,cy) on radial
	vec2    getCoords1() const { return mCoords1; } // (x2,y2) on linear, (fx,fy) on radial
	float   getRadius0() const { return mRadius0; } // (r) on radial
	float   getRadius1() const { return mRadius1; } // (fr) on radial
	bool    useObjectBoundingBox() const { return mUseObjectBoundingBox; }
	bool    specifiesTransform() const { return mSpecifiesTransform; }
	mat3    getTransform() const { return mTransform; }
	bool    specifiesSpreadMethod() const { return mSpecifiesSpreadMethod; }
	uint8_t getSpreadMethod() const { return mSpreadMethod; }

	const std::string &getId() const { return mId; }

	bool operator==( const Paint &rhs ) const { return mType == rhs.mType && mStops == rhs.mStops; }
	bool operator!=( const Paint &rhs ) const { return !( *this == rhs ); }

	uint8_t                                 mType;
	std::vector<std::pair<float, ColorA8u>> mStops;

	vec2    mCoords0, mCoords1;
	float   mRadius0, mRadius1;
	bool    mUseObjectBoundingBox;
	mat3    mTransform;
	bool    mSpecifiesTransform;
	uint8_t mSpreadMethod;
	bool    mSpecifiesSpreadMethod;
	bool    mNeedsResolve;

	std::string mId;
};

//! SVG Style for a node. Corresponds to SVG Styling: http://www.w3.org/TR/SVG/styling.html
class CI_API Style {
  public:
	Style();
	Style( const XmlTree &xml, const Node *parent );

	//! Returns a Style set appropriately for global defaults
	static Style makeGlobalDefaults();
	//! Marks all styles as unspecified
	void clear();

	bool				specifiesFill() const { return mSpecifiesFill; }
	void				unspecifyFill() { mSpecifiesFill = false; }
	const Paint&		getFill() const { return mFill; }
	void				setFill( const Paint &fill ) { mSpecifiesFill = true; mFill = fill; }
	static const Paint&	getFillDefault();

	bool				specifiesStroke() const { return mSpecifiesStroke; }
	void				unspecifyStroke() { mSpecifiesStroke = false; }
	const Paint&		getStroke() const { return mStroke; }
	void				setStroke( const Paint &stroke ) { mSpecifiesStroke = true; mStroke = stroke; }
	static const Paint&	getStrokeDefault();

	bool			specifiesOpacity() const { return mSpecifiesOpacity; }
	void			unspecifyOpacity() { mSpecifiesOpacity = false; }
	float			getOpacity() const { return mOpacity; }
	void			setOpacity( float opacity ) { mSpecifiesOpacity = true; mOpacity = opacity; }
	static float	getOpacityDefault() { return 1.0f; }

	bool			specifiesStrokeOpacity() const { return mSpecifiesStrokeOpacity; }
	void			unspecifyStrokeOpacity() { mSpecifiesStrokeOpacity = false; }
	float			getStrokeOpacity() const { return mStrokeOpacity; }
	void			setStrokeOpacity( float strokeOpacity ) { mSpecifiesStrokeOpacity = true; mStrokeOpacity = strokeOpacity; }
	static float	getStrokeOpacityDefault() { return 1.0f; }

	bool			specifiesFillOpacity() const { return mSpecifiesFillOpacity; }
	void			unspecifyFillOpacity() { mSpecifiesFillOpacity = false; }
	float			getFillOpacity() const { return mFillOpacity; }
	void			setFillOpacity( float fillOpacity ) { mSpecifiesFillOpacity = true; mFillOpacity = fillOpacity; }
	static float	getFillOpacityDefault() { return 1.0f; }

	bool			specifiesStrokeWidth() const { return mSpecifiesStrokeWidth; }
	void			unspecifyStrokeWidth() { mSpecifiesStrokeWidth = false; }
	float			getStrokeWidth() const { return mStrokeWidth; }
	void			setStrokeWidth( float strokeWidth ) { mSpecifiesStrokeWidth = true; mStrokeWidth = strokeWidth; }	
	static float	getStrokeWidthDefault() { return 1.0f; }

	bool			specifiesFillRule() const { return mSpecifiesFillRule; }
	void			unspecifyFillRule() { mSpecifiesFillRule = false; }
	FillRule		getFillRule() const { return mFillRule; }
	void			setFillRule( FillRule fillRule ) { mSpecifiesFillRule = true; mFillRule = fillRule; }	
	static FillRule	getFillRuleDefault() { return svg::FILL_RULE_NONZERO; }

	bool			specifiesLineCap() const { return mSpecifiesLineCap; }
	void			unspecifyLineCap() { mSpecifiesLineCap = false; }
	LineCap			getLineCap() const { return mLineCap; }
	void			setLineCap( LineCap lineCap ) { mSpecifiesLineCap = true; mLineCap = lineCap; }	
	static LineCap	getLineCapDefault() { return svg::LINE_CAP_BUTT; }

	bool			specifiesLineJoin() const { return mSpecifiesLineJoin; }
	void			unspecifyLineJoin() { mSpecifiesLineJoin = false; }
	LineJoin		getLineJoin() const { return mLineJoin; }
	void			setLineJoin( LineJoin lineJoin ) { mSpecifiesLineJoin = true; mLineJoin = lineJoin; }	
	static LineJoin	getLineJoinDefault() { return svg::LINE_JOIN_MITER; }

	bool			specifiesMiterLimit() const { return mSpecifiesMiterLimit; }
	void			unspecifyMiterLimit() { mSpecifiesMiterLimit = false; }
	float			getMiterLimit() const { return mMiterLimit; }
	void			setMiterLimit( float miterLimit ) { mSpecifiesMiterLimit = true; mMiterLimit = miterLimit; }
	static float	getMiterLimitDefault() { return 4; }

	bool								specifiesDashArray() const { return mSpecifiesDashArray; }
	void								unspecifyDashArray() { mSpecifiesDashArray = false; }
	const std::vector<float>&			getDashArray() const { return mDashArray; }
	void								setDashArray( const std::vector<float> &dashArray ) { mSpecifiesDashArray = true; mDashArray = dashArray; }
	static const std::vector<float>&	getDashArrayDefault() { static std::vector<float> sNone; return sNone; }

	bool			specifiesDashOffset() const { return mSpecifiesDashOffset; }
	void			unspecifyDashOffset() { mSpecifiesDashOffset = false; }
	float			getDashOffset() const { return mDashOffset; }
	void			setDashOffset( float dashOffset ) { mSpecifiesDashOffset = true; mDashOffset = dashOffset; }
	static float	getDashOffsetDefault() { return 0; }

	// clip path
	bool               specifiesClipPath() const { return mSpecifiesClipPath; }
	void               unspecifyClipPath() { mSpecifiesClipPath = false; }
	const std::string &getClipPath() const { return mClipPath; }
	void               setClipPath( const std::string &clipPath ) { mSpecifiesClipPath = true; mClipPath = clipPath; }

	// fonts
	bool                            specifiesFontFamilies() const { return mSpecifiesFontFamilies; }
	void                            unspecifyFontFamilies() { mSpecifiesFontFamilies = false; }
	const std::vector<std::string>&			getFontFamilies() const { return mFontFamilies; }
	std::vector<std::string>&				getFontFamilies() { return mFontFamilies; }
	void									setFontFamily( const std::string &family ) { mSpecifiesFontFamilies = true; mFontFamilies.clear(); mFontFamilies.push_back( family ); }
	void									setFontFamilies( const std::vector<std::string> &families ) { mSpecifiesFontFamilies = true; mFontFamilies = families; }
	static const std::vector<std::string>&	getFontFamiliesDefault();
	
	bool			specifiesFontSize() const { return mSpecifiesFontSize; }
	void			unspecifyFontSize() { mSpecifiesFontSize = false; }
	Value			getFontSize() const { return mFontSize; }
	void			setFontSize( const Value &fontSize ) { mSpecifiesFontSize = true; mFontSize = fontSize; }
	static Value	getFontSizeDefault() { return Value( 12 ); }

	bool				specifiesFontWeight() const { return mSpecifiesFontWeight; }
	void				unspecifyFontWeight() { mSpecifiesFontWeight = false; }
	FontWeight			getFontWeight() const { return mFontWeight; }
	void				setFontWeight( FontWeight weight ) { mSpecifiesFontWeight = true; mFontWeight = weight; }
	static FontWeight	getFontWeightDefault() { return svg::WEIGHT_NORMAL; }

	bool			specifiesVisible() const { return mSpecifiesVisible; }
	bool			isVisible() const { return mVisible; }
	void			setVisible( bool visible ) { mSpecifiesVisible = true; mVisible = visible; }
	void			unspecifyVisible() { mSpecifiesVisible = false; }
	
	bool			isDisplayNone() const { return mDisplayNone; }
	void			setDisplayNone( bool displayNone ) { mDisplayNone = displayNone; }

	void startRender( Renderer &renderer, const Node *node ) const;
	void finishRender( Renderer &renderer, const Node *node ) const;

	void parseClassAttribute( const std::string &stylePropertyString, const Node *parent );
	void parseStyleAttribute( const std::string &stylePropertyString, const Node *parent );
	bool parseProperty( const std::string &key, const std::string &value, const Node *parent );

	// Attempts to find a Style definition in one of its parents for the value stored in mId.
	void resolve( const Node *node ) const;

	bool operator==( const Style &other ) const;
	bool operator!=( const Style &other ) const { return !( *this == other ); }

	// Merges the right-hand Style into the left-hand one.
	void operator+=( const Style &other );
	// Merges the two styles.
	Style operator+( const Style &other ) const;

  protected:
	bool  mSpecifiesOpacity;
	float mOpacity;
	bool  mSpecifiesFillOpacity, mSpecifiesStrokeOpacity;
	float mFillOpacity, mStrokeOpacity;

	bool               mSpecifiesFill, mSpecifiesStroke;
	mutable Paint      mFill, mStroke; // Paint might need to be resolved from a 'const' method.
	bool               mSpecifiesStrokeWidth;
	float              mStrokeWidth;
	bool               mSpecifiesFillRule;
	FillRule           mFillRule;
	bool               mSpecifiesLineCap;
	LineCap            mLineCap;
	bool               mSpecifiesLineJoin;
	LineJoin           mLineJoin;
	bool               mSpecifiesMiterLimit;
	float              mMiterLimit;
	bool               mSpecifiesDashArray;
	std::vector<float> mDashArray;
	bool               mSpecifiesDashOffset;
	float              mDashOffset;
	bool               mSpecifiesClipPath;
	std::string        mClipPath;

	// fonts
	bool                     mSpecifiesFontFamilies, mSpecifiesFontSize, mSpecifiesFontWeight;
	std::vector<std::string> mFontFamilies;
	Value                    mFontSize;
	FontWeight               mFontWeight;

	// visibility
	bool mSpecifiesVisible, mVisible, mDisplayNone;
};

//! Base class for an element of an SVG Document
class CI_API Node {
  public:
	Node( Node *parent ) : mParent( parent ),  mSpecifiesTransform( false ), mBoundingBoxCached( false )
	{
		mUuid = nextUuid();
	}
	virtual ~Node() {}

	//! Returns the unique id for this node.
	size_t getUuid() const { return mUuid; }
	//! Returns the svg::Doc this Node is an element of
	class Doc *getDoc() const;
	//! Returns the immediate parent of this node
	const Node *getParent() const { return mParent; }
	//! Returns the tag of this Node when present (e.g. 'svg').
	const std::string &getTag() const { return mTag; }
	//! Returns the ID of this Node when present.
	const std::string &getId() const { return mId; }
	//! Returns a DOM-style path to this node.
	std::string getDomPath() const;
	//! Returns the style elements defined on this Node but not inherited from ancestors.
	const Style &getStyle() const
	{
		mStyle.resolve( this );
		return mStyle;
	}
	//! Sets the style defined on this Node but not inherited from ancestors.
	void setStyle( const Style &style ) { mStyle = style; }
	//! Returns the node's Style, including attributes inherited from its ancestors for attributes it does not specify
	Style calcInheritedStyle() const;

	//! Returns the ClipPath for this node. Returns NULL on failure.
	virtual const ClipPath *getClipPath() const;
		//! Returns the ClipPath for the specified \a style. Returns NULL on failure.
	virtual const ClipPath *getClipPath( const Style &style ) const;

	//! Returns whether the point \a pt is inside of the Node's shape.
	virtual bool containsPoint( const vec2 & /*pt*/ ) const { return false; }

	//! Renders the node and its descendants.
	void render( Renderer &renderer ) const;

	//! Finds the node with ID \a elementId amongst this Node's ancestors. Returns NULL on failure.
	virtual const Node *findInAncestors( const std::string &elementId ) const;
	//! Finds the svg::Paint node with ID \a elementId amongst this Node's ancestors. Returns a default svg::Paint instance on failure.
	Paint findPaintInAncestors( const std::string &paintName ) const;
	//! Recursively searches for a <tag> node and returns the first it finds. Returns NULL on failure.
	virtual const Node *findTagInAncestors( const std::string &tag ) const;

	//! Returns whether this Node specifies a transformation
	bool specifiesTransform() const { return mSpecifiesTransform; }
	//! Returns the local transformation of this node. Returns identity if the Node's transform isn't specified.
	mat3 getTransform() const { return mTransform; }
	//! Sets the local transformation of this node.
	void setTransform( const mat3 &transform ) { mTransform = transform; mSpecifiesTransform = true; }
	//! Removes the local transformation of this node, effectively making it the identity matrix.
	void unspecifyTransform() { mSpecifiesTransform = false; }
	//! Returns the inverse of the local transformation of this node. Returns identity if the Node's transform isn't specified.
	mat3 getTransformInverse() const { return ( mSpecifiesTransform ) ? inverse( mTransform ) : mat3(); }
	//! Returns the absolute transformation of this node, which includes inherited transformations.
	mat3 getTransformAbsolute() const;
	//! Returns the inverse of the absolute transformation of this node, which includes inherited transformations.
	mat3 getTransformAbsoluteInverse() const { return inverse( getTransformAbsolute() ); }

	//! Returns the local bounding box of the Node. Calculated and cached the first time it is requested.
	Rectf			getBoundingBox() const { if( ! mBoundingBoxCached ) { mBoundingBox = calcBoundingBox(); mBoundingBoxCached = true; } return mBoundingBox;  }
	//! Returns the absolute bounding box of the Node. Calculated and cached the first time it is requested.
	Rectf getBoundingBoxAbsolute() const { return getBoundingBox().transformed( getTransformAbsolute() ); }

	//! Returns a Shape2d representing the node in local coordinates. Not supported for Text.
	virtual Shape2d getShape() const { return Shape2d(); }
	//! Returns a Shape2d representing the node in absolute coordinates. Not supported for Text.
	Shape2d getShapeAbsolute() const { return getShape().transformed( getTransformAbsolute() ); }

	//! Returns node's fill, or the first among its ancestors when it has none
	const Paint &getFill() const;
	//! Returns node's stroke, or the first among its ancestors when it has none
	const Paint &getStroke() const;
	//! Returns node's opacity, or the first among its ancestors when it has none
	float getOpacity() const;
	//! Returns node's fill opacity, or the first among its ancestors when it has none
	float getFillOpacity() const;
	//! Returns node's stroke opacity, or the first among its ancestors when it has none
	float getStrokeOpacity() const;
	//! Returns node's fill rule, or the first among its ancestors when it has none
	FillRule getFillRule() const;
	//! Returns node's line cap, or the first among its ancestors when it has none
	LineCap getLineCap() const;
	//! Returns node's line join, or the first among its ancestors when it has none
	LineJoin getLineJoin() const;
	//! Returns node's miter limit, or the first among its ancestors when it has none
	float getMiterLimit() const;
	//! Returns node's dash array, or the first among its ancestors when it has none
	const std::vector<float> &getDashArray() const;
	//! Returns node's dash offset, or the first among its ancestors when it has none
	float getDashOffset() const;
	//! Returns node's stroke width, or the first among its ancestors when it has none
	float getStrokeWidth() const;
	//! Returns node's font families, or the first among its ancestors when it has none
	const std::vector<std::string> &getFontFamilies() const;
	//! Returns node's font size, or the first among its ancestors when it has none
	Value getFontSize() const;
	//! Returns whether this Node is visible, or the first among its ancestors when unspecified
	bool isVisible() const;
	//! Returns whether the Display property of this Node is set to 'None', preventing rendering of the node and its children
	bool isDisplayNone() const { return getStyle().isDisplayNone(); }
	//! Returns whether this type of node directly renders anything. Everything but groups and gradients.
	virtual bool isDrawable() const { return true; }

	//
	static size_t nextUuid()
	{
		static size_t uuid = 0;
		return ++uuid;
	}

  protected:
	Node( Node *parent, const XmlTree &xml );

	void         startRender( Renderer &renderer, const Style &style ) const;
	void         finishRender( Renderer &renderer, const Style &style ) const;
	virtual void renderSelf( Renderer &renderer ) const = 0;

	virtual Rectf calcBoundingBox() const { return Rectf( 0, 0, 0, 0 ); }

	static Paint parsePaint( const char *value, bool *specified, const Node *parentNode );
	static mat3  parseTransform( const std::string &value );
	static bool  parseTransformComponent( const char **c, mat3 *result );

	static std::string findStyleValue( const std::string &styleString, const std::string &key );
	void               parseStyle( const std::string &value );

	size_t        mUuid;
	Node         *mParent;
	std::string   mTag;
	std::string   mId;
	bool          mSpecifiesTransform;
	mat3          mTransform;
	mutable bool  mBoundingBoxCached;
	mutable Rectf mBoundingBox;
	Style         mStyle; // Try avoiding directly accessing this variable, use getStyle() instead if possible.

  private:
	void firstStartRender( Renderer &renderer ) const;

	friend class Group;
	friend class Use;
};

//! Base class for SVG Gradients. See SVG Gradients: http://www.w3.org/TR/SVG/pservers.html#Gradients
class CI_API Gradient : public Node {
  public:
	using SpreadMethod = enum { PAD, REFLECT, REPEAT };

	Gradient( Node *parent, const XmlTree &xml );

	class CI_API Stop {
	  public:
		Stop( const Node *parent, const XmlTree &xml );

		float    mOffset; // normalized 0-1
		ColorA8u mColor;
		float    mOpacity;
		bool     mSpecifiesColor, mSpecifiesOpacity;
	};

	bool useObjectBoundingBox() const { return mUseObjectBoundingBox; }

	SpreadMethod getSpreadMethod() const { return mSpreadMethod; }

	static SpreadMethod parseSpreadMethod( const std::string &s );

  protected:
	void renderSelf( Renderer & /*renderer*/ ) const override {}

	void          parse( const Node *parent, const XmlTree &xml );
	void          copyAttributesFrom( const Gradient &rhs );
	virtual Paint asPaint() const;

	std::vector<Stop> mStops;
	bool              mUseObjectBoundingBox;
	bool              mSpecifiesSpreadMethod;
	SpreadMethod      mSpreadMethod;
};

//! SVG Linear gradient
class CI_API LinearGradient : public Gradient {
  public:
	LinearGradient( Node *parent, const XmlTree &xml );

	Paint asPaint() const override;

	bool isDrawable() const override { return false; }

  protected:
	void parse( const XmlTree &xml );
	void copyAttributesFrom( const LinearGradient &rhs );

	Value mX1{ 0, Value::PERCENT };
	Value mY1{ 0, Value::PERCENT };
	Value mX2{ 100, Value::PERCENT };
	Value mY2{ 0, Value::PERCENT };

	friend class Gradient;
};

//! SVG Radial gradient
class CI_API RadialGradient : public Gradient {
  public:
	RadialGradient( Node *parent, const XmlTree &xml );

	Paint asPaint() const override;

	bool isDrawable() const override { return false; }

  protected:
	void parse( const XmlTree &xml );
	void copyAttributesFrom( const RadialGradient &rhs );

	Value mCx{ 50, Value::PERCENT };
	Value mCy{ 50, Value::PERCENT };
	Value mR{ 50, Value::PERCENT };
	Value mFx{ mCx };
	Value mFy{ mCy };
	Value mFr{ 0, Value::PERCENT };

	friend class Gradient;
};

//! SVG Circle element: http://www.w3.org/TR/SVG/shapes.html#CircleElement
class CI_API Circle : public Node {
  public:
	Circle( Node *parent ) : Node( parent ) {}
	Circle( Node *parent, const XmlTree &xml );

	vec2  getCenter() const { return mCenter; }
	void  setCenter( const vec2 &center ) { mCenter = center; }
	float getRadius() const { return mRadius; }
	void  setRadius( float radius ) { mRadius = radius; }

	bool containsPoint( const vec2 &pt ) const override { return distance2( pt, mCenter ) < mRadius * mRadius; }

	Shape2d getShape() const override;

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { return Rectf( mCenter.x - mRadius, mCenter.y - mRadius, mCenter.x + mRadius, mCenter.y + mRadius ); }

	vec2  mCenter;
	float mRadius;
};

//! SVG Ellipse element: http://www.w3.org/TR/SVG/shapes.html#EllipseElement
class CI_API Ellipse : public Node {
  public:
	Ellipse( Node *parent ) : Node( parent ) {}
	Ellipse( Node *parent, const XmlTree &xml );

	vec2  getCenter() const { return mCenter; }
	void  setCenter( const vec2 &center ) { mCenter = center; }
	float getRadiusX() const { return mRadiusX; }
	void  setRadiusX( float radiusX ) { mRadiusX = radiusX; }
	float getRadiusY() const { return mRadiusY; }
	void  setRadiusY( float radiusY ) { mRadiusY = radiusY; }

	bool containsPoint( const vec2 &pt ) const override;

	Shape2d getShape() const override;

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { return Rectf( mCenter.x - mRadiusX, mCenter.y - mRadiusY, mCenter.x + mRadiusX, mCenter.y + mRadiusY ); }

	vec2  mCenter;
	float mRadiusX, mRadiusY;
};

//! SVG Path element: http://www.w3.org/TR/SVG/paths.html#PathElement
class CI_API Path : public Node {
  public:
	Path( Node *parent ) : Node( parent ) {}
	Path( Node *parent, const XmlTree &xml );

	const Shape2d &getShape2d() const { return mPath; }
	void           appendShape2d( Shape2d *appendTo ) const;

	bool containsPoint( const vec2 &pt ) const override { return mPath.contains( pt ); }

	Shape2d getShape() const override { return mPath; }
	void    setShape( const Shape2d &shape ) { mPath = shape; }

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { return mPath.calcPreciseBoundingBox(); }

	Shape2d mPath;
};

//! SVG Line element: http://www.w3.org/TR/SVG/shapes.html#LineElement
class CI_API Line : public Node {
  public:
	Line( Node *parent ) : Node( parent ) {}
	Line( Node *parent, const XmlTree &xml );

	const vec2 &getPoint1() const { return mPoint1; }
	const vec2 &getPoint2() const { return mPoint2; }

	Shape2d getShape() const override;

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { return Rectf( std::min( mPoint1.x, mPoint2.x ), std::min( mPoint1.y, mPoint2.y ), std::max( mPoint1.x, mPoint2.x ), std::max( mPoint1.y, mPoint2.y ) ); }

	vec2 mPoint1, mPoint2;
};

//! SVG Rect element: http://www.w3.org/TR/SVG/shapes.html#RectElement
class CI_API Rect : public Node {
  public:
	Rect( Node *parent ) : Node( parent ) {}
	Rect( Node *parent, const XmlTree &xml );

	const Rectf &getRect() const { return mRect; }
	void         setRect( const Rectf &rect ) { mRect = rect; }
	void         setWidth( float width ) { mRect = Rectf( mRect.x1, mRect.y1, mRect.x1 + width, mRect.y2 ); }
	void         setHeight( float height ) { mRect = Rectf( mRect.x1, mRect.y1, mRect.x2, mRect.y1 + height ); }

	float        getRx() const;
	float        getRy() const;

	bool containsPoint( const vec2 &pt ) const override { return mRect.contains( pt ); }

	Shape2d getShape() const override;

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { return mRect; }

	Rectf mRect;
	Value mRx; // No default value.
	Value mRy; // No default value.
};

//! SVG Polygon Element: http://www.w3.org/TR/SVG/shapes.html#PolygonElement
class CI_API Polygon : public Node {
  public:
	Polygon( Node *parent ) : Node( parent ) {}
	Polygon( Node *parent, const XmlTree &xml );

	const PolyLine2f &getPolyLine() const { return mPolyLine; }
	PolyLine2f &      getPolyLine() { return mPolyLine; }

	bool containsPoint( const vec2 &pt ) const override { return mPolyLine.contains( pt ); }

	Shape2d getShape() const override;

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { return Rectf( mPolyLine.getPoints() ); }

	PolyLine2f mPolyLine;
};

//! SVG Polyline Element: http://www.w3.org/TR/SVG/shapes.html#PolylineElement
class CI_API Polyline : public Node {
  public:
	Polyline( Node *parent ) : Node( parent ) {}
	Polyline( Node *parent, const XmlTree &xml );

	const PolyLine2f &getPolyLine() const { return mPolyLine; }
	PolyLine2f &      getPolyLine() { return mPolyLine; }

	bool containsPoint( const vec2 &pt ) const override { return mPolyLine.contains( pt ); }

	Shape2d getShape() const override;

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { return Rectf( mPolyLine.getPoints() ); }

	PolyLine2f mPolyLine;
};

//! SVG Use Element, which instantiates a different element: http://www.w3.org/TR/SVG/struct.html#UseElement
class CI_API Use : public Node {
  public:
	Use( Node *parent, const XmlTree &xml );

	bool isDrawable() const override { return false; }

	Shape2d	getShape() const override { if( mReferenced ) return mReferenced->getShape(); else return Shape2d(); }

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { if( mReferenced ) return mReferenced->getBoundingBox(); else return Rectf(0,0,0,0); }

	void parse( const XmlTree &xml );

	const Node *mReferenced;
};

//!
class CI_API PreserveAspectRatio {
public:
	PreserveAspectRatio() = default;

	explicit PreserveAspectRatio( Align align, MeetOrSlice meetOrSlice = MEET )
		: align( align )
		, meetOrSlice( meetOrSlice )
	{
	}

	explicit PreserveAspectRatio( const std::string &value );

	mat3 calcTransform( const Rectf &element, const Rectf &viewBox, bool normalized = false ) const;

	Align       align{ X_MID_Y_MID };
	MeetOrSlice meetOrSlice{ MEET };
};

//! SVG Image Element. Represents an unpremultiplied bitmap. http://www.w3.org/TR/SVG/struct.html#ImageElement
class CI_API Image : public Node {
  public:
	Image() : Node(nullptr) {}
	Image( Node *parent, const XmlTree &xml );

	operator bool() const { return bool(mImage); }

	const Rectf &              getRect() const { return mBounds; }
	std::shared_ptr<Surface8u> getSurface() const { return mImage; }

	bool containsPoint( const vec2 &pt ) const override { return mBounds.contains( pt ); }

	//! Returns a transformation matrix for the texture coordinates.
	const mat3 &getTextureMatrix() const { return mTextureMatrix; }

  protected:
	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override { return mBounds; }

	static std::shared_ptr<Surface8u> parseDataImage( const std::string &data );

	Rectf                      mBounds;
	fs::path                   mFilePath;
	std::shared_ptr<Surface8u> mImage;
	mat3                       mTextureMatrix;
};

using TextSpanRef = std::shared_ptr<TextSpan>;

//! SVG tspan Element. Generally owned by a svg::Text Node. http://www.w3.org/TR/SVG/text.html#TSpanElement
class CI_API TextSpan : public Node {
  public:
	class CI_API Attributes {
	  public:
		Attributes() {}
		Attributes( const XmlTree &xml );

		void startRender( Renderer &renderer ) const;
		void finishRender( Renderer &renderer ) const;

		void setTextPen( const vec2 &textPen );

		std::vector<Value> mX, mY;
		float              mDx, mDy;
		std::vector<Value> mRotate;
		float              mTextLength;
		float              mLengthAdjust;
		std::vector<Value> mLetterSpacing;
	};

	TextSpan( Node *parent, const XmlTree &xml );
	TextSpan( Node *parent, const std::string &spanString );

	const std::string &getString() const { return mString; }
	void               setString( const std::string &s ) { mString = s; }
	text::Font *       getFont() const;
	//! Returns a vector of glyph IDs and positions for the string, ignoring rotation. Cached and lazily calculated.
	std::vector<std::pair<uint16_t, vec2>> getGlyphMeasures() const;
	vec2                                   getTextPen() const;
	void                                   setTextPen( const vec2 &textPen );
	float                                  getRotation() const;
	Value                                  getLetterSpacing() const;

	std::vector<TextSpanRef> &      getSpans() { return mSpans; }
	const std::vector<TextSpanRef> &getSpans() const { return mSpans; }

  protected:
	void renderSelf( Renderer &renderer ) const override;

	bool                                                            mIgnoreAttributes; // TextSpans that are actually the contents of Text's attributes should be ignored
	Attributes                                                      mAttributes;
	std::string                                                     mString;
	mutable text::Font *                                            mFont;
	mutable std::shared_ptr<std::vector<std::pair<uint16_t, vec2>>> mGlyphMeasures;
	mutable std::shared_ptr<Shape2d>                                mShape;

	std::vector<TextSpanRef> mSpans;

	friend class Text;
};

//! SVG Text element. http://www.w3.org/TR/SVG/text.html#TextElement
class CI_API Text : public Node {
  public:
	Text( Node *parent, const XmlTree &xml );

	vec2  getTextPen() const;
	void  setTextPen( const vec2 &textPen ) { mAttributes.setTextPen( textPen ); }
	float getRotation() const;
	Value getLetterSpacing() const;

	std::vector<TextSpanRef> &      getSpans() { return mSpans; }
	const std::vector<TextSpanRef> &getSpans() const { return mSpans; }

  protected:
	void renderSelf( Renderer &renderer ) const override;

	TextSpan::Attributes mAttributes;

	std::vector<TextSpanRef> mSpans;
};

//! Represents a group of SVG elements. http://www.w3.org/TR/SVG/struct.html#Groups
class CI_API Group : public Node, private Noncopyable {
  public:
	Group( Node *parent ) : Node( parent ) {}
	Group( Node *parent, const XmlTree &xml );
	~Group() override;

	//! Recursively searches for a child element of type <tt>svg::T</tt> named \a id. Returns NULL on failure to find the object or if it is not of type T.
    template<typename T>
	const T*				find( const std::string &id ) const { return dynamic_cast<const T*>( findNode( id ) ); }
	//! Recursively searches for a child element of type <tt>svg::T</tt> named \a id. Returns NULL on failure to find the object or if it is not of type T.
    template<typename T>
	T*						find( const std::string &id ) { return dynamic_cast<T*>( findNode( id ) ); }
	//! Recursively searches for a child element named \a id. Returns NULL on failure.
	virtual const Node*		findNode( const std::string &id, bool recurse = true ) const;
	//! Recursively searches for a child element named \a id. Returns NULL on failure.
	Node*					findNode( const std::string &id, bool recurse = true ) { return const_cast<Node*>( const_cast<const Group*>( this )->findNode( id, recurse ) ); }
	//! Recursively searches for a child element of type <tt>svg::T</tt> whose name contains \a idPartial. Returns NULL on failure to find the object or if it is not of type T.
    template<typename T>
	const T*				findByIdContains( const std::string &idPartial ) const { return dynamic_cast<const T*>( findNodeByIdContains( idPartial ) ); }
	//! Recursively searches for a child element whose name contains \a idPartial. Returns NULL on failure. (null_ptr later?)
	const Node*				findNodeByIdContains( const std::string &idPartial, bool recurse = true ) const;
	//! Recursively searches for a child element with the specified \a tag. Returns NULL on failure.
	virtual const Node*		findNodeByTag( const std::string &tag, bool recurse = true ) const;
	//! Finds the node with ID \a elementId amongst this Node's ancestors. Returns NULL on failure.
	const Node *findInAncestors( const std::string &elementId ) const override;
	//! Recursively searches for a <tag> node and returns the first it finds. Returns NULL on failure.
	const Node *findTagInAncestors( const std::string &elementTag ) const override;
	//! Returns a reference to the child named \a id. Throws svg::ExcChildNotFound if not found.
	const Node &getChild( const std::string &id ) const;
	//! Returns a reference to the child named \a id. Throws svg::ExcChildNotFound if not found.
	Node &getChild( const std::string &id ) { return const_cast<Node &>( const_cast<const Group *>( this )->getChild( id ) ); }
	//! Returns a reference to the child named \a id. Throws svg::ExcChildNotFound if not found.
	const Node &operator/( const std::string &id ) const { return getChild( id ); }

	//! Returns the merged Shape2d for all children of the group
	Shape2d getShape() const override { return getMergedShape2d(); }

	//! Appends the merged Shape2d for the group to \a appentTo.
	void appendMergedShape2d( Shape2d *appendTo ) const;

	//! Returns a reference to the list of the Group's children.
	const std::list<Node *> &getChildren() const { return mChildren; }
	//! Returns a reference to the list of the Group's children.
	std::list<Node *> &getChildren() { return mChildren; }
	//! Returns a reference to the child at \a index. Throws svg::ExcChildNotFound if \a index is out of range.
	const Node &getChild( size_t index ) const;
	//! Returns a reference to the child at \a index. Throws svg::ExcChildNotFound if \a index is out of range.
	Node &getChild( size_t index ) { return const_cast<Node &>( const_cast<const Group *>( this )->getChild( index ) ); }

	//! Recursively iterates all Nodes in Group or Doc, passing each to \a fn to be optionally manipulated
	virtual void iterate( const std::function<void( Node * )> &fn );

	//!
	static Node *create( Node *parent, const XmlTree &xml );
	
	bool isDrawable() const override { return false; }

  protected:
	Node *  nodeUnderPoint( const vec2 &absolutePoint, const mat3 &parentInverseMatrix ) const;
	Shape2d getMergedShape2d() const;

	void  renderSelf( Renderer &renderer ) const override;
	Rectf calcBoundingBox() const override;

	virtual void parse( const XmlTree &xml );

	std::list<Node *> mChildren;

	friend class Node;
};

//!
class CI_API Defs : public Group {
  public:
	Defs( Node *parent ) : Group( parent ) {}
	Defs( Node *parent, const XmlTree &xml );

	const Node *findNode( const std::string &id, bool recurse ) const override;

  protected:
	void renderSelf( Renderer &renderer ) const override
	{ /* never render */
	}

	XmlTree mXml;
};

//! SVG ClipPath element: https://www.w3.org/TR/SVG/render.html#ClippingAndMasking
class CI_API ClipPath : public Group {
  public:
	ClipPath( Node *parent )
		: Group( parent )
	{
	}
	ClipPath( Node *parent, const XmlTree &xml );

protected:
	void renderSelf( Renderer &renderer ) const override
	{ /* never render */
	}
};

//!
class CI_API Styles : public Node {
  public:
	Styles( Node *parent ) : Node( parent ) {}
	Styles( Node *parent, const XmlTree &xml );

	bool   empty() const { return mStyleList.empty(); }
	size_t size() const { return mStyleList.size(); }

	Style findStyle( const std::string &id ) const;

	void renderSelf( Renderer &renderer ) const override {}

  protected:
	std::unordered_map<std::string, Style> mStyleList;
};


using DocRef = std::shared_ptr<Doc>;
//! Represents an SVG Document. See SVG Document Structure http://www.w3.org/TR/SVG/struct.html
class CI_API Doc : public Group {
  public:
	Doc();
	Doc( const fs::path &filePath );
	Doc( const DataSourceRef &dataSource, const fs::path &filePath = fs::path() );

	static DocRef create( const fs::path &filePath );
	static DocRef create( const DataSourceRef &dataSource, const fs::path &filePath = fs::path() );
	static DocRef createFromSvgz( const DataSourceRef &dataSource, const fs::path &filePath = fs::path() );

	//! Returns the width of the document in pixels
	float getWidth() const { return mBounds.getWidth(); }
	//! Returns the height of the document in pixels
	float getHeight() const { return mBounds.getHeight(); }
	//! Returns the size of the document in pixels
	vec2 getSize() const { return { getWidth(), getHeight() }; }
	//! Returns the aspect ratio of the Doc (width / height)
	float getAspectRatio() const { return getWidth() / getHeight(); }
	//! Returns the bounds of the Doc (0,0,width,height)
	const Rectf &getBounds() const { return mBounds; }

	//! Returns the document's dots-per-inch. Currently hardcoded to 72.
	float getDpi() const { return 72.0f; }

	//! Returns the top-most Node which contains \a pt. Returns NULL if no Node contains the point.
	Node *nodeUnderPoint( const vec2 &pt ) const;

	//! Utility function to load an image relative to the document. Caches results.
	std::shared_ptr<Surface8u> loadImage( const fs::path &relativePath );

  private:
	void loadDoc( const DataSourceRef &source, const fs::path &filePath );

	void renderSelf( Renderer &renderer ) const override;

	std::shared_ptr<XmlTree>                       mXmlTree;
	std::map<fs::path, std::shared_ptr<Surface8u>> mImageCache;

	fs::path            mFilePath;
	Rectf               mBounds;
	Rectf               mViewBox;
};

//! SVG Exception base-class
class CI_API Exc : public Exception
{};

class CI_API ValueExc : public Exc
{};

class CI_API FloatParseExc : public Exc
{};

class CI_API PathParseExc : public Exc
{};

class CI_API TransformParseExc : public Exc
{};

class CI_API ExcChildNotFound : public Exc {
  public:
	ExcChildNotFound( const std::string &child );
};


} } // namespace cinder::svg
