#include "gameshared/editor_materials.h"

constexpr const EditorMaterial editor_materials[] = {
	{ "textures/editor/discard", "discard", false, Solid_World | Solid_PlayerClip | Solid_WeaponClip },
	{ "textures/editor/ladder", "ladder", false, Solid_World | Solid_Ladder | Solid_PlayerClip },
	{ "textures/editor/clip", "clip", false, Solid_World | Solid_PlayerClip | Solid_WeaponClip },
	{ "textures/editor/playerclip", "playerclip", false, Solid_World | Solid_PlayerClip },
	{ "textures/editor/weaponclip", "weaponclip", false, Solid_WeaponClip },
	{ "textures/editor/trigger", "trigger", false, SolidMask_Everything },
	{ "textures/editor/wallbangable", "wallbangable", true, Solid_World | Solid_Wallbangable | Solid_PlayerClip },
	{ "textures/editor/door", "door", true, Solid_World | Solid_WeaponClip },
};

const EditorMaterial * FindEditorMaterial( StringHash name ) {
	for( const EditorMaterial & material : editor_materials ) {
		if( material.radiant_name == name || material.short_name == name ) {
			return &material;
		}
	}

	return NULL;
}
