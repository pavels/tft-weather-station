#include "HttpClient.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

#include "http_parser.h"

/* A read that gets no data for this long ends the request, s. */
#define HTTP_RECV_TIMEOUT_S 5

typedef struct {
	HttpReceiveCallback receive;
	void *ctx;
} http_get_context_t;

static int on_body(http_parser *parser, const char *at, size_t length)
{
	http_get_context_t *ctx = parser->data;
	if (parser->status_code == 200) ctx->receive(at, (int)length, ctx->ctx);
	return 0;
}

static int http_socket_write(int s, const char *content)
{
	if (write(s, content, strlen(content)) < 0) {
		return -1;
	}
	return 1;
}

int http_get(const char *host, const char *path, HttpReceiveCallback receive, void *ctx)
{
	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};
	struct addrinfo *res;
	int s, r;
	char recv_buf[128];

	int err = getaddrinfo(host, "80", &hints, &res);
	if (err != 0 || res == NULL) {
		return -1;
	}

	s = socket(res->ai_family, res->ai_socktype, 0);
	if (s < 0) {
		freeaddrinfo(res);
		return -1;
	}

	if (connect(s, res->ai_addr, res->ai_addrlen) != 0) {
		close(s);
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);

	struct timeval receiving_timeout = { .tv_sec = HTTP_RECV_TIMEOUT_S, .tv_usec = 0 };
	if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
		close(s);
		return -1;
	}

	http_socket_write(s, "GET ");
	http_socket_write(s, path);
	http_socket_write(s, " HTTP/1.0\r\n");
	http_socket_write(s, "Host: ");
	http_socket_write(s, host);
	http_socket_write(s, "\r\n");
	http_socket_write(s, "User-Agent: tft_weather_station/1.0\r\n");
	http_socket_write(s, "\r\n");

	http_get_context_t get_ctx = { .receive = receive, .ctx = ctx };

	http_parser_settings settings;
	http_parser_settings_init(&settings);
	settings.on_body = on_body;

	http_parser parser;
	http_parser_init(&parser, HTTP_RESPONSE);
	parser.data = &get_ctx;

	do {
		r = read(s, recv_buf, sizeof(recv_buf));
		if (r > 0) {
			http_parser_execute(&parser, &settings, recv_buf, r);
		}
	} while (r > 0);

	/* Signal EOF so http_parser can close out an HTTP/1.0 body that's
	 * delimited by connection close rather than Content-Length/chunked. */
	http_parser_execute(&parser, &settings, NULL, 0);

	close(s);

	return parser.status_code == 200 ? 1 : -1;
}
