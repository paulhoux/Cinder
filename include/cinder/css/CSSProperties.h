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

#include <string>
#include <string_view>
#include <unordered_map>

namespace cinder {
namespace css {

static constexpr std::string_view CSS10{ "CSS1.0" };
static constexpr std::string_view CSS20{ "CSS2.0" };
static constexpr std::string_view CSS21{ "CSS2.1" };
static constexpr std::string_view CSS30{ "CSS3.0" };
static constexpr std::string_view SVG10{ "SVG1.0" };

static constexpr std::string_view MIN_CSS10{ "CSS1.0,CSS2.0,CSS2.1,CSS3.0" };
static constexpr std::string_view MIN_CSS20{ "CSS2.0,CSS2.1,CSS3.0" };
static constexpr std::string_view MIN_CSS30{ "CSS3.0" };

class Properties {
  public:
	static Properties &instance()
	{
		static Properties instance;
		return instance;
	}

	bool contains( const std::string &property ) const;

	std::string levels( const std::string &property );

  private:
	Properties();

	std::unordered_map<std::string, std::string> mAllProperties;
};

} // namespace css
} // namespace cinder
