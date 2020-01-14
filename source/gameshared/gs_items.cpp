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

const Item * GS_FindItemByType( ItemType type ) {
	assert( type >= 0 && type < Item_Count );
	assert( type == itemdefs[ type ].type );
	return &itemdefs[ type ];
}

const Item * GS_FindItemByName( const char * name ) {
	for( size_t i = 0; i < ARRAY_COUNT( itemdefs ); i++ ) {
		if( Q_stricmp( name, itemdefs[ i ].name ) == 0 )
			return &itemdefs[ i ];
		if( Q_stricmp( name, itemdefs[ i ].short_name ) == 0 )
			return &itemdefs[ i ];
	}

	return NULL;
}

bool GS_CanEquip( const SyncPlayerState * player, WeaponType weapon ) {
	return ( player->pmove.stats[ PM_STAT_FEATURES ] & PMFEAT_WEAPONSWITCH ) != 0 && player->weapons[ weapon ].owned;
}
