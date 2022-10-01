/* FILE: arraywrapper.h                 -*-Mode: c++-*-
 *
 * Basic array templates --- these are essentially exception-safe
 * wrappers for the standard C++ new[]/delete[] operators.
 *
 * Note: Compare also against the older Array2D and Array3D classes.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/07/16 06:19:37 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_ARRAYWRAPPER
#define _NB_ARRAYWRAPPER

#include <cassert>
#include "oc.h"

/* End includes */

#define NB_ARRAYWRAPPER_ERRBUFSIZE 1024

////////////////////////////////////////////////////////////////////////
// Wrapper template for standard C++ new[]/delete[] memory allocation.
// Bare new's inside class constructors don't provide automatic cleanup
// (i.e., delete[] calls) if an exception is thrown inside the
// constructor.  On the other hand, STL container classes place copy
// constructor and operator= requirements on their templated class.
// This class provides a workaround for both problems.
template<class T> class Nb_ArrayWrapper
{
private:
  OC_INDEX size;
  T* const objs;
  char* objbase;  // Alloc base, used iff alignment is requested

  // Disable copy operations by declaring the following
  // without providing definitions.
  Nb_ArrayWrapper(const Nb_ArrayWrapper<T>&);
  Nb_ArrayWrapper<T>& operator=(const Nb_ArrayWrapper<T>&);

public:
  void Free(); // Calls destructors for objs, releases memory,
  /// and sets size to 0.

  void SetSize(OC_INDEX new_size,OC_INDEX alignment=0);
  // Throws an exception if size!=0 on entry.  If you want to use this
  // function, call Free() first.

  OC_INDEX GetSize() const { return size; }

  Nb_ArrayWrapper(OC_INDEX initial_size=0,OC_INDEX alignment=0)
    : size(0), objs(0), objbase(0) {
    SetSize(initial_size,alignment);
  }

  ~Nb_ArrayWrapper() { Free(); }

#ifdef NDEBUG
  T& operator[](OC_INDEX i) { return objs[i]; }
  const T& operator[](OC_INDEX i) const { return objs[i]; }
  /// NB: No range check!
#else
  T& operator[](OC_INDEX i);
  const T& operator[](OC_INDEX i) const;
  /// Ranged checked versions
#endif

  // Of course, the return value from GetPtr is only valid
  // through the next call to Free or object destruction.
  const T* GetPtr() { return objs; }


#if OC_INDEX_CHECKS
  // OC_INDEX import type checks for some of the above.  We declare but
  // don't define the following, so if they are used, it should raise an
  // error at link time.  Find and fix!
# if OC_INDEX_WIDTH != 2 && OC_HAS_INT2
  void SetSize(OC_INT2 new_size,OC_INT2 alignment);
  Nb_ArrayWrapper(OC_INT2 initial_size,OC_INT2 alignment);
  T& operator[](OC_INT2 i);
  const T& operator[](OC_INT2 i) const;
  void SetSize(OC_UINT2 new_size,OC_UINT2 alignment);
  Nb_ArrayWrapper(OC_UINT2 initial_size,OC_UINT2 alignment);
  T& operator[](OC_UINT2 i);
  const T& operator[](OC_UINT2 i) const;
# endif
# if OC_INDEX_WIDTH != 4 && OC_HAS_INT4
  void SetSize(OC_INT4 new_size,OC_INT4 alignment);
  Nb_ArrayWrapper(OC_INT4 initial_size,OC_INT4 alignment);
  T& operator[](OC_INT4 i);
  const T& operator[](OC_INT4 i) const;
  void SetSize(OC_UINT4 new_size,OC_UINT4 alignment);
  Nb_ArrayWrapper(OC_UINT4 initial_size,OC_UINT4 alignment);
  T& operator[](OC_UINT4 i);
  const T& operator[](OC_UINT4 i) const;
# endif
# if OC_INDEX_WIDTH != 8 && OC_HAS_INT8
  void SetSize(OC_INT8 new_size,OC_INT8 alignment);
  Nb_ArrayWrapper(OC_INT8 initial_size,OC_INT8 alignment);
  T& operator[](OC_INT8 i);
  const T& operator[](OC_INT8 i) const;
  void SetSize(OC_UINT8 new_size,OC_UINT8 alignment);
  Nb_ArrayWrapper(OC_UINT8 initial_size,OC_UINT8 alignment);
  T& operator[](OC_UINT8 i);
  const T& operator[](OC_UINT8 i) const;
# endif
#endif // OC_INDEX_CHECKS

};

