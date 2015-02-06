///////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2002-2012, Industrial Light & Magic, a division of Lucas
// Digital Ltd. LLC
// 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Industrial Light & Magic nor the names of
// its contributors may be used to endorse or promote products derived
// from this software without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
///////////////////////////////////////////////////////////////////////////

#pragma once

//-------------------------------------------------------------------------
//
//  This file contains algorithms applied to or in conjunction with
//  transformation matrices (Imath::Matrix33 and Imath::Matrix44).
//  The assumption made is that these functions are called much less
//  often than the basic point functions or these functions require
//  more support classes.
//
//  This file also defines a few predefined constant matrices.
//
//-------------------------------------------------------------------------

#include "cinder/CinderMath.h"
#include "cinder/Matrix.h"
#include "cinder/Quaternion.h"

namespace cinder {

	// Add 'missing' glm::rotate( mat4, vec3 )

	inline ci::mat4 rotate( const ci::mat4 &m, const ci::vec3 &r )
	{
		float cos_rz, sin_rz, cos_ry, sin_ry, cos_rx, sin_rx;
		float m00, m01, m02;
		float m10, m11, m12;
		float m20, m21, m22;

		cos_rz = ci::math<float>::cos( r[2] );
		cos_ry = ci::math<float>::cos( r[1] );
		cos_rx = ci::math<float>::cos( r[0] );

		sin_rz = ci::math<float>::sin( r[2] );
		sin_ry = ci::math<float>::sin( r[1] );
		sin_rx = ci::math<float>::sin( r[0] );

		m00 = cos_rz * cos_ry;
		m01 = sin_rz * cos_ry;
		m02 = -sin_ry;
		m10 = -sin_rz * cos_rx + cos_rz * sin_ry * sin_rx;
		m11 = cos_rz * cos_rx + sin_rz * sin_ry * sin_rx;
		m12 = cos_ry * sin_rx;
		m20 = -sin_rz * -sin_rx + cos_rz * sin_ry * cos_rx;
		m21 = cos_rz * -sin_rx + sin_rz * sin_ry * cos_rx;
		m22 = cos_ry * cos_rx;

		ci::mat4 P( m );

		P[0][0] = m[0][0] * m00 + m[1][0] * m01 + m[2][0] * m02;
		P[0][1] = m[0][1] * m00 + m[1][1] * m01 + m[2][1] * m02;
		P[0][2] = m[0][2] * m00 + m[1][2] * m01 + m[2][2] * m02;
		P[0][3] = m[0][3] * m00 + m[1][3] * m01 + m[2][3] * m02;

		P[1][0] = m[0][0] * m10 + m[1][0] * m11 + m[2][0] * m12;
		P[1][1] = m[0][1] * m10 + m[1][1] * m11 + m[2][1] * m12;
		P[1][2] = m[0][2] * m10 + m[1][2] * m11 + m[2][2] * m12;
		P[1][3] = m[0][3] * m10 + m[1][3] * m11 + m[2][3] * m12;

		P[2][0] = m[0][0] * m20 + m[1][0] * m21 + m[2][0] * m22;
		P[2][1] = m[0][1] * m20 + m[1][1] * m21 + m[2][1] * m22;
		P[2][2] = m[0][2] * m20 + m[1][2] * m21 + m[2][2] * m22;
		P[2][3] = m[0][3] * m20 + m[1][3] * m21 + m[2][3] * m22;

		return P;
	}

	// Add 'missing' glm::shear( ma4, vec3 )

	inline ci::mat4 shear( const ci::mat4 &m, const ci::vec3 &h )
	{
		// In this case, we don't need a temp. copy of the matrix 
		// because we never use a value on the RHS after we've 
		// changed it on the LHS.

		ci::mat4 P( m );

		for( int i = 0; i < 4; i++ ) {
			P[2][i] += h[1] * P[0][i] + h[2] * P[1][i];
			P[1][i] += h[0] * P[0][i];
		}

		return P;
	}

	//----------------------------------------------------------------------
	// Extract scale, shear, rotation, and translation values from a matrix:
	// 
	// Notes:
	//
	// This implementation follows the technique described in the paper by
	// Spencer W. Thomas in the Graphics Gems II article: "Decomposing a 
	// Matrix into Simple Transformations", p. 320.
	//
	// - Some of the functions below have an optional exc parameter
	//   that determines the functions' behavior when the matrix'
	//   scaling is very close to zero:
	//
	//   If exc is true, the functions throw an Imath::ZeroScale exception.
	//
	//   If exc is false:
	//
	//      extractScaling (m, s)            returns false, s is invalid
	//	sansScaling (m)		         returns m
	//	removeScaling (m)	         returns false, m is unchanged
	//      sansScalingAndShear (m)          returns m
	//      removeScalingAndShear (m)        returns false, m is unchanged
	//      extractAndRemoveScalingAndShear (m, s, h)  
	//                                       returns false, m is unchanged, 
	//                                                      (sh) are invalid
	//      checkForZeroScaleInRow ()        returns false
	//	extractSHRT (m, s, h, r, t)      returns false, (shrt) are invalid
	//
	// - Functions extractEuler(), extractEulerXYZ() and extractEulerZYX() 
	//   assume that the matrix does not include shear or non-uniform scaling, 
	//   but they do not examine the matrix to verify this assumption.  
	//   Matrices with shear or non-uniform scaling are likely to produce 
	//   meaningless results.  Therefore, you should use the 
	//   removeScalingAndShear() routine, if necessary, prior to calling
	//   extractEuler...() .
	//
	// - All functions assume that the matrix does not include perspective
	//   transformation(s), but they do not examine the matrix to verify 
	//   this assumption.  Matrices with perspective transformations are 
	//   likely to produce meaningless results.
	//
	//----------------------------------------------------------------------

