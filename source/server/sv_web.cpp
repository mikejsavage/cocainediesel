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
#include "qcommon/q_trie.h"
#include "qcommon/fs.h"
#include "qcommon/string.h"
#include "qcommon/threads.h"

#define MAX_INCOMING_HTTP_CONNECTIONS           48
#define MAX_INCOMING_HTTP_CONNECTIONS_PER_ADDR  3

#define INCOMING_HTTP_CONNECTION_RECV_TIMEOUT   5 // seconds
#define INCOMING_HTTP_CONNECTION_SEND_TIMEOUT   15 // seconds

#define HTTP_SERVER_SLEEP_TIME                  50 // milliseconds

enum http_query_method_t {
	HTTP_METHOD_BAD = -1,
	HTTP_METHOD_NONE = 0,
	HTTP_METHOD_GET  = 1,
	HTTP_METHOD_POST = 2,
	HTTP_METHOD_PUT  = 3,
	HTTP_METHOD_HEAD = 4,
};

enum http_response_code_t {
	HTTP_RESP_NONE = 0,
	HTTP_RESP_OK = 200,
	HTTP_RESP_BAD_REQUEST = 400,
	HTTP_RESP_FORBIDDEN = 403,
	HTTP_RESP_NOT_FOUND = 404,
	HTTP_RESP_REQUEST_TOO_LARGE = 413,
	HTTP_RESP_SERVICE_UNAVAILABLE = 503,
};

enum sv_http_connstate_t {
	HTTP_CONN_STATE_NONE,
	HTTP_CONN_STATE_RECV,
	HTTP_CONN_STATE_RESP,
	HTTP_CONN_STATE_SEND,
};

struct sv_http_stream_t {
	size_t header_length;
	char header_buf[256];
	size_t header_buf_p;
	bool header_done;

	size_t content_p;
	size_t content_length;
};

struct sv_http_request_t {
	http_query_method_t method;
	http_response_code_t error;
	sv_http_stream_t stream;

	char *method_str;
	char *resource;
	const char *query_string;
	char *http_ver;

	int clientNum;
	char *clientSession;
	netadr_t realAddr;

	bool got_start_line;
};

struct sv_http_response_t {
	http_response_code_t code;
	sv_http_stream_t stream;

	FILE * file;
	char * filename;
};

struct sv_http_connection_t {
	bool open;
	sv_http_connstate_t state;

	socket_t socket;
	netadr_t address;

	int64_t last_active;

	sv_http_request_t request;
	sv_http_response_t response;
};

struct http_game_client_t {
	int clientNum;
	char session[16];               // session id for HTTP requests
	netadr_t remoteAddress;
};

static bool sv_http_initialized = false;
static volatile bool sv_http_running = false;

static sv_http_connection_t sv_http_connections[MAX_INCOMING_HTTP_CONNECTIONS];

static socket_t sv_socket_http;
static socket_t sv_socket_http6;

static uint64_t sv_http_request_autoicr;

static trie_t *sv_http_clients = NULL;
static Mutex *sv_http_clients_mutex = NULL;

static Thread *sv_http_thread = NULL;
static void SV_Web_ThreadProc( void *param );

// ============================================================================

static void SV_Web_ResetStream( sv_http_stream_t *stream ) {
	stream->header_done = false;
	stream->header_length = 0;
	stream->header_buf_p = 0;
	stream->content_length = 0;
	stream->content_p = 0;
}

static void SV_Web_ResetRequest( sv_http_request_t *request ) {
	if( request->method_str ) {
		Mem_Free( request->method_str );
		request->method_str = NULL;
	}
	if( request->resource ) {
		Mem_Free( request->resource );
		request->resource = NULL;
	}
	if( request->http_ver ) {
		Mem_Free( request->http_ver );
		request->http_ver = NULL;
	}

	if( request->clientSession ) {
		Mem_Free( request->clientSession );
		request->clientSession = NULL;
	}

	request->query_string = "";
	SV_Web_ResetStream( &request->stream );

	NET_InitAddress( &request->realAddr, NA_NOTRANSMIT );

	request->got_start_line = false;
	request->error = HTTP_RESP_NONE;
	request->clientNum = -1;
}

static uint64_t SV_Web_GetNewRequestId() {
	return sv_http_request_autoicr++;
}

