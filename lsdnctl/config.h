#ifndef CONFIG_H
#define CONFIG_H

#include "../netmodel/include/lsdn.h"

#include <stdbool.h>
#include <yaml.h>

struct config_file;

enum config_value_type {
	CONFIG_VALUE_SCALAR,
	CONFIG_VALUE_MAP
};

struct config_map {
	yaml_node_t *node;
	yaml_node_pair_t *pair;
	size_t num_pairs;
};

struct config_pair {
	char *key;
	enum config_value_type value_type;	/* type of value stored */

	union {
		char *value;			/* for CONFIG_VALUE_SCALAR */
		struct config_map values;	/* for CONFIG_VALUE_MAP */
	};
};

struct config_file *config_file_open(char *filename);
struct config_file *config_file_open_stdin();
bool config_file_get_root_map(struct config_file *file, struct config_map *map);
void config_file_close(struct config_file *file);

bool config_map_next_pair(struct config_map *map, struct config_pair *pair);
void config_map_reset(struct config_map *map);
size_t config_map_num_pairs(struct config_map *map);

enum config_option_type {
	CONFIG_OPTION_BOOL,
	CONFIG_OPTION_INT,
	CONFIG_OPTION_MAC,
	CONFIG_OPTION_STRING
};

struct config_option {
	char *name;
	enum config_option_type type;
	void *ptr;
	bool required;
};

bool config_map_getopt(struct config_map *map, struct config_option *options);

struct config_action {
	char *keyword;
	bool (*handler)(struct config_map *map, void *arg);
	void *arg;
};

bool config_map_dispatch(struct config_map *map, char *dispatch_key,
	struct config_action *action);

#endif
