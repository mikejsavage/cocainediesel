textures/lights/01neutrel
{
	qer_editorimage textures/lights/01neutrel.tga
	q3map_lightimage textures/lights/01neutrel.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutrel.tga
	}
}

textures/lights/01neutrel_500
{
	qer_editorimage textures/lights/01neutrel.tga
	q3map_lightimage textures/lights/01neutrel.tga
	q3map_surfacelight 500
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutrel.tga
	}
}

textures/lights/01neutrel_5000
{
	qer_editorimage textures/lights/01neutrel.tga
	q3map_lightimage textures/lights/01neutrel.tga
	q3map_surfacelight 5000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutrel.tga
	}
}

textures/lights/01neutres
{
	qer_editorimage textures/lights/01neutres.tga
	q3map_lightimage textures/lights/01neutres.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutres.tga
	}
}

textures/lights/01neutres_500
{
	qer_editorimage textures/lights/01neutres.tga
	q3map_lightimage textures/lights/01neutres.tga
	q3map_surfacelight 500
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutres.tga
	}
}

textures/lights/01neutres_5000
{
	qer_editorimage textures/lights/01neutres.tga
	q3map_lightimage textures/lights/01neutres.tga
	q3map_surfacelight 5000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutres.tga
	}
}

textures/lights/01bluel
{
	qer_editorimage textures/lights/01bluel.tga
	q3map_lightimage textures/lights/01bluel.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01bluel.tga
	}
}

textures/lights/01bluel_1000
{
	qer_editorimage textures/lights/01bluel.tga
	q3map_lightimage textures/lights/01bluel.tga
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01bluel.tga
	}
}

textures/lights/01bluel_10000
{
	qer_editorimage textures/lights/01bluel.tga
	q3map_lightimage textures/lights/01bluel.tga
	q3map_surfacelight 10000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01bluel.tga
	}
}

textures/lights/01blues
{
	qer_editorimage textures/lights/01blues.tga
	q3map_lightimage textures/lights/01blues.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues.tga
	}
}

textures/lights/01blues_500
{
	qer_editorimage textures/lights/01blues.tga
	q3map_lightimage textures/lights/01blues.tga
	q3map_surfacelight 500
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues.tga
	}
}

textures/lights/01blues_1000
{
	qer_editorimage textures/lights/01blues.tga
	q3map_lightimage textures/lights/01blues.tga
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues.tga
	}
}

textures/lights/01blues_5000
{
	qer_editorimage textures/lights/01blues.tga
	q3map_lightimage textures/lights/01blues.tga
	q3map_surfacelight 5000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues.tga
	}
}

textures/lights/01blues_10000
{
	qer_editorimage textures/lights/01blues.tga
	q3map_lightimage textures/lights/01blues.tga
	q3map_surfacelight 10000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues.tga
	}
}

textures/lights/blacktrim01_1000
{
	qer_editorimage textures/lights/blacktrim01
	q3map_lightRGB 1 1 1
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/blacktrim01.tga
	}
}

textures/lights/tech
{
	qer_editorimage textures/lights/tech.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	q3map_lightmapSampleSize 128

	if ! deluxe
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/lights/tech.tga
		blendfunc filter
	}
	{
		map textures/lights/tech.blend.tga
		blendfunc add
	}
	endif

	if deluxe
	{
		material textures/lights/tech.tga $blankbumpimage - textures/lights/tech.blend.tga
	}
	endif
}

textures/lights/tech_1000
{
	qer_editorimage textures/lights/tech.tga
	q3map_lightimage   textures/lights/tech.tga
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	q3map_lightmapSampleSize 128

	if ! deluxe
	{
		map $lightmap
		rgbGen identity
	}
	{
		map textures/lights/tech.tga
		blendfunc filter
	}
	{
		map textures/lights/tech.blend.tga
		blendfunc add
	}
	endif

	if deluxe
	{
		material textures/lights/tech.tga $blankbumpimage - textures/lights/tech.blend.tga
	}
	endif
}

textures/lights/square_light_tile
{
	qer_editorimage textures/lights/square_light_tile.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map textures/lights/square_light_tile.tga
	}
}

