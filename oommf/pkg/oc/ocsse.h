/* FILE: ocsse.h                   -*-Mode: c++-*-
 *
 *      Wrappers for SSE code.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/03/09 04:51:08 $
 * Last modified by: $Author: donahue $
 *
 */

#ifndef _OCSSE
#define _OCSSE

#include "ocport.h"

#if OC_USE_SSE

#include <emmintrin.h>
#ifndef OC_SSE_NO_ALIGNED
# error OOMMF header error: OC_SSE_NO_ALIGNED not defined
#endif

#if OC_FMA_TYPE
# ifdef _MSC_VER
#  include <intrin.h>
# else
#  include <x86intrin.h>
# endif
#endif

/* End includes */     /* Optional directive to pimake */

/*************************************************************************
 ***  WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING  ***
 ***                                                                   ***
 *** The SSE version of Oc_Duet has alignment restrictions that aren't ***
 *** supported inside function parameter lists when building 32-bit    ***
 *** binaries using Visual C++ (at least as of Feb 2012).  This means  ***
 *** that                                                              ***
 ***                                                                   ***
 ***      instances of Oc_Duet should NOT BE PASSED BY VALUE!!!        ***
 ***                                                                   ***
 *** So, ALWAYS pass Oc_Duet by reference or pointer.  Interestingly,  ***
 *** gcc can handle the alignment issues and pass Oc_Duet by value     ***
 *** just fine.  Also, not a problem with Visual C++ if making 64-bit  ***
 *** binaries.  I don't know about other compilers.  BTW, Visual       ***
 *** C++/32 will allow up to three bare __m128d objects to be passed   ***
 *** by value but not four.  This is because of Microsoft calling      ***
 *** conventions that pass up to three __m128d objects in the XMM      ***
 *** registers.                                                        ***
 ***                                                                   ***
 ***  WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING  ***
 *************************************************************************/

/*************************************************************************
 ***  WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING  ***
 ***                                                                   ***
 ***  Oc_Duet objects cannot be used in std::vector or other standard  ***
 ***  template library containers, because on 32-bit platforms the     ***
 ***  alignment of the start of the vector array is only guaranteed to ***
 ***  have 8-byte alignment (whereas __m128d require 16-byte           ***
 ***  alignment).  If you really need to do this, then AFAICT the only ***
 ***  solution is to implement a nonstandard allocator for std::vector ***
 ***  (and etc.)
 ***                                                                   ***
 ***  WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING  ***
 *************************************************************************/

// Prefetching
enum Oc_PrefetchDirective {
  Ocpd_NTA=_MM_HINT_NTA, Ocpd_T2=_MM_HINT_T2,
  Ocpd_T1=_MM_HINT_T1, Ocpd_T0=_MM_HINT_T0
  // The xmmintrin.h header values for these are
  //    _MM_HINT_NTA == 0, _MM_HINT_T2 == 1,
  //    _MM_HINT_T1 == 2,  _MM_HINT_T0 == 3
};

#if 0
template<Oc_PrefetchDirective hint> inline void Oc_Prefetch(const void* addr) {
  OC_THROW("Unrecognized Oc_PrefetchDirective request");
}
#else
// Undefined base template
template<Oc_PrefetchDirective hint> inline void Oc_Prefetch(const void* addr);
#endif
// Specializations.
// Note 1: Some compilers declare address type to be const void * (gcc),
//  while others declare the type as const char* (Intel, Visual C++).
//  The code below uses casts to dance about these differences.  (C++
//  specs allow any pointer to be cast to a void*, but strict
//  conformance requires explicit cast from a void* to another pointer
//  type.
// Note2 : I've tried making Oc_Prefetch an inline non-template
//  function, but many compilers, if building with optimizations turned
//  off, complain that the second argument to _mm_prefetch must be a
//  constant.
template<> inline void Oc_Prefetch<Ocpd_NTA>(const void* addr) {
  _mm_prefetch(static_cast<const char*>(addr),_MM_HINT_NTA);
}
template<> inline  void Oc_Prefetch<Ocpd_T2>(const void* addr) {
  _mm_prefetch(static_cast<const char*>(addr),_MM_HINT_T2);
}
template<> inline  void Oc_Prefetch<Ocpd_T1>(const void* addr) {
  _mm_prefetch(static_cast<const char*>(addr),_MM_HINT_T1);
}
template<> inline  void Oc_Prefetch<Ocpd_T0>(const void* addr) {
  _mm_prefetch(static_cast<const char*>(addr),_MM_HINT_T0);
}


