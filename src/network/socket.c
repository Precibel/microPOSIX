/**
 * microPOSIX Socket API for ESP32
 * 
 * This file implements a BSD-style socket API for TCP/IP networking.
 * It provides a consistent interface across different platforms.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "microposix/network/socket.h"
#include "microposix/debug/log.h"
#include "microposix/kernel/thread.h"

// Platform detection
#if defined(MICROPOSIX_PLATFORM_ESP32)
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_log.h"
#define SOCKET_BACKEND_ESP32 1
#elif defined(MICROPOSIX_PLATFORM_POSIX)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#define SOCKET_BACKEND_POSIX 1
#endif

// Socket state
typedef struct mp_socket {
    int fd;                     // Platform-specific file descriptor
    mp_socket_domain_t domain;  // Socket domain (AF_INET, AF_INET6)
    mp_socket_type_t type;      // Socket type (SOCK_STREAM, SOCK_DGRAM)
    mp_socket_protocol_t protocol; // Protocol
    bool blocking;              // Whether socket is blocking
    bool connected;             // Whether socket is connected
} mp_socket_t;

// Socket table
static mp_socket_t sockets[MP_SOCKET_MAX_SOCKETS] = {0};

// Mutex for thread-safe access
static bool socket_mutex_locked = false;

/**
 * @brief Lock socket mutex
 */
static void mp_socket_lock(void) {
    while (socket_mutex_locked) {
        mp_thread_sleep(1);
    }
    socket_mutex_locked = true;
}

/**
 * @brief Unlock socket mutex
 */
static void mp_socket_unlock(void) {
    socket_mutex_locked = false;
}

/**
 * @brief Find free socket slot
 */
static int mp_socket_find_free_slot(void) {
    for (int i = 0; i < MP_SOCKET_MAX_SOCKETS; i++) {
        if (sockets[i].fd == MP_SOCKET_INVALID_FD) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Create a new socket
 */
mp_socket_t mp_socket(int domain, int type, int protocol) {
    mp_socket_lock();
    
    int slot = mp_socket_find_free_slot();
    if (slot == -1) {
        MP_LOGE("SOCKET", "No free socket slots");
        mp_socket_unlock();
        return MP_SOCKET_INVALID;
    }
    
    // Create platform-specific socket
    int fd = -1;
    
    #if defined(SOCKET_BACKEND_ESP32)
    fd = lwip_socket(mp_socket_domain_to_lwip(domain), 
                     mp_socket_type_to_lwip(type), 
                     mp_socket_protocol_to_lwip(protocol));
    #elif defined(SOCKET_BACKEND_POSIX)
    fd = socket(domain, type, protocol);
    #else
    MP_LOGE("SOCKET", "No socket backend for this platform");
    mp_socket_unlock();
    return MP_SOCKET_INVALID;
    #endif
    
    if (fd < 0) {
        MP_LOGE("SOCKET", "Failed to create socket: %d", fd);
        mp_socket_unlock();
        return MP_SOCKET_INVALID;
    }
    
    // Store socket information
    sockets[slot].fd = fd;
    sockets[slot].domain = domain;
    sockets[slot].type = type;
    sockets[slot].protocol = protocol;
    sockets[slot].blocking = true;
    sockets[slot].connected = false;
    
    MP_LOGI("SOCKET", "Socket created: %d (domain=%d, type=%d, protocol=%d)", 
            slot, domain, type, protocol);
    
    mp_socket_unlock();
    return (mp_socket_t)slot;
}

/**
 * @brief Close a socket
 */
int mp_close(mp_socket_t sockfd) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        MP_LOGE("SOCKET", "Invalid socket: %d", sockfd);
        return -1;
    }
    
    mp_socket_lock();
    
    #if defined(SOCKET_BACKEND_ESP32)
    lwip_close(sockets[sockfd].fd);
    #elif defined(SOCKET_BACKEND_POSIX)
    close(sockets[sockfd].fd);
    #endif
    
    sockets[sockfd].fd = MP_SOCKET_INVALID_FD;
    sockets[sockfd].connected = false;
    
    mp_socket_unlock();
    MP_LOGI("SOCKET", "Socket %d closed", sockfd);
    return 0;
}

