/* FILE: oxsthread.cc              -*-Mode: c++-*-
 *
 * OXS thread support.
 *
 */

#include "oxsexcept.h"
#include "oxsthread.h"

/* End includes */

// In Tcl versions 8.3 and earlier, and Tcl 8.4 released prior to
// 2004-07-16 (i.e., Tcl 8.4.6 and earlier), Tcl_ExitThread
// unconditionally calls Tcl_FinalizeNotifier.  This produces a ref
// counting bug if Tcl_InitNotifier has not been called in the thread.
// This bug can cause OOMMF to hang during shutdown.  (In this
// particular instance, the problem is triggered by Oxs_ThreadThrowaway
// threads used for checkpointing.)
//   However, Tcl_CreateInterp includes a call to Tcl_InitNotifier; the
// safest workaround appears to be to create an interp in each child
// thread.  (Testing prior to Tcl 8.4 presumably didn't much consider
// the case of threads without interps.)  At some time we may have an
// active use for such interps, but for now only create thread interps
// as needed as a bug workaround.
#if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION < 4)
# define _OXS_THREAD_MAKE_CHILD_INTERPS 1
#else
# define _OXS_THREAD_MAKE_CHILD_INTERPS 0
#endif

// Thread-safe fprintf
void Oxs_ThreadPrintf(FILE* fptr,const char *format, ...) {
  static Oxs_Mutex oxs_threadprintf_mutex;
  oxs_threadprintf_mutex.Lock();

  va_list arg_ptr;
  va_start(arg_ptr,format);
  vfprintf(fptr,format,arg_ptr);
  va_end(arg_ptr);

  fflush(fptr);

  oxs_threadprintf_mutex.Unlock();
}

#if OOMMF_THREADS
Oxs_Mutex Oxs_ThreadError::mutex;
int Oxs_ThreadError::error(0);
String Oxs_ThreadError::msg;
#endif

// Notes on Tcl_MutexLock, Tcl_ConditionNotify, and Tcl_ConditionWait:
//  1) A lock should be held on a mutex before issuing a
//     notify or performing a wait.
//  2) When a wait is issued, the mutex lock is released.
//     When the thread awakens, it automatically reclaims
//     the mutex lock.
//  3) After issuing a notify, the code needs to explicitly
//     release the mutex lock.  Threads waiting on the
//     notify condition will not begin executing until
//     they can grab the mutex (as explained in 2).
//  4) After a notify is issued, there is no guarantee that
//     upon release of the mutex that the next mutex lock
//     will go to a thread waiting at a condition wait.  In
//     particular, consider this scenario:
//
//        Initially, count=0, thread 1 has a lock on mutex.
//
//        Thread 1:
//           Tcl_ConditionWait(&cond,&mutex,NULL);
//           cout << count;
//           Tcl_MutexUnlock(mutex);
//
//        Threads 2 & 3:
//           Tcl_Lock(&mutex);
//           ++count;
//           Tcl_ConditionNotify(&cond);
//           Tcl_Unlock(&mutex);
//
//     The value for count output by thread 1 may be either 1 or 2.
//     For example, suppose thread 2 succeeds in acquiring mutex first.
//     Then thread 2 increments count to 1, and notifies on cond.  Then
//     if thread 1 acquires mutex next, count will be 1.  However, it
//     is just as likely that thread 3 will acquire mutex, in which case
//     thread 3 will increment count to 2.  Assuming that no other
//     threads are waiting for mutex, then thread 1 will finally acquire
//     mutex, and the value for count at this time will be 2.
//        Similarly, even if there is only one thread waiting on a
//     particular condition, the number of times that condition is
//     notified may be greater than the number of times the waiting
//     thread is awaken.  For example, if Thread 1 above is changed to
//
//        Thread 1: (BAD BAD BAD!)
//           Tcl_ConditionWait(&cond,&mutex,NULL);
//           cout << count;
//           Tcl_ConditionWait(&cond,&mutex,NULL);
//           cout << count;
//           Tcl_MutexUnlock(mutex);
//
//     then this may hang at the second wait.  The correct way to
//     wait on multiple notifies is like so:
//
//        Thread 1: (GOOD)
//           while(count<2) { // Assume mutex held coming in
//              Tcl_ConditionWait(&cond,&mutex,NULL);
//           }
//           count << count;
//           Tcl_MutexUnlock(mutex);
//


////////////////////////////////////////////////////////////////////////
// Thread local storage.  Each thread gets its own instance.

#if OOMMF_THREADS //    --- threads version ----------------------------

void Oxs_ThreadLocalMap::DeleteLocker(Tcl_ThreadDataKey* mapkey)
{ // Destroy locker items, version for threaded builds
  void** vtmp = (void**)Tcl_GetThreadData(mapkey,sizeof(void*));
  assert(vtmp != NULL);
  if(*vtmp != NULL) {
    OXS_THREADMAP* lockerptr = static_cast<OXS_THREADMAP*>(*vtmp);
    OXS_THREADMAP::iterator it;
    for(it = lockerptr->begin();it!=lockerptr->end();++it) {
      // Delete object pointed to by Oxs_ThreadMapDataObject*
      delete it->second;
    }
    delete lockerptr;
    *vtmp = NULL; // Safety
  }
}

OXS_THREADMAP*
Oxs_ThreadLocalMap::GetLockerPointer()
{ // Returns pointer to thread-specific instance of map.
  // If map does not exist, then automatically creates it.
  // (Threaded version.)

  void** vtmp = (void**)Tcl_GetThreadData(mapkey,sizeof(void*));
  assert(vtmp != NULL);

  OXS_THREADMAP* lockerptr = NULL;

  if(*vtmp == NULL) {
    // Map doesn't exist.  Create it.
    lockerptr = new OXS_THREADMAP;
    *vtmp = lockerptr;
  } else {
    // Map already exists.  Return pointer.
    lockerptr = static_cast<OXS_THREADMAP*>(*vtmp);
  }

  return lockerptr;
}

#else // OOMMF_THREADS   --- no-threads version ------------------------

