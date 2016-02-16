#pragma once

#include "cinder/Cinder.h"
#include "cinder/Signals.h"
#include "cinder/gl/gl.h"

inline ci::vec3 extractTranslation( const ci::mat4 &m )
{
	return ci::vec3( m[3] );
}

inline ci::vec3 extractScale( const ci::mat4 &m )
{
	ci::vec3 scale;
	scale.x = glm::length( ci::vec3( m[0] ) ) * glm::sign( m[0][0] );
	scale.y = glm::length( ci::vec3( m[1] ) ) * glm::sign( m[1][1] );
	scale.z = glm::length( ci::vec3( m[2] ) ) * glm::sign( m[2][2] );
	return scale;
}

// ------------------------------------------------------------------------------------------------

class Viewport {
public:
	Viewport() {}
	Viewport( const Viewport& ) = default;

	//! Constructs a viewport from the specified origin \a x and \a y, and the size \a w and \a h. 
	Viewport( int x, int y, int w, int h )
		: mOrigin( x, y ), mSize( w, h )
	{
	}
	//! Constructs a viewport by specifying an \a origin and \a size.
	Viewport( const ci::ivec2 &origin, const ci::ivec2 &size )
		: mOrigin( origin ), mSize( size )
	{
	}
	//! Constructs a viewport for use by Cinder.
	Viewport( const std::pair<ci::ivec2, ci::ivec2> &viewport )
		: mOrigin( viewport.first ), mSize( viewport.second )
	{
	}
	//! Constructs a viewport for use by GLM.
	Viewport( const ci::vec4 &viewport )
		: mOrigin( viewport.x, viewport.y ), mSize( viewport.z, viewport.w )
	{
	}

	// Conversions.
	operator ci::vec4() const { return ci::vec4( mOrigin, mSize ); }
	operator std::pair<ci::ivec2, ci::ivec2>() const { return std::pair<ci::ivec2, ci::ivec2>( mOrigin, mSize ); }

	//! Returns the origin of the viewport.
	const ci::ivec2& getOrigin() const { return mOrigin; }
	//! Returns the size of the viewport.
	const ci::ivec2& getSize() const { return mSize; }
	//! Returns the width of the viewport.
	int getWidth() const { return mSize.x; }
	//! Returns the height of the viewport.
	int getHeight() const { return mSize.y; }
	//! Returns \c true if the viewport is vertically flipped.
	bool isFlipped() const { return mSize.y < 0; }

	//! Flips the viewport vertically, adjusting both the origin and the size.
	Viewport& flip() { mSize.y = -mSize.y; mOrigin.y = mOrigin.y - mSize.y; return *this; }
	//! Returns a vertically flipped copy of the viewport.
	Viewport flipped() const { return Viewport( *this ).flip(); }

private:
	ci::ivec2 mOrigin, mSize;
};

// ------------------------------------------------------------------------------------------------

