local dark_grey = "#222"
local yellow = RGBALinear( 1, 0.64, 0.0225, 1 )
local red = RGBALinear( 0.8, 0, 0.05, 1 )

local function hotkeys( state )
	return state.show_hotkeys and state.chasing == NOT_CHASING
end

local function DrawTopInfo( state )
	local options = {
		color = "#ffff",
		border = "#000b",
		font = "bold",
		font_size = state.viewport_height / 40,
		alignment = "center top",
	}

	local posX = state.viewport_width / 2
	local posY = state.viewport_height * 0.05

	if state.round_type == RoundType_MatchPoint then
		cd.text( options, posX, posY, "MATCH POINT" )
	elseif state.round_type == RoundType_Overtime then
		cd.text( options, posX, posY, "OVERTIME" )
	elseif state.round_type == RoundType_OvertimeMatchPoint then
		cd.text( options, posX, posY, "OVERTIME MATCH POINT" )
	end

	if state.match_state < MatchState_Playing then
		cd.text( options, posX, state.viewport_height * 0.015, "WARMUP" )

		if state.team ~= TEAM_SPECTATOR then
			if state.ready or state.match_state == MatchState_Countdown then
				options.color = "#5f6f"
				cd.text( options, posX, posY, "READY" )
			else
				options.color = cd.attentionGettingColor()
				cd.text( options, posX, state.viewport_height * 0.04, "Press [" .. cd.getBind( "toggleready" ) .. "] to ready up" )
			end
		end
	elseif state.match_state == MatchState_Playing then
		options.font_size = state.viewport_height / 25
		local seconds = cd.getClockTime()

		if seconds >= 0 then
			local minutes = seconds / 60
			seconds = seconds % 60

			if minutes < 1 and seconds < 11 and seconds ~= 0 then
				options.color = "#f00"
			end
			cd.text( options, posX, state.viewport_height * 0.012, string.format( "%d:%02i", minutes, seconds ) )
		elseif state.teambased then
			local size = state.viewport_height * 0.055
			cd.box( posX - size/2.4, state.viewport_height * 0.025 - size/2, size, size, dark_grey, assets.bomb )
		end


		if state.teambased then
			options.color = cd.getTeamColor( TEAM_ALPHA )
			cd.text( options, posX - posX / 11, state.viewport_height * 0.012, state.scoreAlpha )
			options.color = cd.getTeamColor( TEAM_BETA )
			cd.text( options, posX + posX / 11, state.viewport_height * 0.012, state.scoreBeta )

			local y = state.viewport_height * 0.008
			local scaleX = state.viewport_height / 40
			local scaleY = scaleX * 1.6
			local step = scaleX
			local grey = "#555"
			local material = assets.guy

			local color = cd.getTeamColor( TEAM_ALPHA )
			local x = posX - posX * 0.2

			for i = 0, state.totalAlpha - 1, 1 do
				if i < state.aliveAlpha then
					cd.box( x - step * i, y, scaleX, scaleY, color, material )
				else
					cd.box( x - step * i, y, scaleX, scaleY, grey, material )
				end
			end

			color = cd.getTeamColor( TEAM_BETA )
			x = posX + posX * 0.2

			for i = 0, state.totalBeta - 1, 1 do
				if i < state.aliveBeta then
					cd.box( x + step * i, y, scaleX, scaleY, color, material )
				else
					cd.box( x + step * i, y, scaleX, scaleY, grey, material )
				end
			end
		end
	end
end

local function DrawHotkeys( state, options, x, y )
	options.alignment = "center middle"
	options.font_size *= 0.8

	y -= options.font_size * 0.1
	if state.can_change_loadout then
		options.color = "#fff"
		cd.text( options, x, y, "Press "..cd.getBind( "gametypemenu" ).." to change loadout" )
	elseif state.can_plant then
		options.color = cd.plantableColor()
		cd.text( options, x, y, "Press "..cd.getBind( "+plant" ).." to plant" )
	elseif state.is_carrier then
		options.color = "#fff"
		cd.text( options, x, y, "Press "..cd.getBind( "drop" ).." to drop the bomb" )
	end
end

