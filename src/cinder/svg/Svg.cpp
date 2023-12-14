/*
 Copyright (c) 2012, The Cinder Project
 All rights reserved.

 Copyright (c) Microsoft Open Technologies, Inc. All rights reserved.

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

#include "cinder/svg/Svg.h"
#include "cinder/Base64.h"
#include "cinder/ImageIo.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"
#include "cinder/css/CSSParser.h"
#include "cinder/text/Text.h"

using namespace std;

namespace cinder { namespace svg {

namespace {

float parseFloat( const char **sInOut )
{
	char          temp[256];
	unsigned char i = 0;
	const char   *s = *sInOut;
	while( *s && ( isspace( *s ) || *s == ',' ) )
		s++;
	if( !s )
		throw FloatParseExc();
	if( isNumeric( *s ) ) {
		while( *s == '-' || *s == '+' ) {
			temp[i++] = *s;
			if( !i )
				throw FloatParseExc(); // buffer overflow
			s++;
		}
		bool parsingExponent = false;
		bool startingExponent = false;
		bool seenDecimal = false;
		while( *s && ( parsingExponent || ( *s != '-' && *s != '+' ) ) && isNumeric( *s ) ) {
			startingExponent = false;
			if( *s == '.' && seenDecimal )
				break;
			else if( *s == '.' )
				seenDecimal = true;
			temp[i++] = *s;
			if( !i )
				throw FloatParseExc(); // buffer overflow
			if( *s == 'e' || *s == 'E' ) {
				parsingExponent = true;
				startingExponent = true;
			}
			else
				parsingExponent = false;
			s++;
		}
		if( startingExponent ) { // if we got a false positive on an exponent, for example due to "ex" or "em" unit, back up one
			--i;
			--s;
		}
		temp[i] = 0;
		auto result = float( strtod( temp, nullptr ) );
		*sInOut = s;
		return result;
	}
	else
		throw FloatParseExc();
}

// parses float from comma-separated parenthetical list
vector<float> parseFloatList( const char **c )
{
	vector<float> result;
	while( **c && isspace( **c ) )
		( *c )++;
	if( **c != '(' )
		return result; // failure
	( *c )++;
	do {
		result.push_back( parseFloat( c ) );
		while( **c && ( **c == ',' || isspace( **c ) ) )
			( *c )++;
	} while( **c && **c != ')' );

	// get rid of trailing closing paren
	if( **c )
		( *c )++;

	return result;
}

vector<Value> parseValueList( const char **c, bool requireParens = true )
{
	vector<Value> result;
	while( **c && isspace( **c ) )
		( *c )++;
	if( requireParens ) {
		if( **c != '(' )
			return result; // failure
		( *c )++;
	}
	do {
		result.push_back( Value::parse( c ) );
		while( **c && ( **c == ',' || isspace( **c ) ) )
			( *c )++;
	} while( **c && **c != ')' );

	// get rid of trailing closing paren
	if( requireParens && **c )
		( *c )++;

	return result;
}

vector<Value> readValueList( const std::string &s, bool requireParens = true )
{
	const char *temp = s.c_str();
	return parseValueList( &temp, requireParens );
}

vector<Value> readValueList( const char *c, bool requireParens = true )
{
	const char *temp = c;
	return parseValueList( &temp, requireParens );
}

Value readValue( const std::string &s, float minV, float maxV )
{
	const char *temp = s.c_str();
	Value       result = Value::parse( &temp );
	if( result.value() < minV )
		result = minV;
	if( result.value() > maxV )
		result = maxV;
	return result;
}

Value readValue( const std::string &s )
{
	const char *temp = s.c_str();
	Value       result = Value::parse( &temp );
	return result;
}

// breaks comma-separated list into strings, optionally strips single or double quotes; removes all leading and trailing white space
vector<string> readStringList( const std::string &s, bool stripQuotes = false )
{
	vector<string> result = split( s, "," );
	for( auto &resultIt : result ) {
		auto trimmed = trim( resultIt );
		if( stripQuotes ) {
			trimmed.erase( std::remove( trimmed.begin(), trimmed.end(), '"' ), trimmed.end() );
			trimmed.erase( std::remove( trimmed.begin(), trimmed.end(), '\'' ), trimmed.end() );
		}
		resultIt = trimmed;
	}

	return result;
}

} // anonymous namespace

////////////////////////////////////////////////////////////////////////////////////
// Renderer
void Renderer::setVisitor( const function<bool( const Node &, Style * )> &visitor )
{
	mVisitor = std::make_shared<function<bool( const Node &, Style * )>>( visitor );
}

////////////////////////////////////////////////////////////////////////////////////
// Statics
namespace {
const Paint sPaintNone = Paint();
const Paint sPaintBlack = Paint( Color::black() );
} // namespace

////////////////////////////////////////////////////////////////////////////////////
// Paint
Paint::Paint()
	: Paint( NONE )
{
}

Paint::Paint( Type type, std::string id )
	: mType( type )
	, mId( std::move( id ) )
{
	mStops.emplace_back( 0.0f, ColorA8u::black() );
}

Paint::Paint( const ColorA8u &color )
	: mType( COLOR )
{
	mStops.emplace_back( 0.0f, color );
}

Paint::Paint( std::string url )
	: mNeedsResolve( true )
	, mId( std::move( url ) )
{
}

Paint Paint::parse( const char *value, bool *specified, const Node *parentNode )
{
	*specified = false;
	while( *value && isspace( *value ) )
		value++;

	if( !*value )
		return {};

	if( !strncmp( value, "inherit", 7 ) ) {
		*specified = false;
		return {};
	}

	if( !strncmp( value, "currentColor", 12 ) ) {
		*specified = true;
		return { parentNode->getColor() };
	}

	if( value[0] == '#' ) { // hex color
		uint32_t v = 0;
		if( strlen( value ) > 4 ) {
			for( int c = 0; c < 6; ++c ) {
				char     ch = charToUpper( value[1 + c] );
				uint32_t col = ch - ( ( ch > '9' ) ? ( 'A' - 10 ) : '0' );
				v += col << ( ( 5 - c ) * 4 );
			}
		}
		else { // 3-digit hex shorthand; double each digit
			for( int c = 0; c < 3; ++c ) {
				char     ch = charToUpper( value[1 + c] );
				uint32_t col = ch - ( ( ch > '9' ) ? ( 'A' - 10 ) : '0' );
				v += col << ( ( 5 - ( c * 2 + 0 ) ) * 4 );
				v += col << ( ( 5 - ( c * 2 + 1 ) ) * 4 );
			}
		}
		*specified = true;
		return { ColorA8u( char( v >> 16 ), char( v >> 8 ) & 255, char( v ) & 255, 255 ) };
	}
	else if( !strncmp( value, "none", 4 ) ) {
		*specified = true;
		return {};
	}
	else if( !strncmp( value, "rgba", 4 ) ) {
		vector<Value> values = readValueList( value + 4 );
		if( values.size() == 4 ) {
			*specified = true;
			return { ColorA8u( uint8_t( values[0].asUser( 255 ) ), uint8_t( values[1].asUser( 255 ) ), uint8_t( values[2].asUser( 255 ) ), uint8_t( 255 * values[3].asUser( 1 ) ) ) };
		}
		*specified = false;
		return {};
	}
	else if( !strncmp( value, "rgb", 3 ) ) {
		vector<Value> values = readValueList( value + 3 );
		if( values.size() == 3 ) {
			*specified = true;
			return { ColorA8u( uint8_t( values[0].asUser( 255 ) ), uint8_t( values[1].asUser( 255 ) ), uint8_t( values[2].asUser( 255 ) ), 255 ) };
		}
		*specified = false;
		return {};
	}
	else if( !strncmp( value, "url", 3 ) ) {
		char        id[1024];
		const char *hash = strchr( value, '#' );
		const char *closeParen = strchr( value, ')' );
		if( ( !closeParen ) || ( !hash ) || ( closeParen - hash >= 1024 ) )
			return {};
		strncpy( id, hash + 1, closeParen - hash - 1 );
		id[closeParen - hash - 1] = 0;

		Paint result = parentNode->findPaintInAncestors( id );
		if( ( closeParen + 1 ) ) // Parse fallback color.
			result.mFallback = std::make_shared<Paint>( parse( closeParen + 1, specified, parentNode ) );

		*specified = true;

		return result;
	}
	else { // try to find color amongst named colors
		Color8u result = svgNameToRgb( value, specified );
		if( specified )
			return { result };
		else
			return {};
	}
}

bool Paint::isTransparent() const
{
	for( const auto &[color, offset] : mStops )
		if( color.a < 1 )
			return true;

	return false;
}

const ColorA8u &Paint::getColor() const
{
	static ColorA8u sBlack{ 0, 0, 0, 255 };
	static ColorA8u sTransparent{ 0, 0, 0, 0 };

	switch( mType ) {
	case NONE:
		return sTransparent;
	case COLOR:
		return getColor( 0 );
	default:
		if( mFallback )
			return mFallback->getColor();
		return sBlack;
	}
}

void Paint::set( float offset, const ColorA8u &color )
{
	const auto t = glm::clamp( offset, 0.0f, 1.0f );
	for( auto &stop : mStops ) {
		if( approxEqual( t, stop.offset ) ) {
			stop.color = color;
			return;
		}
		if( t < stop.offset )
			break;
	}

	mStops.emplace_back( offset, color );
	std::sort( mStops.begin(), mStops.end() );
}

ColorA8u Paint::at( float t, bool preMultiply ) const
{
	ColorA8u result;

	const auto &lo = floor( t );
	const auto &hi = ceil( t );

	// See: https://svgwg.org/svg2-draft/pservers.html#GradientStops
	// "If two gradient stops have the same offset value, then the latter gradient stop controls the color value at the overlap point."
	if( approxEqual( lo.offset, hi.offset ) ) {
		result = hi.color; // TODO: check specifiesColor?
	}
	else {
		float f = clamp( ( t - lo.offset ) / ( hi.offset - lo.offset ), 0.0f, 1.0f );
		result = lo.color.lerp( uint8_t( f * 255.0f ), hi.color ); // TODO: check specifiesColor?
	}

	return preMultiply ? result.premultiplied() : result;
}

const Paint::Stop &Paint::floor( float t ) const
{
	assert( !mStops.empty() );

	for( auto itr = mStops.rbegin(); itr != mStops.rend(); ++itr ) {
		if( itr->offset <= t )
			return *itr;
	}

	return mStops.front();
}

const Paint::Stop &Paint::ceil( float t ) const
{
	assert( !mStops.empty() );

	for( auto itr = mStops.begin(); itr != mStops.end(); ++itr ) {
		if( itr->offset > t )
			return *itr;
	}

	return mStops.back();
}

std::unique_ptr<uint8_t[]> Paint::data( int32_t width, int32_t height, bool preMultiply, float from, float to ) const
{
	auto result = std::make_unique<uint8_t[]>( size_t( width ) * size_t( height ) * sizeof( ColorA8u ) );

	for( int x = 0; x < width; ++x ) {
		float t = mix( from, to, float( x ) / float( width - 1 ) );

		const auto c = at( t, preMultiply );
		for( int y = 0; y < height; ++y ) {
			const int64_t i = ( int64_t( x ) + int64_t( y ) * int64_t( width ) ) * sizeof( ColorA8u );
			result[size_t( i ) + 0] = uint8_t( c.r );
			result[size_t( i ) + 1] = uint8_t( c.g );
			result[size_t( i ) + 2] = uint8_t( c.b );
			result[size_t( i ) + 3] = uint8_t( c.a );
		}
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Style
Style::Style()
{
	clear();
}

Style::Style( const XmlTree &xml, const Node *parent )
{
	clear();

	// Make sure to parse the 'color' property before 'fill' or 'stroke',
	// so that 'fill="currentColor"' works. See: https://www.w3.org/TR/SVGTiny12/painting.html#ColorProperty
	if( xml.hasAttribute( "color" ) )
		parseProperty( "color", xml.getAttribute( "color" ).getValue(), parent );

	for( const auto &attrib : xml.getAttributes() ) {
		if( attrib.getName() == "class" )
			parseClassAttribute( attrib.getValue(), parent );
		else if( attrib.getName() == "style" )
			parseStyleAttribute( attrib.getValue(), parent );
		else
			parseProperty( attrib.getName(), attrib.getValue(), parent );
	}
}

Style Style::makeGlobalDefaults()
{
	Style result;

	result.setColor( getColorDefault() );
	result.setFill( getFillDefault() );
	result.setStroke( getStrokeDefault() );
	result.setFillOpacity( getFillOpacityDefault() );
	result.setStrokeOpacity( getStrokeOpacityDefault() );

	result.setStrokeWidth( getStrokeWidthDefault() );
	result.setFillRule( getFillRuleDefault() );
	result.setLineCap( getLineCapDefault() );
	result.setLineJoin( getLineJoinDefault() );
	result.setMiterLimit( getMiterLimitDefault() );
	result.setDashArray( getDashArrayDefault() );
	result.setDashOffset( getDashOffsetDefault() );
	result.setStopColor( getStopColorDefault() );
	result.setStopOpacity( getStopOpacityDefault() );

	result.setFontFamilies( getFontFamiliesDefault() );
	result.setFontSize( getFontSizeDefault() );
	result.setFontWeight( getFontWeightDefault() );
	result.setTextAnchor( getTextAnchorDefault() );

	result.setVisible( true );
	result.setDisplayNone( false );

	return result;
}

void Style::clear()
{
	mSpecifiesColor = false;
	mSpecifiesFill = mSpecifiesStroke = false;
	mSpecifiesOpacity = mSpecifiesFillOpacity = mSpecifiesStrokeOpacity = false;
	mOpacity = 1.0f;
	mSpecifiesStrokeWidth = false;
	mSpecifiesFillRule = false;
	mSpecifiesLineCap = false;
	mSpecifiesLineJoin = false;
	mSpecifiesMiterLimit = false;
	mSpecifiesDashArray = false;
	mSpecifiesDashOffset = false;
	mSpecifiesClipPath = false;
	mSpecifiesStopColor = false;
	mSpecifiesStopOpacity = false;
	mSpecifiesFontFamilies = mSpecifiesFontSize = mSpecifiesFontWeight = mSpecifiesTextAnchor = false;
	mSpecifiesVisible = false;
	mVisible = true;
	mDisplayNone = false;
	mClipPath = nullptr;
}

const ColorA8u &Style::getColorDefault()
{
	static ColorA8u sBlack( 0, 0, 0, 255 ); // Default color depends on user agent.
	return sBlack;
}

const Paint &Style::getFillDefault()
{
	return sPaintBlack;
}
const Paint &Style::getStrokeDefault()
{
	return sPaintNone;
}

text::Font *Style::getFont() const
{
	return Style::getFont( mFontFamilies, mFontSize.asUser() );
}

text::Font *Style::getFont( const std::vector<std::string> &fontFamilies, float fontSize )
{
	for( const auto &fontFamily : fontFamilies ) {
		try {
			text::Font *result = font( text::loadSystemFace( fontFamily ), fontSize );
			if( !result )
				throw Exception( "Failed to load font '" + fontFamily + "'" );

			return result;
		}
		catch( Exception &exc ) {
			CI_LOG_W( exc.what() << " - loading default font." );

			if( fontFamilies != getFontFamiliesDefault() )
				return getFont( getFontFamiliesDefault(), fontSize );
		}
	}
	return nullptr;
}

const std::vector<std::string> &Style::getFontFamiliesDefault()
{
	static shared_ptr<vector<string>> sDefault;
	if( !sDefault ) {
		sDefault = std::make_shared<vector<string>>();
		sDefault->push_back( "Times New Roman" ); // Most browsers use Times New Roman as the default font.
	}

	return *sDefault;
}

void Style::parseClassAttribute( const std::string &stylePropertyString, const Node *parent )
{
	// Find <style> tag in ancestors.
	auto styles = dynamic_cast<const Styles *>( parent->findTagInAncestors( "style" ) );

	// Merge styles.
	if( styles )
		*this += styles->findStyle( stylePropertyString );
}

void Style::parseStyleAttribute( const std::string &stylePropertyString, const Node *parent )
{
	// separate into pairs based on semicolons
	vector<string> valuePairs = split( stylePropertyString, ';' );
	for( const auto &pair : valuePairs ) {
		vector<string> valuePair = split( pair, ':' );
		if( valuePair.size() != 2 )
			continue;

		parseProperty( trim( valuePair[0] ), trim( valuePair[1] ), parent );
	}
}

bool Style::parseProperty( const std::string &key, const std::string &value, const Node *parent )
{
	if( key == "color" ) {
		mColor = Paint::parse( value.c_str(), &mSpecifiesColor, parent ).getColor();
		return true;
	}
	else if( key == "fill" ) {
		mFill = Paint::parse( value.c_str(), &mSpecifiesFill, parent );
		return true;
	}
	else if( key == "stroke" ) {
		mStroke = Paint::parse( value.c_str(), &mSpecifiesStroke, parent );
		return true;
	}
	else if( key == "opacity" ) {
		mOpacity = readValue( value, 0, 1 ).asUser();
		mSpecifiesOpacity = true;
		return true;
	}
	else if( key == "fill-opacity" ) {
		mFillOpacity = readValue( value, 0, 1 ).asUser();
		mSpecifiesFillOpacity = true;
		return true;
	}
	else if( key == "stroke-opacity" ) {
		mStrokeOpacity = readValue( value, 0, 1 ).asUser();
		mSpecifiesStrokeOpacity = true;
		return true;
	}
	else if( key == "stroke-width" ) {
		if( value != "inherit" ) {
			mSpecifiesStrokeWidth = true;
			mStrokeWidth = float( strtod( value.c_str(), nullptr ) );
		}
		return true;
	}
	else if( key == "fill-rule" ) {
		if( value == "evenodd" ) {
			mSpecifiesFillRule = true;
			mFillRule = FILL_RULE_EVENODD;
		}
		else if( value == "nonzero" ) {
			mSpecifiesFillRule = true;
			mFillRule = FILL_RULE_NONZERO;
		}
		return true;
	}
	else if( key == "stroke-linecap" ) {
		if( value == "butt" ) {
			mSpecifiesLineCap = true;
			mLineCap = LINE_CAP_BUTT;
		}
		else if( value == "round" ) {
			mSpecifiesLineCap = true;
			mLineCap = LINE_CAP_ROUND;
		}
		else if( value == "square" ) {
			mSpecifiesLineCap = true;
			mLineCap = LINE_CAP_SQUARE;
		}
		return true;
	}
	else if( key == "stroke-linejoin" ) {
		if( value == "miter" ) {
			mSpecifiesLineJoin = true;
			mLineJoin = LINE_JOIN_MITER;
		}
		else if( value == "round" ) {
			mSpecifiesLineJoin = true;
			mLineJoin = LINE_JOIN_ROUND;
		}
		else if( value == "bevel" ) {
			mSpecifiesLineJoin = true;
			mLineJoin = LINE_JOIN_BEVEL;
		}
		return true;
	}
	else if( key == "stroke-miterlimit" ) {
		if( value != "inherit" ) {
			mSpecifiesMiterLimit = true;
			mMiterLimit = float( atof( value.c_str() ) ); // should be >= 1, otherwise error
		}
		return true;
	}
	else if( key == "stroke-dasharray" ) {
		if( value != "inherit" ) {
			mSpecifiesDashArray = true;
			mDashArray.clear();
			if( !( value == "none" || value.empty() ) ) {
				const auto values = readValueList( value, false );
				for( const auto &val : values )
					mDashArray.push_back( val.asUser() );
			}
		}
		return true;
	}
	else if( key == "stroke-dashoffset" ) {
		if( value != "inherit" ) {
			if( !( value == "none" || value.empty() ) ) {
				mSpecifiesDashOffset = true;
				mDashOffset = Value::parse( value ).asUser();
			}
		}
		return true;
	}
	else if( key == "clip-path" ) {
		if( value != "inherit" ) {
			if( !strncmp( value.c_str(), "url", 3 ) ) {
				char        id[1024];
				const char *hash = strchr( value.c_str(), '#' );
				const char *closeParen = strchr( value.c_str(), ')' );
				if( ( !closeParen ) || ( !hash ) || ( closeParen - hash >= 1024 ) )
					return false;
				strncpy( id, hash + 1, closeParen - hash - 1 );
				id[closeParen - hash - 1] = 0;
				mSpecifiesClipPath = true;
				mClipPathId = id;
				return true;
			}
			return false;
		}
		return true;
	}
	else if( key == "stop-color" ) {
		if( value != "inherit" )
			mStopColor = Paint::parse( value.c_str(), &mSpecifiesStopColor, nullptr ).getColor();
		return true;
	}
	else if( key == "stop-opacity" ) {
		if( value != "inherit" ) {
			mStopOpacity = readValue( value, 0, 1 ).asUser();
			mSpecifiesStopOpacity = true;
		}
		return true;
	}
	else if( key == "font-family" ) {
		mSpecifiesFontFamilies = true;
		setFontFamilies( readStringList( value, true ) );
		return true;
	}
	else if( key == "font-size" ) {
		if( !value.empty() && isdigit( value[0] ) ) { // we don't parse something like font-size:medium
			mSpecifiesFontSize = true;
			setFontSize( readValue( value ) );
		}
		return true;
	}
	else if( key == "font-weight" ) {
		string weightString = trim( value );
		if( isdigit( weightString[0] ) ) {
			int v = strtol( weightString.c_str(), nullptr, 10 );
			if( v > 900 )
				v = 900;
			if( v < 100 )
				v = 100;
			mFontWeight = FontWeight( static_cast<int>( WEIGHT_100 ) + ( ( v / 100 ) - 1 ) );
			mSpecifiesFontWeight = true;
		}
		else if( asciiCaseEqual( weightString, "normal" ) ) {
			mFontWeight = WEIGHT_NORMAL;
			mSpecifiesFontWeight = true;
		}
		else if( asciiCaseEqual( weightString, "bold" ) ) {
			mFontWeight = WEIGHT_BOLD;
			mSpecifiesFontWeight = true;
		}
		return true;
	}
	else if( key == "text-anchor" ) {
		mSpecifiesTextAnchor = true;
		if( asciiCaseEqual( value, "start" ) )
			mTextAnchor = TEXT_ANCHOR_START;
		if( asciiCaseEqual( value, "middle" ) )
			mTextAnchor = TEXT_ANCHOR_MIDDLE;
		else if( asciiCaseEqual( value, "end" ) )
			mTextAnchor = TEXT_ANCHOR_END;
		else
			mSpecifiesTextAnchor = false;
		return true;
	}
	else if( key == "display" ) {
		// we can't handle most of the possibilities yet; only 'none'
		if( value == "none" )
			mDisplayNone = true;
		else
			mDisplayNone = false;
		return true;
	}
	else if( key == "visibility" ) {
		if( value != "inherit" ) {
			mSpecifiesVisible = true;
			if( value == "hidden" || value == "collapse" )
				mVisible = false;
			else
				mVisible = true;
		}
		return true;
	}
	else
		return false;
}

void Style::resolve( const Node *node ) const
{
	if( mSpecifiesFill && mFill.needsResolve() ) {
		mFill = node->findPaintInAncestors( mFill.getId() );
	}
	if( mSpecifiesStroke && mStroke.needsResolve() ) {
		mStroke = node->findPaintInAncestors( mStroke.getId() );
	}
}

bool Style::operator==( const Style &other ) const
{
	if( mSpecifiesOpacity && !approxEqual( mOpacity, other.mOpacity ) )
		return false;
	if( mSpecifiesFillOpacity && !approxEqual( mFillOpacity, other.mFillOpacity ) )
		return false;
	if( mSpecifiesStrokeOpacity && !approxEqual( mStrokeOpacity, other.mStrokeOpacity ) )
		return false;
	if( mSpecifiesFill && mFill != other.mFill )
		return false;
	if( mSpecifiesStroke && mStroke != other.mStroke )
		return false;
	if( mSpecifiesStrokeWidth && !approxEqual( mStrokeWidth, other.mStrokeWidth ) )
		return false;
	if( mSpecifiesFillRule && mFillRule != other.mFillRule )
		return false;
	if( mSpecifiesLineCap && mLineCap != other.mLineCap )
		return false;
	if( mSpecifiesLineJoin && mLineJoin != other.mLineJoin )
		return false;
	if( mSpecifiesMiterLimit && !approxEqual( mMiterLimit, other.mMiterLimit ) )
		return false;
	if( mSpecifiesDashArray && mDashArray != other.mDashArray )
		return false;
	if( mSpecifiesDashOffset && !approxEqual( mDashOffset, other.mDashOffset ) )
		return false;
	if( mSpecifiesClipPath && mClipPathId != other.mClipPathId )
		return false;
	return true;
}

void Style::operator+=( const Style &other )
{
	if( other.mSpecifiesOpacity )
		setOpacity( other.mOpacity );
	if( other.mSpecifiesFillOpacity )
		setFillOpacity( other.mFillOpacity );
	if( other.mSpecifiesStrokeOpacity )
		setStrokeOpacity( other.mStrokeOpacity );
	if( other.mSpecifiesFill )
		setFill( other.mFill );
	if( other.mSpecifiesStroke )
		setStroke( other.mStroke );
	if( other.mSpecifiesStrokeWidth )
		setStrokeWidth( other.mStrokeWidth );
	if( other.mSpecifiesFillRule )
		setFillRule( other.mFillRule );
	if( other.mSpecifiesLineCap )
		setLineCap( other.mLineCap );
	if( other.mSpecifiesLineJoin )
		setLineJoin( other.mLineJoin );
	if( other.mSpecifiesMiterLimit )
		setMiterLimit( other.mMiterLimit );
	if( other.mSpecifiesDashArray )
		setDashArray( other.mDashArray );
	if( other.mSpecifiesDashOffset )
		setDashOffset( other.mDashOffset );
	if( other.mSpecifiesClipPath )
		setClipPath( other.mClipPathId );
	mClipPath = other.mClipPath;
}

Style Style::operator+( const Style &other ) const
{
	Style result( *this );
	result += other;
	return result;
}

void Style::startRender( Renderer &renderer, const Node *node ) const
{
	if( mSpecifiesFill )
		renderer.pushFill( mFill );
	if( mSpecifiesStroke )
		renderer.pushStroke( mStroke );
	if( mSpecifiesOpacity ) {
		// if this node draws, we'll force both fill opacity and stroke opacity to be 'opacity'
		if( node->isDrawable() ) {
			renderer.pushFillOpacity( mOpacity );
			renderer.pushStrokeOpacity( mOpacity );
		}
	}
	else {
		if( mSpecifiesFillOpacity )
			renderer.pushFillOpacity( mFillOpacity );
		if( mSpecifiesStrokeOpacity )
			renderer.pushStrokeOpacity( mStrokeOpacity );
	}
	if( mSpecifiesStrokeWidth )
		renderer.pushStrokeWidth( mStrokeWidth );
	if( mSpecifiesFillRule )
		renderer.pushFillRule( mFillRule );
	if( mSpecifiesLineCap )
		renderer.pushLineCap( mLineCap );
	if( mSpecifiesLineJoin )
		renderer.pushLineJoin( mLineJoin );
	if( mSpecifiesMiterLimit )
		renderer.pushMiterLimit( mMiterLimit );
	if( mSpecifiesDashArray )
		renderer.pushDashArray( mDashArray );
	if( mSpecifiesDashOffset )
		renderer.pushDashOffset( mDashOffset );
	if( mSpecifiesClipPath ) {
		if( !mClipPath )
			mClipPath = node->getClipPath( *this );
		if( mClipPath->useObjectBoundingBox() ) {
			// Calculate object space transform matrix.
			const auto bounds = node->getBoundingBox();
			const auto transform = glm::scale( glm::translate( mat3(), bounds.getUpperLeft() ), bounds.getSize() );

			// Apply matrix prior to rendering the clip path and restore afterwards.
			renderer.pushMatrix( transform );
			renderer.pushClipPath( *mClipPath );
			renderer.popMatrix();
		}
		else
			renderer.pushClipPath( *mClipPath );
	}
}

void Style::finishRender( Renderer &renderer, const Node *node ) const
{
	if( mSpecifiesClipPath )
		renderer.popClipPath();
	if( mSpecifiesDashOffset )
		renderer.popDashOffset();
	if( mSpecifiesDashArray )
		renderer.popDashArray();
	if( mSpecifiesMiterLimit )
		renderer.popMiterLimit();
	if( mSpecifiesLineJoin )
		renderer.popLineJoin();
	if( mSpecifiesLineCap )
		renderer.popLineCap();
	if( mSpecifiesFillRule )
		renderer.popFillRule();
	if( mSpecifiesStrokeWidth )
		renderer.popStrokeWidth();
	if( ( mSpecifiesOpacity && node->isDrawable() ) || ( ( !mSpecifiesOpacity ) && mSpecifiesStrokeOpacity ) )
		renderer.popStrokeOpacity();
	if( ( mSpecifiesOpacity && node->isDrawable() ) || ( ( !mSpecifiesOpacity ) && mSpecifiesFillOpacity ) )
		renderer.popFillOpacity();
	if( mSpecifiesStroke )
		renderer.popStroke();
	if( mSpecifiesFill )
		renderer.popFill();
}

////////////////////////////////////////////////////////////////////////////////////
// Value
float Value::asUser( float percentOf, float dpi, float fontSize, float fontXHeight ) const
{
	switch( mUnit ) {
	case USER:
	case PX:
		return mValue;
	case PERCENT:
		return mValue * percentOf / 100;
	case PT:
		return mValue * ( dpi / 72 );
	case PC:
		return mValue * 12 * ( dpi / 72 ); // there are 12pts in a pica
	case MM:
		return mValue * dpi / 25.4f; // 25.4mm in an inch
	case CM:
		return mValue * dpi / 2.54f; // 2.54cm in an inch
	case INCH:
		return mValue * dpi;
	case EM:
		return mValue * fontSize;
	case EX:
		return mValue * fontXHeight;
	}

	return mValue;
}

float Value::asUser( const Doc *doc, const Style &style ) const
{
	return asUser( 100, doc->getDpi(), style.getFontSize().asUser(), style.getFontSize().asUser() * 7.0f / 12.0f );
}

float Value::asUserWidth( const Doc *doc, const Style &style ) const
{
	return asUser( doc->getWidth(), doc->getDpi(), style.getFontSize().asUser(), style.getFontSize().asUser() * 7.0f / 12.0f );
}

float Value::asUserHeight( const Doc *doc, const Style &style ) const
{
	return asUser( doc->getHeight(), doc->getDpi(), style.getFontSize().asUser(), style.getFontSize().asUser() * 7.0f / 12.0f );
}

// Reads the suffix and converts it to user units based on dpi
Value Value::parse( const char **sInOut )
{
	float v = parseFloat( sInOut );
	if( strncmp( *sInOut, "px", 2 ) == 0 ) {
		*sInOut += 2;
		return { v, PX };
	}
	else if( **sInOut == '%' ) {
		*sInOut += 1;
		return { v, PERCENT };
	}
	else if( strncmp( *sInOut, "pt", 2 ) == 0 ) {
		*sInOut += 2;
		return { v, PT };
	}
	else if( strncmp( *sInOut, "pc", 2 ) == 0 ) { // picas
		*sInOut += 2;
		return { v, PC };
	}
	else if( strncmp( *sInOut, "mm", 2 ) == 0 ) {
		*sInOut += 2;
		return { v, MM };
	}
	else if( strncmp( *sInOut, "cm", 2 ) == 0 ) {
		*sInOut += 2;
		return { v, CM };
	}
	else if( strncmp( *sInOut, "in", 2 ) == 0 ) {
		*sInOut += 2;
		return { v, INCH };
	}
	else if( strncmp( *sInOut, "em", 2 ) == 0 ) {
		*sInOut += 2;
		return { v, EM };
	}
	else if( strncmp( *sInOut, "ex", 2 ) == 0 ) {
		*sInOut += 2;
		return { v, EX };
	}
	else if( strncmp( *sInOut, "auto", 4 ) == 0 ) {
		*sInOut += 4;
		return { 0, AUTO };
	}
	else
		return { v, USER };
}

Value Value::parse( const std::string &s )
{
	const char *temp = s.c_str();
	return parse( &temp );
}

////////////////////////////////////////////////////////////////////////////////////
// Node
Node::Node( Node *parent, const XmlTree &xml )
	: mParent( parent )
	, mBoundingBoxCached( false )
	, mStyle( xml, this )
{
	mUuid = nextUuid();
	mSpecifiesTransform = false;
	mTag = xml.getTag();
	mId = xml["id"];
	if( xml.hasAttribute( "transform" ) ) {
		mSpecifiesTransform = true;
		mTransform = parseTransform( xml["transform"] );
	}
	else
		mTransform = mat3();
}

const Doc *Node::getDoc() const
{
	const Node *parent = mParent;
	while( parent ) {
		const Doc *doc = dynamic_cast<const Doc *>( parent );
		if( doc )
			return doc;

		parent = parent->mParent;
	}

	return nullptr;
}

string Node::getDomPath() const
{
	string      result = mId;
	const Node *parent = this;
	while( parent && parent->mParent ) {
		parent = parent->mParent;
		result = parent->getId();
		result += string( "/" ) + result;
	}

	return result;
}

const ColorA8u &Node::getColor() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesColor() )
		return style.getColor();
	else if( mParent )
		return mParent->getColor();
	else
		return Style::getColorDefault();
}

const Paint &Node::getFill() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesFill() )
		return style.getFill();
	else if( mParent )
		return mParent->getFill();
	else
		return Style::getFillDefault();
}

const Paint &Node::getStroke() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesStroke() )
		return style.getStroke();
	else if( mParent )
		return mParent->getStroke();
	else
		return Style::getStrokeDefault();
}

float Node::getStrokeWidth() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesStrokeWidth() )
		return style.getStrokeWidth();
	else if( mParent )
		return mParent->getStrokeWidth();
	else
		return Style::getStrokeWidthDefault();
}

float Node::getOpacity() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesOpacity() )
		return style.getOpacity();
	else if( mParent )
		return mParent->getOpacity();
	else
		return Style::getOpacityDefault();
}

float Node::getFillOpacity() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesFillOpacity() )
		return style.getFillOpacity();
	else if( mParent )
		return mParent->getFillOpacity();
	else
		return Style::getFillOpacityDefault();
}

float Node::getStrokeOpacity() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesStrokeOpacity() )
		return style.getStrokeOpacity();
	else if( mParent )
		return mParent->getStrokeOpacity();
	else
		return Style::getStrokeOpacityDefault();
}

FillRule Node::getFillRule() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesFillRule() )
		return style.getFillRule();
	else if( mParent )
		return mParent->getFillRule();
	else
		return Style::getFillRuleDefault();
}

LineCap Node::getLineCap() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesLineCap() )
		return style.getLineCap();
	else if( mParent )
		return mParent->getLineCap();
	else
		return Style::getLineCapDefault();
}

LineJoin Node::getLineJoin() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesLineJoin() )
		return style.getLineJoin();
	else if( mParent )
		return mParent->getLineJoin();
	else
		return Style::getLineJoinDefault();
}

float Node::getMiterLimit() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesMiterLimit() )
		return style.getMiterLimit();
	else if( mParent )
		return mParent->getMiterLimit();
	else
		return Style::getMiterLimitDefault();
}

const std::vector<float> &Node::getDashArray() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesDashArray() )
		return style.getDashArray();
	else if( mParent )
		return mParent->getDashArray();
	else
		return Style::getDashArrayDefault();
}

float Node::getDashOffset() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesDashOffset() )
		return style.getDashOffset();
	else if( mParent )
		return mParent->getDashOffset();
	else
		return Style::getDashOffsetDefault();
}

ColorA8u Node::getStopColor() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesStopColor() )
		return style.getStopColor();
	else if( mParent )
		return mParent->getStopColor();
	else
		return Style::getStopColorDefault();
}

float Node::getStopOpacity() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesStopOpacity() )
		return style.getStopOpacity();
	else if( mParent )
		return mParent->getStopOpacity();
	else
		return Style::getStopOpacityDefault();
}

const vector<string> &Node::getFontFamilies() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesFontFamilies() )
		return style.getFontFamilies();
	else if( mParent )
		return mParent->getFontFamilies();
	else
		return Style::getFontFamiliesDefault();
}

Value Node::getFontSize() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesFontSize() )
		return style.getFontSize();
	else if( mParent )
		return mParent->getFontSize();
	else
		return Style::getFontSizeDefault();
}

TextAnchor Node::getTextAnchor() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesTextAnchor() )
		return style.getTextAnchor();
	else if( mParent )
		return mParent->getTextAnchor();
	else
		return Style::getTextAnchorDefault();
}

bool Node::isVisible() const
{
	const auto &style = getStyle(); // Resolves style if needed.
	if( style.specifiesVisible() )
		return style.isVisible();
	else if( mParent )
		return mParent->isVisible();
	else
		return true;
}

Paint Node::parsePaint( const char *value, bool *specified, const Node *parentNode )
{
	*specified = false;
	while( *value && isspace( *value ) )
		value++;

	if( !*value )
		return {};

	if( !strncmp( value, "inherit", 7 ) ) {
		*specified = false;
		return {};
	}

	if( !strncmp( value, "currentColor", 12 ) ) {
		*specified = true;
		return { parentNode->getColor() };
	}

	if( value[0] == '#' ) { // hex color
		uint32_t v = 0;
		if( strlen( value ) > 4 ) {
			for( int c = 0; c < 6; ++c ) {
				char     ch = charToUpper( value[1 + c] );
				uint32_t col = ch - ( ( ch > '9' ) ? ( 'A' - 10 ) : '0' );
				v += col << ( ( 5 - c ) * 4 );
			}
		}
		else { // 3-digit hex shorthand; double each digit
			for( int c = 0; c < 3; ++c ) {
				char     ch = charToUpper( value[1 + c] );
				uint32_t col = ch - ( ( ch > '9' ) ? ( 'A' - 10 ) : '0' );
				v += col << ( ( 5 - ( c * 2 + 0 ) ) * 4 );
				v += col << ( ( 5 - ( c * 2 + 1 ) ) * 4 );
			}
		}
		*specified = true;
		return { ColorA8u( char( v >> 16 ), char( v >> 8 ) & 255, char( v ) & 255, 255 ) };
	}
	else if( !strncmp( value, "none", 4 ) ) {
		*specified = true;
		return {};
	}
	else if( !strncmp( value, "rgb", 3 ) ) {
		vector<Value> values = readValueList( value + 3 );
		if( values.size() == 3 ) {
			*specified = true;
			return { ColorA8u( uint8_t( values[0].asUser( 255 ) ), uint8_t( values[1].asUser( 255 ) ), uint8_t( values[2].asUser( 255 ) ), 255 ) };
		}
		*specified = false;
		return {};
	}
	else if( !strncmp( value, "url", 3 ) ) {
		char        id[1024];
		const char *hash = strchr( value, '#' );
		const char *closeParen = strchr( value, ')' );
		if( ( !closeParen ) || ( !hash ) || ( closeParen - hash >= 1024 ) )
			return {};
		strncpy( id, hash + 1, closeParen - hash - 1 );
		id[closeParen - hash - 1] = 0;
		*specified = true;
		return parentNode->findPaintInAncestors( id );
	}
	else { // try to find color amongst named colors
		Color8u result = svgNameToRgb( value, specified );
		if( specified )
			return { result };
		else
			return {};
	}
}

mat3 Node::parseTransform( const std::string &value )
{
	const char *c = value.c_str();
	mat3        curMat;
	mat3        nextMat;
	while( parseTransformComponent( &c, &nextMat ) ) {
		curMat = curMat * nextMat;
	}
	return curMat;
}

bool Node::parseTransformComponent( const char **c, mat3 *result )
{
	// skip leading whitespace
	while( **c && ( isspace( **c ) || ( **c == ',' ) ) )
		( *c )++;

	mat3 m;
	if( !strncmp( *c, "scale", 5 ) ) {
		*c += 5; // strlen( "scale" );
		vector<float> v = parseFloatList( c );
		if( v.size() == 1 ) {
			m = scale( mat3(), vec2( v[0] ) );
		}
		else if( v.size() == 2 ) {
			m = scale( mat3(), vec2( v[0], v[1] ) );
		}
		else
			throw TransformParseExc();
	}
	else if( !strncmp( *c, "translate", 9 ) ) {
		*c += 9; // strlen( "translate" );
		vector<float> v = parseFloatList( c );
		if( v.size() == 1 )
			m = translate( mat3(), vec2( v[0], 0 ) );
		else if( v.size() == 2 ) {
			m = translate( mat3(), vec2( v[0], v[1] ) );
		}
		else
			throw TransformParseExc();
	}
	else if( !strncmp( *c, "rotate", 6 ) ) {
		*c += 6; // strlen( "rotate" );
		vector<float> v = parseFloatList( c );
		if( v.size() == 1 ) {
			float a = toRadians( v[0] );
			m = rotate( mat3(), a );
		}
		else if( v.size() == 3 ) { // rotate around point
			float a = toRadians( v[0] );
			vec2  origin( v[1], v[2] );
			m = translate( mat3(), origin );
			m = rotate( m, a );
			m = translate( m, -origin );
		}
		else
			throw TransformParseExc();
	}
	else if( !strncmp( *c, "matrix", 6 ) ) {
		*c += 6; // strlen( "matrix" );
		vector<float> v = parseFloatList( c );
		if( v.size() == 6 )
			m = mat3( v[0], v[1], 0, v[2], v[3], 0, v[4], v[5], 1 );
		else
			throw TransformParseExc();
	}
	else if( !strncmp( *c, "skewX", 5 ) ) {
		*c += 5; // strlen( "skewX" );
		vector<float> v = parseFloatList( c );
		if( v.size() == 1 ) {
			float a = toRadians( v[0] );
			m = shearY( mat3(), tan( a ) );
		}
		else
			throw TransformParseExc();
	}
	else if( !strncmp( *c, "skewY", 5 ) ) {
		*c += 5; // strlen( "skewY" );
		vector<float> v = parseFloatList( c );
		if( v.size() == 1 ) {
			float a = toRadians( v[0] );
			m = shearX( mat3(), tan( a ) );
		}
		else
			throw TransformParseExc();
	}
	else
		return false;

	*result = m;
	return true;
}

// Parse a 'style' attribute searching for the key 'key', and returning its corresponding value or the empty string if not found
std::string Node::findStyleValue( const std::string &styleString, const std::string &key )
{
	vector<string> valuePairs = split( styleString, ';' );
	for( const auto &pair : valuePairs ) {
		vector<string> valuePair = split( pair, ':' );
		if( valuePair.size() != 2 )
			continue;
		if( valuePair[0] == key )
			return valuePair[1];
	}
	return {};
}

void Node::parseStyle( const XmlTree &xml )
{
	mStyle = Style( xml, this );
}

Style Node::calcInheritedStyle() const
{
	Style result;
	result.setFill( getFill() );
	result.setStroke( getStroke() );
	result.setOpacity( getOpacity() );
	result.setFillOpacity( getFillOpacity() );
	result.setStrokeOpacity( getStrokeOpacity() );
	result.setFillRule( getFillRule() );
	result.setLineCap( getLineCap() );
	result.setLineJoin( getLineJoin() );
	result.setMiterLimit( getMiterLimit() );
	result.setDashArray( getDashArray() );
	result.setDashOffset( getDashOffset() );
	result.setStrokeWidth( getStrokeWidth() );
	result.setStopColor( getStopColor() );
	result.setStopOpacity( getStopOpacity() );
	result.setFontFamilies( getFontFamilies() );
	result.setFontSize( getFontSize() );
	return result;
}

const ClipPath *Node::getClipPath() const
{
	if( getStyle().specifiesClipPath() )
		return dynamic_cast<const ClipPath *>( findInAncestors( getStyle().getClipPath() ) );

	return nullptr;
}

const ClipPath *Node::getClipPath( const Style &style ) const
{
	if( style.specifiesClipPath() )
		return dynamic_cast<const ClipPath *>( findInAncestors( style.getClipPath() ) );

	return nullptr;
}

void Node::render( Renderer &renderer ) const
{
	renderer.start();

	Style style = calcInheritedStyle();
	if( mParent )
		renderer.pushMatrix( mParent->getTransformAbsolute() );

	startRender( renderer, style );
	renderSelf( renderer );
	finishRender( renderer, style );

	renderer.finish();
}

void Node::firstStartRender( Renderer &renderer ) const
{
	renderer.pushFill( getFill() );
}

void Node::startRender( Renderer &renderer, const Style &style ) const
{
	if( mSpecifiesTransform )
		renderer.pushMatrix( mTransform );
	renderer.pushStyle( style );
	style.startRender( renderer, this );
}

void Node::finishRender( Renderer &renderer, const Style &style ) const
{
	style.finishRender( renderer, this );
	renderer.popStyle();
	if( mSpecifiesTransform )
		renderer.popMatrix();
}

const Node *Node::findInAncestors( const std::string &elementId ) const
{
	if( mId == elementId )
		return this;
	else if( mParent )
		return mParent->findInAncestors( elementId );
	else
		return nullptr;
}

Paint Node::findPaintInAncestors( const std::string &paintName ) const
{
	const Node *node = findInAncestors( paintName );
	if( !node )
		return { paintName }; // Needs to be resolved later.

	if( typeid( LinearGradient ) == typeid( *node ) ) {
		const auto *linearGradient = static_cast<const LinearGradient *>( node );
		return linearGradient->asPaint();
	}
	else if( typeid( RadialGradient ) == typeid( *node ) ) {
		const auto *radialGradient = static_cast<const RadialGradient *>( node );
		return radialGradient->asPaint();
	}
	else
		return {};
}

const Node *Node::findTagInAncestors( const std::string &elementTag ) const
{
	if( mTag == elementTag )
		return this;
	else if( mParent )
		return mParent->findTagInAncestors( elementTag );
	else
		return nullptr;
}

mat3 Node::getTransformAbsolute() const
{
	mat3 result;
	if( mSpecifiesTransform )
		result = mTransform;
	else
		result = mat3();

	const Node *parent = mParent;
	while( parent ) {
		if( parent->specifiesTransform() )
			result = parent->getTransform() * result;
		parent = parent->getParent();
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Gradient
Gradient::Gradient( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	parse( parent, xml );
}

SpreadMethod Gradient::parseSpreadMethod( const std::string &s )
{
	auto m = trim( toLower( s ) );
	if( m == "reflect" )
		return SPREAD_METHOD_REFLECT;
	if( m == "repeat" )
		return SPREAD_METHOD_REPEAT;

	return SPREAD_METHOD_PAD;
}

void Gradient::parse( const Node *parent, const XmlTree &xml )
{
	std::string ref;
	if( xml.hasAttribute( "xlink:href" ) )
		ref = xml.getAttributeValue<string>( "xlink:href" );
	else if( xml.hasAttribute( "href" ) )
		ref = xml.getAttributeValue<string>( "href" );

	if( ref.size() > 1 ) {
		if( ref[0] == '#' ) {
			string      elementId = ref.substr( 1, string::npos );
			const auto *referencedGrad = dynamic_cast<const Gradient *>( findInAncestors( elementId ) );
			if( referencedGrad ) {
				copyAttributesFrom( *referencedGrad );
			}
		}
	}

	for( XmlTree::ConstIter stopsIt = xml.begin( "stop" ); stopsIt != xml.end(); ++stopsIt ) {
		mStops.emplace_back( this, *stopsIt );
	}
	if( xml.hasAttribute( "gradientUnits" ) )
		mUseObjectBoundingBox = xml.getAttributeValue<string>( "gradientUnits" ) != string( "userSpaceOnUse" );
	if( xml.hasAttribute( "gradientTransform" ) ) {
		mSpecifiesTransform = true;
		mTransform = parseTransform( xml.getAttributeValue<string>( "gradientTransform" ) );
	}
	if( xml.hasAttribute( "spreadMethod" ) ) {
		mSpecifiesSpreadMethod = true;
		mSpreadMethod = parseSpreadMethod( xml.getAttributeValue<string>( "spreadMethod" ) );
	}

	// See: https://svgwg.org/svg2-draft/pservers.html#GradientStops
	// "Each gradient offset value is required to be equal to or greater than the previous gradient stop's offset value.
	//  If a given gradient stop's offset value is not equal to or greater than all previous offset values, then the offset
	//  value is adjusted to be equal to the largest of all previous offset values."
	if( !mStops.empty() ) {
		float offset = mStops.front().offset;
		for( auto &stop : mStops ) {
			offset = stop.offset = glm::max( offset, stop.offset );
		}
	}
}

void Gradient::copyAttributesFrom( const Gradient &rhs )
{
	mStops = rhs.mStops;
	mUseObjectBoundingBox = rhs.mUseObjectBoundingBox;
	if( rhs.mSpecifiesTransform ) {
		mSpecifiesTransform = true;
		mTransform = rhs.mTransform;
	}
	if( rhs.mSpecifiesSpreadMethod ) {
		mSpecifiesSpreadMethod = true;
		mSpreadMethod = rhs.mSpreadMethod;
	}
}

Gradient::Stop::Stop( const Node *parent, const XmlTree &xml )
{
	if( xml.hasAttribute( "offset" ) )
		offset = Value::parse( xml.getAttributeValue<string>( "offset" ) ).asUser( 1 ); // Percentages will be converted to decimals, where 100% = 1.0f
	if( xml.hasAttribute( "stop-color" ) )
		color = parsePaint( xml.getAttributeValue<string>( "stop-color" ).c_str(), &specifiesColor, parent ).getColor();
	if( xml.hasAttribute( "stop-opacity" ) ) {
		const auto value = xml.getAttributeValue<string>( "stop-opacity" );
		if( value == "inherit" ) {
			specifiesOpacity = false;
		}
		else {
			specifiesOpacity = true;
			color.a = uint8_t( Value::parse( value ).asUser() * 255 );
		}
	}

	if( xml.hasAttribute( "style" ) ) {
		string stopColorString = findStyleValue( xml.getAttributeValue<string>( "style" ), "stop-color" );
		if( !stopColorString.empty() )
			color = parsePaint( stopColorString.c_str(), &specifiesColor, parent ).getColor();
		string stopOpacityString = findStyleValue( xml.getAttributeValue<string>( "style" ), "stop-opacity" );
		if( !stopOpacityString.empty() ) {
			color.a = uint8_t( Value::parse( stopOpacityString ).asUser() * 255 );
		}
	}

	// See: https://svgwg.org/svg2-draft/pservers.html#GradientStops
	// "Gradient offset values less than 0 (or less than 0%) are rounded up to 0%.
	//  Gradient offset values greater than 1 (or greater than 100%) are rounded down to 100%."
	offset = clamp( offset, 0.0f, 1.0f );
}

Paint Gradient::asPaint() const
{
	Paint result;
	result.mId = getId();

	if( getStyle().isDisplayNone() || !getStyle().isVisible() )
		result.mType = Paint::NONE;
	else {
		result.mStops.clear();
		for( const auto &stop : mStops ) {
			if( stop.specifiesColor )
				result.mStops.emplace_back( stop.offset, stop.color );
			else
				result.mStops.emplace_back( stop.offset, getStopColor() );

			// See: https://svgwg.org/svg2-draft/pservers.html#GradientStops
			// "The opacity value used for the gradient calculation is the product of the value of stop-opacity and the opacity of the value of stop-color."
			auto &opacity = result.mStops.back().color.a;
			if( stop.specifiesOpacity )
				opacity = ( opacity * uint8_t( stop.opacity * 255.0f ) ) / 255;
			else
				opacity = ( opacity * uint8_t( getStopOpacity() * 255.0f ) ) / 255;
		}

		result.mUseObjectBoundingBox = mUseObjectBoundingBox;

		if( mSpecifiesTransform ) {
			result.mSpecifiesTransform = true;
			result.mTransform = mTransform;
		}
		if( mSpecifiesSpreadMethod ) {
			result.mSpecifiesSpreadMethod = true;
			result.mSpreadMethod = mSpreadMethod;
		}
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// LinearGradient
LinearGradient::LinearGradient( Node *parent, const XmlTree &xml )
	: Gradient( parent, xml )
{
	parse( xml );
}

void LinearGradient::parse( const XmlTree &xml )
{
	std::string ref;
	if( xml.hasAttribute( "xlink:href" ) )
		ref = xml.getAttributeValue<string>( "xlink:href" );
	else if( xml.hasAttribute( "href" ) )
		ref = xml.getAttributeValue<string>( "href" );

	if( ref.size() > 1 ) {
		if( ref[0] == '#' ) {
			string      elementId = ref.substr( 1, string::npos );
			const auto *referencedGrad = dynamic_cast<const LinearGradient *>( findInAncestors( elementId ) );
			if( referencedGrad ) {
				copyAttributesFrom( *referencedGrad );
			}
		}
	}

	if( xml.hasAttribute( "x1" ) )
		mX1 = Value::parse( xml.getAttributeValue<string>( "x1" ) );
	if( xml.hasAttribute( "y1" ) )
		mY1 = Value::parse( xml.getAttributeValue<string>( "y1" ) );
	if( xml.hasAttribute( "x2" ) )
		mX2 = Value::parse( xml.getAttributeValue<string>( "x2" ) );
	if( xml.hasAttribute( "y2" ) )
		mY2 = Value::parse( xml.getAttributeValue<string>( "y2" ) );
}

void LinearGradient::copyAttributesFrom( const LinearGradient &rhs )
{
	mX1 = rhs.mX1;
	mY1 = rhs.mY1;
	mX2 = rhs.mX2;
	mY2 = rhs.mY2;
}

Paint LinearGradient::asPaint() const
{
	Paint result = Gradient::asPaint();
	// See: https://svgwg.org/svg2-draft/pservers.html#GradientStops
	// "If no stops are defined, then painting shall occur as if 'none' were specified as the paint style."
	if( result.mStops.empty() )
		result.mType = Paint::NONE;
	else {
		result.mType = Paint::LINEAR_GRADIENT;
		result.mCoords0.x = mX1.asUser( 1 ); // Percentages will be converted to decimals, where 100% = 1.0f
		result.mCoords0.y = mY1.asUser( 1 );
		result.mCoords1.x = mX2.asUser( 1 );
		result.mCoords1.y = mY2.asUser( 1 );
	}
	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// RadialGradient
RadialGradient::RadialGradient( Node *parent, const XmlTree &xml )
	: Gradient( parent, xml )
{
	parse( xml );
}

void RadialGradient::parse( const XmlTree &xml )
{
	std::string ref;
	if( xml.hasAttribute( "xlink:href" ) )
		ref = xml.getAttributeValue<string>( "xlink:href" );
	else if( xml.hasAttribute( "href" ) )
		ref = xml.getAttributeValue<string>( "href" );

	if( ref.size() > 1 ) {
		if( ref[0] == '#' ) {
			string      elementId = ref.substr( 1, string::npos );
			const auto *referencedGrad = dynamic_cast<const RadialGradient *>( findInAncestors( elementId ) );
			if( referencedGrad ) {
				copyAttributesFrom( *referencedGrad );
			}
		}
	}

	if( xml.hasAttribute( "cx" ) )
		mCx = Value::parse( xml.getAttributeValue<string>( "cx" ) );
	if( xml.hasAttribute( "cy" ) )
		mCy = Value::parse( xml.getAttributeValue<string>( "cy" ) );
	if( xml.hasAttribute( "r" ) )
		mR = Value::parse( xml.getAttributeValue<string>( "r" ) );
	if( xml.hasAttribute( "fx" ) )
		mFx = Value::parse( xml.getAttributeValue<string>( "fx" ) );
	else
		mFx = mCx;
	if( xml.hasAttribute( "fy" ) )
		mFy = Value::parse( xml.getAttributeValue<string>( "fy" ) );
	else
		mFy = mCy;
	if( xml.hasAttribute( "fr" ) )
		mFr = Value::parse( xml.getAttributeValue<string>( "fr" ) );
}

void RadialGradient::copyAttributesFrom( const RadialGradient &rhs )
{
	mCx = rhs.mCx;
	mCy = rhs.mCy;
	mR = rhs.mR;
	mFx = rhs.mFx;
	mFy = rhs.mFy;
	mFr = rhs.mFr;
}

Paint RadialGradient::asPaint() const
{
	Paint result = Gradient::asPaint();
	// See: https://svgwg.org/svg2-draft/pservers.html#GradientStops
	// "If no stops are defined, then painting shall occur as if 'none' were specified as the paint style."
	if( result.mStops.empty() )
		result.mType = Paint::NONE;
	else {
		result.mType = Paint::RADIAL_GRADIENT;
		result.mCoords0.x = mCx.asUser( 1 ); // Percentages will be converted to decimals, where 100% = 1.0f
		result.mCoords0.y = mCy.asUser( 1 );
		result.mCoords1.x = mFx.asUser( 1 );
		result.mCoords1.y = mFy.asUser( 1 );
		result.mRadius0 = mR.asUser( 1 );
		result.mRadius1 = mFr.asUser( 1 );
	}
	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Circle
Circle::Circle( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	const auto doc = getDoc();
	const auto style = calcInheritedStyle();
	if( xml.hasAttribute( "cx" ) )
		mCenter.x = Value::parse( xml.getAttributeValue<string>( "cx" ) ).asUserWidth( doc, style );
	if( xml.hasAttribute( "cy" ) )
		mCenter.y = Value::parse( xml.getAttributeValue<string>( "cy" ) ).asUserHeight( doc, style );

	const auto m = getTransformAbsolute();
	mRadius = Value::parse( xml.getAttributeValue<string>( "r" ) ).asUser( 100 * m[0][0] ); // Use absolute scale.
}

void Circle::renderSelf( Renderer &renderer ) const
{
	if( mRadius > 0 ) // Zero-radius circles should never be drawn.
		renderer.drawCircle( *this );
}

Shape2d Circle::getShape() const
{
	Shape2d result;
	result.arc( mCenter, mRadius, 0, float( M_PI ) * 2 );
	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Ellipse
Ellipse::Ellipse( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	const auto doc = getDoc();
	const auto style = calcInheritedStyle();
	if( xml.hasAttribute( "cx" ) )
		mCenter.x = Value::parse( xml.getAttributeValue<string>( "cx" ) ).asUserWidth( doc, style );
	if( xml.hasAttribute( "cy" ) )
		mCenter.y = Value::parse( xml.getAttributeValue<string>( "cy" ) ).asUserHeight( doc, style );
	mRadiusX = Value::parse( xml.getAttributeValue<string>( "rx" ) ).asUserWidth( doc, style );
	mRadiusY = Value::parse( xml.getAttributeValue<string>( "ry" ) ).asUserHeight( doc, style );
}

void Ellipse::renderSelf( Renderer &renderer ) const
{
	if( mRadiusX > 0 && mRadiusY > 0 ) // Zero-radius ellipses should never be drawn.
		renderer.drawEllipse( *this );
}

bool Ellipse::containsPoint( const vec2 &pt ) const
{
	float x = ( pt.x - mCenter.x ) * ( pt.x - mCenter.x ) / ( mRadiusX * mRadiusX );
	float y = ( pt.y - mCenter.y ) * ( pt.y - mCenter.y ) / ( mRadiusY * mRadiusY );
	return x + y < 1;
}

Shape2d Ellipse::getShape() const
{
	Shape2d result;

	constexpr float magic = 0.552284749830793398402f; // 4/3*(sqrt(2)-1)
	const vec2      offset( mRadiusX * magic, mRadiusY * magic );

	result.moveTo( vec2( mCenter.x + mRadiusX, mCenter.y ) );
	result.curveTo( vec2( mCenter.x + mRadiusX, mCenter.y + offset.y ), vec2( mCenter.x + offset.x, mCenter.y + mRadiusY ), vec2( mCenter.x, mCenter.y + mRadiusY ) );
	result.curveTo( vec2( mCenter.x - offset.x, mCenter.y + mRadiusY ), vec2( mCenter.x - mRadiusX, mCenter.y + offset.y ), vec2( mCenter.x - mRadiusX, mCenter.y ) );
	result.curveTo( vec2( mCenter.x - mRadiusX, mCenter.y - offset.y ), vec2( mCenter.x - offset.x, mCenter.y - mRadiusY ), vec2( mCenter.x, mCenter.y - mRadiusY ) );
	result.curveTo( vec2( mCenter.x + offset.x, mCenter.y - mRadiusY ), vec2( mCenter.x + mRadiusX, mCenter.y - offset.y ), vec2( mCenter.x + mRadiusX, mCenter.y ) );
	result.close();

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
//
void ellipticalArc( Shape2d &path, float x1, float y1, float x2, float y2, float rx, float ry, float xAxisRotation, bool largeArcFlag, bool sweepFlag )
{
	// This is a translation of the  spec section "Elliptical Arc Implementation Notes"
	// http://www.w3.org/TR//implnote.html#ArcImplementationNotes
	float      cosXAxisRotation = cosf( xAxisRotation );
	float      sinXAxisRotation = sinf( xAxisRotation );
	const vec2 cPrime( cosXAxisRotation * ( x2 - x1 ) * 0.5f + sinXAxisRotation * ( y2 - y1 ) * 0.5f, -sinXAxisRotation * ( x2 - x1 ) * 0.5f + cosXAxisRotation * ( y2 - y1 ) * 0.5f );

	// http://www.w3.org/TR//implnote.html#ArcCorrectionOutOfRangeRadii
	float radiiScale = ( cPrime.x * cPrime.x ) / ( rx * rx ) + ( cPrime.y * cPrime.y ) / ( ry * ry );
	if( radiiScale > 1 ) {
		radiiScale = math<float>::sqrt( radiiScale );
		rx *= radiiScale;
		ry *= radiiScale;
	}

	vec2  invRadius( 1.0f / rx, 1.0f / ry );
	vec2  point1 = vec2( cosXAxisRotation * x1 + sinXAxisRotation * y1, -sinXAxisRotation * x1 + cosXAxisRotation * y1 ) * invRadius;
	vec2  point2 = vec2( cosXAxisRotation * x2 + sinXAxisRotation * y2, -sinXAxisRotation * x2 + cosXAxisRotation * y2 ) * invRadius;
	vec2  delta = point2 - point1;
	float d = delta.x * delta.x + delta.y * delta.y;
	if( d <= 0 )
		return;

	float theta1;
	float thetaDelta;
	vec2  center;

	float s = math<float>::sqrt( std::max<float>( 1 / d - 0.25f, 0 ) );
	if( sweepFlag == largeArcFlag )
		s = -s;

	center = vec2( 0.5f * ( point1.x + point2.x ) - delta.y * s, 0.5f * ( point1.y + point2.y ) + delta.x * s );

	theta1 = math<float>::atan2( point1.y - center.y, point1.x - center.x );
	float theta2 = math<float>::atan2( point2.y - center.y, point2.x - center.x );

	thetaDelta = theta2 - theta1;
	if( thetaDelta < 0 && sweepFlag )
		thetaDelta += 2 * float( M_PI );
	else if( thetaDelta > 0 && ( !sweepFlag ) )
		thetaDelta -= 2 * float( M_PI );

	// divide the full arc delta into pi/2 arcs and convert those to cubic beziers
	int segments = int( ceilf( fabsf( thetaDelta / ( float( M_PI ) / 2 ) ) ) + 1 );
	for( int i = 0; i < segments; ++i ) {
		float thetaStart = theta1 + i * thetaDelta / segments;
		float thetaEnd = theta1 + ( i + 1 ) * thetaDelta / segments;
		float t = ( 4 / 3.0f ) * tanf( 0.25f * ( thetaEnd - thetaStart ) );
		float sinThetaStart = math<float>::sin( thetaStart );
		float cosThetaStart = math<float>::cos( thetaStart );
		float sinThetaEnd = math<float>::sin( thetaEnd );
		float cosThetaEnd = math<float>::cos( thetaEnd );

		vec2 startPoint = vec2( cosThetaStart - t * sinThetaStart, sinThetaStart + t * cosThetaStart ) + center;
		startPoint = vec2( cosXAxisRotation * startPoint.x * rx - sinXAxisRotation * startPoint.y * ry, sinXAxisRotation * startPoint.x * rx + cosXAxisRotation * startPoint.y * ry );
		vec2 endPoint = vec2( cosThetaEnd, sinThetaEnd ) + center;
		vec2 transformedEndPoint = vec2( cosXAxisRotation * endPoint.x * rx - sinXAxisRotation * endPoint.y * ry, sinXAxisRotation * endPoint.x * rx + cosXAxisRotation * endPoint.y * ry );
		vec2 midPoint = endPoint + vec2( t * sinThetaEnd, -t * cosThetaEnd );
		midPoint = vec2( cosXAxisRotation * midPoint.x * rx - sinXAxisRotation * midPoint.y * ry, sinXAxisRotation * midPoint.x * rx + cosXAxisRotation * midPoint.y * ry );
		path.curveTo( startPoint, midPoint, transformedEndPoint );
	}
}

static const char *getNextPathItem( const char *s, char it[64] )
{
	int i = 0;
	it[0] = '\0';
	// Skip white spaces and commas
	while( *s && ( isspace( *s ) || *s == ',' ) )
		s++;
	if( !*s )
		return s;
	if( isNumeric( *s ) ) {
		while( *s == '-' || *s == '+' ) {
			if( i < 63 )
				it[i++] = *s;
			s++;
		}
		bool parsingExponent = false;
		while( *s && ( parsingExponent || ( *s != '-' && *s != '+' ) ) && isNumeric( *s ) ) {
			if( i < 63 )
				it[i++] = *s;
			if( *s == 'e' || *s == 'E' )
				parsingExponent = true;
			else
				parsingExponent = false;
			s++;
		}
		it[i] = '\0';
	}
	else {
		it[0] = *s++;
		it[1] = '\0';
		return s;
	}

	return s;
}

char readNextCommand( const char **sInOut )
{
	const char *s = *sInOut;
	while( *s && ( isspace( *s ) || *s == ',' ) )
		s++;
	*sInOut = s + 1;
	return *s;
}

bool readFlag( const char **sInOut )
{
	const char *s = *sInOut;
	while( *s && ( isspace( *s ) || *s == ',' || *s == '-' || *s == '+' ) )
		s++;
	*sInOut = s + 1;
	return *s != '0';
}

bool nextItemIsFloat( const char *s )
{
	while( *s && ( isspace( *s ) || *s == ',' ) )
		s++;
	return isNumeric( *s );
}

Shape2d parsePath( const std::string &p )
{
	const char *s = p.c_str();
	vec2        v0;
	vec2        v1;
	vec2        v2;
	vec2        lastPoint;
	vec2        lastPoint2;

	Shape2d result;
	try {
		bool done = false;
		bool firstCmd = true;
		char prevCmd = '\0';
		while( !done ) {
			char cmd = readNextCommand( &s );
			switch( cmd ) {
			case 'm':
			case 'M':
				v0.x = parseFloat( &s );
				v0.y = parseFloat( &s );
				if( ( !firstCmd ) && ( cmd == 'm' ) )
					v0 += lastPoint;
				result.moveTo( v0 );
				lastPoint2 = lastPoint;
				lastPoint = v0;
				while( nextItemIsFloat( s ) ) {
					v0.x = parseFloat( &s );
					v0.y = parseFloat( &s );
					if( cmd == 'm' )
						v0 += lastPoint;
					result.lineTo( v0 );
					lastPoint2 = lastPoint;
					lastPoint = v0;
				}
				break;
			case 'l':
			case 'L':
				do {
					v0.x = parseFloat( &s );
					v0.y = parseFloat( &s );
					if( cmd == 'l' )
						v0 += lastPoint;
					result.lineTo( v0 );
					lastPoint2 = lastPoint;
					lastPoint = v0;
				} while( nextItemIsFloat( s ) );
				break;
			case 'H':
			case 'h':
				do {
					float x = parseFloat( &s );
					v0 = vec2( ( cmd == 'h' ) ? ( lastPoint.x + x ) : x, lastPoint.y );
					result.lineTo( v0 );
					lastPoint2 = lastPoint;
					lastPoint = v0;
				} while( nextItemIsFloat( s ) );
				break;
			case 'V':
			case 'v':
				do {
					float y = parseFloat( &s );
					v0 = vec2( lastPoint.x, ( cmd == 'v' ) ? ( lastPoint.y + y ) : ( y ) );
					result.lineTo( v0 );
					lastPoint2 = lastPoint;
					lastPoint = v0;
				} while( nextItemIsFloat( s ) );
				break;
			case 'C':
			case 'c':
				do {
					v0.x = parseFloat( &s );
					v0.y = parseFloat( &s );
					v1.x = parseFloat( &s );
					v1.y = parseFloat( &s );
					v2.x = parseFloat( &s );
					v2.y = parseFloat( &s );
					if( cmd == 'c' ) { // relative
						v0 += lastPoint;
						v1 += lastPoint;
						v2 += lastPoint;
					}
					result.curveTo( v0, v1, v2 );
					lastPoint2 = v1;
					lastPoint = v2;
				} while( nextItemIsFloat( s ) );
				break;
			case 'S':
			case 's':
				do {
					if( prevCmd == 's' || prevCmd == 'S' || prevCmd == 'c' || prevCmd == 'C' )
						v0 = lastPoint * 2.0f - lastPoint2;
					else
						v0 = lastPoint;
					prevCmd = cmd; // set this now in case we loop
					v1.x = parseFloat( &s );
					v1.y = parseFloat( &s );
					v2.x = parseFloat( &s );
					v2.y = parseFloat( &s );
					if( cmd == 's' ) { // relative
						v1 += lastPoint;
						v2 += lastPoint;
					}
					result.curveTo( v0, v1, v2 );
					lastPoint2 = v1;
					lastPoint = v2;
				} while( nextItemIsFloat( s ) );
				break;
			case 'Q':
			case 'q':
				do {
					v0.x = parseFloat( &s );
					v0.y = parseFloat( &s );
					v1.x = parseFloat( &s );
					v1.y = parseFloat( &s );
					if( cmd == 'q' ) { // relative
						v0 += lastPoint;
						v1 += lastPoint;
					}
					result.quadTo( v0, v1 );
					lastPoint2 = v0;
					lastPoint = v1;
				} while( nextItemIsFloat( s ) );
				break;
			case 'T':
			case 't':
				do {
					if( prevCmd == 't' || prevCmd == 'T' || prevCmd == 'q' || prevCmd == 'Q' )
						v0 = lastPoint * 2.0f - lastPoint2;
					else
						v0 = lastPoint;
					prevCmd = cmd; // set this now in case we loop
					v1.x = parseFloat( &s );
					v1.y = parseFloat( &s );
					if( cmd == 't' ) { // relative
						v1 += lastPoint;
					}
					result.quadTo( v0, v1 );
					lastPoint2 = v0;
					lastPoint = v1;
				} while( nextItemIsFloat( s ) );
				break;
			case 'a':
			case 'A': {
				do {
					float ra = parseFloat( &s );
					float rb = parseFloat( &s );
					float xAxisRotation = parseFloat( &s ) * float( M_PI ) / 180.0f;
					bool  largeArc = readFlag( &s );
					bool  sweepFlag = readFlag( &s );
					v0.x = parseFloat( &s );
					v0.y = parseFloat( &s );
					if( cmd == 'a' ) { // relative
						v0 += lastPoint;
					}
					ellipticalArc( result, lastPoint.x, lastPoint.y, v0.x, v0.y, ra, rb, xAxisRotation, largeArc, sweepFlag );
					lastPoint2 = lastPoint;
					lastPoint = v0;
				} while( nextItemIsFloat( s ) );
			} break;
			case 'z':
			case 'Z':
				result.close();
				lastPoint2 = lastPoint;
				lastPoint = ( result.empty() || result.getContours().back().empty() ) ? vec2() : result.getContours().back().getPoint( 0 );
				break;
			case '\0':
			default: // technically noise at the end of the string is acceptable according to the spec; see W3C_SVG_11/paths-data-18.svg
				done = true;
				break;
			}
			firstCmd = false;
			prevCmd = cmd;
		}
	}
	catch( ... ) {
	}

	// For consistency, make sure paths are defined in CCW order for filled sections, CW for holes.
	// This is especially important when using instanced rendering.
	bool isClockwise = result.getContour( 0 ).calcClockwise();
	if( isClockwise )
		result.reverse();

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Path
Path::Path( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	auto p = xml.getAttributeValue<string>( "d", "" );
	if( !p.empty() ) {
		mPath = parsePath( p );
	}
}

void Path::appendShape2d( Shape2d *appendTo ) const
{
	for( const auto &contour : mPath.getContours() ) {
		appendTo->appendContour( contour );
	}
}

void Path::renderSelf( Renderer &renderer ) const
{
	renderer.drawPath( *this );
}

////////////////////////////////////////////////////////////////////////////////////
// Line
Line::Line( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	const auto doc = getDoc();
	const auto style = calcInheritedStyle();

	// If the attribute is not specified, the effect is as if a value of "0" were specified.
	if( xml.hasAttribute( "x1" ) )
		mPoint1.x = Value::parse( xml.getAttributeValue<string>( "x1" ) ).asUserWidth( doc, style );
	if( xml.hasAttribute( "y1" ) )
		mPoint1.y = Value::parse( xml.getAttributeValue<string>( "y1" ) ).asUserHeight( doc, style );
	if( xml.hasAttribute( "x2" ) )
		mPoint2.x = Value::parse( xml.getAttributeValue<string>( "x2" ) ).asUserWidth( doc, style );
	if( xml.hasAttribute( "y2" ) )
		mPoint2.y = Value::parse( xml.getAttributeValue<string>( "y2" ) ).asUserHeight( doc, style );
}

void Line::renderSelf( Renderer &renderer ) const
{
	renderer.drawLine( *this );
}

Shape2d Line::getShape() const
{
	Shape2d result;
	result.moveTo( mPoint1 );
	result.lineTo( mPoint2 );
	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Rect
Rect::Rect( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	const auto doc = getDoc();
	const auto style = calcInheritedStyle();

	if( xml.hasAttribute( "x" ) )
		mRect.x1 = Value::parse( xml["x"] ).asUserWidth( doc, style );
	else
		mRect.x1 = 0;
	if( xml.hasAttribute( "y" ) )
		mRect.y1 = Value::parse( xml["y"] ).asUserHeight( doc, style );
	else
		mRect.y1 = 0;

	float width = 0;
	float height = 0;
	if( xml.hasAttribute( "width" ) )
		width = Value::parse( xml["width"] ).asUserWidth( doc, style );
	if( xml.hasAttribute( "height" ) )
		height = Value::parse( xml["height"] ).asUserHeight( doc, style );
	mRect.x2 = mRect.x1 + width;
	mRect.y2 = mRect.y1 + height;

	if( xml.hasAttribute( "rx" ) )
		mRx = Value::parse( xml["rx"] );
	if( xml.hasAttribute( "ry" ) )
		mRy = Value::parse( xml["ry"] );

	// See: https://developer.mozilla.org/en-US/docs/Web/SVG/Attribute/rx
	if( !xml.hasAttribute( "rx" ) )
		mRx = mRy;
	if( !xml.hasAttribute( "ry" ) )
		mRy = mRx;

	if( mRx.isPercent() )
		mRx = Value( glm::clamp( mRx.asUser(), 0.0f, 50.0f ), Value::PERCENT );
	else
		mRx = Value( glm::clamp( mRx.asUser(), 0.0f, 0.5f * width ) );
	if( mRy.isPercent() )
		mRy = Value( glm::clamp( mRy.asUser(), 0.0f, 50.0f ), Value::PERCENT );
	else
		mRy = Value( glm::clamp( mRy.asUser(), 0.0f, 0.5f * height ) );

	mBoundingBox = mRect;
}

void Rect::renderSelf( Renderer &renderer ) const
{
	if( mRect.getWidth() > 0 && mRect.getHeight() > 0 ) // Zero-width or height rectangles should never be drawn.
		renderer.drawRect( *this );
}

float Rect::getRx() const
{
	if( mRx.isPercent() )
		return mRx.asUser( 1 ) * mRect.getWidth(); // Percentages will be converted to decimals, where 100% = 1.0f
	return mRx.asUser();
}

float Rect::getRy() const
{
	if( mRy.isPercent() )
		return mRy.asUser( 1 ) * mRect.getHeight(); // Percentages will be converted to decimals, where 100% = 1.0f
	return mRy.asUser();
}

Shape2d Rect::getShape() const
{
	Shape2d result;

	float rx = getRx();
	float ry = getRy();
	if( rx > 0 || ry > 0 )
		result.appendContour( Path2d::roundedRectangle( mRect, rx, ry ) );
	else
		result.appendContour( Path2d::rectangle( mRect ) );

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Polygon
vector<vec2> parsePointList( const std::string &p )
{
	vector<vec2> result;

	if( !p.empty() ) {
		char        item[64];
		const char *s = p.c_str();
		bool        odd = false;
		float       lastVal;
		while( *s ) {
			s = getNextPathItem( s, item );
			if( !odd )
				lastVal = float( strtod( item, nullptr ) );
			else
				result.emplace_back( lastVal, float( strtod( item, nullptr ) ) );
			odd = !odd;
		}
	}

	return result;
}

Polygon::Polygon( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	mPolyLine = PolyLine2f( parsePointList( xml.getAttributeValue<string>( "points", "" ) ) );
	mPolyLine.setClosed( true );
}

void Polygon::renderSelf( Renderer &renderer ) const
{
	renderer.drawPolygon( *this );
}

Shape2d Polygon::getShape() const
{
	Shape2d result;

	if( mPolyLine.getPoints().size() <= 1 )
		return result;

	result.moveTo( mPolyLine.getPoints()[0] );
	for( auto ptIt = mPolyLine.getPoints().begin() + 1; ptIt != mPolyLine.getPoints().end(); ++ptIt )
		result.lineTo( *ptIt );

	result.close();

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Polyline
Polyline::Polyline( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	mPolyLine = PolyLine2f( parsePointList( xml.getAttributeValue<string>( "points", "" ) ) );
	mPolyLine.setClosed( false );
}

void Polyline::renderSelf( Renderer &renderer ) const
{
	renderer.drawPolyline( *this );
}

Shape2d Polyline::getShape() const
{
	Shape2d result;

	if( mPolyLine.getPoints().size() <= 1 )
		return result;

	result.moveTo( mPolyLine.getPoints()[0] );
	for( auto ptIt = mPolyLine.getPoints().begin() + 1; ptIt != mPolyLine.getPoints().end(); ++ptIt )
		result.lineTo( *ptIt );

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// Group
Group::Group( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	Group::parse( xml );
}

Group::~Group()
{
	for( auto &child : mChildren )
		delete child;
}

void Group::parse( const XmlTree &xml )
{
	if( !approxEqual( getOpacity(), 1.0f ) )
		CI_LOG_W( "Group '" << getId() << "' opacity of " << getOpacity() << " is currently not supported." );

	for( XmlTree::ConstIter treeIt = xml.begin(); treeIt != xml.end(); ++treeIt ) {
		Node *node = create( this, *treeIt );
		if( node )
			mChildren.push_back( node );
	}

	if( xml.hasAttribute( "clip-path" ) ) {
		auto value = xml.getAttributeValue<std::string>( "clip-path" );

		if( !strncmp( value.c_str(), "url", 3 ) ) {
			char        id[1024];
			const char *hash = strchr( value.c_str(), '#' );
			const char *closeParen = strchr( value.c_str(), ')' );
			if( ( closeParen ) && ( hash ) && ( closeParen - hash < 1024 ) ) {
				strncpy( id, hash + 1, closeParen - hash - 1 );
				id[closeParen - hash - 1] = 0;

				mStyle.setClipPath( id );
			}
		}
	}
}

Node *Group::create( Node *parent, const XmlTree &xml )
{
	if( xml.getTag() == "clipPath" )
		return new ClipPath( parent, xml );
	if( xml.getTag() == "defs" )
		return new Defs( parent, xml );
	if( xml.getTag() == "g" )
		return new Group( parent, xml );
	if( xml.getTag() == "svg" )
		return new Doc( parent, xml );
	if( xml.getTag() == "path" )
		return new Path( parent, xml );
	if( xml.getTag() == "polygon" )
		return new Polygon( parent, xml );
	if( xml.getTag() == "polyline" )
		return new Polyline( parent, xml );
	if( xml.getTag() == "line" )
		return new Line( parent, xml );
	if( xml.getTag() == "rect" )
		return new Rect( parent, xml );
	if( xml.getTag() == "circle" )
		return new Circle( parent, xml );
	if( xml.getTag() == "ellipse" )
		return new Ellipse( parent, xml );
	if( xml.getTag() == "use" )
		return new Use( parent, xml );
	if( xml.getTag() == "image" )
		return new Image( parent, xml );
	if( xml.getTag() == "linearGradient" )
		return new LinearGradient( parent, xml );
	if( xml.getTag() == "radialGradient" )
		return new RadialGradient( parent, xml );
	if( xml.getTag() == "style" )
		return new Styles( parent, xml );
	if( xml.getTag() == "text" )
		return new Text( parent, xml );

	// Treat <switch> tags as normal groups and parse their contents.
	if( xml.getTag() == "switch" ) {
		CI_LOG_W( "The `switch` tag is currently not supported and will be treated as a normal group." );
		return new Group( parent, xml );
	}

	CI_LOG_W( "The `" << xml.getTag() << "` tag is currently not supported or recognized." );

	return nullptr;
}

const Node *Group::findNodeByIdContains( const std::string &idPartial, bool recurse ) const
{
	for( const auto &child : mChildren ) {
		if( child->getId().find( idPartial ) != string::npos ) {
			return child;
		}
	}

	if( recurse ) {
		for( auto child : mChildren ) {
			auto group = dynamic_cast<Group *>( child );
			if( group ) {
				const Node *result = group->findNodeByIdContains( idPartial );
				if( result )
					return result;
			}
		}
	}

	return nullptr;
}

const Node *Group::findNodeByTag( const std::string &tag, bool recurse ) const
{
	// see if any immediate children have tag 'tag'
	for( auto child : mChildren ) {
		if( child->getTag() == tag ) {
			return child;
		}
	}

	// see if any groups contain children with tag 'tag'
	if( recurse ) {
		for( auto child : mChildren ) {
			auto group = dynamic_cast<Group *>( child );
			if( group ) {
				const Node *result = group->findNodeByTag( tag );
				if( result )
					return result;
			}
		}
	}

	return nullptr;
}

const Node *Group::findNode( const std::string &id, bool recurse ) const
{
	// see if any immediate children are named 'id'
	for( auto child : mChildren ) {
		if( child->getId() == id ) {
			return child;
		}
	}

	// see if any groups contain children named 'id'
	if( recurse ) {
		for( auto child : mChildren ) {
			auto group = dynamic_cast<Group *>( child );
			if( group ) {
				const Node *result = group->findNode( id );
				if( result )
					return result;
			}
		}
	}

	return nullptr;
}

Node *Group::nodeUnderPoint( const vec2 &absolutePoint, const mat3 &parentInverseMatrix ) const
{
	mat3 invTransform = parentInverseMatrix;
	if( mSpecifiesTransform )
		invTransform = inverse( mTransform ) * invTransform;
	vec2 localPt = vec2( invTransform * vec3( absolutePoint, 1 ) );

	for( auto nodeIt = mChildren.rbegin(); nodeIt != mChildren.rend(); ++nodeIt ) {
		auto group = dynamic_cast<Group *>( *nodeIt );
		if( group ) {
			Node *node = group->nodeUnderPoint( absolutePoint, invTransform );
			if( node )
				return node;
		}
		else {
			if( ( *nodeIt )->specifiesTransform() ) {
				mat3 childInvTransform = ( *nodeIt )->getTransformInverse() * invTransform;
				if( ( *nodeIt )->containsPoint( vec2( childInvTransform * vec3( absolutePoint, 1 ) ) ) )
					return *nodeIt;
			}
			else if( ( *nodeIt )->containsPoint( localPt ) )
				return *nodeIt;
		}
	}

	return nullptr;
}

const Node *Group::findInAncestors( const std::string &elementId ) const
{
	if( elementId.empty() )
		return nullptr;

	const Node *result;

	if( getId() == elementId )
		return this;
	else if( ( result = findNode( elementId, true ) ) != nullptr )
		return result;
	else if( getParent() )
		return getParent()->findInAncestors( elementId );
	else
		return nullptr;
}

const Node *Group::findTagInAncestors( const std::string &elementTag ) const
{
	const Node *result;

	if( getTag() == elementTag )
		return this;
	else if( ( result = findNodeByTag( elementTag, true ) ) != nullptr )
		return result;
	else if( getParent() )
		return getParent()->findTagInAncestors( elementTag );
	else
		return nullptr;
}

const Node &Group::getChild( const std::string &id ) const
{
	const Node *result = findNode( id, false );
	if( !result )
		throw ExcChildNotFound( id );
	else
		return *result;
}

Shape2d Group::getMergedShape2d() const
{
	Shape2d result;
	appendMergedShape2d( &result );
	return result;
}

void Group::appendMergedShape2d( Shape2d *appendTo ) const
{
	for( auto child : mChildren ) {
		const auto *group = dynamic_cast<const Group *>( child );
		if( group )
			group->appendMergedShape2d( appendTo );
		else
			appendTo->append( child->getShape().transformed( child->getTransform() ) );
	}
}

const Node &Group::getChild( size_t index ) const
{
	auto childIt = mChildren.begin();
	while( index ) {
		--index;
		if( childIt == mChildren.end() )
			break;
	}

	if( childIt == mChildren.end() )
		throw ExcChildNotFound( "index " + to_string( index ) );

	return **childIt;
}

void Group::renderSelf( Renderer &renderer ) const
{
	renderer.pushGroup( *this, getStyle().getOpacity() );

	for( auto child : mChildren ) {
		Style style = child->getStyle();
		if( !renderer.visit( *child, &style ) )
			continue;
		if( child->getStyle().isDisplayNone() ) // display: none we don't even descend groups
			continue;
		if( ( !child->isVisible() ) && ( typeid( Group ) != typeid( *child ) ) ) // if this isn't visible and isn't a group, just move along
			continue;
		child->startRender( renderer, style );
		child->renderSelf( renderer );
		child->finishRender( renderer, style );
	}

	renderer.popGroup();
}

Rectf Group::calcBoundingBox() const
{
	bool  empty = true;
	Rectf result( 0, 0, 0, 0 );
	for( auto child : mChildren ) {
		Rectf childBounds = child->getBoundingBox().transformed( child->getTransform() );
		// only use child area if it exists (text nodes return [0,0,0,0])
		if( ( childBounds.getWidth() > 0 ) || ( childBounds.getHeight() > 0 ) ) {
			if( empty ) {
				result = childBounds;
				empty = false;
			}
			else {
				result.include( childBounds );
			}
		}
	}
	return result;
}

void Group::iterate( const std::function<void( Node * )> &fn )
{
	for( auto &child : mChildren ) {
		fn( child );
		if( typeid( *child ) == typeid( Group ) )
			static_cast<Group *>( child )->iterate( fn );
	}
}

////////////////////////////////////////////////////////////////////////////////////
// Use
Use::Use( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
	, mReferenced( nullptr )
{
	parse( xml );
}

void Use::parse( const XmlTree &xml )
{
	const auto doc = getDoc();
	const auto style = calcInheritedStyle();

	std::string ref;
	if( xml.hasAttribute( "xlink:href" ) )
		ref = xml.getAttributeValue<string>( "xlink:href" );
	else if( xml.hasAttribute( "href" ) )
		ref = xml.getAttributeValue<string>( "href" );

	vec2 translate{ 0 };
	if( xml.hasAttribute( "x" ) ) {
		translate.x = Value::parse( xml.getAttributeValue<std::string>( "x" ) ).asUserWidth( doc, style );
		mSpecifiesTransform = true;
	}
	if( xml.hasAttribute( "y" ) ) {
		translate.y = Value::parse( xml.getAttributeValue<std::string>( "y" ) ).asUserHeight( doc, style );
		mSpecifiesTransform = true;
	}
	mTransform = glm::translate( mTransform, translate );

	if( ref.size() > 1 ) {
		if( ref[0] == '#' ) {
			string elementId = ref.substr( 1, string::npos );
			mReferenced = findInAncestors( elementId );
		}
	}
}

void Use::renderSelf( Renderer &renderer ) const
{
	if( mReferenced ) {
		Style style = mReferenced->getStyle();
		if( !renderer.visit( *mReferenced, &style ) )
			return;
		mReferenced->startRender( renderer, style );
		mReferenced->renderSelf( renderer );
		mReferenced->finishRender( renderer, style );
	}
}

////////////////////////////////////////////////////////////////////////////////////
// PreserveAspectRatio
PreserveAspectRatio::PreserveAspectRatio( const std::string &value )
{
	const auto keywords = split( trim( toLower( value ) ), ' ', true );
	if( keywords.empty() )
		return; // Error!

	if( keywords[0] == "none" )
		align = ALIGN_NONE;
	else if( keywords[0] == "xminymin" )
		align = ALIGN_X_MIN_Y_MIN;
	else if( keywords[0] == "xmidymin" )
		align = ALIGN_X_MID_Y_MIN;
	else if( keywords[0] == "xmaxymin" )
		align = ALIGN_X_MAX_Y_MIN;
	else if( keywords[0] == "xminymid" )
		align = ALIGN_X_MIN_Y_MID;
	else if( keywords[0] == "xmidymid" )
		align = ALIGN_X_MID_Y_MID;
	else if( keywords[0] == "xmaxymid" )
		align = ALIGN_X_MAX_Y_MID;
	else if( keywords[0] == "xminymax" )
		align = ALIGN_X_MIN_Y_MIN;
	else if( keywords[0] == "xmidymax" )
		align = ALIGN_X_MID_Y_MAX;
	else if( keywords[0] == "xmaxymax" )
		align = ALIGN_X_MAX_Y_MAX;

	if( keywords.size() > 1 ) {
		if( keywords[1] == "slice" )
			meetOrSlice = SLICE;
	}
}

mat3 PreserveAspectRatio::calcTransform( const Rectf &element, const Rectf &viewBox, bool normalized ) const
{
	// See: https://svgwg.org/svg2-draft/coords.html#ComputingAViewportsTransform
	mat3 m33;

	if( viewBox.getWidth() > 0 && viewBox.getHeight() > 0 ) {
		m33[0][0] = element.getWidth() / viewBox.getWidth();          // scale-x
		m33[1][1] = element.getHeight() / viewBox.getHeight();        // scale-y
		if( align != ALIGN_NONE && meetOrSlice == MEET )              //
			m33[0][0] = m33[1][1] = glm::min( m33[0][0], m33[1][1] ); //
		else if( align != ALIGN_NONE && meetOrSlice == SLICE )        //
			m33[0][0] = m33[1][1] = glm::max( m33[0][0], m33[1][1] ); //
		m33[2][0] = element.x1 - ( viewBox.x1 * m33[0][0] );          // translate-x
		m33[2][1] = element.y1 - ( viewBox.y1 * m33[1][1] );          // translate-y

		if( align == ALIGN_X_MID_Y_MIN || align == ALIGN_X_MID_Y_MID || align == ALIGN_X_MID_Y_MAX )
			m33[2][0] += ( element.getWidth() - viewBox.getWidth() * m33[0][0] ) * 0.5f;
		else if( align == ALIGN_X_MAX_Y_MIN || align == ALIGN_X_MAX_Y_MID || align == ALIGN_X_MAX_Y_MAX )
			m33[2][0] += element.getWidth() - viewBox.getWidth() * m33[0][0];
		if( align == ALIGN_X_MIN_Y_MID || align == ALIGN_X_MID_Y_MID || align == ALIGN_X_MAX_Y_MID )
			m33[2][1] += ( element.getHeight() - viewBox.getHeight() * m33[1][1] ) * 0.5f;
		else if( align == ALIGN_X_MIN_Y_MAX || align == ALIGN_X_MID_Y_MAX || align == ALIGN_X_MAX_Y_MAX )
			m33[2][1] += element.getHeight() - viewBox.getHeight() * m33[1][1];
	}

	// Normalize.
	if( normalized && element.getWidth() > 0 && element.getHeight() > 0 ) {
		m33[0][0] = float( viewBox.getWidth() ) * m33[0][0] / element.getWidth();
		m33[1][1] = float( viewBox.getHeight() ) * m33[1][1] / element.getHeight();
		m33[2][0] /= element.getWidth();
		m33[2][1] /= element.getHeight();
	}

	return m33;
}

////////////////////////////////////////////////////////////////////////////////////
// Image
Image::Image( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
	, mBounds( 0, 0, 0, 0 )
{
	const auto doc = getDoc();
	const auto style = calcInheritedStyle();

	if( xml.hasAttribute( "x" ) )
		mBounds.x1 = Value::parse( xml.getAttributeValue<string>( "x" ) ).asUserWidth( doc, style );
	if( xml.hasAttribute( "y" ) )
		mBounds.y1 = Value::parse( xml.getAttributeValue<string>( "y" ) ).asUserHeight( doc, style );
	if( xml.hasAttribute( "width" ) )
		mBounds.x2 = mBounds.x1 + Value::parse( xml.getAttributeValue<string>( "width" ) ).asUserWidth( doc, style );
	if( xml.hasAttribute( "height" ) )
		mBounds.y2 = mBounds.y1 + Value::parse( xml.getAttributeValue<string>( "height" ) ).asUserHeight( doc, style );

	std::string ref;
	if( xml.hasAttribute( "xlink:href" ) )
		ref = xml.getAttributeValue<string>( "xlink:href" );
	else if( xml.hasAttribute( "href" ) )
		ref = xml.getAttributeValue<string>( "href" );

	if( ref.find( "data:" ) == 0 ) {
		parseDataImage( ref );
	}
	else if( !ref.empty() ) {
		auto ext = fs::path( ref ).extension().string();
		if( ext == ".svg" ) {
			const auto path = doc->getFilePath() / ref;
			mSvg = svg::Doc::create( this, loadFile( path ), path );
		}
		else
			mImage = doc->loadImage( ref );
	}

	// Calculate texture transform matrix.
	Rectf element( 0, 0, mBounds.getWidth(), mBounds.getHeight() );
	Rectf viewBox = mImage ? Rectf( mImage->getBounds() ) : mSvg ? mSvg->getBounds() : Rectf{};
	if( xml.hasAttribute( "preserveAspectRatio" ) )
		mTextureMatrix = PreserveAspectRatio( xml.getAttributeValue<string>( "preserveAspectRatio" ) ).calcTransform( element, viewBox, true );
	else
		mTextureMatrix = PreserveAspectRatio().calcTransform( element, viewBox, true );


	if( xml.hasAttribute( "clip-path" ) ) {
		auto value = xml.getAttributeValue<std::string>( "clip-path" );

		if( !strncmp( value.c_str(), "url", 3 ) ) {
			char        id[1024];
			const char *hash = strchr( value.c_str(), '#' );
			const char *closeParen = strchr( value.c_str(), ')' );
			if( closeParen && hash && closeParen - hash < 1024 ) {
				strncpy( id, hash + 1, closeParen - hash - 1 );
				id[closeParen - hash - 1] = 0;

				mStyle.setClipPath( id );
			}
		}
	}
}

bool Image::parseDataImage( const string &data )
{
	mImage.reset();
	mSvg.reset();

	size_t dataOffset = data.find( "data:" ) + 5;
	size_t semi = data.find( ';' );
	size_t comma = data.find( ',' );
	if( semi == string::npos || comma == string::npos )
		return false;

	size_t len = data.size() - comma - 1;
	auto   buf = make_shared<Buffer>( fromBase64( &data[comma + 1], len ) );

	string mime = data.substr( dataOffset, semi - dataOffset );
	if( mime == "image/svg+xml" ) {
		// See also: https://www.w3.org/TR/SVG2/embedded.html#ImageElement
		mSvg = svg::Doc::createFromSvgz( DataSourceBuffer::create( buf ) );

		// To prevent breaking changes, use a placeholder image.
		unsigned char bytes[4] = { 255, 0, 0, 255 };
		mImage = std::make_shared<Surface8u>( bytes, 1, 1, 4, SurfaceChannelOrder::RGBA );

		return true;
	}
	else {
		string extension;
		if( mime == "image/png" )
			extension = "png";
		else if( mime == "image/jpeg" )
			extension = "jpeg";

		try {
			mImage = std::make_shared<Surface8u>( loadImage( DataSourceBuffer::create( buf ), ImageSource::Options(), extension ) );
			return true;
		}
		catch( std::exception &exc ) {
			CI_LOG_W( "failed to parse data image, what: " << exc.what() );
		}
	}
	return false;
}

void Image::renderSelf( Renderer &renderer ) const
{
	if( mImage || mSvg )
		renderer.drawImage( *this );
}

////////////////////////////////////////////////////////////////////////////////////
// Text
Text::Text( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
	, mAttributes( xml )
{
	for( XmlTree::ConstIter treeIt = xml.begin(); treeIt != xml.end(); ++treeIt ) {
		if( treeIt->getTag().empty() ) { // data!
			mSpans.push_back( std::make_shared<TextSpan>( this, treeIt->getValue() ) );
		}
		else if( treeIt->getTag() == "tspan" ) { // tspan!
			mSpans.push_back( std::make_shared<TextSpan>( this, *treeIt ) );
		}
	}
}

#if 0 
Shape2d Text::getShape() const
{
	Shape2d result;	
	for( vector<TextSpanRef>::const_iterator spanIt = mSpans.begin(); spanIt != mSpans.end(); ++spanIt )
		result.append( (*spanIt)->getShape() );
	
	return result;
}
#endif

vec2 Text::getTextPen() const
{
	if( !mAttributes.mX.isSet() || !mAttributes.mY.isSet() ) {
		return {};
	}

	return { mAttributes.mX.asUser(), mAttributes.mY.asUser() };
}

float Text::getRotation() const
{
	if( mAttributes.mRotate.size() != 1 ) {
		return 0;
	}
	else
		return mAttributes.mRotate[0].asUser();
}

Value Text::getLetterSpacing() const
{
	if( mAttributes.mLetterSpacing.size() != 1 ) {
		return { 0 };
	}
	else
		return mAttributes.mLetterSpacing[0];
}

void Text::renderSelf( Renderer &renderer ) const
{
	renderer.pushTextPen( vec2() ); // this may be overridden by the attributes, but that's ok
	mAttributes.startRender( renderer );
	for( const auto &span : mSpans ) {
		span->renderSelf( renderer );
	}
	mAttributes.finishRender( renderer );
	renderer.popTextPen();
}

////////////////////////////////////////////////////////////////////////////////////
// TextSpan
TextSpan::TextSpan( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
	, mIgnoreAttributes( false )
	, mAttributes( xml )
	, mFont( nullptr )
{
	for( XmlTree::ConstIter treeIt = xml.begin(); treeIt != xml.end(); ++treeIt ) {
		if( treeIt->getTag().empty() ) { // data!
			mSpans.push_back( std::make_shared<TextSpan>( this, treeIt->getValue() ) );
		}
		else if( treeIt->getTag() == "tspan" ) { // tspan!
			mSpans.push_back( std::make_shared<TextSpan>( this, *treeIt ) );
		}
	}
}

TextSpan::TextSpan( Node *parent, const std::string &str )
	: Node( parent )
	, mIgnoreAttributes( true )
	, mFont( nullptr )
{
	// replace all multi-char whitespace with single space
	/*size_t c = 0;
	while( str[c] ) {
		if( isspace(str[c]) ) mString += ' ';
		while( str[c] && isspace(str[c]) )
			nextCharUtf8( str.c_str(), &c );
		while( str[c] && ( ! isspace(str[c]) ) ) // this is not really correct - does not work with UTF32 code points that are >255
			mString += nextCharUtf8( str.c_str(), &c );
	}*/
	// Technically multi-char whitespace should be reduced to single chars; needs to be revisited with unicode-aware version of this method
	mString = str;
}

