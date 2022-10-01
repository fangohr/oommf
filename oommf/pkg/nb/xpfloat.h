/* FILE: xpfloat.h                    -*-Mode: c++-*-
 *
 * Class for extended floating point precision using
 * compensated summation.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2012/11/20 01:12:44 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>

#include "oc.h"

#ifndef _NB_XPFLOAT_H
#define _NB_XPFLOAT_H

#if OC_USE_SSE
# include <emmintrin.h>
#endif

/* End includes */     /* Optional directive to pimake */

/* The floating point associativity controls can be set in the platform
 * config files, in which case oommf/pkg/oc/procs.tcl writes the values
 * into ocport.h. Otherwise set values here depending on C++ compiler
 */
#ifdef OC_COMPILER_FILE_ASSOCIATIVITY_CONTROL
# define NB_XPFLOAT_COMPILER_FILE_ASSOCIATIVITY_CONTROL \
  OC_COMPILER_FILE_ASSOCIATIVITY_CONTROL
#else
# define NB_XPFLOAT_COMPILER_FILE_ASSOCIATIVITY_CONTROL 1
/// All compilers should have some way to respect the operation
/// order specified in the code.
#endif

#ifdef OC_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL
# define NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL \
  OC_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL
#else
# if defined(_MSC_VER) || defined(_INTEL_COMPILER) \
  || defined(__clang__) || defined(__PGIC__) \
  || (defined(__GNUC__) && __GNUC__ > 4)
#  define NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL 1
# else
#  define NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL 0
# endif
#endif

// The following preprocessor block defines macros for enabling
// floating-point associativity control at function scope. There
// are four macros:
//  NB_XPFLOAT_ASSOC_CONTROL_START_A - placed before function name
//  NB_XPFLOAT_ASSOC_CONTROL_START_B - placed after function name
//                                     but before opening brace
//  NB_XPFLOAT_ASSOC_CONTROL_START_C - placed after opening brace,
//                                     before first code line.
//  NB_XPFLOAT_ASSOC_CONTROL_END     - place after function closing brace
// The _Pragma operator is part of the C99 and C++11 standards

#if NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL
# if defined(_MSC_VER) && _MSC_VER<1926
#  define PRAGMA(X) __pragma(X)  // Microsoft-specific extension
# else
#  define PRAGMA(X) _Pragma(#X)  // Part of C99 and C++-11 standards
#endif

# if defined(_MSC_VER)
#  define NB_XPFLOAT_ASSOC_CONTROL_START_A PRAGMA(float_control(precise,on,push))
#  define NB_XPFLOAT_ASSOC_CONTROL_END     PRAGMA(float_control(pop))

# elif defined(_INTEL_COMPILER)
#  define NB_XPFLOAT_ASSOC_CONTROL_START_C PRAGMA(float_control(precise,on))

# elif defined(__clang__)
#  define NB_XPFLOAT_ASSOC_CONTROL_START_B __attribute__ ((optnone))
// Note: The Apple variant of clang doesn't support #pragma clang optimize off

# elif defined(__PGIC__)
#  define NB_XPFLOAT_ASSOC_CONTROL_START_C PRAGMA(routine noassoc)
// Portland Group Compiler; Alternatively, PRAGMA(routine opt 1)

# elif defined(__GNUC__)
// A number of compilers other than gcc #define the __GNUC__ macro,
// so keep this elif block last.
#  define NB_XPFLOAT_ASSOC_CONTROL_START_A \
   PRAGMA(GCC push_options) PRAGMA(GCC optimize ("-fno-associative-math"))
// If -fno-associative-math fails, try __attribute__((optimize(2,"no-fast-math")));
#  define NB_XPFLOAT_ASSOC_CONTROL_END PRAGMA(GCC pop_options)

# else
#  error OC_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL enabled\
 but compiler support missing
# endif // Compiler select
#endif // OC_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL

