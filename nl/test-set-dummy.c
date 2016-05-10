#include "nl_link.h"


#include <stdio.h>

int main(int argc, char ** argv){
    if (argc < 2){
        printf("Usage: %s <DEVICE NAME>\n", argv[0]);
        return 1;
    }
    nl_link_set_up(argv[1]);
    return 0;
}
