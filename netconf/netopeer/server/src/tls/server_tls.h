/**
 * @file server_tls.h
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief Netopeer server TLS part header
 *
 * Copyright (C) 2015 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is, and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#ifndef _SERVER_TLS_H_
#define _SERVER_TLS_H_

#include <openssl/ssl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <libnetconf.h>

/* for each client */
struct client_struct_tls {
	NC_TRANSPORT transport;

	int sock;
	struct sockaddr_storage saddr;
	pthread_t tid;
	char* username;
	volatile int to_free;
	struct client_struct* next;

	SSL* tls;
	X509* cert;
	struct nc_session* nc_sess;
	volatile struct timeval last_rpc_time;	// timestamp of the last RPC either in or out
};

struct np_state_tls {
	int last_tls_idx;
	pthread_mutex_t* tls_mutex_buf;
};

int np_tls_client_netconf_rpc(struct client_struct_tls* client);

int np_tls_client_transport(struct client_struct_tls* client);

void np_tls_thread_cleanup(void);

void np_tls_init(void);

SSL_CTX* np_tls_server_id_check(SSL_CTX* ctx);

int np_tls_session_count(void);

int np_tls_kill_session(const char* sid, struct client_struct_tls* cur_client);

int np_tls_create_client(struct client_struct_tls* new_client, SSL_CTX* tlsctx);

void np_tls_cleanup(void);

void client_free_tls(struct client_struct_tls* client);

#endif /* _SERVER_TLS_H_ */