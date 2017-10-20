#ifndef _DOMAIN_H
#define _DOMAIN_H


void create_domain(char *name, char *kernel, char *modroot,
		char *rootfs, char *tap, char *mac,
		char *init_script);
void destroy_domain(char *name);


#endif