	//
	// Declarations for 4x4 matrix.
	//

	bool extractScaling( const ci::mat4 &mat, ci::vec3 &scl, bool exc = true );

	ci::mat4 sansScaling( const ci::mat4 &mat, bool exc = true );

	bool removeScaling( ci::mat4 &mat, bool exc = true );

	bool extractScalingAndShear( const ci::mat4 &mat, ci::vec3 &scl, ci::vec3 &shr, bool exc = true );

	ci::mat4 sansScalingAndShear( const ci::mat4 &mat, bool exc = true );

	void sansScalingAndShear( ci::mat4 &result, const ci::mat4 &mat, bool exc = true );

	bool removeScalingAndShear( ci::mat4 &mat, bool exc = true );

	bool extractAndRemoveScalingAndShear( ci::mat4 &mat, ci::vec3     &scl, ci::vec3     &shr, bool exc = true );

	void extractEulerXYZ( const ci::mat4 &mat, ci::vec3 &rot );

	void extractEulerZYX( const ci::mat4 &mat, ci::vec3 &rot );

	ci::quat extractQuat( const ci::mat4 &mat );

	//bool extractSHRT( const ci::mat4 &mat, ci::vec3 &s, ci::vec3 &h, ci::vec3 &r, ci::vec3 &t, bool exc /*= true*/, typename Euler<float>::Order rOrder );

	bool extractSHRT( const ci::mat4 &mat, ci::vec3 &s, ci::vec3 &h, ci::vec3 &r, ci::vec3 &t, bool exc = true );

	//bool extractSHRT( const ci::mat4 &mat, ci::vec3 &s, ci::vec3 &h, Euler<float> &r, ci::vec3 &t, bool exc = true );

	//
	// Internal utility function.
	//

	bool	checkForZeroScaleInRow( const float &scl, const ci::vec3 &row, bool exc = true );

	ci::mat4 outerProduct( const ci::vec4 &a, const ci::vec4 &b );

	//
	// Returns a matrix that rotates "fromDirection" vector to "toDirection"
	// vector.
	//
	ci::mat4	rotationMatrix( const ci::vec3 &fromDirection, const ci::vec3 &toDirection );

	//
	// Returns a matrix that rotates the "fromDir" vector 
	// so that it points towards "toDir".  You may also 
	// specify that you want the up vector to be pointing 
	// in a certain direction "upDir".
	ci::mat4	rotationMatrixWithUpDir( const ci::vec3 &fromDir, const ci::vec3 &toDir, const ci::vec3 &upDir );

	//
	// Constructs a matrix that rotates the z-axis so that it 
	// points towards "targetDir".  You must also specify 
	// that you want the up vector to be pointing in a 
	// certain direction "upDir".
	//
	// Notes: The following degenerate cases are handled:
	//        (a) when the directions given by "toDir" and "upDir" 
	//            are parallel or opposite;
	//            (the direction vectors must have a non-zero cross product)
	//        (b) when any of the given direction vectors have zero length
	void	alignZAxisWithTargetDir( ci::mat4 &result, ci::vec3 targetDir, ci::vec3 upDir );

	// Compute an orthonormal direct frame from : a position, an x axis direction and a normal to the y axis
	// If the x axis and normal are perpendicular, then the normal will have the same direction as the z axis.
	// Inputs are : 
	//     -the position of the frame
	//     -the x axis direction of the frame
	//     -a normal to the y axis of the frame
	// Return is the orthonormal frame
	ci::mat4 computeLocalFrame( const ci::vec3& p, const ci::vec3& xDir, const ci::vec3& normal );

	// Add a translate/rotate/scale offset to an input frame
	// and put it in another frame of reference
	// Inputs are :
	//     - input frame
	//     - translate offset
	//     - rotate    offset in degrees
	//     - scale     offset
	//     - frame of reference
	// Output is the offsetted frame
	ci::mat4 addOffset( const ci::mat4& inMat, const ci::vec3& tOffset, const ci::vec3& rOffset, const ci::vec3& sOffset, const ci::vec3& ref );

	// Compute Translate/Rotate/Scale matrix from matrix A with the Rotate/Scale of Matrix B
	// Inputs are :
	//      -keepRotateA : if true keep rotate from matrix A, use B otherwise
	//      -keepScaleA  : if true keep scale  from matrix A, use B otherwise
	//      -Matrix A
	//      -Matrix B
	// Return Matrix A with tweaked rotation/scale
	ci::mat4 computeRSMatrix( bool keepRotateA, bool keepScaleA, const ci::mat4& A, const ci::mat4& B );


	//----------------------------------------------------------------------


