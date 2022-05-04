/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "server/server.h"
#include "qcommon/version.h"
#include "qcommon/time.h"

struct SvMasterServer {
	NetAddress ipv4, ipv6;
};

static SvMasterServer master_servers[ ARRAY_COUNT( MASTER_SERVERS ) ];

extern Cvar * rcon_password;         // password for remote server commands
extern Cvar * sv_iplimit;

//==============================================================================
//
//MASTER SERVERS MANAGEMENT
//
//==============================================================================

static void SV_ResolveMaster() {
	memset( master_servers, 0, sizeof( master_servers ) );

	if( sv.state > ss_game ) {
		return;
	}

	if( !sv_public->integer ) {
		return;
	}

	for( size_t i = 0; i < ARRAY_COUNT( MASTER_SERVERS ); i++ ) {
		bool ipv4 = DNS( MASTER_SERVERS[ i ], &master_servers[ i ].ipv4, DNSFamily_IPv4 );
		bool ipv6 = DNS( MASTER_SERVERS[ i ], &master_servers[ i ].ipv6, DNSFamily_IPv6 );

		if( !ipv4 && !ipv6 ) {
			Com_Printf( "Can't resolve master server: %s\n", MASTER_SERVERS[ i ] );
			continue;
		}

		Com_GGPrintNL( "Added new master server #{} at", i );

		if( ipv4 ) {
			master_servers[ i ].ipv4.port = PORT_MASTER;
			Com_GGPrintNL( " {}", master_servers[ i ].ipv4 );
		}
		if( ipv6 ) {
			master_servers[ i ].ipv6.port = PORT_MASTER;
			Com_GGPrintNL( "{}{}", ipv4 ? " and " : " ", master_servers[ i ].ipv6 );
		}
		Com_Printf( "\n" );
	}

	svc.lastMasterResolve = Now();
}

void SV_InitMaster() {
	SV_ResolveMaster();

	svc.nextHeartbeat = Now();
}

void SV_UpdateMaster() {
	// refresh master server IP addresses periodically
	if( svc.lastMasterResolve + Days( 1 ) < Now() ) {
		SV_ResolveMaster();
	}
}

void SV_MasterHeartbeat() {
	Time time = Now();

	if( svc.nextHeartbeat > time ) {
		return;
	}

	svc.nextHeartbeat = time + Minutes( 5 );

	if( !sv_public->integer ) {
		return;
	}

	if( sv.state > ss_game ) {
		return;
	}

	for( const SvMasterServer & master : master_servers ) {
		// warning: "DarkPlaces" is a protocol name here, not a game name. Do not replace it.
		if( master.ipv4 != NULL_ADDRESS ) {
			Netchan_OutOfBandPrint( svs.socket, master.ipv4, "heartbeat DarkPlaces\n" );
		}
		if( master.ipv6 != NULL_ADDRESS ) {
			Netchan_OutOfBandPrint( svs.socket, master.ipv6, "heartbeat DarkPlaces\n" );
		}
	}
}

//============================================================================

/*
* SV_LongInfoString
* Builds the string that is sent as heartbeats and status replies
*/
static char *SV_LongInfoString( bool fullStatus ) {
	char tempstr[1024] = { 0 };
	static char status[MAX_MSGLEN - 16];
	int i, bots, count;
	client_t *cl;
	size_t statusLength;
	size_t tempstrLength;

	Q_strncpyz( status, Cvar_GetServerInfo(), sizeof( status ) );

	statusLength = strlen( status );

	bots = 0;
	count = 0;
	for( i = 0; i < sv_maxclients->integer; i++ ) {
		cl = &svs.clients[i];
		if( cl->state >= CS_CONNECTED ) {
			if( cl->edict->s.svflags & SVF_FAKECLIENT ) {
				bots++;
			}
			count++;
		}
	}

	if( bots ) {
		snprintf( tempstr, sizeof( tempstr ), "\\bots\\%i", bots );
	}
	snprintf( tempstr + strlen( tempstr ), sizeof( tempstr ) - strlen( tempstr ), "\\clients\\%i%s", count, fullStatus ? "\n" : "" );
	tempstrLength = strlen( tempstr );
	if( statusLength + tempstrLength >= sizeof( status ) ) {
		return status; // can't hold any more
	}
	Q_strncpyz( status + statusLength, tempstr, sizeof( status ) - statusLength );
	statusLength += tempstrLength;

	if( fullStatus ) {
		for( i = 0; i < sv_maxclients->integer; i++ ) {
			cl = &svs.clients[i];
			if( cl->state >= CS_CONNECTED ) {
				snprintf( tempstr, sizeof( tempstr ), "%i %i \"%s\" %i\n",
							 cl->edict->r.client->r.frags, cl->ping, cl->name, cl->edict->s.team );
				tempstrLength = strlen( tempstr );
				if( statusLength + tempstrLength >= sizeof( status ) ) {
					break; // can't hold any more
				}
				Q_strncpyz( status + statusLength, tempstr, sizeof( status ) - statusLength );
				statusLength += tempstrLength;
			}
		}
	}

	return status;
}

