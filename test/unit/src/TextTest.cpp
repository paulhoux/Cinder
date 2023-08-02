#include "cinder/text/Text.h"
#include "cinder/text/AttrString.h"

#include "cinder/Unicode.h"
#include "cinder/DataSource.h"
#include "cinder/Utilities.h"
#include "cinder/app/Platform.h"
#include "cinder/app/App.h"
#include "catch.hpp"

#include <string>

using namespace ci;
using namespace std;

size_t countRuns( const text::AttrString &as )
{
	text::AttrStringIter iter = as.iterate( text::loadFont( text::systemDefaultFace(), 12 ) );
	size_t runCount = 0;
	while( iter.nextRun() )
		runCount++;

	return runCount;
}

bool verifyAlignmentRuns( const text::AttrString &as, const vector<pair<text::Alignment, size_t>> &expectedRuns )
{
	text::AttrStringIter iter = as.iterate( text::loadFont( text::systemDefaultFace(), 12 ) );
	size_t runCount = 0;
	while( iter.nextRun() ) {
		if( runCount >= expectedRuns.size() )
			return false;
		if( expectedRuns[runCount].first != iter.getAlignment( text::Alignment::DEFAULT ) || expectedRuns[runCount].second != iter.getLengthCh() )
			return false;
		runCount++;
	}

	return runCount == expectedRuns.size();
}

//! receives a vector of Placeholders widths. A negative width is a non-Placeholder length of runs of characters; eg "ABC" is -3
bool verifyPlaceholderRuns( const text::AttrString &as, const vector<int32_t> &expectedRuns )
{
	text::AttrStringIter iter = as.iterate( text::loadFont( text::systemDefaultFace(), 12 ) );
	size_t runCount = 0;
	while( iter.nextRun() ) {
		if( runCount >= expectedRuns.size() )
			return false;
		if( expectedRuns[runCount] > 0 ) { // is a placeholder
			if( ! iter.isPlaceholder() || fabs( expectedRuns[runCount] - iter.getPlaceholder().getWidth() ) > 0.001f )
				return false;
		}
		else {
			if( iter.isPlaceholder() || iter.getLengthCh() != -expectedRuns[runCount] )
				return false;
		}
		runCount++;
	}

	return runCount == expectedRuns.size();
}

TEST_CASE("Text")
{
	SECTION("AttrString")
	{
		text::AttrString as1;
		CHECK( (as1.size() == 0 && as1.empty()) );
		as1 << "hello";
		CHECK( (as1.size() == 5 && ! as1.empty()) );
		CHECK( countRuns( as1 ) == 1 );
		as1.clear();
		CHECK( (as1.size() == 0 && as1.empty()) );
		CHECK( countRuns( as1 ) == 0 );

		text::AttrString as2;
		// single attr
		as2 << text::Leading::mult( 1.2f ) << "Hello";
		CHECK( countRuns( as2 ) == 1 );
		// trailing attr doesn't create extra run
		as2 << text::Leading::mult( 1.3f );
		CHECK( countRuns( as2 ) == 1 );
		// but appending text creates second run
		as2 << " world";
		CHECK( countRuns( as2 ) == 2 );

		// check alignment runs
		text::AttrString as3;
		as3 << "A";
		CHECK( countRuns( as3 ) == 1 );
		CHECK( verifyAlignmentRuns( as3, { { text::Alignment::DEFAULT, 1 } } ) );
		as3 << text::Alignment::CENTER;
		CHECK( verifyAlignmentRuns( as3, { { text::Alignment::DEFAULT, 1 } } ) );
		as3 << "AA";		
		CHECK( verifyAlignmentRuns( as3, { { text::Alignment::DEFAULT, 1 }, { text::Alignment::CENTER, 2 } } ) );
		as3 << text::Alignment::RIGHT << "AAA";
		CHECK( verifyAlignmentRuns( as3, { { text::Alignment::DEFAULT, 1 }, { text::Alignment::CENTER, 2 }, {text::Alignment::RIGHT, 3 } } ) );
		as3 << "A";
		CHECK( verifyAlignmentRuns( as3, { { text::Alignment::DEFAULT, 1 }, { text::Alignment::CENTER, 2 }, {text::Alignment::RIGHT, 4 } } ) );

		// check RunBreaks
		text::AttrString as4;
		as4 << "A" << text::RunBreak() << "B";
		CHECK( countRuns( as4 ) == 2 );
		// trailing RunBreak should not result in an additional Run
		as4 << text::RunBreak();
		CHECK( countRuns( as4 ) == 2 );

		// check Placeholders
		text::AttrString as5; // leading placeholder
		as5 << text::Placeholder{ {111, 222}, "!" } << "ABC";
		CHECK( verifyPlaceholderRuns( as5, { 111, -3 } ) );
		text::AttrString as6; // two consecutive leading placeholders
		as6 << text::Placeholder{ {111, 0}, "!" } << text::Placeholder{ {222, 0}, "!" } << "ABC";
		CHECK( verifyPlaceholderRuns( as6, { 111, 222, -3 } ) ); 
		text::AttrString as7; // midway placeholder
		as7 << "AB" << text::Placeholder{ {111, 0}, "!" } << "ABC";
		CHECK( verifyPlaceholderRuns( as7, { -2, 111, -3 } ) ); 
	}

	SECTION("Placeholders")
	{
		
	}
}