/** \file
 * LSDN network model serialization related definitions. */
#pragma once

#include "lsdn.h"

/** @name Dump to various formats
 * @{ */
/** @ingroup misc */

/** Dump the internal LSDN network model in JSON format.
 * @returns
 *	A C string containing the context's representation in JSON format.
 *	Caller is responsible for deallocating the string using ``free``.
 */
char *lsdn_dump_context_json(struct lsdn_context *ctx);

/** Dump the internal LSDN network model in TCL format.
 * @returns
 *	A C string containing the context's representation in lsctl-compatible form.
 *	Caller is responsible for deallocating the string using ``free``.
 */
char *lsdn_dump_context_tcl(struct lsdn_context *ctx);

/** @} */