/*
* SV_ShortInfoString
* Generates a short info string for broadcast scan replies
*/
#define MAX_STRING_SVCINFOSTRING 180
#define MAX_SVCINFOSTRING_LEN ( MAX_STRING_SVCINFOSTRING - 4 )
static char *SV_ShortInfoString() {
	static char string[MAX_STRING_SVCINFOSTRING];
	char hostname[64];
	char entry[20];

	int bots = 0;
	int count = 0;
	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		if( svs.clients[i].state >= CS_CONNECTED ) {
			if( svs.clients[i].edict->s.svflags & SVF_FAKECLIENT ) {
				bots++;
			} else {
				count++;
			}
		}
	}
	int maxcount = sv_maxclients->integer - bots;

	//format:
	//" \377\377\377\377info\\n\\server_name\\m\\map name\\u\\clients/maxclients\\EOT "

	Q_strncpyz( hostname, sv_hostname->value, sizeof( hostname ) );
	snprintf( string, sizeof( string ),
				 "\\\\n\\\\%s\\\\m\\\\%s\\\\u\\\\%2i/%2i\\\\",
				 hostname,
				 sv.mapname,
				 Min2( count, 99 ),
				 Min2( maxcount, 99 )
				 );

	size_t len = strlen( string );

	const char * password = Cvar_String( "sv_password" );
	if( password[0] != '\0' ) {
		snprintf( entry, sizeof( entry ), "p\\\\1\\\\" );
		if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) ) {
			Q_strncatz( string, entry, sizeof( string ) );
			len = strlen( string );
		}
	}

	if( bots ) {
		snprintf( entry, sizeof( entry ), "b\\\\%2i\\\\", Min2( bots, 99 ) );
		if( MAX_SVCINFOSTRING_LEN - len > strlen( entry ) ) {
			Q_strncatz( string, entry, sizeof( string ) );
			len = strlen( string );
		}
	}

	// finish it
	Q_strncatz( string, "EOT", sizeof( string ) );
	return string;
}

//==============================================================================
//
//OUT OF BAND COMMANDS
//
//==============================================================================

static void SVC_InfoResponse( const NetAddress & address ) {
	if( sv_showInfoQueries->integer ) {
		Com_GGPrint( "Info Packet {}", address );
	}

	Netchan_OutOfBandPrint( svs.socket, address, "info\n%s%s", Cmd_Argv( 1 ), SV_ShortInfoString() );
}

static void MasterOrLivesowResponse( const NetAddress & address, const char * command, bool include_players ) {
	if( sv_showInfoQueries->integer ) {
		Com_GGPrint( "getstatus {}", address );
	}

	Netchan_OutOfBandPrint( svs.socket, address, "%s\n\\challenge\\%s%s", command, Cmd_Argv( 1 ), SV_LongInfoString( include_players ) );
}

static void SVC_GetStatusResponse( const NetAddress & address ) {
	MasterOrLivesowResponse( address, "statusResponse", true );
}

static void SVC_MasterServerResponse( const NetAddress & address ) {
	MasterOrLivesowResponse( address, "infoResponse", false );
}

