/* FILE: ocnuma.cc                   -*-Mode: c++-*-
 *
 *      Support for C-level NUMA (non-uniform memory access) code
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * NOTE: The routines in this file are *not* thread safe, and should be
 *       called *only* from the main thread, preferably during
 *       initialization.
 *
 * Last modified on: $Date: 2015/10/09 21:23:12 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>
#include <cerrno>

#include "ocexcept.h"
#include "ocnuma.h"
#include "messages.h"

#if OC_USE_NUMA
// Note: The numa-devel package is needed to get numaif.h.
# include <numaif.h>
#endif // OC_USE_NUMA

void Oc_StrError(int errnum,char* buf,size_t buflen);  // From oc.cc

/* End includes */     /* Optional directive to pimake */

#if !OC_USE_NUMA
void* Oc_AllocThreadLocal(size_t size)
{
  void* foo = malloc(size);
  if(!foo) { OC_THROW("Out of memory."); }
  return foo;
}
void Oc_FreeThreadLocal(void* start,size_t /* size */)
{
  if(!start) return; // Free of NULL pointer always allowed.
  free(start);
}

// NOTE: non-NUMA versions of the other routines defined in
// this file are found in the ocnuma.h header.

// NOTE 2: See also the Tcl wrappers at the bottom of this
// file.
#endif // !OC_USE_NUMA

#if OC_USE_NUMA
// On some kernels, with some versions of the NUMA library (at least,
// Linux 2.6.35.13-91.fc14.x86_64 + numactl-2.0.3-8.fc13.x86_64),
// there is a bug than can cause a segfault and kernel oops, with
// this error reported in /var/log/messages:
//
//    kernel BUG at mm/mmap.c:2381!
//    invalid opcode: 0000 [#1] SMP 
//    last sysfs file: /sys/devices/system/node/node3/cpumap
//   <snip>
// 
// The line 2381 in mmap.c is a "BUG_ON" line in the kernel source:
//
//    /* Release all mmaps. */
//    void exit_mmap(struct mm_struct *mm)
//    {
//            struct mmu_gather *tlb;
//            struct vm_area_struct *vma;
//            unsigned long nr_accounted = 0;
//            unsigned long end;
//
//            /* mm's last user has gone, and its about to be pulled down */
//            mmu_notifier_release(mm);
// <snip>
//            /*
//             * Walk the list again, actually closing and freeing it,
//             * with preemption enabled, without holding any MM locks.
//             */
//            while (vma)
//                    vma = remove_vma(vma);
//
//            BUG_ON(mm->nr_ptes > (FIRST_USER_ADDRESS+PMD_SIZE-1)>>PMD_SHIFT);
//    }
//
// I don't know the root problem, but it seems it can be triggered if
// mbind is called with an address range where the first or last
// memory page is not wholly contained in one array.  For example,
//
//    char* start = (char*)malloc(len);
//    mbind(start - start%PAGESIZE,len + (start%PAGESIZE),
//          MPOL_PREFERRED,nodemask,nodemask_size,0);
//
// If start is not on a PAGESIZE boundary, then the first part of the
// page may be allocated to other variables, which may be written by
// other threads with different memory policy.  Similar considerations
// apply at the far end.  The kernel oopsing appears to go away if the
// address range to mbind is rounded up (rather than down) to a page
// boundary, and if len is adjusted downwarded to a full page.
//   On the same platform, numa_alloc_onnode() suffers from what looks
// like the same bug, but unlike mbind there is no apparent
// workaround.  See the follow-on discussion below in
// Oc_AllocThreadLocal(), but it seems the best course of action is to
// avoid numa_alloc_onnode and use instead mbind with the above
// workaround.
//   The macro OC_BAD_MBIND_OOPS controls whether or not the above
// mbind workaround is implemented.  So far as I can tell, this is
// not a problem with the NUMA 0.9 series.  I don't know one way
// or the other wrt to the short-lived NUMA 1.x series.
//   The macro OC_BAD_MBIND_MAXNODE turns on a workaround for a bug in
// mm/mempolicy.c.  It affects the code below through the mbind call ---
// the docs seem to indicate that the value for maxnode should be
// numa_max_node()+1, but all the example code I could find used
// numa_max_node()+2.  On 26-July-2010, Andre Przywara submitted a patch
// for mm/mempolicy.c,
//
//    https://patchwork.kernel.org/patch/114250/
//
// about which he writes:
//
//      When the mbind() syscall implementation processes the node
//      mask provided by the user, the last node is accidentally
//      masked out. This is present since the dawn of time (aka
//      Before Git), I guess nobody realized that because libnuma as
//      the most prominent user of mbind() uses large masks
//      (sizeof(long)) and nobody cared if the 64th node is not
//      handled properly. But if the user application defers the
//      masking to the kernel and provides the number of valid bits
//      in maxnodes, there is always the last node missing.  However
//      this also affect the special case with maxnodes=0, the
//      manpage reads that mbind(ptr, len, MPOL_DEFAULT, &some_long,
//      0, 0); should reset the policy to the default one, but in
//      fact it returns EINVAL.  This patch just removes the
//      decrease-by-one statement, I hope that there is no
//      workaround code in the wild that relies on the bogus
//      behavior.
//
// So, maybe numa_max_node()+2 in the example code is a workaround for
// this bug?  I have verified that in the NUMA 0.9 and 2.0.3 series
// that if maxnode is set to any value smaller than numa_max_node()+2,
// then flags setting the top node (i.e., node number numa_max_node())
// are ignored.

