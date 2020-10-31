weapons/pg
{
	{
		map $whiteimage
		rgbGen const 0.1 0.1 0.1
	}
}

weapons/pg/cell
{
	cull disable
	{
		map weapons/pg/cell
		blendFunc add
		tcmod rotate -150
		rgbgen entity
	}
}

models/weapons/plasmagun/plasmagun_fx
{
	cull disable
	{
		map weapons/pg/plasmagun_fx
		blendFunc add
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}
