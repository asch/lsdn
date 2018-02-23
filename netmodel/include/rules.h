/** \file
 * Virt Rules engine related definitions and structs. */
#pragma once

#include "lsdn.h"
#include "nettypes.h"
#include "util.h"

/** Generator for #lsdn_rule_target.
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

/** Virt rule method generator.
 * For each of the possible virt rule targets (see) */
#define lsdn_vr_shortcuts(doc, name, type, fullmask) \
	/** Add virt rule for a specified doc.
	@param rule Pointer to virt rule.
	@param value Full address.
	@see lsdn_vr_shortcuts. */ \
	static inline void lsdn_vr_add_##name(struct lsdn_vr *rule, type value) \
	{ \
		lsdn_vr_add_masked_##name(rule, value, (fullmask)); \
	} \
	/** Create virt rule for doc with a mask.
	@param virt LSDN virt.
	@param dir Incoming or outgoing rule.
	@param prio Rule priority.
	@param value Address value.
	@param mask Mask value.
	@param action Rule action.
	@return New lsdn_vr struct. Caller is responsible for freeing it.
	@see lsdn_vr_shortcuts. */ \
	static inline struct lsdn_vr *lsdn_vr_new_masked_##name( \
		struct lsdn_virt *virt, enum lsdn_direction dir, uint16_t prio, \
		type value, type mask, struct lsdn_vr_action *action) \
	{ \
		struct lsdn_vr *vr = lsdn_vr_new(virt, prio, dir, action); \
		lsdn_vr_add_masked_##name(vr, value, mask); \
		return vr; \
	} \
	/** Create virt rule for a specified doc.
	@param virt LSDN virt.
	@param dir Incoming or outgoing rule.
	@param prio Rule priority.
	@param value Full address.
	@param action Rule action.
	@return New lsdn_vr struct. Caller is responsible for freeing it.
	@see lsdn_vr_shortcuts. */ \
	static inline struct lsdn_vr *lsdn_vr_new_##name( \
		struct lsdn_virt *virt, enum lsdn_direction dir, \
		uint16_t prio, type value, struct lsdn_vr_action *action)  \
	{ \
		return lsdn_vr_new_masked_##name(virt, prio, dir, value, (fullmask), action); \
	}


/** @defgroup rules Rules engine
 * @{ */
/** Rule target. */
LSDN_ENUM(rule_target, LSDN_MATCH);

#define LSDN_MAX_MATCHES 2

/** Minimum Virt Rule priority. */
#define LSDN_VR_PRIO_MIN 0
/** Upper limit for Virt Rule priority.
 * Actual priority must be strictly lower than this. */
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

extern struct lsdn_vr_action LSDN_VR_DROP;

/** Add virt rule for source MAC with a mask. */
void lsdn_vr_add_masked_src_mac(struct lsdn_vr *rule, lsdn_mac_t mask, lsdn_mac_t value);
lsdn_vr_shortcuts(source MAC, src_mac, lsdn_mac_t, lsdn_single_mac_mask)

/** Add virt rule for destination MAC with a mask. */
void lsdn_vr_add_masked_dst_mac(struct lsdn_vr *rule, lsdn_mac_t mask, lsdn_mac_t value);
lsdn_vr_shortcuts(destination MAC, dst_mac, lsdn_mac_t, lsdn_single_mac_mask)

/** Add virt rule for source IP with a mask. */
void lsdn_vr_add_masked_src_ip(struct lsdn_vr *rule, lsdn_ip_t mask, lsdn_ip_t value);
lsdn_vr_shortcuts(source IP, src_ip, lsdn_ip_t, (value.v == LSDN_IPv4) ? lsdn_single_ipv4_mask : lsdn_single_ipv6_mask)

/** Add virt rule for destination IP with a mask. */
void lsdn_vr_add_masked_dst_ip(struct lsdn_vr *rule, lsdn_ip_t mask, lsdn_ip_t value);
lsdn_vr_shortcuts(destination IP, dst_ip, lsdn_ip_t, (value.v == LSDN_IPv4) ? lsdn_single_ipv4_mask : lsdn_single_ipv6_mask)

/** @} */

#undef lsdn_vr_shortcuts