OXS_THREADMAP Oxs_ThreadLocalMap::nothreads_locker;

void Oxs_ThreadLocalMap::DeleteLocker(Tcl_ThreadDataKey*)
{ // Destroy locker items, version for non-threaded builds
  OXS_THREADMAP::iterator it;
  for(it = nothreads_locker.begin();it!=nothreads_locker.end();++it) {
    // Delete object pointed to by Oxs_ThreadMapDataObject*
    delete it->second;
  }
  nothreads_locker.clear();
}

// No-threads version of GetLockerPointer() is inlined in oxsthread.h

#endif // OOMMF_THREADS

void
Oxs_ThreadLocalMap::AddItem
(String name,
 Oxs_ThreadMapDataObject* obj)
{ // Throws an exception if "name" is already in map
  OXS_THREADMAP& locker = *GetLockerPointer();

  OXS_THREADMAP::iterator it = locker.find(name);
  if(it != locker.end()) {
    OXS_THROW(Oxs_ProgramLogicError, String("Map item \"") + name
              + String("\" already in map."));
  }
  // New value
  locker[name] = obj;
}

Oxs_ThreadMapDataObject*
Oxs_ThreadLocalMap::GetItem
(String name)
{ // Returns null pointer if "name" is not in map.
  OXS_THREADMAP& locker = *GetLockerPointer();
  OXS_THREADMAP::iterator it = locker.find(name);
  if(it == locker.end()) {
    return NULL;
  }
  return it->second;
}

Oxs_ThreadMapDataObject*
Oxs_ThreadLocalMap::UnmapItem
(String name)
{ // Removes item from map, and returns pointer to item.  Destructor
  // for item is not called.  Throws an exception if "name" is not in
  // map.
  OXS_THREADMAP& locker = *GetLockerPointer();
  OXS_THREADMAP::iterator it = locker.find(name);
  if(it == locker.end()) {
    OXS_THROW(Oxs_ProgramLogicError, String("Map item \"") + name
              + String("\" not in map; can't be unmapped."));
  }
  Oxs_ThreadMapDataObject* objptr = it->second;
  locker.erase(it);
  return objptr;
}

void
Oxs_ThreadLocalMap::DeleteItem
(String name)
{ // Removes item from map and calls item's destructor.  Throws an
  // exception if "name" is not in map.
  Oxs_ThreadMapDataObject* objptr = UnmapItem(name);
  delete objptr;
}

Tcl_ThreadDataKey Oxs_ThreadRunObj::thread_data_map;

#if OOMMF_THREADS
////////////////////////////////////////////////////////////////////////
// Mainline for actual (OS) threads
// Note: The return type differs by OS; on unix it is void, on
//       Windows it is unsigned.

#if (OC_SYSTEM_TYPE==OC_WINDOWS)
#include <signal.h>
extern "C" {
void thread_abort_handler(int)
{
  signal(SIGABRT,SIG_DFL); // Reset to default handler, to avoid recursion
  fprintf(stderr,"*** THREAD ABORT (Oxs_BadThread) ***\n");
  abort();
}}
#endif // OC_WINDOWS

Tcl_ThreadCreateProc _Oxs_Thread_threadmain;
Tcl_ThreadCreateType _Oxs_Thread_threadmain(ClientData clientdata)
{ // NB: Be careful, this routine and any routine called from inside
  //     must be re-entrant!

#if (OC_SYSTEM_TYPE==OC_WINDOWS)
  signal(SIGABRT,thread_abort_handler); // "assert" handling
  /// The above signal usage was put in place to improve assert
  /// handling with Windows Visual C++
#endif // OC_WINDOWS

  // Cast and dereference clientdata
  // NB: It is assumed that the referenced object remains valid
  // throughout the life of the thread.
  Oxs_Thread& othread = *static_cast<Oxs_Thread *>(clientdata);
  Oc_NumaRunOnNode(othread.thread_number);

  // Break out references to object data.  Copy thread_number on the
  // assumption that it is fixed throughout object lifetime.
  // The stop, runobj, and data objects may change between calls
  // (i.e., wakeups); we hold a references to these pointers to keep
  // this up-to-date.
  int thread_number         = othread.thread_number;
  Oxs_ThreadControl& start  = othread.start;
  Oxs_ThreadControl* &stop  = othread.stop;
  Oxs_ThreadRunObj* &runobj = othread.runobj;
  void* &data               = othread.data;

  std::vector<Oxs_Thread*> &launch_threads = othread.launch_threads;

#if OC_CHILD_COPY_FPU_CONTROL_WORD
  // Copy FPU control data (which includes floating point precision
  // and rounding) from parent to child.
  Oc_FpuControlData fpu_control_data = othread.fpu_control_data;
  fpu_control_data.WriteData();
  {
    Oc_AutoBuf ab;
    Oc_FpuControlData fpu_data;
    fpu_data.ReadData();
    fpu_data.GetDataString(ab);
    // Oxs_ThreadPrintf(stderr,"Thread %d FPU control data:%s\n",
    //                 thread_number,ab.GetStr());
  }
#endif // OC_CHILD_COPY_FPU_CONTROL_WORD

#if OC_USE_NUMA
  {
    Oc_AutoBuf runmask;
    Oc_NumaGetRunMask(runmask);
    Oxs_ThreadPrintf(stderr,"Thread %2d nodemask: %s\n",
                     thread_number,runmask.GetStr());
  }
#endif

#if _OXS_THREAD_MAKE_CHILD_INTERPS
  Tcl_Interp *thread_interp = Tcl_CreateInterp();
#endif

  // Notify parent Oxs_Thread that we are ready and waiting
  start.Lock();
  start.count = 1; // Ready to wait
  start.Notify();

  // Error handling
  char errbuf[256];
  String threadstr;
  Oc_Snprintf(errbuf,sizeof(errbuf),"\nException thrown in thread %d",
              thread_number);
  threadstr = errbuf;

  // Action loop
  while(1) {
    start.WaitForZero();
    if(0==runobj) break;
    try {
      if(runobj->multilevel) {
        // Wake up siblings
        std::vector<Oxs_Thread*>::iterator ti;
        for(ti=launch_threads.begin();ti!=launch_threads.end();++ti) {
          (*ti)->RunCmd(*stop,runobj,data);
        }
      }
      runobj->Cmd(thread_number,data);
    } catch (Oxs_Exception& oxserr) {
      Oxs_ThreadError::SetError(oxserr.MessageText() + threadstr);
    } catch (Oc_Exception& ocerr) {
      Oc_AutoBuf msg;
      ocerr.ConstructMessage(msg);
      msg += threadstr;
      Oxs_ThreadError::SetError(msg.GetStr());
    } catch(String& smsg) {
      Oxs_ThreadError::SetError(smsg + threadstr);
    } catch(const char* cmsg) {
      Oxs_ThreadError::SetError(String(cmsg) + threadstr);
    } catch(...) {
      Oc_Snprintf(errbuf,sizeof(errbuf),
                  "Unrecognized exception thrown in thread %d\n",
                  thread_number);
      Oxs_ThreadError::SetError(String(errbuf));
    }
    stop->Lock();
    // NB: stop->mutex must be acquired before changing stop->count.
    //     Note too that only the last thread to finish needs to
    //     notify .
    if((--stop->count) == 0) stop->Notify();
    start.count = 1; // Ready to wait.  Set this before freeing stop
    /// mutex, so that the thread signals as ready if tested from
    /// inside Oxs_ThreadTree::Launch.
    stop->Unlock();
  }
  start.count = 1;
  start.Notify();
  start.Unlock();

  // Clean-up thread-local storage
  Oxs_ThreadLocalMap::DeleteLocker(&Oxs_ThreadRunObj::thread_data_map);
#if _OXS_THREAD_MAKE_CHILD_INTERPS
  Tcl_DeleteInterp(thread_interp);
#endif
  Tcl_ExitThread(TCL_OK);
  TCL_THREAD_CREATE_RETURN;
}

