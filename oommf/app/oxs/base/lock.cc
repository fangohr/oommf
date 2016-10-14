/* FILE: lock.cc                 -*-Mode: c++-*-
 *
 * Data structure locking class
 *
 */

#include "oc.h"
#include "lock.h"
#include "oxsexcept.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported

/* End includes */

Oxs_Mutex Oxs_Lock::class_mutex;
OC_UINT4m Oxs_Lock::id_count=0;

Oxs_Lock::~Oxs_Lock()
{
  // Note: Destructors aren't suppose to throw
  const char* errmsg = 0;
  instance_mutex.Lock();
  if(write_lock>0) {
    errmsg = "Oxs_BadLock: Delete with open write lock";
  } else if(read_lock>0) {
    errmsg = "Oxs_BadLock: Delete with open read lock(s)";
  } else if(dep_lock>0) {
    errmsg = "Oxs_BadLock: Delete with open dep lock(s)";
  }
  instance_mutex.Unlock();
  if(errmsg != 0) {
    fputs(errmsg,stderr);
    std::terminate();
  }
  obj_id=0;
}

OC_BOOL Oxs_Lock::SetDepLock()
{
  instance_mutex.Lock();
  ++dep_lock;
  instance_mutex.Unlock();
  return 1;
}

OC_BOOL Oxs_Lock::ReleaseDepLock()
{
  OC_BOOL rtncode = 0;
  instance_mutex.Lock();
  if(dep_lock>0) {
    --dep_lock;
    rtncode=1;
  }
  instance_mutex.Unlock();
  return rtncode;
}


OC_BOOL Oxs_Lock::SetReadLock()
{
  OC_BOOL rtncode = 0;
  instance_mutex.Lock();
  if(!write_lock) {
    ++read_lock;
    rtncode=1;
  }
  instance_mutex.Unlock();
  return rtncode;
}

OC_BOOL Oxs_Lock::ReleaseReadLock()
{
  OC_BOOL rtncode = 0;
  instance_mutex.Lock();
  if(read_lock>0) { // read_lock == 0 is probably an error
    --read_lock;
    rtncode=1;
  }
  instance_mutex.Unlock();
  return rtncode;
}

OC_BOOL Oxs_Lock::SetWriteLock()
{
  OC_BOOL rtncode = 0;
  instance_mutex.Lock();
  if(!read_lock && !write_lock) {
    write_lock=1;
    obj_id=0;
    rtncode=1;
  }
  instance_mutex.Unlock();
  return rtncode;
}

OC_BOOL Oxs_Lock::ReleaseWriteLock()
{
  OC_BOOL rtncode = 0;
  instance_mutex.Lock();
  try {
    if(write_lock) {
      obj_id=GetNextFreeId(); // Uses class_mutex and may throw
      write_lock=0;
      rtncode=1;
    }
  } catch(...) {
    instance_mutex.Unlock();
    throw;
  }
  instance_mutex.Unlock();
  return rtncode;
}
