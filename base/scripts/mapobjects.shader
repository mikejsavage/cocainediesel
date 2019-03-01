models_mapobjects_lamp_lamp
{
	qer_editorimage models/mapobjects/lamp/lamp.tga
	surfaceparm nolightmap
	nomipmaps
	glossExponent 96

	{
		rgbgen vertex
		material models/mapobjects/lamp/lamp.tga
	}

	{
		map models/mapobjects/lamp/lamp_alpha.tga
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

models_mapobjects_lamp_lamp_yellow
{
	qer_editorimage models/mapobjects/lamp/lamp_yellow.tga
	surfaceparm nolightmap
	nomipmaps
	glossExponent 96

	{
		rgbgen vertex
		material models/mapobjects/lamp/lamp_yellow.tga models/mapobjects/lamp/lamp_norm.tga models/mapobjects/lamp/lamp_gloss.tga
	}

	{
		map models/mapobjects/lamp/lamp_alpha.tga
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

models_mapobjects_lamp_lamp_blue
{
	qer_editorimage models/mapobjects/lamp/lamp_blue.tga
	surfaceparm nolightmap
	nomipmaps
	glossExponent 96

	{
		rgbgen vertex
		material models/mapobjects/lamp/lamp_blue.tga models/mapobjects/lamp/lamp_norm.tga models/mapobjects/lamp/lamp_gloss.tga
	}

	{
		map models/mapobjects/lamp/lamp_alpha.tga
		alphaGen wave sin 0.75 0.25 0.75 1.5
		blendFunc blend
	}
}

models_mapobjects_lamp_lamp_halo
{
	qer_editorimage textures/wsw_flareshalos/glow_halo_white.tga
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
		clampmap textures/wsw_flareshalos/glow_halo_white.tga
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
		map models/mapobjects/jumppad/flame.tga
		blendfunc add
		rgbGen wave sin 0.5 1 0 0.3
	}
}

models/mapobjects/jumppad/jumppad1
{
	cull none
	qer_editorimage models/mapobjects/jumppad/jumppad1.tga

	if deluxe
	{
		material models/mapobjects/jumppad/jumppad1_diffuse.tga models/mapobjects/jumppad/jumppad1_norm.tga models/mapobjects/jumppad/jumppad1_gloss.tga
	}
	endif

	if ! deluxe
	{
		map $lightmap
	}
	{
		map models/mapobjects/jumppad/jumppad1.tga
		blendfunc filter
	}
	endif
	{
		map models/mapobjects/jumppad/jumppad1_light.tga
		blendFunc add
	}
}

models/mapobjects/jumppad1/diffuse
{
	qer_editorimage models/mapobjects/jumppad1/diffuse.tga
	surfaceparm nomarks
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/jumppad1/diffuse.tga models/mapobjects/jumppad1/normal.tga
	}

	{
		animmap 8 models/mapobjects/jumppad1/glow_01.jpg models/mapobjects/jumppad1/glow_02.jpg  models/mapobjects/jumppad1/glow_03.jpg
		blendfunc add
	}
}

models/mapobjects/jumppad/u_ring
{
	cull none
	surfaceparm nolightmap
	deformVertexes move 0 0 4 sin 0 1 0 0.5
	{
		map models/mapobjects/jumppad/u_ring.tga
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
		map models/mapobjects/jumppad/l_ring.tga
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
		material models/mapobjects/lights/leds_iron_frame.tga
	}
}

mapobjects_leds_orange
{
	qer_editorimage models/mapobjects/lights/leds_light_orange.tga
	surfaceparm nolightmap
	surfaceparm nomarks

	{
		map models/mapobjects/lights/leds_light_orange.tga
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
		material models/mapobjects/decor_misc/powerline.tga
	}
}

models_mapobjects_decor_misc_barrel
{
	qer_editorimage models/mapobjects/decor_misc/barrel
	surfaceparm nolightmap
	surfaceparm nomarks

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/barrel.tga
	}
}

models_mapobjects_decor_misc_fireextinguisher
{
	qer_editorimage models/mapobjects/decor_misc/fireextinguisher
	surfaceparm nolightmap
	surfaceparm nomarks
	glossExponent 64

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/fireextinguisher.tga
	}
}

mapobjects_decor_misc_hammer
{
	qer_editorimage models/mapobjects/decor_misc/hammer.tga
	surfaceparm nolightmap
	surfaceparm nomarks
	glossExponent 128

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/hammer.tga $blankbumpimage models/mapobjects/decor_misc/hammer_gloss.tga
	}
}

mapobjects_decor_misc_spanner
{
	qer_editorimage models/mapobjects/decor_misc/spanner.tga
	surfaceparm nolightmap
	surfaceparm nomarks
	glossExponent 128

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/spanner.tga $blankbumpimage $whiteImage
	}
}

