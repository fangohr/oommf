/* FILE: demagcoef.cc            -*-Mode: c++-*-
 *
 * Demag coefficients.
 *
 * Constant magnetization demag routines, based on formulae presented in
 * "A Generalization of the Demagnetizing Tensor for Nonuniform
 * Magnetization," by Andrew J. Newell, Wyn Williams, and David
 * J. Dunlop, Journal of Geophysical Research - Solid Earth, vol 98,
 * p 9551-9555, June 1993.  This formulae clearly satisfy necessary
 * symmetry and scaling properties, which is not true of the formulae
 * presented in "Magnetostatic Energy Calculations in Two- and
 * Three-Dimensional Arrays of Ferromagnetic Prisms," M. Maicas,
 * E. Lopez, M. CC. Sanchez, C. Aroca and P. Sanchez, IEEE Trans Mag,
 * vol 34, May 1998, p601-607.  (Side note: The units in the latter
 * paper are apparently cgs.)  It appears likely that there is an error
 * in the latter paper (attempts to implement the formulae presented
 * there did not produce the proper symmetries), as well as in the older
 * paper, "Magnetostatic Interaction Fields for a Three-Dimensional
 * Array of Ferromagnetic Cubes," Manfred E. Schabes and Amikam Aharoni,
 * IEEE Trans Mag, vol 23, November 1987, p3882-3888.  (Note: The Newell
 * paper deals with uniformly sized rectangular prisms, the Maicas paper
 * allows non-uniformly sized rectangular prisms, and the Schabes paper
 * only considers cubes.)
 *
 *   The kernel here is based on an analytically derived energy, and the
 * effective (discrete) demag field is calculated from the (discrete)
 * energy.
 *
 * Check values: Below are several values for Nxx and Nxy, calculated
 *  using Maple 7.0 with 100 decimal digits precision, rounded to 50
 *  digits for display.  Normal double precision IEEE floating point
 *  provides approximately 16 decimal digits of accuracy, and so-called
 *  "quadruple" precision provides about 34 decimal digits.
 *
 * 
 *  x  y  z  dx dy dz |  Nxx(x,y,z,dx,dy,dz)
 * -------------------+-------------------------------------------------------
 *  0  0  0  50 10  1 |  0.021829576458713811627717362556500594396802771830582
 *  0  0  0   1  1  1 |  0.33333333333333333333333333333333333333333333333333
 *  0  0  0   1  1  2 |  0.40084192360558096752690050789034014000452668298259
 *  0  0  0   2  1  1 |  0.19831615278883806494619898421931971999094663403481
 *  0  0  0   1  2  3 |  0.53879030592371444784959040590642585972177691027128
 *  0  0  0   2  1  3 |  0.27839171603589255540904462483920117770716945564364
 *  0  0  0   3  2  1 |  0.18281797804039299674136496925437296257105363408509
 *  1  0  0   1  1  1 | -0.13501718054449526838713434911401361334238669929852
 *  0  1  0   1  1  1 |  0.067508590272247634193567174557006806671193349649259
 *  1  1  0   1  2  3 | -0.083703755298020462084677435631518487669613050161980
 *  1  1  1   1  1  1 |  0
 *  1  1  1   1  2  3 | -0.056075776617493854142226134670956166201885395511639
 *  1  2  3   1  2  3 |  0.0074263570277919738841364667668809954237071479183522
 * 10  1  1   1  2  3 | -0.00085675752896240944969766580856030369571736381174932
 * 10  4  6   1  2  3 | -0.00025381260722622800624859078080421562302790329870984
 *  3  6  9   1  2  3 |  0.00027042781311956323573639739731014558846864626288393
 *  6  6  6   1  2  3 | -0.000017252712652486259473939673630209925239022411008957
 *
 *  x  y  z  dx dy dz |  Nxy(x,y,z,dx,dy,dz)
 * -------------------+-------------------------------------------------------
 *  0  0  0  50 10  1 |  0
 *  0  0  0   1  1  1 |  0
 *  0  0  0   1  1  2 |  0
 *  0  0  0   2  1  1 |  0
 *  1  0  0   1  1  1 |  0
 *  0  1  0   1  1  1 |  0
 *  1  1  0   1  2  3 | -0.077258075615212400146921495217230818857603260305042
 *  1  1  1   1  1  1 | -0.016062127810508233979724830686189874772059681376565
 *  1  1  1   1  2  3 | -0.060966146490263272608967587158170018091418469887162
 *  1  2  3   1  2  3 | -0.0088226536707711039322880437795490754270432698781039
 * 10  1  1   1  2  3 | -0.00012776400247172360221892601892504762520639604467656
 * 10  4  6   1  2  3 | -0.00020004764005741154294387738750612412053502841766241
 *  3  6  9   1  2  3 | -0.00015720240165711869024193166157368874130207143569916
 *  6  6  6   1  2  3 | -0.00043908646098482886546108269881031774163900540796564
 *
 *
 *
 * STANDALONE NOTE: This file can also be built as a standalone
 * executable to test the computed tensor elements for periodic
 * and non-periodic boundary conditions.  For example:
 *
 *    g++ -DSTANDALONE -DOC_USE_SSE=1 demagcoef.cc -lm -o pbctest
 *
 * See '#ifdef STANDALONE / #endif' blocks for details.
 *
 */

#if defined(STANDALONE) && STANDALONE==0
# undef STANDALONE
#endif

// By default, MinGW uses I/O from the Windows Microsoft C runtime,
// which doesn't support 80-bit floats.  Substitute MinGW versions for
// printf etc.  This macro must be set before stdio.h is included.
#if defined(STANDALONE) && (defined(__MINGW32__) || defined(__MINGW64__))
# define __USE_MINGW_ANSI_STDIO 1
#endif

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef USE_LOG1P
# define USE_LOG1P 1  // Use log1p in place of log(1+x) where possible.
/// This should make the results more accurate, but test results are
/// mixed.
#endif

#ifndef STANDALONE
# include "oc.h"
# include "nb.h"
# include "demagcoef.h"
  OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// For some compilers this is needed to get "long double"
/// versions of the basic math library functions, e.g.,
/// long double atan(long double);
#endif // STANDALONE

/* End includes */

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
//
// The "STANDALONE" blocks provide support for building this file
// + demagcoef.h as a standalone application for PBC testing.
//
#ifdef STANDALONE

#ifndef OC_REALWIDE_IS_REAL8
# define OC_REALWIDE_IS_REAL8 0
#endif

typedef long OC_INDEX;
typedef int  OC_INT4m;
typedef double OC_REAL8m;

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


#if OC_REALWIDE_IS_REAL8==0  // OC_REALWIDE is long double

typedef long double OC_REALWIDE;
// Define wrappers for long double versions of standard math functions.
inline OC_REALWIDE Oc_SqrtRW(OC_REALWIDE x) { return sqrtl(x); }
inline OC_REALWIDE Oc_FloorRW(OC_REALWIDE x) { return floorl(x); }
inline OC_REALWIDE Oc_CeilRW(OC_REALWIDE x) { return ceill(x); }
inline OC_REALWIDE Oc_LogRW(OC_REALWIDE x) { return logl(x); }
inline OC_REALWIDE Oc_PowRW(OC_REALWIDE x,OC_REALWIDE y)  { return powl(x,y); }
inline OC_REALWIDE Oc_FabsRW(OC_REALWIDE x) { return fabsl(x); }
inline OC_REALWIDE Oc_AtanRW(OC_REALWIDE x) { return atanl(x); }
inline OC_REALWIDE Oc_Atan2RW(OC_REALWIDE y,OC_REALWIDE x) {
  if(x==0.0 && y==0.0) return 0.0L;
  return atan2l(y,x);
}
#else   // OC_REALWIDE is double

typedef double OC_REALWIDE;
// Define wrappers for double versions of standard math functions.
inline OC_REALWIDE Oc_SqrtRW(OC_REALWIDE x) { return sqrt(x); }
inline OC_REALWIDE Oc_FloorRW(OC_REALWIDE x) { return floor(x); }
inline OC_REALWIDE Oc_CeilRW(OC_REALWIDE x) { return ceil(x); }
inline OC_REALWIDE Oc_LogRW(OC_REALWIDE x) { return log(x); }
inline OC_REALWIDE Oc_PowRW(OC_REALWIDE x,OC_REALWIDE y)  { return pow(x,y); }
inline OC_REALWIDE Oc_FabsRW(OC_REALWIDE x) { return fabs(x); }
inline OC_REALWIDE Oc_AtanRW(OC_REALWIDE x) { return atan(x); }
inline OC_REALWIDE Oc_Atan2RW(OC_REALWIDE y,OC_REALWIDE x) {
  if(x==0.0 && y==0.0) return 0.0;
  return atan2(y,x);
}
#endif // OC_REALWIDE == double?

OC_REALWIDE Oc_Nop(OC_REALWIDE);  // Declaration; definition is a end of file.

OC_REALWIDE Oc_Log1pRW(OC_REALWIDE x)
{ // Routine to calculate log(1+x), accurate for small x.
  // The log1p function is in the C99 spec.  That version
  // is probably fast and more accurate than the version below,
  // so should probably use that if available.
  //    BTW, for very small values of x a short series expansion
  // of log(1+x) will be faster than the following code.
  if(Oc_FabsRW(x)>=0.5) return Oc_LogRW(1.0+x);
  // Otherwise, use little trick.  One should check that
  // compiler doesn't screw this up with "extra" precision.
  OC_REALWIDE y1 = Oc_Nop(1.0 + x);
  if(y1 == 1.0) return x;
  OC_REALWIDE y2 = y1 - 1.0;
  OC_REALWIDE rat = Oc_LogRW(y1)/y2;
  return x*rat;
}

/* Wrappers that self-select by type */
inline double Oc_Floor(double x) { return floor(x); }
inline double Oc_Ceil(double x) { return ceil(x); }
inline double Oc_Sqrt(double x) { return sqrt(x); }
inline double Oc_Fabs(double x) { return fabs(x); }
inline double Oc_Pow(double x,double y) { return pow(x,y); }

inline long double Oc_Floor(long double x) { return floorl(x); }
inline long double Oc_Ceil(long double x) { return ceill(x); }
inline long double Oc_Sqrt(long double x) { return sqrtl(x); }
inline long double Oc_Fabs(long double x) { return fabsl(x); }
inline long double Oc_Pow(long double x,long double y) { return powl(x,y); }

#if 0
inline double Oc_Log(double x) { return log(x); }
inline double Oc_Atan(double x) { return atan(x); }
inline double Oc_Atan2(double y,double x) { return atan2(y,x); }

inline long double Oc_Log(long double x) { return logl(x); }
inline long double Oc_Atan(long double x) { return atanl(x); }
inline long double Oc_Atan2(long double y,long double x) { return atan2l(y,x); }
#endif

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

  Oc_Duet& operator=(const Oc_Duet &other) { value = other.value; return *this; }


  Oc_Duet& SetZero() { value = _mm_setzero_pd(); return *this; }


  Oc_Duet& operator+=(const Oc_Duet& other) {
    value = _mm_add_pd(value,other.value); return *this;
  }
  Oc_Duet& operator+=(const double& x) {
    value = _mm_add_pd(value,_mm_set1_pd(x)); return *this;
  }
  Oc_Duet& operator-=(const Oc_Duet& other) {
    value = _mm_sub_pd(value,other.value); return *this;
  }
  Oc_Duet& operator-=(const double& x) {
    value = _mm_sub_pd(value,_mm_set1_pd(x)); return *this;
  }
  Oc_Duet& operator*=(const Oc_Duet& other) {
    value = _mm_mul_pd(value,other.value); return *this;
  }
  Oc_Duet& operator*=(const double& x) {
    value = _mm_mul_pd(value,_mm_set1_pd(x)); return *this;
  }
  Oc_Duet& operator/=(const Oc_Duet& other) {
    value = _mm_div_pd(value,other.value); return *this;
  }
  Oc_Duet& operator/=(const double& x) {
    value = _mm_div_pd(value,_mm_set1_pd(x)); return *this;
  }

  // Use GetA/GetB for temporary use, but StoreA/StoreB
  // are more efficient for permanent storage.
  double GetA() const { return ((double *)&value)[0]; }
  double GetB() const { return ((double *)&value)[1]; }

  void StoreA(double& Aout) const { _mm_storel_pd(&Aout,value); }
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

  friend const Oc_Duet Oc_Sqrt(const Oc_Duet& w);

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


inline const Oc_Duet Oc_Sqrt(const Oc_Duet& w)
{ return Oc_Duet(_mm_sqrt_pd(w.value)); }

#endif // OXS_DEMAG_ASYMP_USE_SSE


#ifndef PI
# define PI       OC_REAL8m(3.141592653589793238462643383279502884L)
#endif

#ifndef WIDE_PI
# define WIDE_PI  OC_REALWIDE(3.141592653589793238462643383279502884L)
#endif

#include "demagcoef.h"

#endif // STANDALONE

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////
// Routines to do accurate summation
extern "C" {
static int AS_Compare(const void* px,const void* py)
{
  // Comparison based on absolute values
  OC_REALWIDE x=Oc_FabsRW(*((const OC_REALWIDE *)px));
  OC_REALWIDE y=Oc_FabsRW(*((const OC_REALWIDE *)py));
  if(x<y) return 1;
  if(x>y) return -1;
  return 0;
}
}

static OC_REALWIDE
Oxs_AccurateSum(int n,OC_REALWIDE *arr)
{
  // Order by decreasing magnitude
  qsort(arr,n,sizeof(OC_REALWIDE),AS_Compare);

  // Add up using doubly compensated summation.  If necessary, mark
  // these variables "volatile" to protect against problems arising
  // from extra precision.  Also, don't expect the compiler to respect
  // order with respect to parentheses at high levels of optimization,
  // i.e., write "u=x; u-=(y-corr)" as opposed to "u=x-(y-corr)".
#if (OC_REALWIDE_IS_REAL8 && OC_FP_DOUBLE_EXTRA_PRECISION) \
  || (!OC_REALWIDE_IS_REAL8 && OC_FP_LONG_DOUBLE_EXTRA_PRECISION)
  volatile OC_REALWIDE sum;  volatile OC_REALWIDE corr;
  volatile OC_REALWIDE y;    volatile OC_REALWIDE u;
  volatile OC_REALWIDE t;    volatile OC_REALWIDE v;
  volatile OC_REALWIDE z;    volatile OC_REALWIDE x;
  volatile OC_REALWIDE tmp;
#else
  OC_REALWIDE sum,corr,y,u,t,v,z,x,tmp;
#endif

  sum=arr[0]; corr=0;
  for(int i=1;i<n;i++) {
    x=arr[i];
    y=corr+x;
    tmp=y-corr;
    u=x-tmp;
    t=y+sum;
    tmp=t-sum;
    v=y-tmp;
    z=u+v;
    sum=t+z;
    tmp=sum-t;
    corr=z-tmp;
  }
  return sum;
}


////////////////////////////////////////////////////////////////////////////
// Routines to calculate kernel coefficients
// See Newell et al. for details. The code below follows the
// naming conventions in that paper.

#ifdef OLDE_CODE
OC_REALWIDE Oxs_SelfDemagNx(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // Here Hx = -Nxx.Mx (formula (16) in Newell).
  // Note: egcs-2.91.57 on Linux/x86 with -O1 mangles this
  //  function (produces NaN's) unless we manually group terms.

  if(x<=0.0 || y<=0.0 || z<=0.0) return 0.0;
  if(x==y && y==z) {  // Special case: cube
    return OC_REALWIDE(1.)/OC_REALWIDE(3.);
  }

  OC_REALWIDE xsq=x*x,ysq=y*y,zsq=z*z;
  OC_REALWIDE diag=Oc_SqrtRW(xsq+ysq+zsq);
  OC_REALWIDE arr[15];

  OC_REALWIDE mpxy = (x-y)*(x+y);
  OC_REALWIDE mpxz = (x-z)*(x+z);

  arr[0] = -4*(2*xsq*x-ysq*y-zsq*z);
  arr[1] =  4*(xsq+mpxy)*Oc_SqrtRW(xsq+ysq);
  arr[2] =  4*(xsq+mpxz)*Oc_SqrtRW(xsq+zsq);
  arr[3] = -4*(ysq+zsq)*Oc_SqrtRW(ysq+zsq);
  arr[4] = -4*diag*(mpxy+mpxz);

  arr[5] = 24*x*y*z*Oc_AtanRW(y*z/(x*diag));
  arr[6] = 12*(z+y)*xsq*Oc_LogRW(x);

  arr[7] =  12*z*ysq*Oc_LogRW((Oc_SqrtRW(ysq+zsq)+z)/y);
  arr[8] = -12*z*xsq*Oc_LogRW(Oc_SqrtRW(xsq+zsq)+z);
  arr[9] =  12*z*mpxy*Oc_LogRW(diag+z);
  arr[10] = -6*z*mpxy*Oc_LogRW(xsq+ysq);

  arr[11] =  12*y*zsq*Oc_LogRW((Oc_SqrtRW(ysq+zsq)+y)/z);
  arr[12] = -12*y*xsq*Oc_LogRW(Oc_SqrtRW(xsq+ysq)+y);
  arr[13] =  12*y*mpxz*Oc_LogRW(diag+y);
  arr[14] =  -6*y*mpxz*Oc_LogRW(xsq+zsq);

  OC_REALWIDE Nxx = Oxs_AccurateSum(15,arr)/(12*WIDE_PI*x*y*z);
  return Nxx;
}
#endif // OLDE_CODE

