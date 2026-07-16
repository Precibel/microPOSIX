/**
 * microPOSIX HTTP Client for ESP32
 * 
 * This file implements a simple HTTP client for making HTTP requests.
 * It provides GET, POST, PUT, DELETE methods with support for headers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include "microposix/network/http_client.h"
#include "microposix/network/socket.h"
#include "microposix/debug/log.h"
#include "microposix/kernel/thread.h"

// Default HTTP port
#define HTTP_PORT 80
#define HTTPS_PORT 443

// HTTP request timeout (in milliseconds)
#define HTTP_REQUEST_TIMEOUT 10000

// HTTP response buffer size
#define HTTP_RESPONSE_BUFFER_SIZE 2048

/**
 * @brief Create an HTTP request
 */
mp_http_request_t *mp_http_request_create(const char *url) {
    if (url == NULL) {
        return NULL;
    }
    
    // Parse URL
    mp_http_request_t *request = malloc(sizeof(mp_http_request_t));
    if (request == NULL) {
        return NULL;
    }
    
    memset(request, 0, sizeof(mp_http_request_t));
    
    // Parse URL
    if (mp_http_parse_url(url, &request->host, &request->port, &request->path) != 0) {
        free(request);
        return NULL;
    }
    
    // Set default values
    request->method = MP_HTTP_METHOD_GET;
    request->port = (request->port == 0) ? HTTP_PORT : request->port;
    request->timeout_ms = HTTP_REQUEST_TIMEOUT;
    
    return request;
}

/**
 * @brief Destroy an HTTP request
 */
void mp_http_request_destroy(mp_http_request_t *request) {
    if (request == NULL) {
        return;
    }
    
    // Free headers
    for (int i = 0; i < request->num_headers; i++) {
        free(request->headers[i].name);
        free(request->headers[i].value);
    }
    
    // Free body
    free(request->body);
    
    // Free response
    free(request->response);
    
    free(request);
}

/**
 * @brief Set HTTP method
 */
void mp_http_request_set_method(mp_http_request_t *request, mp_http_method_t method) {
    if (request == NULL) {
        return;
    }
    request->method = method;
}

/**
 * @brief Set HTTP path
 */
void mp_http_request_set_path(mp_http_request_t *request, const char *path) {
    if (request == NULL || path == NULL) {
        return;
    }
    strncpy(request->path, path, sizeof(request->path) - 1);
}

/**
 * @brief Add HTTP header
 */
int mp_http_request_add_header(mp_http_request_t *request, const char *name, const char *value) {
    if (request == NULL || name == NULL || value == NULL) {
        return -1;
    }
    
    if (request->num_headers >= MP_HTTP_MAX_HEADERS) {
        return -1;
    }
    
    request->headers[request->num_headers].name = strdup(name);
    request->headers[request->num_headers].value = strdup(value);
    
    if (request->headers[request->num_headers].name == NULL || 
        request->headers[request->num_headers].value == NULL) {
        free(request->headers[request->num_headers].name);
        free(request->headers[request->num_headers].value);
        return -1;
    }
    
    request->num_headers++;
    return 0;
}

/**
 * @brief Set HTTP body
 */
int mp_http_request_set_body(mp_http_request_t *request, const uint8_t *body, size_t len) {
    if (request == NULL) {
        return -1;
    }
    
    // Free existing body
    free(request->body);
    
    if (body != NULL && len > 0) {
        request->body = malloc(len);
        if (request->body == NULL) {
            return -1;
        }
        memcpy(request->body, body, len);
        request->body_len = len;
    } else {
        request->body = NULL;
        request->body_len = 0;
    }
    
    return 0;
}

/**
 * @brief Set HTTP timeout
 */
void mp_http_request_set_timeout(mp_http_request_t *request, uint32_t timeout_ms) {
    if (request == NULL) {
        return;
    }
    request->timeout_ms = timeout_ms;
}

/**
 * @brief Send HTTP request
 */
