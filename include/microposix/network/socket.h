#ifndef MICROPOSIX_NETWORK_SOCKET_H
#define MICROPOSIX_NETWORK_SOCKET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Maximum number of sockets
#define MP_SOCKET_MAX_SOCKETS 16

// Invalid socket descriptor
#define MP_SOCKET_INVALID -1
#define MP_SOCKET_INVALID_FD -1

// Socket types
typedef int mp_socket_t;
typedef size_t mp_socklen_t;
typedef int ssize_t;

// Address families
#define MP_AF_UNSPEC 0
#define MP_AF_INET 2
#define MP_AF_INET6 10
#define MP_AF_UNIX 1

// Socket types
#define MP_SOCK_STREAM 1
#define MP_SOCK_DGRAM 2
#define MP_SOCK_RAW 3

// Protocol families
#define MP_PF_INET MP_AF_INET
#define MP_PF_INET6 MP_AF_INET6
#define MP_PF_UNIX MP_AF_UNIX

// Protocol numbers
#define MP_IPPROTO_IP 0
#define MP_IPPROTO_TCP 6
#define MP_IPPROTO_UDP 17
#define MP_IPPROTO_ICMP 1
#define MP_IPPROTO_RAW 255

// Socket options
#define MP_SOL_SOCKET 0xFFFF
#define MP_SO_REUSEADDR 0x0004
#define MP_SO_KEEPALIVE 0x0008
#define MP_SO_BROADCAST 0x0020
#define MP_SO_LINGER 0x0080
#define MP_SO_RCVBUF 0x1002
#define MP_SO_SNDBUF 0x1003
#define MP_SO_ERROR 0x1007

// Socket flags
#define MP_MSG_PEEK 0x02
#define MP_MSG_DONTWAIT 0x40
#define MP_MSG_MORE 0x8000

// Address structure
struct mp_sockaddr {
    uint16_t sa_family;  // Address family
    char sa_data[14];    // Protocol-specific address
};

// IPv4 address structure
struct mp_sockaddr_in {
    uint16_t sin_family;     // AF_INET
    uint16_t sin_port;       // Port number
    struct mp_in_addr {
        uint32_t s_addr;      // IP address in network byte order
    } sin_addr;
    char sin_zero[8];        // Padding
};

// IPv6 address structure
struct mp_sockaddr_in6 {
    uint16_t sin6_family;     // AF_INET6
    uint16_t sin6_port;       // Port number
    struct mp_in6_addr {
        uint8_t s6_addr[16];   // IPv6 address
    } sin6_addr;
    uint32_t sin6_flowinfo;   // Flow information
    uint32_t sin6_scope_id;   // Scope ID
};

// Host entry structure
struct mp_hostent {
    char *h_name;            // Official name of host
    char **h_aliases;         // Alias list
    int h_addrtype;          // Host address type
    int h_length;            // Length of address
    char **h_addr_list;      // List of addresses
};

// Socket domain type
typedef enum {
    MP_SOCKET_DOMAIN_UNSPEC = 0,
    MP_SOCKET_DOMAIN_INET = 1,
    MP_SOCKET_DOMAIN_INET6 = 2,
    MP_SOCKET_DOMAIN_UNIX = 3
} mp_socket_domain_t;

// Socket type type
typedef enum {
    MP_SOCKET_TYPE_STREAM = 1,
    MP_SOCKET_TYPE_DGRAM = 2,
    MP_SOCKET_TYPE_RAW = 3
} mp_socket_type_t;

// Socket protocol type
typedef enum {
    MP_SOCKET_PROTOCOL_IP = 0,
    MP_SOCKET_PROTOCOL_TCP = 6,
    MP_SOCKET_PROTOCOL_UDP = 17,
    MP_SOCKET_PROTOCOL_ICMP = 1
} mp_socket_protocol_t;

// Socket functions

