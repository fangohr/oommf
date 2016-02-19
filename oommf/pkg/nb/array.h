/* FILE: array.h                 -*-Mode: c++-*-
 *
 * Array 2D and 3D  C++ template declarations
 *
 * Note: Array2D and Array3D were developed separately, and as a
 *       result don't have identical API's.  It is expected that
 *       new development (>July 1998) will be concerned with only
 *       3D structures, so Array2D can be considered effectively
 *       deprecated.  If this turns out to not be the case, then
 *       modifications should be made to bring them together.
 *
 * Note 2: Compare also against the more recent ArrayWrapper classes.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/07/30 01:18:45 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_ARRAY
#define _NB_ARRAY

#include "errhandlers.h"

/* End includes */     /* Optional directive to build.tcl */

#ifndef DEBUGLVL
# ifndef NDEBUG
#  define DEBUGLVL 11
# else
#  define DEBUGLVL 0
# endif
#endif

// #undef DEBUGLVL
// #define DEBUGLVL 11

/////////////////// Array2D Template Declaration ////////////////////
template<class T> class Nb_Array2D {
private:
  OC_INDEX dim1,dim2;  // I would *like* these private
  T *data,**row;  // I would *like* these private
public:
  Nb_Array2D() { dim1=dim2=0; data=(T*)NULL; row=(T**)NULL; }
  Nb_Array2D(Nb_Array2D<T> &src); // Copy constructor
  ~Nb_Array2D() { Deallocate(); }
  void Allocate(OC_INDEX newdim1,OC_INDEX newdim2);
  void Deallocate();
  typedef T* T_ptr; // Some compilers aren't too smart
  typedef const T* cT_ptr; // Some compilers aren't too smart
  T_ptr operator[](OC_INDEX index1);  // Returns pointer to row (or column,
  //  depending on how you think about these things. ;-)
  cT_ptr operator[](OC_INDEX index1) const;
  void Swap(Nb_Array2D<T> &other); // Interchanges *this & other
  OC_INDEX GetDim1() { return dim1; }
  OC_INDEX GetDim2() { return dim2; }
  OC_INDEX GetSize(OC_INDEX &d1,OC_INDEX &d2) { d1=dim1; d2=dim2; return d1*d2; }
  OC_INDEX GetSize() { return dim1*dim2; }
  typedef T** T_ptr_ptr; // Some compilers aren't too smart
  operator T_ptr_ptr() { return row; }
};

template<class T> Nb_Array2D<T>::Nb_Array2D(Nb_Array2D<T> &src)
{ // Copy constructor
  dim1=dim2=0; data=(T*)NULL; row=(T**)NULL;
  Allocate(src.dim1,src.dim2);
  OC_INDEX i,j;
  T *tp,*tsrcp;
  for(i=0,tp=data,tsrcp=src.data;i<dim1;i++) {
    for(j=0;j<dim2;j++) *(tp++)=*(tsrcp++); // Copy data
  }
}

template<class T> void Nb_Array2D<T>::Allocate(OC_INDEX newdim1,OC_INDEX newdim2)
{ // IMPORTANT NOTE: This routine allocates a one dimensional array,
  // and then sets up pointers into that array.
  OC_INDEX i;
  if(data!=(T*)NULL) Deallocate();
  dim1=newdim1; dim2=newdim2;
  // Overflow check
  if(dim1<1 || dim2<1 || (OC_INDEX_MAX/dim1)<dim2) {
    PlainError(1,"Error in Nb_Array2D::Allocate: %s",ErrBadParam);
  }

  // Allocate one big chunk of memory for the T's
  if((data=new T[dim1*dim2])==0) {
    PlainError(1,"Error in Nb_Array2D::Allocate: %s",ErrNoMem);
  }
  // Allocate row pointers
  if((row=new T*[dim1])==0) {
    PlainError(1,"Error in Nb_Array2D::Allocate: %s",ErrNoMem);
  }
  // Initialize row pointers
  for(i=0;i<dim1;i++) row[i]=data+i*dim2;
}

template<class T> void Nb_Array2D<T>::Deallocate(void)
{
  if(data!=(T*)NULL && dim1>0 && dim2>0) {
    delete[] row;
    delete[] data;
  }
  data=(T*)NULL; row=(T**)NULL;
  dim1=dim2=0;
}

template<class T> T* Nb_Array2D<T>::operator[](OC_INDEX index1)
{
#if DEBUGLVL >= 10
  if(index1<0 || index1>dim1)
    PlainError(1,"Error in Nb_Array2D::operator[]: %s",ErrArrOutOfBounds);
#endif // DEBUG_LVL
  return(row[index1]);
}

