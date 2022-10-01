/* FILE: oxsthread.h              -*-Mode: c++-*-
 *
 * OXS thread support.
 *
 * Threaded/Non-Threaded Note: oxsthread.h and oxsthread.cc are designed
 * so that a threaded program (#if OOMMF_THREADS) can compile and run
 * properly in a non-threaded (#if !OOMMF_THREADS) context, provided that
 * one uses only the following functions and classes:
 *
 *           Oxs_ThreadPrintf
 *           Oxs_ThreadRunObj
 *           Oxs_ThreadControl
 *           Oxs_ThreadTree
 *           Oxs_ThreadMapDataObject
 *           Oxs_ThreadLocalMap
 *
 *           Oxs_ThreadBush
 *           Oxs_ThreadTwig
 *
 *           Oxs_ThreadThrowaway
 */

/* The thread timers are implemented using thread local storage, but
 * this turns out to be rather awkward.  It might be cleaner to have
 * an array of length equal to the number of threads, with each entry
 * in the array holding either an array of timers (one array for each
 * thread) or else a map that provides a reworked version of
 * Oxs_ThreadLocalMap.  The advantage of such a structure is that the
 * threads, in particular the main thread, could access
 * thread-specific data from other threads.  Of course, one needs to
 * do that in a thread-safe manner, but the main thread can do this
 * easily outside of thread commands when all of the child threads are
 * blocked.  The Oxs_Thread array would seem a natural place to put
 * the thread local array, but as it is currently constituted the main
 * thread (thread 0) is not represented in the Oxs_Thread array.  This
 * actually makes code maintenance trickier because many places one has
 * to iterate through the Oxs_Thread array but remember that the
 * length of this array is one less than the total number of threads,
 * and include special handling for main thread (aka thread 0, which
 * is not element 0 of the Oxs_Thread array).  It might be nice to
 * change that so that the main thread is represented in the
 * Oxs_Thread array (possibly by a distinct subclass up or down).
 */

#ifndef _OXS_THREAD
#define _OXS_THREAD

#include <cassert>
#include <cstring> // For memset
#include <unordered_map> // Used for thread local storage
#include <vector>

#include <functional>   // Provides support for std::function<>, which
/// is helpful for using lambda expression as template parameters.

#if OOMMF_THREADS
# include <thread>    // std::thread
# include <mutex>     // std::mutex, std::lock
# include <condition_variable> // std::condition_variable
#endif // OOMMF_THREADS

#include "oc.h"  /* includes tcl.h */
#include "nb.h"  /* Nb_WallWatch */

#include "oxsexcept.h"

/* End includes */

// Number (per thread) of thread timer objects.  Set this to zero to
// disable thread timing and reports.
//#define OXS_THREAD_TIMER_COUNT 15
#define OXS_THREAD_TIMER_COUNT 0

// Write to stderr each time memory for a Oxs_StripedArray object is
// allocated or released.
#ifndef _OXS_THREAD_TRACK_MEMORY
# define _OXS_THREAD_TRACK_MEMORY 0
#endif

/* For NUMA builds, we align stripes to pages because that is the
 * the granularity of memory mapping to nodes.  For non-NUMA, align
 * instead to cache lines, to allow more equal division of small
 * arrays among many threads.
 *   NB: If OC_PAGESIZE is smaller than the actual machine page size
 * then the memory might not get assigned to the requested node.
*/
#if OC_USE_NUMA
# define OXS_STRIPE_BLOCKSIZE OC_PAGESIZE
#else
# define OXS_STRIPE_BLOCKSIZE OC_CACHE_LINESIZE
#endif


// TODO: Check mutex and thread handling in the case where an exception
// is thrown inside one thread while other threads are running.

// Thread-safe printf
void Oxs_ThreadPrintf(FILE* fptr,const char *format, ...);

class Oxs_ThreadControl; // Forward reference

// Oxs thread number access
#if OOMMF_THREADS
int Oxs_IsRootThread();
int Oxs_GetOxsThreadNumber(); // For diagnostic use only!
#else
inline int Oxs_IsRootThread() { return 1; }
inline int Oxs_GetOxsThreadNumber() { return 0; } // For diagnostic use only!
#endif

#if OOMMF_THREADS
class Oxs_ThreadError {
private:
  static std::mutex mutex;
  static int error;
  static String msg;
public:
  static void SetError(String errmsg) {
    std::lock_guard<std::mutex> lck(mutex);
    if(error) msg += String("\n---\n");
    error=1;
    msg += errmsg;
  }
  static void ClearError() {
    std::lock_guard<std::mutex> lck(mutex);
    error=0;
    msg.clear();
  }
  static int IsError(String* errmsg=0)  {
    std::lock_guard<std::mutex> lck(mutex);
    int check=error;
    if(check && errmsg!=0) *errmsg = msg;
    return check;
  }
  static int CheckAndClearError(String* errmsg=0) {
    std::lock_guard<std::mutex> lck(mutex);
    int check=error;
    if(check) {
      if(errmsg!=0) *errmsg = msg;
      error=0;
      msg.clear();
    }
    return check;
  }
};

#else  // OOMMF_THREADS
class Oxs_ThreadError {
public:
  static void SetError(String) {}
  static void ClearError() {}
  static int IsError(String* =0)  { return 0; }
  static int CheckAndClearError(String* =0) { return 0; }
};

#endif // OOMMF_THREADS


// Thread control trio: mutex, condition, counter
#if OOMMF_THREADS
class Oxs_ThreadControl {
  // Thread control trio: mutex, condition, counter
  // NB: Be certain to lock mutex, typically using std::lock_guard or
  // std::unique_lock, before accessing count or calling cond.wait();
  //
  // TODO: The class needs to be given some thought and redesigned.
private:
  std::unique_lock<std::mutex> control_lock; // This lock is associated
  /// with mutex at object construction.  It is use for long-term
  /// locking across client routines.
public:
  std::mutex mutex;
  std::condition_variable cond;
  int count;
  Oxs_ThreadControl() : control_lock(mutex,defer_lock), count(0) {}
  ~Oxs_ThreadControl() {}
  void LockAndIncrement(int offset) {
    std::lock_guard<std::mutex> lck(mutex);
    count += offset;
  }
  void LockAndSet(int newval) {
    std::lock_guard<std::mutex> lck(mutex);
    count = newval;
  }
  void LockAndWaitForZero() {
    std::unique_lock<std::mutex> lck(mutex);
    while(count!=0) cond.wait(lck);
    // Note: Lock is freed on exit, when lck is destroyed.
  }
  void Lock() { control_lock.lock(); }
  void Unlock() { control_lock.unlock(); }
  void WaitForZero() {
    // Wait until "count" is zero.  This routine must be used in
    // conjunction with Lock() and Unlock().  In particular, Lock() must
    // be called prior to WaitForZero(), and Unlock() called sometime
    // after WaitForZero() returns.
    while(count!=0) cond.wait(control_lock);
  }
  void Wait(OC_UINT4m wait_interval=0) {
    // Wait for up to wait_interval microseconds for "count" to change.
    // Set wait_interval to 0 for no (i.e. infinite) timeout.
    //   Like WaitForZero(), this routine is to be used in conjunction
    // with Lock() and Unlock().
    int refcount = count;  // We assume lock held on entry
    if(wait_interval==0) {
      while(count == refcount) cond.wait(control_lock);
    } else {
      while(count == refcount) {
        cond.wait_for(control_lock,
                      std::chrono::microseconds(wait_interval));
      }
    }
  }
  bool OwnsLock() const noexcept { return control_lock.owns_lock(); }

