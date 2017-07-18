#include <stdlib.h>
#include <tcl.h>
#include "lsext.h"

int main(int argc, const char *argv[])
{
	int ret;
	int exitcode = 0;
	/* TODO: prepare a safe TCL interpreter */
	Tcl_FindExecutable(argv[0]);
	Tcl_Interp* interp = Tcl_CreateInterp();
	if (Tcl_Init(interp) != TCL_OK) {
		return 1;
	}
	register_lsdn_tcl(interp);

	/* process remaining arguments (taken from expect) */
	char argc_rep[20];
	snprintf(argc_rep, sizeof(argc_rep), "%d", argc - 1);
	Tcl_SetVar(interp, "argc", argc_rep, 0);
	Tcl_SetVar(interp, "argv0", argv[1] ,0);
	char *tcl_argv = Tcl_Merge(argc - 1, argv + 1);
	Tcl_SetVar(interp,"argv", tcl_argv, 0);
	Tcl_Free(tcl_argv);

	if( (ret = Tcl_EvalFile(interp, argv[1])) ) {
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

	Tcl_Finalize();
	return exitcode;
}
