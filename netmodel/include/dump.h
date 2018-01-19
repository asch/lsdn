/** \file
 * LSDN network model serialization related definitions. */
#pragma once

#include "lsdn.h"

/** Dump the internal LSDN network model in JSON format.
*/
char *lsdn_dump_context_json(struct lsdn_context *ctx);

/** Dump the internal LSDN network model in TCL format.
*/
char *lsdn_dump_context_tcl(struct lsdn_context *ctx);
