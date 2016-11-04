#include <stdio.h>
#include <stdlib.h>

#include "../netmodel/private/nl.h"

int main(int argc, char **argv)
{
	if (argc != 4) {
		// test_vlan eth0 eth0.100 100
		printf("Usage: test_vlan <underlying interface> <vlan name> <vlan id>\n");
		return 1;
	}
	char *if_name = argv[1];
	char *vlan_name = argv[2];
	int vlan_id = atoi(argv[3]);

	struct mnl_socket *sock = lsdn_socket_init();
	if (sock == NULL) {
		return 1;
	}

	struct lsdn_if lif;
	int ret = lsdn_link_vlan_create(sock, &lif, if_name, vlan_name, vlan_id);
	if (ret != 0) {
		printf("vlan_create = %s\n", strerror(-ret));
	} else {
		printf("ok\n");
	}
	return 0;
}
