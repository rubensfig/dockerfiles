/**
 * @file server_ssh.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief Netopeer server SSH part
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

#define _GNU_SOURCE
#define _XOPEN_SOURCE

#include <libnetconf_xml.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <shadow.h>
#include <pwd.h>

#include <libssh/libssh.h>
#include <libssh/callbacks.h>
#include <libssh/server.h>

#include "../server.h"

static const char rcsid[] __attribute__((used)) ="$Id: "__FILE__": "RCSID" $";

extern int quit, restart_soft;

/* one global structure holding all the client information */
extern struct np_state netopeer_state;
extern struct np_options netopeer_options;

static inline void _chan_free(struct client_struct_ssh* client, struct chan_struct* chan) {
	if (chan->nc_sess != NULL) {
		nc_verb_error("%s: internal error: freeing a channel with an opened NC session", __func__);
		nc_session_free(chan->nc_sess);
	}

	if (chan->ssh_chan != NULL && client->ssh_chans->next != NULL) {
		ssh_channel_free(chan->ssh_chan);
	}
}

void client_free_ssh(struct client_struct_ssh* client) {
	if (client->ssh_chans != NULL) {
		nc_verb_error("%s: internal error: freeing a client with some channels", __func__);
	}

	if (client->ssh_sess != NULL) {
		/* !! frees all the associated channels as well !! (if any left) */
		ssh_free(client->ssh_sess);
	}

	/* ssh_free does this for us */
	/*if (client->sock != -1) {
		close(client->sock);
	}*/

	free(client->username);
	free(client);
}

static struct client_struct_ssh* client_find_by_sshsession(struct client_struct* root, ssh_session sshsession) {
	struct client_struct_ssh* client;

	if (sshsession == NULL) {
		return NULL;
	}

	for (client = (struct client_struct_ssh*)root; client != NULL; client = (struct client_struct_ssh*)client->next) {
		if (client->transport == NC_TRANSPORT_SSH && client->ssh_sess == sshsession) {
			break;
		}
	}

	return client;
}

static struct chan_struct* client_find_channel_by_sshchan(struct client_struct_ssh* client, ssh_channel sshchannel) {
	struct chan_struct* chan = NULL;

	for (chan = client->ssh_chans; chan != NULL; chan = chan->next) {
		if (chan->ssh_chan == sshchannel) {
			break;
		}
	}

	return chan;
}

static struct chan_struct* client_free_channel(struct client_struct_ssh* client, struct chan_struct* chan) {
	struct chan_struct* cur_chan, *prev_chan = NULL;

	for (cur_chan = client->ssh_chans; cur_chan != NULL; cur_chan = cur_chan->next) {
		if (cur_chan == chan) {
			break;
		}

		prev_chan = cur_chan;
	}

	if (cur_chan == NULL) {
		nc_verb_error("%s: internal error: channel not found (%s:%d)", __func__, __FILE__, __LINE__);
		return NULL;
	}

	if (prev_chan == NULL) {
		_chan_free(client, cur_chan);
		client->ssh_chans = cur_chan->next;
		free(cur_chan);
		return client->ssh_chans;
	}

	prev_chan->next = cur_chan->next;
	_chan_free(client, cur_chan);
	free(cur_chan);
	return prev_chan;
}

static struct chan_struct* client_find_channel_by_sid(struct client_struct_ssh* client, const char* sid) {
	struct chan_struct* chan = NULL;

	if (client == NULL || sid == NULL) {
		return NULL;
	}

	for (chan = client->ssh_chans; chan != NULL; chan = chan->next) {
		if (chan->nc_sess == NULL) {
			continue;
		}

		if (strcmp(sid, nc_session_get_id(chan->nc_sess)) == 0) {
			break;
		}
	}

	return chan;
}

static int create_netconf_session(struct client_struct_ssh* client, struct chan_struct* channel) {
	struct nc_cpblts* caps = NULL;

	caps = nc_session_get_cpblts_default();
	channel->nc_sess = nc_session_accept_libssh_channel(caps, client->username, channel->ssh_chan);
	nc_cpblts_free(caps);
	if (channel->to_free == 1) {
		/* probably a signal received */
		if (channel->nc_sess != NULL) {
			/* unlikely to happen */
			nc_session_free(channel->nc_sess);
			channel->nc_sess = NULL;
		}
		return EXIT_FAILURE;
	}
	if (channel->nc_sess == NULL) {
		nc_verb_error("%s: failed to create a new NETCONF session", __func__);
		channel->to_free = 1;
		return EXIT_FAILURE;
	}

	/* new session was created */
	nc_verb_verbose("New server session for '%s' with ID %s", client->username, nc_session_get_id(channel->nc_sess));
	gettimeofday((struct timeval*)&channel->last_rpc_time, NULL);

	return EXIT_SUCCESS;
}

