mask/base
{
	shaded
	specular 10
	shininess 8
	{
		map $whiteimage
		rgbGen const 1.0 1.0 1.0
	}
}

mask/dark
{
	shaded
	specular 10
	shininess 8
	{
		map $whiteimage
		rgbGen const 0.25 0.25 0.25
	}
}
