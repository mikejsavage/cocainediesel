/*
Copyright (C) 2015 Chasseur de bots

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

#include "qcommon/base.h"
#include "cgame/cg_local.h"
#include "client/keys.h"

static int cg_inputFrameTime;

/*
 * keyboard
 */

struct Button {
	int keys[ 2 ];
	bool down;
	bool edge;
};

static Button button_forward;
static Button button_back;
static Button button_left;
static Button button_right;

static Button button_jump;
static Button button_special;
static Button button_crouch;
static Button button_walk;

static Button button_attack;
static Button button_zoom;

static void ClearButton( Button * b ) {
	b->keys[ 0 ] = 0;
	b->keys[ 1 ] = 0;
	b->down = false;
	b->edge = false;
}

static void KeyDown( Button * b ) {
	const char * c = Cmd_Argv( 1 );
	int k = -1;
	if( c[0] ) {
		k = atoi( c );
	}

	if( k == b->keys[ 0 ] || k == b->keys[ 1 ] ) {
		return;
	}

	if( b->keys[ 0 ] == 0 ) {
		b->keys[ 0 ] = k;
	}
	else if( b->keys[ 1 ] == 0 ) {
		b->keys[ 1 ] = k;
	}
	else {
		Com_Printf( "Three keys down for a button!\n" );
		return;
	}

	b->edge = !b->down;
	b->down = true;
}

static void KeyUp( Button * b ) {
	const char * c = Cmd_Argv( 1 );
	if( !c[0] ) {
		b->keys[ 0 ] = 0;
		b->keys[ 1 ] = 0;
		b->down = false;
		b->edge = true;
		return;
	}

	int k = atoi( c );
	if( b->keys[ 0 ] == k ) {
		b->keys[ 0 ] = 0;
	}
	else if( b->keys[ 1 ] == k ) {
		b->keys[ 1 ] = 0;
	}
	else {
		return;
	}

	b->edge = b->down;
	b->down = false;
}

static void IN_ForwardDown() { KeyDown( &button_forward ); }
static void IN_ForwardUp() { KeyUp( &button_forward ); }
static void IN_BackDown() { KeyDown( &button_back ); }
static void IN_BackUp() { KeyUp( &button_back ); }
static void IN_LeftDown() { KeyDown( &button_left ); }
static void IN_LeftUp() { KeyUp( &button_left ); }
static void IN_RightDown() { KeyDown( &button_right ); }
static void IN_RightUp() { KeyUp( &button_right ); }

static void IN_JumpDown() { KeyDown( &button_jump ); }
static void IN_JumpUp() { KeyUp( &button_jump ); }
static void IN_SpecialDown() { KeyDown( &button_special ); }
static void IN_SpecialUp() { KeyUp( &button_special ); }
static void IN_CrouchDown() { KeyDown( &button_crouch ); }
static void IN_CrouchUp() { KeyUp( &button_crouch ); }
static void IN_WalkDown() { KeyDown( &button_walk ); }
static void IN_WalkUp() { KeyUp( &button_walk ); }

static void IN_AttackDown() { KeyDown( &button_attack ); }
static void IN_AttackUp() { KeyUp( &button_attack ); }
static void IN_ZoomDown() { KeyDown( &button_zoom ); }
static void IN_ZoomUp() { KeyUp( &button_zoom ); }

unsigned int CG_GetButtonBits() {
	unsigned int buttons = BUTTON_NONE;

	if( button_attack.down || button_attack.edge ) {
		buttons |= BUTTON_ATTACK;
		button_attack.edge = false;
	}

	if( button_special.down || button_special.edge ) {
		buttons |= BUTTON_SPECIAL;
		button_special.edge = false;
	}

	if( button_zoom.down ) {
		buttons |= BUTTON_ZOOM;
	}

	if( button_walk.down ) {
		buttons |= BUTTON_WALK;
	}

	return buttons;
}

void CG_AddMovement( vec3_t movement ) {
	movement[ 0 ] += ( button_right.down ? 1.0f : 0.0f ) - ( button_left.down ? 1.0f : 0.0f );
	movement[ 1 ] += ( button_forward.down ? 1.0f : 0.0f ) - ( button_back.down ? 1.0f : 0.0f );
	movement[ 2 ] += ( button_jump.down ? 1.0f : 0.0f ) - ( button_crouch.down ? 1.0f : 0.0f );
}

