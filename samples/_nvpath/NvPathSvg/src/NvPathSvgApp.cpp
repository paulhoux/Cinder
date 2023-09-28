#include "cinder/CanvasUi.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/nvp/NvPath.h"
#include "cinder/svg/Svg.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class SvgRendererNvp : public svg::Renderer {
  public:
	SvgRendererNvp()
		: svg::Renderer()
	{
		mMatrixStack.emplace_back(); // 1.f, 0.f, 0.f, 0.f, -1.f, 0.f, 0.f, 0.f, 1.f ); // Invert Y.

		mMatrixStackContainsIllegal = false;

		mStyleStack.emplace_back();
		mFillStack.emplace_back( Color::black() );
		mStrokeStack.emplace_back();
		mFillOpacityStack.push_back( 1.0f );
		mStrokeOpacityStack.push_back( 1.0f );
		mGroupOpacityStack.push_back( 1.0f );

		mStrokeWidthStack.push_back( 1.0f );
		mFillRuleStack.push_back( 0xFF /* == svg::FILL_RULE_NONZERO */ );
		mLineCapStack.push_back( svg::LineCap::LINE_CAP_BUTT );
		mLineJoinStack.push_back( svg::LineJoin::LINE_JOIN_MITER );
		mMiterLimitStack.push_back( 4.0f );
		mDashArrayStack.emplace_back();
		mDashOffsetStack.push_back( 0.0f );

		pushTextPen( vec2( 0 ) );
		mTextRotationStack.push_back( 0 );
	}
	~SvgRendererNvp() override = default;

	static glm::mat3x2 toMat3x2( const glm::mat3x3 &m ) { return glm::mat3x2{ m[0][0], m[0][1], m[1][0], m[1][1], m[2][0], m[2][1] }; }
	static glm::mat4x4 toMat4x4( const glm::mat3x3 &m )
	{
		return glm::mat4x4{                  //
			m[0][0], m[0][1], 0.0f, m[0][2], //
			m[1][0], m[1][1], 0.0f, m[1][2], //
			0.0f, 0.0f, 1.0f, 0.0f,          //
			m[2][0], m[2][1], 0.0f, m[2][2]
		};
	}

	bool shouldRender() const { return ( !mMatrixStackContainsIllegal ) && ( ( !mFillStack.back().isNone() ) || ( !mStrokeStack.back().isNone() ) ); }

	void start() override
	{
		assert( nullptr == mCtx );

		mCtx = gl::context();

		// Enable stencil buffer testing.
		mCtx->pushBoolState( GL_STENCIL_TEST, GL_TRUE );
		gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
		gl::stencilOp( GL_KEEP, GL_KEEP, GL_ZERO );

		// Enable premultiplied alpha.
		mCtx->pushBoolState( GL_BLEND, GL_TRUE );
		mCtx->pushBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

		// Enable sRGB correct rendering.
		mCtx->pushBoolState( GL_FRAMEBUFFER_SRGB, GL_TRUE );

		// Bind gradient texture.
		const auto &tex = mGradientsCache.getTexture();
		mCtx->pushTextureBinding( tex->getTarget(), tex->getId(), 0 );
	}

	void finish() override
	{
		assert( nullptr != mCtx );

		const auto &tex = mGradientsCache.getTexture();
		mCtx->popTextureBinding( tex->getTarget(), 0 );

		mCtx->popBoolState( GL_FRAMEBUFFER_SRGB );

		mCtx->popBlendFuncSeparate();
		mCtx->popBoolState( GL_BLEND );

		mCtx->popBoolState( GL_STENCIL_TEST );

		mCtx = nullptr;
	}

	void render( const nvp::PathRef &shape ) const
	{
		gl::pushModelView();
		gl::matrixLoadfEXT( GL_MODELVIEW, value_ptr( gl::getModelView() ) );
		gl::matrixMult3x3fNV( GL_MODELVIEW, value_ptr( mMatrixStack.back() ) );

		if( !mFillStack.back().isNone() ) {
			ColorA solidColor = mFillStack.back().getColor();
			solidColor.a *= mFillOpacityStack.back() * mGroupOpacityStack.back();

			const auto type = preparePaint( mFillStack.back(), mFillOpacityStack.back() );

			nvp::ScopedShader scpShader( type );
			scpShader.setColor( solidColor );

			gl::stencilFunc( GL_NOTEQUAL, 0, mFillRuleStack.back() );
			gl::stencilThenCoverFillPathNV( shape->getId(), GL_COUNT_UP_NV, mFillRuleStack.back(), GL_BOUNDING_BOX_NV );
		}

		if( !mStrokeStack.back().isNone() ) {
			ColorA solidColor = mStrokeStack.back().getColor();
			solidColor.a *= mStrokeOpacityStack.back() * mGroupOpacityStack.back();

			const auto type = preparePaint( mStrokeStack.back(), mStrokeOpacityStack.back() );

			nvp::ScopedShader scpShader( type );
			scpShader.setColor( solidColor );

			gl::stencilFunc( GL_NOTEQUAL, 0, 0xFF );
			gl::stencilThenCoverStrokePathNV( shape->getId(), GL_COUNT_UP_NV, 0xFF, GL_BOUNDING_BOX_NV );
		}

		gl::popModelView();
	}

	void pushGroup( const svg::Group &group, float opacity ) override { mGroupOpacityStack.push_back( opacity ); }
	void popGroup() override { mGroupOpacityStack.pop_back(); }
	void drawPath( const svg::Path &path ) override
	{
		if( !shouldRender() )
			return;

		if( mPathCache.count( &path ) )
			return render( mPathCache.at( &path ) );

		auto shape = std::make_shared<nvp::Path>( path.getShape2d() );
		shape->setMiterLimit( mMiterLimitStack.back() );
		shape->setDashPattern( mDashArrayStack.back() );
		shape->setDashOffset( mDashOffsetStack.back() );
		shape->setEndCaps( nvp::toCapsStyle( mLineCapStack.back() ) );
		shape->setJoinStyle( nvp::toJoinStyle( mLineJoinStack.back() ) );
		shape->setStrokeWidth( mStrokeWidthStack.back() );
		mPathCache.insert_or_assign( &path, shape );

		render( shape );
	}
	void drawPolyline( const svg::Polyline &polyline ) override
	{
		if( !shouldRender() )
			return;

		if( mPathCache.count( &polyline ) )
			return render( mPathCache.at( &polyline ) );

		auto shape = std::make_shared<nvp::Path>( polyline.getPolyLine() );
		shape->setMiterLimit( mMiterLimitStack.back() );
		shape->setDashPattern( mDashArrayStack.back() );
		shape->setDashOffset( mDashOffsetStack.back() );
		mPathCache.insert_or_assign( &polyline, shape );

		render( shape );
	}
	void drawPolygon( const svg::Polygon &polygon ) override
	{
		if( !shouldRender() )
			return;

		if( mPathCache.count( &polygon ) )
			return render( mPathCache.at( &polygon ) );

		auto shape = std::make_shared<nvp::Path>( polygon.getPolyLine() );
		shape->setMiterLimit( mMiterLimitStack.back() );
		shape->setDashPattern( mDashArrayStack.back() );
		shape->setDashOffset( mDashOffsetStack.back() );
		mPathCache.insert_or_assign( &polygon, shape );

		render( shape );
	}
	void drawLine( const svg::Line &line ) override
	{
		if( !shouldRender() )
			return;

		if( mPathCache.count( &line ) )
			return render( mPathCache.at( &line ) );

		auto shape = std::make_shared<nvp::Path>( line.getShape() );
		shape->setMiterLimit( mMiterLimitStack.back() );
		shape->setDashPattern( mDashArrayStack.back() );
		shape->setDashOffset( mDashOffsetStack.back() );
		mPathCache.insert_or_assign( &line, shape );

		render( shape );
	}
	void drawRect( const svg::Rect &rect ) override
	{
		if( !shouldRender() )
			return;

		if( mPathCache.count( &rect ) )
			return render( mPathCache.at( &rect ) );

		auto shape = std::make_shared<nvp::Path>( Path2d::rectangle( rect.getRect() ) );
		shape->setMiterLimit( mMiterLimitStack.back() );
		shape->setDashPattern( mDashArrayStack.back() );
		shape->setDashOffset( mDashOffsetStack.back() );
		mPathCache.insert_or_assign( &rect, shape );

		render( shape );
	}
	void drawCircle( const svg::Circle &circle ) override
	{
		if( !shouldRender() )
			return;

		if( mPathCache.count( &circle ) )
			return render( mPathCache.at( &circle ) );

		auto shape = std::make_shared<nvp::Path>( circle.getShape() );
		shape->setMiterLimit( mMiterLimitStack.back() );
		shape->setDashPattern( mDashArrayStack.back() );
		shape->setDashOffset( mDashOffsetStack.back() );
		mPathCache.insert_or_assign( &circle, shape );

		render( shape );
	}
	void drawEllipse( const svg::Ellipse &ellipse ) override
	{
		if( !shouldRender() )
			return;

		if( mPathCache.count( &ellipse ) )
			return render( mPathCache.at( &ellipse ) );

		auto shape = std::make_shared<nvp::Path>( ellipse.getShape() );
		shape->setMiterLimit( mMiterLimitStack.back() );
		shape->setDashPattern( mDashArrayStack.back() );
		shape->setDashOffset( mDashOffsetStack.back() );
		mPathCache.insert_or_assign( &ellipse, shape );

		render( shape );
	}
	void drawImage( const svg::Image &image ) override {}
	void drawTextSpan( const svg::TextSpan & ) override {}
	void pushMatrix( const mat3 &m ) override { mMatrixStack.push_back( mMatrixStack.back() * m ); }
	void popMatrix() override { mMatrixStack.pop_back(); }
	void pushStyle( const svg::Style &style ) override { mStyleStack.push_back( style ); }
	void popStyle() override { mStyleStack.pop_back(); }
	void pushFill( const svg::Paint &fill ) override { mFillStack.push_back( fill ); }
	void popFill() override { mFillStack.pop_back(); }
	void pushStroke( const svg::Paint &stroke ) override { mStrokeStack.push_back( stroke ); }
	void popStroke() override { mStrokeStack.pop_back(); }
	void pushFillOpacity( float opacity ) override { mFillOpacityStack.push_back( opacity ); }
	void popFillOpacity() override { mFillOpacityStack.pop_back(); }
	void pushStrokeOpacity( float opacity ) override { mStrokeOpacityStack.push_back( opacity ); }
	void popStrokeOpacity() override { mStrokeOpacityStack.pop_back(); }
	void pushStrokeWidth( float width ) override { mStrokeWidthStack.push_back( width ); }
	void popStrokeWidth() override { mStrokeWidthStack.pop_back(); }
	void pushFillRule( svg::FillRule rule ) override { mFillRuleStack.push_back( rule == svg::FILL_RULE_EVENODD ? 0x1 : 0xFF ); }
	void popFillRule() override { mFillRuleStack.pop_back(); }
	void pushLineCap( svg::LineCap cap ) override { mLineCapStack.push_back( cap ); }
	void popLineCap() override { mLineCapStack.pop_back(); }
	void pushLineJoin( svg::LineJoin join ) override { mLineJoinStack.push_back( join ); }
	void popLineJoin() override { mLineJoinStack.pop_back(); }
	void pushMiterLimit( float miterLimit ) override { mMiterLimitStack.push_back( miterLimit ); }
	void popMiterLimit() override { mMiterLimitStack.pop_back(); }
	void pushDashArray( const std::vector<float> &dashArray ) override { mDashArrayStack.push_back( dashArray ); }
	void popDashArray() override { mDashArrayStack.pop_back(); }
	void pushDashOffset( float dashOffset ) override { mDashOffsetStack.push_back( dashOffset ); }
	void popDashOffset() override { mDashOffsetStack.pop_back(); }
	void pushTextPen( const vec2 &penPos ) override { mTextPenStack.push_back( penPos ); }
	void popTextPen() override { mTextPenStack.pop_back(); }
	void pushTextRotation( float ) override {}
	void popTextRotation() override {}

	//! Clears the cache.
	void clear() const
	{
		mPathCache.clear();
		mGradientsCache.clear();
	}
	//!
	bool empty() const { return mPathCache.empty(); }
	//! Returns the number of cached paths.
	size_t size() const { return mPathCache.size(); }

  private:
	nvp::Shader::Type preparePaint( const svg::Paint &paint, float opacity = 1 ) const
	{
		if( paint.isLinearGradient() )
			return prepareLinearGradient( paint, opacity );

		if( paint.isRadialGradient() )
			return prepareRadialGradient( paint, opacity );

		return nvp::Shader::Type::SOLID_COLOR;
	}
	nvp::Shader::Type prepareLinearGradient( const svg::Paint &paint, float opacity = 1 ) const
	{
		auto gradient = std::dynamic_pointer_cast<nvp::LinearGradient>( mGradientsCache.at( paint.getId() ) );
		if( !gradient ) {
			gradient = nvp::LinearGradient::create( paint.getId().c_str() );
			gradient->units( paint.mUseObjectBoundingBox ? nvp::GradientUnits::OBJECT_BOUNDING_BOX : nvp::GradientUnits::USER_SPACE_ON_USE ); //
			gradient->transform( paint.getTransform() );
			gradient->from( paint.getCoords0() );
			gradient->to( paint.getCoords1() );
			for( size_t i = 0; i < paint.getNumColors(); ++i )
				gradient->stop( paint.getOffset( i ), paint.getColor( i ) );

			mGradientsCache.set( gradient );
		}

		nvp::ScopedShader scpShader( nvp::Shader::Type::LINEAR_GRADIENT );
		scpShader.setCoords( GLenum( gradient->getUnits() ), mat3{ gradient->getTransform() } );
		scpShader.uniform( "index", mGradientsCache.index( paint.getId() ) );
		scpShader.uniform( "gradTab", 0 );
		scpShader.uniform( "gradStart", vec2( gradient->getX1(), gradient->getY1() ) );
		scpShader.uniform( "gradEnd", vec2( gradient->getX2(), gradient->getY2() ) );
		scpShader.uniform( "opacity", opacity );

		return nvp::Shader::Type::LINEAR_GRADIENT;
	}
	nvp::Shader::Type prepareRadialGradient( const svg::Paint &paint, float opacity = 1 ) const
	{
		auto gradient = std::dynamic_pointer_cast<nvp::RadialGradient>( mGradientsCache.at( paint.getId() ) );
		if( !gradient ) {
			gradient = nvp::RadialGradient::create( paint.getId().c_str() );
			gradient->units( paint.useObjectBoundingBox() ? nvp::GradientUnits::OBJECT_BOUNDING_BOX : nvp::GradientUnits::USER_SPACE_ON_USE ); //
			gradient->transform( paint.getTransform() );
			gradient->center( paint.getCoords0() );
			gradient->radius( paint.getRadius0() );
			gradient->focal( paint.getCoords1(), paint.getRadius1() );
			for( size_t i = 0; i < paint.getNumColors(); ++i )
				gradient->stop( paint.getOffset( i ), paint.getColor( i ) );

			mGradientsCache.set( gradient );
		}

		nvp::ScopedShader scpShader( nvp::Shader::Type::RADIAL_GRADIENT );
		scpShader.setCoords( GLenum( gradient->getUnits() ), mat3{ gradient->getTransform() } );
		scpShader.uniform( "index", mGradientsCache.index( paint.getId() ) );
		scpShader.uniform( "gradTab", 0 );
		scpShader.uniform( "focalToCenter", vec2( gradient->getCx() - gradient->getFx(), gradient->getCy() - gradient->getFy() ) );
		scpShader.uniform( "centerRadius", gradient->getR() );
		scpShader.uniform( "focalRadius", gradient->getFr() );
		scpShader.uniform( "translationPoint", vec2( gradient->getFx(), gradient->getFy() ) );
		scpShader.uniform( "opacity", opacity );

		return nvp::Shader::Type::RADIAL_GRADIENT;
	}

	gl::Context *                   mCtx{ nullptr };
	std::vector<mat3>               mMatrixStack;
	bool                            mMatrixStackContainsIllegal{ false };
	std::vector<svg::Style>         mStyleStack;
	std::vector<svg::Paint>         mFillStack, mStrokeStack;
	std::vector<float>              mFillOpacityStack, mStrokeOpacityStack;
	std::vector<float>              mGroupOpacityStack;
	std::vector<float>              mStrokeWidthStack;
	std::vector<GLuint>             mFillRuleStack;
	std::vector<svg::LineCap>       mLineCapStack;
	std::vector<svg::LineJoin>      mLineJoinStack;
	std::vector<float>              mMiterLimitStack;
	std::vector<std::vector<float>> mDashArrayStack;
	std::vector<float>              mDashOffsetStack;
	std::vector<vec2>               mTextPenStack;
	std::vector<float>              mTextRotationStack;

	using Cache = std::unordered_map<const svg::Node *, nvp::PathRef>;
	mutable Cache          mPathCache;
	mutable nvp::Gradients mGradientsCache;
};

