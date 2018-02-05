/* FILE: vf.cc                   -*-Mode: c++-*-
 *
 *	The Vector Field extension.
 *
 *	This extension provides a collection of C++ classes and functions
 * for handling vector fields.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2007/03/21 23:29:38 $
 * Last modified by: $Author: donahue $
 */

#include "oc.h"

/*
 * This above was #include "vf.h", but the gnu linker complains if
 * "vecfile.h" gets included here, e.g.,
 *
 *   gcc linalp/bitmap.o linalp/colormap.o linalp/display.o                 \
 *       linalp/mmdinit.o linalp/mmdisp.o -L../../ext/vf/linalp             \
 *       -lvf -L../../ext/nb/linalp -lnb -L../../ext/oc/linalp              \
 *       -loc -L/usr/local/lib -ltk8.0 -L/usr/local/lib -ltcl8.0            \
 *       -L/usr/X11R6/lib -lX11 -ldl -lm -o linalp/mmdisp
 *   ld: Warning: size of symbol `Reset__t7Nb_List1Z14Vf_FileInputIDi'      \
 *       changed from 376 to 416 in vf.o
 *   ld: Warning: size of symbol `RestartGet__t7Nb_List1Z14Vf_FileInputID'  \
 *       changed from 40 to 48 in vf.o
 *   ld: Warning: size of symbol                                            \
 *       `Copy__t7Nb_List1Z14Vf_FileInputIDRCt7Nb_List1Z14Vf_FileInputID'   \
 *       changed from 596 to 636 in vf.o
 *   ld: Warning: size of symbol `ExtendSpine__t7Nb_List1Z14Vf_FileInputIDi'\
 *       changed from 408 to 392 in vf.o
 *   ld: Warning: size of symbol `AddRib__t7Nb_List1Z14Vf_FileInputID'      \
 *       changed from 264 to 276 in vf.o
 *
 * The problem appears to be the
 *       static Nb_List<Vf_FileInputID> child_list;
 * declaration in
 *       class Vf_FileInput
 *
 * It appears that all the templates in the included header files get
 * stuck into vf.o, and later on this one has resolution problems.
 * (Use 'nm', and/or check the size of vf.o with and without the
 * additional header files from vf.h.
 * 
 * Addendum: DGP 2000 March 15:
 *	Because we don't include vf.h, we need the prototype below to
 * satisfy picky compilers like Sun WorkShop 5.0.
 */

Tcl_PackageInitProc	Vf_Init;

#include "version.h"

/* End includes */     

int 
Vf_Init(Tcl_Interp *interp)
{
// Use macro instead of goto
#define RETURN_TCL_ERROR                                                \
    Tcl_AddErrorInfo(interp, OC_CONST84_CHAR("\n    (in Vf_Init())"));  \
    return TCL_ERROR

    if (Tcl_PkgPresent(interp, OC_CONST84_CHAR("Oc"),
		       OC_CONST84_CHAR("2"), 0) == NULL) {
        Tcl_AppendResult(interp,
		OC_CONST84_CHAR("\n\t(Vf " VF_VERSION " needs Oc 2)"),
                NULL);
        RETURN_TCL_ERROR;
    }
    if (Tcl_PkgPresent(interp, OC_CONST84_CHAR("Nb"),
		       OC_CONST84_CHAR("2"), 0) == NULL) {
        Tcl_AppendResult(interp,
		OC_CONST84_CHAR("\n\t(Vf " VF_VERSION " needs Nb 2)"),
                NULL);
        RETURN_TCL_ERROR;
    }
    if (Tcl_PkgProvide(interp, OC_CONST84_CHAR("Vf"),
		       OC_CONST84_CHAR(VF_VERSION)) != TCL_OK) {
        RETURN_TCL_ERROR;
    }
    if (Oc_InitScript(interp, "Vf", VF_VERSION) != TCL_OK) {
        RETURN_TCL_ERROR;
    }

    return TCL_OK;

#undef RETURN_TCL_ERROR
}

