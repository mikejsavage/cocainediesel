textures/wsw_cave1/stairs1
{
	qer_editorimage textures/wsw_cave1/stairs1.tga
	surfaceparm nomarks
	cull none
	sort alphatest

	{
		material
		alphafunc GE128
		depthwrite
	}
}

textures/wsw_cave1/scratchedmetal1
{
	qer_editorimage textures/wsw_cave1/scratchedmetal1.tga
	surfaceparm nomarks
	cull none

	{
		map $lightmap
	}
	{
		map textures/wsw_cave1/scratchedmetal1.tga
		blendfunc filter
	}
}