local function DrawBoxOutline( x, y, sizeX, sizeY, outline_size )
	cd.box( x - outline_size, y - outline_size, sizeX + outline_size * 2, sizeY + outline_size * 2, dark_grey )
end

local function AmmoColor( frac )
	return RGBALinear( 
		(red.r - red.r*frac + yellow.r*frac), 
		(red.g - red.g*frac + yellow.g*frac),
		(red.b - red.b*frac + yellow.b*frac), 1 )
end

local function DrawAmmoFrac( options, x, y, size, ammo, frac, material )
	if frac == -1 then
		cd.box( x, y, size, size, yellow )
		cd.box( x, y, size, size, dark_grey, material )
		return
	end

	if frac > 1 then
		frac = 1
	end

	local ammo_color = AmmoColor( frac )

	cd.box( x, y, size, size, RGBA8( 45, 45, 45 ) )
	cd.box( x, y, size, size, "#666", material )

	cd.boxuv( x, y + size - size*frac, size, size*frac,
		0, 1 - frac, 1, 1,
		ammo_color, nil )
	cd.boxuv( x, y + size - size*frac, size, size*frac,
		0, 1 - frac, 1, 1,
		dark_grey, material )

	options.color = ammo_color
	cd.text( options, x + options.font_size * 0.28, y + options.font_size * 0.28, ammo )
end

local function DrawPerk( state, x, y, size, outline_size )
	DrawBoxOutline( x, y, size, size, outline_size )
	cd.box( x, y, size, size, "#fff" )
	cd.box( x, y, size, size, dark_grey, cd.getPerkIcon( state.perk ) )
end

local function DrawUtility( state, options, x, y, size, outline_size )
	if state.gadget ~= Gadget_None then
		options.font_size = size * 0.3
		if hotkeys( state ) then
			options.color = "#fff"
			options.alignment = "center middle"
			cd.text( options, x + size / 2, y - options.font_size, cd.getBind("+gadget") )
		end

		DrawBoxOutline( x, y, size, size, outline_size )
		options.alignment = "left top"
		DrawAmmoFrac( options, x, y, size, state.gadget_ammo, state.gadget_ammo/cd.getGadgetAmmo( state.gadget ), cd.getGadgetIcon( state.gadget ) )
	end
end

local function DrawWeaponBar( state, options, x, y, width, height, padding )
	x += padding
	y += padding
	height -= padding * 2
	width -= padding * 2
	local show_bind = hotkeys( state )

	for k, v in ipairs( state.weapons ) do
		local h = height
		local ammo = v.ammo
		local max_ammo = v.max_ammo

		if show_bind then
			options.color = "#fff"
			options.alignment = "center middle"
			options.font_size = width * 0.2
			cd.text( options, x + width/2, y - options.font_size, cd.getBind( string.format("weapon %d", k) ) )
		end


		if state.weapon == v.weapon then
			h *= 1.05
		end

		local frac = -1
		if max_ammo ~= 0 then
			frac = ammo/max_ammo
		end

		if state.weapon == v.weapon then
			h *= 1.05
			if state.weapon_state == WeaponState_Reloading or state.weapon_state == WeaponState_StagedReloading then
				frac = state.weapon_state_time/cd.getWeaponReloadTime( v.weapon )
			end
		end

		DrawBoxOutline( x, y, width, h, padding )

		options.font_size = width * 0.22
		options.alignment = "left top"

		DrawAmmoFrac( options, x, y, width, ammo, frac, cd.getWeaponIcon( v.weapon ) )

		options.color = "#fff"
		options.alignment = "center middle"
		options.font_size = width * 0.18
		cd.text( options, x + width/2, y + width + (h - width + padding)/2, v.name )

		x += width + padding * 3
	end

	if state.is_carrier then
		DrawBoxOutline( x, y, width, width, padding )
		if state.can_plant then
			cd.box( x, y, width, width, cd.plantableColor() )
		else
			cd.box( x, y, width, width, "#666" )
		end
		cd.box( x, y, width, width, dark_grey, assets.bomb )
	end
end

