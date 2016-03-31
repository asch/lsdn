#ifndef _LSDN_NETTYPES_H
#define _LSDN_NETTYPES_H

#include <stdint.h>
#include "errors.h"

typedef union lsdn_mac {
	uint8_t bytes[6];
} lsdn_mac_t;

lsdn_err_t lsdn_parse_mac(lsdn_mac_t* mac, const char* ascii);

/* These definitions are useful for the u32 TC classifier, where only
 * 32-bit values are accepted.
 */
static inline uint16_t lsdn_mac_low16(const lsdn_mac_t* mac)
{
	const uint8_t* b = mac->bytes;
	return (b[4] << 8) | b[5];
}
static inline uint32_t lsdn_mac_low32(const lsdn_mac_t* mac)
{
	const uint8_t* b = mac->bytes;
	return (b[2] << 24) | (b[3] << 16) | (b[4] << 8) | b[5];
}
static inline uint32_t lsdn_mac_high32(const lsdn_mac_t* mac)
{
	const uint8_t* b = mac->bytes;
	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}
static inline uint16_t lsdn_mac_high16(const lsdn_mac_t* mac)
{
	const uint8_t* b = mac->bytes;
	return (b[0] << 8) | b[1];
}

#endif
