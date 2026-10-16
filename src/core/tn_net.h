/*
 * gtetrinet-ng — portable non-blocking TCP and 0xFF-framed message buffers.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TN_NET_H
#define TN_NET_H

#include <stddef.h>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET tn_socket;
#define TN_BAD_SOCKET INVALID_SOCKET
#else
typedef int tn_socket;
#define TN_BAD_SOCKET (-1)
#endif

int tn_net_init(void);

/* Resolves (blocking) and starts a non-blocking connect, preferring IPv4 because the login
 * obfuscation needs the server's IPv4 address. */
tn_socket tn_tcp_connect(const char *host, int port, char *err, size_t errsz);
/* 1 connected, 0 still connecting, -1 failed */
int tn_tcp_connected(tn_socket s, char *err, size_t errsz);
/* loopback_only: bind 127.0.0.1 instead of every interface. port 0 picks a free one. */
tn_socket tn_tcp_listen(int port, int loopback_only, char *err, size_t errsz);
int tn_tcp_port(tn_socket s);
tn_socket tn_tcp_accept(tn_socket ls);
/* IPv4 address of the other end (zeros for IPv6 that is not v4-mapped) */
void tn_tcp_peer_ipv4(tn_socket s, unsigned char ip[4]);
void tn_tcp_close(tn_socket s);

/* A message connection: buffered, framed with 0xFF (a bare \n is accepted on input too). */
typedef struct tn_conn {
    tn_socket sock;
    char in[16384];
    size_t inlen;
    char *out;
    size_t outlen, outcap;
    int dead;
    char err[128];
} tn_conn;

void tn_conn_init(tn_conn *c, tn_socket s);
/* Queues a message (the terminator is added). */
void tn_conn_send(tn_conn *c, const char *msg);
/* Same, ended with a newline: the query protocol's answers are plain text lines. */
void tn_conn_send_line(tn_conn *c, const char *msg);
/* Reads what is available, writes what can be written. Call once per frame. */
void tn_conn_pump(tn_conn *c);
/* Next complete message, or 0 if none. */
int tn_conn_next(tn_conn *c, char *msg, size_t msgsz);
void tn_conn_close(tn_conn *c);

#endif