#if OC_COMPILER_HAS_MM_CVTSD_F54
# define oc_sse_cvtsd_f64(a) _mm_cvtsd_f64(a)
#else 
inline OC_REAL8 oc_sse_cvtsd_f64(const __m128d& a) {
# if !defined(__PGIC__)
  OC_REAL8 osc_f64_;
  _mm_store_sd(&osc_f64_,(a));
  return osc_f64_;
# else
  // For some reason Portland Group C++ 16.10-0 chokes on the above
  // with an internal compiler error.  Use instead this workaround:
  return *(reinterpret_cast<const OC_REAL8*>(&a));
# endif
}
#endif

#if OC_COMPILER_HAS_BROKEN_MM_STOREL_PD
// Wrapper replacing SSE2 intrinsic _mm_storel_pd calls with the
// equivalent SSE2 intrinsic _mm_store_sd.  (_mm_storel_pd
// implementation is broken on some compilers.)  It is important that
// this define occurs after #include <emmintrin.h> --- otherwise we
// create a duplicate definition of _mm_store_sd.
# define _mm_storel_pd(x,y)  _mm_store_sd(x,y)
#endif


// Utility functions for extracting OC_REAL8 values from an __m128d object
inline OC_REAL8 Oc_SseGetLower(const __m128d& val)
{ return oc_sse_cvtsd_f64(val); }

inline OC_REAL8 Oc_SseGetUpper(const __m128d& val)
{ return oc_sse_cvtsd_f64(_mm_unpackhi_pd(val,val)); }
// One might want to do a speed comparison bettern Oc_SseGetUpper
// and code looking like 'return ((OC_REAL8*)(&val))[1];'


// Wrapper for SSE __m128d object
class Oc_Duet {
public:
  Oc_Duet& Set(OC_REAL8 a,OC_REAL8 b) { value = _mm_set_pd(b,a); return *this; }
  // Stores "a" in low (first) half of value, "b" in high (second) half.

  Oc_Duet& Set(OC_REAL8 x) { value = _mm_set1_pd(x); return *this; } // a=b=x

  Oc_Duet& LoadUnaligned(const OC_REAL8& pair) {
    value = _mm_loadu_pd(&pair);
    return *this;
  }

  Oc_Duet& LoadAligned(const OC_REAL8& pair) {
    // Address of "pair" must be 16-byte aligned.
#if !OC_SSE_NO_ALIGNED
    value = _mm_load_pd(&pair);
#else
    value = _mm_loadu_pd(&pair);
#endif
    return *this;
  }

  Oc_Duet(OC_REAL8 a,OC_REAL8 b) { Set(a,b); }
  Oc_Duet(OC_REAL8 x) { Set(x); }
  Oc_Duet(const __m128d& newval) : value(newval) {}
  Oc_Duet(const Oc_Duet& other) : value(other.value) {}
  Oc_Duet() {}

  Oc_Duet& operator=(const Oc_Duet &other)
  { value = other.value; return *this; }

  Oc_Duet& SetZero() { value = _mm_setzero_pd(); return *this; }


