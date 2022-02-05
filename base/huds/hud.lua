local function DrawFPS( state )
	if state.show_fps then
		local options = {
			color = "#fffb",
			border = "#000b",
			font = "bold",
			font_size = state.viewport_height / 50,
			alignment = "right top",
		}
		cd.text( options, state.viewport_width - 4, 4, state.fps )
	end
end

return function( state )
	DrawFPS( state )
end
