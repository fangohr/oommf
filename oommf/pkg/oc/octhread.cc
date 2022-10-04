/* FILE: octhread.cc                   -*-Mode: c++-*-
 *
 *      Support for C-level multi-threaded (parallel) code
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * NOTE: Except where otherwise marked, the routines in this file are
 *       *not* thread safe, and should be called *only* from the main
 *       thread, preferably during initialization.
 *
 * Last modified on: $Date: 2011/03/31 22:26:47 $
 * Last modified by: $Author: donahue $
 */

#include <cerrno>

#include "ocexcept.h"
#include "octhread.h"
#include "messages.h"

/* End includes */     /* Optional directive to pimake */

/*
 * Number of threads to run
 */
#if OOMMF_THREADS
static int oommf_thread_count = 1;
int Oc_GetMaxThreadCount() { return oommf_thread_count; }

void Oc_SetMaxThreadCount(int threads) {
  if(threads<1) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"",
                          "Oc_SetMaxThreadCount",
                          1024,
                          "Import parameter error:"
                          " \"threads\" value %d must be >=1.",threads));
  }
  oommf_thread_count = threads;
}
#endif // OOMMF_THREADS

////////////////////////////////////////////////////////////////////////
// Tcl wrappers for thread code
int OcHaveThreads(ClientData, Tcl_Interp *interp, int argc,CONST84 char **argv) 
{
  Tcl_ResetResult(interp);
  if (argc != 1) {
    Tcl_AppendResult(interp, argv[0], " takes no arguments", (char *) NULL);
    return TCL_ERROR;
  }
  char buf[256];
  Oc_Snprintf(buf,sizeof(buf),"%d",Oc_HaveThreads());
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int
OcGetMaxThreadCount(ClientData, Tcl_Interp *interp, int argc,CONST84 char **argv) 
{
  Tcl_ResetResult(interp);
  if (argc != 1) {
    Tcl_AppendResult(interp, argv[0], " takes no arguments", (char *) NULL);
    return TCL_ERROR;
  }
  char buf[256];
  Oc_Snprintf(buf,sizeof(buf),"%d",Oc_GetMaxThreadCount());
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int
OcSetMaxThreadCount(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  char buf[1024];
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(buf,sizeof(buf),
            "Oc_SetMaxThreadCount must be called with 1 argument,"
	    " thread_count (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int thread_count=atoi(argv[1]);

  if(thread_count<1) {
    Oc_Snprintf(buf,sizeof(buf),
            "Oc_SetMaxThreadCount input must be a positive integer"
	    " (got %d)", thread_count);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  // Enforce thread limits (if any)
  Oc_Snprintf(buf,sizeof(buf),"Oc_EnforceThreadLimit %d",thread_count);
  Tcl_Eval(interp,buf);
  thread_count=atoi(Tcl_GetStringResult(interp));
  Tcl_ResetResult(interp);

  try {
    Oc_SetMaxThreadCount(thread_count);
  } catch(char* errmsg) {
    Tcl_AppendResult(interp,errmsg,(char *)NULL);
    return TCL_ERROR;
  }

  return TCL_OK;
}
