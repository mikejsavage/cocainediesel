weapons/gl/grenade
{
	{
		map $whiteimage
		rgbgen entitycolorwave 1 1 1 sin 0 0.5 1 3
	}
}

models/weapons/glauncher/f_glaunch
{
	sort nearest
	cull disable
	softParticle
	{
		map models/weapons/glauncher/f_glaunch
		rgbgen entity
		blendFunc add
	}
	{
		map models/weapons/glauncher/f_glaunch_spark
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
		map models/weapons/glauncher/f_glaunch_2
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
		map models/weapons/glauncher/f_glaunch_2
		rgbgen entity
		blendfunc add
	}
}
