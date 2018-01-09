/**
 * @file cfgnetopeer_transapi.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @author David Kupka <xkupka01@stud.fit.vutbr.cz>
 * @brief NETCONF device module to configure netconf server
 *
 * Copyright (C) 2011-2015 CESNET, z.s.p.o.
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
#include <pthread.h>
#include <stdlib.h>
#include <libxml/tree.h>
#include <libnetconf_xml.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <string.h>

#include "server.h"

static const char rcsid[] __attribute__((used)) ="$Id: "__FILE__": "RCSID" $";

extern int quit, restart_soft, restart_hard;
extern int server_start;

/* transAPI version which must be compatible with libnetconf */
/* int transapi_version = 6; */

/* Signal to libnetconf that configuration data were modified by any callback.
 * 0 - data not modified
 * 1 - data have been modified
 */
int netopeer_config_modified = 0;

/* Do not modify or set! This variable is set by libnetconf to announce edit-config's error-option
Feel free to use it to distinguish module behavior for different error-option values.
 * Possible values:
 * NC_EDIT_ERROPT_STOP - Following callback after failure are not executed, all successful callbacks executed till
                         failure point must be applied to the device.
 * NC_EDIT_ERROPT_CONT - Failed callbacks are skipped, but all callbacks needed to apply configuration changes are executed
 * NC_EDIT_ERROPT_ROLLBACK - After failure, following callbacks are not executed, but previous successful callbacks are
                         executed again with previous configuration data to roll it back.
 */
NC_EDIT_ERROPT_TYPE netopeer_erropt = NC_EDIT_ERROPT_NOTSET;

struct np_options netopeer_options = {
	.binds_lock = PTHREAD_MUTEX_INITIALIZER
};

extern struct transapi server_transapi;
struct transapi netopeer_transapi;

char* get_node_content(const xmlNodePtr node) {
	if (node == NULL || node->children == NULL) {
		return NULL;
	}
	return (char*)node->children->content;
}

void module_free(struct np_module* module) {
	if (module->ds) {
		module_disable(module, 1);
	} else {
		free(module->name);
		free(module);
	}
}

/*
 * if repo_type is -1, then we are working with augment models specifications
 */