class NvPathSvgApp : public App {
  public:
	void setup() override;
	void update() override;
	void draw() override;
	void resize() override;
	void keyDown( KeyEvent event ) override;
	void fileDrop( FileDropEvent event ) override;

	bool loadSvgFile( const fs::path &file );
	void loadPreviousSvgFile();
	void loadNextSvgFile();

  private:
	CanvasUi       mCanvasUi;
	nvp::Canvas    mCanvas; //{ 32, 16, false };
	nvp::Svg       mSvg;
	svg::DocRef    mDoc;
	SvgRendererNvp mRenderer;
	ci::fs::path   mFilePath;
	size_t         mPathCount = 0;
	bool           mUseSvg = true;
};

void NvPathSvgApp::setup()
{
	disableFrameRate();

	mCanvasUi.connect( getWindow() );
}

void NvPathSvgApp::update()
{
	// Show application name and frame rate in the window title bar.
	std::string name = app::getAppPath().stem().string();

	// Use filename instead if available.
	if( !mFilePath.empty() )
		name = mFilePath.stem().string();

	std::string title;
	title.resize( 255 );
	snprintf( title.data(), title.size(), "%s (%.0f FPS) : %s - %d paths", name.c_str(), static_cast<double>( getAverageFps() ), mUseSvg ? "nvp::Svg" : "svg::Doc", mPathCount );

	getWindow()->setTitle( title );
}