  Oc_Duet& operator+=(const Oc_Duet& other) {
    value = _mm_add_pd(value,other.value); return *this;
  }
  Oc_Duet& operator+=(const OC_REAL8& x) {
    value = _mm_add_pd(value,_mm_set1_pd(x)); return *this;
  }
  Oc_Duet& operator-=(const Oc_Duet& other) {
    value = _mm_sub_pd(value,other.value); return *this;
  }
  Oc_Duet& operator-=(const OC_REAL8& x) {
    value = _mm_sub_pd(value,_mm_set1_pd(x)); return *this;
  }
  Oc_Duet& operator*=(const Oc_Duet& other) {
    value = _mm_mul_pd(value,other.value); return *this;
  }
  Oc_Duet& operator*=(const OC_REAL8& x) {
    value = _mm_mul_pd(value,_mm_set1_pd(x)); return *this;
  }
  Oc_Duet& operator/=(const Oc_Duet& other) {
    value = _mm_div_pd(value,other.value); return *this;
  }
  Oc_Duet& operator/=(const OC_REAL8& x) {
    value = _mm_div_pd(value,_mm_set1_pd(x)); return *this;
  }

  Oc_Duet& MultLower(const OC_REAL8& x) {
    // Multiplies lower part by x.  Upper part is unchanged
    value = _mm_mul_sd(value,_mm_set_sd(x));
    return *this;
  }

  // Use GetA/GetB for temporary use, but StoreA/StoreB
  // are more efficient for permanent storage.
  OC_REAL8 GetA() const { return Oc_SseGetLower(value); }
  OC_REAL8 GetB() const { return Oc_SseGetUpper(value); }

  void StoreA(OC_REAL8& Aout) const { _mm_store_sd(&Aout,value); }
  void StoreB(OC_REAL8& Bout) const { _mm_storeh_pd(&Bout,value); }

  // Write out both values with one call.  Use StoreUnaligned if you
  // can't guarantee the address &pair is 16-bit aligned.  Use
  // StoreAligned if &pair is 16-bit aligned, and if it might possibly
  // be in the cache either now or in the near future.  Use StoreStream
  // if &pair is 16-bit aligned and you want to bypass the cache and
  // write the value directly to memory.  Note that StoreStream is
  // significantly slower than StoreAligned if the &pair is currently
  // cached.
  void StoreUnaligned(OC_REAL8& pair) const { _mm_storeu_pd(&pair,value); }
#if !OC_SSE_NO_ALIGNED
  void StoreAligned(OC_REAL8& pair) const { _mm_store_pd(&pair,value); }
  void StoreStream(OC_REAL8& pair) const { _mm_stream_pd(&pair,value); }
#else
  void StoreAligned(OC_REAL8& pair) const { _mm_storeu_pd(&pair,value); }
  void StoreStream(OC_REAL8& pair) const { _mm_storeu_pd(&pair,value); }
#endif

  void KeepMin(const Oc_Duet& tst) { value = _mm_min_pd(value,tst.value); }
  void KeepMax(const Oc_Duet& tst) { value = _mm_max_pd(value,tst.value); }

  // GetSSEValue() and GetSSERef are only available in the
  // OC_USE_SSE branch
  __m128d  GetSSEValue() const { return value; }
  __m128d& GetSSERef() { return value; }
  const __m128d& GetSSERef() const { return value; }

