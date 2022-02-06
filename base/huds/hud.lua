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
				cd.text( options, posX, state.viewport_height * 0.04, "Press " .. cd.getBind( "toggleready" ) .. " to ready up" )
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
	if state.chasing == NOTSET then
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

return function( state )
	if state.matchState < MatchState_PostMatch then
		cd.drawBombIndicators( state.viewport_height / 26, state.viewport_height / 60 ) -- site name size, site message size (ATTACK/DEFEND/...)

		DrawTopInfo( state )

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


	cd.drawCallvote( state.viewport_width * 0.05, state.viewport_width * 0.15,
					 state.viewport_width * 0.3, state.viewport_width * 0.2,
					 state.viewport_width / 40, "left middle" )
end
