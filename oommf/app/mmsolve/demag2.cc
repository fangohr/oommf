/* File: demag2.cc */
/*
 * (New) Constant magnetization demag routines, based on formulae
 * presented in "A Generalization of the Demagnetizing Tensor for
 * Nonuniform Magnetization," by Andrew J. Newell, Wyn Williams, and
 * David J. Dunlop, Journal of Geophysical Research, vol 98, p 9551-9555,
 * June 1993.  This formulae clearly satisfy necessary symmetry and
 * scaling properties, which is not true of the formulae presented in
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

#include "nb.h"

#include "demag.h"
#include "fft.h"

#ifdef USE_MPI
#include "mmsmpi.h"
#endif /* USE_MPI */

/* End includes */

DF_Init     ConstMagInit;
DF_Destroy  ConstMagDestroy;
DF_Calc     ConstMagField;

static OC_REALWIDE Aspect(1.0);  // film thickness/cellsize
static int Nx(1),Ny(1);     // Input array dimensions, space domain
static int NFx(1),NFy(1);   // Array dimensions, in freq domain

// Kernel arrays, in frequency domain.  See MJD NOTES, 13-Dec-1998, p189
// for details.  The k,l superscripts correspond to the coordinate
// directions, with 0=>x, 1=>y, 2=>z.
static MyComplex **A00 = NULL;
static MyComplex **A01 = NULL;
static MyComplex **A11 = NULL;
static MyComplex **A22 = NULL;		 /// Other terms are zero.
// Magnetization arrays
static double **spacework = NULL;   // Space domain workspace
static MyComplex **freqwork = NULL; // Freqency domain workspace
static MyComplex **M0 = NULL;
static MyComplex **M1 = NULL;
static MyComplex **M2 = NULL;    // Frequeny domain
                                /// magnetizations, M0 is x-component,
                                /// M1 is y-component, M2 is z-component.

#ifndef USE_MPI
static FFTReal2D *fft = NULL;
// Class to handle forward and reverse FFT's.
#else
static FFTReal2D_mpi *fft = NULL;
// Class to handle forward and reverse FFT's, MPI-style.
// NOTE: We make this a dynamically allocated beastie so it can
//       be dynamically de-allocated _before_ MPI_Finalize() is
//       called.
#endif

////////////////////////////////////////////////////////////////////////////
// Routines to do accurate summation
extern "C" {
static int AS_Compare(const void* px,const void* py)
{
  // Comparison based on absolute values
  OC_REALWIDE x=fabs(*((const OC_REALWIDE *)px));
  OC_REALWIDE y=fabs(*((const OC_REALWIDE *)py));
  if(x<y) return 1;
  if(x>y) return -1;
  return 0;
}
}

static OC_REALWIDE
AccurateSum(int n,OC_REALWIDE *arr)
{
  // Order by decreasing magnitude
  qsort(arr,n,sizeof(OC_REALWIDE),AS_Compare);

  // Add up using doubly compensated summation
  OC_REALWIDE sum,corr,y,u,t,v,z,x;
  sum=arr[0]; corr=0;
  for(int i=1;i<n;i++) {
    x=arr[i];
    y=corr+x;
    u=x-(y-corr);
    t=y+sum;
    v=y-(t-sum);
    z=u+v;
    sum=t+z;
    corr=z-(sum-t);
  }
  return sum;
}


////////////////////////////////////////////////////////////////////////////
// Routines to calculate kernel coefficients
// See Newell et al. for details. The code below follows the
// naming conventions in that paper.