template<class T>
void Nb_ArrayWrapper<T>::Free()
{
  if(size==0 && objs==0 && objbase==0) return; // Nothing to do
  if(size==0 || objs==0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ArrayWrapper<T>","Free",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,
                          "Size and objs pointer incompatibility detected."));
  }
  if(objbase==0) { // Standard alloc, no special alignment
    size=0;
    delete[] objs;
    const_cast<T*&>(objs) = 0;
  } else { // Aligned data; Implement "placement delete"
    while(size>0) objs[--size].~T(); // Explicit destructor call
    const_cast<T*&>(objs)=0;
    delete[] objbase;
    objbase=0;
  }
}

template<class T>
void Nb_ArrayWrapper<T>::SetSize(OC_INDEX new_size,OC_INDEX alignment)
{
  if(size!=0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ArrayWrapper<T>","SetSize",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,
                          "Array not empty; call Free first."));
  }
  if(new_size==0) return; // Nothing to do.
  if(new_size<0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ArrayWrapper<T>","SetSize",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,
                          "Invalid import value(s)."));
  }
  if(alignment<2) {
    objbase = 0;  // No alignment request; alloc straight into objs
    if((const_cast<T*&>(objs) = new T[size_t(new_size)])==0) {
      // For news that don't throw bad_alloc 
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ArrayWrapper<T>","SetSize",
                            NB_ARRAYWRAPPER_ERRBUFSIZE,
                            "Memory allocation error."));
    }
    size=new_size;
  } else {
    // Alignment requested
    size_t blocksize = size_t(new_size)*sizeof(T)+alignment-1;
    if(blocksize/sizeof(T) < size_t(new_size) + (alignment-1)/sizeof(T)) {
      char tbuf[512];
      Oc_Snprintf(tbuf,sizeof(tbuf),
                  "Allocation request size too big:"
                  " %" OC_INDEX_MOD "d items of size %lu",
                  new_size,(unsigned long)sizeof(T));
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ArrayWrapper<T>","SetSize",
                            NB_ARRAYWRAPPER_ERRBUFSIZE,tbuf));
    }
    if((objbase = new char[blocksize])==0) {
      // For news that don't throw bad_alloc 
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ArrayWrapper<T>","SetSize",
                            NB_ARRAYWRAPPER_ERRBUFSIZE,
                            "Memory allocation error."));
    }
    size_t alignoff = reinterpret_cast<size_t>(objbase) % alignment;
    if(alignoff>0) {
      alignoff = alignment - alignoff;
    }

#ifdef OC_NO_PLACEMENT_NEW_ARRAY
    // Compiler does not support placment new[].  Assume that
    // the single item placement new works, and work around by
    // using that in a loop.
    const_cast<T*&>(objs) = new(objbase + alignoff)T;
    for(OC_INDEX ix=1;ix<new_size;++ix) {
      new(objbase + alignoff + ix*sizeof(T))T;
    }
#else
    const_cast<T*&>(objs)
      = new(objbase + alignoff)T[new_size];  // Placement new
#endif
    assert(reinterpret_cast<size_t>(objs) % alignment == 0);
    size=new_size;
  }
}

#ifndef NDEBUG
template<class T>
T& Nb_ArrayWrapper<T>::operator[](OC_INDEX i)
{
  if(i<0 || i>=size) {
    char errmsg[64];
    Oc_Snprintf(errmsg,sizeof(errmsg),
                "Index out of range: %" OC_INT8_MOD "d/%" OC_INT8_MOD "d\n",
                i,size);
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ArrayWrapper<T>","operator[]",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,errmsg));
  } 
  return objs[i];
}

template<class T>
const T& Nb_ArrayWrapper<T>::operator[](OC_INDEX i) const
{
  if(i>=size) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ArrayWrapper<T>",
                          "operator[] const",NB_ARRAYWRAPPER_ERRBUFSIZE,
                          "Index too big."));
  } 
  return objs[i];
}
#endif // NDEBUG

