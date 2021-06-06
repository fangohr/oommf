/* FILE: xp.cc                      -*-Mode: c++-*-
 *
 * The OOMMF eXtra Precision extension.
 *
 * This extensions provides classes for floating point computations at
 * higher than normal (i.e., double) precision.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 */

#include "xpint.h"

/* End includes */     // Optional directive to pimake

int
Xp_Init(Tcl_Interp *interp)
{
#define RETURN_TCL_ERROR                                               \
    Tcl_AddErrorInfo(interp, OC_CONST84_CHAR("\n    (in Xp_Init())")); \
    return TCL_ERROR

    if (Tcl_PkgPresent(interp, OC_CONST84_CHAR("Oc"),
		       OC_CONST84_CHAR("1.1"), 0) == NULL) {
        Tcl_AppendResult(interp,
		OC_CONST84_CHAR("\n\t(Xp " XP_VERSION " needs Oc 1.1)"),
                NULL);
        RETURN_TCL_ERROR;
    }

    if (Tcl_PkgProvide(interp, OC_CONST84_CHAR("Xp"),
		       OC_CONST84_CHAR(XP_VERSION)) != TCL_OK) {
      RETURN_TCL_ERROR;
    }

/*
 * Currently there is no Tcl part to the Xp extension
 *
    if (Oc_InitScript(interp, "Xp", XP_VERSION) != TCL_OK) {
        RETURN_TCL_ERROR;
    }
 *
 */

    return TCL_OK;

#undef RETURN_TCL_ERROR
}
