/* FILE: key.h                 -*-Mode: c++-*-
 *
 * Oxs_Key class, intended for use with the Oxs_Lock family.
 *
 * The Oxs_Lock class is thread-safe.  However, this companion Oxs_Key
 * class is not thread-safe.  This means that while an Oxs_Lock object
 * can be shared between threads, each Oxs_Key object should be accessed
 * from only its home thread.
 *
 * Usage notes:  In general, in passing around pointers to Oxs_Lock
 * objects, one has 3 choices: pass a raw pointer, pass an
 * Oxs_ConstKey object, or pass an Oxs_Key object.  Here are some
 * general guidelines (T below represents the Oxs_Lock subclass):
 *
 *    1) const T* : Pass a 'const T* ' when the receiving routine needs
 *        immediate but not long-term read access to T.  The point here
 *        is that setting a read lock on T requires write access to T.
 *
 *    2) Oxs_ConstKey<T> :  Pass an 'Oxs_ConstKey<T>' object by value
 *        when the receiving routine needs only read access to T, but
 *        may want to cache T for future (i.e., after return) use as
 *        well.  Because Oxs_ConstKey<T> is not "const", the callee
 *        can gain a read lock, if desired.  The object should usually
 *        be passed by value (as opposed to reference) so the caller
 *        can invoke with a Oxs_Key<T> object.
 *
 *    3) Oxs_Key<T>& : Pass an 'Oxs_Key<T>' object by reference when the
 *        receiving routine needs to write to the object.  This *must*
 *        be a 'by reference' call, because to obtain a write lock there
 *        can be only 1 key pointing to the object.  The receiving
 *        routine should wrap the code with the outstanding write lock
 *        in a try block to be certain the write lock gets released.
 *
 *   4) Oxs_Key<T> : An 'Oxs_Key<T>' object may be passed by value as
 *        the return value from a function.  However, because this
 *        involves a copy constructor in the caller, the key must be
 *        set to the read state.  Some compilers may optimize away
 *        this copy, but one should not count on it.  Moreover, some
 *        compilers may be lazy about (i.e., procrastinate) destroying
 *        objects in the called function, so it is best not to put
 *        a write lock on a key returned by value.
 *
 * The key point about passing around an Oxs_Key<T> object is that the
 * receiver will be allowed by the compiler to try to place a write lock
 * on it.  If there is in existence another key pointing to the same
 * T object, then the lock attempt will fail...but this will only be
 * caught at runtime.  It is better to use the conventions outlined
 * above to more clearly specify the contract between the caller and
 * receiver, and to allow violations of that contract to be detected
 * during compilation.
 *
 * Addendum, 15-Sep-2000 (mjd): I've just added the DEP (dependency)
 *   Lock_Held state.  Keys holding dep locks can be passed around
 *   pretty much without restriction, as long as one can ensure that all
 *   such keys are deleted (or at least release their dep lock) before
 *   the lock object is deleted.  This adds some more possibilities to
 *   the above key passing discussion, which will hopefully be augmented
 *   in the future after I gain some experience using the dep lock
 *   feature.  (There is additional information about the various
 *   lock states in the lock.h header file.)
 */

#ifndef _OXS_KEY
#define _OXS_KEY

#include "oc.h"
#include "oxsexcept.h"  // Needed in template definitions

/* End includes */

template<class T> class Oxs_ConstKey;  // Forward declaration

