#pragma once

#include "qcommon/types.h"

void InitVisualEffects();
void HotloadVisualEffects();

void DoVisualEffect( const char * name, Vec3 origin, Vec3 normal = Vec3( 0.0f, 0.0f, 1.0f ), float count = 1.0f, Vec4 color = Vec4( 1.0f ), float decal_lifetime_scale = 1.0f );
void DoVisualEffect( StringHash name, Vec3 origin, Vec3 normal = Vec3( 0.0f, 0.0f, 1.0f ), float count = 1.0f, Vec4 color = Vec4( 1.0f ), float decal_lifetime_scale = 1.0f );

void DrawParticles();
void ClearParticles();
