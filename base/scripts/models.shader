//------------
//WEAPONS
//------------

models/weapons/electrobolt/electrobolt
{
	cull front

	{
		material models/weapons/electrobolt/electrobolt.tga
		rgbgen entity
		alphagen entity
	}
}

models/weapons/electrobolt/electrobolt_fx
{
	cull disable
	{
		map models/weapons/electrobolt/electrobolt_fx.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}
models/weapons/electrobolt/electrobolt_fx_1
{
	cull disable
	//deformVertexes autosprite2
	{
		animmap 15 models/weapons/electrobolt/electrobolt_fx_1.tga models/weapons/electrobolt/electrobolt_fx_2.tga models/weapons/electrobolt/electrobolt_fx_3.tga models/weapons/electrobolt/electrobolt_fx_4.tga models/weapons/electrobolt/electrobolt_fx_5.tga models/weapons/electrobolt/electrobolt_fx_6.tga models/weapons/electrobolt/electrobolt_fx_7.tga models/weapons/electrobolt/electrobolt_fx_8.tga
		blendfunc add
	}
}

models/weapons/glauncher/glauncher
{
	cull front

	{
		material models/weapons/glauncher/glauncher.tga models/weapons/glauncher/glauncher_norm.tga models/weapons/glauncher/glauncher_gloss.tga
		rgbgen entity
	}
}

models/weapons/glauncher/glauncher_fx
{
	cull disable
	{
		map models/weapons/glauncher/glauncher_fx.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}

models/weapons/plasmagun/plasmagun
{
	cull front

	{
		material models/weapons/plasmagun/plasmagun.tga
		rgbgen entity
	}
}

models/weapons/plasmagun/plasmagun_fx
{
	cull disable
	{
		map models/weapons/plasmagun/plasmagun_fx.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}

models/weapons/riotgun/riotgun
{
	cull front

	{
		material models/weapons/riotgun/riotgun.tga
		rgbgen entity
	}
}

models/weapons/gunblade/gunblade
{
	cull front

	{
		material models/weapons/gunblade/gunblade.tga
		rgbgen entity
	}
}


models/weapons/gunblade/barrel
{
	cull front

	{
		material models/weapons/gunblade/barrel.tga
		rgbgen entity
	}
}

models/weapons/machinegun/machinegun
{
	cull front

	{
		material models/weapons/machinegun/machinegun.tga
		rgbgen entity
	}
}

models/weapons/rlauncher/rlauncher
{
	cull front

	{
		material models/weapons/rlauncher/rlauncher.tga
		rgbgen entity
	}
}

models/weapons/rlauncher/rlauncher_fx
{
	cull disable
	{
		map models/weapons/rlauncher/rlauncher_fx.tga
		blendFunc GL_ONE GL_ONE
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}
models/weapons/lasergun/lasergun
{
	cull front

	{
		material models/weapons/lasergun/lasergun.tga
		rgbgen entity
	}
}

models/weapons/lg_fx
{
	cull disable
	{
		map models/weapons/lg_fx.tga
		blendFunc GL_ONE GL_ONE
		tcmod scroll -2 0
	}
	{
		map models/weapons/lg_fx.tga
		blendFunc GL_ONE GL_ONE
		tcmod scroll 2 0
	}
}

//---------------------
//WEAPON PROJECTILES
//---------------------

models/objects/projectile/plasmagun/plnew
{
	//sort additive
	cull disable
	{
		map models/objects/projectile/plasmagun/plnew.tga
		blendFunc add
		//blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		tcmod rotate -150
	}
}

models/objects/projectile/glauncher/grenadegradstrong
{
	cull disable
	{
		map models/objects/projectile/glauncher/grenadegradstrong.tga
		blendFunc add
		rgbGen wave triangle .07 .1 0 5
		tcmod scroll 0.2 0
	}
}

models/objects/projectile/glauncher/grenadestrong
{
	{
		map models/objects/projectile/glauncher/grenadestrong.tga
	}
}

models/objects/projectile/glauncher/grenadestrong_flare
{
	//deformVertexes autosprite
	cull none
	{
		map models/objects/projectile/glauncher/grenadestrong_flare.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
	}
}

models/objects/projectile/rlauncher/rocket_strong
{
	sort additive
	cull disable
	{
		map models/objects/projectile/rlauncher/rocket_strong.tga
		blendFunc add
		rgbGen entity
		tcmod rotate -360
	}
}

models/objects/projectile/rlauncher/rocket_flare_2
{
	sort additive
	cull disable
	{
		map models/objects/projectile/rlauncher/rocket_flare_2.tga
		blendFunc add
		rgbGen wave triangle .1 .1 0 1
		tcmod scroll 3.2 0
	}
}

models/objects/projectile/rlauncher/rocketgradstrong
{
	cull disable
	{
		map models/objects/projectile/rlauncher/rocketgradstrong.tga
		blendFunc add
		rgbGen wave triangle .01 .15 0 20
		tcmod scroll 0.2 0
	}
}

models/objects/projectile/lasergun/laserbeam
{
	nomipmaps
	cull none
	deformVertexes autosprite2
	{
		map models/objects/projectile/lasergun/laserbeam.tga
		blendFunc add
		tcMod scroll 6 0
	}
}

//-----------------------------------------

//-----------------
//FLASH WEAPONS
//-----------------

//models/v_weapons/electrobolt/f_electro
//models/v_weapons/plasmagun/f_plasma
//models/v_weapons/lasergun/f_laser
//models/v_weapons/rlauncher/f_rlaunch
//models/v_weapons/glauncher/f_glaunch
//models/v_weapons/riotgun/f_riot

models/v_weapons/generic/f_generic
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/generic/f_generic.tga
		rgbgen entity
		tcmod rotate 90
		blendFunc add
	}
}

models/weapons/plasmagun/f_plasma
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/plasmagun/f_plasma.tga
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
		map models/weapons/plasmagun/f_plasma_2.tga
		rgbgen entity
		blendFunc add
	}
}

