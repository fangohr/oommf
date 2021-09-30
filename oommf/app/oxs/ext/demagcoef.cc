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
 */

// The "STANDALONE" blocks provide support for building this file with
// ../demagtensor.cc and demagcoef.h as a standalone application for
// testing and development.
#ifndef STANDALONE
# define STANDALONE 0
#endif

#ifndef DUMPKNOTS
# define DUMPKNOTS 0
#endif
#if DUMPKNOTS
#include <sstream>
#endif // DUMPKNOTS

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <limits>

#ifndef USE_LOG1P
# define USE_LOG1P 1  // Use log1p in place of log(1+x) where possible.
/// This should make the results more accurate, but test results are
/// mixed.
#endif

#if !STANDALONE
# include "oc.h"
# include "nb.h"
# include "oxsthread.h"
# include "oxswarn.h"
  OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported.
/// For some compilers this is needed to get "long double"
/// versions of the basic math library functions.
#endif // STANDALONE

# include "demagcoef.h"

/* End includes */

#if !STANDALONE
// Revision information; in the original design the data here
// would be set via CVS keyword substitution.  This needs
// reworking to do something sensible in a Git environment. *****
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1$",
   "$Date: 2018/01/25 19:09:00 $",
   "$Author: donahue $",
   "Michael J. Donahue (michael.donahue@nist.gov)");
#endif // STANDALONE

#if STANDALONE
const Xp_DoubleDouble Xp_DoubleDouble::DD_PI
   = 3.141592653589793238462643383279502884L;
const Xp_DoubleDouble Xp_DoubleDouble::DD_SQRT2
   = 1.414213562373095048801688724209698079L;
#endif // STANDALONE

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
// Routines to calculate kernel coefficients
// See Newell et al. for details. The code below follows the
// naming conventions in that paper.

OXS_DEMAG_REAL_ANALYTIC Oxs_SelfDemagNx
(OXS_DEMAG_REAL_ANALYTIC x,
 OXS_DEMAG_REAL_ANALYTIC y,
 OXS_DEMAG_REAL_ANALYTIC z)
{ // Here Hx = -Nxx.Mx (formula (16) in Newell).
  // Note: egcs-2.91.57 on Linux/x86 with -O1 mangles this
  //  function (produces NaN's) unless we manually group terms.
  // See NOTES V, 14-Mar-2011, p. 181-184 for details on
  // accurate numerics.

  if(x<=0.0 || y<=0.0 || z<=0.0) return 0.0;
  if(x==y && y==z) {  // Special case: cube
    return OXS_DEMAG_REAL_ANALYTIC(1.)/OXS_DEMAG_REAL_ANALYTIC(3.);
  }

  // NOTE: The sqrt, Atan, log1p, etc. functions resolve to
  //       calls in the Xp_DoubleDouble class.

  OXS_DEMAG_REAL_ANALYTIC xsq=x*x,ysq=y*y,zsq=z*z;
  OXS_DEMAG_REAL_ANALYTIC R   = sqrt(xsq+ysq+zsq);
  OXS_DEMAG_REAL_ANALYTIC Rxy = sqrt(xsq+ysq);
  OXS_DEMAG_REAL_ANALYTIC Rxz = sqrt(xsq+zsq);
  OXS_DEMAG_REAL_ANALYTIC Ryz = sqrt(ysq+zsq);

  OXS_DEMAG_REAL_ANALYTIC sum;

  sum = 2*x*y*z
    * ( (x/(x+Rxy)+(2*xsq+ysq+zsq)/(R*Rxy+x*Rxz))
        /(x+Rxz)
      + (x/(x+Rxz)+(2*xsq+ysq+zsq)/(R*Rxz+x*Rxy))
        /(x+Rxy))
    / ((x+R)*(Rxy+Rxz+R));
  sum += -1*x*y*z
    * ( (y/(y+Rxy)+(2*ysq+xsq+zsq)/(R*Rxy+y*Ryz))
        /(y+Ryz)
      + (y/(y+Ryz)+(2*ysq+xsq+zsq)/(R*Ryz+y*Rxy))
        /(y+Rxy))
    / ((y+R)*(Rxy+Ryz+R));
  sum += -1*x*y*z
    * ( (z/(z+Rxz)+(2*zsq+xsq+ysq)/(R*Rxz+z*Ryz))
        /(z+Ryz)
      + (z/(z+Ryz)+(2*zsq+xsq+ysq)/(R*Ryz+z*Rxz))
        /(z+Rxz))
    / ((z+R)*(Rxz+Ryz+R));

  sum +=  6*atan(y*z/(x*R));

  OXS_DEMAG_REAL_ANALYTIC piece4
    = -y*z*z*(1/(x+Rxz)+y/(Rxy*Rxz+x*R))/(Rxz*(y+Rxy));
  if(piece4 > -0.5) {
#if USE_LOG1P
    sum += 3*x*log1p(piece4)/z;
#else
    sum += 3*x*log(1.0+piece4)/z;
#endif
  } else {
    sum += 3*x*log(x*(y+R)/(Rxz*(y+Rxy)))/z;
  }

  OXS_DEMAG_REAL_ANALYTIC piece5
    = -y*y*z*(1/(x+Rxy)+z/(Rxy*Rxz+x*R))/(Rxy*(z+Rxz));
  if(piece5 > -0.5) {
#if USE_LOG1P
    sum += 3*x*log1p(piece5)/y;
#else
    sum += 3*x*log(1.0+piece5)/y;
#endif
  } else {
    sum += 3*x*log(x*(z+R)/(Rxy*(z+Rxz)))/y;
  }

  OXS_DEMAG_REAL_ANALYTIC piece6
    = -x*x*z*(1/(y+Rxy)+z/(Rxy*Ryz+y*R))/(Rxy*(z+Ryz));
  if(piece6 > -0.5) {
#if USE_LOG1P
    sum += -3*y*log1p(piece6)/x;
#else
    sum += -3*y*log(1.0+piece6)/x;
#endif
  } else {
    sum += -3*y*log(y*(z+R)/(Rxy*(z+Ryz)))/x;
  }

  OXS_DEMAG_REAL_ANALYTIC piece7
    = -x*x*y*(1/(z+Rxz)+y/(Rxz*Ryz+z*R))/(Rxz*(y+Ryz));
  if(piece7 > -0.5) {
#if USE_LOG1P
    sum += -3*z*log1p(piece7)/x;
#else
    sum += -3*z*log(1.0+piece7)/x;
#endif
  } else {
    sum += -3*z*log(z*(y+R)/(Rxz*(y+Ryz)))/x;
  }

  OXS_DEMAG_REAL_ANALYTIC scale = 3 * OXS_DEMAG_REAL_ANALYTIC_PI;
  OXS_DEMAG_REAL_ANALYTIC Nxx = sum/scale;
  return Nxx;
}

OXS_DEMAG_REAL_ANALYTIC
Oxs_SelfDemagNy(OXS_DEMAG_REAL_ANALYTIC xsize,
                OXS_DEMAG_REAL_ANALYTIC ysize,
                OXS_DEMAG_REAL_ANALYTIC zsize)
{ return Oxs_SelfDemagNx(ysize,zsize,xsize); }

OXS_DEMAG_REAL_ANALYTIC
Oxs_SelfDemagNz(OXS_DEMAG_REAL_ANALYTIC xsize,
                OXS_DEMAG_REAL_ANALYTIC ysize,
                OXS_DEMAG_REAL_ANALYTIC zsize)
{ return Oxs_SelfDemagNx(zsize,xsize,ysize); }


OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_f
(OXS_DEMAG_REAL_ANALYTIC x,
 OXS_DEMAG_REAL_ANALYTIC y,
 OXS_DEMAG_REAL_ANALYTIC z)
{ // There is mucking around here to handle case where imports
  // are near zero.  In particular, asinh(t) is written as
  // log(t+sqrt(1+t)) because the latter appears easier to
  // handle if t=y/x (for example) as x -> 0.

  // NOTE: The sqrt, atan, log1p, etc. functions resolve to
  //       calls in the Xp_DoubleDouble class.

  // This function is even; the fabs()'s just simplify special case handling.
  x=fabs(x); OXS_DEMAG_REAL_ANALYTIC xsq=x*x;
  y=fabs(y); OXS_DEMAG_REAL_ANALYTIC ysq=y*y;
  z=fabs(z); OXS_DEMAG_REAL_ANALYTIC zsq=z*z;

  OXS_DEMAG_REAL_ANALYTIC Rsq=xsq+ysq+zsq;
  if(Rsq<=0.0) return 0.0;
  OXS_DEMAG_REAL_ANALYTIC R=sqrt(Rsq);

  // f(x,y,z)
  OXS_DEMAG_REAL_ANALYTIC sum = 0.0;
  if(z>0.) { // For 2D grids, half the calls from F1 have z==0.
    OXS_DEMAG_REAL_ANALYTIC temp1,temp2,temp3;
    sum += 2*(2*xsq-ysq-zsq)*R;
    if((temp1=x*y*z)>0.)
      sum += -12*temp1*Oc_Atan2(y*z,x*R);
    if(y>0. && (temp2=xsq+zsq)>0.) {
#if USE_LOG1P
      OXS_DEMAG_REAL_ANALYTIC dummy = log1p(2*y*(y+R)/temp2);
#else
      OXS_DEMAG_REAL_ANALYTIC dummy = log(((y+R)*(y+R))/temp2);
#endif
      sum += 3*y*(zsq-xsq)*dummy;
    }
    if((temp3=xsq+ysq)>0.) {
#if USE_LOG1P
      OXS_DEMAG_REAL_ANALYTIC dummy = log1p(2*z*(z+R)/temp3);
#else
      OXS_DEMAG_REAL_ANALYTIC dummy = log(((z+R)*(z+R))/temp3);
#endif
      sum += 3*z*(ysq-xsq)*dummy;
    }
  } else {
    // z==0
    if(x==y) {
      const OXS_DEMAG_REAL_ANALYTIC K
        = 2*OXS_DEMAG_REAL_ANALYTIC_SQRT2
        -6*log(1+OXS_DEMAG_REAL_ANALYTIC_SQRT2);
      /// K = -2.4598143973710680537922785014593576970294L
      sum += K*xsq*x;
    } else {
      sum += 2*(2*xsq-ysq)*R;
      if(y>0. && x>0.) {
#if USE_LOG1P
	sum += -3*y*xsq*log1p(2*y*(y+R)/(x*x));
#else
	sum += -6*y*xsq*log((y+R)/x);
#endif
      }
    }
  }

  return sum/12.;
}

OXS_DEMAG_REAL_ANALYTIC
Oxs_CalculateNxx
(OXS_DEMAG_REAL_ANALYTIC x,
 OXS_DEMAG_REAL_ANALYTIC y,
 OXS_DEMAG_REAL_ANALYTIC z,
 OXS_DEMAG_REAL_ANALYTIC dx,
 OXS_DEMAG_REAL_ANALYTIC dy,
 OXS_DEMAG_REAL_ANALYTIC dz)
{ // This is Nxx in Newell's paper.
  OXS_DEMAG_REAL_ANALYTIC result=0.;
  if(x==0. && y==0. && z==0.) {
    // Self demag term.  The base routine can handle x==y==z==0,
    // but this should be more accurate.
    result = Oxs_SelfDemagNx(dx,dy,dz);
  } else {
    // Simplified (collapsed) formula based on Newell's paper.
    // This saves about half the calls to f().  There is still
    // quite a bit of redundancy from one cell site to the next,
    // but as this is an initialization-only issue speed shouldn't
    // be too critical.
    result = -1*Oxs_Newell_f(x+dx,y+dy,z+dz);
    result += -1*Oxs_Newell_f(x+dx,y-dy,z+dz);
    result += -1*Oxs_Newell_f(x+dx,y-dy,z-dz);
    result += -1*Oxs_Newell_f(x+dx,y+dy,z-dz);
    result += -1*Oxs_Newell_f(x-dx,y+dy,z-dz);
    result += -1*Oxs_Newell_f(x-dx,y+dy,z+dz);
    result += -1*Oxs_Newell_f(x-dx,y-dy,z+dz);
    result += -1*Oxs_Newell_f(x-dx,y-dy,z-dz);

    result +=  2*Oxs_Newell_f(x,y-dy,z-dz);
    result +=  2*Oxs_Newell_f(x,y-dy,z+dz);
    result +=  2*Oxs_Newell_f(x,y+dy,z+dz);
    result +=  2*Oxs_Newell_f(x,y+dy,z-dz);
    result +=  2*Oxs_Newell_f(x+dx,y+dy,z);
    result +=  2*Oxs_Newell_f(x+dx,y,z+dz);
    result +=  2*Oxs_Newell_f(x+dx,y,z-dz);
    result +=  2*Oxs_Newell_f(x+dx,y-dy,z);
    result +=  2*Oxs_Newell_f(x-dx,y-dy,z);
    result +=  2*Oxs_Newell_f(x-dx,y,z+dz);
    result +=  2*Oxs_Newell_f(x-dx,y,z-dz);
    result +=  2*Oxs_Newell_f(x-dx,y+dy,z);

    result += -4*Oxs_Newell_f(x,y-dy,z);
    result += -4*Oxs_Newell_f(x,y+dy,z);
    result += -4*Oxs_Newell_f(x,y,z-dz);
    result += -4*Oxs_Newell_f(x,y,z+dz);
    result += -4*Oxs_Newell_f(x+dx,y,z);
    result += -4*Oxs_Newell_f(x-dx,y,z);

    result +=  8*Oxs_Newell_f(x,y,z);
    result /= (4*OXS_DEMAG_REAL_ANALYTIC_PI*dx*dy*dz);
  }
  return result;
}


