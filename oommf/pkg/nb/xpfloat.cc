/* FILE: xpfloat.cc                    -*-Mode: c++-*-
 *
 * Class for extended floating point precision using
 * compensated summation.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/07/17 22:47:46 $
 * Last modified by: $Author: donahue $
 */

#include <cstdlib>

#include "xpfloat.h"

/* Use double-double library from oommf/pkg/xp/ for multiplication. */
#include "xp.h"

/* End includes */


int Nb_Xpfloat::Test() {
  // Test that Accum() code works as designed.
  /// Returns 0 on success, 1 on error.
  NB_XPFLOAT_TYPE a,b;
  Nb_Xpfloat test = 1.0 + (rand()-1)/RAND_MAX;  // Dummy zero value
  test += pow(2.,-80); /// designed so compiler can't eat whole test.
  test.GetValue(a,b);
  if(b==0.0) return 1;
  return 0;
}

#if !NB_XPFLOAT_USE_SSE
void Nb_Xpfloat::Accum(const Nb_Xpfloat &y)
{
  if((x>0 && y.x>0) || (x<0 && y.x<0)) {
    Accum(y.corr);  Accum(y.x);
  } else {
    Accum(y.x);  Accum(y.corr);
  }
}

Nb_Xpfloat& Nb_Xpfloat::operator-=(const Nb_Xpfloat &y)
{
  if((x>0 && y.x<0) || (x<0 && y.x>0)) {
    Accum(-y.corr);  Accum(-y.x);
  } else {
    Accum(-y.x);  Accum(-y.corr);
  }
  return *this;
}

#else // NB_XPFLOAT_USE_SSE ////////////////////////////////////////////////

void Nb_Xpfloat::Accum(const Nb_Xpfloat &y)
{
  double a0 = Oc_SseGetLower(xpdata);
  double b0 = Oc_SseGetLower(y.xpdata);
  if((a0>0 && b0>0) || (a0<0 && b0<0)) {
    Accum(Oc_SseGetUpper(y.xpdata)); Accum(Oc_SseGetLower(y.xpdata));
  } else {
    Accum(Oc_SseGetLower(y.xpdata)); Accum(Oc_SseGetUpper(y.xpdata));
  }
}

Nb_Xpfloat& Nb_Xpfloat::operator-=(const Nb_Xpfloat &y)
{
  Nb_Xpfloat tmp = y;
  tmp *= -1.0;
  Accum(tmp);
  return *this;
}

# if !NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL
// If NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL is true then
// the following function is defined instead in the xpfloat.h header.
void
Nb_XpfloatDualAccum(Nb_Xpfloat& xpA,Nb_Xpfloat& xpB,const __m128d& y)
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
  __m128d wx_hi = _mm_unpackhi_pd(xpA.xpdata,xpB.xpdata);
  __m128d sum1  = _mm_add_pd(y,wx_hi);
  __m128d wx_lo = _mm_unpacklo_pd(xpA.xpdata,xpB.xpdata);
  __m128d sum2  = _mm_add_pd(wx_lo,sum1);
  __m128d diff  = _mm_sub_pd(wx_lo,sum2);
  __m128d corrtemp = _mm_add_pd(diff,sum1);
  xpA.xpdata = _mm_unpacklo_pd(sum2,corrtemp);
  xpB.xpdata = _mm_unpackhi_pd(sum2,corrtemp);
}
# endif // !NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL

#endif // NB_XPFLOAT_USE_SSE

// Assume NB_XPFLOAT_COMPILER_FILE_ASSOCIATIVITY_CONTROL is true,
// because otherwise what can we do?
#if !NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL
#if  !NB_XPFLOAT_USE_SSE
void Nb_Xpfloat::Accum(NB_XPFLOAT_TYPE y)
{
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

# else // NB_XPFLOAT_USE_SSE

void Nb_Xpfloat::Accum(NB_XPFLOAT_TYPE y) {
  // SSE double precision doesn't carry any extra precision, so we
  // don't need to use "volatile"
  __m128d corr = _mm_unpackhi_pd(xpdata,_mm_setzero_pd());
  __m128d sum1 = _mm_add_sd(_mm_set_sd(y),corr);
  __m128d sum2 = _mm_add_sd(sum1,xpdata);
  __m128d corrtemp = _mm_add_sd(sum1,_mm_sub_sd(xpdata,sum2));
  xpdata = _mm_unpacklo_pd(sum2,corrtemp);
}

# endif // NB_XPFLOAT_USE_SSE
#endif // !NB_XPFLOAT_COMPILER_FUNCTION_ASSOCIATIVITY_CONTROL

Nb_Xpfloat operator+(const Nb_Xpfloat& x,const Nb_Xpfloat& y)
{
  Nb_Xpfloat z(x);
  z.Accum(y);
  return z;
}

Nb_Xpfloat operator-(const Nb_Xpfloat& x,const Nb_Xpfloat& y)
{
  Nb_Xpfloat z(y);
  z *= -1.;
  z.Accum(x);
  return z;
}

////////////////////////////////////////////////////////////////////////
// Multiplication
Nb_Xpfloat& Nb_Xpfloat::operator*=(NB_XPFLOAT_TYPE y) {
  // Use double-double library to compute product
#if !NB_XPFLOAT_USE_SSE
  Xp_DoubleDouble dd(x,corr);
  dd *= y;
  x    = static_cast<NB_XPFLOAT_TYPE>(dd.Hi());
  corr = static_cast<NB_XPFLOAT_TYPE>(dd.Lo());
#else
  NB_XPFLOAT_TYPE big,small;
  GetValue(big,small);
  Xp_DoubleDouble dd(big,small);
  dd *= y;
  xpdata = _mm_set_pd(static_cast<NB_XPFLOAT_TYPE>(dd.Lo()),
                      static_cast<NB_XPFLOAT_TYPE>(dd.Hi()));
#endif
  return *this;
}

Nb_Xpfloat operator*(const Nb_Xpfloat& x,NB_XPFLOAT_TYPE y)
{
  Nb_Xpfloat z(x);
  z *= y;
  return z;
}


Nb_Xpfloat& Nb_Xpfloat::operator*=(const Nb_Xpfloat& y)
{
#if !NB_XPFLOAT_USE_SSE
  Xp_DoubleDouble ddx(x,corr);
  Xp_DoubleDouble ddy(y.x,y.corr);
  ddx *= ddy;
  x    = static_cast<NB_XPFLOAT_TYPE>(ddx.Hi());
  corr = static_cast<NB_XPFLOAT_TYPE>(ddx.Lo());
#else
  NB_XPFLOAT_TYPE xbig,xsmall,ybig,ysmall;
  GetValue(xbig,xsmall);
  Xp_DoubleDouble ddx(xbig,xsmall);
  y.GetValue(ybig,ysmall);
  Xp_DoubleDouble ddy(ybig,ysmall);
  ddx *= ddy;
  xpdata = _mm_set_pd(static_cast<NB_XPFLOAT_TYPE>(ddx.Lo()),
                      static_cast<NB_XPFLOAT_TYPE>(ddx.Hi()));
#endif
  return *this;
}

Nb_Xpfloat operator*(const Nb_Xpfloat& x,const Nb_Xpfloat& y)
{
  Nb_Xpfloat z(x);
  z *= y;
  return z;
}