models/weapons/glauncher/f_glaunch
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/glauncher/f_glaunch.tga
		rgbgen entity
		blendFunc add
	}
	{
		map models/weapons/glauncher/f_glaunch_spark.tga
		rgbgen entity
		tcMod stretch sawtooth .65 .3 0 8
		blendFunc add
	}
}
models/weapons/glauncher/f_glaunch_2
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/glauncher/f_glaunch_2.tga
		rgbgen entity
		blendfunc add
	}
}
models/weapons/glauncher/f_glaunch_3
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/glauncher/f_glaunch_2.tga
		rgbgen entity
		blendfunc add
	}
}
models/weapons/riotgun/f_riot
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/riotgun/f_riot.tga
		rgbgen entity
		blendfunc add
	}
	{
		map models/weapons/riotgun/f_riot_spark.tga
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
		map models/weapons/riotgun/f_riot_2.tga
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
		map models/weapons/riotgun/f_riot_3.tga
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
		map models/weapons/gunblade/f_gunblade.tga
		rgbgen entity
		tcmod rotate 200
		blendFunc add
	}
	{
		map models/weapons/gunblade/f_gunblade_1.tga
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
		map models/weapons/gunblade/f_gunblade_2.tga
		rgbgen entity
		blendfunc add
	}
}

models/weapons/rlauncher/f_rlaunch
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/rlauncher/f_rlaunch.tga
		rgbgen entity
		//tcmod rotate 90
		blendFunc add
	}
}

models/weapons/rlauncher/f_rlaunch_2
{
	sort nearest
	cull disable
	//deformVertexes autosprite2
	softParticle
	{
		map models/weapons/rlauncher/f_rlaunch_2.tga
		rgbgen entity
		blendFunc add
	}
}

models/weapons/electrobolt/f_electrobolt
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/electrobolt/f_electro.tga
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
		map models/weapons/electrobolt/f_electro_2.tga
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
		animMap 6 models/weapons/electrobolt/f_electro_3.tga models/weapons/electrobolt/f_electro_4.tga models/weapons/electrobolt/f_electro_5.tga models/weapons/electrobolt/f_electro_6.tga
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
		map models/weapons/lasergun/f_laser.tga
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
		map models/weapons/lasergun/f_laser_2.tga
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
		animMap 6 models/weapons/machinegun/machinegun_flash_1.tga models/weapons/machinegun/machinegun_flash_2.tga models/weapons/machinegun/machinegun_flash_3.tga models/weapons/machinegun/machinegun_flash_4.tga
		rgbgen entity
		blendfunc add
	}
}

// DEBRIS
models/objects/debris/debris_template
{
	cull front

	{
		map models/objects/debris/debris$1.tga
	}

	cubemap env/cell
	blendFunc filter
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

