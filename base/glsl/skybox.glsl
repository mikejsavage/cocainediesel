in vec3      iResolution;           // viewport resolution (in pixels)
in float     iTime;                 // shader playback time (in seconds)
in float     iTimeDelta;            // render time (in seconds)
in int       iFrame;                // shader playback frame
in float     iChannelTime[4];       // channel playback time (in seconds)
in vec3      iChannelResolution[4]; // channel resolution (in pixels)
in vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
in vec4      iDate;                 // (year, month, day, time in seconds)
in float     iSampleRate;           // sound sample rate (i.e., 44100)
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
in vec2 fragCoord;
// Return random noise in the range [0.0, 1.0], as a function of x.
float Noise2d( in vec2 x )
{
    float xhash = cos( x.x * 37.0 );
    float yhash = cos( x.y * 57.0 );
    return fract( 415.92653 * ( xhash + yhash ) );
}

// Convert Noise2d() into a "star field" by stomping everthing below fThreshhold to zero.
float NoisyStarField( in vec2 vSamplePos, float fThreshhold )
{
    float StarVal = Noise2d( vSamplePos );
    if ( StarVal >= fThreshhold )
        StarVal = pow( (StarVal - fThreshhold)/(1.0 - fThreshhold), 6.0 );
    else
        StarVal = 0.0;
    return StarVal;
}

// Stabilize NoisyStarField() by only sampling at integer values.
float StableStarField( in vec2 vSamplePos, float fThreshhold )
{
    // Linear interpolation between four samples.
    // Note: This approach has some visual artifacts.
    // There must be a better way to "anti alias" the star field.
    float fractX = fract( vSamplePos.x );
    float fractY = fract( vSamplePos.y );
    vec2 floorSample = floor( vSamplePos );    
    float v1 = NoisyStarField( floorSample, fThreshhold );
    float v2 = NoisyStarField( floorSample + vec2( 0.0, 1.0 ), fThreshhold );
    float v3 = NoisyStarField( floorSample + vec2( 1.0, 0.0 ), fThreshhold );
    float v4 = NoisyStarField( floorSample + vec2( 1.0, 1.0 ), fThreshhold );

    float StarVal =   v1 * ( 1.0 - fractX ) * ( 1.0 - fractY )
        			+ v2 * ( 1.0 - fractX ) * fractY
        			+ v3 * fractX * ( 1.0 - fractY )
        			+ v4 * fractX * fractY;
	return StarVal;
}

void main()
{
// Sky Background Color
	vec3 vColor = vec3( 0.1, 0.2, 0.4 ) * fragCoord.y / iResolution.y;

    // Note: Choose fThreshhold in the range [0.99, 0.9999].
    // Higher values (i.e., closer to one) yield a sparser starfield.
    float StarFieldThreshhold = 0.97;

    // Stars with a slow crawl.
    float xRate = 0.2;
    float yRate = -0.06;
    vec2 vSamplePos = fragCoord.xy + vec2( xRate * float( iFrame ), yRate * float( iFrame ) );
	float StarVal = StableStarField( vSamplePos, StarFieldThreshhold );
    vColor += vec3( StarVal );
	
	fragColor = vec4(vColor, 1.0);
}

out vec4 fragColor;