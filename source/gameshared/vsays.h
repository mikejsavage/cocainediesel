#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

struct Vsay {
	Span< const char > description;
	Span< const char > short_name;
	StringHash sfx;

	constexpr Vsay( Span< const char > d, Span< const char > n )
		: description( d ),
		short_name( n ),
		sfx( Hash64_CT( n.ptr, n.n, Hash64_CT( "sounds/vsay/" ) ) ) { }
};

constexpr Vsay vsays[] = {
	Vsay( "Acne pack", "acne" ),
	Vsay( "Fart pack", "fart" ),
	Vsay( "Guyman pack", "guyman" ),
	Vsay( "Dodonga pack", "dodonga" ),
	Vsay( "Helena pack", "helena" ),
	Vsay( "Larp pack", "larp" ),
	Vsay( "Fam pack", "fam" ),
	Vsay( "Mike pack", "mike" ),
	Vsay( "User pack", "user" ),
	Vsay( "Valley pack", "valley" ),
	Vsay( "Zombie pack", "zombie" ),
	Vsay( "Good game", "goodgame" ),
	Vsay( "Boomstick", "boomstick" ),
};