/* return 0 - OK, -1 error */
static int sshcb_channel_subsystem(struct client_struct_ssh* client, struct chan_struct* channel, const char* subsystem) {
	if (strcmp(subsystem, "netconf") == 0) {
		if (channel->netconf_subsystem) {
			nc_verb_warning("Client '%s' requested subsystem 'netconf' for the second time", client->username);
		} else {
			channel->netconf_subsystem = 1;
		}
	} else {
		nc_verb_warning("Client '%s' requested unknown subsystem '%s'", client->username, subsystem);
	}

	return 0;
}

static char* auth_password_get_pwd_hash(const char* username) {
	struct passwd* pwd;
	struct spwd* spwd;
	char* pass_hash = NULL;

	pwd = getpwnam(username);
	if (pwd == NULL) {
		nc_verb_verbose("User '%s' not found locally.", username);
		return NULL;
	}

	if (strcmp(pwd->pw_passwd, "x") == 0) {
		spwd = getspnam(username);
		if (spwd == NULL) {
			nc_verb_verbose("Failed to retrieve the shadow entry for '%s'.", username);
			return NULL;
		}

		pass_hash = spwd->sp_pwdp;
	} else {
		pass_hash = pwd->pw_passwd;
	}

	if (pass_hash == NULL) {
		nc_verb_error("%s: no password could be retrieved for '%s'", __func__, username);
		return NULL;
	}

	/* check the hash structure for special meaning */
	if (strcmp(pass_hash, "*") == 0 || strcmp(pass_hash, "!") == 0) {
		nc_verb_verbose("User '%s' is not allowed to authenticate using a password.", username);
		return NULL;
	}
	if (strcmp(pass_hash, "*NP*") == 0) {
		nc_verb_verbose("Retrieving password for '%s' from a NIS+ server not supported.", username);
		return NULL;
	}

	return pass_hash;
}

static int auth_password_compare_pwd(const char* pass_hash, const char* pass_clear) {
	char* new_pass_hash;

	if (strcmp(pass_hash, "") == 0) {
		if (strcmp(pass_clear, "") == 0) {
			nc_verb_verbose("User authentication successful with an empty password!");
			return 0;
		} else {
			/* the user did now know he does not need any password,
			 * (which should not be used) so deny authentication */
			return 1;
		}
	}

	new_pass_hash = crypt(pass_clear, pass_hash);
	if (!new_pass_hash) {
		nc_verb_error("%s: crypt() failed (setting \"%s\").", __func__, pass_hash);
		return 1;
	}
	return strcmp(new_pass_hash, pass_hash);
}

static void sshcb_auth_password(struct client_struct_ssh* client, ssh_message msg) {
	char* pass_hash;

	pass_hash = auth_password_get_pwd_hash(client->username);
	if (pass_hash != NULL && auth_password_compare_pwd(pass_hash, ssh_message_auth_password(msg)) == 0) {
		nc_verb_verbose("User '%s' authenticated.", client->username);
		ssh_message_auth_reply_success(msg, 0);
		client->authenticated = 1;
		return;
	}

	client->auth_attempts++;
	nc_verb_verbose("Failed user '%s' authentication attempt (#%d).", client->username, client->auth_attempts);
	ssh_message_reply_default(msg);
}

static void sshcb_auth_kbdint(struct client_struct_ssh* client, ssh_message msg) {
	char* pass_hash;

	if (!ssh_message_auth_kbdint_is_response(msg)) {
		const char* prompts[] = {"Password: "};
		char echo[] = {0};

		ssh_message_auth_interactive_request(msg, "Interactive SSH Authentication", "Type your password", 1, prompts, echo);
	} else {
		if (ssh_userauth_kbdint_getnanswers(client->ssh_sess) != 1) {
			ssh_message_reply_default(msg);
			return;
		}
		pass_hash = auth_password_get_pwd_hash(client->username);
		if (pass_hash == NULL) {
			ssh_message_reply_default(msg);
			return;
		}
		if (auth_password_compare_pwd(pass_hash, ssh_userauth_kbdint_getanswer(client->ssh_sess, 0)) == 0) {
			nc_verb_verbose("User '%s' authenticated.", client->username);
			client->authenticated = 1;
			ssh_message_auth_reply_success(msg, 0);
		} else {
			client->auth_attempts++;
			nc_verb_verbose("Failed user '%s' authentication attempt (#%d).", client->username, client->auth_attempts);
			ssh_message_reply_default(msg);
		}
	}
}

