const uint MOD_NONE = 0;
const uint MOD_BASE = 1;
const uint MOD_EXTRA = 2;
const uint MOD_ALWAYS = 4;
const uint MOD_SILLY = MOD_BASE | MOD_EXTRA;
const uint MOD_VERYSILLY = MOD_ALWAYS | MOD_SILLY;

funcdef void modInit();
funcdef void modThink();
funcdef void modStop();

Cvar gt_mod_mode = Cvar("gt_mod_mode", "0", 0);
uint numMods = 0;
uint[] modTypes;
float[] modWeights;
String[] modNames;
modInit@[] modInits;
modThink@[] modThinks;
modStop@[] modStops;
uint[] modWeaponMasks;

int currentMod = -1;

String ModModeToString( uint mod_mode )
{
	switch( mod_mode )
	{
		case MOD_BASE:
			return "base";
		case MOD_SILLY:
			return "silly";
		case MOD_VERYSILLY:
			return "verysilly";
	}
	return "none";
}

uint ModStringToMode( String& mod_string )
{
	if ( mod_string == "base" )
		return MOD_BASE;
	if ( mod_string == "silly" )
		return MOD_SILLY;
	if ( mod_string == "verysilly" )
		return MOD_VERYSILLY;

	return MOD_NONE;
}

bool isValidMode( String& mod_string )
{
	return ( mod_string == "none" || mod_string == "base" || mod_string == "silly" || mod_string == "verysilly" );
}

bool isValidMode( uint mod_mode )
{
	return ( mod_mode == MOD_NONE || mod_mode == MOD_BASE || mod_mode == MOD_SILLY || mod_mode == MOD_VERYSILLY );
}

void LoadRandomModifier()
{
	// select mod from current mode
	uint mod_mode = uint(gt_mod_mode.integer);

	if ( mod_mode == MOD_NONE )
	{
		LoadModifier(-1);
		return;
	}

	uint index;
	do {
		index = uint(brandom(0,numMods));
		G_Print("index: "+index+"\n");
	} while ( mod_mode & modTypes[index] == 0 || modWeaponMasks[index] & (1 << randWeap) != 0 );

	// apply weight unless MOD_ALWAYS
	float rand;
	if ( mod_mode & MOD_ALWAYS == 0 )
		rand = brandom(0,1);
	else 
		rand = 0.0;

	if ( rand <= modWeights[index] )
	{
		LoadModifier(index);
	} else {
		LoadModifier(-1);
	}
}

void LoadModifier(int index)
{
	if ( uint(index) >= numMods )
	{
		currentMod = -1;
		return;
	}

	currentMod = index;
}

void InitModifiers()
{
	if ( currentMod == -1 )
		return;

	if ( @modInits[currentMod] != null )
		modInits[currentMod]();

	G_PrintMsg(null, S_COLOR_WHITE + "Modifier: " + S_COLOR_YELLOW + "[ " + modNames[currentMod] + " " + S_COLOR_YELLOW + "]\n");
}

void ThinkModifiers()
{
	if ( currentMod == -1 )
		return;

	if ( @modThinks[currentMod] != null )
		modThinks[currentMod]();
}

void StopModifiers()
{
	if ( currentMod == -1 )
		return;

	if ( @modStops[currentMod] != null )
		modStops[currentMod]();
}

void addMod(uint modType, float weight, String& name, modInit@ init, modThink@ think, modStop@ stop, uint weaponMask)
{
	modTypes.push_back(modType);
	modWeights.push_back(weight);
	modNames.push_back(name);
	modInits.push_back(@init);
	modThinks.push_back(@think);
	modStops.push_back(@stop);
	modWeaponMasks.push_back(weaponMask);
	numMods++;
}

void LoadAllModifiers()
{
	addMod(
		MOD_BASE, 0.05, "Extra Knockback",
		@MOD_knockback_init,
		null,
		@MOD_knockback_stop,
		0
	);
	Cvar g_knockback_scale( "g_knockback_scale", "1.0", 0 );
	g_knockback_scale.reset();

	addMod(
		MOD_BASE, 0.05, "Triple Shot",
		null,
		@MOD_tripleshot_think,
		null,
		1 << WEAP_LASERGUN
	);
	// needs fix.
	/*addMod(
	  MOD_BASE, 0.05, "Ricochet",
	  null,
	  @MOD_ricochet_think,
	  null,
	  (1<<WEAP_LASERGUN)|(1<<WEAP_ELECTROBOLT)|(1<<WEAP_RIOTGUN)|(1<<WEAP_MACHINEGUN)|(1<<WEAP_GRENADELAUNCHER)
	  );*/
	/*addMod(
	  MOD_EXTRA, 0.05, "Drunk",
	  null,
	  @MOD_drunk_think,
	  null,
	  0
	  );
	  addMod(
	  MOD_EXTRA, 0.05, "Drunk2",
	  null,
	  @MOD_drunk2_think,
	  null,
	  0
	  );*/
	addMod(
		MOD_EXTRA, 0.05, "Upside Down",
		@MOD_upsidedown_init,
		@MOD_upsidedown_think,
		@MOD_upsidedown_stop,
		0
	);
	addMod(
		MOD_EXTRA, 0.05, "No Move",
		@MOD_nomove_init,
		null,
		@MOD_nomove_stop,
		0
	);
}
