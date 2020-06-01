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

#include <stdlib.h>

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

#endif // NB_XPFLOAT_USE_SSE

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