template <class T>
inline void hash_combine( std::size_t &seed, const T &v )
{
	std::hash<T> hasher;
	seed ^= hasher( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

namespace std {
template <> struct hash < ci::gl::TextureBase::Format > {
	inline size_t operator()( const ci::gl::TextureBase::Format &a ) const
	{
		size_t seed = 0;
		hash_combine( seed, a.getTarget() );
		hash_combine( seed, a.getWrapS() );
		hash_combine( seed, a.getWrapT() );
#if ! defined( CINDER_GL_ES )
		hash_combine( seed, a.getWrapR() );
#endif
		hash_combine( seed, a.getMinFilter() );
		hash_combine( seed, a.getMagFilter() );
		hash_combine( seed, a.getCompareMode() );
		hash_combine( seed, a.getCompareFunc() );
		hash_combine( seed, a.getBaseMipmapLevel() );
		hash_combine( seed, a.getMaxMipmapLevel() );
		hash_combine( seed, a.getMaxAnisotropy() );
		hash_combine( seed, a.getInternalFormat() );
		hash_combine( seed, a.getDataType() );
		hash_combine( seed, a.getSwizzleMask()[0] );
		hash_combine( seed, a.getSwizzleMask()[1] );
		hash_combine( seed, a.getSwizzleMask()[2] );
		hash_combine( seed, a.getSwizzleMask()[3] );
		return seed;
	}
};

template <> struct hash < ci::gl::Texture2d::Format > {
	inline size_t operator()( const ci::gl::Texture2d::Format &a ) const
	{
		auto fmt = dynamic_cast<const ci::gl::TextureBase::Format&>( a );

		std::hash<ci::gl::TextureBase::Format> hasher;
		return hasher( fmt );
	}
};

template <> struct hash < ci::gl::Fbo::Format > {
	inline size_t operator()( const ci::gl::Fbo::Format &a ) const
	{
		size_t seed = 0;
		hash_combine( seed, a.getDepthBufferInternalFormat() );
		hash_combine( seed, a.getSamples() );
		hash_combine( seed, a.getCoverageSamples() );
		hash_combine( seed, a.getColorTextureFormat() );
		hash_combine( seed, a.getDepthTextureFormat() );
		hash_combine( seed, a.hasColorTexture() );
		hash_combine( seed, a.hasDepthBuffer() );
		hash_combine( seed, a.hasStencilBuffer() );
		return seed;
	}
};
}

class TextureCache {
public:
	~TextureCache() {}

	static TextureCache& instance() { static TextureCache instance; return instance; }

	ci::gl::Texture2dRef load( const ci::DataSourceRef &src, const ci::gl::Texture2d::Format &format = ci::gl::Texture2d::Format() )
	{
		if( src->isFilePath() )
			return load( src->getFilePath(), format );

		return nullptr;
	}

	ci::gl::Texture2dRef load( const ci::fs::path &path, const ci::gl::Texture2d::Format &format = ci::gl::Texture2d::Format() )
	{
		// Check if file exists and query last write time.
		if( !ci::fs::exists( path ) )
			return nullptr;

		time_t modified = ci::fs::last_write_time( path );

		// Create hash.
		size_t uuid = 0;
		hash_combine( uuid, path.generic_string() );
		hash_combine( uuid, format );
		hash_combine( uuid, modified );

		// Find texture in cache.
		do {
			std::lock_guard<std::mutex> lock( mLock );

			if( mCache.count( uuid ) > 0 ) {
				auto texture = mCache.at( uuid ).lock();
				if( texture ) {
					return texture;
				}

				mCache.erase( uuid );
			}
		} while( false );

		// Load texture and store it in cache.
		try {
			auto texture = ci::gl::Texture2d::create( ci::loadImage( ci::loadFile( path ) ), format );

			std::lock_guard<std::mutex> lock( mLock );
			mCache.insert( std::make_pair( uuid, std::weak_ptr<ci::gl::Texture2d>( texture ) ) );

			return texture;
		}
		catch( const std::exception &exc ) {
			CI_LOG_EXCEPTION( "Failed to load texture.", exc );
			return nullptr;
		}
	}

private:
	TextureCache() {}

	TextureCache( const TextureCache& ) = delete;
	TextureCache& operator=( const TextureCache& ) = delete;

private:
	std::mutex mLock;
	std::unordered_map<size_t, std::weak_ptr<ci::gl::Texture2d>> mCache;
};

// ------------------------------------------------------------------------------------------------

using SpriteSheetRef = std::shared_ptr<class SpriteSheet>;

class SpriteSheet {
public:
	SpriteSheet( const ci::DataSourceRef &src, const ci::ivec2 &cellSize = ci::ivec2( 128 ) )
		: mCellSize( cellSize )
	{
		mTexture = TextureCache::instance().load( src, ci::gl::Texture2d::Format().mipmap().wrap( GL_CLAMP_TO_EDGE ).minFilter( GL_LINEAR_MIPMAP_LINEAR ).target( GL_TEXTURE_RECTANGLE ) );
		mCellsPerRow = mTexture ? mTexture->getWidth() / mCellSize.x : 0;
	}
	SpriteSheet( const ci::fs::path &path, const ci::ivec2 &cellSize = ci::ivec2( 128 ) )
		: mCellSize( cellSize )
	{
		mTexture = TextureCache::instance().load( path, ci::gl::Texture2d::Format().mipmap().wrap( GL_CLAMP_TO_EDGE ).minFilter( GL_LINEAR_MIPMAP_LINEAR ).target( GL_TEXTURE_RECTANGLE ) );
		mCellsPerRow = mTexture ? mTexture->getWidth() / mCellSize.x : 0;
	}

	const ci::ivec2& getCellSize() const { return mCellSize; }

	ci::Area getSrcArea( uint16_t cellIndex )
	{
		uint32_t x = cellIndex % mCellsPerRow;
		uint32_t y = cellIndex / mCellsPerRow;
		return ci::Area( mCellSize * ci::ivec2( x, y ), mCellSize * ci::ivec2( x + 1, y + 1 ) );
	}

	void draw( uint16_t cellIndex, const ci::vec2 &position = ci::vec2( 0 ) )
	{
		ci::Rectf dstRect( ci::vec2( 0 ), mCellSize );
		dstRect.offset( position );

		draw( cellIndex, dstRect );
	}

	void draw( uint16_t cellIndex, const ci::Rectf &dstRect )
	{
		auto ctx = ci::gl::context();
		if( !ctx )
			return;

		if( !mTexture )
			return;

		ci::gl::ScopedTextureBind texture( mTexture );

		void* curGlslProg = nullptr; // ctx->getGlslProg();
		if( !curGlslProg )
			ctx->pushGlslProg( ci::gl::getStockShader( ci::gl::ShaderDef().color().texture( mTexture ) ) );

		ci::Area srcArea = getSrcArea( cellIndex );
		ci::Rectf coords = mTexture->getAreaTexCoords( srcArea );
		ci::gl::drawSolidRect( dstRect, coords.getUpperLeft(), coords.getLowerRight() );

		if( !curGlslProg )
			ctx->popGlslProg();
	}

private:
	ci::gl::Texture2dRef  mTexture;
	ci::ivec2             mCellSize;
	uint32_t          mCellsPerRow;
};

// ------------------------------------------------------------------------------------------------

using UiTransformBaseRef = std::shared_ptr<class UiTransformBase>;
using UiTransformBaseWeakRef = std::weak_ptr<class UiTransformBase>;

class UiTransformBase {
public:
	UiTransformBase() {}
	virtual ~UiTransformBase() {}

	virtual operator ci::mat4() const = 0;

	virtual void invalidate()
	{
		for( auto itr = mDependencies.begin(); itr != mDependencies.end(); ++itr ) {
			auto dependency = itr->lock();
			if( dependency )
				dependency->invalidate();
			else
				itr = mDependencies.erase( itr );
		}
	}

	void addDependency( const UiTransformBaseRef &transform )
	{
		assert( transform );
		mDependencies.push_back( UiTransformBaseWeakRef( transform ) );
	}

	void removeDependency( const UiTransformBaseRef &transform )
	{
		assert( transform );
		mDependencies.erase( std::remove_if( mDependencies.begin(), mDependencies.end(), [transform]( const UiTransformBaseWeakRef& item ) {
			return transform == item.lock(); } ), mDependencies.end() );
	}

protected:
	std::vector<UiTransformBaseWeakRef>  mDependencies;
};

// ------------------------------------------------------------------------------------------------

using UiTransformRef = std::shared_ptr<class UiTransform>;
using UiTransformWeakRef = std::weak_ptr<class UiTransform>;

class UiTransform : public UiTransformBase {
public:
	UiTransform() : mDirty( false ), mPosition( 0, 0, 0 ), mRotation( 0 ), mScale( 1, 1, 1 ) {}
	virtual ~UiTransform() {}

	//! Returns the position.
	const ci::vec3& getPosition() const { return mPosition; }
	//! Returns the rotation around the Z-axis in radians.
	float getRotation() const { return mRotation; }
	//! Returns the scale.
	const ci::vec3& getScale() const { return mScale; }
	//! Returns the anchor.
	const ci::vec3& getAnchor() const { return mAnchor; }

	void setPosition( float x, float y ) { mPosition = ci::vec3( x, y, 0 ); invalidate(); }
	void setPosition( const ci::vec2 &p ) { mPosition.x = p.x; mPosition.y = p.y; invalidate(); }

	void setRotation( float radians ) { mRotation = radians; invalidate(); }

	void setScale( float x, float y ) { mScale = ci::vec3( x, y, 1 ); invalidate(); }
	void setScale( const ci::vec2 &s ) { mScale.x = s.x; mScale.y = s.y; invalidate(); }

	void setAnchor( float x, float y ) { mAnchor = ci::vec3( x, y, 0 ); invalidate(); }
	void setAnchor( const ci::vec2 &p ) { mAnchor.x = p.x; mAnchor.y = p.y; invalidate(); }

	operator ci::mat4() const override
	{
		if( mDirty ) {
			mMatrix = glm::translate( mPosition )
				* glm::rotate( mRotation, ci::vec3( 0, 0, 1 ) )
				* glm::scale( mScale )
				* glm::translate( -mAnchor );

			mDirty = false;
		}

		return mMatrix;
	}

	void invalidate() override
	{
		if( mDirty )
			return;

		mDirty = true;

		UiTransformBase::invalidate();
	}

private:
	mutable ci::mat4        mMatrix;
	mutable bool            mDirty;

	ci::vec3                mPosition;
	float                   mRotation; //! Rotation around Z-axis in radians.
	ci::vec3                mScale;
	ci::vec3                mAnchor;
};

// ------------------------------------------------------------------------------------------------

using UiTransformMultRef = std::shared_ptr<class UiTransformMult>;

class UiTransformMult : public UiTransformBase {
public:
	UiTransformMult( const UiTransformBaseRef &lhs, const UiTransformBaseRef &rhs )
		: mDirty( true ), mLHS( UiTransformBaseWeakRef( lhs ) ), mRHS( UiTransformBaseWeakRef( rhs ) )
	{
	}
	UiTransformMult( const UiTransformBaseRef &rhs )
		: mDirty( true ), mRHS( UiTransformBaseWeakRef( rhs ) )
	{
	}

	operator ci::mat4() const override
	{
		if( mDirty ) {
			auto lhs = mLHS.lock();
			auto rhs = mRHS.lock();

			if( lhs && rhs ) {
				mMatrix = ci::mat4( *lhs ) * ci::mat4( *rhs );
			}
			else if( rhs ) {
				mMatrix = ci::mat4( *rhs );
			}
			else {
				assert( false ); //! Should never happen.
				mMatrix = ci::mat4();
			}

			mDirty = false;
		}

		return mMatrix;
	}

	void invalidate() override
	{
		if( mDirty )
			return;

		mDirty = true;

		UiTransformBase::invalidate();
	}

private:
	mutable ci::mat4        mMatrix;
	mutable bool            mDirty;

	UiTransformBaseWeakRef  mLHS, mRHS;
};

// ------------------------------------------------------------------------------------------------

using UiNodeRef = std::shared_ptr<class UiNode>;
using UiNodeWeakRef = std::weak_ptr<class UiNode>;

class UiNode : public std::enable_shared_from_this<UiNode> {
public:
	static UiNodeRef create() { return std::make_shared<UiNode>(); }
	static UiNodeRef create( const char *name ) { return std::make_shared<UiNode>( name ); }

	UiNode() : UiNode( "UiNode" ) {}
	UiNode( const char *name ) : mName( name ), mSize( 1 )
	{
		mLocalMatrix = std::make_shared<UiTransform>();
		mGlobalMatrix = std::make_shared<UiTransformMult>( mLocalMatrix );
		mLocalMatrix->addDependency( mGlobalMatrix );
	}
	virtual ~UiNode() {}

	virtual void update( double elapsed = 0.0 ) {}
	virtual void draw() {}

	virtual void mouseMove( ci::app::MouseEvent &event ) {}
	virtual void mouseDown( ci::app::MouseEvent &event ) {}
	virtual void mouseDrag( ci::app::MouseEvent &event ) {}
	virtual void mouseUp( ci::app::MouseEvent &event ) {}

	virtual float getWidth() const { return mSize.x; }
	virtual float getHeight() const { return mSize.y; }
	virtual const ci::vec2& getSize() const { return mSize; }
	virtual ci::Rectf getBounds() const { return ci::Rectf( 0, 0, mSize.x, mSize.y ); }

	virtual void setSize( float x, float y ) { mSize.x = x; mSize.y = y; }
	virtual void setSize( const ci::vec2 &size ) { mSize.x = size.x; mSize.y = size.y; }

	//! Returns the position relative to the node's parent coordinate system.
	ci::vec2 getPosition() const { return ci::vec2( mLocalMatrix->getPosition() ); }
	//! Returns the rotation around the Z-axis in radians.
	float getRotation() const { return mLocalMatrix->getRotation(); }
	//! Returns the scale relative to the node's parent coordinate system.
	ci::vec2 getScale() const { return ci::vec2( mLocalMatrix->getScale() ); }
	//! Returns the anchor in units relative to the node's parent coordinate system.
	ci::vec2 getAnchor() const { return ci::vec2( mLocalMatrix->getAnchor() ); }
	//! Returns the position relative to the root's coordinate system.
	ci::vec2 getAbsolutePosition() const { return ci::vec2( extractTranslation( *mGlobalMatrix ) ); }
	//! Returns the scale relative to the root's coordinate system.
	ci::vec2 getAbsoluteScale() const { return ci::vec2( extractScale( *mGlobalMatrix ) ); }

	void setPosition( float x, float y ) { mLocalMatrix->setPosition( x, y ); }
	void setPosition( const ci::vec2 &p ) { mLocalMatrix->setPosition( p ); }

	void setRotation( float radians ) { mLocalMatrix->setRotation( radians ); }

	void setScale( float x, float y ) { mLocalMatrix->setScale( x, y ); }
	void setScale( const ci::vec2 &s ) { mLocalMatrix->setScale( s ); }

	void setAnchor( float x, float y ) { mLocalMatrix->setAnchor( x, y ); }
	void setAnchor( const ci::vec2 &p ) { mLocalMatrix->setAnchor( p ); }

	void setParent( const UiNodeRef &parent )
	{
		auto current = mParent.lock();

		if( parent == current )
			return;

		if( current ) {
			current->removeChild( shared_from_this() );

			current->getModelMatrix()->removeDependency( mGlobalMatrix );
			mLocalMatrix->removeDependency( mGlobalMatrix );
		}

		if( parent ) {
			mParent = UiNodeWeakRef( parent );

			mGlobalMatrix = std::make_shared<UiTransformMult>( parent->getModelMatrix(), mLocalMatrix );
			parent->getModelMatrix()->addDependency( mGlobalMatrix );
			mLocalMatrix->addDependency( mGlobalMatrix );

			parent->addChild( shared_from_this() );
		}
		else {
			mParent = UiNodeWeakRef();

			mGlobalMatrix = std::make_shared<UiTransformMult>( mLocalMatrix );
			mLocalMatrix->addDependency( mGlobalMatrix );
		}
	}

	//! Converts window coordinates (e.g. mouse position) to global coordinates. Assumes z == 0.
	ci::vec3 windowToGlobal( const ci::ivec2 &pt ) const
	{
		auto view = ci::gl::getViewMatrix();
		auto projection = ci::gl::getProjectionMatrix();
		auto viewport = Viewport( ci::gl::getViewport() );

		return windowToGlobal( pt, view, projection, viewport );
	}

	//! Converts window coordinates (e.g. mouse position) to global coordinates. Assumes z == 0.
	ci::vec3 windowToGlobal( const ci::ivec2 &pt, const ci::mat4 &view, const ci::mat4 &projection, const Viewport &viewport ) const
	{
		auto pNear = glm::unProject( ci::vec3( pt, 0 ), view, projection, ( ci::vec4 )viewport.flipped() );
		auto pFar = glm::unProject( ci::vec3( pt, 1 ), view, projection, ( ci::vec4 )viewport.flipped() );

		float t = ( 0 - pNear.z ) / ( pFar.z - pNear.z );
		return glm::mix( pNear, pFar, t );
	}

	//! Converts window coordinates (e.g. mouse position) to local coordinates.
	ci::vec2 windowToLocal( const ci::ivec2 &pt ) const
	{
		auto global = ci::vec4( windowToGlobal( pt ), 1 );
		auto node = glm::inverse( ( ci::mat4 )*mGlobalMatrix ) * global;
		return ci::vec2( node );
	}

	//! Calls \a function on this node, then recursively on each of its children.
	template<class B = UiNode>
	void recursePreorder( void( B::* function )( void ) )
	{
		auto ptr = dynamic_cast<B*>( this );
		if( nullptr != ptr )
			( ptr->*function )( );

		auto nodes( mChildren );
		for( auto itr = nodes.begin(); itr != nodes.end(); ++itr )
			( *itr )->recursePreorder( function );
	}

	//! Calls \a function on this node, then recursively on each of its children.
	template<typename T, class B = UiNode>
	void recursePreorder( void( B::* function )( T ), T arg )
	{
		auto ptr = dynamic_cast<B*>( this );
		if( nullptr != ptr )
			( ptr->*function )( arg );

		auto nodes( mChildren );
		for( auto itr = nodes.begin(); itr != nodes.end(); ++itr )
			( *itr )->recursePreorder<T>( function, arg );
	}

	//! Calls \a function recursively on each of this node's children, then on itself.
	template<class B = UiNode>
	void recursePostorder( void( B::* function )( void ) )
	{
		auto nodes( mChildren );
		for( auto itr = nodes.rbegin(); itr != nodes.rend(); ++itr )
			( *itr )->recursePostorder( function );

		auto ptr = dynamic_cast<B*>( this );
		if( nullptr != ptr )
			( ptr->*function )( );
	}

	//! Calls \a function recursively on each of this node's children, then on itself.
	template<typename T, class B = UiNode>
	void recursePostorder( void( B::* function )( T ), T arg )
	{
		auto nodes( mChildren );
		for( auto itr = nodes.rbegin(); itr != nodes.rend(); ++itr )
			( *itr )->recursePostorder<T>( function, arg );

		auto ptr = dynamic_cast<B*>( this );
		if( nullptr != ptr )
			( ptr->*function )( arg );
	}

protected:
	void addChild( const UiNodeRef &child )
	{
		if( std::find( mChildren.begin(), mChildren.end(), child ) == mChildren.end() )
			mChildren.push_back( child );
	}

	void removeChild( const UiNodeRef &child )
	{
		mChildren.erase( std::remove( mChildren.begin(), mChildren.end(), child ), mChildren.end() );
	}

	UiTransformBaseRef getModelMatrix() { return std::static_pointer_cast<UiTransformBase>( mGlobalMatrix ); }

private:
	std::string            mName;

	UiNodeWeakRef          mParent;
	std::vector<UiNodeRef> mChildren;

	UiTransformRef         mLocalMatrix;  //! Local transformation matrix.
	UiTransformMultRef     mGlobalMatrix; //! Combined matrix of this node and the nodes preceding it.

	ci::vec2               mSize;
};

// ------------------------------------------------------------------------------------------------

using UiButtonRef = std::shared_ptr<class UiButton>;

class UiButton : public UiNode {
public:
	using State = enum { DEFAULT, DOWN, OVER, DISABLED };

public:
	static UiButtonRef create() { return std::make_shared<UiButton>(); }
	static UiButtonRef create( const char *name ) { return std::make_shared<UiButton>( name ); }

	UiButton() : UiButton( "" ) {}
	UiButton( const char *name )
		: UiNode( name ), mCellIndices( 4, 0 )
		, mIsEnabled( true ), mIsClickable( true ), mIsOn( false ), mIsOver( false ), mIsDown( false ), mIsToggle( false )
	{
	}

	//! Allows you to set a \a callback that is called when the user presses the button.
	ci::signals::Connection pressed( const std::function<void( const UiButtonRef& )> &callback )
	{
		return mPressed.connect( callback );
	}
	//! Allows you to set a \a callback that is called when the user releases the button.
	ci::signals::Connection released( const std::function<void( const UiButtonRef& )> &callback )
	{
		return mReleased.connect( callback );
	}
	//! Allows you to set a \a callback that is called when the button is toggled on.
	ci::signals::Connection toggledOn( const std::function<void( const UiButtonRef& )> &callback )
	{
		return mToggledOn.connect( callback );
	}
	//! Allows you to set a \a callback that is called when the button is toggled off.
	ci::signals::Connection toggledOff( const std::function<void( const UiButtonRef& )> &callback )
	{
		return mToggledOff.connect( callback );
	}

	void setSprites( const SpriteSheetRef &sprites, uint32_t defaultIdx, uint32_t overIdx, uint32_t downIdx )
	{
		assert( sprites );

		mSprites = sprites;
		mCellIndices[DEFAULT] = defaultIdx;
		mCellIndices[DOWN] = downIdx;
		mCellIndices[OVER] = overIdx;
		mCellIndices[DISABLED] = defaultIdx;

		setSize( sprites->getCellSize() );
	}

	void setToggle( bool toggle = true ) { mIsToggle = toggle; }
	void setClickable( bool clickable = true ) { mIsClickable = clickable; }

	void toggly()
	{
		toggle( !mIsOn );
	}

	void toggle( bool on )
	{
		if( mIsOn != on ) {
			mIsOn = on;

			if( mIsOn )
				mToggledOn.emit( std::static_pointer_cast<UiButton>( shared_from_this() ) );
			else
				mToggledOff.emit( std::static_pointer_cast<UiButton>( shared_from_this() ) );
		}
	}

	void draw() override
	{
		if( !mSprites )
			return;

		mSprites->draw( mCellIndices[getState()], getAbsolutePosition() );
	}

	void mouseMove( ci::app::MouseEvent &event ) override
	{
		if( !mIsClickable )
			return;

		auto pt = windowToLocal( event.getPos() );
		mIsOver = getBounds().contains( pt );
	}

	void mouseDown( ci::app::MouseEvent &event ) override
	{
		if( !mIsClickable )
			return;

		if( mIsOver ) {
			mIsDown = true;

			mPressed.emit( std::static_pointer_cast<UiButton>( shared_from_this() ) );
		}
	}

	void mouseDrag( ci::app::MouseEvent &event ) override
	{
		if( !mIsClickable )
			return;

		auto pt = windowToLocal( event.getPos() );
		mIsOver = getBounds().contains( pt );
	}

	void mouseUp( ci::app::MouseEvent &event ) override
	{
		if( mIsOver && mIsDown ) {
			mReleased.emit( std::static_pointer_cast<UiButton>( shared_from_this() ) );

			if( mIsToggle ) {
				toggly();
			}
		}

		mIsDown = false;
	}

protected:
	State getState()
	{
		State state;

		if( !mIsEnabled ) {
			state = DISABLED;
		}
		else if( mIsOn ) {
			if( mIsDown )
				state = OVER;
			else if( mIsOver )
				state = DOWN;
			else
				state = DOWN;
		}
		else {
			if( mIsDown )
				state = OVER;
			else if( mIsOver )
				state = DEFAULT;
			else
				state = DEFAULT;
		}

		return state;
	}

private:
	SpriteSheetRef         mSprites;
	std::vector<uint16_t>  mCellIndices;

	ci::signals::Signal<void( const UiButtonRef& )>  mPressed;
	ci::signals::Signal<void( const UiButtonRef& )>  mReleased;
	ci::signals::Signal<void( const UiButtonRef& )>  mToggledOn;
	ci::signals::Signal<void( const UiButtonRef& )>  mToggledOff;

	bool                   mIsEnabled;
	bool                   mIsClickable;

	bool                   mIsOn;
	bool                   mIsOver;
	bool                   mIsDown;
	bool                   mIsToggle;
};