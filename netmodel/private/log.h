#ifndef LSDN_LOG_H
#define LSDN_LOG_H

#include <stdarg.h>

#define lsdn_foreach_log_category(x) \
	x(NETOPS, "netops") \
	x(RULES, "rules") \
	x(MAX, NULL)

#define lsdn_netops_enum(id, string) LSDNL_##id ,

enum lsdn_log_category {
	lsdn_foreach_log_category(lsdn_netops_enum)
};

void lsdn_log_init();
void lsdn_log(enum lsdn_log_category category, const char *format, ...);
void lsdn_vlog(enum lsdn_log_category category, const char *format, va_list arg);
const char* lsdn_log_category_name(enum lsdn_log_category category);
static inline const char* lsdn_nullable(const char *nullable) {
	return nullable ? nullable : "*";
}

#endif
