#include "gameshared/editor_materials.h"

constexpr const EditorMaterial editor_materials[] = {
	{ "discard", false, Solid_World | Solid_PlayerClip | Solid_WeaponClip },
	{ "ladder", false, Solid_World | Solid_Ladder | Solid_PlayerClip },
	{ "clip", false, Solid_World | Solid_PlayerClip | Solid_WeaponClip },
	{ "playerclip", false, Solid_World | Solid_PlayerClip },
	{ "weaponclip", false, Solid_WeaponClip },
	{ "trigger", false, SolidMask_Everything },
	{ "wallbangable", true, Solid_World | Solid_Wallbangable | Solid_PlayerClip },
	{ "door", true, Solid_World | Solid_WeaponClip },
};

const EditorMaterial * FindEditorMaterial( StringHash name ) {
	for( const EditorMaterial & material : editor_materials ) {
		if( material.name == name ) {
			return &material;
		}
	}

	return NULL;
}
