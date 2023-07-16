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

TEST_CASE("Text")
{
	SECTION("AttrString")
	{
		text::AttrString as1;
		REQUIRE( (as1.size() == 0 && as1.empty()) );
		as1 << "hello";
		REQUIRE( (as1.size() == 5 && ! as1.empty()) );
		as1.clear();
		REQUIRE( (as1.size() == 0 && as1.empty()) );
	}
}