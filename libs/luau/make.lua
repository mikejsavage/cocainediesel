if OS ~= "linux" then
	prebuilt_lib( "luau", { "Luau.AST", "Luau.Compiler", "Luau.VM" } )
else
	prebuilt_lib( "luau", { "luaucompiler", "luauast", "luauvm" } )
end
