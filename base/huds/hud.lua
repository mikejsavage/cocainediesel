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

	if state.show_speed then
		cd.text( options, state.viewport_width - 4, state.viewport_height - options[ "font_size" ], state.speed )
	end
end


local function DrawObituaries( state )
	local options = {
		font_size = state.viewport_height / 45,
		alignment = "right top",
		width = state.viewport_width / 10,
		height = state.viewport_width / 10,
		internal_align = 3,
		icon_size = state.viewport_width / 40,
	}
	cd.drawObituaries( options, state.viewport_width - 10, 2 ) -- options, X, Y
end


local function DrawBombProgress( state )
	if state.bomb_progress ~= 0 then
		local width = state.viewport_width * 0.3
		local progress = state.bomb_progress/100
		local color = RGBALinear( 1, 1, 1, progress )
		cd.box( (state.viewport_width - width)/2, state.viewport_height * 0.7, width * progress, state.viewport_height / 30, color )
	end
end




return function( state )
	cd.drawBombIndicators( state.viewport_height / 26, state.viewport_height / 60 ) -- site name size, site message size (ATTACK/DEFEND/...)

	DrawDevInfo( state )

	DrawBombProgress( state )

	cd.drawCrosshair()
	cd.drawDamageNumbers( state.viewport_height / 30, state.viewport_height / 50 ) -- obituary msg size, dmg numbers size
	cd.drawPointed( state.viewport_height / 80, "#fff", 1 ) -- font size, color, border

	DrawObituaries( state )
end
