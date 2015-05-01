#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Rand.h"
#include "cinder/BinPacker.h"
#include "cinder/Utilities.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class CinderBinPackerApp : public App {
public:
	enum Mode { SINGLE, MULTI };

	static void prepareSettings( Settings *settings );

	void setup();
	void keyDown( KeyEvent event );
	void update();
	void draw();

	void pack();

	BinPacker					mBinPackerSingle;
	MultiBinPacker				mBinPackerMulti;

	std::vector<Area>			mUnpacked;
	std::vector<PackedArea>		mPacked;

	Mode						mMode;
};

void CinderBinPackerApp::prepareSettings( Settings *settings )
{
	settings->setWindowSize( 512, 512 );
}

void CinderBinPackerApp::setup()
{
	mMode = SINGLE;

	mBinPackerSingle.setSize( 128, 128 );
	mBinPackerMulti.setSize( 128, 128 );

	mUnpacked.clear();
	mPacked.clear();

	for( size_t i = 0; i < 0; ++i ) {
		int size = Rand::randInt( 16, 64 );
		mUnpacked.push_back( Area( 0, 0, size, size ) );
	}

	pack();
}

void CinderBinPackerApp::keyDown( KeyEvent event )
{
	switch( event.getCode() ) {
		case KeyEvent::KEY_1:
		// enable single bin
		mMode = SINGLE;
		break;
		case KeyEvent::KEY_2:
		// enable multi bin
		mMode = MULTI;
		break;
		default:
		// add an Area of random size to mUnpacked
		int size = Rand::randInt( 4, 16 );
		mUnpacked.push_back( Area( 0, 0, size, size ) );
		break;
	}

	pack();
}

void CinderBinPackerApp::update()
{
}

void CinderBinPackerApp::draw()
{
	gl::clear( Color( 0, 0, 0 ) );

	// draw all packed Area's
	Rand rnd;

	switch( mMode ) {
		case SINGLE:
		// draw the borders of the bin
		gl::color( Color( 1, 1, 0 ) );
		gl::drawStrokedRect( Rectf( vec2( 0 ), mBinPackerSingle.getSize() ) );

		// draw the binned rectangles
		for( unsigned i = 0; i < mPacked.size(); ++i ) {
			rnd.seed( i + 12345 );
			gl::color( Color( ( rnd.nextUint() & 0xFF ) / 255.0f, ( rnd.nextUint() & 0xFF ) / 255.0f, ( rnd.nextUint() & 0xFF ) / 255.0f ) );
			gl::drawSolidRect( Rectf( mPacked[i] ) );
		}
		break;
		case MULTI:
		{
			unsigned n = (unsigned) ceil( getWindowWidth() / (float) mBinPackerMulti.getWidth() );

			// 
			for( unsigned i = 0; i < mPacked.size(); ++i ) {
				int bin = mPacked[i].getBin();

				gl::pushModelView();
				gl::translate( (float) ( ( bin % n ) * mBinPackerMulti.getWidth() ), (float) ( ( bin / n ) * mBinPackerMulti.getHeight() ), 0.0f );

				// draw the binned rectangle
				rnd.seed( i + 12345 );
				gl::color( Color( ( rnd.nextUint() & 0xFF ) / 255.0f, ( rnd.nextUint() & 0xFF ) / 255.0f, ( rnd.nextUint() & 0xFF ) / 255.0f ) );
				gl::drawSolidRect( Rectf( mPacked[i] ) );

				// draw the borders of the bin
				gl::color( Color( 1, 1, 0 ) );
				gl::drawStrokedRect( Rectf( vec2( 0 ), mBinPackerMulti.getSize() ) );

				gl::popModelView();
			}
		}
		break;
	}
}

void CinderBinPackerApp::pack()
{
	Timer t( true );

	switch( mMode ) {
		case SINGLE:
		try {
			// mPacked will contain all Area's of mUnpacked in the exact same order,
			// but moved to a different spot in the bin and represented as a PackedArea.
			// BinnedAreas can be used directly as Areas, conversion will happen automatically.
			// Unpacked will not be altered.
			mBinPackerSingle.clear();
			mPacked = mBinPackerSingle.insert( mUnpacked );
		}
		catch( ... ) {
			console() << "Could not pack " << mUnpacked.size() << " areas." << std::endl;
			// the bin is not large enough to contain all Area's, so let's
			// double the size...
			int size = mBinPackerSingle.getWidth();
			mBinPackerSingle.setSize( size << 1, size << 1 );

			/// ...and try again
			pack();
			return;
		}

		// show the total number of Area's in the window title bar
		getWindow()->setTitle( "CinderBinPackerApp | Single Bin | " + ci::toString( mPacked.size() ) );
		break;
		case MULTI:
		try {
			//  mPacked will contain all Area's of mUnpacked in the exact same order,
			// but moved to a different spot in the bin and represented as a PackedArea.
			// BinnedAreas can be used directly as Areas, conversion will happen automatically.
			// Use the PackedArea::getBin() method to find out to which bin the Area belongs.
			// Unpacked will not be altered.
			mBinPackerMulti.clear();
			mPacked = mBinPackerMulti.insert( mUnpacked );
		}
		catch( ... ) {
			// will only throw if any of the input rects is too big to fit a single bin, 
			// which is not the case in this demo
		}

		// show the total number of Area's in the window title bar
		getWindow()->setTitle( "CinderBinPackerApp | Multi Bin | " + ci::toString( mPacked.size() ) );
		break;
	}

	t.stop();
	console() << "Packing " << mPacked.size() << " areas took " << t.getSeconds() << " seconds." << std::endl;
}

CINDER_APP( CinderBinPackerApp, RendererGl, &CinderBinPackerApp::prepareSettings )