/**
 * @brief Bind a socket to an address
 */
int mp_bind(mp_socket_t sockfd, const struct mp_sockaddr *addr, mp_socklen_t addrlen) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    struct sockaddr_storage storage;
    mp_sockaddr_to_lwip(addr, &storage, addrlen);
    return lwip_bind(sockets[sockfd].fd, (struct sockaddr *)&storage, addrlen);
    #elif defined(SOCKET_BACKEND_POSIX)
    return bind(sockets[sockfd].fd, (struct sockaddr *)addr, addrlen);
    #else
    return -1;
    #endif
}

/**
 * @brief Connect a socket to an address
 */
int mp_connect(mp_socket_t sockfd, const struct mp_sockaddr *addr, mp_socklen_t addrlen) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    struct sockaddr_storage storage;
    mp_sockaddr_to_lwip(addr, &storage, addrlen);
    int result = lwip_connect(sockets[sockfd].fd, (struct sockaddr *)&storage, addrlen);
    if (result == 0) {
        sockets[sockfd].connected = true;
    }
    return result;
    #elif defined(SOCKET_BACKEND_POSIX)
    int result = connect(sockets[sockfd].fd, (struct sockaddr *)addr, addrlen);
    if (result == 0) {
        sockets[sockfd].connected = true;
    }
    return result;
    #else
    return -1;
    #endif
}

/**
 * @brief Listen for connections on a socket
 */
int mp_listen(mp_socket_t sockfd, int backlog) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    return lwip_listen(sockets[sockfd].fd, backlog);
    #elif defined(SOCKET_BACKEND_POSIX)
    return listen(sockets[sockfd].fd, backlog);
    #else
    return -1;
    #endif
}

/**
 * @brief Accept a connection on a socket
 */
mp_socket_t mp_accept(mp_socket_t sockfd, struct mp_sockaddr *addr, mp_socklen_t *addrlen) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return MP_SOCKET_INVALID;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    struct sockaddr_storage storage;
    mp_socklen_t storage_len = sizeof(storage);
    int fd = lwip_accept(sockets[sockfd].fd, (struct sockaddr *)&storage, &storage_len);
    if (fd < 0) {
        return MP_SOCKET_INVALID;
    }
    
    // Find free slot
    int slot = mp_socket_find_free_slot();
    if (slot == -1) {
        lwip_close(fd);
        return MP_SOCKET_INVALID;
    }
    
    // Store new socket
    sockets[slot].fd = fd;
    sockets[slot].domain = sockets[sockfd].domain;
    sockets[slot].type = sockets[sockfd].type;
    sockets[slot].protocol = sockets[sockfd].protocol;
    sockets[slot].blocking = true;
    sockets[slot].connected = true;
    
    // Convert address if requested
    if (addr != NULL && addrlen != NULL) {
        mp_sockaddr_from_lwip((struct sockaddr *)&storage, storage_len, addr, addrlen);
    }
    
    return (mp_socket_t)slot;
    
    #elif defined(SOCKET_BACKEND_POSIX)
    struct sockaddr_storage storage;
    socklen_t storage_len = sizeof(storage);
    int fd = accept(sockets[sockfd].fd, (struct sockaddr *)&storage, &storage_len);
    if (fd < 0) {
        return MP_SOCKET_INVALID;
    }
    
    // Find free slot
    int slot = mp_socket_find_free_slot();
    if (slot == -1) {
        close(fd);
        return MP_SOCKET_INVALID;
    }
    
    // Store new socket
    sockets[slot].fd = fd;
    sockets[slot].domain = sockets[sockfd].domain;
    sockets[slot].type = sockets[sockfd].type;
    sockets[slot].protocol = sockets[sockfd].protocol;
    sockets[slot].blocking = true;
    sockets[slot].connected = true;
    
    // Convert address if requested
    if (addr != NULL && addrlen != NULL) {
        *addrlen = storage_len;
        memcpy(addr, &storage, *addrlen);
    }
    
    return (mp_socket_t)slot;
    
    #else
    return MP_SOCKET_INVALID;
    #endif
}

