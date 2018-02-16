/** \file
 * Network-related routines.
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <arpa/inet.h>
#include "include/nettypes.h"

/** Broadcast MAC address.
 * Its value is FF:FF:FF:FF:FF:FF. */
const lsdn_mac_t lsdn_broadcast_mac = LSDN_INITIALIZER_MAC(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
/** Multicast MAC address.
 * Its value is 01:00:00:00:00:00. */
const lsdn_mac_t lsdn_multicast_mac_mask = LSDN_INITIALIZER_MAC(0x01, 0x00, 0x00, 0x00, 0x00, 0x00);
/** Single MAC mask.
 * Network mask that matches a single MAC address. Its value is all ones. */
const lsdn_mac_t lsdn_single_mac_mask = LSDN_INITIALIZER_MAC(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
/** All zeroes MAC mask.
 * Matches every MAC address. Its value, obviously, is all zeroes. */
const lsdn_mac_t lsdn_all_zeroes_mac = LSDN_INITIALIZER_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

/** Single IPv4 mask.
 * Network mask that matches a single IPv4 address. Its value is all ones, or a prefix 32. */
const lsdn_ip_t lsdn_single_ipv4_mask = LSDN_INITIALIZER_IPV4(0xFF, 0xFF, 0xFF, 0xFF);
/** Single IPv6 mask.
 * Network mask that matches a single IPv6 address. Its value is all ones, or a prefix 128. */
const lsdn_ip_t lsdn_single_ipv6_mask = LSDN_INITIALIZER_IPV6(
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);

/** Convert hexadecimal string to `uint8_t` number.
 * Parses an octet and advances the `ascii` pointer beyond the octet value.
 * Used as "token eater" in MAC parsing function.
 * @param[inout] ascii pointer to a string.
 * @param[out] dst parsed octet value.
 * @retval LSDNE_OK after successfully parsing an octet.
 * @retval LSDNE_PARSE if the first two characters of `ascii` are not a hexadecimal number. */
static lsdn_err_t parse_octet(const char ** ascii, uint8_t *dst)
{
	if ((*ascii)[0] == 0 || (*ascii)[1] == 0)
		return 0;

	char octet_ascii[3] = {(*ascii)[0], (*ascii)[1], 0};
	char *end;
	uint8_t value = (uint8_t) strtoul(octet_ascii, &end, 16);
	if (*end != 0)
		return LSDNE_PARSE;

	*ascii += 2;
	*dst = value;
	return LSDNE_OK;

}

/** Parse MAC address string into #lsdn_mac_t.
 * @param[in] ascii MAC address string.
 * @param[out] mac parsed MAC address struct.
 * @retval LSDNE_OK if parsed successfully.
 * @retval LSDNE_PARSE if `ascii` could not be parsed into a MAC address. */
lsdn_err_t lsdn_parse_mac(lsdn_mac_t *mac, const char *ascii)
{
	for (int i = 0; i < LSDN_MAC_LEN; i++) {
		if (parse_octet(&ascii, &mac->bytes[i]) != LSDNE_OK)
			return LSDNE_PARSE;

		if (*ascii == ':')
			ascii++;
	}
	if(*ascii)
		return LSDNE_PARSE;
	return LSDNE_OK;
}

/** Compare two #lsdn_mac_t for equality.
 * @param a MAC address.
 * @param b MAC address.
 * @return `true` if `a` and `b` are equal, `false` otherwise. */
bool lsdn_mac_eq(lsdn_mac_t a, lsdn_mac_t b)
{
	return memcmp(a.bytes, b.bytes, LSDN_MAC_LEN) == 0;
}

/** Parse IP address string into #lsdn_ip_t.
 * @param[in] ascii IP address string.
 * @param[out] ip parsed IP address struct.
 * @retval #LSDNE_OK if parsed successfully.
 * @retval #LSDNE_PARSE if `ascii` could not be parsed into an IP address. */
lsdn_err_t lsdn_parse_ip(lsdn_ip_t *ip, const char *ascii)
{
	int res = 0;
	if(strchr(ascii, '.')) {
		ip->v = LSDN_IPv4;
		res = inet_pton(AF_INET, ascii, ip->v4.bytes);
	}else if(strchr(ascii, ':')) {
		ip->v = LSDN_IPv6;
		res = inet_pton(AF_INET6, ascii, ip->v6.bytes);
	}
	return res == 1 ? LSDNE_OK : LSDNE_PARSE;
}

/** Compare two #lsdn_ipv4_t for equality.
 * @param a IPv4 address.
 * @param b IPv4 address.
 * @return `true` if `a` and `b` are equal, `false` otherwise. */
static bool lsdn_ipv4_eq(lsdn_ipv4_t a, lsdn_ipv4_t b)
{
	return memcmp(a.bytes, b.bytes, LSDN_IPv4_LEN) == 0;
}

/** Compare two #lsdn_ipv6_t for equality.
 * @param a IPv6 address.
 * @param b IPv6 address.
 * @return `true` if `a` and `b` are equal, `false` otherwise. */
static bool lsdn_ipv6_eq(lsdn_ipv6_t a, lsdn_ipv6_t b)
{
	return memcmp(a.bytes, b.bytes, LSDN_IPv6_LEN) == 0;
}

/** Compare two #lsdn_ip_t for equality.
 * @param a IP address.
 * @param b IP address.
 * @return `true` if `a` and `b` are equal, `false` otherwise. */
bool lsdn_ip_eq(lsdn_ip_t a, lsdn_ip_t b)
{
	if(a.v != b.v)
		return false;

	if(a.v == LSDN_IPv4)
		return lsdn_ipv4_eq(a.v4, b.v4);
	else
		return lsdn_ipv6_eq(a.v6, b.v6);
}

/** Compare two #lsdn_ip_t for IP version equality.
 * @param a IP address.
 * @param b IP address.
 * @return `true` if both `a` and `b` are of the same IP version, `false` otherwise. */
bool lsdn_ipv_eq(lsdn_ip_t a, lsdn_ip_t b)
{
	return a.v == b.v;
}

/** Format #lsdn_mac_t as ASCII string.
 * `buf` must be able to hold at least #LSDN_MAC_STRING_LEN bytes.
 * @param[in] mac MAC address.
 * @param[out] buf destination buffer for the ASCII string. */
void lsdn_mac_to_string(const lsdn_mac_t *mac, char *buf)
{
	sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", mac->bytes[0], mac->bytes[1], mac->bytes[2],
			mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

/** Format #lsdn_ipv4_t as ASCII string.
 * `buf` must be able to hold at least #LSDN_IPv4_STRING_LEN bytes.
 * @param[in] ipv4 IPv4 address.
 * @param[out] buf destination buffer for the ASCII string. */
void lsdn_ipv4_to_string(const lsdn_ipv4_t *ipv4, char *buf)
{
	sprintf(buf,
		"%d.%d.%d.%d",
		ipv4->bytes[0], ipv4->bytes[1],
		ipv4->bytes[2], ipv4->bytes[3]);
}

/** Format #lsdn_ipv6_t as ASCII string.
 * `buf` must be able to hold at least #LSDN_IPv6_STRING_LEN bytes.
 * @param[in] ipv6 IPv6 address.
 * @param[out] buf destination buffer for the ASCII string. */
void lsdn_ipv6_to_string(const lsdn_ipv6_t *ipv6, char *buf)
{
	sprintf(buf,
		"%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
		"%02x%02x:%02x%02x:%02x%02x:%02x%02x",
		ipv6->bytes[0], ipv6->bytes[1],
		ipv6->bytes[2], ipv6->bytes[3],
		ipv6->bytes[4], ipv6->bytes[5],
		ipv6->bytes[6], ipv6->bytes[7],
		ipv6->bytes[8], ipv6->bytes[9],
		ipv6->bytes[10], ipv6->bytes[11],
		ipv6->bytes[12], ipv6->bytes[13],
		ipv6->bytes[14], ipv6->bytes[15]);
}

/** Format #lsdn_ip_t as ASCII string.
 * `buf` must be able to hold at least #LSDN_IP_STRING_LEN bytes.
 * @param[in] ip IP address.
 * @param[out] buf destination buffer for the ASCII string. */
void lsdn_ip_to_string(const lsdn_ip_t *ip, char *buf)
{
	if (ip->v == LSDN_IPv4)
		lsdn_ipv4_to_string(&ip->v4, buf);
	else
		lsdn_ipv6_to_string(&ip->v6, buf);
}

/** Generate an address mask in a byte array.
 * Sets `prefix` leading bits of the array to 1 and leaves the rest at 0.
 * Used by #lsdn_ip_mask_from_prefix to generate IP address masks.
 * @param[in] prefix Number of leading bits to set to 1
 * @param[out] dst Byte array for the result. */
static void gen_prefix(uint8_t *dst, int prefix)
{
	for (; prefix > 0; prefix -= 8, dst++) {
		int byte_prefix = prefix;
		if (byte_prefix > 8)
			byte_prefix = 8;
		int zero_suffix = (8 - byte_prefix);
		*dst = ~((1 << zero_suffix) - 1);
	}
}

/** Return an IPv4/6 address mask for the given network prefix.
 * Sets `prefix` leading bits to 1 and leaves the rest at 0.
 * For example, `lsdn_ipv4_prefix_mask(LSDN_IPV4, 24)` generates address `255.255.255.0`.
 * @param v IP version.
 * @param prefix network prefix -- number of leading bits to set to 1.
 * @return IP address mask of the appropriate version. */
lsdn_ip_t lsdn_ip_mask_from_prefix(enum lsdn_ipv v, int prefix)
{
	lsdn_ip_t ip;
	bzero(&ip, sizeof(ip));
	ip.v = v;
	assert(lsdn_is_prefix_valid(v, prefix));
	switch (v) {
	case LSDN_IPv4:
		gen_prefix(ip.v4.bytes, prefix);
		break;
	case LSDN_IPv6:
		gen_prefix(ip.v6.bytes, prefix);
		break;
	default:
		abort();
	}
	return ip;
}

/** Check if the given IP address is a valid address mask.
 * A valid network mask is an IP address which, in bits, is a sequence of 1s followed by a sequence of 0s.
 * @param mask address to check.
 * @return `true` if the IP address is a valid mask, `false` otherwise. */
bool lsdn_ip_mask_is_valid(const lsdn_ip_t *mask)
{
	size_t LEN;
	const uint8_t *bytes;
	if (mask->v == LSDN_IPv4) {
		LEN = LSDN_IPv4_LEN;
		bytes = mask->v4.bytes;
	} else {
		LEN = LSDN_IPv6_LEN;
		bytes = mask->v6.bytes;
	}
	for (size_t i = 0; i < LEN; i++) {
		if (bytes[i] == 0xFF)
			continue;

		uint8_t pow = 1;
		uint8_t pref = 0xFF - pow;
		bool ok = false;
		for (size_t j = 0; j < 7; j++) {
			if (bytes[i] == pref)
				ok = true;
			pow *= 2;
			pref -= pow;
		}
		if (!ok)
			return false;

		for (size_t j = i + 1; j < LEN; j++)
			if (bytes[j] != 0)
				return false;
		return true;
	}
	return true;
}

/** Calculate length of prefix from a network mask.
 * Prefix is a number of leading 1 bits in the mask.
 * @param mask network mask to check.
 * @return number of leading 1 bits. */
int lsdn_ip_prefix_from_mask(const lsdn_ip_t *mask)
{
	assert(lsdn_ip_mask_is_valid(mask));
	int prefix = 0;
	size_t LEN;
	const uint8_t *bytes;
	if (mask->v == LSDN_IPv4) {
		LEN = LSDN_IPv4_LEN;
		bytes = mask->v4.bytes;
	} else {
		LEN = LSDN_IPv6_LEN;
		bytes = mask->v6.bytes;
	}
	for (size_t i = 0; i < LEN; i++) {
		for (size_t j = 0; j < 8; j++)
			if (bytes[i] & (1 << j))
				prefix++;
	}
	return prefix;
}

/** Check if the size of network prefix makes sense for given ip version.
 * Prefix size must not exceed number of bits of the given address.
 * @param ipv IP version.
 * @param prefix network prefix to check.
 * @return `true` if `prefix` makes sense for the IP version, `false` otherwise. */
bool lsdn_is_prefix_valid(enum lsdn_ipv ipv, int prefix)
{
	switch (ipv) {
		case LSDN_IPv4:
			return prefix >= 0 && prefix <= 32;
		case LSDN_IPv6:
			return prefix >= 0 && prefix <= 128;
		default:
			abort();
	}
}