mapobjects_decor_misc_povian
{
	qer_editorimage models/mapobjects/decor_misc/povian.tga
	surfaceparm nolightmap
	surfaceparm nomarks
	glossExponent 128

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/povian.tga $blankbumpimage $whiteImage
	}
}

models/mapobjects/crates/container_red
{
	qer_editorimage models/mapobjects/crates/container_red.tga
	q3map_forceMeta
	glossExponent 96

	{
		material models/mapobjects/crates/container_red.tga models/mapobjects/crates/container_norm.tga models/mapobjects/crates/container_gloss.tga
	}
}

models/mapobjects/crates/container_blue
{
	qer_editorimage models/mapobjects/crates/container_blue.tga
	q3map_forceMeta
	glossExponent 96

	{
		material models/mapobjects/crates/container_blue.tga models/mapobjects/crates/container_norm.tga models/mapobjects/crates/container_gloss.tga
	}
}

models/mapobjects/crates/container_blue_trans
{
	qer_editorimage models/mapobjects/crates/container_blue.tga
	qer_trans 0.8
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm alphashadow
	cull none
	q3map_forceMeta
	glossExponent 96

	{
		material models/mapobjects/crates/container_blue.tga models/mapobjects/crates/container_norm.tga models/mapobjects/crates/container_gloss.tga
		blendFunc blend
	}
}

models/mapobjects/crates/container_green
{
	qer_editorimage models/mapobjects/crates/container_green.tga
	q3map_forceMeta
	glossExponent 96

	{
		material models/mapobjects/crates/container_green.tga models/mapobjects/crates/container_norm.tga models/mapobjects/crates/container_gloss.tga
	}
}

models/mapobjects/crates/container_green_trans
{
	qer_editorimage models/mapobjects/crates/container_green.tga
	qer_trans 0.8
	surfaceparm trans
	surfaceparm nomarks
	surfaceparm alphashadow
	cull none
	q3map_forceMeta
	glossExponent 96

	{
		material models/mapobjects/crates/container_green.tga models/mapobjects/crates/container_norm.tga models/mapobjects/crates/container_gloss.tga
		blendFunc blend
	}
}

models/mapobjects/decor_misc/aircondition_01
{
	qer_editorimage models/mapobjects/decor_misc/aircondition_01.tga
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/aircondition_01.tga models/mapobjects/decor_misc/aircondition_01_norm.tga models/mapobjects/decor_misc/aircondition_01_gloss.tga
	}
}

models/mapobjects/decor_misc/aircondition_02
{
	qer_editorimage models/mapobjects/decor_misc/aircondition_02.tga
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/aircondition_02.tga models/mapobjects/decor_misc/aircondition_01_norm.tga models/mapobjects/decor_misc/aircondition_01_gloss.tga
	}
}

models/mapobjects/decor_misc/aircondition_02a
{
	qer_editorimage models/mapobjects/decor_misc/aircondition_02a.tga
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/aircondition_02a.tga models/mapobjects/decor_misc/aircondition_01_norm.tga models/mapobjects/decor_misc/aircondition_01_gloss.tga
	}
}

models/mapobjects/decor_misc/aircondition_fan_01
{
	qer_editorimage models/mapobjects/decor_misc/aircondition_fan_01.tga
	qer_trans 0.25
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/decor_misc/aircondition_fan_01.tga
		blendFunc blend
		tcmod rotate 160
	}
}

models/mapobjects/lights/coldlight_01
{
	qer_editorimage models/mapobjects/lights/coldlight_01.tga
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/lights/coldlight_01.tga models/mapobjects/lights/coldlight_01_norm.tga models/mapobjects/lights/coldlight_01_gloss.tga
	}
}

models/mapobjects/lights/coldlight_01_glass
{
	qer_editorimage models/mapobjects/lights/coldlight_01_glass.tga
	qer_trans 0.25
	surfaceparm nolightmap
	surfaceparm nomarks
	{
		map models/mapobjects/lights/grad.tga
		blendfunc add
		tcGen environment
	}
	{
		map models/mapobjects/lights/coldlight_01_glass.tga
		blendFunc blend
	}
}

models/mapobjects/lights/coldlight_01a
{
	qer_editorimage models/mapobjects/lights/coldlight_01a.tga
	surfaceparm nolightmap

	{
		rgbgen vertex
		material models/mapobjects/lights/coldlight_01a.tga models/mapobjects/lights/coldlight_01a_norm.tga models/mapobjects/lights/coldlight_01a_gloss.tga
	}
}

models/mapobjects/vehicles/forklift
{
	qer_editorimage models/mapobjects/vehicles/forklift.tga
	surfaceparm nomarks
	surfaceparm nolightmap
	//surfaceparm nonsolid

	{
		rgbgen vertex
		material models/mapobjects/vehicles/forklift
	}
}
