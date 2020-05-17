models/mapobjects/jumppad/flame
{
	cull none
	surfaceparm nolightmap
	deformVertexes autosprite2

	{
		detail
		map models/mapobjects/jumppad/flame
		blendfunc add
		rgbGen wave sin 0.5 1 0 0.3
	}
}

models/mapobjects/jumppad/jumppad1
{
	cull none
	qer_editorimage models/mapobjects/jumppad/jumppad1

	map models/mapobjects/jumppad/jumppad1
}

models/mapobjects/jumppad1/diffuse
{
	qer_editorimage models/mapobjects/jumppad1/diffuse
	surfaceparm nomarks
	surfaceparm nolightmap

	{
		rgbgen vertex
		map models/mapobjects/jumppad1/diffuse
	}

	{
		animmap 8 models/mapobjects/jumppad1/glow_01 models/mapobjects/jumppad1/glow_02  models/mapobjects/jumppad1/glow_03
		blendfunc add
	}
}

models/mapobjects/jumppad/u_ring
{
	cull none
	surfaceparm nolightmap
	deformVertexes move 0 0 4 sin 0 1 0 0.5
	{
		map models/mapobjects/jumppad/u_ring
		blendfunc add
		alphaFunc GT0
	}
}

models/mapobjects/jumppad/l_ring
{
	cull none
	surfaceparm nolightmap
	deformVertexes move 0 0 8 sin 0 1 0.5 0.6

	{
		map models/mapobjects/jumppad/l_ring
		blendfunc add
		alphaFunc GT0
	}
}
