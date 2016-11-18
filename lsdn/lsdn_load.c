#include "common.h"
#include "config.h"
#include "../netmodel/include/lsdn.h"

#include <ctype.h>
#include <getopt.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int verbose;
char *program_name;

static struct option long_options[] = {
	{ "verbose",	no_argument,	&verbose,	1 },
	{ "help",	no_argument,	NULL,		'h' },
	{ 0,		0,		0,		0 }
};

static void usage(int status)
{
	if (status != EXIT_SUCCESS) {
		fprintf(stderr, "Try '%s --help' for more information.\n", program_name);
	}
	else {
		fprintf(stderr, "Usage: %s FILE\n", program_name);
		fprintf(stderr, "Load network definition from FILE and create the network.\n\n");
	}

	exit(status);
}

static bool is_name_valid(const char *str)
{	
	if (!isalpha(str[0])) /* will fail for empty name as well */
		return false;

	char c;
	int i = 0;
	while ((c = str[i++]) != '\0') {
		if (!isalpha(c) && !isdigit(c) && c != '_' && c != '-')
			return false;
	}

	return true;
}

static bool is_effectively_empty(struct config_item *item)
{
	if(item == NULL)
		return true;
	if(item->value_type == CONFIG_VALUE_MAP && config_map_num_items(&item->values) == 0)
		return true;
	if(item->value_type == CONFIG_VALUE_SCALAR && item->value[0] == 0)
		return true;
	return false;
}

static bool load_port(
	struct lsdn_node *node,
	const char *port_name,
	lsdn_port_type_t ptype,
	struct config_item *attributes)
{
	size_t port_name_len = strlen(port_name);
	if(port_name[port_name_len - 1] == '*'){
		if(!is_effectively_empty(attributes)){
			DEBUG_PRINTF("A port class is no allowed to have attributes");
			return false;
		}

		char* clean_name = strdup(port_name);
		clean_name[port_name_len - 1] = 0;
		if(!is_name_valid(clean_name)){
			DEBUG_PRINTF("Invalid port group name %s", clean_name);
			free(clean_name);
			return false;
		}

		struct lsdn_port_group *pg = lsdn_create_port_group(node, clean_name, ptype);
		if(!pg)
			/* TODO: also handle name conflict */
			abort_msg("Can not allocate port group\n");
		DEBUG_PRINTF("Created portgroup %s", port_name);
		free(clean_name);
	}else{
		if(!is_name_valid(port_name)){
			DEBUG_PRINTF("Invalid port name %s", port_name);
			return false;
		}
		struct lsdn_port *p = lsdn_create_port(node, ptype);
		if(!p)
			abort_msg("Can not allocate port");
		lsdn_port_set_name(p, port_name);
		DEBUG_PRINTF("Created port %s", port_name);
	}
	/* TODO: add error messages */
	return true;
}

static bool load_ports(
	struct config_map *map,
	const char* port_key,
	struct lsdn_node *netdev,
	lsdn_port_type_t ptype,
	bool *should_create_default)
{
	/* TODO: add error messages */
	struct config_item port_def;
	if(config_map_get(map, port_key, &port_def)){
		if(port_def.value_type == CONFIG_VALUE_LIST){
			/* a port definition can be a simple list of port names */
			struct config_item port;
			while(config_list_next_item(&port_def.list, &port)){
				if(port.value_type != CONFIG_VALUE_SCALAR){
					DEBUG_PRINTF("A list of ports can contain only scalars\n");
					return false;
				}
				if(!load_port(netdev, port.value, ptype, NULL))
					return false;
			}
		}else if(port_def.value_type == CONFIG_VALUE_MAP){
			/* or a map of ports with attributes */
			struct config_item port;
			while(config_map_next_item(&port_def.values, &port)){
				if(!(port.value_type == CONFIG_VALUE_SCALAR
				   || port.value_type == CONFIG_VALUE_MAP)){
					DEBUG_PRINTF("Port attribute must be scalar or map\n");
					return false;
				}
				if(!load_port(netdev, port.key, ptype, &port))
					return false;
			}
		}else{

			return false;
		}
		*should_create_default = false;
	}else{
		*should_create_default = true;
	}
	return true;
}

static bool load_netdev(struct config_item *item, void *arg)
{
	struct lsdn_network *net = arg;
	const char* iface = NULL;
	bool defports;

	struct config_option options[] = {
		{ "if",		CONFIG_OPTION_STRING,	&iface,         false},
		{ 0,		0,			0,		0 }
	};

	if (!config_map_getopt(&item->values, options)) {
		return false;
	}
	if(!iface)
		iface = item->key;

	/* TODO: check validity */

	struct lsdn_netdev* netdev = lsdn_netdev_new(net, iface);
	if(!netdev)
		abort_msg("Can not allocate netdev\n");
	if(!load_ports(
		   &item->values, "ports",
		   (struct lsdn_node*)netdev, LSDN_PORTT_DEFAULT, &defports))
		return false;

	if(defports){
		struct lsdn_port * p = lsdn_create_port((struct lsdn_node*) netdev, LSDN_PORTT_DEFAULT);
		if(!p)
			abort_msg("Can not create netdev default port");
		lsdn_port_set_name(p, "default");
	}
	return true;

}