textures/lights/square_light_tile_2000
{
	qer_editorimage textures/lights/square_light_tile.tga
	q3map_lightRGB 1 0.88 0.47
	q3map_surfacelight 2000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map textures/lights/square_light_tile.tga
	}
}

textures/lights/panel1
{
	qer_editorimage textures/lights/panel1.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map textures/lights/panel1.tga
	}
}

textures/lights/panel1_500
{
	qer_editorimage textures/lights/panel1.tga
	q3map_lightimage   textures/lights/panel1.tga
	q3map_surfacelight 500
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map textures/lights/panel1.tga
	}
}

textures/lights/purewhite
{
	qer_editorimage textures/lights/purewhite.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map $whiteimage
	}
}

textures/lights/purewhite_1000
{
	qer_editorimage textures/lights/purewhite.tga
	q3map_lightimage   textures/lights/purewhite.tga
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map $whiteimage
	}
}

textures/lights/bright2
{
	qer_editorimage textures/lights/bright2.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/bright2.tga
	}
}

textures/lights/bright2_1000
{
	qer_editorimage textures/lights/bright2.tga
	q3map_lightimage textures/lights/bright2.tga
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/bright2.tga
	}
}

textures/lights/bright2_2500
{
	qer_editorimage textures/lights/bright2.tga
	q3map_lightimage textures/lights/bright2.tga
	q3map_surfacelight 2500
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/bright2.tga
	}
}

textures/lights/turqs2
{
	qer_editorimage textures/lights/turqs2.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta

	if ! deluxe
	{
		map textures/lights/turqs2.tga
		blendFunc blend
	}

	{
		map $lightmap
		blendFunc filter
	}
	endif

	if deluxe
	{
		material textures/lights/turqs2.tga
		blendFunc blend
	}
	endif

	{
		map textures/lights/turqs2.blend.tga
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

textures/lights/turqs2_1000
{
	qer_editorimage textures/lights/turqs2.tga
	q3map_lightimage textures/lights/turqs2.blend.tga
	q3map_surfacelight 1000
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta

	if ! deluxe
	{
		map textures/lights/turqs2.tga
		blendFunc blend
	}

	{
		map $lightmap
		blendFunc filter
	}
	endif

	if deluxe
	{
		material textures/lights/turqs2.tga
		blendFunc blend
	}
	endif

	{
		map textures/lights/turqs2.blend.tga
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

textures/lights/oc3
{
	qer_editorimage textures/lights/oc3.tga
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/oc3.tga
	}
}

textures/lights/oc3_1000
{
	qer_editorimage textures/lights/oc3.tga
	q3map_lightimage textures/lights/oc3.tga
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/oc3.tga
	}
}

textures/lights/oc3_10000
{
	qer_editorimage textures/lights/oc3.tga
	q3map_lightimage textures/lights/oc3.tga
	q3map_surfacelight 10000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/oc3.tga
	}
}

textures/lights/strip
{
	qer_editorimage textures/lights/strip.blend.tga
	q3map_lightimage textures/lights/strip.blend.tga
	q3map_surfacelight 100
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta

	if ! deluxe
	{
		map $lightmap
	}

	{
		map textures/lights/strip.tga
		blendFunc filter
	}

	{
		map textures/lights/strip.blend.tga
		blendFunc blend
	}
	endif

	if deluxe
	{
		material textures/lights/strip.tga textures/lights/strip_norm.tga textures/lights/strip_gloss.tga textures/lights/strip.blend.tga
	}
	endif
}

textures/lights/stripred
{
	qer_editorimage textures/lights/stripred.blend.tga
	q3map_lightimage textures/lights/stripred.blend.tga
	q3map_surfacelight 100
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta

	if ! deluxe
	{
		map $lightmap
	}

	{
		map textures/lights/strip.tga
		blendFunc filter
	}

	{
		map textures/lights/stripred.blend.tga
		blendFunc blend
	}
	endif

	if deluxe
	{
		material textures/lights/strip.tga textures/lights/strip_norm.tga textures/lights/strip_gloss.tga textures/lights/stripred.blend.tga
	}
	endif
}
