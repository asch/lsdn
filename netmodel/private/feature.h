#include "rule.h"
#include "port.h"

/*
 * Add a rule to the ruleset that either drops the packet or continues with the processing.
 *
 * If the processing continues, return a ruleset to which it will be redirected.
 * Usually, an action of the form:
 *    if not (match) DROP
 * will be added and the same ruleset will be returned, because the processing
 * continues with the same ruleset.
 */

struct lsdn_ruleset* mk_generic_rules(struct lsdn_ruleset* r);
struct lsdn_ruleset* mk_dst_rules(struct lsdn_ruleset* r, struct lsdn_port* dst_port);
struct lsdn_ruleset* mk_src_rules(struct lsdn_ruleset* r, struct lsdn_port* src_port);
