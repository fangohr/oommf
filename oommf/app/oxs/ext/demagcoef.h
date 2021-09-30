/* FILE: demagcoef.h            -*-Mode: c++-*-
 *
 * Demag coefficients.
 *
 * Constant magnetization demag routines, based on formulae presented in
 * "A Generalization of the Demagnetizing Tensor for Nonuniform
 * Magnetization," by Andrew J. Newell, Wyn Williams, and David
 * J. Dunlop, Journal of Geophysical Research, vol 98, p 9551-9555, June
 * 1993.  This formulae clearly satisfy necessary symmetry and scaling
 * properties, which is not true of the formulae presented in
 * "Magnetostatic Energy Calculations in Two- and Three-Dimensional
 * Arrays of Ferromagnetic Prisms," M. Maicas, E. Lopez, M. CC. Sanchez,
 * C. Aroca and P. Sanchez, IEEE Trans Mag, vol 34, May 1998, p601-607.
 * (Side note: The units in the latter paper are apparently cgs.)  It
 * appears likely that there is an error in the latter paper (attempts
 * to implement the formulae presented there did not produce the proper
 * symmetries), as well as in the older paper, "Magnetostatic
 * Interaction Fields for a Three-Dimensional Array of Ferromagnetic
 * Cubes," Manfred E. Schabes and Amikam Aharoni, IEEE Trans Mag, vol
 * 23, November 1987, p3882-3888.  (Note: The Newell paper deals with
 * uniformly sized rectangular prisms, the Maicas paper allows
 * non-uniformly sized rectangular prisms, and the Schabes paper only
 * considers cubes.)
 *
 *   The kernel here is based on an analytically derived energy, and the
 * effective (discrete) demag field is calculated from the (discrete)
 * energy.
 *
 */

#ifndef _OXS_DEMAGCOEF
#define _OXS_DEMAGCOEF

// The "STANDALONE" blocks provide support for building this file with
// ../demagtensor.cc and demagcoef.h as a standalone application for
// testing and development.
#ifndef STANDALONE
# define STANDALONE 0
#endif

#include <cassert>
#include <vector>

#if !STANDALONE ////////////////////////////////////////////////////////
# include "nb.h"
# include "xp.h"
# include "oxsarray.h"  // Oxs_3DArray
#else // STANDALONE ////////////////////////////////////////////////////
# include <stdexcept>
# include <iostream>
using namespace std;  // Get automatic type overloading on standard
                      // library functions.

typedef long OC_INDEX;
typedef int  OC_INT4m;
typedef double OC_REAL8m;

#define OXS_THROW(x,y) fputs(y,stderr)

#ifndef OC_USE_SSE
# define OC_USE_SSE 0
#endif

#ifndef OXS_DEMAG_ASYMP_USE_SSE
# if OC_USE_SSE
#  define OXS_DEMAG_ASYMP_USE_SSE 1
   /// Assumes OXS_DEMAG_REAL_ASYMP is OC_REAL8m is double!
# else
#  define OXS_DEMAG_ASYMP_USE_SSE 0
# endif
#endif

// Dummy wrapper of long double as Xp_DoubleDouble.  Of course, this
// does not have the mantissa precision of the real Xp_DoubleDouble,
// and is provided solely for use in STANDALONE builds of this file.

typedef long double XP_DDFLOAT_TYPE;

class Xp_DoubleDouble {
public:
  // Reference values
  const static Xp_DoubleDouble DD_SQRT2;
  const static Xp_DoubleDouble DD_PI;

  Xp_DoubleDouble() {}
  Xp_DoubleDouble(XP_DDFLOAT_TYPE x) : a(x) {}
  Xp_DoubleDouble(XP_DDFLOAT_TYPE xhi,XP_DDFLOAT_TYPE xlo)
    : a(xhi+ xlo) {}

  const XP_DDFLOAT_TYPE Hi() const { return a; } // Returns high word
  const XP_DDFLOAT_TYPE Lo() const { return 0; } // Returns low word

  Xp_DoubleDouble& operator+=(const Xp_DoubleDouble& x) {
    a += x.a;
    return *this;
  }
  Xp_DoubleDouble& operator-=(const Xp_DoubleDouble& x) {
    a -= x.a;
    return *this;
  }
  Xp_DoubleDouble& operator*=(const Xp_DoubleDouble& x) {
    a *= x.a;
    return *this;
  }
  Xp_DoubleDouble& operator/=(const Xp_DoubleDouble& x) {
    a /= x.a;
    return *this;
  }
  Xp_DoubleDouble& operator/=(const XP_DDFLOAT_TYPE x) {
    a /= x;
    return *this;
  }

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

  friend Xp_DoubleDouble fabs(Xp_DoubleDouble x);
  friend Xp_DoubleDouble log(Xp_DoubleDouble x);
  friend Xp_DoubleDouble sqrt(Xp_DoubleDouble x);
  friend Xp_DoubleDouble atan(Xp_DoubleDouble x);
  friend Xp_DoubleDouble atan2(Xp_DoubleDouble y,Xp_DoubleDouble x);
  friend Xp_DoubleDouble log1p(Xp_DoubleDouble x);
  friend int signbit(Xp_DoubleDouble x);

  friend int Xp_Compare(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y);
  /// Compare returns -1 if x<y, 0 if x==y, or 1 if x>y.  NaN
  /// behavior undefined.

private:
  XP_DDFLOAT_TYPE a;
};

