#include <lsdn.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static struct lsdn_context *ctx;
static struct lsdn_settings *settings;
static struct lsdn_net *vlan1, *vlan2;
static struct lsdn_phys *phys_a, *phys_b, *phys_c;
static struct lsdn_virt *virt_a1, *virt_a2, *virt_a3, *virt_b1, *virt_b2, *virt_c1;

int main(int argc, const char* argv[])
{
	assert(argc == 2);

	ctx = lsdn_context_new("ls");
	lsdn_context_abort_on_nomem(ctx);

	const char *nettype = getenv("LSCTL_NETTYPE");
	if (!nettype) {
		fprintf(stderr, "no LSCTL_NETTYPE\n");
		abort();
	} else if (!strcmp(nettype, "vlan")) {
		settings = lsdn_settings_new_vlan(ctx);
	} else if (!strcmp(nettype, "vxlan/e2e")) {
		settings = lsdn_settings_new_vxlan_e2e(ctx, 0);
	} else if (!strcmp(nettype, "vxlan/static")) {
		settings = lsdn_settings_new_vxlan_static(ctx, 0);
	} else if (!strcmp(nettype, "vxlan/mcast")) {
		settings = lsdn_settings_new_vxlan_mcast(ctx, LSDN_MK_IPV4(239,239,239,239), 0);
	} else if (!strcmp(nettype, "direct")) {
		settings = lsdn_settings_new_direct(ctx);
	} else {
		fprintf(stderr, "Unknown nettype: %s\n", nettype);
		abort();
	}


	/* LSCTL: phys -if out -name a -ip 172.16.0.1 */
	phys_a = lsdn_phys_new(ctx);
	lsdn_phys_set_ip(phys_a, LSDN_MK_IPV4(172, 16, 0, 1));
	lsdn_phys_set_iface(phys_a, "out");
	lsdn_phys_set_name(phys_a, "a");
	/* LSCTL; phys -if out -name b -ip 172.16.0.2 */
	phys_b = lsdn_phys_new(ctx);
	lsdn_phys_set_ip(phys_b, LSDN_MK_IPV4(172, 16, 0, 2));
	lsdn_phys_set_iface(phys_b, "out");
	lsdn_phys_set_name(phys_b, "b");
	/* LSCTL: phys -if out -name c -ip 172.16.0.3 */
	phys_c = lsdn_phys_new(ctx);
	lsdn_phys_set_ip(phys_c, LSDN_MK_IPV4(172, 16, 0, 3));
	lsdn_phys_set_iface(phys_c, "out");
	lsdn_phys_set_name(phys_c, "c");

	/* LSCTL: net 1 { */
	vlan1 = lsdn_net_new(settings, 1);
		/* LSCTL: attach a b c */
		lsdn_phys_attach(phys_a, vlan1);
		lsdn_phys_attach(phys_b, vlan1);
		lsdn_phys_attach(phys_c, vlan1);
		/* LSCTL: virt -phys a -if 1 -mac 00:00:00:00:00:a1 */
		virt_a1 = lsdn_virt_new(vlan1);
		lsdn_virt_connect(virt_a1, phys_a, "1");
		lsdn_virt_set_mac(virt_a1, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xa1));
		/* LSCTL: virt -phys a -if 2 -mac 00:00:00:00:00:a2 */
		virt_a2 = lsdn_virt_new(vlan1);
		lsdn_virt_connect(virt_a2, phys_a, "2");
		lsdn_virt_set_mac(virt_a2, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xa2));
		/* LSCTL: virt -phys b -if 1 -mac 00:00:00:00:00:b1 */
		virt_b1 = lsdn_virt_new(vlan1);
		lsdn_virt_connect(virt_b1, phys_b, "1");
		lsdn_virt_set_mac(virt_b1, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xb1));
		/* LSCTL: virt -phys c -if 1 -mac 00:00:00:00:00:c1 */
		virt_c1 = lsdn_virt_new(vlan1);
		lsdn_virt_connect(virt_c1, phys_c, "1");
		lsdn_virt_set_mac(virt_c1, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xc1));
	/* LSCTL: } */

	/* LSCTL: net 2 { */
	vlan2 = lsdn_net_new(settings, 2);
		/* LSCTL: attach a b c */
		lsdn_phys_attach(phys_a, vlan2);
		lsdn_phys_attach(phys_b, vlan2);
		/* LSCTL: virt -phys a -if 3 -mac 00:00:00:00:00:a3 */
		virt_a3 = lsdn_virt_new(vlan2);
		lsdn_virt_connect(virt_a3, phys_a, "3");
		lsdn_virt_set_mac(virt_a3, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xa3));
		/* LSCTL: virt -phys b -if 2 -mac 00:00:00:00:00:b2 */
		virt_b2 = lsdn_virt_new(vlan2);
		lsdn_virt_connect(virt_b2, phys_b, "2");
		lsdn_virt_set_mac(virt_b2, LSDN_MK_MAC(0x00, 0x00, 0x00, 0x00, 0x00, 0xb2));
	/* LSCTL: } */

	/* LSCTL: claimLocal [lindex $::argv 0] */
	struct lsdn_phys *local = lsdn_phys_by_name(ctx, argv[1]);
	assert(local != NULL);
	lsdn_phys_claim_local(local);

	lsdn_commit(ctx, lsdn_problem_stderr_handler, NULL);

	lsdn_context_free(ctx);
	return 0;
}