local function DrawPlayerBar( state )
	if state.health <= 0 or state.team == TEAM_SPECTATOR or state.zooming then
		return
	end

	local offset = state.viewport_width * 0.015
	local stamina_bar_height = state.viewport_width * 0.014
	local health_bar_height = state.viewport_width * 0.023
	local empty_bar_height = state.viewport_width * 0.021
	local padding = math.floor( offset/5 );

	local width = state.viewport_width * 0.24
	local height = stamina_bar_height + health_bar_height + empty_bar_height + padding * 4

	local x = offset
	local y = state.viewport_height - offset - height


	local perks_utility_size = state.viewport_width * 0.035
	local perkX = x + padding
	local perkY = y - perks_utility_size - padding * 2
	local weapons_options = {
		border = "#000f",
		font = "bolditalic",
		alignment = "left top",
	}
	DrawPerk( state, perkX, perkY, perks_utility_size, padding )
	DrawUtility( state, weapons_options, perkX + perks_utility_size + padding * 3 , perkY, perks_utility_size, padding )
	DrawWeaponBar( state, weapons_options, x + width + padding, y, height * 0.85, height, padding )

	cd.box( x, y, width, height, dark_grey )

	x += padding
	y += padding
	width -= padding * 2

	local bg_color = RGBALinear( 0.04, 0.04, 0.04, 1 )
	local stamina_color = cd.getTeamColor( TEAM_ALLY )

	if state.perk == Perk_Hooligan then
		cd.box( x, y, width, stamina_bar_height, bg_color )

		local steps = math.floor( state.stamina_stored )
		local cell_width = width/steps
		stamina_color.a = math.min( state.stamina * steps, 1 )
		cd.box( x, y, cell_width, stamina_bar_height, stamina_color )

		for i = 0, steps, 1 do
			stamina_color.a = math.clamp( state.stamina * steps - i, 0, 1 )
			cd.box( x + cell_width * i, y, cell_width, stamina_bar_height, stamina_color )
		end

		local uvwidth = width * math.floor( state.stamina * steps + 0.99 )/steps
		if state.stamina > 0 then
			cd.boxuv( x, y,
				uvwidth, stamina_bar_height,
				0, 0, uvwidth/8, stamina_bar_height/8, --8 is the size of the texture
				RGBALinear( 0, 0, 0, 0.5 ), assets.diagonal_pattern )
		end

		for i = 1, steps - 1, 1 do
			cd.box( x + cell_width * i - padding/2, y, padding, stamina_bar_height, dark_grey )
		end
	else
		if state.perk == Perk_Midget and state.stamina <= 0 and state.stamina_state == Stamina_UsingAbility then
			bg_color = cd.attentionGettingColor()
			bg_color.a = 0.05
		elseif state.perk == Perk_Jetpack then
			local s = 1 - math.min( 1.0, state.stamina + 0.5 )
			bg_color = RGBALinear( 0.06 + s * 0.8, 0.06 + s * 0.1, 0.06 + s * 0.1, 0.5 + 0.5 * state.stamina )
		end

		cd.box( x, y, width, stamina_bar_height, bg_color )
		cd.box( x, y, width * state.stamina, stamina_bar_height, stamina_color )

		cd.boxuv( x, y,
			width * state.stamina, stamina_bar_height,
			0, 0, (width * state.stamina)/8, stamina_bar_height/8, --8 is the size of the texture
			RGBALinear( 0, 0, 0, 0.5 ), assets.diagonal_pattern )
	end

	y += stamina_bar_height + padding

	cd.box( x, y, width, health_bar_height, bg_color )
	local hp = state.health / state.max_health
	local hp_color = RGBALinear( sRGBToLinear(1 - hp), sRGBToLinear(hp), sRGBToLinear(hp * 0.3), 1 )
	cd.box( x, y, width * hp, health_bar_height, hp_color )

	y += health_bar_height + padding * 2
	x += padding
	local cross_long = empty_bar_height - padding * 2
	local cross_short = cross_long / 4
	cd.box( x, y + cross_long/2 - cross_short/2, cross_long, cross_short, hp_color )
	cd.box( x + cross_long/2 - cross_short/2, y, cross_short, cross_long, hp_color )

	x += cross_long + padding * 3
	local options = {
		color = hp_color,
		border = "#000a",
		font = "bold",
		font_size = empty_bar_height * 1.05,
		alignment = "left top",
	}

	local hp_text = string.format("%d", state.health)
	cd.text( options, x, y, hp_text )

	if hotkeys( state ) then
		DrawHotkeys( state, options, x + ( width - x + offset + options.font_size * 0.5 * string.len( hp_text ))/2, y + cross_long /2 )
	end