inline std::ostream& operator<<(std::ostream& os,const Xp_DoubleDouble& x)
{
  os << x.Hi();
  return os;
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


inline int Xp_Compare(const Xp_DoubleDouble& x,const Xp_DoubleDouble& y)
{ // Xp_Compare() returns -1 if x<y, 0 if x==y, or 1 if x>y. If one
  // argument is a NaN then this routine probably returns 0, but NaN's
  // aren't reliably handled by all compilers.
  if(x.a < y.a) return -1;
  if(x.a > y.a) return  1;
  return 0;  // Equal or else at least one is a NaN
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

// Math routines; std:: version self-select by type
// Note: With some compilers, #include <cmath> without a "using"
// statement will cause math library calls to revert to the old C
// standard where the argument(s) is converted to a double and the
// double version of the routine is called.  OTOH, explicitly calling
// for example std::sin(x) with x a type other than float, double, or
// long double requires adding an extension to the std namespace.
using std::atan;
using std::atan2;
using std::ceil;
using std::exp;
using std::expm1;
using std::fabs;
using std::floor;
using std::log;
using std::log1p;
using std::pow;
using std::sqrt;
using std::signbit;  // Return 1 if import is negative, 0 if positive

// Standard version of atan2 throws a domain error if y=x=0.
// Oc_Atan2 is modeled on the fp_atan2 function that is
// defined at y=x=0 depending on the sign of x and y:
//      y     x     Oc_Atan2(y,x)
//    +0.0  +0.0      atan2(y,1) which should be +0.0
//    -0.0  +0.0      atan2(y,1) which should be -0.0
//    +0.0  -0.0      atan2(y,-1) which should be +pi
//    +0.0  -0.0      atan2(y,-1) which should be -pi
template<typename T>
T Oc_Atan2(T y,T x) {
  if(y!=0 || x!=0) return atan2(y,x);
  if(signbit(x)) { // x == -0.0
    return atan2(y,-1);
  }
  return atan2(y,1);
}

inline Xp_DoubleDouble fabs(Xp_DoubleDouble x)
 { return Xp_DoubleDouble(fabs(x.a)); }
inline Xp_DoubleDouble  log(Xp_DoubleDouble x)
 { return Xp_DoubleDouble(log(x.a)); }
inline Xp_DoubleDouble  log1p(Xp_DoubleDouble x)
 { return Xp_DoubleDouble(log1p(x.a)); }
inline Xp_DoubleDouble sqrt(Xp_DoubleDouble x)
 { return Xp_DoubleDouble(sqrt(x.a)); }
inline Xp_DoubleDouble atan(Xp_DoubleDouble x)
 { return Xp_DoubleDouble(atan(x.a)); }
inline Xp_DoubleDouble atan2(Xp_DoubleDouble y,Xp_DoubleDouble x)
 { return Xp_DoubleDouble(atan2(y.a,x.a)); }
inline int signbit(Xp_DoubleDouble x)
 { return signbit(x.a); }

typedef Xp_DoubleDouble OC_REALWIDE;

OC_REALWIDE Oc_Nop(OC_REALWIDE);  // Declaration; definition is a end of file.

#if OXS_DEMAG_ASYMP_USE_SSE
// Wrapper for SSE __m128d object
#include <emmintrin.h>
class Oc_Duet {
public:
  Oc_Duet& Set(double a,double b) { value = _mm_set_pd(b,a); return *this; }
  // Stores "a" in low (first) half of value, "b" in high (second) half.

  Oc_Duet& Set(double x) { value = _mm_set1_pd(x); return *this; } // a=b=x
  Oc_Duet(double a,double b) { Set(a,b); }
  Oc_Duet(double x) { Set(x); }
  Oc_Duet(const __m128d& newval) : value(newval) {}
  Oc_Duet(const Oc_Duet& other) : value(other.value) {}
  Oc_Duet() {}

  Oc_Duet& operator=(const Oc_Duet &other)
   { value = other.value; return *this; }
  Oc_Duet& SetZero()
   { value = _mm_setzero_pd(); return *this; }
  Oc_Duet& operator+=(const Oc_Duet& other)
   { value = _mm_add_pd(value,other.value); return *this; }
  Oc_Duet& operator+=(const double& x)
   { value = _mm_add_pd(value,_mm_set1_pd(x)); return *this; }
  Oc_Duet& operator-=(const Oc_Duet& other)
   { value = _mm_sub_pd(value,other.value); return *this; }
  Oc_Duet& operator-=(const double& x)
   { value = _mm_sub_pd(value,_mm_set1_pd(x)); return *this; }
  Oc_Duet& operator*=(const Oc_Duet& other)
   { value = _mm_mul_pd(value,other.value); return *this; }
  Oc_Duet& operator*=(const double& x)
   { value = _mm_mul_pd(value,_mm_set1_pd(x)); return *this; }
  Oc_Duet& operator/=(const Oc_Duet& other)
   { value = _mm_div_pd(value,other.value); return *this; }
  Oc_Duet& operator/=(const double& x)
   { value = _mm_div_pd(value,_mm_set1_pd(x)); return *this; }

  // Use GetA/GetB for temporary use, but StoreA/StoreB
  // are more efficient for permanent storage.
  double GetA() const { return ((double *)&value)[0]; }
  double GetB() const { return ((double *)&value)[1]; }

  void StoreA(double& Aout) const { _mm_store_sd(&Aout,value); }
  void StoreB(double& Bout) const { _mm_storeh_pd(&Bout,value); }

  friend const Oc_Duet operator+(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator+(double x,const Oc_Duet& w);
  friend const Oc_Duet operator+(const Oc_Duet& w,double x);

  friend const Oc_Duet operator-(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator-(double x,const Oc_Duet& w);
  friend const Oc_Duet operator-(const Oc_Duet& w,double x);

  friend const Oc_Duet operator*(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator*(double x,const Oc_Duet& w);
  friend const Oc_Duet operator*(const Oc_Duet& w,double x);

  friend const Oc_Duet operator/(const Oc_Duet& w1,const Oc_Duet& w2);
  friend const Oc_Duet operator/(double x,const Oc_Duet& w);
  friend const Oc_Duet operator/(const Oc_Duet& w,double x);

  friend const Oc_Duet sqrt(const Oc_Duet& w);

private:
  __m128d value;
};

inline const Oc_Duet operator+(const Oc_Duet& w1,const Oc_Duet& w2)
 { return Oc_Duet(_mm_add_pd(w1.value,w2.value)); }
inline const Oc_Duet operator+(double x,const Oc_Duet& w)
 { return Oc_Duet(_mm_add_pd(_mm_set1_pd(x),w.value)); }
inline const Oc_Duet operator+(const Oc_Duet& w,double x)
 { return Oc_Duet(_mm_add_pd(w.value,_mm_set1_pd(x))); }

inline const Oc_Duet operator-(const Oc_Duet& w1,const Oc_Duet& w2)
 { return Oc_Duet(_mm_sub_pd(w1.value,w2.value)); }
inline const Oc_Duet operator-(double x,const Oc_Duet& w)
 { return Oc_Duet(_mm_sub_pd(_mm_set1_pd(x),w.value)); }
inline const Oc_Duet operator-(const Oc_Duet& w,double x)
 { return Oc_Duet(_mm_sub_pd(w.value,_mm_set1_pd(x))); }

inline const Oc_Duet operator*(const Oc_Duet& w1,const Oc_Duet& w2)
 { return Oc_Duet(_mm_mul_pd(w1.value,w2.value)); }
inline const Oc_Duet operator*(double x,const Oc_Duet& w)
 { return Oc_Duet(_mm_mul_pd(_mm_set1_pd(x),w.value)); }
inline const Oc_Duet operator*(const Oc_Duet& w,double x)
 { return Oc_Duet(_mm_mul_pd(w.value,_mm_set1_pd(x))); }

inline const Oc_Duet operator/(const Oc_Duet& w1,const Oc_Duet& w2)
 { return Oc_Duet(_mm_div_pd(w1.value,w2.value)); }
inline const Oc_Duet operator/(double x,const Oc_Duet& w)
 { return Oc_Duet(_mm_div_pd(_mm_set1_pd(x),w.value)); }
inline const Oc_Duet operator/(const Oc_Duet& w,double x)
 { return Oc_Duet(_mm_div_pd(w.value,_mm_set1_pd(x))); }


inline const Oc_Duet sqrt(const Oc_Duet& w)
 { return Oc_Duet(_mm_sqrt_pd(w.value)); }

#endif // OXS_DEMAG_ASYMP_USE_SSE


#ifndef PI
# define PI       OC_REAL8m(3.141592653589793238462643383279502884L)
#endif

#ifndef WIDE_PI
# define WIDE_PI  3.141592653589793238462643383279502884L
#endif

// Dummy implementation of Oxs_3DArray
template<class T> class Oxs_3DArray
{
private:
  OC_INDEX dim1,dim2,dim3;
  OC_INDEX dim12;  // = dim1*dim2
  T* objs;

public:
  void Free() {
    if(objs) {
      delete[] objs;
      objs = 0;
    }
    dim1 = dim2 = dim3 = dim12 = 0;
  }

  void SetDimensions(OC_INDEX new_dim1,OC_INDEX new_dim2,OC_INDEX new_dim3) {
    if(new_dim1<0 || new_dim2<0 || new_dim3<0) {
      throw std::runtime_error("Negative dimension request.");
    }
    Free();
    dim1 = new_dim1;  dim2 = new_dim2;  dim3 = new_dim3;
    dim12 = dim1*dim2;
    objs = new T[dim12*dim3];
  }

  OC_INDEX GetDim1() const { return dim1; }
  OC_INDEX GetDim2() const { return dim2; }
  OC_INDEX GetDim3() const { return dim3; }

  Oxs_3DArray(OC_INDEX idim1=0,OC_INDEX idim2=0,OC_INDEX idim3=0)
    : dim1(0), dim2(0), dim3(0), dim12(0), objs(0) {
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
      throw std::runtime_error("Array index out-of-bounds.");
    }
    return objs[dim12*k+j*dim1+i];
  }
  const T& operator()(OC_INDEX i,OC_INDEX j,OC_INDEX k) const {
    if(i<0 || i>=dim1 || j<0 || j>=dim2  || k<0 || k>=dim3 ) {
      throw std::runtime_error("Array index out-of-bounds.");
    }
    return objs[dim12*k+j*dim1+i];
  }
  /// Ranged checked versions.
#endif

  // Use pointer access with care.  There is no bounds checking and the
  // client is responsible for insuring proper indexing.
  T* GetArrBase() const { return objs; }
};

#endif // STANDALONE ///////////////////////////////////////////////////

/* End includes */

// Variable type for analytic computations
#if !STANDALONE
typedef Xp_DoubleDouble OXS_DEMAG_REAL_ANALYTIC;
# define OXS_DEMAG_REAL_ANALYTIC_PI (Xp_DoubleDouble::DD_PI)
# define OXS_DEMAG_REAL_ANALYTIC_SQRT2 (Xp_DoubleDouble::DD_SQRT2)
#else //STANDALONE
typedef Xp_DoubleDouble OXS_DEMAG_REAL_ANALYTIC;
# define OXS_DEMAG_REAL_ANALYTIC_PI     3.141592653589793238462643383279502884L
# define OXS_DEMAG_REAL_ANALYTIC_SQRT2  1.414213562373095048801688724209698079L
#endif // STANDALONE

// If OXS_DEMAG_REAL_ASYMP is anything other than OC_REAL8m,
// then OXS_DEMAG_ASYMP_USE_SSE should be forced to 0
typedef OC_REAL8m OXS_DEMAG_REAL_ASYMP;

#ifndef OXS_DEMAG_ASYMP_USE_SSE
# if OC_USE_SSE
#  define OXS_DEMAG_ASYMP_USE_SSE 1
   /// Assumes OXS_DEMAG_REAL_ASYMP is OC_REAL8m!
# else
#  define OXS_DEMAG_ASYMP_USE_SSE 0
# endif
#endif

// Maybe these routines should be wrapped up in a class?

OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_f(OXS_DEMAG_REAL_ANALYTIC x,
             OXS_DEMAG_REAL_ANALYTIC y,
             OXS_DEMAG_REAL_ANALYTIC z);

OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_g(OXS_DEMAG_REAL_ANALYTIC x,
             OXS_DEMAG_REAL_ANALYTIC y,
             OXS_DEMAG_REAL_ANALYTIC z);

inline OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_f_xx(OXS_DEMAG_REAL_ANALYTIC x,
                OXS_DEMAG_REAL_ANALYTIC y,
                OXS_DEMAG_REAL_ANALYTIC z) {
  return Oxs_Newell_f(x,y,z);
}
inline OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_f_yy(OXS_DEMAG_REAL_ANALYTIC x,
                OXS_DEMAG_REAL_ANALYTIC y,
                OXS_DEMAG_REAL_ANALYTIC z) {
  return Oxs_Newell_f(y,x,z);
}
inline OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_f_zz(OXS_DEMAG_REAL_ANALYTIC x,
                OXS_DEMAG_REAL_ANALYTIC y,
                OXS_DEMAG_REAL_ANALYTIC z) {
  return Oxs_Newell_f(z,y,x);
}
inline OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_g_xy(OXS_DEMAG_REAL_ANALYTIC x,
                OXS_DEMAG_REAL_ANALYTIC y,
                OXS_DEMAG_REAL_ANALYTIC z) {
  return Oxs_Newell_g(x,y,z);
}
inline OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_g_xz(OXS_DEMAG_REAL_ANALYTIC x,
                OXS_DEMAG_REAL_ANALYTIC y,
                OXS_DEMAG_REAL_ANALYTIC z) {
  return Oxs_Newell_g(x,z,y);
}
inline OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_g_yz(OXS_DEMAG_REAL_ANALYTIC x,
                OXS_DEMAG_REAL_ANALYTIC y,
                OXS_DEMAG_REAL_ANALYTIC z) {
  return Oxs_Newell_g(y,z,x);
}


OXS_DEMAG_REAL_ANALYTIC
Oxs_CalculateNxx(OXS_DEMAG_REAL_ANALYTIC x,
                 OXS_DEMAG_REAL_ANALYTIC y,
                 OXS_DEMAG_REAL_ANALYTIC z,
                 OXS_DEMAG_REAL_ANALYTIC dx,
                 OXS_DEMAG_REAL_ANALYTIC dy,
                 OXS_DEMAG_REAL_ANALYTIC dz);

inline OXS_DEMAG_REAL_ANALYTIC
Oxs_CalculateNyy(OXS_DEMAG_REAL_ANALYTIC x,
                 OXS_DEMAG_REAL_ANALYTIC y,
                 OXS_DEMAG_REAL_ANALYTIC z,
                 OXS_DEMAG_REAL_ANALYTIC dx,
                 OXS_DEMAG_REAL_ANALYTIC dy,
                 OXS_DEMAG_REAL_ANALYTIC dz)
{ return Oxs_CalculateNxx(y,x,z,dy,dx,dz); }

inline OXS_DEMAG_REAL_ANALYTIC
Oxs_CalculateNzz(OXS_DEMAG_REAL_ANALYTIC x,
                 OXS_DEMAG_REAL_ANALYTIC y,
                 OXS_DEMAG_REAL_ANALYTIC z,
                 OXS_DEMAG_REAL_ANALYTIC dx,
                 OXS_DEMAG_REAL_ANALYTIC dy,
                 OXS_DEMAG_REAL_ANALYTIC dz)
{ return Oxs_CalculateNxx(z,y,x,dz,dy,dx); }


OXS_DEMAG_REAL_ANALYTIC
Oxs_CalculateNxy(OXS_DEMAG_REAL_ANALYTIC x,
                 OXS_DEMAG_REAL_ANALYTIC y,
                 OXS_DEMAG_REAL_ANALYTIC z,
                 OXS_DEMAG_REAL_ANALYTIC dx,
                 OXS_DEMAG_REAL_ANALYTIC dy,
                 OXS_DEMAG_REAL_ANALYTIC dz);

inline OXS_DEMAG_REAL_ANALYTIC
Oxs_CalculateNxz(OXS_DEMAG_REAL_ANALYTIC x,
                 OXS_DEMAG_REAL_ANALYTIC y,
                 OXS_DEMAG_REAL_ANALYTIC z,
                 OXS_DEMAG_REAL_ANALYTIC dx,
                 OXS_DEMAG_REAL_ANALYTIC dy,
                 OXS_DEMAG_REAL_ANALYTIC dz)
{ return Oxs_CalculateNxy(x,z,y,dx,dz,dy); }

inline OXS_DEMAG_REAL_ANALYTIC
Oxs_CalculateNyz(OXS_DEMAG_REAL_ANALYTIC x,
                 OXS_DEMAG_REAL_ANALYTIC y,
                 OXS_DEMAG_REAL_ANALYTIC z,
                 OXS_DEMAG_REAL_ANALYTIC dx,
                 OXS_DEMAG_REAL_ANALYTIC dy,
                 OXS_DEMAG_REAL_ANALYTIC dz)
{ return Oxs_CalculateNxy(y,z,x,dy,dz,dx); }


OXS_DEMAG_REAL_ANALYTIC
Oxs_SelfDemagNx(OXS_DEMAG_REAL_ANALYTIC xsize,
                OXS_DEMAG_REAL_ANALYTIC ysize,
                OXS_DEMAG_REAL_ANALYTIC zsize);

OXS_DEMAG_REAL_ANALYTIC
Oxs_SelfDemagNy(OXS_DEMAG_REAL_ANALYTIC xsize,
                OXS_DEMAG_REAL_ANALYTIC ysize,
                OXS_DEMAG_REAL_ANALYTIC zsize);

OXS_DEMAG_REAL_ANALYTIC
Oxs_SelfDemagNz(OXS_DEMAG_REAL_ANALYTIC xsize,
                OXS_DEMAG_REAL_ANALYTIC ysize,
                OXS_DEMAG_REAL_ANALYTIC zsize);


////////////////////////////////////////////////////////////////////////
// Given a function f, define F as
//
//  F(x,y,z) = sum_i sum_j sum_k f(x+i*Wx,y+j*Wy,z+k*Wz)
//
// where i, j, k run across [-Wxcount,Wxcount], [-Wycount,Wycount],
// [-Wzcount,Wzcount], respectively.  (W?count>0 provides partial
// support for periodic boundary conditions.)
//
// Import arr should be pre-sized to dimensions xdim, ydim+2, zdim+2.
//
// This routine then computes D6[F] = D2z[D2y[D2x[F]]] across the range
// [0,xdim-1] x [0,ydim-1] x [0,zdim-1].  The results are stored in arr.
// The outer planes j=ydim,ydim+1 and k=zdim,zdim+1 are used as
// workspace.
void ComputeD6f
(OXS_DEMAG_REAL_ANALYTIC (*f)(OXS_DEMAG_REAL_ANALYTIC,
                              OXS_DEMAG_REAL_ANALYTIC,
                              OXS_DEMAG_REAL_ANALYTIC),
 Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC>& arr,
 OXS_DEMAG_REAL_ANALYTIC dx,
 OXS_DEMAG_REAL_ANALYTIC dy,
 OXS_DEMAG_REAL_ANALYTIC dz,
 OXS_DEMAG_REAL_ANALYTIC Wx,OC_INDEX Wxcount,
 OXS_DEMAG_REAL_ANALYTIC Wy,OC_INDEX Wycount,
 OXS_DEMAG_REAL_ANALYTIC Wz,OC_INDEX Wzcount
 );

class Oxs_DemagAsymptotic; // Forward declaration

void ComputeD6f
(OXS_DEMAG_REAL_ANALYTIC (*f)(OXS_DEMAG_REAL_ANALYTIC,
                              OXS_DEMAG_REAL_ANALYTIC,
                              OXS_DEMAG_REAL_ANALYTIC),
 const Oxs_DemagAsymptotic& fasymp,
 Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC>& arr,
 OXS_DEMAG_REAL_ASYMP hx,
 OXS_DEMAG_REAL_ASYMP hy,
 OXS_DEMAG_REAL_ASYMP hz,
 OXS_DEMAG_REAL_ASYMP Arad,
 OC_INDEX ioff,
 OC_INDEX joff,
 OC_INDEX koff
 );

// Helper function for filling arrays of demag tensor coefficients with
// periodic boundaries
void Oxs_FoldWorkspace
(Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC>& workspace,
 OC_INDEX wdimx,OC_INDEX wdimy,OC_INDEX wdimz,
 OC_INDEX rdimx,OC_INDEX rdimy,OC_INDEX rdimz,
 int xperiodic,int yperiodic,int zperiodic,
 int xodd, int yodd, int zodd,
 OXS_DEMAG_REAL_ANALYTIC& val,
 OC_INDEX i,OC_INDEX j,OC_INDEX k);

////////////////////////////////////////////////////////////////////////
// Classes for coordinate permutations:
struct OxsDemagAdapter {
public:
  OxsDemagAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                  const OXS_DEMAG_REAL_ASYMP& iy,
                  const OXS_DEMAG_REAL_ASYMP& iz) : x(ix), y(iy), z(iz) {}
  const OXS_DEMAG_REAL_ASYMP& x;
  const OXS_DEMAG_REAL_ASYMP& y;
  const OXS_DEMAG_REAL_ASYMP& z;

  // Disable the following three (otherwise implicit) functions by
  // declaring but not defining them.  These need to be public rather
  // than private as otherwise g++ complains about them being hidden
  // before it figures out it can't use them.
  OxsDemagAdapter();
  OxsDemagAdapter(const OxsDemagAdapter&);
  OxsDemagAdapter& operator=(const OxsDemagAdapter&);
};
struct OxsDemagXZYAdapter : public OxsDemagAdapter {
public:
  OxsDemagXZYAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(ix,iz,iy) {}
};
struct OxsDemagYXZAdapter : public OxsDemagAdapter {
public:
  OxsDemagYXZAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(iy,ix,iz) {}
};
struct OxsDemagYZXAdapter : public OxsDemagAdapter {
public:
  OxsDemagYZXAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(iy,iz,ix) {}
};
struct OxsDemagZXYAdapter : public OxsDemagAdapter {
public:
  OxsDemagZXYAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(iz,ix,iy) {}
};
struct OxsDemagZYXAdapter : public OxsDemagAdapter {
public:
  OxsDemagZYXAdapter(const OXS_DEMAG_REAL_ASYMP& ix,
                     const OXS_DEMAG_REAL_ASYMP& iy,
                     const OXS_DEMAG_REAL_ASYMP& iz)
    : OxsDemagAdapter(iz,iy,ix) {}
};


////////////////////////////////////////////////////////////////////////
// A couple of data structures for parameter passing.  Main intent
// is to allow caching of reused quantities such as sqrt(x*x+y*y+z*z).
// Intended for internal use only
struct OxsDemagNabData {
public:
  OXS_DEMAG_REAL_ASYMP x,y,z;
  OXS_DEMAG_REAL_ASYMP tx2,ty2,tz2;
  OXS_DEMAG_REAL_ASYMP R,iR;
  OXS_DEMAG_REAL_ASYMP R2,iR2;

  inline void Set(OXS_DEMAG_REAL_ASYMP import_x,
                  OXS_DEMAG_REAL_ASYMP import_y,
                  OXS_DEMAG_REAL_ASYMP import_z);

  static void SetPair(OXS_DEMAG_REAL_ASYMP ixa,
                      OXS_DEMAG_REAL_ASYMP iya,
                      OXS_DEMAG_REAL_ASYMP iza,
                      OXS_DEMAG_REAL_ASYMP ixb,
                      OXS_DEMAG_REAL_ASYMP iyb,
                      OXS_DEMAG_REAL_ASYMP izb,
                      OxsDemagNabData& pta,
                      OxsDemagNabData& ptb);
  // Fills two OxsDemagNabData at one time.  The purpose of this routine
  // is to allow efficient SSE code.

  OxsDemagNabData() : x(0), y(0), z(0),
                      tx2(0), ty2(0), tz2(0),
                      R(0), iR(0), R2(0), iR2(0) {}

  OxsDemagNabData(OXS_DEMAG_REAL_ASYMP import_x,
                  OXS_DEMAG_REAL_ASYMP import_y,
                  OXS_DEMAG_REAL_ASYMP import_z) {
    Set(import_x,import_y,import_z);
  }

  OxsDemagNabData(OXS_DEMAG_REAL_ASYMP import_x,
                  OXS_DEMAG_REAL_ASYMP import_y,
                  OXS_DEMAG_REAL_ASYMP import_z,
                  OXS_DEMAG_REAL_ASYMP import_tx2,
                  OXS_DEMAG_REAL_ASYMP import_ty2,
                  OXS_DEMAG_REAL_ASYMP import_tz2,
                  OXS_DEMAG_REAL_ASYMP import_R,
                  OXS_DEMAG_REAL_ASYMP import_iR,
                  OXS_DEMAG_REAL_ASYMP import_R2,
                  OXS_DEMAG_REAL_ASYMP import_iR2)
    : x(import_x), y(import_y), z(import_z),
      tx2(import_tx2), ty2(import_ty2), tz2(import_tz2),
      R(import_R), iR(import_iR), R2(import_R2), iR2(import_iR2) {}
};

template<class Adapter>
class OxsTDemagNabData : public OxsDemagNabData
{
public:
  OxsTDemagNabData(const OxsDemagNabData& data) : OxsDemagNabData(data)
  {
    // NOTE: "Adapter" works with references, so it is important to
    // initialize a and b off of data rather than (x,y,z) and
    // (tx2,ty2,tz2), because if one did that then the subsequent copies
    // would fail by overwriting some data before it got copied, e.g.,
    // "a.z" could actually be a reference to one of x or y.
    Adapter a(data.x,data.y,data.z);          x = a.x;   y = a.y;   z = a.z;
    Adapter b(data.tx2,data.ty2,data.tz2);  tx2 = b.x; ty2 = b.y; tz2 = b.z;
  }

  // Disable the following functions by declaring but not defining them.
  OxsTDemagNabData();
  OxsTDemagNabData(const OxsTDemagNabData<Adapter>&);
  OxsTDemagNabData<Adapter>& operator=(const OxsTDemagNabData<Adapter>&);
};

struct OxsDemagNabPairData {
public:
  OXS_DEMAG_REAL_ASYMP ubase, uoff;
  OxsDemagNabData ptp;  // ubase + uoff
  OxsDemagNabData ptm;  // ubase - uoff

  OxsDemagNabPairData() : ubase(0), uoff(0) {}

  OxsDemagNabPairData(OXS_DEMAG_REAL_ASYMP import_ubase,
                      OXS_DEMAG_REAL_ASYMP import_uoff,
                      const OxsDemagNabData& import_ptp,
                      const OxsDemagNabData& import_ptm)
    : ubase(import_ubase), uoff(import_uoff),
      ptp(import_ptp), ptm(import_ptm) {}
};

template<class Adapter>
class OxsTDemagNabPairData : public OxsDemagNabPairData
{
public:
  OxsTDemagNabPairData(const OxsDemagNabPairData& data)
    : OxsDemagNabPairData()
  {
    ubase = data.ubase;
    uoff  = data.uoff;
    OxsTDemagNabData<Adapter> tptp(data.ptp);  ptp = tptp;
    OxsTDemagNabData<Adapter> tptm(data.ptm);  ptm = tptm;
  }

  // Disable the following functions by declaring but not defining them.
  OxsTDemagNabPairData();
  OxsTDemagNabPairData(const OxsTDemagNabPairData<Adapter>&);
  OxsTDemagNabPairData<Adapter>&
   operator=(const OxsTDemagNabPairData<Adapter>&);
};

inline void OxsDemagNabData::Set
(OXS_DEMAG_REAL_ASYMP import_x,
 OXS_DEMAG_REAL_ASYMP import_y,
 OXS_DEMAG_REAL_ASYMP import_z)
{
  x = import_x;  y = import_y;  z = import_z;
  const OXS_DEMAG_REAL_ASYMP x2 = x*x;
  const OXS_DEMAG_REAL_ASYMP y2 = y*y;
  R2 = x2 + y2;
  const OXS_DEMAG_REAL_ASYMP z2 = z*z;
  R2 += z2;
  const OXS_DEMAG_REAL_ASYMP R4 = R2*R2;
  R = sqrt(R2);
  if(R2 != 0.0) {
    tx2 = x2/R4;  ty2 = y2/R4;  tz2 = z2/R4;
    iR2 = 1.0/R2;
    iR  = 1.0/R;
  } else {
    // I don't think this branch ever happens
    tx2 = ty2 = tz2 = 0.0;
    iR2 = R = iR = 0.0;
  }
}

#if !OXS_DEMAG_ASYMP_USE_SSE
inline void OxsDemagNabData::SetPair
(OXS_DEMAG_REAL_ASYMP ixa,
 OXS_DEMAG_REAL_ASYMP iya,
 OXS_DEMAG_REAL_ASYMP iza,
 OXS_DEMAG_REAL_ASYMP ixb,
 OXS_DEMAG_REAL_ASYMP iyb,
 OXS_DEMAG_REAL_ASYMP izb,
 OxsDemagNabData& pta,
 OxsDemagNabData& ptb)
{ // Fills two OxsDemagNabData at one time.
  // Non-SSE version:
  pta.Set(ixa,iya,iza);
  ptb.Set(ixb,iyb,izb);
}
#else // !OXS_DEMAG_ASYMP_USE_SSE
inline void OxsDemagNabData::SetPair
(OXS_DEMAG_REAL_ASYMP ixa,
 OXS_DEMAG_REAL_ASYMP iya,
 OXS_DEMAG_REAL_ASYMP iza,
 OXS_DEMAG_REAL_ASYMP ixb,
 OXS_DEMAG_REAL_ASYMP iyb,
 OXS_DEMAG_REAL_ASYMP izb,
 OxsDemagNabData& pta,
 OxsDemagNabData& ptb)
{ // Fills two OxsDemagNabData at one time.  The purpose of this routine
  // is to allow efficient SSE code.

  if((ixa==0.0 && iya==0.0 && iza==0.0) ||
     (ixb==0.0 && iyb==0.0 && izb==0.0)   ) {
    // This code insures against the case where pta.R or ptb.R
    // are zero.  As far as I can tell, this never happens, so
    // this check doesn't do anything other than slow things
    // a bit.
    pta.Set(ixa,iya,iza); ptb.Set(ixb,iyb,izb);
    return;
  }
  // At point we are assured that pta.R and ptb.R are both >0.0

  pta.x = ixa;  pta.y = iya;  pta.z = iza;
  ptb.x = ixb;  ptb.y = iyb;  ptb.z = izb;

  // "a" goes into lower half, "b" into upper half
  Oc_Duet wx2(ixa,ixb); wx2 *= wx2;
  Oc_Duet wy2(iya,iyb); wy2 *= wy2;
  Oc_Duet wR2 = wx2 + wy2;
  Oc_Duet wz2(iza,izb); wz2 *= wz2;
  wR2 += wz2;

  wR2.StoreA(pta.R2); wR2.StoreB(ptb.R2);

  Oc_Duet wR4 = wR2*wR2;

  wx2 /= wR4;  wx2.StoreA(pta.tx2);  wx2.StoreB(ptb.tx2);
  wy2 /= wR4;  wy2.StoreA(pta.ty2);  wy2.StoreB(ptb.ty2);
  wz2 /= wR4;  wz2.StoreA(pta.tz2);  wz2.StoreB(ptb.tz2);

  Oc_Duet wone(1.0);
  Oc_Duet wiR2 = wone/wR2;   wiR2.StoreA(pta.iR2); wiR2.StoreB(ptb.iR2);
  Oc_Duet wR = sqrt(wR2); wR.StoreA(pta.R);     wR.StoreB(ptb.R);
  Oc_Duet wiR  = wone/wR;    wiR.StoreA(pta.iR);   wiR.StoreB(ptb.iR);
}
#endif // !OXS_DEMAG_ASYMP_USE_SSE


////////////////////////////////////////////////////////////////////////
// High-order asymptotic approximations
class Oxs_DemagNxxAsymptoticBase {
public:
  Oxs_DemagNxxAsymptoticBase(OXS_DEMAG_REAL_ASYMP dx,
                             OXS_DEMAG_REAL_ASYMP dy,
                             OXS_DEMAG_REAL_ASYMP dz,
                             int in_max_order);
  ~Oxs_DemagNxxAsymptoticBase() {}

  OXS_DEMAG_REAL_ASYMP Asymptotic(const OxsDemagNabData& ptdata) const;

#if OXS_DEMAG_ASYMP_USE_SSE
  OXS_DEMAG_REAL_ASYMP AsymptoticPair(const OxsDemagNabData& ptA,
                                         const OxsDemagNabData& ptB) const;
#else
  OXS_DEMAG_REAL_ASYMP AsymptoticPair(const OxsDemagNabData& ptA,
                                         const OxsDemagNabData& ptB) const
  { return Asymptotic(ptA) + Asymptotic(ptB); }
#endif

  OXS_DEMAG_REAL_ASYMP
  AsymptoticPair(const OxsDemagNabPairData& ptdatapair) const {
    return AsymptoticPair(ptdatapair.ptp,ptdatapair.ptm);
  }

  OXS_DEMAG_REAL_ASYMP Asymptotic(OXS_DEMAG_REAL_ASYMP x,
                                  OXS_DEMAG_REAL_ASYMP y,
                                  OXS_DEMAG_REAL_ASYMP z) const {
    OxsDemagNabData ptdata(x,y,z);
    return Asymptotic(ptdata);
  }

private:
  int cubic_cell;
  int max_order;
  OXS_DEMAG_REAL_ASYMP self_demag;
  OXS_DEMAG_REAL_ASYMP lead_weight;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3,a4,a5,a6;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6,b7,b8,b9,b10;
  OXS_DEMAG_REAL_ASYMP c1,c2,c3,c4,c5,c6,c7,c8,c9,c10,c11,c12,c13,c14,c15;
};

class Oxs_DemagAsymptotic {
  // Interface class.
  //
  // Note: In this class the Asymptotic(x,y,z) interface is primary,
  //   with Asymptotic(const OxsDemagNabData& ptdata) usually defined in
  //   terms of the former.  But if the extra data in ptdata can be used
  //   to advantage, then the child class may well prefer to reverse
  //   that relationship.
  //
  // Design creep: Each asymptotic demag object has a
  //   std::vector<SubregionApproximate> list that controls crossover
  //   between different asymptotic methods.  Computing a full demag
  //   tensor requires six of the demag objects, one each for Nxx, Nxy,
  //   Nxz, Nyy, Nyz, and Nzz.  However, for a given demag tensor all
  //   six should presumably be using the same crossover schedule.  A
  //   proper design would probably compute and keep a single schedule,
  //   and maybe wrap the analytic demag computation into the mix so all
  //   could share pertinent implementation details.
  
public:

  virtual ~Oxs_DemagAsymptotic() {}

  virtual OXS_DEMAG_REAL_ASYMP
  Asymptotic(OXS_DEMAG_REAL_ASYMP x,
             OXS_DEMAG_REAL_ASYMP y,
             OXS_DEMAG_REAL_ASYMP z) const = 0;

  virtual OXS_DEMAG_REAL_ASYMP
  Asymptotic(const OxsDemagNabData& ptdata) const =0;

  virtual OXS_DEMAG_REAL_ASYMP
  AsymptoticPair(const OxsDemagNabPairData& ptdata) const {
    return Asymptotic(ptdata.ptp) +  Asymptotic(ptdata.ptm);
  }

  virtual OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const =0;
  // Returns the offset R between the outer edge of the analytic
  // and inner edge of asymptotic regimes.
  
  // Given cell dimensions dx x dy x dz, FindRefinementCount returns the
  // number of subcells in the coarsest refinement having all subcell
  // edge dimensions no bigger than import h.
  //  Conversely, FindBestRefinement take desired cell count nrequest
  // as input, and returns edge refinement counts nx, ny, nz, for
  // the refinement with the smallest max edge length not using more
  // than nrequest cells.
  //  Routine FindNeededRefinement computes the refinement needed
  // to meet the requested error for a given offset and asymptotic
  // method order.
  static OC_INDEX FindRefinementCount(OXS_DEMAG_REAL_ASYMP dx,
                                      OXS_DEMAG_REAL_ASYMP dy,
                                      OXS_DEMAG_REAL_ASYMP dz,
                                      OXS_DEMAG_REAL_ASYMP h,
                                      OC_INDEX& nx,OC_INDEX& ny,OC_INDEX& nz);
  static void FindBestRefinement(OXS_DEMAG_REAL_ASYMP dx,
                                 OXS_DEMAG_REAL_ASYMP dy,
                                 OXS_DEMAG_REAL_ASYMP dz,
                                 OC_INDEX nrequest,
                                 OC_INDEX& nx,OC_INDEX& ny,OC_INDEX& nz);

  static OC_INDEX FindNeededRefinement(OXS_DEMAG_REAL_ASYMP dx,
                                       OXS_DEMAG_REAL_ASYMP dy,
                                       OXS_DEMAG_REAL_ASYMP dz,
                                       OXS_DEMAG_REAL_ASYMP R,
                                       OXS_DEMAG_REAL_ASYMP allowed_error,
                                       int order,  // method order
                                       OC_INDEX& nx,OC_INDEX& ny,OC_INDEX& nz);

  // Structure used to describe asymptotic subregions.  For point
  // (x,y,z) with sqrt(x^2+y^2+z^2)>=r, specifies using an asymptotic
  // approximation with nx x ny x nz subdivisions of degree such that
  // the error is O(1/r^order).
  struct DemagKnot {
  public:
    OXS_DEMAG_REAL_ASYMP r;
    OC_INDEX nx;
    OC_INDEX ny;
    OC_INDEX nz;
    int order;
    DemagKnot(OXS_DEMAG_REAL_ASYMP ir,
              OC_INDEX inx,OC_INDEX iny,OC_INDEX inz,
              int iorder)
      : r(ir), nx(inx), ny(iny), nz(inz), order(iorder) {}
  };

  // Using built-in assumptions about analytic, asymptotic, and refined
  // asymptotic method errors and speed, builds an ordered list of knots
  // yielding an optimal demag schedule.  The return value is the
  // smallest (i.e., last) schedule radius --- smaller than that radius
  // the analytic method should be used.
  static OXS_DEMAG_REAL_ASYMP
  FindKnotSchedule(OXS_DEMAG_REAL_ASYMP dx, // Import
                   OXS_DEMAG_REAL_ASYMP dy, // Import
                   OXS_DEMAG_REAL_ASYMP dz, // Import
                   OXS_DEMAG_REAL_ASYMP maxerror,  // Import
                   int asymptotic_order,    // Import
                   std::vector<DemagKnot>& knots); // Export
};


struct OxsDemagAsymptoticRefineInfo {
public:
  void Init(OXS_DEMAG_REAL_ASYMP dx,
            OXS_DEMAG_REAL_ASYMP dy,
            OXS_DEMAG_REAL_ASYMP dz,
            OC_INDEX in_xcount,
            OC_INDEX in_ycount,
            OC_INDEX in_zcount)
  {
    xcount = in_xcount;  ycount = in_ycount;  zcount = in_zcount;
    rdx = dx/in_xcount;  rdy = dy/in_ycount;
    rdz = dz/in_zcount;
    result_scale = 1.0/(OXS_DEMAG_REAL_ASYMP(in_xcount)
                       *OXS_DEMAG_REAL_ASYMP(in_ycount)
                       *OXS_DEMAG_REAL_ASYMP(in_zcount));
  }

  OxsDemagAsymptoticRefineInfo(OXS_DEMAG_REAL_ASYMP dx,
                               OXS_DEMAG_REAL_ASYMP dy,
                               OXS_DEMAG_REAL_ASYMP dz,
                               OC_INDEX in_xcount,
                               OC_INDEX in_ycount,
                               OC_INDEX in_zcount)
  { Init(dx,dy,dz,in_xcount,in_ycount,in_zcount); }

  OxsDemagAsymptoticRefineInfo(OXS_DEMAG_REAL_ASYMP dx,
                               OXS_DEMAG_REAL_ASYMP dy,
                               OXS_DEMAG_REAL_ASYMP dz,
                               OXS_DEMAG_REAL_ASYMP R,
                               OXS_DEMAG_REAL_ASYMP allowed_error,
                               int method_order)
  {
    Oxs_DemagAsymptotic::FindNeededRefinement(dx,dy,dz,R,
                                              allowed_error,method_order,
                                              xcount,ycount,zcount);
    rdx = dx/xcount;  rdy = dy/ycount;  rdz = dz/zcount;
    result_scale = 1.0/(OXS_DEMAG_REAL_ASYMP(xcount)
                       *OXS_DEMAG_REAL_ASYMP(ycount)
                       *OXS_DEMAG_REAL_ASYMP(zcount));
  }

  OC_INDEX xcount,ycount,zcount;      // Refinement counts
  OXS_DEMAG_REAL_ASYMP rdx, rdy, rdz; // Refined cell dimensions
  OXS_DEMAG_REAL_ASYMP result_scale;  // Scaling relative to full cell
};


class Oxs_DemagNxxAsymptotic : public Oxs_DemagAsymptotic {
public:
  Oxs_DemagNxxAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         const std::vector<DemagKnot>& spec);
  // NOTE: Import spec should be ordered with decreasing r

  Oxs_DemagNxxAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxerror=0,
                         int asymptotic_order=11);
  // maxerror=0 selects default value of 8*machine epsilon
  // for OXS_DEMAG_REAL_ASYMP type.

  ~Oxs_DemagNxxAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP Asymptotic(const OxsDemagNabData& ptdata) const;

  OXS_DEMAG_REAL_ASYMP Asymptotic(OXS_DEMAG_REAL_ASYMP x,
                                  OXS_DEMAG_REAL_ASYMP y,
                                  OXS_DEMAG_REAL_ASYMP z) const {
    OxsDemagNabData ptdata(x,y,z);
    return Asymptotic(ptdata);
  }

  OXS_DEMAG_REAL_ASYMP
  AsymptoticPair(const OxsDemagNabPairData& ptdata) const
  { return Asymptotic(ptdata.ptp) +  Asymptotic(ptdata.ptm); }

  OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { // Returns the offset R between the outer edge of the analytic
    // and inner edge of asymptotic regimes.
    return sqrt(Nxxvec.back().rsq);
  }
  
private:
  struct SubregionApproximate {
  public:
    OXS_DEMAG_REAL_ASYMP rsq;
    OxsDemagAsymptoticRefineInfo refine_info;
    Oxs_DemagNxxAsymptoticBase Nxx;
    SubregionApproximate(OXS_DEMAG_REAL_ASYMP ir,
                         const OxsDemagAsymptoticRefineInfo& iinfo,
                         int iorder)
      : rsq(ir*ir), refine_info(iinfo),
        Nxx(iinfo.rdx,iinfo.rdy,iinfo.rdz,iorder) {}
  };
  std::vector<SubregionApproximate> Nxxvec;

  // Support code for constructors:
  void FillNxxvec(OXS_DEMAG_REAL_ASYMP dx,
                  OXS_DEMAG_REAL_ASYMP dy,
                  OXS_DEMAG_REAL_ASYMP dz,
                  const std::vector<DemagKnot>& knots);
};

class Oxs_DemagNxyAsymptoticBase {
public:
  Oxs_DemagNxyAsymptoticBase(OXS_DEMAG_REAL_ASYMP dx,
                             OXS_DEMAG_REAL_ASYMP dy,
                             OXS_DEMAG_REAL_ASYMP dz,
                             int in_max_order);
  ~Oxs_DemagNxyAsymptoticBase() {}

  OXS_DEMAG_REAL_ASYMP Asymptotic(const OxsDemagNabData& ptdata) const;

  OXS_DEMAG_REAL_ASYMP Asymptotic(OXS_DEMAG_REAL_ASYMP x,
                                  OXS_DEMAG_REAL_ASYMP y,
                                  OXS_DEMAG_REAL_ASYMP z) const {
    OxsDemagNabData ptdata(x,y,z);
    return Asymptotic(ptdata);
  }

  // The AsymptoticPairX routines evaluate asymptotic approximation to
  //    Nxy(x+xoff,y,z) + Nxy(x-xoff,y,z)
  // If |xoff| >> |x|, then this is better than making two calls to
  // Asymptotic because the PairX variant handles cancellations in
  // the lead O(1/R^3) term algebraically.  This cancellation does not
  // occur across the other (y- or z-) axes, so there is no corresponding
  // PairY or PairZ routines --- in those circumstances just make two
  // calls to Asymptotic.
  OXS_DEMAG_REAL_ASYMP
  AsymptoticPairX(const OxsDemagNabPairData& ptdata) const;

#if OXS_DEMAG_ASYMP_USE_SSE
  OXS_DEMAG_REAL_ASYMP AsymptoticPair(const OxsDemagNabData& ptA,
                                      const OxsDemagNabData& ptB) const;
#else
  OXS_DEMAG_REAL_ASYMP AsymptoticPair(const OxsDemagNabData& ptA,
                                      const OxsDemagNabData& ptB) const
  { return Asymptotic(ptA) + Asymptotic(ptB); }
#endif

private:
  int cubic_cell;
  int max_order;
  OXS_DEMAG_REAL_ASYMP lead_weight;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6;
  OXS_DEMAG_REAL_ASYMP c1,c2,c3,c4,c5,c6,c7,c8,c9,c10;
};

class Oxs_DemagNxyAsymptotic : public Oxs_DemagAsymptotic {
public:
  Oxs_DemagNxyAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         const std::vector<DemagKnot>& spec);
  // NOTE: Import spec should be ordered with decreasing r

  Oxs_DemagNxyAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxerror=0,
                         int asymptotic_order=11);
  // maxerror=0 selects default value of 8*machine epsilon
  // for OXS_DEMAG_REAL_ASYMP type.

  ~Oxs_DemagNxyAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP Asymptotic(const OxsDemagNabData& ptdata) const;

  OXS_DEMAG_REAL_ASYMP Asymptotic(OXS_DEMAG_REAL_ASYMP x,
                                  OXS_DEMAG_REAL_ASYMP y,
                                  OXS_DEMAG_REAL_ASYMP z) const {
    OxsDemagNabData ptdata(x,y,z);
    return Asymptotic(ptdata);
  }

  OXS_DEMAG_REAL_ASYMP
  AsymptoticPairX(const OxsDemagNabPairData& ptdata) const;
  OXS_DEMAG_REAL_ASYMP
  AsymptoticPairZ(const OxsDemagNabPairData& ptdata) const {
    return Asymptotic(ptdata.ptp) + Asymptotic(ptdata.ptm);
  }

  OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { // Returns the offset R between the outer edge of the analytic
    // and inner edge of asymptotic regimes.
    return sqrt(Nxyvec.back().rsq);
  }
  
