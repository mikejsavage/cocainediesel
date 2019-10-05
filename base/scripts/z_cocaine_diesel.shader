models/objects/gibs/gib
{
	cull front

	{
		map $whiteImage
		rgbGen entity
	}
}

models/weapon_hits/lasergun/hit_blastexp
{
	cull none
	softParticle
	{
		map models/weapon_hits/lasergun/hit_blastexp
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbgen entity
		alphagen wave distanceramp 0 1 10 150
		tcMod rotate 1051
	}
}

sky
{
	{
		map $whiteimage
	}
}
