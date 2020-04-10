textures/factory/glass
{
	qer_editorimage textures/factory/glass
	qer_trans .5
	surfaceparm trans
	surfaceparm nolightmap
	cull disable

	{
		map textures/factory/glass
		tcGen environment
		blendFunc gl_one gl_one
	}
}

textures/factory/glass_singlesided_distancebloom
{
	qer_editorimage textures/factory/glass
	qer_trans .5
	surfaceparm trans
	surfaceparm nolightmap

	{
		map textures/factory/glass
		tcGen environment
		blendFunc gl_one gl_one
	}
}