static int parse_model_cfg(struct np_module* module, xmlNodePtr node, NCDS_TYPE repo_type) {
	char *transapi_path = NULL, *model_path = NULL, *feature, *name, *aux;
	struct transapi *st = NULL;
	xmlNodePtr aux_node;

	if (strcmp(module->name, NETOPEER_MODULE_NAME) == 0) {
		st = &netopeer_transapi;
	} else if (strcmp(module->name, NCSERVER_MODULE_NAME) == 0) {
		st = &server_transapi;
	}

	for (aux_node = node->children; aux_node != NULL; aux_node = aux_node->next) {
		if (xmlStrcmp(aux_node->name, BAD_CAST "path") == 0) {
			model_path = (char*)xmlNodeGetContent(aux_node);
		}
		if (xmlStrcmp(aux_node->name, BAD_CAST "transapi") == 0) {
			transapi_path = (char*)xmlNodeGetContent(aux_node);
		}
		if (model_path && transapi_path) {
			break;
		}
	}
	/* Netopeer module is something extra */
	if (st != NULL && model_path) {
		/* internal static server (Netopeer) module */
		if (repo_type == -1 && transapi_path) {
			/* augment transapi module */
			nc_verb_verbose("Adding augment transapi \"%s\"", model_path);
			ncds_add_augment_transapi(model_path, transapi_path);
		} else if (repo_type == -1) {
			/* augment model */
			nc_verb_verbose("Adding augment model \"%s\"", model_path);
			ncds_add_model(model_path);
		} else {
			nc_verb_verbose("Adding static transapi \"%s\"", model_path);
			if ((module->ds = ncds_new_transapi_static(repo_type, model_path, st)) == NULL) {
				free(model_path);
				free(transapi_path);
				return (EXIT_FAILURE);
			}
		}
	} else if (model_path && transapi_path) {
		if (repo_type == -1) {
			/* augment transapi module */
			nc_verb_verbose("Adding augment transapi \"%s\"", model_path);
			ncds_add_augment_transapi(model_path, transapi_path);
		} else {
			/* base transapi module for datastore */
			nc_verb_verbose("Adding transapi \"%s\"", model_path);
			if ((module->ds = ncds_new_transapi(repo_type, model_path, transapi_path)) == NULL) {
				free(model_path);
				free(transapi_path);
				return (EXIT_FAILURE);
			}
		}
	} else if (model_path) {
		if (repo_type == -1) {
			/* augment model */
			nc_verb_verbose("Adding augment model \"%s\"", model_path);
			ncds_add_model(model_path);
		} else {
			/* base model for datastore */
			nc_verb_verbose("Adding base model \"%s\"", model_path);
			if ((module->ds = ncds_new2(repo_type, model_path, NULL)) == NULL) {
				free(model_path);
				return (EXIT_FAILURE);
			}
		}
	} else {
		nc_verb_error("Configuration mismatch: missing model path in %s config.", module->name);
	}
	name = strdup(basename(model_path));
	/* cut off the .yin suffix */
	aux = strrchr(name, '.');
	if (aux) { *aux = '\0';}
	/* cut off @revision info */
	aux = strrchr(name, '@');
	if (aux) { *aux = '\0';}

	free(model_path);
	free(transapi_path);

	/* set features */
	for (aux_node = node->children; aux_node != NULL; aux_node = aux_node->next) {
		if (xmlStrcmp(aux_node->name, BAD_CAST "feature") == 0) {
			feature = (char*)xmlNodeGetContent(aux_node);
			if (strcmp(feature, "*") == 0) {
				if (ncds_features_enableall(name)) {
					nc_verb_error("Enabling all features in \"%s\" module failed.", name);
					free(name);
					return EXIT_FAILURE;
				}
				nc_verb_verbose("All features in \"%s\" module enabled.", name);
			} else {
				if (ncds_feature_enable(name, feature)) {
					nc_verb_error("Enabling \"%s\" features in \"%s\" module failed.", feature, name);
					free(name);
					return EXIT_FAILURE;
				}
				nc_verb_verbose("\"%s\" features in \"%s\" module enabled.", feature, name);
			}
			free(feature);
		}
	}
	free(name);

	return (EXIT_SUCCESS);
}

