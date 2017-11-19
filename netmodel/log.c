#define __STDC_WANT_LIB_EXT1__ 1
#include "private/log.h"
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef uint32_t log_mask_t;

#define lsdn_netops_name(id, string) string,
static const char* netops_names[] = {
	lsdn_foreach_log_category(lsdn_netops_name)
};

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once_log_mask_from_env = PTHREAD_ONCE_INIT;
static log_mask_t mask = 0;

#define MAX_ENV_LEN 100
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
			mask |= (1 << LSDNL_MAX) - 1;
			found  = true;
		}
		for(size_t i = 0; i<LSDNL_MAX && !found; i++) {
			if (strcmp(netops_names[i], tok) == 0) {
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

void lsdn_log_init()
{
	pthread_once(&once_log_mask_from_env, log_mask_from_env);
}

const char* lsdn_log_category_name(enum lsdn_log_category category)
{
	return netops_names[category];
}

void lsdn_log(enum lsdn_log_category category, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	lsdn_vlog(category, format, args);
	va_end(args);
}

bool lsdn_log_enabled(enum lsdn_log_category category)
{
	return ((1 << category) & mask);
}

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
