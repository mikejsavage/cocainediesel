textures/wsw_city1/wires1
{
	cull none
	surfaceparm nonsolid
	surfaceparm nomarks
	surfaceparm trans
	qer_editorimage textures/wsw_city1/wires1
	surfaceparm nolightmap

	{
		map textures/wsw_city1/wires1
		rgbgen vertex
		alphafunc GT0
		depthWrite
		blendfunc blend
	}
}

textures/wsw_city1/lighthalo1_high
{
	qer_editorimage textures/wsw_flareshalos/glow_halo_white
	cull none
	surfaceparm nomarks
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	deformVertexes autosprite2
	forceoverlays
	qer_trans 0.6

	{
		detail
		clampmap textures/wsw_flareshalos/glow_halo_white
		blendfunc add
		rgbgen wave distanceramp 0 1 20 350
	}
}

textures/wsw_city1/grate_nosolid
{
	qer_editorimage textures/wsw_city1/grate
	surfaceparm nomarks
	surfaceparm alphashadow
	surfaceparm nonsolid

	{
		map textures/wsw_city1/grate
		alphatest 0.5
	}
}

textures/wsw_city1/tubes1_bulge
{
	qer_editorimage textures/wsw_city1/tubes1
	deformVertexes bulge 4 7 -1.5 1

	{
		map textures/wsw_city1/tubes1
	}
}

textures/wsw_city1/townsign01
{
	qer_editorimage textures/wsw_city1/townsign01
	surfaceparm nomarks
	surfaceparm alphashadow
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none

	{
		rgbgen vertex
		map textures/wsw_city1/townsign01
		alphatest 0.5
	}
}

textures/wsw_city1/light_bulb_side
{
	qer_editorimage textures/wsw_city1/light_bulb_side
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nonsolid
	cull none
	sort banner
	deformVertexes autosprite2

	{
		map textures/wsw_city1/light_bulb_side
		blendfunc blend
		rgbGen lightingIdentity
		alphaFunc GT0
	}
}

textures/wsw_city1/light_bulb_top
{
	qer_editorimage textures/wsw_city1/light_bulb_top
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nonsolid
	//cull none
	sort additive

	deformVertexes autosprite

	{
		map textures/wsw_city1/light_bulb_top
		blendfunc blend
		rgbGen lightingIdentity
		alphaFunc GT0
	}
}

textures/wsw_city1/light_bulb_corona
{
	qer_editorimage textures/wsw_city1/light_bulb_corona
	qer_trans 0.1
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nonsolid
	//cull none
	sort 7

	deformVertexes autosprite

	{
		detail
		clampmap textures/wsw_city1/light_bulb_corona
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbgen wave distanceramp 0.5 1 0 400
	}
}