#ifndef OC_BAD_MBIND_OOPS
# if defined(LIBNUMA_API_VERSION) && LIBNUMA_API_VERSION>=2
#  define OC_BAD_MBIND_OOPS 1
# else
#  define OC_BAD_MBIND_OOPS 0
# endif
#endif // OC_BAD_MBIND_OOPS

#ifndef OC_BAD_MBIND_MAXNODE
# define OC_BAD_MBIND_MAXNODE 1
#endif // OC_BAD_MBIND_MAXNODE

/* The client selects which node each thread runs on via the import
 * "nodes" to Oc_NumaInit.  If the no nodes are specified, then in the
 * current implementation the default node list starts in the middle
 * of the system node range and spirals outward.  For example, for a
 * system with eight nodes the default nodes list is 4, 3, 5, 2, 6, 1,
 * 7, 0.  This default may change in the future without notice --- in
 * particular, it might be good to use node topology information from
 * numa_distance() or 'numactl --hardware' to let successive threads
 * lie on nearby nodes.
 *
 * If there are more threads than nodes, then successive threads are
 * grouped together and the groups divided up as equally as possible
 * among the nodes, with extra threads distributed among the earlier
 * nodes in the nodes list first.  For example, if the node list is 2,
 * 1, 3, 0 and 10 threads are requested, then the thread to node
 * mapping would be
 *
 *        Thread   Node      Thread   Node   
 *           0 ---> 2           5 ---> 1     
 *           1 ---> 2           6 ---> 3     
 *           2 ---> 2           7 ---> 3     
 *           3 ---> 1           8 ---> 0
 *           4 ---> 1           9 ---> 0     
 *
 * so 3 threads map to nodes 2 and 1, and 2 threads to 3 and 0.
 *
 * User specified nodes list may repeat nodes in the list.  In
 * particular, if the length of the import nodes list equals the
 * number of threads then the list directly specifies the thread to
 * node mapping.
*/ 

static vector<int> nodeselect;
static int numa_ready = 0;  // Set to 1 by Oc_NumaInit.

void Oc_NumaDisable() {
  numa_ready = 0;
  nodeselect.clear();
  // Turn off interleaving
#if defined(LIBNUMA_API_VERSION) && LIBNUMA_API_VERSION>=2
  numa_set_interleave_mask(numa_no_nodes_ptr);
#else
  numa_set_interleave_mask(&numa_no_nodes);
#endif
}