static OC_REALWIDE
SelfDemagDx(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // Note: Assumes x, y, and z are all >0.
  // Note 2: egcs-2.91.57 on Linux/x86 with -O1 mangles this
  //  function (produces NaN's) unless we manually group terms.
  // Here self demag Hx = -Dx Mx / (4*PI*x*y*z).

  if(x==y && y==z) return 4*PI*x*x*x/3.;  // Special case: cube

  OC_REALWIDE xsq=x*x,ysq=y*y,zsq=z*z;
  OC_REALWIDE diag=sqrt(xsq+ysq+zsq);
  OC_REALWIDE arr[15];

  OC_REALWIDE mpxy = (x-y)*(x+y);
  OC_REALWIDE mpxz = (x-z)*(x+z);

  arr[0] = -4*(2*xsq*x-ysq*y-zsq*z);
  arr[1] =  4*(xsq+mpxy)*sqrt(xsq+ysq);
  arr[2] =  4*(xsq+mpxz)*sqrt(xsq+zsq);
  arr[3] = -4*(ysq+zsq)*sqrt(ysq+zsq);
  arr[4] = -4*diag*(mpxy+mpxz);

  arr[5] = 24*x*y*z*atan(y*z/(x*diag));
  arr[6] = 12*(z+y)*xsq*log(x);

  arr[7] = 12*z*ysq*log((sqrt(ysq+zsq)+z)/y);
  arr[8] = -12*z*xsq*log(sqrt(xsq+zsq)+z);
  arr[9] = 12*z*mpxy*log(diag+z);
  arr[10] = -6*z*mpxy*log(xsq+ysq);

  arr[11] =  12*y*zsq*log((sqrt(ysq+zsq)+y)/z);
  arr[12] = -12*y*xsq*log(sqrt(xsq+ysq)+y);
  arr[13] =  12*y*mpxz*log(diag+y);
  arr[14] =  -6*y*mpxz*log(xsq+zsq);

  OC_REALWIDE Dx = AccurateSum(15,arr)/3.;

  return Dx;
}

OC_REALWIDE SelfDemagNx(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize)
{
  if(xsize<=0.0 || ysize<=0.0 || zsize<=0.0) return 0.0;
  return SelfDemagDx(xsize,ysize,zsize)/(4*PI*xsize*ysize*zsize);
  /// 4*PI*xsize*ysize*zsize is scaling factor to convert internal
  /// Dx value to proper scaling consistent with Newell's Nxx,
  /// where Hx = -Nxx.Mx (formula (16) in Newell).
}
OC_REALWIDE SelfDemagNy(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize)
{ return SelfDemagNx(ysize,zsize,xsize); }
OC_REALWIDE SelfDemagNz(OC_REALWIDE xsize,OC_REALWIDE ysize,OC_REALWIDE zsize)
{ return SelfDemagNx(zsize,xsize,ysize); }


static OC_REALWIDE
f(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // There is mucking around here to handle case where imports
  // are near zero.  In particular, asinh(t) is written as
  // log(t+sqrt(1+t)) because the latter appears easier to
  // handle if t=y/x (for example) as x -> 0.

 // This function is even; the fabs()'s just simplify special case handling.
  x=fabs(x); OC_REALWIDE xsq=x*x;
  y=fabs(y); OC_REALWIDE ysq=y*y;
  z=fabs(z); OC_REALWIDE zsq=z*z; 

  OC_REALWIDE R=xsq+ysq+zsq;
  if(R<=0.0) return 0.0;
  else       R=sqrt(R);

  // f(x,y,z)
  OC_REALWIDE piece[8];
  int piececount=0;
  if(z>0.) { // For 2D grids, half the calls from F1 have z==0.
    OC_REALWIDE temp1,temp2,temp3;
    piece[piececount++] = 2*(2*xsq-ysq-zsq)*R;
    if((temp1=x*y*z)>0.)
      piece[piececount++] = -12*temp1*Oc_Atan2(y*z,x*R);
    if(y>0. && (temp2=xsq+zsq)>0.) {
      OC_REALWIDE dummy = log(((y+R)*(y+R))/temp2);
      piece[piececount++] = 3*y*zsq*dummy;
      piece[piececount++] = -3*y*xsq*dummy;
    }
    if((temp3=xsq+ysq)>0.) {
      OC_REALWIDE dummy = log(((z+R)*(z+R))/temp3);
      piece[piececount++] = 3*z*ysq*dummy;
      piece[piececount++] = -3*z*xsq*dummy;
    }
  } else {
    // z==0
    if(x==y) {
      const double K = -2.45981439737106805379;
      /// K = 2*sqrt(2)-6*log(1+sqrt(2))
      piece[piececount++] = K*xsq*x;
    } else {
      piece[piececount++] = 2*(2*xsq-ysq)*R;
      if(y>0. && x>0.)
	piece[piececount++] = -6*y*xsq*log((y+R)/x);
    }
  }

  return AccurateSum(piececount,piece)/12.;
}