OXS_DEMAG_REAL_ANALYTIC
Oxs_Newell_g
(OXS_DEMAG_REAL_ANALYTIC x,
 OXS_DEMAG_REAL_ANALYTIC y,
 OXS_DEMAG_REAL_ANALYTIC z)
{ // There is mucking around here to handle case where imports
  // are near zero.  In particular, asinh(t) is written as
  // log(t+sqrt(1+t)) because the latter appears easier to
  // handle if t=y/x (for example) as x -> 0.

  // This function is even in z and odd in x and y.  The fabs()'s
  // simplify special case handling.
  OXS_DEMAG_REAL_ANALYTIC result_sign=1.0;
  if(x<0.0) result_sign *= -1.0;
  if(y<0.0) result_sign *= -1.0;
  x=fabs(x); OXS_DEMAG_REAL_ANALYTIC xsq=x*x;
  y=fabs(y); OXS_DEMAG_REAL_ANALYTIC ysq=y*y;
  z=fabs(z); OXS_DEMAG_REAL_ANALYTIC zsq=z*z;

  OXS_DEMAG_REAL_ANALYTIC Rsq=xsq+ysq+zsq;
  if(Rsq<=0.0) return 0.0;
  OXS_DEMAG_REAL_ANALYTIC R=sqrt(Rsq);

  // g(x,y,z)
  OXS_DEMAG_REAL_ANALYTIC sum;
  sum = -2*x*y*R;;
  if(z>0.) {
    // For 2D grids, 1/3 of the calls from Oxs_CalculateNxy have z==0.
    sum +=   -z*zsq*Oc_Atan2(x*y,z*R);
    sum += -3*z*ysq*Oc_Atan2(x*z,y*R);
    sum += -3*z*xsq*Oc_Atan2(y*z,x*R);

    OXS_DEMAG_REAL_ANALYTIC temp1,temp2,temp3;
    if((temp1=xsq+ysq)>0.) {
#if USE_LOG1P
      sum += 3*x*y*z*log1p(2*z*(z+R)/temp1);
#else
      sum += 3*x*y*z*log((z+R)*(z+R)/temp1);
#endif
    }

    if((temp2=ysq+zsq)>0.){
#if USE_LOG1P
      sum += 0.5*y*(3*zsq-ysq)*log1p(2*x*(x+R)/temp2);
#else
      sum += 0.5*y*(3*zsq-ysq)*log((x+R)*(x+R)/temp2);
#endif
    }

    if((temp3=xsq+zsq)>0.) {
#if USE_LOG1P
      sum += 0.5*x*(3*zsq-xsq)*log1p(2*y*(y+R)/temp3);
#else
      sum += 0.5*x*(3*zsq-xsq)*log((y+R)*(y+R)/temp3);
#endif
    }

  } else {
    // z==0.
#if USE_LOG1P
    if(y>0.) sum += -0.5*y*ysq*log1p(2*x*(x+R)/(y*y));
    if(x>0.) sum += -0.5*x*xsq*log1p(2*y*(y+R)/(x*x));
#else
    if(y>0.) sum += -y*ysq*log((x+R)/y);
    if(x>0.) sum += -x*xsq*log((y+R)/x);
#endif
  }

  return result_sign*sum/6.;
}

OXS_DEMAG_REAL_ANALYTIC
Oxs_CalculateNxy
(OXS_DEMAG_REAL_ANALYTIC x,
 OXS_DEMAG_REAL_ANALYTIC y,
 OXS_DEMAG_REAL_ANALYTIC z,
 OXS_DEMAG_REAL_ANALYTIC dx,
 OXS_DEMAG_REAL_ANALYTIC dy,
 OXS_DEMAG_REAL_ANALYTIC dz)
{ // This is Nxy in Newell's paper.
  // Nxy is odd in x, odd in y, and even in z This implies that the
  // return value is zero if (x,y,z) lies in the yz-plane (i.e., x=0) or
  // xz-planes (y=0).
  if(x==0.0 || y==0.0) return OXS_DEMAG_REAL_ANALYTIC(0.0);

  // Simplified (collapsed) formula based on Newell's paper.
  // This saves about half the calls to g().  There is still
  // quite a bit of redundancy from one cell site to the next,
  // but as this is an initialization-only issue speed shouldn't
  // be too critical.
  OXS_DEMAG_REAL_ANALYTIC sum;

  sum = -1*Oxs_Newell_g(x-dx,y-dy,z-dz);
  sum += -1*Oxs_Newell_g(x-dx,y-dy,z+dz);
  sum += -1*Oxs_Newell_g(x+dx,y-dy,z+dz);
  sum += -1*Oxs_Newell_g(x+dx,y-dy,z-dz);
  sum += -1*Oxs_Newell_g(x+dx,y+dy,z-dz);
  sum += -1*Oxs_Newell_g(x+dx,y+dy,z+dz);
  sum += -1*Oxs_Newell_g(x-dx,y+dy,z+dz);
  sum += -1*Oxs_Newell_g(x-dx,y+dy,z-dz);

  sum +=  2*Oxs_Newell_g(x,y+dy,z-dz);
  sum +=  2*Oxs_Newell_g(x,y+dy,z+dz);
  sum +=  2*Oxs_Newell_g(x,y-dy,z+dz);
  sum +=  2*Oxs_Newell_g(x,y-dy,z-dz);
  sum +=  2*Oxs_Newell_g(x-dx,y-dy,z);
  sum +=  2*Oxs_Newell_g(x-dx,y+dy,z);
  sum +=  2*Oxs_Newell_g(x-dx,y,z-dz);
  sum +=  2*Oxs_Newell_g(x-dx,y,z+dz);
  sum +=  2*Oxs_Newell_g(x+dx,y,z+dz);
  sum +=  2*Oxs_Newell_g(x+dx,y,z-dz);
  sum +=  2*Oxs_Newell_g(x+dx,y-dy,z);
  sum +=  2*Oxs_Newell_g(x+dx,y+dy,z);

  sum += -4*Oxs_Newell_g(x-dx,y,z);
  sum += -4*Oxs_Newell_g(x+dx,y,z);
  sum += -4*Oxs_Newell_g(x,y,z+dz);
  sum += -4*Oxs_Newell_g(x,y,z-dz);
  sum += -4*Oxs_Newell_g(x,y-dy,z);
  sum += -4*Oxs_Newell_g(x,y+dy,z);

  sum +=  8*Oxs_Newell_g(x,y,z);

  return sum/(4*OXS_DEMAG_REAL_ANALYTIC_PI*dx*dy*dz);
}


////////////////////////////////////////////////////////////////////////
//
// This routine computes D6[f] = D2z[D2y[D2x[f]]] across the range the
// specified range, for |(x,y,z)| < Arad, and divides the result by
// 4*Pi*dx*dy*dz.  If f is the precursor function Oxs_Newell_f or
// Oxs_Newell_g then the result is Nxx or Nxy, respectively.
//
// For points |(x,y,z)|>=Arad, fasymp(x,y,z) is computed instead.
//
// Import arr should be pre-sized to dimensions xdim, ydim+2, zdim+2.
// Imports (ioff,joff,koff) represent the offset of the mesh base point,
// so that (x,y,z) runs over
//
//    [ioff*dx,(ioff+xdim)*dx)
//      x [joff*dy,(joff+ydim)*dy)
//      x [koff*dz,(koff+zdim)*dz).
//
// The offset allows this routine to be as one component of the
// computation for periodic boundary conditions (PBC).  For non-PBC,
// call this routine once with offset = 0.
//
// Results are stored in arr, for i across [0,xdim), j across [0,ydim),
// and z across [0,zdim).  The outer planes j=ydim,ydim+1 and
// k=zdim,zdim+1 are used as workspace for the D2y and D2z computations.
//

