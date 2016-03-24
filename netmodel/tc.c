#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include "tc.h"

void runcmd(const char* format, ...)
{
	char cmdbuf[120];
	va_list args;
	va_start(args, format);
	vsnprintf(cmdbuf, sizeof(cmdbuf), format, args);
	va_end(args);
	printf("Running: %s\n", cmdbuf);
	system(cmdbuf);
}
