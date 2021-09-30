/* FILE: alt_doubledouble.h
 * Header file for alternative Xp_DoubleDouble class,
 * which is a simple wrapper for a user-defined floating
 * point type (usually long double or long long double).
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * NOTE: Code external to this package should NOT #include
 * this file directly.  Instead, use
 *     #include "xp.h"
 * which includes logic to automatically include either
 * doubledouble.h or alt_doubledouble.h, as appropriate.
 */

#ifndef _XP_DOUBLEDOUBLE
#define _XP_DOUBLEDOUBLE

#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <iomanip>
#include <iostream>
#include <string>

#include "xpport.h" // Various compiler and compiler-option specific values.
#include "oc.h"     // #defines of floor and ceil in ocport.h don't play nice!

#include "xpcommon.h"

/* End includes */

#define XP_DOUBLEDOUBLE_PRECISION (XP_DDFLOAT_MANTISSA_PRECISION)

class Xp_DoubleDouble {
private:
  const static XP_DDFLOAT_TYPE REF_ZERO;

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
  Xp_DoubleDouble(XP_DDFLOAT_TYPE x) : a(x) {}
  Xp_DoubleDouble(XP_DDFLOAT_TYPE xhi,XP_DDFLOAT_TYPE xlo)
    : a(xhi+ xlo) {}
#if XP_USE_FLOAT_TYPE != XP_FLOAT_TYPE
  // XP_DDFLOAT_TYPE is not float
  Xp_DoubleDouble(float x) : a(x) {}
  Xp_DoubleDouble(float xhi,float xlo)
    : a(xhi+xlo) {}
#endif // XP_USE_FLOAT_TYPE
#if XP_USE_FLOAT_TYPE != XP_DOUBLE_TYPE
  // XP_DDFLOAT_TYPE is not double
  Xp_DoubleDouble(double x) : a(x) {}
  Xp_DoubleDouble(double xhi,double xlo)
    : a(xhi+xlo) {}
#endif // XP_USE_FLOAT_TYPE
#if XP_USE_FLOAT_TYPE != XP_LONGDOUBLE_TYPE
  // XP_DDFLOAT_TYPE is not long double
  Xp_DoubleDouble(long double x) : a(x) {}
  Xp_DoubleDouble(long double xhi,long double xlo)
    : a(xhi+xlo) {}
#endif // XP_USE_FLOAT_TYPE

  // Integer constructors.  These may be needed depending on what
  // constructors the underlying base type defines.
  Xp_DoubleDouble(OC_INT4 ix)  : a(ix) {}
  Xp_DoubleDouble(OC_UINT4 ix) : a(ix) {}
  Xp_DoubleDouble(OC_INT8 ix)  : a(ix) {}
  Xp_DoubleDouble(OC_UINT8 ix) : a(ix) {}
#if !defined(OC_INT4_IS_LONG) && !defined(OC_INT8_IS_LONG)
  // On some platforms OC_INT4 will be int and OC_INT8 will be long long
  // or int64, in which case the above don't define a constructor for long.
  Xp_DoubleDouble(long ix) : a(ix) {}
  Xp_DoubleDouble(unsigned long ix) : a(ix) {}
#endif

  // High precision constructor
  Xp_DoubleDouble(const Xp_BigFloatVec& bv)
    : a(Xp_UnsplitBigFloat<XP_DDFLOAT_TYPE>(bv)) {}

  // Use default destructor, copy constructor, copy-assignment operator

  // Routine that returns the width of the mantissa, in bits.
  static int GetMantissaWidth () {
    return XP_DOUBLEDOUBLE_PRECISION;
  }
  // Note: I wanted to implement this routine as
  //   std::numeric_limits<Xp_DoubleDouble>::digits
  // but if there are any functions taking a mfpr type as a parameter
  // then g++ 7.3.0 using BOOST/MPFR 1.66/4.0.1-p6 complains
  //
  //   error: specialization of 'std::numeric_limits<Xp_DoubleDouble>'
  //   after instantiation
  //     template<> class numeric_limits<Xp_DoubleDouble> {
  //                      ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //   alt_doubledouble.h:343:26: error: redefinition of 'class
  //   std::numeric_limits<Xp_DoubleDouble>' In file included from
  //   <...>/oommf/pkg/xp/alt_doubledouble.cc:20:0:
  //   <...>/include/c++/limits:312:12: note: previous definition of
  //   'class std::numeric_limits<Xp_DoubleDouble>' struct
  //   numeric_limits : public __numeric_limits_base

