/* FILE: doubledouble.h
 * Header file for Xp_DoubleDouble class
 *
 *  Algorithms based on TJ Dekker, "A floating-point technique for
 *  extending the available precision," Numer. Math. 18, 224-242 (1971),
 *  and Jonathan Richard Shewchuk, "Adaptive precision floating-point
 *  arithmetic and fast robust geometric predicates," Discrete and
 *  Computational Geometry, 18, 305-363 (1997).
 *
 *  See also NOTES VI, 22-Nov-2012 through 19-Jan-2013, pp 107-137.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * NOTE: 32-bit Visual C++ apparently can't handle alignment
 *       properly for pass-by-value SSE.  In this case we
 *       need to fall back to non-SSE implementation.
 *
 * NOTE: Code external to this package should NOT #include
 * this file directly.  Instead, use
 *     #include "xp.h"
 * which includes logic to automatically include either
 * doubledouble.h or alt_doubledouble.h, as appropriate.
 */

/*
 * SSE notes:
 *
 * gnu defines macros __SSE2__ and __SSE2_MATH__ macros to identify
 * -msse and -mfpmath=sse2 command line options, respectively.
 *
 * VC++ uses the macro _M_IX86_FP with documented values 0, 1, and 2:
 *   0 => /arch was not used
 *   1 => /arch:SSE was used
 *   2 => /arch:SSE2 was used
 */

#ifndef _XP_DOUBLEDOUBLE
#define _XP_DOUBLEDOUBLE

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <iostream>
#include <string>

#include "oc.h"
#include "xpport.h" // Various compiler and compiler-option specific values.

#include "xpcommon.h"

/* End includes */

#define XP_DOUBLEDOUBLE_PRECISION (2*XP_DDFLOAT_MANTISSA_PRECISION+1)

#ifndef XP_DD_USE_SSE
# define XP_DD_USE_SSE 0  // SSE code not yet implemented
#endif

////////////////////////////////////////////////////////////////////////
// If XP_RANGE_CHECK is 1, then the code in this module will do some
// rudimentary checking for overflows, and set the results to Inf as
// appropriate.  Set XP_RANGE_CHECK to 0 to remove these checks, which
// will speed up the code somewhat at the expense of less graceful
// overflow behavior.  If XP_RANGE_CHECK is not set then the behavior is
// tied to NDEBUG.
#ifndef XP_RANGE_CHECK
# ifndef NDEBUG
#  define XP_RANGE_CHECK 1
# else
#  define XP_RANGE_CHECK 0
# endif
#endif

////////////////////////////////////////////////////////////////////////

class Xp_DoubleDouble {
 public:
  // Quick test intended to compiler and hardware incompatibilities.
  // Return value is 0 on success.
  static int QuickTest();

  // Reference values
  const static Xp_DoubleDouble DD_SQRT2;
  const static Xp_DoubleDouble DD_LOG2;
  const static Xp_DoubleDouble DD_PI;
  const static Xp_DoubleDouble DD_HALFPI;

  // Public constructors
  Xp_DoubleDouble() {}
  Xp_DoubleDouble(float x);
  Xp_DoubleDouble(double x);
  Xp_DoubleDouble(long double x); // Cover all the bases
  Xp_DoubleDouble(float xhi,float xlo);
  Xp_DoubleDouble(double xhi,double xlo);
  Xp_DoubleDouble(long double xhi,long double xlo);
  // Note: Xp_DoubleDouble(double,double) constructor and friends
  // have in-built normalization; if xhi and xlo are already normalized,
  // it is faster for member and friend functions to use default
  // constructor and set elements directly.

  // Integer constructors: The OC_INT4 forms assumes that an OC_INT4
  // fits into XP_DDFLOAT_TYPE type w/o data loss.  The OC_INT8 forms
  // keep as many bits as possible.
  Xp_DoubleDouble(OC_INT4 ix);
  Xp_DoubleDouble(OC_UINT4 ix);
  Xp_DoubleDouble(OC_INT8 ix);
  Xp_DoubleDouble(OC_UINT8 ix);

