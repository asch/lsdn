#define	DEBUG	1

#include "common.h"
#include "parser.h"
#include "strbuf.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <strings.h>
#include <yaml.h>


#define	TOKEN_TYPE_CASE(type_name)\
	case type_name:\
		return #type_name;


/**
 * @brief Set parser error state and error message.
 */
static void set_error(struct lsdn_parser *parser, char *msg, ...)
{
	va_list args;

	va_start(args, msg);
	strbuf_vprintf_at(&parser->str_error_buf, 0, msg, args);
	va_end(args);

	strbuf_append(&parser->str_error_buf, " at line %d column %d",
		parser->token.start_mark.line + 1, parser->token.start_mark.column);

	parser->parse_error = true;
}


/**
 * @brief Convert internal token type designation to a string representation.
 *	For debugging purposes.
 */
static const char *token_type_str(yaml_token_type_t type)
{
	switch (type) {
	TOKEN_TYPE_CASE(YAML_NO_TOKEN);
	TOKEN_TYPE_CASE(YAML_STREAM_START_TOKEN);
	TOKEN_TYPE_CASE(YAML_STREAM_END_TOKEN);
	TOKEN_TYPE_CASE(YAML_VERSION_DIRECTIVE_TOKEN);
	TOKEN_TYPE_CASE(YAML_TAG_DIRECTIVE_TOKEN);
	TOKEN_TYPE_CASE(YAML_DOCUMENT_START_TOKEN);
	TOKEN_TYPE_CASE(YAML_BLOCK_SEQUENCE_START_TOKEN);
	TOKEN_TYPE_CASE(YAML_BLOCK_MAPPING_START_TOKEN);
	TOKEN_TYPE_CASE(YAML_DOCUMENT_END_TOKEN);
	TOKEN_TYPE_CASE(YAML_BLOCK_END_TOKEN);
	TOKEN_TYPE_CASE(YAML_FLOW_SEQUENCE_START_TOKEN);
	TOKEN_TYPE_CASE(YAML_FLOW_SEQUENCE_END_TOKEN);
	TOKEN_TYPE_CASE(YAML_FLOW_MAPPING_START_TOKEN);
	TOKEN_TYPE_CASE(YAML_FLOW_MAPPING_END_TOKEN);
	TOKEN_TYPE_CASE(YAML_KEY_TOKEN);
	TOKEN_TYPE_CASE(YAML_VALUE_TOKEN);
	TOKEN_TYPE_CASE(YAML_ALIAS_TOKEN);
	TOKEN_TYPE_CASE(YAML_ANCHOR_TOKEN);
	TOKEN_TYPE_CASE(YAML_TAG_TOKEN);
	TOKEN_TYPE_CASE(YAML_SCALAR_TOKEN);

	default:
		return "(invalid token type)";
	}
}

/**
 * @brief Convert token to a string representation. For debugging purposes.
 */ 
static void token_str(struct lsdn_parser *parser)
{
	fprintf(stderr, "token %s", token_type_str(parser->token.type));
	if (parser->token.type == YAML_SCALAR_TOKEN)
		fprintf(stderr, ", value: %s", parser->token.data.scalar.value);

	fprintf(stderr, "\n");
}

/**
 * @brief Parse next token (and free previous token if needed.) The token is
 *	saved into &parser->token.
 */
static void next_token(struct lsdn_parser *parser)
{
	assert(parser->source_file_set);

	if (parser->delete_token) {
		yaml_token_delete(&parser->token);
	}

	yaml_parser_scan(&parser->yaml_parser, &parser->token);
	token_str(parser);

	parser->delete_token = true;
}

/**
 * @brief Is string a valid lsdn name?
 */
static bool is_valid_name(char *str)
{
	char c;
	unsigned i = 0;

	/*
	 * Expand allowed symbols as needed. The set should be as restrictive as
	 * possible to be forward-compatible with future releases.
	 */
	while ((c = str[i++]) != '\0') {
		if (!isalnum(c) && c != '_')
			return (false);
	}

	return (true);
}

void lsdn_parser_init(struct lsdn_parser *parser)
{
	yaml_parser_initialize(&parser->yaml_parser);
	strbuf_init(&parser->str_error_buf);
	parser->parse_error = false;
	parser->delete_token = false;
}

void lsdn_parser_set_file(struct lsdn_parser *parser, FILE *f)
{
	yaml_parser_set_input_file(&parser->yaml_parser, f);
	parser->source_file_set = true;
}

char *lsdn_parser_strerror(struct lsdn_parser *parser)
{
	return strbuf_copy(&parser->str_error_buf);
}

struct lsdn_network *lsdn_parse_network(struct lsdn_parser *parser)
{
	struct lsdn_network *net = NULL;
	char *netname;
	
	/*
	 * At the moment, we require one network definition per file. Network
	 * definition therefore has to start with a start-of-stream token
	 * (aka YAML_STREAM_START_TOKEN).
	 */
	next_token(parser);
	if (parser->token.type != YAML_STREAM_START_TOKEN) {
		set_error(parser, "expected start of stream");
		goto error;
	}

	next_token(parser);
	if (parser->token.type != YAML_BLOCK_MAPPING_START_TOKEN) {
		set_error(parser, "network block expected, "
			"maybe you forgot a ':' after network name?");
		goto error;
	}

	next_token(parser);
	if (parser->token.type != YAML_KEY_TOKEN) {
		set_error(parser, "network name expected");
		goto error;
	}

	//match_token_or_error(parser, YAML_KEY_TOKEN, "network name expected");

	next_token(parser);
	if (parser->token.type != YAML_SCALAR_TOKEN) {
		set_error(parser, "network name expected");
		goto error;
	}

	netname = (char *)parser->token.data.scalar.value;
	if (!is_valid_name(netname)) {
		set_error(parser, "'%s' is not a valid network name "
			"(@see is_valid_name(char *str)) in %s",
			netname, __FILE__);
	}

	net = lsdn_network_new(netname);

	next_token(parser);
	if (parser->token.type != YAML_VALUE_TOKEN) {
		set_error(parser, "network block expected");
		goto error;
	}

	next_token(parser);
	if (parser->token.type != YAML_BLOCK_MAPPING_START_TOKEN) {
		set_error(parser, "network block expected");
		goto error;
	}

	while (1) {
		next_token(parser);

		if (parser->token.type == YAML_BLOCK_END_TOKEN)
			break;

		/*
		 * At the moment, only key: value pairs are supported in the
		 * network definition block. Valueless directives may be
		 * supported later if we need them.
		 */
		if (parser->token.type != YAML_KEY_TOKEN) {
			set_error(parser, "missing key, did you miss a ':'?");
			goto error;
		}

		switch (parser->token.type) {
		case YAML_VALUE_TOKEN:
			break;

		case YAML_KEY_TOKEN:
			break;

		default:
			set_error(parser, "%s was unexpected",
				token_type_str(parser->token.type));
			goto error;
		}
	}

	next_token(parser);
	if (parser->token.type != YAML_STREAM_END_TOKEN) {
		set_error(parser, "end-of-stream was expected");
		goto error;
	}

	return (net);

error:
	free(net);
	return (NULL);
}

void lsdn_parser_end(struct lsdn_parser *parser)
{
	yaml_parser_delete(&parser->yaml_parser);
	if (parser->delete_token) {
		yaml_token_delete(&parser->token);
		parser->delete_token = false;
	}
	strbuf_free(&parser->str_error_buf);
}
