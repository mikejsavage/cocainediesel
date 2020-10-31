#include "qcommon/base.h"
#include "qcommon/qcommon.h"

#include "client/downloads.h"

#define CURL_STATICLIB
#include "curl/curl.h"

static CURLM * curl;

static void CheckEasyError( const char * func, CURLcode err ) {
	if( err != CURLE_OK ) {
		Com_Error( ERR_FATAL, "Curl error in %s: %s (%d)", func, curl_easy_strerror( err ), err );
	}
}

static void CheckMultiError( const char * func, CURLMcode err ) {
	if( err != CURLM_OK ) {
		Com_Error( ERR_FATAL, "Curl error in %s: %s (%d)", func, curl_multi_strerror( err ), err );
	}
}

void InitDownloads() {
	curl_global_init( CURL_GLOBAL_DEFAULT );
	curl = curl_multi_init();
	if( curl == NULL ) {
		Com_Error( ERR_FATAL, "Couldn't init curl" );
	}

	CheckMultiError( "curl_multi_setopt", curl_multi_setopt( curl, CURLMOPT_MAX_TOTAL_CONNECTIONS, 8l ) );
}

void ShutdownDownloads() {
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
	DownloadDataCallback data_callback = ( DownloadDataCallback ) user_data;
	size_t len = size * nmemb;

	return data_callback( data, len ) ? len : 0;
}

/*
 * the headers curl_slist needs to stay alive for the entire request and
 * there's no way to get it from a request object. we already have the done
 * callback in PRIVATE so now we need to do this
 *
 * https://curl-library.cool.haxx.narkive.com/RJBkHeNm/http-headers-free-and-multi lol
 */
struct CurlIsTrash {
	DownloadDoneCallback done_callback;
	curl_slist * headers;
};

void StartDownload( const char * url, DownloadDataCallback data_callback, DownloadDoneCallback done_callback, const char ** headers, size_t num_headers ) {
	CURL * request = curl_easy_init();
	if( request == NULL ) {
		Com_Error( ERR_FATAL, "curl_easy_init" );
	}

	CurlIsTrash * trash = ALLOC( sys_allocator, CurlIsTrash );
	trash->done_callback = done_callback;
	trash->headers = NULL;

	for( size_t i = 0; i < num_headers; i++ ) {
		trash->headers = curl_slist_append( trash->headers, headers[ i ] );
		if( trash->headers == NULL ) {
			Com_Error( ERR_FATAL, "curl_slist_append" );
		}
	}

	CheckedEasyOpt( request, CURLOPT_URL, url );
	CheckedEasyOpt( request, CURLOPT_HTTPHEADER, trash->headers );
	CheckedEasyOpt( request, CURLOPT_WRITEFUNCTION, CurlDataCallback );
	CheckedEasyOpt( request, CURLOPT_FOLLOWLOCATION, 1l );
	CheckedEasyOpt( request, CURLOPT_NOSIGNAL, 1l );
	CheckedEasyOpt( request, CURLOPT_CONNECTTIMEOUT, 10l );
	CheckedEasyOpt( request, CURLOPT_LOW_SPEED_TIME, 10l );
	CheckedEasyOpt( request, CURLOPT_LOW_SPEED_LIMIT, 10l );

	CheckedEasyOpt( request, CURLOPT_WRITEDATA, data_callback );
	CheckedEasyOpt( request, CURLOPT_PRIVATE, trash );

	CheckMultiError( "curl_multi_add_handle", curl_multi_add_handle( curl, request ) );
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

		CurlIsTrash * trash;
		CheckEasyError( "curl_easy_getinfo", curl_easy_getinfo( msg->easy_handle, CURLINFO_PRIVATE, &trash ) );

		long http_status;
		CheckEasyError( "curl_easy_getinfo", curl_easy_getinfo( msg->easy_handle, CURLINFO_RESPONSE_CODE, &http_status ) );

		if( msg->data.result == CURLE_OK && http_status / 100 == 2 ) {
			trash->done_callback( true, http_status );
		}
		else {
			trash->done_callback( false, http_status );
		}

		CheckMultiError( "curl_multi_remove_handle", curl_multi_remove_handle( curl, msg->easy_handle ) );
		curl_easy_cleanup( msg->easy_handle );
		curl_slist_free_all( trash->headers );
		FREE( sys_allocator, trash );
	}
}
