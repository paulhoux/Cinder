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

#include "cinder/nvp/Core.h"
#include "cinder/nvp/Path.h"
#include "cinder/text/Face.h"

typedef struct hb_blob_t hb_blob_t;
// typedef struct hb_buffer_t  hb_buffer_t;
typedef struct hb_feature_t hb_feature_t;
typedef struct hb_font_t    hb_font_t;

typedef struct FT_Vector_ FT_Vector;

namespace cinder {
namespace nvp {

CI_API struct Glyph {
	float                scale{ 1 };
	std::vector<GLubyte> commands;
	std::vector<GLfloat> coords;

	void clear()
	{
		commands.clear();
		coords.clear();
	}
};

using FaceRef = std::shared_ptr<class Face>;

CI_API class Face {
  public:
	////! Loads a system font.
	// static FaceRef create( const std::string &name ) { return std::make_shared<Face>( name ); }
	////! Loads a font from file.
	// static FaceRef create( const DataSourceRef &dataSource ) { return std::make_shared<Face>( dataSource ); }

	static FaceRef create( const text::Face *face ) { return std::make_shared<Face>( face ); }

	Face() = default;

	Face( const Face & ) = delete;
	Face( Face && ) = default;
	Face &operator=( const Face & ) = delete;
	Face &operator=( Face && ) = default;

	////! Creates a font from the specified SYSTEM font \a name.
	// explicit Face( std::string name );
	////! Creates a font from the specified \a source FILE.
	// explicit Face( const DataSourceRef &source );

	explicit Face( const text::Face *face );

	~Face();

	//!
	[[nodiscard]] GLuint getBaseId() const { return mBaseId; }
	//! Returns the base path id, pointing to the first glyph of the font. Returns zero if no id was assigned.
	[[nodiscard]] GLuint getId( uint32_t index = 0 ) const { return mBaseId > 0 ? mBaseId + index : 0; }
	//! Returns the ascender height in pixels.
	[[nodiscard]] GLfloat getAscender( float size = 1 ) const { return mAscender * size; }
	//! Returns the descender height in pixels. Note: negative value!
	[[nodiscard]] GLfloat getDescender( float size = 1 ) const { return mDescender * size; }
	//! Returns the total height in pixels, which usually is the same as ascender and descender combined.
	[[nodiscard]] GLfloat getHeight( float size = 1 ) const { return mHeight * size; }
	//! Returns the line gap in pixels. TODO: better description.
	[[nodiscard]] GLfloat getLineGap( float size = 1 ) const { return mLineGap * size; }
	//!
	[[nodiscard]] GLsizei getNumGlyphs() const { return mNumGlyphs; }
	//!
	[[nodiscard]] const fs::path &getFilePath() const { return mPath; }
	//!
	[[nodiscard]] hb_face_t *getFacePtr() const { return mFacePtr; }
	//!
	[[nodiscard]] hb_font_t *getFontPtr() const { return mFontPtr; }
	//!
	static fs::path findPath( const std::string &face );

	static uint32_t constexpr feature( char name[4] )
	{
		return ( ( static_cast<uint32_t>( name[0] ) & 0xFF ) << 24 ) | ( ( static_cast<uint32_t>( name[1] ) & 0xFF ) << 16 ) | ( ( static_cast<uint32_t>( name[2] ) & 0xFF ) << 8 ) | ( static_cast<uint32_t>( name[3] ) & 0xFF );
	}
	static uint32_t constexpr feature( char c1, char c2, char c3, char c4 )
	{
		return ( ( static_cast<uint32_t>( c1 ) & 0xFF ) << 24 ) | ( ( static_cast<uint32_t>( c2 ) & 0xFF ) << 16 ) | ( ( static_cast<uint32_t>( c3 ) & 0xFF ) << 8 ) | ( static_cast<uint32_t>( c4 ) & 0xFF );
	}

	//!
	static std::vector<hb_feature_t> createFeatures( const std::unordered_map<uint32_t, bool> &features );

	inline static constexpr float BASE_SIZE = 1000;

  private:
	//! Sets the stroke style for all glyphs.
	void setStrokeStyle( float width, JoinStyle joinStyle, CapsStyle capsStyle ) const;
	//!
	void createPaths() const;

	static int moveTo( const FT_Vector *to, void *user );
	static int lineTo( const FT_Vector *to, void *user );
	static int quadTo( const FT_Vector *control, const FT_Vector *to, void *user );
	static int cubicTo( const FT_Vector *control1, const FT_Vector *control2, const FT_Vector *to, void *user );

	static void enumerateSystemFonts();

	static const fs::path &getWindowsPath();

	GLuint      mBaseId{ 0 };       //
	std::string mFace;              // Name of the face.
	GLfloat     mAscender{ -1 };    //
	GLfloat     mDescender{ -1 };   //
	GLfloat     mHeight{ -1 };      //
	GLfloat     mLineGap{ 0 };      //
	GLuint      mNumGlyphs{ 0 };    //
	Rectf       mBounds;            // Bounds of biggest glyph.
	fs::path    mPath;              // Path to font file.
	FT_Face     mFtFace{};          // FreeType face.
	hb_blob_t * mBlobPtr = nullptr; // HarfBuzz blob.
	hb_face_t * mFacePtr = nullptr; // HarfBuzz face.
	hb_font_t * mFontPtr = nullptr; // HarfBuzz font.

	inline static std::unordered_map<std::string, fs::path> sFontFiles{};

	friend class Font;
};

} // namespace nvp
} // namespace cinder