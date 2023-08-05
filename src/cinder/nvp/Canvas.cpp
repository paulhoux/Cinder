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

#include "cinder/nvp/Canvas.h"

#include "cinder/gl/draw.h"
#include "cinder/nvp/Core.h"

namespace cinder {
namespace nvp {

Canvas::Canvas( int samples, int coverageSamples, bool useFloats )
	: mSamples( samples )
	, mCoverageSamples( coverageSamples )
	, mUseFloats( useFloats )
{
}

Canvas::Canvas( int width, int height, int samples, int coverageSamples, bool useFloats )
	: mWidth( width )
	, mHeight( height )
	, mSamples( samples )
	, mCoverageSamples( coverageSamples )
	, mUseFloats( useFloats )
{
}

void Canvas::bind() const
{
	if( !mIsBound ) {
		if( !mFbo ) {
			mCtx = gl::context();
			mFbo = gl::Fbo::create( mWidth, mHeight, getFboFormat() );
		}
		if( mCtx == gl::context() ) {
			mCtx->pushFramebuffer( mFbo );
			mCtx->pushViewport( std::make_pair( ivec2( 0 ), mFbo->getSize() ) );
			mCtx->pushGlslProg( nullptr );
			gl::pushMatrices();
			gl::pushModelMatrix();
			gl::setMatricesWindow( mWidth, mHeight );
			gl::popModelMatrix();
			gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
			gl::matrixLoadfEXT( GL_PROJECTION, value_ptr( gl::getProjectionMatrix() ) );
			gl::clearColor( ColorA( 0, 0, 0, 0 ) );
			gl::clear( GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT );
			gl::clearStencil( 0 );
			gl::stencilMask( ~0 );
			mIsBound = true;
		}
	}
}

void Canvas::unbind() const
{
	if( mIsBound && mCtx == gl::context() ) {
		mIsBound = false;
		gl::popMatrices();
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::matrixLoadfEXT( GL_PROJECTION, value_ptr( gl::getProjectionMatrix() ) );
		mCtx->popGlslProg( true );
		mCtx->popViewport();
		mCtx->popFramebuffer();
	}
}

void Canvas::draw() const
{
	if( mFbo && !mIsBound ) {
		gl::draw( mFbo->getColorTexture() );
	}
}

void Canvas::draw( const Rectf &bounds ) const
{
	if( mFbo && !mIsBound ) {
		gl::draw( mFbo->getColorTexture(), bounds );
	}
}

gl::Texture2dRef Canvas::getTexture( GLenum attachment ) const
{
	if( mFbo )
		return mFbo->getTexture2d( attachment );

	return nullptr;
}

gl::Fbo::Format Canvas::getFboFormat() const
{
	if( mUseFloats ) {
		// Using GL_NEAREST, we try to avoid additional blurring of the final image.
		const auto format = gl::Texture2d::Format().internalFormat( GL_RGBA16F ).dataType( GL_FLOAT ).minFilter( GL_NEAREST ).magFilter( GL_NEAREST );
		return gl::Fbo::Format().samples( mSamples ).coverageSamples( mCoverageSamples ).stencilBuffer().disableDepth().colorTexture( format );
	}

	// Using GL_NEAREST, we try to avoid additional blurring of the final image.
	const auto format = gl::Texture2d::Format()/*.internalFormat( GL_SRGB8_ALPHA8 )*/.minFilter( GL_NEAREST ).magFilter( GL_NEAREST );
	return gl::Fbo::Format().samples( mSamples ).coverageSamples( mCoverageSamples ).stencilBuffer().disableDepth().colorTexture( format );
}

void Canvas::resize( const ivec2 &size )
{
	if( size.x > 0 && size.y > 0 && ( mWidth != size.x || mHeight != size.y ) ) {
		mWidth = size.x;
		mHeight = size.y;

		const bool isBound = mIsBound;
		if( isBound )
			unbind();

		mFbo.reset();

		if( isBound )
			bind();
	}
}

} // namespace nvp
} // namespace cinder
