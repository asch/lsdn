/** \file
 * Network-related structs and definitions.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "errors.h"

#if 0
/* TODO these defs are unused */
#define LSDN_BITS(x) ((x) / 8)
#define LSDN_KBITS(x) ((x) * 1000 / 8)
#define LSDN_MBITS(x) ((x) * 1000 * 1000 / 8)
#define LSDN_GBITS(x) ((x) * 1000 * 1000 * 1000 / 8)

#define LSDN_BYTES(x) (x)
#define LSDN_KBYTES(x) ((x) * 1000)
#define LSDN_MBYTES(x) ((x) * 1000 * 1000)
#define LSDN_GBYTES(x) ((x) * 1000 * 1000 * 1000)
#endif

/** MAC address size in bytes. */
#define LSDN_MAC_LEN 6
/** IPv4 address size in bytes. */
#define LSDN_IPv4_LEN 4
/** IPv6 address size in bytes. */
#define LSDN_IPv6_LEN 16

/** IP protocol version. */
enum lsdn_ipv {
	/** IPv4 */
	LSDN_IPv4 = 4,
	/** IPv6 */
	LSDN_IPv6 = 6
};

/** MAC address. */
typedef union lsdn_mac {
	/** address as `uint8_t`. */
	uint8_t bytes[LSDN_MAC_LEN];
	/** address as `char`. */
	char chr[LSDN_MAC_LEN];
} lsdn_mac_t;

/** IPv4 address. */
typedef union lsdn_ipv4 {
	/** address as `uint8_t`. */
	uint8_t bytes[LSDN_IPv4_LEN];
	/** address as `char`. */
	char chr[LSDN_IPv4_LEN];
} lsdn_ipv4_t;

/** IPv6 address. */
typedef union lsdn_ipv6 {
	/** address as `uint8_t`. */
	uint8_t bytes[LSDN_IPv6_LEN];
	/** address as `char`. */
	char chr[LSDN_IPv6_LEN];
} lsdn_ipv6_t;

/** IP address (any version). */
typedef struct lsdn_ip {
	/** IP version. */
	enum lsdn_ipv v;
	/** Actual address. */
	union {
		/** IPv4 address. */
		lsdn_ipv4_t v4;
		/** IPv6 address. */
		lsdn_ipv6_t v6;
	};
} lsdn_ip_t;

/** Construct a `lsdn_ip` IPv4 address from a 4-tuple. */
#define LSDN_MK_IPV4(a, b, c, d)\
	(struct lsdn_ip) LSDN_INITIALIZER_IPV4(a, b, c, d)
/** struct literal for a `lsdn_ip` IPv4 address constructed from a 4-tuple. */
#define LSDN_INITIALIZER_IPV4(a, b, c, d)\
	{ .v = LSDN_IPv4, .v4 = { .bytes = { (a), (b), (c), (d) } } }

/** Construct a `lsdn_ip` IPv6 address from a 16-tuple. */
#define LSDN_MK_IPV6(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)\
	(struct lsdn_ip) LSDN_INITIALIZER_IPV6(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)
/** struct literal for a `lsdn_ip` IPv6 address constructed from a 16-tuple. */
#define LSDN_INITIALIZER_IPV6(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) \
	{ .v = LSDN_IPv6, .v6 = { .bytes = { \
		(a), (b), (c), (d), \
		(e), (f), (g), (h), \
		(i), (j), (k), (l), \
		(m), (n), (o), (p) } } }

/** Construct a `lsdn_mac_t` from a 6-tuple. */
#define LSDN_MK_MAC(a, b, c, d, e, f) \
	(union lsdn_mac) LSDN_INITIALIZER_MAC(a, b, c, d, e, f)
/** struct literal for a `lsdn_mac_t` from a 6-tuple. */
#define LSDN_INITIALIZER_MAC(a, b, c, d, e, f) \
	{ .bytes = {(a), (b), (c), (d), (e), (f)}}

extern const lsdn_mac_t lsdn_broadcast_mac;
extern const lsdn_mac_t lsdn_all_zeroes_mac;
extern const lsdn_mac_t lsdn_multicast_mac_mask;
extern const lsdn_mac_t lsdn_single_mac_mask;
extern const lsdn_ip_t lsdn_single_ipv4_mask;
extern const lsdn_ip_t lsdn_single_ipv6_mask;

lsdn_err_t lsdn_parse_mac(lsdn_mac_t *mac, const char *ascii);
bool lsdn_mac_eq(lsdn_mac_t a, lsdn_mac_t b);
lsdn_err_t lsdn_parse_ip(lsdn_ip_t *ip, const char *ascii);
bool lsdn_ip_eq(lsdn_ip_t a, lsdn_ip_t b);
bool lsdn_ipv_eq(lsdn_ip_t a, lsdn_ip_t b);

/** Maximum length of MAC address string.
 * Five colons, six hexadecimal octets. */
#define LSDN_MAC_STRING_LEN (LSDN_MAC_LEN - 1 + LSDN_MAC_LEN * 2)
/** Maximum length of IPv4 address string.
 * Three dots, four decimal octets. */
#define LSDN_IPv4_STRING_LEN (LSDN_IPv4_LEN - 1 + LSDN_IPv4_LEN * 3)
/** Maximum length of IPv6 address string.
 * Seven colons, sixteen hexadecimal octets. */
#define LSDN_IPv6_STRING_LEN (LSDN_IPv6_LEN / 2 - 1 + LSDN_IPv6_LEN * 2)
/** Maximum length of address string for any IP address. */
#define LSDN_IP_STRING_LEN ((LSDN_IPv6_STRING_LEN) > (LSDN_IPv4_STRING_LEN) ? \
	(LSDN_IPv6_STRING_LEN) : (LSDN_IPv4_STRING_LEN))

void lsdn_mac_to_string(const lsdn_mac_t *mac, char *buf);
void lsdn_ipv4_to_string(const lsdn_ipv4_t *ipv4, char *buf);
void lsdn_ipv6_to_string(const lsdn_ipv6_t *ipv6, char *buf);
void lsdn_ip_to_string(const lsdn_ip_t *ip, char *buf);
lsdn_ip_t lsdn_ip_mask_from_prefix(enum lsdn_ipv v, int prefix);
bool lsdn_is_prefix_valid(enum lsdn_ipv ipv, int prefix);
int lsdn_ip_prefix_from_mask(const lsdn_ip_t *mask);
bool lsdn_ip_mask_is_valid(const lsdn_ip_t *mask);

/** Convert `lsdn_ipv4_t` to `uint32_t`.
 * @param v4 address to convert.
 * @return IP address represented as a single `uint32_t` value. */
static inline uint32_t lsdn_ip4_u32(const lsdn_ipv4_t *v4)
{
	const uint8_t *b = v4->bytes;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}
