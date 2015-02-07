///////////////////////////////////////////////////////////////////////////////////////////////////
// OpenGL Mathematics Copyright (c) 2005 - 2014 G-Truc Creation (www.g-truc.net)
///////////////////////////////////////////////////////////////////////////////////////////////////
// Created : 2015-02-06
// Updated : 2015-02-06
// Licence : This source is under MIT License
// File    : glm/gtx/decomposition.inl
///////////////////////////////////////////////////////////////////////////////////////////////////

namespace glm {
	template <typename T, precision P>
	GLM_FUNC_DECL bool extractSHRT( const detail::tmat4x4<T, P> &mat, detail::tvec3<T, P> & s, detail::tvec3<T, P> & h, detail::tvec3<T, P> & r, detail::tvec3<T, P> & t )
	{
		detail::tmat4x4<T, P> rot( mat );

		if( !extractAndRemoveScalingAndShear( rot, s, h ) )
			return false;

		extractEulerXYZ( rot, r );

		t.x = mat[3][0];
		t.y = mat[3][1];
		t.z = mat[3][2];

		return true;
	}

	template <typename T, precision P>
	GLM_FUNC_DECL bool extractAndRemoveScalingAndShear( detail::tmat4x4<T, P> & mat, detail::tvec3<T, P> & scl, detail::tvec3<T, P> & shr )
	{
		// This implementation follows the technique described in the paper by
		// Spencer W. Thomas in the Graphics Gems II article: "Decomposing a 
		// Matrix into Simple Transformations", p. 320.

		detail::tvec3<T, P> row[3];
		row[0] = detail::tvec3<T, P>( mat[0][0], mat[0][1], mat[0][2] );
		row[1] = detail::tvec3<T, P>( mat[1][0], mat[1][1], mat[1][2] );
		row[2] = detail::tvec3<T, P>( mat[2][0], mat[2][1], mat[2][2] );

		T maxVal = 0;
		for( int i = 0; i < 3; ++i ) {
			for( int j = 0; j < 3; ++j ) {
				if( abs( row[i][j] ) > maxVal ) {
					maxVal = abs( row[i][j] );
				}
			}
		}

		// We normalize the 3x3 matrix here.
		// It was noticed that this can improve numerical stability significantly,
		// especially when many of the upper 3x3 matrix's coefficients are very
		// close to zero; we correct for this step at the end by multiplying the 
		// scaling factors by maxVal (shear and rotation are not affected by the 
		// normalization).
		if( maxVal != 0 ) {
			for( int i = 0; i < 3; ++i ) {
				if( !checkForZeroScaleInRow( maxVal, row[i] ) )
					return false;
				else
					row[i] /= maxVal;
			}
		}

		// Compute X scale factor. 
		scl.x = length( row[0] );
		if( !checkForZeroScaleInRow( scl.x, row[0] ) )
			return false;

		// Normalize first row.
		row[0] /= scl.x;

		// An XY shear factor will shear the X coordinate as the Y coordinate changes.
		// There are 6 combinations (XY, XZ, YZ, YX, ZX, ZY), although we only
		// extract the first 3 because we can affect the last 3 by shearing in
		// XY, XZ, YZ combined rotations and scales.
		//
		// shear matrix <   1,  YX,  ZX,  0,
		//                 XY,   1,  ZY,  0,
		//                 XZ,  YZ,   1,  0,
		//                  0,   0,   0,  1 >

		// Compute XY shear factor and make 2nd row orthogonal to 1st.
		shr[0] = dot( row[0], row[1] );
		row[1] -= shr[0] * row[0];

		// Now, compute Y scale.
		scl.y = length( row[1] );
		if( !checkForZeroScaleInRow( scl.y, row[1] ) )
			return false;

		// Normalize 2nd row and correct the XY shear factor for Y scaling.
		row[1] /= scl.y;
		shr[0] /= scl.y;

		// Compute XZ and YZ shears, orthogonalize 3rd row.
		shr[1] = dot( row[0], row[2] );
		row[2] -= shr[1] * row[0];
		shr[2] = dot( row[1], row[2] );
		row[2] -= shr[2] * row[1];

		// Next, get Z scale.
		scl.z = length( row[2] );
		if( !checkForZeroScaleInRow( scl.z, row[2] ) )
			return false;

		// Normalize 3rd row and correct the XZ and YZ shear factors for Z scaling.
		row[2] /= scl.z;
		shr[1] /= scl.z;
		shr[2] /= scl.z;

		// At this point, the upper 3x3 matrix in mat is orthonormal.
		// Check for a coordinate system flip. If the determinant
		// is less than zero, then negate the matrix and the scaling factors.
		if( dot( row[0], cross( row[1], row[2] ) ) < 0 ) {
			for( int i = 0; i < 3; i++ ) {
				scl[i] *= -1;
				row[i] *= -1;
			}
		}

		// Copy over the orthonormal rows into the returned matrix.
		// The upper 3x3 matrix in mat is now a rotation matrix.
		for( int i = 0; i < 3; ++i ) {
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

	template <typename T, precision P>
	GLM_FUNC_DECL bool checkForZeroScaleInRow( const T & scale, const detail::tvec3<T, P> & row )
	{
		for( int i = 0; i < 3; ++i ) {
			if( ( abs( scale ) < 1 && abs( row[i] ) >= std::numeric_limits<T>::max() * abs( scale ) ) ) {
				return false;
			}
		}

		return true;
	}

	template <typename T, precision P>
	GLM_FUNC_DECL void extractEulerXYZ( const detail::tmat4x4<T, P> & mat, detail::tvec3<T, P> & angles )
	{
		// Normalize the local x, y and z axes to remove scaling.
		detail::tvec3<T, P> i( mat[0][0], mat[0][1], mat[0][2] );
		detail::tvec3<T, P> j( mat[1][0], mat[1][1], mat[1][2] );
		detail::tvec3<T, P> k( mat[2][0], mat[2][1], mat[2][2] );

		normalize( i );
		normalize( j );
		normalize( k );

		detail::tmat4x4<T, P> M( i[0], i[1], i[2], 0,
					j[0], j[1], j[2], 0,
					k[0], k[1], k[2], 0,
					0, 0, 0, 1 );

		// Extract the first angle, rot.x.
		angles.x = atan( M[1][2], M[2][2] );

		// Remove the rot.x rotation from M, so that the remaining
		// rotation, N, is only around two axes, and gimbal lock
		// cannot occur.
		detail::tmat4x4<T, P> N;
		N = rotate( N, -angles.x, detail::tvec3<T, P>( 1, 0, 0 ) );
		N = N * M;

		// Extract the other two angles, rot.y and rot.z, from N.
		float cy = sqrt( N[0][0] * N[0][0] + N[0][1] * N[0][1] );
		angles.y = atan( -N[0][2], cy );
		angles.z = atan( -N[1][0], N[1][1] );
	}

	template <typename T, precision P>
	GLM_FUNC_DECL detail::tquat<T, P> extractQuat( const detail::tmat4x4<T, P> & mat )
	{
		detail::tmat4x4<T, P> rot;

		T   tr, s;
		T   q[4];
		int i, j, k;
		detail::tquat<T, P> quat;

		int nxt[3] = { 1, 2, 0 };
		tr = mat[0][0] + mat[1][1] + mat[2][2];

		// check the diagonal
		if( tr > 0.0 ) {
			s = sqrt( tr + T( 1.0 ) );
			quat.w = s / T( 2.0 );
			s = T( 0.5 ) / s;

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
			s = sqrt( ( mat[i][i] - ( mat[j][j] + mat[k][k] ) ) + T( 1.0 ) );

			q[i] = s * T( 0.5 );
			if( s != T( 0.0 ) )
				s = T( 0.5 ) / s;

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

}//namespace glm