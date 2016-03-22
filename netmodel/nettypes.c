#include <stdlib.h>
#include "nettypes.h"

static lsdn_err_t parse_octet(const char** ascii, uint8_t* dst)
{
        if((*ascii)[0] == 0 || (*ascii)[1] == 0)
                return 0;

        char octet_ascii[3] = {(*ascii)[0], (*ascii)[1], 0};
        char* end;
        uint8_t value = (uint8_t) strtoul(octet_ascii, &end, 16);
        if(*end != 0)
                return LSDNE_PARSE;

        *ascii += 2;
        *dst = value;
        return LSDNE_OK;

}

lsdn_err_t lsdn_parse_mac(lsdn_mac_t* mac, const char* ascii)
{
        lsdn_err_t err;
        for(int i = 0; i<6; i++)
        {
                if((err = parse_octet(&ascii, &mac->bytes[i])) != LSDNE_OK)
                        return err;
                if(*ascii == ':')
                        ascii++;
        }
        return LSDNE_OK;
}
