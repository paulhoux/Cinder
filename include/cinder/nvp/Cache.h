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

//#include "nvp/Face.h"
#include "cinder/Log.h"
#include "cinder/nvp/Core.h"

namespace cinder {
namespace nvp {

class Cache {
	// std::unordered_map<std::string, FaceRef>    mFonts;
	std::unordered_map<Shader::Type, ShaderRef> mShaders;

  public:
	static Cache &get()
	{
		thread_local static Cache instance;
		return instance;
	}

	// static FaceRef loadFace( const std::string &name )
	//{
	//	Cache &self = get();

	//	if( self.mFonts.count( name ) && static_cast<bool>( self.mFonts.at( name ) ) ) {
	//		// CI_LOG_V( "Using cached font for " << name << " (" << std::this_thread::get_id() << ")" );
	//		return self.mFonts.at( name );
	//	}

	//	CI_LOG_V( "Loading font for " << name << " (" << std::this_thread::get_id() << ")" );
	//	auto face = Face::create( name );
	//	self.mFonts.insert_or_assign( name, face );

	//	return face;
	//}

	// static FaceRef loadFace( const ci::DataSourceRef &src )
	//{
	//	Cache &self = get();

	//	const auto file = src->getFilePath().generic_string();
	//	if( self.mFonts.count( file ) && static_cast<bool>( self.mFonts.at( file ) ) ) {
	//		// CI_LOG_V( "Using cached font for " << file << " (" << std::this_thread::get_id() << ")" );
	//		return self.mFonts.at( file );
	//	}

	//	CI_LOG_V( "Loading font for " << file << " (" << std::this_thread::get_id() << ")" );
	//	auto face = Face::create( src );
	//	self.mFonts.insert_or_assign( file, face );

	//	return face;
	//}

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
		// auto &fonts = get().mFonts;
		// for( auto itr = fonts.begin(); itr != fonts.end(); ) {
		//	const auto &item = *itr;
		//	if( item.second.use_count() < 2 ) {
		//		CI_LOG_V( "Removing font " << item.first << " (" << std::this_thread::get_id() << ")" );
		//		itr = fonts.erase( itr );
		//	}
		//	else
		//		++itr;
		// }

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
		// CI_LOG_V( "Removing all fonts (" << std::this_thread::get_id() << ")" );
		// get().mFonts.clear();
		CI_LOG_V( "Removing all shaders (" << std::this_thread::get_id() << ")" );
		get().mShaders.clear();
	}

  private:
	Cache() = default;
};

} // namespace nvp
} // namespace cinder