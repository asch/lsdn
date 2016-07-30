#include "config.h"
#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <yaml.h>


struct config_file {
	FILE *file;		/* config file handle */
	yaml_parser_t parser;	/* YAML parser */
	yaml_document_t doc;	/* YAML document */
	yaml_node_t *root_node;	/* YAML root node */
};

struct config_file *config_file;

static bool load_map_from_node(struct config_map *map, yaml_node_t *node)
{
	if (node->type != YAML_MAPPING_NODE)
		return false;

	map->node = node;
	config_map_reset(map);

	return true;
}

struct config_file *config_file_open(char *filename)
{
	struct config_file *config;

	assert(filename != NULL);
	
	config = malloc(sizeof(struct config_file));
	if (config == NULL)
		goto error;

	/* TODO global evil */
	config_file = config;

	config->file = fopen(filename, "r");
	if (config->file == NULL) {
		return NULL;
	}

	yaml_parser_initialize(&config->parser);
	yaml_parser_set_input_file(&config->parser, config->file);

	yaml_parser_load(&config->parser, &config->doc);
	config->root_node = yaml_document_get_root_node(&config->doc);

	if (config->root_node == NULL)
		goto error2;

	return config;

error2:
	yaml_document_delete(&config->doc);
	yaml_parser_delete(&config->parser);

error:
	fclose(config->file);
	return NULL;
}

struct config_file *config_file_open_stdin()
{
	NOT_IMPLEMENTED;
}

bool config_file_get_root_map(struct config_file *config, struct config_map *map)
{
	assert(config != NULL);
	assert(map != NULL);

	return load_map_from_node(map, config->root_node);
}

void config_file_close(struct config_file *config)
{
	assert(config != NULL);

	yaml_document_delete(&config->doc);
	yaml_parser_delete(&config->parser);
	fclose(config->file);
}

bool config_map_next_pair(struct config_map *map, struct config_pair *pair)
{
	yaml_node_t *key_node, *value_node;

	assert(map != NULL);
	assert(pair != NULL);

	if (map->pair == map->node->data.mapping.pairs.top)
		return false;

	key_node = yaml_document_get_node(&config_file->doc, map->pair->key);
	value_node = yaml_document_get_node(&config_file->doc, map->pair->value);

	pair->key = (char *)key_node->data.scalar.value;

	switch (value_node->type) {
	case YAML_SCALAR_NODE:
		pair->value_type = CONFIG_VALUE_SCALAR;
		pair->value = (char *)value_node->data.scalar.value;
		break;

	case YAML_MAPPING_NODE:
		pair->value_type = CONFIG_VALUE_MAP;
		load_map_from_node(&pair->values, value_node);
		break;

	case YAML_SEQUENCE_NODE:
		//pair->value_type = CONFIG_VALUE_LIST;
		NOT_IMPLEMENTED;

	default:
		DEBUG_MSG("Invalid pair value type");
		return false;
	}

	map->pair++;
	return true;
}

void config_map_reset(struct config_map *map)
{
	map->pair = map->node->data.mapping.pairs.start;
	map->num_pairs = (map->node->data.mapping.pairs.top
		- map->node->data.mapping.pairs.start);
}

size_t config_map_num_pairs(struct config_map *map)
{
	return map->num_pairs;
}

bool config_map_getopt(struct config_map *map, struct config_option *options)
{
	struct config_pair pair;
	struct config_option *opt;
	bool opt_found;

	for (opt = options; opt->name != NULL; opt++) {
		config_map_reset(map);
		opt_found = false;

		while (config_map_next_pair(map, &pair)) {
			if (strcmp(pair.key, opt->name) != 0)
				continue;

			opt_found = true;

			switch (opt->type) {
			case CONFIG_OPTION_INT:
				*((int *)opt->ptr) = atol(pair.value);
				break;

			case CONFIG_OPTION_STRING:
				*((char **)opt->ptr) = pair.value;
				break;

			default:
				NOT_IMPLEMENTED;
			}
		}

		if (!opt_found) {
			/* error: missing required option */
			return false;
		}
	}

	return true;
}

bool config_map_dispatch(struct config_map *map, char *dispatch_key,
	struct config_action *actions)
{
	NOT_IMPLEMENTED;

	struct config_pair pair;
	struct config_action *action;

	while (config_map_next_pair(map, &pair)) {
		for (action = actions; action->keyword != NULL; action++) {
		}
	}

	return true;
}
