textures/cocaine_diesel/ui_sky
{
	qer_editorimage env/ui/ui_ft.tga
	surfaceparm sky

	skyparms env/ui/ui - -
}

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
		map models/weapon_hits/lasergun/hit_blastexp.tga
		blendfunc GL_SRC_ALPHA GL_ONE
		rgbgen entity
		alphagen wave distanceramp 0 1 10 150
		tcMod turb 0 0.5 0 30
	}
}

sky
{
	cull none
	sort sky
	{
		map $whiteimage
	}
}
