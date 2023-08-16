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

#include "cinder/nvp/NvpFace.h"
#include "cinder/Cinder.h"
#include "cinder/Log.h"
#include "cinder/Unicode.h"
#include "cinder/Utilities.h"
#include "cinder/nvp/Primitives.h"

#include <freetype/ftoutln.h>

#include <hb-ft.h>
#include <hb.h>

#if defined( CINDER_MSW )
#include <cinder/msw/CinderMsw.h>
#endif

namespace {

class ScopedWorkingDir {
	ci::fs::path mPrevious = ci::fs::current_path();

  public:
	explicit ScopedWorkingDir( const ci::fs::path &path ) { current_path( path ); }
	~ScopedWorkingDir() { current_path( mPrevious ); }

	ScopedWorkingDir( const ScopedWorkingDir & ) = delete;
	ScopedWorkingDir( ScopedWorkingDir && ) = delete;
	ScopedWorkingDir &operator=( const ScopedWorkingDir & ) = delete;
	ScopedWorkingDir &operator=( ScopedWorkingDir && ) = delete;
};

} // namespace

namespace cinder {
namespace nvp {

// Face::Face( std::string name )
//	: mFace( std::move( name ) )
//{
//	mPath = canonical( findPath( mFace ) );
//
//	FT_Error error = FT_New_Face( FreeTypeLibrary::ref(), mPath.string().c_str(), 0, &mFtFace );
//	if( error == FT_Err_Ok ) {
//		bool hasVariations = ( mFtFace->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS ) != 0;
//
//		mFacePtr = hb_ft_face_create_referenced( mFtFace );
//		mFontPtr = hb_font_create( mFacePtr );
//
//		mNumGlyphs = hb_face_get_glyph_count( mFacePtr );
//		mBaseId = gl::genPathsNV( mNumGlyphs );
//
//		createPaths();
//
//		hb_font_extents_t extents;
//		hb_font_get_h_extents( mFontPtr, &extents );
//
//		const auto units = 1.0f / float( hb_face_get_upem( mFacePtr ) );
//		mAscender = float( extents.ascender ) * units;
//		mDescender = float( extents.descender ) * units;
//		mHeight = float( extents.ascender - extents.descender ) * units;
//	}
//}
//
// Face::Face( const DataSourceRef &source )
//	: mFace( source->getFilePath().stem().string() )
//{
//	mPath = canonical( source->getFilePath() );
//
//	FT_Error error = FT_New_Face( FreeTypeLibrary::ref(), mPath.string().c_str(), 0, &mFtFace );
//	if( error == FT_Err_Ok ) {
//		bool hasVariations = ( mFtFace->face_flags & FT_FACE_FLAG_MULTIPLE_MASTERS ) != 0;
//
//		mFacePtr = hb_ft_face_create_referenced( mFtFace );
//		mFontPtr = hb_font_create( mFacePtr );
//
//		mNumGlyphs = hb_face_get_glyph_count( mFacePtr );
//		mBaseId = gl::genPathsNV( mNumGlyphs );
//
//		createPaths();
//
//		hb_font_extents_t extents;
//		hb_font_get_h_extents( mFontPtr, &extents );
//
//		const auto units = 1.0f / float( hb_face_get_upem( mFacePtr ) );
//		mAscender = float( extents.ascender ) * units;
//		mDescender = float( extents.descender ) * units;
//		mHeight = float( extents.ascender - extents.descender ) * units;
//	}
//}

Face::Face( const text::Face *face )
{
	mFace = face->getFamilyName();
	mPath = canonical( face->getFilePath() );

	mFtFace = face->getFtFace();
	mFacePtr = hb_ft_face_create_referenced( mFtFace );
	mFontPtr = hb_font_create( mFacePtr );

	mNumGlyphs = hb_face_get_glyph_count( mFacePtr );
	mBaseId = gl::genPathsNV( mNumGlyphs );

	createPaths();

	hb_font_extents_t extents;
	hb_font_get_h_extents( mFontPtr, &extents );

	const auto units = 1.0f / float( hb_face_get_upem( mFacePtr ) );
	mAscender = float( extents.ascender ) * units;
	mDescender = float( extents.descender ) * units;
	mHeight = float( extents.ascender - extents.descender ) * units;
}

Face::~Face()
{
	if( mBaseId > 0 )
		gl::deletePathsNV( mBaseId, mNumGlyphs );

	hb_font_destroy( mFontPtr );
	hb_face_destroy( mFacePtr );
	hb_blob_destroy( mBlobPtr );

	// FT_Done_Face( mFtFace );
}

fs::path Face::findPath( const std::string &face )
{
	if( sFontFiles.empty() )
		enumerateSystemFonts();

	if( sFontFiles.count( face ) )
		return sFontFiles.at( face );

	return {};
}

std::vector<hb_feature_t> Face::createFeatures( const std::unordered_map<uint32_t, bool> &features )
{
	std::vector<hb_feature_t> result;
	for( const auto &feature : features )
		result.push_back( { feature.first, static_cast<uint32_t>( feature.second ), HB_FEATURE_GLOBAL_START, HB_FEATURE_GLOBAL_END } );

	return result;
}

void Face::setStrokeStyle( float width, JoinStyle joinStyle, CapsStyle capsStyle ) const
{
	for( GLsizei i = 0; i < mNumGlyphs; ++i ) {
		gl::pathParameteriNV( mBaseId + i, GL_PATH_STROKE_WIDTH_NV, width );
		gl::pathParameteriNV( mBaseId + i, GL_PATH_JOIN_STYLE_NV, GLint( joinStyle ) );
		gl::pathParameteriNV( mBaseId + i, GL_PATH_END_CAPS_NV, GLint( capsStyle ) );
	}
}

void Face::createPaths() const
{
	FT_Outline_Funcs functions;
	functions.move_to = moveTo;
	functions.line_to = lineTo;
	functions.conic_to = quadTo;
	functions.cubic_to = cubicTo;
	functions.shift = 0;
	functions.delta = 0;

	Glyph glyph;
	glyph.scale = BASE_SIZE / float( mFtFace->units_per_EM );

	for( FT_UInt i = 0; i < mNumGlyphs; ++i ) {
		FT_Load_Glyph( mFtFace, i, FT_LOAD_NO_HINTING | FT_LOAD_NO_SCALE | FT_LOAD_NO_BITMAP );
		FT_Outline_Decompose( &mFtFace->glyph->outline, &functions, &glyph );

		gl::pathCommandsNV( mBaseId + i, GLsizei( glyph.commands.size() ), glyph.commands.data(), GLsizei( glyph.coords.size() ), GL_FLOAT, glyph.coords.data() );

		glyph.clear();
	}
}

int Face::moveTo( const FT_Vector *to, void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );
	glyph.commands.push_back( GL_MOVE_TO_NV );
	glyph.coords.push_back( to->x * glyph.scale );
	glyph.coords.push_back( to->y * glyph.scale );
	return 0;
}

int Face::lineTo( const FT_Vector *to, void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );
	glyph.commands.push_back( GL_LINE_TO_NV );
	glyph.coords.push_back( to->x * glyph.scale );
	glyph.coords.push_back( to->y * glyph.scale );
	return 0;
}

