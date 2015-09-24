/* FILE: xpfloat.h                    -*-Mode: c++-*-
 *
 * Class for extended floating point precision using
 * compensated summation.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2012-08-29 22:38:47 $
 * Last modified by: $Author: donahue $
 */

#include <assert.h>

#include "oc.h"

#ifndef _NB_XPFLOAT_H
#define _NB_XPFLOAT_H

#if OC_USE_SSE
# include <emmintrin.h>
#endif

/* End includes */     /* Optional directive to pimake */

/* NOTE: The same considerations regarding passing Oc_Duet objects in
 * parameter lists (see oc/ocsse.h) apply to Nb_Xpfloat objects too,
 * since Nb_Xpfloat's may comprise SSE objects.  So, in particular, DO
 * NOT PASS Nb_Xpfloat BY VALUE!
 */

/* Specify floating point type to use inside Nb_Xpfloat.  The main
 * problem here is that on x86 machines 64-bit double arithmetic may
 * compute intermediate results using 80-bit long double arithmetic.
 * The results can play havoc with the compensated summation code
 * below.
 *
 * Considerations are:
 *   
 *   NB_XPFLOAT_TYPE should be at least as wide as OC_REAL8m.
 *
 *   We need to insure compensated addition steps are not messed up by
 *   intermediate results have extra precision.
 *
 *   This code is widely used, so don't make it slower than necessary.
 *
 * Preferences are:
 *
 *   1) If SSE is available and OC_REAL8m is 8 bytes wide, then use SSE.
 *
 *   2) If OC_REAL8m is 8 bytes wide and double doesn't suffer from
 *      extra precision, then use double.
 *
 *   3) If long double doesn't suffer from extra precision, then
 *      use that.  In this case we are assuming that long double
 *      is relatively fast, at any rate faster than volatile
 *      double.  In practice this should be okay because the only
 *      platform we see that exhibits extra precision is x86, and
 *      in that case long double is supported in hardware.
 *
 *   4) Last option: volatile double.
 *
 * Question: Do we want to provide an option to allow the user
 * to make NB_XPFLOAT_TYPE long double regardless, to provide
 * maximum precision regardless of speed?
 *
 * WARNING WARNING WARNING: If SSE is used, then Nb_Xpfloat's
 *  *** CANNOT BE USED IN std::vector *** or other standard template
 *  library containers, because on 32-bit platforms the alignment of
 *  the start of the vector array is only guaranteed to have 8-byte
 *  alignment (whereas __m128d require 16-byte alignment).  If you
 *  really need to do this, then AFAICT the only solution is to
 *  implement a nonstandard allocator for std::vector (and etc.)
 *
 */
#if !OC_REAL8m_IS_DOUBLE
typedef long double NB_XPFLOAT_TYPE; // No choice
# define NB_XPFLOAT_USE_SSE 0
# if OC_FP_LONG_DOUBLE_EXTRA_PRECISION
#  define NB_XPFLOAT_NEEDS_VOLATILE 1
# else
#  define NB_XPFLOAT_NEEDS_VOLATILE 0
# endif

#elif OC_USE_SSE
typedef OC_REAL8 NB_XPFLOAT_TYPE;
# define NB_XPFLOAT_USE_SSE 1
# define NB_XPFLOAT_NEEDS_VOLATILE 0

#elif !OC_FP_DOUBLE_EXTRA_PRECISION
typedef double NB_XPFLOAT_TYPE;
# define NB_XPFLOAT_USE_SSE 0
# define NB_XPFLOAT_NEEDS_VOLATILE 0

#elif !OC_FP_LONG_DOUBLE_EXTRA_PRECISION
typedef long double NB_XPFLOAT_TYPE;
# define NB_XPFLOAT_USE_SSE 0
# define NB_XPFLOAT_NEEDS_VOLATILE 0

#else
typedef double NB_XPFLOAT_TYPE;
# define NB_XPFLOAT_USE_SSE 0
# define NB_XPFLOAT_NEEDS_VOLATILE 1

#endif

class Nb_Xpfloat
{
  friend void Nb_XpfloatDualAccum(Nb_Xpfloat&,NB_XPFLOAT_TYPE,
                                  Nb_Xpfloat&,NB_XPFLOAT_TYPE);
#if OC_USE_SSE
  // The following interface is available if OC_USE_SSE is true, even if
  // Nb_Xpfloat is not using SSE.
  friend void Nb_XpfloatDualAccum(Nb_Xpfloat&,Nb_Xpfloat&,__m128d);
#endif

  friend void Nb_XpfloatDualAccum(Nb_Xpfloat&,Nb_Xpfloat&,const Oc_Duet&);

private:
#if !NB_XPFLOAT_USE_SSE
  NB_XPFLOAT_TYPE x,corr;
#else // NB_XPFLOAT_USE_SSE
  __m128d xpdata;
#endif // NB_XPFLOAT_USE_SSE

public:

#if !NB_XPFLOAT_USE_SSE
  Nb_Xpfloat() : x(0), corr(0) {}
  Nb_Xpfloat(NB_XPFLOAT_TYPE newx) : x(newx), corr(0) {}
  Nb_Xpfloat(const Nb_Xpfloat& y) : x(y.x), corr(y.corr) {}

