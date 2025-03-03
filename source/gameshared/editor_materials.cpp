#include "gameshared/editor_materials.h"

constexpr const EditorMaterial editor_materials[] = {
	{ "editor/discard", "discard", false, Solid_World | Solid_PlayerClip | Solid_WeaponClip },
	{ "editor/ladder", "ladder", false, Solid_World | Solid_Ladder | Solid_PlayerClip },
	{ "editor/clip", "clip", false, Solid_World | Solid_PlayerClip | Solid_WeaponClip },
	{ "editor/playerclip", "playerclip", false, Solid_World | Solid_PlayerClip },
	{ "editor/weaponclip", "weaponclip", false, Solid_WeaponClip },
	{ "editor/trigger", "trigger", false, SolidMask_Everything },
	{ "editor/wallbangable", "wallbangable", true, Solid_World | Solid_Wallbangable | Solid_PlayerClip },
	{ "editor/door", "door", true, Solid_World | Solid_WeaponClip },
};

const EditorMaterial * FindEditorMaterial( StringHash name ) {
	for( const EditorMaterial & material : editor_materials ) {
		if( material.radiant_name == name || material.short_name == name ) {
			return &material;
		}
	}

	return NULL;
}
