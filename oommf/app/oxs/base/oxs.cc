/* FILE: oxs.cc                 -*-Mode: c++-*-
 *
 * Skeleton for oxs interpreter shell application.
 *
 */

#include "oc.h"
#include "nb.h"
#include "vf.h"
#include "oxs.h"

#include "director.h"
#include "ext.h"	// Needed to make MSVC++ 5.x happy
#include "energy.h"	// Needed to make MSVC++ 5.x happy

/* End includes */

extern void OxsInitializeExt();
extern void OxsRegisterInterfaceCommands(Oxs_Director* director,
					 Tcl_Interp* interp);
/// These are not in any header file because they should only be
/// called by Oxs_Init().

/*
 *----------------------------------------------------------------------
 *
 * OxsDeleteDirector --
 *
 *      Deletes Oxs_Director instance that has been associated with
 *      a Tcl_Interp.  This routine is called by the interpreter
 *      as part of its destruction.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
Tcl_InterpDeleteProc	OxsDeleteDirector;

void OxsDeleteDirector(ClientData clientdata,Tcl_Interp *interp)
{
  Oxs_Director *dir = (Oxs_Director *)clientdata;
  if(dir->VerifyInterp(interp)) {
    delete dir;
  }
  // Else, throw exception?
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_Init --
 *
 *      This procedure initializes Oxs
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Tcl_PackageInitProc	Oxs_Init;

int Oxs_Init(Tcl_Interp *interp)
{
#define RETURN_TCL_ERROR                              \
    Tcl_AddErrorInfo(interp,"\n    (in Oxs_Init())"); \
    return TCL_ERROR

  Oc_AutoBuf ab;

  if (Tcl_PkgRequire(interp,"Oc","2", 0) == NULL) {
    Tcl_AppendResult(interp,ab("\n\t(Oxs " OXS_VERSION
				"needs Oc 2)"), NULL);
    RETURN_TCL_ERROR;
  }

  if (Tcl_PkgRequire(interp,"Nb", "2", 0) == NULL) {
    Tcl_AppendResult(interp,ab("\n\t(Oxs " OXS_VERSION
				"needs Nb 2)"), NULL);
    RETURN_TCL_ERROR;
  }

  // Create director for this Tcl interpreter.
  Oxs_Director *director = new Oxs_Director(interp);

  // Arrange for *interp destruction to destroy *director
  Tcl_SetAssocData(interp,"director", OxsDeleteDirector,director);

  // Run Ext child class initialization (including registration).
  OxsInitializeExt();

  // Add Tcl interface commands to interpreter.
  OxsRegisterInterfaceCommands(director,interp);

  if(Tcl_PkgProvide(interp,"Oxs", OXS_VERSION) != TCL_OK) {
    RETURN_TCL_ERROR;
  }
  if (Oc_InitScript(interp,"Oxs",OXS_VERSION) == TCL_OK) {
    return TCL_OK;
  }
  RETURN_TCL_ERROR;

#undef RETURN_TCL_ERROR
}

int Tcl_AppInit(Tcl_Interp *interp)
{
    Tcl_DString buf;

    Oc_SetDefaultTkFlag(0);
    if (Oc_Init(interp) != TCL_OK) {
       return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Oc", Oc_Init, NULL);

    if (Nb_Init(interp) == TCL_ERROR) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf, "Oc_Log Log", -1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_DStringAppendElement(&buf, "error");
        Tcl_Eval(interp, Tcl_DStringValue(&buf));
        Tcl_DStringFree(&buf);
        Tcl_Exit(1);
    }
    Tcl_StaticPackage(interp, "Nb", Nb_Init, NULL);

    if (Vf_Init(interp) == TCL_ERROR) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf, "Oc_Log Log", -1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_DStringAppendElement(&buf, "error");
        Tcl_Eval(interp, Tcl_DStringValue(&buf));
        Tcl_DStringFree(&buf);
        Tcl_Exit(1);
    }
    Tcl_StaticPackage(interp, "Vf", Vf_Init, NULL);

    if (Oxs_Init(interp) != TCL_OK) {
        Tcl_DStringInit(&buf);
        Tcl_DStringAppend(&buf,"Oc_Log Log",-1);
        Tcl_DStringAppendElement(&buf, Tcl_GetStringResult(interp));
        Tcl_DStringAppendElement(&buf,"panic");
        Tcl_Eval(interp, Tcl_DStringValue(&buf));
        Tcl_DStringFree(&buf);
        Tcl_Exit(1);
    }
    Tcl_StaticPackage(interp, "Oxs", Oxs_Init, NULL);
    return TCL_OK;
}

int Oc_AppMain(int argc, char **argv)
{
  Oc_Main(argc,argv,Tcl_AppInit);

  // Oc_Main and Tcl_Main does not return, so control never gets here.
  // Make picky compilers happy.
  return 0;
}

