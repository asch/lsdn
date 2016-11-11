#include <stdio.h>
#include <stdlib.h>
#include <net/if.h>
#include "../netmodel/include/lsdn.h"

static void c(lsdn_err_t err)
{
	if(err != LSDNE_OK){
		printf("LSDN Error %d\n", err);
		abort();
	}
}

struct switch_ports{
	struct lsdn_port *vm0, *vm1, *trunk;
};

int main(int argc, const char* argv[]){
	struct lsdn_network *net;
	struct lsdn_static_switch *sswitch;
	struct lsdn_linux_switch *lswitch;
	struct lsdn_node *vm[4];
	struct switch_ports sswitch_ports[3];
	struct switch_ports lswitch_ports[3];

	int broadcast = 1;
	if(argc >= 2)
		broadcast = atoi(argv[1]);
	printf("Broadcast %s\n", broadcast ? "enabled" : "disabled");

	net = lsdn_network_new("lsdn-test");

	lswitch = lsdn_linux_switch_new(net);
	sswitch = lsdn_static_switch_new(net);
	lsdn_static_switch_enable_broadcast(sswitch, broadcast);

	for(size_t i = 0; i<4; i++){
		char ifname[IF_NAMESIZE];
		sprintf(ifname, "ltif%ld", i+1);
		vm[i] = (struct lsdn_node*) lsdn_netdev_new(net, ifname);
	}

	lsdn_connect(vm[0], NULL, (struct lsdn_node*) sswitch, &sswitch_ports->vm0);
	lsdn_connect(vm[1], NULL, (struct lsdn_node*) sswitch, &sswitch_ports->vm1);
	lsdn_connect(vm[2], NULL, (struct lsdn_node*) lswitch, &lswitch_ports->vm0);
	lsdn_connect(vm[3], NULL, (struct lsdn_node*) lswitch, &lswitch_ports->vm1);
	lsdn_connect(
		(struct lsdn_node*) sswitch, &sswitch_ports->trunk,
		(struct lsdn_node*)lswitch, &lswitch_ports->trunk);

	// And now the rules. The next test we make should already be
	// able to auto-generate these rules from the network description
	// (as long as we set the network information on netdevs)
	lsdn_mac_t mac;

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:01"));
	c(lsdn_static_switch_add_rule(&mac, sswitch_ports->vm0));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:02"));
	c(lsdn_static_switch_add_rule(&mac, sswitch_ports->vm1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:03"));
	c(lsdn_static_switch_add_rule(&mac, sswitch_ports->trunk));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:04"));
	c(lsdn_static_switch_add_rule(&mac, sswitch_ports->trunk));

	c(lsdn_network_create(net));
	lsdn_network_free(net);

	return 0;
}
