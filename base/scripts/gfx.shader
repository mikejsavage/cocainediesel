gfx/decals/shadow
{
	polygonOffset
	sort banner
	{
		alphamaskclampmap gfx/decals/shadow
		blendFunc blend
		rgbGen const 0 0 0
		alphaGen vertex
	}
}

particle
{
	deformvertexes autoparticle
	cull none
	entityMergable		// allow all the sprites to be merged together
	{
		map $particleimage
		blendfunc blend
		rgbgen vertex
		alphagen vertex
	}
}

flareShader
{
	cull none
	softParticle
	{
		clampmap gfx/misc/flare
		blendFunc add
		rgbGen vertex
	}
}

gfx/misc/ctf_flare
{
	cull none
	entityMergable		// allow all the sprites to be merged together
	softParticle
	{
		clampmap gfx/misc/ctf_flare_a
		blendFunc blend
		//rgbGen	vertex
		alphaGen	vertex
		tcMod rotate 3
	}
	{
		clampmap gfx/misc/ctf_flare_b
		blendFunc add
		rgbGen	vertex
		alphaGen	vertex
		tcMod rotate 10
	}
}

alphaBlendedPuff_Template
{
	cull none
	entityMergable		// allow all the sprites to be merged together
	softParticle
	{
		clampmap gfx/misc/$1
		blendFunc blend
		rgbGen vertex
		alphaGen vertex
	}
}

teleporterSmokePuff
{
	template alphaBlendedPuff_Template teleportsmokepuff
}

gfx/misc/smokepuff
{
	template alphaBlendedPuff_Template smokepuff
}

gfx/misc/rocketsmokepuff
{
	template alphaBlendedPuff_Template rocketsmokepuff
}

gfx/misc/strong_rocket_fire
{
	template alphaBlendedPuff_Template strong_rocket_fire
}

gfx/grenadetrail_smoke_puf
{
	template alphaBlendedPuff_Template grenadetrail_smoke
}

gfx/misc/bloodtrail_puff
{
	template alphaBlendedPuff_Template bloodtrail_puff
}

gfx/misc/bloodimpact_puff
{
	template alphaBlendedPuff_Template bloodimpact_puff
}

gfx/misc/bloodtrailliquid_puff
{
	template alphaBlendedPuff_Template bloodtrailliquid_puff
}

//
// wall marks
// use blendFunc GL_ZERO GL_ONE_MINUS_SRC_COLOR so that
// their "contribution" can be damped down in fog volumes
// with distance

decals_Hit_Exact_Template
{
	polygonOffset
	cull none
	sort banner
	{
		map gfx/decals/$1
		blendFunc GL_ZERO GL_ONE_MINUS_SRC_COLOR
		rgbGen exactVertex
	}
}

