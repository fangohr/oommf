/* FILE: nullchannel.h                   -*-Mode: c++-*-
 *
 *	Routines which provide message display services
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2000/08/25 17:01:25 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NULLCHANNEL
#define _NULLCHANNEL

#include "imports.h"
/* End includes */     /* Optional directive to pimake */

/* Functions to be passed to the Tcl/Tk libraries */
Tcl_PackageInitProc	Nullchannel_Init;

/* Prototypes for the routines defined in nullchannel.cc */
Tcl_Channel	Nullchannel_Open();

#endif /* _NULLCHANNEL */