OC_REALWIDE Oxs_SelfDemagNx(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // Here Hx = -Nxx.Mx (formula (16) in Newell).
  // Note: egcs-2.91.57 on Linux/x86 with -O1 mangles this
  //  function (produces NaN's) unless we manually group terms.
  // See NOTES V, 14-Mar-2011, p. 181-184 for details on
  // accurate numerics.

  if(x<=0.0 || y<=0.0 || z<=0.0) return 0.0;
  if(x==y && y==z) {  // Special case: cube
    return OC_REALWIDE(1.)/OC_REALWIDE(3.);
  }

  OC_REALWIDE xsq=x*x,ysq=y*y,zsq=z*z;
  OC_REALWIDE R   = Oc_SqrtRW(xsq+ysq+zsq);
  OC_REALWIDE Rxy = Oc_SqrtRW(xsq+ysq);
  OC_REALWIDE Rxz = Oc_SqrtRW(xsq+zsq);
  OC_REALWIDE Ryz = Oc_SqrtRW(ysq+zsq);


  OC_REALWIDE arr[8];

  arr[0] = 2*x*y*z
    * ( (x/(x+Rxy)+(2*xsq+ysq+zsq)/(R*Rxy+x*Rxz))
        /(x+Rxz)
      + (x/(x+Rxz)+(2*xsq+ysq+zsq)/(R*Rxz+x*Rxy))
        /(x+Rxy))
    / ((x+R)*(Rxy+Rxz+R));
  arr[1] = -1*x*y*z
    * ( (y/(y+Rxy)+(2*ysq+xsq+zsq)/(R*Rxy+y*Ryz))
        /(y+Ryz)
      + (y/(y+Ryz)+(2*ysq+xsq+zsq)/(R*Ryz+y*Rxy))
        /(y+Rxy))
    / ((y+R)*(Rxy+Ryz+R));
  arr[2] = -1*x*y*z
    * ( (z/(z+Rxz)+(2*zsq+xsq+ysq)/(R*Rxz+z*Ryz))
        /(z+Ryz)
      + (z/(z+Ryz)+(2*zsq+xsq+ysq)/(R*Ryz+z*Rxz))
        /(z+Rxz))
    / ((z+R)*(Rxz+Ryz+R));

  arr[3] =  6*Oc_AtanRW(y*z/(x*R));

  OC_REALWIDE piece4 = -y*z*z*(1/(x+Rxz)+y/(Rxy*Rxz+x*R))/(Rxz*(y+Rxy));
  if(piece4 > -0.5) {
#if USE_LOG1P
    arr[4] = 3*x*Oc_Log1pRW(piece4)/z;
#else
    arr[4] = 3*x*Oc_LogRW(1.0+piece4)/z;
#endif
  } else {
    arr[4] = 3*x*Oc_LogRW(x*(y+R)/(Rxz*(y+Rxy)))/z;
  }

  OC_REALWIDE piece5 = -y*y*z*(1/(x+Rxy)+z/(Rxy*Rxz+x*R))/(Rxy*(z+Rxz));
  if(piece5 > -0.5) {
#if USE_LOG1P
    arr[5] = 3*x*Oc_Log1pRW(piece5)/y;
#else
    arr[5] = 3*x*Oc_LogRW(1.0+piece5)/y;
#endif
  } else {
    arr[5] = 3*x*Oc_LogRW(x*(z+R)/(Rxy*(z+Rxz)))/y;
  }

  OC_REALWIDE piece6 = -x*x*z*(1/(y+Rxy)+z/(Rxy*Ryz+y*R))/(Rxy*(z+Ryz));
  if(piece6 > -0.5) {
#if USE_LOG1P
    arr[6] = -3*y*Oc_Log1pRW(piece6)/x;
#else
    arr[6] = -3*y*Oc_LogRW(1.0+piece6)/x;
#endif
  } else {
    arr[6] = -3*y*Oc_LogRW(y*(z+R)/(Rxy*(z+Ryz)))/x;
  }

  OC_REALWIDE piece7 = -x*x*y*(1/(z+Rxz)+y/(Rxz*Ryz+z*R))/(Rxz*(y+Ryz));
  if(piece7 > -0.5) {
#if USE_LOG1P
    arr[7] = -3*z*Oc_Log1pRW(piece7)/x;
#else
    arr[7] = -3*z*Oc_LogRW(1.0+piece7)/x;
#endif
  } else {
    arr[7] = -3*z*Oc_LogRW(z*(y+R)/(Rxz*(y+Ryz)))/x;
  }

  OC_REALWIDE Nxx = Oxs_AccurateSum(8,arr)/(3*WIDE_PI);
  return Nxx;
}

OC_REALWIDE
Oxs_SelfDemagNy(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize)
{ return Oxs_SelfDemagNx(ysize,zsize,xsize); }

OC_REALWIDE
Oxs_SelfDemagNz(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize)
{ return Oxs_SelfDemagNx(zsize,xsize,ysize); }


OC_REALWIDE
Oxs_Newell_f(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // There is mucking around here to handle case where imports
  // are near zero.  In particular, asinh(t) is written as
  // log(t+sqrt(1+t)) because the latter appears easier to
  // handle if t=y/x (for example) as x -> 0.

 // This function is even; the fabs()'s just simplify special case handling.
  x=Oc_FabsRW(x); OC_REALWIDE xsq=x*x;
  y=Oc_FabsRW(y); OC_REALWIDE ysq=y*y;
  z=Oc_FabsRW(z); OC_REALWIDE zsq=z*z; 

  OC_REALWIDE Rsq=xsq+ysq+zsq;
  if(Rsq<=0.0) return 0.0;
  OC_REALWIDE R=Oc_SqrtRW(Rsq);

  // f(x,y,z)
  OC_REALWIDE piece[8];
  int piececount=0;
  if(z>0.) { // For 2D grids, half the calls from F1 have z==0.
    OC_REALWIDE temp1,temp2,temp3;
    piece[piececount++] = 2*(2*xsq-ysq-zsq)*R;
    if((temp1=x*y*z)>0.)
      piece[piececount++] = -12*temp1*Oc_Atan2RW(y*z,x*R);
    if(y>0. && (temp2=xsq+zsq)>0.) {
#if USE_LOG1P
      OC_REALWIDE dummy = Oc_Log1pRW(2*y*(y+R)/temp2);
#else
      OC_REALWIDE dummy = Oc_LogRW(((y+R)*(y+R))/temp2);
#endif
      piece[piececount++] = 3*y*(zsq-xsq)*dummy;
    }
    if((temp3=xsq+ysq)>0.) {
#if USE_LOG1P
      OC_REALWIDE dummy = Oc_Log1pRW(2*z*(z+R)/temp3);
#else
      OC_REALWIDE dummy = Oc_LogRW(((z+R)*(z+R))/temp3);
#endif
      piece[piececount++] = 3*z*(ysq-xsq)*dummy;
    }
  } else {
    // z==0
    if(x==y) {
      const OC_REALWIDE K = 2*Oc_SqrtRW(static_cast<OC_REALWIDE>(2.0))
        -6*Oc_LogRW(1+Oc_SqrtRW(static_cast<OC_REALWIDE>(2.0)));
      /// K = -2.4598143973710680537922785014593576970294L
      piece[piececount++] = K*xsq*x;
    } else {
      piece[piececount++] = 2*(2*xsq-ysq)*R;
      if(y>0. && x>0.) {
#if USE_LOG1P
	piece[piececount++] = -3*y*xsq*Oc_Log1pRW(2*y*(y+R)/(x*x));
#else
	piece[piececount++] = -6*y*xsq*Oc_LogRW((y+R)/x);
#endif
      }
    }
  }

  return Oxs_AccurateSum(piececount,piece)/12.;
}

#if 1 // For debugging
OC_INDEX SDA00_count = 0;
OC_INDEX SDA01_count = 0;
#endif

OC_REALWIDE
Oxs_CalculateSDA00(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
                   OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ // This is Nxx*(4*PI*tau) in Newell's paper,
  // where tau = dx*dy*dz.
#if 1
  ++SDA00_count;
#endif
  OC_REALWIDE result=0.;
  if(x==0. && y==0. && z==0.) {
    // Self demag term.  The base routine can handle x==y==z==0,
    // but this should be more accurate.
    result = Oxs_SelfDemagNx(dx,dy,dz)*(4*WIDE_PI*dx*dy*dz);
  } else {
    // Simplified (collapsed) formula based on Newell's paper.
    // This saves about half the calls to f().  There is still
    // quite a bit of redundancy from one cell site to the next,
    // but as this is an initialization-only issue speed shouldn't
    // be too critical.
    OC_REALWIDE arr[27];
    arr[ 0] = -1*Oxs_Newell_f(x+dx,y+dy,z+dz);
    arr[ 1] = -1*Oxs_Newell_f(x+dx,y-dy,z+dz);
    arr[ 2] = -1*Oxs_Newell_f(x+dx,y-dy,z-dz);
    arr[ 3] = -1*Oxs_Newell_f(x+dx,y+dy,z-dz);
    arr[ 4] = -1*Oxs_Newell_f(x-dx,y+dy,z-dz);
    arr[ 5] = -1*Oxs_Newell_f(x-dx,y+dy,z+dz);
    arr[ 6] = -1*Oxs_Newell_f(x-dx,y-dy,z+dz);
    arr[ 7] = -1*Oxs_Newell_f(x-dx,y-dy,z-dz);

    arr[ 8] =  2*Oxs_Newell_f(x,y-dy,z-dz);
    arr[ 9] =  2*Oxs_Newell_f(x,y-dy,z+dz);
    arr[10] =  2*Oxs_Newell_f(x,y+dy,z+dz);
    arr[11] =  2*Oxs_Newell_f(x,y+dy,z-dz);
    arr[12] =  2*Oxs_Newell_f(x+dx,y+dy,z);
    arr[13] =  2*Oxs_Newell_f(x+dx,y,z+dz);
    arr[14] =  2*Oxs_Newell_f(x+dx,y,z-dz);
    arr[15] =  2*Oxs_Newell_f(x+dx,y-dy,z);
    arr[16] =  2*Oxs_Newell_f(x-dx,y-dy,z);
    arr[17] =  2*Oxs_Newell_f(x-dx,y,z+dz);
    arr[18] =  2*Oxs_Newell_f(x-dx,y,z-dz);
    arr[19] =  2*Oxs_Newell_f(x-dx,y+dy,z);

    arr[20] = -4*Oxs_Newell_f(x,y-dy,z);
    arr[21] = -4*Oxs_Newell_f(x,y+dy,z);
    arr[22] = -4*Oxs_Newell_f(x,y,z-dz);
    arr[23] = -4*Oxs_Newell_f(x,y,z+dz);
    arr[24] = -4*Oxs_Newell_f(x+dx,y,z);
    arr[25] = -4*Oxs_Newell_f(x-dx,y,z);

    arr[26] =  8*Oxs_Newell_f(x,y,z);

    result=Oxs_AccurateSum(27,arr);
  }
  return result;
  /// Multiply result by 1./(4*PI*dx*dy*dz) to get effective "demag"
  /// factor Nxx from Newell's paper.
}


OC_REALWIDE
Oxs_Newell_g(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // There is mucking around here to handle case where imports
  // are near zero.  In particular, asinh(t) is written as
  // log(t+sqrt(1+t)) because the latter appears easier to
  // handle if t=y/x (for example) as x -> 0.

  // This function is even in z and odd in x and y.  The fabs()'s
  // simplify special case handling.
  OC_REALWIDE result_sign=1.0;
  if(x<0.0) result_sign *= -1.0;  if(y<0.0) result_sign *= -1.0;
  x=Oc_FabsRW(x); OC_REALWIDE xsq=x*x;
  y=Oc_FabsRW(y); OC_REALWIDE ysq=y*y;
  z=Oc_FabsRW(z); OC_REALWIDE zsq=z*z;

  OC_REALWIDE Rsq=xsq+ysq+zsq;
  if(Rsq<=0.0) return 0.0;
  OC_REALWIDE R=Oc_SqrtRW(Rsq);

  // g(x,y,z)
  OC_REALWIDE piece[7];
  int piececount=0;
  piece[piececount++] = -2*x*y*R;;
  if(z>0.) {
    // For 2D grids, 1/3 of the calls from Oxs_CalculateSDA01 have z==0.
    piece[piececount++] = -z*zsq*Oc_Atan2RW(x*y,z*R);
    piece[piececount++] = -3*z*ysq*Oc_Atan2RW(x*z,y*R);
    piece[piececount++] = -3*z*xsq*Oc_Atan2RW(y*z,x*R);

    OC_REALWIDE temp1,temp2,temp3;
    if((temp1=xsq+ysq)>0.) {
#if USE_LOG1P
      piece[piececount++] = 3*x*y*z*Oc_Log1pRW(2*z*(z+R)/temp1);
#else
      piece[piececount++] = 3*x*y*z*Oc_LogRW((z+R)*(z+R)/temp1);
#endif
    }

    if((temp2=ysq+zsq)>0.){
#if USE_LOG1P
      piece[piececount++] = 0.5*y*(3*zsq-ysq)*Oc_Log1pRW(2*x*(x+R)/temp2);
#else
      piece[piececount++] = 0.5*y*(3*zsq-ysq)*Oc_LogRW((x+R)*(x+R)/temp2);
#endif
    }

    if((temp3=xsq+zsq)>0.) {
#if USE_LOG1P
      piece[piececount++] = 0.5*x*(3*zsq-xsq)*Oc_Log1pRW(2*y*(y+R)/temp3);
#else
      piece[piececount++] = 0.5*x*(3*zsq-xsq)*Oc_LogRW((y+R)*(y+R)/temp3);
#endif
    }

  } else {
    // z==0.
#if USE_LOG1P
    if(y>0.) piece[piececount++] = -0.5*y*ysq*Oc_Log1pRW(2*x*(x+R)/(y*y));
    if(x>0.) piece[piececount++] = -0.5*x*xsq*Oc_Log1pRW(2*y*(y+R)/(x*x));
#else
    if(y>0.) piece[piececount++] = -y*ysq*Oc_LogRW((x+R)/y);
    if(x>0.) piece[piececount++] = -x*xsq*Oc_LogRW((y+R)/x);
#endif
  }

  return result_sign*Oxs_AccurateSum(piececount,piece)/6.;
}

OC_REALWIDE
Oxs_CalculateSDA01(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
                   OC_REALWIDE l,OC_REALWIDE h,OC_REALWIDE e)
{ // This is Nxy*(4*PI*tau) in Newell's paper.
#if 1
  ++SDA01_count;
#endif

  // A01 (aka Nxy) is odd in x, odd in y, and even in z
  // This implies that the return value is zero if (x,y,z)
  // lies in the yz-plane (i.e., x=0) or xz-planes (y=0).
  if(x==0.0 || y==0.0) return OC_REALWIDE(0.0);

  // Simplified (collapsed) formula based on Newell's paper.
  // This saves about half the calls to g().  There is still
  // quite a bit of redundancy from one cell site to the next,
  // but as this is an initialization-only issue speed shouldn't
  // be too critical.
  OC_REALWIDE arr[27];

  arr[ 0] = -1*Oxs_Newell_g(x-l,y-h,z-e);
  arr[ 1] = -1*Oxs_Newell_g(x-l,y-h,z+e);
  arr[ 2] = -1*Oxs_Newell_g(x+l,y-h,z+e);
  arr[ 3] = -1*Oxs_Newell_g(x+l,y-h,z-e);
  arr[ 4] = -1*Oxs_Newell_g(x+l,y+h,z-e);
  arr[ 5] = -1*Oxs_Newell_g(x+l,y+h,z+e);
  arr[ 6] = -1*Oxs_Newell_g(x-l,y+h,z+e);
  arr[ 7] = -1*Oxs_Newell_g(x-l,y+h,z-e);

  arr[ 8] =  2*Oxs_Newell_g(x,y+h,z-e);
  arr[ 9] =  2*Oxs_Newell_g(x,y+h,z+e);
  arr[10] =  2*Oxs_Newell_g(x,y-h,z+e);
  arr[11] =  2*Oxs_Newell_g(x,y-h,z-e);
  arr[12] =  2*Oxs_Newell_g(x-l,y-h,z);
  arr[13] =  2*Oxs_Newell_g(x-l,y+h,z);
  arr[14] =  2*Oxs_Newell_g(x-l,y,z-e);
  arr[15] =  2*Oxs_Newell_g(x-l,y,z+e);
  arr[16] =  2*Oxs_Newell_g(x+l,y,z+e);
  arr[17] =  2*Oxs_Newell_g(x+l,y,z-e);
  arr[18] =  2*Oxs_Newell_g(x+l,y-h,z);
  arr[19] =  2*Oxs_Newell_g(x+l,y+h,z);

  arr[20] = -4*Oxs_Newell_g(x-l,y,z);
  arr[21] = -4*Oxs_Newell_g(x+l,y,z);
  arr[22] = -4*Oxs_Newell_g(x,y,z+e);
  arr[23] = -4*Oxs_Newell_g(x,y,z-e);
  arr[24] = -4*Oxs_Newell_g(x,y-h,z);
  arr[25] = -4*Oxs_Newell_g(x,y+h,z);

  arr[26] =  8*Oxs_Newell_g(x,y,z);

  return Oxs_AccurateSum(27,arr);
  /// Multiply result by 1./(4*PI*l*h*e) to get effective "demag"
  /// factor Nxy from Newell's paper.
}



////////////////////////////////////////////////////////////////////////
// Asymptotic approximations to N
// See NOTES IV, 27-Mar-2007, p116-118
OxsDemagAsymptoticRefineData::OxsDemagAsymptoticRefineData
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 OXS_DEMAG_REAL_ASYMP maxratio)
  : rdx(0), rdy(0), rdz(0), result_scale(0),
    xcount(0), ycount(0), zcount(0)
{
  if(dz<=dx && dz<=dy) {
    // dz is minimal
    OXS_DEMAG_REAL_ASYMP xratio = ceil(dx/(maxratio*dz));
    xcount = static_cast<OC_INT4m>(xratio);
    rdx = dx/xratio;
    OXS_DEMAG_REAL_ASYMP yratio = ceil(dy/(maxratio*dz));
    ycount = static_cast<OC_INT4m>(yratio);
    rdy = dy/yratio;
    zcount = 1;
    rdz = dz;
  } else if(dy<=dx && dy<=dz) {
    // dy is minimal
    OXS_DEMAG_REAL_ASYMP xratio = ceil(dx/(maxratio*dy));
    xcount = static_cast<OC_INT4m>(xratio);
    rdx = dx/xratio;
    OXS_DEMAG_REAL_ASYMP zratio = ceil(dz/(maxratio*dy));
    zcount = static_cast<OC_INT4m>(zratio);
    rdz = dz/zratio;
    ycount = 1;
    rdy = dy;
  } else {
    // dx is minimal
    OXS_DEMAG_REAL_ASYMP yratio = ceil(dy/(maxratio*dx));
    ycount = static_cast<OC_INT4m>(yratio);
    rdy = dy/yratio;
    OXS_DEMAG_REAL_ASYMP zratio = ceil(dz/(maxratio*dx));
    zcount = static_cast<OC_INT4m>(zratio);
    rdz = dz/zratio;
    xcount = 1;
    rdx = dx;
  }
#if 0
  fprintf(stderr,"In  sizes: %10.6g %10.6g %10.6g\n",dx,dy,dz);
  fprintf(stderr,"Out sizes: %10.6g %10.6g %10.6g\n",rdx,rdy,rdz);
#endif
  result_scale = 1.0/(OXS_DEMAG_REAL_ASYMP(xcount)
                      *OXS_DEMAG_REAL_ASYMP(ycount)
                      *OXS_DEMAG_REAL_ASYMP(zcount));
}

