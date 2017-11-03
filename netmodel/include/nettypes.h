/** \file
 * Network-related structs and definitions.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "errors.h"


enum lsdn_ethertype_{
	LSDN_ETHERTYPE_IPV4 = 0x0800,
	LSDN_ETHERTYPE_IPV6 = 0x86DD
};

/** IP protocol version. */
enum lsdn_ipv {
	/** IPv4 */
	LSDN_IPv4,
	/** IPv6 */
	LSDN_IPv6
};

/** MAC address. */
typedef union lsdn_mac {
	uint8_t bytes[6];
	char chr[6];
} lsdn_mac_t;

/** IPv4 address. */
typedef union lsdn_ipv4 {
	uint8_t bytes[4];
} lsdn_ipv4_t;

/** IPv6 address. */
typedef union lsdn_ipv6 {
	uint8_t bytes[16];
} lsdn_ipv6_t;

/** IP address (any version). */
typedef struct lsdn_ip {
	enum lsdn_ipv v;
	union{
		lsdn_ipv4_t v4;
		lsdn_ipv6_t v6;
	};
} lsdn_ip_t;

/** Construct a `lsdn_ip` IPv4 address from a 4-tuple. */
#define LSDN_MK_IPV4(a, b, c, d)\
	(struct lsdn_ip) { .v = LSDN_IPv4, .v4 = { .bytes = { (a), (b), (c), (d) } } }

extern const lsdn_mac_t lsdn_broadcast_mac;
extern const lsdn_mac_t lsdn_all_zeroes_mac;
extern const lsdn_mac_t lsdn_multicast_mac_mask;
extern const lsdn_mac_t lsdn_single_mac_mask;

lsdn_err_t lsdn_parse_mac(lsdn_mac_t *mac, const char *ascii);
lsdn_err_t lsdn_parse_ip(lsdn_ip_t *ip, const char *ascii);
lsdn_err_t lsdn_parse_ipv4(lsdn_ipv4_t *ip, const char *ascii);
lsdn_err_t lsdn_parse_ipv6(lsdn_ipv6_t *ip, const char *ascii);
bool lsdn_ip_eq(lsdn_ip_t a, lsdn_ip_t b);
bool lsdn_ipv4_eq(lsdn_ipv4_t a, lsdn_ipv4_t b);
bool lsdn_ipv6_eq(lsdn_ipv6_t a, lsdn_ipv6_t b);

/* Five colons, six octets */
#define LSDN_MAC_STRING_LEN (5 + 6 *2)

void lsdn_mac_to_string(const lsdn_mac_t *mac, char *buf);
void lsdn_ip_to_string(const lsdn_ip_t *ip, char *buf);

/** Convert `lsdn_ipv4` to `uint32_t`. */
static inline uint32_t lsdn_ip4_u32(const lsdn_ipv4_t *v4)
{
	const uint8_t *b = v4->bytes;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}