int module_enable(struct np_module* module, int add) {
	char *config_path = NULL, *repo_path = NULL, *repo_type_str = NULL;
	int repo_type = -1, main_model_count;
	xmlDocPtr module_config;
	xmlNodePtr node;
	xmlXPathContextPtr xpath_ctxt;
	xmlXPathObjectPtr xpath_obj;

	if (asprintf(&config_path, "%s/%s.xml", MODULES_CFG_DIR, module->name) == -1) {
		nc_verb_error("asprintf() failed (%s:%d).", __FILE__, __LINE__);
		return(EXIT_FAILURE);
	}
	if ((module_config = xmlReadFile(config_path, NULL, XML_PARSE_NOBLANKS|XML_PARSE_NSCLEAN|XML_PARSE_NOWARNING|XML_PARSE_NOERROR)) == NULL) {
		nc_verb_error("Reading configuration for %s module failed", module->name);
		free(config_path);
		return(EXIT_FAILURE);
	}
	free(config_path);

	if ((xpath_ctxt = xmlXPathNewContext(module_config)) == NULL) {
		nc_verb_error("Creating XPath context failed (%s:%d - module %s)", __FILE__, __LINE__, module->name);
		return (EXIT_FAILURE);
	}

	/* get datastore information */
	if ((xpath_obj = xmlXPathEvalExpression(BAD_CAST "/device/repo", xpath_ctxt)) == NULL) {
		nc_verb_error("XPath evaluating error (%s:%d)", __FILE__, __LINE__);
		goto err_cleanup;
	} else if (xpath_obj->nodesetval == NULL || xpath_obj->nodesetval->nodeNr != 1) {
		nc_verb_verbose("repo is not unique in %s transAPI module configuration.", module->name);
		xmlXPathFreeObject(xpath_obj);
		goto err_cleanup;
	}

	for (node = xpath_obj->nodesetval->nodeTab[0]->children; node != NULL; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}
		if (xmlStrcmp(node->name, BAD_CAST "type") == 0) {
			repo_type_str = (char*)xmlNodeGetContent(node);
		} else if (xmlStrcmp(node->name, BAD_CAST "path") == 0) {
			repo_path = (char*)xmlNodeGetContent(node);
		}
	}
	if (repo_type_str == NULL) {
		nc_verb_warning("Missing attribute \'type\' in repo element for %s transAPI module.", module->name);
		repo_type_str = strdup("unknown");
	}
	if (strcmp(repo_type_str, "empty") == 0) {
		repo_type = NCDS_TYPE_EMPTY;
	} else if (strcmp(repo_type_str, "file") == 0) {
		repo_type = NCDS_TYPE_FILE;
	} else {
		nc_verb_warning("Unknown repo type \'%s\' in %s transAPI module configuration", repo_type_str, module->name);
		nc_verb_warning("Continuing with \'empty\' datastore type.");
		repo_type = NCDS_TYPE_EMPTY;
	}
	free(repo_type_str);

	if (repo_type == NCDS_TYPE_FILE && repo_path == NULL) {
		nc_verb_error("Missing path for \'file\' datastore type in %s transAPI module configuration.", module->name);
		xmlXPathFreeObject(xpath_obj);
		goto err_cleanup;
	}
	xmlXPathFreeObject(xpath_obj);

	/* get data-models element */
	if ((xpath_obj = xmlXPathEvalExpression(BAD_CAST "/device/data-models", xpath_ctxt)) == NULL) {
		nc_verb_error("XPath evaluating error (%s:%d)", __FILE__, __LINE__);
		goto err_cleanup;
	} else if (xpath_obj->nodesetval == NULL || xpath_obj->nodesetval->nodeNr != 1) {
		nc_verb_verbose("data-models is not unique in %s transAPI module configuration.", module->name);
		xmlXPathFreeObject(xpath_obj);
		goto err_cleanup;
	}

	/* parse models in the config-defined order, both main and augments */
	main_model_count = 0;
	for (node = xpath_obj->nodesetval->nodeTab[0]->children; node != NULL; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}
		if (xmlStrcmp(node->name, BAD_CAST "model") == 0) {
			parse_model_cfg(module, node, -1);
		}
		if (xmlStrcmp(node->name, BAD_CAST "model-main") == 0) {
			parse_model_cfg(module, node, repo_type);
			main_model_count++;
		}
	}
	xmlXPathFreeObject(xpath_obj);
	if (main_model_count == 0) {
		nc_verb_verbose("model-main is not present in %s transAPI module configuration.", module->name);
		goto err_cleanup;
	} else if (main_model_count > 1) {
		nc_verb_verbose("model-main is not unique in %s transAPI module configuration.", module->name);
		goto err_cleanup;
	}

	if (repo_type == NCDS_TYPE_FILE) {
		if (ncds_file_set_path(module->ds, repo_path)) {
			nc_verb_verbose("Unable to set path to datastore of the \'%s\' transAPI module.", module->name);
			goto err_cleanup;
		}
	}
	free(repo_path);
	repo_path = NULL;

	if ((module->id = ncds_init(module->ds)) < 0) {
		goto err_cleanup;
	}

	xmlXPathFreeContext(xpath_ctxt);
	xmlFreeDoc(module_config);

	if (ncds_consolidate() != 0) {
		nc_verb_warning("%s: consolidating libnetconf datastores failed for module %s.", __func__, module->name);
		return (EXIT_FAILURE);
	}

	/* remove datastore locks if any kept */
	if (server_start) {
		ncds_break_locks(NULL);
	}
	if (ncds_device_init(&(module->id), NULL, 1) != 0) {
		nc_verb_error("Device initialization of module %s failed.", module->name);
		ncds_free(module->ds);
		module->ds = NULL;
		return (EXIT_FAILURE);
	}

	if (add) {
		if (netopeer_options.modules) {
			netopeer_options.modules->prev = module;
		}
		module->next = netopeer_options.modules;
		netopeer_options.modules = module;
	}

	return (EXIT_SUCCESS);

