mipmapped_GFX_Template
{
	{
		clampmap gfx/$1.tga
		blendfunc blend
		rgbgen vertex
		alphagen vertex
	}
}

mipmapped_HUD_Template
{
	{
		clampmap gfx/hud/$1.tga
		blendfunc blend
		rgbgen vertex
		alphagen vertex
	}
}

gfx/hud/net
{
	template mipmapped_HUD_Template net
}

gfx/hud/damage
{
	nomipmaps
	{
		alphamaskclampmap gfx/hud/damage.png
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbgen vertex
	}
}

//
// weapon icons
//
simpleIcon_HUD_Template
{
	{
		// not clampmap to avoid loading the textures twice when simple items are on and because these have wide empty areas near the edges
		map gfx/hud/icons/$1.tga
		blendfunc blend
	}
}

weaponIcon_HUD_Template
{
	{
		map gfx/hud/icons/$1.tga
		blendfunc blend
		alphagen vertex
	}
}

gfx/hud/icons/weapon/electro
{
	template weaponIcon_HUD_Template weapon/electro
}
gfx/hud/icons/weapon/grenade
{
	template weaponIcon_HUD_Template weapon/grenade
}
gfx/hud/icons/weapon/plasma
{
	template weaponIcon_HUD_Template weapon/plasma
}
gfx/hud/icons/weapon/riot
{
	template weaponIcon_HUD_Template weapon/riot
}
gfx/hud/icons/weapon/machinegun
{
	template weaponIcon_HUD_Template weapon/machinegun
}
gfx/hud/icons/weapon/rocket
{
	template weaponIcon_HUD_Template weapon/rocket
}
gfx/hud/icons/weapon/gunblade
{
	template weaponIcon_HUD_Template weapon/gunblade
}
gfx/hud/icons/weapon/gunblade_blast
{
	template weaponIcon_HUD_Template weapon/gunblade_blast
}
gfx/hud/icons/weapon/laser
{
	template weaponIcon_HUD_Template weapon/laser
}

key_HUD_Template
{
	{
		clampmap gfx/hud/keys/$1.tga
		blendfunc blend
		alphaGen vertex
	}
}
gfx/hud/keys/key_forward
{
	template key_HUD_Template key_forward
}
gfx/hud/keys/key_back
{
	template key_HUD_Template key_back
}
gfx/hud/keys/key_left
{
	template key_HUD_Template key_left
}
gfx/hud/keys/key_right
{
	template key_HUD_Template key_right
}
gfx/hud/keys/act_fire
{
	template key_HUD_Template act_fire
}
gfx/hud/keys/act_fire_wide
{
	template key_HUD_Template act_fire_wide
}
gfx/hud/keys/act_fire_wide_flip
{
	template key_HUD_Template act_fire_wide_flip
}
gfx/hud/keys/act_jump
{
	template key_HUD_Template act_jump
}
gfx/hud/keys/act_jump_wide
{
	template key_HUD_Template act_jump_wide
}
gfx/hud/keys/act_jump_wide_flip
{
	template key_HUD_Template act_jump_wide_flip
}
gfx/hud/keys/act_crouch
{
	template key_HUD_Template act_crouch
}
gfx/hud/keys/act_crouch_flip
{
	template key_HUD_Template act_crouch_flip
}
gfx/hud/keys/act_special
{
	template key_HUD_Template act_special
}
gfx/hud/keys/act_special_flip
{
	template key_HUD_Template act_special_flip
}
gfx/hud/keys/act_cam
{
	template key_HUD_Template act_cam
}
gfx/hud/keys/act_scores
{
	template key_HUD_Template act_scores
}
gfx/hud/keys/act_scores_flip
{
	template key_HUD_Template act_scores_flip
}
gfx/hud/keys/act_quickmenu
{
	template key_HUD_Template act_quickmenu
}
gfx/hud/keys/act_quickmenu_flip
{
	template key_HUD_Template act_quickmenu_flip
}
gfx/hud/keys/touch_movedir
{
	template key_HUD_Template touch_movedir
}

gfx/hud/icons/drop/bomb
{
	template mipmapped_HUD_Template icons/drop/bomb
}

gfx/bomb/carriericon
{
	template mipmapped_GFX_Template bomb/carriericon
}


//
// strafe helper
//
gfx/hud/strafe
{
	template mipmapped_HUD_Template strafe
}

//
// spectator icons
//
gfx/hud/arrow
{
	template mipmapped_HUD_Template arrow
}

gfx/hud/cam
{
	template mipmapped_HUD_Template cam
}

//
// teammate indicators
//
gfx/indicators/teammate_indicator
{
	template mipmapped_GFX_Template indicators/teammate_indicator
}

gfx/indicators/teamcarrier_indicator
{
	template mipmapped_GFX_Template indicators/teamcarrier_indicator
}
