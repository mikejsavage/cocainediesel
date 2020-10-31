models/weapons/rlauncher/rlauncher_fx
{
	cull disable
	{
		map models/weapons/rlauncher/rlauncher_fx
		blendFunc add
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}

weapons/rl/exhaust
{
	cull disable
	{
		map weapons/rl/exhaust
		blendFunc add
		rgbGen entity
		tcmod rotate -360
	}
}

models/objects/projectile/rlauncher/rocket_flare_2
{
	cull disable
	{
		map weapons/rl/flare
		blendFunc add
		rgbGen wave triangle .1 .1 0 1
		tcmod scroll 3.2 0
	}
}