#ifdef OLD_CODE
static OC_REALWIDE
GetSDA00(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
	 OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ // This is Nxx*(4*PI*tau) in Newell's paper
  OC_REALWIDE result=0.;
  if(x==0. && y==0. && z==0.) {
    // Self demag term.  The base routine can handle x==y==z==0,
    // but this should be more accurate.
    result = SelfDemagDx(dx,dy,dz);
  } else {
    // Simplified (collapsed) formula based on Newell's paper.
    // This saves about half the calls to f().  There is still
    // quite a bit of redundancy from one cell site to the next,
    // but as this is an initialization-only issue speed shouldn't
    // be too critical.
    OC_REALWIDE result1,result2,result4,result8;
    result1 =
      -1*(f(x+dx,y+dy,z+dz)+f(x+dx,y-dy,z+dz)+f(x+dx,y-dy,z-dz)+
	  f(x+dx,y+dy,z-dz)+f(x-dx,y+dy,z-dz)+f(x-dx,y+dy,z+dz)+
	  f(x-dx,y-dy,z+dz)+f(x-dx,y-dy,z-dz));
    result2 =
      2*(f(x,y-dy,z-dz)+f(x,y-dy,z+dz)+f(x,y+dy,z+dz)+f(x,y+dy,z-dz)+
	 f(x+dx,y+dy,z)+f(x+dx,y,z+dz)+f(x+dx,y,z-dz)+f(x+dx,y-dy,z)+
	 f(x-dx,y-dy,z)+f(x-dx,y,z+dz)+f(x-dx,y,z-dz)+f(x-dx,y+dy,z));
    result4 =
      -4*(f(x,y-dy,z)+f(x,y+dy,z)+f(x,y,z-dz)+
	  f(x,y,z+dz)+f(x+dx,y,z)+f(x-dx,y,z));
    result8 =
      8*f(x,y,z);
    result=result1+result2+result4+result8;
  }
  return result;
  /// Multiply result by 1./(4*PI*dx*dy*dz) to get effective "demag"
  /// factor Nxx from Newell's paper.
}
#else // OLD_CODE
static OC_REALWIDE
GetSDA00(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
	 OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ // This is Nxx*(4*PI*tau) in Newell's paper
  OC_REALWIDE result=0.;
  if(x==0. && y==0. && z==0.) {
    // Self demag term.  The base routine can handle x==y==z==0,
    // but this should be more accurate.
    result = SelfDemagDx(dx,dy,dz);
  } else {
    // Simplified (collapsed) formula based on Newell's paper.
    // This saves about half the calls to f().  There is still
    // quite a bit of redundancy from one cell site to the next,
    // but as this is an initialization-only issue speed shouldn't
    // be too critical.
    OC_REALWIDE arr[27];
    arr[ 0] = -1*f(x+dx,y+dy,z+dz);
    arr[ 1] = -1*f(x+dx,y-dy,z+dz);
    arr[ 2] = -1*f(x+dx,y-dy,z-dz);
    arr[ 3] = -1*f(x+dx,y+dy,z-dz);
    arr[ 4] = -1*f(x-dx,y+dy,z-dz);
    arr[ 5] = -1*f(x-dx,y+dy,z+dz);
    arr[ 6] = -1*f(x-dx,y-dy,z+dz);
    arr[ 7] = -1*f(x-dx,y-dy,z-dz);

    arr[ 8] =  2*f(x,y-dy,z-dz);
    arr[ 9] =  2*f(x,y-dy,z+dz);
    arr[10] =  2*f(x,y+dy,z+dz);
    arr[11] =  2*f(x,y+dy,z-dz);
    arr[12] =  2*f(x+dx,y+dy,z);
    arr[13] =  2*f(x+dx,y,z+dz);
    arr[14] =  2*f(x+dx,y,z-dz);
    arr[15] =  2*f(x+dx,y-dy,z);
    arr[16] =  2*f(x-dx,y-dy,z);
    arr[17] =  2*f(x-dx,y,z+dz);
    arr[18] =  2*f(x-dx,y,z-dz);
    arr[19] =  2*f(x-dx,y+dy,z);

    arr[20] = -4*f(x,y-dy,z);
    arr[21] = -4*f(x,y+dy,z);
    arr[22] = -4*f(x,y,z-dz);
    arr[23] = -4*f(x,y,z+dz);
    arr[24] = -4*f(x+dx,y,z);
    arr[25] = -4*f(x-dx,y,z);

    arr[26] =  8*f(x,y,z);

    result=AccurateSum(27,arr);
  }
  return result;
  /// Multiply result by 1./(4*PI*dx*dy*dz) to get effective "demag"
  /// factor Nxx from Newell's paper.
}
#endif // OLD_CODE