/**
 * @brief Send data on a socket
 */
ssize_t mp_send(mp_socket_t sockfd, const void *buf, size_t len, int flags) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    return lwip_send(sockets[sockfd].fd, buf, len, flags);
    #elif defined(SOCKET_BACKEND_POSIX)
    return send(sockets[sockfd].fd, buf, len, flags);
    #else
    return -1;
    #endif
}

/**
 * @brief Receive data from a socket
 */
ssize_t mp_recv(mp_socket_t sockfd, void *buf, size_t len, int flags) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    return lwip_recv(sockets[sockfd].fd, buf, len, flags);
    #elif defined(SOCKET_BACKEND_POSIX)
    return recv(sockets[sockfd].fd, buf, len, flags);
    #else
    return -1;
    #endif
}

/**
 * @brief Send data to a specific address (for connectionless sockets)
 */
ssize_t mp_sendto(mp_socket_t sockfd, const void *buf, size_t len, int flags,
                   const struct mp_sockaddr *dest_addr, mp_socklen_t addrlen) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    struct sockaddr_storage storage;
    mp_sockaddr_to_lwip(dest_addr, &storage, addrlen);
    return lwip_sendto(sockets[sockfd].fd, buf, len, flags,
                      (struct sockaddr *)&storage, addrlen);
    #elif defined(SOCKET_BACKEND_POSIX)
    return sendto(sockets[sockfd].fd, buf, len, flags,
                  (struct sockaddr *)dest_addr, addrlen);
    #else
    return -1;
    #endif
}

/**
 * @brief Receive data from a socket and get sender address
 */
ssize_t mp_recvfrom(mp_socket_t sockfd, void *buf, size_t len, int flags,
                     struct mp_sockaddr *src_addr, mp_socklen_t *addrlen) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    struct sockaddr_storage storage;
    mp_socklen_t storage_len = sizeof(storage);
    ssize_t result = lwip_recvfrom(sockets[sockfd].fd, buf, len, flags,
                                   (struct sockaddr *)&storage, &storage_len);
    if (result >= 0 && src_addr != NULL && addrlen != NULL) {
        mp_sockaddr_from_lwip((struct sockaddr *)&storage, storage_len, src_addr, addrlen);
    }
    return result;
    #elif defined(SOCKET_BACKEND_POSIX)
    struct sockaddr_storage storage;
    socklen_t storage_len = sizeof(storage);
    ssize_t result = recvfrom(sockets[sockfd].fd, buf, len, flags,
                              (struct sockaddr *)&storage, &storage_len);
    if (result >= 0 && src_addr != NULL && addrlen != NULL) {
        *addrlen = storage_len;
        memcpy(src_addr, &storage, *addrlen);
    }
    return result;
    #else
    return -1;
    #endif
}

/**
 * @brief Set socket options
 */
int mp_setsockopt(mp_socket_t sockfd, int level, int optname, const void *optval, mp_socklen_t optlen) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    return lwip_setsockopt(sockets[sockfd].fd, level, optname, optval, optlen);
    #elif defined(SOCKET_BACKEND_POSIX)
    return setsockopt(sockets[sockfd].fd, level, optname, optval, optlen);
    #else
    return -1;
    #endif
}

/**
 * @brief Get socket options
 */
