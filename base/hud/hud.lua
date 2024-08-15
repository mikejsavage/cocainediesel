local dark_grey = "#222"
local yellow = RGBALinear( 1, 0.64, 0.0225, 1 )
local red = RGBALinear( 0.8, 0, 0.05, 1 )

local function hotkeys( state )
	return state.show_hotkeys and state.chasing == NOT_CHASING
end

local function Override( t, overrides )
	local t2 = table.clone( t ) -- note that this is a shallow copy
	for k, v in pairs( overrides ) do
		t2[ k ] = v
	end
	return t2
end

local function DrawClockOrBomb( state, posX )
	if state.round_state < RoundState_Countdown or state.round_state > RoundState_Round then
		return
	end

	local fontSize1 = state.viewport_height / 25
	local testPosY = state.viewport_height * 0.013
	local options = {
		color = "#ffff",
		border = "#000b",
		font = "bold",
		font_size = fontSize1,
		alignment = "center top",
	}

	if state.round_state == RoundState_Countdown and state.gametype == Gametype_Bomb then
		local text = "DEFUSE"
		if state.attacking_team == team then
			text = "PLANT"
		end

		options.font_size = fontSize1 * 0.8
		options.alignement = "middle top"
		cd.text( options, posX, testPosY * 1.2, text )
	else
		local time = cd.getClockTime()
		local seconds = time * 0.001
		local milliseconds = time % 1000

		if seconds >= 0 then
			local minutes = seconds / 60
			seconds = seconds % 60

			if minutes < 1 and seconds < 11 and seconds ~= 0 then
				options.color = "#f00" -- TODO: attention getting red
			end

			options.alignment = "right top"
			cd.text( options, posX, testPosY, string.format( "%02i", seconds ) )

			options.alignment = "left bottom"
			local ms_options = Override( options, { font_size = fontSize1 * 0.6 } )
			cd.text( ms_options, posX + posX * 0.001, testPosY + fontSize1 * 0.6, string.format( ".%03i", milliseconds ))
		elseif state.gametype == Gametype_Bomb then
			local size = state.viewport_height * 0.055
			cd.box( posX - size/2.4, state.viewport_height * 0.025 - size/2, size, size, "#fff", assets.bomb )
		end
	end
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

		if state.real_team then
			if state.ready or state.match_state == MatchState_Countdown then
				options.color = "#5f6f"
				cd.text( options, posX, posY, "READY" )
			else
				options.color = cd.attentionGettingColor()
				cd.text( options, posX, state.viewport_height * 0.04, "Press [" .. cd.getBind( "toggleready" ) .. "] to ready up" )
			end
		end
	elseif state.match_state == MatchState_Playing then
		DrawClockOrBomb( state, posX )

		if state.gametype == Gametype_Bomb then
			options.font_size = state.viewport_height / 25

			options.color = cd.getTeamColor( Team_One )
			cd.text( options, posX - posX / 11, state.viewport_height * 0.012, state.scoreAlpha )
			options.color = cd.getTeamColor( Team_Two )
			cd.text( options, posX + posX / 11, state.viewport_height * 0.012, state.scoreBeta )

			local y = state.viewport_height * 0.008
			local scaleX = state.viewport_height / 40
			local scaleY = scaleX * 1.6
			local step = scaleX
			local grey = "#222"
			local material = assets.guy

			local color = cd.getTeamColor( Team_One )
			local x = posX - posX * 0.2

			for i = 0, state.totalAlpha - 1, 1 do
				if i < state.aliveAlpha then
					cd.box( x - step * i, y, scaleX, scaleY, color, material )
				else
					cd.box( x - step * i, y, scaleX, scaleY, grey, material )
				end
			end

			color = cd.getTeamColor( Team_Two )
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

local function DrawChasing( state, x1, y1, x2, y2 )
	if not state.chasing then
		return
	end

	local scale_text = state.viewport_width * 0.0198
	local scale_name = state.viewport_width * 0.03

	local options = {
		color = "#eeef",
		border = "#000b",
		font = "normal",
		font_size = scale_text,
		alignment = "left bottom",
	}

	cd.text( options, x1, y1 - scale_text * 0.5, "Spectating :" )

	options.color = "#ffff"
	options.font_size = scale_name
	options.font = "bold"
	cd.text( options, x2, y2 - scale_name * 0.25, cd.getPlayerName( state.chasing ) )
end

