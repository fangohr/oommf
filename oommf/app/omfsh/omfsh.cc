/* FILE: omfsh.cc                 -*-Mode: c++-*-
 *
 *	A Tcl shell extended by the OOMMF core (Oc) extension
 *
 * NOTICE: Plase see the file ../../LICENSE
 *
 * Last modified on: $Date: 2009/10/30 07:02:04 $
 * Last modified by: $Author: donahue $
 */

/* Header files for system libraries */
#include <cstring>

/* Header files for the OOMMF extensions */
#include "oc.h"
#include "nb.h"
#include "if.h"

/* End includes */     

/* 
 * The "application initialization" for OOMMF applications.
 */
int 
Tcl_AppInit(Tcl_Interp *interp)
{
    Oc_AutoBuf ab;
    if (Oc_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, ab("Oc"), Oc_Init, NULL);

    // Eventually move to support selection of which extensions
    // to initialize on the basis of command line switches to
    // the shell.

    if (Tcl_PkgPresent(interp, ab("Tk"), NULL, 0) != NULL) {
	// Adding support for image formats only makes sense
	// if Tk is initialized.

	// Load the Img package, if available, and in any
        // case the OOMMF If package.  The If package provides
        // support for the PPM "P3" ASCII format and for the
        // Microsoft BMP format.
        Tcl_PkgRequire(interp, ab("Img"), NULL, 0);
	if (If_Init(interp) != TCL_OK) return TCL_ERROR;
	Tcl_StaticPackage(interp, ab("If"), If_Init, NULL);
    }

    if (Nb_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, ab("Nb"), Nb_Init, NULL);

    return TCL_OK;
}

int Oc_AppMain(int argc, char **argv)
{
  // Turn over control to the Oc library.
  Oc_Main(argc, argv, Tcl_AppInit);

  // Oc_Main and Tcl_Main does not return, so control never gets here.
  // Make picky compilers happy.
  return 0;
}

