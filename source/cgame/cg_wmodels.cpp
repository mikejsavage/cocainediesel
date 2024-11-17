#include "cgame/cg_local.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"

static WeaponModelMetadata weapon_model_metadata[ Weapon_Count ];
static GadgetModelMetadata gadget_model_metadata[ Gadget_Count ];

static bool ParseWeaponModelConfig( WeaponModelMetadata * metadata, Span< const char > filename ) {
	Span< const char > contents = AssetString( filename );
	if( contents.ptr == NULL )
		return false;

	Span< const char > token = ParseToken( &contents, Parse_StopOnNewLine );
	if( token != "handOffset" ) {
		Com_GGPrint( S_COLOR_YELLOW "Bad weapon model config ({}): {}", filename, token );
		return false;
	}

	for( int i = 0; i < 3; i++ ) {
		metadata->handpositionOrigin[ i ] = ParseFloat( &contents, 0.0f, Parse_StopOnNewLine );
	}

	metadata->handpositionAngles.pitch = ParseFloat( &contents, 0.0f, Parse_StopOnNewLine );
	metadata->handpositionAngles.yaw = ParseFloat( &contents, 0.0f, Parse_StopOnNewLine );
	metadata->handpositionAngles.roll = ParseFloat( &contents, 0.0f, Parse_StopOnNewLine );

	return true;
}

static WeaponModelMetadata BuildWeaponModelMetadata( WeaponType weapon ) {
	TempAllocator temp = cls.frame_arena.temp();

	WeaponModelMetadata metadata;

	Span< const char > name = GS_GetWeaponDef( weapon )->name;

	metadata.model = StringHash( temp( "loadout/{}/weapon", name ) );

	ParseWeaponModelConfig( &metadata, temp.sv( "loadout/{}/model.cfg", name ) );

	metadata.fire_sound = StringHash( temp.sv( "loadout/{}/fire", name ) );
	metadata.reload_sound = StringHash( temp.sv( "loadout/{}/reload", name ) );
	metadata.switch_in_sound = StringHash( temp.sv( "loadout/{}/up", name ) );
	metadata.zoom_in_sound = StringHash( temp.sv( "loadout/{}/zoom_in", name ) );
	metadata.zoom_out_sound = StringHash( temp.sv( "loadout/{}/zoom_out", name ) );

	return metadata;
}

static GadgetModelMetadata BuildGadgetModelMetadata( GadgetType gadget ) {
	TempAllocator temp = cls.frame_arena.temp();
	Span< const char > name = GetGadgetDef( gadget )->name;

	GadgetModelMetadata metadata;

	metadata.model = StringHash( temp.sv( "loadout/{}/projectile", name ) );
	metadata.use_sound = StringHash( temp.sv( "loadout/{}/use", name ) );
	metadata.switch_in_sound = StringHash( temp.sv( "loadout/{}/switch_in", name ) );

	return metadata;
}

void InitWeaponModels() {
	weapon_model_metadata[ Weapon_None ] = { };
	for( WeaponType i = WeaponType( Weapon_None + 1 ); i < Weapon_Count; i++ ) {
		weapon_model_metadata[ i ] = BuildWeaponModelMetadata( i );
	}

	gadget_model_metadata[ Gadget_None ] = { };
	for( u8 i = u8( Gadget_None ) + 1; i < Gadget_Count; i++ ) {
		gadget_model_metadata[ i ] = BuildGadgetModelMetadata( GadgetType( i ) );
	}
}

const WeaponModelMetadata * GetWeaponModelMetadata( WeaponType weapon ) {
	Assert( weapon < Weapon_Count );
	return &weapon_model_metadata[ weapon ];
}

const GadgetModelMetadata * GetGadgetModelMetadata( GadgetType gadget ) {
	Assert( gadget < Gadget_Count );
	return &gadget_model_metadata[ gadget ];
}

const GLTFRenderData * GetEquippedItemRenderData( const SyncEntityState * ent ) {
	StringHash model = ent->gadget != Gadget_None ?
		GetGadgetModelMetadata( ent->gadget )->model :
		GetWeaponModelMetadata( ent->weapon )->model;
	return FindGLTFRenderData( model );
}

const GLTFRenderData * GetEquippedItemRenderData( const SyncPlayerState * ps ) {
	StringHash model = ps->using_gadget ?
		GetGadgetModelMetadata( ps->gadget )->model :
		GetWeaponModelMetadata( ps->weapon )->model;
	return FindGLTFRenderData( model );
}