  // The following notes concerning notify are from
  //  https://en.cppreference.com/w/cpp/thread/condition_variable/notify_one
  //
  //  The effects of notify_one()/notify_all() and each of the three
  //  atomic parts of wait()/wait_for()/wait_until() (unlock+wait,
  //  wakeup, and lock) take place in a single total order that can be
  //  viewed as modification order of an atomic variable: the order is
  //  specific to this individual condition_variable. This makes it
  //  impossible for notify_one() to, for example, be delayed and
  //  unblock a thread that started waiting just after the call to
  //  notify_one() was made.
  //
  //  The notifying thread does not need to hold the lock on the same
  //  mutex as the one held by the waiting thread(s); in fact doing so
  //  is a pessimization, since the notified thread would immediately
  //  block again, waiting for the notifying thread to release the
  //  lock. However, some implementations (in particular many
  //  implementations of pthreads) recognize this situation and avoid
  //  this "hurry up and wait" scenario by transferring the waiting
  //  thread from the condition variable's queue directly to the queue
  //  of the mutex within the notify call, without waking it up.
  //
  //  Notifying while under the lock may nevertheless be necessary when
  //  precise scheduling of events is required, e.g. if the waiting
  //  thread would exit the program if the condition is satisfied,
  //  causing destruction of the notifying thread's
  //  condition_variable. A spurious wakeup after mutex unlock but
  //  before notify would result in notify called on a destroyed object.
};

#else // ! OOMMF_THREADS

class Oxs_ThreadControl {
  // Thread control trio: mutex, condition, counter
  // NB: Be certain to lock mutex, typically using std::lock_guard or
  // std::unique_lock, before accessing count or calling cond.wait();
public:
  int count;
  Oxs_ThreadControl() : count(0) {}
  ~Oxs_ThreadControl() {}
  void LockAndIncrement(int offset) {
    count += offset;
  }
  void LockAndSet(int newval) {
    count = newval;
  }
  void LockAndWaitForZero() {
    if(count!=0) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Waiting on thread control counter to go"
                " zero without parallel threads.");
    }
  }
};

#endif //  OOMMF_THREADS

#if OOMMF_THREADS
// IMPLEMENTATION FOR THREADED BUILDS //////////////////////////////////

// NOTE: Oxs_Thread is for oxsthread internal use only!  See
// Threaded/Non-Threaded Note above
class Oxs_ThreadRunObj; // forward declarations
class Oxs_ThreadTree;

// Object for representation/control of actual thread
class Oxs_Thread {
 public:
  Oxs_Thread(int thread_number);
  /// Note: (Child) thread numbers start at 1, not 0.  Thread_number 0 is
  /// reserved for the main (parent) thread.

  ~Oxs_Thread();
  void RunCmd(Oxs_ThreadControl& stop,Oxs_ThreadRunObj* runobj,void* data);

  // When start.count>0, control OS thread is paused, waiting for a
  // new command to run.  start.count==0 means thread is active.
  int IsRunning() { return (0==start.count); }

  int GetThreadNumber() const { return thread_number; }

  void GetStatus(String& results); // For debugging.

 private:
  friend void _Oxs_Thread_threadmain(Oxs_Thread*);
  friend class Oxs_ThreadRunObj;
  friend class Oxs_ThreadTree;

  int thread_number;  // Assigned by Oxs_ThreadTree static control.
  /// These numbers are 1-based, because thread_number 0 is reserved
  /// for the main (parent) thread.

  Oxs_ThreadControl start; // Unique to thread object
  Oxs_ThreadControl* stop; // Assigned by and unique to tree object.
  // The stop ptr is set in RunCmd call before child thread is notified.
  // stop should only be accessed while holding the start mutex.
  Oxs_ThreadRunObj* runobj; // Assigned by user code through Oxs_ThreadTree
  void* data;               // Ditto

  // Child threads this thread is responsible for launching in
  // multi-level launch scenario
  std::vector<Oxs_Thread*> launch_threads;

#if OC_CHILD_COPY_FPU_CONTROL_WORD
  // Buffer used to transfer floating point control data from
  // parent to child thread.  This controls properties such as
  // floating point precision and rounding.
  Oc_FpuControlData fpu_control_data;
#endif

  // Disable copy constructor and operator=() by declaring
  // without providing implementations.
  Oxs_Thread(const Oxs_Thread&);
  Oxs_Thread& operator=(const Oxs_Thread&);
};

// Manager for Oxs_Thread objects.  Call Oxs_ThreadTree to
// start and stop threads.
class Oxs_ThreadTree {
public:
  Oxs_ThreadTree() : threads_unjoined(0) {}
  ~Oxs_ThreadTree();

  static void InitThreads(int threadcount);
  static void EndThreads();

  void Launch(Oxs_ThreadRunObj& runobj,void* data); // Launch
  /// threads, one at a time.
  void Join(); // Joins all running threads.

  void LaunchRoot(Oxs_ThreadRunObj& runobj,void* data); // Runs
  /// command on root (i.e., 0) thread, and calls Join() to wait for
  /// all children to finish, with error handling.

  // Run on a range of threads.  Creates new threads as needed.  Set
  // first_thread to 0 to include main (host) thread.  Set last_thread
  // to -1 to include uppermost existing thread.  This routine is
  // self-joining.  Do not use this routine in parallel with Launch.
  // RunOnThreadRange will block until there are no running threads.
  // NB: This routine launches the threads serially directly from the
  // master thread, i.e., it does *not* use the hierarchical
  // multi-level launch/join used in LaunchTree.
  void RunOnThreadRange(int first_thread,int last_thread,
                        Oxs_ThreadRunObj& runobj,void* data);

  void LaunchTree(Oxs_ThreadRunObj& runobj,void* data); // Functionally
  /// equivalent to Launch + LaunchRoot, this single call starts all
  /// children via multi-level start/stop approach.  All threads share
  /// a reference to a single Oxs_ThreadRunObj object, so any write
  /// access into that object should be either protected by a mutex or
  /// otherwise protected against simultaneous access.  (For example,
  /// a vector could be initialized with length equal to the number of
  /// threads, and then restrict each thread to write only to the
  /// element matching its thread number.)  Also, on NUMA machines the
  /// data in the Oxs_ThreadRunObj object will live on only one memory
  /// node, so frequently accessed data inside the Oxs_ThreadRunObj
  /// object should be read once by each thread and stored locally.

  void DeleteLockerItem(String name); // Deletes local locker item
  /// named "name" in all existing threads (including thread 0).
  /// No error is raised if name does not exist in some or all
  /// of the thread lockers.

  static void GetStatus(String& results); // For debugging.  Returns
  /// string detailing "probable" status for all mutexes and thread
  /// controls that Oxs_ThreadTree knows about, in human readable
  /// format.

private:
  Oxs_ThreadControl stop; // One per tree
  int threads_unjoined;   // Count of launched, unjoined threads.
  /// Mirrors stop.count, except modified only by *this, not
  /// client threads.

  static std::mutex launch_mutex;
  static std::vector<Oxs_Thread*> threads;
  /// Note: Child thread_number's are one larger than their index into
  /// the "threads" vector, because thread_number 0 is reserved for the
  /// main (parent) thread, which is not referenced in "threads".

  static std::vector<Oxs_Thread*> root_launch_threads; // Threads
  /// directly launched by root thread (thread number 0) when
  /// multi-level launch is active.

