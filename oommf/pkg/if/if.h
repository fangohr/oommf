/* FILE: if.h                    -*-Mode: c++-*-
 *
 *	The OOMMF Image Formats extension public header file.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2000/08/25 17:01:25 $
 * Last modified by: $Author: donahue $
 */

#ifndef _IF
#define _IF

#include "version.h"
#include "oc.h"

#define IF_VERSION OC_MAKE_VERSION(IF)

/* End includes */     // Optional directive to pimake

// Prototypes for the routines defined in if.cc
Tcl_PackageInitProc If_Init;

#endif /* _IF */