void NvPathSvgApp::draw()
{
	gl::clear( Color::white() );

	{
		nvp::ScopedCanvas     scpCanvas( mCanvas );
		gl::ScopedModelMatrix scpModel( mCanvasUi.getModelMatrix() );

		if( mDoc && !mUseSvg ) {
			gl::ScopedModelMatrix sm;

			// Scale SVG to canvas.
			if( mDoc->getWidth() > 0 && mDoc->getHeight() > 0 ) {
				auto bounds = Area::proportionalFit( Area( mDoc->getBounds() ), mCanvas.getBounds(), true, true );
				auto scale = vec2( bounds.getSize() ) / vec2( mDoc->getSize() );
				auto offset = bounds.getUL();

				gl::translate( offset );
				gl::scale( scale );
			}

			// Send SVG document to our renderer.
			mDoc->render( mRenderer );

			mPathCount = mRenderer.size();
		}
		else if( mUseSvg ) {
			gl::ScopedModelMatrix sm;

			// Scale SVG to canvas.
			if( mSvg.getWidth() > 0 && mSvg.getHeight() > 0 ) {
				auto bounds = Area::proportionalFit( mSvg.getBounds(), mCanvas.getBounds(), true, true );
				auto scale = vec2( bounds.getSize() ) / vec2( mSvg.getSize() );
				auto offset = bounds.getUL();

				gl::translate( offset );
				gl::scale( scale );
			}

			mSvg.draw();

			mPathCount = mSvg.size();
		}
	}

	// Use premultiplied alpha!
	gl::ScopedBlendPremult scpBlend;
	gl::ScopedColor        scpColor( 1, 1, 1 );

	mCanvas.draw();
}