////////////////////////////////////////////////////////////////////////
// Support classes for creating and accessing thread timers
#if OXS_THREAD_TIMER_COUNT
class _Oxs_ThreadTree_CreateThreadTimers : public Oxs_ThreadRunObj {
public:
  _Oxs_ThreadTree_CreateThreadTimers() {}
  void Cmd(int threadnumber, void* /* data */) {

    Oxs_ThreadMapDataObject* foo
      = local_locker.GetItem(String(OXS_THREAD_TIMER_NAME));
    if(!foo) {
      local_locker.AddItem(String(OXS_THREAD_TIMER_NAME),
                           new Oxs_ThreadTimers(OXS_THREAD_TIMER_COUNT));
    } else {
      Oxs_ThreadTimers* timers = dynamic_cast<Oxs_ThreadTimers*>(foo);
      if(!timers) {
        Oxs_ThreadError::SetError(String("Error in"
           "_Oxs_ThreadTree_CreateThreadTimers::Cmd(): downcast failed."));
        return;
      }
      timers->Reset();
    }
  }
};

class _Oxs_ThreadTree_GetThreadTimers : public Oxs_ThreadRunObj {
public: 
  _Oxs_ThreadTree_GetThreadTimers() {}
  void Cmd(int threadnumber, void* data) {
    std::vector< std::vector<Nb_WallWatch> > *foo
      = static_cast< std::vector< std::vector<Nb_WallWatch> > * >(data);
    if(static_cast<size_t>(threadnumber) >= foo->size()) {
      Oxs_ThreadError::SetError(String("Error in"
           "_Oxs_ThreadTree_GetThreadTimers::Cmd(): Short timer vector."));
      return;
    }
    std::vector<Nb_WallWatch>& timercopy = (*foo)[threadnumber];

    Oxs_ThreadTimers* timers = dynamic_cast<Oxs_ThreadTimers*>
      (local_locker.GetItem(String(OXS_THREAD_TIMER_NAME)));
    if(!timers) {
      if(threadnumber>0) {
        Oxs_ThreadError::SetError(String("Error in"
           "_Oxs_ThreadTree_GetThreadTimers::Cmd(): timers not found."));
      }
      // For thread 0 failure may just mean that the threads weren't
      // initialized yet; this can happen when the program is first
      // coming up and calls Oxs_ThreadTree::EndThreads() just for
      // safety.
      return;
    }

    timercopy.resize(OXS_THREAD_TIMER_COUNT);
    for(int i=0;i<OXS_THREAD_TIMER_COUNT;++i) {
      timercopy[i] = timers->timers[i];
    }
  }
};
#endif // OXS_THREAD_TIMER_COUNT


////////////////////////////////////////////////////////////////////////
// Oxs_Thread class, which is control object for actual thread.

Oxs_Thread::Oxs_Thread
(int thread_number_x)
  : thread_number(thread_number_x),
    stop(0), runobj(0), data(0)
{

#if OC_CHILD_COPY_FPU_CONTROL_WORD
  fpu_control_data.ReadData(); // Save (current) parent FPU state.
  { // Print FPU state of master thread
    Oc_AutoBuf ab;
    fpu_control_data.GetDataString(ab);
    // Oxs_ThreadPrintf(stderr,"Thread %d FPU control data:%s\n",
    //                 0,ab.GetStr());
  }
#endif // OC_CHILD_COPY_FPU_CONTROL_WORD

  start.Lock();
  if(TCL_OK
     != Tcl_CreateThread(&thread_id, _Oxs_Thread_threadmain, this,
                         TCL_THREAD_STACK_DEFAULT, TCL_THREAD_NOFLAGS)) {
    start.Unlock();
    OXS_THROW(Oxs_BadResourceAlloc,"Thread creation failed."
              "(Is the Tcl library thread enabled?)");
  }
  start.count = 0;
  while(0 == start.count) {
    // I don't know that this loop is really necessary, because
    // this is the only thread waiting on start.cond and only
    // one thread is notifying on start.cond, but all the
    // references say to do this, and it shouldn't hurt.
    start.Wait(0);
  }
  start.Unlock();
}

