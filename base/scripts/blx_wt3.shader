textures/blx_wtest3/blx_wt3_surfmetal1
{
	qer_editorimage textures/blx_wtest3/blx_wt3_surfmetal1

	{
		material textures/blx_wtest3/blx_wt3_surfmetal1 $blankBumpImage
	}
}

textures/blx_wtest3/blx_wt3_surfmetal2
{
	qer_editorimage textures/blx_wtest3/blx_wt3_surfmetal2

	{
		material textures/blx_wtest3/blx_wt3_surfmetal2 $blankBumpImage textures/blx_wtest3/blx_wt3_surfmetal_gloss
	}
}

textures/blx_wtest3/blx_wt3_surfmetal2_nosolid
{
	qer_editorimage textures/blx_wtest3/blx_wt3_surfmetal2
	surfaceparm nonsolid

	{
		material textures/blx_wtest3/blx_wt3_surfmetal2 $blankBumpImage textures/blx_wtest3/blx_wt3_surfmetal_gloss
	}
}

textures/blx_wtest3/blx_wt3_surfmetal3
{
	qer_editorimage textures/blx_wtest3/blx_wt3_surfmetal3

	{
		material textures/blx_wtest3/blx_wt3_surfmetal3 $blankBumpImage textures/blx_wtest3/blx_wt3_surfmetal_gloss
	}
}

textures/blx_wtest3/blx_wt3_grid
{
	qer_editorimage textures/grates/simplegrid
	surfaceparm nomarks
	surfaceparm alphashadow
	cull none
	smallestmipmapsize 8

	{
		material textures/grates/simplegrid
		alphatest 0.5
	}
}

textures/blx_wtest3/blx_wt3_grey
{
	qer_editorimage textures/blx_wtest3/blx_wt3_grey

	{
		material textures/blx_wtest3/blx_wt3_grey $blankbumpimage
	}
}
