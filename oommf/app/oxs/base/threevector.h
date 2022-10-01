/* FILE: threevector.h        -*-Mode: c++-*-
 *
 * Simple 3D vector class.  *All* client code should use the
 * ThreeVector typedef in case this class gets moved into an
 * OOMMF extension library.
 *
 */

#ifndef _OXS_THREEVECTOR
#define _OXS_THREEVECTOR

#if OC_USE_SSE
# include <emmintrin.h>
# ifndef OC_SSE_NO_ALIGNED
#  error OOMMF header error: OC_SSE_NO_ALIGNED not defined
# endif
#endif

#include <cassert>

#include "oc.h"

OC_USE_STD_NAMESPACE;  // For std::fabs()

/* End includes */

class Oxs_ThreeVector;  // Forward declaration for typedef
typedef Oxs_ThreeVector ThreeVector;

class Oxs_ThreeVector {
private:
  // C++11 provides std::fma() that computes fused-multiply-add with
  // correct (i.e., single) rounding. This is very useful for building
  // numerically finicky code like double-double multiplication
  // (cf. oommf/pkg/xp/doubledouble.cc), but can be painfully slow if
  // the compiler target doesn't have hardware support for fma. If
  // precision is not an issue, then you'll likely get faster binaries
  // by simply expanding out fma(a,b,c) as a*b+c and letting the
  // compiler decide when and where to insert fma's in the executable.
  //   The following inline function is a transparent wrapper around an
  // expanded fma. It is provided and used below to make it easy to
  // compare the speed vs. precission trade-offs of using the expanded
  // form vs. std::fma; just change the "a*b+c" to std::fma(a,b,c) to
  // test.
  inline static OC_REAL8m FMA_Block(OC_REAL8m a,OC_REAL8m b,OC_REAL8m c) {
    return a*b+c;
  }
  // Friend access for FMA_Block
  friend const Oxs_ThreeVector operator^(const Oxs_ThreeVector& v,
                                         const Oxs_ThreeVector& w);
  friend OC_REAL8m operator*(const Oxs_ThreeVector& lhs,
                             const Oxs_ThreeVector& rhs);
public:
  OC_REAL8m x,y,z;

  // Note: Using default copy constructor and copy assignment operators
  // in the class allows Oxs_MeshValue<T>::operator=(const
  // Oxs_MeshValue<T>& other) to use memcpy to copy objects.

  // Constructors
  Oxs_ThreeVector(): x(0.), y(0.), z(0.) {}
  Oxs_ThreeVector(OC_REAL8m xi,OC_REAL8m yi,OC_REAL8m zi)
    : x(xi), y(yi), z(zi) {}
  Oxs_ThreeVector(OC_REAL8m *arr) : x(arr[0]), y(arr[1]), z(arr[2]) {}
  // Note default copy constructor

  // Assignment operators (note default copy assignment operator)
  Oxs_ThreeVector& Set(OC_REAL8m xi,OC_REAL8m yi,OC_REAL8m zi)
    { x=xi; y=yi; z=zi; return *this; }
  Oxs_ThreeVector& operator+=(const Oxs_ThreeVector& v) { 
    x+=v.x; y+=v.y; z+=v.z;
    return *this;
  }
  Oxs_ThreeVector& operator-=(const Oxs_ThreeVector& v) {
    x-=v.x; y-=v.y; z-=v.z;
    return *this;
  }

  Oxs_ThreeVector& operator^=(const Oxs_ThreeVector& w) { // Cross product
#if 0
    OC_REAL8m tx = y * w.z  -  z * w.y;
    OC_REAL8m ty = z * w.x  -  x * w.z;
    OC_REAL8m tz = x * w.y  -  y * w.x;
    x = tx; y = ty; z = tz;
#else
    OC_REAL8m tx = FMA_Block(y,w.z,-z*w.y);
    OC_REAL8m ty = FMA_Block(z,w.x,-x*w.z);
    OC_REAL8m tz = FMA_Block(x,w.y,-y*w.x);
    x = tx; y = ty; z = tz;
#endif
    return *this;
  }

