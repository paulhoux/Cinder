#version 330 core

uniform mat4 ciViewMatrix;
uniform mat4 ciProjectionMatrixInverse;

in vec4 ciPosition; // In world space.

out vec3 vertDirection; // In world space.

void main( void )
{
    // See for implementation details: https://gamedev.stackexchange.com/questions/60313/implementing-a-skybox-with-glsl-version-330/60377#60377
	mat3 inverseView = transpose( mat3( ciViewMatrix ) );
	vertDirection = inverseView * ( ciProjectionMatrixInverse * ciPosition ).xyz;

	vertDirection.x *= -1.0; // Convert to LH system from RH system.

	gl_Position = ciPosition;
}