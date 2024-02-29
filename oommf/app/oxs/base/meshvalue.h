/* FILE: meshvalue.h                 -*-Mode: c++-*-
 *
 * Oxs_MeshValue templated class, intended for use with the
 * Oxs_Mesh family.
 *
 * Note 1: To maintain a clean dependency hierarchy and prevent cyclic
 *   dependencies, this class does not maintain a lock on the mesh
 *   used to construct it.  Only the size of the mesh is retained,
 *   although a client may call Oxs_MeshValue<T>::CheckMesh() to
 *   check that the size is compatible with a given mesh.  If
 *   Oxs_MeshValue becomes more complex in the future, it may be
 *   necessary to add a pointer to the mesh that construction was
 *   based upon, in which case it will be the responsibility of the
 *   holder and/or user of the Oxs_MeshValue object to insure the
 *   correctness of that pointer.
 *
 * Note 2: Many of the class member functions are threaded using the Oxs
 *   thread pool and the Oxs_RunThreaded template with lambda
 *   expressions.  Those members and the Oxs_MeshValue allocation and
 *   resizing operations should only be accessed from the Oxs main
 *   thread.  Child threads should only access the array indexing
 *   operator[] and class const members.
 */

#ifndef _OXS_MESHVALUE
#define _OXS_MESHVALUE

#include <cstring>  // memcpy
#include <memory>   // std::shared_ptr
#include <vector>

#include "vf.h"

#include "oxsexcept.h"
#include "oxsthread.h"
#include "threevector.h"
#include "util.h"

/* End includes */

#ifdef NDEBUG
# define OxsMeshValueReadOnlyCheck
#else // NDEBUG not defined
# define OxsMeshValueReadOnlyCheck                          \
    if(read_only_lock) {                                    \
      OXS_THROW(Oxs_ProgramLogicError,                      \
                String("Write access attempt to read-only"  \
                       " Oxs_MeshValue object at address ") \
                  + Oc_MakeString((void*)this));            \
    }
#endif // NDEBUG


///////////////////////////////////////////////////////////////////////
/// OVF 2.0 output routines

// Forward declarations
template<class T> class Oxs_MeshValue;

// For Oxs_MeshValue<OC_REAL8m> output
void Oxs_MeshValueOutputField
(Tcl_Channel channel,    // Output channel
 OC_BOOL do_headers,        // If false, then output only raw data
 const char* title,      // Long filename or title
 const char* desc,       // Description to embed in output file
 vector<String>& valuelabels, // Value label, e.g. "Exchange energy density"
 vector<String>& valueunits,  // Value units, such as "J/m^3".
 Vf_Ovf20_MeshType meshtype,   // Either rectangular or irregular
 Vf_OvfDataStyle datastyle,   // vf_oascii, vf_obin4, or vf_obin8
 const char* textfmt,  // vf_oascii output only, printf-style format
 const Vf_Ovf20_MeshNodes* mesh,    // Mesh
 const Oxs_MeshValue<OC_REAL8m>* val,  // Scalar array
 Vf_OvfFileVersion ovf_version);

// For Oxs_MeshValue<ThreeVector> output
void Oxs_MeshValueOutputField
(Tcl_Channel channel,    // Output channel
 OC_BOOL do_headers,        // If false, then output only raw data
 const char* title,      // Long filename or title
 const char* desc,       // Description to embed in output file
 vector<String>& valuelabels, // Value label, e.g. "Exchange energy density"
 vector<String>& valueunits,  // Value units, such as "J/m^3".
 Vf_Ovf20_MeshType meshtype,   // Either rectangular or irregular
 Vf_OvfDataStyle datastyle,   // vf_oascii, vf_obin4, or vf_obin8
 const char* textfmt,  // vf_oascii output only, printf-style format
 const Vf_Ovf20_MeshNodes* mesh,    // Mesh
 const Oxs_MeshValue<ThreeVector>* val,  // Scalar array
 Vf_OvfFileVersion ovf_version);

