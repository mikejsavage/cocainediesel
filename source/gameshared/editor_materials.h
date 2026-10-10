#pragma once

#include "qcommon/hash.h"
#include "gameshared/q_collision.h"

struct EditorMaterial {
	StringHash name;
	bool visible_in_maps;
	SolidBits solidity;
};

const EditorMaterial * FindEditorMaterial( StringHash name );
