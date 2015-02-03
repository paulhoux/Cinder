#version 150

uniform mat4 ciModelViewProjection;

in vec4 ciPosition;
in vec4 ciTexCoord0;
in vec4 ciColor;

in vec3 vInstanceSize; // per-instance
in vec4 vInstancePosition; // per-instance: 2D position and radius, w = unused
in vec4 vInstanceAttribs; // per-instance: thickness and length, z = offset, w = texture index

out vec4 vertTexCoord0;
out vec3 vertSize;
out vec3 vertAttribs;

void main(void)
{
	vertTexCoord0 = ciTexCoord0;
	vertSize = vInstanceSize;
	vertAttribs = vInstanceAttribs.xyz;

	vec4 position = ciPosition;
	position.xy *= vInstancePosition.z;
	position.xy += vInstancePosition.xy;

	gl_Position = ciModelViewProjection * position;
}