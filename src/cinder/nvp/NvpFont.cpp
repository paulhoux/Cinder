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

#include "cinder/nvp/NvpFont.h"

#include "cinder/gl/scoped.h"
#include "cinder/nvp/Cache.h"

#include <hb-ft.h>
#include <hb.h>

namespace {

class ScopedWorkingDir {
	ci::fs::path mPrevious = ci::fs::current_path();

  public:
	ScopedWorkingDir( const ci::fs::path &path ) { current_path( path ); }
	~ScopedWorkingDir() { current_path( mPrevious ); }

	ScopedWorkingDir( const ScopedWorkingDir & ) = delete;
	ScopedWorkingDir( ScopedWorkingDir && ) = delete;
	ScopedWorkingDir &operator=( const ScopedWorkingDir & ) = delete;
	ScopedWorkingDir &operator=( ScopedWorkingDir && ) = delete;
};

} // namespace

namespace cinder {
namespace nvp {

std::unordered_map<uint32_t, bool> Font::sFeatures = { { HB_TAG( 'k', 'e', 'r', 'n' ), true }, { HB_TAG( 'l', 'i', 'g', 'a' ), true }, { HB_TAG( 'c', 'l', 'i', 'g' ), true }, { HB_TAG( 'c', 'a', 'l', 't' ), true } };

// Font::Font( const std::string &name, float size )
//	: Font( Cache::loadFace( name ), size )
//{
//}
//
// Font::Font( const DataSourceRef &src, float size )
//	: Font( Cache::loadFace( src ), size )
//{
//}

Font::Font( FaceRef face, float size )
	: mFace( std::move( face ) )
	, mSize( size )
{
}

float Font::getUnits() const
{
	return mSize / float( hb_face_get_upem( mFace->getFacePtr() ) );
}

void Font::stroke( const char32_t *data, size_t size, const ColorAf &color, float strokeWidth, const vec2 &offset ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return; // Font not properly loaded.

	// TODO find a more generic way to typeset the text.
	const auto features = Face::createFeatures( sFeatures );

	const auto units = Face::BASE_SIZE / float( hb_face_get_upem( getFace()->getFacePtr() ) );
	// const auto buffer = Text::typeset( *mFace, data, features );

	hb_buffer_t *buffer{}; // TODO!!
	const auto   count = hb_buffer_get_length( buffer );
	const auto   glyphs = hb_buffer_get_glyph_infos( buffer, nullptr );
	const auto   positions = hb_buffer_get_glyph_positions( buffer, nullptr );

	if( count > 0 ) {
		std::vector<GLfloat> advances;
		advances.reserve( count );

		std::vector<uint32_t> indices;
		indices.reserve( count );

		GLfloat cursor = 0;
		for( size_t i = 0; i < count; ++i ) {
			indices.push_back( glyphs[i].codepoint );
			advances.push_back( cursor + positions[i].x_offset * units );
			cursor += positions[i].x_advance * units;
		}

		if( static_cast<GLsizei>( count ) < mFace->getNumGlyphs() ) {
			for( size_t i = 0; i < count; ++i ) {
				gl::pathParameterfNV( mFace->getBaseId() + glyphs[i].codepoint, GL_PATH_STROKE_WIDTH_NV, Face::BASE_SIZE * strokeWidth / mSize );
				gl::pathParameteriNV( mFace->getBaseId() + glyphs[i].codepoint, GL_PATH_JOIN_STYLE_NV, GLint( JoinStyle::ROUND ) );
			}
		}
		else {
			for( GLsizei i = 0; i < mFace->getNumGlyphs(); ++i ) {
				gl::pathParameterfNV( mFace->getBaseId() + i, GL_PATH_STROKE_WIDTH_NV, Face::BASE_SIZE * strokeWidth / mSize );
				gl::pathParameteriNV( mFace->getBaseId() + glyphs[i].codepoint, GL_PATH_JOIN_STYLE_NV, GLint( JoinStyle::ROUND ) );
			}
		}

		gl::ScopedModelMatrix scpModel;
		gl::translate( offset );
		gl::scale( mSize / Face::BASE_SIZE, -mSize / Face::BASE_SIZE );

		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		gl::ScopedBlendPremult scpBlend;
		gl::ScopedColor        scpColor;
		ScopedShader           scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverStrokePathInstancedNV(
			static_cast<GLsizei>( count ), GL_UNSIGNED_INT, indices.data(), mFace->getBaseId(), GL_PATH_STROKE_BOUNDING_BOX_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_TRANSLATE_X_NV, advances.data() );
	}
}

void Font::fill( const char32_t *data, size_t size, const ColorAf &color, const vec2 &offset ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return; // Font not properly loaded.

	// TODO find a more generic way to typeset the text.
	const auto features = Face::createFeatures( sFeatures );

	const auto units = Face::BASE_SIZE / float( hb_face_get_upem( getFace()->getFacePtr() ) );
	// const auto buffer = Text::typeset( *mFace, data, features );

	hb_buffer_t *buffer{}; // TODO!!
	const auto   count = hb_buffer_get_length( buffer );
	const auto   glyphs = hb_buffer_get_glyph_infos( buffer, nullptr );
	const auto   positions = hb_buffer_get_glyph_positions( buffer, nullptr );

	if( count > 0 ) {
		std::vector<GLfloat> advances;
		advances.reserve( count );

		std::vector<uint32_t> indices;
		indices.reserve( count );

		GLfloat cursor = 0;
		for( size_t i = 0; i < count; ++i ) {
			indices.push_back( glyphs[i].codepoint );
			advances.push_back( cursor + positions[i].x_offset * units );
			cursor += positions[i].x_advance * units;
		}

		gl::ScopedModelMatrix scpModel;
		gl::translate( offset );
		gl::scale( mSize / Face::BASE_SIZE, -mSize / Face::BASE_SIZE );

		gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		gl::ScopedBlendPremult scpBlend;
		gl::ScopedColor        scpColor;
		ScopedShader           scpShader( color.premultiplied() );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilThenCoverFillPathInstancedNV( static_cast<GLsizei>( count ), GL_UNSIGNED_INT, indices.data(), mFace->getBaseId(), GL_PATH_FILL_MODE_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_TRANSLATE_X_NV, advances.data() );
	}
}

void Font::stencil( const char32_t *data, size_t size, const ci::vec2 &offset ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return; // Font not properly loaded.

	// TODO find a more generic way to typeset the text.
	const auto features = Face::createFeatures( sFeatures );

	const auto units = Face::BASE_SIZE / float( hb_face_get_upem( getFace()->getFacePtr() ) );
	// const auto buffer = Text::typeset( *mFace, data, features );

	hb_buffer_t *buffer{}; // TODO!!
	const auto   count = hb_buffer_get_length( buffer );
	const auto   glyphs = hb_buffer_get_glyph_infos( buffer, nullptr );
	const auto   positions = hb_buffer_get_glyph_positions( buffer, nullptr );

	if( count > 0 ) {
		std::vector<GLfloat> advances;
		advances.reserve( count );

		std::vector<uint32_t> indices;
		indices.reserve( count );

		GLfloat cursor = 0;
		for( size_t i = 0; i < count; ++i ) {
			indices.push_back( glyphs[i].codepoint );
			advances.push_back( cursor + positions[i].x_offset * units );
			cursor += positions[i].x_advance * units;
		}

		gl::ScopedModelMatrix scpModel;
		gl::translate( offset );
		gl::scale( mSize / Face::BASE_SIZE, -mSize / Face::BASE_SIZE );

		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::stencilFillPathInstancedNV( static_cast<GLsizei>( count ), GL_UNSIGNED_INT, indices.data(), mFace->getBaseId(), GL_COUNT_UP_NV, 0xFF, GL_TRANSLATE_X_NV, advances.data() );
	}
}

void Font::strokeInstanced( const std::vector<GLuint> &ids, const std::vector<glm::mat3x2> &transforms, const ColorA &color, float strokeWidth ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return; // Font not properly loaded.

	assert( ids.size() == transforms.size() );

	if( static_cast<GLsizei>( ids.size() ) < mFace->getNumGlyphs() ) {
		for( const GLuint i : ids ) {
			gl::pathParameterfNV( mFace->getBaseId() + i, GL_PATH_STROKE_WIDTH_NV, Face::BASE_SIZE * strokeWidth / mSize );
			gl::pathParameteriNV( mFace->getBaseId() + i, GL_PATH_JOIN_STYLE_NV, GLint( JoinStyle::ROUND ) );
		}
	}
	else {
		for( GLsizei i = 0; i < mFace->getNumGlyphs(); ++i ) {
			gl::pathParameterfNV( mFace->getBaseId() + i, GL_PATH_STROKE_WIDTH_NV, Face::BASE_SIZE * strokeWidth / mSize );
			gl::pathParameteriNV( mFace->getBaseId() + i, GL_PATH_JOIN_STYLE_NV, GLint( JoinStyle::ROUND ) );
		}
	}

	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	gl::ScopedBlendPremult scpBlend;
	gl::ScopedColor        scpColor;
	ScopedShader           scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverStrokePathInstancedNV( static_cast<GLsizei>( ids.size() ), GL_UNSIGNED_INT, ids.data(), mFace->getBaseId(), GL_PATH_STROKE_BOUNDING_BOX_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV,
		reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

void Font::fillInstanced( const std::vector<GLuint> &ids, const std::vector<glm::mat3x2> &transforms, const ColorAf &color ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return; // Font not properly loaded.

	assert( ids.size() == transforms.size() );

	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	gl::ScopedBlendPremult scpBlend;
	gl::ScopedColor        scpColor;
	ScopedShader           scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathInstancedNV(
		static_cast<GLsizei>( ids.size() ), GL_UNSIGNED_INT, ids.data(), mFace->getBaseId(), GL_PATH_FILL_MODE_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );
}

void Font::strokeOnPath( GLuint pathId, const std::u32string &utf32String, const ColorAf &color, float strokeWidth, float offset, bool flip ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return; // Font not properly loaded.

	std::vector<GLuint> ids;
	const auto          transforms = placeOnPath( pathId, utf32String, ids, offset, flip );

	strokeInstanced( ids, transforms, color, strokeWidth );
}

void Font::fillOnPath( GLuint pathId, const std::u32string &utf32String, const ColorAf &color, float offset, bool flip ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return; // Font not properly loaded.

	std::vector<GLuint> ids;

	const auto transforms = placeOnPath( pathId, utf32String, ids, offset, flip );
	fillInstanced( ids, transforms, color );
}

float Font::measureWidth( const char32_t *data, size_t size, float advanceScale, float kerningScale ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return 0; // Font not properly loaded.

	// TODO find a more generic way to typeset the text.
	const auto features = Face::createFeatures( sFeatures );

	const auto units = mSize / float( hb_face_get_upem( getFace()->getFacePtr() ) );
	// const auto buffer = Text::typeset( *mFace, data, features );

	hb_buffer_t *buffer{}; // TODO!!
	const auto   count = hb_buffer_get_length( buffer );
	const auto   positions = hb_buffer_get_glyph_positions( buffer, nullptr );

	if( count > 0 ) {
		GLfloat cursor = 0;
		for( size_t i = 0; i < count; ++i ) {
			cursor += positions[i].x_advance * units; // TODO: advanceScale and kerningScale
		}

		return cursor;
	}

	return 0;
}

void Font::calcAdvances( const char32_t *data, size_t size, std::vector<GLfloat> &advances, float advanceScale, float kerningScale ) const
{
	if( !mFace || mFace->getHeight() < 0 || size < 1 )
		return; // Font not properly loaded.

	// TODO find a more generic way to typeset the text.
	const auto features = Face::createFeatures( sFeatures );

	const auto units = mSize / float( hb_face_get_upem( getFace()->getFacePtr() ) );
	// const auto buffer = Text::typeset( *mFace, data, features );

	hb_buffer_t *buffer{}; // TODO!!
	const auto   count = hb_buffer_get_length( buffer );
	const auto   positions = hb_buffer_get_glyph_positions( buffer, nullptr );

	advances.clear();
	advances.reserve( count );

	GLfloat cursor = 0;
	for( size_t i = 0; i < count; ++i ) {
		advances.push_back( cursor );
		cursor += positions[i].x_advance * units; // TODO: advanceScale and kerningScale
	}
}

bool NvpTextProcessor::addLine( text::Alignment justification, vec2 drawOffset, float ascender, float descender, float lineGap, float measuredWidth )
{
	mCursor = drawOffset;

	return true;
}

bool NvpTextProcessor::addRun( const text::Font *font, const char32_t *utf32Str, size_t chLen, const std::vector<uint32_t> &clusters, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX,
	float measuredWidth, text::PlaceholderInfo *info )
{
	// Cache the font face.
	auto face = nvp::Cache::loadFace( font->getFace() );

	// Create the font.
	auto fnt = nvp::Font( face, font->getSize() );

	// Create transforms.
	auto scale = font->getSize() / Face::BASE_SIZE;

	std::vector<glm::mat3x2> transforms;
	transforms.reserve( len );
	for( size_t i = 0; i < len; ++i ) {
		transforms.emplace_back( scale, 0, 0, -scale, mCursor.x + penX + glyphPositions[i].x, mCursor.y + glyphPositions[i].y );
	}

	// Draw immediately.
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	gl::ScopedBlendPremult scpBlend;
	gl::ScopedColor        scpColor;
	ScopedShader           scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathInstancedNV(
		static_cast<GLsizei>( len ), GL_UNSIGNED_INT, glyphIndices, face->getBaseId(), GL_PATH_FILL_MODE_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );

	return true;
}

bool NvpTextOnPath::addLine( text::Alignment justification, vec2 drawOffset, float ascender, float descender, float lineGap, float measuredWidth )
{
	return true;
}

bool NvpTextOnPath::addRun( const text::Font *font, const char32_t *utf32Str, size_t chLen, const std::vector<uint32_t> &clusters, const ColorAf &color, size_t len, const uint32_t glyphIndices[], const vec2 glyphPositions[], float penX,
	float measuredWidth, text::PlaceholderInfo *placeholderInfo )
{
	// Cache the font face.
	auto face = nvp::Cache::loadFace( font->getFace() );

	// Create the font.
	auto fnt = nvp::Font( face, font->getSize() );

	// Create transforms.
	auto scale = font->getSize() / Face::BASE_SIZE;


	// if( placeholderInfo ) {
	//	mGlyphLayout->getPlaceholders().push_back( *placeholderInfo );
	//	mGlyphLayout->getLines().back().getRuns().push_back( text::Run( len, font, utf32Str, chLen, clusters, color, glyphIndices, glyphPositions, nullptr, penX, measuredWidth, placeholderInfo ) );
	//}
	// else {
	//	size_t            glyphIdx = 0;
	//	std::vector<vec2> positions( len );
	//	std::vector<vec2> orientations( len );
	//	do {
	//		float positionTime = mPathCalcCache.calcTimeForDistance( penX + mInitialMargin + glyphPositions[glyphIdx].x, false );
	//		positions[glyphIdx] = vec2( 0, 0 ) + mPath.getPosition( positionTime );
	//		float tangentTime = mPathCalcCache.calcTimeForDistance( penX + mInitialMargin + glyphPositions[glyphIdx].x + font->getGlyphMetrics( glyphIndices[glyphIdx] ).width / 2.0f, false );
	//		vec2  tangent = glm::normalize( mPath.getTangent( tangentTime ) );
	//		orientations[glyphIdx] = vec2( -tangent.y, tangent.x );
	//		++glyphIdx;
	//	} while( penX + mInitialMargin + glyphPositions[glyphIdx].x < mPathCalcCache.getLength() && glyphIdx < len );

	//	if( glyphIdx < len ) // if we terminated because we ran out of path instead of glyphs, we're done
	//		mDone = true;
	//	mGlyphLayout->getLines().back().getRuns().push_back( text::Run( glyphIdx, font, utf32Str, chLen, clusters, color, glyphIndices, positions.data(), orientations.data(), 0, measuredWidth, nullptr ) );
	//}

	std::vector<glm::mat3x2> transforms;
	transforms.reserve( len );

	size_t glyphIdx = 0;
	for( ; glyphIdx < len; ++glyphIdx ) {
		float x = mPenX + glyphPositions[glyphIdx].x;
		if( x /*+ font->getGlyphMetrics( glyphIndices[glyphIdx] ).width*/ > mPathCalcCache.getLength() )
			break;
		float positionTime = mPathCalcCache.calcTimeForDistance( x, false );
		vec2  position = mPath.getPosition( positionTime );
		float tangentTime = mPathCalcCache.calcTimeForDistance( x + font->getGlyphMetrics( glyphIndices[glyphIdx] ).width * 0.5f, false );
		vec2  tangent = scale * glm::normalize( mPath.getTangent( tangentTime ) );

		transforms.emplace_back( tangent.x, tangent.y, tangent.y, -tangent.x, position.x, position.y );
	}

	// Instead of line break, continue on path and add a space.
	if( glyphIdx > 0 ) {
		mPenX += glyphPositions[glyphIdx - 1].x;
		mPenX += font->getGlyphMetrics( font->getCharIndex( U'_' ) ).width;
	}

	// Draw immediately.
	gl::ScopedState scpStencil( GL_STENCIL_TEST, GL_TRUE );
	gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
	gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

	gl::ScopedBlendPremult scpBlend;
	gl::ScopedColor        scpColor;
	ScopedShader           scpShader( color.premultiplied() );

	gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
	gl::stencilThenCoverFillPathInstancedNV(
		static_cast<GLsizei>( glyphIdx ), GL_UNSIGNED_INT, glyphIndices, face->getBaseId(), GL_PATH_FILL_MODE_NV, 0xFF, GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV, GL_AFFINE_2D_NV, reinterpret_cast<const GLfloat *>( transforms.data() ) );

	return true;
}

void NvpTextOnPath::finish()
{
	mPenX = mInitialMargin;
}

std::vector<glm::mat3x2> Font::placeOnPath( GLuint pathId, const char32_t *data, size_t size, std::vector<GLuint> &ids, float offset, bool flip ) const
{
	if( !mFace || mFace->getHeight() < 0 )
		return {}; // Font not properly loaded.

	//
	int numSegments = 0;
	gl::getPathParameterivNV( pathId, GL_PATH_COMMAND_COUNT_NV, &numSegments );
	const float pathLength = gl::getPathLengthNV( pathId, 0, numSegments );

	// TODO find a more generic way to typeset the text.
	const auto features = Face::createFeatures( sFeatures );

	const auto units = 1.0f / float( hb_face_get_upem( getFace()->getFacePtr() ) );
	// const auto buffer = Text::typeset( *mFace, data, features );

	hb_buffer_t *buffer{}; // TODO!!
	const auto   count = hb_buffer_get_length( buffer );
	const auto   glyphs = hb_buffer_get_glyph_infos( buffer, nullptr );
	const auto   positions = hb_buffer_get_glyph_positions( buffer, nullptr );

	//
	ids.clear();
	ids.reserve( count );

	std::vector<glm::mat3x2> transforms;
	transforms.reserve( count );

	hb_glyph_extents_t extents;
	vec2               position;
	vec2               tangent;

	GLfloat cursor = 0;
	for( size_t i = 0; i < count; ++i ) {
		ids.push_back( glyphs[i].codepoint );

		hb_font_get_glyph_extents( getFace()->getFontPtr(), glyphs[i].codepoint, &extents );
		const auto anchor = float( extents.x_bearing + extents.width / 2 ) * units; // Rotate around anchor at baseline center of glyph, instead of lower left corner.

		if( flip ) {
			const auto distance = pathLength - ( cursor + offset + anchor * mSize + positions[i].x_offset * mSize * units );
			if( gl::pointAlongPathNV( pathId, 0, numSegments, wrap( distance, 0.0f, pathLength ), &position.x, &position.y, &tangent.x, &tangent.y ) ) {
				tangent = -mSize * normalize( tangent );
				position -= tangent * anchor; // Adjust for rotation anchor.
				tangent /= Face::BASE_SIZE;   // Scale.
				transforms.emplace_back( glm::mat3x2( tangent.x, tangent.y, tangent.y, -tangent.x, position.x, position.y ) );
			}
		}
		else {
			const auto distance = cursor + offset + anchor * mSize + positions[i].x_offset * mSize * units;
			if( gl::pointAlongPathNV( pathId, 0, numSegments, wrap( distance, 0.0f, pathLength ), &position.x, &position.y, &tangent.x, &tangent.y ) ) {
				tangent = mSize * normalize( tangent );
				position -= tangent * anchor; // Adjust for rotation anchor.
				tangent /= Face::BASE_SIZE;   // Scale.
				transforms.emplace_back( glm::mat3x2( tangent.x, tangent.y, tangent.y, -tangent.x, position.x, position.y ) );
			}
		}

		cursor += positions[i].x_advance * mSize * units; // TODO: advanceScale and kerningScale
	}

	return transforms;
}

} // namespace nvp
} // namespace cinder