/*
* SVC_GetChallenge
*
* Returns a challenge number that can be used
* in a subsequent client_connect command.
* We do this to prevent denial of service attacks that
* flood the server with invalid connection IPs.  With a
* challenge, they must give a valid IP address.
*/
static void SVC_GetChallenge( const NetAddress & address ) {
	int oldest = 0;
	int oldestTime = 0x7fffffff;

	if( sv_showChallenge->integer ) {
		Com_GGPrint( "Challenge Packet {}", address );
	}

	// see if we already have a challenge for this ip
	int i;
	for( i = 0; i < MAX_CHALLENGES; i++ ) {
		if( EqualIgnoringPort( address, svs.challenges[i].adr ) ) {
			break;
		}
		if( svs.challenges[i].time < oldestTime ) {
			oldestTime = svs.challenges[i].time;
			oldest = i;
		}
	}

	if( i == MAX_CHALLENGES ) {
		// overwrite the oldest
		svs.challenges[oldest].challenge = RandomUniform( &svs.rng, 0, S16_MAX );
		svs.challenges[oldest].adr = address;
		svs.challenges[oldest].time = Sys_Milliseconds();
		i = oldest;
	}

	Netchan_OutOfBandPrint( svs.socket, address, "challenge %i", svs.challenges[i].challenge );
}

/*
* SVC_DirectConnect
* A connection request that did not come from the master
*/
static void SVC_DirectConnect( const NetAddress & address ) {
	u64 version = SpanToU64( MakeSpan( Cmd_Argv( 1 ) ), U64_MAX );
	if( version != APP_PROTOCOL_VERSION ) {
		Netchan_OutOfBandPrint( svs.socket, address, "reject\n%i\nServer and client don't have the same version\n", 0 );
		return;
	}

	u64 session_id = SpanToU64( MakeSpan( Cmd_Argv( 2 ) ), 0 );
	int challenge = atoi( Cmd_Argv( 3 ) );

	if( !Info_Validate( Cmd_Argv( 4 ) ) ) {
		Netchan_OutOfBandPrint( svs.socket, address, "reject\n%i\nInvalid userinfo string\n", 0 );
		return;
	}

	char userinfo[ MAX_INFO_STRING ];
	Q_strncpyz( userinfo, Cmd_Argv( 4 ), sizeof( userinfo ) );

	// see if the challenge is valid
	{
		int i;
		for( i = 0; i < MAX_CHALLENGES; i++ ) {
			if( EqualIgnoringPort( address, svs.challenges[i].adr ) ) {
				if( challenge == svs.challenges[i].challenge ) {
					svs.challenges[i].challenge = 0; // wsw : r1q2 : reset challenge
					svs.challenges[i].time = 0;
					svs.challenges[i].adr = NULL_ADDRESS;
					break; // good
				}
				Netchan_OutOfBandPrint( svs.socket, address, "reject\n%i\nBad challenge\n", DROP_FLAG_AUTORECONNECT );
				return;
			}
		}
		if( i == MAX_CHALLENGES ) {
			Netchan_OutOfBandPrint( svs.socket, address, "reject\n%i\nNo challenge for address\n", DROP_FLAG_AUTORECONNECT );
			return;
		}
	}

	//r1: limit connections from a single IP
	if( sv_iplimit->integer ) {
		int previousclients = 0;
		for( int i = 0; i < sv_maxclients->integer; i++ ) {
			client_t * cl = &svs.clients[ i ];
			if( cl->state == CS_FREE ) {
				continue;
			}
			if( EqualIgnoringPort( address, cl->netchan.remoteAddress ) ) {
				//r1: zombies are less dangerous
				if( cl->state == CS_ZOMBIE ) {
					previousclients++;
				} else {
					previousclients += 2;
				}
			}
		}

		if( previousclients >= sv_iplimit->integer * 2 ) {
			Netchan_OutOfBandPrint( svs.socket, address, "reject\n%i\nToo many connections from your host\n", DROP_FLAG_AUTORECONNECT );
			return;
		}
	}

	client_t * newcl = NULL;

	// find a client slot
	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		client_t * cl = &svs.clients[ i ];
		if( cl->state == CS_FREE ) {
			newcl = cl;
			break;
		}
		// overwrite fakeclient if no free spots found
		if( cl->state && cl->edict && ( cl->edict->s.svflags & SVF_FAKECLIENT ) ) {
			newcl = cl;
		}
	}
	if( !newcl ) {
		Netchan_OutOfBandPrint( svs.socket, address, "reject\n%i\nServer is full\n", DROP_FLAG_AUTORECONNECT );
		return;
	}
	if( newcl->state && newcl->edict && ( newcl->edict->s.svflags & SVF_FAKECLIENT ) ) {
		SV_DropClient( newcl, "%s", "Need room for a real player" );
	}

	// get the game a chance to reject this connection or modify the userinfo
	if( !SV_ClientConnect( address, newcl, userinfo, session_id, challenge, false ) ) {
		const char * rejmsg = Info_ValueForKey( userinfo, "rejmsg" );

		Netchan_OutOfBandPrint( svs.socket, address, "reject\n%s\n", rejmsg );

		return;
	}

	// send the connect packet to the client
	Netchan_OutOfBandPrint( svs.socket, address, "client_connect" );
}

