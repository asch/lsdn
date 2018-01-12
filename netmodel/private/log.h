/** \file
 * Logging related structs and definitions. */
#pragma once

#include "../include/util.h"

#include <stdarg.h>
#include <stdbool.h>

/** Generator macro for log categories.
 * Used to keep consistency between possible values of `lsdn_log_category`
 * and their string names in `netops_names`. */
#define lsdn_enumgen_log_category(x) \
	/** Network operations. */ \
	x(LSDNL_NETOPS, "netops") \
	/** TC rules. */ \
	x(LSDNL_RULES, "rules") \
	/** Netlink and other low-level failures */ \
	x(LSDN_NLERR, "nlerr")

/** Log category. */
LSDN_ENUM(log_category, LSDNL);


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