Oxs_Thread::~Oxs_Thread()
{
  start.Lock();
  stop = 0;
  runobj = 0;
  data = 0;
  start.count = 0;
  start.Notify();
  while(0 == start.count) {
    // Is this loop necessary?  Cf. notes in check_start
    // loop in Oxs_Thread constructor.
    start.Wait(0);
  }
  start.Unlock();
  // Note: start.cond and start.mutex are automatically finalized
  // inside the Oxs_ThreadControl destructor
}

void Oxs_Thread::RunCmd
(Oxs_ThreadControl& stop_x,
 Oxs_ThreadRunObj* runobj_x,
 void* data_x)
{
  // runobj == NULL is used as a message to threadmain to
  // terminate.  Don't expose this implementation detail
  // to the user.  Instead, raise an error.
  if(0 == runobj_x) {
    OXS_THROW(Oxs_BadParameter,"NULL thread runobj reference.");
  }
  start.Lock();
  stop = &stop_x;
  runobj = runobj_x;
  data = data_x;
  start.count = 0; // Run signal
  start.Notify();
  start.Unlock();
}

void Oxs_Thread::GetStatus(String& results)
{ // For debugging.
  char buf[1024];
  int start_locked,start_count;
  start.GetStatus(start_locked,start_count);
  Oc_Snprintf(buf,sizeof(buf),"start (%p) mutex %s/count %d",
              &start,(start_locked ? "locked" : "free"),start_count);
  results += String(buf);
  
  if(stop) {
    int stop_locked,stop_count;
    stop->GetStatus(stop_locked,stop_count);
    Oc_Snprintf(buf,sizeof(buf),", stop (%p) mutex %s/count %d",
                stop,(stop_locked ? "locked" : "free"),stop_count);
    results += String(buf);
  }
}

////////////////////////////////////////////////////////////////////////
// ThreadTree class, manager for Oxs_Thread objects
// Note: stop.mutex is grabbed by Oxs_ThreadTree upon the
//       first Launch command, and is held until Join
//       is called.  Each tree has its own stop control,
//       but the thread array is shared by all trees.
Oxs_Mutex Oxs_ThreadTree::launch_mutex;
std::vector<Oxs_Thread*> Oxs_ThreadTree::threads;
std::vector<Oxs_Thread*> Oxs_ThreadTree::root_launch_threads;
int Oxs_ThreadTree::multi_level_thread_count = 0;

void Oxs_ThreadTree::Launch(Oxs_ThreadRunObj& runobj,void* data)
{ // Launched threads, one at a time.
  if(0==threads_unjoined) stop.Lock();

  launch_mutex.Lock(); // Make sure no new threads get
  /// run while we're looking for a free thread.
  int free_thread=0;
  while(size_t(free_thread)<threads.size()) {
    if(!threads[free_thread]->IsRunning()) break;
    ++free_thread;
  }
  if(size_t(free_thread)==threads.size()) {
    // All threads in use; create a new thread.
    // Note: The thread_number is one larger than the index into
    // the "threads" vector, because thread_number 0 is reserved
    // for the main (parent) thread, which is not referenced in
    // the "threads" vector.  (The "threads" vector only holds
    // child threads.)
    threads.push_back(new Oxs_Thread(free_thread+1));
  }
  ++threads_unjoined;
  ++stop.count;
  threads[free_thread]->RunCmd(stop,&runobj,data);
  launch_mutex.Unlock();
}

void Oxs_ThreadTree::Join()
{ // Joins all running threads, and releases mutex_stop
  String errmsg;

  if(0==threads_unjoined) {
    if(Oxs_ThreadError::IsError(&errmsg)) OXS_THROW(Oxs_BadThread,errmsg);
    return;
  }

  // Note: If threads_unjoined>0, then we should be
  //       holding stop.mutex
  stop.WaitForZero();
  threads_unjoined = 0;
  stop.Unlock();
  if(Oxs_ThreadError::IsError(&errmsg)) OXS_THROW(Oxs_BadThread,errmsg);
}

void Oxs_ThreadTree::LaunchRoot(Oxs_ThreadRunObj& runobj,void* data)
{ // Runs command on root (i.e., 0) thread, and calls Join() to wait for
  // all children to finish, with error handling.
  try {
    runobj.Cmd(0,data);
  } catch(...) {
    if(threads_unjoined>0) {
      try {
        Join();
      } catch(...) {
        // Ignore for now.  More generally, should probably
        // catch Oxs_BadThread exceptions and append them
        // to exception from root thread.
      }
    }
    throw;
  }

  // Wait for children to finish
  if(threads_unjoined>0) Join();

  String errmsg;
  if(Oxs_ThreadError::IsError(&errmsg)) OXS_THROW(Oxs_BadThread,errmsg);
}

// "gcc (GCC) 4.1.2 20080704 (Red Hat 4.1.2-52)" complains if the
// following struct definition is moved inside
// Oxs_ThreadTree::InitThreads, so it is placed here at file scope with
// a long-winded name.
struct _Oxs_ThreadTree_NodeDistData {
  int nodenumber;
  std::vector<int> threads;
  _Oxs_ThreadTree_NodeDistData(int nn,int tn) : nodenumber(nn) {
    threads.push_back(tn);
  }
  _Oxs_ThreadTree_NodeDistData() : nodenumber(-1) {}
};