void Oc_NumaInit(int max_threads,vector<int>& nodes) {
  // Init numa library
  assert(max_threads>0);
  numa_ready = 0;
  if(numa_available() < 0) {
    OC_THROW("ProgramLogicError: NUMA (non-uniform memory access)"
             " library not available.");
  }

#if 0
  numa_set_bind_policy(1);  /**/ // TEST
  numa_set_strict(1);
#endif
  // Set up nodes select list
  nodeselect.clear();
  if(nodes.size() == 0) {
    // Use default node list
    int node_count = numa_max_node()+1;
    int threads_per_node = max_threads/node_count;
    int leftovers = max_threads - threads_per_node*node_count;
    int work_node = node_count/2;
    for(int i=0;i<node_count;++i) {
      for(int j=0; j<threads_per_node + (i<leftovers ? 1:0); ++j) {
        nodeselect.push_back(work_node);
      }
      if(2*work_node>=node_count) {
        work_node = node_count - work_node - 1;
      } else {
        work_node = node_count - work_node;
      }
    }
  } else {
    // Use user supplied node list
    int node_count = nodes.size();
    int threads_per_node = max_threads/node_count;
    int leftovers = max_threads - threads_per_node*node_count;
    for(int i=0;i<node_count;++i) {
      if(nodes[i]<0 || nodes[i]>numa_max_node()) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_NumaInit",1024,
           "NUMA (non-uniform memory access) init error;"
           " requested node (%d) is outside machine node range [0-%d].",
           nodes[i],numa_max_node()));
      }
      for(int j=0; j<threads_per_node + (i<leftovers ? 1:0); ++j) {
        nodeselect.push_back(nodes[i]);
      }
    }
  }
  assert(nodeselect.size() == size_t(max_threads));

  // Memory interleaving
#if defined(LIBNUMA_API_VERSION) && LIBNUMA_API_VERSION>=2
  struct bitmask* mask = numa_allocate_nodemask();
  numa_bitmask_clearall(mask);
  int ibitmax = static_cast<int>(nodeselect.size());
  for(int ibit=0;ibit<ibitmax;++ibit) {
    numa_bitmask_setbit(mask,static_cast<unsigned int>(nodeselect[ibit]));
  }
  numa_set_interleave_mask(mask);
  numa_free_nodemask(mask);
#else // LIBNUMA_API_VERSION
  nodemask_t mask;
  nodemask_zero(&mask);
  int ibitmax = static_cast<int>(nodeselect.size());
  for(int ibit=0;ibit<ibitmax;++ibit) {
    nodemask_set(&mask,nodeselect[ibit]);
  }
  numa_set_interleave_mask(&mask);
#endif // LIBNUMA_API_VERSION
  numa_ready = 1;
}

int Oc_NumaReady()
{
  return numa_ready;
}

void Oc_NumaRunOnNode(int thread)
{
  if(nodeselect.size()>0) {
    if(!numa_ready) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_NumaRunOnNode",
       1024,
       "NUMA (non-uniform memory access) init error;"
       " numa_run_on_node called w/o NUMA library initialization"));
    }
    assert(0<=thread && size_t(thread)<nodeselect.size());
    if(numa_run_on_node(nodeselect[thread]) != 0) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_NumaRunOnNode",1024,
          "Error in numa_run_on_node(%d) call",int(nodeselect[thread])));
    }
  }
}

void Oc_NumaRunOnAnyNode()
{
  if(!numa_ready) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_NumaRunOnAnyNode",
     1024,
     "NUMA (non-uniform memory access) init error;"
     " numa_run_on_node called w/o NUMA library initialization"));
  }
  if(numa_run_on_node(-1) != 0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_NumaRunOnAnyNode",
       1024,
       "Error in numa_run_on_node(-1) call"));
  }
}


