textures/world/lava
{
	q3map_globaltexture
	//surfaceparm trans
	surfaceparm nonsolid
	surfaceparm noimpact
	surfaceparm lava
	surfaceparm nolightmap
	surfaceparm nomarks
	q3map_surfacelight 200
	tesssize 64
	qer_editorimage textures/world/sh/lava2
	fog

	deformVertexes wave 100 sin 3 2 .1 0.1

	{
		map $whiteimage
		rgbgen const 1.0 0.19 0.0
	}
}

textures/world/bluewater
{
	qer_editorimage textures/colors/blue
	surfaceparm noimpact
	surfaceparm nolightmap
	surfaceparm water
	surfaceparm nomarks
	cull disable
	deformVertexes wave 100 sin 3 2 0.1 0.1
	qer_trans 0.5
	q3map_lightsubdivide 32
	q3map_globaltexture
	sort underwater
	{
		map textures/colors/blue
		blendfunc add
		rgbGen const ( 0.227451 0.227451 0.227451 )
	}
}

textures/world/bluewater1
{
	qer_editorimage textures/world/sh/water2
	q3map_globaltexture
	q3map_surfacelight 10
	qer_trans .75
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm water
	surfaceparm nomarks
	surfaceparm nolightmap
	cull none
	sort underwater

	{
		map $whiteimage
		rgbgen const 0.2 0.6 1.0
		alphagen const 0.25
		blendFunc GL_DST_COLOR GL_ONE
		tcmod scale .25 .25
		tcmod scroll .02 .01
	}
}

textures/world/bluedistortwater
{
	qer_editorimage textures/world/sh/water2
	q3map_globaltexture
	qer_trans .35
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm water
	surfaceparm nodlight
	surfaceparm nomarks
	cull none
	sort underwater

	{
		map $whiteimage
		rgbgen const 0.2 0.6 1.0
		alphagen const 0.25
	}
}
