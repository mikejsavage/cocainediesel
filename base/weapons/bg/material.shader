weapons/bg
{
	{
		map $whiteimage
		rgbGen const 0.265 0.314 1.000
	}
}

weapons/bg/cell
{
	cull disable
	{
		blendFunc add
		map weapons/bg/cell
		rgbgen entity
	}
}