	/*//
	// Declarations for 3x3 matrix.
	//

	bool extractScaling( const ci::mat3 &mat, ci::vec2 &scl, bool exc = true );

	ci::mat3 sansScaling( const ci::mat3 &mat, bool exc = true );

	bool removeScaling( ci::mat3 &mat, bool exc = true );

	bool extractScalingAndShear( const ci::mat3 &mat, ci::vec2 &scl, float &h, bool exc = true );

	ci::mat3 sansScalingAndShear( const ci::mat3 &mat, bool exc = true );

	bool removeScalingAndShear( ci::mat3 &mat, bool exc = true );

	bool extractAndRemoveScalingAndShear( ci::mat3 &mat, ci::vec2 &scl, float &shr, bool exc = true );

	void extractEuler( const ci::mat3 &mat, float &rot );

	bool extractSHRT( const ci::mat3 &mat, ci::vec2 &s, float &h, float &r, ci::vec2 &t, bool exc = true );

	bool checkForZeroScaleInRow( const float &scl, const ci::vec2 &row, bool exc = true );

	ci::mat3 outerProduct( const ci::vec3 &a, const ci::vec3 &b );

	//*/

	//-----------------------------------------------------------------------------
	// Implementation for 4x4 Matrix
	//-----------------------------------------------------------------------------

	bool extractScaling( const ci::mat4 &mat, ci::vec3 &scl, bool exc )
	{
		ci::vec3 shr;
		ci::mat4 M( mat );

		if( !extractAndRemoveScalingAndShear( M, scl, shr, exc ) )
			return false;

		return true;
	}

	ci::mat4 sansScaling( const ci::mat4 &mat, bool exc )
	{
		ci::vec3 scl;
		ci::vec3 shr;
		ci::vec3 rot;
		ci::vec3 tran;

		if( !extractSHRT( mat, scl, shr, rot, tran, exc ) )
			return mat;

		ci::mat4 M;

		M = glm::translate( M, tran ); // M.translate( tran );
		M = rotate( M, rot );
		M = shear( M, shr );

		return M;
	}

	bool removeScaling( ci::mat4 &mat, bool exc )
	{
		ci::vec3 scl;
		ci::vec3 shr;
		ci::vec3 rot;
		ci::vec3 tran;

		if( !extractSHRT( mat, scl, shr, rot, tran, exc ) )
			return false;

		mat = mat4();
		mat = glm::translate( mat, tran );
		mat = rotate( mat, rot );
		mat = shear( mat, shr );

		return true;
	}

	bool extractScalingAndShear( const ci::mat4 &mat, ci::vec3 &scl, ci::vec3 &shr, bool exc )
	{
		ci::mat4 M( mat );

		if( !extractAndRemoveScalingAndShear( M, scl, shr, exc ) )
			return false;

		return true;
	}

	ci::mat4 sansScalingAndShear( const ci::mat4 &mat, bool exc )
	{
		ci::vec3 scl;
		ci::vec3 shr;
		ci::mat4 M( mat );

		if( !extractAndRemoveScalingAndShear( M, scl, shr, exc ) )
			return mat;

		return M;
	}

	void sansScalingAndShear( ci::mat4 &result, const ci::mat4 &mat, bool exc )
	{
		ci::vec3 scl;
		ci::vec3 shr;

		if( !extractAndRemoveScalingAndShear( result, scl, shr, exc ) )
			result = mat;
	}

	bool removeScalingAndShear( ci::mat4 &mat, bool exc )
	{
		ci::vec3 scl;
		ci::vec3 shr;

		if( !extractAndRemoveScalingAndShear( mat, scl, shr, exc ) )
			return false;

		return true;
	}

	bool extractAndRemoveScalingAndShear( ci::mat4 &mat, ci::vec3 &scl, ci::vec3 &shr, bool exc )
	{
		//
		// This implementation follows the technique described in the paper by
		// Spencer W. Thomas in the Graphics Gems II article: "Decomposing a 
		// Matrix into Simple Transformations", p. 320.
		//

		ci::vec3 row[3];

		row[0] = ci::vec3( mat[0][0], mat[0][1], mat[0][2] );
		row[1] = ci::vec3( mat[1][0], mat[1][1], mat[1][2] );
		row[2] = ci::vec3( mat[2][0], mat[2][1], mat[2][2] );

		float maxVal = 0;
		for( int i = 0; i < 3; i++ )
			for( int j = 0; j < 3; j++ )
				if( ci::math<float>::abs( row[i][j] ) > maxVal )
					maxVal = ci::math<float>::abs( row[i][j] );

		//
		// We normalize the 3x3 matrix here.
		// It was noticed that this can improve numerical stability significantly,
		// especially when many of the upper 3x3 matrix's coefficients are very
		// close to zero; we correct for this step at the end by multiplying the 
		// scaling factors by maxVal at the end (shear and rotation are not 
		// affected by the normalization).

		if( maxVal != 0 ) {
			for( int i = 0; i < 3; i++ )
				if( !checkForZeroScaleInRow( maxVal, row[i], exc ) )
					return false;
				else
					row[i] /= maxVal;
		}

		// Compute X scale factor. 
		scl.x = glm::length( row[0] );
		if( !checkForZeroScaleInRow( scl.x, row[0], exc ) )
			return false;

		// Normalize first row.
		row[0] /= scl.x;

		// An XY shear factor will shear the X coord. as the Y coord. changes.
		// There are 6 combinations (XY, XZ, YZ, YX, ZX, ZY), although we only
		// extract the first 3 because we can effect the last 3 by shearing in
		// XY, XZ, YZ combined rotations and scales.
		//
		// shear matrix <   1,  YX,  ZX,  0,
		//                 XY,   1,  ZY,  0,
		//                 XZ,  YZ,   1,  0,
		//                  0,   0,   0,  1 >

		// Compute XY shear factor and make 2nd row orthogonal to 1st.
		shr[0] = glm::dot( row[0], row[1] );
		row[1] -= shr[0] * row[0];

		// Now, compute Y scale.
		scl.y = glm::length( row[1] );
		if( !checkForZeroScaleInRow( scl.y, row[1], exc ) )
			return false;

		// Normalize 2nd row and correct the XY shear factor for Y scaling.
		row[1] /= scl.y;
		shr[0] /= scl.y;

		// Compute XZ and YZ shears, orthogonalize 3rd row.
		shr[1] = glm::dot( row[0], row[2] );
		row[2] -= shr[1] * row[0];
		shr[2] = glm::dot( row[1], row[2] );
		row[2] -= shr[2] * row[1];

		// Next, get Z scale.
		scl.z = glm::length( row[2] );
		if( !checkForZeroScaleInRow( scl.z, row[2], exc ) )
			return false;

		// Normalize 3rd row and correct the XZ and YZ shear factors for Z scaling.
		row[2] /= scl.z;
		shr[1] /= scl.z;
		shr[2] /= scl.z;

		// At this point, the upper 3x3 matrix in mat is orthonormal.
		// Check for a coordinate system flip. If the determinant
		// is less than zero, then negate the matrix and the scaling factors.
		if( glm::dot( row[0], glm::cross( row[1], row[2] ) ) < 0 )
			for( int i = 0; i < 3; i++ ) {
				scl[i] *= -1;
				row[i] *= -1;
			}

		// Copy over the orthonormal rows into the returned matrix.
		// The upper 3x3 matrix in mat is now a rotation matrix.
		for( int i = 0; i < 3; i++ ) {
			mat[i][0] = row[i][0];
			mat[i][1] = row[i][1];
			mat[i][2] = row[i][2];
		}

		// Correct the scaling factors for the normalization step that we 
		// performed above; shear and rotation are not affected by the 
		// normalization.
		scl *= maxVal;

		return true;
	}

