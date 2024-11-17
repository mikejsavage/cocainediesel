#include "server/server.h"
#include "qcommon/application.h"
#include "qcommon/fs.h"
#include "qcommon/net.h"
#include "qcommon/string.h"
#include "qcommon/threads.h"
#include "qcommon/time.h"
#include "gameshared/q_shared.h"

#include "picohttpparser/picohttpparser.h"

#include <atomic>

static constexpr Time REQUEST_TIMEOUT = Seconds( 10 );
static constexpr Time RESPONSE_INACTIVITY_TIMEOUT = Seconds( 15 );

// server checks for shutdown this frequently so don't make it too big
static constexpr s64 HTTP_SERVER_SLEEP_TIME = 50;

enum HTTPResponseCode {
	HTTPResponseCode_Ok = 200,
	HTTPResponseCode_BadRequest = 400,
	HTTPResponseCode_Forbidden = 403,
	HTTPResponseCode_NotFound = 404,
};

struct HTTPResponse {
	String< 256 > headers;
	size_t headers_sent;

	FILE * file;
	size_t file_size;
	size_t file_sent;
};

struct HTTPConnection {
	bool should_close;
	bool received_request;

	Socket socket;
	NetAddress address;

	Time last_activity;

	char request[ 256 ];
	size_t request_size;

	HTTPResponse response;
};

static std::atomic< bool > web_server_running = false;

static HTTPConnection connections[ 32 ];

static ArenaAllocator web_server_arena;
static Socket web_server_socket;
static Thread * web_server_thread = NULL;

static HTTPConnection * TryAllocConnection() {
	for( HTTPConnection & con : connections ) {
		if( con.address == NULL_ADDRESS ) {
			return &con;
		}
	}

	return NULL;
}

static void FreeConnection( HTTPConnection * con ) {
	if( con->response.file != NULL ) {
		fclose( con->response.file );
	}

	*con = { };
}

static const char * ResponseCodeMessage( HTTPResponseCode code ) {
	switch( code ) {
		case HTTPResponseCode_Ok: return "OK";
		case HTTPResponseCode_BadRequest: return "Bad Request";
		case HTTPResponseCode_Forbidden: return "Forbidden";
		case HTTPResponseCode_NotFound: return "Not Found";
	}

	Assert( false );
	return "";
}

static HTTPResponseCode RouteRequest( HTTPConnection * con, Span< const char > method, Span< const char > path_with_leading_slash, const phr_header * headers, size_t num_headers ) {
	for( size_t i = 0; i < num_headers; i++ ) {
		Span< const char > header = Span< const char >( headers[ i ].name, headers[ i ].name_len );
		if( StrCaseEqual( header, "Content-Length" ) ) {
			Span< const char > value = Span< const char >( headers[ i ].value, headers[ i ].value_len );
			if( SpanToInt( value, 0 ) != 0 ) {
				return HTTPResponseCode_BadRequest;
			}
		}
	}

	bool head_request = StrCaseEqual( method, "HEAD" );
	if( !head_request && !StrCaseEqual( method, "GET" ) ) {
		return HTTPResponseCode_BadRequest;
	}

	TempAllocator temp = web_server_arena.temp();

	Span< const char > path = path_with_leading_slash + 1;

	// check for malicious URLs
	if( !COM_ValidateRelativeFilename( path ) ) {
		return HTTPResponseCode_Forbidden;
	}

	if( !EndsWith( path, ".cdmap.zst" ) && !EndsWith( path, APP_DEMO_EXTENSION_STR ) ) {
		return HTTPResponseCode_Forbidden;
	}

	HTTPResponse * response = &con->response;
	response->file = OpenFile( sys_allocator, temp( "{}", path ), OpenFile_Read );
	if( response->file == NULL ) {
		return HTTPResponseCode_NotFound;
	}

	response->file_size = FileSize( response->file );

	if( head_request ) {
		fclose( response->file );
		response->file_sent = response->file_size;
		response->file = NULL;
	}

	return HTTPResponseCode_Ok;
}

