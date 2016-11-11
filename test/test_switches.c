#include <stdio.h>
#include <stdlib.h>
#include "../netmodel/include/lsdn.h"
#include <net/if.h>

static void c(lsdn_err_t err)
{
	if(err != LSDNE_OK){
		printf("LSDN Error %d\n", err);
		abort();
	}
}

struct switch_ports{
	struct lsdn_port *vm0, *vm1, *t1, *t2;
};

int main(int argc, const char* argv[]){
	struct lsdn_network *net;
	struct lsdn_static_switch *sswitch[3];
	struct switch_ports sports[3];
	struct lsdn_netdev *vm[6];

	int broadcast = 1;
	if(argc >= 2)
		broadcast = atoi(argv[1]);
	printf("Broadcast %s\n", broadcast ? "enabled" : "disabled");

	net = lsdn_network_new("lsdn-test");

	sswitch[0] = lsdn_static_switch_new(net);
	sswitch[1] = lsdn_static_switch_new(net);
	sswitch[2] = lsdn_static_switch_new(net);

	for(size_t i = 0; i<3; i++){
		lsdn_static_switch_enable_broadcast(sswitch[0], broadcast);
	}

	for(size_t i = 0; i<6; i++){
		char ifname[IF_NAMESIZE];
		sprintf(ifname, "ltif%ld", i+1);
		vm[i] = lsdn_netdev_new(net, ifname);
	}

	lsdn_connect((struct lsdn_node*)vm[0], NULL, (struct lsdn_node*)sswitch[0], &sports[0].vm0);
	lsdn_connect((struct lsdn_node*)vm[1], NULL, (struct lsdn_node*)sswitch[0], &sports[0].vm1);
	lsdn_connect((struct lsdn_node*)vm[2], NULL, (struct lsdn_node*)sswitch[1], &sports[1].vm0);
	lsdn_connect((struct lsdn_node*)vm[3], NULL, (struct lsdn_node*)sswitch[1], &sports[1].vm1);
	lsdn_connect((struct lsdn_node*)vm[4], NULL, (struct lsdn_node*)sswitch[2], &sports[2].vm0);
	lsdn_connect((struct lsdn_node*)vm[5], NULL, (struct lsdn_node*)sswitch[2], &sports[2].vm1);

	lsdn_connect(
		(struct lsdn_node*)sswitch[0], &sports[0].t1,
		(struct lsdn_node*)sswitch[1], &sports[1].t1);
	lsdn_connect(
		(struct lsdn_node*)sswitch[0], &sports[0].t2,
		(struct lsdn_node*)sswitch[2], &sports[2].t1);

	// And now the rules. The next test we make should already be
	// able to auto-generate these rules from the network description
	// (as long as we set the network information on netdevs)
	lsdn_mac_t mac;

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:01"));
	c(lsdn_static_switch_add_rule(&mac, sports[0].vm0));
	c(lsdn_static_switch_add_rule(&mac, sports[1].t1));
	c(lsdn_static_switch_add_rule(&mac, sports[2].t1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:02"));
	c(lsdn_static_switch_add_rule(&mac, sports[0].vm1));
	c(lsdn_static_switch_add_rule(&mac, sports[1].t1));
	c(lsdn_static_switch_add_rule(&mac, sports[2].t1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:03"));
	c(lsdn_static_switch_add_rule(&mac, sports[0].t1));
	c(lsdn_static_switch_add_rule(&mac, sports[1].vm0));
	c(lsdn_static_switch_add_rule(&mac, sports[2].t1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:04"));
	c(lsdn_static_switch_add_rule(&mac, sports[0].t1));
	c(lsdn_static_switch_add_rule(&mac, sports[1].vm1));
	c(lsdn_static_switch_add_rule(&mac, sports[2].t1));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:05"));
	c(lsdn_static_switch_add_rule(&mac, sports[0].t2));
	c(lsdn_static_switch_add_rule(&mac, sports[1].t1));
	c(lsdn_static_switch_add_rule(&mac, sports[2].vm0));

	c(lsdn_parse_mac(&mac, "00:00:00:00:00:06"));
	c(lsdn_static_switch_add_rule(&mac, sports[0].t2));
	c(lsdn_static_switch_add_rule(&mac, sports[1].t1));
	c(lsdn_static_switch_add_rule(&mac, sports[2].vm1));

	c(lsdn_network_create(net));
	lsdn_network_free(net);

	return 0;
}
