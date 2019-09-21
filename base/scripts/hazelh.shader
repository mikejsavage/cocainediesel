textures/hazelh/verticalborder2
{
	qer_editorimage textures/hazelh/verticalborder2
	q3map_normali	mage textures/hazelh/verticalborder_norm

	{
		material textures/hazelh/verticalborder2 textures/hazelh/verticalborder_norm textures/hazelh/verticalborder_gloss
	}
}

textures/hazelh/verticalborder2_whitelight
{
	qer_editorimage textures/hazelh/verticalborder2_whitelight
	q3map_normalimage textures/hazelh/verticalborder_lights_norm

	if deluxe
	{
		material textures/hazelh/verticalborder2_whitelight textures/hazelh/verticalborder_lights_norm textures/hazelh/verticalborder_gloss textures/hazelh/verticalborder2_whitelight_bright
	}
	endif

	if ! deluxe
	{
		map $lightmap
	}
	{
		map textures/hazelh/flat/verticalborder2_whitelight
		blendfunc filter
	}
	{
		map textures/hazelh/verticalborder2_whitelight_bright
		blendfunc add
	}
	endif
}

textures/hazelh/vertex_base_dblue2
{
	qer_editorimage textures/hazelh/base_dblue2
	surfaceparm nolightmap

	{
		material textures/hazelh/base_dblue2
		rgbgen vertex
	}
}

textures/hazelh/grate
{
	qer_editorimage textures/hazelh/grate
	surfaceparm metalsteps
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm trans
	cull none

	{
		map textures/hazelh/grate
		rgbgen vertex
		alphatest 0.5
	}
}

textures/hazelh/glow_orange
{
	nomipmaps
	qer_editorimage textures/hazelh/glow_orange
	cull front
	surfaceparm trans
	surfaceparm nonsolid
	//	qer_trans 0.5
	{
		map textures/hazelh/glow_orange
		blendfunc add
	}
}

textures/hazelh/glow_orange_circle
{
	nomipmaps
	qer_editorimage textures/hazelh/glow_orange_circle
	cull front
	surfaceparm trans
	surfaceparm nonsolid
	//	qer_trans 0.5
	{
		map textures/hazelh/glow_orange_circle
		blendfunc add
	}
}