void TextSpan::renderSelf( Renderer &renderer ) const
{
	// Style style = getStyle(); // Resolves style if needed.
	// if( !renderer.visit( *this, &style ) )
	//	return;
	// startRender( renderer, style );
	// if( !mIgnoreAttributes ) // TextSpans that are actually the contents of Text's attributes should be ignored
	//	mAttributes.startRender( renderer );
	if( !mString.empty() ) {
		renderer.drawTextSpan( *this );
	}
	for( const auto &span : mSpans ) {
		span->renderSelf( renderer );
	}
	// if( !mIgnoreAttributes )
	//	mAttributes.finishRender( renderer );
	// finishRender( renderer, style );
}

std::vector<std::pair<uint16_t, vec2>> TextSpan::getGlyphMeasures() const
{
	if( !mGlyphMeasures ) {
		std::vector<uint32_t> glyphs;
		std::vector<vec2>     positions;

		mFont->shapeString( text::ShapingOptions(), mString.c_str(), mString.size(), 0, 0, &glyphs, nullptr, &positions, nullptr, nullptr, nullptr );
		mGlyphMeasures->resize( glyphs.size() );
		for( size_t g = 0; g < glyphs.size(); ++g )
			( *mGlyphMeasures )[g] = std::make_pair( uint16_t( glyphs[g] ), positions[g] );
	}

	return *mGlyphMeasures;
}

