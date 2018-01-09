/**
 * @file netconf_server_transapi_tls.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief Netopeer ietf-netconf-server transapi module TLS part
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

/*
 * This is automatically generated callbacks file
 * It contains 3 parts: Configuration callbacks, RPC callbacks and state data callbacks.
 * Do NOT alter function signatures or any structures unless you know exactly what you are doing.
 */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>

#include <libxml/tree.h>
#include <libnetconf_xml.h>

#include "../server.h"

static const char rcsid[] __attribute__((used)) ="$Id: "__FILE__": "RCSID" $";

extern struct np_options netopeer_options;
extern int server_config_modified;

extern TRANSAPI_CLBCKS_ORDER_TYPE callbacks_order;

/* Do not modify or set! This variable is set by libnetconf to announce edit-config's error-option
Feel free to use it to distinguish module behavior for different error-option values.
 * Possible values:
 * NC_EDIT_ERROPT_STOP - Following callback after failure are not executed, all successful callbacks executed till
                         failure point must be applied to the device.
 * NC_EDIT_ERROPT_CONT - Failed callbacks are skipped, but all callbacks needed to apply configuration changes are executed
 * NC_EDIT_ERROPT_ROLLBACK - After failure, following callbacks are not executed, but previous successful callbacks are
                         executed again with previous configuration data to roll it back.
 */
extern NC_EDIT_ERROPT_TYPE server_erropt;

/**
 * @brief This callback will be run when node in path /srv:netconf/srv:tls/srv:listen/srv:port changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_srv_netconf_srv_tls_srv_listen_srv_port(void** UNUSED(data), XMLDIFF_OP op, xmlNodePtr old_node, xmlNodePtr new_node, struct nc_err** error) {
	return callback_srv_netconf_srv_listen_srv_port(op, old_node, new_node, error, NC_TRANSPORT_TLS);
}

/**
 * @brief This callback will be run when node in path /srv:netconf/srv:tls/srv:listen/srv:interface changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_srv_netconf_srv_tls_srv_listen_srv_interface(void** UNUSED(data), XMLDIFF_OP op, xmlNodePtr old_node, xmlNodePtr new_node, struct nc_err** error) {
	return callback_srv_netconf_srv_listen_srv_interface(op, old_node, new_node, error, NC_TRANSPORT_TLS);
}

/**
 * @brief This callback will be run when node in path /srv:netconf/srv:tls/srv:call-home/srv:applications/srv:application changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_srv_netconf_srv_tls_srv_call_home_srv_applications_srv_application(void** UNUSED(data), XMLDIFF_OP op, xmlNodePtr old_node, xmlNodePtr new_node, struct nc_err** error) {
	return callback_srv_netconf_srv_call_home_srv_applications_srv_application(op, old_node, new_node, error, NC_TRANSPORT_TLS);
}

int np_tls_chapp_linger_check(struct ch_app* app) {
	struct timeval cur_time;

	gettimeofday(&cur_time, NULL);
	if (timeval_diff(cur_time, ((struct client_struct_tls*)app->client)->last_rpc_time) >= app->rep_linger) {

		/* no data flow for too long, disconnect the client, wait for the set timeout and reconnect */
		nc_verb_verbose("Call Home (app %s) did not communicate for too long, disconnecting.", app->name);
		app->client->to_free = 1;
		sleep(app->rep_timeout*60);
		return 1;
	}

	return 0;
}

int server_transapi_init_tls(void) {
	xmlDocPtr doc = NULL;
	struct nc_err* error = NULL;
	const char* str_err;

	/* set device according to defaults */
	nc_verb_verbose("Setting the default TLS configuration for the ietf-netconf-server module...");

	doc = xmlReadDoc(BAD_CAST "<netconf xmlns=\"urn:ietf:params:xml:ns:yang:ietf-netconf-server\"><tls><listen><port>6513</port></listen></tls></netconf>",
		NULL, NULL, 0);
	if (doc == NULL) {
		nc_verb_error("Unable to parse the default TLS ietf-netconf-server configuration.");
		return EXIT_FAILURE;
	}

	if (callback_srv_netconf_srv_tls_srv_listen_srv_port(NULL, XMLDIFF_ADD, NULL, doc->children->children->children->children, &error) != EXIT_SUCCESS) {
		if (error != NULL) {
			str_err = nc_err_get(error, NC_ERR_PARAM_MSG);
			if (str_err != NULL) {
				nc_verb_error(str_err);
			}
			nc_err_free(error);
		}
		xmlFreeDoc(doc);
		return EXIT_FAILURE;
	}
	xmlFreeDoc(doc);

	return EXIT_SUCCESS;
}

/**
 * @brief Free all resources allocated on plugin runtime and prepare plugin for removal.
 */
void server_transapi_close_tls(void) {

}