#ifndef NB_XPFLOAT_ASSOC_CONTROL_START_A
# define NB_XPFLOAT_ASSOC_CONTROL_START_A
#endif
#ifndef NB_XPFLOAT_ASSOC_CONTROL_START_B
# define NB_XPFLOAT_ASSOC_CONTROL_START_B
#endif
#ifndef NB_XPFLOAT_ASSOC_CONTROL_START_C
# define NB_XPFLOAT_ASSOC_CONTROL_START_C
#endif
#ifndef NB_XPFLOAT_ASSOC_CONTROL_END
# define NB_XPFLOAT_ASSOC_CONTROL_END
#endif

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
#if OC_REAL8m_IS_LONG_DOUBLE
typedef long double NB_XPFLOAT_TYPE; // No choice
# define NB_XPFLOAT_IS_OC_REAL8m 1
# define NB_XPFLOAT_USE_SSE 0
# if OC_FP_LONG_DOUBLE_EXTRA_PRECISION
#  define NB_XPFLOAT_NEEDS_VOLATILE 1
# else
#  define NB_XPFLOAT_NEEDS_VOLATILE 0
# endif

#elif OC_USE_SSE
typedef OC_REAL8 NB_XPFLOAT_TYPE;
# define NB_XPFLOAT_IS_OC_REAL8m OC_REAL8m_IS_OC_REAL8
# define NB_XPFLOAT_USE_SSE 1
# define NB_XPFLOAT_NEEDS_VOLATILE 0

#elif !OC_FP_DOUBLE_EXTRA_PRECISION
typedef double NB_XPFLOAT_TYPE;
# define NB_XPFLOAT_IS_OC_REAL8m OC_REAL8m_IS_DOUBLE
# define NB_XPFLOAT_USE_SSE 0
# define NB_XPFLOAT_NEEDS_VOLATILE 0

#elif !OC_FP_LONG_DOUBLE_EXTRA_PRECISION
typedef long double NB_XPFLOAT_TYPE;
# define NB_XPFLOAT_IS_OC_REAL8m OC_REAL8m_IS_LONG_DOUBLE
# define NB_XPFLOAT_USE_SSE 0
# define NB_XPFLOAT_NEEDS_VOLATILE 0

#else
typedef double NB_XPFLOAT_TYPE;
# define NB_XPFLOAT_IS_OC_REAL8m OC_REAL8m_IS_DOUBLE
# define NB_XPFLOAT_USE_SSE 0
# define NB_XPFLOAT_NEEDS_VOLATILE 1

#endif

// Some of the code below requires that the compiler respect
// floating-point operation non-associativity.  Some compilers are
// sufficiently circumspect wrt optimization either by default or via
// support code directives that this is not a problem.  For others,
// achieve this effect by defining NB_XPFLOAT_NEEDS_VOLATILE
// true which causes some variables to be marked "volatile".
#if !NB_XPFLOAT_COMPILER_FILE_ASSOCIATIVITY_CONTROL \
  && !NB_XPFLOAT_USE_SSE && !NB_XPFLOAT_NEEDS_VOLATILE
# undef  NB_XPFLOAT_NEEDS_VOLATILE
# define NB_XPFLOAT_NEEDS_VOLATILE 1
#endif

class Nb_Xpfloat
{
  friend void Nb_XpfloatDualAccum(Nb_Xpfloat&,NB_XPFLOAT_TYPE,
                                  Nb_Xpfloat&,NB_XPFLOAT_TYPE);
#if OC_USE_SSE
  // The following interface is available if OC_USE_SSE is true, even if
  // Nb_Xpfloat is not using SSE.
  friend void Nb_XpfloatDualAccum(Nb_Xpfloat&,Nb_Xpfloat&,const __m128d&);
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
  void GetValue(NB_XPFLOAT_TYPE& bigval,NB_XPFLOAT_TYPE& smallval) const {
    bigval = x; smallval = corr;
  }
#else // NB_XPFLOAT_USE_SSE
  void GetValue(NB_XPFLOAT_TYPE& bigval,NB_XPFLOAT_TYPE& smallval) const {
    bigval = Oc_SseGetLower(xpdata);
    smallval = Oc_SseGetUpper(xpdata);
  }
#endif // NB_XPFLOAT_USE_SSE
  /// Note: Some Windows header #define's a "small" macro. Seriously???