template<class T> const T* Nb_Array2D<T>::operator[](OC_INDEX index1) const
{
#if DEBUGLVL >= 10
  if(index1<0 || index1>dim1)
    PlainError(1,"Error in Nb_Array2D::operator[]: %s",ErrArrOutOfBounds);
#endif // DEBUGLVL
  return(row[index1]);
}

template<class T> void Nb_Array2D<T>::Swap(Nb_Array2D<T> &other)
{ // Interchanges *this & other
  OC_INDEX id1,id2;
  T *tptr,**tpptr;
  id1=dim1;   dim1=other.dim1; other.dim1=id1;
  id2=dim2;   dim2=other.dim2; other.dim2=id2;
  tptr=data;  data=other.data; other.data=tptr;
  tpptr=row;  row=other.row;   other.row=tpptr;
}

//////////////////////////////////////////////////////////////////////////
// 3D array template
//
// Note 1: Due to limitations on current versions of gcc, the class_doc
//  variable is an instance variable rather than a class static variable.
//  If a need arises to have a large number of 3D arrays, then one should
//  consider removing the class_doc variable, or find some other work
//  around.
// Note 2: The present implementation structures the 3D array as an array
//  of pointers to (plane) pointers to (row) pointers.  This double
//  indirection may be slower than computing the offset directly, depending
//  on the machine, compiler and cache.  This is not the case on Linux/axp
//  with gcc 2.7.2.1.  We might revisit this again later.
//    -mjd 2-Aug-1998
template<class T> class Nb_Array3D {
private:
  T ***arr;
  OC_INDEX dim1,dim2,dim3;
  const ClassDoc class_doc; // Static data member templates not implemented
                            // by gnu C++ compiler.

  // The following function is left undefined on purpose
  Nb_Array3D<T>& operator=(const Nb_Array3D<T>&); // Assignment op

public:
  OC_INDEX Copy(const Nb_Array3D<T> &src);
  Nb_Array3D();
  Nb_Array3D(const Nb_Array3D<T> &src); // Copy constructor
  ~Nb_Array3D() { Deallocate(); }
  OC_INDEX Allocate(OC_INDEX newdim1,OC_INDEX newdim2,OC_INDEX newdim3);
  void Deallocate();
  void Swap(Nb_Array3D<T> &other); // Interchanges *this & other
  OC_INDEX GetDim1() const { return dim1; }
  OC_INDEX GetDim2() const { return dim2; }
  OC_INDEX GetDim3() const { return dim3; }
  OC_INDEX GetSize(OC_INDEX &d1,OC_INDEX &d2,OC_INDEX &d3) const
     { d1=dim1; d2=dim2; d3=dim3; return d1*d2*d3; }
  OC_INDEX GetSize() const { return dim1*dim2*dim3; }
  inline T& operator()(OC_INDEX i1,OC_INDEX i2,OC_INDEX i3);
  inline const T& operator()(OC_INDEX i1,OC_INDEX i2,OC_INDEX i3) const;
  OC_BOOL IsSameArray(const Nb_Array3D<T>& other) const {
    return (arr == other.arr);
  }
};

template<class T> Nb_Array3D<T>::Nb_Array3D() :
  class_doc("Nb_Array3D","Michael J. Donahue (michael.donahue@nist.gov)",
	    "1.0.0","12-Dec-1996")
     // Static data member templates not implemented by gnu C++ v2.7.1.
{
  dim1=dim2=dim3=0; arr=(T***)NULL;
}

template<class T> Nb_Array3D<T>::Nb_Array3D(const Nb_Array3D<T> &src) :
  class_doc("Nb_Array3D","Michael J. Donahue (michael.donahue@nist.gov)",
	    "1.0.0","12-Dec-1996")
     // Static data member templates not implemented by gnu C++ v2.7.1.
{ // Copy constructor
#define MEMBERNAME "Nb_Array3D(Nb_Array3D<T> &)"
  dim1=dim2=dim3=0; arr=(T***)NULL;

  Copy(src);
#undef MEMBERNAME
}

template<class T> OC_INDEX Nb_Array3D<T>::Copy(const Nb_Array3D<T> &src)
{
#define MEMBERNAME "Copy(Nb_Array3D<T> &)"
  if(&src==this) return GetSize(); // Nothing to do!
  Allocate(src.dim1,src.dim2,src.dim3);

  // Copy data
  OC_INDEX total_size=GetSize();
  T *srcdata=&(src.arr[0][0][0]);
  T *destdata=&(arr[0][0][0]);
  for(OC_INDEX i=0;i<total_size;i++) destdata[i]=srcdata[i];
  return total_size;
#undef MEMBERNAME
}