static void MakeResponse( HTTPConnection * con, Span< const char > method, Span< const char > path, const phr_header * request_headers, size_t num_headers ) {
	HTTPResponseCode code = RouteRequest( con, method, path, request_headers, num_headers );

	HTTPResponse * response = &con->response;
	if( response->file != NULL ) {
		Com_GGPrint( "HTTP serving file '{}' to {}", path, con->address );
	}

	response->headers.clear();
	response->headers.append( "HTTP/1.1 {} {}\r\n", code, ResponseCodeMessage( code ) );
	response->headers.append( "Server: " APPLICATION "\r\n" );

	if( code == HTTPResponseCode_Ok ) {
		response->headers.append( "Content-Length: {}\r\n", response->file_size );
		response->headers.append( "Content-Disposition: attachment; filename=\"{}\"\r\n", FileName( path ) );
		response->headers += "\r\n";
	}
	else {
		String< 64 > error( "{} {}\n", code, ResponseCodeMessage( code ) );
		response->headers.append( "Content-Type: text/plain\r\n" );
		response->headers.append( "Content-Length: {}\r\n", error.length() );
		response->headers += "\r\n";
		response->headers += error;
	}

	Assert( response->headers.length() < response->headers.capacity() );
}

static void ReceiveRequest( HTTPConnection * con ) {
	if( con->received_request )
		return;

	while( true ) {
		size_t received;
		if( !TCPReceive( con->socket, con->request + con->request_size, sizeof( con->request ) - con->request_size, &received ) ) {
			con->should_close = true;
			break;
		}
		if( received == 0 ) {
			break;
		}

		size_t last_request_size = con->request_size;
		con->request_size += received;

		// don't update last_activity, we want to kill the connection
		// if they don't send a request in time

		const char * method;
		size_t method_len;

		const char * path;
		size_t path_len;
		int minor_version;

		phr_header headers[ 16 ];
		size_t num_headers = ARRAY_COUNT( headers );

		ssize_t ok = phr_parse_request( con->request, con->request_size, &method, &method_len, &path, &path_len, &minor_version, headers, &num_headers, last_request_size );
		if( ok == -1 ) {
			con->should_close = true;
			break;
		}
		if( ok == -2 ) {
			if( con->request_size == sizeof( con->request ) ) {
				con->should_close = true;
				break;
			}
			continue;
		}

		MakeResponse( con,
			Span< const char >( method, method_len ),
			Span< const char >( path, path_len ),
			headers, num_headers );

		con->received_request = true;
		break;
	}
}

static bool SendFileChunk( Socket socket, FILE * f, size_t offset, size_t * sent ) {
	Seek( f, offset );

	char buf[ 8192 ];
	size_t r = fread( buf, 1, sizeof( buf ), f );
	if( r < sizeof( buf ) && ferror( f ) ) {
		return false;
	}

	*sent = 0;
	while( *sent < r ) {
		size_t w;
		if( !TCPSend( socket, buf + *sent, r - *sent, &w ) ) {
			return false;
		}
		if( w == 0 ) {
			break;
		}
		*sent += w;
	}

	return true;
}

static void SendResponse( HTTPConnection * con, Time now ) {
	if( !con->received_request )
		return;

	HTTPResponse * response = &con->response;
	while( response->headers_sent < response->headers.length() ) {
		size_t sent;
		if( !TCPSend( con->socket, response->headers.c_str() + response->headers_sent, response->headers.length() - response->headers_sent, &sent ) ) {
			con->should_close = true;
			return;
		}
		if( sent == 0 ) {
			break;
		}

		response->headers_sent += sent;
		con->last_activity = now;
	}

	if( response->headers_sent < response->headers.length() ) {
		return;
	}

	while( response->file_sent < response->file_size ) {
		size_t sent;
		if( !SendFileChunk( con->socket, response->file, response->file_sent, &sent ) ) {
			con->should_close = true;
			return;
		}
		if( sent == 0 ) {
			break;
		}

		response->file_sent += sent;
		con->last_activity = now;
	}

	if( response->file_sent == response->file_size ) {
		con->should_close = true;
	}
}