  Oxs_ThreeVector& operator*=(OC_REAL8m a) { x*=a; y*=a; z*=a; return *this; }

#ifdef OC_NO_PARTIAL_TEMPLATE_INSTANTIATION
  // The Oxs_MeshValue template has operator *= and /= member
  // functions.  These are not used with template type
  // Oxs_ThreeVector, but implement a workaround for compilers that
  // don't support partial template class instantiation.
  Oxs_ThreeVector& operator*=(const Oxs_ThreeVector&) {
    throw "Invalid Oxs_ThreeVector operation: *=";
    return *this;
  }
  Oxs_ThreeVector& operator/=(const Oxs_ThreeVector&) {
    throw "Invalid Oxs_ThreeVector operation: /=";
    return *this;
  }
#endif


  Oxs_ThreeVector& Accum(OC_REAL8m a,const Oxs_ThreeVector& v) {
    x = FMA_Block(a,v.x,x);
    y = FMA_Block(a,v.y,y);
    z = FMA_Block(a,v.z,z);
    return *this;
  }

  Oxs_ThreeVector& wxvxw(const Oxs_ThreeVector& w) {
    // Performs w x *this x w
    OC_REAL8m tx = FMA_Block(y,w.z,-z*w.y);
    OC_REAL8m ty = FMA_Block(z,w.x,-x*w.z);
    z = FMA_Block(ty,w.x,-tx*w.y);
    OC_REAL8m tz = FMA_Block(x,w.y,-y*w.x);
    x = FMA_Block(tz, w.y,-ty*w.z);
    y = FMA_Block(tx, w.z,-tz*w.x);
    return *this;
  }

  // Misc
  OC_REAL8m Mag() const { return Oc_Hypot(x,y,z); }
  OC_REAL8m MagSq() const {
    return FMA_Block(x,x,FMA_Block(y,y,z*z));
  }
  OC_REAL8m TaxicabNorm() const { return fabs(x)+fabs(y)+fabs(z); }  // aka L1-norm
  void SetMag(OC_REAL8m mag);  // Adjusts size to "mag"
  void Random(OC_REAL8m mag);  // Makes a random vector of size "mag"
  void PerturbDirection(OC_REAL8m eps); // Perturb the direction of
  /// *this by a random amount.  The magnitude of the perturbation
  /// is no more than eps (>=0).  The norm of *this is unchanged.
  OC_REAL8m MakeUnit(); // High-precision version of SetMag(1.0).
  /// Return value is original MagSq(), i.e., on entry.
};

// Test operators on Oxs_ThreeVector's
inline OC_BOOL operator==(const Oxs_ThreeVector& lhs,
		const Oxs_ThreeVector& rhs)
{ return ((lhs.x==rhs.x) && (lhs.y==rhs.y) && (lhs.z==rhs.z)); }

inline OC_BOOL operator!=(const Oxs_ThreeVector& lhs,
		const Oxs_ThreeVector& rhs)
{ return ((lhs.x!=rhs.x) || (lhs.y!=rhs.y) || (lhs.z!=rhs.z)); }

// The next two operators are defined so MSVC++ 5.0 will accept
// vector<ThreeVector>, but are left undefined because there
// is no way to define them that makes sense in all contexts.
OC_BOOL operator<(const Oxs_ThreeVector&,const Oxs_ThreeVector&);
OC_BOOL operator>(const Oxs_ThreeVector&,const Oxs_ThreeVector&);


// Binary operators on Oxs_ThreeVector's
inline const Oxs_ThreeVector
operator+(const Oxs_ThreeVector& lhs,
	  const Oxs_ThreeVector& rhs)
{ Oxs_ThreeVector result(lhs); return result+=rhs; }

inline const Oxs_ThreeVector
operator-(const Oxs_ThreeVector& lhs,
	  const Oxs_ThreeVector& rhs)
{ Oxs_ThreeVector result(lhs); return result-=rhs; }

