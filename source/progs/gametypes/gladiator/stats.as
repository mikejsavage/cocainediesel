Cvar gt_stats_debug = Cvar("gt_stats_debug", "0", 0);
bool stats_initialized = false;
const String stats_basepath = "stats/gladiator/";
const String invalid_chars = "\\/:*?\"<>|";
StatsFile@ stats_xp;

void G_SPrint( String& msg )
{
	if ( gt_stats_debug.boolean )
		G_Print(msg);
}

Stats_Player[] players( maxClients );
class Stats_Player
{
	Client@ client;
	StatsFile@ stats;

	Stats_Player()
	{
	}

	~Stats_Player() {}

	void load()
	{
		String cleanName = this.client.name.removeColorTokens().tolower();
		for ( uint i = 0; i < invalid_chars.length(); i++ )
		{
			cleanName = cleanName.replace(invalid_chars.substr(i,1), "");
		}
		G_Print("clear "+cleanName+"\n");
		// TODO: find better solution for this
		// random trailing character because files ending in "." don't work
		@this.stats = @StatsFile("players/"+cleanName+"_");
		this.stats["cleanName"] = cleanName;
		this.stats["name"] = client.name;
		this.stats.save();
	}
}

Stats_Player@ GT_Stats_GetPlayer( Client@ client )
{
	if ( @client == null || client.playerNum < 0 )
		return null;

	Stats_Player @player = players[client.playerNum];
	@player.client = client;

	return player;
}

class StatsFile
{
	String path;
	String[] vars;
	String[] values;

	StatsFile(String& path)
	{
		this.path = stats_basepath + path + ".txt";
		String file = G_LoadFile( this.path );
		if ( file.length() <= 0 )
		{
			G_SPrint("^1ERR: ^7failed to load: "+this.path+"\n");
			return;
		}

		uint i = 0;
		while ( file.getToken(i).length() != 0 )
		{
			this.vars.push_back( file.getToken(i++) );
			this.values.push_back( file.getToken(i++) );
		}
		G_SPrint("^2OK: ^7Loaded file: "+this.path+"\n");
	}

	~StatsFile() {}

	String toString()
	{
		String file = "";
		for ( uint i = 0; i < this.vars.length(); i++ )
		{
			file += "\""+this.vars[i]+"\" ";
			file += "\""+this.values[i]+"\"\n";
		}
		return file;
	}

	void save()
	{
		G_WriteFile( this.path, this.toString() );
		G_SPrint("^2OK: ^7Wrote file: "+this.path+"\n");
	}

	int getIndex(String& var)
	{
		return this.vars.find(var);
	}

	String get_opIndex(String& var)
	{
		int index = this.getIndex(var);
		if ( index >= 0 )
			return this.values[index];
		return "";
	}

	void set_opIndex(String& var, String& value)
	{
		int index = this.getIndex(var);
		if ( index == -1 )
		{
			this.vars.push_back( var );
			this.values.push_back( value );
			return;
		}

		this.values[index] = value;
		this.save();
	}

	void add(String& var, int value)
	{
		value += this[var].toInt();
		this[var] = String(value);
	}
}

void GT_Stats_Init()
{
	if ( stats_initialized )
		return;

	@stats_xp = @StatsFile("xp");

	for ( int i = 0; i < maxClients; i++ )
	{
		Client@ client = @G_GetClient(i);
		if ( @client == null )
			continue;
		if ( client.state() < CS_SPAWNED )
			continue;

		GT_Stats_GetPlayer( client ).load();
	}

	stats_initialized = true;
}
