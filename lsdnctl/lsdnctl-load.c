#include "common.h"
#include "parser.h"

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

int main(int argc, char *argv[])
{
	const char *filename;
	int opt, option_index;
	FILE *config_file;
	struct lsdn_parser parser;

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
		DEBUG_MSG("Reading stdin instead of a regular file");
	}
	else {
		config_file = fopen(filename, "r");

		if (config_file == NULL) {
			fprintf(stderr, "%s: cannot open file for reading: %s.\n",
				program_name, filename);
			exit(EXIT_FAILURE);
		}
	}

	lsdn_parser_init(&parser);
	lsdn_parser_set_file(&parser, config_file);

	lsdn_parse_network(&parser);

	if (parser.parse_error) {
		fprintf(stderr, "%s: parse error: %s\n",
			program_name, lsdn_parser_strerror(&parser));
		exit(EXIT_FAILURE);
	}

	lsdn_parser_end(&parser);
	
	return (EXIT_SUCCESS);
out:
	fclose(config_file);
	return (EXIT_FAILURE);
}