// Cross product
inline const Oxs_ThreeVector
operator^(const Oxs_ThreeVector& v,
	  const Oxs_ThreeVector& w)
{
#if 0
  OC_REAL8m tx = v.y * w.z  -  w.y * v.z;
  OC_REAL8m ty = w.x * v.z  -  v.x * w.z;
  OC_REAL8m tz = v.x * w.y  -  w.x * v.y;
#else
  OC_REAL8m tx = Oxs_ThreeVector::FMA_Block(v.y,w.z,-w.y*v.z);
  OC_REAL8m ty = Oxs_ThreeVector::FMA_Block(w.x,v.z,-v.x*w.z);
  OC_REAL8m tz = Oxs_ThreeVector::FMA_Block(v.x,w.y,-w.x*v.y);
#endif
  return Oxs_ThreeVector(tx,ty,tz);
}


// Product against scalar
inline const Oxs_ThreeVector
operator*(OC_REAL8m scalar,
	  const Oxs_ThreeVector& vec)
{ Oxs_ThreeVector result(vec); return result*=scalar; }

inline const Oxs_ThreeVector
operator*(const Oxs_ThreeVector& vec,
	  OC_REAL8m scalar)
{ Oxs_ThreeVector result(vec); return result*=scalar; }


// Dot product
inline OC_REAL8m
operator*(const Oxs_ThreeVector& lhs,const Oxs_ThreeVector& rhs)
#if 0
{ return lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z; }
#else
{ return Oxs_ThreeVector::FMA_Block(lhs.x,rhs.x,
         Oxs_ThreeVector::FMA_Block(lhs.y,rhs.y,lhs.z*rhs.z)); }
#endif

////////////////////////////////////////////////////////////////////////
// "Pair" routines, that process two vectors at a time.  If SSE is
// available, then these versions can be significantly faster than the
// serial routines.
#if OC_USE_SSE
inline __m128d Oxs_ThreeVectorPairMagSq
(const __m128d vx,const __m128d vy,const __m128d vz)
{ // Returns |vec0|^2 in in lower half, |vec1|^2 in upper half.
  __m128d tzsq = _mm_mul_pd(vz,vz);
#if OC_FMA_TYPE == 3
  return _mm_fmadd_pd(vx,vx,_mm_fmadd_pd(vy,vy,tzsq));
#elif OC_FMA_TYPE == 4
  return _mm_macc_pd(vx,vx,_mm_macc_pd(vy,vy,tzsq));
#else
  __m128d txsq = _mm_mul_pd(vx,vx);
  __m128d tysq = _mm_mul_pd(vy,vy);
  return _mm_add_pd(_mm_add_pd(txsq,tysq),tzsq);
#endif
}

inline __m128d Oxs_ThreeVectorPairMagSq
(const Oxs_ThreeVector& vec0,
 const Oxs_ThreeVector& vec1)
{ // Returns |vec0|^2 in in lower half, |vec1|^2 in upper half.
  return Oxs_ThreeVectorPairMagSq(_mm_set_pd(vec1.x,vec0.x),
                                  _mm_set_pd(vec1.y,vec0.y),
                                  _mm_set_pd(vec1.z,vec0.z));
}
#endif // OC_USE_SSE

inline void Oxs_ThreeVectorPairMagSq
(const Oxs_ThreeVector& vec0,
 const Oxs_ThreeVector& vec1,
 OC_REAL8m& magsq0, OC_REAL8m& magsq1)
{ // Returns |vec0|^2 in mag0, and |vec1|^2 in mag1
#if !OC_USE_SSE
  magsq0 = vec0.x*vec0.x + vec0.y*vec0.y + vec0.z*vec0.z;
  magsq1 = vec1.x*vec1.x + vec1.y*vec1.y + vec1.z*vec1.z;
#else
  __m128d tmagsq = Oxs_ThreeVectorPairMagSq(vec0,vec1);
  _mm_store_sd(&magsq0,tmagsq);
  _mm_storeh_pd(&magsq1,tmagsq);
#endif
}

#if OC_USE_SSE
void Oxs_ThreeVectorPairMakeUnit
(__m128d& tx,__m128d& ty,__m128d& tz,
 OC_REAL8m* magsq0=0, OC_REAL8m* magsq1=0);
// Here imports/exports tx,ty,tz are packed double-precision
// components of two three vectors,
//  tx = (t0.x,t1.x), ty = (t0.y,t1.y), tz = (t0.z,t1.z)
// where t0 is placed in the lower-half of the __m128d variables.
// Code for this routine is in the file threevector.cc.