private:
  struct SubregionApproximate {
  public:
    OXS_DEMAG_REAL_ASYMP rsq;
    OxsDemagAsymptoticRefineInfo refine_info;
    Oxs_DemagNxyAsymptoticBase Nxy;
    SubregionApproximate(OXS_DEMAG_REAL_ASYMP ir,
                         const OxsDemagAsymptoticRefineInfo& iinfo,
                         int iorder)
      : rsq(ir*ir), refine_info(iinfo),
        Nxy(iinfo.rdx,iinfo.rdy,iinfo.rdz,iorder) {}
  };
  std::vector<SubregionApproximate> Nxyvec;

  // Support code for constructors:
  void FillNxyvec(OXS_DEMAG_REAL_ASYMP dx,
                  OXS_DEMAG_REAL_ASYMP dy,
                  OXS_DEMAG_REAL_ASYMP dz,
                  const std::vector<DemagKnot>& knots);
};

class Oxs_DemagNyyAsymptotic : public Oxs_DemagAsymptotic {
public:
  Oxs_DemagNyyAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         const std::vector<DemagKnot>& spec)
    : Nxx(dy,dx,dz,spec) {}
  // NOTE: Import spec should be ordered with decreasing r

  Oxs_DemagNyyAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxerror=0,
                         int asymptotic_order=11)
    : Nxx(dy,dx,dz,maxerror,asymptotic_order) {}
  // maxerror=0 selects default value of 8*machine epsilon
  // for OXS_DEMAG_REAL_ASYMP type.

  ~Oxs_DemagNyyAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP Asymptotic(OXS_DEMAG_REAL_ASYMP x,
                                  OXS_DEMAG_REAL_ASYMP y,
                                  OXS_DEMAG_REAL_ASYMP z) const
  { return Nxx.Asymptotic(y,x,z); }

  OXS_DEMAG_REAL_ASYMP
  Asymptotic(const OxsDemagNabData& ptdata) const
  { return Nxx.Asymptotic(ptdata.y,ptdata.x,ptdata.z); }

  OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { // Returns the offset R between the outer edge of the analytic
    // and inner edge of asymptotic regimes.
    return Nxx.GetAsymptoticStart();
  }