void Oxs_ThreadTree::InitThreads(int import_threadcount)
{ // Initializes structures and threads for multi-level launching and
  // joining of threads.  The number of child threads is set to
  // import_threadcount - 1.  ("-1" accounts for root thread "0" which
  // is not included in child thread lists.
#if OC_CHILD_COPY_FPU_CONTROL_WORD
  {
    Oc_AutoBuf ab;
    Oc_FpuControlData fpu_data;
    fpu_data.ReadData();
    fpu_data.GetDataString(ab);
    // Oxs_ThreadPrintf(stderr,"Thread %d FPU control data:%s\n",
    //                 0,ab.GetStr());
  }
#endif // OC_CHILD_COPY_FPU_CONTROL_WORD

  launch_mutex.Lock(); // Insures that no new threads get started
  /// somewhere else during this routine.  However, only the root
  /// thread should be starting threads, so if this is actually needed
  /// then other code is most likely badly broken.  Nonetheless, this
  /// routine should only be called once per simulation, so the lock
  /// overhead is negligible.

  try {
    // If current number of threads is smaller than requested,
    // then create new threads to match.
#if OC_USE_NUMA
    if(threads.size()==0) { // Initial initialize
      // Print node mask for master thread
      Oc_AutoBuf runmask;
      Oc_NumaGetRunMask(runmask);
      Oxs_ThreadPrintf(stderr,"Thread %2d nodemask: %s\n",
                       0,runmask.GetStr());
    }
#endif

    int it;
    for(it=static_cast<int>(threads.size());it<import_threadcount-1;++it) {
      // Note: The thread_number is one larger than the index into
      // the "threads" vector, because thread_number 0 is reserved
      // for the main (parent) thread, which is not referenced in
      // the "threads" vector.  (The "threads" vector only holds
      // child threads.)
      threads.push_back(new Oxs_Thread(it+1));
    }
    multi_level_thread_count = import_threadcount - 1;

    // Clear all launch lists
    root_launch_threads.clear();
    for(it=0;it<import_threadcount-1;++it) {
      threads[it]->launch_threads.clear();
    }

    // If NUMA is engaged, then make one thread on each node
    // a launch leader, and the rest slaves.  Otherwise, break
    // leader and slaves up so that the number of slaves for
    // each leader roughly matches the number of leaders.
    if(Oc_NumaReady()) {
      std::vector< _Oxs_ThreadTree_NodeDistData > nodedist;
      for(int ti=0;ti<import_threadcount;++ti) {
        int runnode = Oc_NumaGetRunNode(ti);
        int ni;
        for(ni=0;ni<int(nodedist.size());++ni) {
          if(runnode == nodedist[ni].nodenumber) {
            nodedist[ni].threads.push_back(ti);
            break;
          }
        }
        if(ni >= int(nodedist.size())) {
          // Unmapped node.  Add a new entry
          nodedist.push_back(_Oxs_ThreadTree_NodeDistData(runnode,ti));
        }
      }

      // First, push leader threads launched by root thread into root
      // launch list, and set up each leader launch list.  Once those
      // are all filled, go back and append non-leader threads
      // launched by root thread into the root launch list.
      // NOTE: Accesses into threads[] have a "-1" offset to account for
      //       the difference between thread id's and the thread[]
      //       indices.  Thread id 0 refers to the root thread and is is
      //       not included in threads[].
      for(int ni=1;ni<static_cast<int>(nodedist.size());++ni) {
        Oxs_Thread* tt = threads[nodedist[ni].threads[0]-1];
        root_launch_threads.push_back(tt);
        for(int si=1;si<static_cast<int>(nodedist[ni].threads.size());++si) {
          tt->launch_threads.push_back(threads[nodedist[ni].threads[si]-1]);
        }
      } 
      for(int si=1;si<static_cast<int>(nodedist[0].threads.size());++si) {
        root_launch_threads.push_back(threads[nodedist[0].threads[si]-1]);
      }
    } else {
      int leadercount = int(ceil(sqrt(double(import_threadcount))));
      int slavestep = import_threadcount/leadercount;
      int slaveextra = import_threadcount % leadercount;
      int ni = slavestep - 1; // "-1" because thread 0 not in threads[]
      for(int li=1;li<leadercount;++li) {
        int nni = ni + slavestep + (li<=slaveextra? 1 : 0);
        root_launch_threads.push_back(threads[ni]);
        for(int si=ni+1;si<nni;++si) {
          threads[ni]->launch_threads.push_back(threads[si]);
        }
        ni = nni;
      }
      for(ni=1;ni<slavestep;++ni) {
        root_launch_threads.push_back(threads[ni-1]);
        /// "-1" to account for thread 0 being outside threads[].
      }
    }
  } catch(...) {
    launch_mutex.Unlock();
    throw;
  }
  launch_mutex.Unlock();

#if OXS_THREAD_TIMER_COUNT
  { // Create thread timers in each thread
    _Oxs_ThreadTree_CreateThreadTimers foo;
    Oxs_ThreadTree temp;
    temp.RunOnThreadRange(0,-1,foo,0);
  }
#endif // OXS_THREAD_TIMER_COUNT

#if 0 // Debugging
  printf("Multi-level launch lists ---\n");
  if(Oc_NumaReady()) {
    printf("Node %2d/",Oc_NumaGetRunNode(0));
  }
  printf("Thread %2d:",0);
  for(int mli=0;mli<int(root_launch_threads.size());++mli) {
    printf(" %2d",root_launch_threads[mli]->GetThreadNumber());
  }
  printf("\n");
  for(int mlti=0;mlti<int(threads.size());++mlti) {
    if(threads[mlti]->launch_threads.size()>0) {
      if(Oc_NumaReady()) {
        printf("Node %2d/",Oc_NumaGetRunNode(mlti+1));
      }
      printf("Thread %2d:",mlti+1);
      for(int mli=0;mli<int(threads[mlti]->launch_threads.size());++mli) {
        printf(" %2d",threads[mlti]->launch_threads[mli]->GetThreadNumber());
      }
      printf("\n");
    }
  }
  printf("Thread %d runs on node %d\n",0,Oc_NumaGetRunNode(0));
  for(int it=0;it<static_cast<int>(threads.size());++it) {
    printf("Thread %d runs on node %d\n",it+1,Oc_NumaGetRunNode(it+1));
  }
#endif // debugginG
}