  // Count of child threads (i.e., excluding root thread, "0")
  // launched via the multi-level LaunchTree command.  This is
  // typically Oc_GetMaxThreadCount() - 1;
  static int multi_level_thread_count;

  // Disable copy constructor and operator=() by declaring
  // without providing implementations.
  Oxs_ThreadTree(const Oxs_ThreadTree&);
  Oxs_ThreadTree& operator=(const Oxs_ThreadTree&);
};

#endif // OOMMF_THREADS

////////////////////////////////////////////////////////////////////////
// Base class that ThreadMap objects inherit from.  It is just a wrapper
// around a virtual destructor, so the map object can do its own
// clean-ups.
class Oxs_ThreadMapDataObject {
public:
  virtual ~Oxs_ThreadMapDataObject() {}
};

// Note: In practice, locker is likely to be quite small,
// so std::map (available in the original STL) could be used
// instead of std::unordered_map (new in C++11) with reasonable
// performance.  However, with one compiler inserting items into a
// thread_local std::map from a child thread generated segfaults.
// It was found that using the std::unordered_map reserve()
// member in the child mainline before first map access protected
// against this problem.
using std::unordered_map;
typedef unordered_map<String,Oxs_ThreadMapDataObject*> OXS_THREADMAP;

// Auto-initializing map object, used for thread local storage.
class Oxs_ThreadLocalMap {
private:

#if OOMMF_THREADS
  static thread_local OXS_THREADMAP locker;
#else
  static OXS_THREADMAP locker;
#endif
  OXS_THREADMAP* GetLockerPointer() { return &locker; }

  // Routine to remove all items from locker map, and call destructor on
  // each.  This routine should be called as part of problem release.
  // To help prevent abuse, this is a private function, accessible only
  // by friends --- it is not called by any Oxs_ThreadLocalMap member,
  // including in particular ~Oxs_ThreadLocalMap.
  static void DeleteLocker();

  // Friend access for DeleteLocker
#if OOMMF_THREADS
  friend void _Oxs_Thread_threadmain(Oxs_Thread*);
  friend void Oxs_ThreadTree::EndThreads();
#else
  friend class Oxs_ThreadTree;
#endif

public:
  Oxs_ThreadLocalMap() {}

  ~Oxs_ThreadLocalMap() {} // Note: Destructor does *not* delete
  /// any objects in locker map.  Instead, friend functions should
  /// call DeleteLocker during problem release.

  void AddItem(String name,Oxs_ThreadMapDataObject* obj); // Throws
  /// an exception if "name" is already in map

  Oxs_ThreadMapDataObject* GetItem(String name);  // Returns
  /// null pointer if "name" is not in map.

  Oxs_ThreadMapDataObject* UnmapItem(String name); // Removes
  /// item from map, and returns pointer to item.  Destructor
  /// for item is not called.  Throws an exception if "name"
  /// is not in map.

  void DeleteItem(String name); // Removes item from map and
  /// calls item's destructor.  Throws an exception if "name"
  /// is not in map.

};

#if OXS_THREAD_TIMER_COUNT
class Oxs_ThreadTimers : public Oxs_ThreadMapDataObject {
public:
  std::vector<Nb_WallWatch> timers;
  Oxs_ThreadTimers(int timer_count=0) {
    timers.resize(timer_count);
  }
  ~Oxs_ThreadTimers() {}
  void Reset() {
    for(std::vector<Nb_WallWatch>::iterator it=timers.begin();
        it!=timers.end(); ++it) {
      it->Reset();
    }
  }
};

// Name for local map access to timer vector:
# define OXS_THREAD_TIMER_NAME "OxsThreadTimers"

#endif // OXS_THREAD_TIMER_COUNT

// Base class that Oxs_Thread client should inherit from.
// This class is used in both threaded and non-threaded
// builds.
class Oxs_ThreadRunObj {
private:
#if OOMMF_THREADS
  friend void _Oxs_Thread_threadmain(Oxs_Thread*);
  friend void Oxs_ThreadTree::LaunchTree(Oxs_ThreadRunObj&,void*);
  friend void Oxs_ThreadTree::EndThreads();
  int multilevel;
#else
  friend class Oxs_ThreadTree;
#endif

protected:
  // Allow children of Oxs_ThreadRunObj access to thread local storage.
  Oxs_ThreadLocalMap local_locker;

public:
  Oxs_ThreadRunObj()
#if OOMMF_THREADS
    : multilevel(0)
#endif
  {}

  virtual void Cmd(int threadnumber,void* data) =0;
  virtual ~Oxs_ThreadRunObj() {}
};


#if !OOMMF_THREADS
// IMPLEMENTATION FOR NON-THREADED BUILDS //////////////////////////////

// Manager for Oxs_Thread objects.  Call Oxs_ThreadTree to
// start and stop threads.
//
// WORK NOTES: This should probably be changed to a fully static class
//             to avoid unnecessary mutex and condition
//             creation/destruction.  Also, job control schemes need
//             to know how many threads are running.  We should
//             probably migrate all clients to use the LaunchTree
//             interface, and add a parameter to control the number of
//             children launched.  This will need some consideration
//             on how this works if requested number is smaller than
//             maxthreads in the multi-level launch.  What to do if
//             the request is larger than maxthreads?  Do we launch
//             additional children, or do we flag an error?  Or
//             perhaps the multi-level launch instructions get wrapped
//             up in a class, and we support multiple sets depending
//             on the task?
class Oxs_ThreadTree {
public:
  Oxs_ThreadTree() {}
  ~Oxs_ThreadTree() {}
  static void InitThreads(int) {}
  static void EndThreads() {
    Oxs_ThreadLocalMap::DeleteLocker();
  }

  void Launch(Oxs_ThreadRunObj& runobj,void* data) {
    // Launch "thread" and block until completion.
    runobj.Cmd(0,data);
  }

  void LaunchRoot(Oxs_ThreadRunObj& runobj,void* data) {
    // In non-threaded case, same as Launch
    Launch(runobj,data);
  }

  void LaunchTree(Oxs_ThreadRunObj& runobj,void* data) {
    // In non-threaded case, same as Launch
    Launch(runobj,data);
  }

  void RunOnThreadRange(int first_thread,int /* last_thread */,
                        Oxs_ThreadRunObj& runobj,void* data) {
    if(first_thread == 0) runobj.Cmd(0,data);
  }

  void DeleteLockerItem(String name) {
    // There is only one locker in non-threaded build
    Oxs_ThreadLocalMap locker;
    if(locker.GetItem(name)) {
      locker.DeleteItem(name);
    }
  }

  static void GetStatus(String& results) { // For debugging.
    results = String("No threads.");
  }

  void Join() {}
};
#endif // !OOMMF_THREADS


////////////////////////////////////////////////////////////////////////
// THREAD BUSH AND TWIGS ///////////////////////////////////////////////
//
// This is an threading interface alternative to ThreadTree and friends.
// Unlike ThreadTree, which create semi-permanent threads that are
// reused thoughout problem lifetime, ThreadBush creates one-off threads
// for immediate use, which are terminated at the completion of one job.
//
// To use this interface, first create a child class off Oxs_ThreadTwig,
// which defines the job to be done through the virtual Task() member
// function call.  You can pack whatever data is needed into the child
// class, but only one instance of this class is used in all threads, so
// any differences between the threads has to be selected via the one
// parameter to Oxs_ThreadTwig::Task, namely "oc_thread_id".  When the
// child class is defined, then create one instance of the child class
// and one instance of the Oxs_ThreadBush class.  Call the RunAllThreads
// member of Oxs_ThreadBush with a pointer to the child instance.  Note
// that RunAllThreads is a regular member function, as opposed to a
// static member.  This is because each Oxs_ThreadBush instance has its
// own mutex and condition variables; this allows multiple
// Oxs_ThreadBush objects to be run concurrently.  Of course, in that
// case it is up to the clients to insure against deadlocks.

