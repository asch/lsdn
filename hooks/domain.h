#pragma once

void create_domain(char *name, char *kernel,
		char *rootfs, char *tap, char *mac,
		char *init_script);
void destroy_domain(char *name);
