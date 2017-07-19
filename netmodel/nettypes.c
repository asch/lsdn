#include <stdlib.h>
#include <stdio.h>
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
	lsdn_err_t err;
	for (int i = 0; i<6; i++)
	{
		if ((err = parse_octet(&ascii, &mac->bytes[i])) != LSDNE_OK)
			return err;

		if (*ascii == ':')
			ascii++;
	}
	return LSDNE_OK;
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
