gfx/misc/ebbeam
{
	cull none
	nomipmaps
	deformVertexes autosprite2
	{
		map gfx/misc/electro.tga
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
		map gfx/misc/laserbeam.tga
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
		map gfx/misc/laser.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen const 1 0.5 0
		alphaGen const 0.5
		tcMod scroll 10 0
	}
}

gfx/misc/testpoly
{
	//cull none
	cull back
	//deformVertexes wave 1000 sin 1 0 0 0
	deformVertexes autosprite2
	{
		map $whiteimage
		blendFunc GL_ONE GL_ZERO
	}
}

