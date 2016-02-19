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

Nb_Xpfloat& Nb_Xpfloat::operator*=(const Nb_Xpfloat& y)
{
  Nb_Xpfloat z(corr*y.corr);
  z.Accum(x*y.corr);
  z.Accum(y.x*corr);
  z.Accum(x*y.x);

  x = z.x;
  corr = z.corr;

  return *this;
}

Nb_Xpfloat operator*(const Nb_Xpfloat& x,const Nb_Xpfloat& y)
{
  Nb_Xpfloat z(x.corr*y.corr);
  z.Accum(x.x*y.corr);
  z.Accum(y.x*x.corr);
  z.Accum(x.x*y.x);
  return z;
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
  double a0 = Oc_SseGetLower(xpdata);
  double b0 = Oc_SseGetLower(y.xpdata);
  if((a0>0 && b0<0) || (a0<0 && b0>0)) {
    Accum(Oc_SseGetUpper(y.xpdata)); Accum(Oc_SseGetLower(y.xpdata));
  } else {
    Accum(Oc_SseGetLower(y.xpdata)); Accum(Oc_SseGetUpper(y.xpdata));
  }
  return *this;
}

Nb_Xpfloat& Nb_Xpfloat::operator*=(const Nb_Xpfloat& y)
{
  NB_XPFLOAT_TYPE ax    = Oc_SseGetLower(xpdata);
  NB_XPFLOAT_TYPE acorr = Oc_SseGetUpper(xpdata);
  NB_XPFLOAT_TYPE bx    = Oc_SseGetLower(y.xpdata);
  NB_XPFLOAT_TYPE bcorr = Oc_SseGetUpper(y.xpdata);

  Nb_Xpfloat z(acorr*bcorr);
  z.Accum(ax*bcorr);
  z.Accum(bx*acorr);
  z.Accum(ax*bx);

  xpdata = z.xpdata;
  return *this;
}

Nb_Xpfloat operator*(const Nb_Xpfloat& x,const Nb_Xpfloat& y)
{
  NB_XPFLOAT_TYPE ax    = Oc_SseGetLower(x.xpdata);
  NB_XPFLOAT_TYPE acorr = Oc_SseGetUpper(x.xpdata);
  NB_XPFLOAT_TYPE bx    = Oc_SseGetLower(y.xpdata);
  NB_XPFLOAT_TYPE bcorr = Oc_SseGetUpper(y.xpdata);

  Nb_Xpfloat z(acorr*bcorr);
  z.Accum(ax*bcorr);
  z.Accum(bx*acorr);
  z.Accum(ax*bx);

  return z;
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

Nb_Xpfloat operator*(const Nb_Xpfloat& x,NB_XPFLOAT_TYPE y)
{
  Nb_Xpfloat z(x);
  z*=y;
  return z;
}

