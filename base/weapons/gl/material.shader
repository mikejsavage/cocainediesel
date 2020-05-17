models/weapons/gl
{
	{
		map $whiteimage
		rgbGen const 0.243 0.553 1.000
	}
}

weapons/gl/grenade
{
	{
		map $whiteimage
		rgbgen entitycolorwave 1 1 1 sin 0 0.5 1 3
	}
}

models/weapons/glauncher/glauncher_fx
{
	cull disable
	{
		map weapons/gl/glauncher_fx
		blendFunc add
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}
