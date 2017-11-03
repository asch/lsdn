#pragma once

#include "nl.h"

struct lsdn_virt;

struct lsdn_lbridge {
	struct lsdn_context *ctx;
	struct lsdn_if bridge_if;
};

struct lsdn_lbridge_if {
	struct lsdn_lbridge *br;
	struct lsdn_if *iface;
};

void lsdn_lbridge_init(struct lsdn_context *ctx, struct lsdn_lbridge *br);
void lsdn_lbridge_free(struct lsdn_lbridge *br);
void lsdn_lbridge_add(struct lsdn_lbridge *br, struct lsdn_lbridge_if *br_if, struct lsdn_if *iface);
void lsdn_lbridge_remove(struct lsdn_lbridge_if *iface);

void lsdn_lbridge_add_virt(struct lsdn_virt *v);
void lsdn_lbridge_remove_virt(struct lsdn_virt *v);