#if 0  // Debug helper routine
void PrintArr(const Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC>& arr)
{
  const OC_INDEX xdim = arr.GetDim1();
  const OC_INDEX ydim = arr.GetDim2();
  const OC_INDEX zdim = arr.GetDim3();

  for(OC_INDEX k=0;k<zdim;++k) {
    for(OC_INDEX j=0;j<ydim;++j) {
      for(OC_INDEX i=0;i<xdim;++i) {
        fprintf(stderr,"   (%d,%d,%d)=%11.7f",i,j,k,arr(i,j,k));
      }
      fprintf(stderr,"\n");
    }
    fprintf(stderr,"\n");
  }
}
#endif // PrintArr

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
 )
{
  const OC_INDEX xdim = arr.GetDim1();
  const OC_INDEX ydim = arr.GetDim2()-2;
  const OC_INDEX zdim = arr.GetDim3()-2;

  OXS_DEMAG_REAL_ANALYTIC dx = hx;
  OXS_DEMAG_REAL_ANALYTIC dy = hy;
  OXS_DEMAG_REAL_ANALYTIC dz = hz;

  OXS_DEMAG_REAL_ASYMP Aradsq = Arad*Arad;
  if(Arad<0) {
    // Asymptotic interface disabled; set Aradsq big enough
    // so that fasymp won't be called.
    OC_INDEX imax = (-ioff > ioff + xdim ? -ioff : ioff + xdim );
    OXS_DEMAG_REAL_ASYMP xmax
      = (imax+8)*static_cast<OXS_DEMAG_REAL_ASYMP>(dx.Hi());
    OC_INDEX jmax = (-joff > joff + ydim ? -joff : joff + ydim );
    OXS_DEMAG_REAL_ASYMP ymax
      = (jmax+8)*static_cast<OXS_DEMAG_REAL_ASYMP>(dy.Hi());
    OC_INDEX kmax = (-koff > koff + zdim ? -koff : koff + zdim );
    OXS_DEMAG_REAL_ASYMP zmax
      = (kmax+8)*static_cast<OXS_DEMAG_REAL_ASYMP>(dz.Hi());
    Aradsq = xmax*xmax + ymax*ymax + zmax*zmax;
  }

  // Compute f values and D2x.
  // Note: We don't need to compute f for |(x,|y|-dx,|z|-dz)|>=Arad
  {
    OC_INDEX k=0,j=0,i=0;
    OC_INDEX n=0;
    OC_INDEX nstop=OC_INDEX(zdim+2)*OC_INDEX(ydim+2)*OC_INDEX(xdim);

    OXS_DEMAG_REAL_ANALYTIC z = (k+koff-1)*dz;
    // -1 is because we compute on shifted grid and then collapse
    OXS_DEMAG_REAL_ANALYTIC y = (j+joff-1)*dy;
    // -1 is because we compute on shifted grid and then collapse

    // Compute bound to include 3x3x3 stencil border.
    OXS_DEMAG_REAL_ASYMP zshift
      = fabs(static_cast<OXS_DEMAG_REAL_ASYMP>(z.Hi()))
      - static_cast<OXS_DEMAG_REAL_ASYMP>(dz.Hi());
    OXS_DEMAG_REAL_ASYMP yshift
      = fabs(static_cast<OXS_DEMAG_REAL_ASYMP>(y.Hi()))
      - static_cast<OXS_DEMAG_REAL_ASYMP>(dy.Hi());
    OXS_DEMAG_REAL_ASYMP iBound
      = sqrt(Aradsq - yshift*yshift- zshift*zshift)
      /static_cast<OXS_DEMAG_REAL_ASYMP>(dx.Hi());

    OC_INDEX imax = (xdim <= nstop-n ? xdim : nstop-n);
    OC_INDEX i1 = static_cast<OC_INDEX>(floor(-iBound - ioff))+1;
    if(i1>imax)    i1 = imax;
    OC_INDEX i2 = static_cast<OC_INDEX>(ceil(iBound - ioff));
    if(i2>imax)    i2 = imax;
    
    while(n<nstop) {
      n += imax - i; // Value of n after i increments
      for(;i<i1;++i) {
        // Case |(x,y,z)| - shift >= Arad
        arr(i,j,k) = 0.0;  // Proper value overlaid in D2z stage
      }
      OXS_DEMAG_REAL_ANALYTIC x = (ioff+i)*dx;
      OXS_DEMAG_REAL_ANALYTIC a = f(x-dx,y,z);
      OXS_DEMAG_REAL_ANALYTIC b = f(x,y,z);
      for(;i<i2;++i) {
        // Case |(x,y,z)| - shift < Arad
        x += dx;
        OXS_DEMAG_REAL_ANALYTIC c = f(x,y,z);
        arr(i,j,k) = (b-a)+(b-c);  // -f(x-dx) + 2*f(x) - f(x+dx)
        a = b;  b = c;
      }
      for(;i<imax;++i) {
        // Case |(x,y,z)| - shift >= Arad
        arr(i,j,k) = 0.0;
      }
      if(n >= nstop) break;  // All done!
      if(++j >= ydim) {
        ++k; j=0;
        z = (k+koff-1)*dz;
        // -1 is because we compute on shifted grid and then collapse
        zshift = fabs(static_cast<OXS_DEMAG_REAL_ASYMP>(z.Hi()))
          -static_cast<OXS_DEMAG_REAL_ASYMP>(dz.Hi());
      }
      y = (j+joff-1)*dy;
      // -1 is because we compute on shifted grid and then collapse
      yshift = fabs(static_cast<OXS_DEMAG_REAL_ASYMP>(y.Hi()))
        -static_cast<OXS_DEMAG_REAL_ASYMP>(dy.Hi());

      imax = (xdim <= nstop-n ? xdim : nstop-n);
      iBound = sqrt(Aradsq - yshift*yshift - zshift*zshift)
        /static_cast<OXS_DEMAG_REAL_ASYMP>(dx.Hi());
      i1 = static_cast<OC_INDEX>(floor(-iBound - ioff))+1;
      if(i1>imax)    i1 = imax;
      i2 = static_cast<OC_INDEX>(ceil(iBound - ioff));
      if(i2>imax)    i2 = imax;
      i=0;
    }
  }

  // Compute D2y.  Here we compute D2y for all (i,j,k), but we could
  // skip values outside Arad, provided we leave a margin for the D2z
  // stencil.
  for(OC_INDEX k=0;k<zdim+2;++k) {
    for(OC_INDEX j=0;j<ydim;++j) {
      for(OC_INDEX i=0;i<xdim;++i) {
        // -f(y-dy) + 2*f(y) - f(y+dy)
        arr(i,j,k) = arr(i,j+1,k) - arr(i,j,k);
        arr(i,j,k) += (arr(i,j+1,k) - arr(i,j+2,k));
      }
    }
  }

  // Compute D2z and do clean-up for |(x,y,z)|>=Arad.
  // NB: The analytic computations need to be divided by 4*PI*dx*dy*dz
  //     to get demag tensor values Nxx and etc.; the asymptotic
  //     routines return the properly scaled value.
  const OXS_DEMAG_REAL_ANALYTIC analytic_scaling
    = 1.0/(4*Xp_DoubleDouble::DD_PI*dx*dy*dz);

  for(OC_INDEX k=0;k<zdim;++k) {
    OXS_DEMAG_REAL_ASYMP z
      = (koff+k)*static_cast<OXS_DEMAG_REAL_ASYMP>(dz.Hi());
    for(OC_INDEX j=0;j<ydim;++j) {
      OXS_DEMAG_REAL_ASYMP y
        = (joff+j)*static_cast<OXS_DEMAG_REAL_ASYMP>(dy.Hi());

      const OXS_DEMAG_REAL_ASYMP iBound
        = sqrt(Aradsq - y*y - z*z)/static_cast<OXS_DEMAG_REAL_ASYMP>(dx.Hi());
      OC_INDEX i1 = static_cast<OC_INDEX>(floor(-iBound - ioff))+1;
      if(i1>xdim) i1 = xdim;
      OC_INDEX i2 = static_cast<OC_INDEX>(ceil(iBound - ioff));
      if(i2>xdim) i2 = xdim;

      OC_INDEX i=0;
      for(;i<i1;++i) {
        // Case |(x,y,z)|>=Arad
        OXS_DEMAG_REAL_ASYMP x
          = (ioff+i)*static_cast<OXS_DEMAG_REAL_ASYMP>(dx.Hi());
        arr(i,j,k)
          = static_cast<OXS_DEMAG_REAL_ANALYTIC>(fasymp.Asymptotic(x,y,z));
      }
      for(;i<i2;++i) {
        // Case |(x,y,z)|<Arad
        // If |(x,y,z)|<Arad, compute -f(z-dz) + 2*f(z) - f(z+dz)
        arr(i,j,k) = arr(i,j,k+1) - arr(i,j,k);
        arr(i,j,k) += (arr(i,j,k+1) - arr(i,j,k+2));
        arr(i,j,k) *= analytic_scaling;
      }
      for(;i<xdim;++i) {
        // Case |(x,y,z)|>=Arad
        OXS_DEMAG_REAL_ASYMP x
          = (ioff+i)*static_cast<OXS_DEMAG_REAL_ASYMP>(dx.Hi());
        arr(i,j,k)
          = static_cast<OXS_DEMAG_REAL_ANALYTIC>(fasymp.Asymptotic(x,y,z));
      }
    }
  }
}

// Helper function for filling arrays of demag tensor coefficients with
// periodic boundaries.  NOTE: This routine not intended for in-place
// use, i.e., the export val should not be an element of workspace.
void Oxs_FoldWorkspace
(Oxs_3DArray<OXS_DEMAG_REAL_ANALYTIC>& workspace,
 OC_INDEX wdimx,OC_INDEX wdimy,OC_INDEX wdimz,
 OC_INDEX rdimx,OC_INDEX rdimy,OC_INDEX rdimz,
 int xperiodic,int yperiodic,int zperiodic,
 int xodd, int yodd, int zodd,
 OXS_DEMAG_REAL_ANALYTIC& val,
 OC_INDEX i,OC_INDEX j,OC_INDEX k)
{
  OXS_DEMAG_REAL_ANALYTIC sum = 0.0;
  if(xperiodic) {
    // For a given i,j,k in the base window, reflect the computed values
    // across the yz-plane.
    //    Alternatively, we could sum all positive images into the root
    // window, and then do a single add about the midline to fold in the
    // negative images.  However, doing that would require separate
    // looping across the root window before the A-index selection loop.
    for(OC_INDEX i2=i+rdimx;i2<wdimx;i2+=rdimx) sum += workspace(i2,j,k);
    OXS_DEMAG_REAL_ANALYTIC mirror = 0.0;
    for(OC_INDEX i2=rdimx-i;i2<wdimx;i2+=rdimx) mirror += workspace(i2,j,k);
    if(xodd) sum -= mirror;  // Odd symmetry
    else     sum += mirror;  // Even symmetry
  }
  if(yperiodic) {
    for(OC_INDEX j2=j+rdimy;j2<wdimy;j2+=rdimy) sum += workspace(i,j2,k);
    OXS_DEMAG_REAL_ANALYTIC mirror = 0.0;
    for(OC_INDEX j2=rdimy-j;j2<wdimy;j2+=rdimy) mirror += workspace(i,j2,k);
    if(yodd) sum -= mirror;  // Odd symmetry
    else     sum += mirror;  // Even symmetry
  }
  if(zperiodic) {
    for(OC_INDEX k2=k+rdimz;k2<wdimz;k2+=rdimz) sum += workspace(i,j,k2);
    OXS_DEMAG_REAL_ANALYTIC mirror = 0.0;
    for(OC_INDEX k2=rdimz-k;k2<wdimz;k2+=rdimz) mirror += workspace(i,j,k2);
    if(zodd) sum -= mirror;  // Odd symmetry
    else     sum += mirror;  // Even symmetry
  }
  val += sum;
}

// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// TODO: I think symmetry considerations suggest that the window sum
//       should run across [-Wcount,Wcount), so that 0 is in the middle
//       of the window range; as it is now the the sum runs across
//       [-Wcount,Wcount], which makes the middle W/2.
//
//    Q: What happens if the asymptotic cutoff Arad is smaller than W?
//       Should dimx/y/z be limited to asymptotic box?  Or should the
//       outside values be filled instead with asymptotic values?  What
//       about case where Arad is slightly larger than an integral
//       number of W.  Should we sum up to the smaller number and then
//       add in a combination of analytic values and asymptotic ones?  A
//       combination platter can be handled more easily if we sum D6
//       values instead of D2 values, although that would require
//       additional workspace.
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

////////////////////////////////////////////////////////////////////////
// Asymptotic approximations to N
// See NOTES IV: 26-Mar-2007, p112-113
//               27-Mar-2007, p116-118
//     NOTES  V:  3-May-2011, p198-199
//     NOTES VI: 12-May-2011, p  2-5
//               12-Apr-2012, p 34-37
//                4-Jun-2012, p 42-44
//                6-Jun-2012, p 48-61
//               27-Jul-2012, p 64-67

Oxs_DemagNxxAsymptoticBase::Oxs_DemagNxxAsymptoticBase
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 int in_max_order)
  : max_order(in_max_order)
{
  self_demag           // Special case
    = static_cast<OXS_DEMAG_REAL_ASYMP>(Oxs_SelfDemagNx(dx,dy,dz).Hi());

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
Oxs_DemagNxxAsymptoticBase::Asymptotic
(const OxsDemagNabData& ptdata) const
{ // Asymptotic approximation to Nxx term.

  if(ptdata.iR2<=0.0) {
    // Asymptotic expansion doesn't apply for R==0.  Fall back
    // to self-demag calculation.
    return self_demag;
  }

  // term3 provides 5th order error,
  // term5 provides 7th order error,
  // etc.

  const OXS_DEMAG_REAL_ASYMP& tx2 = ptdata.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2 = ptdata.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2 = ptdata.tz2;

  const OXS_DEMAG_REAL_ASYMP tz4 = tz2*tz2;
  const OXS_DEMAG_REAL_ASYMP tz6 = tz4*tz2;
  const OXS_DEMAG_REAL_ASYMP term3 = (2*tx2 - ty2 - tz2)*lead_weight;
  OXS_DEMAG_REAL_ASYMP term5 = 0.0;
  OXS_DEMAG_REAL_ASYMP term7 = 0.0;
  OXS_DEMAG_REAL_ASYMP term9 = 0.0;
  if(cubic_cell) {
    if(max_order>7) {
      const OXS_DEMAG_REAL_ASYMP ty4 = ty2*ty2;
      term7 = ((b1*tx2
                + (b2*ty2 + b3*tz2))*tx2
               + (b4*ty4 + b6*tz4))*tx2
        + b7*ty4*ty2 + b10*tz6;
    }
  } else {
    if(max_order>5) {
      term5 = (a1*tx2 + (a2*ty2 + a3*tz2))*tx2
        + (a4*ty2 + a5*tz2)*ty2 + a6*tz4;
      if(max_order>7) {
        term7 = ((b1*tx2
                  + (b2*ty2 + b3*tz2))*tx2
                 + ((b4*ty2 + b5*tz2)*ty2 + b6*tz4))*tx2
          + ((b7*ty2 + b8*tz2)*ty2 + b9*tz4)*ty2
          + b10*tz6;
      }
    }
  }
  if(max_order>9) {
    term9 =  (((c1*tx2
                +  (c2*ty2 +  c3*tz2))*tx2
               +  ((c4*ty2 +  c5*tz2)*ty2 +  c6*tz4))*tx2
              +  (  ((c7*ty2 +  c8*tz2)*ty2 +  c9*tz4)*ty2 + c10*tz6  ))*tx2
      +  (((c11*ty2 + c12*tz2)*ty2 + c13*tz4)*ty2 + c14*tz6)*ty2
      + c15*tz4*tz4;
  }

  const OXS_DEMAG_REAL_ASYMP Nxx = (term9 + term7 + term5 + term3)*ptdata.iR;
  // Error should be of order 1/R^11 if all terms are active

  return Nxx;
}


#if OXS_DEMAG_ASYMP_USE_SSE
OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxxAsymptoticBase::AsymptoticPair
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
    return Asymptotic(ptA) + Asymptotic(ptB);
  }

  const Oc_Duet tx2(ptA.tx2,ptB.tx2);
  const Oc_Duet ty2(ptA.ty2,ptB.ty2);
  const Oc_Duet tz2(ptA.tz2,ptB.tz2);

  const Oc_Duet tz4 = tz2*tz2;
  const Oc_Duet tz6 = tz4*tz2;

  const Oc_Duet term3 = lead_weight*(2*tx2 - ty2 - tz2);

  Oc_Duet Nxx(0.0);
  Oc_Duet term7(0.0);
  if(cubic_cell) {
    if(max_order>7) {
      const Oc_Duet ty4 = ty2*ty2;
      term7 = b7*ty4*ty2
        + tx2*(tx2*(b1*tx2 + b2*ty2 + b3*tz2) + b4*ty4 + b6*tz4)
        + b10*tz6;
    }
  } else {
    if(max_order>5) {
      const Oc_Duet term5
        = a6*tz4
        + ty2*(a4*ty2 + a5*tz2)
        + tx2*(a1*tx2 + a2*ty2 + a3*tz2);
      Nxx = term5;
      if(max_order>7) {
        term7 = ty2*(ty2*(b7*ty2 + b8*tz2) + b9*tz4)
          + tx2*(tx2*(b1*tx2 + b2*ty2 + b3*tz2) + ty2*(b4*ty2 + b5*tz2)
                 + b6*tz4)
          + b10*tz6;
      }
    }
  }
  Oc_Duet term9(0.0);
  if(max_order>9) {
    term9 = c15*tz4*tz4
      + ty2*(ty2*(ty2*(c11*ty2 + c12*tz2) + c13*tz4) + c14*tz6)
      + tx2*(tx2*(tx2*(c1*tx2 +  c2*ty2 +  c3*tz2)
                  +  ty2*(c4*ty2 +  c5*tz2) +  c6*tz4)
             + ty2*(ty2*(c7*ty2 +  c8*tz2) +  c9*tz4) + c10*tz6);
  }
  Nxx += (term7 + term9);
  Nxx += term3;
  Nxx *= Oc_Duet(ptA.iR,ptB.iR);
  // Error should be of order 1/R^11 if all terms are active

  return Nxx.GetA() + Nxx.GetB();
}
#endif // OXS_DEMAG_ASYMP_USE_SSE

