#include "qcommon/base.h"
#include "gameshared/gs_public.h"

const Item itemdefs[] = {
	{ Item_Bomb },

	{
		Item_FakeBomb,

		"Dummy bomb", "dummybomb",
		RGB8( 255, 100, 255 ),
		"Hello",
		100,
	},
};

STATIC_ASSERT( ARRAY_COUNT( itemdefs ) == Item_Count );
