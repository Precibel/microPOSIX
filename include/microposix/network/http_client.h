#ifndef MICROPOSIX_NETWORK_HTTP_CLIENT_H
#define MICROPOSIX_NETWORK_HTTP_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Maximum number of HTTP headers
#define MP_HTTP_MAX_HEADERS 16

// Maximum URL length
#define MP_HTTP_MAX_URL_LENGTH 256

// Maximum path length
#define MP_HTTP_MAX_PATH_LENGTH 128

// Maximum host length
#define MP_HTTP_MAX_HOST_LENGTH 64

// HTTP methods
typedef enum {
    MP_HTTP_METHOD_GET,
    MP_HTTP_METHOD_POST,
    MP_HTTP_METHOD_PUT,
    MP_HTTP_METHOD_DELETE,
    MP_HTTP_METHOD_HEAD,
    MP_HTTP_METHOD_PATCH,
    MP_HTTP_METHOD_OPTIONS
} mp_http_method_t;

// HTTP header structure
typedef struct {
    char *name;
    char *value;
} mp_http_header_t;

// HTTP request structure
typedef struct {
    char host[MP_HTTP_MAX_HOST_LENGTH];
    uint16_t port;
    char path[MP_HTTP_MAX_PATH_LENGTH];
    mp_http_method_t method;
    mp_http_header_t headers[MP_HTTP_MAX_HEADERS];
    int num_headers;
    uint8_t *body;
    size_t body_len;
    uint32_t timeout_ms;
    
    // Response
    uint8_t *response;
    size_t response_len;
    int status_code;
} mp_http_request_t;

// HTTP functions

/**
 * @brief Create an HTTP request
 * @param url URL to request
 * @return HTTP request object, or NULL on error
 */
mp_http_request_t *mp_http_request_create(const char *url);

/**
 * @brief Destroy an HTTP request
 * @param request HTTP request object
 */
void mp_http_request_destroy(mp_http_request_t *request);

/**
 * @brief Set HTTP method
 * @param request HTTP request object
 * @param method HTTP method
 */
void mp_http_request_set_method(mp_http_request_t *request, mp_http_method_t method);

/**
 * @brief Set HTTP path
 * @param request HTTP request object
 * @param path Path
 */
void mp_http_request_set_path(mp_http_request_t *request, const char *path);

/**
 * @brief Add HTTP header
 * @param request HTTP request object
 * @param name Header name
 * @param value Header value
 * @return 0 on success, -1 on error
 */
int mp_http_request_add_header(mp_http_request_t *request, const char *name, const char *value);

/**
 * @brief Set HTTP body
 * @param request HTTP request object
 * @param body Body data
 * @param len Body length
 * @return 0 on success, -1 on error
 */
int mp_http_request_set_body(mp_http_request_t *request, const uint8_t *body, size_t len);

/**
 * @brief Set HTTP timeout
 * @param request HTTP request object
 * @param timeout_ms Timeout in milliseconds
 */
void mp_http_request_set_timeout(mp_http_request_t *request, uint32_t timeout_ms);

/**
 * @brief Send HTTP request
 * @param request HTTP request object
 * @return 0 on success, -1 on error
 */
int mp_http_request_send(mp_http_request_t *request);

/**
 * @brief Parse URL
 * @param url URL to parse
 * @param host Output buffer for host
 * @param port Output buffer for port
 * @param path Output buffer for path
 * @return 0 on success, -1 on error
 */
int mp_http_parse_url(const char *url, char **host, uint16_t *port, char **path);

/**
 * @brief Get HTTP status code text
 * @param status_code Status code
 * @return Status code text
 */
const char *mp_http_status_code_to_str(int status_code);

/**
 * @brief Simple HTTP GET request
 * @param url URL to request
 * @param response Output buffer for response (must be freed by caller)
 * @param response_len Output buffer for response length
 * @return HTTP status code, or -1 on error
 */
int mp_http_get(const char *url, char **response, size_t *response_len);

/**
 * @brief Simple HTTP POST request
 * @param url URL to request
 * @param body POST body
 * @param response Output buffer for response (must be freed by caller)
 * @param response_len Output buffer for response length
 * @return HTTP status code, or -1 on error
 */
int mp_http_post(const char *url, const char *body, char **response, size_t *response_len);

#endif // MICROPOSIX_NETWORK_HTTP_CLIENT_H
