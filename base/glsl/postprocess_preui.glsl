#include "include/uniforms.glsl"
#include "include/common.glsl"

#if VERTEX_SHADER

layout( location = VertexAttribute_Position ) in vec4 a_Position;

void main() {
	gl_Position = a_Position;
}

#else

uniform sampler2D u_Screen;

layout( std140 ) uniform u_PostProcess {
	float u_Zoom;
	float u_Exposure;
	float u_Gamma;
	float u_Brightness;
	float u_Contrast;
	float u_Saturation;
};

layout( location = FragmentShaderOutput_Albedo ) out vec4 f_Albedo;

vec3 SampleScreen( vec2 uv ) {
	return texture( u_Screen, uv ).rgb;
}


float minc( vec3 c ) { return min( min( c.r, c.g ), c.b ); }
float maxc( vec3 c ) { return max( max( c.r, c.g ), c.b ); }




float ExposureLuminance( vec3 c ) { return 0.298839*c.r + 0.586811*c.g + 0.11435*c.b; }
float ExposureSaturation( vec3 c ) { return maxc( c ) - minc( c ); }

vec3 ExposureClipColor(vec3 c){
	float l = ExposureLuminance( c );
	float n = minc( c );
	float x = maxc( c );

	if( n < 0.0 ) {
		c = max( vec3( 0.0 ), ( c - l ) * l / ( l - n ) + l );
	}
	if( x > 1.0 ) {
		c = min( vec3( 1.0 ), ( c - l ) * ( 1.0 - l ) / ( x - l ) + l );
	}

	return c;
}

vec3 ExposureSetLuminance( vec3 c, float l ) {
	return ExposureClipColor( c + l - ExposureLuminance( c ) );
}

vec3 ExposureSetSaturation( vec3 c, float s ){
	float cmin = minc( c );
	float cmax = maxc( c );
	float cDiff = cmax - cmin;

	vec3 res = vec3( s, s, s );

	if( cmax > cmin ) {
		if( c.r == cmin && c.b == cmax ) { // R min G mid B max
			res.r = 0.0;
			res.g = ( ( c.g - cmin ) * s ) / cDiff;
		}
		else if( c.r == cmin && c.g == cmax ) { // R min B mid G max
			res.r = 0.0;
			res.b = ( ( c.b - cmin ) * s ) / cDiff;
		}
		else if( c.g == cmin && c.b == cmax ) { // G min R mid B max
			res.r = ( ( c.r - cmin ) * s ) / cDiff; 
			res.g = 0.0;
		}
		else if( c.g == cmin && c.r == cmax ) { // G min B mid R max
			res.g = 0.0;
			res.b = ( ( c.b - cmin ) * s ) / cDiff;
		}
		else if( c.b == cmin && c.r == cmax ) { // B min G mid R max
			res.g = ( ( c.g - cmin ) * s ) / cDiff;
			res.b = 0.0;
		}
		else { // B min R mid G max
			res.r = ( ( c.r - cmin ) * s ) / (cmax-cmin);
			res.b = 0.0;
		}
	}

	return res;
}


float ExposureRamp( float t ){
    t *= 2.0;
    if( t >= 1.0 ) {
      t -= 1.0;
      t = log( 0.5 ) / log( 0.5 * ( 1.0 - t ) + 0.9332 * t );
    }
    return clamp( t, 0.001, 10.0 );
}







vec3 colorCorrection( vec3 color ) {
	const vec3 LumCoeff = vec3( 0.2125, 0.7154, 0.0721 );

	color *= u_Brightness;

	float exposure = mix( 0.009, 0.98, u_Exposure );
	vec3 res = mix( color, vec3( 1.0 ), exposure );
    vec3 blend = mix( vec3(1.0), pow( color, vec3( 1.0/0.7 ) ), exposure );
    res = max( 1.0 - ( ( 1.0 - res ) / blend ), 0.0 );

    color = pow(
    	ExposureSetLuminance(
    		ExposureSetSaturation( color, ExposureSaturation( res ) ),
    		ExposureLuminance( res )
    	),
    	vec3( ExposureRamp( 1.0 - ( u_Gamma + 1.0 ) / 2.0 ) )
    );

 	vec3 AvgLumin = vec3( 0.5, 0.5, 0.5 );
 	vec3 intensity = vec3( dot( color, LumCoeff ) );

	vec3 satColor = mix( intensity, color, u_Saturation );
 	vec3 conColor = mix( AvgLumin, satColor, u_Contrast );

	return conColor;
}

vec3 radialBlur( vec2 uv ) {
	const int SAMPLES = 16;
	const float BLUR_INTENSITY = 0.03;
	const vec2 CENTER = vec2( 0.5, 0.5 );
	const float VIGNETTE_OFFSET = 0.45;

	vec3 col = vec3( 0.0 );
	vec2 dist = uv - CENTER;

	float len = length( dist );
	float l = sqrt( len );
	
	for( int j = 0; j < SAMPLES; j++ ) {
		float scale = 1.0 - ( l * BLUR_INTENSITY * u_Zoom * j ) / SAMPLES;
		col += SampleScreen( clamp( dist * scale + CENTER, vec2( 0.0, 0.0 ), vec2( 1.0, 1.0 ) ) );
	}

	col /= SAMPLES;

	// vignette effect
	float vignette = 1.0 - max( len - VIGNETTE_OFFSET, 0.0 ) * ( 1.0 + u_Zoom );
	return col * vignette;
}

void main() {
	vec2 uv = gl_FragCoord.xy / u_ViewportSize;
	
	vec3 color = texture( u_Screen, uv ).rgb;
	color = mix( color, radialBlur( uv ), u_Zoom );
	color = colorCorrection( color );

	f_Albedo.rgb = color;
}

#endif
