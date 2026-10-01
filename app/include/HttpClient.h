#ifndef APP_HTTPCLIENT_H_
#define APP_HTTPCLIENT_H_

typedef void (*HttpReceiveCallback)(const char *data, int length, void *ctx);

/* Plain HTTP/1.0 GET on port 80; the body of a 200 response is streamed to
 * `receive` chunk by chunk. Returns 1 on a 200 response, -1 otherwise
 * (connection failure or any other status, whose body is dropped). */
int http_get(const char *host, const char *path, HttpReceiveCallback receive, void *ctx);

#endif /* APP_HTTPCLIENT_H_ */
