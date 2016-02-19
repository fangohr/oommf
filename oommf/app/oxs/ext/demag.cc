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
#include "oxsarray.h"  // Oxs_3DArray
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
#if REPORT_TIME
    inittime("init"),fftforwardtime("f-fft"),
    fftxforwardtime("f-fftx"),fftyforwardtime("f-ffty"),
    fftinversetime("i-fft"),fftxinversetime("i-ffx"),
    fftyinversetime("i-ffty"),
    convtime("conv"),dottime("dot"),
#endif // REPORT_TIME
    rdimx(0),rdimy(0),rdimz(0),cdimx(0),cdimy(0),cdimz(0),
    adimx(0),adimy(0),adimz(0),
    xperiodic(0),yperiodic(0),zperiodic(0),
    mesh_id(0),
    Hxfrm(0),asymptotic_radius(-1),Mtemp(0),
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

#if REPORT_TIME
  // Set default names for development timers
  char buf[256];
  for(int i=0;i<dvltimer_number;++i) {
    Oc_Snprintf(buf,sizeof(buf),"dvl[%d]",i);
    dvltimer[i].name = buf;
  }
#endif // REPORT_TIME
  VerifyAllInitArgsUsed();
}

Oxs_Demag::~Oxs_Demag() {
#if REPORT_TIME
  const char* prefix="      subtime ...";
  inittime.Print(stderr,prefix,InstanceName());
  fftforwardtime.Print(stderr,prefix,InstanceName());
  fftinversetime.Print(stderr,prefix,InstanceName());
  fftxforwardtime.Print(stderr,prefix,InstanceName());
  fftxinversetime.Print(stderr,prefix,InstanceName());
  fftyforwardtime.Print(stderr,prefix,InstanceName());
  fftyinversetime.Print(stderr,prefix,InstanceName());
  convtime.Print(stderr,prefix,InstanceName());
  dottime.Print(stderr,prefix,InstanceName());
  for(int i=0;i<dvltimer_number;++i) {
    dvltimer[i].Print(stderr,prefix,InstanceName());
  }
#endif // REPORT_TIME
  ReleaseMemory();
}

OC_BOOL Oxs_Demag::Init()
{
#if REPORT_TIME
  const char* prefix="      subtime ...";
  inittime.Print(stderr,prefix,InstanceName());
  fftforwardtime.Print(stderr,prefix,InstanceName());
  fftinversetime.Print(stderr,prefix,InstanceName());
  fftxforwardtime.Print(stderr,prefix,InstanceName());
  fftxinversetime.Print(stderr,prefix,InstanceName());
  fftyforwardtime.Print(stderr,prefix,InstanceName());
  fftyinversetime.Print(stderr,prefix,InstanceName());
  convtime.Print(stderr,prefix,InstanceName());
  dottime.Print(stderr,prefix,InstanceName());
  for(int i=0;i<dvltimer_number;++i) {
    dvltimer[i].Print(stderr,prefix,InstanceName());
    dvltimer[i].Reset();
  }
  inittime.Reset();
  fftforwardtime.Reset();  fftinversetime.Reset();
  fftxforwardtime.Reset(); fftxinversetime.Reset();
  fftyforwardtime.Reset(); fftyinversetime.Reset();
  convtime.Reset();        dottime.Reset();
#endif // REPORT_TIME
  mesh_id = 0;
  ReleaseMemory();
  return Oxs_Energy::Init();
}

void Oxs_Demag::ReleaseMemory() const
{ // Conceptually const
  A.Free();
  if(Hxfrm!=0)       { delete[] Hxfrm;       Hxfrm=0;       }
  if(Mtemp!=0)       { delete[] Mtemp;       Mtemp=0;       }
  rdimx=rdimy=rdimz=0;
  cdimx=cdimy=cdimz=0;
  adimx=adimy=adimz=0;
}

