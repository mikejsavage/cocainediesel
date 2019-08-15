models/objects/projectile/glauncher/grenade
{
	{
		map $whiteimage
		rgbgen entity
	}
	// pulse
	{
		map $whiteimage
		rgbGen wave sin 0 0.5 1 3
		blendFunc add
	}
}

models/objects/projectile/plasmagun/plnew
{
	cull disable
	{
		map models/objects/projectile/plasmagun/plnew.tga
		blendFunc add
		tcmod rotate -150
		rgbgen entity
	}
}