Oxs_DemagNxxAsymptoticBase::Oxs_DemagNxxAsymptoticBase
(const OxsDemagAsymptoticRefineData& refine_data)
{
  OXS_DEMAG_REAL_ASYMP dx = refine_data.rdx;
  OXS_DEMAG_REAL_ASYMP dy = refine_data.rdy;
  OXS_DEMAG_REAL_ASYMP dz = refine_data.rdz;

  self_demag = Oxs_SelfDemagNx(dx,dy,dz);  // Special case

  OXS_DEMAG_REAL_ASYMP dx2 = dx*dx;
  OXS_DEMAG_REAL_ASYMP dy2 = dy*dy;
  OXS_DEMAG_REAL_ASYMP dz2 = dz*dz;

  OXS_DEMAG_REAL_ASYMP dx4 = dx2*dx2;
  OXS_DEMAG_REAL_ASYMP dy4 = dy2*dy2;
  OXS_DEMAG_REAL_ASYMP dz4 = dz2*dz2;

  OXS_DEMAG_REAL_ASYMP dx6 = dx4*dx2;
  OXS_DEMAG_REAL_ASYMP dy6 = dy4*dy2;
  OXS_DEMAG_REAL_ASYMP dz6 = dz4*dz2;

  lead_weight = (-dx*dy*dz/(4*WIDE_PI));

  // Initialize coefficients for 1/R^5 term
  if(dx2!=dy2 || dx2!=dz2 || dy2!=dz2) { // Non-cube case
    cubic_cell = 0;
    a1 = a2 = a3 = a4 = a5 = a6 = lead_weight/4.0;
    a1 *=   8*dx2  -  4*dy2  -  4*dz2;
    a2 *= -24*dx2  + 27*dy2  -  3*dz2;
    a3 *= -24*dx2  -  3*dy2  + 27*dz2;
    a4 *=   3*dx2  -  4*dy2  +  1*dz2;
    a5 *=   6*dx2  -  3*dy2  -  3*dz2;
    a6 *=   3*dx2  +  1*dy2  -  4*dz2;
  } else { // Cube
    cubic_cell = 1;
    a1 = a2 = a3 = a4 = a5 = a6 = 0.0;
  }

  // Initialize coefficients for 1/R^7 term
  b1 = b2 = b3 = b4 = b5 = b6 = b7 = b8 = b9 = b10 = lead_weight/16.0;
  if(cubic_cell) {
    b1  *=  -14*dx4;
    b2  *=  105*dx4;
    b3  *=  105*dx4;
    b4  *= -105*dx4;
    b6  *= -105*dx4;
    b7  *=    7*dx4;
    b10 *=    7*dx4;
    b5  = b8 = b9 = 0;
  } else {
    b1  *=   32*dx4 -  40*dx2*dy2 -  40*dx2*dz2 +  12*dy4 +  10*dy2*dz2 +  12*dz4;
    b2  *= -240*dx4 + 580*dx2*dy2 +  20*dx2*dz2 - 202*dy4 -  75*dy2*dz2 +  22*dz4;
    b3  *= -240*dx4 +  20*dx2*dy2 + 580*dx2*dz2 +  22*dy4 -  75*dy2*dz2 - 202*dz4;
    b4  *=  180*dx4 - 505*dx2*dy2 +  55*dx2*dz2 + 232*dy4 -  75*dy2*dz2 +   8*dz4;
    b5  *=  360*dx4 - 450*dx2*dy2 - 450*dx2*dz2 - 180*dy4 + 900*dy2*dz2 - 180*dz4;
    b6  *=  180*dx4 +  55*dx2*dy2 - 505*dx2*dz2 +   8*dy4 -  75*dy2*dz2 + 232*dz4;
    b7  *=  -10*dx4 +  30*dx2*dy2 -   5*dx2*dz2 -  16*dy4 +  10*dy2*dz2 -   2*dz4;
    b8  *=  -30*dx4 +  55*dx2*dy2 +  20*dx2*dz2 +   8*dy4 -  75*dy2*dz2 +  22*dz4;
    b9  *=  -30*dx4 +  20*dx2*dy2 +  55*dx2*dz2 +  22*dy4 -  75*dy2*dz2 +   8*dz4;
    b10 *=  -10*dx4 -   5*dx2*dy2 +  30*dx2*dz2 -   2*dy4 +  10*dy2*dz2 -  16*dz4;
  }

  // Initialize coefficients for 1/R^9 term
  c1 = c2 = c3 = c4 = c5 = c6 = c7 = c8 = c9
    = c10 = c11 = c12 = c13 = c14 = c15 = lead_weight/192.0;
  if(cubic_cell) {
    c1  *=    32 * dx6;
    c2  *=  -448 * dx6;
    c3  *=  -448 * dx6;
    c4  *=  -150 * dx6;
    c5  *=  7620 * dx6;
    c6  *=  -150 * dx6;
    c7  *=   314 * dx6;
    c8  *= -3810 * dx6;
    c9  *= -3810 * dx6;
    c10 *=   314 * dx6;
    c11 *=   -16 * dx6;
    c12 *=   134 * dx6;
    c13 *=   300 * dx6;
    c14 *=   134 * dx6;
    c15 *=   -16 * dx6;
  } else {
    c1  *=    384 *dx6 +   -896 *dx4*dy2 +   -896 *dx4*dz2 +    672 *dx2*dy4 +    560 *dx2*dy2*dz2 +    672 *dx2*dz4 +   -120 *dy6 +   -112 *dy4*dz2 +   -112 *dy2*dz4 +   -120 *dz6;
    c2  *=  -5376 *dx6 +  22624 *dx4*dy2 +   2464 *dx4*dz2 + -19488 *dx2*dy4 +  -7840 *dx2*dy2*dz2 +    672 *dx2*dz4 +   3705 *dy6 +   2198 *dy4*dz2 +    938 *dy2*dz4 +   -345 *dz6;
    c3  *=  -5376 *dx6 +   2464 *dx4*dy2 +  22624 *dx4*dz2 +    672 *dx2*dy4 +  -7840 *dx2*dy2*dz2 + -19488 *dx2*dz4 +   -345 *dy6 +    938 *dy4*dz2 +   2198 *dy2*dz4 +   3705 *dz6;
    c4  *=  10080 *dx6 + -48720 *dx4*dy2 +   1680 *dx4*dz2 +  49770 *dx2*dy4 +  -2625 *dx2*dy2*dz2 +   -630 *dx2*dz4 + -10440 *dy6 +  -1050 *dy4*dz2 +   2100 *dy2*dz4 +   -315 *dz6;
    c5  *=  20160 *dx6 + -47040 *dx4*dy2 + -47040 *dx4*dz2 +  -6300 *dx2*dy4 + 133350 *dx2*dy2*dz2 +  -6300 *dx2*dz4 +   7065 *dy6 + -26670 *dy4*dz2 + -26670 *dy2*dz4 +   7065 *dz6;
    c6  *=  10080 *dx6 +   1680 *dx4*dy2 + -48720 *dx4*dz2 +   -630 *dx2*dy4 +  -2625 *dx2*dy2*dz2 +  49770 *dx2*dz4 +   -315 *dy6 +   2100 *dy4*dz2 +  -1050 *dy2*dz4 + -10440 *dz6;
    c7  *=  -3360 *dx6 +  17290 *dx4*dy2 +  -1610 *dx4*dz2 + -19488 *dx2*dy4 +   5495 *dx2*dy2*dz2 +   -588 *dx2*dz4 +   4848 *dy6 +  -3136 *dy4*dz2 +    938 *dy2*dz4 +    -75 *dz6;
    c8  *= -10080 *dx6 +  32970 *dx4*dy2 +  14070 *dx4*dz2 +  -6300 *dx2*dy4 + -66675 *dx2*dy2*dz2 +  12600 *dx2*dz4 + -10080 *dy6 +  53340 *dy4*dz2 + -26670 *dy2*dz4 +   3015 *dz6;
    c9  *= -10080 *dx6 +  14070 *dx4*dy2 +  32970 *dx4*dz2 +  12600 *dx2*dy4 + -66675 *dx2*dy2*dz2 +  -6300 *dx2*dz4 +   3015 *dy6 + -26670 *dy4*dz2 +  53340 *dy2*dz4 + -10080 *dz6;
    c10 *=  -3360 *dx6 +  -1610 *dx4*dy2 +  17290 *dx4*dz2 +   -588 *dx2*dy4 +   5495 *dx2*dy2*dz2 + -19488 *dx2*dz4 +    -75 *dy6 +    938 *dy4*dz2 +  -3136 *dy2*dz4 +   4848 *dz6;
    c11 *=    105 *dx6 +   -560 *dx4*dy2 +     70 *dx4*dz2 +    672 *dx2*dy4 +   -280 *dx2*dy2*dz2 +     42 *dx2*dz4 +   -192 *dy6 +    224 *dy4*dz2 +   -112 *dy2*dz4 +     15 *dz6;
    c12 *=    420 *dx6 +  -1610 *dx4*dy2 +   -350 *dx4*dz2 +    672 *dx2*dy4 +   2345 *dx2*dy2*dz2 +   -588 *dx2*dz4 +    528 *dy6 +  -3136 *dy4*dz2 +   2198 *dy2*dz4 +   -345 *dz6;
    c13 *=    630 *dx6 +  -1470 *dx4*dy2 +  -1470 *dx4*dz2 +   -630 *dx2*dy4 +   5250 *dx2*dy2*dz2 +   -630 *dx2*dz4 +    360 *dy6 +  -1050 *dy4*dz2 +  -1050 *dy2*dz4 +    360 *dz6;
    c14 *=    420 *dx6 +   -350 *dx4*dy2 +  -1610 *dx4*dz2 +   -588 *dx2*dy4 +   2345 *dx2*dy2*dz2 +    672 *dx2*dz4 +   -345 *dy6 +   2198 *dy4*dz2 +  -3136 *dy2*dz4 +    528 *dz6;
    c15 *=    105 *dx6 +     70 *dx4*dy2 +   -560 *dx4*dz2 +     42 *dx2*dy4 +   -280 *dx2*dy2*dz2 +    672 *dx2*dz4 +     15 *dy6 +   -112 *dy4*dz2 +    224 *dy2*dz4 +   -192 *dz6;
  }
}

OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxxAsymptoticBase::NxxAsymptotic
(const OxsDemagNabData& ptdata) const
{ // Asymptotic approximation to Nxx term.

  if(ptdata.iR2<=0.0) {
    // Asymptotic expansion doesn't apply for R==0.  Fall back
    // to self-demag calculation.
    return self_demag;
  }

  const OXS_DEMAG_REAL_ASYMP& tx2 = ptdata.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2 = ptdata.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2 = ptdata.tz2;

  const OXS_DEMAG_REAL_ASYMP tz4 = tz2*tz2;
  const OXS_DEMAG_REAL_ASYMP tz6 = tz4*tz2;
  const OXS_DEMAG_REAL_ASYMP term3 = (2*tx2 - ty2 - tz2)*lead_weight;
  OXS_DEMAG_REAL_ASYMP term5 = 0.0;
  OXS_DEMAG_REAL_ASYMP term7 = 0.0;
  if(cubic_cell) {
    const OXS_DEMAG_REAL_ASYMP ty4 = ty2*ty2; 
    term7 = ((b1*tx2
              + (b2*ty2 + b3*tz2))*tx2
             + (b4*ty4 + b6*tz4))*tx2
      + b7*ty4*ty2 + b10*tz6;
  } else {
    term5 = (a1*tx2 + (a2*ty2 + a3*tz2))*tx2
      + (a4*ty2 + a5*tz2)*ty2 + a6*tz4;
    term7 = ((b1*tx2
              + (b2*ty2 + b3*tz2))*tx2
             + ((b4*ty2 + b5*tz2)*ty2 + b6*tz4))*tx2
      + ((b7*ty2 + b8*tz2)*ty2 + b9*tz4)*ty2
      + b10*tz6;
  }
  const OXS_DEMAG_REAL_ASYMP term9
    =  (((c1*tx2
          +  (c2*ty2 +  c3*tz2))*tx2
         +  ((c4*ty2 +  c5*tz2)*ty2 +  c6*tz4))*tx2
        +  (  ((c7*ty2 +  c8*tz2)*ty2 +  c9*tz4)*ty2 + c10*tz6  ))*tx2
    +  (((c11*ty2 + c12*tz2)*ty2 + c13*tz4)*ty2 + c14*tz6)*ty2
    + c15*tz4*tz4;

  const OXS_DEMAG_REAL_ASYMP Nxx = (term9 + term7 + term5 + term3)*ptdata.iR;
  // Error should be of order 1/R^11

  return Nxx;
}


#if OXS_DEMAG_ASYMP_USE_SSE
OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxxAsymptoticBase::NxxAsymptoticPair
(const OxsDemagNabData& ptA,
 const OxsDemagNabData& ptB) const
{ // Asymptotic approximation to Nxx term.  This routine takes a pair of
  // points and uses SSE intrinsics to accelerate the computation.

  if(ptA.iR2<=0 || ptB.iR2<=0) {
    // Asymptotic expansion doesn't apply for R==0, so for at least
    // one of these points we should call self-demag calculation.
    // This means we can't use the parallel computation below, so
    // pass both points through to the single-point NxxAsymptotic
    // routine, and let it sort them out.
    return NxxAsymptotic(ptA) + NxxAsymptotic(ptB);
  }

  const Oc_Duet tx2(ptA.tx2,ptB.tx2);
  const Oc_Duet ty2(ptA.ty2,ptB.ty2);
  const Oc_Duet tz2(ptA.tz2,ptB.tz2);

  const Oc_Duet tz4 = tz2*tz2;
  const Oc_Duet tz6 = tz4*tz2;

  const Oc_Duet term3 = lead_weight*(2*tx2 - ty2 - tz2);

  Oc_Duet Nxx(0.0);
  Oc_Duet term7;
  if(cubic_cell) {
    const Oc_Duet ty4 = ty2*ty2; 
    term7 = b7*ty4*ty2
      + tx2*(tx2*(b1*tx2 + b2*ty2 + b3*tz2) + b4*ty4 + b6*tz4);
  } else {
    const Oc_Duet term5
      = a6*tz4
      + ty2*(a4*ty2 + a5*tz2)
      + tx2*(a1*tx2 + a2*ty2 + a3*tz2);
    Nxx = term5;

    term7 = ty2*(ty2*(b7*ty2 + b8*tz2) + b9*tz4)
      + tx2*(tx2*(b1*tx2 + b2*ty2 + b3*tz2) + ty2*(b4*ty2 + b5*tz2) + b6*tz4);
  }
  term7 += b10*tz6;
  const Oc_Duet term9
    = c15*tz4*tz4
    + ty2*(ty2*(ty2*(c11*ty2 + c12*tz2) + c13*tz4) + c14*tz6)
    + tx2*(tx2*(tx2*(c1*tx2 +  c2*ty2 +  c3*tz2)
                +  ty2*(c4*ty2 +  c5*tz2) +  c6*tz4)
           + ty2*(ty2*(c7*ty2 +  c8*tz2) +  c9*tz4) + c10*tz6);

  Nxx += (term7 + term9);
  Nxx += term3;
  Nxx *= Oc_Duet(ptA.iR,ptB.iR);
  // Error should be of order 1/R^11

  return Nxx.GetA() + Nxx.GetB();
}
#endif // OXS_DEMAG_ASYMP_USE_SSE 

OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxxAsymptotic::NxxAsymptotic(const OxsDemagNabData& ptdata) const
{ // Presumably at least one of xcount, ycount, or zcount is 1, but this
  // fact is not used in following code.

  // Alias data from refine_data structure.
  const OC_INT4m& xcount = refine_data.xcount;
  const OC_INT4m& ycount = refine_data.ycount;
  const OC_INT4m& zcount = refine_data.zcount;
  const OXS_DEMAG_REAL_ASYMP& rdx = refine_data.rdx;
  const OXS_DEMAG_REAL_ASYMP& rdy = refine_data.rdy;
  const OXS_DEMAG_REAL_ASYMP& rdz = refine_data.rdz;
  const OXS_DEMAG_REAL_ASYMP& result_scale = refine_data.result_scale;

  OxsDemagNabData rptdata,mrptdata;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INT4m k=1-zcount;k<zcount;++k) {
    OXS_DEMAG_REAL_ASYMP zoff = ptdata.z+k*rdz;
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INT4m j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.y+j*rdy;
      rptdata.Set(ptdata.x,yoff,zoff);
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Nxx.NxxAsymptotic(rptdata);
      for(OC_INT4m i=1;i<xcount;++i) {
        rptdata.Set(ptdata.x+i*rdx,yoff,zoff);
        mrptdata.Set(ptdata.x-i*rdx,yoff,zoff);
        xsum += (xcount-i) * Nxx.NxxAsymptoticPair(rptdata,mrptdata);
      }
      // Weight x-strip interactions into xy-plate
      ysum += (ycount - abs(j))*xsum;
    }
    // Weight xy-plate interactions into total sum
    zsum += (zcount - abs(k))*ysum;
  }
  return zsum*result_scale;
}