int mp_getsockopt(mp_socket_t sockfd, int level, int optname, void *optval, mp_socklen_t *optlen) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    return lwip_getsockopt(sockets[sockfd].fd, level, optname, optval, optlen);
    #elif defined(SOCKET_BACKEND_POSIX)
    return getsockopt(sockets[sockfd].fd, level, optname, optval, optlen);
    #else
    return -1;
    #endif
}

/**
 * @brief Set socket to blocking or non-blocking mode
 */
int mp_socket_set_blocking(mp_socket_t sockfd, bool blocking) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    sockets[sockfd].blocking = blocking;
    
    #if defined(SOCKET_BACKEND_ESP32)
    int flags = lwip_fcntl(sockets[sockfd].fd, F_GETFL, 0);
    if (blocking) {
        lwip_fcntl(sockets[sockfd].fd, F_SETFL, flags & ~O_NONBLOCK);
    } else {
        lwip_fcntl(sockets[sockfd].fd, F_SETFL, flags | O_NONBLOCK);
    }
    return 0;
    #elif defined(SOCKET_BACKEND_POSIX)
    int flags = fcntl(sockets[sockfd].fd, F_GETFL, 0);
    if (blocking) {
        fcntl(sockets[sockfd].fd, F_SETFL, flags & ~O_NONBLOCK);
    } else {
        fcntl(sockets[sockfd].fd, F_SETFL, flags | O_NONBLOCK);
    }
    return 0;
    #else
    return -1;
    #endif
}

/**
 * @brief Get socket error
 */
int mp_socket_get_error(mp_socket_t sockfd) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    int error;
    mp_socklen_t len = sizeof(error);
    lwip_getsockopt(sockets[sockfd].fd, SOL_SOCKET, SO_ERROR, &error, &len);
    return error;
    #elif defined(SOCKET_BACKEND_POSIX)
    int error;
    socklen_t len = sizeof(error);
    getsockopt(sockets[sockfd].fd, SOL_SOCKET, SO_ERROR, &error, &len);
    return error;
    #else
    return -1;
    #endif
}

/**
 * @brief Check if data is available to read
 */
