/** \file
 * LSDN network model (de)serialization related definitions. */
#pragma once

#include "lsdn.h"

/** Dump the internal LSDN network model in JSON format.
*/
char *lsdn_dump_context_json(struct lsdn_context *ctx);

/** Convert the JSON representation of the network model
 *  into a TCL netmodel representation.
*/
char *lsdn_convert_context_json2tcl(const char *str);
