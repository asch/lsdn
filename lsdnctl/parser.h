#ifndef	PARSER_H
#define	PARSER_H

#include "../netmodel/include/lsdn.h"
#include <stdio.h>


struct lsdn_parser;

struct lsdn_parser *lsdn_parser_new(FILE *src_file);
struct lsdn_network *lsdn_parser_parse_network(struct lsdn_parser *parser);
char *lsdn_parser_get_error(struct lsdn_parser *parser);
void lsdn_parser_free(struct lsdn_parser *parser);

#endif