end

local function DrawDevInfo( state )
	local options = {
		color = "#fffb",
		border = "#000b",
		font = "bold",
		font_size = state.viewport_height / 50,
		alignment = "right top",
	}

	if state.show_fps then
		cd.text( options, state.viewport_width - 4, 4, state.fps )
	end

	options.alignment = "right bottom"

	if state.show_speed then
		cd.text( options, state.viewport_width - 4, state.viewport_height - 4, state.speed )
	end
end

local function DrawBombProgress( state )
	if state.bomb_progress ~= 0 then
		local width = state.viewport_width * 0.3
		local progress = state.bomb_progress/100
		local color = RGBALinear( 1, 1, 1, progress )
		cd.box( (state.viewport_width - width)/2, state.viewport_height * 0.7, width * progress, state.viewport_height / 30, color )
	end
end

local function DrawChasing( state )
	if not state.chasing then
		return
	end

	local pos = state.viewport_width * 0.01
	local scale = state.viewport_width * 0.05

	local options = {
		color = "#fffb",
		border = "#000b",
		font = "bold",
		font_size = scale,
		alignment = "left middle",
	}

	cd.box( pos, pos, scale, scale, "#fff", assets.cam )
	cd.text( options, pos + scale * 1.2, pos + scale * 0.5, cd.getPlayerName( state.chasing ) )
end


local function DrawLagging( state )
	if state.lagging then
		local width = state.viewport_width * 0.05
		cd.box( width, width, width, width, "#fff", assets.net )
	end
end

local function DrawCallvote( state )
	if string.len( state.vote ) == 0 then
		return
	end
	
	local width = state.viewport_width * 0.25
	local height = state.viewport_width * 0.08

	local offset = state.viewport_width * 0.025
	local padding = width / 20

	local text_color = "#fff"

	local xleft = offset + padding
	local xright = offset + width - padding
	local ytop = offset + padding
	local ybottom = offset + height - padding
	
	if not state.has_voted then
		cd.box( offset, offset, width, height, "#000a" )
		text_color = cd.attentionGettingColor()
	end

	local options = {
		color = text_color,
		border = "#000b",
		font = "bold",
		font_size = height * 0.3,
		alignment = "left top",
	}

	cd.text( options, xleft, ytop, "Vote : " .. state.vote )
	options.alignment = "right top"
	cd.text( options, xright, ytop, state.votes_total .. "/" .. state.votes_required )

	options.font_size *= 0.8
	options.alignment = "left bottom"
	cd.text( options, xleft, ybottom, "["..cd.getBind("vote yes").."] Vote yes" )

	options.alignment = "right bottom"
	cd.text( options, xright, ybottom, "["..cd.getBind("vote no").."] Vote no" )
end

return function( state )
	if state.match_state < MatchState_PostMatch then
		cd.drawBombIndicators( state.viewport_height / 26, state.viewport_height / 60, state.viewport_height / 50 ) -- site name size, site message size (ATTACK/DEFEND/...)

		DrawTopInfo( state )

		DrawPlayerBar( state )

		DrawDevInfo( state )

		DrawBombProgress( state )

		DrawChasing( state )

		cd.drawCrosshair()
		cd.drawDamageNumbers( state.viewport_height / 30, state.viewport_height / 50 ) -- obituary msg size, dmg numbers size
		cd.drawPointed( state.viewport_height / 80, "#fff", 1 ) -- font size, color, border

		DrawLagging( state )

		cd.drawObituaries( state.viewport_width - 10, 2, state.viewport_width / 10, state.viewport_width / 10,
						   state.viewport_height / 20, state.viewport_width / 70, "right top" )
	end

	DrawCallvote( state )
end
