#ifndef _LSDN_TC_H
#define _LSDN_TC_H

#include "rule.h"

const char *actions_for(struct lsdn_action *action);
void runcmd(const char *format, ...);

#endif