/**
 * @brief Create a new socket
 * @param domain Socket domain (MP_AF_INET, MP_AF_INET6, etc.)
 * @param type Socket type (MP_SOCK_STREAM, MP_SOCK_DGRAM, etc.)
 * @param protocol Protocol (MP_IPPROTO_TCP, MP_IPPROTO_UDP, etc.)
 * @return Socket descriptor, or MP_SOCKET_INVALID on error
 */
mp_socket_t mp_socket(int domain, int type, int protocol);

/**
 * @brief Close a socket
 * @param sockfd Socket descriptor
 * @return 0 on success, -1 on error
 */
int mp_close(mp_socket_t sockfd);

/**
 * @brief Bind a socket to an address
 * @param sockfd Socket descriptor
 * @param addr Address to bind to
 * @param addrlen Length of address
 * @return 0 on success, -1 on error
 */
int mp_bind(mp_socket_t sockfd, const struct mp_sockaddr *addr, mp_socklen_t addrlen);

/**
 * @brief Connect a socket to an address
 * @param sockfd Socket descriptor
 * @param addr Address to connect to
 * @param addrlen Length of address
 * @return 0 on success, -1 on error
 */
int mp_connect(mp_socket_t sockfd, const struct mp_sockaddr *addr, mp_socklen_t addrlen);

/**
 * @brief Listen for connections on a socket
 * @param sockfd Socket descriptor
 * @param backlog Maximum length of the pending connections queue
 * @return 0 on success, -1 on error
 */
int mp_listen(mp_socket_t sockfd, int backlog);

/**
 * @brief Accept a connection on a socket
 * @param sockfd Socket descriptor
 * @param addr Address of the connecting peer (can be NULL)
 * @param addrlen Length of address (can be NULL)
 * @return New socket descriptor, or MP_SOCKET_INVALID on error
 */
mp_socket_t mp_accept(mp_socket_t sockfd, struct mp_sockaddr *addr, mp_socklen_t *addrlen);

/**
 * @brief Send data on a socket
 * @param sockfd Socket descriptor
 * @param buf Data to send
 * @param len Length of data
 * @param flags Flags (MP_MSG_PEEK, MP_MSG_DONTWAIT, etc.)
 * @return Number of bytes sent, or -1 on error
 */
ssize_t mp_send(mp_socket_t sockfd, const void *buf, size_t len, int flags);

/**
 * @brief Receive data from a socket
 * @param sockfd Socket descriptor
 * @param buf Buffer to receive into
 * @param len Length of buffer
 * @param flags Flags (MP_MSG_PEEK, MP_MSG_DONTWAIT, etc.)
 * @return Number of bytes received, or -1 on error
 */
ssize_t mp_recv(mp_socket_t sockfd, void *buf, size_t len, int flags);

/**
 * @brief Send data to a specific address (for connectionless sockets)
 * @param sockfd Socket descriptor
 * @param buf Data to send
 * @param len Length of data
 * @param flags Flags
 * @param dest_addr Destination address
 * @param addrlen Length of destination address
 * @return Number of bytes sent, or -1 on error
 */
ssize_t mp_sendto(mp_socket_t sockfd, const void *buf, size_t len, int flags,
                   const struct mp_sockaddr *dest_addr, mp_socklen_t addrlen);

/**
 * @brief Receive data from a socket and get sender address
 * @param sockfd Socket descriptor
 * @param buf Buffer to receive into
 * @param len Length of buffer
 * @param flags Flags
 * @param src_addr Source address (can be NULL)
 * @param addrlen Length of source address (can be NULL)
 * @return Number of bytes received, or -1 on error
 */
ssize_t mp_recvfrom(mp_socket_t sockfd, void *buf, size_t len, int flags,
                     struct mp_sockaddr *src_addr, mp_socklen_t *addrlen);

/**
 * @brief Set socket options
 * @param sockfd Socket descriptor
 * @param level Level (MP_SOL_SOCKET, etc.)
 * @param optname Option name
 * @param optval Option value
 * @param optlen Length of option value
 * @return 0 on success, -1 on error
 */