  void Set(NB_XPFLOAT_TYPE newx) { x=newx; corr=0.; }
  Nb_Xpfloat& operator=(NB_XPFLOAT_TYPE y) { Set(y); return *this; }
  Nb_Xpfloat& operator=(const Nb_Xpfloat& y) {
    x=y.x; corr=y.corr; return *this;
  }

  NB_XPFLOAT_TYPE GetValue() const { return x; }

  // Defining Accum(NB_XPFLOAT_TYPE) here, as opposed to in the
  // xpfloat.cc file, allows significant speedups with the g++ compiler.
  // It makes small difference with the Intel C++ compiler (icpc),
  // presumably because of icpc's interprocedural optimization ability?
  void Accum(NB_XPFLOAT_TYPE y) {
# if NB_XPFLOAT_NEEDS_VOLATILE 
    volatile NB_XPFLOAT_TYPE sum1; // Mark as volatile to protect against
    volatile NB_XPFLOAT_TYPE sum2; // problems arising from extra precision.
    volatile NB_XPFLOAT_TYPE corrtemp;
# else
    NB_XPFLOAT_TYPE sum1;
    NB_XPFLOAT_TYPE sum2;
    NB_XPFLOAT_TYPE corrtemp;
# endif

    // Calculate sum
    sum1=y+corr;
    sum2=x+sum1;

    // Determine new correction term.  Do in two steps, as opposed to
    // "corrtemp = (x-sum2) + sum1", because a) some compilers ignore
    // parentheses (with regards to ordering) at high optimization levels,
    // and b) we want to be sure to drop any extra precision.
    corrtemp = x - sum2;
    corrtemp += sum1;
    
    // Store results
    corr = corrtemp;
    x=sum2;
  }

  Nb_Xpfloat& operator*=(NB_XPFLOAT_TYPE y) { x*=y; corr*=y; return *this; }

#else // NB_XPFLOAT_USE_SSE ////////////////////////////////////////////////

  Nb_Xpfloat() : xpdata(_mm_setzero_pd()) {}
  Nb_Xpfloat(NB_XPFLOAT_TYPE newx) : xpdata(_mm_set_sd(newx)) {}
  Nb_Xpfloat(const Nb_Xpfloat& y) : xpdata(y.xpdata) {}

  void Set(NB_XPFLOAT_TYPE newx) { xpdata=_mm_set_sd(newx); }
  Nb_Xpfloat& operator=(NB_XPFLOAT_TYPE y) { Set(y); return *this; }
  Nb_Xpfloat& operator=(const Nb_Xpfloat& y) {
    xpdata = y.xpdata; return *this;
  }

  NB_XPFLOAT_TYPE GetValue() const { return Oc_SseGetLower(xpdata); }

  // Defining Accum(NB_XPFLOAT_TYPE) here, as opposed to in the
  // xpfloat.cc file, allows significant speedups with the g++ compiler.
  // It makes small difference with the Intel C++ compiler (icpc),
  // presumably because of icpc's interprocedural optimization ability?
  void Accum(NB_XPFLOAT_TYPE y) {
    // SSE double precision doesn't carry any extra precision, so we
    // don't need to use "volatile"
    __m128d corr = _mm_unpackhi_pd(xpdata,_mm_setzero_pd());
    __m128d sum1 = _mm_add_sd(_mm_set_sd(y),corr);
    __m128d sum2 = _mm_add_sd(sum1,xpdata);
    __m128d corrtemp = _mm_add_sd(sum1,_mm_sub_sd(xpdata,sum2));
    xpdata = _mm_unpacklo_pd(sum2,corrtemp);
  }

  Nb_Xpfloat& operator*=(NB_XPFLOAT_TYPE y) {
    xpdata = _mm_mul_pd(xpdata,_mm_set1_pd(y));
    return *this;
  }

#endif // NB_XPFLOAT_USE_SSE


  Nb_Xpfloat& operator+=(NB_XPFLOAT_TYPE y) { Accum(y); return *this; }
  Nb_Xpfloat& operator-=(NB_XPFLOAT_TYPE y) { Accum(-y); return *this; }

  void Accum(const Nb_Xpfloat& y);
  Nb_Xpfloat& operator+=(const Nb_Xpfloat& y) { Accum(y); return *this; }
  Nb_Xpfloat& operator-=(const Nb_Xpfloat& y);
  Nb_Xpfloat& operator*=(const Nb_Xpfloat& y);

  friend Nb_Xpfloat operator*(const Nb_Xpfloat& x,const Nb_Xpfloat& y);
};

