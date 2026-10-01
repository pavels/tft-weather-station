#include <string.h>

#include "JsonFeed.h"

static void copy_key(char* dest, const char* src) {
	strncpy(dest, src, JSON_FEED_KEY_MAX - 1);
	dest[JSON_FEED_KEY_MAX - 1] = '\0';
}

static JsonFeedLevel* top_level(JsonFeed* feed) {
	return feed->depth > 0 ? &feed->levels[feed->depth - 1] : NULL;
}

static const char* current_section(JsonFeed* feed) {
	for (int i = feed->depth - 1; i >= 0; i--) {
		if (feed->levels[i].is_object) return feed->levels[i].key;
	}
	return "";
}

/* A value in the current container is complete: an object now expects its
 * next member key. */
static void value_done(JsonFeed* feed) {
	JsonFeedLevel* top = top_level(feed);
	if (top && top->is_object) top->expect_key = true;
}

static void handle_token(JsonFeed* feed, int type) {
	JsonFeedLevel* top = top_level(feed);

	if (feed->skipped_depth > 0) {
		if (type == StartObjectJSONToken || type == StartArrayJSONToken) feed->skipped_depth++;
		if (type == EndObjectJSONToken || type == EndArrayJSONToken) {
			if (--feed->skipped_depth == 0) value_done(feed);
		}
		return;
	}

	if (type == EndObjectJSONToken || type == EndArrayJSONToken) {
		if (feed->depth > 0) feed->depth--;
		value_done(feed);
		return;
	}

	if (top && top->is_object && top->expect_key) {
		if (type == StringJSONToken) {
			copy_key(top->member_key, feed->text);
			top->expect_key = false;
		}
		return;
	}

	const char* key = "";
	int index = -1;
	if (top) {
		key = top->is_object ? top->member_key : top->key;
		if (!top->is_object) index = top->next_index++;
	}

	if (type == StartObjectJSONToken || type == StartArrayJSONToken) {
		if (feed->depth == JSON_FEED_MAX_DEPTH) {
			feed->skipped_depth = 1;
			return;
		}
		JsonFeedLevel* level = &feed->levels[feed->depth++];
		level->is_object = type == StartObjectJSONToken;
		level->expect_key = level->is_object;
		copy_key(level->key, key);
		level->member_key[0] = '\0';
		level->next_index = 0;
		return;
	}

	JsonValue value = {
		.section = current_section(feed),
		.key = key,
		.index = index,
		.type = type,
		.text = feed->text,
	};
	feed->callback(&value, feed->ctx);
	value_done(feed);
}

void json_feed_init(JsonFeed* feed, JsonValueCallback callback, void* ctx) {
	memset(feed, 0, sizeof(*feed));
	InitialiseJSONParser(&feed->parser);
	feed->callback = callback;
	feed->ctx = ctx;
}

void json_feed_data(JsonFeed* feed, const char* data, int length) {
	ProvideJSONInput(&feed->parser, data, length);

	while (true) {
		JSONToken token = NextJSONToken(&feed->parser);
		int type = JSONTokenType(token);
		if (type == OutOfDataJSONToken) return;
		if (type == ParseErrorJSONToken) {
			feed->text_len = 0;
			continue;
		}

		size_t len = token.end - token.start;
		size_t room = JSON_FEED_TEXT_MAX - 1 - feed->text_len;
		if (len > room) len = room;
		memcpy(feed->text + feed->text_len, token.start, len);
		feed->text_len += len;

		/* Split across chunks: the rest arrives with the next chunk. */
		if (IsJSONTokenPartial(token)) return;

		feed->text[feed->text_len] = '\0';
		handle_token(feed, type);
		feed->text_len = 0;
	}
}