local function DrawHotkeys( state, options, x, y )
	options.alignment = "center middle"
	options.font_size *= 0.8

	y -= options.font_size * 0.1
	if state.can_change_loadout then
		options.color = "#fff"
		cd.text( options, x, y, "Press "..cd.getBind( "loadoutmenu" ).." to change loadout" )
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
	if state.gadget ~= Gadget_None and state.gadget_ammo ~= 0 then
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

local function DrawWeapon( state, options, x, y, width, height, padding, weaponID, weaponInfo, show_bind )
	local h = height
	local ammo = weaponInfo.ammo
	local max_ammo = weaponInfo.max_ammo
	local selected = state.weapon == weaponInfo.weapon
	local whiteBarHeight = height * 0.0125

	if show_bind then
		options.color = "#fff"
		options.alignment = "center middle"
		options.font_size = width * 0.2
		cd.text( options, x + width/2, y - options.font_size, cd.getBind( string.format("weapon %d", weaponID) ) )
	end


	if selected then
		h += whiteBarHeight * 2
	end

	local frac = -1
	if max_ammo ~= 0 then
		frac = ammo/max_ammo
	end

	if state.weapon == weaponInfo.weapon then
		h *= 1.05
		if state.weapon_state == WeaponState_Reloading or state.weapon_state == WeaponState_StagedReloading then
			local reload_frac = state.weapon_state_time/cd.getWeaponReloadTime( weaponInfo.weapon )
			frac = cd.getWeaponStagedReload( weaponInfo.weapon ) and frac + reload_frac / max_ammo or reload_frac
		end
	end

	DrawBoxOutline( x, y, width, h, padding )

	options.font_size = width * 0.22
	options.alignment = "left top"

	DrawAmmoFrac( options, x, y, width, ammo, frac, cd.getWeaponIcon( weaponInfo.weapon ) )
	if selected then
		cd.box( x, y + width + padding, width, whiteBarHeight, "#fff" )
		cd.box( x, y + h, width, whiteBarHeight, "#fff" )
	end

	if selected then
		options.color = "#fff"
	else
		options.color = "#999"
	end
	options.alignment = "center middle"
	options.font_size = width * 0.18
	cd.text( options, x + width/2, y + width + (h - width + padding)/2, weaponInfo.name )
end

local function DrawWeaponBar( state, options, x, y, width, height, padding )
	x += padding
	y += padding
	height -= padding * 2
	width -= padding * 2
	local show_bind = hotkeys( state )

	for k, v in ipairs( state.weapons ) do
		DrawWeapon( state, options, x, y, width, height, padding, k, v, show_bind )
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

local function DrawStaminaBar( state, x, y, width, height, padding, bg_color )
	local stamina_color = cd.allyColor()

	if state.perk == Perk_Hooligan then
		DrawBoxOutline( x, y, width, height, padding )

		local steps = 2
		local cell_width = width/steps

		for i = 0, steps - 1, 1 do
			if (state.stamina * steps) >= (i + 1) then
				cd.box( x + cell_width * i, y, cell_width, height, stamina_color )
			else
				stamina_color.a = 0.1
				cd.box( x + cell_width * i, y, cell_width * (state.stamina * steps - i), height, stamina_color )
				break
			end
		end

		local uvwidth = width * math.floor( state.stamina * steps )/steps
		if state.stamina > 0 then
			cd.boxuv( x, y,
				uvwidth, height,
				0, 0, uvwidth/8, height/8, --8 is the size of the texture
				RGBALinear( 0, 0, 0, 0.5 ), assets.diagonal_pattern )
		end

		for i = 1, steps - 1, 1 do
			cd.box( x + cell_width * i - padding/2, y, padding, height, dark_grey )
		end
	else
		if state.perk == Perk_Wheel or state.perk == Perk_Jetpack then
			local s = 1 - math.min( 1.0, state.stamina + 0.3 )
			if state.stamina_state == Stamina_Reloading then
				s = 1 - math.min( 1.0, state.stamina - 0.15 )
			end

			stamina_color.r = math.min( 1.0, stamina_color.r + s )
			stamina_color.g = math.max( 0.0, stamina_color.g - s )
			stamina_color.b = math.max( 0.0, stamina_color.b - s * 2.0 )
		else
			cd.box( x, y, width, height, bg_color )
		end

		cd.box( x, y, width * state.stamina, height, stamina_color )
		cd.boxuv( x, y,
			width * state.stamina, height,
			0, 0, (width * state.stamina)/8, height/8, --8 is the size of the texture
			RGBALinear( 0, 0, 0, 0.5 ), assets.diagonal_pattern )
	end
end

