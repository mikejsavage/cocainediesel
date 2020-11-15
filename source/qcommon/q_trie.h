#pragma once

/* Forward declaration of trie structures (layout hidden) */
struct trie_t;
struct trie_node_t;

/* Definitions of externalized types */

enum trie_error_t {
	TRIE_OK = 0,
	TRIE_DUPLICATE_KEY,
	TRIE_KEY_NOT_FOUND,
	TRIE_INVALID_ARGUMENT
};

enum trie_find_mode_t {
	TRIE_PREFIX_MATCH,
	TRIE_EXACT_MATCH
};

enum trie_dump_what_t {
	TRIE_DUMP_KEYS = 1,
	TRIE_DUMP_VALUES = 2,
	TRIE_DUMP_BOTH = TRIE_DUMP_KEYS | TRIE_DUMP_VALUES
};

enum trie_casing_t {
	TRIE_CASE_SENSITIVE,
	TRIE_CASE_INSENSITIVE
};

struct trie_key_value_t {
	const char *key;
	void *value;
};

struct trie_dump_t {
	unsigned int size;
	trie_dump_what_t what;
	trie_key_value_t *key_value_vector;
};

/* Trie life-cycle functions */

trie_error_t Trie_Create(
	trie_casing_t casing,       // case sensitive or not
	trie_t **trie        // output parameter, the created trie
	);

trie_error_t Trie_Destroy(
	trie_t *trie         // the trie to destroy
	);

trie_error_t Trie_Clear(
	trie_t *trie         // the trie to clear from keys and values
	);

trie_error_t Trie_GetSize(
	trie_t *trie,
	unsigned int *size          // output parameter, size of trie
	);

/* Key/data insertion and removal */

trie_error_t Trie_Insert(
	trie_t *trie,
	const char *key,            // key to insert
	void *data                  // data to insert
	);

trie_error_t Trie_Remove(
	trie_t *trie,
	const char *key,            // key to match
	void **data                 // output parameter, data of removed node
	);

trie_error_t Trie_Replace(
	trie_t *trie,
	const char *key,            // key to match
	void *data_new,             // data to set
	void **data_old             // output parameter, replaced data
	);

trie_error_t Trie_Find(
	const trie_t *trie,
	const char *key,            // key to match
	trie_find_mode_t mode,      // mode (exact or prefix only)
	void **data                 // output parameter, data of node found
	);

trie_error_t Trie_FindIf(
	const trie_t *trie,
	const char *key,            // key to match
	trie_find_mode_t mode,      // mode (exact or prefix only)
	int ( *predicate )( void *value, const void *cookie ),     // predicate function to be true
	const void *cookie,                   // the cookie passed to predicate
	void **data                 // output parameter, data of node found
	);

trie_error_t Trie_NoOfMatches(
	const trie_t *trie,
	const char *prefix,         // key prefix to match
	unsigned int *matches       // output parameter, number of matches
	);

trie_error_t Trie_NoOfMatchesIf(
	const trie_t *trie,
	const char *prefix,         // key prefix to match
	int ( *predicate )( void *value, const void *cookie ),     // predicate function to be true
	const void *cookie,               // the cookie passed to predicate
	unsigned int *matches       // output parameter, number of matches
	);

/* Dump by prefix */

trie_error_t Trie_Dump(
	const trie_t *trie,
	const char *prefix,             // prefix to match
	trie_dump_what_t what,          // what to dump
	trie_dump_t **dump       // output parameter, deallocate with Trie_FreeDump
	);

trie_error_t Trie_DumpIf(
	const trie_t *trie,
	const char *prefix,             // prefix to match
	trie_dump_what_t what,          // what to dump
	int ( *predicate )( void *value, const void *cookie ),     // predicate function to be true
	const void *cookie,                   // the cookie passed to predicate
	trie_dump_t **dump       // output parameter, deallocate with Trie_FreeDump
	);

trie_error_t Trie_FreeDump(
	trie_dump_t *dump        // allocated by Trie_Dump or Trie_DumpIf
	);
