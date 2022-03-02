/*
Copyright (C) 2013 Victor Luchits

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
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/threads.h"

#include "tracy/Tracy.hpp"

static constexpr size_t MAX_INCOMING_HTTP_CONNECTIONS = 48;
static constexpr int MAX_INCOMING_HTTP_CONNECTIONS_PER_ADDR = 3;

static constexpr s64 INCOMING_HTTP_CONNECTION_RECV_TIMEOUT = 50000;
static constexpr s64 INCOMING_HTTP_CONNECTION_SEND_TIMEOUT = 15000;

// server checks for shutdown this frequently so don't make it too big
static constexpr s64 HTTP_SERVER_SLEEP_TIME = 50;

enum http_query_method_t {
	HTTP_METHOD_NONE,
	HTTP_METHOD_GET,
	HTTP_METHOD_HEAD,
};

enum http_response_code_t {
	HTTP_RESP_NONE = 0,
	HTTP_RESP_OK = 200,
	HTTP_RESP_BAD_REQUEST = 400,
	HTTP_RESP_FORBIDDEN = 403,
	HTTP_RESP_NOT_FOUND = 404,
	HTTP_RESP_REQUEST_TOO_LARGE = 413,
};

enum sv_http_connstate_t {
	HTTP_CONN_STATE_NONE,
	HTTP_CONN_STATE_RECV,
	HTTP_CONN_STATE_SEND,
};

struct sv_http_stream_t {
	size_t header_length;
	char header_buf[256];
	size_t header_buf_p;
	bool header_done;
};

struct sv_http_request_t {
	http_query_method_t method;
	http_response_code_t error;
	sv_http_stream_t stream;

	char * resource;
	const char * query_string;

	bool got_start_line;
};

struct sv_http_response_t {
	http_response_code_t code;
	sv_http_stream_t stream;

	FILE * file;
	char * filename;
	size_t filesize;
	size_t filesent;
};

struct sv_http_connection_t {
	bool open;
	sv_http_connstate_t state;

	Socket socket;
	NetAddress address;

	int64_t last_active;

	sv_http_request_t request;
	sv_http_response_t response;
};

static volatile bool sv_http_running = false;

static sv_http_connection_t sv_http_connections[MAX_INCOMING_HTTP_CONNECTIONS];

static ArenaAllocator web_server_arena;
static Socket web_server_socket;
// TODO: this should be fully async and doesn't need to run on a thread anymore
static Thread * web_server_thread = NULL;

// ============================================================================

static void SV_Web_ResetStream( sv_http_stream_t * stream ) {
	stream->header_done = false;
	stream->header_length = 0;
	stream->header_buf_p = 0;
}

static void SV_Web_ResetRequest( sv_http_request_t * request ) {
	if( request->resource ) {
		FREE( sys_allocator, request->resource );
		request->resource = NULL;
	}

	request->query_string = "";
	SV_Web_ResetStream( &request->stream );

	request->got_start_line = false;
	request->error = HTTP_RESP_NONE;
}

static void SV_Web_ResetResponse( sv_http_response_t * response ) {
	if( response->filename ) {
		FREE( sys_allocator, response->filename );
		response->filename = NULL;
	}
	if( response->file ) {
		fclose( response->file );
		response->file = NULL;
	}

	response->filesize = 0;
	response->filesent = 0;

	SV_Web_ResetStream( &response->stream );

	response->code = HTTP_RESP_NONE;
}

static sv_http_connection_t * SV_Web_AllocConnection() {
	for( sv_http_connection_t & con : sv_http_connections ) {
		if( con.state == HTTP_CONN_STATE_NONE ) {
			return &con;
		}
	}

	return NULL;
}

static void SV_Web_FreeConnection( sv_http_connection_t * con ) {
	SV_Web_ResetRequest( &con->request );
	SV_Web_ResetResponse( &con->response );

	con->state = HTTP_CONN_STATE_NONE;
}

static void SV_Web_ShutdownConnections() {
	for( sv_http_connection_t & con : sv_http_connections ) {
		if( con.state == HTTP_CONN_STATE_NONE )
			continue;

		if( con.open ) {
			CloseSocket( con.socket );
			SV_Web_FreeConnection( &con );
		}
	}
}

static bool SV_Web_ConnectionLimitReached( const NetAddress & address ) {
	int n = 0;
	for( const sv_http_connection_t & con : sv_http_connections ) {
		if( con.state != HTTP_CONN_STATE_NONE && address == con.address ) {
			n++;
			if( n >= MAX_INCOMING_HTTP_CONNECTIONS_PER_ADDR ) {
				return true;
			}
		}
	}

	return false;
}

static size_t SendFileChunk( sv_http_connection_t * con, FILE * f, size_t offset ) {
	fseek( f, offset, SEEK_SET );

	char buf[ 8192 ];
	size_t r = fread( buf, 1, sizeof( buf ), f );
	if( r < sizeof( buf ) && ferror( f ) ) {
		con->open = false;
		return 0;
	}

	size_t sent = 0;
	while( sent < r ) {
		size_t w;
		if( !TCPSend( con->socket, buf + sent, r - sent, &w ) ) {
			con->open = false;
			return 0;
		}
		sent += w;
	}

	return sent;
}

static void SV_Web_ParseStartLine( sv_http_request_t * request, char * line ) {
	char * ptr;
	char * token, * delim;

	ptr = line;

	token = ptr;
	ptr = strchr( token, ' ' );
	if( !ptr ) {
		request->error = HTTP_RESP_BAD_REQUEST;
		return;
	}
	*ptr = '\0';

	if( StrCaseEqual( token, "GET" ) ) {
		request->method = HTTP_METHOD_GET;
	} else if( StrCaseEqual( token, "HEAD" ) ) {
		request->method = HTTP_METHOD_HEAD;
	} else {
		request->error = HTTP_RESP_BAD_REQUEST;
	}

	token = ptr + 1;
	while( *token <= ' ' || *token == '/' ) {
		token++;
	}
	ptr = strrchr( token, ' ' );
	if( !ptr ) {
		request->error = HTTP_RESP_BAD_REQUEST;
		return;
	}
	*ptr = '\0';

	request->resource = CopyString( sys_allocator, strlen( token ) > 0 ? token : "/" );
	Q_urldecode( request->resource, request->resource, strlen( request->resource ) + 1 );

	// split resource into filepath and query string
	delim = strstr( request->resource, "?" );
	if( delim ) {
		*delim = '\0';
		request->query_string = delim + 1;
	}

	token = ptr + 1;
	while( *token <= ' ' ) {
		token++;
	}

	if( !StrEqual( token, "HTTP/1.1" ) ) {
		request->error = HTTP_RESP_BAD_REQUEST;
	}
}

static void SV_Web_AnalyzeHeader( sv_http_request_t * request, const char * key, const char * value ) {
	if( StrCaseEqual( key, "Content-Length" ) ) {
		u64 length;
		bool ok = TryStringToU64( value, &length );
		if( !ok ) {
			request->error = HTTP_RESP_BAD_REQUEST;
		}
		else if( length > 0 ) {
			request->error = HTTP_RESP_REQUEST_TOO_LARGE;
		}
	}
}

/*
* SV_Web_ParseHeaderLine
*
* Parses and splits the header line into key-value pair
*/
static void SV_Web_ParseHeaderLine( sv_http_request_t * request, char * line ) {
	char * value;
	size_t offset;
	const char * colon;
	const char * key;

	if( request->error ) {
		return;
	}

	colon = strchr( line, ':' );
	if( !colon ) {
		return;
	}

	offset = colon - line;
	line[offset] = '\0';
	key = Q_trim( line );
	value = line + offset + 1;

	// ltrim
	while( *value <= ' ' ) {
		value++;
	}
	SV_Web_AnalyzeHeader( request, key, value );
}