  // High precision constructor
  Xp_DoubleDouble(const Xp_BigFloatVec& data) {
    Xp_DoubleDouble two_m
      = XpLdExp(static_cast<XP_DDFLOAT_TYPE>(1),data.width);
    int n
      = static_cast<int>(data.chunk.size()); // Sure hope int is wide enough!
    Xp_DoubleDouble val = data.chunk[n-1];
    for(int i=n-2;i>=0;--i) {
      val = val/two_m + data.chunk[i];
      /// Note: No rounding error if T is a radix 2 float.
    }
    val *= data.sign*XpLdExp(static_cast<XP_DDFLOAT_TYPE>(1),data.offset);
    *this = val;
  }

  // Use default destructor, copy constructor, copy-assignment operator

  // Routine that returns the width of the mantissa, in bits.
  static int GetMantissaWidth () {
    return XP_DOUBLEDOUBLE_PRECISION;
  }
  // Note: I wanted to implement this routine as
  //   std::numeric_limits<Xp_DoubleDouble>::digits
  // but there were problem using the mpfr type.  For details see
  // this function declaration in alt_doubledouble.h.

  const XP_DDFLOAT_TYPE& Hi() const; // Returns high word
  const XP_DDFLOAT_TYPE& Lo() const; // Returns low word

  void DebugBits(XP_DDFLOAT_TYPE& hi,XP_DDFLOAT_TYPE& lo) const {
    // Returns parts w/o any sanity checks.  Intended for use
    // in error handling code only.
    hi=a0;  lo=a1;
  }
  
  // NB: Don't provide a double or float type cast, as that makes
  // operations with mixed types ambiguous.  Instead, client code
  // should make explict Hi() or DownConvert() calls.

  // Explicit down-conversions.  The pointer versions return the value
  // of the given type, and also assign that value to the pointer if and
  // only if the ptr is non-null.  This allows the pointer versions to
  // be used for inline assignment.  For example:
  //
  //   Xp_DoubleDouble seven(7);
  //   const double foo = seven.DownConvert(static_cast<double*>(nullptr));
  //
  void DownConvert(double& d) const;
  void DownConvert(long double& ld) const;
  double DownConvert(double* dptr) const;
  long double DownConvert(long double* ldptr) const;

  // Size of unit-in-the-last-place.  This code assumes that Hi and Lo
  // values are close-packed, which is what we usually want.
  XP_DDFLOAT_TYPE ULP() const;

  // Compute difference, relative to refulp.  Returns absolute difference
  // if refulp is zero.
  XP_DDFLOAT_TYPE ComputeDiffULP(const Xp_DoubleDouble& ref,
                                 XP_DDFLOAT_TYPE refulp) const;
  
  // Member operator functions
  Xp_DoubleDouble& operator+=(const Xp_DoubleDouble& x) {
    *this = *this + x;
    return *this;
  }
  Xp_DoubleDouble& operator-=(const Xp_DoubleDouble& x) {
    *this = *this - x;
    return *this;
  }
  Xp_DoubleDouble& operator*=(const Xp_DoubleDouble& x) {
    *this = *this * x;
    return *this;
  }
  Xp_DoubleDouble& operator/=(const Xp_DoubleDouble& x) {
    *this = *this / x;
    return *this;
  }
  Xp_DoubleDouble& operator/=(const XP_DDFLOAT_TYPE x) {
    *this = *this / x;
    return *this;
  }
  Xp_DoubleDouble& Square();
  /// replaces *this with (*this)*(*this)

  // Comparison operations
  OC_BOOL IsPos() {
    assert(IsNormalized());
    return (a0>0.0);
  }
  OC_BOOL IsNeg() {
    assert(IsNormalized());
    return (a0<0.0);
  }
  OC_BOOL IsZero() {
    assert(IsNormalized());
    return (a0 == 0.0);
  }
  friend int Xp_Compare(const Xp_DoubleDouble& a,const Xp_DoubleDouble& b);
  /// Compare returns -1 if a<b, 0 if a==b, or 1 if a>b.  NaN behavior
  /// undefined.

