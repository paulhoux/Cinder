#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/Path2d.h"
#include "cinder/gl/gl.h"
#include "cinder/CinderImGui.h"
#include "cinder/ip/Fill.h"
#include "cinder/text/Text.h"

#include <vector>

using namespace ci;
using namespace ci::app;
using namespace std;

class TextOnPathApp : public App {
public:
	TextOnPathApp() : mTrackedPoint(-1) {}

	void setup() override;
	void update() override;
	void mouseDown( MouseEvent event );
	void mouseUp( MouseEvent event );
	void mouseDrag( MouseEvent event );
	void keyDown( KeyEvent event );
	void updateText();
	void drawCurve();
	void draw() override;

	Path2d	mPath;
	int		mTrackedPoint;
	bool	mReverse = false;
	cinder::text::Face* mFace;
	cinder::text::Font* mFont;
	gl::TextureRef	mTex;
};

void TextOnPathApp::setup()
{
	ImGui::Initialize();

	mFace = text::loadSystemFace( "Perpetua" );
	//mFace = text::loadFace( "C:\\Windows\\Fonts\\SEGUIEMJ.TTF" );
	mFont = text::loadFont(mFace, 48);
}

void TextOnPathApp::update()
{
	ImGui::Begin("Parameters");
	ImGui::Checkbox( "Reverse", &mReverse );
	ImGui::End();
}

void TextOnPathApp::mouseDown(MouseEvent event)
{
	if (event.isLeftDown()) { // line
		if (mPath.empty()) {
			mPath.moveTo(event.getPos());
			mTrackedPoint = 0;
		}
		else
			mPath.lineTo(event.getPos());
	}
}

void TextOnPathApp::mouseDrag(MouseEvent event)
{
	if (mTrackedPoint >= 0) {
		mPath.setPoint(mTrackedPoint, event.getPos());
	}
	else { // first bit of dragging, so switch our line to a cubic or a quad if Shift is down
		// we want to preserve the end of our current line, because it will still be the end of our curve
		vec2 endPt = mPath.getPoint(mPath.getNumPoints() - 1);
		// and now we'll delete that line and replace it with a curve
		mPath.removeSegment(mPath.getNumSegments() - 1);

		Path2d::SegmentType prevType = (mPath.getNumSegments() == 0) ? Path2d::MOVETO : mPath.getSegmentType(mPath.getNumSegments() - 1);

		if (event.isShiftDown() || prevType == Path2d::MOVETO) { // add a quadratic curve segment
			mPath.quadTo(event.getPos(), endPt);
		}
		else { // add a cubic curve segment
			vec2 tan1;
			if (prevType == Path2d::CUBICTO) { 		// if the segment before was cubic, let's replicate and reverse its tangent
				vec2 prevDelta = mPath.getPoint(mPath.getNumPoints() - 2) - mPath.getPoint(mPath.getNumPoints() - 1);
				tan1 = mPath.getPoint(mPath.getNumPoints() - 1) - prevDelta;
			}
			else if (prevType == Path2d::QUADTO) {
				// we can figure out what the equivalent cubic tangent would be using a little math
				vec2 quadTangent = mPath.getPoint(mPath.getNumPoints() - 2);
				vec2 quadEnd = mPath.getPoint(mPath.getNumPoints() - 1);
				vec2 prevDelta = (quadTangent + (quadEnd - quadTangent) / 3.0f) - quadEnd;
				tan1 = quadEnd - prevDelta;
			}
			else
				tan1 = mPath.getPoint(mPath.getNumPoints() - 1);

			mPath.curveTo(tan1, event.getPos(), endPt);
		}

		// our second-to-last point is the tangent next to the end, and we'll track that
		mTrackedPoint = mPath.getNumPoints() - 2;
	}
	
}

void TextOnPathApp::mouseUp(MouseEvent event)
{
	mTrackedPoint = -1;
}

void TextOnPathApp::keyDown(KeyEvent event)
{
	if (event.getChar() == 'x')
		mPath.clear();
}

void TextOnPathApp::drawCurve()
{
	// draw the control points
	gl::color(Color(1, 1, 0));
	for (size_t p = 0; p < mPath.getNumPoints(); ++p)
		gl::drawSolidCircle(mPath.getPoint(p), 2.5f);

	// draw the curve itself
	gl::color(Color(1.0f, 0.5f, 0.25f));
	gl::draw(mPath);

	if( mPath.getNumSegments() > 0 ) {
		// draw some tangents
		gl::color(Color(0.2f, 0.9f, 0.2f));
		for( int i = 0; i <= 20; ++i ) {
			float nT = mPath.calcNormalizedTime( i / 20.0f, false );
			auto pos = mPath.getPosition( nT );
			auto tangent = mPath.getTangent( nT );
			gl::drawLine( pos, pos + normalize( vec2{ -tangent.y, tangent.x } ) * 40.0f);
		}
	}
}



void TextOnPathApp::updateText()
{
	if( mPath.empty() )
		return;

	text::AttrString str{ "Hello World" };
	auto textOnPath = text::TextOnPath( str, mPath, text::TypesetOptions().ignoreLineMetrics( false ).defaultAlignment( text::Alignment::LEFT ).defaultShapingOptions( text::ShapingOptions().ignoreMissingGlyphs(true) ) );

	Surface8u surface = Surface8u( getWindowWidth(), getWindowHeight(), true );
	surface.setPremultiplied( true );
	ip::fill( &surface, ColorA8u( 0, 0, 0, 0 ) );

	text::render( textOnPath, &surface );

	//mTex = gl::Texture::create( surface );
}

void TextOnPathApp::draw()
{
	gl::clear(Color(0.0f, 0.1f, 0.2f));

	drawCurve();
	
	if( mTex )
		gl::draw( mTex );
}


CINDER_APP( TextOnPathApp, RendererGl )