  static int Test(); // Test that code works as designed. In particular,
  /// see if compiler has broken compensated summation code in Accum().
  /// Returns 0 on success, 1 on error.

#if NB_XPFLOAT_USE_SSE
  static size_t Alignment() { return 16; }
#else
  static size_t Alignment() { return 8; }
#endif

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

  Nb_Xpfloat& operator*=(NB_XPFLOAT_TYPE y);

#else // NB_XPFLOAT_USE_SSE ////////////////////////////////////////////////

# ifndef NDEBUG
  Nb_Xpfloat() {
    assert(sizeof(Nb_Xpfloat)==16 && reinterpret_cast<OC_UINDEX>(&xpdata)%16==0);
    xpdata = _mm_setzero_pd();
  }
  Nb_Xpfloat(NB_XPFLOAT_TYPE newx) {
    assert(sizeof(Nb_Xpfloat)==16 && reinterpret_cast<OC_UINDEX>(&xpdata)%16==0);
    xpdata = _mm_set_sd(newx);
  }
  Nb_Xpfloat(const Nb_Xpfloat& y) {
    assert(sizeof(Nb_Xpfloat)==16 && reinterpret_cast<OC_UINDEX>(&xpdata)%16==0);
    xpdata = y.xpdata;
  }
# else
  Nb_Xpfloat() : xpdata(_mm_setzero_pd()) {}
  Nb_Xpfloat(NB_XPFLOAT_TYPE newx) : xpdata(_mm_set_sd(newx)) {}
  Nb_Xpfloat(const Nb_Xpfloat& y) : xpdata(y.xpdata) {}
# endif

  void Set(NB_XPFLOAT_TYPE newx) { xpdata=_mm_set_sd(newx); }
  Nb_Xpfloat& operator=(NB_XPFLOAT_TYPE y) { Set(y); return *this; }
  Nb_Xpfloat& operator=(const Nb_Xpfloat& y) {
    xpdata = y.xpdata; return *this;
  }

  NB_XPFLOAT_TYPE GetValue() const { return Oc_SseGetLower(xpdata); }

  Nb_Xpfloat& operator*=(NB_XPFLOAT_TYPE y);

#endif // NB_XPFLOAT_USE_SSE

  void Accum(NB_XPFLOAT_TYPE y);

  Nb_Xpfloat& operator+=(NB_XPFLOAT_TYPE y) { Accum(y); return *this; }
  Nb_Xpfloat& operator-=(NB_XPFLOAT_TYPE y) { Accum(-y); return *this; }

  void Accum(const Nb_Xpfloat& y);
  Nb_Xpfloat& operator+=(const Nb_Xpfloat& y) { Accum(y); return *this; }
  Nb_Xpfloat& operator-=(const Nb_Xpfloat& y);
  Nb_Xpfloat& operator*=(const Nb_Xpfloat& y);

  operator NB_XPFLOAT_TYPE() const { return GetValue(); }

  friend Nb_Xpfloat operator*(const Nb_Xpfloat& x,const Nb_Xpfloat& y);

#if !NB_XPFLOAT_IS_OC_REAL8m
  // If NB_XPFLOAT_TYPE is different than OC_REAL8m, then overloaded functions
  // taking either NB_XPFLOAT_TYPE or Nb_Xpfloat won't know which way to cast
  // an OC_REAL8m.  Provide overloads taking OC_REAL8m explicitly.
  Nb_Xpfloat(OC_REAL8m newx) : x(newx), corr(0) {}
  void Set(OC_REAL8m newx) { Set(static_cast<NB_XPFLOAT_TYPE>(newx)); }
  void Accum(OC_REAL8m y) { Accum(static_cast<NB_XPFLOAT_TYPE>(y)); }
  Nb_Xpfloat& operator+=(OC_REAL8m y) {
    return operator+=(static_cast<NB_XPFLOAT_TYPE>(y));
  }
  Nb_Xpfloat& operator-=(OC_REAL8m y) {
    return operator-=(static_cast<NB_XPFLOAT_TYPE>(y));
  }
  Nb_Xpfloat& operator*=(OC_REAL8m y) {
    return operator*=(static_cast<NB_XPFLOAT_TYPE>(y));
  }
#endif // !NB_XPFLOAT_IS_OC_REAL8m
};

