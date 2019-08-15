textures/hazelh/verticalborder2
{
	qer_editorimage textures/hazelh/verticalborder2.tga
	q3map_normali	mage textures/hazelh/verticalborder_norm.tga

	{
		material textures/hazelh/verticalborder2.tga textures/hazelh/verticalborder_norm.tga textures/hazelh/verticalborder_gloss.tga
	}
}

textures/hazelh/verticalborder2_whitelight
{
	qer_editorimage textures/hazelh/verticalborder2_whitelight.tga
	q3map_normalimage textures/hazelh/verticalborder_lights_norm.tga

	if deluxe
	{
		material textures/hazelh/verticalborder2_whitelight.tga textures/hazelh/verticalborder_lights_norm.tga textures/hazelh/verticalborder_gloss.tga textures/hazelh/verticalborder2_whitelight_bright.tga
	}
	endif

	if ! deluxe
	{
		map $lightmap
	}
	{
		map textures/hazelh/flat/verticalborder2_whitelight.tga
		blendfunc filter
	}
	{
		map textures/hazelh/verticalborder2_whitelight_bright.tga
		blendfunc add
	}
	endif
}

textures/hazelh/vertex_base_dblue2
{
	qer_editorimage textures/hazelh/base_dblue2.tga
	surfaceparm nolightmap

	{
		material textures/hazelh/base_dblue2.tga
		rgbgen vertex
	}
}

textures/hazelh/grate
{
	qer_editorimage textures/hazelh/grate.jpg
	surfaceparm metalsteps
	surfaceparm alphashadow
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm trans
	cull none

	{
		map textures/hazelh/grate.tga
		rgbgen vertex
		alphafunc GE128
		depthWrite
	}
}

textures/hazelh/glow_orange
{
	nomipmaps
	qer_editorimage textures/hazelh/glow_orange.tga
	cull front
	surfaceparm trans
	surfaceparm nonsolid
	//	qer_trans 0.5
	{
		map textures/hazelh/glow_orange.tga
		blendfunc add
	}
}

textures/hazelh/glow_orange_circle
{
	nomipmaps
	qer_editorimage textures/hazelh/glow_orange_circle.tga
	cull front
	surfaceparm trans
	surfaceparm nonsolid
	//	qer_trans 0.5
	{
		map textures/hazelh/glow_orange_circle.tga
		blendfunc add
	}
}
