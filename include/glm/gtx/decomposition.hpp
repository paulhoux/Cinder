///////////////////////////////////////////////////////////////////////////////////
/// OpenGL Mathematics (glm.g-truc.net)
///
/// Copyright (c) 2005 - 2014 G-Truc Creation (www.g-truc.net)
/// Copyright (c) 2002 - 2012 Industrial Light & Magic, a division of Lucas
/// Digital Ltd. LLC
///
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
/// 
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
///
/// Neither the name of Industrial Light & Magic nor the names of
/// its contributors may be used to endorse or promote products derived
/// from this software without specific prior written permission. 
/// 
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
///
/// @ref gtx_decomposition
/// @file glm/gtx/decomposition.hpp
/// @date 2005-02-06
/// @author Paul Houx
///
/// @see core (dependence)
///
/// @defgroup gtx_decomposition GLM_GTX_decomposition
/// @ingroup gtx
/// 
/// @brief Allows matrix decomposition into translation, rotation, scale and shear.
/// 
/// <glm/gtx/decomposition.hpp> need to be included to use these functionalities.
///////////////////////////////////////////////////////////////////////////////////

#ifndef GLM_GTX_decomposition
#define GLM_GTX_decomposition

// Dependency:
#include "../glm.hpp"

#if(defined(GLM_MESSAGES) && !defined(GLM_EXT_INCLUDED))
#	pragma message("GLM: GLM_GTX_decomposition extension included")
#endif

namespace glm {
	/// @addtogroup gtx_decomposition
	/// @{

	/// Extracts scaling, shearing, rotation and translation from the given matrix, without modifying it. Rotation will be expressed in euler angles (X * Y * Z).
	/// @see gtx_decomposition
	template <typename T, precision P>
	GLM_FUNC_DECL bool extractSHRT(
		const detail::tmat4x4<T, P> &mat,
		detail::tvec3<T, P> & scale,
		detail::tvec3<T, P> & shear,
		detail::tvec3<T, P> & angles,
		detail::tvec3<T, P> & trans );

	/// Extracts, then removes, scaling and shearing from the given matrix.
	/// @see gtx_decomposition
	template <typename T, precision P>
	GLM_FUNC_DECL bool extractAndRemoveScalingAndShear(
		detail::tmat4x4<T, P> & mat,
		detail::tvec3<T, P> & scale,
		detail::tvec3<T, P> & shear );

	/// -
	/// @see gtx_decomposition
	template <typename T, precision P>
	GLM_FUNC_DECL bool checkForZeroScaleInRow(
		const T & scale,
		const ci::vec3 & row );

	/// Extracts euler angles (X * Y * Z) from the given matrix, without modifying it.
	/// @see gtx_decomposition
	template <typename T, precision P>
	GLM_FUNC_DECL void extractEulerXYZ(
		const  detail::tmat4x4<T, P> & mat,
		detail::tvec3<T, P> & angles );

	/// Extracts rotation as a quaternion from the given matrix, without modifying it.
	/// @see gtx_decomposition
	template <typename T, precision P>
	GLM_FUNC_DECL detail::tquat<T, P> extractQuat(
		const detail::tmat4x4<T, P> & mat );

	/// @}
}//namespace glm

#include "decomposition.inl"

#endif//GLM_GTX_decomposition