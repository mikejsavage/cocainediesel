textures/leaves/leaves0108_s
{
	qer_editorimage textures/leaves/leaves0108_s
	surfaceparm nomarks
	surfaceparm	nonsolid
	//surfaceparm alphashadow
	surfaceparm lightfilter      // Use texture's RGB and alpha channels to generate colored alpha
	surfaceparm trans
	cull none
	surfaceparm nolightmap

	{
		map textures/leaves/leaves0108_s
		alphatest 0.5
		rgbGen vertex
	}
}
