/* FILE: lock.h                 -*-Mode: c++-*-
 *
 * Data structure locking class, for insuring data integrity.
 *
 * The Oxs_Lock class is thread safe except for the constructor and
 * destructor.  In particular, be sure only one thread has access
 * to an Oxs_Lock object at destruction time.
 *
 * Note that the companion Oxs_Key class is not thread-safe.  This
 * means that a Oxs_Lock object can be shared between threads, but
 * each Oxs_Key object should be either accessed from only its home
 * thread, or else multi-thread access needs to be carefully
 * controlled to prevent race conditions.
 *
 * Background: The Oxs_Lock class offers three types of locks: read,
 * write and dep (dependency).  A read lock prevents a write lock or
 * object deletion.  There can be multiple read locks held at one
 * time.  A write lock prevents read locks, object deletion, and
 * additional write locks --- only one write lock may be in force at
 * one time.  A dep (dependency) lock prevents object deletion, but
 * otherwise has no effect.  A dep lock is typically set when a client
 * is caching data that is tied to the object data, but can be
 * regenerated if necessary.  Before the cached data is used, the
 * client checks the lock Id to see if the lock object has changed.
 * If not, then the cached data is still valid.  Otherwise, the client
 * must regenerate the cached data.
 *
 * Access to the lock class is generally performed through the Oxs_Key
 * class.  This insures proper semantics and guarantees lock removal
 * when the Oxs_Key is destroyed.
 *
 * This class was originally written for single threaded code, and
 * then later only accessed from the main thread.  However, later the
 * problem checkpoint write code was transitioned into a separate
 * (throwaway) thread: The main thread puts a read lock on a
 * Oxs_SimState to guarantee that the state is not destroyed before it
 * is checkpointed to disk, and then launches a thread to write the
 * state.  The child thread removes the read lock once the state is
 * saved.  In this scenario it is important that the lock decrement in
 * the child thread occurs atomically wrt lock count adjustments in
 * the main thread.  It was found that this was not guaranteed on
 * Linux, and occasionally a read lock decrement would be "lost,"
 * resulting in an error at problem termination when the involved
 * Oxs_SimState could not be destroyed.  The regression test
 * chkpt_thread was created to check for this condition.
 *
 * At that time (July 2016) Oxs_Lock was modified with mutexes to make
 * it thread safe.  Since the vast majority of Oxs_Lock access is from
 * the main thread, and lock contention very rarely occurs with the
 * checkpoint thread, the mutex-based version performs quite well.
 * Nonetheless, on the C++11-required OOMMF development branch a
 * lock-free variant was coded.  On very small problems with
 * checkpoints every step, the lock-free version is perhaps 1% faster.
 *
 * The thread-safe coding appears to be satisfactory for read and dep
 * locks.  Write locks are somewhat problematic, as only a single
 * write lock can be held at one time, so one has to be careful to
 * coordinate write lock requests.  Similarly, one needs to insure
 * that no thread attempts to acquire a lock of any type during
 * Oxs_Lock object deletion, because there is no mutex lock protecting
 * ~Oxs_Lock in the lock-free variant.  This seems justified by the
 * observation that lock acquisition attempts *after* the destruction
 * of an Oxs_Lock are improper, and guarding against the latter should
 * make possible a guard against the former.
 *
 */

#ifndef _OXS_LOCK
#define _OXS_LOCK

#include <atomic>

#include "oc.h"

#include "oxsthread.h"

/* End includes */

// Lock class
class Oxs_Lock {
private:

  // Note on lock id's: 4 bytes suffices to supply a unique id every
  // millisecond for a month.  If this is not enough id's, change the
  // id_count type to OC_UINT8m, have GetNextFreeId return Oc_UINT8m,
  // and change the lock_data layout to expand the id field at the
  // expense ofthe read_lock field.
  static std::atomic<OC_UINT4> id_count;  // Top id used.

