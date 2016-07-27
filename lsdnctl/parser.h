#ifndef	PARSER_H
#define	PARSER_H

#include "../netmodel/include/lsdn.h"
#include "strbuf.h"

#include <assert.h>
#include <stdbool.h>
#include <yaml.h>

struct lsdn_parser {
	yaml_parser_t yaml_parser;	/* YAML parser (libyaml) */
	yaml_token_t token;		/* current YAML token (libyaml) */
	strbuf str_error_buf;		/* error string buffer */
	bool parse_error;		/* was there a parse error? */
	bool source_file_set;		/* was source file/string set? */
	bool delete_token;		/* @see next_token */
};

void lsdn_parser_init(struct lsdn_parser *parser);
struct lsdn_network *lsdn_parse_network(struct lsdn_parser *parser);
char *lsdn_parser_strerror(struct lsdn_parser *parser);
void lsdn_parser_end(struct lsdn_parser *parser);


#endif