Oxs_DemagNxyAsymptoticBase::Oxs_DemagNxyAsymptoticBase
(const OxsDemagAsymptoticRefineData& refine_data) {
  OXS_DEMAG_REAL_ASYMP dx = refine_data.rdx;
  OXS_DEMAG_REAL_ASYMP dy = refine_data.rdy;
  OXS_DEMAG_REAL_ASYMP dz = refine_data.rdz;

  OXS_DEMAG_REAL_ASYMP dx2 = dx*dx;
  OXS_DEMAG_REAL_ASYMP dy2 = dy*dy;
  OXS_DEMAG_REAL_ASYMP dz2 = dz*dz;

  OXS_DEMAG_REAL_ASYMP dx4 = dx2*dx2;
  OXS_DEMAG_REAL_ASYMP dy4 = dy2*dy2;
  OXS_DEMAG_REAL_ASYMP dz4 = dz2*dz2;

  OXS_DEMAG_REAL_ASYMP dx6 = dx4*dx2;
  OXS_DEMAG_REAL_ASYMP dy6 = dy4*dy2;
  OXS_DEMAG_REAL_ASYMP dz6 = dz4*dz2;

  lead_weight = (-dx*dy*dz/(4*WIDE_PI));

  // Initialize coefficients for 1/R^5 term
  if(dx2!=dy2 || dx2!=dz2 || dy2!=dz2) { // Non-cube case
    cubic_cell = 0;
    a1 = a2 = a3 = (lead_weight*5.0)/4.0;
    a1 *=  4*dx2  -  3*dy2  -  1*dz2;
    a2 *= -3*dx2  +  4*dy2  -  1*dz2;
    a3 *= -3*dx2  -  3*dy2  +  6*dz2;
  } else { // Cube
    cubic_cell = 1;
    a1 = a2 = a3 = 0.0;
  }

  // Initialize coefficients for 1/R^7 term
  b1 = b2 = b3 = b4 = b5 = b6 = (lead_weight*7.0)/16.0;
  if(cubic_cell) {
    b1  *=  -7*dx4;
    b2  *=  19*dx4;
    b3  *=  13*dx4;
    b4  *=  -7*dx4;
    b5  *=  13*dx4;
    b6  *= -13*dx4;
  } else {
    b1 *=  16*dx4 -  30*dx2*dy2 -  10*dx2*dz2 +  10*dy4 +   5*dy2*dz2 +  2*dz4;
    b2 *= -40*dx4 + 105*dx2*dy2 -   5*dx2*dz2 -  40*dy4 -   5*dy2*dz2 +  4*dz4;
    b3 *= -40*dx4 -  15*dx2*dy2 + 115*dx2*dz2 +  20*dy4 -  35*dy2*dz2 - 32*dz4;
    b4 *=  10*dx4 -  30*dx2*dy2 +   5*dx2*dz2 +  16*dy4 -  10*dy2*dz2 +  2*dz4;
    b5 *=  20*dx4 -  15*dx2*dy2 -  35*dx2*dz2 -  40*dy4 + 115*dy2*dz2 - 32*dz4;
    b6 *=  10*dx4 +  15*dx2*dy2 -  40*dx2*dz2 +  10*dy4 -  40*dy2*dz2 + 32*dz4;
  }

  // Initialize coefficients for 1/R^9 term
  c1 = c2 = c3 = c4 = c5 = c6 = c7 = c8 = c9 = c10 = lead_weight/64.0;
  if(cubic_cell) {
    c1  *=   48 *dx6;
    c2  *= -142 *dx6;
    c3  *= -582 *dx6;
    c4  *= -142 *dx6;
    c5  *= 2840 *dx6;
    c6  *= -450 *dx6;
    c7  *=   48 *dx6;
    c8  *= -582 *dx6;
    c9  *= -450 *dx6;
    c10 *=  180 *dx6;
  } else {
    c1  *=   576 *dx6 +  -2016 *dx4*dy2 +   -672 *dx4*dz2 +   1680 *dx2*dy4 +    840 *dx2*dy2*dz2 +   336 *dx2*dz4 +  -315 *dy6 +   -210 *dy4*dz2 +  -126 *dy2*dz4 +   -45 *dz6;
    c2  *= -3024 *dx6 +  13664 *dx4*dy2 +    448 *dx4*dz2 + -12670 *dx2*dy4 +  -2485 *dx2*dy2*dz2 +   546 *dx2*dz4 +  2520 *dy6 +    910 *dy4*dz2 +    84 *dy2*dz4 +  -135 *dz6;
    c3  *= -3024 *dx6 +   1344 *dx4*dy2 +  12768 *dx4*dz2 +   2730 *dx2*dy4 + -10185 *dx2*dy2*dz2 + -8694 *dx2*dz4 +  -945 *dy6 +   1680 *dy4*dz2 +  2394 *dy2*dz4 +  1350 *dz6;
    c4  *=  2520 *dx6 + -12670 *dx4*dy2 +    910 *dx4*dz2 +  13664 *dx2*dy4 +  -2485 *dx2*dy2*dz2 +    84 *dx2*dz4 + -3024 *dy6 +    448 *dy4*dz2 +   546 *dy2*dz4 +  -135 *dz6;
    c5  *=  5040 *dx6 +  -9940 *dx4*dy2 + -13580 *dx4*dz2 +  -9940 *dx2*dy4 +  49700 *dx2*dy2*dz2 + -6300 *dx2*dz4 +  5040 *dy6 + -13580 *dy4*dz2 + -6300 *dy2*dz4 +  2700 *dz6;
    c6  *=  2520 *dx6 +   2730 *dx4*dy2 + -14490 *dx4*dz2 +    420 *dx2*dy4 +  -7875 *dx2*dy2*dz2 + 17640 *dx2*dz4 +  -945 *dy6 +   3990 *dy4*dz2 +  -840 *dy2*dz4 + -3600 *dz6;
    c7  *=  -315 *dx6 +   1680 *dx4*dy2 +   -210 *dx4*dz2 +  -2016 *dx2*dy4 +    840 *dx2*dy2*dz2 +  -126 *dx2*dz4 +   576 *dy6 +   -672 *dy4*dz2 +   336 *dy2*dz4 +   -45 *dz6;
    c8  *=  -945 *dx6 +   2730 *dx4*dy2 +   1680 *dx4*dz2 +   1344 *dx2*dy4 + -10185 *dx2*dy2*dz2 +  2394 *dx2*dz4 + -3024 *dy6 +  12768 *dy4*dz2 + -8694 *dy2*dz4 +  1350 *dz6;
    c9  *=  -945 *dx6 +    420 *dx4*dy2 +   3990 *dx4*dz2 +   2730 *dx2*dy4 +  -7875 *dx2*dy2*dz2 +  -840 *dx2*dz4 +  2520 *dy6 + -14490 *dy4*dz2 + 17640 *dy2*dz4 + -3600 *dz6;
    c10 *=  -315 *dx6 +   -630 *dx4*dy2 +   2100 *dx4*dz2 +   -630 *dx2*dy4 +   3150 *dx2*dy2*dz2 + -3360 *dx2*dz4 +  -315 *dy6 +   2100 *dy4*dz2 + -3360 *dy2*dz4 +  1440 *dz6;
  }

#if 0
  fprintf(stderr,"h2: %Lg %Lg %Lg\n",dx2,dy2,dz2);
  fprintf(stderr,"Nxy vec: %9.2Le %9.2Le %9.2Le %9.2Le %9.2Le"
          " %9.2Le %9.2Le %9.2Le %9.2Le %9.2Le\n",
          c1*64/lead_weight,c2*64/lead_weight,c3*64/lead_weight,
          c4*64/lead_weight,c5*64/lead_weight,c6*64/lead_weight,
          c7*64/lead_weight,c8*64/lead_weight,c9*64/lead_weight,
          c10*64/lead_weight);
#endif

}

OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptoticBase::NxyAsymptotic
(const OxsDemagNabData& ptdata) const
{ // Asymptotic approximation to Nxy term.

  if(ptdata.R2<=0.0) {
    // Asymptotic expansion doesn't apply for R==0.  Fall back
    // to self-demag calculation.
    return static_cast<OXS_DEMAG_REAL_ASYMP>(0.0);
  }

  const OXS_DEMAG_REAL_ASYMP& tx2 = ptdata.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2 = ptdata.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2 = ptdata.tz2;

  const OXS_DEMAG_REAL_ASYMP term3 = 3*lead_weight;

  OXS_DEMAG_REAL_ASYMP term5 = 0.0;
  if(!cubic_cell) {
    term5 = a1*tx2 + a2*ty2 + a3*tz2;
  }

  const OXS_DEMAG_REAL_ASYMP tz4 = tz2*tz2;
  const OXS_DEMAG_REAL_ASYMP term7
    = (b1*tx2 + (b2*ty2 + b3*tz2))*tx2 + (b4*ty2 + b5*tz2)*ty2 + b6*tz4;

  const OXS_DEMAG_REAL_ASYMP term9
    = ((c1*tx2 + (c2*ty2 + c3*tz2))*tx2 + ((c4*ty2 + c5*tz2)*ty2 + c6*tz4))*tx2
    + ((c7*ty2 + c8*tz2)*ty2 + c9*tz4)*ty2
    + c10*tz4*tz2;

  const OXS_DEMAG_REAL_ASYMP& x = ptdata.x;
  const OXS_DEMAG_REAL_ASYMP& y = ptdata.y;
  const OXS_DEMAG_REAL_ASYMP& iR2 = ptdata.iR2;
  const OXS_DEMAG_REAL_ASYMP& iR  = ptdata.iR;
  const OXS_DEMAG_REAL_ASYMP iR5 = iR2*iR2*iR;
  const OXS_DEMAG_REAL_ASYMP Nxy
    = (term9 + term7 + term5 + term3)*iR5*x*y;
  // Error should be of order 1/R^11

  return Nxy;
}

#if !OXS_DEMAG_ASYMP_USE_SSE
OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptoticBase::NxyAsymptoticPairX
(const OxsDemagNabPairData& ptdata) const
{ // Evaluates asymptotic approximation to
  //    Nxy(x+xoff,y,z) + Nxy(x-xoff,y,z)
  // on the assumption that |xoff| >> |x|.

  assert(ptdata.ptp.y == ptdata.ptm.y &&
         ptdata.ptp.z == ptdata.ptm.z);

  const OXS_DEMAG_REAL_ASYMP& xp = ptdata.ptp.x;
  const OXS_DEMAG_REAL_ASYMP& y = ptdata.ptp.y;
  const OXS_DEMAG_REAL_ASYMP& z = ptdata.ptp.z;
  const OXS_DEMAG_REAL_ASYMP& xm = ptdata.ptm.x;

  const OXS_DEMAG_REAL_ASYMP& R2p  = ptdata.ptp.R2;
  const OXS_DEMAG_REAL_ASYMP& R2m  = ptdata.ptm.R2;

  // Both R2p and R2m must be positive, since asymptotics
  // don't apply for R==0.
  if(R2p<=0.0) return NxyAsymptotic(ptdata.ptm);
  if(R2m<=0.0) return NxyAsymptotic(ptdata.ptp);

  // Cancellation primarily in 1/R^3 term.
  const OXS_DEMAG_REAL_ASYMP& xbase = ptdata.ubase;
  OXS_DEMAG_REAL_ASYMP term3x = 3*lead_weight*xbase; // Main non-canceling part
  OXS_DEMAG_REAL_ASYMP term3cancel = 0; // Main canceling part
  {
    const OXS_DEMAG_REAL_ASYMP& xoff  = ptdata.uoff;
    OXS_DEMAG_REAL_ASYMP A = xbase*xbase + xoff*xoff + y*y + z*z;
    OXS_DEMAG_REAL_ASYMP B = 2*xbase*xoff;
    OXS_DEMAG_REAL_ASYMP R5p = R2p*R2p*ptdata.ptp.R;
    OXS_DEMAG_REAL_ASYMP R5m = R2m*R2m*ptdata.ptm.R;
    OXS_DEMAG_REAL_ASYMP A2 = A*A;
    OXS_DEMAG_REAL_ASYMP B2 = B*B;
    OXS_DEMAG_REAL_ASYMP Rdiff = -2*B*(B2*B2 + 5*A2*(A2+2*B2))
                        / (R5p*R5m*(R5p+R5m));
    term3cancel = 3*lead_weight*xoff*Rdiff;
  }

  // 1/R^5 terms; Note these are zero if cells are cubes
  const OXS_DEMAG_REAL_ASYMP& tx2p = ptdata.ptp.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2p = ptdata.ptp.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2p = ptdata.ptp.tz2;
  const OXS_DEMAG_REAL_ASYMP& tx2m = ptdata.ptm.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2m = ptdata.ptm.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2m = ptdata.ptm.tz2;
  OXS_DEMAG_REAL_ASYMP term5p = 0.0;
  OXS_DEMAG_REAL_ASYMP term5m = 0.0;
  if(!cubic_cell) {
    term5p = a1*tx2p + a2*ty2p + a3*tz2p;
    term5m = a1*tx2m + a2*ty2m + a3*tz2m;
  }

  // 1/R^7 terms
  OXS_DEMAG_REAL_ASYMP tz4p = tz2p*tz2p;
  OXS_DEMAG_REAL_ASYMP tz4m = tz2m*tz2m;
  OXS_DEMAG_REAL_ASYMP term7p
    = (b1*tx2p + (b2*ty2p + b3*tz2p))*tx2p
    + (b4*ty2p + b5*tz2p)*ty2p + b6*tz4p;
  OXS_DEMAG_REAL_ASYMP term7m
    = (b1*tx2m + (b2*ty2m + b3*tz2m))*tx2m
    + (b4*ty2m + b5*tz2m)*ty2m + b6*tz4m;

  // 1/R^9 terms
  OXS_DEMAG_REAL_ASYMP term9p
    = ((c1*tx2p + (c2*ty2p + c3*tz2p))*tx2p
       + ((c4*ty2p + c5*tz2p)*ty2p + c6*tz4p))*tx2p
    + ((c7*ty2p + c8*tz2p)*ty2p + c9*tz4p)*ty2p
    + c10*tz4p*tz2p;
  OXS_DEMAG_REAL_ASYMP term9m
    = ((c1*tx2m + (c2*ty2m + c3*tz2m))*tx2m
       + ((c4*ty2m + c5*tz2m)*ty2m + c6*tz4m))*tx2m
    + ((c7*ty2m + c8*tz2m)*ty2m + c9*tz4m)*ty2m
    + c10*tz4m*tz2m;

  // Totals
  const OXS_DEMAG_REAL_ASYMP& iRp = ptdata.ptp.iR;
  const OXS_DEMAG_REAL_ASYMP& iR2p = ptdata.ptp.iR2;
  const OXS_DEMAG_REAL_ASYMP  iR5p = iR2p*iR2p*iRp;

  const OXS_DEMAG_REAL_ASYMP& iRm = ptdata.ptm.iR;
  const OXS_DEMAG_REAL_ASYMP& iR2m = ptdata.ptm.iR2;
  const OXS_DEMAG_REAL_ASYMP  iR5m = iR2m*iR2m*iRm;

  OXS_DEMAG_REAL_ASYMP Nxy =  y*(term3cancel
     + (xp*(term9p + term7p + term5p)+term3x)*iR5p
     + (xm*(term9m + term7m + term5m)+term3x)*iR5m);
  // Error should be of order 1/R^11

  return Nxy;
}

#else // !OXS_DEMAG_ASYMP_USE_SSE

OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptoticBase::NxyAsymptoticPairX
(const OxsDemagNabPairData& ptdata) const
{ // Evaluates asymptotic approximation to
  //    Nxy(x+xoff,y,z) + Nxy(x-xoff,y,z)
  // on the assumption that |xoff| >> |x|.

  assert(ptdata.ptp.y == ptdata.ptm.y &&
         ptdata.ptp.z == ptdata.ptm.z);

  const OXS_DEMAG_REAL_ASYMP& xp = ptdata.ptp.x;
  const OXS_DEMAG_REAL_ASYMP& y = ptdata.ptp.y;
  const OXS_DEMAG_REAL_ASYMP& z = ptdata.ptp.z;
  const OXS_DEMAG_REAL_ASYMP& xm = ptdata.ptm.x;

  const OXS_DEMAG_REAL_ASYMP& R2p  = ptdata.ptp.R2;
  const OXS_DEMAG_REAL_ASYMP& R2m  = ptdata.ptm.R2;

  // Both R2p and R2m must be positive, since asymptotics
  // don't apply for R==0.
  if(R2p<=0.0) return NxyAsymptotic(xm,y,z);
  if(R2m<=0.0) return NxyAsymptotic(xp,y,z);

  // Cancellation primarily in 1/R^3 term.
  const OXS_DEMAG_REAL_ASYMP& xbase = ptdata.ubase;
  OXS_DEMAG_REAL_ASYMP term3x = 3*lead_weight*xbase; // Main non-canceling part
  OXS_DEMAG_REAL_ASYMP term3cancel = 0; // Main canceling part
  {
    const OXS_DEMAG_REAL_ASYMP& xoff  = ptdata.uoff;
    OXS_DEMAG_REAL_ASYMP A = xbase*xbase + xoff*xoff + y*y + z*z;
    OXS_DEMAG_REAL_ASYMP B = 2*xbase*xoff;
    OXS_DEMAG_REAL_ASYMP R5p = R2p*R2p*ptdata.ptp.R;
    OXS_DEMAG_REAL_ASYMP R5m = R2m*R2m*ptdata.ptm.R;
    OXS_DEMAG_REAL_ASYMP A2 = A*A;
    OXS_DEMAG_REAL_ASYMP B2 = B*B;
    OXS_DEMAG_REAL_ASYMP Rdiff = -2*B*(B2*B2 + 5*A2*(A2+2*B2))
                        / (R5p*R5m*(R5p+R5m));
    term3cancel = 3*lead_weight*xoff*Rdiff;
  }

  // Pack SSE structures, ptp in lower half, ptm in upper half
  Oc_Duet tx2(ptdata.ptp.tx2,ptdata.ptm.tx2);
  Oc_Duet ty2(ptdata.ptp.ty2,ptdata.ptm.ty2);
  Oc_Duet tz2(ptdata.ptp.tz2,ptdata.ptm.tz2);

  // 1/R^5 terms; Note these are zero if cells are cubes
  Oc_Duet term5; term5.SetZero();
  if(!cubic_cell) {
    term5 = a1*tx2 + a2*ty2 + a3*tz2;
  }

  // 1/R^7 terms
  Oc_Duet tz4 = tz2*tz2;
  Oc_Duet term7
    = b6*tz4
    + ty2*(b4*ty2 + b5*tz2)
    + tx2*(b1*tx2 + b2*ty2 + b3*tz2);

  // 1/R^9 terms
  Oc_Duet term9
    = c10*tz4*tz2
    + ty2*(ty2*(c7*ty2 + c8*tz2) + c9*tz4)
    + tx2*(tx2*(c1*tx2 + c2*ty2 + c3*tz2)
           + ty2*(c4*ty2 + c5*tz2) + c6*tz4);

  // Totals
  Oc_Duet iR(ptdata.ptp.iR, ptdata.ptm.iR);
  Oc_Duet iR5(ptdata.ptp.iR2, ptdata.ptm.iR2); iR5 *= iR5; iR5 *= iR;
  Oc_Duet x(xp,xm);

  Oc_Duet Nxyp1 =  (x*(term9 + term7 + term5)+term3x)*iR5;
  OXS_DEMAG_REAL_ASYMP Nxy = y*(term3cancel + Nxyp1.GetA() + Nxyp1.GetB());

  return Nxy;
}
#endif // !OXS_DEMAG_ASYMP_USE_SSE