  friend const Oc_Duet operator+(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator+(OC_REAL8 x,const Oc_Duet& w);
  friend const Oc_Duet operator+(const Oc_Duet& w,OC_REAL8 x);

  friend const Oc_Duet operator-(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator-(OC_REAL8 x,const Oc_Duet& w);
  friend const Oc_Duet operator-(const Oc_Duet& w,OC_REAL8 x);

  friend const Oc_Duet operator*(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator*(OC_REAL8 x,const Oc_Duet& w);
  friend const Oc_Duet operator*(const Oc_Duet& w,OC_REAL8 x);

  friend const Oc_Duet operator/(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator/(OC_REAL8 x,const Oc_Duet& w);
  friend const Oc_Duet operator/(const Oc_Duet& w,OC_REAL8 x);

  friend const Oc_Duet Oc_FMA(const Oc_Duet& w1,const Oc_Duet& w2,
                              const Oc_Duet& w3); // w1*w2+w3

  friend const Oc_Duet sqrt(const Oc_Duet& w);
  friend const Oc_Duet recip(const Oc_Duet& w); // 1.0/w

  friend const Oc_Duet Oc_FlipDuet(const Oc_Duet& w); // flips components

private:
  __m128d value;
};

inline const Oc_Duet operator+(const Oc_Duet& w1,const Oc_Duet& w2)
{ return Oc_Duet(_mm_add_pd(w1.value,w2.value)); }
inline const Oc_Duet operator+(OC_REAL8 x,const Oc_Duet& w)
{ return Oc_Duet(_mm_add_pd(_mm_set1_pd(x),w.value)); }
inline const Oc_Duet operator+(const Oc_Duet& w,OC_REAL8 x)
{ return Oc_Duet(_mm_add_pd(w.value,_mm_set1_pd(x))); }

inline const Oc_Duet operator-(const Oc_Duet& w1,const Oc_Duet& w2)
{ return Oc_Duet(_mm_sub_pd(w1.value,w2.value)); }
inline const Oc_Duet operator-(OC_REAL8 x,const Oc_Duet& w)
{ return Oc_Duet(_mm_sub_pd(_mm_set1_pd(x),w.value)); }
inline const Oc_Duet operator-(const Oc_Duet& w,OC_REAL8 x)
{ return Oc_Duet(_mm_sub_pd(w.value,_mm_set1_pd(x))); }

inline const Oc_Duet operator*(const Oc_Duet& w1,const Oc_Duet& w2)
{ return Oc_Duet(_mm_mul_pd(w1.value,w2.value)); }
inline const Oc_Duet operator*(OC_REAL8 x,const Oc_Duet& w)
{ return Oc_Duet(_mm_mul_pd(_mm_set1_pd(x),w.value)); }
inline const Oc_Duet operator*(const Oc_Duet& w,OC_REAL8 x)
{ return Oc_Duet(_mm_mul_pd(w.value,_mm_set1_pd(x))); }

inline const Oc_Duet operator/(const Oc_Duet& w1,const Oc_Duet& w2)
{ return Oc_Duet(_mm_div_pd(w1.value,w2.value)); }
inline const Oc_Duet operator/(OC_REAL8 x,const Oc_Duet& w)
{ return Oc_Duet(_mm_div_pd(_mm_set1_pd(x),w.value)); }
inline const Oc_Duet operator/(const Oc_Duet& w,OC_REAL8 x)
{ return Oc_Duet(_mm_div_pd(w.value,_mm_set1_pd(x))); }

inline const Oc_Duet Oc_FMA(const Oc_Duet& w1,const Oc_Duet& w2,
                            const Oc_Duet& w3)
{ // Fused multiply-add
#if OC_FMA_TYPE == 3
  return Oc_Duet(_mm_fmadd_pd(w1.value,w2.value,w3.value));
#elif OC_FMA_TYPE == 4
  return Oc_Duet(_mm_macc_pd(w1.value,w2.value,w3.value));
#else
  return w1*w2 + w3;
#endif
}

inline const Oc_Duet sqrt(const Oc_Duet& w)
{ return Oc_Duet(_mm_sqrt_pd(w.value)); }

inline const Oc_Duet recip(const Oc_Duet& w)
{ return Oc_Duet(_mm_div_pd(_mm_set1_pd(1.0),w.value)); }

inline const Oc_Duet Oc_FlipDuet(const Oc_Duet& w)
{ // flips components
  return Oc_Duet(_mm_shuffle_pd(w.value,w.value,1));
}

#else  // !OC_USE_SSE

// Prefetching (NOP)
enum Oc_PrefetchDirective { Ocpd_NTA, Ocpd_T2, Ocpd_T1, Ocpd_T0 };
template<Oc_PrefetchDirective> inline void Oc_Prefetch(const void*) {}

class Oc_Duet {
public:
  Oc_Duet& Set(OC_REAL8m a,OC_REAL8m b) { v0=a; v1=b; return *this; }
  Oc_Duet& Set(OC_REAL8m x) { v0 = v1 = x; return *this; }
  Oc_Duet& LoadUnaligned(const OC_REAL8m& pair) {
    v0=pair; v1=*((&pair)+1); return *this;
  }
  Oc_Duet& LoadAligned(const OC_REAL8m& pair) { return LoadUnaligned(pair); }
  Oc_Duet(OC_REAL8m a,OC_REAL8m b) : v0(a), v1(b) {}
  Oc_Duet(OC_REAL8m x) : v0(x), v1(x) {}
  // NOTE: No Oc_Duet(const __m128d& newval) constructor 
  //       in the non-OC_USE_SSE branch
  Oc_Duet(const Oc_Duet& other) : v0(other.v0),  v1(other.v1) {}
  Oc_Duet() {}

  Oc_Duet& operator=(const Oc_Duet &other) {
    v0 = other.v0;  v1 = other.v1;  return *this;
  }

  Oc_Duet& SetZero() { v0 = v1 = 0.0; return *this; }

  Oc_Duet& operator+=(const Oc_Duet& other) {
    v0 += other.v0;  v1 += other.v1;  return *this;
  }
  Oc_Duet& operator+=(const OC_REAL8m& x) {
    v0 += x;  v1 += x;  return *this;
  }
  Oc_Duet& operator-=(const Oc_Duet& other) {
    v0 -= other.v0;  v1 -= other.v1;  return *this;
  }
  Oc_Duet& operator-=(const OC_REAL8m& x) {
    v0 -= x;  v1 -= x;  return *this;
  }

  Oc_Duet& operator*=(const Oc_Duet& other) {
    v0 *= other.v0;  v1 *= other.v1;  return *this;
  }
  Oc_Duet& operator*=(const OC_REAL8m& x) {
    v0 *= x;  v1 *= x;  return *this;
  }
  Oc_Duet& operator/=(const Oc_Duet& other) {
    v0 /= other.v0;  v1 /= other.v1;  return *this;
  }
  Oc_Duet& operator/=(const OC_REAL8m& x) {
    v0 /= x;  v1 /= x;  return *this;
  }

  Oc_Duet& MultLower(const OC_REAL8m& x) {
    // Multiplies lower part by x.  Upper part is unchanged
    v0 *= x;
    return *this;
  }

  // Use GetA/GetB for temporary use, StoreA/StoreB
  // for permanent storage.
  OC_REAL8m GetA() const { return v0; }
  OC_REAL8m GetB() const { return v1; }

  void StoreA(OC_REAL8m& Aout) const { Aout = v0; }
  void StoreB(OC_REAL8m& Bout) const { Bout = v1; }

  // StoreUnaligned, StoreAligned, and StoreStream write both values
  // to memory.  They are identical in the non-SSE version.
  void StoreUnaligned(OC_REAL8m& pair) const { 
    OC_REAL8m* pptr = & pair;  pptr[0] = v0;  pptr[1] = v1;
  }
  void StoreAligned(OC_REAL8m& pair) const { StoreUnaligned(pair); }
  void StoreStream(OC_REAL8m& pair)  const { StoreUnaligned(pair); }

  void KeepMin(const Oc_Duet& tst) {
    v0 = v0 > tst.v0 ? tst.v0 : v0 ;
    v1 = v1 > tst.v1 ? tst.v1 : v1 ;
  }
  void KeepMax(const Oc_Duet& tst) {
    v0 = v0 < tst.v0 ? tst.v0 : v0 ;
    v1 = v1 < tst.v1 ? tst.v1 : v1 ;
  }

  // NOTE: No GetSSEValue() or GetSSERef in the non-OC_USE_SSE branch

  friend const Oc_Duet operator+(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator+(OC_REAL8m x,const Oc_Duet& w);
  friend const Oc_Duet operator+(const Oc_Duet& w,OC_REAL8m x);

  friend const Oc_Duet operator-(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator-(OC_REAL8m x,const Oc_Duet& w);
  friend const Oc_Duet operator-(const Oc_Duet& w,OC_REAL8m x);

  friend const Oc_Duet operator*(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator*(OC_REAL8m x,const Oc_Duet& w);
  friend const Oc_Duet operator*(const Oc_Duet& w,OC_REAL8m x);

  friend const Oc_Duet operator/(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator/(OC_REAL8m x,const Oc_Duet& w);
  friend const Oc_Duet operator/(const Oc_Duet& w,OC_REAL8m x);

  friend const Oc_Duet Oc_FMA(const Oc_Duet& w1,const Oc_Duet& w2,
                              const Oc_Duet& w3); // w1*w2+w3

  friend const Oc_Duet sqrt(const Oc_Duet& w);
  friend const Oc_Duet recip(const Oc_Duet& w); // 1.0/w

  friend const Oc_Duet Oc_FlipDuet(const Oc_Duet& w); // flips components

private:
  OC_REAL8m v0,v1;
};

inline const Oc_Duet operator+(const Oc_Duet& w1,const Oc_Duet& w2)
{
  return Oc_Duet(w1.v0+w2.v0,w1.v1+w2.v1);
}
inline const Oc_Duet operator+(OC_REAL8m x,const Oc_Duet& w)
{
  return Oc_Duet(x+w.v0,x+w.v1);
}
inline const Oc_Duet operator+(const Oc_Duet& w,OC_REAL8m x)
{
  return Oc_Duet(w.v0+x,w.v1+x);
}

inline const Oc_Duet operator-(const Oc_Duet& w1,const Oc_Duet& w2)
{
  return Oc_Duet(w1.v0-w2.v0,w1.v1-w2.v1);
}
inline const Oc_Duet operator-(OC_REAL8m x,const Oc_Duet& w)
{
  return Oc_Duet(x-w.v0,x-w.v1);
}
inline const Oc_Duet operator-(const Oc_Duet& w,OC_REAL8m x)
{
  return Oc_Duet(w.v0-x,w.v1-x);
}

inline const Oc_Duet operator*(const Oc_Duet& w1,const Oc_Duet& w2)
{
  return Oc_Duet(w1.v0*w2.v0,w1.v1*w2.v1);
}
inline const Oc_Duet operator*(OC_REAL8m x,const Oc_Duet& w)
{
  return Oc_Duet(x*w.v0,x*w.v1);
}
inline const Oc_Duet operator*(const Oc_Duet& w,OC_REAL8m x)
{
  return Oc_Duet(w.v0*x,w.v1*x);
}

inline const Oc_Duet operator/(const Oc_Duet& w1,const Oc_Duet& w2)
{
  return Oc_Duet(w1.v0/w2.v0,w1.v1/w2.v1);
}
inline const Oc_Duet operator/(OC_REAL8m x,const Oc_Duet& w)
{
  return Oc_Duet(x/w.v0,x/w.v1);
}
inline const Oc_Duet operator/(const Oc_Duet& w,OC_REAL8m x)
{
  return Oc_Duet(w.v0/x,w.v1/x);
}

inline const Oc_Duet Oc_FMA(const Oc_Duet& w1,const Oc_Duet& w2,
                            const Oc_Duet& w3)
{ // Fused multiply-add
  return Oc_Duet(w1.v0*w2.v0+w3.v0,w1.v1*w2.v1+w3.v1);
}

inline const Oc_Duet sqrt(const Oc_Duet& w)
{
  return Oc_Duet(sqrt(w.v0),sqrt(w.v1));
}

inline const Oc_Duet recip(const Oc_Duet& w)
{
  return Oc_Duet(OC_REAL8m(1.0)/w.v0,OC_REAL8m(1.0)/w.v1);
}

inline const Oc_Duet Oc_FlipDuet(const Oc_Duet& w)
{ // flips components
  return Oc_Duet(w.v1,w.v0);
}

#endif // OC_USE_SSE
#endif // OCSSE
