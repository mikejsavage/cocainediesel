textures/cha0s_ws/glass
{
	qer_editorimage textures/cha0s_ws/chrome4.tga
	surfaceparm trans
	cull none
	qer_trans 0.5

	{
		map textures/cha0s_ws/chrome4.tga
		blendfunc add
		tcGen environment
		tcmod scale 2 2
	}
	{
		map textures/cha0s_ws/dirt.tga
		blendfunc blend
		tcmod scale .5 .5
	}
}

textures/cha0s_ws/cement_2
{
	qer_editorimage textures/concrete/concrete2.tga

	{
		material textures/concrete/concrete2.tga
	}
}