static char* auth_pubkey_compare_key(ssh_key key) {
	struct np_auth_key* auth_key;
	ssh_key pub_key;
	char* username = NULL;

	/* CLIENT KEYS LOCK */
	pthread_mutex_lock(&netopeer_options.ssh_opts->client_keys_lock);

	for (auth_key = netopeer_options.ssh_opts->client_auth_keys; auth_key != NULL; auth_key = auth_key->next) {
		if (ssh_pki_import_pubkey_file(auth_key->path, &pub_key) != SSH_OK) {
			if (eaccess(auth_key->path, R_OK) != 0) {
				nc_verb_verbose("%s: failed to import the public key \"%s\" (%s)", __func__, auth_key->path, strerror(errno));
			} else {
				nc_verb_verbose("%s: failed to import the public key \"%s\": %s", __func__, auth_key->path, ssh_get_error(pub_key));
			}
			continue;
		}

		if (ssh_key_cmp(key, pub_key, SSH_KEY_CMP_PUBLIC) == 0) {
			ssh_key_free(pub_key);
			break;
		}

		ssh_key_free(pub_key);
	}

	if (auth_key != NULL) {
		username = strdup(auth_key->username);
	}

	/* CLIENT KEYS UNLOCK */
	pthread_mutex_unlock(&netopeer_options.ssh_opts->client_keys_lock);

	return username;
}

static void sshcb_auth_pubkey(struct client_struct_ssh* client, ssh_message msg) {
	char* username;
	int signature_state;

	if ((username = auth_pubkey_compare_key(ssh_message_auth_pubkey(msg))) == NULL) {
		nc_verb_verbose("User '%s' tried to use an unknown (unauthorized) public key.", client->username);
		goto fail;
	} else if (strcmp(client->username, username) != 0) {
		nc_verb_verbose("User '%s' is not the username identified with the presented public key.", client->username);
		goto fail;
	}

	signature_state = ssh_message_auth_publickey_state(msg);
	if (signature_state == SSH_PUBLICKEY_STATE_VALID) {
		nc_verb_verbose("User '%s' authenticated.", client->username);
		client->authenticated = 1;
		ssh_message_auth_reply_success(msg, 0);
	} else if (signature_state == SSH_PUBLICKEY_STATE_NONE) {
		free(username);
		/* accepting only the use of a public key */
		ssh_message_auth_reply_pk_ok_simple(msg);
	} else {
		goto fail;
	}

	return;

fail:
	free(username);
	client->auth_attempts++;
	nc_verb_verbose("Failed user '%s' authentication attempt (#%d).", client->username, client->auth_attempts);
	ssh_message_reply_default(msg);
}

/* return 0 - OK, -1 error */
static int sshcb_channel_open(struct client_struct_ssh* client, ssh_channel channel) {
	struct chan_struct* cur_chan;

	/* GLOBAL LOCK */
	pthread_mutex_lock(&netopeer_state.global_lock);

	if (client->ssh_chans == NULL) {
		client->ssh_chans = calloc(1, sizeof(struct chan_struct));
		cur_chan = client->ssh_chans;
	} else {
		for (cur_chan = client->ssh_chans; cur_chan->next != NULL; cur_chan = cur_chan->next);
		cur_chan->next = calloc(1, sizeof(struct chan_struct));
		cur_chan = cur_chan->next;
	}
	cur_chan->ssh_chan = channel;

	/* GLOBAL UNLOCK */
	pthread_mutex_unlock(&netopeer_state.global_lock);

	gettimeofday((struct timeval*)&cur_chan->last_rpc_time, NULL);

	return 0;
}

int np_ssh_kill_session(const char* sid, struct client_struct_ssh* cur_client) {
	struct client_struct_ssh* kill_client;
	struct chan_struct* kill_chan = NULL;

	if (sid == NULL) {
		return 1;
	}

	/* find the requested session (channel) */
	for (kill_client = (struct client_struct_ssh*)netopeer_state.clients; kill_client != NULL; kill_client = (struct client_struct_ssh*)kill_client->next) {
		if (kill_client->transport != NC_TRANSPORT_SSH || kill_client == cur_client) {
			continue;
		}

		kill_chan = client_find_channel_by_sid(kill_client, sid);
		if (kill_chan != NULL) {
			break;
		}
	}

	if (kill_chan == NULL) {
		return 1;
	}

	kill_chan->to_free = 1;
	return 0;
}