// Wrapper around "channel" versions Oxs_MeshValueOutputField,
// providing "filename" access.  In addition to convenience,
// this class makes the open channel exception-safe.
// If the optional fsync parameter is true (which is not the
// default), then Oc_Fsync will be called to insure that all
// the data get flushed from OS buffers all the way to disk.
// The Oc_Fsync call may block, so only use this option when
// it is important to protect against data loss; example use
// is for writing checkpoint (restart) files.
template<class T>
void Oxs_MeshValueOutputField
(const char* filename,   // Output file
 OC_BOOL do_headers,        // If false, then output only raw data
 const char* title,      // Long filename or title
 const char* desc,       // Description to embed in output file
 vector<String>& valuelabels, // Value label, e.g. "Exchange energy density"
 vector<String>& valueunits,  // Value units, such as "J/m^3".
 Vf_Ovf20_MeshType meshtype,   // Either rectangular or irregular
 Vf_OvfDataStyle datastyle,   // vf_oascii, vf_obin4, or vf_obin8
 const char* textfmt,  // vf_oascii output only, printf-style format
 const Vf_Ovf20_MeshNodes* mesh,    // Mesh
 const Oxs_MeshValue<T>* val,       // Scalar array
 Vf_OvfFileVersion ovf_version,
 int fsync=0)
{
  if(filename==nullptr) { // Bad input
    OXS_THROW(Oxs_BadParameter,
              "Failure in Oxs_MeshValueOutputField:"
              " filename not specified.");
  }

  int orig_errno=Tcl_GetErrno();
  Tcl_SetErrno(0);
  Tcl_Channel channel = Tcl_OpenFileChannel(nullptr,
	Oc_AutoBuf(filename),Oc_AutoBuf("w"),0666);
  int new_errno=Tcl_GetErrno();
  Tcl_SetErrno(orig_errno);

  if(channel==nullptr) {
    // File open failure
    String msg=String("Failure in Oxs_MeshValueOutputField:")
      + String(" Unable to open file ") + String(filename);
    if(new_errno!=0)
      msg += String(" for writing: ") + String(Tcl_ErrnoMsg(new_errno));
    else
      msg += String(" for writing.");
    OXS_THROW(Oxs_BadUserInput,msg);
  }

  try {
    // Set channel options
    Tcl_SetChannelOption(nullptr,channel,Oc_AutoBuf("-buffering"),
			 Oc_AutoBuf("full"));
    Tcl_SetChannelOption(nullptr,channel,Oc_AutoBuf("-buffersize"),
			 Oc_AutoBuf("100000"));
    /// What's a good size???
    if(datastyle != vf_oascii) {
      // Binary mode
      Tcl_SetChannelOption(nullptr,channel,Oc_AutoBuf("-encoding"),
			   Oc_AutoBuf("binary"));
      Tcl_SetChannelOption(nullptr,channel,Oc_AutoBuf("-translation"),
			   Oc_AutoBuf("binary"));
    }

    if(title==nullptr || title[0]=='\0') {
      title=filename;
    }

    Oxs_MeshValueOutputField(channel,do_headers,title,desc,
                             valuelabels,valueunits,meshtype,
                             datastyle,textfmt,mesh,val,ovf_version);
  } catch (...) {
    Tcl_Close(nullptr,channel);
    Nb_Remove(filename); // Delete possibly corrupt or
                        /// empty file on error.
    throw;
  }
  if(fsync) {
    Oc_Fsync(channel); // Flush data to disk
  }
  if(Tcl_Close(nullptr,channel) != TCL_OK) {
    // File close failure
    int close_code = Tcl_GetErrno();
    String errno_id = Tcl_ErrnoId();
    String errno_msg = Tcl_ErrnoMsg(close_code);
    String msg=String("Failure in Oxs_MeshValueOutputField:")
      + String(" Error closing file ") + String(filename);
    OXS_TCLTHROW(msg,errno_id,errno_msg);
  }
}

