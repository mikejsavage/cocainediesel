models/static
{
	shaded
	specular 3
	shininess 8
	{
		map $whiteimage
		rgbGen const 0.45 0.45 0.45
	}
}

models/static_wallbangable
{
	shaded
	specular 3
	shininess 8
	{
		map $whiteimage
		rgbGen const 0.306 0.306 0.306
	}
}

models/static_glass
{
	shaded
	specular 100
	shininess 100
	{
		map $whiteimage
		rgbGen const 0.05 0.05 0.05
	}
}

models/signs/truckad
{
	surfaceparm nonsolid
	surfaceparm wallbangable
	shaded
	// specular 3
	// shininess 8
	{
		blendfunc blend
		// map models/truckad
		map $whiteimage
	}
}

models/signs/billboard
{
	surfaceparm nonsolid
	surfaceparm wallbangable
	shaded
	// specular 3
	// shininess 8
	{
		blendfunc blend
		// map models/billboard
		map $whiteimage
	}
}