bool CG_GetBoundKeysString( const char *cmd, char *keys, size_t keysSize ) {
	int key;
	const char *bind;
	int numKeys = 0;
	const char *keyNames[2];
	char charKeys[2][2];

	memset( charKeys, 0, sizeof( charKeys ) );

	for( key = 0; key < 256; key++ ) {
		bind = Key_GetBindingBuf( key );
		if( !bind || Q_stricmp( bind, cmd ) ) {
			continue;
		}

		if( key >= 'a' && key <= 'z' ) {
			charKeys[numKeys][0] = key - ( 'a' - 'A' );
			keyNames[numKeys] = charKeys[numKeys];
		} else {
			keyNames[numKeys] = Key_KeynumToString( key );
		}

		if( keyNames[numKeys] != NULL ) {
			numKeys++;
			if( numKeys == 2 ) {
				break;
			}
		}
	}

	if( !numKeys ) {
		keyNames[0] = "UNBOUND";
	}

	if( numKeys == 2 ) {
		snprintf( keys, keysSize, "%s or %s", keyNames[0], keyNames[1] );
	} else {
		Q_strncpyz( keys, keyNames[0], keysSize );
	}

	return numKeys > 0;
}

/*
 * mouse
 */

static cvar_t *sensitivity;
static cvar_t *horizontalSensScale;
static cvar_t *zoomsens;
static cvar_t *m_accel;
static cvar_t *m_accelStyle;
static cvar_t *m_accelOffset;
static cvar_t *m_accelPow;
static cvar_t *m_sensCap;

static Vec2 mouse_movement;

float CG_GetSensitivityScale( float sens, float zoomSens ) {
	if( !cgs.demoPlaying && sens && ( cg.predictedPlayerState.pmove.stats[PM_STAT_ZOOMTIME] > 0 ) ) {
		if( zoomSens ) {
			return zoomSens / sens;
		}

		return cg_zoomfov->value / cg_fov->value;
	}

	return 1.0f;
}

// TODO: these belong somewhere else
static float Sign( float x ) {
	return x < 0 ? -1.0f : 1.0f;
}

static Vec2 Sign( Vec2 v ) {
	return Vec2( Sign( v.x ), Sign( v.y ) );
}

static Vec2 Abs( Vec2 v ) {
	return Vec2( Abs( v.x ), Abs( v.y ) );
}

static Vec2 Pow( Vec2 v, float e ) {
	return Vec2( powf( v.x, e ), powf( v.y, e ) );
}

void CG_MouseMove( int mx, int my ) {
	Vec2 m( mx, my );

	float sens = sensitivity->value;

	if( m_accel->value != 0.0f && cg_inputFrameTime != 0 ) {
		// QuakeLive-style mouse acceleration, ported from ioquake3
		// original patch by Gabriel Schnoering and TTimo
		if( m_accelStyle->integer == 1 ) {
			Vec2 base = Abs( m ) / float( cg_inputFrameTime );
			Vec2 power = Pow( base / m_accelOffset->value, m_accel->value );
			m += Sign( m ) * power * m_accelOffset->value;
		} else if( m_accelStyle->integer == 2 ) {
			// ch : similar to normal acceleration with offset and variable pow mechanisms

			// sanitize values
			float accelPow = Max2( m_accelPow->value, 1.0f );
			float accelOffset = Max2( m_accelOffset->value, 0.0f );

			float rate = Max2( Length( m ) / float( cg_inputFrameTime ) - accelOffset, 0.0f );
			sens += powf( rate * m_accel->value, accelPow - 1.0f );

			if( m_sensCap->value > 0 ) {
				sens = Min2( sens, m_sensCap->value );
			}
		} else {
			float rate = Length( m ) / float( cg_inputFrameTime );
			sens += rate * m_accel->value;
		}
	}

	sens *= CG_GetSensitivityScale( sensitivity->value, zoomsens->value );

	mouse_movement = m * sens;
}

void CG_AddViewAngles( vec3_t viewAngles ) {
	// m_pitch/m_yaw used to default to 0.022
	viewAngles[ YAW ] -= 0.022f * horizontalSensScale->value * mouse_movement.x;
	viewAngles[ PITCH ] += 0.022f * mouse_movement.y;
}

void CG_InputFrame( int frameTime ) {
	cg_inputFrameTime = frameTime;
}

void CG_ClearInputState() {
	cg_inputFrameTime = 0;

	ClearButton( &button_forward );
	ClearButton( &button_back );
	ClearButton( &button_left );
	ClearButton( &button_right );

	ClearButton( &button_jump );
	ClearButton( &button_special );
	ClearButton( &button_crouch );
	ClearButton( &button_walk );

	ClearButton( &button_attack );
	ClearButton( &button_zoom );

	mouse_movement = Vec2( 0 );
}