private:
  const Oxs_DemagNxxAsymptotic Nxx;
};

class Oxs_DemagNzzAsymptotic : public Oxs_DemagAsymptotic {
public:
  Oxs_DemagNzzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         const std::vector<DemagKnot>& spec)
    : Nxx(dz,dy,dx,spec) {}
  // NOTE: Import spec should be ordered with decreasing r

  Oxs_DemagNzzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxerror=0,
                         int asymptotic_order=11)
    : Nxx(dz,dy,dx,maxerror,asymptotic_order) {}
  // maxerror=0 selects default value of 8*machine epsilon
  // for OXS_DEMAG_REAL_ASYMP type.
  
  ~Oxs_DemagNzzAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP Asymptotic(OXS_DEMAG_REAL_ASYMP x,
                                  OXS_DEMAG_REAL_ASYMP y,
                                  OXS_DEMAG_REAL_ASYMP z) const
  { return Nxx.Asymptotic(z,y,x); }

  OXS_DEMAG_REAL_ASYMP
  Asymptotic(const OxsDemagNabData& ptdata) const
  { return Nxx.Asymptotic(ptdata.z,ptdata.y,ptdata.x); }

  OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { // Returns the offset R between the outer edge of the analytic
    // and inner edge of asymptotic regimes.
    return Nxx.GetAsymptoticStart();
  }