OC_INDEX
Oxs_DemagAsymptotic::FindRefinementCount
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 OXS_DEMAG_REAL_ASYMP h,
 OC_INDEX& enx,OC_INDEX& eny,OC_INDEX& enz)
{
  assert(dx>0. && dy>0. && dz>0. && h>0.0);

#if STANDALONE
  const static OC_INDEX OC_INDEX_MAX_CUBEROOT = static_cast<OC_INDEX>
    (floor(pow(static_cast<double>(std::numeric_limits<OC_INDEX>::max()),
               1./3.)));
#endif
  
  const OXS_DEMAG_REAL_ASYMP count_limit
    = static_cast<OXS_DEMAG_REAL_ASYMP>(OC_INDEX_MAX_CUBEROOT);
  
  OXS_DEMAG_REAL_ASYMP nx = ceil(dx/h);
  OXS_DEMAG_REAL_ASYMP ny = ceil(dy/h);
  OXS_DEMAG_REAL_ASYMP nz = ceil(dz/h);

  if(nx>count_limit || ny>count_limit || nz>count_limit) {
#if STANDALONE
    static int warncount=1;
    if(warncount>0) {
      fprintf(stderr,"*** WARNING: "
              "Oxs_DemagAsymptotic::FindRefinementCount index overflow;"
              " refinement reduced but errors may be larger than"
              " requested. ***\n");
      --warncount;
    }
#else // !STANDALONE
    static Oxs_WarningMessage badrefinement(2);
    badrefinement.Send(revision_info,OC_STRINGIFY(__LINE__),
              "Refinement index overflow; refinement reduced but errors"
              " may be larger than requested.");
#endif // STANDALONE
  }

  // There is no point in returning extremely large values, as very
  // large values won't fit into an OC_INDEX, and couldn't be indexed
  // anyway.  Moreover, the compute time to evaluate such a refinement
  // would be prohibitive.  It may also be the case that the knot in
  // question is outside the range of the simulation, so it won't ever
  // been used, regardless.
  if(nx>count_limit) nx = count_limit;
  enx = static_cast<OC_INDEX>(nx);
  if(ny>count_limit) ny = count_limit;
  eny = static_cast<OC_INDEX>(ny);
  if(nz>count_limit) nz = count_limit;
  enz = static_cast<OC_INDEX>(nz);

  // Safety check against rounding errors
  if(enx>OC_INDEX_MAX_CUBEROOT) enx = OC_INDEX_MAX_CUBEROOT;
  if(eny>OC_INDEX_MAX_CUBEROOT) eny = OC_INDEX_MAX_CUBEROOT;
  if(enz>OC_INDEX_MAX_CUBEROOT) enz = OC_INDEX_MAX_CUBEROOT;

  return enx*eny*enz;
  // NB: Product guaranteeed <= OC_INDEX_MAX
}

/*
 * Oxs_DemagAsymptotic::FindBestRefinement
 *
 * Given rectangular brick R of dimensions dx x dy x dz, and count
 * request nrequest, this routine solves the problem of finding an exact
 * uniform subdivision of R into rectangular subcells with dimensions hx
 * x hy x hz, with minimal max(hx,hy,hz), subject to the constraint that
 * the the total number of subcells is no more than nrequest.  This
 * implementation using a "pinsheet" approach, where the axis division
 * counts nx, ny, nz are set all initially to 1 and then are gradually
 * raised to reduce max h subject to the constraint nx*ny*nz<=nrequest.
 * (Here hx=dx/nx, hy=dy/ny, hz=dz/nz.)
 *
 * Note: The implementation here is for three dimensional R, but the
 * method extends easily to an arbitrary number of dimensions.
 */
#ifdef _MSC_VER  // Visual C++
# pragma warning(push)
# pragma warning(disable : 4244)   // Ignore loss of data warnings
                   /// for conversions between double and OC_INDEX.
#endif // _MSC_VER
void Oxs_DemagAsymptotic::FindBestRefinement
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 OC_INDEX nrequest,
 OC_INDEX& nx_out,OC_INDEX& ny_out,OC_INDEX& nz_out)
{
  // Compute ratios directly.  If any two dimensions are the same then
  // drat will be exactly 1.0 which should help protect against rounding
  // errors.
  OXS_DEMAG_REAL_ASYMP dratxy=dx/dy, dratxz=dx/dz;
  OXS_DEMAG_REAL_ASYMP dratyx=dy/dx, dratyz=dy/dz;
  OXS_DEMAG_REAL_ASYMP dratzx=dz/dx, dratzy=dz/dy;

  OC_INDEX nx=1,ny=1,nz=1;
  while(1) {
    double ntst=0;
    if(nx*dy <= ny*dx && nx*dz <= nz*dx) { // nx/dx smallest
      if(ny*dz <= nz*dy) ntst = OC_INDEX(floor(double(ny)*dratxy+1.));
      else               ntst = OC_INDEX(floor(double(nz)*dratxz+1.));
      if(ntst*ny*nz<nrequest) nx=ntst;
      else { nx=(nrequest/(ny*nz)); break; }
    } else if(ny*dx <= nx*dy && ny*dz <= nz*dy) { // ny/dy smallest
      if(nx*dz <= nz*dx) ntst = OC_INDEX(floor(double(nx)*dratyx+1.));
      else               ntst = OC_INDEX(floor(double(nz)*dratyz+1.));
      if(nx*ntst*nz<=nrequest) ny=ntst;
      else { ny=(nrequest/(nx*nz)); break; }
    } else { // nz/dz is smallest
      if(nx*dy <= ny*dx) ntst = OC_INDEX(floor(double(nx)*dratzx+1.));
      else               ntst = OC_INDEX(floor(double(ny)*dratzy+1.));
      if(nx*ny*ntst<=nrequest) nz=ntst;
      else { nz=(nrequest/(nx*ny)); break; }
    }
  }
  assert(nx*ny*nz<=nrequest);
  // Deflate any ni as much as possible without raising hmax.
  if(nx*dy <= ny*dx && nx*dz <= nz*dx) { // nx/dx smallest
    OC_INDEX tny = OC_INDEX(ceil(nx*dratyx)); if(tny<ny) ny = tny;
    OC_INDEX tnz = OC_INDEX(ceil(nx*dratzx)); if(tnz<nz) nz = tnz;
  } else if(ny*dx <= nx*dy && ny*dz <= nz*dy) { // ny/dy smallest
    OC_INDEX tnx = OC_INDEX(ceil(ny*dratxy)); if(tnx<nx) nx = tnx;
    OC_INDEX tnz = OC_INDEX(ceil(ny*dratzy)); if(tnz<nz) nz = tnz;
  } else { // nz/dz is smallest
    OC_INDEX tnx = OC_INDEX(ceil(nz*dratxz)); if(tnx<nx) nx = tnx;
    OC_INDEX tny = OC_INDEX(ceil(nz*dratyz)); if(tny<ny) ny = tny;
  }
  // Q: Could rounding error cause the ceil() calls above to
  //    return the wrong value?  The if-tests should protect
  //    against the worse failures.
  nx_out = nx;
  ny_out = ny;
  nz_out = nz;
  assert(nx_out*ny_out*nz_out<=nrequest);
}
#ifdef _MSC_VER  // Visual C++
# pragma warning(pop)
#endif // _MSC_VER

/*
 * Oxs_DemagAsymptotic::FindNeededRefinement
 *
 * Given a rectangular brick of dimensions dx x dy x dz, offset R,
 * allowed relative error allowed_error, and asymptotic method order,
 * computes the refinement (if any) needed to match the error
 * constraint.  The return value is the number of cells in the
 * refinement.  This routine uses the error estimate
 *
 *   Rel Error ~ (8/5) (h/R)^(n-3) sqrt(M)
 *
 * where h is max of refined dimensions, R is offset, n is the
 * asymptotic method order, and M is the number of cells in the
 * refinement.  This code assumes M = (h0/h)^3, where h is the maximum
 * of the original dimensions dx, dy, and dz.  An improvement to this
 * code would tighten the M estimate.
 * 
 */
OC_INDEX
Oxs_DemagAsymptotic::FindNeededRefinement
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 OXS_DEMAG_REAL_ASYMP R,
 OXS_DEMAG_REAL_ASYMP allowed_error,
 int order,  // method order
 OC_INDEX& nx_out,OC_INDEX& ny_out,OC_INDEX& nz_out)
{
  // First compute error without refinement
  OXS_DEMAG_REAL_ASYMP h0 = (dx>dy ? (dx>dz ? dx : dz) : (dy>dz ? dy : dz));
  OXS_DEMAG_REAL_ASYMP E0 = 1.6*pow(h0/R,order-3);

  // If no refinement meets error bound, done.
  if(E0 <= allowed_error) {
    nx_out = ny_out = nz_out = 1;
    return 1;
  }

  // Else compute refinement relative to original
  OXS_DEMAG_REAL_ASYMP hmax = h0*pow(allowed_error/E0,1.0/(order - 4.5));
  nx_out = static_cast<OC_INDEX>(ceil(dx/hmax));
  ny_out = static_cast<OC_INDEX>(ceil(dy/hmax));
  nz_out = static_cast<OC_INDEX>(ceil(dz/hmax));

  return nx_out*ny_out*nz_out;
}