////////////////////////////////////////////////////////////////////////
// 2D array wrapper.  See notes with Nb_ArrayWrapper above.
// NOTE: Unlike the Oxs_RectangularMesh class, Nb_2DArrayWrapper
//  stores values in normal C order, i.e., fast loops access index
//  (i,j) by varying j in the inner loop, i in the outer loop.
template<class T> class Nb_2DArrayWrapper
{
private:
  OC_INDEX size1,size2;
  T** objs;

  // Disable copy operations by declaring the following
  // without providing definitions.
  Nb_2DArrayWrapper(const Nb_2DArrayWrapper<T>&);
  Nb_2DArrayWrapper<T>& operator=(const Nb_2DArrayWrapper<T>&);

public:
  void Free(); // Calls destructors for objs, releases memory,
  /// and sets size to 0.

  void SetSize(OC_INDEX new_size1,OC_INDEX new_size2);  // Throws an
  // exception if either size!=0 on entry.  If you want to use this
  // function, call Free() first.

  void GetSize(OC_INDEX& mysize1,OC_INDEX& mysize2) const {
    mysize1=size1; mysize2=size2;
  }

  Nb_2DArrayWrapper(OC_INDEX isize1=0,OC_INDEX isize2=0)
    : size1(0), size2(0), objs(NULL) {
    SetSize(isize1,isize2);
  }

  ~Nb_2DArrayWrapper() { Free(); }

#ifdef NDEBUG
  T* operator[](OC_INDEX i) { return objs[i]; }
  const T* operator[](OC_INDEX i) const { return objs[i]; }
  T& operator()(OC_INDEX i,OC_INDEX j) { return objs[i][j]; }
  const T& operator()(OC_INDEX i,OC_INDEX j) const { return objs[i][j]; }
  /// NB: No range check!
#else
  T* operator[](OC_INDEX i);
  const T* operator[](OC_INDEX i) const;
  T& operator()(OC_INDEX i,OC_INDEX j);
  const T& operator()(OC_INDEX i,OC_INDEX j) const;
  /// Ranged checked versions.  IMPORTANT: The 'operator[]' version only
  /// checks the leading index.  To get full range check protection, use
  /// the operator(,) version.
#endif

};

template<class T>
void Nb_2DArrayWrapper<T>::Free()
{
  if(size1==0 && size2==0 && objs==NULL) return; // Nothing to do
  if(size1==0 || size2==0 || objs==NULL) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>","Free",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,
                          "Size and objs pointer incompatibility detected."));
  }
  size1=size2=0;
  delete[] objs[0];
  delete[] objs;
  objs=NULL;
}

template<class T>
void Nb_2DArrayWrapper<T>::SetSize(OC_INDEX new_size1,OC_INDEX new_size2)
{
  if(size1!=0 || size2!=0 || objs!=NULL) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>","SetSize",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,
                          "Array not empty; call Free first."));
  }
  if(new_size1==0 || new_size2==0) return; // Nothing to do.

  // Overflow check
  if(new_size1<=0 || new_size2<=0 || (OC_INDEX_MAX/new_size1)<new_size2) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>","SetSize",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,
                          "Index overflow detected; size1=%"
                          OC_INDEX_MOD "d, size2=%" OC_INDEX_MOD "d",
                          new_size1,new_size2));
  }

  size1=new_size1;  size2=new_size2;
  OC_INDEX totalsize = size1*size2;

  // Allocate pointer array
  objs = new T*[size_t(size1)];
  if(objs==NULL) {
    // Safety check for old compilers that don't throw exception
    // on memory allocation failure.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>","SetSize",
                     NB_ARRAYWRAPPER_ERRBUFSIZE,"Memory allocation error."));
  }

  // Allocate base array
  objs[0] = new T[size_t(totalsize)];
  if(objs[0]==NULL) {
    // Safety check for old compilers that don't throw exception
    // on memory allocation failure.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>","SetSize",
                     NB_ARRAYWRAPPER_ERRBUFSIZE,"Memory allocation error."));
  }

  // Initialize pointers into base area
  for(OC_INDEX i=1;i<size1;i++) objs[i] = objs[i-1] + size2;
}

#ifndef NDEBUG
template<class T>
T* Nb_2DArrayWrapper<T>::operator[](OC_INDEX i)
{
  if(i>=size1) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>",
                          "operator[]",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,"Index too big."));
  }
  return objs[i];
}

template<class T>
const T* Nb_2DArrayWrapper<T>::operator[](OC_INDEX i) const
{
  if(i>=size1) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>",
                          "operator[] const",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,"Index too big."));
  } 
  return objs[i];
}

template<class T>
T& Nb_2DArrayWrapper<T>::operator()(OC_INDEX i,OC_INDEX j)
{
  if(i>=size1 || j>=size2) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>",
                          "operator(OC_INDEX,OC_INDEX)",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,"Index too big."));
  } 
  return objs[i][j];
}

template<class T>
const T& Nb_2DArrayWrapper<T>::operator()(OC_INDEX i,OC_INDEX j) const
{
  if(i>=size1 || j>=size2) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_2DArrayWrapper<T>",
                          "operator(OC_INDEX,OC_INDEX) const",
                          NB_ARRAYWRAPPER_ERRBUFSIZE,"Index too big."));
  } 
  return objs[i][j];
}
#endif // NDEBUG

#endif // _NB_ARRAYWRAPPER
