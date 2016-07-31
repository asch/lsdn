#include "config.h"
#include "common.h"
#include "strbuf.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <yaml.h>


struct config_file {
	FILE *file;			/* config file handle */
	yaml_parser_t parser;		/* YAML parser */
	yaml_document_t doc;		/* YAML document */
	yaml_node_t *root_node;		/* YAML root node */
	bool error;			/* error flag */
	struct strbuf error_str_buf;	/* error string buffer */
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

void config_file_set_error(struct config_file *config, char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	config->error = true;
	strbuf_vprintf_at(&config->error_str_buf, 0, msg, args);

	va_end(args);
}

char *config_file_get_error_string(struct config_file *config)
{
	return config->error_str_buf.str;
}

bool config_file_has_errors(struct config_file *config)
{
	return config->error;
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

	config->error = false;
	strbuf_init(&config->error_str_buf);


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

	strbuf_free(&config->error_str_buf);
	yaml_document_delete(&config->doc);
	yaml_parser_delete(&config->parser);
	fclose(config->file);
}

bool config_map_next_item(struct config_map *map, struct config_item *item)
{
	yaml_node_t *key_node, *value_node;

	assert(map != NULL);
	assert(item != NULL);

	if (map->pair == map->node->data.mapping.pairs.top)
		return false;

	key_node = yaml_document_get_node(&config_file->doc, map->pair->key);
	value_node = yaml_document_get_node(&config_file->doc, map->pair->value);

	item->key = (char *)key_node->data.scalar.value;

	switch (value_node->type) {
	case YAML_SCALAR_NODE:
		item->value_type = CONFIG_VALUE_SCALAR;
		item->value = (char *)value_node->data.scalar.value;
		break;

	case YAML_MAPPING_NODE:
		item->value_type = CONFIG_VALUE_MAP;
		load_map_from_node(&item->values, value_node);
		break;

	case YAML_SEQUENCE_NODE:
		/* TODO implement this */
		item->value_type = CONFIG_VALUE_LIST;
		break;

	default:
		DEBUG_MSG("Invalid item value type");
		return false;
	}

	map->pair++;
	return true;
}

void config_map_reset(struct config_map *map)
{
	map->pair = map->node->data.mapping.pairs.start;
	map->num_items = (map->node->data.mapping.pairs.top
		- map->node->data.mapping.pairs.start);
}

size_t config_map_num_items(struct config_map *map)
{
	return map->num_items;
}

bool config_map_get(struct config_map *map, char *key, struct config_item *item)
{	
	/* TODO O(n) time, maybe use a hashtable instead? */
	while (config_map_next_item(map, item)) {
		if (strcmp(item->key, key) == 0)
			return true;
	}

	return false;
}

bool convert_string_to_int(char *str, int *out)
{
	long ret;
	char *endptr;

	errno = 0;
	ret = strtol(str, &endptr, 10);

	if ((errno == ERANGE && (ret == LONG_MAX || ret == LONG_MIN))
		|| (errno != 0 && ret == 0) || endptr == str || *endptr != '\0') {
		config_file_set_error(config_file, "invalid int value: '%s'", str);
		return false;
	}

	*out = (int)ret;
	return true;
}

bool config_map_getopt(struct config_map *map, struct config_option *options)
{
	struct config_item item;
	struct config_option *opt;
	bool opt_found;

	for (opt = options; opt->name != NULL; opt++) {
		opt_found = false;
		config_map_reset(map);
		while (config_map_next_item(map, &item)) {
			if (strcmp(item.key, opt->name) == 0) {
				opt_found = true;
				break;
			}
		}

		if (!opt_found) {
			if (opt->required) {
				config_file_set_error(
					config_file,
					"missing required option '%s'",
					opt->name
				);

				return false;
			}
			continue;

		}

		switch (opt->type) {
		case CONFIG_OPTION_INT:
		case CONFIG_OPTION_STRING:
		case CONFIG_OPTION_MAC:
		case CONFIG_OPTION_BOOL:
			if (item.value_type != CONFIG_VALUE_SCALAR) {
				config_file_set_error(
					config_file,
					"value of '%s' has to be scalar",
					opt->name
				);
				return false;
			}
			break;
		}


		switch (opt->type) {
		case CONFIG_OPTION_INT:
			if (!convert_string_to_int(item.value, (int *)opt->ptr))
				return false;
			break;

		case CONFIG_OPTION_STRING:
			*((char **)opt->ptr) = item.value;
			break;

		case CONFIG_OPTION_MAC:
			/* TODO implement this */
			break;

		default:
			NOT_IMPLEMENTED;
		}
	}

	return true;
}

bool config_map_dispatch(struct config_map *map, char *dispatch_key,
	struct config_action *actions, bool must_dispatch_all)
{
	struct config_item item, dispatch_item;
	struct config_action *action;
	bool dispatched;
	bool dispatch_ok;

	while (config_map_next_item(map, &item)) {
		if (!config_map_get(&item.values, dispatch_key, &dispatch_item)) {
			if (must_dispatch_all) {
				config_file_set_error(config_file, "missing dispatch key '%s'",
					dispatch_key);
				return false;
			}

			continue;
		}

		if (dispatch_item.value_type != CONFIG_VALUE_SCALAR) {
			config_file_set_error(config_file, "dispatch value must be scalar");
			return false;
		}

		dispatched = false;

		for (action = actions; action->keyword != NULL; action++) {
			if (strcmp(action->keyword, dispatch_item.value) == 0) {
				dispatch_ok = action->handler(&item, action->arg); 
				dispatched = true;
			}
		}

		if (!dispatched && must_dispatch_all) {
			config_file_set_error(config_file, "no action registred for value '%s'",
				dispatch_item.value);
			return false;
		}

		if (!dispatch_ok) {
			return false;
		}
	}

	return true;
}
