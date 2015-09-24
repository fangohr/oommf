/* FILE: demag.cc            -*-Mode: c++-*-
 *
 * Average H demag field across rectangular cells.  This is a modified
 * version of the simpledemag class, which uses symmetries in the
 * interaction coefficients to reduce memory usage.
 *
 * The formulae used are reduced forms of equations in A. J. Newell,
 * W. Williams, and D. J. Dunlop, "A Generalization of the Demagnetizing
 * Tensor for Nonuniform Magnetization," Journal of Geophysical Research
 * - Solid Earth 98, 9551-9555 (1993).
 *
 * This code uses the Oxs_FFT3v classes to perform direct FFTs of the
 * import magnetization ThreeVectors.  This Oxs_Demag class is a
 * drop-in replacement for an older Oxs_Demag class that used the
 * scalar Oxs_FFT class.  That older class has been renamed
 * Oxs_DemagOld, and is contained in the demagold.* files.
 *
 * NOTE: This is the non-threaded implementation of the routines
 *       declared in demag.h.  This version is included iff the
 *       compiler macro OOMMF_THREADS is 0.  For the threaded
 *       version of this code, see demag-threaded.cc.
 */

#include "demag.h"  // Includes definition of OOMMF_THREADS macro
#include "demagcoef.h" // Used by both single-threaded code, and
/// also common single/multi-threaded code at bottom of this file.

////////////////// SINGLE-THREADED IMPLEMENTATION  ///////////////
#if !OOMMF_THREADS

#include <assert.h>
#include <string>

#include "nb.h"
#include "director.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

#include "rectangularmesh.h"

OC_USE_STRING;

/* End includes */

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_Demag);

#ifndef VERBOSE_DEBUG
# define VERBOSE_DEBUG 0
#endif

// Size of threevector.  This macro is defined for code legibility
// and maintenance; it should always be "3".
#define ODTV_VECSIZE 3

// Size of complex value, in real units.  This macro is defined for code
// legibility and maintenance; it should always be "2".
#define ODTV_COMPLEXSIZE 2

// Constructor
Oxs_Demag::Oxs_Demag(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    rdimx(0),rdimy(0),rdimz(0),cdimx(0),cdimy(0),cdimz(0),
    adimx(0),adimy(0),adimz(0),
    xperiodic(0),yperiodic(0),zperiodic(0),
    mesh_id(0),
    A(0),Hxfrm(0),asymptotic_radius(-1),Mtemp(0),
    embed_convolution(0),embed_block_size(0)
{
  asymptotic_radius = GetRealInitValue("asymptotic_radius",32.0);
  /// Units of (dx*dy*dz)^(1/3) (geometric mean of cell dimensions).
  /// Value of -1 disables use of asymptotic approximation on
  /// non-periodic grids.  For periodic grids zero or negative values
  /// for asymptotic_radius are reset to half the width of the
  /// simulation window in the periodic dimenions.  This may be
  /// counterintuitive, so it might be better to disallow or modify
  /// the behavior in the periodic setting.

  cache_size = 1024*GetIntInitValue("cache_size_KB",1024);
  /// Cache size in KB.  Default is 1 MB.  Code wants bytes, so multiply
  /// user input by 1024.  cache_size is used to set embed_block_size in
  /// FillCoefficientArrays member function.

  zero_self_demag = GetIntInitValue("zero_self_demag",0);
  /// If true, then diag(1/3,1/3,1/3) is subtracted from the self-demag
  /// term.  In particular, for cubic cells this makes the self-demag
  /// field zero.  This will change the value computed for the demag
  /// energy by a constant amount, but since the demag field is changed
  /// by a multiple of m, the torque and therefore the magnetization
  /// dynamics are unaffected.

  VerifyAllInitArgsUsed();
}

Oxs_Demag::~Oxs_Demag() {
#if REPORT_TIME
  Oc_TimeVal cpu,wall;

  inittime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...   init%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  fftforwardtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...  f-fft%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  fftinversetime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...  i-fft%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  fftxforwardtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ... f-fftx%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  fftxinversetime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ... i-fftx%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  fftyforwardtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ... f-ffty%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  fftyinversetime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ... i-ffty%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  convtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...   conv%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  dottime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...    dot%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  for(int i=0;i<dvltimer_number;++i) {
    dvltimer[i].GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"      subtime ... dvl[%d]%7.2f cpu /%7.2f wall,"
              " (%.1000s)\n",
              i,double(cpu),double(wall),InstanceName());
    }
  }

#endif // REPORT_TIME
  ReleaseMemory();
}

OC_BOOL Oxs_Demag::Init()
{
#if REPORT_TIME
  Oc_TimeVal cpu,wall;

  inittime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...   init%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  fftforwardtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...  f-fft%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  fftinversetime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...  i-fft%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  fftxforwardtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ... f-fftx%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  fftxinversetime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ... i-fftx%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  fftyforwardtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ... f-ffty%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  fftyinversetime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ... i-ffty%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  convtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...   conv%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  dottime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...    dot%7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  for(int i=0;i<dvltimer_number;++i) {
    dvltimer[i].GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"      subtime ... dvl[%d]%7.2f cpu /%7.2f wall,"
              " (%.1000s)\n",
              i,double(cpu),double(wall),InstanceName());
    }
    dvltimer[i].Reset();
  }

  inittime.Reset();
  fftforwardtime.Reset();
  fftinversetime.Reset();
  fftxforwardtime.Reset();
  fftxinversetime.Reset();
  fftyforwardtime.Reset();
  fftyinversetime.Reset();
  convtime.Reset();
  dottime.Reset();
#endif // REPORT_TIME
  mesh_id = 0;
  ReleaseMemory();
  return Oxs_Energy::Init();
}

void Oxs_Demag::ReleaseMemory() const
{ // Conceptually const
  if(A!=0) { delete[] A; A=0; }
  if(Hxfrm!=0)       { delete[] Hxfrm;       Hxfrm=0;       }
  if(Mtemp!=0)       { delete[] Mtemp;       Mtemp=0;       }
  rdimx=rdimy=rdimz=0;
  cdimx=cdimy=cdimz=0;
  adimx=adimy=adimz=0;
}