private:
  const Oxs_DemagNxxAsymptotic Nxx;
};

class Oxs_DemagNxzAsymptotic : public Oxs_DemagAsymptotic {
public:
  Oxs_DemagNxzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         const std::vector<DemagKnot>& spec)
    : Nxy(dx,dz,dy,spec) {}
  // NOTE: Import spec should be ordered with decreasing r

  Oxs_DemagNxzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxerror=0,
                         int asymptotic_order=11)
    : Nxy(dx,dz,dy,maxerror,asymptotic_order) {}
  // maxerror=0 selects default value of 8*machine epsilon
  // for OXS_DEMAG_REAL_ASYMP type.

  ~Oxs_DemagNxzAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP Asymptotic(OXS_DEMAG_REAL_ASYMP x,
                                  OXS_DEMAG_REAL_ASYMP y,
                                  OXS_DEMAG_REAL_ASYMP z) const
  { return Nxy.Asymptotic(x,z,y); }

  OXS_DEMAG_REAL_ASYMP
  Asymptotic(const OxsDemagNabData& ptdata) const
  { return Nxy.Asymptotic(ptdata.x,ptdata.z,ptdata.y); }

  OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { // Returns the offset R between the outer edge of the analytic
    // and inner edge of asymptotic regimes.
    return Nxy.GetAsymptoticStart();
  }

private:
  const Oxs_DemagNxyAsymptotic Nxy;
};

class Oxs_DemagNyzAsymptotic : public Oxs_DemagAsymptotic {
public:
  Oxs_DemagNyzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         const std::vector<DemagKnot>& spec)
    : Nxy(dy,dz,dx,spec) {}
  // NOTE: Import spec should be ordered with decreasing r

  Oxs_DemagNyzAsymptotic(OXS_DEMAG_REAL_ASYMP dx,
                         OXS_DEMAG_REAL_ASYMP dy,
                         OXS_DEMAG_REAL_ASYMP dz,
                         OXS_DEMAG_REAL_ASYMP maxerror=0,
                         int asymptotic_order=11)
    : Nxy(dy,dz,dx,maxerror,asymptotic_order) {}
  // maxerror=0 selects default value of 8*machine epsilon
  // for OXS_DEMAG_REAL_ASYMP type.

  ~Oxs_DemagNyzAsymptotic() {}

  OXS_DEMAG_REAL_ASYMP Asymptotic(OXS_DEMAG_REAL_ASYMP x,
                                  OXS_DEMAG_REAL_ASYMP y,
                                  OXS_DEMAG_REAL_ASYMP z) const
  { return Nxy.Asymptotic(y,z,x); }

  OXS_DEMAG_REAL_ASYMP
  Asymptotic(const OxsDemagNabData& ptdata) const
  { return Nxy.Asymptotic(ptdata.y,ptdata.z,ptdata.x); }

  OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { // Returns the offset R between the outer edge of the analytic
    // and inner edge of asymptotic regimes.
    return Nxy.GetAsymptoticStart();
  }

