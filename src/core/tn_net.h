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

/* An outgoing connection that never blocks the caller: the name lookup runs on its own
 * thread, then every address is tried in turn (IPv4 first, because the classic login
 * obfuscation needs the server's IPv4 address; IPv6 after that). */
typedef struct tn_connector tn_connector;
/* NULL only when out of memory. */
tn_connector *tn_connector_start(const char *host, int port);
/* 1 connected (*out is the socket, now owned by the caller), 0 still busy, -1 failed (err). */
int tn_connector_poll(tn_connector *c, tn_socket *out, char *err, size_t errsz);
/* 1 while the name lookup has not answered yet */
int tn_connector_resolving(const tn_connector *c);
/* Gives up at any point; a lookup still running finishes and frees itself. */
void tn_connector_free(tn_connector *c);
/* Tests only: every lookup sleeps this long first, as a slow DNS server would. */
extern int tn_net_test_resolve_delay_ms;
/* Tests only: try IPv6 addresses before IPv4 ones. */
extern int tn_net_test_prefer_ipv6;

/* 1 connected, 0 still connecting, -1 failed */
int tn_tcp_connected(tn_socket s, char *err, size_t errsz);
/* loopback_only: bind 127.0.0.1 instead of every interface (IPv6 and IPv4 where the system
 * has IPv6, else IPv4). port 0 picks a free one. */
tn_socket tn_tcp_listen(int port, int loopback_only, char *err, size_t errsz);
int tn_tcp_port(tn_socket s);
tn_socket tn_tcp_accept(tn_socket ls);
/* IPv4 address of the other end (zeros for IPv6 that is not v4-mapped) */
void tn_tcp_peer_ipv4(tn_socket s, unsigned char ip[4]);
/* Who is on the other end, for per-address limits: the IPv4 address (v4-mapped), or the /64
 * prefix of an IPv6 one. All zeros if unknown. */
void tn_tcp_peer_key(tn_socket s, unsigned char key[16]);
void tn_tcp_close(tn_socket s);

/* A message connection: buffered, framed with 0xFF (a bare \n is accepted on input too). */
typedef struct tn_conn {
    tn_socket sock;
    char in[16384];
    size_t inlen;
    char *out;
    size_t outlen, outcap;
    size_t outmax;      /* queued bytes before the peer counts as dead; 0 = 4 MiB */
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
