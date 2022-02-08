local dark_grey = "#222"
local yellow = RGBALinear( 1, 0.64, 0.0225, 1 )
local red = RGBALinear( 1, 0.0484, 0.1444, 1 )

local function SRGBALinear( rgbalinear )

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

	if state.roundType == RoundType_MatchPoint then
		cd.text( options, posX, state.viewport_height * 0.05, "MATCH POINT" )
	elseif state.roundType == RoundType_Overtime then
		cd.text( options, posX, state.viewport_height * 0.05, "OVERTIME" )
	elseif state.roundType == RoundType_OvertimeMatchPoint then
		cd.text( options, posX, state.viewport_height * 0.05, "OVERTIME MATCH POINT" )
	end

	if state.matchState < MatchState_Playing then
		cd.text( options, posX, state.viewport_height * 0.015, "WARMUP" )

		if state.team ~= TEAM_SPECTATOR then
			if state.ready or state.matchState == MatchState_Countdown then
				options.color = "#5f6f"
				cd.text( options, posX, state.viewport_height * 0.05, "READY" )
			else
				options.color = cd.attentionGettingColor()
				cd.text( options, posX, state.viewport_height * 0.04, "Press [" .. cd.getBind( "toggleready" ) .. "] to ready up" )
			end
		end
	elseif state.matchState == MatchState_Playing then
		cd.drawClock( state.viewport_width / 2, state.viewport_width * 0.008, state.viewport_width / 50, "#fff", "center top", 1 )

		if state.teambased then
			options.font_size = state.viewport_height / 20

			options.color = cd.getTeamColor( TEAM_ALPHA )
			cd.text( options, posX - posX / 11, state.viewport_height * 0.012, state.scoreAlpha )
			options.color = cd.getTeamColor( TEAM_BETA )
			cd.text( options, posX + posX / 11, state.viewport_height * 0.012, state.scoreBeta )

			local y = state.viewport_height * 0.008
			local scaleX = state.viewport_height / 40
			local scaleY = scaleX * 1.6
			local step = scaleX
			local grey = "#555"
			local material = cd.asset( "gfx/hud/guy" )

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
	options.border = "#0000"
	options.font_size *= 0.8

	y -= options.font_size * 0.1
	if state.canChangeLoadout then
		options.color = "#fff"
		cd.text( options, x, y, "Press "..cd.getBind( "gametypemenu" ).." to change loadout" )
	elseif state.canPlant then
		options.color = cd.attentionGettingColor()
		cd.text( options, x, y, "Press "..cd.getBind( "+plant" ).." to plant" )
	elseif state.isCarrier then
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

local function DrawAmmoFrac( x, y, size, ammo, ammo_max, material )
	local f = ammo/ammo_max
	local ammo_color = AmmoColor( f )

	cd.box( x, y, size, size, RGBA8( 45, 45, 45 ) )
	cd.box( x, y, size, size, "#666", material )

	cd.boxuv( x, y + size - size*f, size, size*f,
		0, 1 - f, 1, 1,
		ammo_color, nil )
	cd.boxuv( x, y + size - size*f, size, size*f,
		0, 1 - f, 1, 1,
		dark_grey, material )

	local options = {
		color = ammo_color,
		border = "#000f",
		font = "bold",
		font_size = size * 0.4,
		alignment = "left top",
	}
	cd.text( options, x + size * 0.1, y + size * 0.1, ammo )
end

local function DrawPerk( state, x, y, size, outline_size )
	DrawBoxOutline( x, y, size, size, outline_size )
	cd.box( x, y, size, size, "#fff" )
	cd.box( x, y, size, size, dark_grey, cd.getPerkIcon( state.perk ) )
end

local function DrawUtility( state, x, y, size, outline_size )
	if state.gadget ~= Gadget_None then
		DrawBoxOutline( x, y, size, size, outline_size )
		DrawAmmoFrac( x, y, size, state.gadget_ammo, cd.getGadgetAmmo( state.gadget ), cd.getGadgetIcon( state.gadget ) )
	end
end

local function DrawWeaponBar( state, x, y, size, padding )
	cd.drawWeaponBar( x, y + padding * 1.02, size, padding*3, "left top" )
end