/* return: 0 - nothing happened (sleep), 1 - something happened (skip sleep) */
int np_ssh_client_netconf_rpc(struct client_struct_ssh* client) {
	nc_rpc* rpc = NULL;
	nc_reply* rpc_reply = NULL;
	NC_MSG_TYPE rpc_type;
	xmlNodePtr op;
	int closing = 0, skip_sleep = 0;
	struct nc_err* err;
	struct chan_struct* chan;

	if (client->to_free) {
		return 1;
	}

	for (chan = client->ssh_chans; chan != NULL; chan = chan->next) {
		if (chan->to_free) {
			++skip_sleep;
			continue;
		}

		/* block this client until the hello is received */
		if (chan->nc_sess == NULL) {
			if (!chan->netconf_subsystem) {
				continue;
			}
			if (create_netconf_session(client, chan)) {
				continue;
			}
		}

		/* receive a new RPC */
		rpc_type = nc_session_recv_rpc(chan->nc_sess, 0, &rpc);
		if (rpc_type == NC_MSG_WOULDBLOCK || rpc_type == NC_MSG_NONE) {
			/* no RPC, or processed internally */
			continue;
		}

		gettimeofday((struct timeval*)&chan->last_rpc_time, NULL);

		if (rpc_type == NC_MSG_UNKNOWN) {
			if (nc_session_get_status(chan->nc_sess) != NC_SESSION_STATUS_WORKING) {
				/* something really bad happened, and communication is not possible anymore */
				nc_verb_error("%s: failed to receive client's message (nc session not working)", __func__);
				chan->to_free = 1;
			}
			/* ignore */
			continue;
		}

		if (rpc_type != NC_MSG_RPC) {
			/* NC_MSG_HELLO, NC_MSG_REPLY, NC_MSG_NOTIFICATION */
			nc_verb_warning("%s: received a %s RPC from session %s, ignoring", __func__,
							(rpc_type == NC_MSG_HELLO ? "hello" : (rpc_type == NC_MSG_REPLY ? "reply" : "notification")),
							nc_session_get_id(chan->nc_sess));
			++skip_sleep;
			continue;
		}

		++skip_sleep;

		/* process the new RPC */
		switch (nc_rpc_get_op(rpc)) {
		case NC_OP_CLOSESESSION:
			closing = 1;
			rpc_reply = nc_reply_ok();
			break;

		case NC_OP_KILLSESSION:
			if ((op = ncxml_rpc_get_op_content(rpc)) == NULL || op->name == NULL ||
					xmlStrEqual(op->name, BAD_CAST "kill-session") == 0) {
				nc_verb_error("%s: corrupted RPC message", __func__);
				rpc_reply = nc_reply_error(nc_err_new(NC_ERR_OP_FAILED));
				err = NULL;
				xmlFreeNodeList(op);
				break;
			}
			if (op->children == NULL || xmlStrEqual(op->children->name, BAD_CAST "session-id") == 0) {
				nc_verb_error("%s: no session ID found");
				err = nc_err_new(NC_ERR_MISSING_ELEM);
				nc_err_set(err, NC_ERR_PARAM_INFO_BADELEM, "session-id");
				rpc_reply = nc_reply_error(err);
				err = NULL;
				xmlFreeNodeList(op);
				break;
			}

			/* block-local variables */
			char* sid;
			int ret;

			sid = (char*)xmlNodeGetContent(op->children);
			xmlFreeNodeList(op);

			/* check if this client is not requested to be killed */
			if (client_find_channel_by_sid(client, sid) != NULL) {
				free(sid);
				err = nc_err_new(NC_ERR_INVALID_VALUE);
				nc_err_set(err, NC_ERR_PARAM_MSG, "Requested to kill this session.");
				rpc_reply = nc_reply_error(err);
				break;
			}

			ret = 1;
#ifdef NP_TLS
			ret = np_tls_kill_session(sid, (struct client_struct_tls*)client);
#endif
			if (ret != 0 && np_ssh_kill_session(sid, client) != 0) {
				free(sid);
				err = nc_err_new(NC_ERR_OP_FAILED);
				nc_err_set(err, NC_ERR_PARAM_MSG, "No session with the requested ID found.");
				rpc_reply = nc_reply_error(err);
				break;
			}

			nc_verb_verbose("Session with the ID %s killed.", sid);
			rpc_reply = nc_reply_ok();

			free(sid);
			break;

		case NC_OP_CREATESUBSCRIPTION:
			/* create-subscription message */
			if (nc_cpblts_enabled(chan->nc_sess, "urn:ietf:params:netconf:capability:notification:1.0") == 0) {
				rpc_reply = nc_reply_error(nc_err_new(NC_ERR_OP_NOT_SUPPORTED));
				break;
			}

			/* check if notifications are allowed on this session */
			if (nc_session_notif_allowed(chan->nc_sess) == 0) {
				nc_verb_error("%s: notification subscription is not allowed on the session %s", __func__, nc_session_get_id(chan->nc_sess));
				err = nc_err_new(NC_ERR_OP_FAILED);
				nc_err_set(err, NC_ERR_PARAM_TYPE, "protocol");
				nc_err_set(err, NC_ERR_PARAM_MSG, "Another notification subscription is currently active on this session.");
				rpc_reply = nc_reply_error(err);
				err = NULL;
				break;
			}

			rpc_reply = ncntf_subscription_check(rpc);
			if (nc_reply_get_type(rpc_reply) != NC_REPLY_OK) {
				break;
			}

			pthread_t thread;
			struct ntf_thread_config* ntf_config;

			if ((ntf_config = malloc(sizeof(struct ntf_thread_config))) == NULL) {
				nc_verb_error("%s: memory allocation failed", __func__);
				err = nc_err_new(NC_ERR_OP_FAILED);
				nc_err_set(err, NC_ERR_PARAM_MSG, "Memory allocation failed.");
				rpc_reply = nc_reply_error(err);
				err = NULL;
				break;
			}
			ntf_config->session = chan->nc_sess;
			ntf_config->subscribe_rpc = nc_rpc_dup(rpc);

			/* perform notification sending */
			if ((pthread_create(&thread, NULL, client_notif_thread, ntf_config)) != 0) {
				nc_reply_free(rpc_reply);
				err = nc_err_new(NC_ERR_OP_FAILED);
				nc_err_set(err, NC_ERR_PARAM_MSG, "Creating thread for sending Notifications failed.");
				rpc_reply = nc_reply_error(err);
				err = NULL;
				break;
			}
			pthread_detach(thread);
			break;

		default:
			if ((rpc_reply = ncds_apply_rpc2all(chan->nc_sess, rpc, NULL)) == NULL) {
				err = nc_err_new(NC_ERR_OP_FAILED);
				nc_err_set(err, NC_ERR_PARAM_MSG, "For unknown reason no reply was returned by the library.");
				rpc_reply = nc_reply_error(err);
			} else if (rpc_reply == NCDS_RPC_NOT_APPLICABLE) {
				err = nc_err_new(NC_ERR_OP_FAILED);
				nc_err_set(err, NC_ERR_PARAM_MSG, "There is no device/data that could be affected.");
				nc_reply_free(rpc_reply);
				rpc_reply = nc_reply_error(err);
			}

			break;
		}

		/* send reply */
		nc_session_send_reply(chan->nc_sess, rpc, rpc_reply);
		nc_reply_free(rpc_reply);
		nc_rpc_free(rpc);

		if (closing) {
			chan->to_free = 1;
			closing = 0;
		}
	}

	return skip_sleep;
}