  // Friend operator functions
  friend Xp_DoubleDouble operator-(const Xp_DoubleDouble& x); // unary -
  friend Xp_DoubleDouble operator+(const Xp_DoubleDouble& x,
                                   const Xp_DoubleDouble& y);
  friend Xp_DoubleDouble operator-(const Xp_DoubleDouble& x,
                                   const Xp_DoubleDouble& y);

  friend Xp_DoubleDouble operator*(const Xp_DoubleDouble& x,
                                   const Xp_DoubleDouble& y);
  friend Xp_DoubleDouble operator*(XP_DDFLOAT_TYPE x,
                                   const Xp_DoubleDouble& y);
  friend Xp_DoubleDouble operator*(const Xp_DoubleDouble& y,
                                   XP_DDFLOAT_TYPE x);
  friend Xp_DoubleDouble operator/(const Xp_DoubleDouble& x,
                                   const Xp_DoubleDouble& y);
  friend Xp_DoubleDouble operator/(const Xp_DoubleDouble& x,
                                   XP_DDFLOAT_TYPE y);

  friend Xp_DoubleDouble recip(const Xp_DoubleDouble& x); // 1/x
  friend Xp_DoubleDouble sqrt(const Xp_DoubleDouble& x); // sqrt(x)
  friend Xp_DoubleDouble recipsqrt(const Xp_DoubleDouble& x); // 1/sqrt(x)
  friend Xp_DoubleDouble fabs(const Xp_DoubleDouble& x);
  friend Xp_DoubleDouble ldexp(const Xp_DoubleDouble& x,int m);
  friend int signbit(Xp_DoubleDouble x); // Returns 1 if x<=-0.0, 0 otherwise
  
  // Friend transcendental functions
  friend void sincos(const Xp_DoubleDouble& x,
                     Xp_DoubleDouble& sinx,Xp_DoubleDouble& cosx);
  friend Xp_DoubleDouble atan(const Xp_DoubleDouble& x);
  friend Xp_DoubleDouble atan2(const Xp_DoubleDouble& y,
                               const Xp_DoubleDouble& x);
  /// Note: Atan2 has slightly lower accuracy than Atan
  friend Xp_DoubleDouble exp(const Xp_DoubleDouble& x);
  friend Xp_DoubleDouble log(const Xp_DoubleDouble& x);
  friend Xp_DoubleDouble expm1(const Xp_DoubleDouble& x); // exp(x)-1
  friend Xp_DoubleDouble log1p(const Xp_DoubleDouble& x); // log(1+x)

  // Friend test classes
  friend class Xp_TestHarness;

  // Miscellaneous
  int IsNormalized() const;
  Xp_DoubleDouble& ReduceModTwoPi();

  void ReadHexFloat(const char* buf,const char*& endptr); // Reads
  /// two hex-float values from buf, makes a DoubleDouble value
  /// from them, and returns a pointer to the first character
  /// beyond.  Throws a std::string on error.

  friend class PowersOfTen;
  template <int N> friend class Base10Digits;
  friend std::string ScientificFormat(const Xp_DoubleDouble& x,
                                      int width, int precision);

  friend Xp_DoubleDouble floor(const Xp_DoubleDouble& x);
  friend Xp_DoubleDouble  ceil(const Xp_DoubleDouble& x);

 private:
#if !XP_DD_USE_SSE
  XP_DDFLOAT_TYPE a0,a1;  // a0 is high word, a1 is low word
#else // XP_DD_USE_SSE
# error SSE-version of Xp_DoubleDouble not implemented
#endif // XP_DD_USE_SSE

  // Fast, non-broken(?) implementation of ldexp
  inline static XP_DDFLOAT_TYPE LdExp(XP_DDFLOAT_TYPE x,int n) {
    return XpLdExp(x,n);
  }

