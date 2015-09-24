/* FILE: mmdispsh.cc                 -*-Mode: c++-*-
 *
 * A shell program which includes Tcl commands needed to support a
 * vector display application.
 *
 */

#include "oc.h"
#include "vf.h"
#include "mmdispcmds.h"

/* End includes */ 

int Tcl_AppInit(Tcl_Interp *interp)
{
    Oc_AutoBuf ab;
    Tcl_DString buf;

    Oc_SetDefaultTkFlag(1);
    if (Oc_Init(interp) == TCL_ERROR) {
       return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, ab("Oc"), Oc_Init, NULL);

    if (Nb_Init(interp) == TCL_ERROR) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf, ab("Oc_Log Log"), -1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_Eval(interp, Tcl_DStringAppendElement(&buf, ab("error")));
        Tcl_DStringFree(&buf);
    }
    Tcl_StaticPackage(interp, ab("Nb"), Nb_Init, NULL);

    if (Vf_Init(interp) == TCL_ERROR) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf, ab("Oc_Log Log"), -1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_Eval(interp, Tcl_DStringAppendElement(&buf, ab("error")));
        Tcl_DStringFree(&buf);
    }
    Tcl_StaticPackage(interp, ab("Vf"), Vf_Init, NULL);

    if (Mmdispcmds_Init(interp) == TCL_ERROR) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf, ab("Oc_Log Log"), -1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_Eval(interp, Tcl_DStringAppendElement(&buf, ab("error")));
        Tcl_DStringFree(&buf);
    }
    Tcl_StaticPackage(interp, ab("Mmdispcmds"), Mmdispcmds_Init, NULL);

    return TCL_OK;
}

int Oc_AppMain(int argc, char **argv)
{
    Oc_Main(argc,argv,Tcl_AppInit);
    return 0;
}