OXS_DEMAG_REAL_ASYMP
Oxs_DemagAsymptotic::FindKnotSchedule
(OXS_DEMAG_REAL_ASYMP dx,    // Import
 OXS_DEMAG_REAL_ASYMP dy,    // Import
 OXS_DEMAG_REAL_ASYMP dz,    // Import
 OXS_DEMAG_REAL_ASYMP maxerror, // Desired max error; Import
 int asymptotic_order,       // Max asymptoric method order
 std::vector<Oxs_DemagAsymptotic::DemagKnot>& knots) // Export
{ // Using built-in assumptions about analytic, asymptotic, and refined
  // asymptotic method errors and speed, builds an ordered list of knots
  // yielding an optimal demag schedule.  The return value is the
  // smallest (i.e., last) schedule radius --- smaller than that radius
  // the analytic method should be used.

  // Model assumptions:
  //   The analytic method is assumed to be "analytic_speed_factor"
  //   slower than the base asymptotic method using order
  //   "asymptotic_order".  It is further assumed that the speed of the
  //   asymptotic method using a refined subgrid with N elements is N
  //   times slower than the base asymptotic method.  So in this model a
  //   refined asymptotic model with analytic_speed_factor subcells runs
  //   at the same speed as the analytic method.  (This is a simplistic
  //   assumption that could be improved.)
  //
  //   It is assumed that the (absolute) error in the analytic method is
  //
  //      Error_analytic = eps*R^3/V
  //
  //   where eps is the machine epsilon for the floating point type, R
  //   is the distance between the cells, and V is the volume of the
  //   cell.  Note that this is a dimensionless quantity.  This table
  //   lists the machine epsilon for several floating point types:
  //
  //         Type           width       eps
  //         float         4 bytes     2^-23   ~  1 x 10^-7
  //         double        8 bytes     2^-52   ~  2 x 10^-16
  //       long double  10-16 bytes    2^-63   ~  1 x 10^-19
  //       double-double  16 bytes     2^-106  ~  1 x 10^-32
  //
  //   (This is some ambiguity in the literature on the definition of
  //   machine epsilon; some source define machine epsilon to be half
  //   the values given in this table.  The formula for Error_analytic
  //   assumes the machine epsilon definition matching the values in
  //   this table, which matches the ISO C and C++ definition as
  //   implemented in std::numeric_limits<T>::epsilon().)
  //
  //   It is assumed that the (absolute) error in the base asymptotic
  //   method is
  //
  //      Error_asymptotic = V*R^2/(5*(R^2-hmax^2) * hmax^(n-3)/R^n
  //
  //   where V is the cell volume (h1*h2*h3), hmax is the max cell edge
  //   length (max(h1,h2,h3)), R is the distance between the cells, and
  //   n is the method order.  Note that this is a dimensionless
  //   quantity.  (See NOTES VIII, 3-July-2019, p. 61.)
  //
  //   For refined asymptotic, if the base cell is divided into M
  //   subcells, then the error is assumed to be sqrt(M) times the error
  //   computed using the preceding formula using the refined cell edge
  //   dimensions.
  //
  //   For large R, the demag tensor values converge to the dipole
  //   approximation, e.g.
  //
  //      Nxx ~ -V/(4*pi)*(2x^2-y^2-z^2)/R^5
  //
  //      Nxy ~ -V/(4*pi)*(3*x*y)/R^5
  //
  //   which decay like V/R^3.  A practical relative error estimate is
  //   therefore absolute error above multiplied by say 2*pi*R^3/V.
  //   I say "practical" because at some points the demag tensor goes
  //   through zero, so relative error is a not-so-useful quantity.
  //
  //   Therefore, the requested "error" imported is interpreted with
  //   respect to the practical relative error, namely
  //   
  //      PracRelErr_analytic = 8*eps*R^6/V^2
  //
  //      PracRelErr_asymptotic = 8*R^2/(5*(R^2-hmax^2) * (hmax/R)^(n-3)
  //
  //                            ~ (8/5)(hmax/R)^(n-3)  if R>>hmax
  //
  //      PracRelErr_refined_asymptotic(M) = sqrt(M)*PracRelErr_asymptotic
  //
  //   Here the 2*pi factor has been rounded up to 8 to give a little
  //   extra cushion.

  const OXS_DEMAG_REAL_ASYMP asymptotic_machine_eps
    = std::numeric_limits<OXS_DEMAG_REAL_ASYMP>::epsilon();
  if(maxerror<asymptotic_machine_eps) {
    // Can't actually compute errors smaller than machine epsilon for
    // the floating point type.
    maxerror =  asymptotic_machine_eps;
  }

#if STANDALONE
  const OXS_DEMAG_REAL_ASYMP analytic_machine_eps
    = std::numeric_limits<XP_DDFLOAT_TYPE>::epsilon();
#else
  const OXS_DEMAG_REAL_ASYMP analytic_machine_eps = XP_DD_EPS;
#endif
  
  const OXS_DEMAG_REAL_ASYMP Vol = dx*dy*dz;
  const OXS_DEMAG_REAL_ASYMP hmax
    = (dx>dy ? (dx>dz ? dx : dz) : (dy>dz ? dy : dz));

  const OXS_DEMAG_REAL_ASYMP analytic_speed_factor = 60;
  
  assert(analytic_speed_factor>4); // Else asymptotic refinements for
                                   // speed don't make sense.
  assert(asymptotic_order>=5);  // Some code below assumes (order-4.5)>0

  const OXS_DEMAG_REAL_ASYMP R_analytic_max_happy = 32*pow(Vol,1./3.);
  // For speed considerations, we'd like to not compute analytic
  // demag bigger than this.

  const OXS_DEMAG_REAL_ASYMP R_asymptotic_min_rule = 15*hmax;
  // As a rule, don't trust asymptotics below this radius.  However,
  // currently the code below ignores this rule when computing
  // refinements.

  // Compute maximum R that analytic method can be used.
  const OXS_DEMAG_REAL_ASYMP R_analytic_max
    = pow(maxerror*Vol*Vol/(8*analytic_machine_eps),1./6.);

  // Compute minimum R that base asymptotic method can be used.  We drop
  // the R^2/(R^2-hmax^2) here because in practice this
  // appears to be a pretty minor adjustment.  If necessary, one could
  // add a couple of Newton steps to refine the estimate.  Instead,
  // we add the constraint R >= 2*h
  OXS_DEMAG_REAL_ASYMP R_asymptotic_min
    = hmax*pow(8/(5*maxerror),1./(asymptotic_order-3));
  if(R_asymptotic_min<R_asymptotic_min_rule ) {
    R_asymptotic_min=R_asymptotic_min_rule;
  }
  if(R_asymptotic_min<2*hmax) {
    R_asymptotic_min = 2*hmax; // Stay away from pole.
  }

  // Place the outermost knot at R_asymptotic_min
  knots.clear();  // Make sure import knot vector is empty
#if DUMPKNOTS
  std::ostringstream sbuf;
  sbuf << "\n*** Knot start: Geom = < " << dx << ", " << dy << ", " << dz << " >; hmax= " << hmax << " ***\n"; /**/
  double da = pow(dx*dy*dz,1./3.);
#endif // DUMPKNOTS
  knots.push_back(DemagKnot(R_asymptotic_min,1,1,1,asymptotic_order));

  // If R_asymptotic_min is smaller than both R_analytic_max and
  // R_analytic_max_happy then declare victory.
  if(R_asymptotic_min < R_analytic_max
     && R_asymptotic_min < R_analytic_max_happy) {
#if DUMPKNOTS
    sbuf << "*** A Knot: ";
    for(auto&& dk : knots) {
      sbuf << " (" << dk.r/da << " : " << dk.nx << ", " << dk.ny << ", "<< dk.nz << ")";
    }
    sbuf << " ***\n";
    std::cerr << sbuf.str();
#endif // DUMPKNOTS
    return R_asymptotic_min;
  }
  // Otherwise asymptotic refinements are needed.

  if(R_asymptotic_min <= R_analytic_max) {
    // If R_asymptotic_min is smaller than R_analytic_max (but larger
    // than R_analytic_max_happy), then error requirements are met by
    // the one knot already added, but we can speed things up a bit by
    // including a refinement.  First question: What size hmax_refined
    // is needed to reach desired error at R_analytic_max_happy, and
    // what would be the number of cells in this refinement?  For this
    // calculation we assume subdivision count M ~ (hmax/hrefinemax)^3.
    OXS_DEMAG_REAL_ASYMP hrefinemax
      = hmax*pow(R_analytic_max_happy/R_asymptotic_min,
                 (2*asymptotic_order-6.)/(2*asymptotic_order-9.));
    if(hrefinemax>0.5*R_analytic_max_happy) {
      hrefinemax = 0.5*R_analytic_max_happy; // Stay away from pole.
    }
    OC_INDEX rnx,rny,rnz;
    OC_INDEX hrefinecount
      = FindRefinementCount(dx,dy,dz,hrefinemax,rnx,rny,rnz);
    
    // If hrefinecount will significantly speed things up, then do so
    // and done.
    if(OXS_DEMAG_REAL_ASYMP(hrefinecount)<0.5*analytic_speed_factor) {
      knots.push_back(DemagKnot(R_analytic_max_happy,
                                rnx,rny,rnz,asymptotic_order));
#if DUMPKNOTS
    sbuf << "*** B Knot: ";
    for(auto&& dk : knots) {
      sbuf << " (" << dk.r/da << " : " << dk.nx << ", " << dk.ny << ", "<< dk.nz << ")";
    }
    sbuf << " ***\n";
    std::cerr << sbuf.str();
#endif // DUMPKNOTS
      return R_analytic_max_happy;
    }
    
    // Otherwise, find a reasonable middle point between
    // R_analytic_max_happy and R_asymptotic_min to stop refinement
    // use.
    FindBestRefinement(dx,dy,dz,
                       static_cast<OC_INDEX>(0.5*analytic_speed_factor),
                       rnx,rny,rnz);
    double rncount
      = double(rnx)*double(rny)*double(rnz);
    OXS_DEMAG_REAL_ASYMP rhx = dx/rnx;
    OXS_DEMAG_REAL_ASYMP rhy = dy/rny;
    OXS_DEMAG_REAL_ASYMP rhz = dz/rnz;
    OXS_DEMAG_REAL_ASYMP rhmax
      = (rhx>rhy ? (rhx>rhz ? rhx : rhz) : (rhy>rhz ? rhy : rhz));
    OXS_DEMAG_REAL_ASYMP Rmiddle
      = R_asymptotic_min*(rhmax/hmax)*pow(rncount,0.5/(asymptotic_order-3));
    if(Rmiddle<2*rhmax) {
      Rmiddle = 2*rhmax;  // Stay away from pole.
    }
    if(Rmiddle>R_asymptotic_min) {
#if DUMPKNOTS
    sbuf << "*** C Knot: ";
    for(auto&& dk : knots) {
      sbuf << " (" << dk.r/da << " : " << dk.nx << ", " << dk.ny << ", "<< dk.nz << ")";
    }
    sbuf << " ***\n";
    std::cerr << sbuf.str();
#endif // DUMPKNOTS
      return R_asymptotic_min; // Use only knot already added.
    }
    if(Rmiddle<R_analytic_max_happy) Rmiddle = R_analytic_max_happy;
    knots.push_back(DemagKnot(Rmiddle,rnx,rny,rnz,asymptotic_order));
#if DUMPKNOTS
    sbuf << "*** D Knot: ";
    for(auto&& dk : knots) {
      sbuf << " (" << dk.r/da << " : " << dk.nx << ", " << dk.ny << ", "<< dk.nz << ")";
    }
    sbuf << " ***\n";
    std::cerr << sbuf.str();
#endif // DUMPKNOTS
    return Rmiddle;
  }

  // Otherwise, R_asymptotic_min > R_analytic_max.  This is a slow,
  // unhappy place.  Add (potentially) two knots.  One below R_asymptotic_min
  // that's refined for speed, and then a second below that to meet the
  // error threshold from R_analytic_max

  // First, compute refinement necessary to match R_analytic_max.
  OXS_DEMAG_REAL_ASYMP hrefinemax
    = hmax*pow(R_analytic_max/R_asymptotic_min,
               (2*asymptotic_order-6.)/(2*asymptotic_order-9.));
  if(hrefinemax>0.5*R_analytic_max) {
    hrefinemax = 0.5*R_analytic_max; // Stay away from pole.
  }
  OC_INDEX rnx,rny,rnz;
  OC_INDEX needed_refinecount
    = FindRefinementCount(dx,dy,dz,hrefinemax,rnx,rny,rnz);
  DemagKnot needed_knot(R_analytic_max,
                        rnx,rny,rnz,asymptotic_order);
  // We should check that this refinement meets the error constraint,
  // but the place where this fails is too scary to go to.

  // OTOH, if it happens that needed_knot is not too slow, then add just
  // this one and return.
  if(OXS_DEMAG_REAL_ASYMP(needed_refinecount)<0.5*analytic_speed_factor) {
    knots.push_back(needed_knot);
#if DUMPKNOTS
    sbuf << "*** E Knot: ";
    for(auto&& dk : knots) {
      sbuf << " (" << dk.r/da << " : " << dk.nx << ", " << dk.ny << ", "<< dk.nz << ")";
    }
    sbuf << " ***\n";
    std::cerr << sbuf.str();
#endif // DUMPKNOTS
    return R_analytic_max;
  }

  // Otherwise, compute a faster refinement to patch in-between.
  FindBestRefinement(dx,dy,dz,
                     static_cast<OC_INDEX>(0.25*analytic_speed_factor),
                     rnx,rny,rnz);
  double rncount = double(rnx)*double(rny)*double(rnz);
  OXS_DEMAG_REAL_ASYMP rhx = dx/rnx;
  OXS_DEMAG_REAL_ASYMP rhy = dy/rny;
  OXS_DEMAG_REAL_ASYMP rhz = dz/rnz;
  OXS_DEMAG_REAL_ASYMP rhmax
    = (rhx>rhy ? (rhx>rhz ? rhx : rhz) : (rhy>rhz ? rhy : rhz));
  OXS_DEMAG_REAL_ASYMP Rmiddle
      = R_asymptotic_min*(rhmax/hmax)*pow(rncount,0.5/(asymptotic_order-3));
  if(Rmiddle<2*rhmax) {
    Rmiddle = 2*rhmax;  // Stay away from pole.
  }

  if(Rmiddle<R_asymptotic_min && rncount<needed_refinecount) {
    // Only use this refinement if can be used inside the
    // base refinement, and is also faster than the "needed"
    // refinement.
    if(Rmiddle<R_analytic_max) Rmiddle = R_analytic_max;
    knots.push_back(DemagKnot(Rmiddle,rnx,rny,rnz,asymptotic_order));
  }

  assert(R_analytic_max < R_asymptotic_min);

  if(Rmiddle>R_analytic_max || rncount>=needed_refinecount) {
    // Included "needed" knot
    knots.push_back(needed_knot);
  }

#if DUMPKNOTS
    sbuf << "*** F Knot: ";
    for(auto&& dk : knots) {
      sbuf << " (" << dk.r/da << " : " << dk.nx << ", " << dk.ny << ", "<< dk.nz << ")";
    }
    sbuf << " ***\n";
    std::cerr << sbuf.str();
#endif // DUMPKNOTS
  return R_analytic_max;
}