local function DrawPlayerBar( state, offset, padding )
	if state.health <= 0 or state.ghost or state.zooming then
		return
	end

	local stamina_bar_height = state.viewport_width * 0.015
	local health_bar_height = state.viewport_width * 0.022
	local empty_bar_height = state.viewport_width * 0.019

	local width = state.viewport_width * 0.25
	local height = stamina_bar_height + health_bar_height + empty_bar_height + padding * 4

	local x = offset
	local y = state.viewport_height - offset - height

	local perks_utility_size = state.viewport_width * 0.03
	local perkX = x + padding
	local perkY = y - perks_utility_size - padding * 2
	local weapons_options = {
		border = "#000f",
		font = "bolditalic",
		alignment = "left top",
	}
	DrawChasing( state, perkX, perkY, perkX + perks_utility_size * 2 + padding * 5, perkY )
	DrawPerk( state, perkX, perkY, perks_utility_size, padding )
	DrawUtility( state, weapons_options, perkX + perks_utility_size + padding * 3 , perkY, perks_utility_size, padding )
	DrawWeaponBar( state, weapons_options, x + width + padding, y, height * 0.85, height, padding )

	cd.box( x, y, width, height, dark_grey )

	x += padding
	y += padding
	width -= padding * 2

	local bg_color = RGBALinear( 0.04, 0.04, 0.04, 1 )
	DrawStaminaBar( state, x, y, width, stamina_bar_height, padding, bg_color )

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
		font_size = 20,
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
		local width = state.viewport_width * 0.2
		local height = state.viewport_height / 30
		local y = state.viewport_height * 0.8
		local progress = state.bomb_progress / 100

		cd.box( ( state.viewport_width - width ) * 0.5, y, width, height, "#2228" )
		cd.box( ( state.viewport_width - width * progress ) * 0.5, y, width * progress, height, cd.allyColor() )

		local text = { color = "#fff", border = "#000", font_size = height * 0.75, alignment = "center middle" }
		local message = if state.bomb_progress_type == BombProgress_Planting then "Planting..." else "Defusing..."
		cd.text( text, state.viewport_width * 0.5, y + height * 0.4, message )
	end
end

local function DrawLagging( state )
	if state.lagging then
		local width = state.viewport_width * 0.05
		cd.box( width, width, width, width, "#fff", assets.net )
	end
end

local function DrawScoreboardEmptySlot( options, Y, nameX, scoreX, killsX, pingX )
	options.color = "#444"
	options.alignment = "left middle"
	cd.text( options, nameX, Y, "--------" )
	options.alignment = "center middle"
	cd.text( options, scoreX, Y, "---" )
	cd.text( options, killsX, Y, "---" )
	cd.text( options, pingX, Y, "---" )
end

local function DrawScoreboardPlayer( state, options, X, Y, width, height, nameX, scoreX, killsX, pingX, readyX, player, teamColor )
	Y += height/2

	if not player.alive then
		teamColor.a = 0.2
	end
	
	options.font_size = height
	options.color = teamColor
	options.alignment = "left middle"
	cd.text( options, nameX, Y, string.sub( player.name, 1, (scoreX - nameX)/(height * 0.45) ) )
	teamColor.a = 1.0 -- hack because teamcolor seems to be a reference

	options.color = RGBALinear( 1, 1, 1, 1 )
	options.alignment = "center middle"
	cd.text( options, scoreX, Y, player.score )
	cd.text( options, killsX, Y, player.kills )
	options.color.g = math.max(0.0, 1.0 - player.ping * 0.002)
	options.color.b = math.max(0.0, 1.0 - player.ping * 0.01)
	cd.text( options, pingX, Y, player.ping )

	if state.match_state == MatchState_Warmup and player.ready then
		options.color = yellow
		options.alignment = "left middle"
		options.font_size = height * 0.8
		cd.text( options, readyX, Y, "READY" )
		options.font_size = height
	end
end

