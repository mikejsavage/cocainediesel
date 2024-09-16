#pragma once

#include "qcommon/hash.h"
#include "gameshared/q_collision.h"

struct EditorMaterial {
	StringHash radiant_name;
	StringHash short_name;
	bool visible_in_maps;
	SolidBits solidity;
};

const EditorMaterial * FindEditorMaterial( StringHash name );
