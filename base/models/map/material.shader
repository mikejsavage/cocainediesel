models/static
{
	shaded
	{
		map $whiteimage
		rgbGen const 0.35 0.35 0.35
	}
}

models/static_wallbangable
{
	shaded
	{
		map $whiteimage
		rgbGen const 0.2 0.2 0.2
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
		map models/truckad
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
		map models/billboard
	}
}