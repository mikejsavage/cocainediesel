textures/wsw_cave1/stairs1
{
	qer_editorimage textures/wsw_cave1/stairs1
	surfaceparm nomarks
	cull none
	sort alphatest

	{
		material
		alphatest 0.5
	}
}

textures/wsw_cave1/scratchedmetal1
{
	qer_editorimage textures/wsw_cave1/scratchedmetal1
	surfaceparm nomarks
	cull none

	{
		map $lightmap
	}
	{
		map textures/wsw_cave1/scratchedmetal1
		blendfunc filter
	}
}