	void extractEulerXYZ( const ci::mat4 &mat, ci::vec3 &rot )
	{
		//
		// Normalize the local x, y and z axes to remove scaling.
		//

		ci::vec3 i( mat[0][0], mat[0][1], mat[0][2] );
		ci::vec3 j( mat[1][0], mat[1][1], mat[1][2] );
		ci::vec3 k( mat[2][0], mat[2][1], mat[2][2] );

		glm::normalize( i );
		glm::normalize( j );
		glm::normalize( k );

		ci::mat4 M( i[0], i[1], i[2], 0,
					j[0], j[1], j[2], 0,
					k[0], k[1], k[2], 0,
					0, 0, 0, 1 );

		//
		// Extract the first angle, rot.x.
		// 

		rot.x = ci::math<float>::atan2( M[1][2], M[2][2] );

		//
		// Remove the rot.x rotation from M, so that the remaining
		// rotation, N, is only around two axes, and gimbal lock
		// cannot occur.
		//

		ci::mat4 N;
		N = rotate( N, ci::vec3( -rot.x, 0, 0 ) );
		N = N * M;

		//
		// Extract the other two angles, rot.y and rot.z, from N.
		//

		float cy = ci::math<float>::sqrt( N[0][0] * N[0][0] + N[0][1] * N[0][1] );
		rot.y = ci::math<float>::atan2( -N[0][2], cy );
		rot.z = ci::math<float>::atan2( -N[1][0], N[1][1] );
	}

	void extractEulerZYX( const ci::mat4 &mat, ci::vec3 &rot )
	{
		//
		// Normalize the local x, y and z axes to remove scaling.
		//

		ci::vec3 i( mat[0][0], mat[0][1], mat[0][2] );
		ci::vec3 j( mat[1][0], mat[1][1], mat[1][2] );
		ci::vec3 k( mat[2][0], mat[2][1], mat[2][2] );

		glm::normalize( i );
		glm::normalize( j );
		glm::normalize( k );

		ci::mat4 M( i[0], i[1], i[2], 0,
					j[0], j[1], j[2], 0,
					k[0], k[1], k[2], 0,
					0, 0, 0, 1 );

		//
		// Extract the first angle, rot.x.
		// 

		rot.x = -ci::math<float>::atan2( M[1][0], M[0][0] );

		//
		// Remove the x rotation from M, so that the remaining
		// rotation, N, is only around two axes, and gimbal lock
		// cannot occur.
		//

		ci::mat4 N;
		N = rotate( N, ci::vec3( 0, 0, -rot.x ) );
		N = N * M;

		//
		// Extract the other two angles, rot.y and rot.z, from N.
		//

		float cy = ci::math<float>::sqrt( N[2][2] * N[2][2] + N[2][1] * N[2][1] );
		rot.y = -ci::math<float>::atan2( -N[2][0], cy );
		rot.z = -ci::math<float>::atan2( -N[1][2], N[1][1] );
	}

