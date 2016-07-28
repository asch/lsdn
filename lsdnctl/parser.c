#include "common.h"
#include "parser.h"
#include "strbuf.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <yaml.h>


struct lsdn_parser {
	yaml_parser_t yaml_parser;
	char *error_str;
	bool error;
};

enum config_option_type {
	CONFIG_OPTION_INT,
	CONFIG_OPTION_STRING,
	CONFIG_OPTION_BOOL,
	CONFIG_OPTION_MAC
};

struct config_option {
	char *name;
	enum config_option_type type;
	void *ptr;
};

static void error(struct lsdn_parser *parser, char *error_str)
{
	parser->error = true;
	parser->error_str = error_str;
}

static char *node_type_str(yaml_node_type_t node_type)
{
	switch (node_type) {
	case YAML_NO_NODE:
		return "YAML_NO_NODE";

	case YAML_SEQUENCE_NODE:
		return "YAML_SEQUENCE_NODE";

	case YAML_SCALAR_NODE:
		return "YAML_SCALAR_NODE";

	case YAML_MAPPING_NODE:
		return "YAML_MAPPING_NODE";

	default:
		return "(invalid node type)";
	}
}

static void dump_node(yaml_node_t *node)
{
	assert(node);

	printf("node %s\n", node_type_str(node->type));
}

static bool is_valid_name(char *str)
{
	char c;
	unsigned i = 0;

	while ((c = str[i++]) != '\0') {
		if (!isalnum(c) && c != '_')
			return (false);
	}

	return (true);
}

struct lsdn_parser *lsdn_parser_new(FILE *src_file)
{
	struct lsdn_parser *parser = malloc(sizeof (struct lsdn_parser));
	if (!parser)
		return (NULL);

	yaml_parser_initialize(&parser->yaml_parser);
	yaml_parser_set_input_file(&parser->yaml_parser, src_file);

	parser->error = false;
	parser->error_str = NULL;

	return (parser);
}

static int parse_options_block(struct lsdn_parser *parser, yaml_document_t *doc,
	struct config_option *options, yaml_node_t *mapping_node)
{
	struct config_option *option;
	yaml_node_t *key, *value;
	yaml_node_pair_t *pair;

	for (pair = mapping_node->data.mapping.pairs.start;
		pair != mapping_node->data.mapping.pairs.top; pair++) {
		key = yaml_document_get_node(doc, pair->key);
		value = yaml_document_get_node(doc, pair->value);

		for (option = options; option->name != NULL; option++) {
			if (strcmp(option->name, (char *)key->data.scalar.value) != 0)
				continue;

			switch (option->type) {
			case CONFIG_OPTION_STRING:
				*((char *)option->ptr) = (char *)value->data.scalar.value;

			default:
				fprintf(stderr, "Not implemented yet");
				exit(EXIT_FAILURE);
			}

		}

		error(parser, "Unknown option");
		return (-1);
	}

	return (0);
}

static int parse_device(struct lsdn_parser *parser, struct lsdn_network *net,
	yaml_document_t *doc, yaml_node_t *mapping_node)
{
	yaml_node_t *key, *value;
	yaml_node_pair_t *pair;
	char *prop_name;
	bool type_specified = false;
	
	for (pair = mapping_node->data.mapping.pairs.start;
		pair != mapping_node->data.mapping.pairs.top; pair++) {
		
		key = yaml_document_get_node(doc, pair->key);
		value = yaml_document_get_node(doc, pair->value);

		prop_name = (char *)key->data.scalar.value;

		if (strcmp(prop_name, "type") != 0)
			continue;

		type_specified = true;
	}

	if (!type_specified) {
		error(parser, "Device type not specified");
		return (-1);
	}

	return (0);
}

static int parse_devices(struct lsdn_parser *parser,
	struct lsdn_network *net, yaml_document_t *doc, yaml_node_t *mapping_node)
{
	yaml_node_t *key, *value;
	yaml_node_pair_t *pair;
	char *device_name;

	for (pair = mapping_node->data.mapping.pairs.start;
		pair != mapping_node->data.mapping.pairs.top; pair++) {

		key = yaml_document_get_node(doc, pair->key);
		device_name = (char *)key->data.scalar.value; 

		DEBUG_PRINTF("Processing device '%s'", device_name);

		value = yaml_document_get_node(doc, pair->value);
		parse_device(parser, net, doc, value);
	}

	return (0);
}

struct lsdn_network *lsdn_parser_parse_network(struct lsdn_parser *parser)
{
	yaml_document_t doc;
	yaml_node_t *node, *key, *value;
	yaml_node_pair_t *pair;
	struct lsdn_network *net;
	char *netname;

	yaml_parser_load(&parser->yaml_parser, &doc);
	node = yaml_document_get_root_node(&doc);

	if (node->type != YAML_MAPPING_NODE) {
		error(parser, "network block has to be a mapping node");
		goto error;
	}

	for (pair = node->data.mapping.pairs.start;
		pair != node->data.mapping.pairs.top; pair++) {

		key = yaml_document_get_node(&doc, pair->key);
		value = yaml_document_get_node(&doc, pair->value);

		assert(key->type == YAML_SCALAR_NODE);

		netname = (char *)key->data.scalar.value;
		if (!is_valid_name(netname)) {
			error(parser, "network name is not valid");
			goto error;
		}

		DEBUG_PRINTF("Processing network '%s'", netname);

		net = lsdn_network_new(netname);
		parse_devices(parser, net, &doc, value);
	}

	yaml_document_delete(&doc);

	return (net);

error:
	yaml_document_delete(&doc);
	return (NULL);
}

void lsdn_parser_free(struct lsdn_parser *parser)
{
	yaml_parser_delete(&parser->yaml_parser);
	free(parser);
}
