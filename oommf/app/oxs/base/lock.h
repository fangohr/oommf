/* FILE: lock.h                 -*-Mode: c++-*-
 *
 * Data structure locking class.  For insuring data integrity.  The
 * Oxs_Lock class is thread-safe.  However, the companion Oxs_Key class
 * is not thread-safe.  This means that a Oxs_Lock object can be shared
 * between threads, but each Oxs_Key object should be accessed from only
 * its home thread.
 */

#ifndef _OXS_LOCK
#define _OXS_LOCK

#include "oc.h"

#include "oxsthread.h"

/* End includes */

// Lock class
class Oxs_Lock {
private:
  static Oxs_Mutex class_mutex;
  static OC_UINT4m id_count;  // Top id used.

  static OC_UINT4m GetNextFreeId() {
    class_mutex.Lock();
    OC_UINT4m next_id = ++id_count;
    class_mutex.Unlock();
    if(next_id == 0) {
      OXS_THROW(Oxs_BadLock,"Lock count id overflow.");
    }
    return next_id;
  }

  mutable Oxs_Mutex instance_mutex;
  OC_UINT4m obj_id; // Object id.  0 is invalid id that is used when a
  /// write lock is outstanding.

  // A dep (dependency) lock prevents object deletion, but otherwise
  // has no effect.  A dep lock is typically set when a client is
  // caching data that depends on but can be regenerated from the lock
  // object.  Before the cached data is used, the client checks the lock
  // Id to see if the lock object has changed.  If not, then the cached
  // data is still valid.  Otherwise, the client must regenerate the
  // cached data.
  OC_UINT4m dep_lock;    // Number of dep locks

  // The following implement normal locking semantics.  You cannot
  // obtain a read_lock if a write_lock is in place.  You cannot obtain
  // a write_lock if a write_lock or one or more read locks are in
  // place.  A dep lock may always be set or removed, regardless of the
  // read/write lock state.
  OC_UINT4m read_lock;   // Number of read locks
  OC_UINT4m write_lock;  // Number of write locks on this object; 0 or 1

  // Disable copy constructor and assignment operator.
  Oxs_Lock(const Oxs_Lock& other);
  Oxs_Lock& operator=(const Oxs_Lock&);
  
public:
  Oxs_Lock()
    : obj_id(GetNextFreeId()), dep_lock(0), read_lock(0), write_lock(0) {}
  virtual ~Oxs_Lock();

  OC_UINT4m Id() const {
    OC_UINT4m cnt;
    instance_mutex.Lock();
    cnt = obj_id;
    instance_mutex.Unlock();
    return cnt;
  }

  OC_UINT4m DepLockCount() const {
    OC_UINT4m cnt;
    instance_mutex.Lock();
    cnt = dep_lock;
    instance_mutex.Unlock();
    return cnt;
  }
  OC_UINT4m ReadLockCount() const {
    OC_UINT4m cnt;
    instance_mutex.Lock();
    cnt = read_lock;
    instance_mutex.Unlock();
    return cnt;
  }
  OC_UINT4m WriteLockCount() const {
    OC_UINT4m cnt;
    instance_mutex.Lock();
    cnt = write_lock;
    instance_mutex.Unlock();
    return cnt;
  }

  // The following locking functions return 1 on success, 0 on failure.
  // It is important that any code acquiring a lock release it when
  // done...even if an exception is raised.  Release can be managed
  // automatically by using the Oxs_Key class.  
  OC_BOOL SetDepLock();
  OC_BOOL ReleaseDepLock();
  OC_BOOL SetReadLock();
  OC_BOOL ReleaseReadLock();
  OC_BOOL SetWriteLock();
  OC_BOOL ReleaseWriteLock();
};

#endif // _OXS_LOCK