static void SV_Web_ResetResponse( sv_http_response_t *response ) {
	if( response->filename ) {
		FREE( sys_allocator, response->filename );
		response->filename = NULL;
	}
	if( response->file ) {
		fclose( response->file );
		response->file = NULL;
	}

	SV_Web_ResetStream( &response->stream );

	response->code = HTTP_RESP_NONE;
}

static sv_http_connection_t *SV_Web_AllocConnection() {
	for( sv_http_connection_t & con : sv_http_connections ) {
		if( con.state == HTTP_CONN_STATE_NONE ) {
			return &con;
		}
	}

	return NULL;
}

static void SV_Web_FreeConnection( sv_http_connection_t *con ) {
	SV_Web_ResetRequest( &con->request );
	SV_Web_ResetResponse( &con->response );

	con->state = HTTP_CONN_STATE_NONE;
}

static void SV_Web_InitConnections() {
	memset( sv_http_connections, 0, sizeof( sv_http_connections ) );
}

static void SV_Web_ShutdownConnections() {
	for( sv_http_connection_t & con : sv_http_connections ) {
		if( con.state == HTTP_CONN_STATE_NONE )
			continue;

		if( con.open ) {
			NET_CloseSocket( &con.socket );
			SV_Web_FreeConnection( &con );
		}
	}
}

bool SV_Web_AddGameClient( const char *session, int clientNum, const netadr_t *netAdr ) {
	http_game_client_t *client;
	trie_error_t trie_error;

	if( !sv_http_initialized ) {
		return false;
	}

	client = ( http_game_client_t * ) Mem_ZoneMalloc( sizeof( *client ) );
	if( !client ) {
		return false;
	}

	memcpy( client->session, session, HTTP_CLIENT_SESSION_SIZE );
	client->clientNum = clientNum;
	client->remoteAddress = *netAdr;

	Lock( sv_http_clients_mutex );
	trie_error = Trie_Insert( sv_http_clients, client->session, (void *)client );
	Unlock( sv_http_clients_mutex );

	if( trie_error != TRIE_OK ) {
		Mem_ZoneFree( client );
		return false;
	}

	return true;
}

void SV_Web_RemoveGameClient( const char *session ) {
	http_game_client_t *client;
	trie_error_t trie_error;

	if( !sv_http_initialized ) {
		return;
	}

	Lock( sv_http_clients_mutex );
	trie_error = Trie_Remove( sv_http_clients, session, (void **)&client );
	Unlock( sv_http_clients_mutex );

	if( trie_error != TRIE_OK ) {
		return;
	}

	Mem_ZoneFree( client );
}

static bool SV_Web_FindGameClientBySession( const char *session, int clientNum ) {
	http_game_client_t *client;
	trie_error_t trie_error;

	if( !session || !*session ) {
		return false;
	}
	if( clientNum < 0 || clientNum >= sv_maxclients->integer ) {
		return false;
	}

	Lock( sv_http_clients_mutex );
	trie_error = Trie_Find( sv_http_clients, session, TRIE_EXACT_MATCH, (void **)&client );
	Unlock( sv_http_clients_mutex );

	if( trie_error != TRIE_OK ) {
		return false;
	}
	if( client->clientNum != clientNum ) {
		return false;
	}

	return true;
}

/*
* SV_Web_FindGameClientByAddress
*
* Performs lookup for game client in trie by network address. Terribly inefficient.
*/
static bool SV_Web_FindGameClientByAddress( const netadr_t *netadr ) {
	trie_dump_t *dump;

	Lock( sv_http_clients_mutex );
	Trie_Dump( sv_http_clients, "", TRIE_DUMP_VALUES, &dump );
	Unlock( sv_http_clients_mutex );

	bool valid_address = false;
	for( unsigned int i = 0; i < dump->size; i++ ) {
		http_game_client_t *const a = (http_game_client_t *) dump->key_value_vector[i].value;
		if( NET_CompareBaseAddress( netadr, &a->remoteAddress ) ) {
			valid_address = true;
			break;
		}
	}

	Trie_FreeDump( dump );

	return valid_address;
}

static bool SV_Web_ConnectionLimitReached( const netadr_t *addr ) {
	int n = 0;
	for( const sv_http_connection_t & con : sv_http_connections ) {
		if( con.state != HTTP_CONN_STATE_NONE && NET_CompareAddress( addr, &con.address ) ) {
			n++;
			if( n >= MAX_INCOMING_HTTP_CONNECTIONS_PER_ADDR ) {
				return true;
			}
		}
	}

	return false;
}