void Oxs_Demag::FillCoefficientArrays(const Oxs_Mesh* genmesh) const
{ // This routine is conceptually const.

  // Oxs_Demag requires a rectangular mesh
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(genmesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(genmesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(this,msg);
  }

  // Check periodicity
  const Oxs_RectangularMesh* rmesh 
    = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh!=NULL) {
    // Rectangular, periodic mesh
    xperiodic = pmesh->IsPeriodicX();
    yperiodic = pmesh->IsPeriodicY();
    zperiodic = pmesh->IsPeriodicZ();

    // Check for supported periodicity
    if(xperiodic+yperiodic+zperiodic>2) {
      String msg=String("Periodic mesh ")
        + String(genmesh->InstanceName())
        + String("is 3D periodic, which is not supported by Oxs_Demag."
                 "  Select no more than two of x, y, or z.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    if(xperiodic+yperiodic+zperiodic>1) {
      String msg=String("Periodic mesh ")
        + String(genmesh->InstanceName())
      + String("is 2D periodic, which is not supported by Oxs_Demag"
               " at this time.");
      throw Oxs_ExtError(this,msg.c_str());
    }
  } else if (rmesh!=NULL) {
    // Rectangular, non-periodic mesh
    xperiodic=0; yperiodic=0; zperiodic=0;
  } else {
    String msg=String("Unknown mesh type: \"")
      + String(ClassName())
      + String("\".");
    throw Oxs_ExtError(this,msg.c_str());
  }

  // Clean-up from previous allocation, if any.
  ReleaseMemory();

#if REPORT_TIME
    inittime.Start();
#endif // REPORT_TIME
  // Fill dimension variables
  rdimx = mesh->DimX();
  rdimy = mesh->DimY();
  rdimz = mesh->DimZ();
  if(rdimx==0 || rdimy==0 || rdimz==0) return; // Empty mesh!

  // Initialize fft object.  If a dimension equals 1, then zero
  // padding is not required.  Otherwise, zero pad to at least
  // twice the dimension.
  // NOTE: This is not coded yet, but if periodic is requested and
  // dimension is directly compatible with FFT (e.g., the dimension
  // is a power-of-2) then zero padding is not required.
  Oxs_FFT3DThreeVector::RecommendDimensions((rdimx==1 ? 1 : 2*rdimx),
                                            (rdimy==1 ? 1 : 2*rdimy),
                                            (rdimz==1 ? 1 : 2*rdimz),
                                            cdimx,cdimy,cdimz);
  OC_INDEX xfrm_size = ODTV_VECSIZE * 2 * cdimx * cdimy * cdimz;
  // "ODTV_VECSIZE" here is because we work with arrays if ThreeVectors,
  // and "2" because these are complex (as opposed to real)
  // quantities.
  if(xfrm_size<cdimx || xfrm_size<cdimy || xfrm_size<cdimz ||
     long(xfrm_size) != 
     long(2*ODTV_VECSIZE)*long(cdimx)*long(cdimy)*long(cdimz)) {
    // Partial overflow check
    char msgbuf[1024];
    Oc_Snprintf(msgbuf,sizeof(msgbuf),
                ": Product 2*ODTV_VECSIZE*cdimx*cdimy*cdimz = "
                "2*%d*%d*%d*%d too big to fit in a OC_INDEX variable",
                ODTV_VECSIZE,cdimx,cdimy,cdimz);
    String msg =
      String("OC_INDEX overflow in ")
      + String(InstanceName())
      + String(msgbuf);
    throw Oxs_ExtError(this,msg);
  }

  Mtemp = new OXS_FFT_REAL_TYPE[ODTV_VECSIZE*rdimx*rdimy*rdimz];
  /// Temporary space to hold Ms[]*m[].  The plan is to make this space
  /// unnecessary by introducing FFT functions that can take Ms as input
  /// and do the multiplication on the fly.


  // The following 3 statements are cribbed from
  // Oxs_FFT3DThreeVector::SetDimensions().  The corresponding
  // code using that class is
  //
  //  Oxs_FFT3DThreeVector fft;
  //  fft.SetDimensions(rdimx,rdimy,rdimz,cdimx,cdimy,cdimz);
  //  fft.GetLogicalDimensions(ldimx,ldimy,ldimz);
  //
  fftx.SetDimensions(rdimx, (cdimx==1 ? 1 : 2*(cdimx-1)), rdimy);
  ffty.SetDimensions(rdimy, cdimy,
                     ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx,
                     ODTV_VECSIZE*cdimx);
  fftz.SetDimensions(rdimz, cdimz,
                     ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy,
                     ODTV_VECSIZE*cdimx*cdimy);
  OC_INDEX ldimx,ldimy,ldimz; // Logical dimensions
  // The following 3 statements are cribbed from
  // Oxs_FFT3DThreeVector::GetLogicalDimensions()
  ldimx = fftx.GetLogicalDimension();
  ldimy = ffty.GetLogicalDimension();
  ldimz = fftz.GetLogicalDimension();

  adimx = (ldimx/2)+1;
  adimy = (ldimy/2)+1;
  adimz = (ldimz/2)+1;

#if VERBOSE_DEBUG && !defined(NDEBUG)
  fprintf(stderr,"RDIMS: (%d,%d,%d)\n",rdimx,rdimy,rdimz); /**/
  fprintf(stderr,"CDIMS: (%d,%d,%d)\n",cdimx,cdimy,cdimz); /**/
  fprintf(stderr,"LDIMS: (%d,%d,%d)\n",ldimx,ldimy,ldimz); /**/
  fprintf(stderr,"ADIMS: (%d,%d,%d)\n",adimx,adimy,adimz); /**/
#endif // NDEBUG

  // Dimension of array necessary to hold 3 sets of full interaction
  // coefficients in real space:
  OC_INDEX scratch_size = ODTV_VECSIZE * ldimx * ldimy * ldimz;
  if(scratch_size<ldimx || scratch_size<ldimy || scratch_size<ldimz) {
    // Partial overflow check
    String msg =
      String("OC_INDEX overflow in ")
      + String(InstanceName())
      + String(": Product 3*8*rdimx*rdimy*rdimz too big"
               " to fit in a OC_INDEX variable");
    throw Oxs_ExtError(this,msg);
  }

  // Allocate memory for FFT xfrm target H, and scratch space
  // for computing interaction coefficients
  Hxfrm = new OXS_FFT_REAL_TYPE[xfrm_size];
  OXS_FFT_REAL_TYPE* scratch = new OXS_FFT_REAL_TYPE[scratch_size];

  if(Hxfrm==NULL || scratch==NULL) {
    // Safety check for those machines on which new[] doesn't throw
    // BadAlloc.
    String msg = String("Insufficient memory in Demag setup.");
    throw Oxs_ExtError(this,msg);
  }

  // According (16) in Newell's paper, the demag field is given by
  //                        H = -N*M
  // where N is the "demagnetizing tensor," with components Nxx, Nxy,
  // etc.  With the '-1' in 'scale' we store '-N' instead of 'N',
  // so we don't have to multiply the output from the FFT + iFFT
  // by -1 GetEnergy() below.

  // Fill interaction matrices with demag coefs from Newell's paper.
  // Note that A00, A11 and A22 are even in x,y and z.
  // A01 is odd in x and y, even in z.
  // A02 is odd in x and z, even in y.
  // A12 is odd in y and z, even in x.
  // We use these symmetries to reduce storage requirements.  If
  // f is real and even, then f^ is also real and even.  If f
  // is real and odd, then f^ is (pure) imaginary and odd.
  // As a result, the transform of each of the A## interaction
  // matrices will be real, with the same even/odd properties.
  //
  // Notation:  A00:=fs*Nxx, A01:=fs*Nxy, A02:=fs*Nxz,
  //                         A11:=fs*Nyy, A12:=fs*Nyz
  //                                      A22:=fs*Nzz
  //  where fs = -1/((ldimx/2)*ldimy*ldimz)

  OC_REAL8m dx = mesh->EdgeLengthX();
  OC_REAL8m dy = mesh->EdgeLengthY();
  OC_REAL8m dz = mesh->EdgeLengthZ();
  // For demag calculation, all that matters is the relative
  // size of dx, dy and dz.  If possible, rescale these to
  // integers, as this may help reduce floating point error
  // a wee bit.  If not possible, then rescale so largest
  // value is 1.0.
  {
    OC_REALWIDE p1,q1,p2,q2;
    if(Nb_FindRatApprox(dx,dy,1e-12,1000,p1,q1)
       && Nb_FindRatApprox(dz,dy,1e-12,1000,p2,q2)) {
      OC_REALWIDE gcd = Nb_GcdRW(q1,q2);
      dx = p1*q2/gcd;
      dy = q1*q2/gcd;
      dz = p2*q1/gcd;
    } else {
      OC_REALWIDE maxedge=dx;
      if(dy>maxedge) maxedge=dy;
      if(dz>maxedge) maxedge=dz;
      dx/=maxedge; dy/=maxedge; dz/=maxedge;
    }
  }
  OC_REALWIDE scale = 1.0/(4*WIDE_PI*dx*dy*dz);
  const OXS_DEMAG_REAL_ASYMP scaled_arad = asymptotic_radius
    * Oc_Pow(OXS_DEMAG_REAL_ASYMP(dx*dy*dz),
             OXS_DEMAG_REAL_ASYMP(1.)/OXS_DEMAG_REAL_ASYMP(3.));

  // Also throw in FFT scaling.  This allows the "NoScale" FFT routines
  // to be used.  NB: There is effectively a "-1" built into the
  // differencing sections below, because we compute d^6/dx^2 dy^2 dz^2
  // instead of -d^6/dx^2 dy^2 dz^2 as required.
  // Note: Using an Oxs_FFT3DThreeVector fft object, this would be just
  //    scale *= fft.GetScaling();
  scale *= fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();

  OC_INDEX i,j,k;
  OC_INDEX sstridey = ODTV_VECSIZE*ldimx;
  OC_INDEX sstridez = sstridey*ldimy;
  OC_INDEX kstop=1; if(rdimz>1) kstop=rdimz+2;
  OC_INDEX jstop=1; if(rdimy>1) jstop=rdimy+2;
  OC_INDEX istop=1; if(rdimx>1) istop=rdimx+2;
  if(scaled_arad>0) {
    // We don't need to compute analytic formulae outside
    // asymptotic radius
    OC_INDEX ktest = static_cast<OC_INDEX>(Oc_Ceil(scaled_arad/dz))+2;
    if(ktest<kstop) kstop = ktest;
    OC_INDEX jtest = static_cast<OC_INDEX>(Oc_Ceil(scaled_arad/dy))+2;
    if(jtest<jstop) jstop = jtest;
    OC_INDEX itest = static_cast<OC_INDEX>(Oc_Ceil(scaled_arad/dx))+2;
    if(itest<istop) istop = itest;
  }

#if REPORT_TIME
  dvltimer[0].Start();
#endif // REPORT_TIME
  if(!xperiodic && !yperiodic && !zperiodic) {
    // Calculate Nxx, Nxy and Nxz in first octant, non-periodic case.
    // Step 1: Evaluate f & g at each cell site.  Offset by (-dx,-dy,-dz)
    //  so we can do 2nd derivative operations "in-place".
    for(k=0;k<kstop;k++) {
      OC_INDEX kindex = k*sstridez;
      OC_REALWIDE z = dz*(k-1);
      for(j=0;j<jstop;j++) {
        OC_INDEX jkindex = kindex + j*sstridey;
        OC_REALWIDE y = dy*(j-1);
        for(i=0;i<istop;i++) {
          OC_INDEX index = ODTV_VECSIZE*i+jkindex;
#ifndef NDEBUG
          if(index>=scratch_size) {
            String msg = String("Programming error:"
                                " array index out-of-bounds.");
            throw Oxs_ExtError(this,msg);
          }
#endif // NDEBUG
          OC_REALWIDE x = dx*(i-1);
          // Nyy(x,y,z) = Nxx(y,x,z);  Nzz(x,y,z) = Nxx(z,y,x);
          // Nxz(x,y,z) = Nxy(x,z,y);  Nyz(x,y,z) = Nxy(y,z,x);
          scratch[index]   = scale*Oxs_Newell_f(x,y,z);  // For Nxx
          scratch[index+1] = scale*Oxs_Newell_g(x,y,z);  // For Nxy
          scratch[index+2] = scale*Oxs_Newell_g(x,z,y);  // For Nxz
        }
      }
    }

    // Step 2a: Do d^2/dz^2
    if(kstop==1) {
      // Only 1 layer in z-direction of f/g stored in scratch array.
      for(j=0;j<jstop;j++) {
        OC_INDEX jkindex = j*sstridey;
        OC_REALWIDE y = dy*(j-1);
        for(i=0;i<istop;i++) {
          OC_INDEX index = ODTV_VECSIZE*i+jkindex;
          OC_REALWIDE x = dx*(i-1);
          // Function f is even in each variable, so for example
          //    f(x,y,-dz) - 2f(x,y,0) + f(x,y,dz)
          //        =  2( f(x,y,-dz) - f(x,y,0) )
          // Function g(x,y,z) is even in z and odd in x and y,
          // so for example
          //    g(x,-dz,y) - 2g(x,0,y) + g(x,dz,y)
          //        =  2g(x,0,y) = 0.
          // Nyy(x,y,z) = Nxx(y,x,z);  Nzz(x,y,z) = Nxx(z,y,x);
          // Nxz(x,y,z) = Nxy(x,z,y);  Nyz(x,y,z) = Nxy(y,z,x);
          scratch[index]   -= scale*Oxs_Newell_f(x,y,0);
          scratch[index]   *= 2;
          scratch[index+1] -= scale*Oxs_Newell_g(x,y,0);
          scratch[index+1] *= 2;
          scratch[index+2] = 0;
        }
      }
    } else {
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        for(j=0;j<jstop;j++) {
          OC_INDEX jkindex = kindex + j*sstridey;
          for(i=0;i<istop;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index]   += -2*scratch[index+sstridez]
              + scratch[index+2*sstridez];
            scratch[index+1] += -2*scratch[index+sstridez+1]
              + scratch[index+2*sstridez+1];
            scratch[index+2] += -2*scratch[index+sstridez+2]
              + scratch[index+2*sstridez+2];
          }
        }
      }
    }
    // Step 2b: Do d^2/dy^2
    if(jstop==1) {
      // Only 1 layer in y-direction of f/g stored in scratch array.
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        OC_REALWIDE z = dz*k;
        for(i=0;i<istop;i++) {
          OC_INDEX index = ODTV_VECSIZE*i+kindex;
          OC_REALWIDE x = dx*(i-1);
          // Function f is even in each variable, so for example
          //    f(x,y,-dz) - 2f(x,y,0) + f(x,y,dz)
          //        =  2( f(x,y,-dz) - f(x,y,0) )
          // Function g(x,y,z) is even in z and odd in x and y,
          // so for example
          //    g(x,-dz,y) - 2g(x,0,y) + g(x,dz,y)
          //        =  2g(x,0,y) = 0.
          // Nyy(x,y,z) = Nxx(y,x,z);  Nzz(x,y,z) = Nxx(z,y,x);
          // Nxz(x,y,z) = Nxy(x,z,y);  Nyz(x,y,z) = Nxy(y,z,x);
          scratch[index]   -= scale * 
            ((Oxs_Newell_f(x,0,z-dz)+Oxs_Newell_f(x,0,z+dz))
             -2*Oxs_Newell_f(x,0,z));
          scratch[index]   *= 2;
          scratch[index+1]  = 0.0;
          scratch[index+2] -= scale * 
            ((Oxs_Newell_g(x,z-dz,0)+Oxs_Newell_g(x,z+dz,0))
             -2*Oxs_Newell_g(x,z,0));
          scratch[index+2] *= 2;
        }
      }
    } else {
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        for(j=0;j<rdimy;j++) {
          OC_INDEX jkindex = kindex + j*sstridey;
          for(i=0;i<istop;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index]   += -2*scratch[index+sstridey]
              + scratch[index+2*sstridey];
            scratch[index+1] += -2*scratch[index+sstridey+1]
              + scratch[index+2*sstridey+1];
            scratch[index+2] += -2*scratch[index+sstridey+2]
              + scratch[index+2*sstridey+2];
          }
        }
      }
    }

    // Step 2c: Do d^2/dx^2
    if(istop==1) {
      // Only 1 layer in x-direction of f/g stored in scratch array.
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        OC_REALWIDE z = dz*k;
        for(j=0;j<rdimy;j++) {
          OC_INDEX index = kindex + j*sstridey;
          OC_REALWIDE y = dy*j;
          // Function f is even in each variable, so for example
          //    f(x,y,-dz) - 2f(x,y,0) + f(x,y,dz)
          //        =  2( f(x,y,-dz) - f(x,y,0) )
          // Function g(x,y,z) is even in z and odd in x and y,
          // so for example
          //    g(x,-dz,y) - 2g(x,0,y) + g(x,dz,y)
          //        =  2g(x,0,y) = 0.
          // Nyy(x,y,z) = Nxx(y,x,z);  Nzz(x,y,z) = Nxx(z,y,x);
          // Nxz(x,y,z) = Nxy(x,z,y);  Nyz(x,y,z) = Nxy(y,z,x);
          scratch[index]   -= scale * 
            ((4*Oxs_Newell_f(0,y,z)
              +Oxs_Newell_f(0,y+dy,z+dz)+Oxs_Newell_f(0,y-dy,z+dz)
              +Oxs_Newell_f(0,y+dy,z-dz)+Oxs_Newell_f(0,y-dy,z-dz))
             -2*(Oxs_Newell_f(0,y+dy,z)+Oxs_Newell_f(0,y-dy,z)
                 +Oxs_Newell_f(0,y,z+dz)+Oxs_Newell_f(0,y,z-dz)));
          scratch[index]   *= 2;                       // For Nxx
          scratch[index+2]  = scratch[index+1] = 0.0;  // For Nxy & Nxz
        }
      }
    } else {
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        for(j=0;j<rdimy;j++) {
          OC_INDEX jkindex = kindex + j*sstridey;
          for(i=0;i<rdimx;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index]   += -2*scratch[index+  ODTV_VECSIZE]
              + scratch[index+2*ODTV_VECSIZE];
            scratch[index+1] += -2*scratch[index+  ODTV_VECSIZE+1]
              + scratch[index+2*ODTV_VECSIZE+1];
            scratch[index+2] += -2*scratch[index+  ODTV_VECSIZE+2]
              + scratch[index+2*ODTV_VECSIZE+2];
          }
        }
      }
    }

    // Special "SelfDemag" code may be more accurate at index 0,0,0.
    // Note: Using an Oxs_FFT3DThreeVector fft object, this would be
    //    scale *= fft.GetScaling();
    const OXS_FFT_REAL_TYPE selfscale
      = -1 * fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    scratch[0] = Oxs_SelfDemagNx(dx,dy,dz);
    if(zero_self_demag) scratch[0] -= 1./3.;
    scratch[0] *= selfscale;

    scratch[1] = 0.0;  // Nxy[0] = 0.

    scratch[2] = 0.0;  // Nxz[0] = 0.

    // Step 2.5: Use asymptotic (dipolar + higher) approximation for far field
    /*   Dipole approximation:
     *
     *                        / 3x^2-R^2   3xy       3xz    \
     *             dx.dy.dz   |                             |
     *  H_demag = ----------- |   3xy   3y^2-R^2     3yz    |
     *             4.pi.R^5   |                             |
     *                        \   3xz      3yz     3z^2-R^2 /
     */
    // See Notes IV, 26-Feb-2007, p102.
    if(scaled_arad>=0.0) {
      // Note that all distances here are in "reduced" units,
      // scaled so that dx, dy, and dz are either small integers
      // or else close to 1.0.
      OXS_DEMAG_REAL_ASYMP scaled_arad_sq = scaled_arad*scaled_arad;
      OXS_FFT_REAL_TYPE fft_scaling = -1 *
        fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
      /// Note: Since H = -N*M, and by convention with the rest of this
      /// code, we store "-N" instead of "N" so we don't have to multiply
      /// the output from the FFT + iFFT by -1 in GetEnergy() below.

      OXS_DEMAG_REAL_ASYMP xtest
        = static_cast<OXS_DEMAG_REAL_ASYMP>(rdimx)*dx;
      xtest *= xtest;

      Oxs_DemagNxxAsymptotic ANxx(dx,dy,dz);
      Oxs_DemagNxyAsymptotic ANxy(dx,dy,dz);
      Oxs_DemagNxzAsymptotic ANxz(dx,dy,dz);

      for(k=0;k<rdimz;++k) {
        OC_INDEX kindex = k*sstridez;
        OXS_DEMAG_REAL_ASYMP z = dz*k;
        OXS_DEMAG_REAL_ASYMP zsq = z*z;
        for(j=0;j<rdimy;++j) {
          OC_INDEX jkindex = kindex + j*sstridey;
          OXS_DEMAG_REAL_ASYMP y = dy*j;
          OXS_DEMAG_REAL_ASYMP ysq = y*y;

          OC_INDEX istart = 0;
          OXS_DEMAG_REAL_ASYMP test = scaled_arad_sq-ysq-zsq;
          if(test>0) {
            if(test>xtest) {
              istart = rdimx+1;
            } else {
              istart = static_cast<OC_INDEX>(Oc_Ceil(Oc_Sqrt(test)/dx));
            }
          }
          for(i=istart;i<rdimx;++i) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            scratch[index]   = fft_scaling*ANxx.NxxAsymptotic(x,y,z);
            scratch[index+1] = fft_scaling*ANxy.NxyAsymptotic(x,y,z);
            scratch[index+2] = fft_scaling*ANxz.NxzAsymptotic(x,y,z);
          }
        }
      }