local function DrawScoreboardTeam( state, X, Y, width, outline, numPlayersTeam, numLines, team, lineHeight, options )
	local tabHeight = lineHeight * (numLines + 1) + outline * numLines -- add one line for description
	local teamColor = cd.getTeamColor( team )
	local teamState = state.teams[ team ]

	local nameX = X + outline
	local scoreX = X + width * 0.58
	local killsX = X + width * 0.76
	local pingX = X + width * 0.92
	local readyX = X + width * 1.025

	DrawBoxOutline( X, Y, width, tabHeight, outline )
	cd.box( X, Y, width, lineHeight, teamColor )

	options.color = dark_grey
	options.alignment = "left middle"
	local team_text = "DEFENDING"
	if state.attacking_team == team then
		team_text = "ATTACKING"
	end
	cd.text( options, nameX, Y + lineHeight/2, team_text )
	options.alignment = "center middle"
	cd.text( options, scoreX, Y + lineHeight/2, "SCORE" )
	cd.text( options, killsX, Y + lineHeight/2, "KILLS" )
	cd.text( options, pingX, Y + lineHeight/2, "PING" )

	for k, player in pairs( teamState.players ) do
		Y += lineHeight + outline

		DrawScoreboardPlayer( state, options, X, Y, width, lineHeight, nameX, scoreX, killsX, pingX, readyX, player, teamColor )
	end

	if numLines > numPlayersTeam then
		options.color = "#444"
		options.alignment = "left middle"
		for i = 0, numLines - numPlayersTeam - 1, 1 do
			Y += lineHeight + outline
			DrawScoreboardEmptySlot( options, Y + lineHeight/2 + outline, nameX, scoreX, killsX, pingX )
		end
	end

	return tabHeight + outline * 3
end

local function DrawScoreboard( state, offset, outline )
	if not state.scoreboard then
		return
	end

	local width = state.viewport_width * 0.29
	local titleHeight = state.viewport_height * 0.07
	local lineHeight = state.viewport_height * 0.03
	local X = offset
	local Y = offset
	
	local starsX = X
	local starsWidth = lineHeight * state.scorelimit

	local title_options = {
		color = dark_grey,
		border = "#0000",
		font = "bolditalic",
		font_size = titleHeight,
		alignment = "center middle",
	}

	local text_options = {
		border = "#0000",
		font = "bolditalic",
		font_size = lineHeight
	}

	if state.gametype == Gametype_Gladiator then
		width = state.viewport_width * 0.36 + starsWidth * 0.5
	end

	DrawBoxOutline( X, Y, width, titleHeight, outline )
	cd.box( X, Y, width, titleHeight, yellow )
	cd.text( title_options, X + width/2, Y + titleHeight/2, "SCOREBOARD" )
	Y += titleHeight + outline * 3

	if state.gametype == Gametype_Bomb then -- Bomb
		local numPlayerTeamOne = state.teams[ Team_One ].num_players
		--cd.text(title_options, 0, 0, numPlayersTeamOne)
		local numPlayerTeamTwo = state.teams[ Team_Two ].num_players
		--cd.text(title_options, 0, 50, numPlayersTeamTwo)
		local numLines = math.max(4, math.max(numPlayerTeamOne, numPlayerTeamTwo))

		Y += DrawScoreboardTeam( state, X, Y, width, outline, numPlayerTeamOne, numLines, Team_One, lineHeight, text_options )
		Y += DrawScoreboardTeam( state, X, Y, width, outline, numPlayerTeamTwo, numLines, Team_Two, lineHeight, text_options )
	else -- Gladiator
		local placeX = starsX + starsWidth + width * 0.06 - outline
		local nameX = placeX + width * 0.06
		local scoreX = X + width * 0.68
		local killsX = X + width * 0.8
		local pingX = X + width * 0.92
		local readyX = X + width * 1.025

		local numPlayers = 0
		for t = Team_One, Team_Count - 1, 1 do
			numPlayers += state.teams[ t ].num_players
		end

		local numLines = math.max(numPlayers, Team_Count - 1)

		DrawBoxOutline( X, Y, width, lineHeight * (numLines + 1) + outline * numLines, outline )
		cd.box( X, Y, width, lineHeight, "#fff" )
		text_options.color = dark_grey

		text_options.alignment = "center middle"
		cd.text( text_options, starsX + starsWidth * 0.5, Y + lineHeight/2, "SCORE" )
		text_options.alignment = "left middle"
		cd.text( text_options, nameX, Y + lineHeight/2, "NAME" )
		text_options.alignment = "center middle"
		cd.text( text_options, scoreX, Y + lineHeight/2, "WINS" )
		cd.text( text_options, killsX, Y + lineHeight/2, "KILLS" )
		cd.text( text_options, pingX, Y + lineHeight/2, "PING" )
		
		local place = 0
		local line = 0
		local previous_max_score = state.scorelimit
		while line < numLines do
			place += 1

			local max_score = 0
			for k, team in pairs( state.teams ) do
				if team.score > max_score and team.score < previous_max_score then
					max_score = team.score
				end
			end
			previous_max_score = max_score

			for k, team in pairs( state.teams ) do
				if team.score == max_score then
					local teamColor = cd.getTeamColor( k )
					local starColorBack = "#aaa"
					local starColorFront = yellow
					
					if team.num_players == 0 then
						starColorBack = "#555"
					end

					local tmpY = Y + lineHeight + outline
					cd.boxuv( starsX, tmpY, starsWidth, lineHeight, 0, 0, state.scorelimit, 1, starColorBack, assets.star )
					cd.boxuv( starsX + lineHeight * (state.scorelimit - team.score), tmpY, lineHeight * team.score, lineHeight, 0, 0, team.score, 1, starColorFront, assets.star )
					yellow.a = 1.0

					local placeText = ""
					if place == 1 then placeText = "1st"
					elseif place == 2 then placeText = "2nd"
					elseif place == 3 then placeText = "3rd"
					else placeText = place .. "th" end

					text_options.color = RGBALinear( 1.0, 1.0, 1.0, 1.0/place )
					text_options.alignment = "center middle"
					cd.text( text_options, placeX, tmpY + lineHeight/2, placeText )

					if team.num_players == 0 then
						line += 1
						Y += lineHeight + outline
						DrawScoreboardEmptySlot( text_options, Y + lineHeight/2 + outline, nameX, scoreX, killsX, pingX )
					else
						for k, player in pairs( team.players ) do
							line += 1
							Y += lineHeight + outline
							DrawScoreboardPlayer( state, text_options, X, Y, width, lineHeight, nameX, scoreX, killsX, pingX, readyX, player, teamColor )
						end
					end
				end
			end
		end

		Y += lineHeight + outline * 3
	end

	if state.spectating > 0 then
		DrawBoxOutline( X, Y, width, lineHeight + outline, outline )
		text_options.color = "#fff"
		text_options.alignment = "left middle"
		cd.text( text_options, nameX, Y + lineHeight/2, state.spectating .. " spectating")
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
	cd.text( options, xleft, ybottom, "["..cd.getBind("vote_yes").."] Vote yes" )

	options.alignment = "right bottom"
	cd.text( options, xright, ybottom, "["..cd.getBind("vote_no").."] Vote no" )
