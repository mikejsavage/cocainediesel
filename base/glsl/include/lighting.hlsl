#pragma once

float LambertLight( float3 normal, float3 lightDir ) {
	return dot( normal, lightDir );
}

float SpecularLight( float3 normal, float3 lightDir, float3 viewDir, float shininess ) {
	float3 reflectDir = reflect( lightDir, normal );
	return pow( max( 0.0f, dot( viewDir, reflectDir ) ), shininess );
}