decals_Hit_Template
{
	polygonOffset
	cull none
	sort banner
	{
		map gfx/decals/$1
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}

gfx/decals/d_bullet_hit
{
	template decals_Hit_Exact_Template d_bullet_hit
}

gfx/decals/d_blade_hit
{
	template decals_Hit_Exact_Template d_blade_hit
}

gfx/decals/d_explode_hit
{
	template decals_Hit_Exact_Template d_explode_hit
}

gfx/decals/d_plasma_hit
{
	template decals_Hit_Template d_plasma_hit
}

gfx/decals/ebimpact
{
	polygonOffset
	cull none
	sort banner
	{
		alphamaskclampmap gfx/decals/ebimpact
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}


//
// weapon hits
// All of these will be put on entities with a shaderTime offset, so they
// can be expected to start at time 0 for each separate explosion
//
// Many of these are use smoothly morphed animating textures by pairing
// sawtooth and iverse sawtooth rgbGens

weapon_Hit_Template
{
	cull none
	{
		map models/weapon_hits/$1/$2
		rgbgen entity
		alphagen entity
		blendfunc GL_SRC_ALPHA GL_ONE
	}
}

models/weapon_hits/bullet/hit_bullet
{
	template weapon_Hit_Template bullet hit_bullet
}

models/weapon_hits/lasergun/hit_blastexp
{
	cull none
	softParticle
	{
		map models/weapon_hits/lasergun/hit_blastexp
		rgbgen entity
		alphagen entity
		blendfunc GL_SRC_ALPHA GL_ONE
		tcMod stretch sin 1 0.05 0 2.75
	}
}

models/weapon_hits/plasmagun/plasmagun_hit
{
	cull none
	{
		map models/weapon_hits/plasmagun/plasmagun_hit
		rgbgen entity
		alphagen entity
		blendfunc GL_SRC_ALPHA GL_ONE
		tcMod stretch sawtooth 0.9 0.8 0 1.4
	}
}

models/weapon_hits/electrobolt/hit_electrobolt
{
	template weapon_Hit_Template electrobolt hit_electrobolt
}

models/weapon_hits/glauncher/hit_glauncher
{
	{
		rgbgen entity
		alphagen entity
		blendfunc blend
		map models/weapon_hits/glauncher/hit_glauncher
	}
}

// Rocket Explosion

gfx/rocket_explosion
{
	cull disable
	softParticle
	{
		animmap 8 gfx/misc/rlexplo1  gfx/misc/rlexplo2 gfx/misc/rlexplo3 gfx/misc/rlexplo4 gfx/misc/rlexplo5 gfx/misc/rlexplo6 gfx/misc/rlexplo7 gfx/misc/rlexplo8
		rgbGen entitycolorwave 1 1 1 inversesawtooth 0 1 0 8
		blendfunc add
	}
	{
		animmap 8 gfx/misc/rlexplo2 gfx/misc/rlexplo3 gfx/misc/rlexplo4 gfx/misc/rlexplo5 gfx/misc/rlexplo6 gfx/misc/rlexplo7 gfx/misc/rlexplo8 $blackimage
		rgbGen entitycolorwave 1 1 1 sawtooth 0 1 0 8
		blendfunc add
	}
}

gfx/rocket_explosion_ring
{
	cull disable
	softParticle
	{
		Clampmap gfx/misc/rlexplo_ring
		rgbGen entitycolorwave 1 1 1 Inversesawtooth 0 1 0 4
		blendfunc add
		tcMod stretch sawtooth .75 .5 0 4
	}
}

gfx/grenade_explosion
{
	cull disable
	softParticle
	{
		animmap 8 gfx/misc/rlexplo1  gfx/misc/rlexplo2 gfx/misc/rlexplo3 gfx/misc/rlexplo4 gfx/misc/rlexplo5 gfx/misc/rlexplo6 gfx/misc/rlexplo7 gfx/misc/rlexplo8
		rgbGen entitycolorwave 1 1 1 inversesawtooth 0 1 0 8
		blendfunc add
	}
	{
		animmap 8 gfx/misc/rlexplo2 gfx/misc/rlexplo3 gfx/misc/rlexplo4 gfx/misc/rlexplo5 gfx/misc/rlexplo6 gfx/misc/rlexplo7 gfx/misc/rlexplo8 $blackimage
		rgbGen entitycolorwave 1 1 1 sawtooth 0 1 0 8
		blendfunc add
	}
}

gfx/grenade_explosion_ring
{
	cull disable
	softParticle
	{
		Clampmap gfx/misc/rlexplo_ring
		rgbGen entitycolorwave 1 1 1 Inversesawtooth 0 1 0 4
		blendfunc add
		tcMod stretch sawtooth .75 .5 0 4
	}
}

gfx/misc/waterBubble
{
	entityMergable		// allow all the sprites to be merged together
	{
		map gfx/misc/waterBubble
		rgbGen vertex
		alphaGen vertex
		blendFunc blend
	}
}

gfx/raceghost
{
	{
		map gfx/powerups/warshell
		blendfunc add
		alphagen vertex
		rgbgen entity
	}
}

gfx/indicators/radar_decal
{
	polygonOffset

	{
		alphamaskclampmap gfx/indicators/radar_1
		blendFunc blend
		rgbgen vertex
		alphagen vertex
	}

	{
		alphamaskclampmap gfx/indicators/radar_2
		blendFunc blend
		tcMod rotate 6
	}

	{
		alphamaskclampmap gfx/indicators/radar_3
		blendFunc blend
		tcMod rotate -3
	}
}

gfx/misc/playerspawn
{
	cull none
	{
		clampmap gfx/misc/playerspawn
		blendfunc blend
		rgbgen entity
		alphagen wave distanceramp 0 1 80 300
	}
}

gfx/misc/playerspawnmarker
{
	polygonOffset
	sort banner
	{
		clampmap gfx/misc/playerspawnmarker
		blendFunc blend
		rgbgen vertex
		alphagen vertex
	}
}

gfx/misc/teleportshell
{
	cull front
	deformVertexes bulge 2 5 -1.5 1

	{
		map gfx/misc/teleportshell
		blendfunc add
		rgbgen entity
		tcmod scale 2 2
		tcmod scroll 0.25 0.5
		tcMod turb 0.6 0.3 0 0.2
	}
}