  // Bread-and-butter routines for double-double
  static void OrderedTwoSum(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
           XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v);
  static void OrderedTwoSum(XP_DDFLOAT_TYPE& x,XP_DDFLOAT_TYPE& y);
  static void TwoSum(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
           XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v);
  static void TwoDiff(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
           XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v);
  static void Split(XP_DDFLOAT_TYPE x, // Prep for multiplication
           XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v);
  static void TwoProd(XP_DDFLOAT_TYPE x,XP_DDFLOAT_TYPE y,
           XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v);
  static void SquareProd(XP_DDFLOAT_TYPE x, // Computes x*x
           XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v);
  static void Rescale(const XP_DDFLOAT_TYPE x,const XP_DDFLOAT_TYPE y,
                      const XP_DDFLOAT_TYPE rescale, // power-of-two
                      XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v);
  static void Rescale(const XP_DDFLOAT_TYPE x,const XP_DDFLOAT_TYPE y,
                      const XP_DDFLOAT_TYPE z,
                      const XP_DDFLOAT_TYPE rescale, // power-of-two
                      XP_DDFLOAT_TYPE& u,XP_DDFLOAT_TYPE& v);

  // Coalesce three XP_DDFLOAT_TYPE elements down to two.
  // The imports may overlap, but it is assumed that
  // |a0| >= |a1| >= |a2|.
  static void Coalesce
    (XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,XP_DDFLOAT_TYPE& a2);
  static void CoalescePlus
    (const XP_DDFLOAT_TYPE a0,const XP_DDFLOAT_TYPE a1,
     const XP_DDFLOAT_TYPE a2,XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1);

  // Support code for signed zero handling in multiplication
  // and division routines.
  static XP_DDFLOAT_TYPE GetSignedZero
    (XP_DDFLOAT_TYPE afactor,XP_DDFLOAT_TYPE bfactor);

  // The following are triple-double routines, intended for use inside
  // Xp_DoubleDouble members where a little extra precision is needed.
  // Those marked "sloppy" have errors may be many ULPs (with respect to
  // triple-double accuracy), in contrast to the public double-double
  // code which tries to keep the error under 0.5 ULP (with respect to
  // double-double accuracy).
  static void ThreeSum
    (XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
     XP_DDFLOAT_TYPE b0,XP_DDFLOAT_TYPE b1,XP_DDFLOAT_TYPE b2,
     XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2);
  static void ThreeIncrement
    (XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,XP_DDFLOAT_TYPE& a2,
     XP_DDFLOAT_TYPE b0);   // a += b0
  static void SloppyProd
    (XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
     XP_DDFLOAT_TYPE b0,XP_DDFLOAT_TYPE b1,XP_DDFLOAT_TYPE b2,
     XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2);
  static void SloppySquare
    (XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
     XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2);
  static void SloppySqrt
    (XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
     XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2);
  static void SloppyRecip
    (XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
     XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2);
  static void Floor
    (XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
     XP_DDFLOAT_TYPE& c0,XP_DDFLOAT_TYPE& c1,XP_DDFLOAT_TYPE& c2);
  static void Normalize
    (XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
     XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,XP_DDFLOAT_TYPE& b2);
  static void LdExp10
    (XP_DDFLOAT_TYPE a0,XP_DDFLOAT_TYPE a1,XP_DDFLOAT_TYPE a2,
     int m,
     XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,XP_DDFLOAT_TYPE& b2);
  /// Computes a*10^m
  static void SinCos(const Xp_DoubleDouble& x,
                     Xp_DoubleDouble& sinx,XP_DDFLOAT_TYPE& sinx_a2,
                     Xp_DoubleDouble& cosx,XP_DDFLOAT_TYPE& cosx_a2);

