gfx/misc/ebbeam
{
	cull none
	nomipmaps
	deformVertexes autosprite2
	{
		map gfx/misc/electro
		rgbgen vertex
		alphaGen vertex
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
	}
}

gfx/misc/lgbeam
{
	nomipmaps
	cull none
	deformVertexes autosprite2
	softParticle
	{
		map gfx/misc/laserbeam
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbgen vertex
	}
}

gfx/misc/laser
{
	nomipmaps
	cull none
	deformVertexes autosprite2
	{
		map gfx/misc/laser
		blendFunc blend
		rgbGen const 1 0.5 0
		alphaGen const 0.5
		tcMod scroll 10 0
	}
}