static size_t SV_Web_ParseHeaders( sv_http_request_t * request, char * data ) {
	char * line, * p;

	line = data;
	while( ( p = strstr( line, "\r\n" ) ) != NULL ) {
		if( p == line ) {
			line = p + 2;
			request->stream.header_done = true;
			break;
		}

		*p = *( p + 1 ) = '\0';

		if( request->got_start_line ) {
			SV_Web_ParseHeaderLine( request, line );
		} else {
			SV_Web_ParseStartLine( request, line );
			request->got_start_line = true;
		}

		line = p + 2;
	}
	return ( line - data );
}

static const char * SV_Web_ResponseCodeMessage( http_response_code_t code ) {
	switch( code ) {
		case HTTP_RESP_OK: return "OK";
		case HTTP_RESP_BAD_REQUEST: return "Bad Request";
		case HTTP_RESP_FORBIDDEN: return "Forbidden";
		case HTTP_RESP_NOT_FOUND: return "Not Found";
		case HTTP_RESP_REQUEST_TOO_LARGE: return "Request Entity Too Large";
		default: return "Unknown Error";
	}
}

static void SV_Web_RouteRequest( const sv_http_request_t * request, sv_http_response_t * response ) {
	response->filename = CopyString( sys_allocator, request->resource );

	if( request->method != HTTP_METHOD_GET && request->method != HTTP_METHOD_HEAD ) {
		response->code = HTTP_RESP_BAD_REQUEST;
		return;
	}

	// check for malicious URL's
	if( !COM_ValidateRelativeFilename( request->resource ) ) {
		response->code = HTTP_RESP_FORBIDDEN;
		return;
	}

	if( !EndsWith( request->resource, ".bsp.zst" ) && !SV_IsDemoDownloadRequest( request->resource ) ) {
		response->code = HTTP_RESP_FORBIDDEN;
		return;
	}

	response->file = OpenFile( sys_allocator, request->resource, "rb" );
	if( response->file == NULL ) {
		response->code = HTTP_RESP_NOT_FOUND;
		return;
	}

	response->filesize = FileSize( response->file );
	response->code = HTTP_RESP_OK;
}