#if OXS_DEMAG_ASYMP_USE_SSE
OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptoticBase::NxyAsymptoticPair
(const OxsDemagNabData& ptA,const OxsDemagNabData& ptB) const
{ // Asymptotic approximation to Nxy term.  This routine takes a pair of
  // points and uses SSE intrinsics to accelerate the computation.

  // Both R2p and R2m must be positive, since asymptotics don't apply
  // for R==0.  Note self-demag term for Nxy == 0.
  if(ptA.R2<=0.0) return NxyAsymptotic(ptB);
  if(ptB.R2<=0.0) return NxyAsymptotic(ptA);

  const Oc_Duet tx2(ptA.tx2,ptB.tx2);
  const Oc_Duet ty2(ptA.ty2,ptB.ty2);
  const Oc_Duet tz2(ptA.tz2,ptB.tz2);

  const Oc_Duet term3 = 3*lead_weight;
  Oc_Duet Nxy = term3;

  if(!cubic_cell) {
    const Oc_Duet term5 = a1*tx2 + a2*ty2 + a3*tz2;
    Nxy += term5;
  }

  const Oc_Duet tz4 = tz2*tz2;
  const Oc_Duet term7
    =  b6*tz4 + ty2*(b4*ty2 + b5*tz2) + tx2*(b1*tx2 + b2*ty2 + b3*tz2);

  const Oc_Duet term9
    = c10*tz4*tz2
    + ty2*(ty2*(c7*ty2 + c8*tz2) + c9*tz4)
    + tx2*(tx2*(c1*tx2 + c2*ty2 + c3*tz2) + ty2*(c4*ty2 + c5*tz2) + c6*tz4);

  Nxy += (term7 + term9);

  const Oc_Duet iR(ptA.iR,ptB.iR);
  Oc_Duet iR5(ptA.iR2,ptB.iR2); iR5 *= iR5; iR5 *= iR;

  Nxy *= iR5;

  Nxy *= Oc_Duet(ptA.x,ptB.x) * Oc_Duet(ptA.y,ptB.y);

  OXS_DEMAG_REAL_ASYMP Nxysum = Nxy.GetA() + Nxy.GetB();

  return Nxysum;
}
#endif // OXS_DEMAG_ASYMP_USE_SSE

OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptotic::NxyAsymptotic(const OxsDemagNabData& ptdata) const
{ // Presumably at least one of xcount, ycount, or zcount is 1, but this
  // fact is not used in following code.

  // Alias data from refine_data structure.
  const OC_INT4m& xcount = refine_data.xcount;
  const OC_INT4m& ycount = refine_data.ycount;
  const OC_INT4m& zcount = refine_data.zcount;
  const OXS_DEMAG_REAL_ASYMP& rdx = refine_data.rdx;
  const OXS_DEMAG_REAL_ASYMP& rdy = refine_data.rdy;
  const OXS_DEMAG_REAL_ASYMP& rdz = refine_data.rdz;
  const OXS_DEMAG_REAL_ASYMP& result_scale = refine_data.result_scale;

  OxsDemagNabData rptdata,mrptdata;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INT4m k=1-zcount;k<zcount;++k) {
    OXS_DEMAG_REAL_ASYMP zoff = ptdata.z+k*rdz;
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INT4m j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.y+j*rdy;
      rptdata.Set(ptdata.x,yoff,zoff);
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Nxy.NxyAsymptotic(rptdata);
      for(OC_INT4m i=1;i<xcount;++i) {
        rptdata.Set(ptdata.x+i*rdx,yoff,zoff);
        mrptdata.Set(ptdata.x-i*rdx,yoff,zoff);
        xsum += (xcount-i) * Nxy.NxyAsymptoticPair(rptdata,mrptdata);
      }
      // Weight x-strip interactions into xy-plate
      ysum += (ycount - abs(j))*xsum;
    }
    // Weight xy-plate interactions into total sum
    zsum += (zcount - abs(k))*ysum;
  }
  return zsum*result_scale;
}

OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptotic::NxyAsymptoticPairX
(const OxsDemagNabPairData& ptdata) const
{ // Evaluates asymptotic approximation to
  //    Nxy(x+xoff,y,z) + Nxy(x-xoff,y,z)
  // on the assumption that |xoff| >> |x|.

  assert(ptdata.ptp.y == ptdata.ptm.y &&
         ptdata.ptp.z == ptdata.ptm.z);

  // Presumably at least one of xcount, ycount, or zcount is 1, but this
  // fact is not used in following code.

  // Alias data from refine_data structure.
  const OC_INT4m& xcount = refine_data.xcount;
  const OC_INT4m& ycount = refine_data.ycount;
  const OC_INT4m& zcount = refine_data.zcount;
  const OXS_DEMAG_REAL_ASYMP& rdx = refine_data.rdx;
  const OXS_DEMAG_REAL_ASYMP& rdy = refine_data.rdy;
  const OXS_DEMAG_REAL_ASYMP& rdz = refine_data.rdz;
  const OXS_DEMAG_REAL_ASYMP& result_scale = refine_data.result_scale;


  OxsDemagNabPairData work;
  work.ubase = ptdata.ubase;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INT4m k=1-zcount;k<zcount;++k) {
    OXS_DEMAG_REAL_ASYMP zoff = ptdata.ptp.z+k*rdz; // .ptm.z == .ptp.z
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INT4m j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.ptp.y+j*rdy; // .ptm.y == .ptp.y
      work.uoff = ptdata.uoff;
      OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                               work.ubase - work.uoff,yoff,zoff,
                               work.ptp, work.ptm);
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Nxy.NxyAsymptoticPairX(work);
      for(OC_INT4m i=1;i<xcount;++i) {
        work.uoff = ptdata.uoff + i*rdx;
        OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                                 work.ubase - work.uoff,yoff,zoff,
                                 work.ptp, work.ptm);
        OXS_DEMAG_REAL_ASYMP tmpsum = Nxy.NxyAsymptoticPairX(work);
        work.uoff = ptdata.uoff - i*rdx;
        OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                                 work.ubase - work.uoff,yoff,zoff,
                                 work.ptp, work.ptm);
        tmpsum += Nxy.NxyAsymptoticPairX(work);
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
// See NOTES V, 17-Feb-2011/2-Mar-2011, p150-167, and also Maple
// worksheets from that time.

// These classes are intended for internal use only.
//

OxsDemagNxxIntegralXBase::OxsDemagNxxIntegralXBase
(const OxsDemagAsymptoticRefineData& refine_data,
 OXS_DEMAG_REAL_ASYMP Wx)
  : scale((refine_data.rdx*refine_data.rdy*refine_data.rdz)/(4*WIDE_PI*Wx))
{
  // In "scale", 1/Wx comes from scaling of the integration variable,
  // and the sign takes into account that we evaluate the +infinity
  // integral at the lower end (the upper end being 0).

  OXS_DEMAG_REAL_ASYMP dx = refine_data.rdx;
  OXS_DEMAG_REAL_ASYMP dy = refine_data.rdy;
  OXS_DEMAG_REAL_ASYMP dz = refine_data.rdz;

  OXS_DEMAG_REAL_ASYMP dx2 = dx*dx;
  OXS_DEMAG_REAL_ASYMP dy2 = dy*dy;
  OXS_DEMAG_REAL_ASYMP dz2 = dz*dz;

  OXS_DEMAG_REAL_ASYMP dx4 = dx2*dx2;
  OXS_DEMAG_REAL_ASYMP dy4 = dy2*dy2;
  OXS_DEMAG_REAL_ASYMP dz4 = dz2*dz2;

  // See NOTES V, 17-Feb-2011, p150-153
  if(dx2!=dy2 || dx2!=dz2 || dy2!=dz2) { // Non-cube case
    cubic_cell = 0;

    a1 = ( 2*dx2  -   dy2  -   dz2)*0.25*scale;
    a2 = (-3*dx2  + 4*dy2  -   dz2)*0.25*scale;
    a3 = (-3*dx2  -   dy2  + 4*dz2)*0.25*scale;

    b1 = b2 = b3 = b4 = b5 = b6 = scale/OXS_DEMAG_REAL_ASYMP(48.);
    b1  *=  16*dx4 -  20*dx2*dy2 -  20*dx2*dz2  +  6*dy4 +   5*dy2*dz2 +  6*dz4;
    b2  *= -80*dx4 + 205*dx2*dy2 -   5*dx2*dz2  - 72*dy4 -  25*dy2*dz2 + 12*dz4;
    b3  *= -80*dx4 -   5*dx2*dy2 + 205*dx2*dz2  + 12*dy4 -  25*dy2*dz2 - 72*dz4;
    b4  *=  30*dx4 -  90*dx2*dy2 +  15*dx2*dz2  + 48*dy4 -  30*dy2*dz2 +  6*dz4;
    b5  *=  60*dx4 -  75*dx2*dy2 -  75*dx2*dz2  - 72*dy4 + 255*dy2*dz2 - 72*dz4;
    b6  *=  30*dx4 +  15*dx2*dy2 -  90*dx2*dz2  +  6*dy4 -  30*dy2*dz2 + 48*dz4;
  } else {
    cubic_cell = 1;

    a1 = a2 = a3 = 0.0;

    b1 = b2 = b3 = b4 = b5 = b6 = dx4*scale/OXS_DEMAG_REAL_ASYMP(48.);
    b1  *= OXS_DEMAG_REAL_ASYMP( -7.);
    b2  *= OXS_DEMAG_REAL_ASYMP( 35.);
    b3  *= OXS_DEMAG_REAL_ASYMP( 35.);
    b4  *= OXS_DEMAG_REAL_ASYMP(-21.);
    b5  *= OXS_DEMAG_REAL_ASYMP( 21.);
    b6  *= OXS_DEMAG_REAL_ASYMP(-21.);
  }
}

OXS_DEMAG_REAL_ASYMP
OxsDemagNxxIntegralXBase::Compute
(const OxsDemagNabPairData& pairdata) const
{ // Integral of asymptotic approximation to Nxx term, from xp to
  // infinity and -infinity to xm (xp must be > 0, and xm must be < 0),
  // with window x dimension Wx.  This routine is used internally for
  // periodic demag computations.
  //
  // See NOTES V, 17-Feb-2011, pp. 150-153, and also Maple worksheet
  // from that date.

  const OXS_DEMAG_REAL_ASYMP& xp = pairdata.ptp.x;
  const OXS_DEMAG_REAL_ASYMP& xm = pairdata.ptm.x;

  assert(xp>0.0 && xm<0.0);

  const OXS_DEMAG_REAL_ASYMP& tx2p = pairdata.ptp.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2p = pairdata.ptp.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2p = pairdata.ptp.tz2;

  const OXS_DEMAG_REAL_ASYMP& tx2m = pairdata.ptm.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2m = pairdata.ptm.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2m = pairdata.ptm.tz2;

  const OXS_DEMAG_REAL_ASYMP term3 = scale;

  OXS_DEMAG_REAL_ASYMP term5p = 0.0;
  OXS_DEMAG_REAL_ASYMP term5m = 0.0;
  if(!cubic_cell) { // Non-cube case
    term5p = a1*tx2p + a2*ty2p + a3*tz2p;
    term5m = a1*tx2m + a2*ty2m + a3*tz2m;
  }

  OXS_DEMAG_REAL_ASYMP term7p = 0.0;
  OXS_DEMAG_REAL_ASYMP term7m = 0.0;
  {
    term7p = (b1*tx2p + (b2*ty2p + b3*tz2p))*tx2p
      + (b4*ty2p + b5*tz2p)*ty2p + b6*tz2p*tz2p;
    term7m = (b1*tx2m + (b2*ty2m + b3*tz2m))*tx2m
      + (b4*ty2m + b5*tz2m)*ty2m + b6*tz2m*tz2m;
  }

  const OXS_DEMAG_REAL_ASYMP& iRp  = pairdata.ptp.iR;
  const OXS_DEMAG_REAL_ASYMP& iR2p = pairdata.ptp.iR2;

  const OXS_DEMAG_REAL_ASYMP& iRm = pairdata.ptm.iR;
  const OXS_DEMAG_REAL_ASYMP& iR2m = pairdata.ptm.iR2;

  OXS_DEMAG_REAL_ASYMP INxxp = (term7p + term5p + term3)*iR2p*iRp*xp;
  OXS_DEMAG_REAL_ASYMP INxxm = (term7m + term5m + term3)*iR2m*iRm*xm;

  OXS_DEMAG_REAL_ASYMP INxx = INxxm - INxxp;
  // For error, see NOTES V, 20-22 Feb 2011, pp.154-157.

#if 0
printf("CHECK term7p = %#.17g\n",(double)term7p); /**/
printf("CHECK term7m = %#.17g\n",(double)term7m); /**/
printf("CHECK INxx = %#.17g\n",(double)INxx); /**/
#endif

  return INxx;
}


OxsDemagNxyIntegralXBase::OxsDemagNxyIntegralXBase
(const OxsDemagAsymptoticRefineData& refine_data,
 OXS_DEMAG_REAL_ASYMP Wx)
  : scale((refine_data.rdx*refine_data.rdy*refine_data.rdz)/(4*WIDE_PI*Wx))
{
  // In "scale", 1/Wx comes from scaling of the integration variable,
  // and the sign takes into account that we evaluate the +infinity
  // integral at the lower end (the upper end being 0).

  OXS_DEMAG_REAL_ASYMP dx = refine_data.rdx;
  OXS_DEMAG_REAL_ASYMP dy = refine_data.rdy;
  OXS_DEMAG_REAL_ASYMP dz = refine_data.rdz;

  OXS_DEMAG_REAL_ASYMP dx2 = dx*dx;
  OXS_DEMAG_REAL_ASYMP dy2 = dy*dy;
  OXS_DEMAG_REAL_ASYMP dz2 = dz*dz;

  OXS_DEMAG_REAL_ASYMP dx4 = dx2*dx2;
  OXS_DEMAG_REAL_ASYMP dy4 = dy2*dy2;
  OXS_DEMAG_REAL_ASYMP dz4 = dz2*dz2;

  // See NOTES V, 17-Feb-2011, p150-153
  if(dx2!=dy2 || dx2!=dz2 || dy2!=dz2) { // Non-cube case
    cubic_cell = 0;

    a1 = ( 4*dx2  - 3*dy2  -   dz2)*0.25*scale;
    a2 = (-1*dx2  + 2*dy2  -   dz2)*0.25*scale;
    a3 = (-1*dx2  - 3*dy2  + 4*dz2)*0.25*scale;

    b1 = b2 = b3 = b4 = b5 = b6 = scale/OXS_DEMAG_REAL_ASYMP(48.);
    b1  *=  48*dx4 -  90*dx2*dy2 -  30*dx2*dz2 + 30*dy4 +  15*dy2*dz2 +  6*dz4;
    b2  *= -72*dx4 + 205*dx2*dy2 -  25*dx2*dz2 - 80*dy4 -   5*dy2*dz2 + 12*dz4;
    b3  *= -72*dx4 -  75*dx2*dy2 + 255*dx2*dz2 + 60*dy4 -  75*dy2*dz2 - 72*dz4;
    b4  *=   6*dx4 -  20*dx2*dy2 +   5*dx2*dz2 + 16*dy4 -  20*dy2*dz2 +  6*dz4;
    b5  *=  12*dx4 -   5*dx2*dy2 -  25*dx2*dz2 - 80*dy4 + 205*dy2*dz2 - 72*dz4;
    b6  *=   6*dx4 +  15*dx2*dy2 -  30*dx2*dz2 + 30*dy4 -  90*dy2*dz2 + 48*dz4;
  } else {
    cubic_cell = 1;

    a1 = a2 = a3 = 0.0;

    b1 = b2 = b3 = b4 = b5 = b6 = dx4*scale/OXS_DEMAG_REAL_ASYMP(48.);
    b1  *= OXS_DEMAG_REAL_ASYMP(-21);
    b2  *= OXS_DEMAG_REAL_ASYMP( 35);
    b3  *= OXS_DEMAG_REAL_ASYMP( 21);
    b4  *= OXS_DEMAG_REAL_ASYMP( -7);
    b5  *= OXS_DEMAG_REAL_ASYMP( 35);
    b6  *= OXS_DEMAG_REAL_ASYMP(-21);
  }
}