#if 0
// This is not implemented
Shape2d TextSpan::getShape() const
{
	if( ! mShape ) {
		mShape = shared_ptr<Shape2d>( new Shape2d() );
		
		if( ! mString.empty() ) {
			shared_ptr<Font> font = getFont();
			if( ! font )
				return Shape2d();
			TextBox tbox = TextBox().font( *font ).text( mString );
			vector<pair<uint16_t,vec2> > glyphs = getGlyphMeasures();
			vec2 textPen = getTextPen();
			float rotation = getRotation();
			bool shouldRotate = fabs( rotation ) > 0.0001f;
			MatrixAffine2f rotationMatrix = MatrixAffine2f::makeRotate( toRadians( rotation ) );
			for( size_t g = 0; g < glyphs.size(); ++g ) {
				MatrixAffine2f m = MatrixAffine2f::makeTranslate( textPen + vec2( glyphs[g].second.x, 0 ) );
				if( shouldRotate )
					m *= rotationMatrix;
				mShape->append( font->getGlyphShape( glyphs[g].first ).getTransform( m ) );
			}
		}

		for( vector<TextSpanRef>::const_iterator spanIt = mSpans.begin(); spanIt != mSpans.end(); ++spanIt )
			mShape->append( (*spanIt)->getShape() );
	}
		
	return *mShape;
}
#endif

