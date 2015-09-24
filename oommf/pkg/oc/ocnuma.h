/* FILE: ocnuma.h                   -*-Mode: c++-*-
 *
 *      Support for C-level multi-threaded (parallel) code
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2012-08-25 00:04:00 $
 * Last modified by: $Author: donahue $
 *
 * NOTE: The routines in this file are *not* thread safe, and should be
 *       called *only* from the main thread, preferably during
 *       initialization.
 */

#ifndef _OCNUMA
#define _OCNUMA

#include <vector>

/* Include file imports.h #includes ocport.h and tcl.h (among others).
 * Note that OC_USE_NUMA is defined in ocport.h, so this should come
 * before the #if OC_USE_NUMA line below.
 */
#include "imports.h"

/* NUMA (non-uniform memory access) support */
#if OC_USE_NUMA
// Note: The numa-devel package is needed to get numa.h.
# include <numa.h>
#endif // OC_USE_NUMA

#include "autobuf.h"

/* End includes */     /* Optional directive to pimake */

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// Used by "vector" template.

// NOTE: The numa_available() function from the NUMA library returns a
// negative value on error, a 0 or positive value on success.  This
// is rather non-intuitive; the Oc_NumaAvailable routine and the associated
// Tcl wrapper return 1 on success, 0 on error (i.e., if NUMA routines
// are not available).
#if OC_USE_NUMA
inline int  Oc_NumaAvailable() {
  if(numa_available()>=0) return 1;
  return 0;
}
int Oc_NumaReady(); // Returns 1 if Oc_NumaInit has been successfully
/// run; 0 if Oc_NumaInit hasn't been called (perhaps the user requested
/// a non-NUMA run) or if Oc_NumaInit failed.

// Oc_NumaInit initializes the numa library and sets up the run node
//  select array.
// Oc_NumaRunOnNode restricts the current thread to run on the node
//  specifiec by the run node select array for thread "thread".  This
//  is intended for use by these octhread support classes.
// Oc_SetMemoryPolicyFirstTouch sets the node allocation policy for
//  the given memory address range to "First Touch", meaning each page
//  is allocated to the node running the thread that first accesses
//  a location on that page.  This overrides the Oc_Numa default of
//  interleaved across the nodes specified in Oc_NumaInit.  Note that
//  Oc_SetMemoryPolicyFirstTouch must be called after memory allocation
//  (so an address is held) but before memory use.
void Oc_NumaDisable();
void Oc_NumaInit(int max_threads,vector<int>& nodes);
void Oc_NumaRunOnNode(int thread);
void Oc_SetMemoryPolicyFirstTouch(char* start,OC_UINDEX len);
// Oc_NumaGetRunNode queries the node select array to find out which
//   node thread "thread" is suppose to run on.
// Oc_NumaGetLocalNode queries the numa run mask for the current
//   thread, and returns the lowest numbered node in the mask.
// Oc_NumaNodeFirstThread returns the threadid for the first thread
//   in the run node select array that runs on node "node"
int Oc_NumaGetRunNode(int thread);    // Returns -1 on error
int Oc_NumaGetLocalNode();            // Returns -1 on error
int Oc_NumaNodeFirstThread(int node); // Returns -1 on error
inline int Oc_NumaGetNodeCount() { return numa_max_node()+1; }
void Oc_NumaGetInterleaveMask(Oc_AutoBuf& ab);
void Oc_NumaGetRunMask(Oc_AutoBuf& ab);

#else // OC_USE_NUMA
inline int  Oc_NumaAvailable() { return 0; }
inline int  Oc_NumaReady() { return 0; }
inline void Oc_NumaDisable() {}
inline void Oc_NumaInit(int /* max_threads */, vector<int>& /* nodes */) {}
inline void Oc_NumaRunOnNode(int /* thread */) {}
inline int  Oc_NumaGetRunNode(int /* thread */) { return 0; }
inline void Oc_SetMemoryPolicyFirstTouch(char*,OC_UINDEX) {}
inline int  Oc_NumaGetLocalNode()               { return 0; }
inline int  Oc_NumaNodeFirstThread(int node) {
  if(node==0) return 0;
  return -1;
}
inline int Oc_NumaGetNodeCount() { return 1; }
inline void Oc_NumaGetInterleaveMask(Oc_AutoBuf& ab) { ab = "1"; }
inline void Oc_NumaGetRunMask(Oc_AutoBuf& ab) { ab = "1"; }
#endif // OC_USE_NUMA

// Memory allocation.  If NUMA is enabled, then Oc_AllocThreadLocal
// will allocate memory on the local node.  If NUMA is not enabled,
// then Oc_AllocThreadLocal falls back to a vanilla malloc.
// Memory allocated using Oc_AllocThreadLocal must be released by
// Oc_FreeThreadLocal.  Note that Oc_FreeThreadLocal requires the
// user to keep track of the size of the allocated chunk.
void* Oc_AllocThreadLocal(size_t size);
void Oc_FreeThreadLocal(void* start,size_t size);

/* Tcl wrappers for some of the above. */
#ifdef __cplusplus
extern "C" {
#endif
  Tcl_CmdProc OcNumaAvailable;
  Tcl_CmdProc OcNumaDisable;
  Tcl_CmdProc OcNumaInit;
#ifdef __cplusplus
}	/* end of extern "C" */
#endif

#endif // _OCNUMA
