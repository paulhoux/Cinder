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

#include "cinder/Log.h"
#include "cinder/nvp/Core.h"
#include "cinder/nvp/NvpFace.h"

namespace cinder {
namespace nvp {

class Cache {
	std::unordered_map<std::string, FaceRef>    mFaces;
	std::unordered_map<Shader::Type, ShaderRef> mShaders;

  public:
	static Cache &get()
	{
		thread_local static Cache instance;
		return instance;
	}

	static FaceRef loadFace( const text::Face *face )
	{
		Cache &self = get();

		const auto &name = face->getFamilyName();

		if( self.mFaces.count( name ) && static_cast<bool>( self.mFaces.at( name ) ) ) {
			// CI_LOG_V( "Using cached face for " << name << " (" << std::this_thread::get_id() << ")" );
			return self.mFaces.at( name );
		}

		CI_LOG_V( "Loading face for " << name << " (" << std::this_thread::get_id() << ")" );

		auto cached = Face::create( face );
		self.mFaces.insert_or_assign( name, cached );

		return cached;
	}

	static FaceRef loadFace( const text::Font *font ) { return loadFace( font->getFace() ); }

	static ShaderRef loadShader( Shader::Type type )
	{
		Cache &self = get();

		if( self.mShaders.count( type ) && static_cast<bool>( self.mShaders.at( type ) ) ) {
			return self.mShaders.at( type );
		}

		CI_LOG_V( "Creating shader (" << std::this_thread::get_id() << ")" );
		auto shader = Shader::create( type );
		self.mShaders.insert_or_assign( type, shader );

		return shader;
	}

	static void clean()
	{
		auto &fonts = get().mFaces;
		for( auto itr = fonts.begin(); itr != fonts.end(); ) {
			const auto &item = *itr;
			if( item.second.use_count() < 2 ) {
				CI_LOG_V( "Removing face " << item.first << " (" << std::this_thread::get_id() << ")" );
				itr = fonts.erase( itr );
			}
			else
				++itr;
		}

		auto &shaders = get().mShaders;
		for( auto itr = shaders.begin(); itr != shaders.end(); ) {
			const auto &item = *itr;
			if( item.second.use_count() < 2 ) {
				CI_LOG_V( "Removing shader (" << std::this_thread::get_id() << ")" );
				itr = shaders.erase( itr );
			}
			else
				++itr;
		}
	}

	static void clear()
	{
		CI_LOG_V( "Removing all faces (" << std::this_thread::get_id() << ")" );
		get().mFaces.clear();
		CI_LOG_V( "Removing all shaders (" << std::this_thread::get_id() << ")" );
		get().mShaders.clear();
	}

  private:
	Cache() = default;
};

} // namespace nvp
} // namespace cinder