inline void Oxs_ThreeVectorPairMakeUnit
(__m128d tx,__m128d ty,__m128d tz,
 Oxs_ThreeVector& vec0,
 Oxs_ThreeVector& vec1,
 OC_REAL8m* magsq0=0, OC_REAL8m* magsq1=0)
{ // Here imports tx,ty,tz are packed double-precision components of two
  // three vectors,
  //  tx = (t0.x,t1.x), ty = (t0.y,t1.y), tz = (t0.z,t1.z)
  // where t0 is placed in the lower-half of the __m128d variables.

  Oxs_ThreeVectorPairMakeUnit(tx,ty,tz,magsq0,magsq1);

  _mm_store_sd(&vec0.x,tx);
  _mm_store_sd(&vec0.y,ty);
  _mm_store_sd(&vec0.z,tz);

  _mm_storeh_pd(&vec1.x,tx);
  _mm_storeh_pd(&vec1.y,ty);
  _mm_storeh_pd(&vec1.z,tz);
}
#endif

void Oxs_ThreeVectorPairMakeUnit
(Oxs_ThreeVector& vec0,
 Oxs_ThreeVector& vec1,
 OC_REAL8m* magsq0=0, OC_REAL8m* magsq1=0);
// Parameters vec0 and vec1 are both imports and exports.
// Code for this routine is in the file threevector.cc.

#if !OC_USE_SSE
void Oxs_ThreeVectorPairMakeUnit
(Oc_Duet& tx,Oc_Duet& ty,Oc_Duet& tz,
 OC_REAL8m* magsq0=0, OC_REAL8m* magsq1=0);
/// Non-SSE implementation in threevector.cc
#else // OC_USE_SSE
inline void Oxs_ThreeVectorPairMakeUnit
(Oc_Duet& tx,Oc_Duet& ty,Oc_Duet& tz,
 OC_REAL8m* magsq0=0, OC_REAL8m* magsq1=0)
{ // Here imports tx,ty,tz are packed double-precision components of two
  // three vectors,
  //  tx = (t0.x,t1.x), ty = (t0.y,t1.y), tz = (t0.z,t1.z)
  // where t0 is placed in the lower-half of the __m128d variables.
  Oxs_ThreeVectorPairMakeUnit(tx.GetSSERef(),
                              ty.GetSSERef(),
                              tz.GetSSERef(),magsq0,magsq1);
}
#endif // OC_USE_SSE

#if !OC_USE_SSE
inline void Oxs_ThreeVectorPairLoadAligned
(const Oxs_ThreeVector* vec,
 Oc_Duet& vx,Oc_Duet& vy,Oc_Duet& vz) {
  // Loads pair of vectors vec[0], vec[1] into doublets vx, vy, vz.
  // Note: Non-SSE version doesn't actually care if vec[0] is
  //       16-bit aligned or not.
  vx.Set(vec[0].x,vec[1].x);
  vy.Set(vec[0].y,vec[1].y);
  vz.Set(vec[0].z,vec[1].z);
}
inline void Oxs_ThreeVectorPairStreamAligned
(const Oc_Duet& vx,const Oc_Duet& vy,const Oc_Duet& vz,
 Oxs_ThreeVector* vec) {
  // Writes doublets vx, vy, vz into vec[0], vec[1].
  // Note: Non-SSE version doesn't actually care if vec[0] is
  //       16-bit aligned or not, and also doesn't "stream"
  vx.StoreA(vec[0].x);  vy.StoreA(vec[0].y);  vz.StoreA(vec[0].z);
  vx.StoreB(vec[1].x);  vy.StoreB(vec[1].y);  vz.StoreB(vec[1].z);
}

inline void Oxs_ThreeVectorPairStoreAligned
(const Oc_Duet& vx,const Oc_Duet& vy,const Oc_Duet& vz,
 Oxs_ThreeVector* vec) {
  // Non-SSE version of this function same as "stream" call.
  Oxs_ThreeVectorPairStreamAligned(vx,vy,vz,vec);
}

