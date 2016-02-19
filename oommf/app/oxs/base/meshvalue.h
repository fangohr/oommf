/* FILE: meshvalue.h                 -*-Mode: c++-*-
 *
 * Oxs_MeshValue templated class, intended for use with the
 * Oxs_Mesh family.
 *
 * Note: To maintain a clean dependency hierarchy and prevent cyclic
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
 * Note on threads: Prior to Dec-2008, this file contained code that
 *    provided threaded versions of several of the Oxs_MeshValue
 *    operations, such as operator+=, etc.  However, the time
 *    required to perform the floating point operations inside these
 *    tends to be rather small as compared to the memory overhead,
 *    so that the threading does not provide substantial speed-up
 *    because of memory bandwidth limits.  For simplicity, this
 *    code has been dropped from the head; if it makes sense to
 *    bring it back at some point, check the cvs repository.
 *       At the same time, a mutex has been introduced in support of
 *    Oxs_MeshValue lazy allocation.  This should be a NOP if
 *    OOMMF_THREADS is not defined, otherwise the overhead is
 *    hopefully minimal as OOMMF generally tries to limit the number
 *    of Oxs_MeshValue memory allocations.  If this turns out to be
 *    a performance problem, then we may want to consider exposing
 *    the mutex or else provide versions of the allocators that
 *    don't use the mutex.  -mjd, Dec-2008
 *
 */

#ifndef _OXS_MESHVALUE
#define _OXS_MESHVALUE

#include <string.h>  // For memcpy

#include <vector>

#include "vf.h"

#include "oxsexcept.h"
#include "oxsthread.h"
#include "threevector.h"