// Oc_SetMemoryPolicyFirstTouch sets the node allocation policy for
//  the given memory address range to "First Touch", meaning each page
//  is allocated to the node running the thread that first accesses
//  a location on that page.  This overrides the Oc_Numa default of
//  interleaved across the nodes specified in Oc_NumaInit.  Note that
//  Oc_SetMemoryPolicyFirstTouch must be called after memory allocation
//  (so an address is held) but before memory use.
void Oc_SetMemoryPolicyFirstTouch(char* start,OC_UINDEX len)
{
  // What to do if the numa library is not initialized?  Options are
  // throw an exception or else return without doing anything.  The
  // advantage of the latter is that it may allow NUMA-compiled code
  // to run on systems w/o a working NUMA installation (the libraries
  // can be installed but won't be functional if the procfs isn't
  // setup for numa).  The advantage of the former is that it may
  // catch some coding errors.  For now, try the latter.
  if(!numa_ready) return;

  // For some reason, mbind reports an EINVAL (illegal parameter) error
  // if called as below if the system has only one node.  This routine
  // should be a nop in that case, so just handle this case specially.
  if(numa_max_node()==0) return;

  if(len == 0) return; // Duh

  // The original documentation on mbind does not mention this, and
  // later documentation is not quite as direct about this as it could
  // be, but mbind returns an error if the import "start" is not on a
  // page boundary.  Memory policy is set by memory page, so it is not
  // possible to select a subset of a page.  So, effectively, the
  // imports to mbind should specify full pages; in other words, import
  // start should point to the start of a page, and import len should be
  // a multiple of pagesize.  In practice, mbind returns an error if
  // start doesn't point to the start of a page, but silently rounds len
  // up to the next multiple of pagesize.
  //    The first attempt at this code rounded start down to a page
  // boundary.  The docs say that if the page is already assigned then
  // mbind has no effect, so this seems a reasonable course of action,
  // and appeared to work well with the 0.9.8 version of numactl.
  // Unfortunately, with numactl-2.0.3-8.fc13.x86_64 this tactic can
  // produce to kernel oopses, so I guess we have to round up instead.
  // For big allocs it shouldn't make much difference.
  OC_UINDEX pgsize = numa_pagesize();
  OC_INDEX offset = reinterpret_cast<OC_UINDEX>(start) % pgsize;
#if OC_BAD_MBIND_OOPS
  if(offset>0) {
    OC_UINDEX clip = pgsize - offset;
    start += clip; // Round up to page boundary
    if(len<=clip) len = 0;
    else          len -= clip;
  }
  len -= (len % pgsize); // Round down to page boundary
  if(len == 0) return;
#else // OC_BAD_MBIND_OOPS
  start -= offset; // Round down to page boundary
  len += offset;
  offset = len % pgsize;
  if(offset>0) {
    offset += pgsize - offset; // Round up to page boundary
  }
#endif // OC_BAD_MBIND_OOPS

  // Older documentation is not as clear as it could be on this point,
  // but calling mbind with MPOL_PREFERRED and an empty node mask
  // implements a page-by-page first touch policy.  Current docs
  // (Jun-2011) state this more clearly, but also claim that an empty
  // node mask is indicated by a NULL value for nodemask (4th arg) or a
  // 0 value for maxnode (5th arg).  But numactl-2.0.3-8.fc13.x86_64
  // spits back an invalid argument if maxnode == 0 --- this is
  // presumably due to the mm/mempolicy.c patched by Andre Przywara on
  // 26-July-2010.  It doesn't matter here, though, because we want all
  // node flags cleared anyway.  But see the use of mbind in
  // Oc_AllocThreadLocal below.
  int errcode = mbind(start,len,MPOL_PREFERRED,NULL,numa_max_node()+1,0);
  if(errcode != 0) {
    int errsav = errno;  // Save code from global errno
    char errbuf[1024];
    Oc_StrError(errsav,errbuf,sizeof(errbuf));
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"",
                          "Oc_SetMemoryPolicyFirstTouch",
                          1024,"%.256s; start=%lX, len=%lX, max_node=%d",
                          errbuf,
                          (unsigned long)start,
                          (unsigned long)len,
                          numa_max_node()));
  }
}

int Oc_NumaGetRunNode(int thread)
{
  if(nodeselect.size()>0) {
    assert(0<=thread && size_t(thread)<nodeselect.size());
    return nodeselect[thread];
  }
  return -1;
}

int Oc_NumaGetLocalNode()
{ // Returns lowest-numbered node in run mask for current thread.
  if(!Oc_NumaAvailable()) {
    // An alternative to throwing an exception here is to suppose this
    // is a NUMA build running w/o a working NUMA library, and just
    // return 0 like the non-NUMA Oc_NumaGetLocalNode() counterpart.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_NumaGetLocalNode",
       1024,
       "NUMA (non-uniform memory access) init error;"
       " Oc_NumaGetLocalNode() called w/o NUMA library initialization"));
  }
#if defined(LIBNUMA_API_VERSION) && LIBNUMA_API_VERSION>=2
  struct bitmask* mask = numa_get_run_node_mask();
  const int top_node = numa_max_node();
  int node = 0;
  while(node <= top_node) {
    if(numa_bitmask_isbitset(mask,(unsigned int)node)) break;
    ++node;
  }
  numa_free_nodemask(mask); // Docs don't say who is responsible for
  /// freeing mask, but some sample code in the numa compatibility
  /// library does this, so we will too.
  if(node>top_node) return -1;
  return node;