////////////////////////////////////////////////////////////////////////
// Key class.  Class T should be a child of Oxs_Lock.
// Compare also the Oxs_ConstKey class.
template<class T> class Oxs_Key
{
  friend class Oxs_ConstKey<T>;
private:
  OC_UINT4m id;
  enum Lock_Held { DEP, READ, WRITE, INVALID } lock;
  // lock==INVALID iff id==0 and ptr==NULL
  T* ptr;
public:
  void Release(); // Release lock and ptr, and go into the INVALID state.

  void Set(T* tptr); // Releases current locks, if any, sets ptr to tptr,
  /// and if tptr!=NULL sets a DEP lock on it.

  Oxs_Key() : id(0), lock(INVALID), ptr(0) {}
  Oxs_Key(T* tptr) : id(0), ptr(0), lock(INVALID) { Set(tptr); }
  ~Oxs_Key() {
    // Note: Destructors are not suppose to throw
    try {
      Release();
    } catch (Oxs_Exception& oxserr) {
      fputs(oxserr.MessageText().c_str(),stderr);
      std::terminate();
    }
  }

  void Swap(Oxs_Key<T>& other);

  // Copy constructor.  Duplicates id and ptr info, and
  // sets a dep lock.    NB: id is set from other.id.  This
  // may not be the same as ptr->Id(), so SameState() will
  // return the same result on the copy as on the original.
  // If desired, the client may call GetDepLock() to reset the
  // key id.  (Of course, if other holds a read or write lock,
  // then other.id and ptr->Id() will be the same, so this issue
  // only arises if other holds a dep lock.)
  Oxs_Key(const Oxs_Key<T>& other);
  Oxs_Key(const Oxs_ConstKey<T>& other);

  Oxs_Key<T>& operator=(const Oxs_Key<T>& other);
  /// Same behavior as copy constructor, after releasing
  /// any locks on current object.

  OC_UINT4m ObjectId() const { return id; }

  // SameState is useful to detect changes in the lock object
  // when the key is holding a dep lock.
  OC_BOOL SameState() const {  // Returns 1 if id == ptr->Id();
    return (ptr!=NULL && id==ptr->Id());
  }
  OC_BOOL SameState(const T* other) const {
    if(other!=ptr) return 0;
    return SameState();
  }
  inline OC_BOOL SameState(const Oxs_ConstKey<T>& other) const;

  // The following routines set the lock state.  If the new state is
  // different than the old, then the old lock is first released.
  // Exceptions are thrown if the new lock isn't obtained.  Also, in all
  // cases the key id is updated to match the lock id.  (In particular,
  // if a dep lock is held, calling GetDepReference() will reset the key
  // id.  Clients using dep locks will generally use SameState() to
  // check for changes in the lock object, update internal structures as
  // necessary, and then call GetDepReference() to update the key id.)
  const T& GetDepReference();
  const T& GetReadReference();
  T& GetWriteReference();

  const T* GetPtr() const { return ptr; } // Returns pointer
  /// with no changes to lock.  A NULL indicates an unset state.
};


////////////////////////////////////////////////////////////////////////
// ConstKey class.  Class T should be a child of Oxs_Lock.
// This is just the Oxs_Key class above but without WriteLock access.
// Note that ConstKey holds a T* as opposed to a const T*.  This
// is because we need write access to T* to set even a read lock.
// Such a lock can be detected externally, so setting a read lock
// isn't really "conceptually constant" (See Item 21 of Scott Meyer's
// "Effective C++.")
// MAINTAINER NOTE: Be sure to mirror any changes between the two
// classes.
template<class T> class Oxs_ConstKey
{
  friend class Oxs_Key<T>;
private:
  OC_UINT4m id;
  T* ptr;
  enum Lock_Held { DEP, READ, INVALID } lock;
  // lock==INVALID iff id==0 and ptr==NULL
public:
  void Release(); // Release lock and ptr, and go into the INVALID state.
  void Set(T* tptr); // Releases current locks, if any, sets ptr to tptr,
  /// and if tptr!=NULL sets a DEP lock on it.

  Oxs_ConstKey() : id(0), ptr(0), lock(INVALID) {}
  Oxs_ConstKey(T* tptr)
    : id(0), ptr(0), lock(INVALID) { Set(tptr); }
  ~Oxs_ConstKey() {
    // Note: Destructors are not suppose to throw
    try {
      Release();
    } catch (Oxs_Exception& oxserr) {
      fputs(oxserr.MessageText().c_str(),stderr);
      std::terminate();
    }
  }
  void Swap(Oxs_ConstKey<T>& other);

  // Copy constructor.  Duplicates id and ptr info, and
  // sets a dep lock.    NB: id is set from other.id.  This
  // may not be the same as ptr->Id(), so SameState() will
  // return the same result on the copy as on the original.
  // If desired, the client may call GetDepLock() to reset the
  // key id.  (Of course, if other holds a read or write lock,
  // then other.id and ptr->Id() will be the same, so this issue
  // only arises if other holds a dep lock.)
  Oxs_ConstKey(const Oxs_ConstKey<T>& other);
  Oxs_ConstKey(const Oxs_Key<T>& other);

  Oxs_ConstKey<T>& operator=(const Oxs_ConstKey<T>& other);
  /// Same behavior as copy constructor, after releasing
  /// any locks on current object.

  OC_UINT4m ObjectId() const { return id; }

  // SameState is useful to detect changes in the lock object
  // when the key is holding a dep lock.
  OC_BOOL SameState() const {  // Returns 1 if id == ptr->Id();
    return (ptr!=NULL && id==ptr->Id());
  }
  OC_BOOL SameState(const T* other) const {
    if(other!=ptr) return 0;
    return SameState();
  }
  inline OC_BOOL SameState(const Oxs_ConstKey<T>& other) const;

  // The following routines set the lock state.  If the new state is
  // different than the old, then the old lock is first released.
  // Exceptions are thrown if the new lock isn't obtained.  Also, in all
  // cases the key id is updated to match the lock id.  (In particular,
  // if a dep lock is held, calling GetDepReference() will reset the key
  // id.  Clients using dep locks will generally use SameState() to
  // check for changes in the lock object, update internal structures as
  // necessary, and then call GetDepReference() to update the key id.)
  const T& GetDepReference();
  const T& GetReadReference();

  const T* GetPtr() const { return ptr; } // Returns pointer
  /// with no changes to lock.  A NULL indicates an unset state.
};


