local dark_grey = "#222"
local yellow = RGBALinear( 1, 0.64, 0.0225, 1 )
local red = RGBALinear( 0.8, 0, 0.05, 1 )

local function ShowHotkeys( state )
	return state.show_hotkeys and state.chasing == NOT_CHASING
end

local function Override( t, overrides )
	local t2 = table.clone( t ) -- note that this is a shallow copy
	for k, v in pairs( overrides ) do
		t2[ k ] = v
	end
	return t2
end

local function DrawChasing( state, x1, y1, x2, y2 )
	if not state.chasing then
		return
	end

	local scale_text = state.viewport_width * 0.0145
	local scale_name = state.viewport_width * 0.022

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

local function DrawBoxOutline( x, y, sizeX, sizeY, outline_size )
	cd.box( x - outline_size, y - outline_size, sizeX + outline_size * 2, sizeY + outline_size * 2, dark_grey )
end

local function DrawDevInfo( state )
	local options = {
		color = "#fffb",
		border = "#000b",
		font = "bold",
		font_size = 16,
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

		local text = { color = "#fff", border = "#000", font_size = height * 0.55, alignment = "center middle" }
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

	options.font_size = height * 0.75
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
		options.font_size = height * 0.6
		cd.text( options, readyX, Y, "READY" )
		options.font_size = height * 0.75
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
		font_size = titleHeight * 0.7,
		alignment = "center middle",
	}

	local text_options = {
		border = "#0000",
		font = "bolditalic",
		font_size = lineHeight * 0.7,
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
					if team.num_players == 0 then placeText = "---"
					elseif place == 1 then placeText = "1st"
					elseif place == 2 then placeText = "2nd"
					elseif place == 3 then placeText = "3rd"
					else placeText = place .. "th" end

					if team.num_players == 0 then
						text_options.color = "#444"
					else
						text_options.color = RGBALinear( 1.0, 1.0, 1.0, 1.0/place )
					end

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

	local style = {
		font = "bold",
		color = state.has_voted and "#fff" or cd.attentionGettingColor(),
	}

	local top_row_style = Override( style, { font_size = "3vh" } )
	local bottom_row_style = Override( style, { font_size = "2.5vh" } )

	local right_style = {
		alignment = "top-right",
		width = "grow",
	}

	cd.render( cd.node( {
		float = "top-left top-left",
		x_offset = "4vh",
		y_offset = "4vh",
		background = state.has_voted and "#0000" or "#000b",
		width = "fit",
		height = "fit",
		gap = "3vh",
		padding = "2vh",
		flow = "vertical",
	}, {
		cd.node( { width = "grow", height = "fit", gap = "5vh" }, {
			cd.node( top_row_style, string.format( "Vote: %s", state.vote ) ),
			cd.node( Override( top_row_style, right_style ), string.format( "%d/%d", state.votes_total, state.votes_required ) ),
		} ),
		cd.node( { width = "grow", height = "fit", gap = "8vh" }, {
			cd.node( bottom_row_style, string.format( "[%s] Vote yes", cd.getBind( "vote_yes" ) ) ),
			cd.node( Override( bottom_row_style, right_style ), string.format( "[%s] Vote no", cd.getBind( "vote_no" ) ) ),
		} ),
	} ) )
end

local function DrawClock( state, top_style )
	if state.round_state > RoundState_Round then
		return false
	end

	local time = cd.getClockTime()
	local seconds = math.floor( time / 1000 )
	local milliseconds = time % 1000

	local width = "13vh"

	if seconds >= 0 then
		local color = seconds < 10 and cd.attentionGettingRed() or "#fff"

		return cd.node( { alignment = "bottom-center", width = width, height = "grow" }, {
			cd.node( { alignment = "bottom-center" }, {
				cd.node( Override( top_style, { color = color } ), tostring( seconds ) ),
				cd.node( Override( top_style, { color = color, font_size = "1.5vh", padding_bottom = "0.2vh" } ), string.format( ".%03d", milliseconds ) ),
			} )
		} )
	end

	return cd.node( { width = width, height = "grow" }, {
		cd.node( { height = "grow" }, assets.bomb ),
	} )
end

local function DrawPlayerIcons( team, num_alive, total )
	local color = cd.getTeamColor( team )
	local icons = { }
	for i = 1, total, 1 do
		local alive
		if team == Team_One then
			alive = i > total - num_alive
		else
			alive = i <= num_alive
		end
		table.insert( icons, cd.node( { color = alive and color or "#222", height = "grow", width = 60 }, assets.guy ) )
	end

	return cd.node( { alignment = team == Team_One and "top-right" or "top-left", width = "20vh", height = "grow", padding_x = "5vh" }, icons )
end

local function DrawScore( team, score, top_style )
	return cd.node( Override( top_style, { color = cd.getTeamColor( team ) } ), tostring( score ) )
end

local function DrawTop( state )
	local top_style = {
		width = "grow",
		alignment = "top-center",
		font = "bold",
		font_size = "1.75vh",
		text_border = "#000",
	}

	if state.match_state < MatchState_Playing then
		local ready_node = false
		if state.real_team then
			if state.ready or state.match_state == MatchState_Countdown then
				ready_node = cd.node( Override( top_style, { color = "#5f6" } ), "READY" )
			else
				local str = string.format( "Press [%s] to ready up", cd.getBind( "toggleready" ) )
				ready_node = cd.node( Override( top_style, { color = cd.attentionGettingColor() } ), str )
			end
		end

		cd.render( cd.node( {
			float = "top-center top-center",
			y_offset = "1.5vh",
			width = "fit",
			height = "fit",
			flow = "vertical",
			gap = "1vh",
		}, {
			cd.node( top_style, "WARMUP" ),
			ready_node,
		} ) )

		return
	end

	local top_big_style = Override( top_style, { font_size = "3vh" } )

	local match_point_text = false
	if state.round_type == RoundType_MatchPoint then
		match_point_text = "MATCH POINT"
	elseif state.round_type == RoundType_Overtime then
		match_point_text = "OVERTIME"
	elseif state.round_type == RoundType_OvertimeMatchPoint then
		match_point_text = "OVERTIME MATCH POINT"
	end

	local clock = DrawClock( state, top_big_style )
	local clock_and_scores = { clock }
	if state.gametype == Gametype_Bomb then
		clock_and_scores = {
			DrawPlayerIcons( Team_One, state.aliveAlpha, state.totalAlpha ),
			DrawScore( Team_One, state.scoreAlpha, top_big_style ),
			clock,
			DrawScore( Team_Two, state.scoreBeta, top_big_style ),
			DrawPlayerIcons( Team_Two, state.aliveBeta, state.totalBeta ),
		}
	end

	cd.render( cd.node( {
		alignment = "top-center",
		float = "top-center top-center",
		y_offset = "1.5vh",
		width = "fit",
		height = "fit",
		flow = "vertical",
		gap = "1vh",
	}, {
		cd.node( { flow = "horizontal" }, clock_and_scores ),
		match_point_text and cd.node( top_style, match_point_text ),
	} ) )
end

local function DrawPerk( state )
	return cd.node( {
		background = "#fff",
		height = "100%",
		width = "fit",
		border = 8,
		border_color = dark_grey,
		padding = 8,
	}, {
		-- TODO: clay can solve the width here but it doesn't bubble up properly yet
		cd.node( { color = "#000", height = "100%", width = "4.5vh" }, cd.getPerkIcon( state.perk ) ),
	} )
end

local function Lerp( a, t, b )
	return a * ( 1 - t ) + b * t
end

local function AmmoColor( frac )
	return RGBALinear( Lerp( red.r, frac, yellow.r ), Lerp( red.g, frac, yellow.g ), Lerp( red.b, frac, yellow.b ), 1 )
end

local function DrawWeaponIconForeground( x, y, w, h, icon_and_ammo_frac )
	local frac = icon_and_ammo_frac[ 2 ]
	local overlay_height = h * frac
	cd.box( x, y + h - overlay_height, w, overlay_height, AmmoColor( frac ) )
	cd.boxuv( x, y + h - overlay_height, w, overlay_height,
		0, 1 - frac, 1, 1,
		dark_grey, icon_and_ammo_frac[ 1 ] )
end

local function DrawWeaponOrGadget( width, ammo_font_size, icon, ammo, ammo_frac, selected, name, hotkey )
	local container = {
		background = dark_grey,
		flow = "vertical",
		width = width,
		height = "fit",
		padding = 8,
		gap = 8,
		border = 8,
		border_color = dark_grey,
	}

	local icon_container = {
		width = "grow",
	}

	local icon_background = {
		color = "#666",
		width = "100%",
	}

	local icon_foreground = {
		float = "top-left top-left",
		width = "grow",
		height = "grow",
	}

	local name_style = {
		alignment = "middle-center",
		background = dark_grey,
		height = "1.1vh",
		width = "100%",
		font = "bold-italic",
		fit = "center",
		padding = "0.15vh",
	}

	local hotkey_style = {
		float = "bottom-center top-center",
		y_offset = "-0.75vh",
		font = "italic",
		font_size = "1.25vh",
		text_border = "#000",
	}

	local ammo_color = AmmoColor( ammo_frac )

	local ammo_style = {
		float = "top-left top-left",
		x_offset = "0.5vh",
		y_offset = "0.5vh",
		width = "fit",
		height = "fit",
		text_border = "#000",
		font = "bold-italic",
		font_size = ammo_font_size,
		color = ammo_color,
	}

	return cd.node( container, {
		cd.node( icon_container, {
			cd.node( icon_background, icon ),
			cd.node( icon_foreground, DrawWeaponIconForeground, { icon, ammo_frac } ),
			ammo and cd.node( ammo_style, string.format( "%d", ammo ) ) or false,
		} ),
		name and cd.node( Override( name_style, { color = selected and "#fff" or "#999" } ), name ) or false,
		hotkey and cd.node( hotkey_style, hotkey ) or false,
	} )
end

local function DrawGadget( state, show_hotkeys )
	local ammo_frac = state.gadget_ammo / cd.getGadgetMaxAmmo( state.gadget )
	return DrawWeaponOrGadget( "100%", "1vh", cd.getGadgetIcon( state.gadget ), state.gadget_ammo, ammo_frac, false, nil, cd.getBind( "+gadget" ) )
end

local function DrawStaminaBarBackground( x, y, w, h, frac )
	local texture_size = 8
	local color = cd.allyColor()
	color.a *= frac
	cd.box( x, y, w, h, color )
	cd.boxuv( x, y, w, h, 0, 0, w / texture_size, h / texture_size, RGBALinear( 0, 0, 0, 0.5 ), assets.diagonal_pattern )
end

local function DrawStaminaBar( state )
	local steps = 1
	if state.perk == Perk_Hooligan then
		steps = 2
	end

	local nodes = { }
	for i = 1, steps do
		local w = state.stamina - ( i - 1 ) * ( 1 / steps )
		w = math.max( 0, math.min( 1 / steps, w ) )
		table.insert( nodes, cd.node( { height = "100%", width = string.format( "%f%%", w * 100 ) }, DrawStaminaBarBackground, w * steps ) )
	end

	return nodes
end

local function HealthColor( frac )
	return RGBALinear( sRGBToLinear( 1 - frac ), sRGBToLinear( frac ), sRGBToLinear( frac * 0.3 ), 1 )
end

local function DrawHealthCross( x, y, w, h, color )
	local thickness = w / 4
	cd.box( x, y + h/2 - thickness/2, w, thickness, color )
	cd.box( x + w/2 - thickness/2, y, thickness, h, color )
end

local function DrawHealthBarHotkey( state )
	local style = {
		width = "grow",
		height = "100%",
		font = "bold",
		fit = "center",
	}

	if state.can_change_loadout then
		style.color = "#fff"
		return cd.node( style, string.format( "Press %s to change loadout", cd.getBind( "loadoutmenu" ) ) )
	elseif state.can_plant then
		style.color = cd.plantableColor()
		return cd.node( style, string.format( "Press %s to plant", cd.getBind( "+plant" ) ) )
	elseif state.is_carrier then
		style.color = "#fff"
		return cd.node( style, string.format( "Press %s to drop the bomb", cd.getBind( "drop" ) ) )
	end

	return false
end

local function AmmoFraction( weapon )
	if weapon.max_ammo == 0 then
		return 1
	end

	local frac = weapon.ammo / weapon.max_ammo

	-- if state.weapon_state == WeaponState_Reloading or state.weapon_state == WeaponState_StagedReloading then
	-- 	local reload_frac = state.weapon_state_time / cd.getWeaponReloadTime( weapon.weapon )
	-- 	return state.weapon_state == WeaponState_StagedReloading and frac + reload_frac / max_ammo or reload_frac
	-- end

	return frac
end

local function DrawWeapon( i, weapon, selected_weapon, show_hotkeys )
	local hotkey = show_hotkeys and cd.getBind( string.format( "weapon %d", i ) )
	return DrawWeaponOrGadget( "9.1vh", "1.25vh", cd.getWeaponIcon( weapon.weapon ),
		weapon.max_ammo > 0 and weapon.ammo or nil,
		AmmoFraction( weapon ), weapon.weapon == selected_weapon,
		weapon.name:upper(), hotkey )
end

local function DrawBottomLeft( state )
	if state.health <= 0 or state.ghost or state.zooming then
		return
	end

	local show_hotkeys = ShowHotkeys( state )

	local perk_and_gadget = {
		DrawPerk( state ),
		DrawGadget( state, show_hotkeys ),
	}

	local health_color = HealthColor( state.health / state.max_health )
	local health_percent = string.format( "%f%%", 100 * ( state.health / state.max_health ) )

	local health_node = cd.node( {
		flow = "vertical",
		width = "40vh",
		height = "100%",
		background = dark_grey,
		gap = 8,
		border = 8,
		padding = 8,
		border_color = dark_grey,
	}, {
		cd.node( { height = "30%", width = "100%", gap = 8 }, DrawStaminaBar( state ) ),
		cd.node( { height = "35%", width = "100%", background = RGBALinear( 0.04, 0.04, 0.04, 1 ) }, {
			cd.node( { height = "100%", width = health_percent, background = health_color } ),
		} ),
		cd.node( { height = "35%", width = "grow", gap = "1vh", padding = "0.5vh" }, {
			cd.node( { height = "100%", width = 39 }, DrawHealthCross, health_color ),
			cd.node( { color = health_color, height = "100%", width = "5vh", font = "bold", fit = "left" }, string.format( "%d", state.health ) ),
			DrawHealthBarHotkey( state ),
		} ),
	} )

	local health_and_weapons = { }
	table.insert( health_and_weapons, health_node )
	for i, weapon in ipairs( state.weapons ) do
		table.insert( health_and_weapons, DrawWeapon( i, weapon, state.weapon, show_hotkeys ) )
	end

	if state.is_carrier then
		table.insert( health_and_weapons, cd.node( {
			background = state.can_plant and cd.plantableColor() or "#666",
			border = 8,
			border_color = dark_grey,
			padding = 8,
			width = "9.1vh",
			height = "fit",
		}, { cd.node( { width = "100%", color = dark_grey }, assets.bomb ) } ) )
	end

	cd.render(
		cd.node( {
			float = "bottom-left bottom-left",
			x_offset = "1.5vh",
			y_offset = "-1.5vh",
			width = "fit",
			height = "fit",
			gap = "0.5vh",
			flow = "vertical",
		}, {
			cd.node( { width = "fit", height = "5.5vh", gap = 8 }, perk_and_gadget ),
			cd.node( { width = "fit", height = "fit", gap = "0.5vh" }, health_and_weapons ),
		} )
	)
end

return function( state )
	local offset = state.viewport_width * 0.01
	local padding = math.floor( offset * 0.3 )

	if state.match_state < MatchState_PostMatch then
		cd.drawBombIndicators( state.viewport_height / 36, state.viewport_height / 80, state.viewport_height / 70 ) -- site name size, site message size (ATTACK/DEFEND/...)

		DrawTop( state )
		DrawBottomLeft( state )
		DrawDevInfo( state )

		DrawBombProgress( state )

		cd.drawCrosshair()
		cd.drawDamageNumbers( state.viewport_height / 40, state.viewport_height / 70 ) -- obituary msg size, dmg numbers size
		cd.drawPointed( state.viewport_height / 110, "#fff" )

		DrawLagging( state )

		cd.drawObituaries( state.viewport_width - 10, 2, state.viewport_width / 10, state.viewport_width / 10,
						   state.viewport_height / 20, state.viewport_width / 95 )
	end

	DrawScoreboard( state, offset, padding )

	DrawCallvote( state )
end