int mp_setsockopt(mp_socket_t sockfd, int level, int optname, const void *optval, mp_socklen_t optlen);

/**
 * @brief Get socket options
 * @param sockfd Socket descriptor
 * @param level Level (MP_SOL_SOCKET, etc.)
 * @param optname Option name
 * @param optval Output buffer for option value
 * @param optlen Length of output buffer
 * @return 0 on success, -1 on error
 */
int mp_getsockopt(mp_socket_t sockfd, int level, int optname, void *optval, mp_socklen_t *optlen);

/**
 * @brief Set socket to blocking or non-blocking mode
 * @param sockfd Socket descriptor
 * @param blocking Whether to set blocking mode
 * @return 0 on success, -1 on error
 */
int mp_socket_set_blocking(mp_socket_t sockfd, bool blocking);

/**
 * @brief Get socket error
 * @param sockfd Socket descriptor
 * @return Error code, or -1 on error
 */
int mp_socket_get_error(mp_socket_t sockfd);

/**
 * @brief Check if data is available to read
 * @param sockfd Socket descriptor
 * @return true if data is available, false otherwise
 */
bool mp_socket_available(mp_socket_t sockfd);

/**
 * @brief Get host by name (DNS lookup)
 * @param name Hostname
 * @param hostent Output buffer for host entry
 * @return 0 on success, -1 on error
 */
int mp_gethostbyname(const char *name, struct mp_hostent *hostent);

// Utility functions

/**
 * @brief Convert IP address to string
 * @param ip IP address in network byte order
 * @param str Output string buffer (must be at least 16 bytes)
 */
static inline void mp_ip_to_str(uint32_t ip, char *str) {
    snprintf(str, 16, "%d.%d.%d.%d",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF,
             ip & 0xFF);
}

/**
 * @brief Convert string to IP address
 * @param str IP address string (e.g., "192.168.1.1")
 * @return IP address in network byte order, or 0 on error
 */
static inline uint32_t mp_str_to_ip(const char *str) {
    uint32_t a, b, c, d;
    if (sscanf(str, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        return (a << 24) | (b << 16) | (c << 8) | d;
    }
    return 0;
}

/**
 * @brief Fill in IPv4 address structure
 * @param addr Address structure
 * @param ip IP address in network byte order
 * @param port Port number in host byte order
 */
static inline void mp_sockaddr_in_init(struct mp_sockaddr_in *addr, uint32_t ip, uint16_t port) {
    addr->sin_family = MP_AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = ip;
    memset(addr->sin_zero, 0, sizeof(addr->sin_zero));
}

/**
 * @brief Fill in IPv6 address structure
 * @param addr Address structure
 * @param ip IPv6 address (16 bytes)
 * @param port Port number in host byte order
 */
static inline void mp_sockaddr_in6_init(struct mp_sockaddr_in6 *addr, uint8_t *ip, uint16_t port) {
    addr->sin6_family = MP_AF_INET6;
    addr->sin6_port = htons(port);
    memcpy(&addr->sin6_addr, ip, sizeof(addr->sin6_addr));
    addr->sin6_flowinfo = 0;
    addr->sin6_scope_id = 0;
}

/**
 * @brief Convert port number to network byte order
 */
static inline uint16_t htons(uint16_t port) {
    return ((port & 0xFF) << 8) | ((port >> 8) & 0xFF);
}

/**
 * @brief Convert port number from network byte order to host byte order
 */
static inline uint16_t ntohs(uint16_t port) {
    return htons(port);
}

/**
 * @brief Convert 32-bit value to network byte order
 */
static inline uint32_t htonl(uint32_t value) {
    return ((value & 0xFF) << 24) | 
           ((value & 0xFF00) << 8) | 
           ((value & 0xFF0000) >> 8) | 
           ((value >> 24) & 0xFF);
}

/**
 * @brief Convert 32-bit value from network byte order to host byte order
 */
static inline uint32_t ntohl(uint32_t value) {
    return htonl(value);
}

#endif // MICROPOSIX_NETWORK_SOCKET_H
