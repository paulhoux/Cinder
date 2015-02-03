#include "cinder/evr/SharedTexture.h"

namespace cinder {
	namespace msw {
		namespace video {

			PooledTexture2d::~PooledTexture2d()
			{
				auto pool = mPool.lock();
				if( pool ) {
					CI_LOG_V( "Texture destroyed, returning it to the pool." );

					// Pool still exists.
					pool->FreeUsedTexture( this );

					// Don't actually delete the texture, we're gonna reuse it later. But we do need to notify the gl::context() that we're done using it.
					auto ctx = gl::context();
					if( ctx )
						ctx->textureDeleted( this );
				}
				else {
					CI_LOG_V( "Texture destroyed after the pool was destructed." );

					// Poll no longer exists, use default destructor.
					ci::gl::Texture2d::~Texture2d();
				}
			}

		}
	}
}