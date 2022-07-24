#include "qcommon/base.h"
#include "qcommon/qcommon.h"
#include "qcommon/array.h"

#include "client/downloads.h"

#define CURL_STATICLIB
#include "curl/curl.h"

static CURLM * curl;
static CURL * request;

/*
 * the headers curl_slist needs to stay alive for the entire request and
 * there's no way to get it from a request object so put them in here too
 *
 * https://curl-library.cool.haxx.narkive.com/RJBkHeNm/http-headers-free-and-multi lol
 */
struct CurlRequestContext {
	NonRAIIDynamicArray< u8 > data;
	CurlDoneCallback done_callback;
	curl_slist * headers;
};

static void CheckEasyError( const char * func, CURLcode err ) {
	if( err != CURLE_OK ) {
		Fatal( "Curl error in %s: %s (%d)", func, curl_easy_strerror( err ), err );
	}
}

static void CheckMultiError( const char * func, CURLMcode err ) {
	if( err != CURLM_OK ) {
		Fatal( "Curl error in %s: %s (%d)", func, curl_multi_strerror( err ), err );
	}
}

void InitDownloads() {
	TracyZoneScoped;

	curl_global_init( CURL_GLOBAL_DEFAULT );
	curl = curl_multi_init();
	if( curl == NULL ) {
		Fatal( "Couldn't init curl" );
	}

	CheckMultiError( "curl_multi_setopt", curl_multi_setopt( curl, CURLMOPT_MAX_TOTAL_CONNECTIONS, 8l ) );

	request = NULL;
}

void ShutdownDownloads() {
	CancelDownload();
	curl_multi_cleanup( curl );
	curl_global_cleanup();
}

template< typename S, typename T >
struct SameType { static constexpr bool value = false; };
template< typename T >
struct SameType< T, T > { static constexpr bool value = true; };

template< typename T >
static void CheckedEasyOpt( CURL * request, CURLoption opt, T val ) {
	STATIC_ASSERT( ( !SameType< T, int >::value || SameType< int, long >::value ) );
	CheckEasyError( "curl_easy_setopt", curl_easy_setopt( request, opt, val ) );
}

static size_t CurlDataCallback( char * data, size_t size, size_t nmemb, void * user_data ) {
	CurlRequestContext * context = ( CurlRequestContext * ) user_data;
	size_t len = size * nmemb;

	constexpr size_t DOWNLOAD_MAX_SIZE = 50 * 1000 * 1000; // 50MB
	if( context->data.size() + len > DOWNLOAD_MAX_SIZE ) {
		return 0;
	}

	size_t old_size = context->data.extend( len );
	memcpy( context->data.ptr() + old_size, data, len );

	return len;
}

void StartDownload( const char * url, CurlDoneCallback done_callback, const char ** headers, size_t num_headers ) {
	request = curl_easy_init();
	if( request == NULL ) {
		Fatal( "curl_easy_init" );
	}

	CurlRequestContext * context = ALLOC( sys_allocator, CurlRequestContext );
	context->data.init( sys_allocator );
	context->done_callback = done_callback;
	context->headers = NULL;

	for( size_t i = 0; i < num_headers; i++ ) {
		context->headers = curl_slist_append( context->headers, headers[ i ] );
		if( context->headers == NULL ) {
			Fatal( "curl_slist_append" );
		}
	}

	CheckedEasyOpt( request, CURLOPT_URL, url );
	CheckedEasyOpt( request, CURLOPT_HTTPHEADER, context->headers );
	CheckedEasyOpt( request, CURLOPT_WRITEFUNCTION, CurlDataCallback );
	CheckedEasyOpt( request, CURLOPT_FOLLOWLOCATION, 1l );
	CheckedEasyOpt( request, CURLOPT_NOSIGNAL, 1l );
	CheckedEasyOpt( request, CURLOPT_CONNECTTIMEOUT, 10l );
	CheckedEasyOpt( request, CURLOPT_LOW_SPEED_TIME, 10l );
	CheckedEasyOpt( request, CURLOPT_LOW_SPEED_LIMIT, 10l );

	CheckedEasyOpt( request, CURLOPT_WRITEDATA, context );
	CheckedEasyOpt( request, CURLOPT_PRIVATE, context );

	CheckMultiError( "curl_multi_add_handle", curl_multi_add_handle( curl, request ) );
}

void CancelDownload() {
	if( request == NULL )
		return;

	CurlRequestContext * context;
	CheckEasyError( "curl_easy_getinfo", curl_easy_getinfo( request, CURLINFO_PRIVATE, &context ) );

	CheckMultiError( "curl_multi_remove_handle", curl_multi_remove_handle( curl, request ) );
	curl_easy_cleanup( request );
	request = NULL;

	curl_slist_free_all( context->headers );
	context->data.shutdown();
	FREE( sys_allocator, context );
}

void PumpDownloads() {
	while( true ) {
		int dont_care;
		CURLMcode ok = curl_multi_perform( curl, &dont_care );
		if( ok == CURLM_OK )
			break;
		CheckMultiError( "curl_multi_perform", ok );
	}

	while( true ) {
		int dont_care;
		CURLMsg * msg = curl_multi_info_read( curl, &dont_care );
		if( msg == NULL )
			break;

		assert( msg->msg == CURLMSG_DONE );

		CurlRequestContext * context;
		CheckEasyError( "curl_easy_getinfo", curl_easy_getinfo( msg->easy_handle, CURLINFO_PRIVATE, &context ) );

		long http_status;
		CheckEasyError( "curl_easy_getinfo", curl_easy_getinfo( msg->easy_handle, CURLINFO_RESPONSE_CODE, &http_status ) );

		if( msg->data.result == CURLE_OK && http_status / 100 == 2 ) {
			context->done_callback( http_status, context->data.span() );
		}
		else {
			context->done_callback( http_status, Span< const u8 >() );
		}

		CancelDownload();
	}
}
