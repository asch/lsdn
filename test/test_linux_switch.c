#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include "../netmodel/include/lsdn.h"

static void c(lsdn_err_t err)
{
	if(err != LSDNE_OK)
		abort();
}

enum {PVM1, PVM2, PTRUNK1};

int main(int argc, const char* argv[]){
	struct lsdn_network *net;
	struct lsdn_static_switch *sswitch;
	struct lsdn_linux_switch *lswitch;
	struct lsdn_netdev *vm[4];

	int broadcast = 1;
	if(argc >= 2)
		broadcast = atoi(argv[1]);
	printf("Broadcast %s\n", broadcast ? "enabled" : "disabled");

	net = lsdn_network_new("lsdn-test");

	lswitch = lsdn_linux_switch_new(net, 3);
	sswitch = lsdn_static_switch_new(net, 3);
	lsdn_static_switch_enable_broadcast(sswitch, broadcast);

	for(size_t i = 0; i<4; i++){
		char ifname[IF_NAMESIZE];
		sprintf(ifname, "ltif%ld", i+1);
		vm[i] = lsdn_netdev_new(net, ifname);
	}

/* TODO: Include some better solution in the library (so we don't have to typecast
 * like crazy or use these macros)
 */
#define PORTOF(node, port) lsdn_get_port((struct lsdn_node *)(node), port)
#define VMPORT(i) PORTOF(vm[i],0)

	lsdn_connect(VMPORT(0), PORTOF(sswitch, PVM1));
	lsdn_connect(VMPORT(1), PORTOF(sswitch, PVM2));
	lsdn_connect(VMPORT(2), PORTOF(lswitch, PVM1));
	lsdn_connect(VMPORT(3), PORTOF(lswitch, PVM2));

	lsdn_connect(PORTOF(lswitch, PTRUNK1), PORTOF(sswitch, PTRUNK1));

	// And now the rules. The next test we make should already be
	// able to auto-generate these rules from the network description
	// (as long as we set the network information on netdevs)
	lsdn_mac_t mac;

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:01"));
	c(lsdn_static_switch_add_rule(sswitch, &mac, PVM1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:02"));
	c(lsdn_static_switch_add_rule(sswitch, &mac, PVM2));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:03"));
	c(lsdn_static_switch_add_rule(sswitch, &mac, PTRUNK1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:04"));
	c(lsdn_static_switch_add_rule(sswitch, &mac, PTRUNK1));

	c(lsdn_network_create(net));
	lsdn_network_free(net);

	return 0;
}
