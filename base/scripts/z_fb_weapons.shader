fullbright_weapon
{
	{
		map $whiteImage
		rgbGen const $1 $2 $3
	}
}

models/weapons/electrobolt/electrobolt
{
	template fullbright_weapon 0.314 0.953 1.000
}

models/weapons/glauncher/glauncher
{
	template fullbright_weapon 0.243 0.553 1.000
}

models/weapons/gunblade/gunblade
{
	template fullbright_weapon 0.502 0.502 0.502
}

models/weapons/gunblade/barrel
{
	template fullbright_weapon 0.502 0.502 0.502
}

models/weapons/lg
{
	template fullbright_weapon 0.322 0.988 0.372
}

models/weapons/plasmagun/plasmagun
{
	template fullbright_weapon 0.675 0.314 1.000
}

models/weapons/riotgun/riotgun
{
	template fullbright_weapon 1.000 0.675 0.118
}

models/weapons/rl
{
	template fullbright_weapon 1.000 0.227 0.259
}

models/weapons/machinegun/machinegun
{
	template fullbright_weapon 0.250 0.250 0.250
}

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

weapons/SMG/SMGtrail
{
	{
		blendfunc add
		map gfx/misc/laserbeam //todo
		rgbgen entity
		alphagen entity
	}
}

gfx/misc/laser
{
	{
		map gfx/misc/laser
		blendfunc add
		rgbGen const 1 0.5 0
		alphaGen const 0.5
		tcMod scroll 10 0
	}
}