////////////////////////////////////////////////////////////////////////
// Include all definitions here.  This is required by compiler/linkers
// using what gcc calls the "Borland model," as opposed to the "Cfront
// model."  The latter is distinguished by the use of template
// repositories.  With the Cfront model, template definitions can be
// put in a file (*.cc) separate from the header file (*.h).  Putting
// the definitions in the header file along with declarations slows down
// compilation, because the definitions are re-compiled in each including
// file, but appears to be more portable.

// Oxs_Key class
template<class T>
void Oxs_Key<T>::Release()
{
  if(lock!=INVALID) {
    if(ptr==NULL) OXS_THROW(Oxs_BadPointer,
      "Oxs_Key<T>::Release(): NULL pointer");
    if(lock!=DEP && id!=ptr->Id())
      OXS_THROW(Oxs_BadLock,"Oxs_Key<T>::Release(): Bad lock id");
    if(lock==DEP)        ptr->ReleaseDepLock();
    else if(lock==READ)  ptr->ReleaseReadLock();
    else if(lock==WRITE) ptr->ReleaseWriteLock();
    lock=INVALID;
  }
  id=0;
  ptr=NULL;
}

template<class T>
void Oxs_Key<T>::Set(T* tptr)
{
  Release();
  if(tptr!=NULL) {
    tptr->SetDepLock();
    ptr=tptr;
    id=ptr->Id();
    lock=DEP;
  }
}

// Exchange lock objects
template<class T>
void Oxs_Key<T>::Swap(Oxs_Key<T>& other)
{ // Note: This works okay also if this==&other
  OC_UINT4m      tmpid=id;      id=other.id;      other.id=tmpid;
  T*         tmpptr=ptr;    ptr=other.ptr;    other.ptr=tmpptr;
  Lock_Held tmplock=lock;  lock=other.lock;  other.lock=tmplock;
}

// Copy constructor.  Duplicates id and ptr info, and
// creates a dep lock.  NB: id is set from other.id.  This
// may not be the same as ptr->Id().  The client may call
// GetDepLock() to reset id.
template<class T>
Oxs_Key<T>::Oxs_Key(const Oxs_Key<T>& other)
{
  if(other.lock==INVALID) {
    id=0;
    ptr=NULL;
    lock=INVALID;
  } else {
    id=other.id;
    ptr=other.ptr;
    ptr->SetDepLock();
    lock=DEP;
  }
}