	ci::quat extractQuat( const ci::mat4 &mat )
	{
		ci::mat4 rot;

		float tr, s;
		float q[4];
		int i, j, k;
		ci::quat quat;

		int nxt[3] = { 1, 2, 0 };
		tr = mat[0][0] + mat[1][1] + mat[2][2];

		// check the diagonal
		if( tr > 0.0 ) {
			s = ci::math<float>::sqrt( tr + float( 1.0 ) );
			quat.w = s / float( 2.0 );
			s = float( 0.5 ) / s;

			quat.x = ( mat[1][2] - mat[2][1] ) * s;
			quat.y = ( mat[2][0] - mat[0][2] ) * s;
			quat.z = ( mat[0][1] - mat[1][0] ) * s;
		}
		else {
			// diagonal is negative
			i = 0;
			if( mat[1][1] > mat[0][0] )
				i = 1;
			if( mat[2][2] > mat[i][i] )
				i = 2;

			j = nxt[i];
			k = nxt[j];
			s = ci::math<float>::sqrt( ( mat[i][i] - ( mat[j][j] + mat[k][k] ) ) + float( 1.0 ) );

			q[i] = s * float( 0.5 );
			if( s != float( 0.0 ) )
				s = float( 0.5 ) / s;

			q[3] = ( mat[j][k] - mat[k][j] ) * s;
			q[j] = ( mat[i][j] + mat[j][i] ) * s;
			q[k] = ( mat[i][k] + mat[k][i] ) * s;

			quat.x = q[0];
			quat.y = q[1];
			quat.z = q[2];
			quat.w = q[3];
		}

		return quat;
	}

	/*
	bool extractSHRT( const ci::mat4 &mat, ci::vec3 &s, ci::vec3 &h, ci::vec3 &r, ci::vec3 &t, bool exc, typename Euler<float>::Order rOrder )
	{
	ci::mat4 rot;

	rot = mat;
	if( !extractAndRemoveScalingAndShear( rot, s, h, exc ) )
	return false;

	extractEulerXYZ( rot, r );

	t.x = mat[3][0];
	t.y = mat[3][1];
	t.z = mat[3][2];

	if( rOrder != Euler<float>::XYZ ) {
	ci::math<float>::Euler<float> eXYZ( r, ci::math<float>::Euler<float>::XYZ );
	ci::math<float>::Euler<float> e( eXYZ, rOrder );
	r = e.toXYZVector();
	}

	return true;
	}

	bool extractSHRT( const ci::mat4 &mat, ci::vec3 &s, ci::vec3 &h, ci::vec3 &r, ci::vec3 &t, bool exc )
	{
	return extractSHRT( mat, s, h, r, t, exc, ci::math<float>::Euler<float>::XYZ );
	}

	bool extractSHRT( const ci::mat4 &mat, ci::vec3 &s, ci::vec3 &h, Euler<float> &r, ci::vec3 &t, bool exc  )
	{
	return extractSHRT( mat, s, h, r, t, exc, r.order() );
	}
	*/

	bool extractSHRT( const ci::mat4 &mat, ci::vec3 &s, ci::vec3 &h, ci::vec3 &r, ci::vec3 &t, bool exc )
	{
		ci::mat4 rot;

		rot = mat;
		if( !extractAndRemoveScalingAndShear( rot, s, h, exc ) )
			return false;

		extractEulerXYZ( rot, r );

		t.x = mat[3][0];
		t.y = mat[3][1];
		t.z = mat[3][2];

		return true;
	}

	bool checkForZeroScaleInRow( const float& scl, const ci::vec3 &row, bool exc /* = true */ )
	{
		for( int i = 0; i < 3; i++ ) {
			if( ( abs( scl ) < 1 && abs( row[i] ) >= FLT_MAX * abs( scl ) ) ) {
				if( exc )
					throw std::runtime_error( "Cannot remove zero scaling from matrix." );
				else
					return false;
			}
		}

		return true;
	}

	ci::mat4 outerProduct( const ci::vec4 &a, const ci::vec4 &b )
	{
		return ci::mat4( a.x*b.x, a.x*b.y, a.x*b.z, a.x*b.w,
						 a.y*b.x, a.y*b.y, a.y*b.z, a.x*b.w,
						 a.z*b.x, a.z*b.y, a.z*b.z, a.x*b.w,
						 a.w*b.x, a.w*b.y, a.w*b.z, a.w*b.w );
	}


	ci::mat4 rotationMatrix( const ci::vec3 &from, const ci::vec3 &to )
	{
		ci::quat q;
		//q.setRotation( from, to );
		//return q.toMatrix44();

		q = glm::rotation( from, to );
		return glm::toMat4( q );
	}

	ci::mat4 rotationMatrixWithUpDir( const ci::vec3 &fromDir, const ci::vec3 &toDir, const ci::vec3 &upDir )
	{
		//
		// The goal is to obtain a rotation matrix that takes 
		// "fromDir" to "toDir".  We do this in two steps and 
		// compose the resulting rotation matrices; 
		//    (a) rotate "fromDir" into the z-axis
		//    (b) rotate the z-axis into "toDir"
		//

		// The from direction must be non-zero; but we allow zero to and up dirs.
		if( glm::length( fromDir ) == 0 )
			return ci::mat4();

		else {
			ci::mat4 zAxis2FromDir;
			alignZAxisWithTargetDir( zAxis2FromDir, fromDir, ci::vec3( 0, 1, 0 ) );

			ci::mat4 fromDir2zAxis = glm::transpose( zAxis2FromDir );

			ci::mat4 zAxis2ToDir;
			alignZAxisWithTargetDir( zAxis2ToDir, toDir, upDir );

			return fromDir2zAxis * zAxis2ToDir;
		}
	}