void CG_InitInput() {
	CG_ClearInputState();

	Cmd_AddCommand( "+forward", IN_ForwardDown );
	Cmd_AddCommand( "-forward", IN_ForwardUp );
	Cmd_AddCommand( "+back", IN_BackDown );
	Cmd_AddCommand( "-back", IN_BackUp );
	Cmd_AddCommand( "+left", IN_LeftDown );
	Cmd_AddCommand( "-left", IN_LeftUp );
	Cmd_AddCommand( "+right", IN_RightDown );
	Cmd_AddCommand( "-right", IN_RightUp );

	Cmd_AddCommand( "+jump", IN_JumpDown );
	Cmd_AddCommand( "-jump", IN_JumpUp );
	Cmd_AddCommand( "+special", IN_SpecialDown );
	Cmd_AddCommand( "-special", IN_SpecialUp );
	Cmd_AddCommand( "+crouch", IN_CrouchDown );
	Cmd_AddCommand( "-crouch", IN_CrouchUp );
	Cmd_AddCommand( "+walk", IN_WalkDown );
	Cmd_AddCommand( "-walk", IN_WalkUp );

	Cmd_AddCommand( "+attack", IN_AttackDown );
	Cmd_AddCommand( "-attack", IN_AttackUp );
	Cmd_AddCommand( "+zoom", IN_ZoomDown );
	Cmd_AddCommand( "-zoom", IN_ZoomUp );

	// legacy command names
	Cmd_AddCommand( "+moveleft", IN_LeftDown );
	Cmd_AddCommand( "-moveleft", IN_LeftUp );
	Cmd_AddCommand( "+moveright", IN_RightDown );
	Cmd_AddCommand( "-moveright", IN_RightUp );
	Cmd_AddCommand( "+moveup", IN_JumpDown );
	Cmd_AddCommand( "-moveup", IN_JumpUp );
	Cmd_AddCommand( "+movedown", IN_CrouchDown );
	Cmd_AddCommand( "-movedown", IN_CrouchUp );
	Cmd_AddCommand( "+speed", IN_WalkDown );
	Cmd_AddCommand( "-speed", IN_WalkUp );

	sensitivity = Cvar_Get( "sensitivity", "3", CVAR_ARCHIVE );
	horizontalSensScale = Cvar_Get( "horizontalsensscale", "1", CVAR_ARCHIVE );
	zoomsens = Cvar_Get( "zoomsens", "0", CVAR_ARCHIVE );
	m_accel = Cvar_Get( "m_accel", "0", CVAR_ARCHIVE );
	m_accelStyle = Cvar_Get( "m_accelStyle", "0", CVAR_ARCHIVE );
	m_accelOffset = Cvar_Get( "m_accelOffset", "0", CVAR_ARCHIVE );
	m_accelPow = Cvar_Get( "m_accelPow", "2", CVAR_ARCHIVE );
	m_sensCap = Cvar_Get( "m_sensCap", "0", CVAR_ARCHIVE );
}

void CG_ShutdownInput() {
	Cmd_RemoveCommand( "+forward" );
	Cmd_RemoveCommand( "-forward" );
	Cmd_RemoveCommand( "+back" );
	Cmd_RemoveCommand( "-back" );
	Cmd_RemoveCommand( "+left" );
	Cmd_RemoveCommand( "-left" );
	Cmd_RemoveCommand( "+right" );
	Cmd_RemoveCommand( "-right" );

	Cmd_RemoveCommand( "+jump" );
	Cmd_RemoveCommand( "-jump" );
	Cmd_RemoveCommand( "+special" );
	Cmd_RemoveCommand( "-special" );
	Cmd_RemoveCommand( "+crouch" );
	Cmd_RemoveCommand( "-crouch" );
	Cmd_RemoveCommand( "+walk" );
	Cmd_RemoveCommand( "-walk" );

	Cmd_RemoveCommand( "+attack" );
	Cmd_RemoveCommand( "-attack" );
	Cmd_RemoveCommand( "+zoom" );
	Cmd_RemoveCommand( "-zoom" );

	// legacy command names
	Cmd_RemoveCommand( "+moveleft" );
	Cmd_RemoveCommand( "-moveleft" );
	Cmd_RemoveCommand( "+moveright" );
	Cmd_RemoveCommand( "-moveright" );
	Cmd_RemoveCommand( "+moveup" );
	Cmd_RemoveCommand( "-moveup" );
	Cmd_RemoveCommand( "+movedown" );
	Cmd_RemoveCommand( "-movedown" );
	Cmd_RemoveCommand( "+speed" );
	Cmd_RemoveCommand( "-speed" );
}