static void SV_Web_MakeResponse( sv_http_connection_t * con ) {
	sv_http_request_t * request = &con->request;
	sv_http_response_t * response = &con->response;
	sv_http_stream_t * resp_stream = &response->stream;

	if( request->error != 0 ) {
		response->code = request->error;
	}
	else {
		SV_Web_RouteRequest( request, response );

		if( response->file != NULL ) {
			Com_GGPrint( "HTTP serving file '{}' to {}", response->filename, con->address );
		}

		if( request->method == HTTP_METHOD_HEAD && response->file != NULL ) {
			fclose( response->file );
			response->file = NULL;
		}
	}

	String< sizeof( resp_stream->header_buf ) - 1 > headers;
	headers.append( "HTTP/1.1 {} {}\r\n", response->code, SV_Web_ResponseCodeMessage( response->code ) );
	headers.append( "Server: " APPLICATION "\r\n" );

	if( response->code == HTTP_RESP_OK ) {
		headers.append( "Content-Length: {}\r\n", response->filesize );
		headers.append( "Content-Disposition: attachment; filename=\"{}\"\r\n", FileName( response->filename ) );
		headers += "\r\n";
	}
	else {
		String< 64 > error( "{} {}\n", response->code, SV_Web_ResponseCodeMessage( response->code ) );

		headers.append( "Content-Type: text/plain\r\n" );
		headers.append( "Content-Length: {}\r\n", error.length() );
		headers += "\r\n";
		headers += error;
	}

	ggformat( resp_stream->header_buf, sizeof( resp_stream->header_buf ), "{}", headers );

	resp_stream->header_length = headers.length();
	con->state = HTTP_CONN_STATE_SEND;
}

static void SV_Web_ReceiveRequest( sv_http_connection_t * con ) {
	if( con->state != HTTP_CONN_STATE_RECV )
		return;

	sv_http_request_t * request = &con->request;
	while( !request->stream.header_done ) {
		char * recvbuf = request->stream.header_buf + request->stream.header_buf_p;
		size_t recvbuf_size = sizeof( request->stream.header_buf ) - request->stream.header_buf_p;
		if( recvbuf_size <= 1 ) {
			request->error = HTTP_RESP_BAD_REQUEST;
			break;
		}

		size_t received;
		if( !TCPReceive( con->socket, recvbuf, recvbuf_size - 1, &received ) ) {
			con->open = false;
			return;
		}
		if( received == 0 )
			break;

		con->last_active = Sys_Milliseconds();

		recvbuf[ received ] = '\0';
		size_t advance = SV_Web_ParseHeaders( request, request->stream.header_buf );
		if( !advance ) {
			request->stream.header_buf_p += received;
			continue;
		}

		char * end = request->stream.header_buf + advance;
		size_t rem = ( request->stream.header_buf_p + received ) - advance;
		memmove( request->stream.header_buf, end, rem );
		request->stream.header_buf_p = rem;
		request->stream.header_length += advance;

		if( request->error ) {
			break;
		}
	}

	if( request->error || request->stream.header_done ) {
		SV_Web_MakeResponse( con );
	}
}