class Oxs_ThreadTwig {
  // This base class encapsulates a thread job for Oxs_ThreadBush
public:
  virtual void Task(int oc_thread_id) =0;
  virtual ~Oxs_ThreadTwig() {}
};

#if OOMMF_THREADS
class Oxs_ThreadBush
{
public:
  void RunAllThreads(Oxs_ThreadTwig* twig);
private:
  struct TwigBundle {
  public:
    Oxs_ThreadTwig* twig;
    Oxs_ThreadControl* thread_control;
    int oc_thread_id;
    TwigBundle() : twig(0), thread_control(0), oc_thread_id(-1) {}
    TwigBundle(Oxs_ThreadTwig* twigptr,Oxs_ThreadControl* ctrl, int id)
      : twig(twigptr), thread_control(ctrl), oc_thread_id(id) {}
  };
  friend void _Oxs_ThreadBush_main(Oxs_ThreadBush::TwigBundle&&);
};
#endif // OOMMF_THREADS

#if !OOMMF_THREADS
class Oxs_ThreadBush
{
public:
  void RunAllThreads(Oxs_ThreadTwig* twig) {
    twig->Task(0);
  }
private:
  struct TwigBundle {
  public:
    Oxs_ThreadTwig* twig;
    int oc_thread_id;
    TwigBundle() : twig(0), oc_thread_id(-1) {}
  };
};
#endif // OOMMF_THREADS


////////////////////////////////////////////////////////////////////////
// THROWAWAY THREADS ///////////////////////////////////////////////////
//
// Class for threads that are launched to run in the background and
// forgotten; one example would be a background write-to-disk task.  To
// use, create a child instance and call the Launch() method.  In the
// non-threaded version Launch() will block until the task completes.
//
// The child thread is not tied to any NUMA node.  This is preferred in
// the original use case, as we want background threads to run wherever
// they can with the least disruption to the main compute threads.  This
// should be easy to change, however, if the need arises.  See the
// Oc_NumaRunOnAnyNode() call in _Oxs_ThreadThrowaway_main().
//
// This base class does not provide any way for the launcher to
// determine when or if the child thread finishes.  If that is needed,
// embed a mutex and flag inside the child class, and have Task adjust
// the flag appropriately (through the mutex) upon completion.
//
// The base class also does not provide any protection against
// reentrancy --- use with care!
class Oxs_ThreadThrowaway {
public:
  const String name;
  Oxs_ThreadThrowaway(const char* iname)
    : name(iname), active_thread_count(0)
  {}
  virtual ~Oxs_ThreadThrowaway();
  OC_INDEX ActiveThreadCount() const;
protected:
  // It is not clear if Launch and/or Task should be protected or
  // public.  The first use of Oxs_ThreadThrowaway is
  // Oxs_Driver::BackgroundCheckpoint, and there Lauch is not intended
  // to be called from outside the ::BackgroundCheckpoint class, so as a
  // first guess we make both Launch and Task protected.
  void Launch();
  virtual void Task() = 0;
private:
#if OOMMF_THREADS
  friend void _Oxs_ThreadThrowaway_main(Oxs_ThreadThrowaway*);
  mutable std::mutex mutex;
#endif
  OC_INDEX active_thread_count;
  // Declare but don't define the following members
  Oxs_ThreadThrowaway(const Oxs_ThreadThrowaway&);
  Oxs_ThreadThrowaway& operator=(const Oxs_ThreadThrowaway&);
};

#if !OOMMF_THREADS
inline void Oxs_ThreadThrowaway::Launch()
{
  ++active_thread_count;
  Task();
  --active_thread_count;
}

inline OC_INDEX Oxs_ThreadThrowaway::ActiveThreadCount() const
{ return active_thread_count; }

inline Oxs_ThreadThrowaway::~Oxs_ThreadThrowaway() {}
#endif // !OOMMF_THREADS

////////////////////////////////////////////////////////////////////////
// Oxs_StripedArray is an array wrapper that stripes the memory across
// memory nodes.  This is only meaningful on a NUMA (non-uniform memory
// access) system.  In other cases there is effectively only one node,
// so there is no striping.  However, in all cases the start of the
// array is aligned: If the requested memory size is larger than the memory
// page size, then the array start is aligned to a page boundary.  If
// the requested memory size is smaller than the memory page size, then
// the array start is aligned to the cache line size.
//   Note: This class is defined here, inside oxsthread.h, because
// it is accessed by the Oxs_JobControl class.

// Oxs_StripedArray helper thread function
class _Oxs_StripedArray_StripeThread : public Oxs_ThreadTwig {
  // This class represents the threads that do the actual initializing
  // of the allocated memory.  It is the act of initializing the
  // memory that causes it to be assigned to a specific memory node.
  // If the memory policy is set for node-local allocation (see
  // mbind(2)), then writing to a memory page from a thread restricted
  // to a particular node will cause that page to be taken from the
  // memory store of that node.
private:
  char* const bufbase;
  const vector<OC_UINDEX> strip_positions;

   // Declare but don't define the following members
  _Oxs_StripedArray_StripeThread();
  _Oxs_StripedArray_StripeThread(const _Oxs_StripedArray_StripeThread&);
  _Oxs_StripedArray_StripeThread&
                       operator=(const _Oxs_StripedArray_StripeThread&);

public:
  _Oxs_StripedArray_StripeThread(char* in_bufbase,
                                 const vector<OC_UINDEX>& in_strip_positions)
    : bufbase(in_bufbase), strip_positions(in_strip_positions) {}

  void Task(OC_INT4m oc_thread_id) {
    // NB: The algorithm here needs to agree with the code
    // in Oxs_StripedArray<T>::GetStripPosition().
    OC_UINDEX mystart = strip_positions[oc_thread_id];
    OC_UINDEX mystop  = strip_positions[oc_thread_id+1];
    if(mystop>mystart) {
      // If full_size is small relative to OXS_STRIPE_BLOCKSIZE, then
      // the first one or few strips eat up the whole array.  In this
      // case the latter threads get nothing.
      memset(bufbase+mystart,0,mystop-mystart);
    }
  }
};