void Oxs_DemagNxxAsymptotic::FillNxxvec
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 const std::vector<DemagKnot>& spec)
{ // Support code for constructors
  OXS_DEMAG_REAL_ASYMP rlast
    = std::numeric_limits<OXS_DEMAG_REAL_ASYMP>::max();
  for(auto&& dk : spec) {
    if(dk.r >= rlast) {
      OXS_THROW(Oxs_BadParameter,
                "Knot radii not in strictly decreasing order "
                "(Oxs_DemagNxxAsymptotic constructor)");
    }
    rlast = dk.r;
    // Fill Nxxvec
    OxsDemagAsymptoticRefineInfo refine_info(dx,dy,dz,dk.nx,dk.ny,dk.nz);
    Nxxvec.push_back(SubregionApproximate(dk.r,refine_info,dk.order));
  }
}

Oxs_DemagNxxAsymptotic::Oxs_DemagNxxAsymptotic
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 const std::vector<DemagKnot>& spec)
{ // NOTE: Import spec should be ordered with decreasing r
  FillNxxvec(dx,dy,dz,spec);
}

Oxs_DemagNxxAsymptotic::Oxs_DemagNxxAsymptotic
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 OXS_DEMAG_REAL_ASYMP maxerror,
 int asymptotic_order)
{
  if(maxerror<=0.0) {
    // Use default error setting
    maxerror = 8*std::numeric_limits<OXS_DEMAG_REAL_ASYMP>::epsilon();
  }
  std::vector<DemagKnot> knots;
  FindKnotSchedule(dx,dy,dz,maxerror,asymptotic_order,knots);
  FillNxxvec(dx,dy,dz,knots);
}


OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxxAsymptotic::Asymptotic(const OxsDemagNabData& ptdata) const
{
  // Match pt to approximate.  The elements of Nxxvec are ordered with
  // outside (biggest rsq) first.  Since the farther out one goes the
  // more points there are, this ordering reduces the overall number of
  // comparisons necessary to determine the approximate.
  OXS_DEMAG_REAL_ASYMP rsq
    = ptdata.x*ptdata.x+ptdata.y*ptdata.y+ptdata.z*ptdata.z;
  const OxsDemagAsymptoticRefineInfo* refine_info = 0;
  const Oxs_DemagNxxAsymptoticBase* Nxx = 0;
  for(auto&& approx : Nxxvec) {
    if(rsq>=approx.rsq) { // Use this approximate
      refine_info = &approx.refine_info;
      Nxx = &approx.Nxx;
    }
  }
  if(Nxx == nullptr) {
    OXS_THROW(Oxs_BadParameter,"No matching asymptotic range "
                        "(Oxs_DemagNxxAsymptotic::Asymptotic)");
  }
  
  // Copy data from refine_info structure.
  const OC_INDEX xcount = refine_info->xcount;
  const OC_INDEX ycount = refine_info->ycount;
  const OC_INDEX zcount = refine_info->zcount;
  const OXS_DEMAG_REAL_ASYMP rdx = refine_info->rdx;
  const OXS_DEMAG_REAL_ASYMP rdy = refine_info->rdy;
  const OXS_DEMAG_REAL_ASYMP rdz = refine_info->rdz;
  const OXS_DEMAG_REAL_ASYMP result_scale = refine_info->result_scale;

  OxsDemagNabData rptdata,mrptdata;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INDEX k=1-zcount;k<zcount;++k) {
    OXS_DEMAG_REAL_ASYMP zoff = ptdata.z+k*rdz;
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INDEX j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.y+j*rdy;
      rptdata.Set(ptdata.x,yoff,zoff);
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Nxx->Asymptotic(rptdata);
      for(OC_INDEX i=1;i<xcount;++i) {
        rptdata.Set(ptdata.x+i*rdx,yoff,zoff);
        mrptdata.Set(ptdata.x-i*rdx,yoff,zoff);
        xsum += (xcount-i) * Nxx->AsymptoticPair(rptdata,mrptdata);
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
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 int in_max_order)
  : max_order(in_max_order)
{
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
Oxs_DemagNxyAsymptoticBase::Asymptotic
(const OxsDemagNabData& ptdata) const
{ // Asymptotic approximation to Nxy term.

  if(ptdata.R2<=0.0) {
    // Asymptotic expansion doesn't apply for R==0.  Fall back
    // to self-demag calculation.
    return static_cast<OXS_DEMAG_REAL_ASYMP>(0.0);
  }

  // term3 provides 5th order error,
  // term5 provides 7th order error,
  // etc.

  const OXS_DEMAG_REAL_ASYMP& tx2 = ptdata.tx2;
  const OXS_DEMAG_REAL_ASYMP& ty2 = ptdata.ty2;
  const OXS_DEMAG_REAL_ASYMP& tz2 = ptdata.tz2;

  const OXS_DEMAG_REAL_ASYMP term3 = 3*lead_weight;

  OXS_DEMAG_REAL_ASYMP term5 = 0.0;
  if(!cubic_cell) {
    if(max_order>5) {
      term5 = a1*tx2 + a2*ty2 + a3*tz2;
    }
  }

  const OXS_DEMAG_REAL_ASYMP tz4 = tz2*tz2;
  OXS_DEMAG_REAL_ASYMP term7 = 0.0;
  if(max_order>7) {
    term7 = (b1*tx2 + (b2*ty2 + b3*tz2))*tx2 + (b4*ty2 + b5*tz2)*ty2 + b6*tz4;
  }

  OXS_DEMAG_REAL_ASYMP term9 = 0.0;
  if(max_order>9) {
    term9 =
      ((c1*tx2 + (c2*ty2 + c3*tz2))*tx2 + ((c4*ty2 + c5*tz2)*ty2 + c6*tz4))*tx2
      + ((c7*ty2 + c8*tz2)*ty2 + c9*tz4)*ty2
      + c10*tz4*tz2;
  }
  const OXS_DEMAG_REAL_ASYMP& x = ptdata.x;
  const OXS_DEMAG_REAL_ASYMP& y = ptdata.y;
  const OXS_DEMAG_REAL_ASYMP& iR2 = ptdata.iR2;
  const OXS_DEMAG_REAL_ASYMP& iR  = ptdata.iR;
  const OXS_DEMAG_REAL_ASYMP iR5 = iR2*iR2*iR;
  const OXS_DEMAG_REAL_ASYMP Nxy
    = (term9 + term7 + term5 + term3)*iR5*x*y;
  // Error should be of order 1/R^11 if all terms are active

  return Nxy;
}

#if !OXS_DEMAG_ASYMP_USE_SSE
OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptoticBase::AsymptoticPairX
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
  if(R2p<=0.0) return Asymptotic(ptdata.ptm);
  if(R2m<=0.0) return Asymptotic(ptdata.ptp);

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
  if(!cubic_cell && max_order>5) {
    term5p = a1*tx2p + a2*ty2p + a3*tz2p;
    term5m = a1*tx2m + a2*ty2m + a3*tz2m;
  }

  // 1/R^7 terms
  OXS_DEMAG_REAL_ASYMP tz4p = tz2p*tz2p;
  OXS_DEMAG_REAL_ASYMP tz4m = tz2m*tz2m;
  OXS_DEMAG_REAL_ASYMP term7p = 0.0;
  OXS_DEMAG_REAL_ASYMP term7m = 0.0;
  if(max_order>7) {
    term7p = (b1*tx2p + (b2*ty2p + b3*tz2p))*tx2p
      + (b4*ty2p + b5*tz2p)*ty2p + b6*tz4p;
    term7m = (b1*tx2m + (b2*ty2m + b3*tz2m))*tx2m
      + (b4*ty2m + b5*tz2m)*ty2m + b6*tz4m;
  }

  // 1/R^9 terms
  OXS_DEMAG_REAL_ASYMP term9p = 0.0;
  OXS_DEMAG_REAL_ASYMP term9m = 0.0;
  if(max_order>9) {
  term9p = ((c1*tx2p + (c2*ty2p + c3*tz2p))*tx2p
            + ((c4*ty2p + c5*tz2p)*ty2p + c6*tz4p))*tx2p
    + ((c7*ty2p + c8*tz2p)*ty2p + c9*tz4p)*ty2p
    + c10*tz4p*tz2p;
  term9m = ((c1*tx2m + (c2*ty2m + c3*tz2m))*tx2m
            + ((c4*ty2m + c5*tz2m)*ty2m + c6*tz4m))*tx2m
    + ((c7*ty2m + c8*tz2m)*ty2m + c9*tz4m)*ty2m
    + c10*tz4m*tz2m;
  }

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
  // Error should be of order 1/R^11 if all terms are active.

  return Nxy;
}

#else // !OXS_DEMAG_ASYMP_USE_SSE

OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptoticBase::AsymptoticPairX
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
  if(R2p<=0.0) return Asymptotic(xm,y,z);
  if(R2m<=0.0) return Asymptotic(xp,y,z);

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
  if(!cubic_cell && max_order>5) {
    term5 = a1*tx2 + a2*ty2 + a3*tz2;
  }

  // 1/R^7 terms
  Oc_Duet tz4 = tz2*tz2;
  Oc_Duet term7; term7.SetZero();
  if(max_order>7) {
    term7 = b6*tz4
      + ty2*(b4*ty2 + b5*tz2)
      + tx2*(b1*tx2 + b2*ty2 + b3*tz2);
  }

  // 1/R^9 terms
  Oc_Duet term9; term9.SetZero();
  if(max_order>7) {
    term9 = c10*tz4*tz2
      + ty2*(ty2*(c7*ty2 + c8*tz2) + c9*tz4)
      + tx2*(tx2*(c1*tx2 + c2*ty2 + c3*tz2)
             + ty2*(c4*ty2 + c5*tz2) + c6*tz4);
  }

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
Oxs_DemagNxyAsymptoticBase::AsymptoticPair
(const OxsDemagNabData& ptA,const OxsDemagNabData& ptB) const
{ // Asymptotic approximation to Nxy term.  This routine takes a pair of
  // points and uses SSE intrinsics to accelerate the computation.

  // Both R2p and R2m must be positive, since asymptotics don't apply
  // for R==0.  Note self-demag term for Nxy == 0.
  if(ptA.R2<=0.0) return Asymptotic(ptB);
  if(ptB.R2<=0.0) return Asymptotic(ptA);

  const Oc_Duet tx2(ptA.tx2,ptB.tx2);
  const Oc_Duet ty2(ptA.ty2,ptB.ty2);
  const Oc_Duet tz2(ptA.tz2,ptB.tz2);

  const Oc_Duet term3 = 3*lead_weight;
  Oc_Duet Nxy = term3;

  if(!cubic_cell && max_order>5) {
    const Oc_Duet term5 = a1*tx2 + a2*ty2 + a3*tz2;
    Nxy += term5;
  }

  const Oc_Duet tz4 = tz2*tz2;
  Oc_Duet term7 = 0.0;
  if(max_order>7) {
    term7 =  b6*tz4 + ty2*(b4*ty2 + b5*tz2) + tx2*(b1*tx2 + b2*ty2 + b3*tz2);
  }

  Oc_Duet term9 = 0.0;
  if(max_order>9) {
    term9 = c10*tz4*tz2
      + ty2*(ty2*(c7*ty2 + c8*tz2) + c9*tz4)
      + tx2*(tx2*(c1*tx2 + c2*ty2 + c3*tz2) + ty2*(c4*ty2 + c5*tz2) + c6*tz4);
  }

  Nxy += (term7 + term9);

  const Oc_Duet iR(ptA.iR,ptB.iR);
  Oc_Duet iR5(ptA.iR2,ptB.iR2); iR5 *= iR5; iR5 *= iR;

  Nxy *= iR5;

  Nxy *= Oc_Duet(ptA.x,ptB.x) * Oc_Duet(ptA.y,ptB.y);

  OXS_DEMAG_REAL_ASYMP Nxysum = Nxy.GetA() + Nxy.GetB();

  return Nxysum;
}
#endif // OXS_DEMAG_ASYMP_USE_SSE

void Oxs_DemagNxyAsymptotic::FillNxyvec
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 const std::vector<DemagKnot>& spec)
{ // Support code for constructors
  OXS_DEMAG_REAL_ASYMP rlast
    = std::numeric_limits<OXS_DEMAG_REAL_ASYMP>::max();
  for(auto&& dk : spec) {
    if(dk.r >= rlast) {
      OXS_THROW(Oxs_BadParameter,
                "Knot radii not in strictly decreasing order "
                "(Oxs_DemagNxyAsymptotic constructor)");
    }
    rlast = dk.r;
    // Fill Nxyvec
    OxsDemagAsymptoticRefineInfo refine_info(dx,dy,dz,dk.nx,dk.ny,dk.nz);
    Nxyvec.push_back(SubregionApproximate(dk.r,refine_info,dk.order));
  }
}

