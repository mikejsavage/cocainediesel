textures/grates/fence01
{
	qer_trans 0.5
	qer_editorimage textures/grates/fence01.tga
	surfaceparm trans
	surfaceparm nomarks
	//surfaceparm	nonsolid
	surfaceparm alphashadow
	cull none
	nomipmaps
	q3map_forceMeta
	q3map_lightmapsamplesize 64

	{
		material textures/grates/fence01.tga textures/grates/fence01_norm.tga
		alphaFunc GE128
		depthWrite
	}
}

textures/grates/fence01_noshadow
{
	qer_trans 0.5
	qer_editorimage textures/grates/fence01.tga
	surfaceparm trans
	surfaceparm nomarks
	cull none
	nomipmaps
	q3map_forceMeta
	q3map_lightmapsamplesize 64

	{
		material textures/grates/fence01.tga textures/grates/fence01_norm.tga
		alphaFunc GE128
		depthWrite
	}
}

textures/grates/simplegrid
{
	qer_trans 0.5
	qer_editorimage textures/grates/simplegrid.tga
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm alphashadow
	cull none
	smallestmipmapsize 8
	q3map_forceMeta
	q3map_lightmapsamplesize 64

	{
		material textures/grates/simplegrid.tga
		alphaFunc GE128
		depthWrite
	}
}