template<class T> class Oxs_StripedArray
{
private:
  char* datablock;
  T* const arr;
  OC_INDEX arr_size;
  OC_INDEX strip_count;   // Number of strips

  OC_UINDEX strip_size; // Nominal strip size, in bytes
  vector<OC_UINDEX> strip_pos; // Strip positions, in bytes.  This vector
  /// has size strip_count + 1, with indices i in strip n running
  ///     strip_pos[n] <= i < strip_pos[n+1].

  // Disable all copy operators by declaring them but not providing
  // a definition.
  Oxs_StripedArray(const Oxs_StripedArray<T>&);
  Oxs_StripedArray<T>& operator=(const Oxs_StripedArray<T>&);
public:
  void Free() {
    if(datablock) {
#if _OXS_THREAD_TRACK_MEMORY
      fprintf(stderr,"--- Oxs_StripedArray at %p deleted\n",datablock);
#endif
      // Implement "placement delete"
      while(arr_size>0) arr[--arr_size].~T(); // Explicit destructor call
      const_cast<T*&>(arr)=0;
      delete[] datablock;
      datablock=0;
    }
    const_cast<T*&>(arr) = 0;      // Safety
    arr_size = 0;
    strip_count = 0;
    strip_size = 0;
    strip_pos.clear();
  }

  Oxs_StripedArray() : datablock(0), arr(0),
                       arr_size(0), strip_count(0), strip_size(0) {}
  ~Oxs_StripedArray() { Free(); }

  void Swap(Oxs_StripedArray<T>& other) {
    char* tdb = datablock;
    datablock = other.datablock;
    other.datablock=tdb;

    T* tarr = arr;
    const_cast<T*&>(arr) = other.arr;
    const_cast<T*&>(other.arr) = tarr;

    OC_INDEX tsize = arr_size;
    arr_size = other.arr_size;
    other.arr_size = tsize;

    OC_INDEX tsc = strip_count;
    strip_count = other.strip_count;
    other.strip_count = tsc;

    OC_UINDEX ssize = strip_size;
    strip_size = other.strip_size;
    other.strip_size = ssize;

    strip_pos.swap(other.strip_pos);
  }

  void SetSize(OC_INDEX newsize);
  OC_INDEX GetSize() const { return arr_size; }

  T* GetArrBase() const { return arr; }

  inline const T& operator[](OC_INDEX index) const { return arr[index]; }
  inline T& operator[](OC_INDEX index) { return arr[index]; }

  // Get offsets for a given strip, where
  //           0 <= strip_number < strip_count
  // By default, strip_count is set to Oc_GetMaxThreadCount().
  // The returned offsets in units of sizeof(T), relative to arr.
  void GetStripPosition(OC_INT4m strip_number,
                        OC_INDEX& start_offset,
                        OC_INDEX& stop_offset) const;

  OC_INDEX GetStripCount() const { return strip_count; }
};

template<class T> void Oxs_StripedArray<T>::GetStripPosition
(OC_INT4m number,
 OC_INDEX& start_offset,
 OC_INDEX& stop_offset) const
{ // NB: The algorithm here needs to agree with the code
  // in _Oxs_StripedArray_StripeThread::Task().

  assert(0<=number && number<strip_count);

  // First, compute strip position in bytes.
  OC_UINDEX mystart = strip_pos[number];
  OC_UINDEX mystop  = strip_pos[number+1];

  // Convert units to sizeof(T).  If sizeof(T) does not divide
  // strip_size, then the convention in terms of T is that the strip
  // starts with the first full T element entirely inside the strip, and
  // ends with the last T element with at least one byte in the strip.
  // Check that this code works properly in the case mystart == mystop,
  // which happens for example if the array size is too small to give
  // all threads a piece.
  start_offset = (mystart + sizeof(T) - 1)/sizeof(T);
  OC_INDEX endpt = arr_size;
  if(number<strip_count-1) { // Last thread gets (entire) remainder
    OC_INDEX testpt = (mystop + sizeof(T) - 1)/sizeof(T);
    if(testpt<endpt) endpt = testpt;
  }
  stop_offset = endpt;
  if(start_offset>stop_offset) {
    // Safety, for unsuspecting clients.
    start_offset = stop_offset;
  }
}

template<class T> void Oxs_StripedArray<T>::SetSize
(OC_INDEX newsize)
{
  if(newsize == arr_size) return; // Nothing to do.
  Free();  // Release existing memory, if any.

  if(newsize<0) {
    char tbuf[1024];
    Oc_Snprintf(tbuf,sizeof(tbuf),
                "Oxs_StripedArray: Invalid size request to SetSize:"
                " %" OC_INDEX_MOD "d (may indicate index overflow)",
                newsize);
    OXS_THROW(Oxs_BadParameter,tbuf);
  }
  if(newsize<=0) return;

  OC_UINDEX fullsize = static_cast<OC_UINDEX>(newsize)*sizeof(T);
  /// fullsize is array size in bytes

  // Determine strip sizes
  strip_count = Oc_GetMaxThreadCount();
  OC_INT4m augment_count;
  if(strip_count>1 && fullsize>OXS_STRIPE_BLOCKSIZE) {
    strip_size = static_cast<OC_UINDEX>(newsize/strip_count) * sizeof(T);
    strip_size -= strip_size % OXS_STRIPE_BLOCKSIZE; // Reduce to page boundary

    // strip_size * strip_count is guaranteed not larger than
    // newsize*sizeof(T), but may fall short by up to
    // strip_count*(OXS_STRIPE_BLOCKSIZE+sizeof(T)). We don't want this
    // to bunch up on last strip, so instead distribute excess across
    // the strips, starting at the beginning, one page each as needed.
    OC_INDEX leftovers
      = fullsize - static_cast<OC_UINDEX>(strip_count)*strip_size;
    augment_count = static_cast<OC_INT4m>(leftovers/OXS_STRIPE_BLOCKSIZE);
  } else {
    // Put everything into the first strip.
    strip_size = fullsize;
    augment_count = 0;
  }

  // Assign strip offsets, in bytes
  strip_pos.resize(strip_count+1);
  for(OC_INT4m istrip=0;istrip<strip_count;++istrip) {
    OC_UINDEX ipos = 0;
    if(istrip < augment_count) {
      ipos = istrip*(strip_size + OXS_STRIPE_BLOCKSIZE);
    } else {
      ipos = istrip*strip_size + augment_count*OXS_STRIPE_BLOCKSIZE;
    }
    // If array is small relative to the number of strips and
    // OXS_STRIPE_BLOCKSIZE, then the first few strips can eat up the
    // entire array and the last strips end up empty.
    if(ipos>fullsize) ipos = fullsize;
    strip_pos[istrip] = ipos;
  }
  strip_pos[strip_count] = fullsize;

  // Allocate and initialize memory using "placement new".  We need to
  // allocate additional space so that the start of the working space
  // can meet the alignment restriction.  This means a need to add in
  // up to alignment-1 in the front.  If we want to insure that the
  // back bit isn't shared with some other memory structure which may
  // interfere with cache locality or memory node allocation, then we
  // need to add up to alignment-1 on the backside too.
  OC_INDEX alignment = 0;
  OC_UINDEX blocksize = newsize*sizeof(T);
  if(blocksize>=OXS_STRIPE_BLOCKSIZE) alignment = OXS_STRIPE_BLOCKSIZE;
  else                                alignment = OC_CACHE_LINESIZE;
  blocksize += 2*(alignment - 1);

  // Overflow check
  if( (OC_UINDEX_MAX/OC_UINDEX(newsize)) < sizeof(T)
      || blocksize<OC_UINDEX(newsize)*sizeof(T)) {
    char tbuf[1024];
    Oc_Snprintf(tbuf,sizeof(tbuf),
                "Oxs_StripedArray: Allocation request size too big:"
                " %" OC_INDEX_MOD "d items of size %lu",
                newsize,(unsigned long)sizeof(T));
    OXS_THROW(Oxs_BadParameter,tbuf);
  }

  datablock = new char[blocksize];
  if(!datablock) {
    // Handling for case where "new" is old broken kind that
    // doesn't throw an exception on error.
    char tbuf[1024];
    Oc_Snprintf(tbuf,sizeof(tbuf),
                "Oxs_StripedArray: Failure to allocate memory block"
                " of size %lu bytes",(unsigned long)blocksize);
    OXS_THROW(Oxs_NoMem,tbuf);
  }
#if _OXS_THREAD_TRACK_MEMORY
  fprintf(stderr,
          "+++ Oxs_StripedArray at %p allocated: %10lu x %2lu (%5.1f GB)\n",
          datablock,(unsigned long)newsize,(unsigned long)sizeof(T),
          double(blocksize)/(1024.*1024.*1024.));
#endif

  // Note: It is important to cast datablock to type OC_UINDEX rather
  // than OC_INDEX, because as a pointer (as opposed to an index) it
  // can't be assumed that datablock will evaluate as a positive integer
  // (it happens).  In that case alignoff would (probably) be negative.
  // (According to ISO/IEC 14882:2003, paragraph 5.6.4, the sign on a%b
  // for a and/or b negative is "implementation defined," although it
  // does need to be consistent with a/b, i.e., a = (a/b)*b + a%b.  In
  // other words, whether a/b rounds up or down for a or b negative is
  // also, "implementation defined.")
  OC_INDEX alignoff = reinterpret_cast<OC_UINDEX>(datablock) % alignment;
  if(alignoff>0) {
    alignoff = alignment - alignoff;
  }

  // Set memory policy (memory node selection) for datablock to
  // "first touch"
  Oc_SetMemoryPolicyFirstTouch(datablock+alignoff,
                               static_cast<OC_UINDEX>(newsize)*sizeof(T));

  // Stripe data
  _Oxs_StripedArray_StripeThread stripe(datablock+alignoff,strip_pos);
  Oxs_ThreadBush thread_bush;
  thread_bush.RunAllThreads(&stripe);

#ifdef OC_NO_PLACEMENT_NEW_ARRAY
  // Compiler does not support placment new[].  Assume that
  // the single item placement new works, and work around by
  // using that in a loop.
  const_cast<T*&>(arr) = new(datablock + alignoff)T;
  for(OC_INDEX ix=1;ix<newsize;++ix) {
    new(datablock + alignoff + ix*sizeof(T))T;
  }
#else
  const_cast<T*&>(arr)
    = new(datablock + alignoff)T[newsize];  // Placement new
#endif
  assert(reinterpret_cast<OC_UINDEX>(arr) % alignment == 0);
  /// Might want to put a fence here, to insure that arr gets set
  /// before arr_size, for any threads that are relying on arr_size to
  /// decide whether or not arr is usable.  In any event, it is
  /// probably safer to set "arr" before "arr_size".
  arr_size = newsize;
}