inline void Oxs_ThreeVectorPairUnpack
(const Oc_Duet& tA,const Oc_Duet& tB,const Oc_Duet& tC,
 Oc_Duet& vx,Oc_Duet& vy,Oc_Duet& vz) {
  // NOTE: This code assumes tA, tB, tC are
  //       distinct from all of vx, vy, vz !!!
  // Takes packed vector imports tA, tB, tC where
  //    tA = (vec[0].x, vec[0].y)
  //    tB = (vec[0].z, vec[1].x)
  //    tC = (vec[1].y, vec[1].z)
  // and unpacks into exports vx, vy, vz so that
  //    vx = (vec[0].x, vec[1].x)
  //    vy = (vec[0].y, vec[1].y)
  //    vz = (vec[0].z, vec[1].z)
  vx.Set(tA.GetA(),tB.GetB());
  vy.Set(tA.GetB(),tC.GetA());
  vz.Set(tB.GetA(),tC.GetB());
}
inline void Oxs_ThreeVectorPairPack
(const Oc_Duet& vx,const Oc_Duet& vy,const Oc_Duet& vz,
 Oc_Duet& tA,Oc_Duet& tB,Oc_Duet& tC) {
  // Inverse of Oxs_ThreeVectorPairUnpack
  // NOTE: This code assumes tA, tB, tC are
  //       distinct from all of vx, vy, vz !!!
  tA.Set(vx.GetA(),vy.GetA());
  tB.Set(vz.GetA(),vx.GetB());
  tC.Set(vy.GetB(),vz.GetB());
}
#else // OC_USE_SSE
inline void Oxs_ThreeVectorPairLoadAligned
(const Oxs_ThreeVector* vec,
 Oc_Duet& vx,Oc_Duet& vy,Oc_Duet& vz) {
  // Loads pair of vectors vec[0], vec[1] into doublets vx, vy, vz.
  // Code assumes vec[0] is 16-bit aligned.
  const double *dptr = &(vec[0].x);
#if !OC_SSE_NO_ALIGNED
  assert(size_t(dptr) % 16 == 0);
  __m128d tA = _mm_load_pd(dptr);   // vec[0].x, vec[0].y
  __m128d tB = _mm_load_pd(dptr+2); // vec[0].z, vec[1].x
  __m128d tC = _mm_load_pd(dptr+4); // vec[1].y, vec[1].z
#else
  __m128d tA = _mm_loadu_pd(dptr);   // vec[0].x, vec[0].y
  __m128d tB = _mm_loadu_pd(dptr+2); // vec[0].z, vec[1].x
  __m128d tC = _mm_loadu_pd(dptr+4); // vec[1].y, vec[1].z
#endif
  vx = _mm_shuffle_pd(tA,tB,2);
  vy = _mm_shuffle_pd(tA,tC,1);
  vz = _mm_shuffle_pd(tB,tC,2);
}
inline void Oxs_ThreeVectorPairStreamAligned
(const Oc_Duet& vx,const Oc_Duet& vy,const Oc_Duet& vz,
 Oxs_ThreeVector* vec) {
  // Writes doublets vx, vy, vz into vec[0], vec[1], via SSE instruction
  // _mm_stream_pd.  This instruction writes straight to main memory
  // bypassing cache.  Note that this will result in *much* slower code
  // if the output vec[0], vec[1] address is in the cache, or will soon
  // be needed in cache.
  // NB: This code assumes vec[0] is 16-bit aligned.
  double* dptr = (double*)vec;
  assert(size_t(dptr) % 16 == 0);
#if !OC_SSE_NO_ALIGNED
  _mm_stream_pd(dptr,   _mm_unpacklo_pd(vx.GetSSERef(),vy.GetSSERef()));
  _mm_stream_pd(dptr+2, _mm_shuffle_pd( vz.GetSSERef(),vx.GetSSERef(),2));
  _mm_stream_pd(dptr+4, _mm_unpackhi_pd(vy.GetSSERef(),vz.GetSSERef()));
#else
  _mm_storeu_pd(dptr,   _mm_unpacklo_pd(vx.GetSSERef(),vy.GetSSERef()));
  _mm_storeu_pd(dptr+2, _mm_shuffle_pd( vz.GetSSERef(),vx.GetSSERef(),2));
  _mm_storeu_pd(dptr+4, _mm_unpackhi_pd(vy.GetSSERef(),vz.GetSSERef()));
#endif
}
inline void Oxs_ThreeVectorPairStoreAligned
(const Oc_Duet& vx,const Oc_Duet& vy,const Oc_Duet& vz,
 Oxs_ThreeVector* vec) {
  // Same as Oxs_ThreeVectorPairStreamAligned, except uses _mm_store_pd
  // instead of _mm_stream_pd.  _mm_store_pd does not bypass the cache,
  // and is significantly faster if the store address is either already
  // in the cache or will soon be so.
  // NB: This code assumes vec[0] is 16-bit aligned.
  double* dptr = (double*)vec;
  assert(size_t(dptr) % 16 == 0);
#if !OC_SSE_NO_ALIGNED
  _mm_store_pd(dptr,   _mm_unpacklo_pd(vx.GetSSERef(),vy.GetSSERef()));
  _mm_store_pd(dptr+2, _mm_shuffle_pd( vz.GetSSERef(),vx.GetSSERef(),2));
  _mm_store_pd(dptr+4, _mm_unpackhi_pd(vy.GetSSERef(),vz.GetSSERef()));
#else
  _mm_storeu_pd(dptr,   _mm_unpacklo_pd(vx.GetSSERef(),vy.GetSSERef()));
  _mm_storeu_pd(dptr+2, _mm_shuffle_pd( vz.GetSSERef(),vx.GetSSERef(),2));
  _mm_storeu_pd(dptr+4, _mm_unpackhi_pd(vy.GetSSERef(),vz.GetSSERef()));
#endif
}