// TextSpan::Atributes
TextSpan::Attributes::Attributes( const XmlTree &xml )
{
	if( xml.hasAttribute( "x" ) )
		mX = readValue( xml["x"] );
	if( xml.hasAttribute( "y" ) )
		mY = readValue( xml["y"] );
	if( xml.hasAttribute( "rotate" ) )
		mRotate = readValueList( xml["rotate"], false );
	if( xml.hasAttribute( "letter-spacing" ) )
		mLetterSpacing = readValueList( xml["letter-spacing"], false );
}

text::Font *TextSpan::getFont() const
{
	return getFont( getFontFamilies() );
}

text::Font *TextSpan::getFont( const std::vector<std::string> &fontFamilies ) const
{
	if( !mFont )
		mFont = Style::getFont( fontFamilies, getFontSize().asUser() );

	return mFont;
}

vec2 TextSpan::getTextPen() const
{
	if( mIgnoreAttributes || ( !mAttributes.mX.isSet() ) || ( !mAttributes.mY.isSet() ) ) {
		if( !mParent )
			return {};
		else if( typeid( *mParent ) == typeid( TextSpan ) )
			return reinterpret_cast<const TextSpan *>( mParent )->getTextPen();
		else if( typeid( *mParent ) == typeid( Text ) )
			return reinterpret_cast<const Text *>( mParent )->getTextPen();
		else
			return {};
	}
	else
		return { mAttributes.mX.asUser(), mAttributes.mY.asUser() };
}

