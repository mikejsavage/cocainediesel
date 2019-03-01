textures/neon/neon_square
{
	surfaceparm nomarks
	surfaceparm nonsolid
	qer_editorimage textures/neon/neon_square.tga
	q3map_lightimage textures/neon/neon_square.tga
	q3map_surfacelight 50
	{
		map textures/neon/neon_square.tga
		blendfunc add
	}
}

textures/neon/basskick
{
	qer_editorimage textures/neon/basskick1.jpg
	q3map_lightimage   textures/neon/basskick1.jpg
	q3map_surfacelight 500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm trans
	surfaceparm nonsolid
	{
		animmap 1 textures/neon/basskick1.jpg textures/neon/basskick2.jpg textures/neon/basskick3.jpg textures/neon/basskick4.jpg
	}
}