///////////////////////////////////////////////////////////////////////

template<class T> class Oxs_MeshValue {
private:
  T* arr;         // Convenience variables; these shadow settings
  OC_INDEX size; /// in arrblock;

  // arrblock are filled and/or recycled from the arrblock_pool
  // static member.
  static Oxs_Pool< Oxs_StripedArray<T> > arrblock_pool;
  OXS_POOL_PTR< Oxs_StripedArray<T> > arrblock;

  friend Oxs_Director; // Allow Oxs_Director to call
  /// arrblock_pool.EmptyPool() on simulation exits.


  OC_BOOL read_only_lock; // If true, then only const members(*) can be
  /// accessed. (Actually only enforced if NDEBUG is not defined.)
  /// read_only_lock is set either directly through the MarkAsReadOnly()
  /// member, or implicitly when the SharedCopy() member is called. The
  /// read_only_lock can be cleared via the Reset() or Release() members
  /// (which invalidate the current array data) or by destructing the
  /// object.
  /// (*) Exception: The primary purpose of read_only_lock is to protect
  ///     shared copies of arrblock being changed. The Swap() member
  ///     only moves the arrblock pointer around, so Swap() is allowed
  ///     regardless of the read_only_lock setting. We may want to
  ///     revisit this if read_only_lock usage broadens.
  ///
  /// NB: The locking is NOT atomic, and this code does not provide any
  /// multi-thread access protection. It is the responsibility of the
  /// client to provide any necessary thread synchronization.
  ///
  /// Note: It might be convenient to be able to reset the
  ///       read_only_lock if the arrblock pointer use_count indicates
  ///       that it's not shared. For example, if a shared copy is made
  ///       to insert into an Oxs_SimState's DerivedData area, but the
  ///       insert fails, then the shared copy is not needed. When the
  ///       shared copy is deleted the shared copy count is decremented,
  ///       but this read_only_lock stays set.

  void Free() { // Releases arrblock memory
    arrblock=nullptr;
    arr = nullptr;
    size = 0;
    read_only_lock=0;
  }

  static const OC_INDEX MIN_THREADING_SIZE = 10000; // If size is
  // smaller than this, then don't thread array operations.

public:
  Oxs_MeshValue() : arr(0), size(0), read_only_lock(0) {}
  Oxs_MeshValue(const Vf_Ovf20_MeshNodes* mesh)
    : arr(0), size(0), read_only_lock(0) {
    AdjustSize(mesh);
  }
  Oxs_MeshValue(OC_INDEX size_import)
    : arr(0), size(0), read_only_lock(0) {
    AdjustSize(size_import);
  }
  ~Oxs_MeshValue() { Free(); }

  // Note: The copy constructor and operator= duplicate the
  // arrblock contents. Use SharedCopy() if you want instead
  // a quick copy with a shared_ptr to arrblock. (NB: In the
  // shared scenario both objects get marked read_only.)
  Oxs_MeshValue(const Oxs_MeshValue<T> &other);  // Copy constructor
  Oxs_MeshValue<T>& operator=(const Oxs_MeshValue<T> &other);

  // Move constructor and assignment operator.
  Oxs_MeshValue(Oxs_MeshValue<T>&& other);
  Oxs_MeshValue<T>& operator=(Oxs_MeshValue<T>&& other);

  // Mark as read-only. Once set, cannot be unset.
  // This facility is intended for debugging Oxs_MeshValue use through a
  // std::shared_ptr. If the compiler macro NDEBUG is not defined, then
  // an exception is thrown if any non-const member other than the
  // destructor is called. This check is not airtight, since the
  // destructor could be called from a non-const pointer obtained via
  // the T* GetPtr() member prior to the MarkAsReadOnly call.
  OC_BOOL MarkAsReadOnly() {
    OxsMeshValueReadOnlyCheck; // Logically, marking something as
    // read-only that is already read-only doesn't change anything,
    // so we could consider this member to be const and not raise
    // an error if called multiple times.
    return (read_only_lock=1);
  }
  OC_BOOL IsReadOnly() const {
    return read_only_lock;
  }

  // The +=, -=, *=, and /= operators can only be used with
  // types T that support the corresponding operator at the
  // individual T-element level.
  Oxs_MeshValue<T>& operator+=(const Oxs_MeshValue<T> &other);
  Oxs_MeshValue<T>& operator-=(const Oxs_MeshValue<T> &other);
  Oxs_MeshValue<T>& operator*=(const Oxs_MeshValue<T> &other);
  Oxs_MeshValue<T>& operator/=(const Oxs_MeshValue<T> &other);
  Oxs_MeshValue<T>& operator+=(const T &value);
  Oxs_MeshValue<T>& operator*=(OC_REAL8m mult);

  Oxs_MeshValue<T>& operator=(const T &value); // Fills out
  /// *this with size copies of value

  Oxs_MeshValue<T>& Accumulate(OC_REAL8m mult,
                               const Oxs_MeshValue<T> &other);

  Oxs_MeshValue<T>& HornerStep(OC_REAL8m mult,
                               const Oxs_MeshValue<T> &other);

  void Swap(Oxs_MeshValue<T>& other);

  void Reset() { // Similar to Release() in that the data
    // in arrblock is invalidated, but differs in that the arrblock
    // memory is nominally retained---what actually happens is that
    // a request is made to arrblock_pool for a fresh block of
    // memory. If the current block is not in use elsewhere then it will
    // be immediately returned.
    read_only_lock=0;
    arrblock_pool.Recycle(arrblock);
    if(arrblock) arr = arrblock->GetArrBase();
    else         arr = nullptr;
    // As a debugging aid, one might want to initialize arrblock
    // in some way to help catch use of old values after resets.
  }

  Oxs_MeshValue<T> SharedCopy() {
    // Makes a copy with a shared arrblock, and sets read_only_lock
    // to protect against changes.
    Oxs_MeshValue<T> copy;
    copy.arr = arr;
    copy.size = size;
    copy.arrblock = arrblock; // Increments use_count
    copy.read_only_lock = read_only_lock = 1;
    return copy;
  }

  Oxs_MeshValue<T> SharedCopy() const {
    // Same as non-const version, but throws an error if read_only_lock
    // is not already set.
    if(!read_only_lock) {
      OXS_THROW(Oxs_BadLock,"SharedCopy() attempt with a"
                " const Oxs_MeshValue and read_only_lock not set.");
    }
    Oxs_MeshValue<T> copy;
    copy.arr = arr;
    copy.size = size;
    copy.arrblock = arrblock; // Increments use_count
    copy.read_only_lock = 1;
    return copy;
  }

  void Release() {  // Frees arr
    Free();
  }
  void AdjustSize(const Vf_Ovf20_MeshNodes* newmesh);
  /// Compares current size to size of newmesh.  If different, deletes
  /// current arr and allocates new one compatible with newmesh.

  void AdjustSize(const Oxs_MeshValue<T>& other);  // Compares current size
  /// to size of other.  If different, deletes current arr and
  /// allocates new one compatible with other.

  void AdjustSize(OC_INDEX newsize);

  OC_BOOL CheckMesh(const Vf_Ovf20_MeshNodes* mesh) const;
  /// Returns true if current arr size is compatible with mesh.

  inline OC_BOOL IsSame(const Oxs_MeshValue<T>& other) const {
    return ( (this->arr) == (other.arr) );
  }

  inline OC_INDEX Size() const { return size; }
  inline const T& operator[](OC_INDEX index) const;
  inline T& operator[](OC_INDEX index);

  // Avoid using the GetPtr() functions, since these expose
  // implementation details.
  const T* GetPtr() const { return arr; }
  T* GetPtr() {
    OxsMeshValueReadOnlyCheck;
    return arr;
  }

  inline const Oxs_StripedArray<T>* GetArrayBlock() const {
    // Provides memory node striping information. Use sparingly!
    return arrblock.get();
  }

  // For debugging: FirstDifferent() returns 0 if all the elements of
  // arr have the same value (as measured by operator==).  Otherwise,
  // the return value is the index of the first element that is
  // different from arr[0].
  OC_INDEX FirstDifferent() const {
    for(OC_INDEX i=1;i<size;i++) {
      if(arr[i] != arr[0]) return i;
    }
    return 0;
  }

#if OC_INDEX_CHECKS
  // Declare but don't define these.  If used, should raise an
  // error at link time.  Find and fix!
# if OC_INDEX_WIDTH != 2 && OC_HAS_INT2
  const T& operator[](OC_INT2 index) const;
  T& operator[](OC_INT2 index);
  const T& operator[](OC_UINT2 index) const;
  T& operator[](OC_UINT2 index);
# endif
# if OC_INDEX_WIDTH != 4 && OC_HAS_INT4
  const T& operator[](OC_INT4 index) const;
  T& operator[](OC_INT4 index);
  const T& operator[](OC_UINT4 index) const;
  T& operator[](OC_UINT4 index);
# endif
# if OC_INDEX_WIDTH != 8 && OC_HAS_INT8
  const T& operator[](OC_INT8 index) const;
  T& operator[](OC_INT8 index);
  const T& operator[](OC_UINT8 index) const;
  T& operator[](OC_UINT8 index);
# endif
#endif // OC_INDEX_CHECKS


  // Friends, for OVF 2.0 output.
  // NOTE 1: Oxs_MeshValueOutputField is *intentionally* non-templated.
  //       This is not an error, despite what most compilers suggest.
  // NOTE 2: Only accesses Oxs_MeshValue<T> via a const ptr, so should
  //         not trigger any problems with read-only access.
  friend void Oxs_MeshValueOutputField
  (Tcl_Channel channel,
   OC_BOOL do_headers,
   const char* title,
   const char* desc,
   vector<String>& valuelabels,
   vector<String>& valueunits,
   Vf_Ovf20_MeshType meshtype,
   Vf_OvfDataStyle datastyle,
   const char* textfmt,
   const Vf_Ovf20_MeshNodes* mesh,
   const Oxs_MeshValue<T>* val,
   Vf_OvfFileVersion ovf_version);

};