static bool load_static_switch(struct config_item *item, void *arg)
{
	struct lsdn_network *net = arg;
	char *name = item->key;
	bool broadcast = false;
	bool mkdefault;

	// TODO: we should return errors, here, but how, if I don't have config_file ?
	// (directed at david)


	DEBUG_PRINTF("Processing static_switch '%s'", name);

	struct config_option options[] = {
		{ "broadcast",  CONFIG_OPTION_BOOL,	&broadcast,      false},
		{ 0,		0,			0,		0 }
	};

	if (!config_map_getopt(&item->values, options)) {
		return false;
	}
	struct lsdn_static_switch* ss = lsdn_static_switch_new(net);
	if(!ss)
		abort_msg("Can not allocate static switch\n");

	lsdn_static_switch_enable_broadcast(ss, broadcast);
	return load_ports(
		&item->values, "ports",
		(struct lsdn_node*)ss, LSDN_PORTT_DEFAULT,
		&mkdefault);
}

static struct lsdn_network *load_network(struct config_file *config)
{
	struct config_map root_map;
	struct config_item item;
	struct lsdn_network *net = NULL;
	char *netname = strdup("lsdn");

	config_file_get_root_map(config, &root_map);
	if(config_map_get(&root_map, "name", &item)){
		if(item.value_type != CONFIG_VALUE_SCALAR)
			config_file_set_error(config, "Network name must be a string");

		free(netname);
		netname = strdup(item.value);
		if(netname == NULL)
			abort();
	}

	if (!is_name_valid(netname)) {
		config_file_set_error(config, "Invalid network name '%s'", netname);
		goto error;
	}

	DEBUG_PRINTF("Processing network '%s'", netname);
	net = lsdn_network_new(netname);
	if(net == NULL){
		abort_msg("Can not create network ");
	}

	if(config_map_get(&root_map, "devices", &item)){
		if(item.value_type != CONFIG_VALUE_MAP){
			config_file_set_error(config, "The devices section must be a map");
			goto error;
		}
		struct config_action actions[] = {
			{ "static_switch",	load_static_switch,	net },
			{ "switch",		load_static_switch,	net },
			{ "netdev",		load_netdev,		net },
			{ NULL,			NULL,			NULL }
		};

		if (!config_map_dispatch(&item.values, "type", actions, true)) {
			goto error;
		}
	}else{
		config_file_set_error(config, "The network definition must contain 'devices' key");
		goto error;
	}

	if(config_map_get(&root_map, "connect", &item)){
		if(item.value_type != CONFIG_VALUE_LIST){
			config_file_set_error(config, "The connect section must be a list");
			goto error;
		}
	}

	/* todo check that we don't have any unsupported names in the map */
	
	error:
	free(netname);
	DEBUG_MSG("Config processed OK");
	return net;
}

int main(int argc, char *argv[])
{
	char *filename;
	int opt, option_index;
	struct lsdn_network *net;
	struct config_file *config_file;

	(void) net;

	program_name = argv[0];

	while ((opt = getopt_long(argc, argv, "hv", long_options, &option_index)) != -1) {
		switch (opt) {
			case 0:
			case 1:
				break;

			case 'h':
				usage(EXIT_SUCCESS);

			default:
				usage(EXIT_FAILURE);
		}
	}

	if (argc - optind == 0) {
		fprintf(stderr, "%s: missing FILE operand.\n", program_name);
		usage(EXIT_FAILURE);
	}

	if (argc - optind > 1) {
		fprintf(stderr, "%s: too many arguments.\n", program_name);
		usage(EXIT_FAILURE);
	}

	filename = argv[optind];

	if (strcasecmp(filename, "-") == 0) {
		config_file = config_file_open_stdin();
	}
	else {
		config_file = config_file_open(filename);
	}

	if (config_file_has_errors(config_file)) {
		fprintf(stderr, "%s\n", config_file_get_error_string(config_file));
		return EXIT_FAILURE;
	}

	net = load_network(config_file);

	if (config_file_has_errors(config_file)) {
		fprintf(stderr, "%s: %s: %s\n", argv[0], filename,
			config_file_get_error_string(config_file));
		return EXIT_FAILURE;
	}
	
	config_file_close(config_file);
	return EXIT_SUCCESS;
}
