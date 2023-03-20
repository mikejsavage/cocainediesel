models/static
{
	specular 3
	shininess 8
	shaded
	{
		map $whiteimage
		rgbGen const 0.35 0.35 0.35
	}
}

models/dynamic
{
	specular 3
	shininess 8
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
		map maps/decals/truckad
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
		map maps/decals/billboard
	}
}

models/map/cabinets/crt/screen
{
	surfaceparm nonsolid
	surfaceparm wallbangable
	shaded
	specular 9
	shininess 24
	{
		map $whiteimage
		rgbGen const 0.03 0.02 0.02
	}
}
