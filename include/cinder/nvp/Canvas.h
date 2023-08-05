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

#include "cinder/gl/Context.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/GlslProg.h"

namespace cinder {
namespace nvp {

using CanvasRef = std::shared_ptr<class Canvas>;

//! Sets up a canvas to render NvPath graphics to.
class Canvas {
  public:
	Canvas( int samples = 8, int coverageSamples = 16, bool useFloats = false );
	Canvas( int width, int height, int samples = 8, int coverageSamples = 16, bool useFloats = false );

	explicit Canvas( const ivec2 &size, int samples = 8, int coverageSamples = 16, bool useFloats = false )
		: Canvas( size.x, size.y, samples, useFloats )
	{
	}

	//! Binds the canvas. Please consider using ScopedCanvas instead.
	void bind() const;
	//! Unbinds the canvas. Please consider using ScopedCanvas instead.
	void unbind() const;
	//! Returns whether the canvas is currently bound.
	bool isBound() const { return mIsBound; }

	//! Draws the canvas.
	void draw() const;
	//! Draws the canvas.
	void draw( const Rectf &bounds ) const;

	//! Returns the width of the canvas in pixels.
	int32_t getWidth() const { return mWidth; }
	//! Returns the height of the canvas in pixels.
	int32_t getHeight() const { return mHeight; }
	//! Returns the size of the canvas in pixels.
	ivec2 getSize() const { return { mWidth, mHeight }; }
	//! Returns the bounds of the canvas in pixels.
	Area getBounds() const { return { 0, 0, mWidth, mHeight }; }

	//! Returns the number of samples used per pixel.
	int getSamples() const { return getFboFormat().getSamples(); }

	//! Returns the canvas texture if it exists, otherwise returns nullptr.
	gl::Texture2dRef getTexture( GLenum attachment = GL_COLOR_ATTACHMENT0 ) const;
	//! Returns the frame buffer format for this canvas.
	gl::Fbo::Format getFboFormat() const;

	//! Sets the size of the canvas in pixels.
	void resize( const ivec2 &size );

  private:
	int32_t              mWidth{ 640 };          //
	int32_t              mHeight{ 480 };         //
	int                  mSamples{ 8 };          //
	int                  mCoverageSamples{ 16 }; //
	bool                 mUseFloats{ false };    //
	mutable gl::Context *mCtx = nullptr;         //
	mutable gl::FboRef   mFbo;                   //
	mutable bool         mIsBound = false;       //
};

class ScopedCanvas {
	const Canvas &mCanvas;

  public:
	explicit ScopedCanvas( const Canvas &canvas )
		: mCanvas( canvas )
	{
		mCanvas.bind();
	}
	~ScopedCanvas() { mCanvas.unbind(); }

	ScopedCanvas( const ScopedCanvas & ) = delete;
	ScopedCanvas( ScopedCanvas && ) = delete;
	ScopedCanvas &operator=( const ScopedCanvas & ) = delete;
	ScopedCanvas &operator=( ScopedCanvas && ) = delete;
};

} // namespace nvp
} // namespace cinder