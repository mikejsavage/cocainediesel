models/mapobjects/nateleaf1/tree2
{
	cull disable
	surfaceparm lightfilter      // Use texture's RGB and alpha channels to generate colored alpha
	surfaceparm nonsolid
	surfaceparm nomarks
	surfaceparm noimpact
	surfaceparm nolightmap

	{
		map models/mapobjects/nateleaf1/tree2
		alphatest 0.5
		rgbGen vertex
	}
}

textures/natestah/nateswater
{
	qer_editorimage textures/natestah/nateswater
	surfaceparm nonsolid
	surfaceparm trans
	surfaceparm water
	cull disable
	deformVertexes wave 64 sin 0.25 0.25 0 0.5
	qer_trans 0.5
	q3map_globaltexture
	{
		map textures/natestah/nateswater
		blendfunc add
		rgbGen identity
		tcMod scale 0.5 0.5
		tcMod scroll 0.025 0.01
	}
}
