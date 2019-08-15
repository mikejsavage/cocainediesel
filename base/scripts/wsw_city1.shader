textures/wsw_city1/wires1
{
	cull none
	surfaceparm nonsolid
	surfaceparm nomarks
	surfaceparm trans
	qer_editorimage textures/wsw_city1/wires1.tga
	surfaceparm nolightmap

	{
		map textures/wsw_city1/wires1.tga
		rgbgen vertex
		alphafunc GT0
		depthWrite
		blendfunc blend
	}
}

textures/wsw_city1/tech_wall5
{
	qer_editorimage textures/wsw_city1/tech_wall5.tga

	{
		material textures/wsw_city1/tech_wall5.tga textures/wsw_city1/tech_wall3_norm.tga textures/wsw_city1/tech_wall3_gloss.tga
	}
}

textures/wsw_city1/tech_concrete_tiles_mate
{
	qer_editorimage textures/wsw_city1/tech_concrete_tiles.tga

	{
		material textures/wsw_city1/tech_concrete_tiles.tga textures/wsw_city1/tech_concrete_tiles_norm.tga
	}
}

textures/wsw_city1/lighthalo1_high
{
	qer_editorimage textures/wsw_flareshalos/glow_halo_white.tga
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
		clampmap textures/wsw_flareshalos/glow_halo_white.tga
		blendfunc add
		rgbgen wave distanceramp 0 1 20 350
	}
}

textures/wsw_city1/grate_nosolid
{
	qer_editorimage textures/wsw_city1/grate.tga
	surfaceparm nomarks
	surfaceparm alphashadow
	surfaceparm nonsolid

	{
		material textures/wsw_city1/grate.tga
		alphaFunc GE128
		depthWrite
	}
}

textures/wsw_city1/tubes1_bulge
{
	qer_editorimage textures/wsw_city1/tubes1.tga
	deformVertexes bulge 4 7 -1.5 1

	{
		material textures/wsw_city1/tubes1.tga
	}
}

textures/wsw_city1/townsign01
{
	qer_editorimage textures/wsw_city1/townsign01.tga
	surfaceparm nomarks
	surfaceparm alphashadow
	surfaceparm nonsolid
	surfaceparm nolightmap
	cull none

	{
		rgbgen vertex
		material textures/wsw_city1/townsign01.tga
		alphaFunc GE128
		depthWrite
	}
}

textures/wsw_city1/light_bulb_side
{
	qer_editorimage textures/wsw_city1/light_bulb_side.tga
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nonsolid
	cull none
	sort banner
	deformVertexes autosprite2

	{
		map textures/wsw_city1/light_bulb_side.tga
		blendfunc blend
		rgbGen lightingIdentity
		alphaFunc GT0
	}
}

textures/wsw_city1/light_bulb_top
{
	qer_editorimage textures/wsw_city1/light_bulb_top.tga
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm nonsolid
	//cull none
	sort additive

	deformVertexes autosprite

	{
		map textures/wsw_city1/light_bulb_top.tga
		blendfunc blend
		//blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
		rgbGen lightingIdentity
		alphaFunc GT0
	}
}

textures/wsw_city1/light_bulb_corona
{
	qer_editorimage textures/wsw_city1/light_bulb_corona.tga
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
		clampmap textures/wsw_city1/light_bulb_corona.tga
		//blendfunc add
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbgen wave distanceramp 0.5 1 0 400
	}
}

textures/wsw_city1/stepside02
{
	qer_editorimage textures/wsw_city1/stepside02.tga

	{
		material textures/wsw_city1/stepside02.tga textures/wsw_city1/stepside01_norm.tga textures/wsw_city1/stepside01_gloss.tga
	}
}