/* return: 0 - nothing happened (sleep), 1 - something happened (skip sleep) */
int np_ssh_client_transport(struct client_struct_ssh* client) {
	struct chan_struct* chan;
	struct timeval cur_time;
	int skip_sleep = 0;

	/* special corner case */
	if (quit && client->ssh_chans == NULL) {
		client->to_free = 1;
	}

	/* check whether the client shouldn't be freed */
	if (client->to_free) {
		return 1;
	}

	gettimeofday(&cur_time, NULL);

	/* check the client for authentication timeout and failed attempts */
	if (!client->authenticated) {
		if (timeval_diff(cur_time, client->conn_time) >= netopeer_options.ssh_opts->auth_timeout) {
			if (client->username == NULL) {
				nc_verb_warning("Failed to authenticate for too long, dropping a client.");
			} else {
				nc_verb_warning("Failed to authenticate for too long, dropping client '%s'.", client->username);
			}

			/* mark client for deletion */
			client->to_free = 1;
			return 0;
		}

		if (client->auth_attempts >= netopeer_options.ssh_opts->auth_attempts) {
			if (client->username == NULL) {
				nc_verb_warning("Reached the number of failed authentication attempts, dropping a client.");
			} else {
				nc_verb_warning("Reached the number of failed authentication attempts, dropping client '%s'.", client->username);
			}

			client->to_free = 1;
			return 0;
		}
	}

	if (ssh_get_status(client->ssh_sess) & (SSH_CLOSED | SSH_CLOSED_ERROR)) {
		nc_verb_error("Client session closed, removing it.");
		if (client->ssh_chans != NULL) {
			for (chan = client->ssh_chans; chan != NULL; chan = chan->next) {
				chan->to_free = 1;
			}
		} else {
			client->to_free = 1;
			return 1;
		}
	}

	if (ssh_execute_message_callbacks(client->ssh_sess) != SSH_OK) {
		if (client->username == NULL) {
			nc_verb_error("Failed to receive new messages (%s), dropping a client.", ssh_get_error(client->ssh_sess));
		} else {
			nc_verb_error("Failed to receive new messages (%s), dropping client '%s'.", ssh_get_error(client->ssh_sess), client->username);
		}

		if (client->ssh_chans != NULL) {
			for (chan = client->ssh_chans; chan != NULL; chan = chan->next) {
				chan->to_free = 1;
			}
		} else {
			client->to_free = 1;
		}
	}
	if (client->new_ssh_msg) {
		skip_sleep = 1;
		client->new_ssh_msg = 0;
	}

	/* check every channel of the client for removal and timeout */
	for (chan = client->ssh_chans; chan != NULL; chan = chan->next) {
		if (chan->to_free || quit) {
			chan->to_free = 1;

			/* don't sleep, we may have been asked to quit */
			skip_sleep = 1;
			nc_verb_verbose("Freeing session for '%s'", client->username);
			nc_session_free(chan->nc_sess);
			chan->nc_sess = NULL;

			/* make sure the channel iteration continues correctly */
			chan = client_free_channel(client, chan);
			if (chan == NULL) {
				/* last channel removed, remove client */
				client->to_free = 1;
				return skip_sleep;
			}
		}

		/* check the channel for idle timeout */
		if (timeval_diff(cur_time, chan->last_rpc_time) >= netopeer_options.idle_timeout) {
			/* check for active event subscriptions, in that case we can never disconnect an idle session */
			if (chan->nc_sess == NULL || !ncntf_session_get_active_subscription(chan->nc_sess)) {
				nc_verb_warning("Session of client '%s' did not send/receive an RPC for too long, disconnecting.", client->username);
				chan->to_free = 1;
			}
		}
	}

	return skip_sleep;
}

