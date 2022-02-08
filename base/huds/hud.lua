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
	options.font_size *= 0.7

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

local function DrawPlayerBar( state )
	if state.health <= 0 then
		return
	end

	local offset = state.viewport_width * 0.015
	local stamina_bar_height = state.viewport_width * 0.016
	local health_bar_height = state.viewport_width * 0.028
	local empty_bar_height = state.viewport_width * 0.025
	local padding = offset/4;


	local width = state.viewport_width * 0.25
	local height = stamina_bar_height + health_bar_height + empty_bar_height + padding * 4

	local x = offset
	local y = state.viewport_height - offset - height
	cd.box( x, y, width, height, "#222" )

	x += padding
	y += padding
	width -= padding * 2

	local stamina_bg_color = RGBALinear( 0.06, 0.06, 0.06, 1 )
	local stamina_color = cd.getTeamColor( TEAM_ALLY )

	if state.perk == Perk_Hooligan then
		cd.box( x, y, width, stamina_bar_height, stamina_bg_color )

		stamina_color.a = math.min( 1, state.stamina * 2 )
		cd.box( x, y, width/2, stamina_bar_height, stamina_color )

		stamina_color.a = math.max( 0.0, (state.stamina - 0.5) * 2 )
		cd.box( x + width/2, y, width/2, stamina_bar_height, stamina_color )


		cd.box( x + width / 2 - padding/2, y, padding, stamina_bar_height, "#222" )
	else
		if state.perk == Perk_Midget and state.stamina <= 0 and state.staminaState == Stamina_UsingAbility then
			stamina_bg_color = cd.attentionGettingColor()
			stamina_bg_color.a = 0.05
		elseif state.perk == Perk_Jetpack then
			local s = 1 - math.min( 1.0, state.stamina + 0.5 )
			stamina_bg_color = RGBALinear( 0.06 + s * 0.8, 0.06 + s * 0.1, 0.06 + s * 0.1, 0.5 + 0.5 * state.stamina )
		end

		stamina_color.a = state.stamina
		cd.box( x, y, width, stamina_bar_height, stamina_bg_color )
		cd.box( x, y, width * state.stamina, stamina_bar_height, stamina_color )
	end


	y += stamina_bar_height + padding

	cd.box( x, y, width, health_bar_height, "#444" )
	local hp = state.health / state.max_health
	local hp_color = RGBALinear( 1 - hp, hp * 0.5, 0, 1 )
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

local function DrawWeaponBar( state )
	-- copied from health bar lol
	local offset = state.viewport_width * 0.015
	local padding = offset / 4;

	local width = state.viewport_width * 0.25
	local height = state.viewport_width * 0.069 + padding * 3

	local size = 0.1 * state.viewport_height
	local padding = 0.02 * state.viewport_height

	cd.drawWeaponBar( offset + width + padding, state.viewport_height - height - offset, size, padding, "left top" )
end

local function DrawPerksUtility( state )
	-- copied from health bar lol
	local offset = state.viewport_width * 0.015
	local padding = offset / 4;

	local height = state.viewport_width * 0.069 + padding * 3

	local size = 0.0666667 * state.viewport_height
	local icon_padding = 0.02 * state.viewport_height
	local border = size * 0.08 -- compensate for Draw2DBoxPadded adding an outer border, still a tiny bit off

	cd.drawPerksUtility( offset + border, state.viewport_height - offset - height - icon_padding, size, icon_padding, "left bottom" )
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
			DrawPerksUtility( state )
			DrawWeaponBar( state )
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
