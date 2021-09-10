/* FILE: oxsarray.h              -*-Mode: c++-*-
 */

#ifndef _OXS_OXSARRAY
#define _OXS_OXSARRAY

#include "oxsexcept.h"
#include "oxsthread.h"

////////////////////////////////////////////////////////////////////////
// 3D array wrapper about an Oxs_StripedArray.  Compare to
// Nb_ArrayWrapper.  This class indexes so that fast loops access
// index (i,j,k) by varying i in the inner loop, k in the outer loop.
template<class T> class Oxs_3DArray
{
private:
  OC_INDEX dim1,dim2,dim3;
  OC_INDEX dim12;  // = dim1*dim2

  Oxs_StripedArray<T> objs;

  // Disable copy operations by declaring the following
  // without providing definitions.
  Oxs_3DArray(const Oxs_3DArray<T>&);
  Oxs_3DArray<T>& operator=(const Oxs_3DArray<T>&);

public:
  void Free() {
    objs.Free();
    dim1 = dim2 = dim3 = dim12 = 0;
  }

  void SetDimensions(OC_INDEX new_dim1,OC_INDEX new_dim2,OC_INDEX new_dim3) {
    if(new_dim1<0 || new_dim2<0 || new_dim3<0) {
      OXS_THROW(Oxs_BadParameter,"Negative dimension request.");
    }
    dim1 = new_dim1;  dim2 = new_dim2;  dim3 = new_dim3;
    dim12 = dim1*dim2;
    objs.SetSize(dim12*dim3);
  }

  OC_INDEX GetDim1() const { return dim1; }
  OC_INDEX GetDim2() const { return dim2; }
  OC_INDEX GetDim3() const { return dim3; }

 Oxs_3DArray(OC_INDEX idim1=0,OC_INDEX idim2=0,OC_INDEX idim3=0)
   : dim1(0), dim2(0), dim3(0), dim12(0) {
    SetDimensions(idim1,idim2,idim3);
  }

  ~Oxs_3DArray() { Free(); }

#ifdef NDEBUG
  T& operator()(OC_INDEX i,OC_INDEX j,OC_INDEX k) {
    return objs[dim12*k+j*dim1+i];
  }
  const T& operator()(OC_INDEX i,OC_INDEX j,OC_INDEX k) const {
    return objs[dim12*k+j*dim1+i];
  }
  /// NB: No range check!
#else
  T& operator()(OC_INDEX i,OC_INDEX j,OC_INDEX k) {
    if(i<0 || i>=dim1 || j<0 || j>=dim2  || k<0 || k>=dim3 ) {
      OXS_THROW(Oxs_BadIndex,"Array index out-of-bounds.");
    }
    return objs[dim12*k+j*dim1+i];
  }
  const T& operator()(OC_INDEX i,OC_INDEX j,OC_INDEX k) const {
    if(i<0 || i>=dim1 || j<0 || j>=dim2  || k<0 || k>=dim3 ) {
      OXS_THROW(Oxs_BadIndex,"Array index out-of-bounds.");
    }
    return objs[dim12*k+j*dim1+i];
  }
  /// Ranged checked versions.
#endif

  // Use pointer access with care.  There is no bounds checking and the
  // client is responsible for insuring proper indexing.
  T* GetArrBase() const { return objs.GetArrBase(); }

};

#endif // _OXS_OXSARRAY
