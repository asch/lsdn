#include "lsdn.h"
#include "domain.h"

static struct lsdn_context *ctx;
static struct lsdn_settings *settings;
static struct lsdn_net *vlan;
static struct lsdn_phys *phys_a, *phys_b, *phys_c;
static struct lsdn_virt
	*dhcp_server,
	*virt_b1, *virt_b2, *virt_b3,
	*virt_c1, *virt_c2;

#define KERNEL "/home/qemu/bzImage"
#define MODROOT "/home/qemu/modroot"

#define str(s) #s
#define ROOTFS(n) str(/home/qemu/rootfs##n.qcow2)
#define ROOTFS0 ROOTFS(0)
#define ROOTFS1 ROOTFS(1)
#define ROOTFS2 ROOTFS(2)
#define ROOTFS3 ROOTFS(3)
#define ROOTFS4 ROOTFS(4)
#define ROOTFS5 ROOTFS(5)

char *server_init = "cat << EOF > /tmp/dhcpd.conf\n"
"default-lease-time 600;\n"
"max-lease-time 7200;\n"
"subnet 192.168.99.0 netmask 255.255.255.0 {\n"
"	option subnet-mask 255.255.255.0;\n"
"	range 192.168.99.2 192.168.99.250;\n"
"}\n"
"EOF\n"
"echo > /tmp/dhcpd.leases\n"
"ip addr replace 192.168.99.1/24 dev eth0\n"
"ip link set dev eth0 up\n"
"dhcpd -cf /tmp/dhcpd.conf -lf /tmp/dhcpd.leases --no-pid eth0\n";

char *client_init = "sleep 10 # let the server some time to start up\n"
"ip link set dev lo up\n"
"dhcpcd eth0\n";

void hook(struct lsdn_net *net, struct lsdn_phys *phys, void *data)
{
	if (phys == phys_a) {
		create_domain("DHCP-SERVER", KERNEL, MODROOT, ROOTFS0, "tap0", "00:00:00:00:00:01", server_init);
		dhcp_server = lsdn_virt_new(net);
		lsdn_virt_connect(dhcp_server, phys, "tap0");
	}
}

int main(int argc, const char* argv[])
{
	assert(argc == 1);

	ctx = lsdn_context_new("ls");
	lsdn_context_abort_on_nomem(ctx);

	settings = lsdn_settings_new_vlan(ctx);
	struct lsdn_user_hooks hooks = {
		.lsdn_startup_hook = hook,
		.lsdn_startup_hook_user = NULL,
		.lsdn_shutdown_hook = NULL,
		.lsdn_shutdown_hook_user = NULL
	};
	lsdn_settings_register_user_hooks(settings, &hooks);

	vlan = lsdn_net_new(settings, 0x0B0B);

	phys_a = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys_a, vlan);
	lsdn_phys_set_iface(phys_a, "vetha");

	phys_b = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys_b, vlan);
	lsdn_phys_set_iface(phys_b, "vethb");

	phys_c = lsdn_phys_new(ctx);
	lsdn_phys_attach(phys_c, vlan);
	lsdn_phys_set_iface(phys_c, "vethc");

	lsdn_phys_claim_local(phys_a);
	lsdn_phys_claim_local(phys_b);
	lsdn_phys_claim_local(phys_c);

	create_domain("CLIENT-B-1", KERNEL, MODROOT, ROOTFS1, "tap1", "00:00:00:00:00:02", client_init);
	virt_b1 = lsdn_virt_new(vlan);
	lsdn_virt_connect(virt_b1, phys_b, "tap1");

	create_domain("CLIENT-B-2", KERNEL, MODROOT, ROOTFS2, "tap2", "00:00:00:00:00:03", client_init);
	virt_b2 = lsdn_virt_new(vlan);
	lsdn_virt_connect(virt_b2, phys_b, "tap2");

	create_domain("CLIENT-B-3", KERNEL, MODROOT, ROOTFS3, "tap3", "00:00:00:00:00:04", client_init);
	virt_b3 = lsdn_virt_new(vlan);
	lsdn_virt_connect(virt_b3, phys_b, "tap3");

	create_domain("CLIENT-C-1", KERNEL, MODROOT, ROOTFS4, "tap4", "00:00:00:00:00:05", client_init);
	virt_c1 = lsdn_virt_new(vlan);
	lsdn_virt_connect(virt_c1, phys_c, "tap4");

	create_domain("CLIENT-C-2", KERNEL, MODROOT, ROOTFS5, "tap5", "00:00:00:00:00:06", client_init);
	virt_c2 = lsdn_virt_new(vlan);
	lsdn_virt_connect(virt_c2, phys_c, "tap5");

	lsdn_commit(ctx, lsdn_problem_stderr_handler, NULL);

	lsdn_context_free(ctx);
	return 0;
}