Oxs_DemagNxyAsymptotic::Oxs_DemagNxyAsymptotic
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 const std::vector<DemagKnot>& spec)
{ // NOTE: Import spec should be ordered with decreasing r
  FillNxyvec(dx,dy,dz,spec);
}

Oxs_DemagNxyAsymptotic::Oxs_DemagNxyAsymptotic
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 OXS_DEMAG_REAL_ASYMP maxerror,
 int asymptotic_order)
{
  if(maxerror<=0.0) {
    // Use default error setting
    maxerror = 8*std::numeric_limits<OXS_DEMAG_REAL_ASYMP>::epsilon();
  }
  std::vector<DemagKnot> knots;
  FindKnotSchedule(dx,dy,dz,maxerror,asymptotic_order,knots);
  FillNxyvec(dx,dy,dz,knots);
}

OXS_DEMAG_REAL_ASYMP
Oxs_DemagNxyAsymptotic::Asymptotic(const OxsDemagNabData& ptdata) const
{
  // Match pt to approximate.  The elements of Nxyvec are ordered with
  // outside (biggest rsq) first.  Since the farther out one goes the
  // more points there are, this ordering reduces the overall number of
  // comparisons necessary to determine the approximate.
  OXS_DEMAG_REAL_ASYMP rsq
    = ptdata.x*ptdata.x+ptdata.y*ptdata.y+ptdata.z*ptdata.z;
  const OxsDemagAsymptoticRefineInfo* refine_info = 0;
  const Oxs_DemagNxyAsymptoticBase* Nxy = 0;
  for(auto&& approx : Nxyvec) {
    if(rsq>=approx.rsq) { // Use this approximate
      refine_info = &approx.refine_info;
      Nxy = &approx.Nxy;
    }
  }
  if(Nxy == nullptr) {
    OXS_THROW(Oxs_BadParameter,"No matching asymptotic range "
                        "(Oxs_DemagNxyAsymptotic::Asymptotic)");
  }
  
  // Copy data from refine_info structure.
  const OC_INDEX xcount = refine_info->xcount;
  const OC_INDEX ycount = refine_info->ycount;
  const OC_INDEX zcount = refine_info->zcount;
  const OXS_DEMAG_REAL_ASYMP rdx = refine_info->rdx;
  const OXS_DEMAG_REAL_ASYMP rdy = refine_info->rdy;
  const OXS_DEMAG_REAL_ASYMP rdz = refine_info->rdz;
  const OXS_DEMAG_REAL_ASYMP result_scale = refine_info->result_scale;

  OxsDemagNabData rptdata,mrptdata;
  OXS_DEMAG_REAL_ASYMP zsum = 0.0;
  for(OC_INDEX k=1-zcount;k<zcount;++k) {
    OXS_DEMAG_REAL_ASYMP zoff = ptdata.z+k*rdz;
    OXS_DEMAG_REAL_ASYMP ysum = 0.0;
    for(OC_INDEX j=1-ycount;j<ycount;++j) {
      // Compute interactions for x-strip
      OXS_DEMAG_REAL_ASYMP yoff = ptdata.y+j*rdy;
      rptdata.Set(ptdata.x,yoff,zoff);
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Nxy->Asymptotic(rptdata);
      for(OC_INDEX i=1;i<xcount;++i) {
        rptdata.Set(ptdata.x+i*rdx,yoff,zoff);
        mrptdata.Set(ptdata.x-i*rdx,yoff,zoff);
        xsum += (xcount-i) * Nxy->AsymptoticPair(rptdata,mrptdata);
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
Oxs_DemagNxyAsymptotic::AsymptoticPairX
(const OxsDemagNabPairData& ptdata) const
{ // Evaluates asymptotic approximation to
  //    Nxy(x+xoff,y,z) + Nxy(x-xoff,y,z)
  // on the assumption that |xoff| >> |x|.

  assert(ptdata.ptp.y == ptdata.ptm.y &&
         ptdata.ptp.z == ptdata.ptm.z);

  // Match pt to approximate.  The elements of Nxyvec are ordered with
  // outside (biggest rsq) first.  Since the farther out one goes the
  // more points there are, this ordering reduces the overall number of
  // comparisons necessary to determine the approximate.
  OXS_DEMAG_REAL_ASYMP smallx
    = (ptdata.ptm.x<ptdata.ptp.x ? ptdata.ptm.x : ptdata.ptp.x);
  OXS_DEMAG_REAL_ASYMP rsq
    = smallx*smallx+ptdata.ptp.y*ptdata.ptp.y+ptdata.ptp.z*ptdata.ptp.z;
  const OxsDemagAsymptoticRefineInfo* refine_info = 0;
  const Oxs_DemagNxyAsymptoticBase* Nxy = 0;
  for(auto&& approx : Nxyvec) {
    if(rsq>=approx.rsq) { // Use this approximate
      refine_info = &approx.refine_info;
      Nxy = &approx.Nxy;
    }
  }
  if(Nxy == nullptr) {
    OXS_THROW(Oxs_BadParameter,"No matching asymptotic range "
                        "(Oxs_DemagNxyAsymptotic::Asymptotic)");
  }
  
  // Copy data from refine_info structure.
  const OC_INDEX xcount = refine_info->xcount;
  const OC_INDEX ycount = refine_info->ycount;
  const OC_INDEX zcount = refine_info->zcount;
  const OXS_DEMAG_REAL_ASYMP rdx = refine_info->rdx;
  const OXS_DEMAG_REAL_ASYMP rdy = refine_info->rdy;
  const OXS_DEMAG_REAL_ASYMP rdz = refine_info->rdz;
  const OXS_DEMAG_REAL_ASYMP result_scale = refine_info->result_scale;

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
      OXS_DEMAG_REAL_ASYMP xsum = xcount * Nxy->AsymptoticPairX(work);
      for(OC_INDEX i=1;i<xcount;++i) {
        work.uoff = ptdata.uoff + i*rdx;
        OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                                 work.ubase - work.uoff,yoff,zoff,
                                 work.ptp, work.ptm);
        OXS_DEMAG_REAL_ASYMP tmpsum = Nxy->AsymptoticPairX(work);
        work.uoff = ptdata.uoff - i*rdx;
        OxsDemagNabData::SetPair(work.ubase + work.uoff,yoff,zoff,
                                 work.ubase - work.uoff,yoff,zoff,
                                 work.ptp, work.ptm);
        tmpsum += Nxy->AsymptoticPairX(work);
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
//
// These classes are intended for internal use only.
//

OxsDemagNxxIntegralXBase::OxsDemagNxxIntegralXBase
(OXS_DEMAG_REAL_ASYMP rdx,OXS_DEMAG_REAL_ASYMP rdy,
 OXS_DEMAG_REAL_ASYMP rdz,OXS_DEMAG_REAL_ASYMP Wx)
  : scale((rdx*rdy*rdz)/(4*WIDE_PI*Wx))
{
  // In "scale", 1/Wx comes from scaling of the integration variable,
  // and the sign takes into account that we evaluate the +infinity
  // integral at the lower end (the upper end being 0).

  OXS_DEMAG_REAL_ASYMP dx = rdx;
  OXS_DEMAG_REAL_ASYMP dy = rdy;
  OXS_DEMAG_REAL_ASYMP dz = rdz;

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
  // For error, see NOTES V, 20-22 Feb 2011, pp. 154-157.  See also
  // NOTES IV, 25-Jan-2007, pp. 93-98, NOTES VI, 4-Jun-2013, p. 160, and
  // NOTES VII, 2-Dec-2015, p. 118.

#if 0
printf("CHECK term7p = %#.17g\n",(double)term7p); /**/
printf("CHECK term7m = %#.17g\n",(double)term7m); /**/
printf("CHECK INxx = %#.17g\n",(double)INxx); /**/
#endif

  return INxx;
}


OxsDemagNxyIntegralXBase::OxsDemagNxyIntegralXBase
(OXS_DEMAG_REAL_ASYMP rdx,OXS_DEMAG_REAL_ASYMP rdy,
 OXS_DEMAG_REAL_ASYMP rdz,OXS_DEMAG_REAL_ASYMP Wx)
  : scale((rdx*rdy*rdz)/(4*WIDE_PI*Wx))
{
  // In "scale", 1/Wx comes from scaling of the integration variable,
  // and the sign takes into account that we evaluate the +infinity
  // integral at the lower end (the upper end being 0).

  OXS_DEMAG_REAL_ASYMP dx = rdx;
  OXS_DEMAG_REAL_ASYMP dy = rdy;
  OXS_DEMAG_REAL_ASYMP dz = rdz;

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
  // For error, see NOTES V, 20-22 Feb 2011, pp. 154-157.  See also
  // NOTES IV, 25-Jan-2007, pp. 93-98, NOTES VI, 4-Jun-2013, p. 160, and
  // NOTES VII, 2-Dec-2015, p. 118.

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
(OXS_DEMAG_REAL_ASYMP rdx,OXS_DEMAG_REAL_ASYMP rdy,
 OXS_DEMAG_REAL_ASYMP rdz,OXS_DEMAG_REAL_ASYMP Wz)
  : scale((rdx*rdy*rdz)/(4*WIDE_PI*Wz))
{
  // In "scale", 1/Wz comes from scaling of the integration variable,
  // and the sign takes into account that we evaluate the +infinity
  // integral at the lower end (the upper end being 0).

  OXS_DEMAG_REAL_ASYMP dx = rdx;
  OXS_DEMAG_REAL_ASYMP dy = rdy;
  OXS_DEMAG_REAL_ASYMP dz = rdz;

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
  zm = fabs(zm);

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
(OXS_DEMAG_REAL_ASYMP rdx,OXS_DEMAG_REAL_ASYMP rdy,
 OXS_DEMAG_REAL_ASYMP rdz,OXS_DEMAG_REAL_ASYMP Wz)
  : scale((rdx*rdy*rdz)/(4*WIDE_PI*Wz))
{
  // In "scale", 1/Wz comes from scaling of the integration variable,
  // and the sign takes into account that we evaluate the +infinity
  // integral at the lower end (the upper end being 0).

  OXS_DEMAG_REAL_ASYMP dx = rdx;
  OXS_DEMAG_REAL_ASYMP dy = rdy;
  OXS_DEMAG_REAL_ASYMP dz = rdz;

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
  zm = fabs(zm);

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

const OXS_DEMAG_REAL_ASYMP Oxs_DemagPeriodic::D[TAIL_TWEAK_COUNT] = {
  464514259. / OXS_DEMAG_REAL_ASYMP(464486400.),
  464115227. / OXS_DEMAG_REAL_ASYMP(464486400.),
  467323119. / OXS_DEMAG_REAL_ASYMP(464486400.),
  438283495. / OXS_DEMAG_REAL_ASYMP(464486400.),
   26202905. / OXS_DEMAG_REAL_ASYMP(464486400.),
   -2836719. / OXS_DEMAG_REAL_ASYMP(464486400.),
     371173. / OXS_DEMAG_REAL_ASYMP(464486400.),
     -27859. / OXS_DEMAG_REAL_ASYMP(464486400.)
};

OC_INDEX
Oxs_DemagPeriodic::ComputeTailOffset
(OXS_DEMAG_REAL_ASYMP dx,
 OXS_DEMAG_REAL_ASYMP dy,
 OXS_DEMAG_REAL_ASYMP dz,
 OC_INDEX rdimx)
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
    = pow(OXS_DEMAG_REAL_ASYMP(dx*dy*dz),
          OXS_DEMAG_REAL_ASYMP(1.)/OXS_DEMAG_REAL_ASYMP(3.));
  return OC_INDEX(ceil(CHECK_VALUE/sqrt(sqrt(rdimx*dx/gamma))-2.0));
}


Oxs_DemagPeriodic::Oxs_DemagPeriodic
(OXS_DEMAG_REAL_ASYMP idx, OXS_DEMAG_REAL_ASYMP idy, OXS_DEMAG_REAL_ASYMP idz,
 OXS_DEMAG_REAL_ASYMP maxerror,
 int asymptotic_order,
 OC_INDEX irdimx, // Periodic window dimension,
 OC_INDEX iwdimx, // Pre-computed (mostly) analytic
 OC_INDEX iwdimy, // window dimensions
 OC_INDEX iwdimz)
  : dx(idx), dy(idy), dz(idz),
    rdimx(irdimx),
    wdimx(iwdimx),wdimy(iwdimy),wdimz(iwdimz),
    ktail(ComputeTailOffset(idx,idy,idz,irdimx)),
    Nxx(idx,idy,idz,maxerror,asymptotic_order,irdimx*idx,ktail),
    Nxy(idx,idy,idz,maxerror,asymptotic_order,irdimx*idx,ktail),
    Nxz(idx,idy,idz,maxerror,asymptotic_order,irdimx*idx,ktail),
    Nyy(idx,idy,idz,maxerror,asymptotic_order,irdimx*idx,ktail),
    Nyz(idx,idy,idz,maxerror,asymptotic_order,irdimx*idx,ktail),
    Nzz(idx,idy,idz,maxerror,asymptotic_order,irdimx*idx,ktail)
{
  assert(wdimx>=0 && wdimy>=0 && wdimz>=0);
}

void Oxs_DemagPeriodic::ComputeAsymptoticLimits
(OC_INDEX i,
 /// x = i*dx = base offset along periodic (x) direction.
 /// Member value wdimx = width of analytic computation window in units of dx
 OC_INDEX j, OC_INDEX k, // Other coordinate indices
 OC_INDEX& k1, OC_INDEX& k2,   // Near field terms
 OC_INDEX& k1a, OC_INDEX& k2a, // Asymmetric far field
 OXS_DEMAG_REAL_ASYMP& newx,   // Symmetric far field
 OXS_DEMAG_REAL_ASYMP& newoffset
 ) const
{
  OXS_DEMAG_REAL_ASYMP y = j*dy;
  OXS_DEMAG_REAL_ASYMP z = k*dz;

  OXS_DEMAG_REAL_ASYMP xlimit = 0.0;

  OXS_DEMAG_REAL_ASYMP Arad = GetAsymptoticStart();
  OXS_DEMAG_REAL_ASYMP Asq = Arad*Arad - y*y - z*z;
  if(Asq>0) xlimit = sqrt(Asq);

  OXS_DEMAG_REAL_ASYMP x = i*dx;
  OXS_DEMAG_REAL_ASYMP W = rdimx*dx;

  k1 = OC_INDEX(floor((-xlimit-x)/W));
  k2 = OC_INDEX(ceil((xlimit-x)/W));

  if(j<wdimy && k<wdimz) {
    // Insure we don't include pre-computed (mostly analytic) values
    // into the computation.  Note: Integer division involving negative
    // values may be implementation defined, so just use floating point
    // ceil and floor to keep it simple.
    if(-wdimx<i+k1*rdimx) k1 = OC_INDEX(floor(double(-wdimx-i)/rdimx));
    if(i+k2*rdimx<wdimx)  k2 = OC_INDEX(ceil(double(wdimx-i)/rdimx));
  }

  if(k1==k2) --k1; // Special case: xlimit=0 and x/W is an integer
  /// (Actually, not xlimit==0 but (x +/- xlimit) == x.)

  assert(k1<k2);
  assert(x+k1*W <= -xlimit);
  assert(x+k2*W >=  xlimit);
  assert(j>=wdimy || k>=wdimz || i+k1*rdimx <= -wdimx);
  assert(j>=wdimy || k>=wdimz || wdimx <= i+k2*rdimx);

  // k1 and k2 are selected above so that x + k1*W < -xlimit
  // and x + k2*W > xlimit.  But we also want x + k1*W and
  // x + k2*W to be as symmetric about the origin as possible,
  // so that we get better canceling on odd terms.  We insure
  // that with this tweak:
  k1a = k1;    k2a = k2;
  if(2*(i+k1*rdimx + i+k2*rdimx) > rdimx) {
    k1a = k1-1;
  } else if(2*(i+k1*rdimx + i+k2*rdimx) < -rdimx) {
    k2a = k2+1;
  }

  // Note that k1 and k2 used for near field, and k1a and
  // k2a used for the far field tweak, are indexed off of
  // the original u. But newu and newoffset are computed so
  // that asymptotic approximate pairs are computed for
  // newu +/- (newoffset + k*W), for k = 0, 1, 2, ...
  newx = x + (k2a + k1a)*W/2;
  newoffset = (k2a - k1a)*W/2;
}

void Oxs_DemagPeriodic::ComputeHoleTensorPBCx
(OC_INDEX ipos,
 OC_INDEX jpos,
 OC_INDEX kpos,
 OXS_DEMAG_REAL_ASYMP& pbcNxx,
 OXS_DEMAG_REAL_ASYMP& pbcNxy,
 OXS_DEMAG_REAL_ASYMP& pbcNxz,
 OXS_DEMAG_REAL_ASYMP& pbcNyy,
 OXS_DEMAG_REAL_ASYMP& pbcNyz,
 OXS_DEMAG_REAL_ASYMP& pbcNzz) const
{ // Computes tensor assuming periodic direction is x, excluding
  // x = i*dx positions with |i|<wdimx.  If wdimx==0, then there
  // is no exclusion hole.
  //   Asymptotic formulae are used for x outside the asymptotic radius,
  // analytic formulae inside.  The latter are computed individually,
  // which is considerably slower than the bulk method which can reuse
  // precursor evaluations between neighboring cells.
  //   The intention of this routine is that the client code uses the
  // fast bulk analytic method for computing all tensor values in a
  // rectangular volume encompassing the asymptotic boundary, and then
  // add in the values computed by this routine for the exterior volume.
  //   However, if you need only isolated tensor values, then you can
  // call this routine with wdimx==0 to compute the full periodic
  // tensor (i.e., no hole).
  //   This routine computes x-periodic BC's.  Other periodic directions
  // can be handled by rotating coordinates.  The necessary coordinate
  // rotations involve the dx, dy, dz constructor imports, the i,j,k
  // imports for this function, and the pbcN?? outputs.  The necessary
  // gyrations are implemented in child classes.

  const OXS_DEMAG_REAL_ASYMP x = ipos*dx;
  const OXS_DEMAG_REAL_ASYMP W = rdimx*dx;
  const OXS_DEMAG_REAL_ASYMP y = jpos*dy;
  const OXS_DEMAG_REAL_ASYMP z = kpos*dz;

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
  ComputeAsymptoticLimits(ipos,jpos,kpos,k1,k2,k1a,k2a,xasm,xoffasm);

  OXS_DEMAG_REAL_ASYMP resultNxx = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNxy = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNxz = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNyy = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNyz = 0.0;
  OXS_DEMAG_REAL_ASYMP resultNzz = 0.0;

  // First compute integral approximations for tails.  See NOTES V, 7-20
  // Jan 2011, pp. 102-108.  The version below uses the n=4 variant on
  // p. 108:
  const OC_INDEX kstop = (ktail - OC_INDEX(floor(xoffasm/W)) > 0 ?
                          ktail - OC_INDEX(floor(xoffasm/W)) : 0);
  OC_INDEX k;
  OxsDemagNabPairData pairpt;  pairpt.ubase = xasm;
  for(k=0;k<TAIL_TWEAK_COUNT;++k) {
    pairpt.uoff = xoffasm+(kstop+k)*W;
    OxsDemagNabData::SetPair(xasm+pairpt.uoff,y,z,
                             xasm-pairpt.uoff,y,z,
                             pairpt.ptp,pairpt.ptm);
    resultNxx += Nxx.FarFieldPair(pairpt) * D[k];
    resultNxy += Nxy.FarFieldPair(pairpt) * D[k];
    resultNxz += Nxz.FarFieldPair(pairpt) * D[k];
    resultNyy += Nyy.FarFieldPair(pairpt) * D[k];
    resultNyz += Nyz.FarFieldPair(pairpt) * D[k];
    resultNzz += Nzz.FarFieldPair(pairpt) * D[k];
  }
  pairpt.uoff = xoffasm+(kstop+(TAIL_TWEAK_COUNT-1)/2.)*W;
  OxsDemagNabData::SetPair(xasm+pairpt.uoff,y,z,
                           xasm-pairpt.uoff,y,z,
                           pairpt.ptp,pairpt.ptm);
  resultNxx += Nxx.FarIntegralPair(pairpt);
  resultNxy += Nxy.FarIntegralPair(pairpt);
  resultNxz += Nxz.FarIntegralPair(pairpt);
  resultNyy += Nyy.FarIntegralPair(pairpt);
  resultNyz += Nyz.FarIntegralPair(pairpt);
  resultNzz += Nzz.FarIntegralPair(pairpt);

  // Next, sum in asymptotic terms that are paired off, +/-.
  //  These use the offset base (xasm+xoffasm,yasm,zasm)
  for(k=0;k<kstop;++k) {
    pairpt.uoff = xoffasm+k*W;
    OxsDemagNabData::SetPair(xasm+pairpt.uoff,y,z,
                             xasm-pairpt.uoff,y,z,
                             pairpt.ptp,pairpt.ptm);
    resultNxx += Nxx.FarFieldPair(pairpt);
    resultNxy += Nxy.FarFieldPair(pairpt);
    resultNxz += Nxz.FarFieldPair(pairpt);
    resultNyy += Nyy.FarFieldPair(pairpt);
    resultNyz += Nyz.FarFieldPair(pairpt);
    resultNzz += Nzz.FarFieldPair(pairpt);
  }

  // Add in assymetric "tweak" terms.  These use the same base
  // (x,y,z) as the NearField.
  OxsDemagNabData pt;
  for(k=k1a+1;k<=k1;++k) {
    pt.Set(x+k*W,y,z);
    resultNxx += Nxx.FarField(pt);
    resultNxy += Nxy.FarField(pt);
    resultNxz += Nxz.FarField(pt);
    resultNyy += Nyy.FarField(pt);
    resultNyz += Nyz.FarField(pt);
    resultNzz += Nzz.FarField(pt);
  }
  for(k=k2;k<k2a;++k) {
    pt.Set(x+k*W,y,z);
    resultNxx += Nxx.FarField(pt);
    resultNxy += Nxy.FarField(pt);
    resultNxz += Nxz.FarField(pt);
    resultNyy += Nyy.FarField(pt);
    resultNyz += Nyz.FarField(pt);
    resultNzz += Nzz.FarField(pt);
  }

  // All far-field elements outside dx*wdimx computed.  In normal
  // usage wdimx is chosen so that dx*wdimx > asymptotic radius,
  // in which case this routine is complete.  This routine should
  // only continue in the case wdimx=wdimy=wdimz=0.  Throw a warning
  // if this is not the case, but compute the remainder using
  // analytic formulae regardless.
  if(ipos+(k1+1)*rdimx<=-wdimx || wdimx<=(k2-1)*rdimx+ipos) {
    // Near field computation requested
    if(wdimx>0) {
#if STANDALONE
      static int warncount=1;
      if(warncount>0) {
        fprintf(stderr,"*** WARNING: "
                "Bad use of ComputePeriodicHoleTensor; hole should"
                " be either 0 or include full near-field volume. ***\n");
        --warncount;
      }
#else // !STANDALONE
      static Oxs_WarningMessage badhole(1);
      badhole.Send(revision_info,OC_STRINGIFY(__LINE__),
                   "Bad use of ComputePeriodicHoleTensor; hole should"
                   " be either 0 or include full near-field volume.");
#endif // STANDALONE
    }
    OXS_DEMAG_REAL_ASYMP nearNxx = 0.0;
    OXS_DEMAG_REAL_ASYMP nearNxy = 0.0;
    OXS_DEMAG_REAL_ASYMP nearNxz = 0.0;
    OXS_DEMAG_REAL_ASYMP nearNyy = 0.0;
    OXS_DEMAG_REAL_ASYMP nearNyz = 0.0;
    OXS_DEMAG_REAL_ASYMP nearNzz = 0.0;

    // Ranges are inclusive
    const OC_INDEX k1start = k1+1;
    const OC_INDEX k1stop  = OC_INDEX(floor(double(-wdimx-ipos)/rdimx));
    const OC_INDEX k2start = OC_INDEX(ceil(double(wdimx-ipos)/rdimx));
    const OC_INDEX k2stop  = k2-1;
    OC_INDEX kstopx = k1stop;
    k=k1start;
    while(1) {
      if(k>kstopx) {
        kstopx=k2stop;
        if(k<k2start) k = k2start; // Handles case wdimx==0 with k1stop==k2stop
        if(k>kstopx) break;
      }
      pt.Set(x+k*W,y,z);
      nearNxx += Nxx.NearField(pt);
      nearNxy += Nxy.NearField(pt);
      nearNxz += Nxz.NearField(pt);
      nearNyy += Nyy.NearField(pt);
      nearNyz += Nyz.NearField(pt);
      nearNzz += Nzz.NearField(pt);
      ++k;
    }
    resultNxx += nearNxx;
    resultNxy += nearNxy;
    resultNxz += nearNxz;
    resultNyy += nearNyy;
    resultNyz += nearNyz;
    resultNzz += nearNzz;
  }

  pbcNxx = resultNxx;
  pbcNxy = resultNxy;
  pbcNxz = resultNxz;
  pbcNyy = resultNyy;
  pbcNyz = resultNyz;
  pbcNzz = resultNzz;
}