static inline OC_REALWIDE
GetSDA11(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,OC_REALWIDE l,OC_REALWIDE h,OC_REALWIDE e)
{ return GetSDA00(y,z,x,h,e,l); }

static inline OC_REALWIDE
GetSDA22(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,OC_REALWIDE l,OC_REALWIDE h,OC_REALWIDE e)
{ return GetSDA00(z,x,y,e,l,h); }

#ifdef OLD_CODE
static OC_REALWIDE
g(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // There is mucking around here to handle case where imports
  // are near zero.  In particular, asinh(t) is written as
  // log(t+sqrt(1+t)) because the latter appears easier to
  // handle if t=y/x (for example) as x -> 0.

  OC_REALWIDE result_sign=1.0;
  if(x<0.0) result_sign *= -1.0;  if(y<0.0) result_sign *= -1.0;
  x=fabs(x); y=fabs(y); z=fabs(z);  // This function is even in z and
  /// odd in x and y.  The fabs()'s simplify special case handling.

  OC_REALWIDE xsq=x*x,ysq=y*y,zsq=z*z;
  OC_REALWIDE R=xsq+ysq+zsq;
  if(R<=0.0) return 0.0;
  else       R=sqrt(R);

  // g(x,y,z)
  OC_REALWIDE result = -2*x*y*R;;
  if(z>0.) { // For 2D grids, 1/3 of the calls from GetSDA01 have z==0.
    OC_REALWIDE temp1,temp2,temp3;
    result += 
      -z*(zsq*Oc_Atan2(x*y,z*R)
	  +3.*(ysq*Oc_Atan2(x*z,y*R)+xsq*Oc_Atan2(y*z,x*R)));

    if((temp1=xsq+ysq)>0.)
      result += 6*x*y*z*log((z+R)/sqrt(temp1));

    if((temp2=ysq+zsq)>0.)
      result += y*(3*zsq-ysq)*log((x+R)/sqrt(temp2));

    if((temp3=xsq+zsq)>0.)
      result += x*(3*zsq-xsq)*log((y+R)/sqrt(temp3));

  } else {
    // z==0.
    if(y>0.) result += -y*ysq*log((x+R)/y);
    if(x>0.) result += -x*xsq*log((y+R)/x);
  }

  return (1./6.)*result_sign*result;

}
#else // OLD_CODE
static OC_REALWIDE
g(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // There is mucking around here to handle case where imports
  // are near zero.  In particular, asinh(t) is written as
  // log(t+sqrt(1+t)) because the latter appears easier to
  // handle if t=y/x (for example) as x -> 0.

  OC_REALWIDE result_sign=1.0;
  if(x<0.0) result_sign *= -1.0;
  if(y<0.0) result_sign *= -1.0;
  x=fabs(x); y=fabs(y); z=fabs(z);  // This function is even in z and
  /// odd in x and y.  The fabs()'s simplify special case handling.

  OC_REALWIDE xsq=x*x,ysq=y*y,zsq=z*z;
  OC_REALWIDE R=xsq+ysq+zsq;
  if(R<=0.0) return 0.0;
  else       R=sqrt(R);

  // g(x,y,z)
  OC_REALWIDE piece[7];
  int piececount=0;
  piece[piececount++] = -2*x*y*R;;
  if(z>0.) { // For 2D grids, 1/3 of the calls from GetSDA01 have z==0.
    piece[piececount++] = -z*zsq*Oc_Atan2(x*y,z*R);
    piece[piececount++] = -3*z*ysq*Oc_Atan2(x*z,y*R);
    piece[piececount++] = -3*z*xsq*Oc_Atan2(y*z,x*R);

    OC_REALWIDE temp1,temp2,temp3;
    if((temp1=xsq+ysq)>0.)
      piece[piececount++] = 6*x*y*z*log((z+R)/sqrt(temp1));

    if((temp2=ysq+zsq)>0.)
      piece[piececount++] = y*(3*zsq-ysq)*log((x+R)/sqrt(temp2));

    if((temp3=xsq+zsq)>0.)
      piece[piececount++] = x*(3*zsq-xsq)*log((y+R)/sqrt(temp3));

  } else {
    // z==0.
    if(y>0.) piece[piececount++] = -y*ysq*log((x+R)/y);
    if(x>0.) piece[piececount++] = -x*xsq*log((y+R)/x);
  }

  return (1./6.)*result_sign*AccurateSum(piececount,piece);
}
#endif

