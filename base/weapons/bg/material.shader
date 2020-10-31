weapons/bg
{
	{
		map $whiteimage
		rgbGen const 0.1 0.1 0.1
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