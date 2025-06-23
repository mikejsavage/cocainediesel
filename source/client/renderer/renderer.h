#pragma once

#include "qcommon/types.h"
#include "qcommon/srgb.h"
#include "client/renderer/api.h"
#include "client/renderer/material.h"
#include "client/renderer/model.h"
#include "client/renderer/gltf.h"
#include "client/renderer/cdmap.h"
#include "client/renderer/shader.h"
#include "cgame/ref.h"

void Draw2DBox( float x, float y, float w, float h, PoolHandle< Material2 > material, Vec4 color = white.vec4 );
void Draw2DBoxUV( float x, float y, float w, float h, Vec2 topleft_uv, Vec2 bottomright_uv, PoolHandle< Material2 > material, Vec4 color = white.vec4 );