void TextSpan::setTextPen( const vec2 &textPen )
{
	if( mIgnoreAttributes ) {
		if( !mParent )
			return;
		else if( typeid( *mParent ) == typeid( TextSpan ) )
			return reinterpret_cast<TextSpan *>( mParent )->setTextPen( textPen );
		else if( typeid( *mParent ) == typeid( Text ) )
			return reinterpret_cast<Text *>( mParent )->setTextPen( textPen );
	}
	else
		mAttributes.setTextPen( textPen );
}

float TextSpan::getRotation() const
{
	if( mIgnoreAttributes || ( mAttributes.mRotate.size() != 1 ) ) {
		if( !mParent )
			return 0;
		else if( typeid( *mParent ) == typeid( TextSpan ) )
			return reinterpret_cast<const TextSpan *>( mParent )->getRotation();
		else if( typeid( *mParent ) == typeid( Text ) )
			return reinterpret_cast<const Text *>( mParent )->getRotation();
		else
			return 0;
	}
	else
		return mAttributes.mRotate[0].asUser();
}

Value TextSpan::getLetterSpacing() const
{
	if( mIgnoreAttributes || ( mAttributes.mLetterSpacing.size() != 1 ) ) {
		if( !mParent )
			return 0;
		else if( typeid( *mParent ) == typeid( TextSpan ) )
			return reinterpret_cast<const TextSpan *>( mParent )->getLetterSpacing();
		else if( typeid( *mParent ) == typeid( Text ) )
			return reinterpret_cast<const Text *>( mParent )->getLetterSpacing();
		else
			return 0;
	}
	else
		return mAttributes.mLetterSpacing[0];
}