#ifdef OLD_CODE
static OC_REALWIDE
GetSDA01(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
	 OC_REALWIDE l,OC_REALWIDE h,OC_REALWIDE e)
{ // This is Nxy*(4*PI*tau) in Newell's paper.

  // Simplified (collapsed) formula based on Newell's paper.
  // This saves about half the calls to g().  There is still
  // quite a bit of redundancy from one cell site to the next,
  // but as this is an initialization-only issure speed shouldn't
  // be too critical.

  OC_REALWIDE result1 =
    -1*(g(x-l,y-h,z-e)+g(x-l,y-h,z+e)+g(x+l,y-h,z+e)+g(x+l,y-h,z-e)+
	g(x+l,y+h,z-e)+g(x+l,y+h,z+e)+g(x-l,y+h,z+e)+g(x-l,y+h,z-e));

  OC_REALWIDE result2 =
    2*(g(x,y+h,z-e)+g(x,y+h,z+e)+g(x,y-h,z+e)+g(x,y-h,z-e)+
       g(x-l,y-h,z)+g(x-l,y+h,z)+g(x-l,y,z-e)+g(x-l,y,z+e)+
       g(x+l,y,z+e)+g(x+l,y,z-e)+g(x+l,y-h,z)+g(x+l,y+h,z));

  OC_REALWIDE result4 =
    -4*(g(x-l,y,z)+g(x+l,y,z)+g(x,y,z+e)+g(x,y,z-e)+
	g(x,y-h,z)+g(x,y+h,z));

  OC_REALWIDE result8 =
    8*g(x,y,z);

  return result1+result2+result4+result8;
  /// Multiply result by 1./(4*PI*l*h*e) to get effective "demag"
  /// factor Nxy from Newell's paper.
}
#else
static OC_REALWIDE
GetSDA01(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
	 OC_REALWIDE l,OC_REALWIDE h,OC_REALWIDE e)
{ // This is Nxy*(4*PI*tau) in Newell's paper.

  // Simplified (collapsed) formula based on Newell's paper.
  // This saves about half the calls to g().  There is still
  // quite a bit of redundancy from one cell site to the next,
  // but as this is an initialization-only issure speed shouldn't
  // be too critical.
  OC_REALWIDE arr[27];

  arr[ 0] = -1*g(x-l,y-h,z-e);
  arr[ 1] = -1*g(x-l,y-h,z+e);
  arr[ 2] = -1*g(x+l,y-h,z+e);
  arr[ 3] = -1*g(x+l,y-h,z-e);
  arr[ 4] = -1*g(x+l,y+h,z-e);
  arr[ 5] = -1*g(x+l,y+h,z+e);
  arr[ 6] = -1*g(x-l,y+h,z+e);
  arr[ 7] = -1*g(x-l,y+h,z-e);

  arr[ 8] =  2*g(x,y+h,z-e);
  arr[ 9] =  2*g(x,y+h,z+e);
  arr[10] =  2*g(x,y-h,z+e);
  arr[11] =  2*g(x,y-h,z-e);
  arr[12] =  2*g(x-l,y-h,z);
  arr[13] =  2*g(x-l,y+h,z);
  arr[14] =  2*g(x-l,y,z-e);
  arr[15] =  2*g(x-l,y,z+e);
  arr[16] =  2*g(x+l,y,z+e);
  arr[17] =  2*g(x+l,y,z-e);
  arr[18] =  2*g(x+l,y-h,z);
  arr[19] =  2*g(x+l,y+h,z);

  arr[20] = -4*g(x-l,y,z);
  arr[21] = -4*g(x+l,y,z);
  arr[22] = -4*g(x,y,z+e);
  arr[23] = -4*g(x,y,z-e);
  arr[24] = -4*g(x,y-h,z);
  arr[25] = -4*g(x,y+h,z);

  arr[26] =  8*g(x,y,z);

  return AccurateSum(27,arr);
  /// Multiply result by 1./(4*PI*l*h*e) to get effective "demag"
  /// factor Nxy from Newell's paper.
}
#endif // OLD_CODE

////////////////////////////////////////////////////////////////////////////

static int NextPowerOfTwo(int n)
{ // Helper function.  Returns first power of two >= n
  int m=1;
  while(m<n) m*=2;
  return m;
}