// Static pool members:
template<class T>
Oxs_Pool< Oxs_StripedArray<T> > Oxs_MeshValue<T>::arrblock_pool;

////////////////////////////////////////////////////////////////////////
// Include all implementations here.  This is required by compiler/linkers
// using what gcc calls the "Borland model," as opposed to the "Cfront
// model."  The latter is distinguished by the use of template
// repositories.  With the Cfront model, template definitions can be
// put in a file (*.cc) separate from the header file (*.h).  Putting
// the definitions in the header file along with declarations slows down
// compilation, because the definitions are re-compiled in each including
// file, but appears to be more portable.

template<class T> OC_BOOL
Oxs_MeshValue<T>::CheckMesh(const Vf_Ovf20_MeshNodes* mesh) const
{ // Returns true if size is compatible with mesh
  return (arr && mesh && mesh->Size() == size);
  /// Include "arr" check to provide a some multi-thread safety.
}

template<class T>
const T& Oxs_MeshValue<T>::operator[](OC_INDEX index) const
{
  assert(0<=index && index<size);
  return arr[index];
}

template<class T>
T& Oxs_MeshValue<T>::operator[](OC_INDEX index)
{
  OxsMeshValueReadOnlyCheck;
#ifndef NDEBUG
  if(index<0 || index>=size) {
    char buf[512];
    Oc_Snprintf(buf,sizeof(buf),
                "T& Oxs_MeshValue<T>::operator[]: "
                "Array out-of-bounds; index=%ld, size=%ld",
                long(index),long(size));
#if 0
    fprintf(stderr,"%s\n",buf); fflush(stderr);
    abort();
#endif
    OXS_THROW(Oxs_BadIndex,buf);
  }
#endif
  return arr[index];
}

