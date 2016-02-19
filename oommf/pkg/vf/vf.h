/* FILE: vf.h                    -*-Mode: c++-*-
 *
 *	The OOMMF Vector Field extension public header file.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2000/08/25 17:01:26 $
 * Last modified by: $Author: donahue $
 */

#ifndef _VF
#define _VF

// Prototypes of functions in this extension

// Declarations of classes in this extension
#include "fileio.h"
#include "mesh.h"
#include "ptsearch.h"
#include "vecfile.h"
#include "version.h"

/* End includes */     // Optional directive to pimake

// Prototypes for the routines defined in vf.cc
Tcl_PackageInitProc	Vf_Init;

#endif /* _VF */