Nb_Xpfloat operator+(const Nb_Xpfloat& x,const Nb_Xpfloat& y);
Nb_Xpfloat operator-(const Nb_Xpfloat& x,const Nb_Xpfloat& y);
Nb_Xpfloat operator*(const Nb_Xpfloat& x,NB_XPFLOAT_TYPE y);
inline
Nb_Xpfloat operator*(NB_XPFLOAT_TYPE y,const Nb_Xpfloat& x) { return x*y; }
Nb_Xpfloat operator*(const Nb_Xpfloat& x,const Nb_Xpfloat& y);

// Defining Accum(NB_XPFLOAT_TYPE) here, as opposed to in the xpfloat.cc
// file, allows significant speedups with the g++ compiler.  It makes
// small difference with the Intel C++ compiler (icpc), presumably because
// of icpc's interprocedural optimization ability?  However, it is
// important to restrict the optimizer to respect the non-associativity of
// floating point addition. If there is no function-scope compiler
// directives for insuring this, then the code is built inside xpfloat.cc
// with file-scope optimization restrictions.  This case is handled with
// the NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL macro.
#if NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL
#if  !NB_XPFLOAT_USE_SSE
NB_XPFLOAT_ASSOC_CONTROL_START_A
inline void Nb_Xpfloat::Accum(NB_XPFLOAT_TYPE y)
NB_XPFLOAT_ASSOC_CONTROL_START_B
{
NB_XPFLOAT_ASSOC_CONTROL_START_C
  // Calculate sum, using compensated (Kahan) summation.
  //
  // We try to select NB_XPFLOAT_TYPE so that there is no hidden
  // precision in intermediate computations.  If this can't be
  // assured, the fallback is to accumulate all operations into
  // variables marked "volatile" to insure that no hidden precision is
  // carried between operations.  This is slow (one test with g++ on
  // an AMD Athlon 64 x2 showed a factor of 8), and moreover for all
  // the machines+compilers I know about the "long double" type
  // carries full precision.
  //
  // A second problem is compilers that ignore associativity
  // restrictions.  At the time of this writing (2012), GCC respects
  // associativity unless the -ffast-math optimization flag is given
  // at compile time.  We protect against this by using the
  // "-no-fast-math" function attribute in the case GCC is detected.
  // Also at this time, the Intel compile *by default* compiles with
  // what it calls the "fast=1" floating point model, which ignores
  // associativity.  This can be changed by adding the switch
  // "-fp-model precise" to the compile line, or else by adding
  // "#pragma float_control(precise,on)" inside this function.  The
  // pragma option operates on a function by function basis, so it is
  // less global.
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
NB_XPFLOAT_ASSOC_CONTROL_END

#else // NB_XPFLOAT_USE_SSE

NB_XPFLOAT_ASSOC_CONTROL_START_A
inline void Nb_Xpfloat::Accum(NB_XPFLOAT_TYPE y)
NB_XPFLOAT_ASSOC_CONTROL_START_B
{
NB_XPFLOAT_ASSOC_CONTROL_START_C
  // SSE double precision doesn't carry any extra precision, so we
  // don't need to use "volatile"
  __m128d corr = _mm_unpackhi_pd(xpdata,_mm_setzero_pd());
  __m128d sum1 = _mm_add_sd(_mm_set_sd(y),corr);
  __m128d sum2 = _mm_add_sd(sum1,xpdata);
  __m128d corrtemp = _mm_add_sd(sum1,_mm_sub_sd(xpdata,sum2));
  xpdata = _mm_unpacklo_pd(sum2,corrtemp);
}
NB_XPFLOAT_ASSOC_CONTROL_END

# endif // !NB_XPFLOAT_USE_SSE
#endif // NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL


#if OC_USE_SSE
# if NB_XPFLOAT_USE_SSE

#  if NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL
// It is only safe to inline the following function if we can
// disable fp associative math. Otherwise, it gets defined in
// the xpfloat.cc file, which is compiled value-safe.
NB_XPFLOAT_ASSOC_CONTROL_START_A
inline void
Nb_XpfloatDualAccum(Nb_Xpfloat& xpA,Nb_Xpfloat& xpB,const __m128d& y)
NB_XPFLOAT_ASSOC_CONTROL_START_B
{
NB_XPFLOAT_ASSOC_CONTROL_START_C
  // Import y holds two packed double precision values.  The
  // lower half of y holds the value to be accumulated into xpA,
  // and the upper half holds the value to be accumulated into xpB.
  // This interface is available if OC_USE_SSE is true, even if
  // Nb_Xpfloat is not using SSE.

  // IMPORTANT NOTE : This code assumes that xpA and xpB are
  // ***DIFFERENT*** objects.  If they are the same object, then one of
  // the accums will be lost!!!!!!!

  // For implementation notes, see the Nb_Xpfloat::Accum routine above.
  assert(&xpA != &xpB);  // See IMPORTANT NOTE above.
  __m128d wx_lo = _mm_unpacklo_pd(xpA.xpdata,xpB.xpdata);
  __m128d wx_hi = _mm_unpackhi_pd(xpA.xpdata,xpB.xpdata);
  __m128d sum1  = _mm_add_pd(y,wx_hi);
  __m128d sum2  = _mm_add_pd(wx_lo,sum1);
  __m128d diff  = _mm_sub_pd(wx_lo,sum2);
  __m128d corrtemp = _mm_add_pd(diff,sum1);
  xpA.xpdata = _mm_unpacklo_pd(sum2,corrtemp);
  xpB.xpdata = _mm_unpackhi_pd(sum2,corrtemp);
}
NB_XPFLOAT_ASSOC_CONTROL_END
#  endif // NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL

# else // !NB_XPFLOAT_USE_SSE

inline void
Nb_XpfloatDualAccum(Nb_Xpfloat& xpA,Nb_Xpfloat& xpB,const __m128d& y)
{ // For implementation notes, see the Nb_Xpfloat::Accum routine above.
  assert(&xpA != &xpB);  // See IMPORTANT NOTE above.
  // Non-SSE version that retains precision in xpA and xpB
  // in case where NB_XPFLOAT_TYPE is wider than REAL8.
  double dummy[2];
  _mm_storeu_pd(dummy,y);
  Nb_XpfloatDualAccum(xpA,dummy[0],xpB,dummy[1]);
}

# endif // NB_XPFLOAT_USE_SSE
#endif // OC_USE_SSE

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
    xpA += yA;
    xpB += yB;
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

#if !NB_XPFLOAT_IS_OC_REAL8m
// If NB_XPFLOAT_TYPE is different than OC_REAL8m, then overloaded functions
// taking either NB_XPFLOAT_TYPE or Nb_Xpfloat won't know which way to cast
// an OC_REAL8m.  Provide overloads taking OC_REAL8m explicitly.
inline Nb_Xpfloat operator*(const Nb_Xpfloat& x,OC_REAL8m y) {
  return x*static_cast<NB_XPFLOAT_TYPE>(y);
}
inline Nb_Xpfloat operator*(OC_REAL8m y,const Nb_Xpfloat& x) {
  return static_cast<NB_XPFLOAT_TYPE>(y)*x;
}
inline void
Nb_XpfloatDualAccum(Nb_Xpfloat& xpA,OC_REAL8m yA,
                    Nb_Xpfloat& xpB,OC_REAL8m yB)
{
  Nb_XpfloatDualAccum(xpA,static_cast<NB_XPFLOAT_TYPE>(yA),
                      xpB,static_cast<NB_XPFLOAT_TYPE>(yB));
}
#endif // !NB_XPFLOAT_IS_OC_REAL8m

#endif // _NB_XPFLOAT_H