#if 0
      fprintf(stderr,"ANxx(%d,%d,%d) = %#.16g (non-threaded)\n",
              int(rdimx-1),int(rdimy-1),int(rdimz-1),
              (double)ANxx.NxxAsymptotic(dx*(rdimx-1),
                                         dy*(rdimy-1),dz*(rdimz-1)));
      OC_INDEX icheck = ODTV_VECSIZE*(rdimx-1)
        + (rdimy-1)*sstridey + (rdimz-1)*sstridez;
      fprintf(stderr,"fft_scaling=%g, product=%#.16g\n",
              (double)fft_scaling,(double)scratch[icheck]);
#endif
    }
#if 0
    {
      OXS_FFT_REAL_TYPE fft_scaling = -1 *
        fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
      OXS_DEMAG_REAL_ASYMP gamma
        = Oc_Pow(OXS_DEMAG_REAL_ASYMP(dx*dy*dz),
                 OXS_DEMAG_REAL_ASYMP(1.)/OXS_DEMAG_REAL_ASYMP(3.));
      fprintf(stderr,"dx=%Lg, dy=%Lg, dz=%Lg\n",dx,dy,dz);
      OC_INDEX qi,qj,qk,qo;
      OXS_DEMAG_REAL_ASYMP qd;
#define FOO(QA,QB,QC) \
      qi = (OC_INDEX)Oc_Floor(0.5+(QA)*gamma/dx), \
        qj = (OC_INDEX)Oc_Floor(0.5+(QB)*gamma/dy),   \
        qk = (OC_INDEX)Oc_Floor(0.5+(QC)*gamma/dz),     \
        qo = ODTV_VECSIZE*qi + qj*sstridey + qk*sstridez, \
        qd = Oc_Sqrt((qi*dx)*(qi*dx)+(qj*dy)*(qj*dy)+(qk*dz)*(qk*dz))/gamma; \
      fprintf(stderr,"Nxx/Nxy(%3ld,%3ld,%3ld) (dist %24.16Le) = %24.16Le    %24.16Le\n", \
              qi,qj,qk,qd,scratch[qo]/fft_scaling,scratch[qo+1]/fft_scaling)
      FOO(7,3,2);
      FOO(15,7,4);
      FOO(22,10,6);
      FOO(30,14,8);
      FOO(45,21,12);
      FOO(60,28,16);
      FOO(90,42,24);
      FOO(120,56,32);
      FOO(240,112,64);
      // FOO(480,224,128);
    }
#endif
  }

  // Step 2.6: If periodic boundaries selected, compute periodic tensors
  // instead.
  // NOTE THAT CODE BELOW CURRENTLY ONLY SUPPORTS 1D PERIODIC!!!
  // NOTE 2: Keep this code in sync with that in
  //         Oxs_Demag::IncrementPreconditioner
#if 1
SDA00_count = 0;
SDA01_count = 0;
#endif
  if(xperiodic) {
    OXS_FFT_REAL_TYPE fft_scaling = -1 *
      fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    /// Note: Since H = -N*M, and by convention with the rest of this
    /// code, we store "-N" instead of "N" so we don't have to multiply
    /// the output from the FFT + iFFT by -1 in GetEnergy() below.

    Oxs_DemagPeriodicX pbctensor(dx,dy,dz,rdimx*dx,scaled_arad);

    for(k=0;k<rdimz;++k) {
      OC_INDEX kindex = k*sstridez;
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(j=0;j<rdimy;++j) {
        OC_INDEX jkindex = kindex + j*sstridey;
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz;

        i=0;
        OXS_DEMAG_REAL_ASYMP x = dx*i;
        OC_INDEX index = ODTV_VECSIZE*i+jkindex;
        pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
        scratch[index]   = fft_scaling*Nxx;
        scratch[index+1] = fft_scaling*Nxy;
        scratch[index+2] = fft_scaling*Nxz;

        for(i=1;2*i<rdimx;++i) {
          // Interior i; reflect results from left to right half
          x = dx*i;
          index = ODTV_VECSIZE*i+jkindex;
          OC_INDEX rindex = ODTV_VECSIZE*(rdimx-i)+jkindex;
          pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
          scratch[index]   = fft_scaling*Nxx; // pbctensor computation
          scratch[index+1] = fft_scaling*Nxy; // *includes* base window
          scratch[index+2] = fft_scaling*Nxz; // term
          scratch[rindex]   =    scratch[index];   // Nxx is even
          scratch[rindex+1] = -1*scratch[index+1]; // Nxy is odd wrt x
          scratch[rindex+2] = -1*scratch[index+2]; // Nxz is odd wrt x
        }

        if(rdimx%2 == 0) { // Do midpoint seperately
          i = rdimx/2;
          x = dx*i;
          index = ODTV_VECSIZE*i+jkindex;
          pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
          scratch[index]   = fft_scaling*Nxx;
          scratch[index+1] = fft_scaling*Nxy;
          scratch[index+2] = fft_scaling*Nxz;
        }

      }
    }
  }
  if(yperiodic) {
    OXS_FFT_REAL_TYPE fft_scaling = -1 *
      fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    /// Note: Since H = -N*M, and by convention with the rest of this
    /// code, we store "-N" instead of "N" so we don't have to multiply
    /// the output from the FFT + iFFT by -1 in GetEnergy() below.

    Oxs_DemagPeriodicY pbctensor(dx,dy,dz,rdimy*dy,scaled_arad);

    for(k=0;k<rdimz;++k) {
      OC_INDEX kindex = k*sstridez;
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(j=0;j<=rdimy/2;++j) {
        OC_INDEX jkindex = kindex + j*sstridey;
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        if(0<j && 2*j<rdimy) {
          // Interior j; reflect results from lower to upper half
          OC_INDEX rjkindex = kindex + (rdimy-j)*sstridey;
          for(i=0;i<rdimx;++i) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OC_INDEX rindex = ODTV_VECSIZE*i+rjkindex;
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz;
            pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
            scratch[index]   = fft_scaling*Nxx;
            scratch[index+1] = fft_scaling*Nxy;
            scratch[index+2] = fft_scaling*Nxz;
            scratch[rindex]   =    scratch[index];   // Nxx is even
            scratch[rindex+1] = -1*scratch[index+1]; // Nxy is odd wrt y
            scratch[rindex+2] =    scratch[index+2]; // Nxz is even wrt y
          }
        } else { // j==0 or midpoint
          for(i=0;i<rdimx;++i) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz;
            pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
            scratch[index]   = fft_scaling*Nxx;
            scratch[index+1] = fft_scaling*Nxy;
            scratch[index+2] = fft_scaling*Nxz;
          }
        }
      }
    }
  }
  if(zperiodic) {
    OXS_FFT_REAL_TYPE fft_scaling = -1 *
      fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    /// Note: Since H = -N*M, and by convention with the rest of this
    /// code, we store "-N" instead of "N" so we don't have to multiply
    /// the output from the FFT + iFFT by -1 in GetEnergy() below.

    Oxs_DemagPeriodicZ pbctensor(dx,dy,dz,rdimz*dz,scaled_arad);

    for(k=0;k<=rdimz/2;++k) {
      OC_INDEX kindex = k*sstridez;
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      if(0<k && 2*k<rdimz) {
        // Interior k; reflect results from lower to upper half
        OC_INDEX rkindex = (rdimz-k)*sstridez;
        for(j=0;j<rdimy;++j) {
          OC_INDEX jkindex = kindex + j*sstridey;
          OC_INDEX rjkindex = rkindex + j*sstridey;
          OXS_DEMAG_REAL_ASYMP y = dy*j;
          for(i=0;i<rdimx;++i) {
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OC_INDEX rindex = ODTV_VECSIZE*i+rjkindex;
            OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz;
            pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
            scratch[index]   = fft_scaling*Nxx;
            scratch[index+1] = fft_scaling*Nxy;
            scratch[index+2] = fft_scaling*Nxz;
            scratch[rindex]   =    scratch[index];   // Nxx is even
            scratch[rindex+1] =    scratch[index+1]; // Nxy is even wrt z
            scratch[rindex+2] = -1*scratch[index+2]; // Nxz is odd wrt z
          }
        }
      } else { // k==0 or midpoint
        for(j=0;j<rdimy;++j) {
          OC_INDEX jkindex = kindex + j*sstridey;
          OXS_DEMAG_REAL_ASYMP y = dy*j;
          for(i=0;i<rdimx;++i) {
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz;
            pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
            scratch[index]   = fft_scaling*Nxx;
            scratch[index+1] = fft_scaling*Nxy;
            scratch[index+2] = fft_scaling*Nxz;
          }
        }
      }
    }
  }
#if REPORT_TIME
  dvltimer[0].Stop();
#endif // REPORT_TIME
#if 1
    printf("rdimx=%ld, rdimy=%ld, rdimz=%ld,"
           " SDA00_count = %ld, SDA01_count = %ld\n",
           (long int)rdimx,(long int)rdimy,(long int)rdimz,
           (long int)SDA00_count,(long int)SDA01_count);
#endif
#if 0
    for(k=0;k<rdimz;++k) {
      OC_INDEX kindex = k*sstridez;
      for(j=0;j<rdimy;++j) {
        OC_INDEX jkindex = kindex + j*sstridey;
        for(i=0;i<rdimx;++i) {
          OC_INDEX index = ODTV_VECSIZE*i+jkindex;
          printf("A[%2d][%2d][%2d].Nxx=%25.16e"
                 " Nxy=%25.16e Nxz=%25.16e\n",
                 i,j,k,
                 scratch[index],
                 scratch[index+1],
                 scratch[index+2]);
        }
      }
    }
#endif

  // Step 3: Use symmetries to reflect into other octants.
  //     Also, at each coordinate plane, set to 0.0 any term
  //     which is odd across that boundary.  It should already
  //     be close to 0, but will likely be slightly off due to
  //     rounding errors.
  // Symmetries: A00, A11, A22 are even in each coordinate
  //             A01 is odd in x and y, even in z.
  //             A02 is odd in x and z, even in y.
  //             A12 is odd in y and z, even in x.
#if REPORT_TIME
  dvltimer[1].Start();