////////////////////////////////////////////////////////////////////////
/// CONSTRUCTORS

template<class T>
void Oxs_MeshValue<T>::AdjustSize(OC_INDEX newsize)
{
  OxsMeshValueReadOnlyCheck;
  if(newsize != size) {
    arrblock = arrblock_pool.GetFreePtr(newsize);
    if(arrblock) arr = arrblock->GetArrBase();
    else         arr = nullptr;
    size = newsize;
    read_only_lock = 0;
  }
}

template<class T>
void Oxs_MeshValue<T>::AdjustSize(const Vf_Ovf20_MeshNodes* newmesh)
{
  OxsMeshValueReadOnlyCheck;
  OC_INDEX newsize = 0;
  if(newmesh != nullptr) {
    newsize = newmesh->Size();
  }
  AdjustSize(newsize);
}

template<class T>
void Oxs_MeshValue<T>::AdjustSize(const Oxs_MeshValue<T>& other)
{
  OxsMeshValueReadOnlyCheck;
  if(this == &other) return;  // Nothing to do.
  AdjustSize(other.size);
}

// Copy assignment operator: This makes a completely separate copy of
// arrblock, and is therefore expensive. Faster alternatives, where
// applicable, are move assignment (which leaves other empty) and
// SharedCopy which uses the arrblock shared_ptr to make a shared
// copy of arrblock.
template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator=(const Oxs_MeshValue<T>& other)
{
  OxsMeshValueReadOnlyCheck;
  if(this == &other) return *this;  // Nothing to do.
  AdjustSize(other);
  if(size < 1) return *this; // Nothing to copy
  if(size<MIN_THREADING_SIZE) {
    memcpy(arr,other.arr,static_cast<size_t>(size)*sizeof(T));
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        memcpy(arr+jstart,other.arr+jstart,
               static_cast<size_t>(jstop-jstart)*sizeof(T));
      });
  }
  return *this;
}