template<class T>
Oxs_Key<T>::Oxs_Key(const Oxs_ConstKey<T>& other)
{
  if(other.lock == Oxs_ConstKey<T>::INVALID) {
    id=0;
    ptr=NULL;
    lock=INVALID;
  } else {
    id=other.id;
    ptr=other.ptr;
    ptr->SetDepLock();
    lock=DEP;
  }
}

template<class T>
Oxs_Key<T>& Oxs_Key<T>::operator=(const Oxs_Key<T>& other)
{ // Same behavior as copy constructor
  if(this==&other) return *this; // Nothing to do
  Release();
  if(other.lock!=INVALID) {
    id=other.id;
    ptr=other.ptr;
    ptr->SetDepLock();
    lock=DEP;
  }
  return *this;
}

template<class T> OC_BOOL
Oxs_Key<T>::SameState(const Oxs_ConstKey<T>& other) const
{
  if(other.ptr!=ptr) return 0;
  return SameState();
}

template<class T>
const T& Oxs_Key<T>::GetDepReference()
{
  if(ptr==NULL) OXS_THROW(Oxs_BadPointer,
     "Oxs_Key<T>::GetDepReference(): NULL pointer");
  if(lock!=DEP) {
    if(lock==READ)       ptr->ReleaseReadLock();
    else if(lock==WRITE) ptr->ReleaseWriteLock();
    ptr->SetDepLock();
    lock=DEP;
  }
  id=ptr->Id();
  return *ptr;
}

template<class T>
const T& Oxs_Key<T>::GetReadReference()
{
  if(ptr==NULL) OXS_THROW(Oxs_BadPointer,
     "Oxs_Key<T>::GetReadReference(): NULL pointer");
  if(lock!=READ) {
    if(lock==DEP)        ptr->ReleaseDepLock();
    else if(lock==WRITE) ptr->ReleaseWriteLock();
    if(!ptr->SetReadLock()) {
      char msg[512];
      Oc_Snprintf(msg,sizeof(msg),
		  "Oxs_Key<T>::GetReadReference(): "
		  "Unable to acquire read lock on object %p",
		  ptr);
      OXS_THROW(Oxs_BadLock,msg);
    }
    lock=READ;
    id=ptr->Id();
  } else {
    if(id!=ptr->Id()) {
      OXS_THROW(Oxs_BadLock,
		"Oxs_Key<T>::GetReadReference(): Bad lock id");
    }
  }
  if(id==0) {
    OXS_THROW(Oxs_BadLock,
	      "Oxs_Key<T>::GetReadReference(): Read lock id==0");
  }
  return *ptr;
}

template<class T>
T& Oxs_Key<T>::GetWriteReference()
{
  if(ptr==NULL) OXS_THROW(Oxs_BadPointer,
     "Oxs_Key<T>::GetWriteReference(): NULL pointer");
  if(lock!=WRITE) {
    if(lock==DEP)       ptr->ReleaseDepLock();
    else if(lock==READ) ptr->ReleaseReadLock();
    if(!ptr->SetWriteLock()) {
      fprintf(stderr,"ERROR: ReadLockCount=%u, WriteLockCount=%u\n",
              static_cast<unsigned int>(ptr->ReadLockCount()),
              static_cast<unsigned int>(ptr->WriteLockCount()));
      OXS_THROW(Oxs_BadLock,
           "Oxs_Key<T>::GetWriteReference(): Unable to acquire write lock");
    }
    lock=WRITE;
    id=ptr->Id(); // Id can't change while a lock is in place
  } else {
    if(id!=ptr->Id()) OXS_THROW(Oxs_BadLock,
      "Oxs_Key<T>::GetWriteReference(): Bad lock id");
  }
  if(id!=0) {
    OXS_THROW(Oxs_BadLock,
	      "Oxs_Key<T>::GetWriteReference(): Write lock id!=0");
  }
  return *ptr;
}


// Oxs_ConstKey class
template<class T>
void Oxs_ConstKey<T>::Release()
{
  if(lock!=INVALID) {
    if(ptr==NULL) OXS_THROW(Oxs_BadPointer,
      "Oxs_ConstKey<T>::Release(): NULL pointer");
    if(lock!=DEP && id!=ptr->Id())
      OXS_THROW(Oxs_BadLock,"Oxs_ConstKey<T>::Release(): Bad lock id");
    if(lock==DEP)        ptr->ReleaseDepLock();
    else if(lock==READ)  ptr->ReleaseReadLock();
    lock=INVALID;
  }
  id=0;
  ptr=NULL;
}

