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

static Button button_attack;
static Button button_gadget;
static Button button_reload;

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

static void IN_AttackDown() { KeyDown( &button_attack ); }
static void IN_AttackUp() { KeyUp( &button_attack ); }
static void IN_GadgetDown() { KeyDown( &button_gadget ); }
static void IN_GadgetUp() { KeyUp( &button_gadget ); }
static void IN_ReloadDown() { KeyDown( &button_reload ); }
static void IN_ReloadUp() { KeyUp( &button_reload ); }

u8 CG_GetButtonBits() {
	u8 buttons = 0;

	if( button_attack.down ) {
		buttons |= BUTTON_ATTACK;
	}

	if( button_gadget.down ) {
		buttons |= BUTTON_GADGET;
	}

	if( button_special.down ) {
		buttons |= BUTTON_SPECIAL;
	}

	if( button_reload.down ) {
		buttons |= BUTTON_RELOAD;
	}

	return buttons;
}

u8 CG_GetButtonDownEdges() {
	u8 edges = 0;

	if( button_attack.down && button_attack.edge ) {
		edges |= BUTTON_ATTACK;
	}

	if( button_gadget.down && button_gadget.edge ) {
		edges |= BUTTON_GADGET;
	}

	if( button_special.down && button_special.edge ) {
		edges |= BUTTON_SPECIAL;
	}

	if( button_reload.down && button_reload.edge ) {
		edges |= BUTTON_RELOAD;
	}

	button_attack.edge = false;
	button_gadget.edge = false;
	button_special.edge = false;
	button_reload.edge = false;

	return edges;
}

Vec3 CG_GetMovement() {
	return Vec3(
		( button_right.down ? 1.0f : 0.0f ) - ( button_left.down ? 1.0f : 0.0f ),
		( button_forward.down ? 1.0f : 0.0f ) - ( button_back.down ? 1.0f : 0.0f ),
		( button_jump.down ? 1.0f : 0.0f ) - ( button_crouch.down ? 1.0f : 0.0f )
	);
}

bool CG_GetBoundKeysString( const char *cmd, char *keys, size_t keysSize ) {
	int keyCodes[ 2 ];
	int numKeys = CG_GetBoundKeycodes( cmd, keyCodes );

	if( numKeys == 0 ) {
		Q_strncpyz( keys, "UNBOUND", keysSize );
	}
	else if( numKeys == 1 ) {
		ggformat( keys, keysSize, "{}", Key_KeynumToString( keyCodes[ 0 ] ) );
	}
	else {
		ggformat( keys, keysSize, "{} or {}", Key_KeynumToString( keyCodes[ 0 ] ), Key_KeynumToString( keyCodes[ 1 ] ) );
	}

	return numKeys > 0;
}

int CG_GetBoundKeycodes( const char *cmd, int keys[ 2 ] ) {
	int numKeys = 0;

	for( int key = 0; key < 256; key++ ) {
		const char * bind = Key_GetBindingBuf( key );
		if( bind == NULL || Q_stricmp( bind, cmd ) != 0 ) {
			continue;
		}

		keys[ numKeys ] = key;
		numKeys++;

		if( numKeys == 2 ) {
			break;
		}
	}

	return numKeys;
}

/*
 * mouse
 */

static Cvar *m_accelStyle;
static Cvar *m_accelOffset;
static Cvar *m_accelPow;
static Cvar *m_sensCap;

static Vec2 mouse_movement;

static float CG_GetSensitivityScale() {
	if( !cgs.demoPlaying && cg.predictedPlayerState.zoom_time > 0 ) {
		return CG_CalcViewFov() / FOV;
	}

	return 1.0f;
}

// TODO: these belong somewhere else
static Vec2 SignedOne( Vec2 v ) {
	return Vec2( SignedOne( v.x ), SignedOne( v.y ) );
}

static Vec2 Abs( Vec2 v ) {
	return Vec2( Abs( v.x ), Abs( v.y ) );
}

static Vec2 Pow( Vec2 v, float e ) {
	return Vec2( powf( v.x, e ), powf( v.y, e ) );
}