/*
* SVC_FakeConnect
* (Not a real out of band command)
* A connection request that came from the game module
*/
int SVC_FakeConnect( char * userinfo ) {
	// find a client slot
	client_t * newcl = NULL;
	for( int i = 0; i < sv_maxclients->integer; i++ ) {
		client_t * cl = &svs.clients[ i ];
		if( cl->state == CS_FREE ) {
			newcl = cl;
			break;
		}
	}
	if( newcl == NULL ) {
		return -1;
	}

	// get the game a chance to reject this connection or modify the userinfo
	if( !SV_ClientConnect( NULL_ADDRESS, newcl, userinfo, 0, -1, true ) ) {
		return -1;
	}

	// directly call the game begin function
	newcl->state = CS_SPAWNED;
	ClientBegin( newcl->edict );

	return NUM_FOR_EDICT( newcl->edict );
}

static bool Rcon_Validate() {
	return StrEqual( rcon_password->value, "" ) || StrEqual( Cmd_Argv( 1 ), rcon_password->value );
}

/*
* SVC_RemoteCommand
*
* A client issued an rcon command.
* Shift down the remaining args
* Redirect all printfs
*/
static void SVC_RemoteCommand( const NetAddress & address ) {
	if( Rcon_Validate() ) {
		Com_GGPrint( "Bad rcon from {}: {}", address, Cmd_Args() );
	}
	else {
		Com_GGPrint( "Rcon from {}: {}", address, Cmd_Args() );
	}

	Com_BeginRedirect( RD_PACKET, sv_outputbuf, SV_OUTPUTBUF_LENGTH, SV_FlushRedirect, &address );

	if( !Rcon_Validate() ) {
		Com_Printf( "Bad rcon_password.\n" );
	}
	else {
		Cbuf_ExecuteLine( Cmd_Args() );
	}

	Com_EndRedirect();
}

struct connectionless_cmd_t {
	const char *name;
	void ( *func )( const NetAddress & address );
};

static connectionless_cmd_t connectionless_cmds[] = {
	{ "info", SVC_InfoResponse },
	{ "getinfo", SVC_MasterServerResponse },
	{ "getstatus", SVC_GetStatusResponse },
	{ "getchallenge", SVC_GetChallenge },
	{ "connect", SVC_DirectConnect },
	{ "rcon", SVC_RemoteCommand },
};

/*
* SV_ConnectionlessPacket
*
* A connectionless packet has four leading 0xff
* characters to distinguish it from a game channel.
* Clients that are in the game can still send
* connectionless packets.
*/
void SV_ConnectionlessPacket( const NetAddress & address, msg_t * msg ) {
	MSG_BeginReading( msg );
	MSG_ReadInt32( msg );    // skip the -1 marker

	const char * s = MSG_ReadStringLine( msg );
	Cmd_TokenizeString( s );

	const char * c = Cmd_Argv( 0 );
	for( auto cmd : connectionless_cmds ) {
		if( StrEqual( c, cmd.name ) ) {
			cmd.func( address );
			return;
		}
	}
}