void TextSpan::Attributes::startRender( Renderer &renderer ) const
{
	if( mX.isSet() && mY.isSet() )
		renderer.pushTextPen( vec2( mX.asUser(), mY.asUser() ) );
	if( mRotate.size() == 1 )
		renderer.pushTextRotation( mRotate[0].asUser() );
	else
		renderer.pushTextRotation( 0 );
}

void TextSpan::Attributes::finishRender( Renderer &renderer ) const
{
	if( mX.isSet() && mY.isSet() )
		renderer.popTextPen();
	renderer.popTextRotation();
}

void TextSpan::Attributes::setTextPen( const vec2 &textPen )
{
	mX = Value( textPen.x );
	mY = Value( textPen.y );
}

////////////////////////////////////////////////////////////////////////////////////
// Defs
Defs::Defs( Node *parent, const XmlTree &xml )
	: Group( parent )
	, mXml( xml )
{
	parse( xml );
}

const Node *Defs::findNode( const std::string &id, bool recurse ) const
{
	const Node *result = Group::findNode( id, recurse );
	if( !result ) {
		// see if any immediate non-instantiated children are named 'id'
		for( XmlTree::ConstIter treeIt = mXml.begin(); treeIt != mXml.end(); ++treeIt ) {
			if( !treeIt->hasAttribute( "id" ) )
				continue;
			if( treeIt->getAttributeValue<std::string>( "id" ) != id )
				continue;

			// instantiate the requested node and return it
			Defs *self = const_cast<Defs *>( this );
			Node *node = Group::create( self, *treeIt );
			if( node )
				self->mChildren.push_back( node );

			return node;
		}
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////////
// ClipPath
ClipPath::ClipPath( Node *parent, const XmlTree &xml )
	: Group( parent, xml )
{
	if( xml.hasAttribute( "clipPathUnits" ) ) {
		mUseObjectBoundingBox = xml.getAttributeValue<string>( "clipPathUnits" ) != string( "userSpaceOnUse" );
	}

	for( const auto &child : mChildren ) {
		if( !child->isDisplayNone() && child->isVisible() ) {
			mIsDisplayNone = false;
			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////
// Styles
Styles::Styles( Node *parent, const XmlTree &xml )
	: Node( parent, xml )
{
	const auto value = trim( xml.getValue() );

	css::Parser parser;
	parser.parse( value );

	{
		Style                    style;
		std::vector<std::string> selectors;
		std::string              key;
		std::string              value;

		css::Parser::Token token = parser.getNextToken();
		while( token.type != css::Parser::CSS_END ) {
			switch( token.type ) {
			case css::Parser::SEL_START:
				selectors = split( token.data, ',', true );
				style.clear();
				break;
			case css::Parser::SEL_END:
				for( const auto &selector : selectors ) {
					const auto id = ltrim_copy( selector, "." );
					if( mStyleList.count( id ) > 0 )
						mStyleList.at( id ) += style;
					else
						mStyleList.insert_or_assign( id, style );
				}
				break;
			case css::Parser::PROPERTY:
				key = token.data;
				break;
			case css::Parser::VALUE:
				value = token.data;
				style.parseProperty( key, value, this );
				break;
			default:
				break;
			}

			token = parser.getNextToken();
		}
	}
}

Style Styles::findStyle( const std::string &id ) const
{
	if( mStyleList.count( id ) > 0 )
		return mStyleList.at( id );

	return {};
}

////////////////////////////////////////////////////////////////////////////////////
// Doc
Doc::Doc()
	: Group( nullptr )
	, mBounds( 0, 0, 0, 0 )
{
}

Doc::Doc( Node *parent, const XmlTree &xml )
	: Group( parent, xml )
	, mBounds( 0, 0, 0, 0 )
{
	loadDoc( xml );
}

Doc::Doc( const fs::path &filePath )
	: Group( nullptr )
	, mBounds( 0, 0, 0, 0 )
{
	loadDoc( loadFile( filePath ), filePath );
}

Doc::Doc( const DataSourceRef &dataSource, const fs::path &filePath )
	: Doc( nullptr, dataSource, filePath )
{
}

Doc::Doc( Node *parent, const DataSourceRef &dataSource, const fs::path &filePath )
	: Group( parent )
	, mBounds( 0, 0, 0, 0 )
{
	fs::path relativePath = filePath;
	if( filePath.empty() )
		relativePath = fs::path( dataSource->getFilePathHint() );
	loadDoc( dataSource, relativePath );
}

DocRef Doc::create( Node *parent, const XmlTree &xml )
{
	return std::make_shared<Doc>( parent, xml );
}

DocRef Doc::create( const fs::path &filePath )
{
	return std::make_shared<Doc>( filePath );
}

DocRef Doc::create( const DataSourceRef &dataSource, const fs::path &filePath )
{
	return std::make_shared<Doc>( dataSource, filePath );
}

DocRef Doc::create( Node *parent, const DataSourceRef &dataSource, const fs::path &filePath )
{
	return std::make_shared<Doc>( parent, dataSource, filePath );
}

DocRef Doc::createFromSvgz( const DataSourceRef &dataSource, const fs::path &filePath )
{
	fs::path relativePath = filePath;
	if( filePath.empty() )
		relativePath = dataSource->getFilePathHint();

	Buffer    compressed( dataSource );
	BufferRef decompressed = make_shared<Buffer>( decompressBuffer( compressed, false, true ) );

	return std::make_shared<Doc>( DataSourceBuffer::create( decompressed, relativePath ) );
}

void Doc::loadDoc( const XmlTree &xml )
{
	if( xml.hasAttribute( "viewBox" ) ) {
		auto        vbox = xml.getAttributeValue<string>( "viewBox" );
		const char *vbCPtr = vbox.c_str();
		mViewBox.x1 = parseFloat( &vbCPtr );
		mViewBox.y1 = parseFloat( &vbCPtr );
		mViewBox.x2 = mViewBox.x1 + parseFloat( &vbCPtr );
		mViewBox.y2 = mViewBox.y1 + parseFloat( &vbCPtr );
	}
	else {
		const Doc *doc = getDoc();
		if( doc )
			mViewBox = doc->mViewBox;
		else
			mViewBox = getBoundingBox().transformed( getTransform() ).scaledCentered( 1.1f );
	}
	if( xml.hasAttribute( "x" ) ) {
		Value val = Value::parse( xml.getAttributeValue<string>( "x" ) );
		if( val.isPercent() )
			mBounds.x1 = val.asUser( 1 ) * mViewBox.getWidth();
		else
			mBounds.x1 = val.asUser( 100, getDpi() );
	}
	if( xml.hasAttribute( "y" ) ) {
		Value val = Value::parse( xml.getAttributeValue<string>( "y" ) );
		if( val.isPercent() )
			mBounds.y1 = val.asUser( 1 ) * mViewBox.getHeight();
		else
			mBounds.y1 = val.asUser( 100, getDpi() );
	}
	if( xml.hasAttribute( "width" ) ) {
		Value val = Value::parse( xml.getAttributeValue<string>( "width" ) );
		if( val.isPercent() )
			mBounds.x2 = mBounds.x1 + val.asUser( 1 ) * mViewBox.getWidth();
		else
			mBounds.x2 = mBounds.x1 + val.asUser( 100, getDpi() );
	}
	else
		mBounds.x2 = mBounds.x1 + mViewBox.getWidth();
	if( xml.hasAttribute( "height" ) ) {
		Value val = Value::parse( xml.getAttributeValue<string>( "height" ) );
		if( val.isPercent() )
			mBounds.y2 = mBounds.y1 + val.asUser( 1 ) * mViewBox.getHeight();
		else
			mBounds.y2 = mBounds.y1 + val.asUser( 100, getDpi() );
	}
	else
		mBounds.y2 = mBounds.y1 + mViewBox.getHeight();

	bool needsViewBoxMapping = mViewBox.getWidth() > 0 && mViewBox.getHeight() > 0 && getWidth() > 0 && getHeight() > 0;
	if( needsViewBoxMapping ) {
		if( xml.hasAttribute( "preserveAspectRatio" ) )
			setTransform( PreserveAspectRatio( xml.getAttributeValue<string>( "preserveAspectRatio" ) ).calcTransform( mBounds, mViewBox ) );
		else
			setTransform( PreserveAspectRatio().calcTransform( mBounds, mViewBox ) );
	}

	Node::parseStyle( xml );
	Group::parse( xml );
}

void Doc::loadDoc( const DataSourceRef &source, const fs::path &filePath )
{
	if( !filePath.empty() )
		mFilePath = filePath.parent_path();

	auto xml = std::make_shared<XmlTree>( source, XmlTree::ParseOptions().ignoreDataChildren( false ) );

	loadDoc( xml->getChild( "svg" ) );
}

shared_ptr<Surface8u> Doc::loadImage( const fs::path &relativePath ) const
{
	if( mImageCache.find( relativePath ) == mImageCache.end() ) {
		try {
			if( relativePath.string().substr( 0, 4 ) == "http" ) {
				mImageCache[relativePath] = std::make_shared<Surface8u>( ci::loadImage( loadUrl( relativePath.string(), UrlOptions() ) ) );
			}
			else {
#if defined( CINDER_UWP )
				fs::path fullPath = ( mFilePath / relativePath );
#else
				fs::path fullPath = ( mFilePath / relativePath ).make_preferred();
#endif

				if( exists( fullPath ) )
					mImageCache[relativePath] = std::make_shared<Surface8u>( ci::loadImage( fullPath ) );
			}
		}
		catch( ... ) {
		}
	}

	if( mImageCache.find( relativePath ) != mImageCache.end() )
		return mImageCache[relativePath];
	else
		return {};
}

Node *Doc::nodeUnderPoint( const vec2 &pt ) const
{
	return Group::nodeUnderPoint( pt, mat3() );
}

void Doc::renderSelf( Renderer &renderer ) const
{
	Group::renderSelf( renderer );
}

ExcChildNotFound::ExcChildNotFound( const string &child )
{
	setDescription( "Could not find child: " + child );
}

} // namespace svg
} // namespace cinder