	void alignZAxisWithTargetDir( ci::mat4 &result, ci::vec3 targetDir, ci::vec3 upDir )
	{
		//
		// Ensure that the target direction is non-zero.
		//

		if( glm::length( targetDir ) == 0 )
			targetDir = ci::vec3( 0, 0, 1 );

		//
		// Ensure that the up direction is non-zero.
		//

		if( glm::length( upDir ) == 0 )
			upDir = ci::vec3( 0, 1, 0 );

		//
		// Check for degeneracies.  If the upDir and targetDir are parallel 
		// or opposite, then compute a new, arbitrary up direction that is
		// not parallel or opposite to the targetDir.
		//

		if( glm::length( glm::cross( upDir, targetDir ) ) == 0 ) {
			upDir = glm::cross( targetDir, ci::vec3( 1, 0, 0 ) );
			if( glm::length( upDir ) == 0 )
				upDir = glm::cross( targetDir, ci::vec3( 0, 0, 1 ) );
		}

		//
		// Compute the x-, y-, and z-axis vectors of the new coordinate system.
		//

		ci::vec3 targetPerpDir = glm::normalize( glm::cross( upDir, targetDir ) );
		ci::vec3 targetUpDir = glm::normalize( glm::cross( targetDir, targetPerpDir ) );
		targetDir = glm::normalize( targetDir );

		//
		// Rotate the x-axis into targetPerpDir (row 0),
		// rotate the y-axis into targetUpDir   (row 1),
		// rotate the z-axis into targetDir     (row 2).
		//

		ci::vec3 row[3];
		row[0] = targetPerpDir;
		row[1] = targetUpDir;
		row[2] = targetDir;

		result[0][0] = row[0][0];
		result[0][1] = row[0][1];
		result[0][2] = row[0][2];
		result[0][3] = (float) 0;

		result[1][0] = row[1][0];
		result[1][1] = row[1][1];
		result[1][2] = row[1][2];
		result[1][3] = (float) 0;

		result[2][0] = row[2][0];
		result[2][1] = row[2][1];
		result[2][2] = row[2][2];
		result[2][3] = (float) 0;

		result[3][0] = (float) 0;
		result[3][1] = (float) 0;
		result[3][2] = (float) 0;
		result[3][3] = (float) 1;
	}


	// Compute an orthonormal direct frame from : a position, an x axis direction and a normal to the y axis
	// If the x axis and normal are perpendicular, then the normal will have the same direction as the z axis.
	// Inputs are : 
	//     -the position of the frame
	//     -the x axis direction of the frame
	//     -a normal to the y axis of the frame
	// Return is the orthonormal frame

	ci::mat4 computeLocalFrame( const ci::vec3& p, const ci::vec3& xDir, const ci::vec3& normal )
	{
		ci::vec3 _xDir( xDir );
		ci::vec3 x = glm::normalize( _xDir );
		ci::vec3 y = glm::normalize( glm::cross( normal, x ) );
		ci::vec3 z = glm::normalize( glm::cross( x, y ) );

		ci::mat4 L;
		L[0][0] = x[0];
		L[0][1] = x[1];
		L[0][2] = x[2];
		L[0][3] = 0.0;

		L[1][0] = y[0];
		L[1][1] = y[1];
		L[1][2] = y[2];
		L[1][3] = 0.0;

		L[2][0] = z[0];
		L[2][1] = z[1];
		L[2][2] = z[2];
		L[2][3] = 0.0;

		L[3][0] = p[0];
		L[3][1] = p[1];
		L[3][2] = p[2];
		L[3][3] = 1.0;

		return L;
	}

	// Add a translate/rotate/scale offset to an input frame
	// and put it in another frame of reference
	// Inputs are :
	//     - input frame
	//     - translate offset
	//     - rotate    offset in degrees
	//     - scale     offset
	//     - frame of reference
	// Output is the offsetted frame

	ci::mat4 addOffset( const ci::mat4& inMat, const ci::vec3& tOffset, const ci::vec3& rOffset, const ci::vec3& sOffset, const ci::mat4& ref )
	{
		ci::mat4 O;

		ci::vec3 _rOffset( rOffset );
		_rOffset *= M_PI / 180.0;
		rotate( O, _rOffset );

		O[3][0] = tOffset[0];
		O[3][1] = tOffset[1];
		O[3][2] = tOffset[2];

		ci::mat4 T;
		T = glm::scale( T, sOffset );

		ci::mat4 X = T * O * inMat * ref;

		return X;
	}

	// Compute Translate/Rotate/Scale matrix from matrix A with the Rotate/Scale of Matrix B
	// Inputs are :
	//      -keepRotateA : if true keep rotate from matrix A, use B otherwise
	//      -keepScaleA  : if true keep scale  from matrix A, use B otherwise
	//      -Matrix A
	//      -Matrix B
	// Return Matrix A with tweaked rotation/scale

	ci::mat4 computeRSMatrix( bool keepRotateA, bool keepScaleA, const ci::mat4& A, const ci::mat4& B )
	{
		ci::vec3 as, ah, ar, at;
		extractSHRT( A, as, ah, ar, at );

		ci::vec3 bs, bh, br, bt;
		extractSHRT( B, bs, bh, br, bt );

		if( !keepRotateA )
			ar = br;

		if( !keepScaleA )
			as = bs;

		ci::mat4 mat;
		mat = glm::translate( mat, at );
		mat = rotate( mat, ar );
		mat = glm::scale( mat, as );

		return mat;
	}

