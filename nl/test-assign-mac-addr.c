#include "nl_link.h"

#include <stdio.h>

int main(int argc, char** argv){
    if (argc < 3){
        printf("Usage: %s <DEVICE NAME> <MAC_ADDR>\n", argv[0]);
        return 1;
    }
    nl_link_assign_addr(argv[1], argv[2]);
    return 0;
}