  // Internal-use code for reducing an angle mode 2.pi.  These
  // routines actually compute the remainder/(2.pi), i.e., the result
  // is in the range [-0.5,0.5] (possibly with a little slop about the
  // range edges).  The export from these routines are essentially
  // triple-doubles.  The public member function above multiplies
  // the export by 2.pi and drops the third double.
  const static XP_DDFLOAT_TYPE hires_pi[5];
  const static XP_DDFLOAT_TYPE hires_log2[3];
  const static XP_DDFLOAT_TYPE hires_log2_mant[3];
  const static XP_DDFLOAT_TYPE hires_sqrt2[3];
  static void ReduceModTwoPiBase(XP_DDFLOAT_TYPE x,
           XP_DDFLOAT_TYPE& r0,XP_DDFLOAT_TYPE& r1,XP_DDFLOAT_TYPE& r2);
  static void ReduceModTwoPi(const Xp_DoubleDouble& angle,
           XP_DDFLOAT_TYPE& r0,XP_DDFLOAT_TYPE& r1,XP_DDFLOAT_TYPE& r2);

  // The CircleReduce function reduces import angle mod pi/2, and
  // returns the "centered" quadrant of the import angle, i.e., return
  // value
  //        |r0| <= pi/4,
  //  and
  //       angle = r + quadrant*(pi/2) + m*(2*pi)
  //
  // for some unspecified integer m.  In particular, if angle is say
  // 2*pi - eps, where 0<eps<pi/4, then upon return r = -eps and
  // quadrant = 0 (even though normally one would consider import angle
  // to lie in the third quadrant).
  static void CircleReduce(const Xp_DoubleDouble& angle,
                           XP_DDFLOAT_TYPE& r0,XP_DDFLOAT_TYPE& r1,
                           XP_DDFLOAT_TYPE& r2,int& quadrant);


  // Internal-use code supporting Exp and Expm1.
  static void ExpBase(const Xp_DoubleDouble& inval,
                      Xp_DoubleDouble& outval,XP_DDFLOAT_TYPE& outval_a2,
                      int& outm);

  // Friend support functions
  friend int XpMultiplicationRescale(XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,
                                     XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,
                                     XP_DDFLOAT_TYPE& rescale);
  friend int XpDivisionRescale(XP_DDFLOAT_TYPE& a0,XP_DDFLOAT_TYPE& a1,
                               XP_DDFLOAT_TYPE& b0,XP_DDFLOAT_TYPE& b1,
                               XP_DDFLOAT_TYPE& rescale);
  friend void AuxiliaryDivide(const Xp_DoubleDouble& x,
                              const Xp_DoubleDouble& y,
                              Xp_DoubleDouble& q,XP_DDFLOAT_TYPE& q2);
  friend void AuxiliaryRecip(const Xp_DoubleDouble& x,
                             const XP_DDFLOAT_TYPE& y0,
                             Xp_DoubleDouble& z,XP_DDFLOAT_TYPE& zextra);
  // AuxiliaryRecip computes 1/x, where y0 is the lead term in the
  // export (i.e., z.a0).
  // For both AuxiliaryDivide and AuxiliaryRecip, export must be
  // physically distinct from imports.  
};


std::ostream& operator<<(std::ostream& os,const Xp_DoubleDouble& x);

inline Xp_DoubleDouble sin(const Xp_DoubleDouble& x) {
  Xp_DoubleDouble sinx,cosx;
  sincos(x,sinx,cosx);
  return sinx;
}
inline Xp_DoubleDouble cos(const Xp_DoubleDouble& x) {
  Xp_DoubleDouble sinx,cosx;
  sincos(x,sinx,cosx);
  return cosx;
}

int Xp_FindRatApprox
(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y,
 XP_DDFLOAT_TYPE relerr, XP_DDFLOAT_TYPE maxq,
 Xp_DoubleDouble& p,Xp_DoubleDouble& q);

// Non-SSE implementation //////////////////////////////////////////////
#if !XP_DD_USE_SSE

inline const XP_DDFLOAT_TYPE& Xp_DoubleDouble::Hi() const {
  if(!IsNormalized()) {
    printf("UNNORMALIZED\n");
    printf("Hi: %g\n",double(a0));
    printf("Lo: %g\n",double(a1));
    // std::cerr << "NOT NORMALIZED: " << *this << "\n";
  }
  assert(IsNormalized());
  return a0;
}
inline const XP_DDFLOAT_TYPE& Xp_DoubleDouble::Lo() const {
  assert(IsNormalized());
  return a1;
}