private:
  const Oxs_DemagNxyAsymptotic Nxy;
};

////////////////////////////////////////////////////////////////////////
// Support classes for periodic boundaries

// Code to compute tails for periodic boundaries:
//
//    class OxsDemagNxxIntegralX
//    class OxsDemagNxyIntegralX
//    class OxsDemagNxxIntegralZ
//    class OxsDemagNxyIntegralZ
//
// These four classes suffice to to compute any NabIntegralC for
// a,b,c \in {x,y,z}.
//
// These classes are intended for internal use only.
//
class OxsDemagNxxIntegralXBase {
public:
  OxsDemagNxxIntegralXBase(OXS_DEMAG_REAL_ASYMP rdx,
                           OXS_DEMAG_REAL_ASYMP rdy,
                           OXS_DEMAG_REAL_ASYMP rdz,
                           OXS_DEMAG_REAL_ASYMP Wx);
  ~OxsDemagNxxIntegralXBase() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt x
private:
  const OXS_DEMAG_REAL_ASYMP scale;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6;
  int cubic_cell;
  // The following members are declared but not defined:
  OxsDemagNxxIntegralXBase(const OxsDemagNxxIntegralXBase&);
  OxsDemagNxxIntegralXBase& operator=(const OxsDemagNxxIntegralXBase&);
};

class OxsDemagNxyIntegralXBase {
public:
  OxsDemagNxyIntegralXBase(OXS_DEMAG_REAL_ASYMP rdx,
                           OXS_DEMAG_REAL_ASYMP rdy,
                           OXS_DEMAG_REAL_ASYMP rdz,
                           OXS_DEMAG_REAL_ASYMP Wx);
  
  ~OxsDemagNxyIntegralXBase() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt x
private:
  const OXS_DEMAG_REAL_ASYMP scale;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6;
  int cubic_cell;
  // The following members are declared but not defined:
  OxsDemagNxyIntegralXBase(const OxsDemagNxyIntegralXBase&);
  OxsDemagNxyIntegralXBase& operator=(const OxsDemagNxyIntegralXBase&);
};

// In the following template, class T should be one of
// OxsDemagNxxIntegralXBase or OxsDemagNxyIntegralXBase.
// See typedef's following template definition.
template<typename T>
class OxsDemagNttIntegralX {
public:
  OxsDemagNttIntegralX(const OxsDemagAsymptoticRefineInfo& ari,
                       OXS_DEMAG_REAL_ASYMP Wx)
    : refine_info(ari), Ntt(ari.rdx,ari.rdy,ari.rdz,Wx) {} // asdfasdf
  ~OxsDemagNttIntegralX() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt x
private:
  OxsDemagAsymptoticRefineInfo refine_info;
  const T Ntt;
};

template<typename T>
OXS_DEMAG_REAL_ASYMP
OxsDemagNttIntegralX<T>::Compute(const OxsDemagNabPairData& ptdata) const
{ // Integral of asymptotic approximation to Ntt term, from xp to
  // infinity and -infinity to xm (xp must be > 0, and xm must be < 0),
  // with window x dimension Wx.  This routine is used internally for
  // periodic demag computations.

  assert(ptdata.ptp.y == ptdata.ptm.y &&
         ptdata.ptp.z == ptdata.ptm.z);

  // Alias data from refine_info structure.
  const OC_INDEX& xcount = refine_info.xcount;
  const OC_INDEX& ycount = refine_info.ycount;
  const OC_INDEX& zcount = refine_info.zcount;
  const OXS_DEMAG_REAL_ASYMP& rdx = refine_info.rdx;
  const OXS_DEMAG_REAL_ASYMP& rdy = refine_info.rdy;
  const OXS_DEMAG_REAL_ASYMP& rdz = refine_info.rdz;
  const OXS_DEMAG_REAL_ASYMP& result_scale = refine_info.result_scale;

  OxsDemagNabPairData work;
  work.ubase = ptdata.ubase;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INDEX k=1-zcount;k<zcount;++k) {
    OXS_DEMAG_REAL_ASYMP zoff = ptdata.ptp.z+k*rdz; // .ptm.z == .ptp.z
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INDEX j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.ptp.y+j*rdy; // .ptm.y == .ptp.y
      work.uoff = ptdata.uoff;
      OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                               work.ubase - work.uoff,yoff,zoff,
                               work.ptp, work.ptm);
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Ntt.Compute(work);
      for(OC_INDEX i=1;i<xcount;++i) {
        work.uoff = ptdata.uoff + i*rdx;
        OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                                 work.ubase - work.uoff,yoff,zoff,
                                 work.ptp, work.ptm);
        OXS_DEMAG_REAL_ASYMP tmpsum = Ntt.Compute(work);
        work.uoff = ptdata.uoff - i*rdx;
        OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                                 work.ubase - work.uoff,yoff,zoff,
                                 work.ptp, work.ptm);
        tmpsum += Ntt.Compute(work);
        xsum += (xcount-i) * tmpsum;
      }
      // Weight x-strip interactions into xy-plate
      ysum += (ycount - abs(j))*xsum;
    }
    // Weight xy-plate interactions into total sum
    zsum += (zcount - abs(k))*ysum;
  }
  return zsum*result_scale;
}

typedef OxsDemagNttIntegralX<OxsDemagNxxIntegralXBase> OxsDemagNxxIntegralX;
typedef OxsDemagNttIntegralX<OxsDemagNxyIntegralXBase> OxsDemagNxyIntegralX;

class OxsDemagNxxIntegralZBase {
public:
  OxsDemagNxxIntegralZBase(OXS_DEMAG_REAL_ASYMP rdx,
                           OXS_DEMAG_REAL_ASYMP rdy,
                           OXS_DEMAG_REAL_ASYMP rdz,
                           OXS_DEMAG_REAL_ASYMP Wz);
  ~OxsDemagNxxIntegralZBase() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt z
private:
  const OXS_DEMAG_REAL_ASYMP scale;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3,a4,a5,a6;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6,b7,b8,b9,b10;
  int cubic_cell;
  // The following members are declared but not defined:
  OxsDemagNxxIntegralZBase(const OxsDemagNxxIntegralZBase&);
  OxsDemagNxxIntegralZBase& operator=(const OxsDemagNxxIntegralZBase&);
};

class OxsDemagNxyIntegralZBase {
public:
  OxsDemagNxyIntegralZBase(OXS_DEMAG_REAL_ASYMP rdx,
                           OXS_DEMAG_REAL_ASYMP rdy,
                           OXS_DEMAG_REAL_ASYMP rdz,
                           OXS_DEMAG_REAL_ASYMP Wz);
  ~OxsDemagNxyIntegralZBase() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt z
private:
  const OXS_DEMAG_REAL_ASYMP scale;
  OXS_DEMAG_REAL_ASYMP a1,a2,a3;
  OXS_DEMAG_REAL_ASYMP b1,b2,b3,b4,b5,b6;
  int cubic_cell;
  // The following members are declared but not defined:
  OxsDemagNxyIntegralZBase(const OxsDemagNxyIntegralZBase&);
  OxsDemagNxyIntegralZBase& operator=(const OxsDemagNxyIntegralZBase&);
};

// In the following template, class T should be one of
// OxsDemagNxxIntegralZBase or OxsDemagNxyIntegralZBase.
// See typedef's following template definition.
template<typename T>
class OxsDemagNttIntegralZ {
public:
  OxsDemagNttIntegralZ(const OxsDemagAsymptoticRefineInfo& ari,
                       OXS_DEMAG_REAL_ASYMP Wz)
    : refine_info(ari), Ntt(ari.rdx,ari.rdy,ari.rdz,Wz) {}
  ~OxsDemagNttIntegralZ() {}
  OXS_DEMAG_REAL_ASYMP Compute(const OxsDemagNabPairData& data) const;
  /// ubase, uoff in data are wrt z
private:
  OxsDemagAsymptoticRefineInfo refine_info;
  const T Ntt;
};

template<typename T>
OXS_DEMAG_REAL_ASYMP
OxsDemagNttIntegralZ<T>::Compute(const OxsDemagNabPairData& ptdata) const
{ // Integral of asymptotic approximation to Ntt term, from zp to
  // infinity and -infinity to zm (zp must be > 0, and zm must be < 0),
  // with window z dimension Wz.  This routine is used internally for
  // periodic demag computations.

  assert(ptdata.ptp.x == ptdata.ptm.x &&
         ptdata.ptp.y == ptdata.ptm.y);

  // Alias data from refine_info structure.
  const OC_INDEX& xcount = refine_info.xcount;
  const OC_INDEX& ycount = refine_info.ycount;
  const OC_INDEX& zcount = refine_info.zcount;
  const OXS_DEMAG_REAL_ASYMP& rdx = refine_info.rdx;
  const OXS_DEMAG_REAL_ASYMP& rdy = refine_info.rdy;
  const OXS_DEMAG_REAL_ASYMP& rdz = refine_info.rdz;
  const OXS_DEMAG_REAL_ASYMP& result_scale = refine_info.result_scale;

  OxsDemagNabPairData work;
  work.uoff = ptdata.uoff;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INDEX k=1-zcount;k<zcount;++k) {
    work.ubase = ptdata.ubase + k*rdz;
    OXS_DEMAG_REAL_ASYMP za = work.ubase + work.uoff;
    OXS_DEMAG_REAL_ASYMP zs = work.ubase - work.uoff;
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INDEX j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.ptp.y+j*rdy; // .ptm.y == .ptp.y
      OxsDemagNabData::SetPair(ptdata.ptp.x,yoff,za,
                               ptdata.ptp.x,yoff,zs,
                               work.ptp, work.ptm);  // .ptm.x == .ptp.x
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Ntt.Compute(work);
      for(OC_INDEX i=1;i<xcount;++i) {
        OXS_DEMAG_REAL_ASYMP xoffa = ptdata.ptp.x+i*rdx; // .ptm.x == .ptp.x
        OxsDemagNabData::SetPair(xoffa,yoff,za,
                                 xoffa,yoff,zs,
                                 work.ptp, work.ptm);
        OXS_DEMAG_REAL_ASYMP tmpsum = Ntt.Compute(work);
        OXS_DEMAG_REAL_ASYMP xoffs = ptdata.ptp.x-i*rdx; // .ptm.x == .ptp.x
        OxsDemagNabData::SetPair(xoffs,yoff,za,
                                 xoffs,yoff,zs,
                                 work.ptp, work.ptm);
        tmpsum += Ntt.Compute(work);
        xsum += (xcount-i) * tmpsum;
      }
      // Weight x-strip interactions into xy-plate
      ysum += (ycount - abs(j))*xsum;
    }
    // Weight xy-plate interactions into total sum
    zsum += (zcount - abs(k))*ysum;
  }
  return zsum*result_scale;
}

typedef OxsDemagNttIntegralZ<OxsDemagNxxIntegralZBase> OxsDemagNxxIntegralZ;
typedef OxsDemagNttIntegralZ<OxsDemagNxyIntegralZBase> OxsDemagNxyIntegralZ;

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//
// Classes to compute periodic Nab kernels:
//
//     OxsDemagNabPeriodic  <-- abstract base class
//
//     OxsDemagNxxPeriodicX
//     OxsDemagNxyPeriodicX
//     OxsDemagNxxPeriodicZ
//     OxsDemagNxyPeriodicZ
//
// These four classes suffice to to compute any NabPeriodicC tensor for
// a,b,C \in {x,y,z}.  The wrapper classes Oxs_DemagPeriodicC, c \in
// {X,Y,Z}, provide the necessary coordinate permutations to use the
// above four classes to produce all 18 NabPeriodicC tensors.
//
// The OxsDemagNabPeriodic* classes are intended for internal use only.
// External code should use the Oxs_DemagPeriodicC wrappers.