// A couple helper macros
#define ALLOC2D(ARRAY,XDIM,YDIM,TYPE,I)                     \
           ARRAY=new TYPE*[XDIM];                           \
           ARRAY[0]=new TYPE[XDIM*YDIM];                    \
           for(I=1;I<XDIM;I++) ARRAY[I]=ARRAY[I-1]+YDIM;

#define DEALLOC2D(ARRAY) \
      if(ARRAY!=NULL) { delete[] ARRAY[0]; delete[] ARRAY; ARRAY=NULL; }


static void DeallocateArrays()
{ // Release memory used by file static arrays
  DEALLOC2D(spacework);
  DEALLOC2D(freqwork);
  DEALLOC2D(M0);   DEALLOC2D(M1);   DEALLOC2D(M2);
  DEALLOC2D(A00);  DEALLOC2D(A01);  DEALLOC2D(A11);  DEALLOC2D(A22);
}


static void AllocateArrays(int xdim,int ydim)
{ // Allocate memory for file static arrays, and sets Nx, Ny, NFx & NFy.
  DeallocateArrays();
  Nx=xdim;  Ny=ydim;

  // To get the correct "aperiodic" convolution from the FFT's,
  // it appears that the kernel needs to be expanded about zero,
  // and the input zero padded, to N + (N-1).  See the notes in
  // fft.h concerning the implicit zero padding of FFT's of real
  // functions.
  NFy=NextPowerOfTwo(Ny+Ny-1);
  NFx=(NextPowerOfTwo(Nx+Nx-1))/2+1; // For real inputs, the resulting
  /// complex transform is symmetric, so only the top half is computed.

  int i;
  ALLOC2D(spacework,Nx,Ny,double,i);

  ALLOC2D(freqwork,NFx,NFy,MyComplex,i);
  ALLOC2D(M0,NFx,NFy,MyComplex,i);
  ALLOC2D(M1,NFx,NFy,MyComplex,i);
  ALLOC2D(M2,NFx,NFy,MyComplex,i);

  ALLOC2D(A00,NFx,NFy,MyComplex,i);
  ALLOC2D(A01,NFx,NFy,MyComplex,i);
  ALLOC2D(A11,NFx,NFy,MyComplex,i);
  ALLOC2D(A22,NFx,NFy,MyComplex,i);
}

