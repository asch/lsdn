#ifndef _LSDN_NETTYPES_H
#define _LSDN_NETTYPES_H

#include <stdint.h>
#include "errors.h"

typedef union lsdn_mac {
         uint8_t bytes[6];
} lsdn_mac_t;

lsdn_err_t lsdn_parse_mac(lsdn_mac_t* mac, const char* ascii);

#endif
