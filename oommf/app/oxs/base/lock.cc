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

OC_UINT4m Oxs_Lock::id_count=0;

Oxs_Lock::~Oxs_Lock()
{
  if(write_lock>0)
    OXS_THROW(Oxs_BadLock,"Delete with open write lock");
  if(read_lock>0)
    OXS_THROW(Oxs_BadLock,"Delete with open read lock(s)");
  if(dep_lock>0)
    OXS_THROW(Oxs_BadLock,"Delete with open dep lock(s)");

  obj_id=0;
}

// Assignment operator does not copy lock counts, but
// does check access restrictions.  obj_id is copied
// if there are no outstanding locks and obj_id>0.
Oxs_Lock& Oxs_Lock::operator=(const Oxs_Lock& other)
{
  if(read_lock>0)
    OXS_THROW(Oxs_BadLock,"Assignment attempt over open read lock(s)");
  if(write_lock==0) {
    // This is the no-lock situation
    if(other.obj_id!=0) {
      obj_id = other.obj_id;
    } else {
      // Take the next valid id
      if((obj_id = ++id_count)==0) {
	obj_id = ++id_count; // Safety
	// Wrap around.  For now make this fatal.
	OXS_THROW(Oxs_BadLock,"Lock count id overflow.");
      }
    }
  }
  // if write_lock>0, then presumably obj_id is already 0.
  return *this;
}

OC_BOOL Oxs_Lock::SetDepLock()
{
  ++dep_lock;
  return 1;
}

OC_BOOL Oxs_Lock::ReleaseDepLock()
{
  if(dep_lock<1) return 0;
  --dep_lock;
  return 1;
}


OC_BOOL Oxs_Lock::SetReadLock()
{
  if(write_lock) return 0;
  ++read_lock;
  return 1;
}

OC_BOOL Oxs_Lock::ReleaseReadLock()
{
  if(read_lock<1) return 0;
  --read_lock;
  return 1;
}

OC_BOOL Oxs_Lock::SetWriteLock()
{
  if(read_lock>0 || write_lock>0) return 0;
  write_lock=1;
  obj_id=0;
  return 1;
}

OC_BOOL Oxs_Lock::ReleaseWriteLock()
{
  if(write_lock!=1) return 0;
  if((obj_id = ++id_count)==0) {
    // Wrap around.  Might want to issue a warning.
    obj_id = ++id_count;
  }
  write_lock=0;
  return 1;
}