err_cleanup:

	xmlXPathFreeContext(xpath_ctxt);
	xmlFreeDoc(module_config);

	ncds_free(module->ds);
	module->ds = NULL;

	free(repo_path);

	return (EXIT_FAILURE);
}

int module_disable(struct np_module* module, int destroy) {
	ncds_free(module->ds);
	module->ds = NULL;

	if (ncds_consolidate() != 0) {
		nc_verb_warning("%s: consolidating libnetconf datastores failed for module %s.", __func__, module->name);
	}

	if (destroy) {
		if (module->next) {
			module->next->prev = module->prev;
		}
		if (module->prev) {
			module->prev->next = module->next;
		}
		if (netopeer_options.modules == module) {
			netopeer_options.modules = module->next;
		}

		module_free(module);
	}
	return(EXIT_SUCCESS);
}

/**
 * @brief Retrieve state data from device and return them as XML document
 *
 * @param model	Device data model. libxml2 xmlDocPtr.
 * @param running	Running datastore content. libxml2 xmlDocPtr.
 * @param[out] err  Double pointer to error structure. Fill error when some occurs.
 * @return State data as libxml2 xmlDocPtr or NULL in case of error.
 */
xmlDocPtr netopeer_get_state_data (xmlDocPtr UNUSED(model), xmlDocPtr UNUSED(running), struct nc_err** UNUSED(err)) {
	/* no state data */
	return(NULL);
}
/*
 * Mapping prefixes with namespaces.
 * Do NOT modify this structure!
 */
struct ns_pair netopeer_namespace_mapping[] = {{"n", "urn:cesnet:tmc:netopeer:1.0"}, {NULL, NULL}};

/*
* CONFIGURATION callbacks
* Here follows set of callback functions run every time some change in associated part of running datastore occurs.
* You can safely modify the bodies of all function as well as add new functions for better lucidity of code.
*/

/**
 * @brief This callback will be run when node in path /n:netopeer/n:hello-timeout changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_n_netopeer_n_hello_timeout(void** UNUSED(data), XMLDIFF_OP op, xmlNodePtr UNUSED(old_node), xmlNodePtr new_node, struct nc_err** error) {
	char* content = NULL, *ptr, *msg;
	uint32_t num;

	if (op & XMLDIFF_REM) {
		/* set default value */
		nc_hello_timeout(600 * 1000);
		return EXIT_SUCCESS;
	}

	content = get_node_content(new_node);
	if (content == NULL) {
		*error = nc_err_new(NC_ERR_OP_FAILED);
		nc_verb_error("%s: node content missing", __func__);
		return EXIT_FAILURE;
	}

	num = strtol(content, &ptr, 10);
	if (*ptr != '\0') {
		*error = nc_err_new(NC_ERR_BAD_ELEM);
		if (asprintf(&msg, "Could not convert '%s' to a number.", content) == 0) {
			nc_err_set(*error, NC_ERR_PARAM_MSG, msg);
			nc_err_set(*error, NC_ERR_PARAM_INFO_BADELEM, "/netopeer/hello-timeout");
			free(msg);
		}
		return EXIT_FAILURE;
	}

	nc_hello_timeout(num * 1000);
	return EXIT_SUCCESS;
}

