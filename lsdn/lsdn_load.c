#include "common.h"
#include "config.h"
#include "../netmodel/include/lsdn.h"

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

static bool load_static_switch(struct config_item *item, void *arg)
{
	DEBUG_PRINTF("Processing static_switch '%s'", item->key);

	struct lsdn_network *net = arg;

	struct config_option options[] = {
		{ 0,		0,			0,		0 }
	};

	if (!config_map_getopt(&item->values, options)) {
		return false;
	}

	lsdn_static_switch_new(net);
	return true;
}

static struct lsdn_network *load_network(struct config_file *config)
{
	struct config_map root_map;
	struct config_item item;
	struct lsdn_network *net = NULL;
	
	config_file_get_root_map(config, &root_map);

	if (config_map_num_items(&root_map) != 1) {
		fprintf(stderr, "Only 1 network definition per file allowed\n");
		exit (EXIT_FAILURE);
	}

	config_map_next_item(&root_map, &item);

	DEBUG_PRINTF("Processing network '%s'", item.key);
	if (item.value_type != CONFIG_VALUE_MAP) {
		fprintf(stderr, "Network definition has to be a map\n");
		exit (EXIT_FAILURE);
	}

	net = lsdn_network_new(item.key);

	struct config_action actions[] = {
		{ "static_switch",	load_static_switch,	net },
		{ "switch",		load_static_switch,	net },
		{ NULL,			NULL,			NULL }
	};

	if (!config_map_dispatch(&item.values, "type", actions, true)) {
		return (NULL);
	}

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
		fprintf(stderr, config_file_get_error_string(config_file));
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