static int SV_Web_Get( sv_http_connection_t *con, void *recvbuf, size_t recvbuf_size ) {
	int read = NET_Get( &con->socket, NULL, recvbuf, recvbuf_size - 1 );
	if( read < 0 ) {
		con->open = false;
		Com_DPrintf( "HTTP connection recv error from %s\n", NET_AddressToString( &con->address ) );
	}
	return read;
}

static int SV_Web_Send( sv_http_connection_t *con, const void *sendbuf, size_t sendbuf_size ) {
	int sent = NET_Send( &con->socket, sendbuf, sendbuf_size, &con->address );
	if( sent < 0 ) {
		Com_DPrintf( "HTTP transmission error to %s\n", NET_AddressToString( &con->address ) );
		con->open = false;
	}
	return sent;
}

static s64 SendFileChunk( sv_http_connection_t * con, FILE * f ) {
	char buf[ 8192 ];
	size_t r = fread( buf, 1, sizeof( buf ), f );
	if( r < sizeof( buf ) && ferror( f ) ) {
		return -1;
	}

	size_t sent = 0;
	while( sent < r ) {
		int w = SV_Web_Send( con, buf + sent, r - sent );
		if( w < 0 )
			return -1;
		sent += w;
	}

	return sent;
}

static void SV_Web_ParseStartLine( sv_http_request_t *request, char *line ) {
	char *ptr;
	char *token, *delim;

	ptr = line;

	token = ptr;
	ptr = strchr( token, ' ' );
	if( !ptr ) {
		request->error = HTTP_RESP_BAD_REQUEST;
		return;
	}
	*ptr = '\0';

	if( !Q_stricmp( token, "GET" ) ) {
		request->method = HTTP_METHOD_GET;
	} else if( !Q_stricmp( token, "POST" ) ) {
		request->method = HTTP_METHOD_POST;
	} else if( !Q_stricmp( token, "PUT" ) ) {
		request->method = HTTP_METHOD_PUT;
	} else if( !Q_stricmp( token, "HEAD" ) ) {
		request->method = HTTP_METHOD_HEAD;
	} else {
		request->error = HTTP_RESP_BAD_REQUEST;
	}

	request->method_str = ZoneCopyString( token );

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

	request->resource = ZoneCopyString( *token ? token : "/" );
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
	request->http_ver = ZoneCopyString( token );

	// check for HTTP/1.1 and greater
	if( strncmp( request->http_ver, "HTTP/", 5 ) ) {
		request->error = HTTP_RESP_BAD_REQUEST;
	} else if( (int)( atof( request->http_ver + 5 ) * 10 ) < 11 ) {
		request->error = HTTP_RESP_BAD_REQUEST;
	}
}

static void SV_Web_AnalyzeHeader( sv_http_request_t *request, const char *key, const char *value ) {
	sv_http_stream_t *stream = &request->stream;

	//
	// store valuable information for quicker access
	if( !Q_stricmp( key, "Content-Length" ) ) {
		stream->content_length = atoi( value );
		if( stream->content_length != 0 ) {
			request->error = HTTP_RESP_REQUEST_TOO_LARGE;
		}
	} else if( !Q_stricmp( key, "Host" ) ) {
		// valid HTTP 1.1 request must contain Host header
		if( !value || !*value ) {
			request->error = HTTP_RESP_BAD_REQUEST;
		}
	} else if( !Q_stricmp( key, "X-Client" ) ) {
		request->clientNum = atoi( value );
	} else if( !Q_stricmp( key, "X-Session" ) ) {
		request->clientSession = ZoneCopyString( value );
	}
}

