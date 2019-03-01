models/objects/projectile/glauncher/grenade
{
	{
		map models/objects/projectile/glauncher/grenade.tga
		rgbgen entity
	}
	// pulse
	{
		map models/objects/projectile/glauncher/grenade.tga
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