////////////////////////////////////////////////////////////////////////
// THREAD JOB CONTROL //////////////////////////////////////////////////

#if OOMMF_THREADS //////////////////////////////////////////////////////
// NB: When selecting the class T to be used as the template type in
// an Oxs_JobControl object, if more than one class seems natural then
// it is usually best to pick the smaller class, especially if the
// size of the smaller class divides into the size of the larger
// class(es), because some effort is made in the scheduling process to
// align jobs with cache lines or page boundaries.  If sizeof(A) =
// n*sizeof(B), with n an integer, and chunk_size*sizeof(B) is a
// multiple of the cache line or page size then chunk_size*sizeof(A) =
// chunk_size*n*sizeof(B) will be too.
template<class T> class Oxs_JobControl
{
private:
  struct JobData {
    OC_INDEX start;
    OC_INDEX stop;
    std::mutex mutex;
    int all_done;
    JobData() : start(-1), stop(-1), all_done(0) {}
  };

  JobData* threadjob;

  void ReassignJobs(OC_INT4m thread_id, OC_INDEX& start, OC_INDEX& stop);

  OC_INT4m thread_count;

  enum { BASE_PIECE_COUNT=32 };

public:
  void Free() {
    if(threadjob != 0) delete[] threadjob;
    threadjob = 0;
    thread_count = -1;
  }

  Oxs_JobControl() : threadjob(0), thread_count(-1) {}

  ~Oxs_JobControl() { Free(); }

  void Init(OC_INT4m number_of_threads,
            const Oxs_StripedArray<T>* arrblock,
            OC_INDEX record_size = 1);
  // record_size is the number of T objects per work unit.  Each job
  // assignment will be an integral multiple of record_size.

  void GetJob(OC_INT4m thread_id,
              OC_INDEX &start, OC_INDEX& stop);

  void ResetJobs() { // Mark all non-trivial jobs as not done
    for(OC_INT4m i=0;i<thread_count;++i) {
      threadjob[i].all_done = (threadjob[i].start >= threadjob[i].stop);
    }
  }
};

template<class T> void Oxs_JobControl<T>::Init
(OC_INT4m number_of_threads,
 const Oxs_StripedArray<T>* arrblock,
 OC_INDEX record_size)
{
  if(record_size < 1) record_size = 1;

  if(thread_count != number_of_threads ||
     threadjob==0) {
    // (re)Alloc.  Otherwise, reuse old space.
    Free();
    thread_count = number_of_threads;
    threadjob = new JobData[thread_count];
  }
  assert(thread_count>0);

  const OC_INDEX strip_count =  arrblock->GetStripCount();
#if OC_USE_NUMA
  if(thread_count < strip_count) {
    OXS_THROW(Oxs_BadCode,"Thread count is smaller than array split count\n");
  }
#endif

  const OC_INDEX arrsize = arrblock->GetSize();
  if(thread_count < strip_count) {
    // In the non-NUMA case, just divide up task evenly among the threads.
    // In the NUMA case, this branch shouldn't happen.
    const OC_INDEX blocksize = arrsize/thread_count;
    const OC_INDEX fudgesize = arrsize - blocksize*thread_count;
    OC_INDEX fencepole = 0;
    for(OC_INT4m i=0;i<thread_count;++i) {
      OC_INDEX adj = blocksize + (i<fudgesize ? 1 : 0);
      threadjob[i].start = fencepole;
      threadjob[i].stop = (fencepole += adj) ;
    }
  } else { // thread_count >= strip_count
    // Copy jobs from strips.
    // NB: This code assumes that
    //   threadjob[i].start <= threadjob[i].stop == threadjob[i+1].start
    OC_INT4m i=0;
    for(;i<strip_count;++i) {
      arrblock->GetStripPosition(i,threadjob[i].start,threadjob[i].stop);
    }
    for(;i<thread_count;++i) {
       // Threads in excess of strip_count get null job
      threadjob[i].start = threadjob[i].stop = arrsize;
    }
  }
  assert(threadjob[0].start == 0 && threadjob[thread_count-1].stop == arrsize);

  if(record_size>1) {
    // Tweak jobs so each assignment is an integral multiple of
    // record_size --- except for the stop point of the last job, which
    // is never changed.
    OC_INDEX fencepole = threadjob[0].start;
    for(OC_INT4m i=1;i<thread_count;++i) {
      OC_INDEX jobsize = OC_MAX(0,threadjob[i].start - fencepole);
      /// jobsize is the size of job i-1
      OC_INDEX adj = jobsize % record_size;
      if(adj != 0) {
        if(adj < record_size/2 && (i>1 || jobsize>adj)) {
          // Round down, unless i==1 and rounding down would
          // result in null job.  This special case is a sop
          // to routines that assume thread 0 will run.
          jobsize -= adj;
        } else { // Round up
          jobsize += record_size - adj;
        }
      }
      if((fencepole += jobsize)>arrsize) fencepole = arrsize;
      threadjob[i].start = fencepole;
    }
    for(OC_INT4m i=0;i<thread_count-1;++i) {
      threadjob[i].stop = threadjob[i+1].start;
    }
  }

  // Empty jobs are marked by setting .start = .stop = -1 and
  // .all_done=1
  for(OC_INT4m i=0;i<thread_count;++i) {
    if(threadjob[i].start < threadjob[i].stop) {
      threadjob[i].all_done = 0;
    } else {
      threadjob[i].start = threadjob[i].stop = -1;
      threadjob[i].all_done = 1;
    }
  }
}