end

local function DrawYogaStuff( state )
	local perk = {
		background_color = "#fff",
		border = 4,
		border_color = dark_grey,
		width = "3.5vw",
		aspect_ratio = 1,
		margin_right = 4,
	}

	local gadget = {
		width = "3.5vw",
		height = "100%",
		flow = "column",
		children = {
			{
				background_color = "#fff4",
				grow = 1,
				content = "hello",
			},
			{
				aspect_ratio = 1,
				background_color = yellow,
				border = 4,
				border_color = dark_grey,
				content = cd.getGadgetIcon( state.gadget ),
			},
		},
	}

	cd.yoga( {
		absolute_position = true,
		align_items = "flex-end",
		left = "1.5vw",
		right = "1.5vw",
		top = "1.5vw",
		height = "8%",
		children = { perk, gadget },
	} )

	-- local weapon_icons = { }
	-- for i = 1, 4 do
	-- 	weapon_icons[ i ] = {
	-- 		aspect_ratio = 1,
	-- 		margin_right = "0.75vw",
	-- 		background_color = RGBALinear( 1, 1, 1, i / 4 ),
	-- 		height = "100%",
	-- 		border_color = "#000",
	-- 		border = 2,
	-- 		border_bottom = 10,
	-- 	}
	-- end
        --
	-- cd.yoga( {
	-- 	absolute_position = true,
	-- 	left = "1.5vw",
	-- 	right = "1.5vw",
	-- 	top = "1.5vw",
	-- 	height = "15%",
	-- 	background_color = "#f00",
	-- 	children = weapon_icons,
	-- } )
end

return function( state )
	local offset = state.viewport_width * 0.01
	local padding = math.floor( offset * 0.3 )

	if state.match_state < MatchState_PostMatch then
		cd.drawBombIndicators( state.viewport_height / 26, state.viewport_height / 60, state.viewport_height / 50 ) -- site name size, site message size (ATTACK/DEFEND/...)

		DrawTopInfo( state )

		DrawPlayerBar( state, offset, padding )

		DrawDevInfo( state )

		DrawBombProgress( state )

		cd.drawCrosshair()
		cd.drawDamageNumbers( state.viewport_height / 30, state.viewport_height / 50 ) -- obituary msg size, dmg numbers size
		cd.drawPointed( state.viewport_height / 80, "#fff", 1 ) -- font size, color, border

		DrawLagging( state )

		cd.drawObituaries( state.viewport_width - 10, 2, state.viewport_width / 10, state.viewport_width / 10,
						   state.viewport_height / 20, state.viewport_width / 70, "right top" )
	end

	DrawScoreboard( state, offset, padding )

	DrawCallvote( state )

	-- DrawYogaStuff( state )
end
