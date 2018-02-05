/* FILE: nb.cc                   -*-Mode: c++-*-
 *
 *	The Nuts & Bolts extension.
 *
 *	This extension provides a collection of C++ classes and functions
 * of general utility to all OOMMF applications.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2012/11/20 01:01:49 $
 * Last modified by: $Author: donahue $
 */

/* Header file for this extension */
#include "oc.h"
#include "evoc.h"
#include "nb.h"

/* End includes */     

int 
Nb_Init(Tcl_Interp *interp)
{
#define RETURN_TCL_ERROR                                               \
    Tcl_AddErrorInfo(interp, OC_CONST84_CHAR("\n    (in Nb_Init())")); \
    return TCL_ERROR

  if (Tcl_PkgPresent(interp, OC_CONST84_CHAR("Oc"),
		     OC_CONST84_CHAR("2"), 0) == NULL) {
    Tcl_AppendResult(interp,
	     OC_CONST84_CHAR("\n\t(Nb " NB_VERSION " needs Oc 2)"), NULL);
    RETURN_TCL_ERROR;
  }

  // Register C++-based Tcl commands of this extension
  Oc_RegisterCommand(interp,"Nb_ComputeCRCBuffer",
		     (Tcl_CmdProc *)NbComputeCRCBufferCmd);
  Oc_RegisterCommand(interp,"Nb_ComputeCRCChannel",
		     (Tcl_CmdProc *)NbComputeCRCChannelCmd);
  Oc_RegisterCommand(interp,"Nb_Gcd",(Tcl_CmdProc *)NbGcdCmd);
  Oc_RegisterCommand(interp,"Nb_RatApprox",(Tcl_CmdProc *)NbRatApprox);
  Oc_RegisterCommand(interp,"Nb_GetColor",(Tcl_CmdProc *)NbGetColor);
  Oc_RegisterCommand(interp,"Oc_Times",(Tcl_CmdProc *)OcTimes);

#if TCL_MAJOR_VERSION < 8  // Use string interface
  Oc_RegisterCommand(interp,"Nb_ImgObj",(Tcl_CmdProc *)NbImgObjCmd);
#else
  Oc_RegisterObjCommand(interp,"Nb_ImgObj",(Tcl_ObjCmdProc *)NbImgObjCmd);
#endif

#if (OC_SYSTEM_TYPE == OC_UNIX)
  Oc_RegisterCommand(interp,"Nb_GetUserId",(Tcl_CmdProc *)NbGetUserId);
#endif
#if (OC_SYSTEM_TYPE == OC_WINDOWS) || (OC_SYSTEM_SUBTYPE == OC_CYGWIN)
  Oc_RegisterCommand(interp,"Nb_GetPidUserName",
                     (Tcl_CmdProc *)NbGetPidUserName);
  Oc_RegisterCommand(interp,"Nb_GetUserName",(Tcl_CmdProc *)NbGetUserName);
#endif

  if (Tcl_PkgProvide(interp, OC_CONST84_CHAR("Nb"),
		     OC_CONST84_CHAR(NB_VERSION)) != TCL_OK) {
    RETURN_TCL_ERROR;
  }

  // Setup Tcl script portion of Nb extension
  if ( Oc_InitScript(interp, "Nb", NB_VERSION) != TCL_OK) {
    RETURN_TCL_ERROR;
  }

  // Test Nb_Xpfloat class for brokenness
  if(Nb_Xpfloat::Test() != 0) {
    Tcl_AppendResult(interp,
	     OC_CONST84_CHAR("\n\tNb_Xpfloat initialization test error"),
             NULL);
    RETURN_TCL_ERROR;
  }

  return TCL_OK;

#undef RETURN_TCL_ERROR
}