// Explicit down-conversions
inline void Xp_DoubleDouble::DownConvert(double& d) const {
  d = static_cast<double>(a0);  d += static_cast<double>(a1);
}

inline void Xp_DoubleDouble::DownConvert(long double& ld) const {
  ld = static_cast<long double>(a0);
  ld += static_cast<long double>(a1);
}

inline double Xp_DoubleDouble::DownConvert(double* dptr) const {
  double d = static_cast<double>(a0);  d += static_cast<double>(a1);
  if(dptr != nullptr) *dptr = d;
  return d;
}

inline long double Xp_DoubleDouble::DownConvert(long double* ldptr) const {
  long double ld = static_cast<long double>(a0);
  ld += static_cast<long double>(a1);
  if(ldptr != nullptr) *ldptr = ld;
  return ld;
}

// Friend operator functions

inline int Xp_Compare
(const Xp_DoubleDouble& a,
 const Xp_DoubleDouble& b)
{ // Xp_Compare() returns -1 if a<b, 0 if a==b, or 1 if a>b. If one
  // argument is a NaN then this routine probably returns 0, but NaN's
  // aren't reliably handled by all compilers.
  assert(a.IsNormalized() && b.IsNormalized());
  if(a.a0 < b.a0) return -1;
  if(a.a0 > b.a0) return  1;
  // Else a.a0 == b.a0 or NaN
  if(a.a1 < b.a1) return -1;
  if(a.a1 > b.a1) return  1;
  return 0;  // Equal or else at least one is a NaN
}

inline Xp_DoubleDouble operator-
(const Xp_DoubleDouble& x)
{ // unary -
  Xp_DoubleDouble result;
  result.a0 = -x.a0;  result.a1 = -x.a1;
  return result;
}

inline Xp_DoubleDouble operator*
(const Xp_DoubleDouble& y,
 XP_DDFLOAT_TYPE x)
{
  return x*y;
}

#if XP_USE_FLOAT_TYPE != XP_LONGDOUBLE_TYPE
// XP_DDFLOAT_TYPE is not long double
inline Xp_DoubleDouble operator*(long double x,const Xp_DoubleDouble& y)
{
  return Xp_DoubleDouble(x)*y;
}
inline Xp_DoubleDouble operator*(const Xp_DoubleDouble& y,long double x)
{
  return x*y;
}
#endif // XP_USE_FLOAT_TYPE != XP_LONGDOUBLE_TYPE

#if XP_USE_FLOAT_TYPE != XP_DOUBLE_TYPE
// XP_DDFLOAT_TYPE is not double
inline Xp_DoubleDouble operator*(double x,const Xp_DoubleDouble& y)
{
  return Xp_DoubleDouble(x)*y;
}
inline Xp_DoubleDouble operator*(const Xp_DoubleDouble& y,double x)
{
  return x*y;
}
#endif // XP_USE_FLOAT_TYPE != XP_DOUBLE_TYPE

inline Xp_DoubleDouble operator*
(OC_INT4 ix,const Xp_DoubleDouble& y)
{
  return static_cast<XP_DDFLOAT_TYPE>(ix)*y;
}

inline Xp_DoubleDouble operator*
(const Xp_DoubleDouble& x,OC_INT4 iy)
{ return iy*x; }

inline Xp_DoubleDouble operator*
(OC_INT8 ix,const Xp_DoubleDouble& y)
{
  return static_cast<XP_DDFLOAT_TYPE>(ix)*y;
}

inline Xp_DoubleDouble operator*
(const Xp_DoubleDouble& x,OC_INT8 iy)
{ return iy*x; }


inline Xp_DoubleDouble operator/
(const Xp_DoubleDouble& x,
 const Xp_DoubleDouble& y)
{
  Xp_DoubleDouble q;  XP_DDFLOAT_TYPE q2;
  AuxiliaryDivide(x,y,q,q2);
  q.a1 += q2;
#if XP_RANGE_CHECK
  if(Xp_IsFinite(q.a0) && q.a0 == 0.0) {
    q.a0 = q.a1 = Xp_DoubleDouble::GetSignedZero(x.a0,y.a0);
  }
#endif
  return q;
}