int Face::quadTo( const FT_Vector *control, const FT_Vector *to, void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );
	glyph.commands.push_back( GL_QUADRATIC_CURVE_TO_NV );
	glyph.coords.push_back( control->x * glyph.scale );
	glyph.coords.push_back( control->y * glyph.scale );
	glyph.coords.push_back( to->x * glyph.scale );
	glyph.coords.push_back( to->y * glyph.scale );
	return 0;
}

int Face::cubicTo( const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user )
{
	auto &glyph = *static_cast<Glyph *>( user );
	glyph.commands.push_back( GL_CUBIC_CURVE_TO_NV );
	glyph.coords.push_back( control1->x * glyph.scale );
	glyph.coords.push_back( control1->y * glyph.scale );
	glyph.coords.push_back( control2->x * glyph.scale );
	glyph.coords.push_back( control2->y * glyph.scale );
	glyph.coords.push_back( to->x * glyph.scale );
	glyph.coords.push_back( to->y * glyph.scale );
	return 0;
}

void Face::enumerateSystemFonts()
{
#if defined( CINDER_MSW )
	static const auto FONT_REGISTRY_PATH = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Fonts";

	HKEY    hKey;
	LSTATUS result = ::RegOpenKeyEx( HKEY_LOCAL_MACHINE, FONT_REGISTRY_PATH, 0, KEY_READ, &hKey );
	if( result != ERROR_SUCCESS )
		return;

	DWORD maxValueNameSize;
	DWORD maxValueDataSize;
	result = ::RegQueryInfoKey( hKey, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &maxValueNameSize, &maxValueDataSize, nullptr, nullptr );
	if( result != ERROR_SUCCESS )
		return;

	DWORD valueIndex = 0;
	DWORD valueNameSize;
	DWORD valueDataSize;
	DWORD valueType;
	auto  valueName = new CHAR[maxValueNameSize];
	auto  valueData = new CHAR[maxValueDataSize];

	do {
		valueDataSize = maxValueDataSize;
		valueNameSize = maxValueNameSize;

		result = RegEnumValueA( hKey, valueIndex++, valueName, &valueNameSize, nullptr, &valueType, reinterpret_cast<BYTE *>( valueData ), &valueDataSize );
		if( result != ERROR_SUCCESS || valueType != REG_SZ )
			continue;

		std::string nameUtf8( valueName, valueNameSize );
		std::string name = trim( nameUtf8 );
		if( auto n = name.find_first_of( '(' ); n != std::string::npos )
			name = trim( name.substr( 0, n ) );

		std::string pathUtf8( valueData, valueDataSize );
		fs::path    path = trim( pathUtf8 );

		if( !path.has_parent_path() )
			sFontFiles.insert_or_assign( name, getWindowsPath() / "Fonts" / path );
		else
			sFontFiles.insert_or_assign( name, path );
	} while( result != ERROR_NO_MORE_ITEMS );

	delete[] valueName;
	delete[] valueData;

	RegCloseKey( hKey );
#endif
}

const fs::path &Face::getWindowsPath()
{
	static fs::path sWindowsPath;
	if( sWindowsPath.empty() ) {
		CHAR winDir[MAX_PATH];
		UINT length = GetWindowsDirectoryA( winDir, MAX_PATH );
		sWindowsPath = fs::path( std::string( winDir, length ) );
	}
	return sWindowsPath;
}

} // namespace nvp
} // namespace cinder