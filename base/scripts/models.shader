//------------
//WEAPONS
//------------

models/weapons/electrobolt/electrobolt_fx
{
	cull disable
	{
		map models/weapons/electrobolt/electrobolt_fx
		blendFunc add
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}
models/weapons/electrobolt/electrobolt_fx_1
{
	cull disable
	//deformVertexes autosprite2
	{
		animmap 15 models/weapons/electrobolt/electrobolt_fx_1 models/weapons/electrobolt/electrobolt_fx_2 models/weapons/electrobolt/electrobolt_fx_3 models/weapons/electrobolt/electrobolt_fx_4 models/weapons/electrobolt/electrobolt_fx_5 models/weapons/electrobolt/electrobolt_fx_6 models/weapons/electrobolt/electrobolt_fx_7 models/weapons/electrobolt/electrobolt_fx_8
		blendfunc add
	}
}

models/weapons/glauncher/glauncher_fx
{
	cull disable
	{
		map models/weapons/glauncher/glauncher_fx
		blendFunc add
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}

models/weapons/plasmagun/plasmagun_fx
{
	cull disable
	{
		map models/weapons/plasmagun/plasmagun_fx
		blendFunc add
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}

models/weapons/lg_fx
{
	cull disable
	{
		map models/weapons/lg_fx
		blendFunc add
		tcmod scroll -2 0
	}
	{
		map models/weapons/lg_fx
		blendFunc add
		tcmod scroll 2 0
	}
}

//-----------------
//FLASH WEAPONS
//-----------------

models/weapons/plasmagun/f_plasma
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/plasmagun/f_plasma
		rgbgen entity
		tcmod rotate 90
		blendFunc add
	}
}
models/weapons/plasmagun/f_plasma_2
{
	sort nearest
	cull disable
	//deformVertexes autosprite2
	softParticle
	{
		map models/weapons/plasmagun/f_plasma_2
		rgbgen entity
		blendFunc add
	}
}

models/weapons/riotgun/f_riot
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/riotgun/f_riot
		rgbgen entity
		blendfunc add
	}
	{
		map models/weapons/riotgun/f_riot_spark
		rgbgen entity
		tcMod stretch sawtooth .65 .3 0 8
		blendFunc add
	}
}
models/weapons/riotgun/f_riot_2
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/riotgun/f_riot_2
		rgbgen entity
		blendfunc add
	}
}
models/weapons/riotgun/f_riot_3
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/riotgun/f_riot_3
		rgbgen entity
		blendfunc add
	}
}
models/weapons/gunblade/f_gunblade
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/gunblade/f_gunblade
		rgbgen entity
		tcmod rotate 200
		blendFunc add
	}
	{
		map models/weapons/gunblade/f_gunblade_1
		rgbgen entity
		tcmod rotate -175
		blendFunc add
	}
}
models/weapons/gunblade/f_gunblade_2
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/gunblade/f_gunblade_2
		rgbgen entity
		blendfunc add
	}
}

models/weapons/electrobolt/f_electrobolt
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/electrobolt/f_electro
		rgbgen entity
		tcmod rotate 90
		blendFunc add
	}
}

models/weapons/electrobolt/f_electrobolt_2
{
	sort nearest
	cull disable
	//deformVertexes autosprite2
	softParticle
	{
		map models/weapons/electrobolt/f_electro_2
		rgbgen entity
		blendFunc add
	}
}
models/weapons/electrobolt/f_electrobolt_3
{
	sort nearest
	cull disable
	softParticle
	{
		animMap 6 models/weapons/electrobolt/f_electro_3 models/weapons/electrobolt/f_electro_4 models/weapons/electrobolt/f_electro_5 models/weapons/electrobolt/f_electro_6
		rgbgen entity
		blendfunc add
	}
}

models/weapons/lasergun/f_laser
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/lasergun/f_laser
		rgbgen entity
		tcmod rotate 180
		blendFunc add
	}
}
models/weapons/lasergun/f_laser_2
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/lasergun/f_laser_2
		rgbgen entity
		tcmod scroll 0 3
		blendFunc add
	}
}

models/weapons/machinegun/machinegun_flash
{
	sort nearest
	cull disable
	softParticle
	//deformVertexes wave 20 noise 5 5 0 15
	{
		animMap 6 models/weapons/machinegun/machinegun_flash_1 models/weapons/machinegun/machinegun_flash_2 models/weapons/machinegun/machinegun_flash_3 models/weapons/machinegun/machinegun_flash_4
		rgbgen entity
		blendfunc add
	}
}

// DEBRIS
models/objects/debris/debris_template
{
	cull front

	{
		map models/objects/debris/debris$1
	}
}

models/objects/debris/debris1
{
	template models/objects/debris/debris_template 1
}

models/objects/debris/debris2
{
	template models/objects/debris/debris_template 2
}

models/objects/debris/debris3
{
	template models/objects/debris/debris_template 3
}

models/objects/debris/debris4
{
	template models/objects/debris/debris_template 4
}

models/objects/debris/debris5
{
	template models/objects/debris/debris_template 5
}

models/objects/debris/debris6
{
	template models/objects/debris/debris_template 6
}
