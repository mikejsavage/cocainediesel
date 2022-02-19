#include "qcommon/base.h"
#include "qcommon/array.h"
#include "qcommon/threads.h"
#include "qcommon/locked.h"
#include "qcommon/version.h"
#include "client/client.h"
#include "client/server_browser.h"

struct MasterServer {
	netadr_t address;
	Thread * resolver_thread;
	bool query_next_frame;
};

struct MasterServers {
	MasterServer servers[ ARRAY_COUNT( MASTER_SERVERS ) ];
	size_t num_dns_queries_in_flight;
};

static Locked< MasterServers > locked_master_servers;
static NonRAIIDynamicArray< ServerBrowserEntry > servers;

void InitServerBrowser() {
	locked_master_servers.data = { };
	locked_master_servers.mutex = NewMutex();

	servers.init( sys_allocator );
}

void ShutdownServerBrowser() {
	servers.shutdown();

	// leak the mutex and resolver threads because we can't shut them down
	// quickly and reliably
}

Span< const ServerBrowserEntry > GetServerBrowserEntries() {
	return servers.span();
}

static void GetMasterServerAddress( void * data ) {
	size_t idx = size_t( uintptr_t( data ) );

	netadr_t thread_local_address;
	NET_StringToAddress( MASTER_SERVERS[ idx ], &thread_local_address );
	NET_SetAddressPort( &thread_local_address, PORT_MASTER );

	if( thread_local_address.type == NA_NOTRANSMIT ) {
		Com_Printf( "Failed to resolve master server address: %s\n", MASTER_SERVERS[ idx ] );
	}

	DoUnderLock( &locked_master_servers, [ idx, thread_local_address ]( MasterServers * master_servers ) {
		master_servers->servers[ idx ].address = thread_local_address;
		master_servers->servers[ idx ].query_next_frame = true;
		master_servers->num_dns_queries_in_flight--;
	} );
}

static void QueryMasterServer( MasterServer * master ) {
	const char * command;
	socket_t * socket;
	if( master->address.type == NA_IPv4 ) {
		command = "getservers";
		socket = &cls.socket_udp;
	}
	else {
		command = "getserversExt";
		socket = &cls.socket_udp6;
	}

	TempAllocator temp = cls.frame_arena.temp();
	const char * query = temp( "{} {} {} full empty", command, APPLICATION_NOSPACES, APP_PROTOCOL_VERSION );
	Netchan_OutOfBandPrint( socket, &master->address, "%s", query );
	master->query_next_frame = false;
}

void ServerBrowserFrame() {
	DoUnderLock( &locked_master_servers, []( MasterServers * master_servers ) {
		for( MasterServer & master : master_servers->servers ) {
			if( master.query_next_frame ) {
				JoinThread( master.resolver_thread );
				QueryMasterServer( &master );
			}
		}
	} );
}

void RefreshServerBrowser() {
	servers.clear();

	// query LAN servers
	{
		TempAllocator temp = cls.frame_arena.temp();
		const char * query = temp( "info {}", cls.monotonicTime );

		netadr_t broadcast;
		NET_BroadcastAddress( &broadcast, PORT_SERVER );
		Netchan_OutOfBandPrint( &cls.socket_udp, &broadcast, "%s", query );
	}

	// query master servers
	DoUnderLock( &locked_master_servers, []( MasterServers * master_servers ) {
		for( MasterServer & master : master_servers->servers ) {
			QueryMasterServer( &master );
		}

		if( master_servers->num_dns_queries_in_flight == 0 ) {
			for( size_t i = 0; i < ARRAY_COUNT( MASTER_SERVERS ); i++ ) {
				if( master_servers->servers[ i ].address.type == NA_NOTRANSMIT ) {
					master_servers->servers[ i ].resolver_thread = NewThread( GetMasterServerAddress, ( void * ) uintptr_t( i ) );
				}
			}
		}
	} );
}

void ParseMasterServerResponse( msg_t * msg, bool allow_ipv6 ) {
	MSG_BeginReading( msg );
	MSG_ReadInt32( msg ); // skip the -1
	MSG_SkipData( msg, strlen( allow_ipv6 ? "getserversExtResponse" : "getServersResponse" ) );

	size_t old_num_servers = servers.size();
	bool ok = false;

	while( true ) {
		char separator = MSG_ReadInt8( msg );
		netadr_t addr;

		if( separator == '\\' ) {
			addr.type = NA_IPv4;
			MSG_ReadData( msg, &addr.ipv4, sizeof( addr.ipv4 ) );
		}
		else if( separator == '/' ) {
			if( !allow_ipv6 ) {
				Com_Printf( "Master server responded with an IPv6 address when we only asked for IPv4\n" );
				break;
			}

			addr.type = NA_IPv6;
			MSG_ReadData( msg, &addr.ipv6, sizeof( addr.ipv6 ) );
		}
		else {
			Com_Printf( "Bad separator in master server response\n" );
			break;
		}

		addr.port = NET_ntohs( MSG_ReadUint16( msg ) );

		if( addr.port == 0 && msg->readcount == msg->cursize ) {
			ok = true;
			break;
		}

		if( msg->readcount > msg->cursize ) {
			Com_Printf( "Truncated master server response\n" );
			break;
		}

		bool is_new = true;
		for( const ServerBrowserEntry & server : servers ) {
			if( server.address == addr ) {
				is_new = false;
				break;
			}
		}

		if( is_new ) {
			ServerBrowserEntry server = { };
			server.address = addr;
			servers.add( server );
		}
	}

	if( ok ) {
		// query game servers
		TempAllocator temp = cls.frame_arena.temp();
		const char * query = temp( "info {}", cls.monotonicTime );

		for( size_t i = old_num_servers; i < servers.size(); i++ ) {
			netadr_t address = servers[ i ].address;
			socket_t * socket = address.type == NA_IPv4 ? &cls.socket_udp : &cls.socket_udp6;
			Netchan_OutOfBandPrint( socket, &address, "%s", query );
		}
	}
	else {
		servers.resize( old_num_servers );
	}
}

void ParseGameServerResponse( msg_t * msg, netadr_t address ) {
	s64 timestamp;
	char name[ 128 ];
	char map[ 32 ];
	int num_players;
	int max_players;

	const char * info = MSG_ReadString( msg );
	int parsed = sscanf( info, "%" SCNi64 "\\\\n\\\\%127[^\\]\\\\m\\\\%31[^\\]\\\\u\\\\%d/%d\\\\EOT", &timestamp, name, map, &num_players, &max_players );
	if( parsed != 5 ) {
		return;
	}

	ServerBrowserEntry * server = NULL;
	for( ServerBrowserEntry & s : servers ) {
		if( s.address == address ) {
			server = &s;
			break;
		}
	}

	if( server == NULL ) {
		server = servers.add();
		server->address = address;
	}

	server->have_details = true;
	Q_strncpyz( server->name, name, sizeof( server->name ) );
	Q_strncpyz( server->map, map, sizeof( server->map ) );
	server->ping = cls.monotonicTime - timestamp;
	server->num_players = num_players;
	server->max_players = max_players;
}