// Copy constructor
template<class T>
Oxs_MeshValue<T>::Oxs_MeshValue(const Oxs_MeshValue<T> &other)
  : arr(0), size(0), read_only_lock(0)
{
  operator=(other);
}

// Move assignment operator. See also SharedCopy
template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator=(Oxs_MeshValue<T>&& other)
{
  OxsMeshValueReadOnlyCheck;
  if(this == &other) return *this;  // Nothing to do.
  arrblock = std::move(other.arrblock);
  arr = other.arr;                       other.arr = nullptr;
  size = other.size;                     other.size = 0;
  read_only_lock = other.read_only_lock; other.read_only_lock=0;
  return *this;
}

// Move constructor
template<class T>
Oxs_MeshValue<T>::Oxs_MeshValue(Oxs_MeshValue<T>&& other)
  : arr(other.arr), size(other.size),
    arrblock(std::move(other.arrblock)),
    read_only_lock(other.read_only_lock)
{
  other.arr = nullptr;
  other.size = 0;
  other.read_only_lock=0;
}

template<class T>
void Oxs_MeshValue<T>::Swap(Oxs_MeshValue<T>& other)
{ // Note: No read-only check
  if( this == &other) return;
  std::swap(size,other.size);
  std::swap(arr,other.arr);
  std::swap(arrblock,other.arrblock);
  std::swap(read_only_lock,other.read_only_lock);
}