  static OC_UINT4m GetNextFreeId() {
    OC_UINT4 next_id = ++id_count;
    if(next_id == 0) {
      OXS_THROW(Oxs_BadLock,"Lock count id overflow.");
    }
    return next_id;
  }

  // Stack obj_id, read_lock (count) and write_lock (flag) into a single
  // 8-byte unsigned int, to allow lock-free atomic access.  (Note: We
  // use a single int with masks rather than a struct because early
  // compiler support for atomic objects outside integral types is
  // uneven.)  Dep locks are independent of the others and so can be
  // handled separately.
  //
  // lock_data layout:
  //  Bits  0-31: Object id.  0 is invalid id that used
  //              when a write lock is outstanding.
  //  Bits 32-62: Number of read locks
  //  Bit     63: Write lock flag
  //
  // Locking semantics: You cannot obtain a read_lock if a write_lock is
  // in place.  You cannot obtain a write_lock if a write_lock or one or
  // more read locks are in place.  A dep lock may always be set or
  // removed, regardless of the read/write lock state.

  typedef OC_UINT8 LOCK_DATA_TYPE;
  std::atomic<LOCK_DATA_TYPE> lock_data;
  const LOCK_DATA_TYPE WRITE_MASK = 0x8000000000000000;
  const LOCK_DATA_TYPE  READ_MASK = 0x7FFFFFFF00000000;
  const LOCK_DATA_TYPE    ID_MASK = 0x00000000FFFFFFFF;
  const unsigned int READ_SHIFT  = 32;
  const unsigned int WRITE_SHIFT = 63;

  // A dep (dependency) lock prevents object deletion, but otherwise
  // has no effect.  A dep lock is typically set when a client is
  // caching data that depends on but can be regenerated from the lock
  // object.  Before the cached data is used, the client checks the lock
  // Id to see if the lock object has changed.  If not, then the cached
  // data is still valid.  Otherwise, the client must regenerate the
  // cached data.
  std::atomic<OC_UINT4m> dep_lock;    // Number of dep locks

  // Disable copy constructor and assignment operator.
  Oxs_Lock(const Oxs_Lock& other);
  Oxs_Lock& operator=(const Oxs_Lock&);
  
public:
  Oxs_Lock() : lock_data(GetNextFreeId()), dep_lock(0) {}
  virtual ~Oxs_Lock();

  OC_UINT4m Id() const {
    LOCK_DATA_TYPE tmp = lock_data.load();
    return static_cast<OC_UINT4m>(tmp & ID_MASK);
  }
  OC_UINT4m ReadLockCount() const {
    LOCK_DATA_TYPE tmp = lock_data.load();
    return static_cast<OC_UINT4m>((tmp & READ_MASK) >> READ_SHIFT);
  }
  OC_UINT4m WriteLockCount() const {
    LOCK_DATA_TYPE tmp = lock_data.load();
    return static_cast<OC_UINT4m>(tmp >> WRITE_SHIFT);
  }

  OC_UINT4m DepLockCount() const {
    return dep_lock.load();
  }

  // SetDepLock always succeeds.  ReleaseDepLock only fails
  // if a DepLock has been released more times that set, in
  // which case ReleaseDepLock throws.
  //
  // SetReadLock fails if a write lock is currently in place,
  // and SetWriteLock fails if any read or write locks are
  // currently set.  Under those conditions SetReadLock and
  // SetWriteLock return 0; otherwise they return 1 (success).
  //
  // Failure of ReleaseReadLock or ReleaseWriteLock can only
  // occur if the lock is not properly acquired.  These routines
  // throw on error.
  //
  // It is important that any code acquiring a lock release it when
  // done...even if an exception is raised.  Release can be managed
  // automatically by using the Oxs_Key class.  
  void SetDepLock() { ++dep_lock; }
  void ReleaseDepLock();
  OC_BOOL SetReadLock();
  void ReleaseReadLock();
  OC_BOOL SetWriteLock();
  void ReleaseWriteLock();
};

#endif // _OXS_LOCK