#endif // REPORT_TIME
  for(k=0;k<rdimz;k++) {
    OC_INDEX kindex = k*sstridez;
    for(j=0;j<rdimy;j++) {
      OC_INDEX jkindex = kindex + j*sstridey;
      for(i=0;i<rdimx;i++) {
        OC_INDEX index = ODTV_VECSIZE*i+jkindex;

        if(i==0 || j==0) scratch[index+1] = 0.0;  // A01
        if(i==0 || k==0) scratch[index+2] = 0.0;  // A02

        OXS_FFT_REAL_TYPE tmpA00 = scratch[index];
        OXS_FFT_REAL_TYPE tmpA01 = scratch[index+1];
        OXS_FFT_REAL_TYPE tmpA02 = scratch[index+2];
        if(i>0) {
          OC_INDEX tindex = ODTV_VECSIZE*(ldimx-i)+j*sstridey+k*sstridez;
          scratch[tindex]   =     tmpA00;
          scratch[tindex+1] =  -1*tmpA01;
          scratch[tindex+2] =  -1*tmpA02;
        }
        if(j>0) {
          OC_INDEX tindex = ODTV_VECSIZE*i+(ldimy-j)*sstridey+k*sstridez;
          scratch[tindex]   =     tmpA00;
          scratch[tindex+1] =  -1*tmpA01;
          scratch[tindex+2] =     tmpA02;
        }
        if(k>0) {
          OC_INDEX tindex = ODTV_VECSIZE*i+j*sstridey+(ldimz-k)*sstridez;
          scratch[tindex]   =     tmpA00;
          scratch[tindex+1] =     tmpA01;
          scratch[tindex+2] =  -1*tmpA02;
        }
        if(i>0 && j>0) {
          OC_INDEX tindex
            = ODTV_VECSIZE*(ldimx-i) + (ldimy-j)*sstridey + k*sstridez;
          scratch[tindex]   =     tmpA00;
          scratch[tindex+1] =     tmpA01;
          scratch[tindex+2] =  -1*tmpA02;
        }
        if(i>0 && k>0) {
          OC_INDEX tindex
            = ODTV_VECSIZE*(ldimx-i) + j*sstridey + (ldimz-k)*sstridez;
          scratch[tindex]   =     tmpA00;
          scratch[tindex+1] =  -1*tmpA01;
          scratch[tindex+2] =     tmpA02;
        }
        if(j>0 && k>0) {
          OC_INDEX tindex
            = ODTV_VECSIZE*i + (ldimy-j)*sstridey + (ldimz-k)*sstridez;
          scratch[tindex]   =     tmpA00;
          scratch[tindex+1] =  -1*tmpA01;
          scratch[tindex+2] =  -1*tmpA02;
        }
        if(i>0 && j>0 && k>0) {
          OC_INDEX tindex
            = ODTV_VECSIZE*(ldimx-i) + (ldimy-j)*sstridey + (ldimz-k)*sstridez;
          scratch[tindex]   =     tmpA00;
          scratch[tindex+1] =     tmpA01;
          scratch[tindex+2] =     tmpA02;
        }
      }
    }
  }

  // Step 3.5: Fill in zero-padded overhang
  for(k=0;k<ldimz;k++) {
    OC_INDEX kindex = k*sstridez;
    if(k<rdimz || k>ldimz-rdimz) { // Outer k
      for(j=0;j<ldimy;j++) {
        OC_INDEX jkindex = kindex + j*sstridey;
        if(j<rdimy || j>ldimy-rdimy) { // Outer j
          for(i=rdimx;i<=ldimx-rdimx;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index] = scratch[index+1] = scratch[index+2] = 0.0;
          }
        } else { // Inner j
          for(i=0;i<ldimx;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index] = scratch[index+1] = scratch[index+2] = 0.0;
          }
        }
      }
    } else { // Middle k
      for(j=0;j<ldimy;j++) {
        OC_INDEX jkindex = kindex + j*sstridey;
        for(i=0;i<ldimx;i++) {
          OC_INDEX index = ODTV_VECSIZE*i+jkindex;
          scratch[index] = scratch[index+1] = scratch[index+2] = 0.0;
        }
      }
    }
  }

#if VERBOSE_DEBUG && !defined(NDEBUG)
  {
    OXS_FFT_REAL_TYPE fft_scaling = -1 *
      fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    for(k=0;k<ldimz;++k) {
      for(j=0;j<ldimy;++j) {
        for(i=0;i<ldimx;++i) {
          OC_INDEX index = ODTV_VECSIZE*((k*ldimy+j)*ldimx+i);
          printf("A00[%02ld][%02ld][%02ld] = %#25.18Le\n",
                 i,j,k,(long double)(scratch[index]/fft_scaling));
          printf("A01[%02ld][%02ld][%02ld] = %#25.18Le\n",
                 i,j,k,(long double)(scratch[index+1]/fft_scaling));
          printf("A02[%02ld][%02ld][%02ld] = %#25.18Le\n",
                 i,j,k,(long double)(scratch[index+2]/fft_scaling));
        }
      }
    }
    fflush(stdout);
  }
#endif // NDEBUG
#if REPORT_TIME
  dvltimer[1].Stop();
#endif // REPORT_TIME

  // Step 4: Transform into frequency domain.  These lines are cribbed
  // from the corresponding code in Oxs_FFT3DThreeVector.
  // Note: Using an Oxs_FFT3DThreeVector fft object, this would be just
  //    fft.AdjustInputDimensions(ldimx,ldimy,ldimz);
  //    fft.ForwardRealToComplexFFT(scratch,Hxfrm);
  //    fft.AdjustInputDimensions(rdimx,rdimy,rdimz); // Safety
  {
#if REPORT_TIME
  dvltimer[2].Start();
#endif // REPORT_TIME
    fftx.AdjustInputDimensions(ldimx,ldimy);
    ffty.AdjustInputDimensions(ldimy,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx,
                               ODTV_VECSIZE*cdimx);
    fftz.AdjustInputDimensions(ldimz,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy,
                               ODTV_VECSIZE*cdimx*cdimy);

    OC_INDEX rxydim = ODTV_VECSIZE*ldimx*ldimy;
    OC_INDEX cxydim = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy;

    for(OC_INDEX m=0;m<ldimz;++m) {
      // x-direction transforms in plane "m"
      fftx.ForwardRealToComplexFFT(scratch+m*rxydim,Hxfrm+m*cxydim);
      
      // y-direction transforms in plane "m"
      ffty.ForwardFFT(Hxfrm+m*cxydim);
    }
    fftz.ForwardFFT(Hxfrm); // z-direction transforms

    fftx.AdjustInputDimensions(rdimx,rdimy);   // Safety
    ffty.AdjustInputDimensions(rdimy,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx,
                               ODTV_VECSIZE*cdimx);
    fftz.AdjustInputDimensions(rdimz,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy,
                               ODTV_VECSIZE*cdimx*cdimy);

#if REPORT_TIME
  dvltimer[2].Stop();
#endif // REPORT_TIME
  }

  // Copy results from scratch into A00, A01, and A02.  We only need
  // store 1/8th of the results because of symmetries.
#if REPORT_TIME
  dvltimer[3].Start();
#endif // REPORT_TIME
  OC_INDEX astridey = adimx;
  OC_INDEX astridez = astridey*adimy;
  OC_INDEX a_size = astridez*adimz;
  assert(0 == A);
  A = new A_coefs[a_size];

  OC_INDEX cstridey = 2*ODTV_VECSIZE*cdimx; // "2" for complex data
  OC_INDEX cstridez = cstridey*cdimy;
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx;i++) {
    OC_INDEX aindex = i+j*astridey+k*astridez;
    OC_INDEX hindex = 2*ODTV_VECSIZE*i+j*cstridey+k*cstridez;
    A[aindex].A00 = Hxfrm[hindex];   // A00
    A[aindex].A01 = Hxfrm[hindex+2]; // A01
    A[aindex].A02 = Hxfrm[hindex+4]; // A02
    // The A## values are all real-valued, so we only need to pull the
    // real parts out of Hxfrm, which are stored in the even offsets.
  }
#if REPORT_TIME
  dvltimer[3].Stop();
#endif // REPORT_TIME

#if 0 // TRANSFORM CHECK
  {
    fftx.AdjustInputDimensions(ldimx,ldimy);
    ffty.AdjustInputDimensions(ldimy,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx,
                               ODTV_VECSIZE*cdimx);
    fftz.AdjustInputDimensions(ldimz,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy,
                               ODTV_VECSIZE*cdimx*cdimy);

    OC_INDEX rxydim = ODTV_VECSIZE*ldimx*ldimy;
    OC_INDEX cxydim = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy;

    fftz.InverseFFT(Hxfrm); // z-direction transforms

    for(OC_INDEX m=0;m<ldimz;++m) {
      // y-direction transforms in plane "m"
      ffty.InverseFFT(Hxfrm+m*cxydim);

      // x-direction transforms in plane "m"
      fftx.InverseComplexToRealFFT(Hxfrm+m*cxydim,scratch+m*rxydim);
    }

    fftx.AdjustInputDimensions(rdimx,rdimy);   // Safety
    ffty.AdjustInputDimensions(rdimy,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx,
                               ODTV_VECSIZE*cdimx);
    fftz.AdjustInputDimensions(rdimz,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy,
                               ODTV_VECSIZE*cdimx*cdimy);
  }
#endif

  // Repeat for Nyy, Nyz and Nzz. //////////////////////////////////////

#if REPORT_TIME
  dvltimer[4].Start();