template<class T>
void Oxs_ConstKey<T>::Set(T* tptr)
{
  Release();
  if(tptr!=NULL) {
    tptr->SetDepLock();
    ptr=tptr;
    id=ptr->Id();
    lock=DEP;
  }
}

// Exchange lock objects
template<class T>
void Oxs_ConstKey<T>::Swap(Oxs_ConstKey<T>& other)
{ // Note: This works okay also if this==&other
  OC_UINT4m      tmpid=id;      id=other.id;      other.id=tmpid;
  T*         tmpptr=ptr;    ptr=other.ptr;    other.ptr=tmpptr;
  Lock_Held tmplock=lock;  lock=other.lock;  other.lock=tmplock;
}


// Copy constructor.  Duplicates id and ptr info, and
// creates a dep lock.  NB: id is set from other.id.  This
// may not be the same as ptr->Id().  The client may call
// GetDepLock() to reset id.
template<class T>
Oxs_ConstKey<T>::Oxs_ConstKey(const Oxs_ConstKey<T>& other)
{
  if(other.lock==INVALID) {
    id=0;
    ptr=NULL;
    lock=INVALID;
  } else {
    id=other.id;
    ptr=other.ptr;
    ptr->SetDepLock();
    lock=DEP;
  }
}

template<class T>
Oxs_ConstKey<T>::Oxs_ConstKey(const Oxs_Key<T>& other)
{
  if(other.lock == Oxs_Key<T>::INVALID) {
    id=0;
    ptr=NULL;
    lock=INVALID;
  } else {
    id=other.id;
    ptr=other.ptr;
    ptr->SetDepLock();
    lock=DEP;
  }
}

template<class T> Oxs_ConstKey<T>&
Oxs_ConstKey<T>::operator=(const Oxs_ConstKey<T>& other)
{ // Same behavior as copy constructor
  if(this==&other) return *this; // Nothing to do
  Release();
  if(other.lock!=INVALID) {
    id=other.id;
    ptr=other.ptr;
    ptr->SetDepLock();
    lock=DEP;
  }
  return *this;
}

template<class T> OC_BOOL
Oxs_ConstKey<T>::SameState(const Oxs_ConstKey<T>& other) const
{
  if(other.ptr!=ptr) return 0;
  return SameState();
}

template<class T>
const T& Oxs_ConstKey<T>::GetDepReference()
{
  if(ptr==NULL) OXS_THROW(Oxs_BadPointer,
     "Oxs_Key<T>::GetDepReference(): NULL pointer");
  if(lock!=DEP) {
    if(lock==READ) ptr->ReleaseReadLock();
    ptr->SetDepLock();
    lock=DEP;
  }
  id=ptr->Id();
  return *ptr;
}

template<class T>
const T& Oxs_ConstKey<T>::GetReadReference()
{
  if(ptr==NULL) OXS_THROW(Oxs_BadPointer,
     "Oxs_ConstKey<T>::GetReadReference(): NULL pointer");
  if(lock!=READ) {
    if(lock==DEP) ptr->ReleaseDepLock();
    if(!ptr->SetReadLock()) {
      char msg[512];
      Oc_Snprintf(msg,sizeof(msg),
		  "Oxs_ConstKey<T>::GetReadReference(): "
		  "Unable to acquire read lock on object %p",
		  ptr);
      OXS_THROW(Oxs_BadLock,msg);
    }
    lock=READ;
    id=ptr->Id();
  } else {
    if(id!=ptr->Id()) OXS_THROW(Oxs_BadLock,
      "Oxs_ConstKey<T>::GetReadReference(): Bad lock id");
  }
  if(id==0) {
    OXS_THROW(Oxs_BadLock,
	      "Oxs_ConstKey<T>::GetReadReference(): Read lock id==0");
  }
  return *ptr;
}

#endif // _OXS_KEY
