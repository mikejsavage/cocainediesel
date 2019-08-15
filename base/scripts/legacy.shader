textures/cha0s_ws/sw_xlight
{
	qer_editorimage textures/lights/blacktrim02.tga
	q3map_lightRGB 1 1 1
	q3map_surfacelight 1000
	q3map_lightsubdivide 72
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	q3map_lightmapSampleSize 128

	{
		map textures/lights/blacktrim02.tga
	}
}

textures/cha0s_ws/realflat_lt
{
	qer_editorimage textures/cha0s_ws/flat_lt.tga

	{
		material textures/cha0s_ws/flat_lt.tga
	}
}

textures/wsw_city1/concrete1_red
{
	qer_editorimage textures/concrete/plasterred01.tga

	{
		material textures/concrete/plasterred01.tga
	}
}

textures/wsw_city1/concrete2
{
	qer_editorimage textures/concrete/stucco01.tga

	{
		material textures/concrete/stucco01.tga
	}
}

textures/wsw_city1/concrete2c
{
	qer_editorimage textures/concrete/stucco01c.tga

	{
		material textures/concrete/stucco01c.tga textures/concrete/stucco01_norm.tga
	}
}

textures/wsw_city1/concrete4
{
	qer_editorimage textures/concrete/blueplaster.tga

	{
		material textures/concrete/blueplaster.tga
	}
}

textures/wsw_city1/ceil02
{
	qer_editorimage textures/cleansurface/pantone432c

	{
		material textures/cleansurface/pantone432c $blankbumpimage
	}
}

textures/wsw_city1/tech_concrete1
{
	qer_editorimage textures/concrete/concretecold01

	{
		material textures/concrete/concretecold01 textures/concrete/concretecold01_norm
		tcMod scale 2 2
	}
}

textures/wsw_city1/tech_concrete2
{
	qer_editorimage textures/concrete/concretecold02

	{
		material textures/concrete/concretecold02 textures/concrete/concretecold01_norm
		tcMod scale 2 2
	}
}

textures/wsw_city1/stepup01
{
	qer_editorimage textures/cleansurface/pantone425c

	{
		material textures/cleansurface/pantone425c $blankbumpimage
	}
}

textures/wsw_city1/stepup02
{
	qer_editorimage textures/cleansurface/pantone7536c

	{
		material textures/cleansurface/pantone7536c $blankbumpimage
	}
}

textures/wsw_city1/window_tall01
{
	qer_editorimage textures/window/purple01.tga

	{
		material textures/window/purple01
	}
}

textures/blx_wtest3/blx_wt3_dgrey
{
	qer_editorimage textures/cleansurface/pantonecoolgray10c.tga

	{
		material textures/cleansurface/pantonecoolgray10c $blankbumpimage
	}
}

textures/blx_wtest3/blx_wt3_pillar3
{
	qer_editorimage textures/blx_wtest3/blx_wt3_pillar3.tga

	{
		material textures/blx_wtest3/blx_wt3_pillar3.tga textures/blx_wtest3/blx_wt3_pillar3_norm.tga textures/blx_wtest3/blx_wt3_pillar3_gloss.tga
	}
}

textures/wsw_cave1/light_sphere1
{
	qer_editorimage textures/lights/oc3.tga
	q3map_lightimage textures/lights/oc3.tga
	q3map_surfacelight 20000
	q3map_lightsubdivide 72
	surfaceparm nomarks
	surfaceparm nodlight
	q3map_forcemeta
	surfaceparm nolightmap
	light 3
	flareshader textures/wsw_flareshalos/flare_sphere_white

	{
		map textures/lights/oc3.tga
	}
}

textures/wdm7/grid_tile
{
	qer_editorimage textures/metal/bigtreadplateg.tga

	if ! deluxe
	{
		map $lightmap
	}
	{
		map textures/metal/bigtreadplateg.tga
		blendFunc filter
	}
	endif

	if deluxe
	{
		material textures/metal/bigtreadplateg.tga
	}
	endif
}

textures/refly_wood/wood3
{
	qer_editorimage textures/wood/woodtables01.tga

	if ! deluxe
	{
		map $lightmap
	}
	{
		map textures/wood/woodtables01.tga
		blendFunc filter
	}
	endif

	if deluxe
	{
		material textures/wood/woodtables01.tga
	}
	endif
}

textures/terrain/rockwall01b
{
	qer_editorimage textures/terrain/rockwall01.tga

	{
		material textures/terrain/rockwall01.tga
	}
}