#endif // REPORT_TIME
  if(!xperiodic && !yperiodic && !zperiodic) {
    // Step 1: Evaluate f & g at each cell site.  Offset by (-dx,-dy,-dz)
    //  so we can do 2nd derivative operations "in-place".
    for(k=0;k<kstop;k++) {
      OC_INDEX kindex = k*sstridez;
      OC_REALWIDE z = dz*(k-1);
      for(j=0;j<jstop;j++) {
        OC_INDEX jkindex = kindex + j*sstridey;
        OC_REALWIDE y = dy*(j-1);
        for(i=0;i<istop;i++) {
          OC_INDEX index = ODTV_VECSIZE*i+jkindex;
          OC_REALWIDE x = dx*(i-1);
          // Nyy(x,y,z) = Nxx(y,x,z);  Nzz(x,y,z) = Nxx(z,y,x);
          // Nxz(x,y,z) = Nxy(x,z,y);  Nyz(x,y,z) = Nxy(y,z,x);
          scratch[index]   = scale*Oxs_Newell_f(y,x,z);  // For Nyy
          scratch[index+1] = scale*Oxs_Newell_g(y,z,x);  // For Nyz
          scratch[index+2] = scale*Oxs_Newell_f(z,y,x);  // For Nzz
        }
      }
    }

    // Step 2a: Do d^2/dz^2
    if(kstop==1) {
      // Only 1 layer in z-direction of f/g stored in scratch array.
      for(j=0;j<jstop;j++) {
        OC_INDEX jkindex = j*sstridey;
        OC_REALWIDE y = dy*(j-1);
        for(i=0;i<istop;i++) {
          OC_INDEX index = ODTV_VECSIZE*i+jkindex;
          OC_REALWIDE x = dx*(i-1);
          // Function f is even in each variable, so for example
          //    f(x,y,-dz) - 2f(x,y,0) + f(x,y,dz)
          //        =  2( f(x,y,-dz) - f(x,y,0) )
          // Function g(x,y,z) is even in z and odd in x and y,
          // so for example
          //    g(x,-dz,y) - 2g(x,0,y) + g(x,dz,y)
          //        =  2g(x,0,y) = 0.
          // Nyy(x,y,z) = Nxx(y,x,z);  Nzz(x,y,z) = Nxx(z,y,x);
          // Nxz(x,y,z) = Nxy(x,z,y);  Nyz(x,y,z) = Nxy(y,z,x);
          scratch[index]   -= scale*Oxs_Newell_f(y,x,0);  // For Nyy
          scratch[index]   *= 2;
          scratch[index+1]  = 0.0;                    // For Nyz
          scratch[index+2] -= scale*Oxs_Newell_f(0,y,x);  // For Nzz
          scratch[index+2] *= 2;
        }
      }
    } else {
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        for(j=0;j<jstop;j++) {
          OC_INDEX jkindex = kindex + j*sstridey;
          for(i=0;i<istop;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index]   += -2*scratch[index+sstridez]
              + scratch[index+2*sstridez];
            scratch[index+1] += -2*scratch[index+sstridez+1]
              + scratch[index+2*sstridez+1];
            scratch[index+2] += -2*scratch[index+sstridez+2]
              + scratch[index+2*sstridez+2];
          }
        }
      }
    }
    // Step 2b: Do d^2/dy^2
    if(jstop==1) {
      // Only 1 layer in y-direction of f/g stored in scratch array.
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        OC_REALWIDE z = dz*k;
        for(i=0;i<istop;i++) {
          OC_INDEX index = ODTV_VECSIZE*i+kindex;
          OC_REALWIDE x = dx*(i-1);
          // Function f is even in each variable, so for example
          //    f(x,y,-dz) - 2f(x,y,0) + f(x,y,dz)
          //        =  2( f(x,y,-dz) - f(x,y,0) )
          // Function g(x,y,z) is even in z and odd in x and y,
          // so for example
          //    g(x,-dz,y) - 2g(x,0,y) + g(x,dz,y)
          //        =  2g(x,0,y) = 0.
          // Nyy(x,y,z) = Nxx(y,x,z);  Nzz(x,y,z) = Nxx(z,y,x);
          // Nxz(x,y,z) = Nxy(x,z,y);  Nyz(x,y,z) = Nxy(y,z,x);
          scratch[index]   -= scale * 
            ((Oxs_Newell_f(0,x,z-dz)+Oxs_Newell_f(0,x,z+dz))
             -2*Oxs_Newell_f(0,x,z));
          scratch[index]   *= 2;   // For Nyy
          scratch[index+1] = 0.0;  // For Nyz
          scratch[index+2] -= scale * 
            ((Oxs_Newell_f(z-dz,0,x)+Oxs_Newell_f(z+dz,0,x))
             -2*Oxs_Newell_f(z,0,x));
          scratch[index+2] *= 2;   // For Nzz
        }
      }
    } else {
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        for(j=0;j<rdimy;j++) {
          OC_INDEX jkindex = kindex + j*sstridey;
          for(i=0;i<istop;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index]   += -2*scratch[index+sstridey]
              + scratch[index+2*sstridey];
            scratch[index+1] += -2*scratch[index+sstridey+1]
              + scratch[index+2*sstridey+1];
            scratch[index+2] += -2*scratch[index+sstridey+2]
              + scratch[index+2*sstridey+2];
          }
        }
      }
    }
    // Step 2c: Do d^2/dx^2
    if(istop==1) {
      // Only 1 layer in x-direction of f/g stored in scratch array.
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        OC_REALWIDE z = dz*k;
        for(j=0;j<rdimy;j++) {
          OC_INDEX index = kindex + j*sstridey;
          OC_REALWIDE y = dy*j;
          // Function f is even in each variable, so for example
          //    f(x,y,-dz) - 2f(x,y,0) + f(x,y,dz)
          //        =  2( f(x,y,-dz) - f(x,y,0) )
          // Function g(x,y,z) is even in z and odd in x and y,
          // so for example
          //    g(x,-dz,y) - 2g(x,0,y) + g(x,dz,y)
          //        =  2g(x,0,y) = 0.
          // Nyy(x,y,z) = Nxx(y,x,z);  Nzz(x,y,z) = Nxx(z,y,x);
          // Nxz(x,y,z) = Nxy(x,z,y);  Nyz(x,y,z) = Nxy(y,z,x);
          scratch[index]   -= scale * 
            ((4*Oxs_Newell_f(y,0,z)
              +Oxs_Newell_f(y+dy,0,z+dz)+Oxs_Newell_f(y-dy,0,z+dz)
              +Oxs_Newell_f(y+dy,0,z-dz)+Oxs_Newell_f(y-dy,0,z-dz))
             -2*(Oxs_Newell_f(y+dy,0,z)+Oxs_Newell_f(y-dy,0,z)
                 +Oxs_Newell_f(y,0,z+dz)+Oxs_Newell_f(y,0,z-dz)));
          scratch[index]   *= 2;  // For Nyy
          scratch[index+1] -= scale * 
            ((4*Oxs_Newell_g(y,z,0)
              +Oxs_Newell_g(y+dy,z+dz,0)+Oxs_Newell_g(y-dy,z+dz,0)
              +Oxs_Newell_g(y+dy,z-dz,0)+Oxs_Newell_g(y-dy,z-dz,0))
             -2*(Oxs_Newell_g(y+dy,z,0)+Oxs_Newell_g(y-dy,z,0)
                 +Oxs_Newell_g(y,z+dz,0)+Oxs_Newell_g(y,z-dz,0)));
          scratch[index+1] *= 2;  // For Nyz
          scratch[index+2] -= scale * 
            ((4*Oxs_Newell_f(z,y,0)
              +Oxs_Newell_f(z+dz,y+dy,0)+Oxs_Newell_f(z+dz,y-dy,0)
              +Oxs_Newell_f(z-dz,y+dy,0)+Oxs_Newell_f(z-dz,y-dy,0))
             -2*(Oxs_Newell_f(z,y+dy,0)+Oxs_Newell_f(z,y-dy,0)
                 +Oxs_Newell_f(z+dz,y,0)+Oxs_Newell_f(z-dz,y,0)));
          scratch[index+2] *= 2;  // For Nzz
        }
      }
    } else {
      for(k=0;k<rdimz;k++) {
        OC_INDEX kindex = k*sstridez;
        for(j=0;j<rdimy;j++) {
          OC_INDEX jkindex = kindex + j*sstridey;
          for(i=0;i<rdimx;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index]   += -2*scratch[index+  ODTV_VECSIZE]
              + scratch[index+2*ODTV_VECSIZE];
            scratch[index+1] += -2*scratch[index+  ODTV_VECSIZE+1]
              + scratch[index+2*ODTV_VECSIZE+1];
            scratch[index+2] += -2*scratch[index+  ODTV_VECSIZE+2]
              + scratch[index+2*ODTV_VECSIZE+2];
          }
        }
      }
    }

    // Special "SelfDemag" code may be more accurate at index 0,0,0.
    const OXS_FFT_REAL_TYPE selfscale
      = -1 * fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    scratch[0] = Oxs_SelfDemagNy(dx,dy,dz);
    if(zero_self_demag) scratch[0] -= 1./3.;
    scratch[0] *= selfscale;

    scratch[1] = 0.0;  // Nyz[0] = 0.

    scratch[2] = Oxs_SelfDemagNz(dx,dy,dz);
    if(zero_self_demag) scratch[2] -= 1./3.;
    scratch[2] *= selfscale;

    // Step 2.5: Use asymptotic (dipolar + higher) approximation for far field
    /*   Dipole approximation:
     *
     *                        / 3x^2-R^2   3xy       3xz    \
     *             dx.dy.dz   |                             |
     *  H_demag = ----------- |   3xy   3y^2-R^2     3yz    |
     *             4.pi.R^5   |                             |
     *                        \   3xz      3yz     3z^2-R^2 /
     */
    // See Notes IV, 26-Feb-2007, p102.
    if(scaled_arad>=0.0) {
      // Note that all distances here are in "reduced" units,
      // scaled so that dx, dy, and dz are either small integers
      // or else close to 1.0.
      OXS_DEMAG_REAL_ASYMP scaled_arad_sq = scaled_arad*scaled_arad;
      OXS_FFT_REAL_TYPE fft_scaling = -1 *
        fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
      /// Note: Since H = -N*M, and by convention with the rest of this
      /// code, we store "-N" instead of "N" so we don't have to multiply
      /// the output from the FFT + iFFT by -1 in GetEnergy() below.

      OXS_DEMAG_REAL_ASYMP xtest
        = static_cast<OXS_DEMAG_REAL_ASYMP>(rdimx)*dx;
      xtest *= xtest;

      Oxs_DemagNyyAsymptotic ANyy(dx,dy,dz);
      Oxs_DemagNyzAsymptotic ANyz(dx,dy,dz);
      Oxs_DemagNzzAsymptotic ANzz(dx,dy,dz);

      for(k=0;k<rdimz;++k) {
        OC_INDEX kindex = k*sstridez;
        OXS_DEMAG_REAL_ASYMP z = dz*k;
        OXS_DEMAG_REAL_ASYMP zsq = z*z;
        for(j=0;j<rdimy;++j) {
          OC_INDEX jkindex = kindex + j*sstridey;
          OXS_DEMAG_REAL_ASYMP y = dy*j;
          OXS_DEMAG_REAL_ASYMP ysq = y*y;

          OC_INDEX istart = 0;
          OXS_DEMAG_REAL_ASYMP test = scaled_arad_sq-ysq-zsq;
          if(test>0) {
            if(test>xtest) {
              istart = rdimx+1;
            } else {
              istart = static_cast<OC_INDEX>(Oc_Ceil(Oc_Sqrt(test)/dx));
            }
          }
          for(i=istart;i<rdimx;++i) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            scratch[index]   = fft_scaling*ANyy.NyyAsymptotic(x,y,z);
            scratch[index+1] = fft_scaling*ANyz.NyzAsymptotic(x,y,z);
            scratch[index+2] = fft_scaling*ANzz.NzzAsymptotic(x,y,z);
          }
        }
      }
    }
  }

  // Step 2.6: If periodic boundaries selected, compute periodic tensors
  // instead.
  // NOTE THAT CODE BELOW ONLY SUPPORTS 1D PERIODIC!!!
  // NOTE 2: Keep this code in sync with that in
  //         Oxs_Demag::IncrementPreconditioner
  if(xperiodic) {
    OXS_FFT_REAL_TYPE fft_scaling = -1 *
      fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    /// Note: Since H = -N*M, and by convention with the rest of this
    /// code, we store "-N" instead of "N" so we don't have to multiply
    /// the output from the FFT + iFFT by -1 in GetEnergy() below.

    Oxs_DemagPeriodicX pbctensor(dx,dy,dz,rdimx*dx,scaled_arad);

    for(k=0;k<rdimz;++k) {
      OC_INDEX kindex = k*sstridez;
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(j=0;j<rdimy;++j) {
        OC_INDEX jkindex = kindex + j*sstridey;
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        OXS_DEMAG_REAL_ASYMP Nyy, Nyz, Nzz;

        i=0;
        OXS_DEMAG_REAL_ASYMP x = dx*i;
        OC_INDEX index = ODTV_VECSIZE*i+jkindex;
        pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
        scratch[index]   = fft_scaling*Nyy;
        scratch[index+1] = fft_scaling*Nyz;
        scratch[index+2] = fft_scaling*Nzz;

        for(i=1;2*i<rdimx;++i) {
          // Interior i; reflect results from left to right half
          x = dx*i;
          index = ODTV_VECSIZE*i+jkindex;
          OC_INDEX rindex = ODTV_VECSIZE*(rdimx-i)+jkindex;
          pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
          scratch[index]   = fft_scaling*Nyy;  // pbctensor computation
          scratch[index+1] = fft_scaling*Nyz;  // *includes* base window
          scratch[index+2] = fft_scaling*Nzz;  // term
          scratch[rindex]   = scratch[index];   // Nyy is even
          scratch[rindex+1] = scratch[index+1]; // Nyz is even wrt x
          scratch[rindex+2] = scratch[index+2]; // Nzz is even
        }

        if(rdimx%2 == 0) { // Do midpoint seperately
          i = rdimx/2;
          x = dx*i;
          index = ODTV_VECSIZE*i+jkindex;
          pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
          scratch[index]   = fft_scaling*Nyy;
          scratch[index+1] = fft_scaling*Nyz;
          scratch[index+2] = fft_scaling*Nzz;
        }

      }
    }
  }
  if(yperiodic) {
    OXS_FFT_REAL_TYPE fft_scaling = -1 *
      fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    /// Note: Since H = -N*M, and by convention with the rest of this
    /// code, we store "-N" instead of "N" so we don't have to multiply
    /// the output from the FFT + iFFT by -1 in GetEnergy() below.

    Oxs_DemagPeriodicY pbctensor(dx,dy,dz,rdimy*dy,scaled_arad);

    for(k=0;k<rdimz;++k) {
      OC_INDEX kindex = k*sstridez;
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(j=0;j<=rdimy/2;++j) {
        OC_INDEX jkindex = kindex + j*sstridey;
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        if(0<j && 2*j<rdimy) {
          // Interior j; reflect results from lower to upper half
          OC_INDEX rjkindex = kindex + (rdimy-j)*sstridey;
          for(i=0;i<rdimx;++i) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OC_INDEX rindex = ODTV_VECSIZE*i+rjkindex;
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            OXS_DEMAG_REAL_ASYMP Nyy, Nyz, Nzz;
            pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
            scratch[index]   = fft_scaling*Nyy;
            scratch[index+1] = fft_scaling*Nyz;
            scratch[index+2] = fft_scaling*Nzz;
            scratch[rindex]   =    scratch[index];   // Nyy is even
            scratch[rindex+1] = -1*scratch[index+1]; // Nyz is odd wrt y
            scratch[rindex+2] =    scratch[index+2]; // Nzz is even
          }
        } else { // j==0 or midpoint
          for(i=0;i<rdimx;++i) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            OXS_DEMAG_REAL_ASYMP Nyy, Nyz, Nzz;
            pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
            scratch[index]   = fft_scaling*Nyy;
            scratch[index+1] = fft_scaling*Nyz;
            scratch[index+2] = fft_scaling*Nzz;
          }
        }
      }
    }
  }
  if(zperiodic) {
    OXS_FFT_REAL_TYPE fft_scaling = -1 *
      fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    /// Note: Since H = -N*M, and by convention with the rest of this
    /// code, we store "-N" instead of "N" so we don't have to multiply
    /// the output from the FFT + iFFT by -1 in GetEnergy() below.

    Oxs_DemagPeriodicZ pbctensor(dx,dy,dz,rdimz*dz,scaled_arad);

    for(k=0;k<=rdimz/2;++k) {
      OC_INDEX kindex = k*sstridez;
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      if(0<k && 2*k<rdimz) {
        // Interior k; reflect results from lower to upper half
        OC_INDEX rkindex = (rdimz-k)*sstridez;
        for(j=0;j<rdimy;++j) {
          OC_INDEX jkindex = kindex + j*sstridey;
          OC_INDEX rjkindex = rkindex + j*sstridey;
          OXS_DEMAG_REAL_ASYMP y = dy*j;
          for(i=0;i<rdimx;++i) {
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OC_INDEX rindex = ODTV_VECSIZE*i+rjkindex;
            OXS_DEMAG_REAL_ASYMP Nyy, Nyz, Nzz;
            pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
            scratch[index]   = fft_scaling*Nyy;
            scratch[index+1] = fft_scaling*Nyz;
            scratch[index+2] = fft_scaling*Nzz;
            scratch[rindex]   =    scratch[index];   // Nyy is even
            scratch[rindex+1] = -1*scratch[index+1]; // Nyz is odd wrt z
            scratch[rindex+2] =    scratch[index+2]; // Nzz is even
          }
        }
      } else { // k==0 or midpoint
        for(j=0;j<rdimy;++j) {
          OC_INDEX jkindex = kindex + j*sstridey;
          OXS_DEMAG_REAL_ASYMP y = dy*j;
          for(i=0;i<rdimx;++i) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            OXS_DEMAG_REAL_ASYMP Nyy, Nyz, Nzz;
            pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
            scratch[index]   = fft_scaling*Nyy;
            scratch[index+1] = fft_scaling*Nyz;
            scratch[index+2] = fft_scaling*Nzz;
          }
        }
      }
    }
  }
