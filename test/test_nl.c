#include "../netmodel/nl.h"

#include <stdio.h>

int main(){
	struct mnl_socket *sock = lsdn_socket_init();
	if (sock == NULL) {
		return -1;
	}

	const char* if_name = "test_dummy";

	printf("Creating interface '%s': ", if_name);
	int err = lsdn_link_dummy_create(sock, if_name);
	printf("%s\n", strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	unsigned int if_index = if_nametoindex(if_name);

	printf("Adding HTB qdisc: ");
	err = lsdn_qdisc_htb_create(sock, if_index,
			TC_H_ROOT, 0x00010000U, 10, 0);
	printf("%s\n", strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	printf("Setting interface up: ");
	err = lsdn_link_set(sock, if_name, true);
	printf("%s\n", strerror(-err));
	if (err != 0) {
		mnl_socket_close(sock);
		return -1;
	}

	mnl_socket_close(sock);

	return 0;
}
