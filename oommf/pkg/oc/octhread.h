/* FILE: octhread.h                   -*-Mode: c++-*-
 *
 *      Support for C-level multi-threaded (parallel) code
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2010/10/26 23:52:36 $
 * Last modified by: $Author: donahue $
 *
 * NOTE: Exccept where otherwise marked, the routines in this file are
 *       *not* thread safe, and should be called *only* from the main
 *       thread, preferably during initialization.
 */

#ifndef _OCTHREAD
#define _OCTHREAD

/* Include file imports.h #includes ocport.h and tcl.h (among others).
 * Note that OOMMF_THREADS is defined in ocport.h, so this should come
 * before the #if OOMMF_THREADS line below.
 */
#include "imports.h"

/* End includes */     /* Optional directive to pimake */

#if OOMMF_THREADS
inline int   Oc_HaveThreads() { return 1; }
int   Oc_GetMaxThreadCount();
void  Oc_SetMaxThreadCount(int threads);
#else
inline int   Oc_HaveThreads() { return 0; }
inline int   Oc_GetMaxThreadCount() { return 1; }
inline void  Oc_SetMaxThreadCount(int) {}
#endif

/* Tcl wrappers for some of the above. */
#ifdef __cplusplus
extern "C" {
#endif
  Tcl_CmdProc OcHaveThreads;
  Tcl_CmdProc OcGetMaxThreadCount;
  Tcl_CmdProc OcSetMaxThreadCount;
#ifdef __cplusplus
}	/* end of extern "C" */
#endif

#endif // _OCTHREAD