static void SV_Web_SendResponse( sv_http_connection_t * con ) {
	if( con->state != HTTP_CONN_STATE_SEND )
		return;

	sv_http_response_t * response = &con->response;
	sv_http_stream_t * stream = &response->stream;

	while( stream->header_buf_p < stream->header_length ) {
		const char * sendbuf = stream->header_buf + stream->header_buf_p;
		size_t sendbuf_size = stream->header_length - stream->header_buf_p;
		size_t sent;
		if( !TCPSend( con->socket, sendbuf, sendbuf_size, &sent ) ) {
			con->open = false;
			return;
		}
		if( sent == 0 )
			break;

		stream->header_buf_p += sent;
		con->last_active = Sys_Milliseconds();
	}

	if( stream->header_buf_p < stream->header_length ) {
		return;
	}

	while( response->filesent < response->filesize ) {
		size_t sent = SendFileChunk( con, response->file, response->filesent );
		if( sent == 0 )
			break;
		response->filesent += sent;
		con->last_active = Sys_Milliseconds();
	}

	if( response->filesent == response->filesize ) {
		con->open = false;
	}
}

static void SV_Web_Accept() {
	Socket client;
	NetAddress address;
	while( TCPAccept( web_server_socket, NonBlocking_Yes, &client, &address ) ) {
		if( SV_Web_ConnectionLimitReached( address ) ) {
			CloseSocket( client );
			continue;
		}

		sv_http_connection_t * con = SV_Web_AllocConnection();
		if( !con ) {
			CloseSocket( client );
			break;
		}

		con->socket = client;
		con->address = address;
		con->last_active = Sys_Milliseconds();
		con->open = true;
		con->state = HTTP_CONN_STATE_RECV;
	}
}

static void SV_Web_Frame() {
	TracyZoneScoped;

	Socket sockets[ MAX_INCOMING_HTTP_CONNECTIONS + 1 ];
	sv_http_connection_t * socket_to_connection[ ARRAY_COUNT( sockets ) ];
	size_t n = 0;

	sockets[ n ] = web_server_socket;
	n++;

	for( sv_http_connection_t & con : sv_http_connections ) {
		if( con.state == HTTP_CONN_STATE_NONE )
			continue;
		sockets[ n ] = con.socket;
		socket_to_connection[ n ] = &con;
		n++;
	}

	TempAllocator temp = web_server_arena.temp();
	WaitForSocketResult results[ ARRAY_COUNT( sockets ) ];
	WaitForSockets( &temp, sockets, n, HTTP_SERVER_SLEEP_TIME, WaitForSocketWriteable_Yes, results );

	if( results[ 0 ].readable ) {
		SV_Web_Accept();
	}

	for( size_t i = 1; i < n; i++ ) {
		if( results[ i ].readable ) {
			SV_Web_ReceiveRequest( socket_to_connection[ i ] );
		}
		if( results[ i ].writeable ) {
			SV_Web_SendResponse( socket_to_connection[ i ] );
		}
	}

	for( sv_http_connection_t & con : sv_http_connections ) {
		if( con.state == HTTP_CONN_STATE_NONE )
			continue;

		if( con.open ) {
			s64 timeout = con.state == HTTP_CONN_STATE_RECV ? INCOMING_HTTP_CONNECTION_RECV_TIMEOUT : INCOMING_HTTP_CONNECTION_SEND_TIMEOUT;
			if( Sys_Milliseconds() > con.last_active + timeout ) {
				ggprint( "{} {} timeout\n", con.last_active, Sys_Milliseconds() );
				con.open = false;
			}
		}

		if( !con.open ) {
			CloseSocket( con.socket );
			SV_Web_FreeConnection( &con );
		}
	}
}

static void SV_Web_ThreadProc( void * param ) {
#if TRACY_ENABLE
	tracy::SetThreadName( "Web server thread" );
#endif

	while( sv_http_running ) {
		SV_Web_Frame();
	}

	SV_Web_ShutdownConnections();
}

void SV_Web_Init() {
	sv_http_running = false;

	if( !StrEqual( sv_downloadurl->value, "" ) ) {
		return;
	}

	memset( sv_http_connections, 0, sizeof( sv_http_connections ) );
	sv_http_running = true;

	web_server_socket = NewTCPServer( sv_port->integer, NonBlocking_Yes );
	web_server_thread = NewThread( SV_Web_ThreadProc );
}

void SV_Web_Shutdown() {
	sv_http_running = false;
	JoinThread( web_server_thread );
	CloseSocket( web_server_socket );
}