int mp_http_request_send(mp_http_request_t *request) {
    if (request == NULL) {
        return -1;
    }
    
    // Free existing response
    free(request->response);
    request->response = NULL;
    request->response_len = 0;
    request->status_code = 0;
    
    // Create socket
    mp_socket_t sockfd = mp_socket(MP_AF_INET, MP_SOCK_STREAM, MP_IPPROTO_TCP);
    if (sockfd == MP_SOCKET_INVALID) {
        MP_LOGE("HTTP", "Failed to create socket");
        return -1;
    }
    
    // Set non-blocking mode
    mp_socket_set_blocking(sockfd, false);
    
    // Resolve hostname
    struct mp_hostent hostent;
    if (mp_gethostbyname(request->host, &hostent) != 0) {
        MP_LOGE("HTTP", "Failed to resolve hostname: %s", request->host);
        mp_close(sockfd);
        return -1;
    }
    
    // Connect to server
    struct mp_sockaddr_in addr;
    mp_sockaddr_in_init(&addr, *(uint32_t *)hostent.h_addr_list[0], request->port);
    
    int result = mp_connect(sockfd, (struct mp_sockaddr *)&addr, sizeof(addr));
    if (result != 0) {
        MP_LOGE("HTTP", "Failed to connect to %s:%d", request->host, request->port);
        mp_close(sockfd);
        return -1;
    }
    
    // Wait for connection to complete
    uint32_t start_time = mp_clock_get_monotonic_ms();
    while (!mp_socket_available(sockfd)) {
        if (mp_clock_get_monotonic_ms() - start_time > request->timeout_ms) {
            MP_LOGE("HTTP", "Connection timeout");
            mp_close(sockfd);
            return -1;
        }
        mp_thread_sleep(10);
    }
    
    // Check if connection was successful
    int error = mp_socket_get_error(sockfd);
    if (error != 0) {
        MP_LOGE("HTTP", "Connection error: %d", error);
        mp_close(sockfd);
        return -1;
    }
    
    // Set blocking mode for sending/receiving
    mp_socket_set_blocking(sockfd, true);
    
    // Build HTTP request
    char request_line[256];
    switch (request->method) {
        case MP_HTTP_METHOD_GET:
            snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.1\r\n", request->path);
            break;
        case MP_HTTP_METHOD_POST:
            snprintf(request_line, sizeof(request_line), "POST %s HTTP/1.1\r\n", request->path);
            break;
        case MP_HTTP_METHOD_PUT:
            snprintf(request_line, sizeof(request_line), "PUT %s HTTP/1.1\r\n", request->path);
            break;
        case MP_HTTP_METHOD_DELETE:
            snprintf(request_line, sizeof(request_line), "DELETE %s HTTP/1.1\r\n", request->path);
            break;
        case MP_HTTP_METHOD_HEAD:
            snprintf(request_line, sizeof(request_line), "HEAD %s HTTP/1.1\r\n", request->path);
            break;
        default:
            snprintf(request_line, sizeof(request_line), "GET %s HTTP/1.1\r\n", request->path);
            break;
    }
    
    // Send request line
    if (mp_send(sockfd, request_line, strlen(request_line), 0) < 0) {
        MP_LOGE("HTTP", "Failed to send request line");
        mp_close(sockfd);
        return -1;
    }
    
    // Send headers
    char header_line[256];
    snprintf(header_line, sizeof(header_line), "Host: %s\r\n", request->host);
    if (mp_send(sockfd, header_line, strlen(header_line), 0) < 0) {
        MP_LOGE("HTTP", "Failed to send Host header");
        mp_close(sockfd);
        return -1;
    }
    
    // Send custom headers
    for (int i = 0; i < request->num_headers; i++) {
        snprintf(header_line, sizeof(header_line), "%s: %s\r\n",
                request->headers[i].name, request->headers[i].value);
        if (mp_send(sockfd, header_line, strlen(header_line), 0) < 0) {
            MP_LOGE("HTTP", "Failed to send header: %s", request->headers[i].name);
            mp_close(sockfd);
            return -1;
        }
    }
    
    // Send Content-Length if there's a body
    if (request->body_len > 0) {
        snprintf(header_line, sizeof(header_line), "Content-Length: %d\r\n", request->body_len);
        if (mp_send(sockfd, header_line, strlen(header_line), 0) < 0) {
            MP_LOGE("HTTP", "Failed to send Content-Length header");
            mp_close(sockfd);
            return -1;
        }
    }
    
    // End of headers
    if (mp_send(sockfd, "\r\n", 2, 0) < 0) {
        MP_LOGE("HTTP", "Failed to send end of headers");
        mp_close(sockfd);
        return -1;
    }
    
    // Send body if present
    if (request->body_len > 0) {
        if (mp_send(sockfd, request->body, request->body_len, 0) < 0) {
            MP_LOGE("HTTP", "Failed to send body");
            mp_close(sockfd);
            return -1;
        }
    }
    
    // Receive response
    char buffer[HTTP_RESPONSE_BUFFER_SIZE];
    size_t total_received = 0;
    bool headers_received = false;
    
    start_time = mp_clock_get_monotonic_ms();
    while (1) {
        // Check for timeout
        if (mp_clock_get_monotonic_ms() - start_time > request->timeout_ms) {
            MP_LOGE("HTTP", "Response timeout");
            mp_close(sockfd);
            return -1;
        }
        
        // Receive data
        ssize_t received = mp_recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (received < 0) {
            // Check if it's a non-blocking error
            if (!mp_socket_available(sockfd)) {
                mp_thread_sleep(10);
                continue;
            }
            MP_LOGE("HTTP", "Failed to receive data");
            mp_close(sockfd);
            return -1;
        }
        
        if (received == 0) {
            // Connection closed
            break;
        }
        
        buffer[received] = '\0';
        
        // If we haven't received headers yet, parse status line
        if (!headers_received) {
            // Look for HTTP status line
            if (strncmp(buffer, "HTTP/", 5) == 0) {
                // Parse status code
                char *status_line = strtok(buffer, "\r\n");
                if (status_line != NULL) {
                    // Find the status code (second token)
                    char *token = strtok(status_line, " ");
                    if (token != NULL) {
                        token = strtok(NULL, " ");
                        if (token != NULL) {
                            request->status_code = atoi(token);
                        }
                    }
                }
                headers_received = true;
            }
        }
        
        // Append to response
        char *new_response = realloc(request->response, total_received + received + 1);
        if (new_response == NULL) {
            MP_LOGE("HTTP", "Failed to allocate memory for response");
            mp_close(sockfd);
            return -1;
        }
        
        request->response = new_response;
        memcpy(request->response + total_received, buffer, received);
        total_received += received;
        request->response_len = total_received;
        
        // Check for end of headers (double CRLF)
        if (!headers_received && strstr(buffer, "\r\n\r\n") != NULL) {
            headers_received = true;
        }
    }
    
    // Close socket
    mp_close(sockfd);
    
    return 0;
}