class OxsDemagNabPeriodic {  // Interface class
public:
  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const = 0;

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const = 0;

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& data) const = 0;

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& data) const = 0;

  virtual OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const = 0;

protected:
  OxsDemagNabPeriodic(OXS_DEMAG_REAL_ASYMP idx,
                      OXS_DEMAG_REAL_ASYMP idy,
                      OXS_DEMAG_REAL_ASYMP idz)
    : dx(idx), dy(idy), dz(idz) {}
  OxsDemagNabPeriodic(const OxsDemagAdapter& ad)
    : dx(ad.x), dy(ad.y), dz(ad.z) {}

  virtual ~OxsDemagNabPeriodic() {}

  const OXS_DEMAG_REAL_ASYMP dx;
  const OXS_DEMAG_REAL_ASYMP dy;
  const OXS_DEMAG_REAL_ASYMP dz;
private:
  // Declare but don't define:
  OxsDemagNabPeriodic();
  OxsDemagNabPeriodic(const OxsDemagNabPeriodic&);
  OxsDemagNabPeriodic& operator=(const OxsDemagNabPeriodic&);

};


class OxsDemagNxxPeriodicX : public OxsDemagNabPeriodic {
public:
  OxsDemagNxxPeriodicX(OXS_DEMAG_REAL_ASYMP idx,
                       OXS_DEMAG_REAL_ASYMP idy,
                       OXS_DEMAG_REAL_ASYMP idz,
                       OXS_DEMAG_REAL_ASYMP maxerror,
                       int asymptotic_order,
                       OXS_DEMAG_REAL_ASYMP Wx,
                       OC_INDEX ktail)
    : OxsDemagNabPeriodic(idx,idy,idz),
      ANxx(idx,idy,idz,maxerror,asymptotic_order),
      NxxIx(OxsDemagAsymptoticRefineInfo(idx,idy,idz,Wx*ktail,
                                         maxerror,asymptotic_order),Wx)
  {}
  OxsDemagNxxPeriodicX(const OxsDemagAdapter& ad,
                       OXS_DEMAG_REAL_ASYMP maxerror,
                       int asymptotic_order,
                       OXS_DEMAG_REAL_ASYMP Wx,
                       OC_INDEX ktail)
    : OxsDemagNabPeriodic(ad),
      ANxx(ad.x,ad.y,ad.z,maxerror,asymptotic_order),
      NxxIx(OxsDemagAsymptoticRefineInfo(ad.x,ad.y,ad.z,Wx*ktail,
                                         maxerror,asymptotic_order),Wx)
  {}
  
