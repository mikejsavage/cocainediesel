textures/leaves/leaves0108_s
{
	qer_editorimage textures/leaves/leaves0108_s.tga
	surfaceparm nomarks
	surfaceparm	nonsolid
	//surfaceparm alphashadow
	surfaceparm lightfilter      // Use texture's RGB and alpha channels to generate colored alpha
	surfaceparm trans
	cull none
	surfaceparm nolightmap

	{
		map textures/leaves/leaves0108_s.tga
		alphaFunc GE128
		rgbGen vertex
		depthWrite
	}
}

vertex/trees/barkdecidious048
{
	qer_editorimage textures/trees/barkdecidious048.tga
	surfaceparm nolightmap

	{
		rgbgen vertex
		material textures/trees/barkdecidious048.tga
	}
}
