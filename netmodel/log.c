/** \file
 * Logging related functions. */
#define __STDC_WANT_LIB_EXT1__ 1
#include "private/log.h"
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/** Log mask.
 * Valid values... XXX */
typedef uint32_t log_mask_t;

#define lsdn_netops_name(id, string) string,
/** Names for each log category.
 * @see lsdn_log_category */
LSDN_ENUM_NAMES(log_category);

/** Mutex for logging. */
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
/** `pthread_once` control for #log_mask_from_env.
 * Ensures that #log_mask_from_env is only called once per process. */
static pthread_once_t once_log_mask_from_env = PTHREAD_ONCE_INIT;
/** Default log mask.
 * XXX i don't know what it does. */
static log_mask_t mask = 0;

/* XXX not used anywhere */
#define MAX_ENV_LEN 100

/** Preconfigure log mask based on environment variables.
 * Reads the `LSDN_DEBUG` value from environment. If the value is
 * `"all"`, enables logging of all categories. Otherwise enables
 * logging for specified categories, separated by commas.
 *
 * @see #lsdn_log_category for possible values. The `LSDN_DEBUG` value
 * is `"netops"` for #LSDNL_NETOPS and so on. */
static void log_mask_from_env()
{
	/* yes getenv is not thread safe, but there is not really any way around it */
	const char* env_orig = getenv("LSDN_DEBUG");
	if (!env_orig)
		return;
	char* env = strdup(env_orig);
	if (!env)
		return;

	char *tok_state;
	char *tok = strtok_r(env, ",", &tok_state);
	while (tok) {
		bool found = false;
		if (strcmp(tok, "all") == 0) {
			mask |= (1 << LSDNL_COUNT) - 1;
			found  = true;
		}
		for(size_t i = 0; i<LSDNL_COUNT && !found; i++) {
			if (strcmp(log_category_names[i], tok) == 0) {
				mask |= 1 << i;
				found = true;
			}
		}
		if (!found) {
			fprintf(stderr, "Unknown LSDN_DEBUG value: %s\n", tok);
			abort();
		}
		tok = strtok_r(NULL, ",", &tok_state);
	}

	free(env);
}

/** Initialize logging service.
 * Ensures that the `LSDN_DEBUG` environment variable is read
 * and the global logging mask set according to it. */
void lsdn_log_init()
{
	pthread_once(&once_log_mask_from_env, log_mask_from_env);
}

/** Convert #lsdn_log_category value to its name.
 * @param[in] category Log category. 
 * @return Constant string containing the name of the category. */
const char* lsdn_log_category_name(enum lsdn_log_category category)
{
	return log_category_names[category];
}

/** Log an event.
 * The call syntax is `sprintf`-like.
 * @param[in] category Category for this log message.
 * @param[in] format `sprintf`-like format string.
 * @param[in] ... arguments for the format string. */
void lsdn_log(enum lsdn_log_category category, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	lsdn_vlog(category, format, args);
	va_end(args);
}

/** Check if logging is enabled for a given category.
 * @param[in] category Category for which to check.
 * @retval true if logging is enabled for `category`.
 * @retval false if logging is disabled for `category`. */
bool lsdn_log_enabled(enum lsdn_log_category category)
{
	return ((1 << category) & mask);
}

/** Perform the logging action.
 * XXX no reason for this to exist */
void lsdn_vlog(enum lsdn_log_category category, const char* format, va_list args)
{
	lsdn_log_init();
	if (!lsdn_log_enabled(category))
		return;

	pthread_mutex_lock(&log_mutex);
	fputs("LS-", stderr);
	fputs(lsdn_log_category_name(category), stderr);
	fputs(": ", stderr);
	vfprintf(stderr, format, args);
	pthread_mutex_unlock(&log_mutex);
}