void Oxs_ThreadTree::LaunchTree(Oxs_ThreadRunObj& runobj,void* data)
{ // Root for multi-level thread start/stop routine.  This routine
  // runs the import command on each child in the root thread (i.e.,
  // thread 0) child launch list, and then runs the command on the
  // root thread.  After this, it waits for all children to finish,
  // with error handling.

  // QUESTION: Do we need to set/unset member variable threads_unjoined?

  if(multi_level_thread_count == 0) {
    // Special case: No child threads
    runobj.Cmd(0,data);
    return;
  }

  stop.Lock();
  stop.count = multi_level_thread_count;
  stop.Unlock();

  try {
    runobj.multilevel=1; // Inform group leaders to launch slaves
    std::vector<Oxs_Thread*>::iterator ti;
    for(ti=root_launch_threads.begin();ti!=root_launch_threads.end();++ti) {
      (*ti)->RunCmd(stop,&runobj,data);
    }
    runobj.Cmd(0,data);
  } catch(...) {
    Oxs_ThreadError::SetError("Error detected in Oxs_ThreadTree::LaunchTree");
    throw;
  }

  stop.Lock();
  stop.WaitForZero();
  stop.Unlock();

  String errmsg;
  if(Oxs_ThreadError::IsError(&errmsg)) OXS_THROW(Oxs_BadThread,errmsg);
}

void
Oxs_ThreadTree::RunOnThreadRange
(int first_thread,
 int last_thread,
 Oxs_ThreadRunObj& runobj,
 void* data)
{ // Run runobj on threads first_thread through last_thread, inclusive.
  // Threads are numbered from 0, where 0 is the main thread (i.e., the
  // thread that calls into here).  New threads will be created as
  // needed to satisfy the thread range.  If last_thread is -1, then the
  // top thread is taken to be the highest existing thread --- the
  // first_thread request is satisfied first, so that threads may be
  // created even if last_thread == -1.  To run on all existing threads
  // (including thread 0), set first_thread = 0 and last_thread = -1.
  // NOTE: If there are any threads running at entry, then this routine
  // will block until all are joined.

  String errmsg;
  if(Oxs_ThreadError::IsError(&errmsg)) OXS_THROW(Oxs_BadThread,errmsg);

  launch_mutex.Lock(); // Make sure nothing new gets launched
  /// before this routine is complete.

  // Join any running threads, and grab stop.mutex
  Join();
  stop.Lock();

  // Create new threads as needed.
  int i = static_cast<int>(threads.size());
  while(i<first_thread) {
    threads.push_back(new Oxs_Thread(++i));
  }
  while(i<last_thread) {
    threads.push_back(new Oxs_Thread(++i));
  }
  if(last_thread == -1) last_thread = static_cast<int>(threads.size());
  if(first_thread<0) first_thread = 0;

  // Run on child threads
  for(i=first_thread;i<=last_thread;++i) {
    if(i<1) continue;
    ++threads_unjoined;
    ++stop.count;
    threads[i-1]->RunCmd(stop,&runobj,data);
  }

  if(first_thread==0) {
    // Run on main thread
    try {
      runobj.Cmd(0,data);
    } catch(...) {
      if(threads_unjoined>0) {
        Join();
      } else {
        stop.Unlock();  // Make sure this gets released.
      }
      launch_mutex.Unlock();
      throw;
    }
  }

  // Wait for children to finish
  if(threads_unjoined>0) {
    Join();
  } else {
    stop.Unlock();  // Make sure this gets released.
  }

  launch_mutex.Unlock();
  if(Oxs_ThreadError::IsError(&errmsg)) OXS_THROW(Oxs_BadThread,errmsg);
}


// Support class for Oxs_ThreadTree::DeleteLockerItem
class _Oxs_ThreadTree_DLI : public Oxs_ThreadRunObj {
private:
  String item_name;
public:
  _Oxs_ThreadTree_DLI(String name) : item_name(name) {}
  void Cmd(int /* threadnumber */, void* /* data */) {
    if(local_locker.GetItem(item_name)) {
      local_locker.DeleteItem(item_name);
    }
  }
};

void Oxs_ThreadTree::DeleteLockerItem(String name)
{ // Deletes local locker item
  _Oxs_ThreadTree_DLI foo(name);
  RunOnThreadRange(0,-1,foo,0);
}


void
Oxs_ThreadTree::GetStatus(String& results)
{ // This routine is intended fo debugging purposes only.  It returns
  // string detailing "probable" status for all mutexes Oxs_ThreadTree
  // knows about, in human readable format.
  results.clear();
  results += String("launch_mutex ");
  if(launch_mutex.IsLockProbablyFree()) {
    results += String("free");
  } else {
    results += String("locked");
  }

  char buf[1024];
  for(size_t i=0;i<threads.size();++i) {
    Oc_Snprintf(buf,sizeof(buf),"\nThread %2d: ",i);
    results += String(buf);
    threads[i]->GetStatus(results);
  }

  return;
}

Oxs_ThreadTree::~Oxs_ThreadTree()
{ // Note: The mutex and condition variables in stop are automatically
  // finalized.

  // If we get here during error processing, it is possible that there
  // are unjoined threads running.  Try to kill these.
  // NB: This code is not well-tested and is probably holey.
  if(threads_unjoined>0) {
    // Note: If threads_unjoined>0, then we should be
    //       holding stop.mutex
    while(threads_unjoined>0 && stop.count>0) {
      stop.Wait(10000000);  // Wait time in microseconds
      --threads_unjoined;
    }
    threads_unjoined = 0;
    stop.Unlock();
  }
  if(stop.count>0) {
    Oxs_ThreadPrintf(stderr,
                     "WARNING: Oxs_ThreadTree destruction with %d"
                     " unjoined threads\n",stop.count);
  }
}

