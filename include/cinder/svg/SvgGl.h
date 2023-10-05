/*
 Copyright (c) 2012, The Cinder Project
 All rights reserved.
 
 This code is designed for use with the Cinder C++ library, http://libcinder.org

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that
 the following conditions are met:

	* Redistributions of source code must retain the above copyright notice, this list of conditions and
	the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
	the following disclaimer in the documentation and/or other materials provided with the distribution.

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

#include "cinder/gl/Texture.h"
#include "cinder/svg/Svg.h"
#include "cinder/Triangulate.h"
#include "cinder/gl/draw.h"
#include "cinder/gl/wrapper.h"

namespace cinder {

class CI_API SvgRendererGl : public svg::Renderer {
  public:
	SvgRendererGl() : svg::Renderer() { 
		mStacks.defaults();
		glLineWidth( 1.0f );
		gl::pushModelMatrix();
	}
	
	~SvgRendererGl() override
	{
		gl::popModelMatrix();
	}
  
	void	pushGroup( const svg::Group &group, float opacity ) override {}
	
	void	drawPath( const svg::Path &path ) override {
		if( ! mStacks.fill.back().isNone() ) {
			gl::color( getCurFillColor() );
			Triangulator::Winding winding = ( mStacks.fillRule.back() == svg::FILL_RULE_NONZERO ) ? Triangulator::WINDING_NONZERO : Triangulator::WINDING_ODD;
			gl::draw( Triangulator( path.getShape2d() ).calcMesh( winding ) );
		}
		if( ! mStacks.stroke.back().isNone() ) {
			gl::color( getCurStrokeColor() );
			gl::draw( path.getShape2d() );
		}
	}

	void	drawPolygon( const svg::Polygon &polygon ) override {
		if( ! mStacks.fill.back().isNone() ) {
			gl::color( getCurFillColor() );
			Triangulator::Winding winding = ( mStacks.fillRule.back() == svg::FILL_RULE_NONZERO ) ? Triangulator::WINDING_NONZERO : Triangulator::WINDING_ODD;
			gl::draw( Triangulator( polygon.getPolyLine() ).calcMesh( winding ) );

		}
		if( ! mStacks.stroke.back().isNone() ) {
			gl::color( getCurStrokeColor() );
			gl::draw( polygon.getPolyLine() );
		}
	}

	void	drawPolyline( const svg::Polyline &polyline ) override {
		if( ! mStacks.fill.back().isNone() ) {
			gl::color( getCurFillColor() );
			Triangulator::Winding winding = ( mStacks.fillRule.back() == svg::FILL_RULE_NONZERO ) ? Triangulator::WINDING_NONZERO : Triangulator::WINDING_ODD;
			gl::draw( Triangulator( polyline.getPolyLine() ).calcMesh( winding ) );

		}
		if( ! mStacks.stroke.back().isNone() ) {
			gl::color( getCurStrokeColor() );
			gl::draw( polyline.getPolyLine() );
		}
	}

	void	drawLine( const svg::Line &line ) override {
		if( ! mStacks.stroke.back().isNone() ) {
			gl::color( getCurStrokeColor() );
			gl::drawLine( line.getPoint1(), line.getPoint2() );
		}
	}

	void	drawRect( const svg::Rect &rect ) override {
		if( ! mStacks.fill.back().isNone() ) {
			gl::color( getCurFillColor() );
			gl::drawSolidRect( rect.getRect() );
		}
		if( ! mStacks.stroke.back().isNone() ) {
			gl::color( getCurStrokeColor() );
			gl::drawStrokedRect( rect.getRect() );
		}
	}

	void	drawCircle( const svg::Circle &circle ) override {
		if( ! mStacks.fill.back().isNone() ) {
			gl::color( getCurFillColor() );
			gl::drawSolidCircle( circle.getCenter(), circle.getRadius() );
		}
		if( ! mStacks.stroke.back().isNone() ) {
			gl::color( getCurStrokeColor() );
			gl::drawStrokedCircle( circle.getCenter(), circle.getRadius() );
		}
	}

	void	drawEllipse( const svg::Ellipse &ellipse ) override {
		if( ! mStacks.fill.back().isNone() ) {
			gl::color( getCurFillColor() );
			gl::drawSolidEllipse( ellipse.getCenter(), ellipse.getRadiusX(), ellipse.getRadiusY() );
		}
		if( ! mStacks.stroke.back().isNone() ) {
			gl::color( getCurStrokeColor() );
			gl::drawStrokedEllipse( ellipse.getCenter(), ellipse.getRadiusX(), ellipse.getRadiusY() );
		}
	}

	void drawImage( const svg::Image &image ) override { drawImage( *image.getSurface(), image.getRect() ); }
	void drawImage( const Surface8u &surface, const Rectf &drawRect ) const
	{
		gl::color( Color::white() );
		gl::draw( gl::Texture::create( surface ), drawRect );
	}

	void	drawTextSpan( const svg::TextSpan &span ) override {
	
	}

	void	popGroup() override {}

	void	pushMatrix( const mat3 &m ) override {
		gl::pushModelMatrix();
		gl::multModelMatrix( transform2dTo3d( m ) );
	}
	void	popMatrix() override {
		gl::popModelMatrix();
	}
	
	void	pushFill( const svg::Paint &paint ) override { mStacks.fill.push_back( paint ); }
	void	popFill() override { mStacks.fill.pop_back(); }
	void	pushStroke( const svg::Paint &paint ) override { mStacks.stroke.push_back( paint ); }
	void	popStroke() override { mStacks.stroke.pop_back(); }
	void	pushFillOpacity( float opacity ) override { mStacks.fillOpacity.push_back( opacity ); }
	void	popFillOpacity() override { mStacks.fillOpacity.pop_back(); }
	void	pushStrokeOpacity( float opacity ) override { mStacks.strokeOpacity.push_back( opacity ); }
	void	popStrokeOpacity() override { mStacks.strokeOpacity.pop_back(); }

	ColorA getCurFillColor() const
	{
		ColorA result( mStacks.fill.back().getColor() );
		result.a = mStacks.fillOpacity.back();
		return result;
	}
	ColorA getCurStrokeColor() const
	{
		ColorA result( mStacks.stroke.back().getColor() );
		result.a = mStacks.strokeOpacity.back();
		return result;
	}


	void	pushStrokeWidth( float width ) override { mStacks.strokeWidth.push_back( width ); glLineWidth( width ); }
	void	popStrokeWidth() override { mStacks.strokeWidth.pop_back(); glLineWidth( mStacks.strokeWidth.back() ); }
	void	pushFillRule( svg::FillRule rule ) override { mStacks.fillRule.push_back( rule ); }
	void	popFillRule() override { mStacks.fillRule.pop_back(); }	

private:
	Stacks mStacks;
};

namespace gl {
inline void draw( const svg::Doc &svg )
{
	SvgRendererGl renderGl;
	svg.render( renderGl );
}
} // namespace gl

} // namespace cinder