#else
  nodemask_t mask = numa_get_run_node_mask();
  int node = 0;
  const int top_node = numa_max_node();
  while(node <= top_node) {
    if(nodemask_isset(&mask,node)) return node;
    ++node;
  }
  return -1;
#endif
}

////////////////////////////////////////////////////////////////////////
// Memory allocation
//
// The Oc_AllocThreadLocal function is modeled on numa_alloc_local or
// numa_alloc_onnode.  But these calls potentially suffer the kernel
// oops problem discussed at the top of this file in the comments
// concerning the OC_BAD_MBIND_OOPS macro, w/o any obvious workaround.
// Also, if we use numa_alloc_*, then the memory needs to be released
// via numa_free, but if this code, although build NUMA-enabled, is
// run w/o NUMA (either at user discretion or because the hardware was
// booted without NUMA support), then we want to be able to fall back
// to using malloc + free.  That means we have to keep track of which
// freeing method to use at runtime.  All-in-all, it seems better to
// ditch the numa_alloc_* family and just use alloc + mbind + free.
void* Oc_AllocThreadLocal(size_t size)
{
  void* start = malloc(size);
  if(!start) { OC_THROW("Out of memory (in Oc_AllocThreadLocal())."); }
  if(!numa_ready) return start;  // NUMA not in use

  // mbind requires starting address to be page-aligned.  Include
  // here too workarounds for OC_BAD_MBIND_OOPS problems.
  const unsigned long PGSIZE = numa_pagesize();
  char* pgstart = (char*)start;
  unsigned long pgsize = size;
  unsigned long pgoffset = (unsigned long)pgstart % PGSIZE;
#if OC_BAD_MBIND_OOPS
  if(pgoffset>0) {
    pgoffset = PGSIZE - pgoffset;
    pgstart += pgoffset;  // Round up to next page
    if(pgsize<pgoffset) pgsize=0;  // Mark region as too short
    else                pgsize -= pgoffset;
  }
  pgsize -= (pgsize % PGSIZE);     // Round down to page boundary
#else // OC_BAD_MBIND_OOPS
  if(pgoffset>0) {
    pgstart -=  pgoffset;  // Round down to page boundary
    pgsize += pgoffset;
  }
  pgoffset = (pgsize % PGSIZE);
  if(pgoffset>0) {
    pgsize += PGSIZE - pgoffset; // Round up to page boundary
  }
#endif // OC_BAD_MBIND_OOPS
  if(pgsize == 0) return start; // Region too small for mbind

  // Match memory allocation to thread run mask
#if defined(LIBNUMA_API_VERSION) && LIBNUMA_API_VERSION>=2

  struct bitmask* runmask = numa_get_run_node_mask();

  // See note earlier in this file about Andre Przywara's
  // mm/mempolicy.c bug fix for the reasons behind the following juju:
#if OC_BAD_MBIND_MAXNODE
  unsigned long maxnode = (unsigned long)numa_max_node() + 2;
  if(maxnode > runmask->size) maxnode = runmask->size;
#else
  unsigned long maxnode = (unsigned long)numa_max_node() + 1;
#endif // OC_BAD_MBIND_MAXNODE

  // Policy options: MPOL_PREFERRED (try) and MPOL_BIND (required)
  int errcode = mbind(pgstart,pgsize,MPOL_PREFERRED,runmask->maskp,maxnode,0);

  // Docs don't say who or how to free runmask, but judging from the
  // code in numactl.c fomr numactl-2.0.3, it is us and like so:
  numa_free_cpumask(runmask);

#else // LIBNUMA_API_VERSION (Old NUMA API)

  nodemask_t runmask = numa_get_run_node_mask();
#if OC_BAD_MBIND_MAXNODE
  unsigned long maxnode = (unsigned long)numa_max_node() + 2;
#else
  unsigned long maxnode = (unsigned long)numa_max_node() + 1;
#endif // OC_BAD_MBIND_MAXNODE
  int errcode = mbind(pgstart,pgsize,MPOL_PREFERRED,runmask.n,maxnode,0);

#endif // LIBNUMA_API_VERSION

  if(errcode != 0) {
    int errsav = errno;  // Save code from global errno
    char errbuf[1024];
    Oc_StrError(errsav,errbuf,sizeof(errbuf));
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"",
                          "Oc_AllocThreadLocal",1024,
                          "%.256s; start=%lX, len=%lX, max_node=%d",
                          errbuf,
                          (unsigned long)pgstart,
                          (unsigned long)pgsize,
                          numa_max_node()));
  }
  return start;
}

