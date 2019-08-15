textures/wsw_flareshalos/flare_sphere_orange
{
	qer_editorimage textures/wsw_flareshalos/flare_sphere_orange.tga
	qer_trans 0.25
	cull back
	surfaceparm nomarks
	surfaceparm trans
	surfaceparm nonsolid
	deformVertexes autosprite
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/flare_sphere_orange.tga
		blendFunc add
	}
}

textures/wsw_flareshalos/flare_sphere_cyan
{
	qer_editorimage textures/wsw_flareshalos/flare_sphere_cyan.tga
	qer_trans 0.25
	cull back
	surfaceparm nomarks
	surfaceparm trans
	surfaceparm nonsolid
	deformVertexes autosprite
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/flare_sphere_cyan.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbgen wave distanceramp 0 1 150 600
	}
}

textures/wsw_flareshalos/flare_sphere_fog_white
{
	qer_editorimage textures/wsw_flareshalos/flare_sphere_fog_white.tga
	qer_trans 0.25
	cull back
	surfaceparm nomarks
	surfaceparm trans
	surfaceparm nonsolid
	deformVertexes autosprite
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/flare_sphere_fog_white.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbgen wave distanceramp 0 1 100 800
	}
}

textures/wsw_flareshalos/glow_neontube_blue
{
	qer_editorimage textures/wsw_flareshalos/glow_neontube_blue.tga
	qer_trans 0.25
	cull back
	surfaceparm	nomarks
	surfaceparm trans
	surfaceparm nonsolid
	deformVertexes autosprite2
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/glow_neontube_blue.tga
		blendFunc add
	}
}

textures/wsw_flareshalos/flare_sphere_white
{
	qer_editorimage textures/wsw_flareshalos/flare_sphere_white.tga
	qer_trans 0.25
	cull back
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nonsolid
	deformVertexes autosprite
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/flare_sphere_white.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
	}
}

textures/wsw_flareshalos/glow_halo_white
{
	qer_editorimage textures/wsw_flareshalos/glow_halo_white.tga
	//qer_trans 0.25
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

textures/wsw_flareshalos/glow_halo_white_soft
{
	qer_editorimage textures/wsw_flareshalos/glow_halo_white.tga
	qer_trans 0.35
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
		rgbgen wave distanceramp 0 0.5 80 400
	}
}

textures/wsw_flareshalos/glow_cone_cyan
{
	qer_editorimage textures/wsw_flareshalos/glow_cone_cyan.tga
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
		clampmap textures/wsw_flareshalos/glow_cone_cyan.tga
		blendFunc add
		rgbgen wave distanceramp 0 0.5 30 350
	}
}

textures/wsw_flareshalos/glow_cone_verycyan
{
	qer_editorimage textures/wsw_flareshalos/glow_cone_verycyan.tga
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
		clampmap textures/wsw_flareshalos/glow_cone_verycyan.tga
		blendFunc GL_ONE GL_ONE_MINUS_SRC_COLOR
		rgbgen wave distanceramp 0 0.6 82 500
	}
}

textures/wsw_flareshalos/glow_neontube_red
{
	qer_editorimage textures/wsw_flareshalos/glow_neontube_red.tga
	qer_trans 0.25
	cull back
	surfaceparm nomarks
	surfaceparm trans
	surfaceparm nonsolid
	deformVertexes autosprite2
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/glow_neontube_red.tga
		blendFunc add
	}
}

textures/wsw_flareshalos/small_light_halo
{
	qer_editorimage textures/wsw_flareshalos/small_light_halo.tga
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
		clampmap textures/wsw_flareshalos/small_light_halo.tga
		blendFunc add
		rgbgen wave distanceramp 0 0.5 30 400
	}
}

textures/wsw_flareshalos/flare_sphere_white_front
{
	qer_editorimage textures/wsw_flareshalos/flare_sphere_white.tga
	qer_trans 0.75
	cull back
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nonsolid
	deformVertexes autosprite
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/flare_sphere_white.tga
		rgbgen wave distanceramp 0 2 20 1300
		blendFunc GL_ONE GL_ONE
	}
}

textures/wsw_flareshalos/flare_sphere_white_front_soft
{
	qer_editorimage textures/wsw_flareshalos/flare_sphere_white.tga
	qer_trans 0.75
	cull back
	surfaceparm nomarks
	surfaceparm nolightmap
	surfaceparm trans
	surfaceparm nonsolid
	deformVertexes autosprite
	softParticle

	{
		detail
		clampmap textures/wsw_flareshalos/flare_sphere_white.tga
		rgbgen wave distanceramp 0 1 10 100
		blendFunc GL_ONE GL_ONE
	}
}
