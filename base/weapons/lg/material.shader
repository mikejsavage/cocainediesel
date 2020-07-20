models/weapons/lg
{
	{
		map $whiteimage
		rgbGen const 0.1 0.1 0.1
	}
}

weapons/lg/beam
{
	softParticle
	{
		blendfunc add
		map weapons/lg/beam
		rgbgen entity
		alphagen entity
	}
}

models/weapons/lasergun/lasergun_fx
{
	{
		map weapons/lg/lg_fx
		blendFunc add
		tcmod scroll -2 0
	}
}
