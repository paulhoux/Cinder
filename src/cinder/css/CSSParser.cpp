#include "cinder/css/CSSParser.h"

#include "cinder/Log.h"
#include "cinder/css/CSSProperties.h"

#include "cinder/Utilities.h"

#include <fstream>

namespace cinder {
namespace css {

Parser::Parser()
	: mTokenPtr( 0 )
	, mLine( 1 )
	//, mStatePos( 0 )
	//, mStateLine( 0 )
	, mSelectorNestLevel( 0 )
{
	mTokens = "{};:()@='\"/,\\!$%&*+.<>?[]^`|~";

	// Used for serializing parsed css
	mCssTemplate.emplace_back( "    " );  //  0 - standard indentation
	mCssTemplate.emplace_back( " {\n" );  //  1 - bracket after @-rule
	mCssTemplate.emplace_back( "" );      //  2 - unused
	mCssTemplate.emplace_back( " {\n" );  //  3 - bracket after selector was "\n{\n"
	mCssTemplate.emplace_back( "" );      //  4 - unused
	mCssTemplate.emplace_back( "" );      //  5 - string after property before value
	mCssTemplate.emplace_back( ";\n" );   //  6 - string after value
	mCssTemplate.emplace_back( "}" );     //  7 - closing bracket - selector
	mCssTemplate.emplace_back( "\n\n" );  //  8 - space between blocks {...}
	mCssTemplate.emplace_back( "}\n\n" ); //  9 - closing bracket @-rule
	mCssTemplate.emplace_back( "" );      // 10 - unused
	mCssTemplate.emplace_back( "" );      // 11 - before comment
	mCssTemplate.emplace_back( "\n" );    // 12 - after comment
	mCssTemplate.emplace_back( "\n" );    // 13 - after last line @-rule

	// at_rule to parser state map
	mAtRules["page"] = IN_SELECTOR;
	mAtRules["font-face"] = IN_SELECTOR;
	mAtRules["charset"] = IN_VALUE;
	mAtRules["import"] = IN_VALUE;
	mAtRules["namespace"] = IN_VALUE;
	mAtRules["media"] = IN_AT_BLOCK;
	mAtRules["keyframes"] = IN_AT_BLOCK;
	mAtRules["supports"] = IN_AT_BLOCK;
	mAtRules["-moz-keyframes"] = IN_AT_BLOCK;
	mAtRules["-ms-keyframes"] = IN_AT_BLOCK;
	mAtRules["-o-keyframes"] = IN_AT_BLOCK;
	mAtRules["-webkit-keyframes"] = IN_AT_BLOCK;

	// descriptive names for each token type
	mTokenTypeNames.emplace_back( "CHARSET" );
	mTokenTypeNames.emplace_back( "IMPORT" );
	mTokenTypeNames.emplace_back( "NAMESP" );
	mTokenTypeNames.emplace_back( "AT_START" );
	mTokenTypeNames.emplace_back( "AT_END" );
	mTokenTypeNames.emplace_back( "SEL_START" );
	mTokenTypeNames.emplace_back( "SEL_END" );
	mTokenTypeNames.emplace_back( "PROPERTY" );
	mTokenTypeNames.emplace_back( "VALUE" );
	mTokenTypeNames.emplace_back( "COMMENT" );
	mTokenTypeNames.emplace_back( "CSS_END" );

	mCssLevel = CSS30;
}


void Parser::setLevel( const std::string &level )
{
	if( level == CSS10 || level == CSS20 || level == CSS21 || level == CSS30 ) {
		mCssLevel = level;
	}
}


void Parser::resetParser()
{
	mTokenPtr = 0;
	mCharset = "";
	mNamespace = "";
	mSelectorNestLevel = 0;
	mLine = 1;
	mImport.clear();
	mCssTokens.clear();
	mCurSelector.clear();
	mCurAt.clear();
	mCurProperty.clear();
	mCurFunction.clear();
	mCurSubValue.clear();
	mCurString.clear();
	mCurSelector.clear();
	mSelSeparate.clear();
}


std::string Parser::getCharset()
{
	return mCharset;
}

std::string Parser::getNamespace()
{
	return mNamespace;
}

std::vector<std::string> Parser::getImport()
{
	return mImport;
}


Parser::Token Parser::getNextToken( int offset )
{
	if( offset >= 0 && offset < int( mCssTokens.size() ) ) {
		mTokenPtr = offset;
	}

	Token token;
	if( mTokenPtr < int( mCssTokens.size() ) ) {
		token = mCssTokens[mTokenPtr];
		mTokenPtr++;
	}

	return token;
}


std::string Parser::getTypeName( TokenType t )
{
	return mTokenTypeNames[t];
}


void Parser::addToken( TokenType type, const std::string &data, const bool force )
{
	Token temp;
	temp.type = type;
	// temp.pos = mStatePos;
	// temp.line = mStateLine;
	temp.data = type == COMMENT ? data : trim( data );
	mCssTokens.push_back( temp );
	if( type == SEL_START )
		mSelectorNestLevel++;
	if( type == SEL_END )
		mSelectorNestLevel--;
}


// void Parser::log( std::string msg, MessageType type, int iline )
//{
//	Message new_msg;
//	new_msg.m = msg;
//	new_msg.t = type;
//	if( iline == 0 ) {
//		iline = mLine;
//	}
//	if( mLogs.count( mLine ) > 0 ) {
//		for( auto &i : mLogs[mLine] ) {
//			if( i.m == new_msg.m && i.t == new_msg.t ) {
//				return;
//			}
//		}
//	}
//	mLogs[mLine].push_back( new_msg );
//}

std::string Parser::unicode( std::string &str, std::string::size_type &i )
{
	++i;
	std::string add;
	bool        replaced = false;

	while( i < str.length() && ( isHexDigit( str[i] ) || isWhiteSpace( str[i] ) ) && add.length() < 6 ) {
		add += str[i];

		if( isWhiteSpace( str[i] ) ) {
			break;
		}
		i++;
	}

	if( hexdec( add ) > 47 && hexdec( add ) < 58 || hexdec( add ) > 64 && hexdec( add ) < 91 || hexdec( add ) > 96 && hexdec( add ) < 123 ) {
		std::string msg = "Replaced unicode notation: Changed \\" + rtrim_copy( add ) + " to ";
		add = static_cast<int>( hexdec( add ) );
		msg += add;
		CI_LOG_I( msg );
		replaced = true;
	}
	else {
		add = trim( "\\" + add );
	}

	if( isHexDigit( str[i + 1] ) && isWhiteSpace( str[i] ) && !replaced || !isWhiteSpace( str[i] ) ) {
		i--;
	}

	if( 1 ) {
		return add;
	}
	return "";
}


bool Parser::isToken( const std::string &str, std::string::size_type i ) const
{
	return inStringArray( mTokens, str[i] ) && !escaped( str, i );
}


void Parser::explodeSelectors()
{
	mSelSeparate = std::vector<int>();
}


int Parser::seekNoComment( const int key, int move ) const
{
	int go = move > 0 ? 1 : -1;
	for( int i = key + 1; abs( key - i ) - 1 < abs( move ); i += go ) {
		if( i < 0 || i >= mCssTokens.size() ) {
			return -1;
		}
		if( mCssTokens[i].type == COMMENT ) {
			move += 1;
			continue;
		}
		return mCssTokens[i].type;
	}
	// FIXME: control can reach end of non-void function
	return -1;
}


std::string Parser::serialize( const std::string &filename, bool tostdout ) const
{
	if( mCharset.empty() && mNamespace.empty() && mImport.empty() && mCssTokens.empty() ) {
		std::cout << "Warning: empty CSS output!" << std::endl;
	}

	std::ofstream file_output;
	if( !filename.empty() ) {
		file_output.open( filename.c_str(), std::ios::binary );
		if( file_output.bad() ) {
			std::cout << "Error when trying to save the output file!" << std::endl;
			return "";
		}
	}

	std::stringstream output;
	int               lvl = 0;
	// std::string       indent;

	for( int i = 0; i < mCssTokens.size(); ++i ) {
		switch( mCssTokens[i].type ) {
		case CHARSET:
			output << "@charset " << mCssTemplate[5] << mCssTokens[i].data << mCssTemplate[6];
			break;

		case IMPORT:
			output << indent( lvl, mCssTemplate[0] ) << "@import " << mCssTemplate[5] << mCssTokens[i].data << mCssTemplate[6];
			break;

		case NAMESPACE:
			output << "@namespace " << mCssTemplate[5] << mCssTokens[i].data << mCssTemplate[6];
			break;

		case AT_START:
			output << indent( lvl, mCssTemplate[0] ) << mCssTokens[i].data << mCssTemplate[1];
			lvl++;
			break;

		case SEL_START:
			output << indent( lvl, mCssTemplate[0] ) << mCssTokens[i].data << mCssTemplate[3];
			lvl++;
			break;

		case PROPERTY:
			output << indent( lvl, mCssTemplate[0] ) << mCssTokens[i].data << ":" << mCssTemplate[5];
			break;

		case VALUE:
			output << mCssTokens[i].data << mCssTemplate[6];
			break;

		case SEL_END:
			lvl--;
			if( lvl < 0 )
				lvl = 0;
			output << indent( lvl, mCssTemplate[0] ) + mCssTemplate[7];
			if( seekNoComment( i, 1 ) != AT_END )
				output << mCssTemplate[8];
			break;

		case AT_END:
			lvl--;
			if( lvl < 0 )
				lvl = 0;
			output << mCssTemplate[13] << indent( lvl, mCssTemplate[0] ) << mCssTemplate[9];
			break;

		case COMMENT:
			output << mCssTemplate[11] << "/*" << mCssTokens[i].data << "*/" << mCssTemplate[12];
			break;

		case CSS_END:
			break;
		}
	}

	std::string output_string = trim( output.str() );

	if( tostdout ) {
		std::cout << output_string << "\n";
	}
	if( !filename.empty() ) {
		file_output << output_string;
		file_output.close();
	}
	return output_string;
}


void Parser::parseInAtBlock( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from )
{
	if( isToken( css, i ) ) {
		if( css[i] == '/' && charAt( css, i + 1 ) == '*' ) {
			status = IN_COMMENT;
			i += 2;
			from = IN_AT_BLOCK;
		}
		else if( css[i] == '{' ) {
			status = IN_SELECTOR;
			addToken( AT_START, mCurAt );
		}
		else if( css[i] == ',' ) {
			mCurAt = trim( mCurAt ) + ",";
		}
		else if( css[i] == '\\' ) {
			mCurAt += unicode( css, i );
		}
		else /*if((css_input[i] == '(') || (css_input[i] == ':') || (css_input[i] == ')') || (css_input[i] == '.'))*/
		{
			if( !inCharArray( "():/.", css[i] ) ) {
				// Strictly speaking, these are only permitted in @media rules
				CI_LOG_W( "Unexpected symbol '" + std::string( css, i, 1 ) + "' in @-rule" );
			}
			mCurAt += css[i]; /* append tokens after media selector */
		}
	}
	else {
		// Skip excess whitespace
		long long lastpos = mCurAt.length() - 1;
		if( lastpos == -1 || !( ( isWhiteSpace( mCurAt[lastpos] ) || isToken( mCurAt, lastpos ) && mCurAt[lastpos] == ',' ) && isWhiteSpace( css[i] ) ) ) {
			mCurAt += css[i];
		}
	}
}


void Parser::parseInSelector( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, bool &invalid_at, char &str_char, std::string::size_type str_size )
{
	if( isToken( css, i ) ) {
		// if(css_input[i] == '/' && CSSUtils::s_at(css_input,i+1) == '*' &&
		// trim(cur_selector) == "") selector as dep doesn't make any sense here, huh?
		if( css[i] == '/' && charAt( css, i + 1 ) == '*' ) {
			status = IN_COMMENT;
			++i;
			from = IN_SELECTOR;
		}
		else if( css[i] == '@' && trim( mCurSelector ).empty() ) {
			// Check for at-rule
			invalid_at = true;


			for( const auto &rule : mAtRules ) {
				if( toLower( css.substr( i + 1, rule.first.length() ) ) == rule.first ) {
					rule.second == IN_AT_BLOCK ? mCurAt = "@" + rule.first : mCurSelector = "@" + rule.first;
					status = rule.second;
					i += rule.first.length();
					invalid_at = false;
				}
			}

			if( invalid_at ) {
				mCurSelector = "@";
				std::string invalid_at_name;
				for( auto j = i + 1; j < str_size; ++j ) {
					if( !isAlpha( css[j] ) ) {
						return;
					}
					invalid_at_name += css[j];
				}
				CI_LOG_W( "Invalid @-rule: " + invalid_at_name + " (removed)" );
			}
		}
		else if( css[i] == '"' || css[i] == '\'' ) {
			mCurString = css[i];
			status = IN_STRING;
			str_char = css[i];
			from = IN_SELECTOR;
		}
		else if( invalid_at && css[i] == ';' ) {
			invalid_at = false;
			status = IN_SELECTOR;
		}
		else if( css[i] == '{' ) {
			status = IN_PROPERTY;
			addToken( SEL_START, mCurSelector );
		}
		else if( css[i] == '}' ) {
			addToken( AT_END, mCurAt );
			mCurAt = "";
			mCurSelector = "";
			mSelSeparate = std::vector<int>();
		}
		else if( css[i] == ',' ) {
			mCurSelector = trim( mCurSelector ) + ",";
			mSelSeparate.push_back( int( mCurSelector.length() ) );
		}
		else if( css[i] == '\\' ) {
			mCurSelector += unicode( css, i );
		}
		// remove unnecessary universal selector,  FS#147
		else if( !( css[i] == '*' && ( charAt( css, i + 1 ) == '.' || charAt( css, i + 1 ) == '[' || charAt( css, i + 1 ) == ':' || charAt( css, i + 1 ) == '#' ) ) ) {
			mCurSelector += css[i];
		}
	}
	else {
		long long lastpos = mCurSelector.length() - 1;
		if( lastpos == -1 || !( ( isWhiteSpace( mCurSelector[lastpos] ) || isToken( mCurSelector, lastpos ) && mCurSelector[lastpos] == ',' ) && isWhiteSpace( css[i] ) ) ) {
			mCurSelector += css[i];
		}
	}
}


void Parser::parseInProperty( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, bool &invalid_at )
{

	if( isToken( css, i ) ) {
		if( css[i] == ':' || css[i] == '=' && !mCurProperty.empty() ) {
			status = IN_VALUE;
			bool valid = true || Properties::instance().contains( mCurProperty ) && Properties::instance().levels( mCurProperty ).find( mCssLevel, 0 ) != std::string::npos;
			if( valid ) {
				addToken( PROPERTY, mCurProperty );
			}
		}
		else if( css[i] == '/' && charAt( css, i + 1 ) == '*' && mCurProperty == "" ) {
			status = IN_COMMENT;
			++i;
			from = IN_PROPERTY;
		}
		else if( css[i] == '}' ) {
			explodeSelectors();
			status = IN_SELECTOR;
			invalid_at = false;
			addToken( SEL_END, mCurSelector );
			mCurSelector = "";
			mCurProperty = "";
		}
		else if( css[i] == ';' ) {
			mCurProperty = "";
		}
		else if( css[i] == '\\' ) {
			mCurProperty += unicode( css, i );
		}
		else if( css[i] == '*' ) {
			// IE7 and below recognize properties that begin with '*'
			if( mCurProperty.empty() ) {
				mCurProperty += css[i];
				CI_LOG_W( "IE7- hack detected: property name begins with '*'" );
			}
		}
		else {
			CI_LOG_E( "Unexpected character '" + std::string( 1, css[i] ) + "'in property name" );
		}
	}
	else if( !isWhiteSpace( css[i] ) ) {
		if( css[i] == '_' && mCurProperty.empty() ) {
			// IE6 and below recognize properties that begin with '_'
			CI_LOG_W( "IE6 hack detected: property name begins with '_'" );
		}
		// TODO: Check for invalid characters
		mCurProperty += css[i];
	}
	// TODO: Check for whitespace inside property names
}


void Parser::parseInValue( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, bool &invalid_at, char &str_char, bool &pn, std::string::size_type str_size )
{
	pn = ( css[i] == '\n' || css[i] == '\r' ) && propertyIsNext( css, i + 1 ) || i == str_size - 1;
	if( pn ) {
		CI_LOG_W( "Added semicolon to the end of declaration" );
	}
	if( isToken( css, i ) || pn ) {
		if( css[i] == '{' && ( mCurSelector == "@import" || mCurSelector == "@charset" || mCurSelector == "@namespace" ) ) {
			CI_LOG_E( "Unexpected character '" + std::string( 1, css[i] ) + "' in " + mCurSelector );
		}
		if( css[i] == '/' && charAt( css, i + 1 ) == '*' ) {
			status = IN_COMMENT;
			++i;
			from = IN_VALUE;
		}
		else if( css[i] == '"' || css[i] == '\'' || css[i] == '(' && mCurSubValue == "url" ) {
			str_char = css[i] == '(' ? ')' : css[i];
			mCurString = css[i];
			status = IN_STRING;
			from = IN_VALUE;
		}
		else if( css[i] == '(' ) {
			// function call or an open parenthesis in a calc() expression
			// url() is a special case that should have been handled above
			assert( mCurSubValue != "url" );

			// cur_sub_value should contain the name of the function, if any
			mCurSubValue = trim( mCurSubValue + "(" );
			// set current function name and push it onto the stack
			mCurFunction = mCurSubValue;
			mCurFunctionArray.push_back( mCurSubValue );
			mCurSubValueArray.push_back( mCurSubValue );
			mCurSubValue = "";
		}
		else if( css[i] == '\\' ) {
			mCurSubValue += unicode( css, i );
		}
		else if( css[i] == ';' || pn ) {
			if( mCurSelector.substr( 0, 1 ) == "@" && mAtRules.count( mCurSelector.substr( 1 ) ) > 0 && mAtRules[mCurSelector.substr( 1 )] == IN_VALUE ) {
				mCurSubValueArray.push_back( trim( mCurSubValue ) );
				status = IN_SELECTOR;

				if( mCurSelector == "@charset" ) {
					mCharset = mCurSubValueArray[0];
					addToken( CHARSET, mCharset );
				}
				else if( mCurSelector == "@import" ) {
					std::string aimport = buildValue( mCurSubValueArray );
					addToken( IMPORT, aimport );
					mImport.push_back( aimport );
				}
				else if( mCurSelector == "@namespace" ) {
					mNamespace = implode( " ", mCurSubValueArray );
					addToken( NAMESPACE, mNamespace );
				}
				mCurSubValueArray.clear();
				mCurSubValue = "";
				mCurSelector = "";
				mSelSeparate = std::vector<int>();
			}
			else {
				status = IN_PROPERTY;
			}
		}
		else if( css[i] == '!' ) {
			mCurSubValueArray.push_back( trim( mCurSubValue ) );
			mCurSubValue = "!";
		}
		else if( css[i] == ',' || css[i] == ')' ) {
			// store the current subvalue, if any
			mCurSubValue = trim( mCurSubValue );
			if( !mCurSubValue.empty() ) {
				mCurSubValueArray.push_back( mCurSubValue );
				mCurSubValue = "";
			}
			bool drop = false;
			if( css[i] == ')' ) {
				if( mCurFunctionArray.empty() ) {
					// No matching open parenthesis, drop this closing one
					CI_LOG_W( "Unexpected closing parenthesis, dropping" );
					drop = true;
				}
				else {
					// Pop function from the stack
					mCurFunctionArray.pop_back();
					mCurFunction = mCurFunctionArray.empty() ? "" : mCurFunctionArray.back();
				}
			}
			if( !drop ) {
				mCurSubValueArray.emplace_back( 1, css[i] );
			}
		}
		else if( css[i] != '}' ) {
			mCurSubValue += css[i];
		}
		if( ( css[i] == '}' || css[i] == ';' || pn ) && !mCurSelector.empty() ) {
			// End of value: normalize, optimize and store property
			if( mCurAt.empty() ) {
				mCurAt = "standard";
			}

			// Kill all whitespace
			mCurAt = trim( mCurAt );
			mCurSelector = trim( mCurSelector );
			mCurProperty = trim( mCurProperty );
			mCurSubValue = trim( mCurSubValue );

			mCurProperty = toLower( mCurProperty );

			if( !mCurSubValue.empty() ) {
				mCurSubValueArray.push_back( mCurSubValue );
				mCurSubValue = "";
			}

			// Check for leftover open parentheses
			if( !mCurFunctionArray.empty() ) {
				std::vector<std::string>::reverse_iterator rit;
				for( rit = mCurFunctionArray.rbegin(); rit != mCurFunctionArray.rend(); ++rit ) {
					CI_LOG_W( "Closing parenthesis missing for '" + *rit + "', inserting" );
					mCurSubValueArray.emplace_back( ")" );
				}
			}

			// Always add token, even if invalid.
			addToken( VALUE, buildValue( mCurSubValueArray ) );

			// bool valid = Properties::instance().contains( mCurProperty ) && Properties::instance().levels( mCurProperty ).find( mCssLevel, 0 ) != std::string::npos;
			// if( !valid ) {
			//	CI_LOG_W( "Invalid property in " + toUpper( mCssLevel ) + ": " + mCurProperty );
			//}

			// Split multiple selectors here if necessary
			mCurProperty = "";
			mCurSubValueArray.clear();
		}
		if( css[i] == '}' ) {
			explodeSelectors();
			addToken( SEL_END, mCurSelector );
			status = IN_SELECTOR;
			invalid_at = false;
			mCurSelector = "";
		}
	}
	else if( !pn ) {
		mCurSubValue += css[i];

		if( isWhiteSpace( css[i] ) ) {
			if( !trim( mCurSubValue ).empty() ) {
				mCurSubValueArray.push_back( trim( mCurSubValue ) );
			}
			mCurSubValue = "";
		}
	}
}


void Parser::parseInComment( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, std::string &cur_comment )
{

	if( css[i] == '*' && charAt( css, i + 1 ) == '/' ) {
		status = from;
		++i;
		addToken( COMMENT, cur_comment );
		cur_comment = "";
	}
	else {
		cur_comment += css[i];
	}
}


void Parser::parseInString( std::string &css, std::string::size_type &i, ParseStatus &status, ParseStatus &from, char &str_char, bool &str_in_str )
{

	if( str_char == ')' && ( css[i] == '"' || css[i] == '\'' ) && str_in_str == false && !escaped( css, i ) ) {
		str_in_str = true;
	}
	else if( str_char == ')' && ( css[i] == '"' || css[i] == '\'' ) && str_in_str == true && !escaped( css, i ) ) {
		str_in_str = false;
	}
	std::string temp_add;
	temp_add += css[i];
	if( ( css[i] == '\n' || css[i] == '\r' ) && !( css[i - 1] == '\\' && !escaped( css, i - 1 ) ) ) {
		temp_add = "\\A ";
		CI_LOG_W( "Fixed incorrect newline in string" );
	}
	if( !( str_char == ')' && std::string( 1, css[i] ).find_first_of( " \n\t\r\0xb" ) != std::string::npos && !str_in_str ) ) {
		mCurString += temp_add;
	}
	if( css[i] == str_char && !escaped( css, i ) && str_in_str == false ) {
		status = from;
		if( mCurFunction.empty() && mCurString.find_first_of( " \n\t\r\0xb" ) == std::string::npos && mCurProperty != "content" && mCurSubValue != "format" ) {
			// If the string is not inside a function call, contains no whitespace,
			// and the current property is not 'content', it may be safe to remove quotes.
			// TODO: Are there any properties other than 'content' where this is unsafe?
			// TODO: What if the string contains a comma or slash, and the property is a list or shorthand?
			if( str_char == '"' || str_char == '\'' ) {
				// If the string is in double or single quotes, remove them
				// FIXME: once url() is handled separately, this may always be the case.
				mCurString = mCurString.substr( 1, mCurString.length() - 2 );
			}
			else if( mCurString.length() > 3 && ( mCurString[1] == '"' || mCurString[1] == '\'' ) ) /* () */
			{
				mCurString = mCurString[0] + mCurString.substr( 2, mCurString.length() - 4 ) + mCurString[mCurString.length() - 1];
			}
		}
		if( from == IN_VALUE ) {
			mCurSubValue += mCurString;
		}
		else if( from == IN_SELECTOR ) {
			mCurSelector += mCurString;
		}
	}
}


void Parser::parse( std::string css )
{
	resetParser();
	css = findReplace( "\r\n", "\n", css ); // Replace newlines
	css += "\n";
	ParseStatus astatus = IN_SELECTOR;
	ParseStatus afrom = IN_SELECTOR;
	// ParseStatus old_status = IN_SELECTOR;
	// recordPosition( IN_SELECTOR, IN_SELECTOR, css, 0, true );
	std::string cur_comment;

	mCurSubValueArray.clear();
	mCurFunctionArray.clear(); // Stack of nested function calls
	char str_char;
	bool str_in_str = false;
	bool invalid_at = false;
	bool pn = false;

	std::string::size_type str_size = css.length();
	for( std::string::size_type i = 0; i < str_size; ++i ) {
		if( css[i] == '\n' || css[i] == '\r' ) {
			++mLine;
		}


		// record current position for selected state transitions
		// if( old_status != astatus ) {
		//	recordPosition( old_status, astatus, css, i );
		//}
		// old_status = astatus;


		switch( astatus ) {
		/* Case in-at-block */
		case IN_AT_BLOCK:
			parseInAtBlock( css, i, astatus, afrom );
			break;

		/* Case in-selector */
		case IN_SELECTOR:
			parseInSelector( css, i, astatus, afrom, invalid_at, str_char, str_size );
			break;

		/* Case in-property */
		case IN_PROPERTY:
			parseInProperty( css, i, astatus, afrom, invalid_at );
			break;

		/* Case in-value */
		case IN_VALUE:
			parseInValue( css, i, astatus, afrom, invalid_at, str_char, pn, str_size );
			break;

		/* Case in-string */
		case IN_STRING:
			parseInString( css, i, astatus, afrom, str_char, str_in_str );
			break;

		/* Case in-comment */
		case IN_COMMENT:
			parseInComment( css, i, astatus, afrom, cur_comment );
			break;
		}
	}
	// validate that every selector start had its own selector end
	if( mSelectorNestLevel != 0 ) {
		CI_LOG_E( "Unbalanced selector braces in style sheet in line " << mLine );
	}
}


bool Parser::propertyIsNext( std::string str, std::string::size_type pos )
{
	str = str.substr( pos, str.length() - pos );
	pos = str.find_first_of( ':', 0 );
	if( pos == std::string::npos )
		return false;

	str = toLower( trim( str.substr( 0, pos ) ) );
	return Properties::instance().contains( str );
}


// std::vector<std::string> Parser::getParseErrors() const
//{
//	return getLogs( ERROR );
//}
//
//
// std::vector<std::string> Parser::getParseWarnings() const
//{
//	return getLogs( WARNING );
//}
//
//
// std::vector<std::string> Parser::getParseInfo() const
//{
//	return getLogs( INFORMATION );
//}


// std::vector<std::string> Parser::getLogs( MessageType type ) const
//{
//	std::vector<std::string> res;
//	if( !mLogs.empty() ) {
//		for( const auto &log : mLogs ) {
//			for( size_t i = 0; i < log.second.size(); ++i ) {
//				if( log.second[i].t == type ) {
//					res.push_back( std::to_string( log.first ) + ": " + log.second[i].m );
//				}
//			}
//		}
//	}
//	return res;
//}


void Parser::setTokens( const std::vector<Token> &tokens )
{
	mCssTokens.clear();
	for( const auto &token : tokens ) {
		mCssTokens.push_back( token );
	}
}


//// update the position only for selected state transitions
// void Parser::recordPosition( ParseStatus from, ParseStatus to, const std::string &css, std::string::size_type i, bool force )
//{
//	// to reach here old_status must be != new_status
//	bool record = false;
//
//	// any state into a  comment
//	if( to == IN_COMMENT )
//		record = true;
//
//	// start of a property
//	if( from == IN_SELECTOR && to == IN_PROPERTY )
//		record = true;
//	if( from == IN_VALUE && to == IN_PROPERTY )
//		record = true;
//
//	// from properties to values or from @charset, @namespace, and @import into values
//	if( from == IN_AT_BLOCK && to == IN_VALUE )
//		record = true;
//	if( from == IN_PROPERTY && to == IN_VALUE )
//		record = true;
//
//	// starting a new selector
//	if( from == IN_AT_BLOCK && to == IN_SELECTOR )
//		record = true;
//	if( from == IN_VALUE && to == IN_SELECTOR )
//		record = true;
//	if( from == IN_PROPERTY && to == IN_SELECTOR )
//		record = true;
//
//	if( record || force ) {
//		mStateLine = mLine;
//		mStatePos = css.find_first_not_of( " \n\t\r\0xb", i );
//		for( std::string::size_type j = i + i; j <= mStatePos && j < css.length(); ++j ) {
//			if( css[j] == '\n' )
//				mStateLine++;
//		}
//	}
//}

} // namespace css
} // namespace cinder