template<class T> void Nb_Array3D<T>::Deallocate(void)
{
#define MEMBERNAME "Deallocate"
  if(arr!=(T***)NULL) {
    delete[] arr[0][0];
    delete[] arr[0];
    delete[] arr;
  }
  arr=(T***)NULL;
  dim1=dim2=dim3=0;
#undef MEMBERNAME
}

template<class T>
OC_INDEX Nb_Array3D<T>::Allocate(OC_INDEX newdim1,OC_INDEX newdim2,
                                 OC_INDEX newdim3)
{ // Insufficient memory is currently a fatal error.
  //  This may change (but requires careful deallocation). -mjd 12/Dec/96
#define MEMBERNAME "Allocate"
  // Check parameters, deallocate old memory and/or handle special cases.
  if(newdim1<0 || newdim2<0 || newdim3<0) FatalError(-1,STDDOC,ErrBadParam);
  if(dim1==newdim1 && dim2==newdim2 && dim3==newdim3) return GetSize();
  Deallocate();
  if(newdim1==0 || newdim2==0 || newdim3==0) return 0;

  OC_INDEX i1,i2,total_size,check;
  check = OC_INDEX_MAX/newdim1;
  check /= newdim2;
  if(check < newdim3) FatalError(-1,STDDOC,ErrOverflow); // Overflow check
  total_size=newdim1*newdim2*newdim3;

  // Allocate memory for plane pointers
  if((arr=new T**[size_t(newdim1)])==0) {
    FatalError(-1,STDDOC,ErrNoMem);
  }

  // Allocate memory for row pointers & initialize plane pointers
  if((arr[0]=new T*[size_t(newdim1*newdim2)])==0) {
    FatalError(-1,STDDOC,ErrNoMem);
  }
  for(i1=1;i1<newdim1;i1++) arr[i1]=arr[0]+i1*newdim2;

  // Allocate memory for individual cells & initialize row pointers
  if((arr[0][0]=new T[size_t(total_size)])==0) {
    FatalError(-1,STDDOC,ErrNoMem);
  }
  for(i1=0;i1<newdim1;i1++) for(i2=0;i2<newdim2;i2++) {
    arr[i1][i2]=arr[0][0]+(i1*newdim2+i2)*newdim3;
  }

  dim1=newdim1; dim2=newdim2; dim3=newdim3;
  return dim1*dim2*dim3;  // Note: For some reason gcc 2.8+
  /// complains if total_size is used here.  It appears to be concerned
  /// that the last "new" call may be a longjmp entry point and total_size
  /// will be undefined or lost.
#undef MEMBERNAME
}

template<class T> void Nb_Array3D<T>::Swap(Nb_Array3D<T> &other)
{
#define MEMBERNAME "Swap"
  OC_INDEX tdim1,tdim2,tdim3; T*** tarr;
  tdim1=dim1;       tdim2=dim2;       tdim3=dim3;       tarr=arr;
  dim1=other.dim1;  dim2=other.dim2;  dim3=other.dim3;  arr=other.arr;
  other.dim1=tdim1; other.dim2=tdim2; other.dim3=tdim3; other.arr=tarr;
#undef MEMBERNAME
}

template<class T> T&
Nb_Array3D<T>::operator()(OC_INDEX i1,OC_INDEX i2,OC_INDEX i3)
{
#define MEMBERNAME "operator()(OC_INDEX i1,OC_INDEX i2,OC_INDEX i3)"
#if DEBUGLVL >= 10
  if(i1<0 || i1>=dim1 || i2<0 || i2>=dim2 || i3<0 || i3>=dim3)
    FatalError(-1,STDDOC,ErrArrOutOfBounds);
#endif
  return arr[i1][i2][i3];
#undef MEMBERNAME
}

template<class T> const T&
Nb_Array3D<T>::operator()(OC_INDEX i1,OC_INDEX i2,OC_INDEX i3) const
{
#define MEMBERNAME "operator() const (OC_INDEX i1,OC_INDEX i2,OC_INDEX i3)"
#if DEBUGLVL >= 10
  if(i1<0 || i1>=dim1 || i2<0 || i2>=dim2 || i3<0 || i3>=dim3)
    FatalError(-1,STDDOC,ErrArrOutOfBounds);
#endif
  return arr[i1][i2][i3];
#undef MEMBERNAME
}

#endif // _NB_ARRAY