/* End includes */

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
  if(filename==NULL) { // Bad input
    OXS_THROW(Oxs_BadParameter,
              "Failure in Oxs_MeshValueOutputField:"
              " filename not specified.");
  }

  int orig_errno=Tcl_GetErrno();
  Tcl_SetErrno(0);
  Tcl_Channel channel = Tcl_OpenFileChannel(NULL,
	Oc_AutoBuf(filename),Oc_AutoBuf("w"),0666);
  int new_errno=Tcl_GetErrno();
  Tcl_SetErrno(orig_errno);

  if(channel==NULL) {
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
    Tcl_SetChannelOption(NULL,channel,Oc_AutoBuf("-buffering"),
			 Oc_AutoBuf("full"));
    Tcl_SetChannelOption(NULL,channel,Oc_AutoBuf("-buffersize"),
			 Oc_AutoBuf("100000"));
    /// What's a good size???
    if(datastyle != vf_oascii) {
      // Binary mode
      Tcl_SetChannelOption(NULL,channel,Oc_AutoBuf("-encoding"),
			   Oc_AutoBuf("binary"));
      Tcl_SetChannelOption(NULL,channel,Oc_AutoBuf("-translation"),
			   Oc_AutoBuf("binary"));
    }

    if(title==NULL || title[0]=='\0') {
      title=filename;
    }

    Oxs_MeshValueOutputField(channel,do_headers,title,desc,
                             valuelabels,valueunits,meshtype,
                             datastyle,textfmt,mesh,val,ovf_version);
  } catch (...) {
    Tcl_Close(NULL,channel);
    Nb_Remove(filename); // Delete possibly corrupt or
                        /// empty file on error.
    throw;
  }
  if(fsync) {
    Oc_Fsync(channel); // Flush data to disk
  }
  if(Tcl_Close(NULL,channel) != TCL_OK) {
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
  T* const arr;         // Convenience variables; these shadow settings
  OC_INDEX const size; /// in arrblock;

  Oxs_Mutex alloc_mutex; // Add some thread safety for lazy allocation

  Oxs_StripedArray<T> arrblock;

  void Free();

public:
  Oxs_MeshValue() : arr(0), size(0) {}
  Oxs_MeshValue(const Vf_Ovf20_MeshNodes* mesh)
    : arr(0), size(0) {
    AdjustSize(mesh);
  }
  ~Oxs_MeshValue() { Free(); }

  Oxs_MeshValue(const Oxs_MeshValue<T> &other);  // Copy constructor
  Oxs_MeshValue<T>& operator=(const Oxs_MeshValue<T> &other);

  // The +=, -=, *=, and /= operators can only be used with
  // types T that support the corresponding operator at the
  // individual T-element level.
  Oxs_MeshValue<T>& operator+=(const Oxs_MeshValue<T> &other);
  Oxs_MeshValue<T>& operator-=(const Oxs_MeshValue<T> &other);
  Oxs_MeshValue<T>& operator*=(const Oxs_MeshValue<T> &other);
  Oxs_MeshValue<T>& operator/=(const Oxs_MeshValue<T> &other);
  Oxs_MeshValue<T>& operator*=(OC_REAL8m mult);

  Oxs_MeshValue<T>& operator=(const T &value); // Fills out
  /// *this with size copies of value

  Oxs_MeshValue<T>& Accumulate(OC_REAL8m mult,
                               const Oxs_MeshValue<T> &other);

  Oxs_MeshValue<T>& HornerStep(OC_REAL8m mult,
                               const Oxs_MeshValue<T> &other);

  void Swap(Oxs_MeshValue<T>& other);

  void Release() { Free(); }  // Frees arr
  void AdjustSize(const Vf_Ovf20_MeshNodes* newmesh);
  /// Compares current size to size of newmesh.  If different, deletes
  /// current arr and allocates new one compatible with newmesh.

  void AdjustSize(const Oxs_MeshValue<T>& other);  // Compares current size
  /// to size of other.  If different, deletes current arr and
  /// allocates new one compatible with other.

  OC_BOOL CheckMesh(const Vf_Ovf20_MeshNodes* mesh) const;
  /// Returns true if current arr size is compatible with mesh.

  inline OC_BOOL IsSame(const Oxs_MeshValue<T>& other) {
    return ( (this->arr) == (other.arr) );
  }

  inline OC_INDEX Size() const { return size; }
  inline const T& operator[](OC_INDEX index) const;
  inline T& operator[](OC_INDEX index);

  // Avoid using the GetPtr() functions, since these expose
  // implementation details.
  const T* GetPtr() const { return arr; }
  T* GetPtr() { return arr; }

  inline const Oxs_StripedArray<T>* GetArrayBlock() const {
    // Provides memory node striping information
    return &arrblock;
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
  // NOTE: Oxs_MeshValueOutputField is *intentionally* non-templated.
  //       This is not an error, despite what most compilers suggest.
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
#ifndef NDEBUG
  if(index>=size) {
    char buf[512];
    Oc_Snprintf(buf,sizeof(buf),
                "const T& Oxs_MeshValue<T>::operator[] const: "
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

template<class T>
T& Oxs_MeshValue<T>::operator[](OC_INDEX index)
{
#ifndef NDEBUG
  if(index>=size) {
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

template<class T>
void Oxs_MeshValue<T>::Free()
{
  // Free value array.  Note that this is *NOT* locked on
  // the alloc_mutex.  Calling routines should lock on an
  // as needed basis.  (One should be careful that a mutex
  // lock call isn't made from a thread already holding a
  // lock on that same mutex.)
  arrblock.Free();
  const_cast<T*&>(arr) = 0;
  const_cast<OC_INDEX&>(size) = 0;
}

////////////////////////////////////////////////////////////////////////
/// CONSTRUCTORS

template<class T>
Oxs_MeshValue<T>::Oxs_MeshValue(const Oxs_MeshValue<T> &other)
  : arr(0), size(0)
{ // Copy constructor.
  AdjustSize(other);
  if(size>0) {
    memcpy(arr,other.arr,static_cast<size_t>(size)*sizeof(T));
  }
}

template<class T>
void Oxs_MeshValue<T>::Swap(Oxs_MeshValue<T>& other)
{ // Note: This works okay also if this==&other
  alloc_mutex.Lock();
  OC_INDEX tsize = size;
  const_cast<OC_INDEX&>(size) = other.size;
  const_cast<OC_INDEX&>(other.size) = tsize;

  T*              tarr  = arr;
  const_cast<T*&>(arr)  = other.arr;
  const_cast<T*&>(other.arr)  = tarr;

  arrblock.Swap(other.arrblock);

  alloc_mutex.Unlock();
}

template<class T>
void Oxs_MeshValue<T>::AdjustSize(const Vf_Ovf20_MeshNodes* newmesh)
{
  if(newmesh == NULL) {
    alloc_mutex.Lock();
    try {
      Free();
    } catch(...) {
      alloc_mutex.Unlock();
      throw;
    }
    alloc_mutex.Unlock();
  } else {
    const OC_INDEX newsize = newmesh->Size();
    alloc_mutex.Lock();
    try {
      if(newsize != size) { // Potential race hazard: what if
        // size gets set to newsize before arr is pointed to valid alloc
        // space -- at least as seen by some thread?  For safety, keep
        // this check inside alloc_mutex.Lock() region.

        // Allocate new value array.  For thread safety, "size"
        // should be adjusted defensively wrt "arr" allocation.
        // (See CheckMesh member function.)
        Free(); // Sets "size" to 0.
        if(newsize>0) {
          arrblock.SetSize(newsize);
          const_cast<T*&>(arr) = arrblock.GetArrBase();
          if(arr==NULL) {
            char errbuf[256];
            Oc_Snprintf(errbuf,sizeof(errbuf),
                        "Unable to allocate memory for %ld objects of size %lu"
                        " (using AdjustSize(Vf_Ovf20_MeshNodes*) interface)",
                        (long)newsize,(unsigned long)sizeof(T));
            OXS_THROW(Oxs_NoMem,errbuf);
          }
          const_cast<OC_INDEX&>(size) = newsize;
        }
      }
    } catch(...) {
      alloc_mutex.Unlock();
      throw;
    }
    alloc_mutex.Unlock();
  }
}

template<class T>
void Oxs_MeshValue<T>::AdjustSize(const Oxs_MeshValue<T>& other)
{
  if(this == &other) return;  // Nothing to do.
  const OC_INDEX newsize = other.size;
  if(newsize != size) {
      alloc_mutex.Lock();
      try {
        if(newsize != size) { // Won race?
          // Allocate new value array.  For thread safety, "size"
          // should be adjusted defensively wrt "arr" allocation.
          // (See CheckMesh member function.)
          Free(); // Sets "size" to 0.
          if(newsize>0) {
            arrblock.SetSize(newsize);
            const_cast<T*&>(arr) = arrblock.GetArrBase();
            if(arr==NULL) {
              char errbuf[256];
              Oc_Snprintf(errbuf,sizeof(errbuf),
                          "Unable to allocate memory for %ld objects of size %lu"
                          " (using AdjustSize(Oxs_MeshValue<T>&) interface)",
                          (long)newsize,(unsigned long)sizeof(T));
              OXS_THROW(Oxs_NoMem,errbuf);
            }
            const_cast<OC_INDEX&>(size) = newsize;
          }
        }
      } catch(...) {
        alloc_mutex.Unlock();
      }
      alloc_mutex.Unlock();
  }
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator=(const Oxs_MeshValue<T>& other)
{
  if(this == &other) return *this;  // Nothing to do.
  AdjustSize(other);
  if(size>0) {
    memcpy(arr,other.arr,static_cast<size_t>(size)*sizeof(T));
  }
  return *this;
}

////////////////////////////////////////////////////////////////////////
// Array operators.

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator+=(const Oxs_MeshValue<T>& other)
{
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::operator+=");
  }
  for(OC_INDEX i=0;i<size;i++) arr[i]+=other.arr[i];
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator-=(const Oxs_MeshValue<T>& other)
{
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::operator-=");
  }
  for(OC_INDEX i=0;i<size;i++) arr[i]-=other.arr[i];
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator*=(const Oxs_MeshValue<T>& other)
{ // NOTE: Use of this operator requires that type T support
  // the operator*=(const T&) operator.
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::operator*=");
  }
  for(OC_INDEX i=0;i<size;i++) arr[i]*=other.arr[i];
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator/=(const Oxs_MeshValue<T>& other)
{ // NOTE: Use of this operator requires that type T support
  // the operator/=(const T&) operator.
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::operator/=");
  }
  for(OC_INDEX i=0;i<size;i++) arr[i]/=other.arr[i];
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator*=(OC_REAL8m mult)
{ // NOTE: Use of this operator requires that type T support
  // the operator*=(OC_REAL8m) operator.
  for(OC_INDEX i=0;i<size;i++) arr[i]*=mult;
  return *this;
}


template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::operator=(const T &value)
{
  for(OC_INDEX i=0;i<size;i++) arr[i]=value;
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::Accumulate
(OC_REAL8m mult,
 const Oxs_MeshValue<T> &other)
{
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::Accumulate");
  }
  for(OC_INDEX i=0;i<size;i++) arr[i]+=mult*other.arr[i];
  return *this;
}

template<class T> Oxs_MeshValue<T>&
Oxs_MeshValue<T>::HornerStep
(OC_REAL8m mult,
 const Oxs_MeshValue<T> &other)
{
  if(size!=other.size) {
    OXS_THROW(Oxs_BadIndex,
             "Size mismatch in Oxs_MeshValue<T>::Accumulate");
  }
  for(OC_INDEX i=0;i<size;i++) {
    arr[i] *= mult;
    arr[i] += other.arr[i];
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

  const OC_REAL8m* arrptr = NULL;
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
