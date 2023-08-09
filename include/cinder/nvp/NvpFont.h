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
#include "cinder/nvp/NvpFace.h"
#include "cinder/nvp/Path.h"
#include "cinder/text/Text.h"

#include <unordered_map>

namespace cinder {
namespace nvp {

class Font {
	FaceRef mFace;
	float   mSize{ 0 };

	static std::unordered_map<uint32_t, bool> sFeatures;

  public:
	Font() = default;

	////!
	// Font( const std::string &name, float size );
	////!
	// Font( const ci::DataSourceRef &src, float size );
	//!
	Font( FaceRef face, float size );

	//!
	explicit operator bool() const { return static_cast<bool>( mFace ); }
	//!
	bool operator==( const Font &other ) const { return mFace == other.mFace && mSize == other.mSize; }
	//!
	bool operator!=( const Font &other ) const { return !( *this == other ); }

	//!
	[[nodiscard]] const FaceRef &getFace() const { return mFace; }
	//! Returns the base path id, pointing to the first glyph of the font. Returns zero if no id was assigned.
	[[nodiscard]] GLuint getId( uint32_t index = 0 ) const { return mFace->getId( index ); }
	//! Returns the size of the font in pixels(?). TODO
	[[nodiscard]] float getSize() const { return mSize; }
	//!
	[[nodiscard]] float getUnits() const;
	//! Returns the ascender height in pixels.
	[[nodiscard]] GLfloat getAscender() const { return mFace->getAscender( mSize ); }
	//! Returns the descender height in pixels. Note: negative value!
	[[nodiscard]] GLfloat getDescender() const { return mFace->getDescender( mSize ); }
	//! Returns the total height in pixels, which usually is the same as ascender and descender combined.
	[[nodiscard]] GLfloat getHeight() const { return mFace->getHeight( mSize ); }
	//! Returns the line gap in pixels.
	[[nodiscard]] GLfloat getLineGap() const { return mFace->getLineGap( mSize ); }

	//! Strokes the provided \a utf32String.
	void stroke( const std::u32string &utf32String, const ci::ColorAf &color, float strokeWidth = 1, const ci::vec2 &offset = ci::vec2( 0 ) ) const { stroke( utf32String.data(), utf32String.size(), color, strokeWidth, offset ); }
	//! Strokes the provided \a utf32String.
	void stroke( const char32_t *data, size_t size, const ci::ColorAf &color, float strokeWidth = 1, const ci::vec2 &offset = ci::vec2( 0 ) ) const;

	//! Fills the provided \a utf32String.
	void fill( const std::u32string &utf32String, const ci::ColorAf &color, const ci::vec2 &offset = ci::vec2( 0 ) ) const { fill( utf32String.data(), utf32String.size(), color, offset ); }
	//! Fills the provided \a utf32String.
	void fill( const char32_t *data, size_t size, const ci::ColorAf &color, const ci::vec2 &offset = ci::vec2( 0 ) ) const;
	//!
	void fill( const text::AttrString &str );

	//! Renders the provided \a utf32String to the stencil buffer only.
	void stencil( const std::u32string &utf32String, const ci::vec2 &offset = ci::vec2( 0 ) ) const { stencil( utf32String.data(), utf32String.size(), offset ); }
	//! Renders the provided \a utf32String to the stencil buffer only.
	void stencil( const char32_t *data, size_t size, const ci::vec2 &offset = ci::vec2( 0 ) ) const;

	//! Strokes the provided \a utf32String using the provided \a transforms.
	void strokeInstanced( const std::vector<GLuint> &ids, const std::vector<glm::mat3x2> &transforms, const ci::ColorA &color, float strokeWidth = 1 ) const;
	//! Fills the provided \a utf32String using the provided \a transforms.
	void fillInstanced( const std::vector<GLuint> &ids, const std::vector<glm::mat3x2> &transforms, const ci::ColorAf &color ) const;

	//! Strokes the provided \a utf32String while placing it on the specified \a path with an optional \a offset in pixels.
	void strokeOnPath( const Path &path, const std::u32string &utf32String, const ci::ColorAf &color, float strokeWidth = 1, float offset = 0, bool flip = false ) const
	{
		strokeOnPath( path.getId(), utf32String, color, strokeWidth, offset, flip );
	}
	//! Strokes the provided \a utf32String while placing it on the specified \a path with an optional \a offset in pixels.
	void strokeOnPath( GLuint pathId, const std::u32string &utf32String, const ci::ColorAf &color, float strokeWidth = 1, float offset = 0, bool flip = false ) const;
	//! Fills the provided \a utf32String while placing it on the specified \a path with an optional \a offset in pixels.
	void fillOnPath( const Path &path, const std::u32string &utf32String, const ci::ColorAf &color, float offset = 0, bool flip = false ) const { fillOnPath( path.getId(), utf32String, color, offset, flip ); }
	//! Fills the provided \a utf32String while placing it on the specified \a path with an optional \a offset in pixels.
	void fillOnPath( GLuint pathId, const std::u32string &utf32String, const ci::ColorAf &color, float offset = 0, bool flip = false ) const;

	//! Calculates the width of the provided \a utf32string in pixels.
	[[nodiscard]] float measureWidth( const std::u32string &utf32String, float advanceScale = 1, float kerningScale = 1 ) const { return measureWidth( utf32String.data(), utf32String.size(), advanceScale, kerningScale ); }
	//! Calculates the width of the provided \a utf32string in pixels.
	[[nodiscard]] float measureWidth( const char32_t *data, size_t size, float advanceScale = 1, float kerningScale = 1 ) const;

	//! Returns the transformations required to place the provided \a utf32String on the specified path.
	[[nodiscard]] std::vector<glm::mat3x2> placeOnPath( GLuint pathId, const std::u32string &utf32String, std::vector<GLuint> &ids, float offset = 0, bool flip = false ) const
	{
		return placeOnPath( pathId, utf32String.data(), utf32String.size(), ids, offset, flip );
	}
	//! Returns the transformations required to place the provided \a utf32String on the specified path.
	[[nodiscard]] std::vector<glm::mat3x2> placeOnPath( GLuint pathId, const char32_t *data, size_t size, std::vector<GLuint> &ids, float offset = 0, bool flip = false ) const;

	//!
	void calcAdvances( const char32_t *data, size_t size, std::vector<GLfloat> &advances, float advanceScale = 1, float kerningScale = 1 ) const;

  private:
	template <typename T>
	static T wrap( T value, T min, T max )
	{
		T range = ( max - min );
		T fractional = ( ( value - min ) / range );
		fractional -= std::floor( fractional );

		return min + ( fractional * range );
	}
};

class CI_API NvpFontProcessor : public text::TypesetProcessor {
	vec2                     mCursor;
	std::vector<GLuint>      mIndices;
	std::vector<glm::mat3x2> mTransforms;

  public:
	bool addLine( text::Alignment justification, vec2 drawOffset, float ascender, float descender, float lineGap, float measuredWidth ) override;
	bool addRun( const text::Font *font, const char32_t *utf32Str, size_t chLen, const std::vector<uint32_t> &clusters, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX,
		float measuredWidth, text::PlaceholderInfo *info ) override;
	void finish() override {}
};

} // namespace nvp
} // namespace cinder