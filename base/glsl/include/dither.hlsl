float3 Dither() {
	float2 texture_size;
	u_BlueNoiseTexture.GetDimensions( texture_size.x, texture_size.y );
	float blue_noise = u_BlueNoiseTexture.Sample( u_StandardSampler, SV_Position.xy / texture_size ).x;
	return ( blue_noise - 0.5f ) / 256.0f;
}