#if REPORT_TIME
  dvltimer[4].Stop();
#endif // REPORT_TIME

  // Step 3: Use symmetries to reflect into other octants.
  //     Also, at each coordinate plane, set to 0.0 any term
  //     which is odd across that boundary.  It should already
  //     be close to 0, but will likely be slightly off due to
  //     rounding errors.
  // Symmetries: A00, A11, A22 are even in each coordinate
  //             A01 is odd in x and y, even in z.
  //             A02 is odd in x and z, even in y.
  //             A12 is odd in y and z, even in x.
#if REPORT_TIME
  dvltimer[5].Start();
#endif // REPORT_TIME
  for(k=0;k<rdimz;k++) {
    OC_INDEX kindex = k*sstridez;
    for(j=0;j<rdimy;j++) {
      OC_INDEX jkindex = kindex + j*sstridey;
      for(i=0;i<rdimx;i++) {
        OC_INDEX index = ODTV_VECSIZE*i+jkindex;

        if(j==0 || k==0) scratch[index+1] = 0.0;  // A12

        OXS_FFT_REAL_TYPE tmpA11 = scratch[index];
        OXS_FFT_REAL_TYPE tmpA12 = scratch[index+1];
        OXS_FFT_REAL_TYPE tmpA22 = scratch[index+2];
        if(i>0) {
          OC_INDEX tindex = ODTV_VECSIZE*(ldimx-i)+j*sstridey+k*sstridez;
          scratch[tindex]   =     tmpA11;
          scratch[tindex+1] =     tmpA12;
          scratch[tindex+2] =     tmpA22;
        }
        if(j>0) {
          OC_INDEX tindex = ODTV_VECSIZE*i+(ldimy-j)*sstridey+k*sstridez;
          scratch[tindex]   =     tmpA11;
          scratch[tindex+1] =  -1*tmpA12;
          scratch[tindex+2] =     tmpA22;
        }
        if(k>0) {
          OC_INDEX tindex = ODTV_VECSIZE*i+j*sstridey+(ldimz-k)*sstridez;
          scratch[tindex]   =     tmpA11;
          scratch[tindex+1] =  -1*tmpA12;
          scratch[tindex+2] =     tmpA22;
        }
        if(i>0 && j>0) {
          OC_INDEX tindex
            = ODTV_VECSIZE*(ldimx-i) + (ldimy-j)*sstridey + k*sstridez;
          scratch[tindex]   =     tmpA11;
          scratch[tindex+1] =  -1*tmpA12;
          scratch[tindex+2] =     tmpA22;
        }
        if(i>0 && k>0) {
          OC_INDEX tindex
            = ODTV_VECSIZE*(ldimx-i) + j*sstridey + (ldimz-k)*sstridez;
          scratch[tindex]   =     tmpA11;
          scratch[tindex+1] =  -1*tmpA12;
          scratch[tindex+2] =     tmpA22;
        }
        if(j>0 && k>0) {
          OC_INDEX tindex
            = ODTV_VECSIZE*i + (ldimy-j)*sstridey + (ldimz-k)*sstridez;
          scratch[tindex]   =     tmpA11;
          scratch[tindex+1] =     tmpA12;
          scratch[tindex+2] =     tmpA22;
        }
        if(i>0 && j>0 && k>0) {
          OC_INDEX tindex
            = ODTV_VECSIZE*(ldimx-i) + (ldimy-j)*sstridey + (ldimz-k)*sstridez;
          scratch[tindex]   =     tmpA11;
          scratch[tindex+1] =     tmpA12;
          scratch[tindex+2] =     tmpA22;
        }
      }
    }
  }

  // Step 3.5: Fill in zero-padded overhang
  for(k=0;k<ldimz;k++) {
    OC_INDEX kindex = k*sstridez;
    if(k<rdimz || k>ldimz-rdimz) { // Outer k
      for(j=0;j<ldimy;j++) {
        OC_INDEX jkindex = kindex + j*sstridey;
        if(j<rdimy || j>ldimy-rdimy) { // Outer j
          for(i=rdimx;i<=ldimx-rdimx;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index] = scratch[index+1] = scratch[index+2] = 0.0;
          }
        } else { // Inner j
          for(i=0;i<ldimx;i++) {
            OC_INDEX index = ODTV_VECSIZE*i+jkindex;
            scratch[index] = scratch[index+1] = scratch[index+2] = 0.0;
          }
        }
      }
    } else { // Middle k
      for(j=0;j<ldimy;j++) {
        OC_INDEX jkindex = kindex + j*sstridey;
        for(i=0;i<ldimx;i++) {
          OC_INDEX index = ODTV_VECSIZE*i+jkindex;
          scratch[index] = scratch[index+1] = scratch[index+2] = 0.0;
        }
      }
    }
  }
#if REPORT_TIME
  dvltimer[5].Stop();
#endif // REPORT_TIME

#if VERBOSE_DEBUG && !defined(NDEBUG)
  {
    OXS_FFT_REAL_TYPE fft_scaling = -1 *
      fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    for(k=0;k<ldimz;++k) {
      for(j=0;j<ldimy;++j) {
        for(i=0;i<ldimx;++i) {
          OC_INDEX index = ODTV_VECSIZE*((k*ldimy+j)*ldimx+i);
          printf("A11[%02ld][%02ld][%02ld] = %#25.18Le\n",
                 i,j,k,(long double)(scratch[index]/fft_scaling));
          printf("A12[%02ld][%02ld][%02ld] = %#25.18Le\n",
                 i,j,k,(long double)(scratch[index+1]/fft_scaling));
          printf("A22[%02ld][%02ld][%02ld] = %#25.18Le\n",
                 i,j,k,(long double)(scratch[index+2]/fft_scaling));
        }
      }
    }
    fflush(stdout);
  }
#endif // NDEBUG

  // Step 4: Transform into frequency domain.  These lines are cribbed
  // from the corresponding code in Oxs_FFT3DThreeVector.
  // Note: Using an Oxs_FFT3DThreeVector fft object, this would be just
  //    fft.AdjustInputDimensions(ldimx,ldimy,ldimz);
  //    fft.ForwardRealToComplexFFT(scratch,Hxfrm);
  //    fft.AdjustInputDimensions(rdimx,rdimy,rdimz); // Safety
  {
#if REPORT_TIME
  dvltimer[6].Start();
#endif // REPORT_TIME
    fftx.AdjustInputDimensions(ldimx,ldimy);
    ffty.AdjustInputDimensions(ldimy,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx,
                               ODTV_VECSIZE*cdimx);
    fftz.AdjustInputDimensions(ldimz,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy,
                               ODTV_VECSIZE*cdimx*cdimy);

    OC_INDEX rxydim = ODTV_VECSIZE*ldimx*ldimy;
    OC_INDEX cxydim = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy;

    for(OC_INDEX m=0;m<ldimz;++m) {
      // x-direction transforms in plane "m"
      fftx.ForwardRealToComplexFFT(scratch+m*rxydim,Hxfrm+m*cxydim);
      
      // y-direction transforms in plane "m"
      ffty.ForwardFFT(Hxfrm+m*cxydim);
    }
    fftz.ForwardFFT(Hxfrm); // z-direction transforms

    fftx.AdjustInputDimensions(rdimx,rdimy);   // Safety
    ffty.AdjustInputDimensions(rdimy,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx,
                               ODTV_VECSIZE*cdimx);
    fftz.AdjustInputDimensions(rdimz,
                               ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy,
                               ODTV_VECSIZE*cdimx*cdimy);

#if REPORT_TIME
  dvltimer[6].Stop();
#endif // REPORT_TIME
  }

  // At this point we no longer need the "scratch" array, so release it.
  delete[] scratch;

  // Copy results from scratch into A11, A12, and A22.  We only need
  // store 1/8th of the results because of symmetries.
#if REPORT_TIME
  dvltimer[7].Start();
#endif // REPORT_TIME
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx;i++) {
    OC_INDEX aindex =   i+j*astridey+k*astridez;
    OC_INDEX hindex = 2*ODTV_VECSIZE*i+j*cstridey+k*cstridez;
    A[aindex].A11 = Hxfrm[hindex];   // A11
    A[aindex].A12 = Hxfrm[hindex+2]; // A12
    A[aindex].A22 = Hxfrm[hindex+4]; // A22
    // The A## values are all real-valued, so we only need to pull the
    // real parts out of Hxfrm, which are stored in the even offsets.
  }
#if REPORT_TIME
  dvltimer[7].Stop();
#endif // REPORT_TIME


  // Do we want to embed "convolution" computation inside z-axis FFTs?
  // If so, setup control variables.
  OC_INDEX footprint
    = ODTV_COMPLEXSIZE*ODTV_VECSIZE*sizeof(OXS_FFT_REAL_TYPE) // Data
    + sizeof(A_coefs)                           // Interaction matrix
    + 2*ODTV_COMPLEXSIZE*sizeof(OXS_FFT_REAL_TYPE); // Roots of unity
  footprint *= cdimz;
  OC_INDEX trialsize = cache_size/(2*footprint); // "2" is fudge factor
  if(trialsize>cdimx) trialsize=cdimx;
  if(cdimz>1 && trialsize>4) {
    // Note: If cdimz==1, then the z-axis FFT is a nop, so there is
    // nothing to embed the "convolution" with and we are better off
    // using the non-embedded code.
    embed_convolution = 1;
    embed_block_size = trialsize;
  } else {
    embed_convolution = 0;
    embed_block_size = 0;  // A cry for help...
  }

#if REPORT_TIME
    inittime.Stop();
#endif // REPORT_TIME
#if 1
    printf("FINAL SDA00_count = %ld, SDA01_count = %ld\n",
           (long int)SDA00_count,(long int)SDA01_count);
#endif

}

