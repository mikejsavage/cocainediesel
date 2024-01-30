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

static Button button_ability1;
static Button button_ability2;
static Button button_plant;

static Button button_attack1;
static Button button_attack2;
static Button button_gadget;
static Button button_reload;

static void ClearButton( Button * b ) {
	b->keys[ 0 ] = 0;
	b->keys[ 1 ] = 0;
	b->edge = b->down;
	b->down = false;
}

static void KeyDown( Button * b, const Tokenized & args ) {
	int k = args.tokens.n < 2 ? -1 : SpanToInt( args.tokens[ 1 ], -1 );
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

static void KeyUp( Button * b, const Tokenized & args ) {
	if( args.tokens.n < 2 ) {
		b->keys[ 0 ] = 0;
		b->keys[ 1 ] = 0;
		b->edge = b->down;
		b->down = false;
		return;
	}

	int k = SpanToInt( args.tokens[ 1 ], -1 );
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

static void IN_ForwardDown( const Tokenized & args ) { KeyDown( &button_forward, args ); }
static void IN_ForwardUp( const Tokenized & args ) { KeyUp( &button_forward, args ); }
static void IN_BackDown( const Tokenized & args ) { KeyDown( &button_back, args ); }
static void IN_BackUp( const Tokenized & args ) { KeyUp( &button_back, args ); }
static void IN_LeftDown( const Tokenized & args ) { KeyDown( &button_left, args ); }
static void IN_LeftUp( const Tokenized & args ) { KeyUp( &button_left, args ); }
static void IN_RightDown( const Tokenized & args ) { KeyDown( &button_right, args ); }
static void IN_RightUp( const Tokenized & args ) { KeyUp( &button_right, args ); }

static void IN_Ability1Down( const Tokenized & args ) { KeyDown( &button_ability1, args ); }
static void IN_Ability1Up( const Tokenized & args ) { KeyUp( &button_ability1, args ); }
static void IN_Ability2Down( const Tokenized & args ) { KeyDown( &button_ability2, args ); }
static void IN_Ability2Up( const Tokenized & args ) { KeyUp( &button_ability2, args ); }
static void IN_PlantDown( const Tokenized & args ) { KeyDown( &button_plant, args ); }
static void IN_PlantUp( const Tokenized & args ) { KeyUp( &button_plant, args ); }

static void IN_Attack1Down( const Tokenized & args ) { KeyDown( &button_attack1, args ); }
static void IN_Attack1Up( const Tokenized & args ) { KeyUp( &button_attack1, args ); }
static void IN_Attack2Down( const Tokenized & args ) { KeyDown( &button_attack2, args ); }
static void IN_Attack2Up( const Tokenized & args ) { KeyUp( &button_attack2, args ); }
static void IN_GadgetDown( const Tokenized & args ) { KeyDown( &button_gadget, args ); }
static void IN_GadgetUp( const Tokenized & args ) { KeyUp( &button_gadget, args ); }
static void IN_ReloadDown( const Tokenized & args ) { KeyDown( &button_reload, args ); }
static void IN_ReloadUp( const Tokenized & args ) { KeyUp( &button_reload, args ); }

UserCommandButton CG_GetButtonBits() {
	UserCommandButton buttons = { };

	if( button_attack1.down ) {
		buttons |= Button_Attack1;
	}

	if( button_attack2.down ) {
		buttons |= Button_Attack2;
	}

	if( button_gadget.down ) {
		buttons |= Button_Gadget;
	}

	if( button_ability1.down ) {
		buttons |= Button_Ability1;
	}

	if( button_ability2.down ) {
		buttons |= Button_Ability2;
	}

	if( button_reload.down ) {
		buttons |= Button_Reload;
	}

	if( button_plant.down ) {
		buttons |= Button_Plant;
	}

	return buttons;
}

UserCommandButton CG_GetButtonDownEdges() {
	UserCommandButton edges = { };

	if( button_attack1.down && button_attack1.edge ) {
		edges |= Button_Attack1;
	}

	if( button_attack2.down && button_attack2.edge ) {
		edges |= Button_Attack2;
	}

	if( button_gadget.down && button_gadget.edge ) {
		edges |= Button_Gadget;
	}

	if( button_ability1.down && button_ability1.edge ) {
		edges |= Button_Ability1;
	}

	if( button_ability2.down && button_ability2.edge ) {
		edges |= Button_Ability2;
	}

	if( button_reload.down && button_reload.edge ) {
		edges |= Button_Reload;
	}

	if( button_plant.down && button_plant.edge ) {
		edges |= Button_Plant;
	}

	button_attack1.edge = false;
	button_attack2.edge = false;
	button_gadget.edge = false;
	button_ability1.edge = false;
	button_ability2.edge = false;
	button_reload.edge = false;
	button_plant.edge = false;

	return edges;
}

Vec2 CG_GetMovement() {
	Vec2 keyboard = Vec2(
		( button_right.down ? 1.0f : 0.0f ) - ( button_left.down ? 1.0f : 0.0f ),
		( button_forward.down ? 1.0f : 0.0f ) - ( button_back.down ? 1.0f : 0.0f )
	);

	Vec2 joystick = GetJoystickMovement();

	return Vec2(
		Clamp( -1.0f, keyboard.x + joystick.x, 1.0f ),
		Clamp( -1.0f, keyboard.y + joystick.y, 1.0f )
	);
}

bool CG_GetBoundKeysString( const char * cmd, char * keys, size_t keysSize ) {
	int keyCodes[ 2 ];
	int numKeys = CG_GetBoundKeycodes( cmd, keyCodes );

	if( numKeys == 0 ) {
		SafeStrCpy( keys, "UNBOUND", keysSize );
	}
	else if( numKeys == 1 ) {
		ggformat( keys, keysSize, "{}", Key_KeynumToString( keyCodes[ 0 ] ) );
	}
	else {
		ggformat( keys, keysSize, "{} or {}", Key_KeynumToString( keyCodes[ 0 ] ), Key_KeynumToString( keyCodes[ 1 ] ) );
	}

	return numKeys > 0;
}

int CG_GetBoundKeycodes( const char * cmd, int keys[ 2 ] ) {
	int numKeys = 0;

	for( int key = 0; key < 256; key++ ) {
		Span< const char > bind = Key_GetBindingBuf( key );
		if( bind == "" || !StrCaseEqual( bind, cmd ) ) {
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

static Vec2 mouse_movement;

static float CG_GetSensitivityScale() {
	if( !cgs.demoPlaying && cg.predictedPlayerState.zoom_time > 0 ) {
		return CG_CalcViewFov() / FOV;
	}

	return 1.0f;
}

void CG_MouseMove( Vec2 m ) {
	mouse_movement = m * Cvar_Float( "sensitivity" ) * CG_GetSensitivityScale();
}

EulerDegrees2 CG_GetDeltaViewAngles() {
	// m_pitch/m_yaw used to default to 0.022
	float x = Cvar_Float( "horizontalsensscale" );
	float y = Cvar_Bool( "m_invertY" ) ? -1.0f : 1.0f;
	return EulerDegrees2(
		0.022f * y * mouse_movement.y,
		-0.022f * x * mouse_movement.x
	);
}

void CG_ClearInputState() {
	ClearButton( &button_forward );
	ClearButton( &button_back );
	ClearButton( &button_left );
	ClearButton( &button_right );

	ClearButton( &button_ability1 );
	ClearButton( &button_ability2 );
	ClearButton( &button_plant );

	ClearButton( &button_attack1 );
	ClearButton( &button_attack2 );
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

	AddCommand( "+ability1", IN_Ability1Down );
	AddCommand( "-ability1", IN_Ability1Up );
	AddCommand( "+ability2", IN_Ability2Down );
	AddCommand( "-ability2", IN_Ability2Up );
	AddCommand( "+plant", IN_PlantDown );
	AddCommand( "-plant", IN_PlantUp );

	AddCommand( "+attack1", IN_Attack1Down );
	AddCommand( "-attack1", IN_Attack1Up );
	AddCommand( "+attack2", IN_Attack2Down );
	AddCommand( "-attack2", IN_Attack2Up );
	AddCommand( "+gadget", IN_GadgetDown );
	AddCommand( "-gadget", IN_GadgetUp );
	AddCommand( "+reload", IN_ReloadDown );
	AddCommand( "-reload", IN_ReloadUp );
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

	RemoveCommand( "+ability1" );
	RemoveCommand( "-ability1" );
	RemoveCommand( "+ability2" );
	RemoveCommand( "-ability2" );
	RemoveCommand( "+plant" );
	RemoveCommand( "-plant" );

	RemoveCommand( "+attack1" );
	RemoveCommand( "-attack1" );
	RemoveCommand( "+attack2" );
	RemoveCommand( "-attack2" );
	RemoveCommand( "+gadget" );
	RemoveCommand( "-gadget" );
	RemoveCommand( "+reload" );
	RemoveCommand( "-reload" );
}
