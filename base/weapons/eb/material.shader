models/weapons/electrobolt/electrobolt
{
	{
		map $whiteimage
		rgbGen const 0.314 0.953 1.000
	}
}

weapons/eb/beam
{
	{
		blendfunc add
		map weapons/eb/beam
		rgbgen entity
		alphagen entity
	}
}

models/weapons/electrobolt/electrobolt_fx
{
	cull disable
	{
		map weapons/eb/electrobolt_fx
		blendFunc add
		rgbGen wave sin 0.5 0.5 0 0.5
	}
}

models/weapons/electrobolt/electrobolt_fx_1
{
	cull disable
	{
		map weapons/eb/electrobolt_fx_1
		animmap 15 weapons/eb/electrobolt_fx_1 weapons/eb/electrobolt_fx_2 weapons/eb/electrobolt_fx_3 weapons/eb/electrobolt_fx_4 weapons/eb/electrobolt_fx_5 weapons/eb/electrobolt_fx_6 weapons/eb/electrobolt_fx_7 weapons/eb/electrobolt_fx_8
		blendfunc add
	}
}
