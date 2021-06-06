/* FILE: nb.h                    -*-Mode: c++-*-
 *
 *	The OOMMF Nuts & Bolts extension public header file.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/11/09 00:50:03 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB
#define _NB

// Version information
#include "version.h"
#include "oc.h"
#define NB_VERSION OC_MAKE_VERSION(NB)

// Prototypes of functions in this extension
#include "crc.h"
#include "functions.h"

// Declarations of classes in this extension
#include "array.h"
#include "arraywrapper.h"
#include "dlist.h"
#include "dstring.h"
#include "floatvec.h"
#include "imgobj.h"
#include "stopwatch.h"
#include "tclcommand.h"
#include "tclobjarray.h"
#include "xpfloat.h"

// Stuff slated for eventual movement into oc extension
#include "evoc.h"
#include "errhandlers.h"
#include "messagelocker.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// This insures standard math functions like floor, ceil, sqrt
/// are available and overloaded properly by argument type.

/* End includes */     // Optional directive to pimake

// Prototypes for the routines defined in nb.cc
Tcl_PackageInitProc	Nb_Init;

#endif /* _NB */

