#ifndef APP_JSONFEED_H_
#define APP_JSONFEED_H_

#include <stdbool.h>
#include <stddef.h>

#include "JsonStream.h"

/* Nesting depth tracked; deeper containers are skipped whole. */
#define JSON_FEED_MAX_DEPTH 6
/* Longest object key kept, including the NUL; longer keys are truncated. */
#define JSON_FEED_KEY_MAX 24
/* Longest value text kept, including the NUL; longer values are truncated.
 * Must fit a SHA-256 in hex (64 chars, the OTA manifest's "sha256"). */
#define JSON_FEED_TEXT_MAX 80

/* One scalar value (string, number, true/false/null) and where it sits. For
 * {"hourly":{"time":[1,2]}} the second number arrives as section "hourly",
 * key "time", index 1. */
typedef struct {
	const char* section; /* key of the nearest enclosing object, "" at top level */
	const char* key;     /* member key; for array elements, the array's key */
	int index;           /* position in the enclosing array, -1 if not in one */
	int type;            /* StringJSONToken, NumberJSONToken, ... */
	const char* text;    /* NUL-terminated token text, strings without quotes */
} JsonValue;

typedef void (*JsonValueCallback)(const JsonValue* value, void* ctx);

typedef struct {
	bool is_object;
	bool expect_key;
	char key[JSON_FEED_KEY_MAX];        /* key this container sits under */
	char member_key[JSON_FEED_KEY_MAX]; /* objects: key of the member being read */
	int next_index;                     /* arrays: index of the next element */
} JsonFeedLevel;

/* Streaming JSON reader: feed it the response chunk by chunk (tokens split
 * across chunks are reassembled), and it calls back once per scalar value. */
typedef struct {
	JSONParser parser;
	JsonValueCallback callback;
	void* ctx;
	char text[JSON_FEED_TEXT_MAX];
	size_t text_len;
	JsonFeedLevel levels[JSON_FEED_MAX_DEPTH];
	int depth;
	int skipped_depth; /* containers nested beyond JSON_FEED_MAX_DEPTH */
} JsonFeed;

void json_feed_init(JsonFeed* feed, JsonValueCallback callback, void* ctx);
void json_feed_data(JsonFeed* feed, const char* data, int length);

#endif /* APP_JSONFEED_H_ */