// Given a function f, and an array arr pre-sized to dimensions
// xdim, ydim+2, zdim+2, computes D6[f] = D2z[D2y[D2x[f]]] across
// the range [0,xdim-1] x [0,ydim-1] x [0,zdim-1].  The results are
// stored in arr.  The outer planes j=ydim,ydim+1 and k=zdim,zdim+1
// are used as workspace.
void ComputeD6f(OC_REALWIDE (*f)(OC_REALWIDE,OC_REALWIDE,OC_REALWIDE),
                Oxs_3DArray<OC_REALWIDE>& arr,
                OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{
  const OC_INDEX xdim = arr.GetDim1();
  const OC_INDEX ydim = arr.GetDim2()-2;
  const OC_INDEX zdim = arr.GetDim3()-2;

  // Compute f values and D2x
  for(OC_INDEX k=0;k<zdim+2;++k) {
    for(OC_INDEX j=0;j<ydim+2;++j) {
      OC_REALWIDE a0 = f(0.0,(j-1)*dy,(k-1)*dz);
      OC_REALWIDE b0 = a0 - f(-1*dx,(j-1)*dy,(k-1)*dz);
      for(OC_INDEX i=0;i<xdim;++i) {
        OC_REALWIDE a1 = f((i+1)*dx,(j-1)*dy,(k-1)*dz);
        OC_REALWIDE b1 = a1 - a0;  a0 = a1;
        arr(i,j,k) = b1 - b0; b0 = b1;
      }
    }
  }

  // Compute D2y.
  for(OC_INDEX k=0;k<zdim+2;++k) {
    for(OC_INDEX j=0;j<ydim;++j) {
      for(OC_INDEX i=0;i<xdim;++i) {
        arr(i,j,k) = arr(i,j+1,k) - arr(i,j,k);
        arr(i,j,k) += (arr(i,j+1,k) - arr(i,j+2,k));
      }
    }
  }

  // Compute D2z.
  for(OC_INDEX k=0;k<zdim;++k) {
    for(OC_INDEX j=0;j<ydim;++j) {
      for(OC_INDEX i=0;i<xdim;++i) {
        arr(i,j,k) = arr(i,j,k+1) - arr(i,j,k);
        arr(i,j,k) += (arr(i,j,k+1) - arr(i,j,k+2));
      }
    }
  }
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
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh==NULL) {
    const Oxs_RectangularMesh* rmesh 
      = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
    if (rmesh!=NULL) {
      // Rectangular, non-periodic mesh
      xperiodic=0; yperiodic=0; zperiodic=0;
    } else {
      String msg=String("Unknown mesh type: \"")
        + String(ClassName())
        + String("\".");
      throw Oxs_ExtError(this,msg.c_str());
    }
  } else {
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

  // Overflow test; restrict to signed value range
  {
    OC_INDEX testval = OC_INDEX_MAX/(2*ODTV_VECSIZE);
    testval /= cdimx;
    testval /= cdimy;
    if(testval<cdimz || cdimx<rdimx || cdimy<rdimy || cdimz<rdimz) {
      char msgbuf[1024];
      Oc_Snprintf(msgbuf,sizeof(msgbuf),"Requested mesh size ("
                  "%" OC_INDEX_MOD "d x "
                  "%" OC_INDEX_MOD "d x "
                  "%" OC_INDEX_MOD "d)"
                  " has too many elements",rdimx,rdimy,rdimz);
      throw Oxs_ExtError(this,msgbuf);
    }
  }

  Mtemp = new OXS_FFT_REAL_TYPE[ODTV_VECSIZE*rdimx*rdimy*rdimz];
  /// Temporary space to hold Ms[]*m[].  Now that there are FFT
  /// functions that can take Ms as input and do the multiplication on
  /// the fly, the use of this buffer should be removed.

  // Allocate memory for FFT xfrm target H
  const OC_INDEX xfrm_size = ODTV_VECSIZE * 2 * cdimx * cdimy * cdimz;
  Hxfrm = new OXS_FFT_REAL_TYPE[xfrm_size];
  /// "ODTV_VECSIZE" here is because we work with arrays if
  /// ThreeVectors, and "2" because these are complex (as opposed to
  /// real) quantities.  (Note: If we embed properly we don't really
  /// need all this space.)

  if(Mtemp==0 || Hxfrm==0) {
    // Safety check for those machines on which new[] doesn't throw
    // BadAlloc.
    String msg = String("Insufficient memory in Demag setup.");
    throw Oxs_ExtError(this,msg);
  }

  // The following 3 statements are cribbed from
  // Oxs_FFT3DThreeVector::SetDimensions().  The corresponding
  // code using that class is
  //
  //  Oxs_FFT3DThreeVector fft;
  //  fft.SetDimensions(rdimx,rdimy,rdimz,cdimx,cdimy,cdimz);
  //  fft.GetLogicalDimensions(ldimx,ldimy,ldimz);
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

  // Dimensions for the demag tensor A.
  adimx = (ldimx/2)+1;
  adimy = (ldimy/2)+1;
  adimz = (ldimz/2)+1;

#if (VERBOSE_DEBUG && !defined(NDEBUG))
  fprintf(stderr,"RDIMS: (%ld,%ld,%ld)\n",(long)rdimx,(long)rdimy,(long)rdimz);
  fprintf(stderr,"CDIMS: (%ld,%ld,%ld)\n",(long)cdimx,(long)cdimy,(long)cdimz);
  fprintf(stderr,"LDIMS: (%ld,%ld,%ld)\n",(long)ldimx,(long)ldimy,(long)ldimz);
  fprintf(stderr,"ADIMS: (%ld,%ld,%ld)\n",(long)adimx,(long)adimy,(long)adimz);
#endif // NDEBUG

  // Overflow test; restrict to signed value range
  {
    OC_INDEX testval = OC_INDEX_MAX/ODTV_VECSIZE;
    testval /= ldimx;
    testval /= ldimy;
    if(ldimx<1 || ldimy<1 || ldimz<1 || testval<ldimz) {
      String msg =
        String("OC_INDEX overflow in ")
        + String(InstanceName())
        + String(": Product 3*8*rdimx*rdimy*rdimz too big"
                 " to fit in an OC_INDEX variable");
      throw Oxs_ExtError(this,msg);
    }
  }

  OC_INDEX astridey = adimx;
  OC_INDEX astridez = astridey*adimy;
  OC_INDEX asize = astridez*adimz;
  A.SetSize(asize);


  // According (16) in Newell's paper, the demag field is given by
  //                        H = -N*M
  // where N is the "demagnetizing tensor," with components Nxx, Nxy,
  // etc.  With the '-1' in 'scale' we store '-N' instead of 'N',
  // so we don't have to multiply the output from the FFT + iFFT
  // by -1 in GetEnergy() below.

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

  if(!xperiodic && !yperiodic && !zperiodic) {
    // Calculate Nxx, Nxy and Nxz in first octant, non-periodic case.
    // Step 1: Evaluate f & g at each cell site.  Offset by (-dx,-dy,-dz)
    //  so we can do 2nd derivative operations "in-place".

    OC_INDEX wdim1 = rdimx;
    OC_INDEX wdim2 = rdimy;
    OC_INDEX wdim3 = rdimz;
    if(scaled_arad>0) {
      // We don't need to compute analytic formulae outside
      // asymptotic radius
      OC_INDEX itest = static_cast<OC_INDEX>(Oc_Ceil(scaled_arad/dx));
      if(itest<wdim1) wdim1 = itest;
      OC_INDEX jtest = static_cast<OC_INDEX>(Oc_Ceil(scaled_arad/dy));
      if(jtest<wdim2) wdim2 = jtest;
      OC_INDEX ktest = static_cast<OC_INDEX>(Oc_Ceil(scaled_arad/dz));
      if(ktest<wdim3) wdim3 = ktest;
    }
    Oxs_3DArray<OC_REALWIDE> workspace;
    workspace.SetDimensions(wdim1,wdim2+2,wdim3+2);
    ComputeD6f(Oxs_Newell_f_xx,workspace,dx,dy,dz);
    for(k=0;k<wdim3;k++) {
      for(j=0;j<wdim2;j++) {
        for(i=0;i<wdim1;i++) {
          A[k*astridez + j*astridey + i].A00 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_g_xy,workspace,dx,dy,dz);
    for(k=0;k<wdim3;k++) {
      for(j=0;j<wdim2;j++) {
        for(i=0;i<wdim1;i++) {
          A[k*astridez + j*astridey + i].A01 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_g_xz,workspace,dx,dy,dz);
    for(k=0;k<wdim3;k++) {
      for(j=0;j<wdim2;j++) {
        for(i=0;i<wdim1;i++) {
          A[k*astridez + j*astridey + i].A02 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_f_yy,workspace,dx,dy,dz);
    for(k=0;k<wdim3;k++) {
      for(j=0;j<wdim2;j++) {
        for(i=0;i<wdim1;i++) {
          A[k*astridez + j*astridey + i].A11 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_g_yz,workspace,dx,dy,dz);
    for(k=0;k<wdim3;k++) {
      for(j=0;j<wdim2;j++) {
        for(i=0;i<wdim1;i++) {
          A[k*astridez + j*astridey + i].A12 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_f_zz,workspace,dx,dy,dz);
    for(k=0;k<wdim3;k++) {
      for(j=0;j<wdim2;j++) {
        for(i=0;i<wdim1;i++) {
          A[k*astridez + j*astridey + i].A22 = scale*workspace(i,j,k);
        }
      }
    }

    // Special "SelfDemag" code may be more accurate at index 0,0,0.
    // Note: Using an Oxs_FFT3DThreeVector fft object, this would be
    //    scale *= fft.GetScaling();
    const OXS_FFT_REAL_TYPE selfscale
      = -1 * fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    A[0].A00 = Oxs_SelfDemagNx(dx,dy,dz);
    A[0].A11 = Oxs_SelfDemagNy(dx,dy,dz);
    A[0].A22 = Oxs_SelfDemagNz(dx,dy,dz);
    if(zero_self_demag) {
      A[0].A00 -= 1./3.;
      A[0].A11 -= 1./3.;
      A[0].A22 -= 1./3.;
    }
    A[0].A00 *= selfscale;
    A[0].A11 *= selfscale;
    A[0].A22 *= selfscale;
    A[0].A01 = A[0].A02 = A[0].A12 = 0.0;

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
      Oxs_DemagNyyAsymptotic ANyy(dx,dy,dz);
      Oxs_DemagNyzAsymptotic ANyz(dx,dy,dz);
      Oxs_DemagNzzAsymptotic ANzz(dx,dy,dz);

      for(k=0;k<rdimz;++k) {
        OXS_DEMAG_REAL_ASYMP z = dz*k;
        OXS_DEMAG_REAL_ASYMP zsq = z*z;
        for(j=0;j<rdimy;++j) {
          OC_INDEX jkindex = k*astridez + j*astridey;
          OXS_DEMAG_REAL_ASYMP y = dy*j;
          OXS_DEMAG_REAL_ASYMP ysq = y*y;
          OC_INDEX istart = 0;
          OXS_DEMAG_REAL_ASYMP test = scaled_arad_sq-ysq-zsq;
          if(test>0.0) {
            if(test>xtest) {
              istart = rdimx+1;  // No asymptotic
            } else {
              istart = static_cast<OC_INDEX>(Oc_Ceil(Oc_Sqrt(test)/dx));
            }
          }
          for(i=istart;i<rdimx;++i) {
            OC_INDEX index = jkindex + i;
            OXS_DEMAG_REAL_ASYMP x = dx*i;
            A[index].A00 = fft_scaling*ANxx.NxxAsymptotic(x,y,z);
            A[index].A01 = fft_scaling*ANxy.NxyAsymptotic(x,y,z);
            A[index].A02 = fft_scaling*ANxz.NxzAsymptotic(x,y,z);
            A[index].A11 = fft_scaling*ANyy.NyyAsymptotic(x,y,z);
            A[index].A12 = fft_scaling*ANyz.NyzAsymptotic(x,y,z);
            A[index].A22 = fft_scaling*ANzz.NzzAsymptotic(x,y,z);
          }
        }
      }
    }
  }

  // Step 2.6: If periodic boundaries selected, compute periodic tensors
  // instead.
  // NOTE THAT CODE BELOW CURRENTLY ONLY SUPPORTS 1D PERIODIC!!!
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
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(j=0;j<rdimy;++j) {
        const OC_INDEX jkindex = k*astridez + j*astridey;
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        for(i=0;i<=(rdimx/2);++i) {
          OXS_DEMAG_REAL_ASYMP x = dx*i;
          OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz, Nyy, Nyz, Nzz;
          pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
          pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
          A_coefs Atmp(fft_scaling*Nxx,fft_scaling*Nxy,fft_scaling*Nxz,
                       fft_scaling*Nyy,fft_scaling*Nyz,fft_scaling*Nzz);
          A[jkindex + i] = Atmp;
          if(0<i && 2*i<rdimx) {
            // Interior point.  Reflect results from left half to
            // right half.  Note that wrt x, Nxy and Nxz are odd,
            // the other terms are even.
            Atmp.A01 *= -1.0;
            Atmp.A02 *= -1.0;
            A[jkindex + rdimx - i] = Atmp;
          }
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
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(j=0;j<=(rdimy/2);++j) {
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        const OC_INDEX jkindex = k*astridez + j*astridey;
        const OC_INDEX mjkindex = k*astridez + (rdimy-j)*astridey;
        for(i=0;i<rdimx;++i) {
          OXS_DEMAG_REAL_ASYMP x = dx*i;
          OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz, Nyy, Nyz, Nzz;
          pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
          pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
          A_coefs Atmp(fft_scaling*Nxx,fft_scaling*Nxy,fft_scaling*Nxz,
                       fft_scaling*Nyy,fft_scaling*Nyz,fft_scaling*Nzz);
          A[jkindex + i] = Atmp;
          if(0<j && 2*j<rdimy) {
            // Interior point.  Reflect results from lower half to
            // upper half.  Note that wrt y, Nxy and Nyz are odd,
            // the other terms are even.
            Atmp.A01 *= -1.0;
            Atmp.A12 *= -1.0;
            A[mjkindex + i] = Atmp;
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
    for(k=0;k<=(rdimz/2);++k) {
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(j=0;j<rdimy;++j) {
        const OC_INDEX kjindex = k*astridez + j*astridey;
        const OC_INDEX mkjindex = (rdimz-k)*astridez + j*astridey;
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        for(i=0;i<rdimx;++i) {
          OXS_DEMAG_REAL_ASYMP x = dx*i;
          OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz, Nyy, Nyz, Nzz;
          pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
          pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
          A_coefs Atmp(fft_scaling*Nxx,fft_scaling*Nxy,fft_scaling*Nxz,
                       fft_scaling*Nyy,fft_scaling*Nyz,fft_scaling*Nzz);
          A[kjindex + i] = Atmp;
          if(0<k && 2*k<rdimz) {
            // Interior point.  Reflect results from bottom half to
            // top half.  Note that wrt z, Nxz and Nyz are odd, the
            // other terms are even.
            Atmp.A02 *= -1.0;
            Atmp.A12 *= -1.0;
            A[mkjindex + i] = Atmp;
          }
        }
      }
    }
  }

  // Step 3: Do FFTs.  We only need store 1/8th of the results because
  //   of symmetries.  In this computation, make use of the relationship
  //   between FFTs of symmetric (even or odd) sequences and zero-padded
  //   sequences, as discussed in NOTES VII, 1-May-2015, p95.  In this
  //   regard, note that ldim is always >= 2*rdim, so it ldim/2<rdim
  //   never happens, so the sequence midpoint=ldim/2, can always be
  //   taken as zero.
  // Note: In this code STL vector objects are used to acquire scratch
  //  buffer space, using the &(arr[0]) idiom.  If using C++-11, the
  //  arr.data() member function could be used instead.
  {
    Oxs_FFT1DThreeVector workfft;
    vector<OXS_FFT_REAL_TYPE> sourcebuf;
    vector<OXS_FFT_REAL_TYPE> targetbuf;

    // FFTs in x-direction
    workfft.SetDimensions(rdimx,ldimx,1);
    sourcebuf.resize(ODTV_VECSIZE*rdimx);
    targetbuf.resize(2*ODTV_VECSIZE*adimx);
    /// "ODTV_VECSIZE" is here because we work with arrays of
    /// ThreeVectors.  The target buf has a factor of 2 because the
    /// results are (nominally) complex (as opposed to real) quantities.
    for(k=0;k<rdimz;++k) {
      for(j=0;j<rdimy;++j) {
        const OC_INDEX aoff = k*astridez +j*astridey;
        for(i=0;i<rdimx;++i) {
          // Fill sourcebuf, diagonal elts
          sourcebuf[ODTV_VECSIZE*i]   = A[aoff + i].A00;
          sourcebuf[ODTV_VECSIZE*i+1] = A[aoff + i].A11;
          sourcebuf[ODTV_VECSIZE*i+2] = A[aoff + i].A22;
        }
        sourcebuf[0] *= 0.5; sourcebuf[1] *= 0.5; sourcebuf[2] *= 0.5;
        workfft.ForwardRealToComplexFFT(&(sourcebuf[0]),&(targetbuf[0]));
        for(i=0;i<adimx;++i) {
          // Copy back from targetbuf.  Since A00, A11, and A22 are all
          // even, we take the real component of the transformed data.
          A[aoff + i].A00 = targetbuf[2*ODTV_VECSIZE*i];
          A[aoff + i].A11 = targetbuf[2*ODTV_VECSIZE*i+2];
          A[aoff + i].A22 = targetbuf[2*ODTV_VECSIZE*i+4];
        }
        for(i=0;i<rdimx;++i) {
          // Fill sourcebuf, off-diagonal elts
          sourcebuf[ODTV_VECSIZE*i]   = A[aoff + i].A01;
          sourcebuf[ODTV_VECSIZE*i+1] = A[aoff + i].A02;
          sourcebuf[ODTV_VECSIZE*i+2] = A[aoff + i].A12;
        }
        sourcebuf[0]=0.0; sourcebuf[1]=0.0; sourcebuf[2]*=0.5;
        // A01 and A02 are odd wrt x, A12 is even.
        workfft.ForwardRealToComplexFFT(&(sourcebuf[0]),&(targetbuf[0]));
        for(i=0;i<adimx;++i) {
          // Copy back from targetbuf.  A01 and A02 are odd wrt x, so
          // take imaginary component for those two.  A12 is even wrt x,
          // so take the real component for it.
          A[aoff + i].A01 = targetbuf[2*ODTV_VECSIZE*i+1];
          A[aoff + i].A02 = targetbuf[2*ODTV_VECSIZE*i+3];
          A[aoff + i].A12 = targetbuf[2*ODTV_VECSIZE*i+4];
        }
      }
    }

    // FFTs in y-direction
    workfft.SetDimensions(rdimy,ldimy,1);
    sourcebuf.resize(ODTV_VECSIZE*rdimy);
    targetbuf.resize(2*ODTV_VECSIZE*adimy);
    for(k=0;k<rdimz;++k) {
      for(i=0;i<adimx;++i) {
        const OC_INDEX aoff = k*astridez + i;
        for(j=0;j<rdimy;++j) {
          // Fill sourcebuf, diagonal elts
          sourcebuf[ODTV_VECSIZE*j]   = A[aoff+j*astridey].A00;
          sourcebuf[ODTV_VECSIZE*j+1] = A[aoff+j*astridey].A11;
          sourcebuf[ODTV_VECSIZE*j+2] = A[aoff+j*astridey].A22;
        }
        sourcebuf[0] *= 0.5; sourcebuf[1] *= 0.5; sourcebuf[2] *= 0.5;
        workfft.ForwardRealToComplexFFT(&(sourcebuf[0]),&(targetbuf[0]));
        for(j=0;j<adimy;++j) {
          // Copy back from targetbuf.  Since A00, A11, and A22 are all
          // even, we take the real component of the transformed data.
          A[aoff+j*astridey].A00 = targetbuf[2*ODTV_VECSIZE*j];
          A[aoff+j*astridey].A11 = targetbuf[2*ODTV_VECSIZE*j+2];
          A[aoff+j*astridey].A22 = targetbuf[2*ODTV_VECSIZE*j+4];
        }
        for(j=0;j<rdimy;++j) {
          // Fill sourcebuf, off-diagonal elts
          sourcebuf[ODTV_VECSIZE*j]   = A[aoff+j*astridey].A01;
          sourcebuf[ODTV_VECSIZE*j+1] = A[aoff+j*astridey].A02;
          sourcebuf[ODTV_VECSIZE*j+2] = A[aoff+j*astridey].A12;
        }
        sourcebuf[0]=0.0; sourcebuf[1]*=0.5; sourcebuf[2]=0.0;
        // A01 and A12 are odd wrt y, A02 is even.
        workfft.ForwardRealToComplexFFT(&(sourcebuf[0]),&(targetbuf[0]));
        for(j=0;j<adimy;++j) {
          // Copy back from targetbuf.  A01 and A12 are odd wrt y, so
          // take imaginary component for those two.  A02 is even wrt y,
          // so take the real component for it.
          A[aoff+j*astridey].A01 = targetbuf[2*ODTV_VECSIZE*j+1];
          A[aoff+j*astridey].A02 = targetbuf[2*ODTV_VECSIZE*j+2];
          A[aoff+j*astridey].A12 = targetbuf[2*ODTV_VECSIZE*j+5];
        }
      }
    }

    // FFTs in z-direction
    workfft.SetDimensions(rdimz,ldimz,1);
    sourcebuf.resize(ODTV_VECSIZE*rdimz);
    targetbuf.resize(2*ODTV_VECSIZE*adimz);
    for(j=0;j<adimy;++j) {
      for(i=0;i<adimx;++i) {
        const OC_INDEX aoff = j*astridey + i;
        for(k=0;k<rdimz;++k) {
          // Fill sourcebuf, diagonal elts
          sourcebuf[ODTV_VECSIZE*k]   = A[aoff+k*astridez].A00;
          sourcebuf[ODTV_VECSIZE*k+1] = A[aoff+k*astridez].A11;
          sourcebuf[ODTV_VECSIZE*k+2] = A[aoff+k*astridez].A22;
        }
        sourcebuf[0]*=0.5; sourcebuf[1]*=0.5; sourcebuf[2]*=0.5;
        workfft.ForwardRealToComplexFFT(&(sourcebuf[0]),&(targetbuf[0]));
        for(k=0;k<adimz;++k) {
          // Copy back from targetbuf.  Since A00, A11, and A22 are all
          // even, we take the real component of the transformed data.
          // The "8*" factor accounts for the zero-padded/symmetry
          // conversion for all the x, y and z transforms.  See NOTES
          // VII, 1-May-2015, p95 for details.
          A[aoff+k*astridez].A00 = 8*targetbuf[2*ODTV_VECSIZE*k];
          A[aoff+k*astridez].A11 = 8*targetbuf[2*ODTV_VECSIZE*k+2];
          A[aoff+k*astridez].A22 = 8*targetbuf[2*ODTV_VECSIZE*k+4];
        }
        for(k=0;k<rdimz;++k) {
          // Fill sourcebuf, off-diagonal elts
          sourcebuf[ODTV_VECSIZE*k]   = A[aoff+k*astridez].A01;
          sourcebuf[ODTV_VECSIZE*k+1] = A[aoff+k*astridez].A02;
          sourcebuf[ODTV_VECSIZE*k+2] = A[aoff+k*astridez].A12;
        }
        sourcebuf[0]*=0.5; sourcebuf[1]=0.0; sourcebuf[2]=0.0;
        workfft.ForwardRealToComplexFFT(&(sourcebuf[0]),&(targetbuf[0]));
        for(k=0;k<adimz;++k) {
          // Copy back from targetbuf.  A02 and A12 are odd wrt z, so
          // take imaginary component for those two.  A01 is even wrt
          // z, so take the real component for it.  Like above for the
          // diagonal components, the "8*" factor accounts for the
          // zero-padded/symmetry conversion for all the x, y and z
          // transforms.  Additionally, each odd symmetry induces a
          // factor of sqrt(-1); each of the off-diagonal terms have
          // odd symmetry across two axis, so the end result in each
          // case is a real value but with a -1 factor.
          A[aoff+k*astridez].A01 = -8*targetbuf[2*ODTV_VECSIZE*k];
          A[aoff+k*astridez].A02 = -8*targetbuf[2*ODTV_VECSIZE*k+3];
          A[aoff+k*astridez].A12 = -8*targetbuf[2*ODTV_VECSIZE*k+5];
        }
      }
    }
    // workfft, sourcebuf, and targetbuf are deleted by end of scope.
  }

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

  OC_REAL8m dimx = static_cast<OC_REAL8m>(mesh->DimX());
  OC_REAL8m dimy = static_cast<OC_REAL8m>(mesh->DimY());
  OC_REAL8m dimz = static_cast<OC_REAL8m>(mesh->DimZ());

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
          double(Nxx),double(Nyy),double(Nzz),
          double(Nxy),double(Nxz),double(Nyz),
          double(Nxx+Nyy+Nzz)); /**/ // asdf

  // Nyy + Nzz = Nsum - Nxx where Nsum = Nxx + Nyy + Nzz, etc.
  ThreeVector cvec(MU0*(Nyy+Nzz),MU0*(Nxx+Nzz),MU0*(Nxx+Nyy));

  for(OC_INDEX i=0;i<size;++i) {
    val[i].Accum(Ms[i],cvec);
  }

  return 1;
}