/**
 * @brief This callback will be run when node in path /n:netopeer/n:idle-timeout changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_n_netopeer_n_idle_timeout(void** UNUSED(data), XMLDIFF_OP op, xmlNodePtr UNUSED(old_node), xmlNodePtr new_node, struct nc_err** error) {
	char* content = NULL, *ptr, *msg;
	uint32_t num;

	if (op & XMLDIFF_REM) {
		netopeer_options.idle_timeout = 3600;
		return EXIT_SUCCESS;
	}

	content = get_node_content(new_node);
	if (content == NULL) {
		*error = nc_err_new(NC_ERR_OP_FAILED);
		nc_verb_error("%s: node content missing", __func__);
		return EXIT_FAILURE;
	}

	num = strtol(content, &ptr, 10);
	if (*ptr != '\0') {
		*error = nc_err_new(NC_ERR_BAD_ELEM);
		if (asprintf(&msg, "Could not convert '%s' to a number.", content) == 0) {
			nc_err_set(*error, NC_ERR_PARAM_MSG, msg);
			nc_err_set(*error, NC_ERR_PARAM_INFO_BADELEM, "/netopeer/idle-timeout");
			free(msg);
		}
		return EXIT_FAILURE;
	}

	netopeer_options.idle_timeout = num;
	return EXIT_SUCCESS;
}

/**
 * @brief This callback will be run when node in path /n:netopeer/n:max-sessions changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_n_netopeer_n_max_sessions(void** UNUSED(data), XMLDIFF_OP op, xmlNodePtr UNUSED(old_node), xmlNodePtr new_node, struct nc_err** error) {
	char* content = NULL, *ptr, *msg;
	uint16_t num;

	if (op & XMLDIFF_REM) {
		netopeer_options.max_sessions = 8;
		return EXIT_SUCCESS;
	}

	content = get_node_content(new_node);
	if (content == NULL) {
		*error = nc_err_new(NC_ERR_OP_FAILED);
		nc_verb_error("%s: node content missing", __func__);
		return EXIT_FAILURE;
	}

	num = strtol(content, &ptr, 10);
	if (*ptr != '\0') {
		*error = nc_err_new(NC_ERR_BAD_ELEM);
		if (asprintf(&msg, "Could not convert '%s' to a number.", content) == 0) {
			nc_err_set(*error, NC_ERR_PARAM_MSG, msg);
			nc_err_set(*error, NC_ERR_PARAM_INFO_BADELEM, "/netopeer/max-sessions");
			free(msg);
		}
		return EXIT_FAILURE;
	}

	netopeer_options.max_sessions = num;
	return EXIT_SUCCESS;
}

/**
 * @brief This callback will be run when node in path /n:netopeer/n:response-time changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_n_netopeer_n_response_time(void** UNUSED(data), XMLDIFF_OP op, xmlNodePtr UNUSED(old_node), xmlNodePtr new_node, struct nc_err** error) {
	char* content = NULL, *ptr, *msg;
	uint16_t num;

	if (op & XMLDIFF_REM) {
		netopeer_options.response_time = 50;
		return EXIT_SUCCESS;
	}

	content = get_node_content(new_node);
	if (content == NULL) {
		*error = nc_err_new(NC_ERR_OP_FAILED);
		nc_verb_error("%s: node content missing", __func__);
		return EXIT_FAILURE;
	}

	num = strtol(content, &ptr, 10);
	if (*ptr != '\0') {
		*error = nc_err_new(NC_ERR_OP_FAILED);
		if (asprintf(&msg, "Could not convert '%s' to a number.", content) == 0) {
			nc_err_set(*error, NC_ERR_PARAM_MSG, msg);
			free(msg);
		}
		return EXIT_FAILURE;
	}

	netopeer_options.response_time = num;
	return EXIT_SUCCESS;
}

/**
 * @brief This callback will be run when node in path /n:netopeer/n:modules/n:module/n:module/n:enabled changes
 *
 * @param[in] data	Double pointer to void. Its passed to every callback. You can share data using it.
 * @param[in] op	Observed change in path. XMLDIFF_OP type.
 * @param[in] node	Modified node. if op == XMLDIFF_REM its copy of node removed.
 * @param[out] error	If callback fails, it can return libnetconf error structure with a failure description.
 *
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
/* !DO NOT ALTER FUNCTION SIGNATURE! */
int callback_n_netopeer_n_modules_n_module_n_enabled(void** UNUSED(data), XMLDIFF_OP op, xmlNodePtr old_node, xmlNodePtr new_node, struct nc_err** UNUSED(error)) {
	xmlNodePtr tmp, node;
	char* module_name = NULL, *module_enabled = NULL;
	struct np_module* module = netopeer_options.modules;

	node = (op & XMLDIFF_REM ? old_node : new_node);
	if (node == NULL) {
		return EXIT_FAILURE;
	}

	for (tmp = node->parent->children; tmp != NULL; tmp = tmp->next) {
		if (xmlStrEqual(tmp->name, BAD_CAST "name")) {
			module_name = get_node_content(tmp);
			break;
		}
	}
	module_enabled = get_node_content(node);

	if (module_name == NULL || module_enabled == NULL) {
		nc_verb_error("%s: missing module \"name\" or \"enabled\" node", __func__);
		return EXIT_FAILURE;
	}

	if ((op & (XMLDIFF_REM | XMLDIFF_ADD)) && strcmp(module_enabled, "false") == 0) {
		/* module is/was not enabled, nothing to enable/disable */
		return EXIT_SUCCESS;
	}

	while (module) {
		if (strcmp(module->name, module_name) == 0) {
			break;
		}
		module = module->next;
	}

	if ((op & XMLDIFF_REM) || ((op & XMLDIFF_MOD) && strcmp(module_enabled, "false") == 0)) {
		if (module == NULL) {
			/* it does not exist, so it is disabled */
			return EXIT_SUCCESS;
		}

		if (module_disable(module, 1)) {
			return EXIT_FAILURE;
		}
	} else if ((op & XMLDIFF_ADD) || ((op & XMLDIFF_MOD) && strcmp(module_enabled, "true") == 0)) {
		if (module != NULL) {
			nc_verb_error("%s: internal error: module to enable already exists", __func__);
			return EXIT_FAILURE;
		}

		module = calloc(1, sizeof(struct np_module));
		module->name = strdup(module_name);
		if (module_enable(module, 1)) {
			free(module->name);
			free(module);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
/*
* Structure transapi_config_callbacks provide mapping between callback and path in configuration datastore.
* It is used by libnetconf library to decide which callbacks will be run.
* DO NOT alter this structure
*/
struct transapi_data_callbacks netopeer_clbks = {
#if defined(NP_SSH) && defined(NP_TLS)
	.callbacks_count = 17,
#else
	.callbacks_count = 11,
#endif
	.data = NULL,
	.callbacks = {
		{.path = "/n:netopeer/n:hello-timeout", .func = callback_n_netopeer_n_hello_timeout},
		{.path = "/n:netopeer/n:idle-timeout", .func = callback_n_netopeer_n_idle_timeout},
		{.path = "/n:netopeer/n:max-sessions", .func = callback_n_netopeer_n_max_sessions},
		{.path = "/n:netopeer/n:response-time", .func = callback_n_netopeer_n_response_time},
#ifdef NP_SSH
		{.path = "/n:netopeer/n:ssh/n:server-keys/n:rsa-key", .func = callback_n_netopeer_n_ssh_n_server_keys_n_rsa_key},
		{.path = "/n:netopeer/n:ssh/n:server-keys/n:dsa-key", .func = callback_n_netopeer_n_ssh_n_server_keys_n_dsa_key},
		{.path = "/n:netopeer/n:ssh/n:client-auth-keys/n:client-auth-key", .func = callback_n_netopeer_n_ssh_n_client_auth_keys_n_client_auth_key},
		{.path = "/n:netopeer/n:ssh/n:password-auth-enabled", .func = callback_n_netopeer_n_ssh_n_password_auth_enabled},
		{.path = "/n:netopeer/n:ssh/n:auth-attempts", .func = callback_n_netopeer_n_ssh_n_auth_attempts},
		{.path = "/n:netopeer/n:ssh/n:auth-timeout", .func = callback_n_netopeer_n_ssh_n_auth_timeout},
#endif
#ifdef NP_TLS
		{.path = "/n:netopeer/n:tls/n:server-cert", .func = callback_n_netopeer_n_tls_n_server_cert},
		{.path = "/n:netopeer/n:tls/n:server-key", .func = callback_n_netopeer_n_tls_n_server_key},
		{.path = "/n:netopeer/n:tls/n:trusted-ca-certs/n:trusted-ca-cert", .func = callback_n_netopeer_n_tls_n_trusted_ca_certs_n_trusted_ca_cert},
		{.path = "/n:netopeer/n:tls/n:trusted-client-certs/n:trusted-client-cert", .func = callback_n_netopeer_n_tls_n_trusted_client_certs_n_trusted_client_cert},
		{.path = "/n:netopeer/n:tls/n:crl-dir", .func = callback_n_netopeer_n_tls_n_crl_dir},
		{.path = "/n:netopeer/n:tls/n:cert-maps/n:cert-to-name", .func = callback_n_netopeer_n_tls_n_cert_maps_n_cert_to_name},
#endif
		{.path = "/n:netopeer/n:modules/n:module/n:enabled", .func = callback_n_netopeer_n_modules_n_module_n_enabled}
	}
};

/**
 * @brief Initialize plugin after loaded and before any other functions are called.

 * This function should not apply any configuration data to the controlled device. If no
 * running is returned (it stays *NULL), complete startup configuration is consequently
 * applied via module callbacks. When a running configuration is returned, libnetconf
 * then applies (via module's callbacks) only the startup configuration data that
 * differ from the returned running configuration data.

 * Please note, that copying startup data to the running is performed only after the
 * libnetconf's system-wide close - see nc_close() function documentation for more
 * information.

 * @param[out] running	Current configuration of managed device.

 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int netopeer_transapi_init(xmlDocPtr* UNUSED(running)) {
	xmlDocPtr doc;
	struct nc_err* error = NULL;
	const char* str_err;

	nc_verb_verbose("Setting the default configuration for the cfgnetopeer module...");

	doc = xmlReadDoc(BAD_CAST "<netopeer xmlns=\"urn:cesnet:tmc:netopeer:1.0\"><hello-timeout>600</hello-timeout><idle-timeout>3600</idle-timeout><max-sessions>8</max-sessions><response-time>50</response-time></netopeer>",
		NULL, NULL, 0);
	if (doc == NULL) {
		nc_verb_error("Unable to parse the default cfgnetopeer configuration.");
		return EXIT_FAILURE;
	}

	if (callback_n_netopeer_n_hello_timeout(NULL, XMLDIFF_ADD, NULL, doc->children->children, &error) != EXIT_SUCCESS) {
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

	if (callback_n_netopeer_n_idle_timeout(NULL, XMLDIFF_ADD, NULL, doc->children->children->next, &error) != EXIT_SUCCESS) {
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

	if (callback_n_netopeer_n_max_sessions(NULL, XMLDIFF_ADD, NULL, doc->children->children->next->next, &error) != EXIT_SUCCESS) {
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

	if (callback_n_netopeer_n_response_time(NULL, XMLDIFF_ADD, NULL, doc->children->children->next->next->next, &error) != EXIT_SUCCESS) {
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

#ifdef NP_SSH
	if (ncds_feature_isenabled("cfgnetopeer", "ssh") && (netopeer_transapi_init_ssh() != EXIT_SUCCESS)) {
		return EXIT_FAILURE;
	}
#endif
#ifdef NP_TLS
	if (ncds_feature_isenabled("cfgnetopeer", "tls") && (netopeer_transapi_init_tls() != EXIT_SUCCESS)) {
		return EXIT_FAILURE;
	}
#endif

	return EXIT_SUCCESS;
}

/**
 * @brief Free all resources allocated on plugin runtime and prepare plugin for removal.
 */
void netopeer_transapi_close(void) {
#ifdef NP_TLS
	if (ncds_feature_isenabled("cfgnetopeer", "tls")) {
		netopeer_transapi_close_tls();
	}
#endif
#ifdef NP_SSH
	if (ncds_feature_isenabled("cfgnetopeer", "ssh")) {
		netopeer_transapi_close_ssh();
	}
#endif

	nc_verb_verbose("Netopeer cleanup.");

	while (netopeer_options.modules) {
		module_disable(netopeer_options.modules, 1);
	}
}

/**
 * @brief Get a node from the RPC input. The first found node is returned, so if traversing lists,
 * call repeatedly with result->next as the node argument.
 *
 * @param name	Name of the node to be retrieved.
 * @param node	List of nodes that will be searched.
 * @return Pointer to the matching node or NULL
 */
xmlNodePtr get_rpc_node(const char* name, const xmlNodePtr node) {
	xmlNodePtr ret = NULL;

	for (ret = node; ret != NULL; ret = ret->next) {
		if (xmlStrEqual(BAD_CAST name, ret->name)) {
			break;
		}
	}

	return ret;
}

/*
* RPC callbacks
* Here follows set of callback functions run every time RPC specific for this device arrives.
* You can safely modify the bodies of all function as well as add new functions for better lucidity of code.
* Every function takes array of inputs as an argument. On few first lines they are assigned to named variables. Avoid accessing the array directly.
* If input was not set in RPC message argument in set to NULL.
*/

nc_reply* rpc_netopeer_reboot(xmlNodePtr input) {
	xmlNodePtr type_node = get_rpc_node("type", input);
	char* type_str = NULL;

	if (type_node) {
		type_str = (char*)xmlNodeGetContent(type_node);
	}

	if (type_str == NULL || strcmp(type_str, "soft") == 0) {
		restart_soft = 1;
	} else if (strcmp(type_str, "hard") == 0) {
		quit = 1;
		restart_hard = 1;
	} else {
		free(type_str);
		return nc_reply_error(nc_err_new(NC_ERR_INVALID_VALUE));
	}
	free(type_str);

	return nc_reply_ok();
}

nc_reply* rpc_reload_module(xmlNodePtr input) {
	xmlNodePtr module_node = get_rpc_node("module", input);
	char* module_name;
	struct np_module* module = netopeer_options.modules;

	if (module_node) {
		module_name = (char*)xmlNodeGetContent(module_node);
	} else {
		return nc_reply_error(nc_err_new(NC_ERR_MISSING_ELEM));
	}

	while (module) {
		if (strcmp(module->name, module_name) == 0) {
			break;
		}
		module = module->next;
	}
	free(module_name);

	if (module == NULL) {
		return nc_reply_error(nc_err_new(NC_ERR_INVALID_VALUE));
	}

	if (module_disable(module, 0) || module_enable(module, 0)) {
		return nc_reply_error(nc_err_new(NC_ERR_OP_FAILED));
	}

	return nc_reply_ok();
}
/*
* Structure transapi_rpc_callbacks provide mapping between callbacks and RPC messages.
* It is used by libnetconf library to decide which callbacks will be run when RPC arrives.
* DO NOT alter this structure
*/
struct transapi_rpc_callbacks netopeer_rpc_clbks = {
	.callbacks_count = 2,
	.callbacks = {
		{.name = "netopeer-reboot", .func = rpc_netopeer_reboot},
		{.name = "reload-module", .func = rpc_reload_module}
	}
};

struct transapi netopeer_transapi = {
	.version = 6,
	.init = netopeer_transapi_init,
	.close = netopeer_transapi_close,
	.get_state = netopeer_get_state_data,
	.clbks_order = TRANSAPI_CLBCKS_LEAF_TO_ROOT,
	.data_clbks = &netopeer_clbks,
	.rpc_clbks = &netopeer_rpc_clbks,
	.ns_mapping = netopeer_namespace_mapping,
	.config_modified = &netopeer_config_modified,
	.erropt = &netopeer_erropt,
	.file_clbks = NULL,
};