void Oxs_Demag::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  OC_INDEX i,j,k;

  // (Re)-initialize mesh coefficient array if mesh has changed.
  if(mesh_id != state.mesh->Id()) {
    mesh_id = 0; // Safety
    FillCoefficientArrays(state.mesh);
    mesh_id = state.mesh->Id();
  }

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;
  energy.AdjustSize(state.mesh);
  field.AdjustSize(state.mesh);

  // Fill Mtemp with Ms[]*spin[].  The plan is to eventually
  // roll this step into the forward FFT routine.
  const OC_INDEX rsize = Ms.Size();
  assert(rdimx*rdimy*rdimz == rsize);
  for(i=0;i<rsize;++i) {
    OC_REAL8m scale = Ms[i];
    const ThreeVector& vec = spin[i];
    Mtemp[3*i]   = scale*vec.x;
    Mtemp[3*i+1] = scale*vec.y;
    Mtemp[3*i+2] = scale*vec.z;
  }

  if(!embed_convolution) {
    // Do not embed convolution inside z-axis FFTs.  Instead,
    // first compute full forward FFT, then do the convolution
    // (really matrix-vector A^*M^ multiply), and then do the
    // full inverse FFT.
    
    // Calculate FFT of Mtemp
#if REPORT_TIME
    fftforwardtime.Start();
#endif // REPORT_TIME
    // Transform into frequency domain.  These lines are cribbed from the
    // corresponding code in Oxs_FFT3DThreeVector.
    // Note: Using an Oxs_FFT3DThreeVector object, this would be just
    //    fft.ForwardRealToComplexFFT(Mtemp,Hxfrm);
    {
      OC_INDEX rxydim = ODTV_VECSIZE*rdimx*rdimy;
      OC_INDEX cxydim = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy;
      for(OC_INDEX m=0;m<rdimz;++m) {
        // x-direction transforms in plane "m"
        fftx.ForwardRealToComplexFFT(Mtemp+m*rxydim,Hxfrm+m*cxydim);
        // y-direction transforms in plane "m"
        ffty.ForwardFFT(Hxfrm+m*cxydim);
      }
      fftz.ForwardFFT(Hxfrm); // z-direction transforms
    }
#if REPORT_TIME
    fftforwardtime.Stop();
#endif // REPORT_TIME

    // Calculate field components in frequency domain.  Make use of
    // realness and even/odd properties of interaction matrices Axx.
    // Note that in transform space only the x>=0 half-space is
    // stored.
    // Symmetries: A00, A11, A22 are even in each coordinate
    //             A01 is odd in x and y, even in z.
    //             A02 is odd in x and z, even in y.
    //             A12 is odd in y and z, even in x.
    assert(adimx>=cdimx);
    assert(cdimy-adimy<adimy);
    assert(cdimz-adimz<adimz);
#if REPORT_TIME
    convtime.Start();
#endif // REPORT_TIME
    const OC_INDEX  jstride = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx;
    const OC_INDEX  kstride = jstride*cdimy;
    const OC_INDEX ajstride = adimx;
    const OC_INDEX akstride = ajstride*adimy;
    for(k=0;k<adimz;++k) {
      // k>=0
      OC_INDEX  kindex = k*kstride;
      OC_INDEX akindex = k*akstride;
      for(j=0;j<adimy;++j) {
        // j>=0, k>=0
        OC_INDEX  jindex =  kindex + j*jstride;
        OC_INDEX ajindex = akindex + j*ajstride;
        for(i=0;i<cdimx;++i) {
          OC_INDEX  index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*i+jindex;
          OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
          OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index+1];
          OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index+2];
          OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index+3];
          OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index+4];
          OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index+5];

          const A_coefs& Aref = A[ajindex+i]; 

          Hxfrm[index]   = Aref.A00*Hx_re + Aref.A01*Hy_re + Aref.A02*Hz_re;
          Hxfrm[index+1] = Aref.A00*Hx_im + Aref.A01*Hy_im + Aref.A02*Hz_im;
          Hxfrm[index+2] = Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
          Hxfrm[index+3] = Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
          Hxfrm[index+4] = Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
          Hxfrm[index+5] = Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
        }
      }
      for(j=adimy;j<cdimy;++j) {
        // j<0, k>=0
        OC_INDEX  jindex =  kindex + j*jstride;
        OC_INDEX ajindex = akindex + (cdimy-j)*ajstride;
        for(i=0;i<cdimx;++i) {
          OC_INDEX  index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*i+jindex;
          OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
          OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index+1];
          OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index+2];
          OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index+3];
          OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index+4];
          OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index+5];

          const A_coefs& Aref = A[ajindex+i]; 

          // Flip signs on a01 and a12 as compared to the j>=0
          // case because a01 and a12 are odd in y.
          Hxfrm[index]   =  Aref.A00*Hx_re - Aref.A01*Hy_re + Aref.A02*Hz_re;
          Hxfrm[index+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im + Aref.A02*Hz_im;
          Hxfrm[index+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
          Hxfrm[index+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
          Hxfrm[index+4] =  Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;
          Hxfrm[index+5] =  Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
        }
      }
    }
    for(k=adimz;k<cdimz;++k) {
      // k<0
      OC_INDEX  kindex = k*kstride;
      OC_INDEX akindex = (cdimz-k)*akstride;
      for(j=0;j<adimy;++j) {
        // j>=0, k<0
        OC_INDEX  jindex =  kindex + j*jstride;
        OC_INDEX ajindex = akindex + j*ajstride;
        for(i=0;i<cdimx;++i) {
          OC_INDEX  index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*i+jindex;
          OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
          OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index+1];
          OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index+2];
          OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index+3];
          OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index+4];
          OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index+5];

          const A_coefs& Aref = A[ajindex+i]; 

          // Flip signs on a02 and a12 as compared to the k>=0, j>=0 case
          // because a02 and a12 are odd in z.
          Hxfrm[index]   =  Aref.A00*Hx_re + Aref.A01*Hy_re - Aref.A02*Hz_re;
          Hxfrm[index+1] =  Aref.A00*Hx_im + Aref.A01*Hy_im - Aref.A02*Hz_im;
          Hxfrm[index+2] =  Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
          Hxfrm[index+3] =  Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
          Hxfrm[index+4] = -Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;
          Hxfrm[index+5] = -Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
        }
      }
      for(j=adimy;j<cdimy;++j) {
        // j<0, k<0
        OC_INDEX  jindex =  kindex + j*jstride;
        OC_INDEX ajindex = akindex + (cdimy-j)*ajstride;
        for(i=0;i<cdimx;++i) {
          OC_INDEX  index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*i+jindex;
          OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
          OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index+1];
          OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index+2];
          OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index+3];
          OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index+4];
          OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index+5];

          const A_coefs& Aref = A[ajindex+i]; 

          // Flip signs on a01 and a02 as compared to the k>=0, j>=0 case
          // because a01 is odd in y and even in z,
          //     and a02 is odd in z and even in y.
          // No change to a12 because it is odd in both y and z.
          Hxfrm[index]   =  Aref.A00*Hx_re - Aref.A01*Hy_re - Aref.A02*Hz_re;
          Hxfrm[index+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im - Aref.A02*Hz_im;
          Hxfrm[index+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
          Hxfrm[index+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
          Hxfrm[index+4] = -Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
          Hxfrm[index+5] = -Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
        }
      }
    }
#if REPORT_TIME
    convtime.Stop();
#endif // REPORT_TIME

#if REPORT_TIME
    fftinversetime.Start();
#endif // REPORT_TIME
    // Transform back into space domain.  These lines are cribbed from the
    // corresponding code in Oxs_FFT3DThreeVector.
    // Note: Using an Oxs_FFT3DThreeVector object, this would be
    //     assert(3*sizeof(OXS_FFT_REAL_TYPE)==sizeof(ThreeVector));
    //     void* fooptr = static_cast<void*>(&(field[0]));
    //     fft.InverseComplexToRealFFT(Hxfrm,
    //                static_cast<OXS_FFT_REAL_TYPE*>(fooptr));
    {
      OC_INDEX m;
      OC_INDEX rxydim = ODTV_VECSIZE*rdimx*rdimy;
      OC_INDEX cxydim = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy;
      assert(3*sizeof(OXS_FFT_REAL_TYPE)<=sizeof(ThreeVector));
      OXS_FFT_REAL_TYPE* fptr
        = static_cast<OXS_FFT_REAL_TYPE*>(static_cast<void*>(&field[OC_INDEX(0)]));
      fftz.InverseFFT(Hxfrm); // z-direction transforms
      for(m=0;m<rdimz;++m) {
        // y-direction transforms
        ffty.InverseFFT(Hxfrm+m*cxydim);
        // x-direction transforms
        fftx.InverseComplexToRealFFT(Hxfrm+m*cxydim,fptr+m*rxydim);
      }

      if(3*sizeof(OXS_FFT_REAL_TYPE)<sizeof(ThreeVector)) {
        // The fftx.InverseComplexToRealFFT calls above assume the
        // target is an array of OXS_FFT_REAL_TYPE.  If ThreeVector is
        // not tightly packed, then this assumption is false; however we
        // can correct the problem by expanding the results in-place.
        // The only setting I know of where ThreeVector doesn't tight
        // pack is under the Borland bcc32 compiler on Windows x86 with
        // OXS_FFT_REAL_TYPE equal to "long double".  In that case
        // sizeof(long double) == 10, but sizeof(ThreeVector) == 36.
        for(m = rsize - 1; m>=0 ; --m) {
          ThreeVector temp(fptr[ODTV_VECSIZE*m],fptr[ODTV_VECSIZE*m+1],
                           fptr[ODTV_VECSIZE*m+2]);
          field[m] = temp;
        }
      }

    }
#if REPORT_TIME
    fftinversetime.Stop();
#endif // REPORT_TIME
  } else { // if(!embed_convolution)
    // Embed "convolution" (really matrix-vector multiply A^*M^) inside
    // z-axis FFTs.  First compute full forward x- and y-axis FFTs.
    // Then, do a small number of z-axis forward FFTs, followed by the
    // the convolution for the corresponding elements, and after that
    // the corresponding number of inverse FFTs.  The number of z-axis
    // forward and inverse FFTs to do in each sandwich is given by the
    // class member variable embed_block_size.
    //    NB: In this branch, the fftforwardtime and fftinversetime timer
    // variables measure the time for the x- and y-axis transforms only.
    // The convtime timer variable includes not only the "convolution"
    // time, but also the wrapping z-axis FFT times.

    // Calculate x- and y-axis FFTs of Mtemp.
    {
      OC_INDEX rxydim = ODTV_VECSIZE*rdimx*rdimy;
      OC_INDEX cxydim = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy;
      for(OC_INDEX m=0;m<rdimz;++m) {
        // x-direction transforms in plane "m"
#if REPORT_TIME
        fftxforwardtime.Start();
#endif // REPORT_TIME
        fftx.ForwardRealToComplexFFT(Mtemp+m*rxydim,Hxfrm+m*cxydim);
#if REPORT_TIME
        fftxforwardtime.Stop();
        fftyforwardtime.Start();
#endif // REPORT_TIME
        // y-direction transforms in plane "m"
        ffty.ForwardFFT(Hxfrm+m*cxydim);
#if REPORT_TIME
        fftyforwardtime.Stop();
#endif // REPORT_TIME
      }
    }

    // Do z-axis FFTs with embedded "convolution" operations.

    // Calculate field components in frequency domain.  Make use of
    // realness and even/odd properties of interaction matrices Axx.
    // Note that in transform space only the x>=0 half-space is
    // stored.
    // Symmetries: A00, A11, A22 are even in each coordinate
    //             A01 is odd in x and y, even in z.
    //             A02 is odd in x and z, even in y.
    //             A12 is odd in y and z, even in x.
    assert(adimx>=cdimx);
    assert(cdimy-adimy<adimy);
    assert(cdimz-adimz<adimz);
#if REPORT_TIME
    convtime.Start();
#endif // REPORT_TIME
    const OC_INDEX  jstride = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx;
    const OC_INDEX  kstride = jstride*cdimy;
    const OC_INDEX ajstride = adimx;
    const OC_INDEX akstride = ajstride*adimy;

    for(j=0;j<adimy;++j) {
      // j>=0
      OC_INDEX  jindex = j*jstride;
      OC_INDEX ajindex = j*ajstride;
      fftz.AdjustArrayCount(ODTV_VECSIZE*embed_block_size);
      for(OC_INDEX m=0;m<cdimx;m+=embed_block_size) {
        // Do one block of forward z-direction transforms
        OC_INDEX istop = m + embed_block_size;
        if(embed_block_size>cdimx-m) {
          // Partial block
          fftz.AdjustArrayCount(ODTV_VECSIZE*(cdimx-m));
          istop = cdimx;
        }
        fftz.ForwardFFT(Hxfrm+jindex+m*ODTV_COMPLEXSIZE*ODTV_VECSIZE);
        // Do matrix-vector multiply ("convolution") for block
        for(k=0;k<adimz;++k) {
          // j>=0, k>=0
          OC_INDEX  kindex =  jindex + k*kstride;
          OC_INDEX akindex = ajindex + k*akstride;
          for(i=m;i<istop;++i) {
            OC_INDEX  index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*i+kindex;
            OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
            OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index+1];
            OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index+2];
            OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index+3];
            OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index+4];
            OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index+5];

            const A_coefs& Aref = A[akindex+i]; 

            Hxfrm[index]   = Aref.A00*Hx_re + Aref.A01*Hy_re + Aref.A02*Hz_re;
            Hxfrm[index+1] = Aref.A00*Hx_im + Aref.A01*Hy_im + Aref.A02*Hz_im;
            Hxfrm[index+2] = Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
            Hxfrm[index+3] = Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
            Hxfrm[index+4] = Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
            Hxfrm[index+5] = Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
        }
        for(k=adimz;k<cdimz;++k) {
          // j>=0, k<0
          OC_INDEX  kindex =  jindex + k*kstride;
          OC_INDEX akindex = ajindex + (cdimz-k)*akstride;
          for(i=m;i<istop;++i) {
            OC_INDEX  index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*i+kindex;
            OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
            OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index+1];
            OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index+2];
            OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index+3];
            OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index+4];
            OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index+5];

            const A_coefs& Aref = A[akindex+i]; 

            // Flip signs on a02 and a12 as compared to the k>=0, j>=0 case
            // because a02 and a12 are odd in z.
            Hxfrm[index]   =  Aref.A00*Hx_re + Aref.A01*Hy_re - Aref.A02*Hz_re;
            Hxfrm[index+1] =  Aref.A00*Hx_im + Aref.A01*Hy_im - Aref.A02*Hz_im;
            Hxfrm[index+2] =  Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
            Hxfrm[index+3] =  Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
            Hxfrm[index+4] = -Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;
            Hxfrm[index+5] = -Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
        }
        // Do inverse z-direction transforms for block
        fftz.InverseFFT(Hxfrm+jindex+m*ODTV_COMPLEXSIZE*ODTV_VECSIZE);
      }
    }
    for(j=adimy;j<cdimy;++j) {
      // j<0
      OC_INDEX  jindex = j*jstride;
      OC_INDEX ajindex = (cdimy-j)*ajstride;
      fftz.AdjustArrayCount(ODTV_VECSIZE*embed_block_size);
      for(OC_INDEX m=0;m<cdimx;m+=embed_block_size) {
        // Do one block of forward z-direction transforms
        OC_INDEX istop = m + embed_block_size;
        if(embed_block_size>cdimx-m) {
          // Partial block
          fftz.AdjustArrayCount(ODTV_VECSIZE*(cdimx-m));
          istop = cdimx;
        }
        fftz.ForwardFFT(Hxfrm+jindex+m*ODTV_COMPLEXSIZE*ODTV_VECSIZE);
        // Do matrix-vector multiply ("convolution") for block
        for(k=0;k<adimz;++k) {
          // j<0, k>=0
          OC_INDEX  kindex =  jindex + k*kstride;
          OC_INDEX akindex = ajindex + k*akstride;
          for(i=m;i<istop;++i) {
            OC_INDEX  index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*i+kindex;
            OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
            OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index+1];
            OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index+2];
            OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index+3];
            OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index+4];
            OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index+5];

            const A_coefs& Aref = A[akindex+i];

            // Flip signs on a01 and a12 as compared to the j>=0
            // case because a01 and a12 are odd in y.
            Hxfrm[index]   =  Aref.A00*Hx_re - Aref.A01*Hy_re + Aref.A02*Hz_re;
            Hxfrm[index+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im + Aref.A02*Hz_im;
            Hxfrm[index+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
            Hxfrm[index+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
            Hxfrm[index+4] =  Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;
            Hxfrm[index+5] =  Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
        }
        for(k=adimz;k<cdimz;++k) {
          // j<0, k<0
          OC_INDEX  kindex =  jindex + k*kstride;
          OC_INDEX akindex = ajindex + (cdimz-k)*akstride;
          for(i=m;i<istop;++i) {
            OC_INDEX  index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*i+kindex;
            OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
            OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index+1];
            OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index+2];
            OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index+3];
            OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index+4];
            OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index+5];

            const A_coefs& Aref = A[akindex+i]; 

            // Flip signs on a01 and a02 as compared to the k>=0, j>=0 case
            // because a01 is odd in y and even in z,
            //     and a02 is odd in z and even in y.
            // No change to a12 because it is odd in both y and z.
            Hxfrm[index]   =  Aref.A00*Hx_re - Aref.A01*Hy_re - Aref.A02*Hz_re;
            Hxfrm[index+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im - Aref.A02*Hz_im;
            Hxfrm[index+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
            Hxfrm[index+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
            Hxfrm[index+4] = -Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
            Hxfrm[index+5] = -Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
        }
        // Do inverse z-direction transforms for block
        fftz.InverseFFT(Hxfrm+jindex+m*ODTV_COMPLEXSIZE*ODTV_VECSIZE);
      }
    }
