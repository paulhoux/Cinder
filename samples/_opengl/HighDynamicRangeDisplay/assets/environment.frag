#version 330 core

uniform samplerCube uMap;
uniform int         uDivider = 0;
uniform float       uWhiteLevel = 80;

in vec3 vertDirection; // In world space.

out vec4 fragColor;

void main( void )
{
	vec3 D = normalize( vertDirection.xyz );
	fragColor = texture( uMap, D );

	// Clip colors to show the difference with SDR.
	if( gl_FragCoord.x < uDivider ) {
		fragColor.rgb = min( fragColor.rgb, vec3( 1.0 ) );
		// Adjust white level.
		fragColor.rgb *= uWhiteLevel / 80.0;
	}
}