void Oxs_ThreadTree::EndThreads()
{ // Note: This is a static member function of Oxs_ThreadTree
#if OXS_THREAD_TIMER_COUNT
  { // Collect and dump thread timer data
    std::vector< std::vector<Nb_WallWatch> >
      timer_arrays(threads.size()+1);
    _Oxs_ThreadTree_GetThreadTimers foo;
    Oxs_ThreadTree temp;
    temp.RunOnThreadRange(0,-1,foo,&timer_arrays);

    int max_clock = 0;
    {
      // Use array for thread 0 as a proxy for all threads
      // in determining which timers to report.
      std::vector<Nb_WallWatch>& timer = timer_arrays[0];
      for(int iclock=0; iclock<int(timer.size()); ++iclock) {
        double elapsed_seconds = timer[iclock].GetTime();
        if(elapsed_seconds>0.0) max_clock = iclock + 1;
      }
    }
    if(max_clock>0) {
      const int timer_count = OC_MIN(max_clock,OXS_THREAD_TIMER_COUNT);
      std::vector<double> minval(timer_count);
      std::vector<double> maxval(timer_count);
      std::vector<double> sum(timer_count);
      Oxs_ThreadPrintf(stderr,"*** Thread timer report *******************\n");
      Oxs_ThreadPrintf(stderr," Thread|Timer");
      for(int iclock=0;iclock<timer_count-1;++iclock) {
        Oxs_ThreadPrintf(stderr,"%3d       ",iclock);
      }
      Oxs_ThreadPrintf(stderr,"%3d\n",timer_count-1);
      for(size_t it=0; it<=threads.size(); ++it) {
        Oxs_ThreadPrintf(stderr," %4d    ",it);
        std::vector<Nb_WallWatch>& timer = timer_arrays[it];
        for(int iclock=0;iclock<timer_count;++iclock) {
          double walltime = timer[iclock].GetTime();
          Oxs_ThreadPrintf(stderr," %9.3f",walltime);
          if(it==0) {
            minval[iclock] = maxval[iclock] = sum[iclock] = walltime;
          } else {
            if(walltime<minval[iclock]) minval[iclock] = walltime;
            if(walltime>maxval[iclock]) maxval[iclock] = walltime;
            sum[iclock] += walltime;
          }
        }
        Oxs_ThreadPrintf(stderr,"\n");
      }
      int iclock;
      Oxs_ThreadPrintf(stderr,"\n Min:    ");
      for(iclock=0;iclock<timer_count;++iclock) {
        Oxs_ThreadPrintf(stderr," %9.3f",minval[iclock]);
      }
      Oxs_ThreadPrintf(stderr,"\n Max:    ");
      for(iclock=0;iclock<timer_count;++iclock) {
        Oxs_ThreadPrintf(stderr," %9.3f",maxval[iclock]);
      }
      Oxs_ThreadPrintf(stderr,"\n Ave:    ");
      for(iclock=0;iclock<timer_count;++iclock) {
        Oxs_ThreadPrintf(stderr," %9.3f",sum[iclock]/(1+threads.size()));
      }
      Oxs_ThreadPrintf(stderr,"\n Max/Min:");
      for(iclock=0;iclock<timer_count;++iclock) {
        if(minval[iclock] > 0.0) {
          Oxs_ThreadPrintf(stderr," %9.2f",maxval[iclock]/minval[iclock]);
        } else {
          Oxs_ThreadPrintf(stderr,"     ---  ");
        }
      }
      Oxs_ThreadPrintf(stderr,"\n        Timer");
      for(int iclock=0;iclock<timer_count-1;++iclock) {
        Oxs_ThreadPrintf(stderr,"%3d       ",iclock);
      }
      Oxs_ThreadPrintf(stderr,"%3d",timer_count-1);
      Oxs_ThreadPrintf(stderr,
                       "\n*******************************************\n");
    }
  }
#endif // OXS_THREAD_TIMER_COUNT

  // If one of the child threads throws an exception during
  // deletion, then an exit handler may be called which ends
  // back here.  Protect against that reentrancy:
  static Oxs_Mutex handler_lock;
  static int inprocess = 0;
  handler_lock.Lock();
  if(inprocess) {
    // Routine is already active.  No show here, go home.
    handler_lock.Unlock();
    return;
  }
  inprocess=1;
  handler_lock.Unlock();

  launch_mutex.Lock();

  size_t i = threads.size();
  while(i>0) {
    // Oxs_ThreadPrintf(stderr,"Ending child thread #%d\n",i); /**/
    delete threads[--i];
  }
  threads.clear();
  launch_mutex.Unlock();
  launch_mutex.Reset();

  // Clean up multi-level launch controls
  root_launch_threads.clear();

  // Destroy local locker data in thread 0 (mainline)
  Oxs_ThreadLocalMap::DeleteLocker(&Oxs_ThreadRunObj::thread_data_map);
  Oxs_ThreadError::ClearError();

  handler_lock.Lock();
  inprocess=0;
  handler_lock.Unlock();
}

#endif // OOMMF_THREADS


////////////////////////////////////////////////////////////////////////
// THREAD BUSH AND TWIGS ///////////////////////////////////////////////

#if OOMMF_THREADS

Tcl_ThreadCreateType _Oxs_ThreadBush_main(ClientData clientdata)
{ // Helper function for Oxs_ThreadBush::RunAllThreads().  There
  // is a declaration of this function in oxsthread.h

  // Cast and dereference clientdata
  // NB: It is assumed that the referenced object remains valid
  // throughout the life of the thread.
  Oxs_ThreadBush::TwigBundle& bundle
    = *static_cast<Oxs_ThreadBush::TwigBundle *>(clientdata);

  const int oc_thread_id = bundle.oc_thread_id;
  Oxs_ThreadTwig& twig = *(bundle.twig);
  Oxs_ThreadControl& thread_control = *(bundle.thread_control);

  Oc_NumaRunOnNode(oc_thread_id);

  // Error handling
  char errbuf[256];
  String threadstr;
  Oc_Snprintf(errbuf,sizeof(errbuf),
              "\nException thrown in thread %d",
              oc_thread_id);
  threadstr = errbuf;

#if _OXS_THREAD_MAKE_CHILD_INTERPS
  Tcl_Interp *thread_interp = Tcl_CreateInterp();
#endif

  // Action loop
  try {
    twig.Task(oc_thread_id);
  } catch(Oxs_Exception& oxserr) {
    Oxs_ThreadError::SetError(oxserr.MessageText() + threadstr);
  } catch(Oc_Exception& ocerr) {
    Oc_AutoBuf msg;
    ocerr.ConstructMessage(msg);
    msg += threadstr;
    Oxs_ThreadError::SetError(msg.GetStr());
  } catch(String& smsg) {
    Oxs_ThreadError::SetError(smsg + threadstr);
  } catch(const char* cmsg) {
    Oxs_ThreadError::SetError(String(cmsg) + threadstr);
  } catch(...) {
    Oc_Snprintf(errbuf,sizeof(errbuf),
                "Unrecognized exception thrown in thread %d\n",
                oc_thread_id);
    Oxs_ThreadError::SetError(String(errbuf));
  }

  thread_control.Lock();
  --thread_control.count;
  thread_control.Notify();
  thread_control.Unlock();
  // NOTE: After this point, the bundle ptr may go invalid.

  // All done.
#if _OXS_THREAD_MAKE_CHILD_INTERPS
  Tcl_DeleteInterp(thread_interp);
#endif
  Tcl_ExitThread(TCL_OK);

  TCL_THREAD_CREATE_RETURN; // pro forma
}

