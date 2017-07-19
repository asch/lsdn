#include <stdlib.h>
#include <tcl.h>
#include <getopt.h>
#include <string.h>
#include "lsext.h"

static void help()
{
	puts(
		"Usage: lsctl [arguments] files ...       Run specified lsctl scripts\n"
		"Arguments:\n"
		"  -d, --define <var=value>    et tcl variable to a given value\n"
		"  -h, --help                  Print this help message and exit\n"
	);
}

int main(int argc, char *argv[])
{
	int ret;
	int exitcode = 0;
	static const struct option opts[] = {
		{"define", required_argument, NULL, 'd'}
	};

	/* Prepare the interpreter */
	/* TODO: prepare a safe TCL interpreter */
	Tcl_FindExecutable(argv[0]);
	Tcl_Interp* interp = Tcl_CreateInterp();
	if (Tcl_Init(interp) != TCL_OK) {
		return 2;
	}

	register_lsdn_tcl(interp);

	/* Parse options */
	int option;
	char* delim;
	while((option = getopt_long(argc, argv, "d:", opts, NULL)) != -1) {
		switch(option) {
		case 'd':
			if((delim = strchr(optarg, '=')) == NULL){
				fprintf(stderr, "error: Argument \"-d %s\" is not of form var=value\n", optarg);
				return 1;
			}
			*delim = 0;
			Tcl_SetVar(interp, optarg, delim + 1, 0);
			break;
		case 'h':
			help();
			return 0;
		case '?':
			return 1;
		default:
			abort();
		}
	}

	if(optind == argc){
		fprintf(stderr, "error: List of scripts expected\n");
		return 1;
	}

	while(optind < argc) {
		/* Evaluate all files from cmdline */
		if( (ret = Tcl_EvalFile(interp, argv[optind])) ) {
			/* print error messages */
			Tcl_Obj *options = Tcl_GetReturnOptions(interp, ret);
			Tcl_Obj *key = Tcl_NewStringObj("-errorinfo", -1);
			Tcl_Obj *stackTrace;
			Tcl_IncrRefCount(key);
			Tcl_DictObjGet(NULL, options, key, &stackTrace);
			Tcl_DecrRefCount(key);
			fprintf(stderr, "error: %s\n", Tcl_GetString(stackTrace));
			Tcl_DecrRefCount(options);
			exitcode = 1;
		}
		optind++;
	}

	Tcl_Finalize();
	return exitcode;
}
