#include "../netmodel/private/nl.h"
#include <stdio.h>

int main(){
	struct mnl_socket *sock = lsdn_socket_init();
	if (sock == NULL) {
		return -1;
	}

	struct lsdn_if if1, if2;
	int ret = lsdn_link_veth_create(sock, &if1,"lsdn-veth-a", &if2, "lsdn-veth-b");
	if(ret != 0){
		printf("veth_create = %s\n", strerror(-ret));
	} else {
		printf("ok\n");
	}
}
