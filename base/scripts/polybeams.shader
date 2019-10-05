gfx/misc/ebbeam
{
	{
		blendfunc add
		map gfx/misc/electro
		rgbgen entity
		alphagen entity
	}
}

gfx/misc/lgbeam
{
	softParticle
	{
		blendfunc add
		map gfx/misc/laserbeam
		rgbgen entity
		alphagen entity
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

