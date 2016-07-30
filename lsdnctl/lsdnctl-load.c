#include "common.h"
#include "config.h"

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
		fprintf(stderr, "Try '%s --help' for more information.", program_name);
	}
	else {
		fprintf(stderr, "Usage: %s FILE\n", program_name);
		fprintf(stderr, "Load network definition from FILE and create the network.\n\n");
	}

	exit(status);
}

static bool load_static_switch(struct config_map *map, void *arg)
{
	struct network *net = arg;
	int num_ports;
	lsdn_mac_t mac;

	(void) net;

	struct config_option options[] = {
		{ "num_ports",	CONFIG_OPTION_INT,	&num_ports,	true },
		{ "mac",	CONFIG_OPTION_MAC,	&mac,		true },
		{ 0,		0,			0,		0 }
	};

	return config_map_getopt(map, options);
}

static struct lsdn_network *load_network(struct config_file *config)
{
	struct config_map root_map;
	struct config_pair pair;
	struct lsdn_network *net = NULL;
	
	config_file_get_root_map(config, &root_map);

	struct config_action actions[] = {
		{ "static_switch",	load_static_switch,	net },
		{ "switch",		load_static_switch,	net },
		{ NULL,			NULL,			NULL }
	};

	if (config_map_num_pairs(&root_map) != 1) {
		fprintf(stderr, "Only 1 network definition per file allowed");
		exit (EXIT_FAILURE);
	}

	while (config_map_next_pair(&root_map, &pair)) {
		DEBUG_PRINTF("Processing network '%s'", pair.key);

		if (pair.value_type != CONFIG_VALUE_MAP) {
			fprintf(stderr, "Network definition has to be a map");
			exit (EXIT_FAILURE);
		}

		config_map_dispatch(&pair.values, "device_type", actions);
	}

	return (net);
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

	if (argc - optind != 1) {
		fprintf(stderr, "%s: missing FILE operand.\n", program_name);
		usage(EXIT_FAILURE);
	}

	filename = argv[optind];

	if (strcasecmp(filename, "-") == 0) {
		config_file = config_file_open_stdin();
	}
	else {
		/*
		config_file = fopen(filename, "r");

		if (config_file == NULL) {
			fprintf(stderr, "%s: cannot open file for reading: %s.\n",
				program_name, filename);
			exit(EXIT_FAILURE);
		}
		*/

		config_file = config_file_open(filename);
	}

	net = load_network(config_file);
	
	config_file_close(config_file);
	return (EXIT_SUCCESS);
}