  const XP_DDFLOAT_TYPE& Hi() const { return a; } // Returns high word
  const XP_DDFLOAT_TYPE& Lo() const { return REF_ZERO; } // Returns low word

  void DebugBits(XP_DDFLOAT_TYPE& hi,XP_DDFLOAT_TYPE& lo) const {
    // Returns parts w/o any sanity checks.  Intended for use
    // in error handling code only.
    hi=a;  lo=REF_ZERO;
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
  void DownConvert(double& d) const { d = static_cast<double>(a); }
  void DownConvert(long double& ld) const { ld = static_cast<long double>(a); }
  double DownConvert(double* dptr) const {
    double tmp = static_cast<double>(a);
    if(dptr != nullptr) *dptr = tmp;
    return tmp;
  }
  long double DownConvert(long double* ldptr) const { 
    double tmp = static_cast<long double>(a);
    if(ldptr != nullptr) *ldptr = tmp;
    return tmp;
  }

  // Size of unit-in-the-last-place.
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
  Xp_DoubleDouble& Square() {
    a *= a;
    return *this;
  }


  // Comparison operations
  OC_BOOL IsPos() {
    return (a>0.0);
  }
  OC_BOOL IsNeg() {
    return (a<0.0);
  }
  OC_BOOL IsZero() {
    return (a == static_cast<XP_DDFLOAT_TYPE>(0.0));
  }
  friend int Xp_Compare(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y);
  /// Compare returns -1 if x<y, 0 if x==y, or 1 if x>y.  NaN
  /// behavior undefined.

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
  friend Xp_DoubleDouble operator/(XP_DDFLOAT_TYPE x,
                                   const Xp_DoubleDouble& y);

  // Friend support functions
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
  int IsNormalized() const { return 1; }
  Xp_DoubleDouble& ReduceModTwoPi();

  void ReadHexFloat(const char* buf,const char*& endptr); // Reads
  /// two hex-float values from buf, makes a DoubleDouble value
  /// from them, and returns a pointer to the first character
  /// beyond.  Throws a std::string on error.

  friend std::string ScientificFormat(const Xp_DoubleDouble& x,
                                      int width,int precision);

  friend Xp_DoubleDouble floor(const Xp_DoubleDouble& x);
  friend Xp_DoubleDouble  ceil(const Xp_DoubleDouble& x);

 private:
  XP_DDFLOAT_TYPE a;

  // Fast, non-broken(?) implementation of ldexp
  inline static XP_DDFLOAT_TYPE LdExp(XP_DDFLOAT_TYPE x,int n) {
    return XpLdExp(x,n);
  }
};

inline Xp_DoubleDouble fabs(const Xp_DoubleDouble& x)
{ return Xp_DoubleDouble(fabs(x.a)); }

inline Xp_DoubleDouble ldexp(const Xp_DoubleDouble& x,int m)
{ return Xp_DoubleDouble(XpLdExp(x.a,m)); }

inline int signbit(Xp_DoubleDouble x)
{ // Returns 1 if x<=-0.0, 0 otherwise
  // There is a possibility of infinite recursion if signbit is not
  // defined for x.a.  Protect against this by using a namespace
  // resolved signbit function.
#if XP_USE_FLOAT_TYPE == XP_MPFR_TYPE
  return boost::math::signbit(x.a);
#else
  return std::signbit(x.a);
#endif
}

std::string ScientificFormat(const Xp_DoubleDouble& x,
                             int width,int precision);

inline Xp_DoubleDouble floor(const Xp_DoubleDouble& x) {
  return Xp_DoubleDouble(XP_FLOOR(x.a));
}

inline Xp_DoubleDouble ceil(const Xp_DoubleDouble& x) {
  return Xp_DoubleDouble(XP_CEIL(x.a));
}

std::ostream& operator<<(std::ostream& os,const Xp_DoubleDouble& x);

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

inline Xp_DoubleDouble sin(const Xp_DoubleDouble& x) {
  Xp_DoubleDouble y = x;
  return Xp_DoubleDouble(sin(y.Hi()));
}
inline Xp_DoubleDouble cos(const Xp_DoubleDouble& x) {
  Xp_DoubleDouble y = x;
  return Xp_DoubleDouble(cos(y.Hi()));
}
int Xp_FindRatApprox
(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y,
 XP_DDFLOAT_TYPE relerr, XP_DDFLOAT_TYPE maxq,
 Xp_DoubleDouble& p,Xp_DoubleDouble& q);

// Friend functions
inline int Xp_Compare
(const Xp_DoubleDouble& x,
 const Xp_DoubleDouble& y)
{ // Xp_Compare() returns -1 if x<y, 0 if x==y, or 1 if x>y. If one
  // argument is a NaN then this routine probably returns 0, but NaN's
  // aren't reliably handled by all compilers.
  if(x.a < y.a) return -1;
  if(x.a > y.a) return  1;
  return 0;  // Equal or else at least one is a NaN
}

inline Xp_DoubleDouble operator-(const Xp_DoubleDouble& x) // unary -
{ return Xp_DoubleDouble(-x.a); }

inline Xp_DoubleDouble operator+(const Xp_DoubleDouble& x,
                                 const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x.a+y.a); }