template<class T> inline void Oxs_JobControl<T>::GetJob
(OC_INT4m thread_id,
 OC_INDEX& start,
 OC_INDEX& stop)
{
  JobData& job = threadjob[thread_id];

  OC_INDEX test_start,test_stop;

  int fini = 0;
  {
    std::lock_guard<std::mutex> lck(job.mutex);
    test_start = job.start;
    job.start = test_stop  = job.stop;
    fini = job.all_done;
  }

  if(test_start<test_stop) {
    // Job found in bucket for thread_id
    start = test_start;
    stop  = test_stop;
    return;
  }

  // Otherwise, all jobs for thread_id have been completed.
  // See if we can find some jobs in a different thread bucket.
  if(fini) {
    // No; all jobs assigned
    start = stop = -1;
  } else {
    ReassignJobs(thread_id,start,stop);
  }

  return;
}

template<class T> void Oxs_JobControl<T>::ReassignJobs
(OC_INT4m thread_id,
 OC_INDEX &start,
 OC_INDEX& stop)
{
  // Reassignments not currently supported
  start = stop = -1;

  JobData& job = threadjob[thread_id];
  {
    std::lock_guard<std::mutex> lck(job.mutex);
    job.all_done = 1;
  }

  return;
}

#else  // !OOMMF_THREADS ///////////////////////////////////////////////

template<class T> class Oxs_JobControl
{ // Unthreaded version
private:
  struct JobRange {
    OC_INDEX start;
    OC_INDEX stop;
    JobRange() : start(0), stop(0) {}
  };
  JobRange threadjobs;
public:
  void Free() {
    threadjobs.start = threadjobs.stop = 0;
  }

  Oxs_JobControl() {}
  ~Oxs_JobControl() { Free(); }

  void Init(OC_INT4m
#ifndef NDEBUG
            number_of_threads
#endif
            , const Oxs_StripedArray<T>* arrblock,
            OC_INDEX /* record_size */ =1) {
    assert(number_of_threads == 1);
    arrblock->GetStripPosition(0,threadjobs.start,threadjobs.stop);
  }

  void GetJob(OC_INT4m /* thread_id */,
              OC_INDEX &start, OC_INDEX& stop) {
    start = threadjobs.start;
    stop  = threadjobs.stop;
    threadjobs.start = threadjobs.stop = -1;
  }
};
#endif // OOMMF_THREADS

////////////////////////////////////////////////////////////////////////
// Simple template interface for launching threaded evaluation of a
// member function of an arbitrary class.  The member function must have
// signature
//  void ()(OC_INT4m thread_number,OC_INDEX jstart,O_INDEX jstop)
// and should cause independent processing of a chunk of array indices
// given by the range [jstart,jstop).
//   In threaded builds, the number of threads run is given by
// Oc_GetMaxThreadCount().  In non-threaded builds a single call is made
// with jstart=0 and jstop=arr.Size().
//   The call syntax is typically
//
//   Oxs_RunMemberThreaded<FOO,OC_REAL8m>(*this,&FOO::Compute_Bar,Ms);
//
// where *this is an instance of class FOO,
// Compute_Bar(OC_INT4m,OC_INDEX,OC_INDEX) is the FOO member function to
// evaluate, and Ms is a const Oxs_MeshValue<OC_REAL8m>& involved in the
// evaluation of Compute_Bar.  Note that all import data for Compute_Bar
// need to be set up separately before the Oxs_RunMemberThreaded call,
// and export data is retrieved separately afterward.  Generally *this
// member variables are used for data import and export.
//   The thread_number import can be used for thread-separate
// accumulation of results, by creating and initializing an array in
// *this of size Oc_GetMaxThreadCount() before making the
// Oxs_RunMemberThreaded call.  Then each thread uses the OC_INT4m
// thread_number to access the relevant array element.  One important
// performance consideration to keep in mind: If threads are running on
// different NUMA memory nodes, then the node holding the accumulation
// array won't be on the local node for some of the threads.  Moreover,
// if two adjacent elements, say sum[i] and sum[i+1] lie on the same
// cache line, and if threads i and i+1 are running on different
// processors then the cache will thrash if the two threads are
// simultaneously trying to write to the same cache line.  The proper
// way to do this is for each thread to create a local copy of its
// sum[i] at the top of the thread chunk function, accumulate into the
// local copy while processing through jstart to jstop, and then copy
// the final result back into sum[i] at the end.  Provided the j-chunks
// are big enough the cache thrashing should not be significant.

// There is also a template (Oxs_RunThreaded) provided primarily for
// use with lambda expressions.  This can be used inside member
// functions directly.  The call sequence looks like
//
//   Oxs_RunThreaded<ThreeVector,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
//                  (spin,
//                   [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
//                    for(OC_INDEX j=jstart;j<jstop;++j) {
//                      ...
//                    }});
//
//
// Individual thread results can be collected as follows:
//
//   const int number_of_threads = Oc_GetMaxThreadCount();
//   std::vector<OC_REAL8m> thread_max_E(number_of_threads,0.0);
//   Oc_AlignedVector<Nb_Xpfloat> thread_E_sum(number_of_threads,0.0);
//   Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
//                  (energy,
//                   [&](OC_INT4m thread_id,OC_INDEX jstart,OC_INDEX jstop) {
//                    OC_REAL8m thd_max_E = thread_max_E[thread_id]; // Local copy
//                    Nb_Xpfloat thd_E_sum = 0.0;                    // Ditto
//                    for(OC_INDEX j=jstart;j<jstop;++j) {
//                      OC_REAL8m E = energy[j] * mesh->Volume(j);
//                      if(E>thd_max_E) thd_max_E = E;
//                      thd_E_sum += E;
//                    }
//                    thread_max_E[thread_id]  = thd_max_E;
//                    thread_E_sum[thread_id] += thd_E_sum;
//   });
//   for(int i=1;i<number_of_threads;++i) {
//      if(thread_max_E[0]<thread_max_E[i]) {
//         thread_max_E[0] = thread_max_E[i];
//      }
//      thread_E_sum[0] += thread_E_sum[i];
//   }
//   OC_REAL8m max_E = thread_max_E[0];
//   OC_REAL8m E_sum = static_cast<OC_REAL8m>(thread_E_sum[0]);
//
// Here vectors are set up with one entry for each thread, and each
// thread accesses only the entry associated with it (via the thread_id
// value).  After parallel processing completes the results across the
// threads are aggregated.  Several notes:
//
//    1) Local working copies are made of the vector entries.  As
//       discussed above in the Oxs_RunMemberThreaded template notes,
//       local copies are important if write access is performed to the
//       data, because if say thread_E_sum[i] and thread_E_sum[i+1]
//       share the same cache line, but thread i and thread i+1 run on
//       different processors, then each write by one thread will
//       invalidate the cache line in the other processor, and so the
//       cache line will thrash.  The computer hardware will insure that
//       the program performs correctly, but performance will suffer; if
//       the threads are running on different processors in a
//       multi-processor machine then the performance hit is extremely
//       critical; the slow down is less pronounced on a one processor
//       box with top level cache shared across all cores, but still
//       significant.  There is still the potential for cache thrashing
//       when the working values are copied back to the vector, but that
//       should not be significant provided the j-chunks are big enough.
//
//    2) Quantities summed across a big mesh will suffer large rounding
//       error.  If the sum is evaluated piecemeal as above (one piece
//       per thread), then the particular rounding error will depend on
//       the number of threads/pieces.  Even if the accuracy loss is not
//       significant for the application at hand, the rounding
//       dependence on thread count and job scheduling makes regression
//       testing difficult.  For these reasons it is often a good idea
//       in this situation to use compensated summation such as provided
//       by the Nb_Xpfloat class to control rounding error.
//
//    3) Variable alignment outside of the base variable types (int,
//       double, pointers, etc.) is not guaranteed by the default
//       allocators in C++ standard library containers.  In particular,
//       the SSE2 __m128d type that may be used by Nb_Xpfloat requires
//       16-byte alignment, and that requirement is not insured in
//       32-bit Windows or Linux executables.  So for these or other
//       types with special alignment requirements, use the
//       Oc_AlignedVector template.  Oc_AlignedVector is a typedef for
//       std::vector with the Oc_AlignedAlloc allocator.  See
//       oommf/pkg/oc/ocalloc.h for details.