void Oc_FreeThreadLocal(void* start,size_t /* size */)
{
  if(!start) return; // Free of NULL pointer always allowed.
  free(start);
}


void Oc_NumaNodemaskStringRep(vector<int>& imask,Oc_AutoBuf& ab)
{ // Converts a vector<int> representing a NUMA interleave
  // mask to a string representation.
  const int node_count = imask.size();
  unsigned int buf = 0;
  unsigned int bit = 1;

  Oc_AutoBuf reverse_ab;
  for(int i=0;i<node_count;++i) {
    if(imask[i]) {
      buf |= bit;
    }
    if( (bit <<= 1) == 0x10 ) {
      char cbuf[2];
      Oc_Snprintf(cbuf,sizeof(cbuf),"%X",buf);
      reverse_ab.Append(cbuf); // "Append" adds to the right, so
      buf = 0;                /// this stores the digits backwards.
      bit = 1;
    }
  }
  if(bit>1) { // Dump leftovers
    char cbuf[2];
    Oc_Snprintf(cbuf,sizeof(cbuf),"%X",buf);
    reverse_ab.Append(cbuf);
  }

  // Reverse and return
  const int strsize = static_cast<int>(strlen(reverse_ab.GetStr()));
  if(strsize==0) {
    ab = "0";
  } else {
    ab.SetLength(strsize);
    for(int j=0;j<strsize;++j) {
      ab[j] = reverse_ab[strsize-1-j];
    }
    ab[strsize] = '\0';
  }
}

void Oc_NumaGetInterleaveMask(Oc_AutoBuf& ab)
{ // Wrapper around nodemask_t numa_get_interleave_mask(void),
  // which fills export "ab" with a hexadecimal rendering of
  // the node mask.
  if(!numa_ready) {
    ab = "numa library not initialized";
  } else {
    vector<int> imask;
    const int maxnode = numa_max_node();
#if defined(LIBNUMA_API_VERSION) && LIBNUMA_API_VERSION>=2
    struct bitmask* bmask = numa_get_interleave_mask();
    for(int i=0;i<=maxnode;++i) {
      imask.push_back(numa_bitmask_isbitset(bmask,i));
    }
    numa_free_nodemask(bmask); // Docs don't say who is responsible for
    /// releasing this memory, but presumably they are us.
#else // Old LIBNUMA
    nodemask_t nmask = numa_get_interleave_mask();
    for(int i=0;i<=maxnode;++i) {
      imask.push_back(nodemask_isset(&nmask,i));
    }
#endif
    Oc_NumaNodemaskStringRep(imask,ab);
  }
}

void Oc_NumaGetRunMask(Oc_AutoBuf& ab)
{ // Wrapper around nodemask_t numa_get_run_node_mask(void),
  // which fills export "ab" with a hexadecimal rendering of
  // the node mask.
  if(!numa_ready) {
    ab = "numa library not initialized";
  } else {
    vector<int> imask;
    const int maxnode = numa_max_node();
#if defined(LIBNUMA_API_VERSION) && LIBNUMA_API_VERSION>=2
    struct bitmask* bmask = numa_get_run_node_mask();
    for(int i=0;i<=maxnode;++i) {
      imask.push_back(numa_bitmask_isbitset(bmask,i));
    }
    numa_free_nodemask(bmask); // Docs don't say who is responsible for
    /// releasing this memory, but presumably they are us.
#else // Old LIBNUMA
    nodemask_t nmask = numa_get_run_node_mask();
    for(int i=0;i<=maxnode;++i) {
      imask.push_back(nodemask_isset(&nmask,i));
    }
#endif
    Oc_NumaNodemaskStringRep(imask,ab);
  }
}
#endif // OC_USE_NUMA

////////////////////////////////////////////////////////////////////////
// Tcl wrappers for NUMA code.