inline void Oxs_ThreeVectorPairUnpack
(const Oc_Duet& tA,const Oc_Duet& tB,const Oc_Duet& tC,
 Oc_Duet& vx,Oc_Duet& vy,Oc_Duet& vz) {
  // NOTE: This code assumes tA, tB, tC are
  //       distinct from all of vx, vy, vz !!!
  // Takes packed vector imports tA, tB, tC where
  //    tA = (vec[0].x, vec[0].y)
  //    tB = (vec[0].z, vec[1].x)
  //    tC = (vec[1].y, vec[1].z)
  // and unpacks into exports vx, vy, vz so that
  //    vx = (vec[0].x, vec[1].x)
  //    vy = (vec[0].y, vec[1].y)
  //    vz = (vec[0].z, vec[1].z)
  vx = _mm_shuffle_pd(tA.GetSSERef(),tB.GetSSERef(),2);
  vy = _mm_shuffle_pd(tA.GetSSERef(),tC.GetSSERef(),1);
  vz = _mm_shuffle_pd(tB.GetSSERef(),tC.GetSSERef(),2);
  // NOTE: If you don't care about the relative ordering of the x, y,
  //       and z components inside the output v Oc_Duets, then you can
  //       unpack with two operations: one unpacklo and one unpackhi.
  //       This is useful if for example v is only being used via dot
  //       products with other vectors that can similarly be
  //       pseudo-unpacked.
  //  For example:
  //
  //    vx = Oc_Duet(_mm_unpacklo_pd(tA.GetSSERef(),tC.GetSSERef()));
  //    vy = tB;
  //    vz = Oc_Duet(_mm_unpackhi_pd(tA.GetSSERef(),tC.GetSSERef()));
  //
  //  yields
  //
  //    vx = (vec[0].x,vec[1].y)
  //    vy = (vec[0].z,vec[1].x)
  //    vz = (vec[0].y,vec[1].z)
}
inline void Oxs_ThreeVectorPairPack
(const Oc_Duet& vx,const Oc_Duet& vy,const Oc_Duet& vz,
 Oc_Duet& tA,Oc_Duet& tB,Oc_Duet& tC) {
  // Inverse of Oxs_ThreeVectorPairUnpack
  // NOTE: This code assumes tA, tB, tC are
  //       distinct from all of vx, vy, vz !!!
  tA = _mm_unpacklo_pd(vx.GetSSERef(),vy.GetSSERef());
  tB = _mm_shuffle_pd( vz.GetSSERef(),vx.GetSSERef(),2);
  tC = _mm_unpackhi_pd(vy.GetSSERef(),vz.GetSSERef());
}
#endif // OC_USE_SSE

#endif // _OXS_THREEVECTOR
