#version 150

const float kPi = 3.141592;
const float kInvPi = 1.0 / kPi;

uniform sampler2DRect uTex0;

in vec4 vertTexCoord0;
in vec3 vertSize;
in vec3 vertAttribs; // x = thickness, y = length, z= offset

out vec4 fragColor;

void main(void)
{
	float thickness = 1.0 - vertAttribs.x;
	float len = vertAttribs.y;

	vec2 pt = vertTexCoord0.xy * vec2( 2.0 ) - vec2( 1.0 );
	float radius = length( pt );

	float angle = atan( pt.x, pt. y ) * kInvPi * 0.5 + 1.0;
	angle = fract( angle - vertAttribs.z );

	float smoothness = fwidth( radius );
	float alpha = smoothstep( 1.0, 1.0 - smoothness, radius );
	alpha -= smoothstep( thickness, thickness - smoothness, radius );

	float segment = smoothstep( len + 0.002, len, angle );
	segment *= smoothstep( 0.0, 0.002, angle );    
	alpha *= mix( segment, 1.0, step( 1.0, len ) );

	alpha = clamp( alpha, 0.0, 1.0 );

	float aspect = vertSize.y / vertSize.x;
	float offset = 0.5 * (vertSize.x - vertSize.y);
	vec2  uv = vertTexCoord0.xy * vertSize.xy;
	uv.x *= aspect;
	uv.x += offset;
	vec3 color = texture( uTex0, uv ).rgb;

	fragColor.rgb = color;
	fragColor.a = alpha;
}