int sshcb_msg(ssh_session session, ssh_message msg, void* UNUSED(data)) {
	const char* str_type, *str_subtype = NULL, *username;
	int subtype, type;
	struct client_struct_ssh* client;
	struct chan_struct* channel = NULL;

	type = ssh_message_type(msg);
	subtype = ssh_message_subtype(msg);

	switch (type) {
	case SSH_REQUEST_AUTH:
		str_type = "request-auth";
		switch (subtype) {
		case SSH_AUTH_METHOD_NONE:
			str_subtype = "none";
			break;
		case SSH_AUTH_METHOD_PASSWORD:
			str_subtype = "password";
			break;
		case SSH_AUTH_METHOD_PUBLICKEY:
			str_subtype = "publickey";
			break;
		case SSH_AUTH_METHOD_HOSTBASED:
			str_subtype = "hostbased";
			break;
		case SSH_AUTH_METHOD_INTERACTIVE:
			str_subtype = "interactive";
			break;
		case SSH_AUTH_METHOD_GSSAPI_MIC:
			str_subtype = "gssapi-mic";
			break;
		default:
			str_subtype = "unknown";
			break;
		}
		break;

	case SSH_REQUEST_CHANNEL_OPEN:
		str_type = "request-channel-open";
		switch (subtype) {
		case SSH_CHANNEL_SESSION:
			str_subtype = "session";
			break;
		case SSH_CHANNEL_DIRECT_TCPIP:
			str_subtype = "direct-tcpip";
			break;
		case SSH_CHANNEL_FORWARDED_TCPIP:
			str_subtype = "forwarded-tcpip";
			break;
		case (int)SSH_CHANNEL_X11:
			str_subtype = "channel-x11";
			break;
		case SSH_CHANNEL_UNKNOWN:
			/* fallthrough */
		default:
			str_subtype = "unknown";
			break;
		}
		break;

	case SSH_REQUEST_CHANNEL:
		str_type = "request-channel";
		switch (subtype) {
		case SSH_CHANNEL_REQUEST_PTY:
			str_subtype = "pty";
			break;
		case SSH_CHANNEL_REQUEST_EXEC:
			str_subtype = "exec";
			break;
		case SSH_CHANNEL_REQUEST_SHELL:
			str_subtype = "shell";
			break;
		case SSH_CHANNEL_REQUEST_ENV:
			str_subtype = "env";
			break;
		case SSH_CHANNEL_REQUEST_SUBSYSTEM:
			str_subtype = "subsystem";
			break;
		case SSH_CHANNEL_REQUEST_WINDOW_CHANGE:
			str_subtype = "window-change";
			break;
		case SSH_CHANNEL_REQUEST_X11:
			str_subtype = "x11";
			break;
		case SSH_CHANNEL_REQUEST_UNKNOWN:
			/* fallthrough */
		default:
			str_subtype = "unknown";
			break;
		}
		break;

	case SSH_REQUEST_SERVICE:
		str_type = "request-service";
		str_subtype = ssh_message_service_service(msg);
		break;

	case SSH_REQUEST_GLOBAL:
		str_type = "request-global";
		switch (subtype) {
		case SSH_GLOBAL_REQUEST_TCPIP_FORWARD:
			str_subtype = "tcpip-forward";
			break;
		case SSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD:
			str_subtype = "cancel-tcpip-forward";
			break;
		case SSH_GLOBAL_REQUEST_UNKNOWN:
			/* fallthrough */
		default:
			str_subtype = "unknown";
			break;
		}
		break;

	default:
		str_type = "unknown";
		str_subtype = "unknown";
		break;
	}

	nc_verb_verbose("Received an SSH message \"%s\" of subtype \"%s\".", str_type, str_subtype);

	if ((client = client_find_by_sshsession(netopeer_state.clients, session)) == NULL) {
		nc_verb_error("%s: internal error (%s:%d)", __func__, __FILE__, __LINE__);
		return 1;
	}

	client->new_ssh_msg = 1;

	if (type == SSH_REQUEST_CHANNEL) {
		if ((channel = client_find_channel_by_sshchan(client, ssh_message_channel_request_channel(msg))) == NULL) {
			nc_verb_error("%s: internal error (%s:%d)", __func__, __FILE__, __LINE__);
			return 1;
		}
	}

	/*
	 * process known messages
	 */
	if (type == SSH_REQUEST_AUTH) {
		if (client->authenticated) {
			nc_verb_warning("User '%s' authenticated, but requested another authentication.", client->username);
			ssh_message_reply_default(msg);
			return 0;
		}

		if (client->auth_attempts >= netopeer_options.ssh_opts->auth_attempts) {
			/* too many failed attempts */
			ssh_message_reply_default(msg);
			return 0;
		}

		/* save the username, do not let the client change it */
		username = ssh_message_auth_user(msg);
		if (client->username == NULL) {
			if (username == NULL) {
				nc_verb_error("Denying an auth request without a username.");
				return 1;
			}

			client->username = strdup(username);
		} else if (username != NULL) {
			if (strcmp(username, client->username) != 0) {
				nc_verb_error("User '%s' changed its username to '%s', disconnecting.", client->username, username);
				client->to_free = 1;
				return 1;
			}
		}

		if (subtype == SSH_AUTH_METHOD_NONE) {
			/* libssh will return the supported auth methods */
			return 1;
		} else if (subtype == SSH_AUTH_METHOD_PASSWORD) {
			sshcb_auth_password(client, msg);
			return 0;
		} else if (subtype == SSH_AUTH_METHOD_PUBLICKEY) {
			sshcb_auth_pubkey(client, msg);
			return 0;
		} else if (subtype == SSH_AUTH_METHOD_INTERACTIVE) {
			sshcb_auth_kbdint(client, msg);
			return 0;
		}
	} else if (client->authenticated) {
		if (type == SSH_REQUEST_CHANNEL_OPEN && subtype == (int)SSH_CHANNEL_SESSION) {
			ssh_channel chan;
			if ((chan = ssh_message_channel_request_open_reply_accept(msg)) == NULL) {
				ssh_message_reply_default(msg);
			}
			sshcb_channel_open(client, chan);
			return 0;
		} else if (type == SSH_REQUEST_CHANNEL && subtype == (int)SSH_CHANNEL_REQUEST_SUBSYSTEM) {
			if (sshcb_channel_subsystem(client, channel, ssh_message_channel_request_subsystem(msg)) == 0) {
				ssh_message_channel_request_reply_success(msg);
			} else {
				ssh_message_reply_default(msg);
			}
			return 0;
		}
	}

	/* we did not process it */
	return 1;
}