inline Xp_DoubleDouble operator-(const Xp_DoubleDouble& x,
                                 const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x.a-y.a); }


inline Xp_DoubleDouble operator*(const Xp_DoubleDouble& x,
                                 const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x.a*y.a); }


inline Xp_DoubleDouble operator*(XP_DDFLOAT_TYPE x,
                                 const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x*y.a); }

inline Xp_DoubleDouble operator*(const Xp_DoubleDouble& y,
                                 XP_DDFLOAT_TYPE x)
{ return Xp_DoubleDouble(y.a*x); }

inline Xp_DoubleDouble operator/(const Xp_DoubleDouble& x,
                                 const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x.a/y.a); }


inline Xp_DoubleDouble operator/(const Xp_DoubleDouble& x,
                                 XP_DDFLOAT_TYPE y)
{ return Xp_DoubleDouble(x.a/y); }

inline Xp_DoubleDouble operator/(XP_DDFLOAT_TYPE x,
                                 const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x/y.a); }


// Common useful overloads.  These are needed depending on what
// conversion operators are defined for the underlying base type.
inline Xp_DoubleDouble operator*(OC_INT4 x,const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x*y.Hi()); }
inline Xp_DoubleDouble operator*(const Xp_DoubleDouble& y,OC_INT4 x)
{ return Xp_DoubleDouble(y.Hi()*x); }
inline Xp_DoubleDouble operator/(OC_INT4 x,const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x/y.Hi()); }
inline Xp_DoubleDouble operator/(const Xp_DoubleDouble& y,OC_INT4 x)
{ return Xp_DoubleDouble(y.Hi()/x); }

inline Xp_DoubleDouble operator*(OC_INT8 x,const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x*y.Hi()); }
inline Xp_DoubleDouble operator*(const Xp_DoubleDouble& y,OC_INT8 x)
{ return Xp_DoubleDouble(y.Hi()*x); }
inline Xp_DoubleDouble operator/(OC_INT8 x,const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x/y.Hi()); }
inline Xp_DoubleDouble operator/(const Xp_DoubleDouble& y,OC_INT8 x)
{ return Xp_DoubleDouble(y.Hi()/x); }

#if XP_USE_FLOAT_TYPE != XP_DOUBLE_TYPE
inline Xp_DoubleDouble operator*(double x,const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x*y.Hi()); }
inline Xp_DoubleDouble operator*(const Xp_DoubleDouble& y,double x)
{ return Xp_DoubleDouble(y.Hi()*x); }
inline Xp_DoubleDouble operator/(double x,const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x/y.Hi()); }
inline Xp_DoubleDouble operator/(const Xp_DoubleDouble& y,double x)
{ return Xp_DoubleDouble(y.Hi()/x); }
#endif

#if XP_USE_FLOAT_TYPE != XP_LONGDOUBLE_TYPE
inline Xp_DoubleDouble operator*(long double x,const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x*y.Hi()); }
inline Xp_DoubleDouble operator*(const Xp_DoubleDouble& y,long double x)
{ return Xp_DoubleDouble(y.Hi()*x); }
inline Xp_DoubleDouble operator/(long double x,const Xp_DoubleDouble& y)
{ return Xp_DoubleDouble(x/y.Hi()); }
inline Xp_DoubleDouble operator/(const Xp_DoubleDouble& y,long double x)
{ return Xp_DoubleDouble(y.Hi()/x); }
#endif

#endif /* _XP_DOUBLEDOUBLE */