// Note 1: The numa_available() function from the NUMA library returns a
//   negative value on error, a 0 or positive value on success.  This
//   is rather non-intuitive; the Oc_NumaAvailable routine and the
//   associated Tcl wrapper return 1 on success, 0 on error (i.e., if
//   NUMA routines are not available).
// Note 2: Unlike Oc_NumaInit, the OcNumaInit wrapper also calls
//   Oc_NumaRunOnNode to tie the current thread to the first node.
int
OcNumaAvailable(ClientData, Tcl_Interp *interp, int argc,CONST84 char **argv) 
{
  Tcl_ResetResult(interp);
  if (argc != 1) {
    Tcl_AppendResult(interp, argv[0], " takes no arguments", (char *) NULL);
    return TCL_ERROR;
  }
  char buf[256];
  Oc_Snprintf(buf,sizeof(buf),"%d", Oc_NumaAvailable() );
  Tcl_AppendResult(interp,buf,(char *)NULL);
  return TCL_OK;
}

int
OcNumaDisable(ClientData, Tcl_Interp *interp, int argc,CONST84 char **argv) 
{
  Tcl_ResetResult(interp);
  if (argc != 1) {
    Tcl_AppendResult(interp, argv[0], " takes no arguments", (char *) NULL);
    return TCL_ERROR;
  }
  Oc_NumaDisable();
  return TCL_OK;
}

int
OcNumaInit(ClientData,Tcl_Interp *interp,int argc,CONST84 char** argv)
{
  char buf[1024];
  Tcl_ResetResult(interp);
  if(argc!=3) {
    Oc_Snprintf(buf,sizeof(buf),"Oc_NumaInit must be called with 2 arguments,"
	    " max_thread_count and node_list (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int max_thread_count=atoi(argv[1]);

  if(max_thread_count<1) {
    Oc_Snprintf(buf,sizeof(buf),"Import max_thread_count to Oc_NumaInit"
                " must be a positive integer (got %d)", max_thread_count);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  if(!Oc_NumaReady() && !Oc_NumaAvailable()) {
    Oc_Snprintf(buf,sizeof(buf),"Unable to initialize NUMA library.");
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  int nodecount = Oc_NumaGetNodeCount();

  vector<int> nodes;
  int node_argc;
  CONST84 char** node_argv;
  if(TCL_OK != Tcl_SplitList(interp, argv[2], &node_argc, &node_argv)) {
    Oc_Snprintf(buf,sizeof(buf),"Import node_list to Oc_NumaInit"
                " is not a proper list (got \"%.500s\")", argv[2]);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }
  for(int i=0;i<node_argc;++i) {
    char* endptr;
    long int number = strtol(node_argv[i],&endptr,10);
    if(node_argv[i] == endptr || *endptr != '\0') {
      // Invalid element
      Oc_Snprintf(buf,sizeof(buf),"Import node_list to Oc_NumaInit"
                  " is not a list of numbers (element %d is \"%.500s\")",
                  i,node_argv[i]);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      Tcl_Free((char *)node_argv);
      return TCL_ERROR;
    }
    if(number<0) {
      // Invalid element
      Oc_Snprintf(buf,sizeof(buf),"Import node_list to Oc_NumaInit"
                  " is not a list of non-negative numbers (element %d is %d)",
                  i,number);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      Tcl_Free((char *)node_argv);
      return TCL_ERROR;
    }
    if(number > nodecount-1) {
      // Invalid element
      Oc_Snprintf(buf,sizeof(buf),"Import node_list to Oc_NumaInit"
                  " requests an unavailable node (element %d is %d>%d)",
                  i,number,nodecount-1);
      Tcl_AppendResult(interp,buf,(char *)NULL);
      Tcl_Free((char *)node_argv);
      return TCL_ERROR;
    }
    nodes.push_back(number);
  }
  Tcl_Free((char *)node_argv);

#if OC_USE_NUMA	
/*
 * Without OC_USE_NUMA, the calls are no-ops, and we can avoid trouble
 * some cranky old compilers have with this "try".
 */
  try {
    Oc_NumaInit(max_thread_count,nodes);
    Oc_NumaRunOnNode(0); // Tie main thread (i.e., this one) to first node
  } catch(Oc_Exception& ocerr) {
    Oc_AutoBuf ab;
    ocerr.ConstructMessage(ab);
    Tcl_AppendResult(interp,ab.GetStr(),(char *)NULL);
    return TCL_ERROR;
  } catch(const char* errmsg) {
    Tcl_AppendResult(interp,errmsg,(char *)NULL);
    return TCL_ERROR;
  }
#endif

  return TCL_OK;
}
