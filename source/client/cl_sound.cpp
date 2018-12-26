/*
Copyright (C) 2002-2003 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "client.h"

void CL_SoundModule_Init() {
	if( !S_Init() ) {
		Com_Printf( S_COLOR_RED "Couldn't initialise audio engine\n" );
	}

	// check memory integrity
	Mem_DebugCheckSentinelsGlobal();
}

void CL_SoundModule_Shutdown() {
	S_Shutdown();
}

void CL_SoundModule_StopAllSounds( bool stopMusic ) {
	S_StopAllSounds( stopMusic );
}

void CL_SoundModule_Update( const vec3_t origin, const vec3_t velocity, const mat3_t axis ) {
	S_Update( origin, velocity, axis );
}

void CL_SoundModule_UpdateEntity( int entNum, vec3_t origin, vec3_t velocity ) {
	S_UpdateEntity( entNum, origin, velocity );
}

void CL_SoundModule_SetWindowFocus( bool focused ) {
	S_SetWindowFocus( focused );
}

void CL_SoundModule_StartFixedSound( StringHash name, const vec3_t origin, int channel, float volume, float attenuation ) {
	S_StartFixedSound( name, origin, channel, volume, attenuation );
}

void CL_SoundModule_StartEntitySound( StringHash name, int entnum, int channel, float volume, float attenuation ) {
	S_StartEntitySound( name, entnum, channel, volume, attenuation );
}

void CL_SoundModule_StartGlobalSound( StringHash name, int channel, float volume ) {
	S_StartGlobalSound( name, channel, volume );
}

void CL_SoundModule_StartLocalSound( StringHash name, int channel, float volume ) {
	S_StartLocalSound( name, channel, volume );
}

void CL_SoundModule_ImmediateSound( StringHash name, int entnum, float volume, float attenuation ) {
	S_ImmediateSound( name, entnum, volume, attenuation );
}

void CL_SoundModule_StartMenuMusic() {
	S_StartMenuMusic();
}

void CL_SoundModule_StopBackgroundTrack() {
	S_StopBackgroundTrack();
}

void CL_SoundModule_BeginAviDemo() {
	S_BeginAviDemo();
}

void CL_SoundModule_StopAviDemo() {
	S_StopAviDemo();
}
