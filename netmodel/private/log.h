/** \file
 * Logging related structs and definitions. */
#pragma once

#include <stdarg.h>
#include <stdbool.h>

#define lsdn_foreach_log_category(x) \
	/** Network operations. */ \
	x(NETOPS, "netops") \
	/** TC rules. */ \
	x(RULES, "rules") \
	/** Guard value. */ \
	x(MAX, NULL)

/** \var LSDNL_MAX
 * By placing the MAX definition at end, an `int` conversion of this value
 * will yield the number of defined categories. XXX do not use. */

#define lsdn_netops_enum(id, string) LSDNL_##id ,

/** Log category. */
enum lsdn_log_category {
	lsdn_foreach_log_category(lsdn_netops_enum)
};


void lsdn_log_init();
void lsdn_log(enum lsdn_log_category category, const char *format, ...);
bool lsdn_log_enabled(enum lsdn_log_category category);
void lsdn_vlog(enum lsdn_log_category category, const char *format, va_list arg);
const char* lsdn_log_category_name(enum lsdn_log_category category);

/** Return original string or "*".
 * @param[in] nullable String or NULL.
 * @return `nullable` if it is not NULL.
 * @return constant string "*" if `nullable` is NULL. */
static inline const char* lsdn_nullable(const char *nullable) {
	return nullable ? nullable : "*";
}