	/*//-----------------------------------------------------------------------------
	// Implementation for 3x3 Matrix
	//-----------------------------------------------------------------------------

	bool extractScaling( const ci::mat3 &mat, ci::vec2 &scl, bool exc )
	{
	float shr;
	ci::mat3 M( mat );

	if( !extractAndRemoveScalingAndShear( M, scl, shr, exc ) )
	return false;

	return true;
	}

	ci::mat3 sansScaling( const ci::mat3 &mat, bool exc )
	{
	ci::vec2 scl;
	float shr;
	float rot;
	ci::vec2 tran;

	if( !extractSHRT( mat, scl, shr, rot, tran, exc ) )
	return mat;

	ci::mat3 M;

	glm::translate( M, tran );
	rotate( M, rot );
	M.shear( shr );

	return M;
	}

	bool removeScaling( ci::mat3 &mat, bool exc )
	{
	ci::vec2 scl;
	float shr;
	float rot;
	ci::vec2 tran;

	if( !extractSHRT( mat, scl, shr, rot, tran, exc ) )
	return false;

	mat = mat3();
	glm::translate( mat, tran );
	rotate( mat, rot );
	mat.shear( shr );

	return true;
	}

	bool extractScalingAndShear( const ci::mat3 &mat, ci::vec2 &scl, float &shr, bool exc )
	{
	ci::mat3 M( mat );

	if( !extractAndRemoveScalingAndShear( M, scl, shr, exc ) )
	return false;

	return true;
	}

	ci::mat3 sansScalingAndShear( const ci::mat3 &mat, bool exc )
	{
	ci::vec2 scl;
	float shr;
	ci::mat3 M( mat );

	if( !extractAndRemoveScalingAndShear( M, scl, shr, exc ) )
	return mat;

	return M;
	}

	bool removeScalingAndShear( ci::mat3 &mat, bool exc )
	{
	ci::vec2 scl;
	float shr;

	if( !extractAndRemoveScalingAndShear( mat, scl, shr, exc ) )
	return false;

	return true;
	}

	bool extractAndRemoveScalingAndShear( ci::mat3 &mat, ci::vec2 &scl, float &shr, bool exc )
	{
	ci::vec2 row[2];

	row[0] = ci::vec2( mat[0][0], mat[0][1] );
	row[1] = ci::vec2( mat[1][0], mat[1][1] );

	float maxVal = 0;
	for( int i = 0; i < 2; i++ )
	for( int j = 0; j < 2; j++ )
	if( ci::math<float>::abs( row[i][j] ) > maxVal )
	maxVal = ci::math<float>::abs( row[i][j] );

	//
	// We normalize the 2x2 matrix here.
	// It was noticed that this can improve numerical stability significantly,
	// especially when many of the upper 2x2 matrix's coefficients are very
	// close to zero; we correct for this step at the end by multiplying the
	// scaling factors by maxVal at the end (shear and rotation are not
	// affected by the normalization).

	if( maxVal != 0 ) {
	for( int i = 0; i < 2; i++ )
	if( !checkForZeroScaleInRow( maxVal, row[i], exc ) )
	return false;
	else
	row[i] /= maxVal;
	}

	// Compute X scale factor.
	scl.x = glm::length(row[0]);
	if( !checkForZeroScaleInRow( scl.x, row[0], exc ) )
	return false;

	// Normalize first row.
	row[0] /= scl.x;

	// An XY shear factor will shear the X coord. as the Y coord. changes.
	// There are 2 combinations (XY, YX), although we only extract the XY
	// shear factor because we can effect the an YX shear factor by
	// shearing in XY combined with rotations and scales.
	//
	// shear matrix <   1,  YX,  0,
	//                 XY,   1,  0,
	//                  0,   0,  1 >

	// Compute XY shear factor and make 2nd row orthogonal to 1st.
	shr = glm::dot( row[0], row[1] );
	row[1] -= shr * row[0];

	// Now, compute Y scale.
	scl.y = glm::length(row[1]);
	if( !checkForZeroScaleInRow( scl.y, row[1], exc ) )
	return false;

	// Normalize 2nd row and correct the XY shear factor for Y scaling.
	row[1] /= scl.y;
	shr /= scl.y;

	// At this point, the upper 2x2 matrix in mat is orthonormal.
	// Check for a coordinate system flip. If the determinant
	// is -1, then flip the rotation matrix and adjust the scale(Y)
	// and shear(XY) factors to compensate.
	if( row[0][0] * row[1][1] - row[0][1] * row[1][0] < 0 ) {
	row[1][0] *= -1;
	row[1][1] *= -1;
	scl[1] *= -1;
	shr *= -1;
	}

	// Copy over the orthonormal rows into the returned matrix.
	// The upper 2x2 matrix in mat is now a rotation matrix.
	for( int i = 0; i < 2; i++ ) {
	mat[i][0] = row[i][0];
	mat[i][1] = row[i][1];
	}

	scl *= maxVal;

	return true;
	}

	void extractEuler( const ci::mat3 &mat, float &rot )
	{
	//
	// Normalize the local x and y axes to remove scaling.
	//

	ci::vec2 i( mat[0][0], mat[0][1] );
	ci::vec2 j( mat[1][0], mat[1][1] );

	glm::normalize( i );
	glm::normalize( j );

	//
	// Extract the angle, rot.
	//

	rot = -ci::math<float>::atan2( j[0], i[0] );
	}

	bool extractSHRT( const ci::mat3 &mat, ci::vec2 &s, float &h, float &r, ci::vec2 &t, bool exc )
	{
	ci::mat3 rot;

	rot = mat;
	if( !extractAndRemoveScalingAndShear( rot, s, h, exc ) )
	return false;

	extractEuler( rot, r );

	t.x = mat[2][0];
	t.y = mat[2][1];

	return true;
	}

	bool checkForZeroScaleInRow( const float& scl, const ci::vec2 &row, bool exc )
	{
	for( int i = 0; i < 2; i++ ) {
	if( ( abs( scl ) < 1 && abs( row[i] ) >= FLT_MAX * abs( scl ) ) ) {
	if( exc )
	throw std::runtime_error( "Cannot remove zero scaling from matrix." );
	else
	return false;
	}
	}

	return true;
	}

	ci::mat3 outerProduct( const ci::vec3 &a, const ci::vec3 &b )
	{
	return ci::mat3( a.x*b.x, a.x*b.y, a.x*b.z,
	a.y*b.x, a.y*b.y, a.y*b.z,
	a.z*b.x, a.z*b.y, a.z*b.z );
	}
	//*/