/*
* SV_Web_ParseHeaderLine
*
* Parses and splits the header line into key-value pair
*/
static void SV_Web_ParseHeaderLine( sv_http_request_t *request, char *line ) {
	char *value;
	size_t offset;
	const char *colon;
	const char *key;

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

static size_t SV_Web_ParseHeaders( sv_http_request_t *request, char *data ) {
	char *line, *p;

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

static void SV_Web_ReceiveRequest( socket_t *socket, sv_http_connection_t *con ) {
	int ret = 0;
	char *recvbuf;
	size_t recvbuf_size;
	sv_http_request_t *request = &con->request;
	size_t total_received = 0;

	if( con->state != HTTP_CONN_STATE_RECV ) {
		return;
	}

	while( !request->stream.header_done && sv_http_running ) {
		char *end;
		size_t rem;
		size_t advance;

		recvbuf = request->stream.header_buf + request->stream.header_buf_p;
		recvbuf_size = sizeof( request->stream.header_buf ) - request->stream.header_buf_p;
		if( recvbuf_size <= 1 ) {
			request->error = HTTP_RESP_BAD_REQUEST;
			break;
		}

		ret = SV_Web_Get( con, recvbuf, recvbuf_size - 1 );
		if( ret <= 0 ) {
			if( total_received == 0 ) {
				// no data on the socket after select() call,
				// the connection has probably been closed on the other end
				con->open = false;
				return;
			}
			break;
		}

		total_received += ret;

		recvbuf[ret] = '\0';
		advance = SV_Web_ParseHeaders( request, request->stream.header_buf );
		if( !advance ) {
			request->stream.header_buf_p += ret;
			continue;
		}

		end = request->stream.header_buf + advance;
		rem = ( request->stream.header_buf_p + ret ) - advance;
		memmove( request->stream.header_buf, end, rem );
		request->stream.header_buf_p = rem;
		request->stream.header_length += advance;

		// request must come from a connected client with a valid session id
		if( is_public_build && !request->error && request->stream.header_done ) {
			if( !SV_Web_FindGameClientBySession( request->clientSession, request->clientNum ) ) {
				request->error = HTTP_RESP_FORBIDDEN;
			}
		}

		if( request->error ) {
			break;
		}
	}

	if( !sv_http_running ) {
		return;
	}

	if( total_received > 0 ) {
		con->last_active = Sys_Milliseconds();
	}

	if( request->error ) {
		con->state = HTTP_CONN_STATE_RESP;
	} else if( request->stream.header_done && request->stream.content_p >= request->stream.content_length ) {
		// yay, fully got the request
		con->state = HTTP_CONN_STATE_RESP;
	}

	if( ret == -1 ) {
		con->open = false;
		Com_DPrintf( "HTTP connection error from %s\n", NET_AddressToString( &con->address ) );
	}
}

// ============================================================================

static const char *SV_Web_ResponseCodeMessage( http_response_code_t code ) {
	switch( code ) {
		case HTTP_RESP_OK: return "OK";
		case HTTP_RESP_BAD_REQUEST: return "Bad Request";
		case HTTP_RESP_FORBIDDEN: return "Forbidden";
		case HTTP_RESP_NOT_FOUND: return "Not Found";
		case HTTP_RESP_REQUEST_TOO_LARGE: return "Request Entity Too Large";
		case HTTP_RESP_SERVICE_UNAVAILABLE: return "Service unavailable";
		default: return "Unknown Error";
	}
}

static void SV_Web_RouteRequest( const sv_http_request_t *request, sv_http_response_t *response, size_t *content_length ) {
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

	Span< const char > ext = FileExtension( request->resource );
	if( ext != ".bsp.zst" && !SV_IsDemoDownloadRequest( request->resource ) ) {
		response->code = HTTP_RESP_FORBIDDEN;
		return;
	}

	response->file = OpenFile( sys_allocator, request->resource, "rb" );
	if( response->file == NULL ) {
		response->code = HTTP_RESP_NOT_FOUND;
		return;
	}

	*content_length = FileSize( response->file );
	response->code = HTTP_RESP_OK;
}

static void SV_Web_RespondToQuery( sv_http_connection_t *con ) {
	size_t content_length = 0;
	sv_http_request_t *request = &con->request;
	sv_http_response_t *response = &con->response;
	sv_http_stream_t *resp_stream = &response->stream;

	if( request->error != 0 ) {
		response->code = request->error;
	}
	else {
		SV_Web_RouteRequest( request, response, &content_length );

		if( response->file ) {
			Com_Printf( "HTTP serving file '%s' to '%s'\n", response->filename, NET_AddressToString( &con->address ) );
		}

		if( request->method == HTTP_METHOD_HEAD && response->file != NULL ) {
			fclose( response->file );
			response->file = NULL;
		}
	}

	con->state = HTTP_CONN_STATE_SEND;

	String< sizeof( resp_stream->header_buf ) - 1 > headers;
	headers.append( "{} {} {}\r\n", request->http_ver, response->code, SV_Web_ResponseCodeMessage( response->code ) );
	headers.append( "Server: " APPLICATION "\r\n" );

	if( response->code == HTTP_RESP_OK ) {
		headers.append( "Content-Length: {}\r\n", content_length );
		headers.append( "Content-Disposition: attachment; filename=\"{}\"\r\n", FileName( response->filename ) );
		headers += "\r\n";
	}
	else {
		String< 64 > error( "{} {}\n", response->code, SV_Web_ResponseCodeMessage( response->code ) );;

		headers.append( "Content-Type: text/plain\r\n" );
		headers.append( "Content-Length: {}\r\n", error.length() );
		headers += "\r\n";
		headers += error;
	}

	ggformat( resp_stream->header_buf, sizeof( resp_stream->header_buf ), "{}", headers );

	resp_stream->header_length = headers.length();
	resp_stream->content_length = content_length;
}

static size_t SV_Web_SendResponse( sv_http_connection_t *con ) {
	size_t total_sent = 0;
	sv_http_response_t *response = &con->response;
	sv_http_stream_t *stream = &response->stream;

	while( !stream->header_done && sv_http_running ) {
		const char * sendbuf = stream->header_buf + stream->header_buf_p;
		size_t sendbuf_size = stream->header_length - stream->header_buf_p;

		int sent = SV_Web_Send( con, sendbuf, sendbuf_size );
		if( sent <= 0 ) {
			break;
		}

		stream->header_buf_p += sent;
		if( stream->header_buf_p >= stream->header_length ) {
			stream->header_done = true;
		}

		total_sent += sent;
	}

	while( stream->content_p < stream->content_length && sv_http_running ) {
		int sent = SendFileChunk( con, response->file );
		if( sent <= 0 )
			break;
		stream->content_p += sent;
		total_sent += sent;
	}

	if( total_sent > 0 ) {
		con->last_active = Sys_Milliseconds();
	}

	if( stream->header_done && stream->content_p >= stream->content_length ) {
		con->open = false;
	}

	return total_sent;
}

static void SV_Web_WriteResponse( socket_t *socket, sv_http_connection_t *con ) {
	if( !sv_http_running ) {
		return;
	}

	switch( con->state ) {
		case HTTP_CONN_STATE_RECV:
			break;
		case HTTP_CONN_STATE_RESP:
			SV_Web_RespondToQuery( con );
			if( con->state != HTTP_CONN_STATE_SEND ) {
				break;
			}
		// fallthrough
		case HTTP_CONN_STATE_SEND:
			SV_Web_SendResponse( con );
			break;
		default:
			Com_DPrintf( "Bad connection state %i\n", con->state );
			con->open = false;
			break;
	}
}

static void SV_Web_InitSocket( const char *addrstr, netadrtype_t adrtype, socket_t *socket ) {
	netadr_t address;

	address.type = NA_NOTRANSMIT;

	NET_StringToAddress( addrstr, &address );
	NET_SetAddressPort( &address, sv_port->integer );

	if( address.type == adrtype ) {
		if( !NET_OpenSocket( socket, SOCKET_TCP, &address, true ) ) {
			Com_Printf( "Couldn't start web server: Couldn't open TCP socket: %s\n", NET_ErrorString() );
		} else if( !NET_Listen( socket ) ) {
			Com_Printf( "Couldn't start web server: Couldn't listen to TCP socket: %s\n", NET_ErrorString() );
			NET_CloseSocket( socket );
		} else {
			Com_Printf( "Web server started on %s\n", NET_AddressToString( &address ) );
		}
	}
}

static void SV_Web_Listen( socket_t *socket ) {
	int ret;
	socket_t newsocket = { };
	netadr_t newaddress;
	sv_http_connection_t *con;

	// accept new connections
	while( ( ret = NET_Accept( socket, &newsocket, &newaddress ) ) ) {
		bool block;

		if( ret == -1 ) {
			Com_Printf( "NET_Accept: Error: %s\n", NET_ErrorString() );
			continue;
		}

		block = false;

		if( !NET_IsLocalAddress( &newaddress ) ) {
			// only accept connections from connected clients
			block = !SV_Web_FindGameClientByAddress( &newaddress );
			if( !block ) {
				block = SV_Web_ConnectionLimitReached( &newaddress );
			}
		}

		if( block ) {
			Com_DPrintf( "HTTP connection refused for %s\n", NET_AddressToString( &newaddress ) );
			NET_CloseSocket( &newsocket );
			continue;
		}

		Com_DPrintf( "HTTP connection accepted from %s\n", NET_AddressToString( &newaddress ) );
		con = SV_Web_AllocConnection();
		if( !con ) {
			break;
		}
		con->socket = newsocket;
		con->address = newaddress;
		con->last_active = Sys_Milliseconds();
		con->open = true;
		con->state = HTTP_CONN_STATE_RECV;
	}
}

void SV_Web_Init() {
	sv_http_initialized = false;
	sv_http_running = false;
	sv_http_request_autoicr = 1;

	SV_Web_InitConnections();

	if( strlen( sv_downloadurl->string ) > 0 ) {
		return;
	}

	SV_Web_InitSocket( sv_ip->string, NA_IP, &sv_socket_http );
	SV_Web_InitSocket( sv_ip6->string, NA_IP6, &sv_socket_http6 );

	sv_http_initialized = ( sv_socket_http.address.type == NA_IP || sv_socket_http6.address.type == NA_IP6 );

	if( !sv_http_initialized ) {
		return;
	}

	sv_http_running = true;

	Trie_Create( TRIE_CASE_SENSITIVE, &sv_http_clients );
	sv_http_clients_mutex = NewMutex();
	sv_http_thread = NewThread( SV_Web_ThreadProc );
}

static void SV_Web_Frame() {
	socket_t *sockets[MAX_INCOMING_HTTP_CONNECTIONS + 1];
	void *connections[MAX_INCOMING_HTTP_CONNECTIONS];

	if( !sv_http_initialized ) {
		return;
	}

	// accept new connections
	if( sv_socket_http.address.type == NA_IP ) {
		SV_Web_Listen( &sv_socket_http );
	}
	if( sv_socket_http6.address.type == NA_IP6 ) {
		SV_Web_Listen( &sv_socket_http6 );
	}

	// handle incoming data
	int num_sockets = 0;
	for( sv_http_connection_t & con : sv_http_connections ) {
		if( con.state == HTTP_CONN_STATE_NONE )
			continue;
		sockets[num_sockets] = &con.socket;
		connections[num_sockets] = &con;
		num_sockets++;
	}
	sockets[num_sockets] = NULL;

	if( num_sockets != 0 ) {
		NET_Monitor( HTTP_SERVER_SLEEP_TIME, sockets,
					 ( void ( * )( socket_t *, void* ) )SV_Web_ReceiveRequest,
					 ( void ( * )( socket_t *, void* ) )SV_Web_WriteResponse,
					 NULL, connections );
	} else {
		// sleep on network sockets if got nothing else to do
		if( sv_socket_http.address.type == NA_IP ) {
			sockets[num_sockets++] = &sv_socket_http;
		}
		if( sv_socket_http6.address.type == NA_IP6 ) {
			sockets[num_sockets++] = &sv_socket_http6;
		}
		sockets[num_sockets] = NULL;
		NET_Sleep( HTTP_SERVER_SLEEP_TIME, sockets );
	}

	// close dead connections
	if( !sv_http_running ) {
		return;
	}
	for( sv_http_connection_t & con : sv_http_connections ) {
		if( con.state == HTTP_CONN_STATE_NONE )
			continue;

		if( con.open ) {
			unsigned int timeout = con.state == HTTP_CONN_STATE_RECV ? INCOMING_HTTP_CONNECTION_RECV_TIMEOUT : INCOMING_HTTP_CONNECTION_SEND_TIMEOUT;
			if( Sys_Milliseconds() > con.last_active + timeout * 1000 ) {
				con.open = false;
				Com_DPrintf( "HTTP connection timeout from %s\n", NET_AddressToString( &con.address ) );
			}
		}

		if( !con.open ) {
			NET_CloseSocket( &con.socket );
			SV_Web_FreeConnection( &con );
		}
	}
}

bool SV_Web_Running() {
	return sv_http_running;
}

static void SV_Web_ThreadProc( void *param ) {
	while( sv_http_running ) {
		SV_Web_Frame();
	}

	SV_Web_ShutdownConnections();
}

void SV_Web_Shutdown() {
	if( !sv_http_initialized ) {
		return;
	}

	sv_http_running = false;
	JoinThread( sv_http_thread );

	NET_CloseSocket( &sv_socket_http );
	NET_CloseSocket( &sv_socket_http6 );

	sv_http_initialized = false;
}
