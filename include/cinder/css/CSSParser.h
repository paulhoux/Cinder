/*
 Copyright (c) 2023, The Barbarian Group
 All rights reserved.

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

#include "cinder/Utilities.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

namespace cinder {
namespace css {

class Parser {
  public:
	enum ParseStatus { IN_SELECTOR, IN_PROPERTY, IN_VALUE, IN_STRING, IN_COMMENT, IN_AT_BLOCK };
	enum MessageType { INFORMATION, WARNING, ERROR };
	enum TokenType { CHARSET, IMPORT, NAMESPACE, AT_START, AT_END, SEL_START, SEL_END, PROPERTY, VALUE, COMMENT, CSS_END };

	struct Token {
		TokenType   type{ CSS_END };
		int         pos{ 0 };
		int         line{ 0 };
		std::string data;
	};

	struct Message {
		std::string m;
		MessageType t{ INFORMATION };
	};

	Parser();

	void setLevel( const std::string &level );

	void parse( std::string css );

	void resetParser();

	//! Serialize the current list of tokens to CSS.
	std::string serialize( const std::string &filename = "", bool tostdout = true ) const;

	// access charset, namespace and imports without having to walk css tokens
	std::string              getCharset();
	std::vector<std::string> getImport();
	std::string              getNamespace();

	// walk the css tokens list, token by token
	// set \a offset to the position you would like to start at in the list
	// leaving it as -1 will simply start at 0 and increment
	// last token is a dummy token with type set to CSS_END
	Token getNextToken( int offset = -1 );

	// covert token type enum value to a descriptive string
	std::string getTypeName( TokenType t );

	// this routine allows external modifications to the css tokens
	// to be brought back into parser in order to serialize them
	// with serialize_css
	void setTokens( const std::vector<Token> &tokens );

  private:
	//!
	static char charToLower( const char c )
	{
		if( c >= 'A' && c <= 'Z' )
			return char( c + 32 );
		return c;
	}
	//!
	static char charToUpper( const char c )
	{
		if( c >= 'a' && c <= 'z' )
			return char( c - 32 );
		return c;
	}
	//!
	static bool isWhiteSpace( const char c ) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 11; }
	//!
	static bool isDigit( const char c ) { return c >= '0' && c <= '9'; }
	//!
	static bool isHexDigit( char c )
	{
		c = charToLower( c );
		return isDigit( c ) || c == 'a' || c == 'b' || c == 'c' || c == 'd' || c == 'e' || c == 'f';
	}
	//!
	static bool isAlpha( char c )
	{
		c = charToLower( c );
		return c >= 'a' && c <= 'z';
	}
	//!
	double hexdec( std::string istring )
	{
		double ret = 0;
		istring = trim( istring );
		for( size_t i = 0; i < istring.length(); ++i ) {
			double multiplier = pow( 16.0, double( istring.length() - i - 1 ) );

			if( isDigit( istring[i] ) )
				ret += double( char( istring[i] ) - '0' ) * multiplier;
			else if( isHexDigit( istring[i] ) )
				ret += double( charToUpper( char( istring[i] ) ) - 'A' + 10 ) * multiplier;
		}

		return ret;
	}
	//!
	static std::string indent( int lvl, const std::string &base )
	{
		std::string ind;
		for( int i = 0; i < lvl; i++ ) {
			ind += base;
		}
		return ind;
	}
	//!
	static bool escaped( const std::string &istring, std::string::size_type pos ) { return !( charAt( istring, pos - 1 ) != '\\' || escaped( istring, pos - 1 ) ); }
	//! Safe replacement for .at()
	static char charAt( const std::string &istring, size_t pos )
	{
		if( pos < istring.length() )
			return istring[pos];

		return 0;
	}
	//!
	static std::vector<std::string> explode( const std::string &e, std::string s, const bool check )
	{
		std::vector<std::string> ret;

		auto iPos = s.find( e, 0 );
		auto iPit = e.length();

		while( iPos != std::string::npos ) {
			if( iPos > 0 || check ) {
				ret.push_back( s.substr( 0, iPos ) );
			}
			s.erase( 0, iPos + iPit );
			iPos = s.find( e, 0 );
		}

		if( !s.empty() || check ) {
			ret.push_back( s );
		}
		return ret;
	}
	//!
	static std::string implode( const std::string &e, const std::vector<std::string> &s )
	{
		std::string ret;
		for( size_t i = 0; i < s.size(); ++i ) {
			ret += s[i];
			if( i != s.size() - 1 )
				ret += e;
		}
		return ret;
	}
	//!
	std::string buildValue( const std::vector<std::string> &subvalues ) const
	{
		std::string ret;
		for( size_t i = 0; i < subvalues.size(); ++i ) {
			ret += subvalues[i];
			if( i != subvalues.size() - 1 ) {
				char last = charAt( subvalues[i], subvalues[i].length() - 1 );
				char next = charAt( subvalues[i + 1], 0 );
				if( strchr( "(,=:", last ) != nullptr || strchr( "),=:", next ) != nullptr ) {
					continue;
				}
				ret += " ";
			}
		}
		return ret;
	}
	//!
	static std::string strReplace( const std::string &find, const std::string &replace, std::string str )
	{
		auto pos = str.find( find );
		while( pos != std::string::npos ) {
			str.replace( pos, find.length(), replace );
			pos = str.find( find, pos + replace.length() );
		}
		return str;
	}
	//!
	static bool inCharArray( const char *haystack, const char needle )
	{
		for( size_t i = 0; i < strlen( haystack ); ++i ) {
			if( haystack[i] == needle ) {
				return true;
			}
		}
		return false;
	}
	//!
	static bool inStringArray( const std::string &haystack, const char needle ) { return haystack.find_first_of( needle, 0 ) != std::string::npos; }

	//!
	void parseInAtBlock( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from );
	//!
	void parseInSelector( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, bool &invalid_at, char &str_char, int str_size );
	//!
	void parseInProperty( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, bool &invalid_at );
	//!
	void parseInValue( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, bool &invalid_at, char &str_char, bool &pn, int str_size );
	//!
	void parseInComment( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, std::string &cur_comment );
	//!
	void parseInString( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, char &str_char, bool &str_in_str );

	//! Parses unicode notations
	std::string unicode( std::string &str, std::string::size_type &i );

	//! checks if the chat in \a str at i is a token
	bool isToken( const std::string &str, std::string::size_type i ) const;
	//!
	void addToken( TokenType type, const std::string &data, bool force = false );

	int seekNoComment( int key, int move ) const;

	void explodeSelectors();

	static bool propertyIsNext( std::string str, std::string::size_type pos );

	// private member variables
	std::vector<std::string>                     mTokenTypeNames{};
	std::unordered_map<std::string, ParseStatus> mAtRules{};
	std::vector<std::string>                     mCssTemplate{};
	std::string                                  mCssLevel;
	std::string                                  mTokens;
	int                                          mTokenPtr;
	int mLine;
	int mSelectorNestLevel;

	std::string              mCharset;
	std::string              mNamespace;
	std::vector<std::string> mImport{};
	std::vector<Token>       mCssTokens{};
	std::string              mCurSelector;
	std::string              mCurAt;
	std::string              mCurProperty;
	std::string              mCurFunction;
	std::string              mCurSubValue;
	std::string              mCurString;
	std::vector<int>         mSelSeparate{};
	std::vector<std::string> mCurSubValueArray{};
	std::vector<std::string> mCurFunctionArray{};
};

} // namespace css
} // namespace cinder
