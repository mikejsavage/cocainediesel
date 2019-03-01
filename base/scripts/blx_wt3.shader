textures/blx_wtest3/blx_wt3_surfmetal1
{
	qer_editorimage textures/blx_wtest3/blx_wt3_surfmetal1.tga

	{
		material textures/blx_wtest3/blx_wt3_surfmetal1.tga $blankBumpImage
	}
}

textures/blx_wtest3/blx_wt3_surfmetal2
{
	qer_editorimage textures/blx_wtest3/blx_wt3_surfmetal2.tga

	{
		material textures/blx_wtest3/blx_wt3_surfmetal2.tga $blankBumpImage textures/blx_wtest3/blx_wt3_surfmetal_gloss.tga
	}
}

textures/blx_wtest3/blx_wt3_surfmetal2_nosolid
{
	qer_editorimage textures/blx_wtest3/blx_wt3_surfmetal2.tga
	surfaceparm nonsolid

	{
		material textures/blx_wtest3/blx_wt3_surfmetal2.tga $blankBumpImage textures/blx_wtest3/blx_wt3_surfmetal_gloss.tga
	}
}

textures/blx_wtest3/blx_wt3_surfmetal3
{
	qer_editorimage textures/blx_wtest3/blx_wt3_surfmetal3.tga

	{
		material textures/blx_wtest3/blx_wt3_surfmetal3.tga $blankBumpImage textures/blx_wtest3/blx_wt3_surfmetal_gloss.tga
	}
}

textures/blx_wtest3/blx_wt3_grid
{
	qer_editorimage textures/grates/simplegrid.tga
	surfaceparm nomarks
	surfaceparm alphashadow
	cull none
	smallestmipmapsize 8

	{
		material textures/grates/simplegrid.tga
		//blendFunc blend
		alphaFunc GE128
		depthWrite
	}
}

textures/blx_wtest3/blx_wt3_grey
{
	qer_editorimage textures/blx_wtest3/blx_wt3_grey.tga

	{
		material textures/blx_wtest3/blx_wt3_grey.tga $blankbumpimage
	}
}
