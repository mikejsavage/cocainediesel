textures/emtown/vine1
{
	qer_editorimage textures/emtown/vine1.tga
	qer_trans 0.9
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight
	surfaceparm nonsolid
	surfaceparm trans
	polygonOffset

	{
		detail
		map textures/emtown/vine1.tga
		blendFunc blend
		//blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen Vertex
	}
}

textures/emtown/emtag
{
	qer_editorimage textures/emtown/emtag.tga
	qer_trans 0.3
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight
	surfaceparm nonsolid
	surfaceparm trans
	polygonOffset

	{
		detail
		map textures/emtown/emtag.tga
		blendFunc filter
		//blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen Vertex
	}
}

textures/emtown/fztag
{
	qer_editorimage textures/emtown/fztag.tga
	qer_trans 0.3
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight
	surfaceparm nonsolid
	surfaceparm trans
	polygonOffset

	{
		detail
		map textures/emtown/fztag.tga
		blendFunc filter
		//blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen Vertex
	}
}

textures/emtown/ilverdi2
{
	qer_editorimage textures/emtown/ilverdi2.tga
	qer_trans 0.3
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight
	surfaceparm nonsolid
	surfaceparm trans
	polygonOffset

	{
		detail
		map textures/emtown/ilverdi2.tga
		blendFunc filter
		//blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen Vertex
	}
}

textures/emtown/b
{
	qer_editorimage textures/emtown/b.tga
	qer_trans 0.3
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight
	surfaceparm nonsolid
	surfaceparm trans
	polygonOffset

	{
		detail
		map textures/emtown/b.tga
		blendFunc filter
		//blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen Vertex
	}
}

textures/emtown/thisway
{
	qer_editorimage textures/emtown/thisway.tga
	qer_trans 0.5
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nonsolid
	surfaceparm trans
	polygonOffset

	{
		detail
		map textures/emtown/thisway.tga
		blendFunc filter
		//blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
		//blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		//rgbGen Vertex
	}
}

textures/emtown/lightwhite
{
	qer_editorimage textures/8x8/fafad2.tga
	q3map_surfacelight   400
	surfaceparm	nomarks
	surfaceparm	nolightmap
	surfaceparm trans
	surfaceparm nonsolid
	cull none
	{
		clampmap textures/8x8/fafad2.tga
	}
}

textures/emtown/lightf4a460
{
	qer_editorimage textures/8x8/f4a460.tga
	q3map_surfacelight   400
	surfaceparm	nomarks
	surfaceparm	nolightmap
	surfaceparm trans
	surfaceparm nonsolid
	cull none
	{
		clampmap textures/8x8/f4a460.tga
	}
}

textures/grates/fence01nolightmap
{
	qer_trans 0.5
	qer_editorimage textures/grates/fence01.tga
	surfaceparm trans
	surfaceparn nolightmap
	surfaceparm nomarks
	//surfaceparm	nonsolid
	//surfaceparm alphashadow
	sort additive
	cull none
	nomipmaps
	q3map_forceMeta
	q3map_lightmapsamplesize 64

	if deluxe
	{
		material textures/grates/fence01.tga textures/grates/fence01_norm.tga
		alphaFunc GE128
		blendFunc blend
		depthWrite
	}
	endif

	if ! deluxe
	{
		map textures/grates/fence01.tga
		blendfunc blend
		alphaFunc GE128
		depthWrite
	}

	{
		map $lightmap
		rgbGen identity
		blendFunc filter
		depthFunc equal
	}
	endif
}

textures/emtown/red1nonsolid
{
	qer_editorimage textures/8x8/red1.tga
	surfaceparm nolightmap
	surfaceparm nomarks
	surfaceparm nodlight
	surfaceparm nonsolid
	surfaceparm trans
	polygonOffset

	{
		detail
		map textures/8x8/red1.tga
		//blendFunc filter
		//blendFunc GL_SRC_ALPHA GL_ONE // blendfunc add the alphamasked part only
		//blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen Vertex
	}
}
