textures/billboard/board_512_256_1
{
	qer_editorimage textures/billboard/bigbill1
	q3map_lightimage textures/billboard/bigbill1
	q3map_surfacelight 500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid

	{
		animClampMap 0.1 textures/billboard/bigbill1 textures/billboard/bigbill2 textures/billboard/bigbill5
		rgbGen wave sawtooth 0.5 1 0 .15
	}

	{
		clampmap textures/billboard/code
		blendfunc add
		tcmod scroll 0 0.3
		rgbGen wave sawtooth 0.1 0.3 0 .1
	}

	{
		map textures/billboard/scanlinenoise
		blendfunc add
		rgbGen wave sin 0.4 0.4 0 .5
		tcmod scroll 10 .15
	}
}

textures/billboard/board_512_256_2
{
	qer_editorimage textures/billboard/bigbill5
	q3map_lightimage textures/billboard/bigbill9
	q3map_surfacelight 500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid

	{
		animClampMap 0.1 textures/billboard/bigbill1 textures/billboard/bigbill6 textures/billboard/bigbill9
		rgbGen wave sawtooth 0.5 1 0 .15
	}

	{
		clampmap textures/billboard/code
		blendfunc add
		tcmod scroll 0 0.3
		rgbGen wave sawtooth 0.1 0.3 0 .1
	}

	{
		map textures/billboard/scanlinenoise
		blendfunc add
		rgbGen wave sin 0.4 0.4 0 .5
		tcmod scroll 10 .15
	}
}

textures/billboard/bigbill1
{
	qer_editorimage textures/billboard/bigbill1
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid

	{
		clampmap textures/billboard/bigbill1
	}
}

textures/billboard/bigbill7
{
	qer_editorimage textures/billboard/bigbill7
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid
	{
		clampmap textures/billboard/bigbill1
	}
}

textures/billboard/bigbill9
{
	qer_editorimage textures/billboard/bigbill9
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid
	{
		clampmap textures/billboard/bigbill9
	}
}

textures/billboard/small1
{
	qer_editorimage textures/billboard/small1
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid

	{
		map $lightmap
	}
	{
		clampmap textures/billboard/small1
		blendfunc filter
	}
	{
		clampmap textures/billboard/small1
		alphagen const 0.5
		blendFunc blend
	}
}

textures/billboard/small2
{
	qer_editorimage textures/billboard/small2
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid

	{
		map $lightmap
	}
	{
		clampmap textures/billboard/small2
		blendfunc filter
	}
	{
		clampmap textures/billboard/small2
		alphagen const 0.5
		blendFunc blend
	}
}

textures/billboard/small3
{
	qer_editorimage textures/billboard/small3
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid

	{
		map $lightmap
	}
	{
		clampmap textures/billboard/small3
		blendfunc filter
	}
	{
		clampmap textures/billboard/small3
		alphagen const 0.5
		blendFunc blend
	}
}

textures/billboard/small4
{
	qer_editorimage textures/billboard/small4
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid

	{
		map $lightmap
	}
	{
		clampmap textures/billboard/small4
		blendfunc filter
	}
	{
		clampmap textures/billboard/small4
		alphagen const 0.5
		blendFunc blend
	}
}

textures/billboard/bigver1
{
	qer_editorimage textures/billboard/bigver1
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid
	{
		clampmap textures/billboard/bigver1
	}
}

textures/billboard/neon1
{
	qer_editorimage textures/billboard/neon1
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid
	{
		clampmap textures/billboard/neon1
	}
}

textures/billboard/terebi
{
	qer_editorimage textures/billboard/terebi
	q3map_lightimage   textures/billboard/terebi_06
	q3map_surfacelight 500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm trans
	surfaceparm nonsolid
	{
		animmap 1 textures/billboard/terebi_01 textures/billboard/terebi_02 textures/billboard/terebi_03 textures/billboard/terebi_04 textures/billboard/terebi_05 textures/billboard/terebi_06 textures/billboard/terebi_07
	}
}

textures/billboard/bullets
{
	surfaceparm nomarks
	surfaceparm nonsolid
	qer_editorimage textures/billboard/bullets
	q3map_lightimage textures/billboard/bullets
	q3map_surfacelight 500
	{
		map textures/billboard/bullets
		blendfunc add
	}
}

textures/billboard/bulletsblue
{
	surfaceparm nomarks
	surfaceparm nonsolid
	qer_editorimage textures/billboard/bulletsblue
	q3map_lightimage textures/billboard/bulletsblue
	q3map_surfacelight 500
	{
		map textures/billboard/bulletsblue
		blendfunc add
	}
}

textures/billboard/PorkyPie
{
	qer_editorimage textures/billboard/PorkyPie
	q3map_surfacelight   500
	q3map_forceMeta
	q3map_lightmapSampleSize 64
	q3map_lightsubdivide 64
	surfaceparm	nomarks
	surfaceparm nonsolid
	{
		clampmap textures/billboard/PorkyPie
	}
}