////////////////////////////////////////////////////////////////////////
// Array operators.

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator+=(const Oxs_MeshValue<T>& other)
{
  OxsMeshValueReadOnlyCheck;
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::operator+=");
  }
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) arr[i]+=other.arr[i];
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] += other.arr[j];
        }
      });
  }
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator-=(const Oxs_MeshValue<T>& other)
{
  OxsMeshValueReadOnlyCheck;
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::operator-=");
  }
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) arr[i]-=other.arr[i];
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] -= other.arr[j];
        }
      });
  }
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator*=(const Oxs_MeshValue<T>& other)
{
  OxsMeshValueReadOnlyCheck;
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::operator*=");
  }
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) arr[i]*=other.arr[i];
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] *= other.arr[j];
        }
      });
  }
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator/=(const Oxs_MeshValue<T>& other)
{
  OxsMeshValueReadOnlyCheck;
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::operator/=");
  }
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) arr[i]/=other.arr[i];
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] /= other.arr[j];
        }
      });
  }
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator+=(const T& value)
{
  OxsMeshValueReadOnlyCheck;
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) arr[i]+=value;
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] += value;
        }
      });
  }
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator*=(OC_REAL8m mult)
{
  OxsMeshValueReadOnlyCheck;
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) arr[i]*=mult;
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] *= mult;
        }
      });
  }
  return *this;
}


template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator=(const T &value)
{
  OxsMeshValueReadOnlyCheck;
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) arr[i]=value;
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] = value;
        }
      });
  }
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::Accumulate
(OC_REAL8m mult,
 const Oxs_MeshValue<T> &other)
{
  OxsMeshValueReadOnlyCheck;
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::Accumulate");
  }
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) arr[i]+=mult*other.arr[i];
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] += mult*other.arr[j];
        }
      });
  }
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::HornerStep
(OC_REAL8m mult,
 const Oxs_MeshValue<T> &other)
{
  OxsMeshValueReadOnlyCheck;
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::HornerStep");
  }
  if(size<MIN_THREADING_SIZE) {
    for(OC_INDEX i=0;i<size;i++) {
      arr[i] *= mult;
      arr[i] += other.arr[i];
    }
  } else {
    Oxs_RunThreaded<T,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*this,
       [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        for(OC_INDEX j=jstart;j<jstop;++j) {
          arr[j] *= mult;
          arr[j] += other.arr[j];
        }
      });
  }
  return *this;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

// Should definitions for the two non-templated
// Oxs_MeshValueOutputField functions be in this header or in the
// meshvalue.cc source file?  See the top of meshvalue.cc for a
// discussion of this problem.  Bottom line is that we put definitions
// in both places, and select one via macros at compile time.
// BIG FAT NOTE: Any changes to the Oxs_MeshValueOutputField functions
// need to be duplicated in *BOTH* places.
#if defined(__DMC__)
# define OXS_MESHVALUEOUTPUTFIELD_IN_HEADER 1
#else
# define OXS_MESHVALUEOUTPUTFIELD_IN_HEADER 0
#endif

#if OXS_MESHVALUEOUTPUTFIELD_IN_HEADER
// The following two overloaded, non-template functions are
// defined in this header rather than in meshvalue.cc as a
// sop to the Digital Marc C++ version 8.57 compiler, which
// otherwise gets confused and fails at the link stage.

