#pragma once

#include "qcommon/hash.h"
#include "gameshared/q_collision.h"

struct EditorMaterial {
	StringHash name;
	bool visible;
	SolidBits solidity;
};

const EditorMaterial * FindEditorMaterial( StringHash name );
