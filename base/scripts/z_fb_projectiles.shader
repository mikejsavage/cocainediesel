models/objects/projectile/glauncher/grenade
{
	{
		map $whiteimage
		rgbgen entitycolorwave 1 1 1 sin 0 0.5 1 3
	}
}

models/objects/projectile/plasmagun/plnew
{
	cull disable
	{
		map models/objects/projectile/plasmagun/plnew
		blendFunc add
		tcmod rotate -150
		rgbgen entity
	}
}
