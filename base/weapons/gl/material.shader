models/weapons/gl
{
	{
		map $whiteimage
		rgbGen const 0.1 0.1 0.1
	}
}

weapons/gl/grenade
{
	{
		map $whiteimage
		rgbgen entitycolorwave 0.7 0.7 0.7 sin 0.25 0.25 0.25 10
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