void
ConstMagInit(const int xdim,const int ydim,
	     int argc, char** argv)
{ // Allocates memory, calculates kernels, and sets global values
  // argv[0] should be "ConstMag", and argv[1] should be the ratio
  // film thickness/cellsize (Aspect).
  if(xdim<1 || ydim<1 || argc!=2 ||
     Nb_StrCaseCmp(argv[0],"ConstMag")!=0) {
    PlainError(1,
	    "FATAL ERROR: Invalid argument "
	    "(->%s<- with argc=%d, dim=(%d,%d))"
	    " to ConstMagInit()\n",argv[0],argc,xdim,ydim);
  }
#ifndef USE_MPI
  fft=new FFTReal2D;
#else
  fft=new FFTReal2D_mpi;
#endif
  Aspect=Nb_Atof(argv[1]);
  AllocateArrays(xdim,ydim);

  // Initialize kernel
  int i,j;
#ifdef FFT_DEBUG
#define R1 4
#define C1 1
#define R2 3
#define C2 1
  double *test[R1];      double testarr[R1*C1];
  test[0]=testarr; for(i=1;i<R1;i++) test[i]=test[i-1]+C1;
  MyComplex *ctest[R2];  MyComplex ctestarr[R2*C2];
  ctest[0]=ctestarr; for(i=1;i<R2;i++) ctest[i]=ctest[i-1]+C2;
  for(i=0;i<R1;i++) for(j=0;j<C1;j++) test[i][j]=1.0+i;
  for(i=0;i<R2;i++) for(j=0;j<C2;j++) ctest[i][j]=MyComplex(-77.777,-55.555);
  printf("\nInitial array---\n");
  for(i=0;i<R1;i++) {
    for(j=0;j<C1;j++) printf("%10.5f ",test[i][j]);
    printf("\n");
  }
  fft->Forward(R1,C1,test,R2,C2,ctest);
  printf("Transformed array---\n");
  for(i=0;i<R2;i++) {
    for(j=0;j<C2;j++)
      printf("(%6.2f,%6.2f) ",ctest[i][j].real(),ctest[i][j].imag());
    printf("\n");
  }
  for(i=0;i<R1;i++) for(j=0;j<C1;j++) test[i][j]=-42.424242;
  fft->Inverse(R2,C2,ctest,R1,C1,test);
  printf("F+I array---\n");
  for(i=0;i<R1;i++) {
    for(j=0;j<C1;j++) printf("%10.5f ",test[i][j]);
    printf("\n");
  }

  FFT fft1d;
  for(j=0;j<C1;j++) ctestarr[j]=MyComplex(1.0+j,0.);
  for(;j<C2;j++) ctestarr[j]=MyComplex(0.,0.);
  printf("\n\nInitial array---\n");
  for(j=0;j<C2;j++)
    printf("(%6.2f,%6.2f) ",ctestarr[j].real(),ctestarr[j].imag());
  fft1d.ForwardDecFreq(C2,ctestarr);
  printf("\nTransformed array---\n");
  for(j=0;j<C2;j++)
    printf("(%6.2f,%6.2f) ",ctestarr[j].real(),ctestarr[j].imag());
  fft1d.InverseDecTime(C2,ctestarr);
  printf("\nF+I array---\n");
  for(j=0;j<C2;j++)
    printf("(%6.2f,%6.2f) ",ctestarr[j].real(),ctestarr[j].imag());
  printf("\n"); fflush(stdout);

#endif /* DEBUG_MPI */

  int Kx=OC_MAX(1,2*(NFx-1));   // Space domain kernel dimensions
  int Ky=NFy;
  double **spacekernel;
  ALLOC2D(spacekernel,Kx,Ky,double,i);
  for(i=0;i<Kx;i++) for(j=0;j<Ky;j++) spacekernel[i][j]=0.;  // Zero fill

  OC_REALWIDE l,h,e,z,scale;
  l=h=1.0; e=Aspect; z=0.0;  // Scale size so distance between adjacent
                            /// cells is 1.
  scale= -1./(4*PI*l*h*e);
  /// According (16) in Newell's paper, the demag field is given by
  ///                        H = -N*M
  /// where N is the "demagnetizing tensor," with components Nxx, Nxy,
  /// etc.  With the '-1' in 'scale' we store '-N' instead of 'N',
  /// so we don't have to multiply the output from the FFT + iFFT
  /// by -1 in ConstMagField() below.

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    spacekernel[i][j]=scale*GetSDA00(OC_REALWIDE(i),OC_REALWIDE(j),z,l,h,e);
    if(i>0) spacekernel[Kx-i][j]=spacekernel[i][j];
    if(j>0) spacekernel[i][Ky-j]=spacekernel[i][j];
    if(i>0 && j>0) spacekernel[Kx-i][Ky-j]=spacekernel[i][j];
  }
  fft->Forward(Kx,Ky,(const double* const*)spacekernel,NFx,NFy,A00);

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    spacekernel[i][j]=scale*GetSDA11(OC_REALWIDE(i),OC_REALWIDE(j),z,l,h,e);
    if(i>0) spacekernel[Kx-i][j]=spacekernel[i][j];
    if(j>0) spacekernel[i][Ky-j]=spacekernel[i][j];
    if(i>0 && j>0) spacekernel[Kx-i][Ky-j]=spacekernel[i][j];
  }
  fft->Forward(Kx,Ky,(const double* const*)spacekernel,NFx,NFy,A11);

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    spacekernel[i][j]=scale*GetSDA22(OC_REALWIDE(i),OC_REALWIDE(j),z,l,h,e);
    if(i>0) spacekernel[Kx-i][j]=spacekernel[i][j];
    if(j>0) spacekernel[i][Ky-j]=spacekernel[i][j];
    if(i>0 && j>0) spacekernel[Kx-i][Ky-j]=spacekernel[i][j];
  }
  fft->Forward(Kx,Ky,(const double* const*)spacekernel,NFx,NFy,A22);

