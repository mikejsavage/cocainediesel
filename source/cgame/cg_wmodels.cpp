#include "cgame/cg_local.h"
#include "client/assets.h"
#include "client/renderer/renderer.h"

static WeaponModelMetadata weapon_model_metadata[ Weapon_Count ];

static bool ParseWeaponModelConfig( WeaponModelMetadata * metadata, const char * filename ) {
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

	for( int i = 0; i < 3; i++ ) {
		metadata->handpositionAngles[ i ] = ParseFloat( &contents, 0.0f, Parse_StopOnNewLine );
	}

	return true;
}

static WeaponModelMetadata LoadWeaponModel( WeaponType weapon ) {
	TempAllocator temp = cls.frame_arena.temp();

	WeaponModelMetadata metadata;

	const char * name = GS_GetWeaponDef( weapon )->short_name;

	metadata.model = FindModel( temp( "weapons/{}/model", name ) );

	ParseWeaponModelConfig( &metadata, temp( "weapons/{}/model.cfg", name ) );

	metadata.fire_sound = StringHash( temp( "weapons/{}/fire", name ) );
	metadata.up_sound = StringHash( temp( "weapons/{}/up", name ) );
	metadata.zoom_in_sound = StringHash( temp( "weapons/{}/zoom_in", name ) );
	metadata.zoom_out_sound = StringHash( temp( "weapons/{}/zoom_out", name ) );

	return metadata;
}

void InitWeaponModels() {
	weapon_model_metadata[ Weapon_None ] = { };

	for( WeaponType i = Weapon_None + 1; i < Weapon_Count; i++ ) {
		weapon_model_metadata[ i ] = LoadWeaponModel( i );
	}
}

const WeaponModelMetadata * GetWeaponModelMetadata( WeaponType weapon ) {
	assert( weapon < Weapon_Count );
	return &weapon_model_metadata[ weapon ];
}
