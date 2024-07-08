float LambertLight( vec3 normal, vec3 lightDir ) {
	return dot( normal, lightDir );
}

float SpecularLight( vec3 normal, vec3 lightDir, vec3 viewDir, float shininess ) {
	vec3 reflectDir = reflect( lightDir, normal );
	return pow( max( 0.0, dot( viewDir, reflectDir ) ), shininess );
}