static bool IPConnectionLimitReached( const NetAddress & address ) {
	constexpr int max_connections_per_ip = 3;

	int n = 0;
	for( const HTTPConnection & con : connections ) {
		if( con.address != NULL_ADDRESS && address == con.address ) {
			n++;
			if( n >= max_connections_per_ip ) {
				return true;
			}
		}
	}

	return false;
}

static void AcceptIncomingConnections( Time now ) {
	Socket client;
	NetAddress address;
	while( TCPAccept( web_server_socket, NonBlocking_Yes, &client, &address ) ) {
		if( IPConnectionLimitReached( address ) ) {
			CloseSocket( client );
			continue;
		}

		HTTPConnection * con = TryAllocConnection();
		if( con == NULL ) {
			CloseSocket( client );
			break;
		}

		con->socket = client;
		con->address = address;
		con->last_activity = now;
	}
}

static void WebServerFrame() {
	TracyZoneScoped;

	Socket sockets[ ARRAY_COUNT( connections ) + 1 ];
	HTTPConnection * socket_to_connection[ ARRAY_COUNT( sockets ) ];
	size_t n = 0;

	sockets[ n ] = web_server_socket;
	n++;

	for( HTTPConnection & con : connections ) {
		if( con.address != NULL_ADDRESS ) {
			sockets[ n ] = con.socket;
			socket_to_connection[ n ] = &con;
			n++;
		}
	}

	TempAllocator temp = web_server_arena.temp();
	WaitForSocketResult results[ ARRAY_COUNT( sockets ) ];
	WaitForSockets( &temp, sockets, n, HTTP_SERVER_SLEEP_TIME, WaitForSocketWriteable_Yes, results );

	Time now = Now();

	if( results[ 0 ].readable ) {
		AcceptIncomingConnections( now );
	}

	for( size_t i = 1; i < n; i++ ) {
		if( results[ i ].readable ) {
			ReceiveRequest( socket_to_connection[ i ] );
		}
		if( results[ i ].writeable ) {
			SendResponse( socket_to_connection[ i ], now );
		}
	}

	for( HTTPConnection & con : connections ) {
		if( con.address == NULL_ADDRESS )
			continue;

		Time timeout = con.received_request ? RESPONSE_INACTIVITY_TIMEOUT : REQUEST_TIMEOUT;
		if( now > con.last_activity + timeout ) {
			con.should_close = true;
		}

		if( con.should_close ) {
			CloseSocket( con.socket );
			FreeConnection( &con );
		}
	}
}

static void WebServerThread( void * param ) {
	TracyCSetThreadName( "Web server thread" );

	while( web_server_running ) {
		WebServerFrame();
	}

	for( HTTPConnection & con : connections ) {
		if( con.address != NULL_ADDRESS ) {
			CloseSocket( con.socket );
			FreeConnection( &con );
		}
	}
}

void InitWebServer() {
	web_server_running = false;

	if( !StrEqual( sv_downloadurl->value, "" ) ) {
		return;
	}

	memset( connections, 0, sizeof( connections ) );
	web_server_running = true;

	constexpr size_t web_server_arena_size = 128 * 1024; // 128KB
	void * web_server_arena_memory = sys_allocator->allocate( web_server_arena_size, 16 );
	web_server_arena = ArenaAllocator( web_server_arena_memory, web_server_arena_size );

	web_server_socket = NewTCPServer( sv_port->integer, NonBlocking_Yes );
	web_server_thread = NewThread( WebServerThread );
}

void ShutdownWebServer() {
	web_server_running = false;
	JoinThread( web_server_thread );
	CloseSocket( web_server_socket );
	Free( sys_allocator, web_server_arena.get_memory() );
}
