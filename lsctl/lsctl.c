/** \file
 * LSCTL shell main code (simple tclsh frontend). */
#include <stdlib.h>
#include <tcl.h>
#include <getopt.h>
#include <string.h>
#include "lsext.h"

int main(int argc, char *argv[])
{
	Tcl_Main(argc, argv, register_lsdn_tcl);
}