#ifdef DUMP_COEF_TEST
fprintf(stderr,"Nxy(1,2,3,1,2,3)=%.17g   Nxy(10,1,1,1,2,3)=%.17g\n",
	(double)GetSDA01(1.,2.,3.,1.,2.,3.)/(4*PI*1.*2.*3.),
	(double)GetSDA01(10.,1.,1.,1.,2.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(-1,2,3,1,2,3)=%.17g   Nxy(10,1,-1,1,2,3)=%.17g\n",
	(double)GetSDA01(-1.,2.,3.,1.,2.,3.)/(4*PI*1.*2.*3.),
	(double)GetSDA01(10.,1.,-1.,1.,2.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(2,1,3,2,1,3)=%.17g   Nxy(1,10,1,2,1,3)=%.17g\n",
	(double)GetSDA01(2.,1.,3.,2.,1.,3.)/(4*PI*1.*2.*3.),
	(double)GetSDA01(1.,10.,1.,2.,1.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(1,1,0,1,2,3)=%.17g   Nxy(1,1,0,2,1,3)=%.17g\n",
	(double)GetSDA01(1.,1.,0.,1.,2.,3.)/(4*PI*1.*2.*3.),
	(double)GetSDA01(1.,1.,0.,2.,1.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(-1,1,0,1,2,3)=%.17g   Nxy(1,-1,0,2,1,3)=%.17g\n",
	(double)GetSDA01(-1.,1.,0.,1.,2.,3.)/(4*PI*1.*2.*3.),
	(double)GetSDA01(1.,-1.,0.,2.,1.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(1,0,0,1,1,1)=%.17g   Nxy(0,0,1,1,2,3)=%.17g\n",
	(double)GetSDA01(1.,0.,0.,1.,1.,1.)/(4*PI*1.*1.*1.),
	(double)GetSDA01(0.,0.,1.,1.,2.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(-1,0,0,1,1,1)=%.17g   Nxy(0,0,-1,1,2,3)=%.17g\n",
	(double)GetSDA01(-1.,0.,0.,1.,1.,1.)/(4*PI*1.*1.*1.),
	(double)GetSDA01(0.,0.,-1.,1.,2.,3.)/(4*PI*1.*2.*3.));
#endif // DUMP_COEF_TEST

  for(i=0;i<Nx;i++) for(j=0;j<Ny;j++) {
    spacekernel[i][j]=scale*GetSDA01(OC_REALWIDE(i),OC_REALWIDE(j),z,l,h,e);
    if(i>0) spacekernel[Kx-i][j]= -1.0*spacekernel[i][j];
    if(j>0) spacekernel[i][Ky-j]= -1.0*spacekernel[i][j];
    if(i>0 && j>0) spacekernel[Kx-i][Ky-j]=spacekernel[i][j];
    // A01 is odd in x and y.
  }
  fft->Forward(Kx,Ky,(const double* const*)spacekernel,NFx,NFy,A01);
  DEALLOC2D(spacekernel);
}

void
ConstMagDestroy(void)
{
  DeallocateArrays();
  delete fft;
  fft=NULL;
}

void
ConstMagField(DF_Export_m* writem,DF_Import_h* readh,
	      void* m,void* h)
{ // Import functions writem and readh must be provided by caller,
  // and provide access into the hidden magnetization and field
  // structures.
  int i,j;

  // Take FFT of magnetization components.
  writem(spacework,m,Nx,Ny,0);
  fft->Forward(Nx,Ny,(const double* const*)spacework,NFx,NFy,M0);
  writem(spacework,m,Nx,Ny,1);
  fft->Forward(Nx,Ny,(const double* const*)spacework,NFx,NFy,M1);
  writem(spacework,m,Nx,Ny,2);
  fft->Forward(Nx,Ny,(const double* const*)spacework,NFx,NFy,M2);

  // Calculate H0 component
  for(i=0;i<NFx;i++) for(j=0;j<NFy;j++) {
    freqwork[i][j]=A00[i][j]*M0[i][j]+A01[i][j]*M1[i][j];
  }
  fft->Inverse(NFx,NFy,(const MyComplex* const*)freqwork,Nx,Ny,spacework);
  readh(spacework,h,Nx,Ny,0);

  // Calculate H1 component
  for(i=0;i<NFx;i++) for(j=0;j<NFy;j++) {
    freqwork[i][j]=A01[i][j]*M0[i][j]+A11[i][j]*M1[i][j];
  }
  fft->Inverse(NFx,NFy,(const MyComplex* const*)freqwork,Nx,Ny,spacework);
  readh(spacework,h,Nx,Ny,1);

  // Calculate H2 component
  for(i=0;i<NFx;i++) for(j=0;j<NFy;j++) {
    freqwork[i][j]=A22[i][j]*M2[i][j];
  }
  fft->Inverse(NFx,NFy,(const MyComplex* const*)freqwork,Nx,Ny,spacework);
  readh(spacework,h,Nx,Ny,2);
}