inline void sincos
(const Xp_DoubleDouble& x,
 Xp_DoubleDouble& sinx,Xp_DoubleDouble& cosx)
{
  XP_DDFLOAT_TYPE dummy_sinx_a2,dummy_cosx_a2; // These get dropped
  Xp_DoubleDouble::SinCos(x,sinx,dummy_sinx_a2,cosx,dummy_cosx_a2);
  // Note: CamelCase version is triple-double precision for class
  // internal use.
  return;
}

inline Xp_DoubleDouble fabs(const Xp_DoubleDouble& x)
{
  assert(x.IsNormalized());
  if(x.a0<0) {
    return Xp_DoubleDouble(-1*x.a0,-1*x.a1);
  }
  return x;
}

inline Xp_DoubleDouble ldexp(const Xp_DoubleDouble& x,int m)
{
  Xp_DoubleDouble result; // No need to normalize, so use
  result.a0 = XpLdExp(x.a0,m); // default constructor.
  result.a1 = XpLdExp(x.a1,m);
  return result;
}

inline int signbit(Xp_DoubleDouble x)
{ // Returns 1 if x<=-0.0, 0 otherwise
  return signbit(x.a0);
}

std::string ScientificFormat(const Xp_DoubleDouble& x,
                             int width,int precision);

inline Xp_DoubleDouble floor(const Xp_DoubleDouble& x) {
  XP_DDFLOAT_TYPE a0 = XP_FLOOR(x.a0);
  XP_DDFLOAT_TYPE a0r = x.a0 - a0;
  XP_DDFLOAT_TYPE a1 = XP_FLOOR(x.a1);
  XP_DDFLOAT_TYPE a1r = x.a1 - a1;
  Xp_DoubleDouble b(a0,a1);    // This is an integer
  Xp_DoubleDouble br(a0r,a1r); // Add remainders
  b += XP_FLOOR(br.a0);        // Adjust for remainder sum
  return b;
}

inline Xp_DoubleDouble ceil(const Xp_DoubleDouble& x) {
  XP_DDFLOAT_TYPE a0 = XP_CEIL(x.a0);
  XP_DDFLOAT_TYPE a0r = x.a0 - a0;  // This is <=0
  XP_DDFLOAT_TYPE a1 = XP_CEIL(x.a1);
  XP_DDFLOAT_TYPE a1r = x.a1 - a1;  // Also <=0
  Xp_DoubleDouble b(a0,a1);    // This is an integer
  Xp_DoubleDouble br(a0r,a1r); // Add remainders
  b += XP_CEIL(br.a0);         // Adjust for remainder sum
  return b;
}

// Boolean comparison operators.  NaN behavior is undefined.
// NB: These are not "friends".
inline int operator<(const Xp_DoubleDouble& a,const Xp_DoubleDouble& b)
{
  return  (Xp_Compare(a,b)<0);
}
inline int operator<=(const Xp_DoubleDouble& a,const Xp_DoubleDouble& b)
{
  return  (Xp_Compare(a,b)<=0);
}
inline int operator>(const Xp_DoubleDouble& a,const Xp_DoubleDouble& b)
{
  return  (Xp_Compare(a,b)>0);
}
inline int operator>=(const Xp_DoubleDouble& a,const Xp_DoubleDouble& b)
{
  return  (Xp_Compare(a,b)>=0);
}
inline int operator==(const Xp_DoubleDouble& a,const Xp_DoubleDouble& b)
{
  return  (Xp_Compare(a,b)==0);
}
inline int operator!=(const Xp_DoubleDouble& a,const Xp_DoubleDouble& b)
{
  return  (Xp_Compare(a,b)!=0);
}

// SSE implementation //////////////////////////////////////////////////
#else // XP_DD_USE_SSE
// Awaiting construction...
# error SSE-version of Xp_DoubleDouble not implemented
#endif // XP_DD_USE_SSE

#endif /* _XP_DOUBLEDOUBLE */