void sshcb_log(int priority, const char* UNUSED(function), const char* buffer, void* UNUSED(userdata)) {
	switch(priority) {
	case 1:
		nc_verb_error("SSH log: %s", buffer);
		break;
	case 2:
		nc_verb_warning("SSH log: %s", buffer);
		break;
	case 3:
		nc_verb_verbose("SSH log: %s", buffer);
		break;
	default:
		nc_verb_error("SSH log callback called with an unknown priority %d.", priority);
		nc_verb_error("SSH log: %s", buffer);
		break;
	}
}

void np_ssh_init(void) {
	ssh_set_log_level(netopeer_options.verbose);
	ssh_set_log_callback(sshcb_log);
}

ssh_bind np_ssh_server_id_check(ssh_bind sshbind) {
	ssh_bind ret;

	/* Check server keys for a change */
	if (netopeer_options.ssh_opts->server_key_change_flag || sshbind == NULL) {
		ssh_bind_free(sshbind);
		if ((ret = ssh_bind_new()) == NULL) {
			nc_verb_error("%s: failed to create SSH bind", __func__);
			return NULL;
		}

		if (netopeer_options.ssh_opts->rsa_key != NULL) {
			ssh_bind_options_set(ret, SSH_BIND_OPTIONS_RSAKEY, netopeer_options.ssh_opts->rsa_key);
		}
		if (netopeer_options.ssh_opts->dsa_key != NULL) {
			ssh_bind_options_set(ret, SSH_BIND_OPTIONS_DSAKEY, netopeer_options.ssh_opts->dsa_key);
		}

		ssh_bind_options_set(ret, SSH_BIND_OPTIONS_LOG_VERBOSITY, &netopeer_options.verbose);

		netopeer_options.ssh_opts->server_key_change_flag = 0;
	} else {
		ret = sshbind;
	}

	return ret;
}

