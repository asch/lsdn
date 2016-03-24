#ifndef _LSDN_NETTYPES_H
#define _LSDN_NETTYPES_H

#include <stdint.h>
#include "errors.h"

typedef union lsdn_mac {
	uint8_t bytes[6];
	/* These definitions are useful for the u32 TC classifier.
	 * However, remember to switch the byte order (ntohs)
	 */
	struct {
		uint16_t low_16;
		uint32_t high_32;
	};
	struct {
		uint32_t low_32;
		uint16_t high_16;
	};
} lsdn_mac_t;

lsdn_err_t lsdn_parse_mac(lsdn_mac_t* mac, const char* ascii);

#endif