/**
 * @brief Parse URL
 */
int mp_http_parse_url(const char *url, char **host, uint16_t *port, char **path) {
    if (url == NULL) {
        return -1;
    }
    
    // Check for protocol
    const char *protocol_end = strstr(url, "://");
    const char *start;
    
    if (protocol_end != NULL) {
        start = protocol_end + 3;
    } else {
        start = url;
    }
    
    // Find host end (either : or /)
    const char *host_end = strchr(start, ':');
    const char *path_start = strchr(start, '/');
    
    if (host_end == NULL && path_start == NULL) {
        // Only host
        *host = strdup(start);
        *port = 0;
        *path = strdup("/");
    } else if (host_end != NULL && (path_start == NULL || host_end < path_start)) {
        // Host with port
        size_t host_len = host_end - start;
        *host = malloc(host_len + 1);
        if (*host == NULL) {
            return -1;
        }
        strncpy(*host, start, host_len);
        (*host)[host_len] = '\0';
        
        // Parse port
        *port = atoi(host_end + 1);
        
        // Parse path
        if (path_start != NULL) {
            *path = strdup(path_start);
        } else {
            *path = strdup("/");
        }
    } else if (path_start != NULL) {
        // Host with path
        size_t host_len = path_start - start;
        *host = malloc(host_len + 1);
        if (*host == NULL) {
            return -1;
        }
        strncpy(*host, start, host_len);
        (*host)[host_len] = '\0';
        
        *port = 0;
        *path = strdup(path_start);
    } else {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Get HTTP status code text
 */
const char *mp_http_status_code_to_str(int status_code) {
    switch (status_code) {
        case 100: return "Continue";
        case 101: return "Switching Protocols";
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        default: return "Unknown";
    }
}

/**
 * @brief Simple HTTP GET request
 */
int mp_http_get(const char *url, char **response, size_t *response_len) {
    mp_http_request_t *request = mp_http_request_create(url);
    if (request == NULL) {
        return -1;
    }
    
    mp_http_request_set_method(request, MP_HTTP_METHOD_GET);
    
    int result = mp_http_request_send(request);
    if (result == 0) {
        *response = strdup((char *)request->response);
        *response_len = request->response_len;
    } else {
        *response = NULL;
        *response_len = 0;
    }
    
    int status = request->status_code;
    mp_http_request_destroy(request);
    
    return status;
}

/**
 * @brief Simple HTTP POST request
 */
int mp_http_post(const char *url, const char *body, char **response, size_t *response_len) {
    mp_http_request_t *request = mp_http_request_create(url);
    if (request == NULL) {
        return -1;
    }
    
    mp_http_request_set_method(request, MP_HTTP_METHOD_POST);
    mp_http_request_set_body(request, (const uint8_t *)body, strlen(body));
    mp_http_request_add_header(request, "Content-Type", "application/x-www-form-urlencoded");
    
    int result = mp_http_request_send(request);
    if (result == 0) {
        *response = strdup((char *)request->response);
        *response_len = request->response_len;
    } else {
        *response = NULL;
        *response_len = 0;
    }
    
    int status = request->status_code;
    mp_http_request_destroy(request);
    
    return status;
}
