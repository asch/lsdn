/** \file
 * Virt Rules engine related definitions and structs. */
#pragma once

#include "lsdn.h"
#include "nettypes.h"
#include "util.h"

/** Generator for `lsdn_rule_target`.
 * @see LSDN_ENUM
 * @see lsdn_rule_target */
#define lsdn_enumgen_rule_target(x) \
	/** Do not match. */ \
	x(LSDN_MATCH_NONE, "none") \
	/** Match source MAC. */ \
	x(LSDN_MATCH_SRC_MAC, "src_mac") \
	/** Match destination MAC. */ \
	x(LSDN_MATCH_DST_MAC, "dst_mac") \
	/** Match source IPv4 address. */ \
	x(LSDN_MATCH_SRC_IPV4, "src_ipv4") \
	/** Match destination IPv4 address. */ \
	x(LSDN_MATCH_DST_IPV4, "dst_ipv4") \
	/** Match source IPv6 address. */ \
	x(LSDN_MATCH_SRC_IPV6, "src_ipv6") \
	/** Match destination IPv6 address. */ \
	x(LSDN_MATCH_DST_IPV6, "dst_ipv6") \
	/** Match tunnel key ID. */ \
	x(LSDN_MATCH_ENC_KEY_ID, "enc_key_id")

/** Rule target. */
LSDN_ENUM(rule_target, LSDN_MATCH);

#define LSDN_MAX_MATCHES 2

const char* lsdn_rule_target_name(enum lsdn_rule_target t);

#define LSDN_MAX_MATCH_LEN 16
union lsdn_matchdata {
	char bytes[LSDN_MAX_MATCH_LEN];
	lsdn_mac_t mac;
	lsdn_ipv4_t ipv4;
	lsdn_ipv6_t ipv6;
	uint32_t enc_key_id;
};

#define LSDN_VR_PRIO_MIN 0
/* exclusive */
#define LSDN_VR_PRIO_MAX 0x8000

/** Use this priority if you want your rule to take place during forwarding decisions. */
#define LSDN_PRIO_FORWARD_DST_MAC (LSDN_VR_PRIO_MAX+1)

/* lsdn virt's rule */
struct lsdn_vr;
struct lsdn_vr_action;

struct lsdn_vr *lsdn_vr_new(
	struct lsdn_virt *virt, uint16_t prio, enum lsdn_direction dir, struct lsdn_vr_action *a);
void lsdn_vr_free(struct lsdn_vr *vr);
void lsdn_vrs_free_all(struct lsdn_virt *virt);

extern struct lsdn_vr_action lsdn_vr_drop;

#define lsdn_vr_shortcuts(name, type, fullmask) \
	static inline void lsdn_vr_add_##name(struct lsdn_vr *rule, type value) \
	{ \
		lsdn_vr_add_masked_##name(rule, value, (fullmask)); \
	} \
	static inline struct lsdn_vr *lsdn_vr_new_masked_##name( \
		struct lsdn_virt *virt, enum lsdn_direction dir, uint16_t prio, \
		type value, type mask, struct lsdn_vr_action *action) \
	{ \
		struct lsdn_vr *vr = lsdn_vr_new(virt, prio, dir, action); \
		lsdn_vr_add_masked_##name(vr, value, mask); \
		return vr; \
	} \
	static inline struct lsdn_vr *lsdn_vr_new_##name( \
		struct lsdn_virt *virt, enum lsdn_direction dir, \
		uint16_t prio, type value, struct lsdn_vr_action *action)  \
	{ \
		return lsdn_vr_new_masked_##name(virt, prio, dir, value, (fullmask), action); \
	}

void lsdn_vr_add_masked_src_mac(struct lsdn_vr *rule, lsdn_mac_t mask, lsdn_mac_t value);
lsdn_vr_shortcuts(src_mac, lsdn_mac_t, lsdn_single_mac_mask)

void lsdn_vr_add_masked_dst_mac(struct lsdn_vr *rule, lsdn_mac_t mask, lsdn_mac_t value);
lsdn_vr_shortcuts(dst_mac, lsdn_mac_t, lsdn_single_mac_mask)

void lsdn_vr_add_masked_src_ip(struct lsdn_vr *rule, lsdn_ip_t mask, lsdn_ip_t value);
lsdn_vr_shortcuts(src_ip, lsdn_ip_t, (value.v == LSDN_IPv4) ? lsdn_single_ipv4_mask : lsdn_single_ipv6_mask)

void lsdn_vr_add_masked_dst_ip(struct lsdn_vr *rule, lsdn_ip_t mask, lsdn_ip_t value);
lsdn_vr_shortcuts(dst_ip, lsdn_ip_t, (value.v == LSDN_IPv4) ? lsdn_single_ipv4_mask : lsdn_single_ipv6_mask)

#undef lsdn_vr_shortcuts
