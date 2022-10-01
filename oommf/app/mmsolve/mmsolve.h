/* FILE: mmsolve.h                 -*-Mode: c++-*-
 *
 * Standard macro definitions.
 *
 */

#ifndef MMSOLVE_H
#define MMSOLVE_H

#include "oc.h"
#include "vf.h"

#ifdef USE_MPI
#include "mmsmpi.h"
#include "fft.h"  // Needed for MyComplex definition.
#endif /* USE_MPI */

/* End includes */

/*
 * NOTE: When version number information is changed here, it must
 * also be changed in the following files:
 *
 */
 
#define MMSOLVE_MAJOR_VERSION    2
#define MMSOLVE_MINOR_VERSION    0
#define MMSOLVE_RELEASE_LEVEL    "b"
#define MMSOLVE_RELEASE_SERIAL   0

#define MMSOLVE_VERSION OC_MAKE_VERSION(MMSOLVE)

Tcl_CmdProc Mms_MifCmd;
Tcl_CmdProc Mms_Grid2DCmd;
Tcl_PackageInitProc	Mmsolve_Init;
Tcl_PackageInitProc	Mmsolve_SafeInit;

#endif // MMSOLVE_H
