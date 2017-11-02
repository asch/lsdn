#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libvirt/libvirt.h>

#include "domain.h"

#define DOMAIN_DESTINATION "/tmp/DOMAIN.xml"
#define MAX_SIZE 16384

void create_domain(char *name, char *kernel, char *modroot,
		char *rootfs, char *tap, char *mac,
		char *init_script)
{
	int res;
	long size;
	size_t cnt;
	pid_t pid;
	FILE *fp;
	char xml_desc[MAX_SIZE];
	virConnectPtr conn;
	virDomainPtr domain;

	switch (pid = fork()) {
	case -1:
		abort();
	case 0:
		execl("../hooks/vm_gen", "vm_gen", name,
			kernel, modroot, rootfs,
			tap, mac, DOMAIN_DESTINATION,
			init_script, NULL);
		abort();
	default:
		wait(NULL);
		break;
	}

	fp = fopen(DOMAIN_DESTINATION, "r+");
	if (!fp)
		abort();

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	if (size >= MAX_SIZE)
		abort();
	rewind(fp);

	cnt = fread(xml_desc, 1, size, fp);
	fclose(fp);
	if (cnt != size)
		abort();
	xml_desc[cnt] = '\0';

	conn = virConnectOpen("qemu:///system");
	if (!conn)
		abort();

	domain = virDomainDefineXML(conn, xml_desc);
	if (!domain)
		abort();
	res = virDomainCreate(domain);
	if (res)
		abort();

	virConnectClose(conn);
}

void destroy_domain(char *name)
{
	virConnectPtr conn;
	virDomainPtr domain;

	conn = virConnectOpen("qemu:///system");
	if (!conn)
		abort();

	domain = virDomainLookupByName(conn, name);
	if (!domain)
		abort();

	virDomainDestroy(domain);
	virDomainFree(domain);
	virConnectClose(conn);
}