OXS_DEMAG_REAL_ASYMP
OxsDemagNxyIntegralXBase::Compute
(const OxsDemagNabPairData& pairdata) const
{ // Integral of asymptotic approximation to Nxy term, from xp to
  // infinity and -infinity to xm (xp must be > 0, and xm must be < 0),
  // with window x dimension Wx.  This routine is used internally for
  // periodic demag computations.
  //
  // See NOTES V, 17-Feb-2011, pp. 150-153, and also Maple worksheet
  // from that date.

  const OXS_DEMAG_REAL_ASYMP& xp = pairdata.ptp.x;
  const OXS_DEMAG_REAL_ASYMP& xm = pairdata.ptm.x;

  assert(xp>0.0 && xm<0.0);

  // Note: We expect significant cancellation between the p and m terms,
  // so replace nominal "term3 = (1.0/R3p - 1.0/R3m)" with the more
  // accurate:
  OXS_DEMAG_REAL_ASYMP x2m = xm*xm;
  OXS_DEMAG_REAL_ASYMP x2p = xp*xp;
  OXS_DEMAG_REAL_ASYMP R2yz
    = pairdata.ptp.y*pairdata.ptp.y + pairdata.ptp.z*pairdata.ptp.z;
  const OXS_DEMAG_REAL_ASYMP& xbase   = pairdata.ubase;
  const OXS_DEMAG_REAL_ASYMP& xoffset = pairdata.uoff;
  const OXS_DEMAG_REAL_ASYMP R3p  = pairdata.ptp.R2*pairdata.ptp.R;
  const OXS_DEMAG_REAL_ASYMP R3m  = pairdata.ptm.R2*pairdata.ptm.R;

  const OXS_DEMAG_REAL_ASYMP term3 = scale *
    4*xoffset*xbase*(x2m*x2m 
                     + (3*R2yz+x2p)*2*(xbase*xbase+xoffset*xoffset)
                     + 3*R2yz*R2yz)
    / (R3p*R3m*(R3p+R3m));

  const OXS_DEMAG_REAL_ASYMP iR3p = pairdata.ptp.iR * pairdata.ptp.iR2;
  const OXS_DEMAG_REAL_ASYMP iR3m = pairdata.ptm.iR * pairdata.ptm.iR2;

  const OXS_DEMAG_REAL_ASYMP& tx2p = pairdata.ptp.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2p = pairdata.ptp.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2p = pairdata.ptp.tz2;

  const OXS_DEMAG_REAL_ASYMP& tx2m = pairdata.ptm.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2m = pairdata.ptm.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2m = pairdata.ptm.tz2;

  OXS_DEMAG_REAL_ASYMP term5 = 0.0;
  if(!cubic_cell) { // Non-cube case
    OXS_DEMAG_REAL_ASYMP term5p = (a1*tx2p + a2*ty2p + a3*tz2p)*iR3p;
    OXS_DEMAG_REAL_ASYMP term5m = (a1*tx2m + a2*ty2m + a3*tz2m)*iR3m;
    term5 = term5m - term5p;
  }

  OXS_DEMAG_REAL_ASYMP term7 = 0.0;
  {
    OXS_DEMAG_REAL_ASYMP term7p
      = ((b1*tx2p + (b2*ty2p + b3*tz2p))*tx2p
         + (b4*ty2p + b5*tz2p)*ty2p + b6*tz2p*tz2p)*iR3p;
    OXS_DEMAG_REAL_ASYMP term7m
      = ((b1*tx2m + (b2*ty2m + b3*tz2m))*tx2m
         + (b4*ty2m + b5*tz2m)*ty2m + b6*tz2m*tz2m)*iR3m;
    term7 = term7m - term7p;
  }

  const OXS_DEMAG_REAL_ASYMP& y = pairdata.ptp.y; // ptp.y and ptm.y are same
  OXS_DEMAG_REAL_ASYMP INxy = y * (term7 + term5 + term3);
  // For error, see NOTES V, 20-22 Feb 2011, pp.154-157.

  return INxy;
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z0_R5(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  return iRzpR*iRzpR*(Q+2)/(-3.);
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z2_R5(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  return iRzpR*((Q+1)*Q+1)/(-3.);
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z0_R9(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  OXS_DEMAG_REAL_ASYMP iRzpR2 = iRzpR*iRzpR;
  return iRzpR2*iRzpR2*(((5*Q+20)*Q+29)*Q+16)/(-35.);
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z2_R9(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  return iRzpR*iRzpR*iRzpR*((((15*Q+45)*Q+48)*Q+24)*Q+8)/(-105.);
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z4_R9(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  return iRzpR*iRzpR*(((((5*Q+10)*Q+8)*Q+6)*Q+4)*Q+2)/(-35.);
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z0_R13(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  OXS_DEMAG_REAL_ASYMP iRzpR2 = iRzpR*iRzpR;
  return iRzpR2*iRzpR2*iRzpR2*(((((63*Q+378)*Q+938)*Q+1218)*Q+843)*Q+256)/(-693.);
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z2_R13(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  OXS_DEMAG_REAL_ASYMP iRzpR2 = iRzpR*iRzpR;
  return iRzpR*iRzpR2*iRzpR2*((((((315*Q+1575)*Q+3185)*Q+3325)*Q+1920)*Q+640)*Q+128)/(-3465.);
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z4_R13(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  OXS_DEMAG_REAL_ASYMP iRzpR2 = iRzpR*iRzpR;
  return iRzpR2*iRzpR2*(((((((105*Q+420)*Q+665)*Q+560)*Q+320)*Q+160)*Q+64)*Q+16)/(-1155.);
}

inline OXS_DEMAG_REAL_ASYMP
DemagIntegralZ_z6_R13(
OXS_DEMAG_REAL_ASYMP iRzpR, // [R*(z+R)]^(-1)
OXS_DEMAG_REAL_ASYMP Q)     // z/R
{
  return iRzpR*iRzpR*iRzpR*((((((((63*Q+189)*Q+224)*Q+168)*Q+120)*Q+80)*Q+48)*Q+24)*Q+8)/(-693.);
}


OxsDemagNxxIntegralZBase::OxsDemagNxxIntegralZBase
(const OxsDemagAsymptoticRefineData &refine_data,
 OXS_DEMAG_REAL_ASYMP Wz)
  : scale((refine_data.rdx*refine_data.rdy*refine_data.rdz)/(4*WIDE_PI*Wz))
{
  // In "scale", 1/Wz comes from scaling of the integration variable,
  // and the sign takes into account that we evaluate the +infinity
  // integral at the lower end (the upper end being 0).

  OXS_DEMAG_REAL_ASYMP dx = refine_data.rdx;
  OXS_DEMAG_REAL_ASYMP dy = refine_data.rdy;
  OXS_DEMAG_REAL_ASYMP dz = refine_data.rdz;

  OXS_DEMAG_REAL_ASYMP dx2 = dx*dx;
  OXS_DEMAG_REAL_ASYMP dy2 = dy*dy;
  OXS_DEMAG_REAL_ASYMP dz2 = dz*dz;

  OXS_DEMAG_REAL_ASYMP dx4 = dx2*dx2;
  OXS_DEMAG_REAL_ASYMP dy4 = dy2*dy2;
  OXS_DEMAG_REAL_ASYMP dz4 = dz2*dz2;

  // See NOTES V, 2-Mar-2011, p163-167
  if(dx2!=dy2 || dx2!=dz2 || dy2!=dz2) { // Non-cube case
    cubic_cell = 0;

    a1 = (  8*dx2  -  4*dy2  -  4*dz2)*0.25*scale;
    a2 = (-24*dx2  + 27*dy2  -  3*dz2)*0.25*scale;
    a3 = (-24*dx2  -  3*dy2  + 27*dz2)*0.25*scale;
    a4 = (  3*dx2  -  4*dy2  +  1*dz2)*0.25*scale;
    a5 = (  6*dx2  -  3*dy2  -  3*dz2)*0.25*scale;
    a6 = (  3*dx2  +  1*dy2  -  4*dz2)*0.25*scale;

    b1 = b2 = b3 = b4 = b5 = b6 = b7 = b8 = b9 = b10 = scale/OXS_DEMAG_REAL_ASYMP(16.);
    b1  *=   32*dx4 -  40*dx2*dy2 -  40*dx2*dz2 +  12*dy4 +  10*dy2*dz2 +  12*dz4;
    b2  *= -240*dx4 + 580*dx2*dy2 +  20*dx2*dz2 - 202*dy4 -  75*dy2*dz2 +  22*dz4;
    b3  *= -240*dx4 +  20*dx2*dy2 + 580*dx2*dz2 +  22*dy4 -  75*dy2*dz2 - 202*dz4;
    b4  *=  180*dx4 - 505*dx2*dy2 +  55*dx2*dz2 + 232*dy4 -  75*dy2*dz2 +   8*dz4;
    b5  *=  360*dx4 - 450*dx2*dy2 - 450*dx2*dz2 - 180*dy4 + 900*dy2*dz2 - 180*dz4;
    b6  *=  180*dx4 +  55*dx2*dy2 - 505*dx2*dz2 +   8*dy4 -  75*dy2*dz2 + 232*dz4;
    b7  *=  -10*dx4 +  30*dx2*dy2 -   5*dx2*dz2 -  16*dy4 +  10*dy2*dz2 -   2*dz4;
    b8  *=  -30*dx4 +  55*dx2*dy2 +  20*dx2*dz2 +   8*dy4 -  75*dy2*dz2 +  22*dz4;
    b9  *=  -30*dx4 +  20*dx2*dy2 +  55*dx2*dz2 +  22*dy4 -  75*dy2*dz2 +   8*dz4;
    b10 *=  -10*dx4 -   5*dx2*dy2 +  30*dx2*dz2 -   2*dy4 +  10*dy2*dz2 -  16*dz4;
  } else {
    cubic_cell = 1;

    a1 = a2 = a3 = a4 = a5 = a6 = 0.0;

    b1 = b2 = b3 = b4 = b5 = b6 = b7 = b8 = b9 = b10 = dx4*scale/OXS_DEMAG_REAL_ASYMP(16.);
    b1  *= OXS_DEMAG_REAL_ASYMP( -14);
    b2  *= OXS_DEMAG_REAL_ASYMP( 105);
    b3  *= OXS_DEMAG_REAL_ASYMP( 105);
    b4  *= OXS_DEMAG_REAL_ASYMP(-105);
    b5  *= OXS_DEMAG_REAL_ASYMP(   0);
    b6  *= OXS_DEMAG_REAL_ASYMP(-105);
    b7  *= OXS_DEMAG_REAL_ASYMP(   7);
    b8  *= OXS_DEMAG_REAL_ASYMP(   0);
    b9  *= OXS_DEMAG_REAL_ASYMP(   0);
    b10 *= OXS_DEMAG_REAL_ASYMP(   7);
  }
}

OXS_DEMAG_REAL_ASYMP
OxsDemagNxxIntegralZBase::Compute
(const OxsDemagNabPairData& pairdata) const
{ // Integral of asymptotic approximation to Nxx term, from zp to
  // infinity and -infinity to zm (zp must be > 0, and zm must be < 0),
  // with window z dimension Wz.  This routine is used internally for
  // periodic demag computations.
  //
  // See NOTES V, 2-Mar-2011, pp. 163-167.

  const OXS_DEMAG_REAL_ASYMP& zp = pairdata.ptp.z;
  OXS_DEMAG_REAL_ASYMP zm = pairdata.ptm.z;

  assert(zp>0.0 && zm<0.0);
  zm = Oc_Fabs(zm);

  const OXS_DEMAG_REAL_ASYMP& Rp = pairdata.ptp.R;
  const OXS_DEMAG_REAL_ASYMP& Rm = pairdata.ptm.R;

  // Imports to integrals.  See NOTES V, 5-Mar-2011, p. 166
  OXS_DEMAG_REAL_ASYMP iRzpRp = 1.0/(Rp*(zp+Rp));
  OXS_DEMAG_REAL_ASYMP Qp = zp/Rp;

  OXS_DEMAG_REAL_ASYMP iRzpRm = 1.0/(Rm*(zm+Rm));
  OXS_DEMAG_REAL_ASYMP Qm = zm/Rm;

  const OXS_DEMAG_REAL_ASYMP& x = pairdata.ptp.x; // ptp.x and ptm.x are same
  const OXS_DEMAG_REAL_ASYMP& y = pairdata.ptp.y; // ptp.y and ptm.y are same
  OXS_DEMAG_REAL_ASYMP x2 = x*x;
  OXS_DEMAG_REAL_ASYMP y2 = y*y;

  // Terms: pre-integration orders 1/R^3, 1/R^5, 1/R^7.
  // After integration wrt z order is one less.
  OXS_DEMAG_REAL_ASYMP term3 = 0.0;
  {
    OXS_DEMAG_REAL_ASYMP I0_5  = DemagIntegralZ_z0_R5(iRzpRp,Qp)
                      + DemagIntegralZ_z0_R5(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I2_5  = DemagIntegralZ_z2_R5(iRzpRp,Qp)
                      + DemagIntegralZ_z2_R5(iRzpRm,Qm);
    term3 = scale*((2*x2 - y2)*I0_5 - I2_5);
  }

  OXS_DEMAG_REAL_ASYMP y4 = y2*y2;

  OXS_DEMAG_REAL_ASYMP term5 = 0.0;
  if(!cubic_cell) { // Non-cube case
    OXS_DEMAG_REAL_ASYMP I0_9  = DemagIntegralZ_z0_R9(iRzpRp,Qp)
                      + DemagIntegralZ_z0_R9(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I2_9  = DemagIntegralZ_z2_R9(iRzpRp,Qp)
                      + DemagIntegralZ_z2_R9(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I4_9  = DemagIntegralZ_z4_R9(iRzpRp,Qp)
                      + DemagIntegralZ_z4_R9(iRzpRm,Qm);
    term5 = ((a1*x2 + a2*y2)*x2 + a4*y4)*I0_9
          + (a3*x2 + a5*y2)*I2_9
          + a6*I4_9;
  }

  OXS_DEMAG_REAL_ASYMP term7 = 0.0;
  {
    OXS_DEMAG_REAL_ASYMP I0_13 = DemagIntegralZ_z0_R13(iRzpRp,Qp)
                      + DemagIntegralZ_z0_R13(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I2_13 = DemagIntegralZ_z2_R13(iRzpRp,Qp)
                      + DemagIntegralZ_z2_R13(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I4_13 = DemagIntegralZ_z4_R13(iRzpRp,Qp)
                      + DemagIntegralZ_z4_R13(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I6_13 = DemagIntegralZ_z6_R13(iRzpRp,Qp)
                      + DemagIntegralZ_z6_R13(iRzpRm,Qm);
    if(!cubic_cell) { // Non-cube case
      term7 = (((b1*x2 + b2*y2)*x2 + b4*y4)*x2 + b7*y4*y2)*I0_13
            + ((b3*x2 + b5*y2)*x2 + b8*y4)*I2_13
            + (b6*x2 + b9*y2)*I4_13
            + b10*I6_13;
    } else {
      // In cubic case, some coefficients are zero.
      term7 = (((b1*x2 + b2*y2)*x2 + b4*y4)*x2 + b7*y4*y2)*I0_13
            + (b3*x2*I2_13 + b6*I4_13)*x2
            + b10*I6_13;
    }
  }

  OXS_DEMAG_REAL_ASYMP INxx = term7 + term5 + term3;

  return INxx;
}

OxsDemagNxyIntegralZBase::OxsDemagNxyIntegralZBase
(const OxsDemagAsymptoticRefineData& refine_data,
 OXS_DEMAG_REAL_ASYMP Wz)
  : scale((refine_data.rdx*refine_data.rdy*refine_data.rdz)/(4*WIDE_PI*Wz))
{
  // In "scale", 1/Wz comes from scaling of the integration variable,
  // and the sign takes into account that we evaluate the +infinity
  // integral at the lower end (the upper end being 0).

  OXS_DEMAG_REAL_ASYMP dx = refine_data.rdx;
  OXS_DEMAG_REAL_ASYMP dy = refine_data.rdy;
  OXS_DEMAG_REAL_ASYMP dz = refine_data.rdz;

  OXS_DEMAG_REAL_ASYMP dx2 = dx*dx;
  OXS_DEMAG_REAL_ASYMP dy2 = dy*dy;
  OXS_DEMAG_REAL_ASYMP dz2 = dz*dz;

  OXS_DEMAG_REAL_ASYMP dx4 = dx2*dx2;
  OXS_DEMAG_REAL_ASYMP dy4 = dy2*dy2;
  OXS_DEMAG_REAL_ASYMP dz4 = dz2*dz2;

  // See NOTES V, 2-Mar-2011, p163-167
  if(dx2!=dy2 || dx2!=dz2 || dy2!=dz2) { // Non-cube case
    cubic_cell = 0;

    a1 = ( 4*dx2  -  3*dy2  -  1*dz2)*1.25*scale;
    a2 = (-3*dx2  +  4*dy2  -  1*dz2)*1.25*scale;
    a3 = (-3*dx2  -  3*dy2  +  6*dz2)*1.25*scale;

    b1 = b2 = b3 = b4 = b5 = b6 = scale*OXS_DEMAG_REAL_ASYMP(7.)/OXS_DEMAG_REAL_ASYMP(16.);
    b1 *=  16*dx4 -  30*dx2*dy2 -  10*dx2*dz2 + 10*dy4 +   5*dy2*dz2 +  2*dz4;
    b2 *= -40*dx4 + 105*dx2*dy2 -   5*dx2*dz2 - 40*dy4 -   5*dy2*dz2 +  4*dz4;
    b3 *= -40*dx4 -  15*dx2*dy2 + 115*dx2*dz2 + 20*dy4 -  35*dy2*dz2 - 32*dz4;
    b4 *=  10*dx4 -  30*dx2*dy2 +   5*dx2*dz2 + 16*dy4 -  10*dy2*dz2 +  2*dz4;
    b5 *=  20*dx4 -  15*dx2*dy2 -  35*dx2*dz2 - 40*dy4 + 115*dy2*dz2 - 32*dz4;
    b6 *=  10*dx4 +  15*dx2*dy2 -  40*dx2*dz2 + 10*dy4 -  40*dy2*dz2 + 32*dz4;
  } else {
    cubic_cell = 1;

    a1 = a2 = a3 = 0.0;

    b1 = b2 = b3 = b4 = b5 = b6 = dx4*scale*OXS_DEMAG_REAL_ASYMP(7.)/OXS_DEMAG_REAL_ASYMP(16.);
    b1  *= OXS_DEMAG_REAL_ASYMP( -7);
    b2  *= OXS_DEMAG_REAL_ASYMP( 19);
    b3  *= OXS_DEMAG_REAL_ASYMP( 13);
    b4  *= OXS_DEMAG_REAL_ASYMP( -7);
    b5  *= OXS_DEMAG_REAL_ASYMP( 13);
    b6  *= OXS_DEMAG_REAL_ASYMP(-13);
  }
}

OXS_DEMAG_REAL_ASYMP
OxsDemagNxyIntegralZBase::Compute
(const OxsDemagNabPairData& pairdata) const
{ // Integral of asymptotic approximation to Nxy term, from zp to
  // infinity and -infinity to zm (zp must be > 0, and zm must be < 0),
  // with window z dimension Wz.  This routine is used internally for
  // periodic demag computations.
  //
  // See NOTES V, 2-Mar-2011, pp. 163-167.

  const OXS_DEMAG_REAL_ASYMP& zp = pairdata.ptp.z;
  OXS_DEMAG_REAL_ASYMP zm = pairdata.ptm.z;

  assert(zp>0.0 && zm<0.0);
  zm = Oc_Fabs(zm);

  const OXS_DEMAG_REAL_ASYMP& Rp = pairdata.ptp.R;
  const OXS_DEMAG_REAL_ASYMP& Rm = pairdata.ptm.R;

  // Imports to integrals.  See NOTES V, 5-Mar-2011, p. 166
  OXS_DEMAG_REAL_ASYMP iRzpRp = 1.0/(Rp*(zp+Rp));
  OXS_DEMAG_REAL_ASYMP Qp = zp/Rp;

  OXS_DEMAG_REAL_ASYMP iRzpRm = 1.0/(Rm*(zm+Rm));
  OXS_DEMAG_REAL_ASYMP Qm = zm/Rm;

  // Terms: pre-integration orders 1/R^3, 1/R^5, 1/R^7.
  // After integration wrt z order is one less.
  const OXS_DEMAG_REAL_ASYMP term3 = 3*scale*(DemagIntegralZ_z0_R5(iRzpRp,Qp)
                                     +DemagIntegralZ_z0_R5(iRzpRm,Qm));

  const OXS_DEMAG_REAL_ASYMP& x = pairdata.ptp.x; // ptp.x and ptm.x are same
  const OXS_DEMAG_REAL_ASYMP& y = pairdata.ptp.y; // ptp.y and ptm.y are same
  const OXS_DEMAG_REAL_ASYMP x2 = x*x;
  const OXS_DEMAG_REAL_ASYMP y2 = y*y;

  OXS_DEMAG_REAL_ASYMP term5 = 0.0;
  if(!cubic_cell) { // Non-cube case
    OXS_DEMAG_REAL_ASYMP I0_9  = DemagIntegralZ_z0_R9(iRzpRp,Qp)
                      + DemagIntegralZ_z0_R9(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I2_9  = DemagIntegralZ_z2_R9(iRzpRp,Qp)
                      + DemagIntegralZ_z2_R9(iRzpRm,Qm);
    term5 = (a1*x2 + a2*y2)*I0_9 + a3*I2_9;
  }

  OXS_DEMAG_REAL_ASYMP term7 = 0.0;
  {
    OXS_DEMAG_REAL_ASYMP I0_13 = DemagIntegralZ_z0_R13(iRzpRp,Qp)
                      + DemagIntegralZ_z0_R13(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I2_13 = DemagIntegralZ_z2_R13(iRzpRp,Qp)
                      + DemagIntegralZ_z2_R13(iRzpRm,Qm);
    OXS_DEMAG_REAL_ASYMP I4_13 = DemagIntegralZ_z4_R13(iRzpRp,Qp)
                      + DemagIntegralZ_z4_R13(iRzpRm,Qm);
    term7 = ((b1*x2 + b2*y2)*x2 + b4*y2*y2)*I0_13
          + (b3*x2 + b5*y2)*I2_13
          + b6*I4_13;
  }

  OXS_DEMAG_REAL_ASYMP INxy = x * y * (term7 + term5 + term3);

  return INxy;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

const OXS_DEMAG_REAL_ASYMP OxsDemagPeriodic::D[TAIL_TWEAK_COUNT] = {
  464514259. / OXS_DEMAG_REAL_ASYMP(464486400.),
  464115227. / OXS_DEMAG_REAL_ASYMP(464486400.),
  467323119. / OXS_DEMAG_REAL_ASYMP(464486400.),
  438283495. / OXS_DEMAG_REAL_ASYMP(464486400.),
   26202905. / OXS_DEMAG_REAL_ASYMP(464486400.),
   -2836719. / OXS_DEMAG_REAL_ASYMP(464486400.),
     371173. / OXS_DEMAG_REAL_ASYMP(464486400.),
     -27859. / OXS_DEMAG_REAL_ASYMP(464486400.)
};

OxsDemagPeriodic::OxsDemagPeriodic
(OXS_DEMAG_REAL_ASYMP dx, OXS_DEMAG_REAL_ASYMP dy, OXS_DEMAG_REAL_ASYMP dz,
 OXS_DEMAG_REAL_ASYMP iW,OXS_DEMAG_REAL_ASYMP iAsymptoticRadius)
  : W(iW),
    SDA_scaling(4*WIDE_PI*dx*dy*dz),
    AsymptoticStart(iAsymptoticRadius>0
                    ? iAsymptoticRadius : iW/2),
    /// Arithmetic mean is larger than geometric mean, but not sure
    /// which to use in this context.  Note that if import
    /// i_AsymptoticRadius is <=0, then use half the periodic width
    /// as the asymptotic start point.  The code in this method
    /// requires some value for this, because the tail computation
    /// uses the asymptotic approximation.
    ktail(0) // Kludged below
{
  // ktail is big enough that the outer sum of asymptotic approximates
  // can be replaced by integrals from ktail*W to infinity, and from
  // -infinity to -ktail*W.  The following computed value for ktail
  // should make the integral approximation to the asymptotic sum less
  // than 1e-16, which should be sufficient if final sum of all parts is
  // O(1).  (See NOTES V, 20-Feb-2011, p155.)
  const OXS_DEMAG_REAL_ASYMP CHECK_VALUE
    = 43.15L; // Value for 1e-16 absolute error
  const OXS_DEMAG_REAL_ASYMP gamma
    = Oc_Pow(OXS_DEMAG_REAL_ASYMP(dx*dy*dz),
             OXS_DEMAG_REAL_ASYMP(1.)/OXS_DEMAG_REAL_ASYMP(3.));
  const_cast<OC_INDEX&>(ktail)
    = OC_INDEX(Oc_Ceil(CHECK_VALUE/Oc_Sqrt(Oc_Sqrt(W/gamma))-2.0));
}

void OxsDemagPeriodic::ComputeAsymptoticLimits
(OXS_DEMAG_REAL_ASYMP u, // base offset along periodic direction
 OXS_DEMAG_REAL_ASYMP v, OXS_DEMAG_REAL_ASYMP w, // Other coordinates
 OC_INDEX& k1, OC_INDEX& k2,   // Near field terms
 OC_INDEX& k1a, OC_INDEX& k2a, // Asymmetric far field
 OXS_DEMAG_REAL_ASYMP& newu, OXS_DEMAG_REAL_ASYMP& newoffset // Symmetric far field
 ) const
{
  OXS_DEMAG_REAL_ASYMP ulimit = 0.0;
  OXS_DEMAG_REAL_ASYMP Asq = AsymptoticStart*AsymptoticStart - v*v - w*w;
  if(Asq>0) ulimit = sqrt(Asq);

  k1 = OC_INDEX(Oc_Floor((-ulimit-u)/W));
  k2 = OC_INDEX(Oc_Ceil((ulimit-u)/W));
  if(k1==k2) --k1; // Special case: ulimit=0 and u/W is an integer
  /// (Actually, not ulimit==0 but (u +/- ulimit) == u.)

  assert(k1<k2);
  assert(u+k1*W <= -ulimit);
  assert(u+k2*W >=  ulimit);

  // k1 and k2 are selected above so that u + k1*W < -ulimit
  // and u + k2*W > ulimit.  But we also want u + k1*W and
  // u + k2*W to be as symmetric about the origin as possible,
  // so that we get better canceling on odd terms.  We insure
  // that with this tweak:
  k1a = k1;    k2a = k2;
  if(u+k1*W + u+k2*W > W/2) {
    k1a = k1-1;
  } else if(u+k1*W + u+k2*W < -W/2) {
    k2a = k2+1;
  }

  // Note that k1 and k2 used for near field, and k1a and
  // k2a used for the far field tweak, are indexed off of
  // the original u. But newu and newoffset are computed so
  // that asymptotic approximate pairs are computed for
  // newu +/- (newoffset + k*W), for k = 0, 1, 2, ...
  newu = u + (k2a + k1a)*W/2;
  newoffset = (k2a - k1a)*W/2;
}


void
Oxs_DemagPeriodicX::ComputeTensor
(const OxsDemagNabPeriodic& Nab,
 const OxsDemagNabPeriodic& Ncd,
 const OxsDemagNabPeriodic& Nef,
 OXS_DEMAG_REAL_ASYMP x,
 OXS_DEMAG_REAL_ASYMP y,
 OXS_DEMAG_REAL_ASYMP z,
 OXS_DEMAG_REAL_ASYMP& pbcNab,
 OXS_DEMAG_REAL_ASYMP& pbcNcd,
 OXS_DEMAG_REAL_ASYMP& pbcNef) const
{ // Note: *Does* include Nab term from base window.

  // Two break points: ASYMPTOTIC_START is the limit beyond
  // which the asymptotic approximations to the tensor are
  // used, and INTEGRAL_START is the point where sums of
  // asymptotic terms are replaced with an integral.
  // The range (k1,k2) is the non-asymptotic regime,
  // (k1a,k1) and (k2,k2a) are potential asymmetric tweaks
  // in the asymptotic regime.  The ranges (-infty,k1a]
  // and [k2a,infty) are roughly symmetric wings in
  // the asymptotic regime. Control ktail specifies
  // the start of the integral regime in this wings.
  OC_INDEX k1, k2;   // k1  < k2
  OC_INDEX k1a, k2a; // k1a <= k1, k2 <= k2a

  OXS_DEMAG_REAL_ASYMP xasm; // Used in place of x in asymptotic regime.
  /// This coordinate is tweaked to improve balance in
  /// +/- asymptotic estimates.

  OXS_DEMAG_REAL_ASYMP xoffasm; // Base offset used in asymptotic regime.
  ComputeAsymptoticLimits(x,y,z,k1,k2,k1a,k2a,xasm,xoffasm);

  OXS_DEMAG_REAL_ASYMP resultNab = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNcd = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNef = 0.0;

  // First compute integral approximations for tails.  See NOTES V, 7-20
  // Jan 2011, pp. 102-108.  The version below uses the n=4 variant on
  // p. 108:
  const OC_INDEX kstop = (ktail - OC_INDEX(Oc_Floor(xoffasm/W)) > 0 ?
                          ktail - OC_INDEX(Oc_Floor(xoffasm/W)) : 0);
  OC_INDEX k;
  OxsDemagNabPairData pairpt;  pairpt.ubase = xasm;
  for(k=0;k<TAIL_TWEAK_COUNT;++k) {
    pairpt.uoff = xoffasm+(kstop+k)*W;
    OxsDemagNabData::SetPair(xasm+pairpt.uoff,y,z,
                             xasm-pairpt.uoff,y,z,
                             pairpt.ptp,pairpt.ptm);
    resultNab += Nab.FarFieldPair(pairpt) * D[k];
    resultNcd += Ncd.FarFieldPair(pairpt) * D[k];
    resultNef += Nef.FarFieldPair(pairpt) * D[k];
  }
  pairpt.uoff = xoffasm+(kstop+(TAIL_TWEAK_COUNT-1)/2.)*W;
  OxsDemagNabData::SetPair(xasm+pairpt.uoff,y,z,
                           xasm-pairpt.uoff,y,z,
                           pairpt.ptp,pairpt.ptm);
  resultNab += Nab.FarIntegralPair(pairpt);
  resultNcd += Ncd.FarIntegralPair(pairpt);
  resultNef += Nef.FarIntegralPair(pairpt);

  // Next, sum in asymptotic terms that are paired off, +/-.
  //  These use the offset base (xasm+xoffasm,yasm,zasm)
  for(k=0;k<kstop;++k) {
    pairpt.uoff = xoffasm+k*W;
    OxsDemagNabData::SetPair(xasm+pairpt.uoff,y,z,
                             xasm-pairpt.uoff,y,z,
                             pairpt.ptp,pairpt.ptm);
    resultNab += Nab.FarFieldPair(pairpt);
    resultNcd += Ncd.FarFieldPair(pairpt);
    resultNef += Nef.FarFieldPair(pairpt);
  }

  // Add in assymetric "tweak" terms.  These use the same base
  // (x,y,z) as the NearField.
  OxsDemagNabData pt;
  for(k=k1a+1;k<=k1;++k) {
    pt.Set(x+k*W,y,z);
    resultNab += Nab.FarField(pt);
    resultNcd += Ncd.FarField(pt);
    resultNef += Nef.FarField(pt);
  }
  for(k=k2;k<k2a;++k) {
    pt.Set(x+k*W,y,z);
    resultNab += Nab.FarField(pt);
    resultNcd += Ncd.FarField(pt);
    resultNef += Nef.FarField(pt);
  }

  // Finally compute terms that are too close to the origin to use
  // asymptotics.  These are likely to be the largest individual terms.
  OXS_DEMAG_REAL_ASYMP nearNab = 0.0;
  OXS_DEMAG_REAL_ASYMP nearNcd = 0.0;
  OXS_DEMAG_REAL_ASYMP nearNef = 0.0;
  for(k = k1+1;k<k2;k++) {
    pt.Set(x+k*W,y,z);
    nearNab += Nab.NearField(pt);
    nearNcd += Ncd.NearField(pt);
    nearNef += Nef.NearField(pt);
  }
  resultNab += nearNab/SDA_scaling; // SDA## = Nab*(4*PI*dx*dy*dz)
  resultNcd += nearNcd/SDA_scaling;
  resultNef += nearNef/SDA_scaling;

  pbcNab = resultNab;   pbcNcd = resultNcd;   pbcNef = resultNef;
}

void
Oxs_DemagPeriodicY::ComputeTensor
(const OxsDemagNabPeriodic& Nab,
 const OxsDemagNabPeriodic& Ncd,
 const OxsDemagNabPeriodic& Nef,
 OXS_DEMAG_REAL_ASYMP x,
 OXS_DEMAG_REAL_ASYMP y,
 OXS_DEMAG_REAL_ASYMP z,
 OXS_DEMAG_REAL_ASYMP& pbcNab,
 OXS_DEMAG_REAL_ASYMP& pbcNcd,
 OXS_DEMAG_REAL_ASYMP& pbcNef) const
{ // Note: *Does* include Nab term from base window.

  // Two break points: ASYMPTOTIC_START is the limit beyond
  // which the asymptotic approximations to the tensor are
  // used, and INTEGRAL_START is the point where sums of
  // asymptotic terms are replaced with an integral.
  // The range (k1,k2) is the non-asymptotic regime,
  // (k1a,k1) and (k2,k2a) are potential asymmetric tweaks
  // in the asymptotic regime.  The ranges (-infty,k1a]
  // and [k2a,infty) are roughly symmetric wings in
  // the asymptotic regime. Control ktail specifies
  // the start of the integral regime in this wings.
  OC_INDEX k1, k2;   // k1  < k2
  OC_INDEX k1a, k2a; // k1a <= k1, k2 <= k2a

  OXS_DEMAG_REAL_ASYMP yasm; // Used in place of y in asymptotic regime.
  /// This coordinate is tweaked to improve balance in
  /// +/- asymptotic estimates.

  OXS_DEMAG_REAL_ASYMP yoffasm; // Base offset used in asymptotic regime.
  ComputeAsymptoticLimits(y,x,z,k1,k2,k1a,k2a,yasm,yoffasm);

  OXS_DEMAG_REAL_ASYMP resultNab = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNcd = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNef = 0.0;

  // First compute integral approximations for tails.  See NOTES V, 7-20
  // Jan 2011, pp. 102-108.  The version below uses the n=4 variant on
  // p. 108:
  const OC_INDEX kstop = (ktail - OC_INDEX(Oc_Floor(yoffasm/W)) > 0 ?
                          ktail - OC_INDEX(Oc_Floor(yoffasm/W)) : 0);
  OC_INDEX k;
  OxsDemagNabPairData pairpt;  pairpt.ubase = yasm;
  for(k=0;k<TAIL_TWEAK_COUNT;++k) {
    pairpt.uoff = yoffasm+(kstop+k)*W;
    OxsDemagNabData::SetPair(x,yasm+pairpt.uoff,z,
                             x,yasm-pairpt.uoff,z,
                             pairpt.ptp,pairpt.ptm);
    resultNab += Nab.FarFieldPair(pairpt) * D[k];
    resultNcd += Ncd.FarFieldPair(pairpt) * D[k];
    resultNef += Nef.FarFieldPair(pairpt) * D[k];
  }
  pairpt.uoff = yoffasm+(kstop+(TAIL_TWEAK_COUNT-1)/2.)*W;
  OxsDemagNabData::SetPair(x,yasm+pairpt.uoff,z,
                           x,yasm-pairpt.uoff,z,
                           pairpt.ptp,pairpt.ptm);
  resultNab += Nab.FarIntegralPair(pairpt);
  resultNcd += Ncd.FarIntegralPair(pairpt);
  resultNef += Nef.FarIntegralPair(pairpt);

  // Next, sum in asymptotic terms that are paired off, +/-.
  //  These use the offset base (xasm+xoffasm,yasm,zasm)
  for(k=0;k<kstop;++k) {
    pairpt.uoff = yoffasm+k*W;
    OxsDemagNabData::SetPair(x,yasm+pairpt.uoff,z,
                             x,yasm-pairpt.uoff,z,
                             pairpt.ptp,pairpt.ptm);
    resultNab += Nab.FarFieldPair(pairpt);
    resultNcd += Ncd.FarFieldPair(pairpt);
    resultNef += Nef.FarFieldPair(pairpt);
  }

  // Add in assymetric "tweak" terms.  These use the same base
  // (x,y,z) as the NearField.
  OxsDemagNabData pt;
  for(k=k1a+1;k<=k1;++k) {
    pt.Set(x,y+k*W,z);
    resultNab += Nab.FarField(pt);
    resultNcd += Ncd.FarField(pt);
    resultNef += Nef.FarField(pt);
  }
  for(k=k2;k<k2a;++k) {
    pt.Set(x,y+k*W,z);
    resultNab += Nab.FarField(pt);
    resultNcd += Ncd.FarField(pt);
    resultNef += Nef.FarField(pt);
  }

  // Finally compute terms that are too close to the origin to use
  // asymptotics.  These are likely to be the largest individual terms.
  OXS_DEMAG_REAL_ASYMP nearNab = 0.0;
  OXS_DEMAG_REAL_ASYMP nearNcd = 0.0;
  OXS_DEMAG_REAL_ASYMP nearNef = 0.0;
  for(k = k1+1;k<k2;k++) {
    pt.Set(x,y+k*W,z);
    nearNab += Nab.NearField(pt);
    nearNcd += Ncd.NearField(pt);
    nearNef += Nef.NearField(pt);
  }
  resultNab += nearNab/SDA_scaling; // SDA## = Nab*(4*PI*dx*dy*dz)
  resultNcd += nearNcd/SDA_scaling;
  resultNef += nearNef/SDA_scaling;

  pbcNab = resultNab;   pbcNcd = resultNcd;   pbcNef = resultNef;
}

void
Oxs_DemagPeriodicZ::ComputeTensor
(const OxsDemagNabPeriodic& Nab,
 const OxsDemagNabPeriodic& Ncd,
 const OxsDemagNabPeriodic& Nef,
 OXS_DEMAG_REAL_ASYMP x,
 OXS_DEMAG_REAL_ASYMP y,
 OXS_DEMAG_REAL_ASYMP z,
 OXS_DEMAG_REAL_ASYMP& pbcNab,
 OXS_DEMAG_REAL_ASYMP& pbcNcd,
 OXS_DEMAG_REAL_ASYMP& pbcNef) const
{ // Note: *Does* include Nab term from base window.

  // Two break points: ASYMPTOTIC_START is the limit beyond
  // which the asymptotic approximations to the tensor are
  // used, and INTEGRAL_START is the point where sums of
  // asymptotic terms are replaced with an integral.
  // The range (k1,k2) is the non-asymptotic regime,
  // (k1a,k1) and (k2,k2a) are potential asymmetric tweaks
  // in the asymptotic regime.  The ranges (-infty,k1a]
  // and [k2a,infty) are roughly symmetric wings in
  // the asymptotic regime. Control ktail specifies
  // the start of the integral regime in this wings.
  OC_INDEX k1, k2;   // k1  < k2
  OC_INDEX k1a, k2a; // k1a <= k1, k2 <= k2a

  OXS_DEMAG_REAL_ASYMP zasm; // Used in place of x in asymptotic regime.
  /// This coordinate is tweaked to improve balance in
  /// +/- asymptotic estimates.

  OXS_DEMAG_REAL_ASYMP zoffasm; // Base offset used in asymptotic regime.
  ComputeAsymptoticLimits(z,x,y,k1,k2,k1a,k2a,zasm,zoffasm);

  OXS_DEMAG_REAL_ASYMP resultNab = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNcd = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNef = 0.0;

  // First compute integral approximations for tails.  See NOTES V, 7-20
  // Jan 2011, pp. 102-108.  The version below uses the n=4 variant on
  // p. 108:
  const OC_INDEX kstop = (ktail - OC_INDEX(Oc_Floor(zoffasm/W)) > 0 ?
                          ktail - OC_INDEX(Oc_Floor(zoffasm/W)) : 0);
  OC_INDEX k;
  OxsDemagNabPairData pairpt;  pairpt.ubase = zasm;
  for(k=0;k<TAIL_TWEAK_COUNT;++k) {
    pairpt.uoff = zoffasm+(kstop+k)*W;
    OxsDemagNabData::SetPair(x,y,zasm+pairpt.uoff,
                             x,y,zasm-pairpt.uoff,
                             pairpt.ptp,pairpt.ptm);
    resultNab += Nab.FarFieldPair(pairpt) * D[k];
    resultNcd += Ncd.FarFieldPair(pairpt) * D[k];
    resultNef += Nef.FarFieldPair(pairpt) * D[k];
  }
  pairpt.uoff = zoffasm+(kstop+(TAIL_TWEAK_COUNT-1)/2.)*W;
  OxsDemagNabData::SetPair(x,y,zasm+pairpt.uoff,
                           x,y,zasm-pairpt.uoff,
                           pairpt.ptp,pairpt.ptm);
  resultNab += Nab.FarIntegralPair(pairpt);
  resultNcd += Ncd.FarIntegralPair(pairpt);
  resultNef += Nef.FarIntegralPair(pairpt);

  // Next, sum in asymptotic terms that are paired off, +/-.
  //  These use the offset base (xasm+xoffasm,yasm,zasm)
  for(k=0;k<kstop;++k) {
    pairpt.uoff = zoffasm+k*W;
    OxsDemagNabData::SetPair(x,y,zasm+pairpt.uoff,
                             x,y,zasm-pairpt.uoff,
                             pairpt.ptp,pairpt.ptm);
    resultNab += Nab.FarFieldPair(pairpt);
    resultNcd += Ncd.FarFieldPair(pairpt);
    resultNef += Nef.FarFieldPair(pairpt);
  }

  // Add in assymetric "tweak" terms.  These use the same base
  // (x,y,z) as the NearField.
  OxsDemagNabData pt;
  for(k=k1a+1;k<=k1;++k) {
    pt.Set(x,y,z+k*W);
    resultNab += Nab.FarField(pt);
    resultNcd += Ncd.FarField(pt);
    resultNef += Nef.FarField(pt);
  }
  for(k=k2;k<k2a;++k) {
    pt.Set(x,y,z+k*W);
    resultNab += Nab.FarField(pt);
    resultNcd += Ncd.FarField(pt);
    resultNef += Nef.FarField(pt);
  }

  // Finally compute terms that are too close to the origin to use
  // asymptotics.  These are likely to be the largest individual terms.
  OXS_DEMAG_REAL_ASYMP nearNab = 0.0;
  OXS_DEMAG_REAL_ASYMP nearNcd = 0.0;
  OXS_DEMAG_REAL_ASYMP nearNef = 0.0;
  for(k = k1+1;k<k2;k++) {
    pt.Set(x,y,z+k*W);
    nearNab += Nab.NearField(pt);
    nearNcd += Ncd.NearField(pt);
    nearNef += Nef.NearField(pt);
  }
  resultNab += nearNab/SDA_scaling; // SDA## = Nab*(4*PI*dx*dy*dz)
  resultNcd += nearNcd/SDA_scaling;
  resultNef += nearNef/SDA_scaling;

  pbcNab = resultNab;   pbcNcd = resultNcd;   pbcNef = resultNef;
}



///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

#ifdef STANDALONE

void Usage()
{
  fprintf(stderr,
          "Usage: pbctest x y z hx hy hz W periodic_direction [arad]\n");
  fprintf(stderr,
          " where x y z is offset,\n"
          "       hx hy hz are cell dimensions,\n"
          "       W is window length in periodic direction,\n"
          "   and periodic_direction is one of x, y, z or n"
          " (n -> not periodic)\n");
  exit(1);
}

#include <ctype.h>
#include <stdio.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

void PrettyFormat(long double x,char* buf,size_t bufsize)
{
  char tmpbuf[64];
  snprintf(tmpbuf,sizeof(tmpbuf),"% .18Le",x);
  char *cptr = tmpbuf;
  int mode = 0;
  int grpcnt = 0;
  size_t i = 0;
  while(i<bufsize-1 && *cptr != '\0') {
    unsigned int ctmp = (unsigned int)(*(cptr++));
    buf[i++] = (char)ctmp;
    switch(mode) {
    case 0:
      if(ctmp == '.') {
        mode = 1;
        grpcnt = 0;
      }
      break;
    case 1:
      if( ctmp < '0' || '9'< ctmp) {
        mode = 2;
      } else {
        if(++grpcnt == 4) {
          grpcnt = 0;
          if(i<bufsize-1) buf[i++] = ' ';
        }
      }
      break;
    default:
      break;
    }
  }
  buf[i] = '\0';
}


int main(int argc,char** argv)
{
  if(argc<9 || argc>10) Usage();

#if defined(_MSC_VER)
# define strtold(x,y) strtod((x),(y))
#elif defined(__BORLANDC__)
# define strtold(x,y) _strtold((x),(y))
#endif

#if OXS_DEMAG_ASYMP_USE_SSE
  printf("SSE-enabled code\n");
#else
  printf("Non-SSE code\n");
#endif

  OXS_DEMAG_REAL_ASYMP  x = strtold(argv[1],0);
  OXS_DEMAG_REAL_ASYMP  y = strtold(argv[2],0);
  OXS_DEMAG_REAL_ASYMP  z = strtold(argv[3],0);
  OXS_DEMAG_REAL_ASYMP hx = strtold(argv[4],0);
  OXS_DEMAG_REAL_ASYMP hy = strtold(argv[5],0);
  OXS_DEMAG_REAL_ASYMP hz = strtold(argv[6],0);
  OXS_DEMAG_REAL_ASYMP  W = strtold(argv[7],0);

  OXS_DEMAG_REAL_ASYMP gamma
    = Oc_Pow(hx*hy*hz,OXS_DEMAG_REAL_ASYMP(1)/OXS_DEMAG_REAL_ASYMP(3));
  OXS_DEMAG_REAL_ASYMP pdist = Oc_Sqrt(x*x+y*y+z*z)/gamma;
  printf("gamma=%Lg, pdist=%Lg\n",(long double)gamma,(long double)pdist);

  char direction = tolower(argv[8][0]);
  if(direction != 'x' && direction != 'y' && direction != 'z'
     && direction != 'n') Usage();

  OXS_DEMAG_REAL_ASYMP asymptotic_radius = 32;
  if(argc>9) asymptotic_radius = strtold(argv[9],0);

  OXS_DEMAG_REAL_ASYMP Nxx,Nxy,Nxz,Nyy,Nyz,Nzz;

  if(direction=='x') {
    Oxs_DemagPeriodicX pbctensor(hx,hy,hz,W,asymptotic_radius);
    pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
    pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
  } else if(direction=='y') {
    Oxs_DemagPeriodicY pbctensor(hx,hy,hz,W,asymptotic_radius);
    pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
    pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
  } else if(direction=='z') {
    Oxs_DemagPeriodicZ pbctensor(hx,hy,hz,W,asymptotic_radius);
    pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
    pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
  } else {
    if(pdist<asymptotic_radius) {
      // Near field
      Nxx = Oxs_CalculateSDA00(x,y,z,hx,hy,hz)/(4*WIDE_PI*hx*hy*hz);
      Nxy = Oxs_CalculateSDA01(x,y,z,hx,hy,hz)/(4*WIDE_PI*hx*hy*hz);
      Nxz = Oxs_CalculateSDA02(x,y,z,hx,hy,hz)/(4*WIDE_PI*hx*hy*hz);
      Nyy = Oxs_CalculateSDA11(x,y,z,hx,hy,hz)/(4*WIDE_PI*hx*hy*hz);
      Nyz = Oxs_CalculateSDA12(x,y,z,hx,hy,hz)/(4*WIDE_PI*hx*hy*hz);
      Nzz = Oxs_CalculateSDA22(x,y,z,hx,hy,hz)/(4*WIDE_PI*hx*hy*hz);
    } else {
      // Far field
      Oxs_DemagNxxAsymptotic ANxx(hx,hy,hz,1.5);
      Nxx = ANxx.NxxAsymptotic(x,y,z);
      Oxs_DemagNxyAsymptotic ANxy(hx,hy,hz);
      Nxy = ANxy.NxyAsymptotic(x,y,z);
      Oxs_DemagNxzAsymptotic ANxz(hx,hy,hz);
      Nxz = ANxz.NxzAsymptotic(x,y,z);
      Oxs_DemagNyyAsymptotic ANyy(hx,hy,hz);
      Nyy = ANyy.NyyAsymptotic(x,y,z);
      Oxs_DemagNyzAsymptotic ANyz(hx,hy,hz);
      Nyz = ANyz.NyzAsymptotic(x,y,z);
      Oxs_DemagNzzAsymptotic ANzz(hx,hy,hz);
      Nzz = ANzz.NzzAsymptotic(x,y,z);
    }
  }

  char buf0[64];
  char buf1[64];
  char buf2[64];

  printf("--- Demag tensor; point offset=%g, asympt rad=%g:\n",
         (double)pdist,(double)asymptotic_radius);

  PrettyFormat(Nxx,buf0,sizeof(buf0));
  PrettyFormat(Nxy,buf1,sizeof(buf1));
  PrettyFormat(Nxz,buf2,sizeof(buf2));
  printf("%30s %30s %30s\n",buf0,buf1,buf2);

  PrettyFormat(Nyy,buf0,sizeof(buf0));
  PrettyFormat(Nyz,buf1,sizeof(buf1));
  PrettyFormat(Nzz,buf2,sizeof(buf2));
  printf("%30s %30s %30s\n",
         "",buf0,buf1);
  printf("%30s %30s %30s\n",
         "", "",buf2);
  printf("-------------------\n");

#if 0
  OXS_DEMAG_REAL_ASYMP dx,dy,dz;
  OXS_DEMAG_REAL_ASYMP maxedge=hx;
  if(hy>maxedge) maxedge=hy;
  if(hz>maxedge) maxedge=hz;
  OXS_DEMAG_REAL_ASYMP qdx = hx/maxedge;
  OXS_DEMAG_REAL_ASYMP qdy = hy/maxedge;
  OXS_DEMAG_REAL_ASYMP qdz = hz/maxedge;
  OXS_DEMAG_REAL_ASYMP qx = x/maxedge;
  OXS_DEMAG_REAL_ASYMP qy = y/maxedge;
  OXS_DEMAG_REAL_ASYMP qz = z/maxedge;
  Oxs_DemagNxxAsymptotic ANxxC(hx,hy,hz);
  printf("ANxxC= %#.18Le\n",(long double)ANxxC.NxxAsymptotic(x,y,z));
  Oxs_DemagNxxAsymptotic ANxx(qdx,qdy,qdz);
  printf("ANxx = %#.18Le\n",(long double)ANxx.NxxAsymptotic(qx,qy,qz));
  Oxs_DemagNxyAsymptotic ANxy(qdx,qdy,qdz);
  printf("ANxy = %#.18Le\n",(long double)ANxy.NxyAsymptotic(qx,qy,qz));
#endif    


#if 0
  OxsDemagNxxIntegralX integral_xx(hx,hy,hz,W);
  printf("Nxx integral x = %# .18Le\n",
         (long double)integral_xx.Compute(x,y,z,16*asymptotic_radius));
  OxsDemagNxyIntegralX integral_yx(hx,hy,hz,W);
  printf("Nxy integral x = %# .18Le\n",
         (long double)integral_yx.Compute(x,y,z,16*asymptotic_radius));
  OxsDemagNxxIntegralZ integral_xz(hx,hy,hz,W);
  printf("Nxx integral z = %# .18Le\n",
         (long double)integral_xz.Compute(x,y,z,16*asymptotic_radius));
  OxsDemagNxyIntegralZ integral_yz(hx,hy,hz,W);
  printf("Nxy integral z = %# .18Le\n",
         (long double)integral_yz.Compute(x,y,z,16*asymptotic_radius));
#endif
#if 0
  OxsDemagNxxPeriodicX chkNxx(hx,hy,hz,W,asymptotic_radius);
  printf("Nxx periodic x = %# .18Le\n",(long double)chkNxx.NabPeriodic(x,y,z));
#endif

#if 0
  Oxs_DemagNxyAsymptotic chkNxyA(1,2,3);
  OXS_DEMAG_REAL_ASYMP chkNxyAa = chkNxyA.NxyAsymptotic(1+1000,90000,-10)
                       + chkNxyA.NxyAsymptotic(1-1000,90000,-10);
  OXS_DEMAG_REAL_ASYMP chkNxyAb = chkNxyA.NxyAsymptoticPair(1,1000,90000,-10);

  Oxs_DemagNxyAsymptotic chkNxyB(1,1,1);
  OXS_DEMAG_REAL_ASYMP chkNxyBa = chkNxyB.NxyAsymptotic(1+1000,-90000,10)
                       + chkNxyB.NxyAsymptotic(1-1000,-90000,10);
  OXS_DEMAG_REAL_ASYMP chkNxyBb = chkNxyB.NxyAsymptoticPair(1,1000,-90000,10);

  printf("ANxy checks: non-cube singlet = %# .18Le\n",chkNxyAa);
  printf("             non-cube    pair = %# .18Le\n",chkNxyAb);
  printf("                 cube singlet = %# .18Le\n",chkNxyBa);
  printf("                 cube    pair = %# .18Le\n",chkNxyBb);
#endif

#if 0
  fprintf(stderr,"TEST CASE: (x,y,z,dx,dy,dz,Wx,x) = 35 73 37 21 7 1.25 71 x\n");
  fprintf(stderr,"Nxx = -1.6143 7575 0748 6188 48e-06\n");
  fprintf(stderr,"Nxy = -6.7854 3365 2804 0251 64e-08\n");
  fprintf(stderr,"Nxz =  3.4748 9767 7164 0223 78e-08\n");
  fprintf(stderr,"Nyy = -3.4872 2508 3919 4894 20e-05\n");
  fprintf(stderr,"Nyz = -4.8955 3905 0807 7719 87e-05\n");
  fprintf(stderr,"Nzz =  3.6486 6265 8994 3513 05e-05\n");
  fprintf(stderr,"See maple script maple-20110225-demag_tensor.mw\n");
#endif
  return 0;
}

OC_REALWIDE Oc_Nop(OC_REALWIDE x) { return x; }

#endif // STANDALONE
