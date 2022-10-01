/* FILE: lock.cc                 -*-Mode: c++-*-
 *
 * Data structure locking class
 *
 */

#include <cstdio>

#include <exception>

#include "oc.h"
#include "lock.h"
#include "oxsexcept.h"

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported

/* End includes */

std::atomic<OC_UINT4> Oxs_Lock::id_count(0);

Oxs_Lock::~Oxs_Lock()
{
  // Note: Destructors aren't suppose to throw
  const char* errmsg = 0;
  LOCK_DATA_TYPE test = lock_data.load();
  if(test & WRITE_MASK) {
    errmsg = "Oxs_BadLock: Delete with open write lock";
  } else if(test & READ_MASK) {
    errmsg = "Oxs_BadLock: Delete with open read lock(s)";
  } else if(dep_lock.load()>0) {
    errmsg = "Oxs_BadLock: Delete with open dep lock(s)";
  }
  if(errmsg != 0) {
    fputs(errmsg,stderr);
    std::terminate();
  }
  lock_data.store(0);
}

void Oxs_Lock::ReleaseDepLock()
{
  OC_UINT4m testval = dep_lock.load();
  do {
    if(testval==0) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Dep lock release request with dep_lock==0");
    }
  } while(!dep_lock.compare_exchange_weak(testval,testval-1));
}

OC_BOOL Oxs_Lock::SetReadLock()
{
  LOCK_DATA_TYPE newval;
  LOCK_DATA_TYPE testval = lock_data.load();
  do {
    if(testval & WRITE_MASK) return 0;  // Fail; write lock held
    newval = (((testval >> READ_SHIFT) + 1 ) << READ_SHIFT)
             | (testval & ID_MASK);
  } while(!lock_data.compare_exchange_weak(testval,newval));
  return 1;
}

void Oxs_Lock::ReleaseReadLock()
{
  LOCK_DATA_TYPE newval;
  LOCK_DATA_TYPE testval = lock_data.load();
  do {
    if((testval & READ_MASK) == 0) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Read lock release request with read_lock==0");
    }
    newval = (((testval >> READ_SHIFT) - 1 ) << READ_SHIFT)
             | (testval & ID_MASK);
  } while(!lock_data.compare_exchange_weak(testval,newval));
}

OC_BOOL Oxs_Lock::SetWriteLock()
{
  LOCK_DATA_TYPE newval = WRITE_MASK; // obj_id is zero when write lock is held
  LOCK_DATA_TYPE testval = lock_data.load();
  do {
    if(testval & (WRITE_MASK | READ_MASK)) {
      return 0;  // Fail; either read or write lock already held
    }
  } while(!lock_data.compare_exchange_weak(testval,newval));
  return 1;
}

void Oxs_Lock::ReleaseWriteLock()
{
  LOCK_DATA_TYPE newval = GetNextFreeId(); // No write or read locks on success
  LOCK_DATA_TYPE testval = lock_data.load();
  do {
    if((testval & WRITE_MASK) == 0) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Write lock release request with write_lock==0");
    }
  } while(!lock_data.compare_exchange_weak(testval,newval));
}
