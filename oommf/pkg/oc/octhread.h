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

#include <mutex>     // std::mutex, std::lock

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

// Wrappers around std mutex and lock facilities.  These are defined
// to be empty classes if OOMMF_THREADS is false, and so can be
// used w/o penalty in non-threaded OOMMF builds.
#if OOMMF_THREADS

class Oc_Mutex {
  friend class Oc_LockGuard;
private:
  std::mutex mutex;
};

class Oc_LockGuard {
private:
  std::lock_guard<std::mutex> lck;
public:
  Oc_LockGuard();  // Disallow
  Oc_LockGuard(Oc_Mutex& mtx) : lck(mtx.mutex) {}
};

#else  // OOMMF_THREADS

class Oc_Mutex {};
class Oc_LockGuard {
public:
  Oc_LockGuard(Oc_Mutex&) {}
};

#endif // OOMMF_THREADS

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