void Oxs_ThreadBush::RunAllThreads(Oxs_ThreadTwig* twig)
{
  const int thread_count = Oc_GetMaxThreadCount();
  task_stop.count = thread_count;  // Do we need to Lock() here???

  TwigBundle* bundle = new TwigBundle[thread_count];

  // Launch all threads (including thread 0)
  for(int i=0;i<thread_count;++i) {
    Tcl_ThreadId tcl_id;  // Not used
    bundle[i].twig = twig;
    bundle[i].thread_control = &task_stop;
    bundle[i].oc_thread_id = i;
    if(TCL_OK
       != Tcl_CreateThread(&tcl_id, _Oxs_ThreadBush_main, &bundle[i],
                           TCL_THREAD_STACK_DEFAULT, TCL_THREAD_NOFLAGS)) {
      OXS_THROW(Oxs_BadResourceAlloc,"Thread creation failed."
                "(Is the Tcl library thread enabled?)");
    }
  }

  // Wait for all threads to finish
  task_stop.Lock();
  while(task_stop.count>0) task_stop.Wait();
  task_stop.Unlock();

  delete[] bundle; // Child threads must not access bundle array
  /// after they decrement task_stop.count.

  // Error check
  String errmsg;
  if(Oxs_ThreadError::IsError(&errmsg)) OXS_THROW(Oxs_BadThread,errmsg);

  // All done!
}

#endif // OOMMF_THREADS


////////////////////////////////////////////////////////////////////////
// THROWAWAY THREADS ///////////////////////////////////////////////////

#if OOMMF_THREADS
Tcl_ThreadCreateType _Oxs_ThreadThrowaway_main(ClientData clientdata)
{ // Helper function for Oxs_ThreadThrowaway class.  There
  // is a declaration of this function in oxsthread.h

  // Cast and dereference clientdata
  // NB: It is assumed that the referenced object remains valid
  // throughout the life of the thread.
  Oxs_ThreadThrowaway& obj
    = *static_cast<Oxs_ThreadThrowaway *>(clientdata);
  String threadstr = "\nException thrown in thread ";
  threadstr += obj.name;

#if _OXS_THREAD_MAKE_CHILD_INTERPS
  Tcl_Interp *thread_interp = Tcl_CreateInterp();
#endif

  try {
    if(Oc_NumaReady()) {
      Oc_NumaRunOnAnyNode();
    }
    obj.Task();
  } catch(Oxs_Exception& oxserr) {
    Oxs_ThreadError::SetError(oxserr.MessageText() + threadstr);
  } catch(Oc_Exception& ocerr) {
    Oc_AutoBuf msg;
    ocerr.ConstructMessage(msg);
    msg += threadstr;
    Oxs_ThreadError::SetError(msg.GetStr());
  } catch(String& smsg) {
    smsg += threadstr;
    Oxs_ThreadError::SetError(smsg);
  } catch(const char* cmsg) {
    String errstr = cmsg;
    errstr += threadstr;
    Oxs_ThreadError::SetError(errstr);
  } catch(...) {
    String errstr = "\nUnrecognized exception thrown in thread ";
    errstr += obj.name;
    Oxs_ThreadError::SetError(errstr);
  }

  // All done.
  obj.mutex.Lock();
  try {
    if(obj.active_thread_count>0) {
      --obj.active_thread_count;
    } else {
      String errstr = "\nActive thread count error in thread ";
      errstr += obj.name;
      Oxs_ThreadError::SetError(errstr);
    }
  }
  catch(...) {}
  obj.mutex.Unlock();

#if _OXS_THREAD_MAKE_CHILD_INTERPS
  Tcl_DeleteInterp(thread_interp);
#endif

  Tcl_ExitThread(TCL_OK);

  TCL_THREAD_CREATE_RETURN; // pro forma
}

void Oxs_ThreadThrowaway::Launch()
{
  Tcl_ThreadId tcl_id;  // Not used
  mutex.Lock();
  ++active_thread_count;
  mutex.Unlock();
  if(TCL_OK
     != Tcl_CreateThread(&tcl_id, _Oxs_ThreadThrowaway_main, this,
                         TCL_THREAD_STACK_DEFAULT, TCL_THREAD_NOFLAGS)) {
    mutex.Lock();
    --active_thread_count;
    mutex.Unlock();
    OXS_THROW(Oxs_BadResourceAlloc,
              "Thread creation failed in ThreadThrowaway::Launch()."
              "(Is the Tcl library thread enabled?)");
  }
}

OC_INDEX Oxs_ThreadThrowaway::ActiveThreadCount() const
{
  OC_INDEX count;
  mutex.Lock();
  count = active_thread_count;
  mutex.Unlock();
  return count;
}

Oxs_ThreadThrowaway::~Oxs_ThreadThrowaway()
{
  // Wait for all threads to finish
  const OC_INDEX sleep = 500;  // Sleep time, in milliseconds
  OC_INDEX timeout = 100; // Timeout, in seconds
  timeout *= (1000/sleep);
  mutex.Lock();
  while(timeout>0 && active_thread_count>0) {
    mutex.Unlock();
    Oc_MilliSleep(sleep);
    timeout -= sleep;
    mutex.Lock();
  }
  mutex.Unlock();
}

#endif // OOMMF_THREADS
