#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <libvirt/libvirt.h>

#include "domain.h"

#define DOMAIN_DESTINATION "/tmp/DOMAIN.xml"
#define MAX_SIZE 16384

void create_domain(char **argv)
{
	pid_t pid;
	FILE *fp;
	char xml_desc[MAX_SIZE];
	virConnectPtr conn;
	virDomainPtr domain;

	switch (pid = fork()) {
	case -1:
		abort();
	case 0:
		execl("../hooks/vm_gen", "vm_gen",
			argv[0], argv[1], argv[2],
			argv[3], argv[4], argv[5],
			argv[6], DOMAIN_DESTINATION, NULL);
		abort();
	default:
		wait(NULL);
		break;
	}

	fp = fopen(DOMAIN_DESTINATION, "r+");
	if (!fp)
		abort();
	fread(xml_desc, 1, MAX_SIZE, fp);
	fclose(fp);

	conn = virConnectOpen("qemu:///system");
	if (!conn)
		abort();

	domain = virDomainCreateXML(conn, xml_desc, 0);
	if (!domain)
		abort();

	virDomainFree(domain);
	virConnectClose(conn);
}

void destroy_domain(char **argv)
{
	virConnectPtr conn;
	virDomainPtr domain;

	conn = virConnectOpen("qemu:///system");
	if (!conn)
		abort();

	domain = virDomainLookupByName(conn, argv[0]);
	if (!domain)
		abort();

	virDomainDestroy(domain);
	virDomainFree(domain);
	virConnectClose(conn);
}

