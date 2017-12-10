/** \file
 * LSDN network model (de)serialization related definitions. */
#pragma once

#include "../private/lsdn.h"
#include "../private/net.h"

/** Dump the internal LSDN network model in JSON format.
*/
char *lsdn_dump_context_json(struct lsdn_context *ctx);

/** Load the JSON representation of the network model
 *  into the internal LSDN model.
*/
struct lsdn_context *lsdn_load_context_json(const char *str);
