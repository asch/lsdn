#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "include/nettypes.h"

const lsdn_mac_t lsdn_broadcast_mac = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};
const lsdn_mac_t lsdn_multicast_mac_mask = {
	{0x01, 0x00, 0x00, 0x00, 0x00, 0x00}
};
const lsdn_mac_t lsdn_single_mac_mask = {
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
};
const lsdn_mac_t lsdn_all_zeroes_mac = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

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

lsdn_err_t lsdn_parse_mac(lsdn_mac_t *mac, const char *ascii)
{
	for (int i = 0; i<6; i++) {
		if (parse_octet(&ascii, &mac->bytes[i]) != LSDNE_OK)
			return LSDNE_PARSE;

		if (*ascii == ':')
			ascii++;
	}
	if(*ascii)
		return LSDNE_PARSE;
	return LSDNE_OK;
}

lsdn_err_t lsdn_parse_ip(lsdn_ip_t *ip, const char *ascii)
{
	if(strchr(ascii, '.')) {
		ip->v = LSDN_IPv4;
		return lsdn_parse_ipv4(&ip->v4, ascii);
	}else if(strchr(ascii, ':')) {
		ip->v = LSDN_IPv6;
		return lsdn_parse_ipv6(&ip->v6, ascii);
	}else
		return LSDNE_PARSE;
}

static lsdn_err_t parse_ipv4_byte(const char **ascii, uint8_t *dst) {
	const char* start = *ascii;
	while(isdigit(**ascii))
		(*ascii)++;

	if(*ascii - start > 3 || *ascii == start)
		return LSDNE_PARSE;
	*dst = atoi(start);
	if(*dst > 0xFF)
		return LSDNE_PARSE;
	return LSDNE_OK;
}

lsdn_err_t lsdn_parse_ipv4(lsdn_ipv4_t *ip, const char *ascii)
{
	for(int i = 0; i<4; i++) {
		if(parse_ipv4_byte(&ascii, &ip->bytes[i]) != LSDNE_OK)
			return LSDNE_PARSE;
		if(i < 3) {
			if(*(ascii++) != '.')
				return LSDNE_PARSE;
		}
	}
	if(*ascii)
		return LSDNE_PARSE;
	return LSDNE_OK;
}

lsdn_err_t lsdn_parse_ipv6(lsdn_ipv6_t *ip, const char *ascii)
{
	/* rfc5952 */
	/* TODO: parse */
	return LSDNE_PARSE;
}

bool lsdn_ip_eq(lsdn_ip_t a, lsdn_ip_t b)
{
	if(a.v != b.v)
		return false;

	if(a.v == LSDN_IPv4)
		return lsdn_ipv4_eq(a.v4, a.v4);
	else
		return lsdn_ipv6_eq(a.v6, a.v6);
}

bool lsdn_ipv4_eq(lsdn_ipv4_t a, lsdn_ipv4_t b)
{
	return memcmp(a.bytes, b.bytes, 4) == 0;
}

bool lsdn_ipv6_eq(lsdn_ipv6_t a, lsdn_ipv6_t b)
{
	return memcmp(a.bytes, b.bytes, 16) == 0;
}

void lsdn_mac_to_string(const lsdn_mac_t *mac, char *buf)
{
	sprintf(buf, "%02x:%02x:%02x:%02x:%02x:%02x", mac->bytes[0], mac->bytes[1], mac->bytes[2],
			mac->bytes[3], mac->bytes[4], mac->bytes[5]);
}

void lsdn_ip_to_string(const lsdn_ip_t *ip, char *buf)
{
	if (ip->v == LSDN_IPv4)
		sprintf(buf, "%d.%d.%d.%d", ip->v4.bytes[0], ip->v4.bytes[1], ip->v4.bytes[2],
			ip->v4.bytes[3]);
	else
		; /* TODO */
}