#if REPORT_TIME
    convtime.Stop();
#endif // REPORT_TIME

    // Do inverse y- and x-axis FFTs, to complete transform back into
    // space domain.
    {
      OC_INDEX m;
      OC_INDEX rxydim = ODTV_VECSIZE*rdimx*rdimy;
      OC_INDEX cxydim = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx*cdimy;
      assert(3*sizeof(OXS_FFT_REAL_TYPE)<=sizeof(ThreeVector));
      OXS_FFT_REAL_TYPE* fptr
        = static_cast<OXS_FFT_REAL_TYPE*>(static_cast<void*>(&field[OC_INDEX(0)]));
      for(m=0;m<rdimz;++m) {
        // y-direction transforms
#if REPORT_TIME
        fftyinversetime.Start();
#endif // REPORT_TIME
        ffty.InverseFFT(Hxfrm+m*cxydim);
#if REPORT_TIME
        fftyinversetime.Stop();
        fftxinversetime.Start();
#endif // REPORT_TIME
        // x-direction transforms
        fftx.InverseComplexToRealFFT(Hxfrm+m*cxydim,fptr+m*rxydim);
#if REPORT_TIME
        fftxinversetime.Stop();
#endif // REPORT_TIME
      }

      if(3*sizeof(OXS_FFT_REAL_TYPE)<sizeof(ThreeVector)) {
        // The fftx.InverseComplexToRealFFT calls above assume the
        // target is an array of OXS_FFT_REAL_TYPE.  If ThreeVector is
        // not tightly packed, then this assumption is false; however we
        // can correct the problem by expanding the results in-place.
        // The only setting I know of where ThreeVector doesn't tight
        // pack is under the Borland bcc32 compiler on Windows x86 with
        // OXS_FFT_REAL_TYPE equal to "long double".  In that case
        // sizeof(long double) == 10, but sizeof(ThreeVector) == 36.
#if REPORT_TIME
        fftxinversetime.Start();
#endif // REPORT_TIME
        for(m = rsize - 1; m>=0 ; --m) {
          ThreeVector temp(fptr[ODTV_VECSIZE*m],fptr[ODTV_VECSIZE*m+1],fptr[ODTV_VECSIZE*m+2]);
          field[m] = temp;
        }
#if REPORT_TIME
        fftxinversetime.Stop();
#endif // REPORT_TIME
      }

    }

  } // if(!embed_convolution)

#if REPORT_TIME
  dottime.Start();
#endif // REPORT_TIME
  // Calculate pointwise energy density: -0.5*MU0*<M,H>
  const OXS_FFT_REAL_TYPE emult =  -0.5 * MU0;
  for(i=0;i<rsize;++i) {
    OXS_FFT_REAL_TYPE dot = spin[i]*field[i];
    energy[i] = emult * dot * Ms[i];
  }
#if REPORT_TIME
  dottime.Stop();
#endif // REPORT_TIME
}

#endif // OOMMF_THREADS


////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
/////////                                                      /////////
///////// CODE COMMON TO BOTH THREADED AND UNTHREADED BRANCHES /////////
/////////                                                      /////////
////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////


OC_INT4m
Oxs_Demag::IncrementPreconditioner(PreconditionerData& pcd)
{
  // NOTE: The code in Oxs_Demag::IncrementPreconditioner needs to be
  //       kept in sync with that in Oxs_Demag::FillCoefficientArrays.

  // For details on this code, see NOTES VI, 25-July-2011, p 13.

  if(!pcd.state || !pcd.val) {
    throw Oxs_ExtError(this,
         "Import to IncrementPreconditioner not properly initialized.");
  }

  const Oxs_SimState& state = *(pcd.state);
  const OC_INDEX size = state.mesh->Size();

  Oxs_MeshValue<ThreeVector>& val = *(pcd.val);
  if(val.Size() != size) {
    throw Oxs_ExtError(this,
         "Import to IncrementPreconditioner not properly initialized.");
  }

  if(pcd.type != DIAGONAL) return 0; // Unsupported preconditioning type

  if(size<1) return 1; // Nothing to do

  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(state.mesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(this,msg);
  }

  // Check periodicity
  const Oxs_RectangularMesh* rmesh 
    = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh!=NULL) {
    // Rectangular, periodic mesh
    xperiodic = pmesh->IsPeriodicX();
    yperiodic = pmesh->IsPeriodicY();
    zperiodic = pmesh->IsPeriodicZ();

    // Check for supported periodicity
    if(xperiodic+yperiodic+zperiodic>2) {
      String msg=String("Periodic mesh ")
        + String(state.mesh->InstanceName())
        + String("is 3D periodic, which is not supported by Oxs_Demag."
                 "  Select no more than two of x, y, or z.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    if(xperiodic+yperiodic+zperiodic>1) {
      String msg=String("Periodic mesh ")
        + String(state.mesh->InstanceName())
      + String("is 2D periodic, which is not supported by Oxs_Demag"
               " at this time.");
      throw Oxs_ExtError(this,msg.c_str());
    }
  } else if (rmesh!=NULL) {
    // Rectangular, non-periodic mesh
    xperiodic=0; yperiodic=0; zperiodic=0;
  } else {
    String msg=String("Unknown mesh type: \"")
      + String(ClassName())
      + String("\".");
    throw Oxs_ExtError(this,msg.c_str());
  }

  OC_REAL8m dimx = mesh->DimX();
  OC_REAL8m dimy = mesh->DimY();
  OC_REAL8m dimz = mesh->DimZ();

  OC_REAL8m dx = mesh->EdgeLengthX();
  OC_REAL8m dy = mesh->EdgeLengthY();
  OC_REAL8m dz = mesh->EdgeLengthZ();
  // For demag calculation, all that matters is the relative
  // size of dx, dy and dz.  If possible, rescale these to
  // integers, as this may help reduce floating point error
  // a wee bit.  If not possible, then rescale so largest
  // value is 1.0.
  {
    OC_REALWIDE p1,q1,p2,q2;
    if(Nb_FindRatApprox(dx,dy,1e-12,1000,p1,q1)
       && Nb_FindRatApprox(dz,dy,1e-12,1000,p2,q2)) {
      OC_REALWIDE gcd = Nb_GcdRW(q1,q2);
      dx = p1*q2/gcd;
      dy = q1*q2/gcd;
      dz = p2*q1/gcd;
    } else {
      OC_REALWIDE maxedge=dx;
      if(dy>maxedge) maxedge=dy;
      if(dz>maxedge) maxedge=dz;
      dx/=maxedge; dy/=maxedge; dz/=maxedge;
    }
  }
  const OXS_DEMAG_REAL_ASYMP scaled_arad = asymptotic_radius
    * Oc_Pow(OXS_DEMAG_REAL_ASYMP(dx*dy*dz),
             OXS_DEMAG_REAL_ASYMP(1.)/OXS_DEMAG_REAL_ASYMP(3.));


  // NOTE: Code currently assumes that periodicity is either
  // 0 or 1D.
  OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz, Nyy, Nyz, Nzz;
  Nxy = Nxz = Nyz = 0.0;
  if(xperiodic) {
    Oxs_DemagPeriodicX pbctensor(dx,dy,dz,dimx*dx,scaled_arad);
    pbctensor.NxxNxyNxz(0,0,0,Nxx,Nxy,Nxz);
    pbctensor.NyyNyzNzz(0,0,0,Nyy,Nyz,Nzz);
  } else if(yperiodic) {
    Oxs_DemagPeriodicY pbctensor(dx,dy,dz,dimy*dy,scaled_arad);
    pbctensor.NxxNxyNxz(0,0,0,Nxx,Nxy,Nxz);
    pbctensor.NyyNyzNzz(0,0,0,Nyy,Nyz,Nzz);
  } else if(zperiodic) {
    Oxs_DemagPeriodicZ pbctensor(dx,dy,dz,dimz*dz,scaled_arad);
    pbctensor.NxxNxyNxz(0,0,0,Nxx,Nxy,Nxz);
    pbctensor.NyyNyzNzz(0,0,0,Nyy,Nyz,Nzz);
  } else {
    // Non-periodic
    Nxx = Oxs_SelfDemagNx(dx,dy,dz);
    Nyy = Oxs_SelfDemagNy(dx,dy,dz);
    Nzz = Oxs_SelfDemagNz(dx,dy,dz);
  }
  // Nxx + Nyy + Nzz should equal 1, up to rounding errors.
  // Off-diagonal terms should be zero.
  fprintf(stderr,"Nxx=%g, Nyy=%g, Nzz=%g, Nxy=%g, Nxz=%g, Nyz=%g, sum=%g\n",
          Nxx,Nyy,Nzz,Nxy,Nxz,Nyz,Nxx+Nyy+Nzz); /**/ // asdf

  // Nyy + Nzz = Nsum - Nxx where Nsum = Nxx + Nyy + Nzz, etc.
  ThreeVector cvec(MU0*(Nyy+Nzz),MU0*(Nxx+Nzz),MU0*(Nxx+Nyy));

  for(OC_INDEX i=0;i<size;++i) {
    val[i].Accum(Ms[i],cvec);
  }

  return 1;
}