void NvPathSvgApp::resize()
{
	mCanvas.resize( getWindowSize() );
}

void NvPathSvgApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
	case KeyEvent::KEY_f:
		setFullScreen( !isFullScreen() );
		break;
	case KeyEvent::KEY_v:
		gl::enableVerticalSync( !gl::isVerticalSyncEnabled() );
		break;
	case KeyEvent::KEY_LEFT:
		loadPreviousSvgFile();
		break;
	case KeyEvent::KEY_RIGHT:
		loadNextSvgFile();
		break;
	case KeyEvent::KEY_UP:
	case KeyEvent::KEY_DOWN:
		if( !mFilePath.empty() )
			loadSvgFile( mFilePath );
		break;
	case KeyEvent::KEY_ESCAPE:
		if( isFullScreen() )
			setFullScreen( false );
		else
			quit();
		break;
	case KeyEvent::KEY_SPACE:
		mUseSvg = !mUseSvg;
		break;
	default:
		App::keyDown( event );
		break;
	}
}

void NvPathSvgApp::fileDrop( FileDropEvent event )
{
	mFilePath.clear();

	for( const auto &file : event.getFiles() ) {
		if( loadSvgFile( file ) )
			break;
	}
}

bool NvPathSvgApp::loadSvgFile( const fs::path &file )
{
	if( file.extension() == ".svgz" )
		mDoc = svg::Doc::createFromSvgz( loadFile( file ) );
	else if( file.extension() == ".svg" )
		mDoc = svg::Doc::create( loadFile( file ) );
	else
		return false;

	mFilePath = file;

	mCanvasUi.reset();
	mRenderer.clear();

	mSvg = nvp::Svg( mDoc );

	return true;
}

void NvPathSvgApp::loadPreviousSvgFile()
{
	if( mFilePath.empty() )
		return;

	const auto            parent = mFilePath.parent_path();
	std::vector<fs::path> files;

	fs::directory_iterator it( parent );
	while( it != fs::directory_iterator() ) {
		files.push_back( it->path() );
		++it;
	}

	auto itr = std::find_if( files.begin(), files.end(), [this]( const fs::path &file ) { return file == mFilePath; } );
	while( itr != files.begin() && itr != files.end() ) {
		--itr;
		if( loadSvgFile( *itr ) )
			break;
	}
}

void NvPathSvgApp::loadNextSvgFile()
{
	if( mFilePath.empty() )
		return;

	const auto            parent = mFilePath.parent_path();
	std::vector<fs::path> files;

	fs::directory_iterator it( parent );
	while( it != fs::directory_iterator() ) {
		files.push_back( it->path() );
		++it;
	}

	auto itr = std::find_if( files.begin(), files.end(), [this]( const fs::path &file ) { return file == mFilePath; } );
	while( ++itr != files.end() ) {
		if( loadSvgFile( *itr ) )
			break;
	}
}

CINDER_APP( NvPathSvgApp, RendererGl() )
