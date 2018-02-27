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
 * For each of the possible virt rule match targets (see #lsdn_rule_target), the shortcuts
 * define inline functions to:
 * * set the rule to match a given value
 * * create a new rule matching the prefilled value
 * * create a new rule matching value + mask.
 * These are defined in terms of the basic action, `lsdn_vr_add_<name>_masked`,
 * which configures the rule to match value + mask.
 * @param doc Human-readable name of the rule target ("IP address").
 * @param name Symbolic name of the rule target ("ip").
 * @param type Type of the value ("lsdn_ip_t").
 * @param fullmask Value of a full mask, i.e., all ones. */
#define lsdn_vr_shortcuts(doc, name, type, fullmask) \
	/** Configure virt rule to match a specified doc.
	@param rule Pointer to virt rule.
	@param value Match value. */ \
	static inline void lsdn_vr_add_##name(struct lsdn_vr *rule, type value) \
	{ \
		lsdn_vr_add_masked_##name(rule, value, (fullmask)); \
	} \
	/** Create virt rule matching doc with a mask.
	@param virt LSDN virt.
	@param dir Incoming or outgoing rule.
	@param prio Rule priority.
	@param value Match value.
	@param mask Mask value.
	@param action Rule action.
	@return New #lsdn_vr struct. */ \
	static inline struct lsdn_vr *lsdn_vr_new_masked_##name( \
		struct lsdn_virt *virt, enum lsdn_direction dir, uint16_t prio, \
		type value, type mask, struct lsdn_vr_action *action) \
	{ \
		struct lsdn_vr *vr = lsdn_vr_new(virt, prio, dir, action); \
		lsdn_vr_add_masked_##name(vr, value, mask); \
		return vr; \
	} \
	/** Create virt rule matching a specified doc.
	@param virt LSDN virt.
	@param dir Incoming or outgoing rule.
	@param prio Rule priority.
	@param value Match value.
	@param action Rule action.
	@return New #lsdn_vr struct. */ \
	static inline struct lsdn_vr *lsdn_vr_new_##name( \
		struct lsdn_virt *virt, enum lsdn_direction dir, \
		uint16_t prio, type value, struct lsdn_vr_action *action)  \
	{ \
		return lsdn_vr_new_masked_##name(virt, prio, dir, value, (fullmask), action); \
	}


/** @defgroup rules Rules engine
 * Virt rules engine configuration.
 *
 * LSDN supports a basic firewall filtering. It is possible to set up packet
 * rules matching several criteria (source or destination addresses or ranges,
 * tunnel key ID) and assign them to inbound or outbound queues of a particular
 * virt. Currently, the firewall can only drop matching packets. There is no
 * support for creating custom firewall actions.
 *
 * #lsdn_vr is its own kind of object, tied to a virt. It can either be created
 * preconfigured to match something, or set as empty and configured later.
 *
 * Rules are evaluated in order of increasing priority. The lower the priority
 * value, the higher the actual priority.
 * @{ */

/** Rule target. */
LSDN_ENUM(rule_target, LSDN_MATCH);

/** Maximum number of match targets per rule.
 * In this implementation, a rule can match on at most
 * two simultaneous objects (e.g. MAC address and IPv4 address). */
#define LSDN_MAX_MATCHES 2

/** Minimum Virt Rule priority. */
#define LSDN_VR_PRIO_MIN 0
/** Upper limit for Virt Rule priority.
 * Actual priority must be strictly lower than this. */
#define LSDN_VR_PRIO_MAX 0x8000

/** Use this priority if you want your rule to take place during forwarding decisions. */
#define LSDN_PRIO_FORWARD_DST_MAC (LSDN_VR_PRIO_MAX+1)

/** Virt rule.
 * @struct lsdn_vr
 * @ingroup rules
 * Represents a packet rule assigned to a virt. The rule has a priority, assigned direction
 * (affecting incoming or outgoing packets), and an action. Usually, the rule will also
 * have match conditions, such as IP or MAC address mask.
 * @see lsdn_vr_new
 * @rstref{capi/rules} */
struct lsdn_vr;

/** Virt rule action.
 * @struct lsdn_vr_action
 * @ingroup rules
 * Represents an action to be performed on a packet that matches a rule.
 *
 * In this version, the only possible action is #LSDN_VR_DROP. */
struct lsdn_vr_action;

struct lsdn_vr *lsdn_vr_new(
	struct lsdn_virt *virt, uint16_t prio, enum lsdn_direction dir, struct lsdn_vr_action *a);
void lsdn_vr_free(struct lsdn_vr *vr);
void lsdn_vrs_free_all(struct lsdn_virt *virt);

extern struct lsdn_vr_action LSDN_VR_DROP;

/** Configure virt rule to match source MAC with a mask.
 * @param rule Virt rule.
 * @param mask Mask value.
 * @param value Match value. */
void lsdn_vr_add_masked_src_mac(struct lsdn_vr *rule, lsdn_mac_t mask, lsdn_mac_t value);
lsdn_vr_shortcuts(source MAC, src_mac, lsdn_mac_t, lsdn_single_mac_mask)

/** Configure virt rule to match destination MAC with a mask.
 * @param rule Virt rule.
 * @param mask Mask value.
 * @param value Match value. */
void lsdn_vr_add_masked_dst_mac(struct lsdn_vr *rule, lsdn_mac_t mask, lsdn_mac_t value);
lsdn_vr_shortcuts(destination MAC, dst_mac, lsdn_mac_t, lsdn_single_mac_mask)

/** Configure virt rule to match source IP address with a mask.
 * @param rule Virt rule.
 * @param mask Mask value.
 * @param value Match value. */
void lsdn_vr_add_masked_src_ip(struct lsdn_vr *rule, lsdn_ip_t mask, lsdn_ip_t value);
lsdn_vr_shortcuts(source IP, src_ip, lsdn_ip_t, (value.v == LSDN_IPv4) ? lsdn_single_ipv4_mask : lsdn_single_ipv6_mask)

/** Configure virt rule to match destination IP address with a mask.
 * @param rule Virt rule.
 * @param mask Mask value.
 * @param value Match value. */
void lsdn_vr_add_masked_dst_ip(struct lsdn_vr *rule, lsdn_ip_t mask, lsdn_ip_t value);
lsdn_vr_shortcuts(destination IP, dst_ip, lsdn_ip_t, (value.v == LSDN_IPv4) ? lsdn_single_ipv4_mask : lsdn_single_ipv6_mask)

/** @} */

#undef lsdn_vr_shortcuts