void CG_MouseMove( int frameTime, Vec2 m ) {
	float sens = Cvar_Float( "sensitivity" );

	if( Cvar_Float( "m_accel" ) != 0.0f && frameTime != 0 ) {
		// QuakeLive-style mouse acceleration, ported from ioquake3
		// original patch by Gabriel Schnoering and TTimo
		if( m_accelStyle->integer == 1 ) {
			Vec2 base = Abs( m ) / float( frameTime );
			Vec2 power = Pow( base / m_accelOffset->number, Cvar_Float( "m_accel" ) );
			m += SignedOne( m ) * power * m_accelOffset->number;
		} else if( m_accelStyle->integer == 2 ) {
			// ch : similar to normal acceleration with offset and variable pow mechanisms

			// sanitize values
			float accelPow = Max2( m_accelPow->number, 1.0f );
			float accelOffset = Max2( m_accelOffset->number, 0.0f );

			float rate = Max2( Length( m ) / float( frameTime ) - accelOffset, 0.0f );
			sens += powf( rate * Cvar_Float( "m_accel" ), accelPow - 1.0f );

			if( m_sensCap->number > 0 ) {
				sens = Min2( sens, m_sensCap->number );
			}
		} else {
			float rate = Length( m ) / float( frameTime );
			sens += rate * Cvar_Float( "m_accel" );
		}
	}

	sens *= CG_GetSensitivityScale();

	mouse_movement = m * sens;
}

Vec3 CG_GetDeltaViewAngles() {
	// m_pitch/m_yaw used to default to 0.022
	float x = Cvar_Float( "horizontalsensscale" );
	float y = Cvar_Bool( "m_invertY" ) ? -1.0f : 1.0f;
	return Vec3(
		0.022f * y * mouse_movement.y,
		-0.022f * x * mouse_movement.x,
		0.0f
	);
}

void CG_ClearInputState() {
	ClearButton( &button_forward );
	ClearButton( &button_back );
	ClearButton( &button_left );
	ClearButton( &button_right );

	ClearButton( &button_jump );
	ClearButton( &button_special );
	ClearButton( &button_crouch );

	ClearButton( &button_attack );
	ClearButton( &button_gadget );
	ClearButton( &button_reload );

	mouse_movement = Vec2( 0.0f );
}

void CG_InitInput() {
	CG_ClearInputState();

	AddCommand( "+forward", IN_ForwardDown );
	AddCommand( "-forward", IN_ForwardUp );
	AddCommand( "+back", IN_BackDown );
	AddCommand( "-back", IN_BackUp );
	AddCommand( "+left", IN_LeftDown );
	AddCommand( "-left", IN_LeftUp );
	AddCommand( "+right", IN_RightDown );
	AddCommand( "-right", IN_RightUp );

	AddCommand( "+jump", IN_JumpDown );
	AddCommand( "-jump", IN_JumpUp );
	AddCommand( "+special", IN_SpecialDown );
	AddCommand( "-special", IN_SpecialUp );
	AddCommand( "+crouch", IN_CrouchDown );
	AddCommand( "-crouch", IN_CrouchUp );

	AddCommand( "+attack", IN_AttackDown );
	AddCommand( "-attack", IN_AttackUp );
	AddCommand( "+gadget", IN_GadgetDown );
	AddCommand( "-gadget", IN_GadgetUp );
	AddCommand( "+reload", IN_ReloadDown );
	AddCommand( "-reload", IN_ReloadUp );

	m_accelStyle = NewCvar( "m_accelStyle", "0", CvarFlag_Archive );
	m_accelOffset = NewCvar( "m_accelOffset", "0", CvarFlag_Archive );
	m_accelPow = NewCvar( "m_accelPow", "2", CvarFlag_Archive );
	m_sensCap = NewCvar( "m_sensCap", "0", CvarFlag_Archive );
}

void CG_ShutdownInput() {
	RemoveCommand( "+forward" );
	RemoveCommand( "-forward" );
	RemoveCommand( "+back" );
	RemoveCommand( "-back" );
	RemoveCommand( "+left" );
	RemoveCommand( "-left" );
	RemoveCommand( "+right" );
	RemoveCommand( "-right" );

	RemoveCommand( "+jump" );
	RemoveCommand( "-jump" );
	RemoveCommand( "+special" );
	RemoveCommand( "-special" );
	RemoveCommand( "+crouch" );
	RemoveCommand( "-crouch" );

	RemoveCommand( "+attack" );
	RemoveCommand( "-attack" );
	RemoveCommand( "+gadget" );
	RemoveCommand( "-gadget" );
	RemoveCommand( "+reload" );
	RemoveCommand( "-reload" );
}