bool mp_socket_available(mp_socket_t sockfd) {
    if (sockfd >= MP_SOCKET_MAX_SOCKETS || sockets[sockfd].fd == MP_SOCKET_INVALID_FD) {
        return false;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    int available;
    mp_socklen_t len = sizeof(available);
    if (lwip_getsockopt(sockets[sockfd].fd, SOL_SOCKET, SO_RCVBUF, &available, &len) == 0) {
        return available > 0;
    }
    return false;
    #elif defined(SOCKET_BACKEND_POSIX)
    int available;
    if (ioctl(sockets[sockfd].fd, FIONREAD, &available) == 0) {
        return available > 0;
    }
    return false;
    #else
    return false;
    #endif
}

/**
 * @brief Get host by name (DNS lookup)
 */
int mp_gethostbyname(const char *name, struct mp_hostent *hostent) {
    if (name == NULL || hostent == NULL) {
        return -1;
    }
    
    #if defined(SOCKET_BACKEND_ESP32)
    struct hostent *he = lwip_gethostbyname(name);
    if (he == NULL) {
        return -1;
    }
    
    hostent->h_name = (char *)he->h_name;
    hostent->h_aliases = he->h_aliases;
    hostent->h_addrtype = he->h_addrtype;
    hostent->h_length = he->h_length;
    hostent->h_addr_list = he->h_addr_list;
    
    return 0;
    #elif defined(SOCKET_BACKEND_POSIX)
    struct hostent *he = gethostbyname(name);
    if (he == NULL) {
        return -1;
    }
    
    memcpy(hostent, he, sizeof(struct mp_hostent));
    return 0;
    #else
    return -1;
    #endif
}

// ============================================================================
// Platform-Specific Conversions
// ============================================================================

#if defined(SOCKET_BACKEND_ESP32)

/**
 * @brief Convert microPOSIX socket domain to lwIP
 */
static int mp_socket_domain_to_lwip(int domain) {
    switch (domain) {
        case MP_AF_INET: return AF_INET;
        case MP_AF_INET6: return AF_INET6;
        case MP_AF_UNIX: return AF_UNIX;
        default: return AF_INET;
    }
}

/**
 * @brief Convert microPOSIX socket type to lwIP
 */
static int mp_socket_type_to_lwip(int type) {
    switch (type) {
        case MP_SOCK_STREAM: return SOCK_STREAM;
        case MP_SOCK_DGRAM: return SOCK_DGRAM;
        case MP_SOCK_RAW: return SOCK_RAW;
        default: return SOCK_STREAM;
    }
}

/**
 * @brief Convert microPOSIX protocol to lwIP
 */
static int mp_socket_protocol_to_lwip(int protocol) {
    switch (protocol) {
        case MP_IPPROTO_TCP: return IPPROTO_TCP;
        case MP_IPPROTO_UDP: return IPPROTO_UDP;
        case MP_IPPROTO_ICMP: return IPPROTO_ICMP;
        default: return 0;
    }
}

/**
 * @brief Convert microPOSIX sockaddr to lwIP sockaddr_storage
 */
static void mp_sockaddr_to_lwip(const struct mp_sockaddr *src, 
                                struct sockaddr_storage *dest, 
                                mp_socklen_t addrlen) {
    if (src == NULL || dest == NULL) {
        return;
    }
    
    switch (src->sa_family) {
        case MP_AF_INET: {
            struct sockaddr_in *dest_in = (struct sockaddr_in *)dest;
            const struct mp_sockaddr_in *src_in = (const struct mp_sockaddr_in *)src;
            dest_in->sin_family = AF_INET;
            dest_in->sin_port = src_in->sin_port;
            dest_in->sin_addr.s_addr = src_in->sin_addr.s_addr;
            break;
        }
        case MP_AF_INET6: {
            struct sockaddr_in6 *dest_in6 = (struct sockaddr_in6 *)dest;
            const struct mp_sockaddr_in6 *src_in6 = (const struct mp_sockaddr_in6 *)src;
            dest_in6->sin6_family = AF_INET6;
            dest_in6->sin6_port = src_in6->sin6_port;
            memcpy(&dest_in6->sin6_addr, &src_in6->sin6_addr, sizeof(struct in6_addr));
            break;
        }
        default:
            memset(dest, 0, addrlen);
            break;
    }
}

/**
 * @brief Convert lwIP sockaddr to microPOSIX sockaddr
 */
static void mp_sockaddr_from_lwip(const struct sockaddr *src, 
                                  socklen_t src_len,
                                  struct mp_sockaddr *dest,
                                  mp_socklen_t *dest_len) {
    if (src == NULL || dest == NULL || dest_len == NULL) {
        return;
    }
    
    switch (src->sa_family) {
        case AF_INET: {
            struct mp_sockaddr_in *dest_in = (struct mp_sockaddr_in *)dest;
            const struct sockaddr_in *src_in = (const struct sockaddr_in *)src;
            dest_in->sin_family = MP_AF_INET;
            dest_in->sin_port = src_in->sin_port;
            dest_in->sin_addr.s_addr = src_in->sin_addr.s_addr;
            *dest_len = sizeof(struct mp_sockaddr_in);
            break;
        }
        case AF_INET6: {
            struct mp_sockaddr_in6 *dest_in6 = (struct mp_sockaddr_in6 *)dest;
            const struct sockaddr_in6 *src_in6 = (const struct sockaddr_in6 *)src;
            dest_in6->sin6_family = MP_AF_INET6;
            dest_in6->sin6_port = src_in6->sin6_port;
            memcpy(&dest_in6->sin6_addr, &src_in6->sin6_addr, sizeof(struct in6_addr));
            *dest_len = sizeof(struct mp_sockaddr_in6);
            break;
        }
        default:
            *dest_len = 0;
            break;
    }
}

#endif // SOCKET_BACKEND_ESP32
