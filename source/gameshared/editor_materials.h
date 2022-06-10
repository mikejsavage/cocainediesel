#pragma once

#include "qcommon/types.h"
#include "qcommon/hash.h"

enum SolidBits : u8 {
	Solid_NotSolid = 0,
	Solid_Player = ( 1 << 1 ),
	Solid_Weapon = ( 1 << 2 ),
	Solid_Wallbangable = ( 1 << 3 ),
	Solid_Ladder = ( 1 << 4 ),
};

struct EditorMaterial {
	StringHash radiant_name;
	StringHash short_name;
	bool visible;
	SolidBits solidity;
};

constexpr SolidBits Solid_Solid = SolidBits( Solid_Player | Solid_Weapon );

constexpr const EditorMaterial editor_materials[] = {
	{ "editor/discard", "discard", false, Solid_Solid },
	{ "editor/ladder", "ladder", false, SolidBits( Solid_Ladder | Solid_Player ) },
	{ "editor/clip", "clip", false, Solid_Solid },
	{ "editor/playerclip", "playerclip", false, Solid_Player },
	{ "editor/weaponclip", "weaponclip", false, Solid_Weapon },
	{ "editor/trigger", "trigger", false, Solid_Solid },
	{ "editor/wallbangable", "wallbangable", true, SolidBits( Solid_Wallbangable | Solid_Player ) },
	{ "editor/door", "door", true, Solid_Weapon },
};

inline const EditorMaterial * FindEditorMaterial( StringHash name ) {
	for( const EditorMaterial & material : editor_materials ) {
		if( material.radiant_name == name || material.short_name == name ) {
			return &material;
		}
	}

	return NULL;
}
