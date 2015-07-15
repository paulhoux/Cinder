#version 150

uniform sampler3D uTexLUT;
uniform sampler2D uTexSrc;

in vec2 vertTexCoord0;

out vec4 fragColor;

void main(void)
{
	fragColor = texture( uTexSrc, vertTexCoord0 );
	fragColor.rgb = texture( uTexLUT, fragColor.rgb ).rgb;
}