models_mapobjects_lamp_lamp
{
	qer_editorimage models/mapobjects/lamp/lamp
	surfaceparm nolightmap
	nomipmaps
	glossExponent 96

	{
		rgbgen vertex
		material models/mapobjects/lamp/lamp
	}

	{
		map models/mapobjects/lamp/lamp_alpha
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

models_mapobjects_lamp_lamp_yellow
{
	qer_editorimage models/mapobjects/lamp/lamp_yellow
	surfaceparm nolightmap
	nomipmaps
	glossExponent 96

	{
		rgbgen vertex
		material models/mapobjects/lamp/lamp_yellow models/mapobjects/lamp/lamp_norm models/mapobjects/lamp/lamp_gloss
	}

	{
		map models/mapobjects/lamp/lamp_alpha
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

models_mapobjects_lamp_lamp_blue
{
	qer_editorimage models/mapobjects/lamp/lamp_blue
	surfaceparm nolightmap
	nomipmaps
	glossExponent 96

	{
		rgbgen vertex
		material models/mapobjects/lamp/lamp_blue models/mapobjects/lamp/lamp_norm models/mapobjects/lamp/lamp_gloss
	}

	{
		map models/mapobjects/lamp/lamp_alpha
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

models_mapobjects_lamp_lamp_halo
{
	qer_editorimage textures/wsw_flareshalos/glow_halo_white
	qer_trans 0.25
	cull none
	surfaceparm nomarks
	surfaceparm trans
	surfaceparm nonsolid
	surfaceparm nolightmap
	deformVertexes autosprite2
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/glow_halo_white
		blendfunc add
		rgbgen wave distanceramp 0 0.7 80 400
	}
}

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

	if deluxe
	{
		material models/mapobjects/jumppad/jumppad1_diffuse models/mapobjects/jumppad/jumppad1_norm models/mapobjects/jumppad/jumppad1_gloss
	}
	endif

	if ! deluxe
	{
		map $lightmap
	}
	{
		map models/mapobjects/jumppad/jumppad1
		blendfunc filter
	}
	endif
	{
		map models/mapobjects/jumppad/jumppad1_light
		blendFunc add
	}
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

mapobjects_leds_iron_frame
{
	qer_editorimage models/mapobjects/lights/leds_iron_frame
	surfaceparm nolightmap
	surfaceparm nomarks

	{
		rgbgen vertex
		material models/mapobjects/lights/leds_iron_frame
	}
}

mapobjects_leds_orange
{
	qer_editorimage models/mapobjects/lights/leds_light_orange
	surfaceparm nolightmap
	surfaceparm nomarks

	{
		map models/mapobjects/lights/leds_light_orange
		rgbgen identity
	}
}

models_mapobjects_decor_misc_powerline
{
	qer_editorimage models/mapobjects/decor_misc/powerline
	surfaceparm nolightmap
	surfaceparm nomarks

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/powerline
	}
}

models/mapobjects/decor_misc/aircondition_01
{
	qer_editorimage models/mapobjects/decor_misc/aircondition_01
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/aircondition_01 models/mapobjects/decor_misc/aircondition_01_norm models/mapobjects/decor_misc/aircondition_01_gloss
	}
}

models/mapobjects/decor_misc/aircondition_02
{
	qer_editorimage models/mapobjects/decor_misc/aircondition_02
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/aircondition_02 models/mapobjects/decor_misc/aircondition_01_norm models/mapobjects/decor_misc/aircondition_01_gloss
	}
}

models/mapobjects/decor_misc/aircondition_02a
{
	qer_editorimage models/mapobjects/decor_misc/aircondition_02a
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/aircondition_02a models/mapobjects/decor_misc/aircondition_01_norm models/mapobjects/decor_misc/aircondition_01_gloss
	}
}

models/mapobjects/decor_misc/aircondition_fan_01
{
	qer_editorimage models/mapobjects/decor_misc/aircondition_fan_01
	qer_trans 0.25
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/aircondition_fan_01
		blendFunc blend
		tcmod rotate 160
	}
}

models/mapobjects/lights/coldlight_01
{
	qer_editorimage models/mapobjects/lights/coldlight_01
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/lights/coldlight_01 models/mapobjects/lights/coldlight_01_norm models/mapobjects/lights/coldlight_01_gloss
	}
}

models/mapobjects/lights/coldlight_01_glass
{
	qer_editorimage models/mapobjects/lights/coldlight_01_glass
	qer_trans 0.25
	surfaceparm nolightmap
	surfaceparm nomarks
	{
		map models/mapobjects/lights/grad
		blendfunc add
		tcGen environment
	}
	{
		map models/mapobjects/lights/coldlight_01_glass
		blendFunc blend
	}
}

models/mapobjects/lights/coldlight_01a
{
	qer_editorimage models/mapobjects/lights/coldlight_01a
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/lights/coldlight_01a models/mapobjects/lights/coldlight_01a_norm models/mapobjects/lights/coldlight_01a_gloss
	}
}