int np_ssh_session_count(void) {
	struct client_struct_ssh* client;
	struct chan_struct* chan;
	int count = 0;

	for (client = (struct client_struct_ssh*)netopeer_state.clients; client != NULL; client = (struct client_struct_ssh*)client->next) {
		if (client->transport != NC_TRANSPORT_SSH) {
			continue;
		}

		if (client->ssh_chans == NULL) {
			/* count this client as one soon-to-be valid session */
			++count;
		}
		for (chan = client->ssh_chans; chan != NULL; chan = chan->next) {
			/* count every channel, we rather include some invalid ones than
			 * exclude some soon-to-be valid, which could cause more sessions
			 * to be allowed than max_sessions
			 */
			++count;
		}
	}

	return count;
}

int np_ssh_create_client(struct client_struct_ssh* new_client, ssh_bind sshbind) {
	int ret;

	new_client->ssh_sess = ssh_new();
	if (new_client->ssh_sess == NULL) {
		nc_verb_error("%s: ssh error: failed to allocate a new SSH session (%s:%d)", __func__, __FILE__, __LINE__);
		return 1;
	}

	if (netopeer_options.ssh_opts->password_auth_enabled) {
		ssh_set_auth_methods(new_client->ssh_sess, SSH_AUTH_METHOD_PASSWORD | SSH_AUTH_METHOD_PUBLICKEY | SSH_AUTH_METHOD_INTERACTIVE);
	} else {
		ssh_set_auth_methods(new_client->ssh_sess, SSH_AUTH_METHOD_PUBLICKEY | SSH_AUTH_METHOD_INTERACTIVE);
	}

	ssh_set_message_callback(new_client->ssh_sess, sshcb_msg, NULL);

	if (ssh_bind_accept_fd(sshbind, new_client->ssh_sess, new_client->sock) == SSH_ERROR) {
		nc_verb_error("%s: SSH failed to accept a new connection: %s", __func__, ssh_get_error(sshbind));
		return 1;
	}

	gettimeofday((struct timeval*)&new_client->conn_time, NULL);

	while ((ret = ssh_handle_key_exchange(new_client->ssh_sess)) == SSH_AGAIN) {
		usleep(READ_SLEEP * 2);
	}
	if (ret != SSH_OK) {
		nc_verb_error("%s: SSH key exchange error (%s:%d): %s", __func__, __FILE__, __LINE__, ssh_get_error(new_client->ssh_sess));
		return 1;
	}

	ssh_set_blocking(new_client->ssh_sess, 0);

	return 0;
}

void np_ssh_cleanup(void) {
	/* nothing to do here, libssh finalize is called by libnetconf */
}