local function DrawPlayerBar( state )
	if state.health <= 0 then
		return
	end

	local offset = state.viewport_width * 0.015
	local stamina_bar_height = state.viewport_width * 0.014
	local health_bar_height = state.viewport_width * 0.023
	local empty_bar_height = state.viewport_width * 0.020
	local padding = math.floor( offset/5 );

	local width = state.viewport_width * 0.25
	local height = stamina_bar_height + health_bar_height + empty_bar_height + padding * 4

	local x = offset
	local y = state.viewport_height - offset - height


	local perks_utility_size = state.viewport_width * 0.035
	local perkX = x + padding
	local perkY = y - perks_utility_size - padding * 2
	DrawPerk( state, perkX, perkY, perks_utility_size, padding )
	DrawUtility( state, perkX + perks_utility_size + padding * 3 , perkY, perks_utility_size, padding )
	DrawWeaponBar( state, x + width + padding * 2, y, height * 0.73, padding )

	cd.box( x, y, width, height, dark_grey )

	x += padding
	y += padding
	width -= padding * 2

	local bg_color = RGBALinear( 0.04, 0.04, 0.04, 1 )
	local stamina_color = cd.getTeamColor( TEAM_ALLY )

	if state.perk == Perk_Hooligan then
		cd.box( x, y, width, stamina_bar_height, bg_color )

		stamina_color.a = math.min( 1, state.stamina * 2 )
		cd.box( x, y, width/2, stamina_bar_height, stamina_color )

		stamina_color.a = math.max( 0.0, (state.stamina - 0.5) * 2 )
		cd.box( x + width/2, y, width/2, stamina_bar_height, stamina_color )


		cd.box( x + width / 2 - padding/2, y, padding, stamina_bar_height, dark_grey )
	else
		if state.perk == Perk_Midget and state.stamina <= 0 and state.staminaState == Stamina_UsingAbility then
			stamina_bg_color = cd.attentionGettingColor()
			stamina_bg_color.a = 0.05
		elseif state.perk == Perk_Jetpack then
			local s = 1 - math.min( 1.0, state.stamina + 0.5 )
			stamina_bg_color = RGBALinear( 0.06 + s * 0.8, 0.06 + s * 0.1, 0.06 + s * 0.1, 0.5 + 0.5 * state.stamina )
		end

		cd.box( x, y, width, stamina_bar_height, stamina_bg_color )
		cd.box( x, y, width * state.stamina, stamina_bar_height, stamina_color )
	end

	cd.boxuv( x, y,
			width * state.stamina, stamina_bar_height,
			0, 0, (width * state.stamina)/8, stamina_bar_height/8, --8 is the size of the texture
			RGBALinear( 0, 0, 0, 0.25 + 0.25 * state.stamina ), cd.asset( "gfx/hud/diagonal_pattern" ) )

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

	if state.show_hotkeys and state.chasing == NOT_CHASING then
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
	if state.chasing == NOT_CHASING then
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

	cd.box( pos, pos, scale, scale, "#fff", cd.asset( "gfx/hud/cam" ) )
	cd.text( options, pos + scale * 1.2, pos + scale * 0.5, cd.getPlayerName( state.chasing ) )
end


local function DrawLagging( state )
	if state.lagging then
		local width = state.viewport_width * 0.05
		cd.box( width, width, width, width, "#fff", cd.asset( "gfx/hud/net" ) )
	end
end

local function DrawCallvote( state )
	local width = state.viewport_width * 0.25
	local height = state.viewport_width * 0.08

	local offset = state.viewport_width * 0.025
	local padding = width / 20

	local text_color = "#fff"

	local xleft = offset + padding
	local xright = offset + width - padding
	local ytop = offset + padding
	local ybottom = offset + height - padding
	
	if not state.hasVoted then
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
	cd.text( options, xright, ytop, state.votesTotal .. "/" .. state.votesRequired )

	options.font_size *= 0.8
	options.alignment = "left bottom"
	cd.text( options, xleft, ybottom, "["..cd.getBind("vote yes").."] Vote yes" )

	options.alignment = "right bottom"
	cd.text( options, xright, ybottom, "["..cd.getBind("vote no").."] Vote no" )
end

return function( state )
	if state.matchState < MatchState_PostMatch then
		cd.drawBombIndicators( state.viewport_height / 26, state.viewport_height / 60 ) -- site name size, site message size (ATTACK/DEFEND/...)

		DrawTopInfo( state )

		if state.team ~= TEAM_SPECTATOR then
			DrawPlayerBar( state )
		end

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


	if string.len( state.vote ) > 0 then
		DrawCallvote( state )
	end
end
