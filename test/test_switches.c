#include <stdio.h>
#include <stdlib.h>
#include "../netmodel/lsdn.h"

static void c(lsdn_err_t err)
{
	if(err != LSDNE_OK)
		abort();
}

enum {PVM1, PVM2, PTRUNK1, PTRUNK2};

int main(){
	struct lsdn_network *net;
	struct lsdn_static_switch *sswitch[3];
	struct lsdn_netdev *vm[6];

	net = lsdn_network_new("sp-ex1");

	sswitch[0] = lsdn_static_switch_new(net, 4);
	sswitch[1] = lsdn_static_switch_new(net, 3);
	sswitch[2] = lsdn_static_switch_new(net, 3);

	for(size_t i = 0; i<6; i++){
		char ifname[20];
		sprintf(ifname, "sp-ex1-%c", (int)('a'+i));
		vm[i] = lsdn_netdev_new(net, ifname);
	}

	/* TODO: Include some better solution in the library (so we don't have to typecast
 * like crazy or use these macros)
 */
#define PORTOF(node, port) lsdn_get_port((struct lsdn_node*)(node), port)
#define VMPORT(i) PORTOF(vm[i],0)

	lsdn_connect(VMPORT(0), PORTOF(sswitch[0], PVM1));
	lsdn_connect(VMPORT(1), PORTOF(sswitch[0], PVM2));
	lsdn_connect(VMPORT(2), PORTOF(sswitch[1], PVM1));
	lsdn_connect(VMPORT(3), PORTOF(sswitch[1], PVM2));
	lsdn_connect(VMPORT(4), PORTOF(sswitch[2], PVM1));
	lsdn_connect(VMPORT(5), PORTOF(sswitch[2], PVM2));

	lsdn_connect(PORTOF(sswitch[0], PTRUNK1), PORTOF(sswitch[1], PTRUNK1));
	lsdn_connect(PORTOF(sswitch[0], PTRUNK2), PORTOF(sswitch[2], PTRUNK1));

	// And now the rules. The next test we make should already be
	// able to auto-generate these rules from the network description
	// (as long as we set the network information on netdevs)
	lsdn_mac_t mac;

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:01"));
	c(lsdn_static_switch_add_rule(sswitch[0], &mac, PVM1));
	c(lsdn_static_switch_add_rule(sswitch[1], &mac, PTRUNK1));
	c(lsdn_static_switch_add_rule(sswitch[2], &mac, PTRUNK1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:02"));
	c(lsdn_static_switch_add_rule(sswitch[0], &mac, PVM2));
	c(lsdn_static_switch_add_rule(sswitch[1], &mac, PTRUNK1));
	c(lsdn_static_switch_add_rule(sswitch[2], &mac, PTRUNK1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:03"));
	c(lsdn_static_switch_add_rule(sswitch[0], &mac, PTRUNK1));
	c(lsdn_static_switch_add_rule(sswitch[1], &mac, PVM1));
	c(lsdn_static_switch_add_rule(sswitch[2], &mac, PTRUNK1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:04"));
	c(lsdn_static_switch_add_rule(sswitch[0], &mac, PTRUNK1));
	c(lsdn_static_switch_add_rule(sswitch[1], &mac, PVM2));
	c(lsdn_static_switch_add_rule(sswitch[2], &mac, PTRUNK1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:05"));
	c(lsdn_static_switch_add_rule(sswitch[0], &mac, PTRUNK2));
	c(lsdn_static_switch_add_rule(sswitch[1], &mac, PTRUNK1));
	c(lsdn_static_switch_add_rule(sswitch[2], &mac, PVM1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:06"));
	c(lsdn_static_switch_add_rule(sswitch[0], &mac, PTRUNK2));
	c(lsdn_static_switch_add_rule(sswitch[1], &mac, PTRUNK1));
	c(lsdn_static_switch_add_rule(sswitch[2], &mac, PVM2));

	lsdn_network_create(net);
	lsdn_network_free(net);

	return 0;
}