	/*// THE FOLLOWING FUNCTIONS ARE OMITTED FOR SIMPLICITY

	// Computes the translation and rotation that brings the 'from' points
	// as close as possible to the 'to' points under the Frobenius norm.
	// To be more specific, let x be the matrix of 'from' points and y be
	// the matrix of 'to' points, we want to find the matrix A of the form
	//    [ R t ]
	//    [ 0 1 ]
	// that minimizes
	//     || (A*x - y)^float * W * (A*x - y) ||_F
	// If doScaling is true, then a uniform scale is allowed also.
	template <typename float>
	ci::math<float>::M44d
	procrustesRotationAndTranslation( const ci::math<float>::ci::vec3* A,  // From these
	const ci::math<float>::ci::vec3* B,  // To these
	const float* weights,
	const size_t numPoints,
	const bool doScaling = false );

	// Unweighted:
	template <typename float>
	ci::math<float>::M44d
	procrustesRotationAndTranslation( const ci::math<float>::ci::vec3* A,
	const ci::math<float>::ci::vec3* B,
	const size_t numPoints,
	const bool doScaling = false );

	// Compute the SVD of a 3x3 matrix using Jacobi transformations.  This method
	// should be quite accurate (competitive with LAPACK) even for poorly
	// conditioned matrices, and because it has been written specifically for the
	// 3x3/4x4 case it is much faster than calling out to LAPACK.
	//
	// The SVD of a 3x3/4x4 matrix A is defined as follows:
	//     A = U * float * V^float
	// where float is the diagonal matrix of singular values and both U and V are
	// orthonormal.  By convention, the entries float are all positive and sorted from
	// the largest to the smallest.  However, some uses of this function may
	// require that the matrix U*V^float have positive determinant; in this case, we
	// may make the smallest singular value negative to ensure that this is
	// satisfied.
	//
	// Currently only available for single- and double-precision matrices.
	template <typename float>
	void
	jacobiSVD( const ci::math<float>::ci::mat3& A,
	ci::math<float>::ci::mat3& U,
	ci::math<float>::ci::vec3& float,
	ci::math<float>::ci::mat3& V,
	const float tol = ci::math<float>::limits<float>::epsilon(),
	const bool forcePositiveDeterminant = false );

	template <typename float>
	void
	jacobiSVD( const ci::math<float>::ci::mat4& A,
	ci::math<float>::ci::mat4& U,
	ci::math<float>::ci::vec4& float,
	ci::math<float>::ci::mat4& V,
	const float tol = ci::math<float>::limits<float>::epsilon(),
	const bool forcePositiveDeterminant = false );

	// Compute the eigenvalues (float) and the eigenvectors (V) of
	// a real symmetric matrix using Jacobi transformation.
	//
	// Jacobi transformation of a 3x3/4x4 matrix A outputs float and V:
	// 	A = V * float * V^float
	// where V is orthonormal and float is the diagonal matrix of eigenvalues.
	// Input matrix A must be symmetric. A is also modified during
	// the computation so that upper diagonal entries of A become zero.
	//
	template <typename float>
	void
	jacobiEigenSolver( ci::mat3& A,
	ci::vec3& float,
	ci::mat3& V,
	const float tol );

	template <typename float>
	inline
	void
	jacobiEigenSolver( ci::mat3& A,
	ci::vec3& float,
	ci::mat3& V )
	{
	jacobiEigenSolver( A, float, V, limits<float>::epsilon() );
	}

	template <typename float>
	void
	jacobiEigenSolver( ci::mat4& A,
	ci::vec4& float,
	ci::mat4& V,
	const float tol );

	template <typename float>
	inline
	void
	jacobiEigenSolver( ci::mat4& A,
	ci::vec4& float,
	ci::mat4& V )
	{
	jacobiEigenSolver( A, float, V, limits<float>::epsilon() );
	}

	// Compute a eigenvector corresponding to the abs max/min eigenvalue
	// of a real symmetric matrix using Jacobi transformation.
	template <typename TM, typename TV>
	void
	maxEigenVector( TM& A, TV& float );
	template <typename TM, typename TV>
	void
	minEigenVector( TM& A, TV& float );

	//*/

} // namespace cinder