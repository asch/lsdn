#include "nl_link.h"

#include <stdio.h>

int main(int argc, char ** argv)
{
        if (argc < 2){
                printf("Usage: %s <DEVICE NAME>\n", argv[0]);
                return 1;
        }
        nl_link_egress_add_qdisc(argv[1], 0x1, 0x0);
        return 0;
}
