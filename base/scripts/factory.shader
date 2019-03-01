textures/factory/glass
{
	qer_editorimage textures/factory/glass.tga
	qer_trans .5
	surfaceparm trans
	surfaceparm nolightmap
	cull disable

	{
		map textures/factory/glass.tga
		tcGen environment
		blendFunc gl_one gl_one
	}
}

textures/factory/glass_singlesided_distancebloom
{
	qer_editorimage textures/factory/glass.tga
	qer_trans .5
	surfaceparm trans
	surfaceparm nolightmap

	{
		map textures/factory/glass.tga
		tcGen environment
		blendFunc gl_one gl_one
	}

	{
		map textures/factory/glass.tga
		tcGen environment
		blendFunc gl_one gl_one
		//blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbgen wave distanceramp 0 1 200 500
	}
}

textures/factory/factory_wall_grey
{
	qer_editorimage textures/factory/factory_wall_grey.tga

	{
		material textures/factory/factory_wall_grey.tga textures/factory/factory_wall_norm.tga
	}
}