// Routines for Oxs_MeshValue<OC_REAL8m> output
inline void Oxs_MeshValueOutputField
(Tcl_Channel channel,    // Output channel
 OC_BOOL do_headers,     // If false, then output only raw data
 const char* title,      // Long filename or title
 const char* desc,       // Description to embed in output file
 vector<String>& valuelabels, // Value label, such as "Exchange energy density"
 vector<String>& valueunits,  // Value units, such as "J/m^3".
 Vf_Ovf20_MeshType meshtype,   // Either rectangular or irregular
 Vf_OvfDataStyle datastyle,   // vf_oascii, vf_obin4, or vf_obin8
 const char* textfmt,  // vf_oascii output only, printf-style format
 const Vf_Ovf20_MeshNodes* mesh,    // Mesh
 const Oxs_MeshValue<OC_REAL8m>* val,  // Scalar array
 Vf_OvfFileVersion ovf_version)
{
  Vf_Ovf20FileHeader fileheader;

  // Fill header
  mesh->DumpGeometry(fileheader,meshtype);
  fileheader.title.Set(String(title));
  fileheader.valuedim.Set(1);  // Scalar field
  fileheader.valuelabels.Set(valuelabels);
  fileheader.valueunits.Set(valueunits);
  fileheader.desc.Set(String(desc));
  fileheader.ovfversion = ovf_version;
  if(!fileheader.IsValid()) {
    OXS_THROW(Oxs_ProgramLogicError,"Oxs_MeshValueOutputField(T=OC_REAL8m)"
              " failed to create a valid OVF fileheader.");
  }

  // Write header
  if(do_headers) fileheader.WriteHeader(channel);

  // Write data
  Vf_Ovf20VecArrayConst data_info(1,val->Size(),val->arr);
  fileheader.WriteData(channel,datastyle,textfmt,mesh,
                       data_info,!do_headers);
}

// For Oxs_MeshValue<ThreeVector> output
inline void Oxs_MeshValueOutputField
(Tcl_Channel channel,    // Output channel
 OC_BOOL do_headers,        // If false, then output only raw data
 const char* title,      // Long filename or title
 const char* desc,       // Description to embed in output file
 vector<String>& valuelabels, // Value label, such as "Exchange energy density"
 vector<String>& valueunits,  // Value units, such as "J/m^3".
 Vf_Ovf20_MeshType meshtype,   // Either rectangular or irregular
 Vf_OvfDataStyle datastyle,   // vf_oascii, vf_obin4, or vf_obin8
 const char* textfmt,  // vf_oascii output only, printf-style format
 const Vf_Ovf20_MeshNodes* mesh,    // Mesh
 const Oxs_MeshValue<ThreeVector>* val,  // Scalar array
 Vf_OvfFileVersion ovf_version)
{
  Vf_Ovf20FileHeader fileheader;

  // Fill header
  mesh->DumpGeometry(fileheader,meshtype);
  fileheader.title.Set(String(title));
  fileheader.valuedim.Set(3);  // Vector field
  fileheader.valuelabels.Set(valuelabels);
  fileheader.valueunits.Set(valueunits);
  fileheader.desc.Set(String(desc));
  fileheader.ovfversion = ovf_version;
  if(!fileheader.IsValid()) {
    OXS_THROW(Oxs_ProgramLogicError,"Oxs_MeshValueOutputField(T=ThreeVector)"
              " failed to create a valid OVF fileheader.");
  }

  // Write header
  if(do_headers) fileheader.WriteHeader(channel);

  const OC_REAL8m* arrptr = nullptr;
  Nb_ArrayWrapper<OC_REAL8m> rbuf;
  if(sizeof(ThreeVector) == 3*sizeof(OC_REAL8m)) {
    // Use MeshValue array directly
    arrptr = reinterpret_cast<const OC_REAL8m*>(val->arr);
  } else {
    // Need intermediate buffer space
    const OC_INDEX valsize = val->Size();
    rbuf.SetSize(3*valsize);
    for(OC_INDEX i=0;i<valsize;++i) {
      rbuf[3*i]   = val->arr[i].x;
      rbuf[3*i+1] = val->arr[i].y;
      rbuf[3*i+2] = val->arr[i].z;
    }
    arrptr = rbuf.GetPtr();
  }


  // Write data
  Vf_Ovf20VecArrayConst data_info(3,val->Size(),arrptr);
  fileheader.WriteData(channel,datastyle,textfmt,mesh,
                       data_info,!do_headers);
}
#endif // OXS_MESHVALUEOUTPUTFIELD_IN_HEADER

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

#endif // _OXS_MESHVALUE