#if !OOMMF_THREADS
template <typename T> class Oxs_MeshValue; // Forward declaration
template<typename OBJCLASS,typename REFARRAYTYPE>
class Oxs_RunMemberThreaded {
public:
  Oxs_RunMemberThreaded(OBJCLASS& obj,
                  void (OBJCLASS::*eval_chunk)(OC_INT4m,OC_INDEX,OC_INDEX),
                  const Oxs_MeshValue<REFARRAYTYPE>& arr) {
    (obj.*eval_chunk)(0,0,arr.Size());
  }
private:
  Oxs_RunMemberThreaded(Oxs_RunMemberThreaded const&);
  Oxs_RunMemberThreaded& operator=(Oxs_RunMemberThreaded const&);
};

// Version for use with lambda expressions:
template<typename REFARRAYTYPE,typename FUNC>
class Oxs_RunThreaded {
public:
  Oxs_RunThreaded(const Oxs_MeshValue<REFARRAYTYPE>& arr,
                  FUNC eval_chunk) {
    eval_chunk(0,0,arr.Size());
  }
private:
  Oxs_RunThreaded(Oxs_RunThreaded const&);
  Oxs_RunThreaded& operator=(Oxs_RunThreaded const&);
};


#else // OOMMF_THREADS /////////////////////////////////////////////////
template <typename T> class Oxs_MeshValue; // Forward declaration
template<typename OBJCLASS,typename REFARRAYTYPE>
class _Oxs_RunMemberThreaded_Support  : public Oxs_ThreadRunObj {
public:
  Oxs_JobControl<REFARRAYTYPE> job_basket;

  _Oxs_RunMemberThreaded_Support
  (int number_of_threads,
   OBJCLASS& import_obj,
   void (OBJCLASS::*import_eval_chunk)(OC_INT4m,OC_INDEX,OC_INDEX),
   const Oxs_MeshValue<REFARRAYTYPE>& arr)
    : obj(import_obj), eval_chunk(import_eval_chunk)
  {
    job_basket.Init(number_of_threads,arr.GetArrayBlock());
  }

  void Cmd(int threadnumber, void* /* data */) {
    // Process array by chunks.  (Note: g++ 4.4.7 apparently doesn't
    // scope parent public member variables into child, so we include
    // full name resolution to job_basket.)
    while(1) {
      OC_INDEX jstart,jstop;
      _Oxs_RunMemberThreaded_Support<OBJCLASS,REFARRAYTYPE>
        ::job_basket.GetJob(threadnumber,jstart,jstop);
        if(jstart>=jstop) break; // No more jobs
        (obj.*eval_chunk)(threadnumber,jstart,jstop);
    }
  }

private:
  OBJCLASS& obj;
  void (OBJCLASS::*eval_chunk)(OC_INT4m,OC_INDEX,OC_INDEX);

  // Disable default copy and assignment members
  _Oxs_RunMemberThreaded_Support(_Oxs_RunMemberThreaded_Support const&);
  _Oxs_RunMemberThreaded_Support& operator=(_Oxs_RunMemberThreaded_Support const&);
};

template<typename OBJCLASS,typename REFARRAYTYPE>
class Oxs_RunMemberThreaded {
public:
  Oxs_RunMemberThreaded(OBJCLASS& obj,
                  void (OBJCLASS::*eval_chunk)(OC_INT4m,OC_INDEX,OC_INDEX),
                  const Oxs_MeshValue<REFARRAYTYPE>& arr) {
    number_of_threads = Oc_GetMaxThreadCount();
    if(number_of_threads <= 1) { // Bypass thread initialization overhead
      (obj.*eval_chunk)(0,0,arr.Size());
    } else {
      Oxs_ThreadTree threadtree;
      _Oxs_RunMemberThreaded_Support<OBJCLASS,REFARRAYTYPE>
        thread_data(number_of_threads,obj,eval_chunk,arr);
      threadtree.LaunchTree(thread_data,0);
    }
  }
private:
  int number_of_threads;
  Oxs_RunMemberThreaded(Oxs_RunMemberThreaded const&);
  Oxs_RunMemberThreaded& operator=(Oxs_RunMemberThreaded const&);
};

// Version for use with lambda expressions:
template<typename REFARRAYTYPE,typename FUNC>
class _Oxs_RunThreaded_Support  : public Oxs_ThreadRunObj {
public:
  Oxs_JobControl<REFARRAYTYPE> job_basket;

  _Oxs_RunThreaded_Support
  (int number_of_threads,
   const Oxs_MeshValue<REFARRAYTYPE>& arr,FUNC import_eval_chunk)
    : eval_chunk(import_eval_chunk)
  {
    job_basket.Init(number_of_threads,arr.GetArrayBlock());
  }

  void Cmd(int threadnumber, void* /* data */) {
    // Process array by chunks.
    while(1) {
      OC_INDEX jstart,jstop;
      _Oxs_RunThreaded_Support<REFARRAYTYPE,FUNC>
        ::job_basket.GetJob(threadnumber,jstart,jstop);
        if(jstart>=jstop) break; // No more jobs
        eval_chunk(threadnumber,jstart,jstop);
    }
  }

private:
  FUNC eval_chunk;

  // Disable default copy and assignment members
  _Oxs_RunThreaded_Support(_Oxs_RunThreaded_Support const&);
  _Oxs_RunThreaded_Support& operator=(_Oxs_RunThreaded_Support const&);
};

template<typename REFARRAYTYPE,typename FUNC>
class Oxs_RunThreaded {
public:
  Oxs_RunThreaded(const Oxs_MeshValue<REFARRAYTYPE>& arr,FUNC eval_chunk) {
    number_of_threads = Oc_GetMaxThreadCount();
    if(number_of_threads <= 1) { // Bypass thread initialization overhead
      eval_chunk(0,0,arr.Size());
    } else {
      Oxs_ThreadTree threadtree;
      _Oxs_RunThreaded_Support<REFARRAYTYPE,FUNC>
        thread_data(number_of_threads,arr,eval_chunk);
      threadtree.LaunchTree(thread_data,0);
    }
  }
private:
  int number_of_threads;
  Oxs_RunThreaded(Oxs_RunThreaded const&); // Disable
  Oxs_RunThreaded& operator=(Oxs_RunThreaded const&); // Disable
};


#endif // OOMMF_THREADS
////////////////////////////////////////////////////////////////////////


#endif // _OXS_THREAD
