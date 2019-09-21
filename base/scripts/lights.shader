textures/lights/01neutrel
{
	qer_editorimage textures/lights/01neutrel
	q3map_lightimage textures/lights/01neutrel
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutrel
	}
}

textures/lights/01neutrel_500
{
	qer_editorimage textures/lights/01neutrel
	q3map_lightimage textures/lights/01neutrel
	q3map_surfacelight 500
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutrel
	}
}

textures/lights/01neutrel_5000
{
	qer_editorimage textures/lights/01neutrel
	q3map_lightimage textures/lights/01neutrel
	q3map_surfacelight 5000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutrel
	}
}

textures/lights/01neutres
{
	qer_editorimage textures/lights/01neutres
	q3map_lightimage textures/lights/01neutres
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutres
	}
}

textures/lights/01neutres_500
{
	qer_editorimage textures/lights/01neutres
	q3map_lightimage textures/lights/01neutres
	q3map_surfacelight 500
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutres
	}
}

textures/lights/01neutres_5000
{
	qer_editorimage textures/lights/01neutres
	q3map_lightimage textures/lights/01neutres
	q3map_surfacelight 5000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01neutres
	}
}

textures/lights/01bluel
{
	qer_editorimage textures/lights/01bluel
	q3map_lightimage textures/lights/01bluel
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01bluel
	}
}

textures/lights/01bluel_1000
{
	qer_editorimage textures/lights/01bluel
	q3map_lightimage textures/lights/01bluel
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01bluel
	}
}

textures/lights/01bluel_10000
{
	qer_editorimage textures/lights/01bluel
	q3map_lightimage textures/lights/01bluel
	q3map_surfacelight 10000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01bluel
	}
}

textures/lights/01blues
{
	qer_editorimage textures/lights/01blues
	q3map_lightimage textures/lights/01blues
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues
	}
}

textures/lights/01blues_500
{
	qer_editorimage textures/lights/01blues
	q3map_lightimage textures/lights/01blues
	q3map_surfacelight 500
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues
	}
}

textures/lights/01blues_1000
{
	qer_editorimage textures/lights/01blues
	q3map_lightimage textures/lights/01blues
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues
	}
}

textures/lights/01blues_5000
{
	qer_editorimage textures/lights/01blues
	q3map_lightimage textures/lights/01blues
	q3map_surfacelight 5000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues
	}
}

textures/lights/01blues_10000
{
	qer_editorimage textures/lights/01blues
	q3map_lightimage textures/lights/01blues
	q3map_surfacelight 10000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/01blues
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
		map textures/lights/blacktrim01
	}
}

textures/lights/tech
{
	qer_editorimage textures/lights/tech
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
		map textures/lights/tech
		blendfunc filter
	}
	{
		map textures/lights/tech.blend
		blendfunc add
	}
	endif

	if deluxe
	{
		material textures/lights/tech $blankbumpimage - textures/lights/tech.blend
	}
	endif
}

textures/lights/tech_1000
{
	qer_editorimage textures/lights/tech
	q3map_lightimage   textures/lights/tech
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
		map textures/lights/tech
		blendfunc filter
	}
	{
		map textures/lights/tech.blend
		blendfunc add
	}
	endif

	if deluxe
	{
		material textures/lights/tech $blankbumpimage - textures/lights/tech.blend
	}
	endif
}

textures/lights/square_light_tile
{
	qer_editorimage textures/lights/square_light_tile
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map textures/lights/square_light_tile
	}
}

textures/lights/square_light_tile_2000
{
	qer_editorimage textures/lights/square_light_tile
	q3map_lightRGB 1 0.88 0.47
	q3map_surfacelight 2000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map textures/lights/square_light_tile
	}
}

textures/lights/panel1
{
	qer_editorimage textures/lights/panel1
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map textures/lights/panel1
	}
}

textures/lights/panel1_500
{
	qer_editorimage textures/lights/panel1
	q3map_lightimage   textures/lights/panel1
	q3map_surfacelight 500
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forceMeta
	surfaceparm nolightmap

	{
		map textures/lights/panel1
	}
}

textures/lights/purewhite
{
	qer_editorimage textures/lights/purewhite
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
	qer_editorimage textures/lights/purewhite
	q3map_lightimage   textures/lights/purewhite
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
	qer_editorimage textures/lights/bright2
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/bright2
	}
}

textures/lights/bright2_1000
{
	qer_editorimage textures/lights/bright2
	q3map_lightimage textures/lights/bright2
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/bright2
	}
}

textures/lights/bright2_2500
{
	qer_editorimage textures/lights/bright2
	q3map_lightimage textures/lights/bright2
	q3map_surfacelight 2500
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/bright2
	}
}

textures/lights/turqs2
{
	qer_editorimage textures/lights/turqs2
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta

	if ! deluxe
	{
		map textures/lights/turqs2
		blendFunc blend
	}

	{
		map $lightmap
		blendFunc filter
	}
	endif

	if deluxe
	{
		material textures/lights/turqs2
		blendFunc blend
	}
	endif

	{
		map textures/lights/turqs2.blend
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

textures/lights/turqs2_1000
{
	qer_editorimage textures/lights/turqs2
	q3map_lightimage textures/lights/turqs2.blend
	q3map_surfacelight 1000
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta

	if ! deluxe
	{
		map textures/lights/turqs2
		blendFunc blend
	}

	{
		map $lightmap
		blendFunc filter
	}
	endif

	if deluxe
	{
		material textures/lights/turqs2
		blendFunc blend
	}
	endif

	{
		map textures/lights/turqs2.blend
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

textures/lights/oc3
{
	qer_editorimage textures/lights/oc3
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/oc3
	}
}

textures/lights/oc3_1000
{
	qer_editorimage textures/lights/oc3
	q3map_lightimage textures/lights/oc3
	q3map_surfacelight 1000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/oc3
	}
}

textures/lights/oc3_10000
{
	qer_editorimage textures/lights/oc3
	q3map_lightimage textures/lights/oc3
	q3map_surfacelight 10000
	q3map_lightsubdivide 128
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap

	{
		map textures/lights/oc3
	}
}

textures/lights/strip
{
	qer_editorimage textures/lights/strip.blend
	q3map_lightimage textures/lights/strip.blend
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
		map textures/lights/strip
		blendFunc filter
	}

	{
		map textures/lights/strip.blend
		blendFunc blend
	}
	endif

	if deluxe
	{
		material textures/lights/strip textures/lights/strip_norm textures/lights/strip_gloss textures/lights/strip.blend
	}
	endif
}

textures/lights/stripred
{
	qer_editorimage textures/lights/stripred.blend
	q3map_lightimage textures/lights/stripred.blend
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
		map textures/lights/strip
		blendFunc filter
	}

	{
		map textures/lights/stripred.blend
		blendFunc blend
	}
	endif

	if deluxe
	{
		material textures/lights/strip textures/lights/strip_norm textures/lights/strip_gloss textures/lights/stripred.blend
	}
	endif
}