Nb_Xpfloat operator+(const Nb_Xpfloat& x,const Nb_Xpfloat& y);
Nb_Xpfloat operator-(const Nb_Xpfloat& x,const Nb_Xpfloat& y);
Nb_Xpfloat operator*(const Nb_Xpfloat& x,NB_XPFLOAT_TYPE y);
inline
Nb_Xpfloat operator*(NB_XPFLOAT_TYPE y,const Nb_Xpfloat& x) { return x*y; }
Nb_Xpfloat operator*(const Nb_Xpfloat& x,const Nb_Xpfloat& y);

#if OC_USE_SSE
inline void
Nb_XpfloatDualAccum(Nb_Xpfloat& xpA,Nb_Xpfloat& xpB,__m128d y)
{ // Import y holds two packed double precision values.  The
  // lower half of y holds the value to be accumulated into xpA,
  // and the upper half holds the value to be accumulated into xpB.

  // This interface is available if OC_USE_SSE is true, even if
  // Nb_Xpfloat is not using SSE.

  // IMPORTANT NOTE : This code assumes that xpA and xpB are
  // ***DIFFERENT*** objects.  If they are the same object, then one of
  // the accums will be lost!!!!!!!

  // For implementation notes, see the Nb_Xpfloat::Accum routine above.
  assert(&xpA != &xpB);  // See IMPORTANT NOTE above.
#if NB_XPFLOAT_USE_SSE
  __m128d wx   = _mm_unpacklo_pd(xpA.xpdata,xpB.xpdata);
  __m128d sum1 = _mm_add_pd(y,_mm_unpackhi_pd(xpA.xpdata,xpB.xpdata));
  __m128d sum2 = _mm_add_pd(wx,sum1);
  __m128d corrtemp = _mm_add_pd(_mm_sub_pd(wx,sum2),sum1);
  xpA.xpdata = _mm_unpacklo_pd(sum2,corrtemp);
  xpB.xpdata = _mm_unpackhi_pd(sum2,corrtemp);
#else
  // Non-SSE version that retains precision in xpA and xpB
  // in case where NB_XPFLOAT_TYPE is wider than REAL8.
  double dummy[2];
  _mm_storeu_pd(dummy,y);
  Nb_XpfloatDualAccum(xpA,dummy[0],xpB,dummy[1]);
#endif
}
#endif

inline void
Nb_XpfloatDualAccum(Nb_Xpfloat& xpA,NB_XPFLOAT_TYPE yA,
                    Nb_Xpfloat& xpB,NB_XPFLOAT_TYPE yB)
{ // Updates two Nb_Xpfloat objects at the same time.  The main
  // reason for the existence of this function is double-scalar
  // processing offered in the SSE implementation.

  // IMPORTANT NOTE : This code assumes that xpA and xpB are
  // ***DIFFERENT*** objects.  If they are the same object, then one of
  // the accums will be lost!!!!!!!

  // For implementation notes, see the Nb_Xpfloat::Accum routine above.


  assert(&xpA != &xpB);  // See IMPORTANT NOTE above.

#if !NB_XPFLOAT_USE_SSE
#if NB_XPFLOAT_NEEDS_VOLATILE 
  volatile NB_XPFLOAT_TYPE sumA1;
  volatile NB_XPFLOAT_TYPE sumB1;
  volatile NB_XPFLOAT_TYPE sumA2;
  volatile NB_XPFLOAT_TYPE sumB2;
  volatile NB_XPFLOAT_TYPE corrtempA;
  volatile NB_XPFLOAT_TYPE corrtempB;
#else
  NB_XPFLOAT_TYPE sumA1,     sumB1;
  NB_XPFLOAT_TYPE sumA2,     sumB2;
  NB_XPFLOAT_TYPE corrtempA, corrtempB;
#endif

  sumA1=yA+xpA.corr;           sumB1=yB+xpB.corr;
  sumA2=xpA.x+sumA1;           sumB2=xpB.x+sumB1;
  corrtempA = xpA.x - sumA2;   corrtempB = xpB.x - sumB2;
  corrtempA += sumA1;          corrtempB += sumB1;

  xpA.corr = corrtempA;
  xpA.x=sumA2;

  xpB.corr = corrtempB;
  xpB.x=sumB2;

#else // NB_XPFLOAT_USE_SSE

  Nb_XpfloatDualAccum(xpA,xpB,_mm_set_pd(yB,yA));

#endif // NB_XPFLOAT_USE_SSE
}

inline void
Nb_XpfloatDualAccum(Nb_Xpfloat& xpA,Nb_Xpfloat& xpB,const Oc_Duet& y)
{
#if OC_USE_SSE
  Nb_XpfloatDualAccum(xpA,xpB,y.GetSSEValue());
#else
  Nb_XpfloatDualAccum(xpA,y.GetA(),xpB,y.GetB());
#endif
}

#endif // _NB_XPFLOAT_H