  virtual ~OxsDemagNxxPeriodicX() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    OXS_DEMAG_REAL_ANALYTIC result
      = Oxs_CalculateNxx(data.x,data.y,data.z,dx,dy,dz);
    return static_cast<OXS_DEMAG_REAL_ASYMP>(result.Hi());
  }

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    return ANxx.Asymptotic(data);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  {
    return ANxx.AsymptoticPair(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    assert(pairdata.ptp.y == pairdata.ptm.y &&
           pairdata.ptp.z == pairdata.ptm.z);
    return NxxIx.Compute(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { return ANxx.GetAsymptoticStart(); }

private:
  const Oxs_DemagNxxAsymptotic ANxx;
  const OxsDemagNxxIntegralX NxxIx;
};

class OxsDemagNxyPeriodicX : public OxsDemagNabPeriodic {
public:
  OxsDemagNxyPeriodicX(OXS_DEMAG_REAL_ASYMP idx,
                       OXS_DEMAG_REAL_ASYMP idy,
                       OXS_DEMAG_REAL_ASYMP idz,
                       OXS_DEMAG_REAL_ASYMP maxerror,
                       int asymptotic_order,
                       OXS_DEMAG_REAL_ASYMP Wx,
                       OC_INDEX ktail)
  : OxsDemagNabPeriodic(idx,idy,idz),
    ANxy(idx,idy,idz,maxerror,asymptotic_order),
    NxyIx(OxsDemagAsymptoticRefineInfo(idx,idy,idz,Wx*ktail,
                                       maxerror,asymptotic_order),Wx)
  {}
  OxsDemagNxyPeriodicX(const OxsDemagAdapter& ad,
                       OXS_DEMAG_REAL_ASYMP maxerror,
                       int asymptotic_order,
                       OXS_DEMAG_REAL_ASYMP Wx,
                       OC_INDEX ktail)
    : OxsDemagNabPeriodic(ad),
      ANxy(ad.x,ad.y,ad.z,maxerror,asymptotic_order),
      NxyIx(OxsDemagAsymptoticRefineInfo(ad.x,ad.y,ad.z,Wx*ktail,
                                         maxerror,asymptotic_order),Wx)
  {}

  virtual ~OxsDemagNxyPeriodicX() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    OXS_DEMAG_REAL_ANALYTIC result
      = Oxs_CalculateNxy(data.x,data.y,data.z,dx,dy,dz);
    return static_cast<OXS_DEMAG_REAL_ASYMP>(result.Hi());
  }

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    return ANxy.Asymptotic(data);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  { // Nxy offers an AsymptoticPairX routine
    return ANxy.AsymptoticPairX(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    assert(pairdata.ptp.y == pairdata.ptm.y &&
           pairdata.ptp.z == pairdata.ptm.z);
    return NxyIx.Compute(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { return ANxy.GetAsymptoticStart(); }

private:
  const Oxs_DemagNxyAsymptotic ANxy;
  const OxsDemagNxyIntegralX NxyIx;
};

class OxsDemagNxxPeriodicZ : public OxsDemagNabPeriodic {
public:
  OxsDemagNxxPeriodicZ(OXS_DEMAG_REAL_ASYMP idx,
                       OXS_DEMAG_REAL_ASYMP idy,
                       OXS_DEMAG_REAL_ASYMP idz,
                       OXS_DEMAG_REAL_ASYMP maxerror,
                       int asymptotic_order,
                       OXS_DEMAG_REAL_ASYMP Wz,
                       OC_INDEX ktail)
    : OxsDemagNabPeriodic(idx,idy,idz),
      ANxx(idx,idy,idz,maxerror,asymptotic_order),
      NxxIz(OxsDemagAsymptoticRefineInfo(idx,idy,idz,Wz*ktail,
                                         maxerror,asymptotic_order),Wz)
  {}
  OxsDemagNxxPeriodicZ(const OxsDemagAdapter& ad,
                       OXS_DEMAG_REAL_ASYMP maxerror,
                       int asymptotic_order,
                       OXS_DEMAG_REAL_ASYMP Wz,
                       OC_INDEX ktail)
    : OxsDemagNabPeriodic(ad),
      ANxx(ad.x,ad.y,ad.z,maxerror,asymptotic_order),
      NxxIz(OxsDemagAsymptoticRefineInfo(ad.x,ad.y,ad.z,Wz*ktail,
                                         maxerror,asymptotic_order),Wz)
  {}

  virtual ~OxsDemagNxxPeriodicZ() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    OXS_DEMAG_REAL_ANALYTIC result =
      Oxs_CalculateNxx(data.x,data.y,data.z,dx,dy,dz);
    return static_cast<OXS_DEMAG_REAL_ASYMP>(result.Hi());
  }

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    return ANxx.Asymptotic(data);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  {
    return ANxx.AsymptoticPair(pairdata);

  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    assert(pairdata.ptp.x == pairdata.ptm.x &&
           pairdata.ptp.y == pairdata.ptm.y);
    return NxxIz.Compute(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { return ANxx.GetAsymptoticStart(); }

private:
  const Oxs_DemagNxxAsymptotic ANxx;
  const OxsDemagNxxIntegralZ NxxIz;
};

class OxsDemagNxyPeriodicZ : public OxsDemagNabPeriodic {
public:
  OxsDemagNxyPeriodicZ(OXS_DEMAG_REAL_ASYMP idx,
                       OXS_DEMAG_REAL_ASYMP idy,
                       OXS_DEMAG_REAL_ASYMP idz,
                       OXS_DEMAG_REAL_ASYMP maxerror,
                       int asymptotic_order,
                       OXS_DEMAG_REAL_ASYMP Wz,
                       OC_INDEX ktail)
    : OxsDemagNabPeriodic(idx,idy,idz),
      ANxy(idx,idy,idz,maxerror,asymptotic_order),
      NxyIz(OxsDemagAsymptoticRefineInfo(idx,idy,idz,Wz*ktail,
                                         maxerror,asymptotic_order),Wz)
  {}
  OxsDemagNxyPeriodicZ(const OxsDemagAdapter& ad,
                       OXS_DEMAG_REAL_ASYMP maxerror,
                       int asymptotic_order,
                       OXS_DEMAG_REAL_ASYMP Wz,
                       OC_INDEX ktail)
    : OxsDemagNabPeriodic(ad),
      ANxy(ad.x,ad.y,ad.z,maxerror,asymptotic_order),
      NxyIz(OxsDemagAsymptoticRefineInfo(ad.x,ad.y,ad.z,Wz*ktail,
                                         maxerror,asymptotic_order),Wz)
  {}

  virtual ~OxsDemagNxyPeriodicZ() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    OXS_DEMAG_REAL_ANALYTIC result =
      Oxs_CalculateNxy(data.x,data.y,data.z,dx,dy,dz);
    return static_cast<OXS_DEMAG_REAL_ASYMP>(result.Hi());
  }

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    return ANxy.Asymptotic(data);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  {
    return ANxy.AsymptoticPairZ(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    assert(pairdata.ptp.x == pairdata.ptm.x &&
           pairdata.ptp.y == pairdata.ptm.y);
    return NxyIz.Compute(pairdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { return ANxy.GetAsymptoticStart(); }

private:
  const Oxs_DemagNxyAsymptotic ANxy;
  const OxsDemagNxyIntegralZ NxyIz;
};

// Extension template
template<class NabPeriodicC,class Adapter>
class OxsTDemagNabPeriodicC : public OxsDemagNabPeriodic {
public:
  OxsTDemagNabPeriodicC(OXS_DEMAG_REAL_ASYMP idx,
                        OXS_DEMAG_REAL_ASYMP idy,
                        OXS_DEMAG_REAL_ASYMP idz,
                        OXS_DEMAG_REAL_ASYMP maxerror,
                        int asymptotic_order,
                        OXS_DEMAG_REAL_ASYMP W,
                        OC_INDEX ktail)
    : OxsDemagNabPeriodic(Adapter(idx,idy,idz)),
      Nab(Adapter(idx,idy,idz),maxerror,asymptotic_order,W,ktail) {}

  virtual ~OxsTDemagNabPeriodicC() {}

  virtual OXS_DEMAG_REAL_ASYMP NearField(const OxsDemagNabData& data) const
  {
    OxsTDemagNabData<Adapter> newdata(data);
    return Nab.NearField(newdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP FarField(const OxsDemagNabData& data) const
  {
    OxsTDemagNabData<Adapter> newdata(data);
    return Nab.FarField(newdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarFieldPair(const OxsDemagNabPairData& pairdata) const
  {
    OxsTDemagNabPairData<Adapter> newdata(pairdata);
    return Nab.FarFieldPair(newdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP
  FarIntegralPair(const OxsDemagNabPairData& pairdata) const
  {
    OxsTDemagNabPairData<Adapter> newdata(pairdata);
    return Nab.FarIntegralPair(newdata);
  }

  virtual OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  { return Nab.GetAsymptoticStart(); }

private:
  const NabPeriodicC Nab;
};


////////////////////////////////////////////////////////////////////////
// PBC Demag Tensor Relations
//
// There are 18 periodic demag tensor terms Nab^d, where ab is in {xx,
// xy, xz, yy, yz, zz} and direction of periodicity d is in {x, y, z}.
// However, using symmetries these 18 terms can be represented via the
// four terms defined above, Nxx^x, Nxy^x, Nxx^z and Nxy^z, by simple
// interchange of coordinates.  These relations are:
//
// |      |            Direction of Periodicity           |
// | Term |     x         |     y         |     z         |
// +------+---------------+---------------+---------------+
// |  Nxx |     -         |  Nxx^z(x,z,y) |     -         |
// |  Nyy |  Nxx^z(y,z,x) |  Nxx^x(y,x,z) |  Nxx^z(y,x,z) |
// |  Nzz |  Nxx^z(z,y,x) |  Nxx^z(z,x,y) |  Nxx^x(z,y,x) |
// +------+---------------+---------------+---------------+
// |  Nxy |     -         |  Nxy^x(y,x,z) |     -         |
// |  Nxz |  Nxy^x(x,z,y) |  Nxy^z(x,z,y) |  Nxy^x(z,x,y) |
// |  Nyz |  Nxy^z(y,z,x) |  Nxy^x(y,z,x) |  Nxy^x(z,y,x) |
// +------+---------------+---------------+---------------+
//
// To save space, the above table lists only the (x,y,z) triples,
// but the Nab values depend on the cell dimension (dx,dy,dz) as
// well.  The (dx,dy,dz) are permuted in the same manner as the
// (x,y,z).
//
// See NOTES V, 2-Mar-2011, p162.
//
// However, rather that write wrappers for all 18 calls, instead we
// just write the six interface functions actually called by the
// OOMMF code, and embed the above relations into those interface
// functions.
//
// Initialization imports dx, dy, and dz are typically in problem
// units (i.e., meters), but clients are free to scale (x,y,z) and
// (dx,dy,dz) if desired.
//
// Import W is the width of the simulation window along the periodic
// direction, in other words, the length of the period.  This is
// measured in the same units as (x,y,z) and (dx,dy,dz), e.g., meters.
//
// The import AsymptoticRadius is radius beyond which asymptotics are
// used to replace the closed-form analytic representations for the
// demag tensor.  This quantity should be scaled with (dx,dy,dz).  In
// the non-periodic demag code setting AsymptoticRadius to zero has the
// effect of using the closed-form analytic representations at all
// points.  This is not possible for the periodic code, because the
// image tail computation is based on the asymptotic formulas.  If the
// AsymptoticRadius import is <=0, then it is reset to match W/2.
// WARNING: If W is very short, then this is probably *not* what one
// wants.
//
// IMPORTANT NOTE: The following routines DO include the
//                 Nab component from the base window.

class Oxs_DemagPeriodic
{
public:
  virtual void ComputePeriodicHoleTensor(OC_INDEX i,OC_INDEX j,OC_INDEX k,
                                         OXS_DEMAG_REAL_ASYMP &pbcNxx,
                                         OXS_DEMAG_REAL_ASYMP &pbcNxy,
                                         OXS_DEMAG_REAL_ASYMP &pbcNxz,
                                         OXS_DEMAG_REAL_ASYMP &pbcNyy,
                                         OXS_DEMAG_REAL_ASYMP &pbcNyz,
                                         OXS_DEMAG_REAL_ASYMP &pbcNzz) = 0;

  virtual ~Oxs_DemagPeriodic() {} // Ensure child destructors get called.

  OXS_DEMAG_REAL_ASYMP GetAsymptoticStart() const
  {
    return Nxx.GetAsymptoticStart(); // Assume all terms have same
    /// asymptotic start radius.
  }
  
protected:
  Oxs_DemagPeriodic(OXS_DEMAG_REAL_ASYMP idx, // Cell sizes
                    OXS_DEMAG_REAL_ASYMP idy,
                    OXS_DEMAG_REAL_ASYMP idz,
                    OXS_DEMAG_REAL_ASYMP maxerror,
                    int asymptotic_order,
                    OC_INDEX irdimx, // Periodic window dimension,
                    OC_INDEX iwdimx, // Pre-computed (mostly) analytic
                    OC_INDEX iwdimy, // window dimensions
                    OC_INDEX iwdimz);
  // The (wdimx,wdimy,wdimz) pre-computed window serves two purposes.
  // First, it uses a fast method to compute the analytic tensor values.
  // Second, it marks the boundaries outside of which periodic, asymptotic
  // values need to be computed and summed into the pre-computed values
  // to get the full periodic tensor.  This suffices for the 1D periodic
  // case; this needs to be revisited when coding up the 2D periodic case
  // to insure there aren't any values outside the pre-computed window
  // which get double-counted.

  void ComputeAsymptoticLimits
  (OC_INDEX i,
   /// x = i*dx = base offset along periodic (x) direction.
   /// Member value wdimx = width of analytic computation window in units of dx
   OC_INDEX j, OC_INDEX k, // Other coordinate indices
   OC_INDEX& k1, OC_INDEX& k2,   // Near field terms
   OC_INDEX& k1a, OC_INDEX& k2a, // Asymmetric far field
   OXS_DEMAG_REAL_ASYMP& newx,   // Symmetric far field
   OXS_DEMAG_REAL_ASYMP& newoffset
   ) const;

  // Computes tensor assuming periodic direction is x, NOT including
  // contributions from the base window.  (The base window includes
  // all points x=i*dx, y=j*dy, z=k*dz such that |i|<windx, |j|<windy,
  // and |k|<windz.
  //   Other periodic directions can be handled by rotating coordinates.
  // The necessary coordinate rotations involve the dx, dy, dz
  // constructor imports, the i,j,k imports for this function, and the
  // pbcN?? outputs.  The necessary gyrations are implemented in child
  // classes.
  void ComputeHoleTensorPBCx(OC_INDEX i,
                             OC_INDEX j,
                             OC_INDEX k,
                             OXS_DEMAG_REAL_ASYMP& pbcNxx,
                             OXS_DEMAG_REAL_ASYMP& pbcNxy,
                             OXS_DEMAG_REAL_ASYMP& pbcNxz,
                             OXS_DEMAG_REAL_ASYMP& pbcNyy,
                             OXS_DEMAG_REAL_ASYMP& pbcNyz,
                             OXS_DEMAG_REAL_ASYMP& pbcNzz) const;

private:
  enum { TAIL_TWEAK_COUNT = 8 };
  static const OXS_DEMAG_REAL_ASYMP D[TAIL_TWEAK_COUNT];

  static OC_INDEX ComputeTailOffset(OXS_DEMAG_REAL_ASYMP dx,
                                    OXS_DEMAG_REAL_ASYMP dy,
                                    OXS_DEMAG_REAL_ASYMP dz,
                                    OC_INDEX rdimx);
  
  OXS_DEMAG_REAL_ASYMP dx, dy, dz;
  const OC_INDEX rdimx;
  const OC_INDEX wdimx,wdimy,wdimz;
  const OC_INDEX ktail;

  const OxsDemagNxxPeriodicX Nxx;
  const OxsDemagNxyPeriodicX Nxy;
  const OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicX,OxsDemagXZYAdapter> Nxz;
  const OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicZ,OxsDemagYZXAdapter> Nyy;
  const OxsTDemagNabPeriodicC<OxsDemagNxyPeriodicZ,OxsDemagYZXAdapter> Nyz;
  const OxsTDemagNabPeriodicC<OxsDemagNxxPeriodicZ,OxsDemagZYXAdapter> Nzz;

  // The following members are declared but not defined:
  Oxs_DemagPeriodic(const Oxs_DemagPeriodic&);
  Oxs_DemagPeriodic& operator=(const Oxs_DemagPeriodic&);
};

class Oxs_DemagPeriodicX : public Oxs_DemagPeriodic
{
public:
  Oxs_DemagPeriodicX(OXS_DEMAG_REAL_ASYMP dx,
                     OXS_DEMAG_REAL_ASYMP dy,
                     OXS_DEMAG_REAL_ASYMP dz,
                     OXS_DEMAG_REAL_ASYMP maxerror,
                     int asymptotic_order,
                     OC_INDEX rdimx,
                     OC_INDEX wdimx,OC_INDEX wdimy,OC_INDEX wdimz)
    : Oxs_DemagPeriodic(dx,dy,dz,maxerror,asymptotic_order,
                        rdimx,wdimx,wdimy,wdimz) {}

  void ComputePeriodicHoleTensor(OC_INDEX i,OC_INDEX j,OC_INDEX k,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxx,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxy,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxz,
                                 OXS_DEMAG_REAL_ASYMP &pbcNyy,
                                 OXS_DEMAG_REAL_ASYMP &pbcNyz,
                                 OXS_DEMAG_REAL_ASYMP &pbcNzz) {
    ComputeHoleTensorPBCx(i,j,k,pbcNxx,pbcNxy,pbcNxz,pbcNyy,pbcNyz,pbcNzz);
  }
};


class Oxs_DemagPeriodicY : public Oxs_DemagPeriodic
{ // Rotate coordinates (x,y,z) -> (y,z,x)
public:
  Oxs_DemagPeriodicY(OXS_DEMAG_REAL_ASYMP dx,
                     OXS_DEMAG_REAL_ASYMP dy,
                     OXS_DEMAG_REAL_ASYMP dz,
                     OXS_DEMAG_REAL_ASYMP maxerror,
                     int asymptotic_order,
                     OC_INDEX rdimy,
                     OC_INDEX wdimx,OC_INDEX wdimy,OC_INDEX wdimz)
    : Oxs_DemagPeriodic(dy,dz,dx,maxerror,asymptotic_order,
                        rdimy,wdimy,wdimz,wdimx) {}

  void ComputePeriodicHoleTensor(OC_INDEX i,OC_INDEX j,OC_INDEX k,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxx,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxy,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxz,
                                 OXS_DEMAG_REAL_ASYMP &pbcNyy,
                                 OXS_DEMAG_REAL_ASYMP &pbcNyz,
                                 OXS_DEMAG_REAL_ASYMP &pbcNzz) {
    ComputeHoleTensorPBCx(j,k,i,pbcNyy,pbcNyz,pbcNxy,pbcNzz,pbcNxz,pbcNxx);
  }
};

class Oxs_DemagPeriodicZ : public Oxs_DemagPeriodic
{ // Rotate coordinates (x,y,z) -> (z,x,y)
public:
  Oxs_DemagPeriodicZ(OXS_DEMAG_REAL_ASYMP dx,
                     OXS_DEMAG_REAL_ASYMP dy,
                     OXS_DEMAG_REAL_ASYMP dz,
                     OXS_DEMAG_REAL_ASYMP maxerror,
                     int asymptotic_order,
                     OC_INDEX rdimz,
                     OC_INDEX wdimx,OC_INDEX wdimy,OC_INDEX wdimz)
    : Oxs_DemagPeriodic(dz,dx,dy,maxerror,asymptotic_order,
                        rdimz,wdimz,wdimx,wdimy) {}

  void ComputePeriodicHoleTensor(OC_INDEX i,OC_INDEX j,OC_INDEX k,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxx,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxy,
                                 OXS_DEMAG_REAL_ASYMP &pbcNxz,
                                 OXS_DEMAG_REAL_ASYMP &pbcNyy,
                                 OXS_DEMAG_REAL_ASYMP &pbcNyz,
                                 OXS_DEMAG_REAL_ASYMP &pbcNzz) {
    ComputeHoleTensorPBCx(k,i,j,pbcNzz,pbcNxz,pbcNyz,pbcNxx,pbcNxy,pbcNyy);
  }                         
};

#endif // _OXS_DEMAGCOEF
