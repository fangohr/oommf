/* FILE: demag-threaded.cc            -*-Mode: c++-*-
 *
 * Threaded version of demag.cc (q.v.).
 */

#include "demag.h"  // Includes definition of OOMMF_THREADS macro

#ifndef USE_FFT_YZ_CONVOLVE
# define USE_FFT_YZ_CONVOLVE 1
#endif

////////////////// MULTI-THREADED IMPLEMENTATION  ///////////////
#if OOMMF_THREADS

#include <assert.h>
#include <string>
#include <vector>

#include "nb.h"
#include "director.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

#include "rectangularmesh.h"
#include "demagcoef.h"

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

class _Oxs_DemagJobControl {
public:

  _Oxs_DemagJobControl() : imax(0),big_block_limit(0),big_blocksize(0),
                            small_blocksize(0),next_job_start(0) {}

  // Note: The initializer is not thread-safe.  It should be called
  // only from the control thread.
  void Init(OC_INDEX i_imax,OC_INT4m threadcount,OC_INDEX minjobsize);

  // ClaimJob is thread safe.  It is called from inside worker
  // threads to get job assignments.  The return value istop
  // is guaranteed to be less than imax.
  void ClaimJob(OC_INDEX& istart,OC_INDEX& istop);

private:
  // Variable next_job_start marks the next available job.  Jobs are
  // incremented by big_blocksize until big_block_limit is reached,
  // after which jobs are handed out of size small_blocksize.
  OC_INDEX imax;
  OC_INDEX big_block_limit;
  OC_INDEX big_blocksize;
  OC_INDEX small_blocksize;
  OC_INDEX next_job_start;
  Oxs_Mutex job_mutex;
};

void _Oxs_DemagJobControl::Init
(OC_INDEX i_imax,
 OC_INT4m threadcount,
 OC_INDEX minjobsize)
{
  imax = i_imax;
  next_job_start = 0;
  assert(imax>=1 && threadcount>=1 && minjobsize>=1);
  if(threadcount<1) threadcount = 1; // Safety
  if(minjobsize<1) minjobsize = 1;

  // big_proportion + small_proportion should be <= 1.0
#if 1
  const OC_REAL8m small_proportion = 0.05;
  const OC_REAL8m big_proportion   = 0.95;
#else
  // On some systems (8 threads, non-numa), slightly more
  // and slightly larger small blocks works slightly better.
  const OC_REAL8m small_proportion = 0.10;
  const OC_REAL8m big_proportion   = 0.80;
#endif

  small_blocksize = OC_INDEX(floor(0.5+(small_proportion*imax)/threadcount));
  // Round small_blocksize to an integral multiple of minjobsize
  if(small_blocksize<=minjobsize) {
    small_blocksize = minjobsize;
  } else {
    small_blocksize = minjobsize*((small_blocksize+minjobsize/2)/minjobsize);
  }

  // Select big_block_limit close to big_proportion of imax, but
  // round so that remainder is an integral multiple of small_blocksize.
  OC_INDEX remainder = imax - OC_INDEX(floor(0.5+big_proportion*imax));
  if(0<remainder && remainder<=small_blocksize) {
    remainder = small_blocksize;
  } else {
    remainder
      = small_blocksize*(OC_INDEX(floor((0.5+remainder)/small_blocksize)));
  }
  big_block_limit = imax - remainder;

  // Pick big_blocksize close to (but not smaller than)
  // big_block_limit/threadcount, and make it a multiple of minjobsize.
  // The size of the last (short) big block will be big_block_limit -
  // big_blocksize*(threadcount-1) (see ::ClaimJob); if imax is a
  // multiple of minjobsize, then big_block_limit will also be a
  // multiple of minjobsize, and so in this case the last (short) big
  // block will also be a multiple of minjobsize.
  big_blocksize = minjobsize*
    ((big_block_limit+minjobsize*threadcount-1)/(minjobsize*threadcount));

#if 0
  // On some systems (8 threads, non-numa), breaking up the
  // big blocks seems to help.
  if(big_blocksize>=4*minjobsize) {
    big_blocksize = (big_blocksize+3)/4;
  }
#endif

static int foocount=0; /**/ // asdf
if(foocount<5) {
  ++foocount;
  fprintf(stderr,"Total limit=%ld, big limit=%ld,"
          " big size=%ld, small size=%ld\n",
          long(imax),long(big_block_limit),
          long(big_blocksize),long(small_blocksize));
 } /**/ // asdf

}

void _Oxs_DemagJobControl::ClaimJob(OC_INDEX& istart,OC_INDEX& istop)
{
  OC_INDEX tmp_start,tmp_stop;
  job_mutex.Lock();
  if((tmp_start = next_job_start)<big_block_limit) {
    if(next_job_start + big_blocksize>big_block_limit) {
      tmp_stop = next_job_start = big_block_limit;
    } else {
      tmp_stop = (next_job_start += big_blocksize);
    }
  } else {
    tmp_stop = (next_job_start += small_blocksize);
  }
  job_mutex.Unlock();
  if(tmp_stop>imax) tmp_stop=imax;  // Guarantee that istop is in-range.
  istart = tmp_start;
  istop  = tmp_stop;
}

////////////////////////////////////////////////////////////////////////
// Oxs_Demag::Oxs_FFTLocker provides thread-specific instance of FFT
// objects.

Oxs_Demag::Oxs_FFTLocker::Oxs_FFTLocker
(const Oxs_Demag::Oxs_FFTLocker_Info& info)
  : ifftx_scratch(0), fftz_Hwork(0), fftyz_Hwork(0),
    fftyz_Hwork_base(0), fftyconvolve_Hwork(0),
    A_copy(0),
    ifftx_scratch_size(0), fftz_Hwork_size(0),
    fftyz_Hwork_size(0), fftyz_Hwork_base_size(0),
    fftyconvolve_Hwork_size(0),
    A_copy_size(0)
{
  // Check import data
  assert(info.rdimx>0 && info.rdimy>0 && info.rdimz>0 &&
         info.cdimx>0 && info.cdimy>0 && info.cdimz>0 &&
         info.embed_block_size>0 && info.embed_yzblock_size>0);
         
  // FFT objects
  fftx.SetDimensions(info.rdimx,
                     (info.cdimx==1 ? 1 : 2*(info.cdimx-1)), info.rdimy);
  ffty.SetDimensions(info.rdimy, info.cdimy,
                     ODTV_COMPLEXSIZE*ODTV_VECSIZE*info.cdimx,
                     ODTV_VECSIZE*info.cdimx);
  fftz.SetDimensions(info.rdimz, info.cdimz,
                     ODTV_COMPLEXSIZE*ODTV_VECSIZE*info.cdimx*info.cdimy,
                     ODTV_VECSIZE*info.cdimx*info.cdimy);

  // Workspace
  ifftx_scratch_size = (info.rdimx+1)*ODTV_VECSIZE; // Make one bigger
  /// than necessary so that we can offset use by one if needed to get
  /// SSE alignment with import/export arrays.
  ifftx_scratch = static_cast<OXS_FFT_REAL_TYPE*>
    (Oc_AllocThreadLocal(ifftx_scratch_size*sizeof(OXS_FFT_REAL_TYPE)));
  if(info.cdimz>1) {
    fftz_Hwork_size = 2*ODTV_VECSIZE*ODTV_COMPLEXSIZE
                       *info.embed_block_size*info.cdimz;
    fftz_Hwork = static_cast<OXS_FFT_REAL_TYPE*>
      (Oc_AllocThreadLocal(fftz_Hwork_size*sizeof(OXS_FFT_REAL_TYPE)));
#if USE_FFT_YZ_CONVOLVE
    fftyz_Hwork_size = ODTV_VECSIZE*ODTV_COMPLEXSIZE
                        *info.embed_yzblock_size*info.cdimy*info.cdimz;
    fftyz_Hwork_base_size = fftyz_Hwork_size*sizeof(OXS_FFT_REAL_TYPE)
                          + OC_CACHE_LINESIZE - 1;
    fftyz_Hwork_base = static_cast<char*>
                        (Oc_AllocThreadLocal(fftyz_Hwork_base_size));
    // Point fftyz_Hwork to aligned spot in fftyz_Hwork_base
    OC_INDEX alignoff = reinterpret_cast<OC_UINDEX>(fftyz_Hwork_base)
                      % OC_CACHE_LINESIZE;
    if(alignoff>0) {
      alignoff = OC_CACHE_LINESIZE - alignoff;
    }
    fftyz_Hwork = reinterpret_cast<OXS_FFT_REAL_TYPE*>
                   (fftyz_Hwork_base + alignoff);
#endif
  } else {
    fftyconvolve_Hwork_size
      = ODTV_VECSIZE*ODTV_COMPLEXSIZE*info.embed_block_size*info.cdimy;
    fftyconvolve_Hwork = static_cast<OXS_FFT_REAL_TYPE*>
      (Oc_AllocThreadLocal(fftyconvolve_Hwork_size*sizeof(OXS_FFT_REAL_TYPE)));
  }

}

Oxs_Demag::Oxs_FFTLocker::~Oxs_FFTLocker()
{
  if(ifftx_scratch) Oc_FreeThreadLocal(ifftx_scratch,
                       ifftx_scratch_size*sizeof(OXS_FFT_REAL_TYPE));
  if(fftz_Hwork)    Oc_FreeThreadLocal(fftz_Hwork,
                       fftz_Hwork_size*sizeof(OXS_FFT_REAL_TYPE));
  if(fftyz_Hwork_base)
                    Oc_FreeThreadLocal(fftyz_Hwork_base,
                                       fftyz_Hwork_base_size);
  if(fftyconvolve_Hwork)
                    Oc_FreeThreadLocal(fftyconvolve_Hwork,
                       fftyconvolve_Hwork_size*sizeof(OXS_FFT_REAL_TYPE));
  if(A_copy)        Oc_FreeThreadLocal(A_copy,
                       A_copy_size*sizeof(Oxs_Demag::A_coefs));
}

////////////////////////////////////////////////////////////////////////
// Oxs_Demag Constructor
Oxs_Demag::Oxs_Demag(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    rdimx(0),rdimy(0),rdimz(0),cdimx(0),cdimy(0),cdimz(0),
    adimx(0),adimy(0),adimz(0),
    xperiodic(0),yperiodic(0),zperiodic(0),
    mesh_id(0),
    A(0),asymptotic_radius(-1),Mtemp(0),
    MaxThreadCount(Oc_GetMaxThreadCount()),
    embed_block_size(0), embed_yzblock_size(0)
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
  if(Mtemp!=0)       { delete[] Mtemp;       Mtemp=0;       }
  Hxfrm_base.Free();
  Hxfrm_base_yz.Free();
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
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh==NULL) {
    // Rectangular, non-periodic mesh
    xperiodic=0; yperiodic=0; zperiodic=0;
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

  Mtemp = new OXS_FFT_REAL_TYPE[ODTV_VECSIZE*rdimx*rdimy*rdimz];
  /// Temporary space to hold Ms[]*m[].  The plan is to make this space
  /// unnecessary by introducing FFT functions that can take Ms as input
  /// and do the multiplication on the fly.

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
  // "ODTV_VECSIZE" here is because we work with arrays of ThreeVectors,
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

  // Compute block size for "convolution" embedded with inner FFT's.
  OC_INDEX footprint
    = ODTV_COMPLEXSIZE*ODTV_VECSIZE*sizeof(OXS_FFT_REAL_TYPE) // Data
    + sizeof(A_coefs)                           // Interaction matrix
    + 2*ODTV_COMPLEXSIZE*sizeof(OXS_FFT_REAL_TYPE); // Roots of unity
  if(cdimz<2) {
    footprint *= cdimy;  // Embed convolution with y-axis FFT's
  } else {
    footprint *= cdimz;  // Embed convolution with z-axis FFT's
  }
  {
    OC_INDEX trialsize = cache_size/(3*footprint); // "3" is fudge factor
    // Tweak trialsize to a good divisor of cdimx
    if(trialsize<=1) {
      trialsize = 1;
    } else if(trialsize<cdimx) {
      OC_INDEX bc = (cdimx + trialsize/2)/trialsize;
      trialsize = (cdimx + bc - 1)/bc;
      if(trialsize>32) {
        // Round to next multiple of eight
        if(trialsize%8) { trialsize += 8 - (trialsize%8); }
      }
    }
    embed_block_size = (trialsize>cdimx ? cdimx : trialsize);
    fprintf(stderr,"Embed block size=%ld\n",long(embed_block_size)); /**/
  }
  embed_yzblock_size = 1; // Default init value, for not used case.
  if(cdimz>1) {
    OC_INDEX trialsize = cache_size/(footprint*cdimy);
    if(trialsize<=1) {
      trialsize = 1;
    } else if(trialsize<cdimx) {
      OC_INDEX bc = (cdimx + trialsize/2)/trialsize;
      trialsize = (cdimx + bc - 1)/bc;
      if(trialsize>32) {
        // Round to next multiple of eight
        if(trialsize%8) { trialsize += 8 - (trialsize%8); }
      }
    }
    embed_yzblock_size = (trialsize>cdimx ? cdimx : trialsize);
    fprintf(stderr,"Embed yz-block size=%ld\n",long(embed_yzblock_size)); /**/
  }

  // The following 3 statements are cribbed from
  // Oxs_FFT3DThreeVector::SetDimensions().  The corresponding
  // code using that class is
  //
  //  Oxs_FFT3DThreeVector fft;
  //  fft.SetDimensions(rdimx,rdimy,rdimz,cdimx,cdimy,cdimz);
  //  fft.GetLogicalDimensions(ldimx,ldimy,ldimz);
  //
  // Set up non-threaded instances of FFT objects.  Threaded code sets
  // up it own copies using keyword "Oxs_DemagFFTObject" to
  // Oxs_ThreadRunObj::local_locker
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
               " to fit in an OC_INDEX variable");
    throw Oxs_ExtError(this,msg);
  }

  // Allocate memory for FFT xfrm target H, and scratch space
  // for computing interaction coefficients
  Hxfrm_base.SetSize(xfrm_size);
  OXS_FFT_REAL_TYPE* scratch = new OXS_FFT_REAL_TYPE[scratch_size];
  if(scratch==NULL) {
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

  if(!xperiodic && !yperiodic && !zperiodic) {
    // Calculate Nxx, Nxy and Nxz in first octant, non-periodic case.
    // Calculate Nxx, Nxy and Nxz in first octant.
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
      fprintf(stderr,"ANxx(%d,%d,%d) = %#.16g (threaded)\n",
              int(rdimx-1),int(rdimy-1),int(rdimz-1),
              ANxx.NxxAsymptotic(dx*(rdimx-1),dy*(rdimy-1),dz*(rdimz-1)));
      OC_INDEX icheck = ODTV_VECSIZE*(rdimx-1) + (rdimy-1)*sstridey + (rdimz-1)*sstridez;
      fprintf(stderr,"fft_scaling=%g, product=%#.16g\n",
              fft_scaling,scratch[icheck]);
#endif
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
  for(k=0;k<ldimz;++k) {
    for(j=0;j<ldimy;++j) {
      for(i=0;i<ldimx;++i) {
        OC_INDEX index = ODTV_VECSIZE*((k*ldimy+j)*ldimx+i);
#if 0
        printf("A00[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index]);
        printf("A01[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index+1]);
        printf("A02[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index+2]);
#else
        printf("A00/01/02[%02d][%02d][%02d] = %#25.12f %#25.12f %#25.12f\n",
               i,j,k,0.5*scratch[index],0.5*scratch[index+1],0.5*scratch[index+2]);
#endif
      }
    }
  }
  fflush(stdout);
#endif // NDEBUG

  // Step 4: Transform into frequency domain.  These lines are cribbed
  // from the corresponding code in Oxs_FFT3DThreeVector.
  // Note: Using an Oxs_FFT3DThreeVector fft object, this would be just
  //    fft.AdjustInputDimensions(ldimx,ldimy,ldimz);
  //    fft.ForwardRealToComplexFFT(scratch,Hxfrm);
  //    fft.AdjustInputDimensions(rdimx,rdimy,rdimz); // Safety
  {
    OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base.GetArrBase();
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

  }

  // Copy results from scratch into A00, A01, and A02.  We only need
  // store 1/8th of the results because of symmetries.
  OC_INDEX astridey = adimx;
  OC_INDEX astridez = astridey*adimy;
  OC_INDEX a_size = astridez*adimz;
  A = new A_coefs[a_size];

  OC_INDEX cstridey = 2*ODTV_VECSIZE*cdimx; // "2" for complex data
  OC_INDEX cstridez = cstridey*cdimy;
  {
    const OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base.GetArrBase();
    for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx;i++) {
          OC_INDEX aindex = i+j*astridey+k*astridez;
          OC_INDEX hindex = 2*ODTV_VECSIZE*i+j*cstridey+k*cstridez;
          A[aindex].A00 = Hxfrm[hindex];   // A00
          A[aindex].A01 = Hxfrm[hindex+2]; // A01
          A[aindex].A02 = Hxfrm[hindex+4]; // A02
          // The A## values are all real-valued, so we only need to
          // pull the real parts out of Hxfrm, which are stored in the
          // even offsets.
        }
  }

  // Repeat for Nyy, Nyz and Nzz. //////////////////////////////////////

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

  // Step 3: Use symmetries to reflect into other octants.
  //     Also, at each coordinate plane, set to 0.0 any term
  //     which is odd across that boundary.  It should already
  //     be close to 0, but will likely be slightly off due to
  //     rounding errors.
  // Symmetries: A00, A11, A22 are even in each coordinate
  //             A01 is odd in x and y, even in z.
  //             A02 is odd in x and z, even in y.
  //             A12 is odd in y and z, even in x.
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

#if VERBOSE_DEBUG && !defined(NDEBUG)
  for(k=0;k<ldimz;++k) {
    for(j=0;j<ldimy;++j) {
      for(i=0;i<ldimx;++i) {
        OC_INDEX index = ODTV_VECSIZE*((k*ldimy+j)*ldimx+i);
#if 0
        printf("A11[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index]);
        printf("A12[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index+1]);
        printf("A22[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index+2]);
#else
        printf("A11/12/22[%02d][%02d][%02d] = %#25.12f %#25.12f %#25.12f\n",
               i,j,k,0.5*scratch[index],0.5*scratch[index+1],0.5*scratch[index+2]);
#endif
      }
    }
  }
  fflush(stdout);
#endif // NDEBUG

  // Step 4: Transform into frequency domain.  These lines are cribbed
  // from the corresponding code in Oxs_FFT3DThreeVector.
  // Note: Using an Oxs_FFT3DThreeVector fft object, this would be just
  //    fft.AdjustInputDimensions(ldimx,ldimy,ldimz);
  //    fft.ForwardRealToComplexFFT(scratch,Hxfrm);
  //    fft.AdjustInputDimensions(rdimx,rdimy,rdimz); // Safety
  {
    OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base.GetArrBase();
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

  }

  // At this point we no longer need the "scratch" array, so release it.
  delete[] scratch;

  // Copy results from scratch into A11, A12, and A22.  We only need
  // store 1/8th of the results because of symmetries.
  {
    const OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base.GetArrBase();
    for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx;i++) {
          OC_INDEX aindex =   i+j*astridey+k*astridez;
          OC_INDEX hindex = 2*ODTV_VECSIZE*i+j*cstridey+k*cstridez;
          A[aindex].A11 = Hxfrm[hindex];   // A11
          A[aindex].A12 = Hxfrm[hindex+2]; // A12
          A[aindex].A22 = Hxfrm[hindex+4]; // A22
          // The A## values are all real-valued, so we only need to
          // pull the real parts out of Hxfrm, which are stored in the
          // even offsets.
        }
  }

#if REPORT_TIME
    inittime.Stop();
#endif // REPORT_TIME
}


class _Oxs_DemagFFTxThread : public Oxs_ThreadRunObj {
public:
  const Oxs_MeshValue<ThreeVector>* spin;
  const Oxs_MeshValue<OC_REAL8m>* Ms;

  OXS_FFT_REAL_TYPE* rarr;
  OXS_FFT_REAL_TYPE* carr;

  Oxs_Demag::Oxs_FFTLocker_Info locker_info;
  Oxs_FFT1DThreeVector* fftx;

  void GetNextSubtask(int threadnumber,
                      OC_INDEX& jkstart,OC_INDEX& jkstop) {
    // NOTE: Import jkstop *must* be zero on first call.
    // Currently, this routine only returns a task on the
    // first call (per thread).
    if(jkstop>0) {
      jkstart=jkstop;  // Fini
      return;      
    }
    OC_INDEX istart,istop;
    const Oxs_StripedArray<ThreeVector>* arrblock = spin->GetArrayBlock();
    arrblock->GetStripPosition(threadnumber,istart,istop);

    // Round both istart and istop up to the next row start (i.e, (x,y,z)
    // such that x==0).
    jkstart = ((istart+spin_xdim-1)/spin_xdim);
    jkstop  = ((istop +spin_xdim-1)/spin_xdim);
  }

  OC_INDEX spin_xdim;
  OC_INDEX spin_xydim;

  OC_INDEX j_dim;
  OC_INDEX j_rstride;
  OC_INDEX j_cstride;

  OC_INDEX k_rstride;
  OC_INDEX k_cstride;

  OC_INDEX jk_max;

  enum { INVALID, FORWARD, INVERSE } direction;
  _Oxs_DemagFFTxThread()
    : rarr(0),carr(0),
      fftx(0),
      spin_xdim(0),spin_xydim(0),
      j_dim(0),j_rstride(0),j_cstride(0),
      k_rstride(0),k_cstride(0),
      jk_max(0),
      direction(INVALID) {}
  void Cmd(int threadnumber, void* data);
};

void _Oxs_DemagFFTxThread::Cmd(int threadnumber, void* /* data */)
{
  // Thread local storage
  if(!fftx) {
    Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info.name);
    if(!foo) {
      // Oxs_FFTLocker object not constructed
      foo = new Oxs_Demag::Oxs_FFTLocker(locker_info);
      local_locker.AddItem(locker_info.name,foo);
    }
    fftx = &(dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo)->fftx);
    if(!fftx) {
      Oxs_ThreadError::SetError(String("Error in"
         "_Oxs_DemagFFTxThread::Cmd(): fftx downcast failed."));
      return;
    }
  }

  OC_INDEX jkstart,jkstop;
  jkstart = jkstop = 0;  // Initialize to request first subtask
  while(1) {
    GetNextSubtask(threadnumber,jkstart,jkstop);
    if(jkstart>=jkstop) break;
    assert(jkstop<=jk_max);

    OC_INDEX kstart = jkstart/j_dim;
    OC_INDEX jstart = jkstart - kstart*j_dim;

    OC_INDEX kstop = jkstop/j_dim;
    OC_INDEX jstop = jkstop - kstop*j_dim;

    for(OC_INDEX k=kstart;k<=kstop;++k) {
      OC_INDEX j_line_stop = j_dim;
      if(k==kstop) j_line_stop = jstop;
      if(jstart<j_line_stop) {
        fftx->AdjustArrayCount(j_line_stop - jstart);
        if(direction == FORWARD) {
          const OC_INDEX istart = jstart*spin_xdim + k*spin_xydim;
          fftx->ForwardRealToComplexFFT(static_cast<const OC_REAL8m*>(&((*spin)[istart].x)), // CHEAT
                                        carr+jstart*j_cstride+k*k_cstride,
                                        static_cast<const OC_REAL8m*>(&((*Ms)[istart]))); // CHEAT
        } else { // direction == INVERSE
          fftx->InverseComplexToRealFFT(carr+jstart*j_cstride+k*k_cstride,
                                        rarr+jstart*j_rstride+k*k_rstride);
        }
      }
      jstart=0;
    }
  }
}

class _Oxs_DemagiFFTxDotThread : public Oxs_ThreadRunObj {
public:
  static Oxs_Mutex result_mutex;
  static Nb_Xpfloat energy_sum;

  OXS_FFT_REAL_TYPE* carr;
  const Oxs_MeshValue<ThreeVector> *spin_ptr;
  const Oxs_MeshValue<OC_REAL8m> *Ms_ptr;
  Oxs_ComputeEnergyData* oced_ptr;

  Oxs_Demag::Oxs_FFTLocker_Info locker_info;
  Oxs_Demag::Oxs_FFTLocker* locker;

  void GetNextSubtask(int threadnumber,
                      OC_INDEX& jkstart,OC_INDEX& jkstop) {
    // NOTE: Import jkstop *must* be zero on first call.
    // Currently, this routine only returns a task on the
    // first call (per thread).
    if(jkstop>0) {
      jkstart=jkstop;  // Fini
      return;      
    }
    OC_INDEX istart,istop;
    const Oxs_StripedArray<ThreeVector>* arrblock = spin_ptr->GetArrayBlock();
    arrblock->GetStripPosition(threadnumber,istart,istop);

    // Round both istart and istop up to the next row start (i.e, (x,y,z)
    // such that x==0).
    jkstart = ((istart+rdimx-1)/rdimx);
    jkstop  = ((istop +rdimx-1)/rdimx);
  }


  OC_INDEX rdimx;

  OC_INDEX j_dim;
  OC_INDEX j_rstride;
  OC_INDEX j_cstride;

  OC_INDEX k_rstride;
  OC_INDEX k_cstride;

  OC_INDEX jk_max;

  _Oxs_DemagiFFTxDotThread()
    : carr(0),
      spin_ptr(0), Ms_ptr(0), oced_ptr(0), locker(0),
      rdimx(0), 
      j_dim(0), j_rstride(0), j_cstride(0),
      k_rstride(0), k_cstride(0),
      jk_max(0) {}
  void Cmd(int threadnumber, void* data);

  static void Init() {
    result_mutex.Lock();
    energy_sum = 0.0;
    result_mutex.Unlock();
  }
};

Oxs_Mutex _Oxs_DemagiFFTxDotThread::result_mutex;
Nb_Xpfloat _Oxs_DemagiFFTxDotThread::energy_sum(0.0);

void _Oxs_DemagiFFTxDotThread::Cmd(int threadnumber, void* /* data */)
{
  // Thread local storage
  if(!locker) {
    Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info.name);
    if(!foo) {
      // Oxs_FFTLocker object not constructed
      foo = new Oxs_Demag::Oxs_FFTLocker(locker_info);
      local_locker.AddItem(locker_info.name,foo);
    }
    locker = dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo);
    if(!locker) {
      Oxs_ThreadError::SetError(String("Error in"
         "_Oxs_DemagFFTxDotThread::Cmd(): locker downcast failed."));
      return;
    }
  }
  Oxs_FFT1DThreeVector* const fftx = &(locker->fftx);

  const OXS_FFT_REAL_TYPE emult =  -0.5 * MU0;

  const Oxs_MeshValue<ThreeVector>& spin = *spin_ptr;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *Ms_ptr;
  Oxs_ComputeEnergyData& oced = *oced_ptr;

  const OC_INDEX ijstride = j_rstride/ODTV_VECSIZE;
  const OC_INDEX ikstride = k_rstride/ODTV_VECSIZE;

  Nb_Xpfloat energy_sumA(0.0), energy_sumB(0.0);

  fftx->AdjustArrayCount(1);

  OC_INDEX jkstart,jkstop;
  jkstart = jkstop = 0;  // Initialize to request first subtask
  while(1) {
    GetNextSubtask(threadnumber,jkstart,jkstop);
    if(jkstart>=jkstop) break;
    assert(jkstop<=jk_max);

    OC_INDEX kstart = jkstart/j_dim;
    OC_INDEX jstart = jkstart - kstart*j_dim;

    OC_INDEX kstop = jkstop/j_dim;
    OC_INDEX jstop = jkstop - kstop*j_dim;

    for(OC_INDEX k=kstart;k<=kstop;++k,jstart=0) {
      OC_INDEX j_line_stop = j_dim;
      if(k==kstop) j_line_stop = jstop;
      if(jstart>=j_line_stop) continue;

      for(OC_INDEX j=jstart;j<j_line_stop;++j) {
        const OC_INDEX ioffset = (j*ijstride+k*ikstride);
        OXS_FFT_REAL_TYPE* scratch = (locker->ifftx_scratch)
          + (ioffset+ (OC_UINDEX(locker->ifftx_scratch)%16)/8)%2;
        /// The base offsets of all import/export arrays are 16-byte
        /// aligned.  For SSE code, we want scratch to align with the
        /// working piece of these arrays, so offset by 1 if necessary.
        /// Note that ifftx_scratch is allocated to size info.rdimx+1
        /// exactly to allow this.

        fftx->InverseComplexToRealFFT(carr+j*j_cstride+k*k_cstride,scratch);

        if(oced.H) {
          for(OC_INDEX i=0;i<rdimx;++i) {
            (*oced.H)[ioffset + i].Set(scratch[3*i],scratch[3*i+1],scratch[3*i+2]);
          }
        }

        if(oced.H_accum) {
          for(OC_INDEX i=0;i<rdimx;++i) {
            (*oced.H_accum)[ioffset + i]
              += ThreeVector(scratch[3*i],scratch[3*i+1],scratch[3*i+2]);
          }
        }

        // The usual cases are
        //
        //     oced.energy!=0, .energy_accum==0, .mxH!=0, .mxH_accum==0
        // or
        //     oced.energy==0, .energy_accum!=0, .mxH==0, .mxH_accum!=0
        //
        // depending on whether (first) or not (second) this term is the
        // first non-chunk energy.  Handle these two cases specially so
        // as to avoid code branches.
        if(sizeof(OXS_FFT_REAL_TYPE) == 8 &&  // Check SSE OK
        ((oced.energy && !oced.energy_accum && oced.mxH && !oced.mxH_accum) ||
         (!oced.energy && oced.energy_accum && !oced.mxH && oced.mxH_accum))) {

          const ThreeVector* ispin = &(spin[ioffset]);
          const OC_REAL8m* iMs     = &(Ms[ioffset]);

          OC_REAL8m* ienergy=0;
          if(oced.energy) ienergy= &((*oced.energy)[ioffset]);

          OC_REAL8m* ienergy_acc=0;
          if(oced.energy_accum) ienergy_acc= &((*oced.energy_accum)[ioffset]);

          ThreeVector* imxH=0;
          if(oced.mxH) imxH = &((*oced.mxH)[ioffset]);

          ThreeVector* imxH_acc=0;
          if(oced.mxH_accum) imxH_acc = &((*oced.mxH_accum)[ioffset]);

          // Note: The following alignment jig works identically for all of
          // the arrays because all have the same length modulo the ALIGNMENT
          // value.  (Note special handling of the scratch array above.)
          const OC_INDEX ALIGNMENT = 16;  // In bytes
          const OC_INDEX ibreak = (OC_UINDEX(ispin)%ALIGNMENT)/8;
          const OC_INDEX STRIDE = 2;
#if OC_USE_SSE
          assert(size_t(&(ispin[ibreak]))%ALIGNMENT==0);
          assert(size_t(&(scratch[ibreak]))%ALIGNMENT==0);
          assert(size_t(&(iMs[ibreak]))%ALIGNMENT==0);
          assert(!ienergy || size_t(&(ienergy[ibreak]))%ALIGNMENT==0);
          assert(!imxH || size_t(&(imxH[ibreak]))%ALIGNMENT==0);
          assert(!ienergy_acc || size_t(&(ienergy_acc[ibreak]))%ALIGNMENT==0);
          assert(!imxH_acc || size_t(&(imxH_acc[ibreak]))%ALIGNMENT==0);
#endif
          OC_INDEX i;
          for(i=0;i<ibreak;++i) {
            const ThreeVector& m = ispin[i];
            const OXS_FFT_REAL_TYPE& tHx = scratch[3*i];
            const OXS_FFT_REAL_TYPE& tHy = scratch[3*i+1];
            const OXS_FFT_REAL_TYPE& tHz = scratch[3*i+2];
            OC_REAL8m ei = emult * iMs[i] * (m.x * tHx + m.y * tHy + m.z * tHz);
            OC_REAL8m tx = m.y * tHz - m.z * tHy; // mxH
            OC_REAL8m ty = m.z * tHx - m.x * tHz;
            OC_REAL8m tz = m.x * tHy - m.y * tHx;
            energy_sumA += ei;
            if(ienergy)         ienergy[i]  = ei;
            if(ienergy_acc) ienergy_acc[i] += ei;
            if(imxH)               imxH[i]  = ThreeVector(tx,ty,tz);
            if(imxH_acc)       imxH_acc[i] += ThreeVector(tx,ty,tz);
          }

          if(ienergy) { // non-accum forms
            for(;i+STRIDE-1<rdimx;i+=STRIDE) {
              Oc_Duet mx,my,mz;
              Oxs_ThreeVectorPairLoadAligned(&(ispin[i]),mx,my,mz);
              
              Oc_Duet tHx,tHy,tHz;
              Oxs_ThreeVectorPairLoadAligned((Oxs_ThreeVector*)(&(scratch[3*i])),
                                             tHx,tHy,tHz);

              Oc_Duet tMs;  tMs.LoadAligned(iMs[i]);

              Oc_Duet ei = Oc_Duet(emult)*tMs*(mx*tHx + my*tHy + mz*tHz);

              Oc_Duet tx = my * tHz - mz * tHy; // mxH
              Oc_Duet ty = mz * tHx - mx * tHz;
              Oc_Duet tz = mx * tHy - my * tHx;

              Nb_XpfloatDualAccum(energy_sumA, energy_sumB, ei);
            
              ei.StoreStream(ienergy[i]);
              Oxs_ThreeVectorPairStreamAligned(tx,ty,tz,&(imxH[i]));
            }
          } else { // accum form
            for(;i+STRIDE-1<rdimx;i+=STRIDE) {
              Oc_Duet mx,my,mz;
              Oxs_ThreeVectorPairLoadAligned(&(ispin[i]),mx,my,mz);
              
              Oc_Duet tHx,tHy,tHz;
              Oxs_ThreeVectorPairLoadAligned((Oxs_ThreeVector*)(&(scratch[3*i])),
                                             tHx,tHy,tHz);

              Oc_Duet tMs;  tMs.LoadAligned(iMs[i]);

              Oc_Duet ei = Oc_Duet(emult)*tMs*(mx*tHx + my*tHy + mz*tHz);

              Oc_Duet tx,ty,tz;
              Oxs_ThreeVectorPairLoadAligned((Oxs_ThreeVector*)(&(imxH_acc[i])),
                                             tx,ty,tz);
              tx += my * tHz - mz * tHy; // mxH
              ty += mz * tHx - mx * tHz;
              tz += mx * tHy - my * tHx;
              Oxs_ThreeVectorPairStoreAligned(tx,ty,tz,
                                           (Oxs_ThreeVector*)(&(imxH_acc[i])));

              Oc_Duet ie_acc; ie_acc.LoadAligned(ienergy_acc[i]);
              ie_acc += ei;
              ie_acc.StoreAligned(ienergy_acc[i]);

              Nb_XpfloatDualAccum(energy_sumA, energy_sumB, ei);
            }
          }

          for(;i<rdimx;++i) {
            const ThreeVector& m = ispin[i];
            const OXS_FFT_REAL_TYPE& tHx = scratch[3*i];
            const OXS_FFT_REAL_TYPE& tHy = scratch[3*i+1];
            const OXS_FFT_REAL_TYPE& tHz = scratch[3*i+2];
            OC_REAL8m ei = emult * iMs[i] * (m.x * tHx + m.y * tHy + m.z * tHz);
            OC_REAL8m tx = m.y * tHz - m.z * tHy; // mxH
            OC_REAL8m ty = m.z * tHx - m.x * tHz;
            OC_REAL8m tz = m.x * tHy - m.y * tHx;
            energy_sumA += ei;
            if(ienergy)         ienergy[i]  = ei;
            if(ienergy_acc) ienergy_acc[i] += ei;
            if(imxH)               imxH[i]  = ThreeVector(tx,ty,tz);
            if(imxH_acc)       imxH_acc[i] += ThreeVector(tx,ty,tz);
          }

        } else { // oced.energy && !oced.energy_accum ...
          OC_INDEX i;
          for(i=0;i<rdimx;++i) {
            const ThreeVector& m = spin[ioffset+i];
            const OXS_FFT_REAL_TYPE& tHx = scratch[3*i];
            const OXS_FFT_REAL_TYPE& tHy = scratch[3*i+1];
            const OXS_FFT_REAL_TYPE& tHz = scratch[3*i+2];

            OC_REAL8m ei = emult * Ms[ioffset+i] * (m.x * tHx + m.y * tHy + m.z * tHz);

            OC_REAL8m tx = m.y * tHz - m.z * tHy; // mxH
            OC_REAL8m ty = m.z * tHx - m.x * tHz;
            OC_REAL8m tz = m.x * tHy - m.y * tHx;

            energy_sumA += ei;

            if(oced.energy)       (*oced.energy)[ioffset + i] = ei;
            if(oced.energy_accum) (*oced.energy_accum)[ioffset + i] += ei;

            if(oced.mxH)          (*oced.mxH)[ioffset + i] = ThreeVector(tx,ty,tz);
            if(oced.mxH_accum)    (*oced.mxH_accum)[ioffset + i] += ThreeVector(tx,ty,tz);

#if defined(__GNUC__) && __GNUC__ == 4                  \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
            Oc_Nop(i);  // gcc 4.1.2 20080704 (Red Hat 4.1.2-44) (others?)
            /// has an optimization bug that segfaults the above code block
            /// if -fprefetch-loop-arrays is enabled.  This nop call is a
            /// workaround.
            /// i686-apple-darwin9-gcc-4.0.1 (OS X Leopard) (others?) has an
            /// optimization bug that segfaults the above code block if
            /// -fstrict-aliasing (or -fast, which includes -fstrict-aliasing)
            /// is enabled.  This nop call is a workaround.
#endif
          } // oced.energy && !oced.energy_accum ...
        } // for-i
      } // for-j
    }   // for-k
  }     // while(1)

  energy_sumA += energy_sumB;
  result_mutex.Lock();
  energy_sum += energy_sumA;
  result_mutex.Unlock();
}

class _Oxs_DemagFFTyThread : public Oxs_ThreadRunObj {
public:
  OXS_FFT_REAL_TYPE* carr;

  Oxs_Demag::Oxs_FFTLocker_Info locker_info;
  Oxs_FFTStrided* ffty;

  static _Oxs_DemagJobControl job_control;

  OC_INDEX k_stride;
  OC_INDEX k_dim;
  OC_INDEX i_dim;

  enum { INVALID, FORWARD, INVERSE } direction;
  _Oxs_DemagFFTyThread()
    : carr(0), ffty(0),
      k_stride(0), k_dim(0),
      i_dim(0), direction(INVALID) {}
  void Cmd(int threadnumber, void* data);
};

_Oxs_DemagJobControl _Oxs_DemagFFTyThread::job_control;

void _Oxs_DemagFFTyThread::Cmd(int /* threadnumber */, void* /* data */)
{
  // Thread local storage
  if(!ffty) {
    Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info.name);
    if(!foo) {
      // Oxs_FFTLocker object not constructed
      foo = new Oxs_Demag::Oxs_FFTLocker(locker_info);
      local_locker.AddItem(locker_info.name,foo);
    }
    ffty = &(dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo)->ffty);
    if(!ffty) {
      Oxs_ThreadError::SetError(String("Error in"
         "_Oxs_DemagFFTyThread::Cmd(): ffty downcast failed."));
      return;
    }
  }

  const OC_INDEX istride = ODTV_COMPLEXSIZE;

  while(1) {
    OC_INDEX ikstart,ikstop;
    job_control.ClaimJob(ikstart,ikstop);

    OC_INDEX kstart = ikstart/i_dim;
    OC_INDEX istart = ikstart - kstart*i_dim;

    OC_INDEX kstop = ikstop/i_dim;
    OC_INDEX istop = ikstop - kstop*i_dim;

    if(kstart>=k_dim) break;
    for(OC_INDEX k=kstart;k<=kstop;++k,istart=0) {
      OC_INDEX i_line_stop = i_dim;
      if(k==kstop) i_line_stop = istop;
      if(istart>=i_line_stop) continue;
      ffty->AdjustArrayCount(i_line_stop - istart);
      if(direction == FORWARD) {
        ffty->ForwardFFT(carr+istart*istride+k*k_stride);
      } else { // direction == INVERSE
        ffty->InverseFFT(carr+istart*istride+k*k_stride);
      }
    }

  }
}

class _Oxs_DemagFFTyConvolveThread : public Oxs_ThreadRunObj {
  // *ONLY* for use when cdimz==1
public:
  OXS_FFT_REAL_TYPE* Hxfrm;
  const Oxs_Demag::A_coefs* A;

  Oxs_Demag::Oxs_FFTLocker_Info locker_info;
  Oxs_Demag::Oxs_FFTLocker* locker;

  static _Oxs_DemagJobControl job_control;

  OC_INDEX embed_block_size;
  OC_INDEX jstride,ajstride;
  OC_INDEX i_dim;

  OC_INDEX rdimy,adimy,cdimy;

  _Oxs_DemagFFTyConvolveThread()
    : Hxfrm(0), A(0), locker(0),
      embed_block_size(0),
      jstride(0),ajstride(0),
      i_dim(0),
      rdimy(0), adimy(0), cdimy(0) {}
  void Cmd(int threadnumber, void* data);
};

_Oxs_DemagJobControl _Oxs_DemagFFTyConvolveThread::job_control;

void _Oxs_DemagFFTyConvolveThread::Cmd(int /* threadnumber */, void* /* data */)
{
  // Thread local storage
  if(!locker) {
    Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info.name);
    if(!foo) {
      // Oxs_FFTLocker object not constructed
      foo = new Oxs_Demag::Oxs_FFTLocker(locker_info);
      local_locker.AddItem(locker_info.name,foo);
    }
    locker = dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo);
    if(!locker) {
      Oxs_ThreadError::SetError(String("Error in"
         "_Oxs_DemagFFTyConvolveThread::Cmd(): locker downcast failed."));
      return;
    }
  }
  Oxs_FFTStrided* const ffty = &(locker->ffty);

  // Hwork:  Data is copied from Hxfrm into and out of this space
  // on each m increment.
  const OC_INDEX Hwstride = ODTV_VECSIZE*ODTV_COMPLEXSIZE*embed_block_size;
  OXS_FFT_REAL_TYPE* const Hwork = locker->fftyconvolve_Hwork;

  // Adjust ffty to use Hwork
  ffty->AdjustInputDimensions(rdimy,Hwstride,
                              ODTV_VECSIZE*embed_block_size);

  const OC_INDEX istride = ODTV_COMPLEXSIZE*ODTV_VECSIZE;
  while(1) {
    OC_INDEX istart,istop;
    job_control.ClaimJob(istart,istop);

    if(istart>=i_dim) break;

    for(OC_INDEX ix=istart;ix<istop;ix+=embed_block_size) {
      OC_INDEX j;

      OC_INDEX ix_end = ix + embed_block_size;
      if(ix_end>istop) ix_end = istop;

      // Copy data from Hxfrm into Hwork
      const size_t Hcopy_line_size
        = static_cast<size_t>(istride*(ix_end-ix))*sizeof(OXS_FFT_REAL_TYPE);
      for(j=0;j<rdimy;++j) {
        const OC_INDEX windex = j*Hwstride;
        const OC_INDEX hindex = j*jstride + ix*istride;
        memcpy(Hwork+windex,Hxfrm+hindex,Hcopy_line_size);
      }

      ffty->AdjustArrayCount((ix_end-ix)*ODTV_VECSIZE);
      ffty->ForwardFFT(Hwork);
        
      { // j==0
        for(OC_INDEX i=ix;i<ix_end;++i) {
          const Oxs_Demag::A_coefs& Aref = A[i];
          {
            OC_INDEX  index = istride*(i-ix);
            OXS_FFT_REAL_TYPE Hx_re = Hwork[index];
            OXS_FFT_REAL_TYPE Hx_im = Hwork[index+1];
            OXS_FFT_REAL_TYPE Hy_re = Hwork[index+2];
            OXS_FFT_REAL_TYPE Hy_im = Hwork[index+3];
            OXS_FFT_REAL_TYPE Hz_re = Hwork[index+4];
            OXS_FFT_REAL_TYPE Hz_im = Hwork[index+5];

            Hwork[index]   = Aref.A00*Hx_re + Aref.A01*Hy_re + Aref.A02*Hz_re;
            Hwork[index+1] = Aref.A00*Hx_im + Aref.A01*Hy_im + Aref.A02*Hz_im;
            Hwork[index+2] = Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
            Hwork[index+3] = Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
            Hwork[index+4] = Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
            Hwork[index+5] = Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
        }
      }

      for(j=1;j<cdimy/2;++j) {
        OC_INDEX ajindex = j*ajstride;
        OC_INDEX  jindex = j*Hwstride;
        OC_INDEX  j2index = (cdimy-j)*Hwstride;

        for(OC_INDEX i=ix;i<ix_end;++i) {
          const Oxs_Demag::A_coefs& Aref = A[ajindex+i];
          { // j>0
            OC_INDEX  index = jindex + istride*(i-ix);
            OXS_FFT_REAL_TYPE Hx_re = Hwork[index];
            OXS_FFT_REAL_TYPE Hx_im = Hwork[index+1];
            OXS_FFT_REAL_TYPE Hy_re = Hwork[index+2];
            OXS_FFT_REAL_TYPE Hy_im = Hwork[index+3];
            OXS_FFT_REAL_TYPE Hz_re = Hwork[index+4];
            OXS_FFT_REAL_TYPE Hz_im = Hwork[index+5];

            Hwork[index]   = Aref.A00*Hx_re + Aref.A01*Hy_re + Aref.A02*Hz_re;
            Hwork[index+1] = Aref.A00*Hx_im + Aref.A01*Hy_im + Aref.A02*Hz_im;
            Hwork[index+2] = Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
            Hwork[index+3] = Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
            Hwork[index+4] = Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
            Hwork[index+5] = Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
          { // j2<0
            OC_INDEX  index2 = j2index + istride*(i-ix);
            OXS_FFT_REAL_TYPE Hx_re = Hwork[index2];
            OXS_FFT_REAL_TYPE Hx_im = Hwork[index2+1];
            OXS_FFT_REAL_TYPE Hy_re = Hwork[index2+2];
            OXS_FFT_REAL_TYPE Hy_im = Hwork[index2+3];
            OXS_FFT_REAL_TYPE Hz_re = Hwork[index2+4];
            OXS_FFT_REAL_TYPE Hz_im = Hwork[index2+5];

            // Flip signs on a01 and a12 as compared to the j>=0
            // case because a01 and a12 are odd in y.
            Hwork[index2]   =  Aref.A00*Hx_re - Aref.A01*Hy_re + Aref.A02*Hz_re;
            Hwork[index2+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im + Aref.A02*Hz_im;
            Hwork[index2+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
            Hwork[index2+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
            Hwork[index2+4] =  Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;
            Hwork[index2+5] =  Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
        }
      }

      // Note special case: If cdimy==adimy==1, then cdimy/2=0, and so j
      // coming out from the previous loop is 1, which is different from
      // cdimy/2.  In this case we want to run *only* the j=0 loop farther
      // above, and not either of the others.
      for(;j<adimy;++j) {
        OC_INDEX ajindex = j*ajstride;
        OC_INDEX  jindex = j*Hwstride;
        for(OC_INDEX i=ix;i<ix_end;++i) {
          const Oxs_Demag::A_coefs& Aref = A[ajindex+i];
          { // j>0
            OC_INDEX  index = jindex + istride*(i-ix);
            OXS_FFT_REAL_TYPE Hx_re = Hwork[index];
            OXS_FFT_REAL_TYPE Hx_im = Hwork[index+1];
            OXS_FFT_REAL_TYPE Hy_re = Hwork[index+2];
            OXS_FFT_REAL_TYPE Hy_im = Hwork[index+3];
            OXS_FFT_REAL_TYPE Hz_re = Hwork[index+4];
            OXS_FFT_REAL_TYPE Hz_im = Hwork[index+5];

            Hwork[index]   = Aref.A00*Hx_re + Aref.A01*Hy_re + Aref.A02*Hz_re;
            Hwork[index+1] = Aref.A00*Hx_im + Aref.A01*Hy_im + Aref.A02*Hz_im;
            Hwork[index+2] = Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
            Hwork[index+3] = Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
            Hwork[index+4] = Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
            Hwork[index+5] = Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
        }
      }

      ffty->InverseFFT(Hwork);

      // Copy data from Hwork back into Hxfrm
      for(j=0;j<rdimy;++j) {
        const OC_INDEX windex = j*Hwstride;
        const OC_INDEX hindex = j*jstride + ix*istride;
        memcpy(Hxfrm+hindex,Hwork+windex,Hcopy_line_size);
      }

    }
  }
}

class _Oxs_DemagFFTzConvolveThread : public Oxs_ThreadRunObj {
public:
  OXS_FFT_REAL_TYPE* Hxfrm;
  const Oxs_Demag::A_coefs* A;

  Oxs_Demag::Oxs_FFTLocker_Info locker_info;
  Oxs_Demag::Oxs_FFTLocker* locker;

  static _Oxs_DemagJobControl job_control;

  OC_INDEX thread_count;
  OC_INDEX cdimx,cdimy,cdimz;
  OC_INDEX adimx,adimy,adimz;
  OC_INDEX rdimz;
  OC_INDEX embed_block_size;
  OC_INDEX jstride,ajstride;
  OC_INDEX kstride,akstride;
  _Oxs_DemagFFTzConvolveThread()
    : Hxfrm(0), A(0), locker(0),
      thread_count(0),
      cdimx(0),cdimy(0),cdimz(0),
      adimx(0),adimy(0),adimz(0),rdimz(0),
      embed_block_size(0),
      jstride(0),ajstride(0),kstride(0),akstride(0) {}
  void Cmd(int threadnumber, void* data);
};

_Oxs_DemagJobControl _Oxs_DemagFFTzConvolveThread::job_control;

void _Oxs_DemagFFTzConvolveThread::Cmd(int /* threadnumber */, void* /* data */)
{
  // Thread local storage
  if(!locker) {
    Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info.name);
    if(!foo) {
      // Oxs_FFTLocker object not constructed
      foo = new Oxs_Demag::Oxs_FFTLocker(locker_info);
      local_locker.AddItem(locker_info.name,foo);
    }
    locker = dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo);
    if(!locker) {
      Oxs_ThreadError::SetError(String("Error in"
         "_Oxs_DemagFFTzConvolveThread::Cmd(): locker downcast failed."));
      return;
    }
  }
  Oxs_FFTStrided* const fftz = &(locker->fftz);

  // Hwork:  Data is copied from Hxfrm into and out of this space
  // on each m increment.  Hwork1 shadows the active j>=0 block of
  // Hxfrm, Hwork2 the j<0 block.
  const OC_INDEX Hwstride = ODTV_VECSIZE*ODTV_COMPLEXSIZE*embed_block_size;
  OXS_FFT_REAL_TYPE* const Hwork1 = locker->fftz_Hwork;
  OXS_FFT_REAL_TYPE* const Hwork2 = Hwork1 + Hwstride * cdimz;

  if(locker->A_copy == NULL) {
    size_t asize = static_cast<size_t>(adimx*adimy*adimz);
    locker->A_copy_size = asize;
    locker->A_copy = static_cast<Oxs_Demag::A_coefs*>
      (Oc_AllocThreadLocal(asize*sizeof(Oxs_Demag::A_coefs)));
    memcpy(locker->A_copy,A,asize*sizeof(Oxs_Demag::A_coefs));
  }
  const Oxs_Demag::A_coefs* const Acopy = locker->A_copy;

  // Adjust fftz to use Hwork
  fftz->AdjustInputDimensions(rdimz,Hwstride,
                              ODTV_VECSIZE*embed_block_size);

  while(1) {
    OC_INDEX i,j,k;

    OC_INDEX jstart,jstop;
    job_control.ClaimJob(jstart,jstop);
    if(jstart>=jstop) break;

    for(j=jstart;j<jstop;++j) {
      // j>=0
      const OC_INDEX  jindex = j*jstride;
      const OC_INDEX ajindex = j*ajstride;

      const OC_INDEX j2 = cdimy - j;
      const OC_INDEX j2index = j2*jstride;

      fftz->AdjustArrayCount(ODTV_VECSIZE*embed_block_size);
      for(OC_INDEX m=0;m<cdimx;m+=embed_block_size) {

        // Do one block of forward z-direction transforms
        OC_INDEX istop_tmp = m + embed_block_size;
        if(embed_block_size>cdimx-m) {
          // Partial block
          fftz->AdjustArrayCount(ODTV_VECSIZE*(cdimx-m));
          istop_tmp = cdimx;
        }
        const OC_INDEX istop = istop_tmp;

        // Copy data into Hwork
        const size_t Hcopy_line_size
          = static_cast<size_t>(ODTV_COMPLEXSIZE*ODTV_VECSIZE*(istop-m))
          *sizeof(OXS_FFT_REAL_TYPE);
        for(k=0;k<rdimz;++k) {
          const OC_INDEX windex = k*Hwstride;
          const OC_INDEX h1index = k*kstride + m*ODTV_COMPLEXSIZE*ODTV_VECSIZE
            + jindex;
          memcpy(Hwork1+windex,Hxfrm+h1index,Hcopy_line_size);
        }
        if(adimy<=j2 && j2<cdimy) {
          for(k=0;k<rdimz;++k) {
            const OC_INDEX windex = k*Hwstride;
            const OC_INDEX h2index = k*kstride + m*ODTV_COMPLEXSIZE*ODTV_VECSIZE
              + j2index;
            memcpy(Hwork2+windex,Hxfrm+h2index,Hcopy_line_size);
          }
        }

        fftz->ForwardFFT(Hwork1);
        if(adimy<=j2 && j2<cdimy) {
          fftz->ForwardFFT(Hwork2);
        }

        // Do matrix-vector multiply ("convolution") for block
        for(k=0;k<adimz;++k) {
          // k>=0
          const OC_INDEX windex = k*Hwstride;
          const OC_INDEX akindex = ajindex + k*akstride;
          for(i=m;i<istop;++i) {
            const Oxs_Demag::A_coefs& Aref = Acopy[akindex+i];
            const OC_INDEX index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*(i-m)+windex;
            {
              OXS_FFT_REAL_TYPE Hx_re = Hwork1[index];
              OXS_FFT_REAL_TYPE Hx_im = Hwork1[index+1];
              OXS_FFT_REAL_TYPE Hy_re = Hwork1[index+2];
              OXS_FFT_REAL_TYPE Hy_im = Hwork1[index+3];
              OXS_FFT_REAL_TYPE Hz_re = Hwork1[index+4];
              OXS_FFT_REAL_TYPE Hz_im = Hwork1[index+5];

              Hwork1[index]   = Aref.A00*Hx_re + Aref.A01*Hy_re + Aref.A02*Hz_re;
              Hwork1[index+1] = Aref.A00*Hx_im + Aref.A01*Hy_im + Aref.A02*Hz_im;
              Hwork1[index+2] = Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
              Hwork1[index+3] = Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
              Hwork1[index+4] = Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
              Hwork1[index+5] = Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
            }
            if(adimy<=j2 && j2<cdimy) {
              OXS_FFT_REAL_TYPE Hx_re = Hwork2[index];
              OXS_FFT_REAL_TYPE Hx_im = Hwork2[index+1];
              OXS_FFT_REAL_TYPE Hy_re = Hwork2[index+2];
              OXS_FFT_REAL_TYPE Hy_im = Hwork2[index+3];
              OXS_FFT_REAL_TYPE Hz_re = Hwork2[index+4];
              OXS_FFT_REAL_TYPE Hz_im = Hwork2[index+5];

              // Flip signs on a01 and a12 as compared to the j>=0
              // case because a01 and a12 are odd in y.
              Hwork2[index]   =  Aref.A00*Hx_re - Aref.A01*Hy_re + Aref.A02*Hz_re;
              Hwork2[index+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im + Aref.A02*Hz_im;
              Hwork2[index+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
              Hwork2[index+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
              Hwork2[index+4] =  Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;
              Hwork2[index+5] =  Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
            }
          }
        }
        for(k=adimz;k<cdimz;++k) {
          // k<0
          const OC_INDEX windex = k*Hwstride;
          const OC_INDEX akindex = ajindex + (cdimz-k)*akstride;
          for(i=m;i<istop;++i) {
            const Oxs_Demag::A_coefs& Aref = Acopy[akindex+i];
            const OC_INDEX index = ODTV_COMPLEXSIZE*ODTV_VECSIZE*(i-m)+windex;
            {
              OXS_FFT_REAL_TYPE Hx_re = Hwork1[index];
              OXS_FFT_REAL_TYPE Hx_im = Hwork1[index+1];
              OXS_FFT_REAL_TYPE Hy_re = Hwork1[index+2];
              OXS_FFT_REAL_TYPE Hy_im = Hwork1[index+3];
              OXS_FFT_REAL_TYPE Hz_re = Hwork1[index+4];
              OXS_FFT_REAL_TYPE Hz_im = Hwork1[index+5];

              // Flip signs on a02 and a12 as compared to the k>=0, j>=0 case
              // because a02 and a12 are odd in z.
              Hwork1[index]   =  Aref.A00*Hx_re + Aref.A01*Hy_re - Aref.A02*Hz_re;
              Hwork1[index+1] =  Aref.A00*Hx_im + Aref.A01*Hy_im - Aref.A02*Hz_im;
              Hwork1[index+2] =  Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
              Hwork1[index+3] =  Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
              Hwork1[index+4] = -Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;
              Hwork1[index+5] = -Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
            }
            if(adimy<=j2 && j2<cdimy) {
              OXS_FFT_REAL_TYPE Hx_re = Hwork2[index];
              OXS_FFT_REAL_TYPE Hx_im = Hwork2[index+1];
              OXS_FFT_REAL_TYPE Hy_re = Hwork2[index+2];
              OXS_FFT_REAL_TYPE Hy_im = Hwork2[index+3];
              OXS_FFT_REAL_TYPE Hz_re = Hwork2[index+4];
              OXS_FFT_REAL_TYPE Hz_im = Hwork2[index+5];

              // Flip signs on a01 and a02 as compared to the k>=0, j>=0 case
              // because a01 is odd in y and even in z,
              //     and a02 is odd in z and even in y.
              // No change to a12 because it is odd in both y and z.
              Hwork2[index]   =  Aref.A00*Hx_re - Aref.A01*Hy_re - Aref.A02*Hz_re;
              Hwork2[index+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im - Aref.A02*Hz_im;
              Hwork2[index+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
              Hwork2[index+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
              Hwork2[index+4] = -Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;
              Hwork2[index+5] = -Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
            }
          }
        }
        // Do inverse z-direction transforms for block
        fftz->InverseFFT(Hwork1);
        if(adimy<=j2 && j2<cdimy) {
          fftz->InverseFFT(Hwork2);
        }

        // Copy data out of Hwork
        for(k=0;k<rdimz;++k) {
          const OC_INDEX windex = k*Hwstride;
          const OC_INDEX h1index = k*kstride + m*ODTV_COMPLEXSIZE*ODTV_VECSIZE
            + jindex;
          memcpy(Hxfrm+h1index,Hwork1+windex,Hcopy_line_size);
        }
        if(adimy<=j2 && j2<cdimy) {
          for(k=0;k<rdimz;++k) {
            const OC_INDEX windex = k*Hwstride;
            const OC_INDEX h2index = k*kstride + m*ODTV_COMPLEXSIZE*ODTV_VECSIZE
              + j2index;
            memcpy(Hxfrm+h2index,Hwork2+windex,Hcopy_line_size);
          }
        }
      }
    }
  }
}

// asdf //////////////////////////////
#if USE_FFT_YZ_CONVOLVE
class _Oxs_DemagFFTyzConvolveThread : public Oxs_ThreadRunObj {
public:
  const Oxs_Demag::A_coefs* A;
  OXS_FFT_REAL_TYPE* carr;

  Oxs_Demag::Oxs_FFTLocker_Info locker_info;
  Oxs_Demag::Oxs_FFTLocker* locker;

  OC_INDEX rdimx,rdimy,rdimz;
  OC_INDEX cdimx,cdimy,cdimz;
  OC_INDEX adimx,adimy,adimz;

  OC_INDEX thread_count;
  OC_INDEX embed_block_size;

  _Oxs_DemagFFTyzConvolveThread()
    : A(0), carr(0), locker(0),
      rdimx(0),rdimy(0),rdimz(0),
      cdimx(0),cdimy(0),cdimz(0),
      adimx(0),adimy(0),adimz(0),
      thread_count(0),
      embed_block_size(0) {}
  void Cmd(int threadnumber, void* data);
};

void _Oxs_DemagFFTyzConvolveThread::Cmd(int thread_number, void* /* data */)
{
  // Thread local storage
  if(!locker) {
    Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info.name);
    if(!foo) {
      // Oxs_FFTLocker object not constructed
      foo = new Oxs_Demag::Oxs_FFTLocker(locker_info);
      local_locker.AddItem(locker_info.name,foo);
    }
    locker = dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo);
    if(!locker) {
      Oxs_ThreadError::SetError(String("Error in"
         "_Oxs_DemagFFTzConvolveThread::Cmd(): locker downcast failed."));
      return;
    }
  }
  Oxs_FFTStrided* const ffty = &(locker->ffty);
  Oxs_FFTStrided* const fftz = &(locker->fftz);

  // Hwork:  Data is copied from carr into and out of this space
  // on each m increment.
  const OC_INDEX Hwjstride = ODTV_VECSIZE*ODTV_COMPLEXSIZE*embed_block_size;
  const OC_INDEX Hwkstride = Hwjstride*cdimy;

  const OC_INDEX cjstride = ODTV_VECSIZE*ODTV_COMPLEXSIZE*cdimx;
  const OC_INDEX ckstride = cjstride*rdimy;

  OXS_FFT_REAL_TYPE* const Hwork = locker->fftyz_Hwork;

#if OC_USE_SSE
  // Check alignment restrictions are met
  assert(reinterpret_cast<OC_UINDEX>(Hwork)%16==0);
  assert(reinterpret_cast<OC_UINDEX>(carr)%16==0);
#endif

  if(locker->A_copy == NULL) {
    size_t asize = static_cast<size_t>(adimx*adimy*adimz);
    locker->A_copy_size = asize;
    locker->A_copy = static_cast<Oxs_Demag::A_coefs*>
      (Oc_AllocThreadLocal(asize*sizeof(Oxs_Demag::A_coefs)));
    memcpy(locker->A_copy,A,asize*sizeof(Oxs_Demag::A_coefs));
  }
  const Oxs_Demag::A_coefs* const Acopy = locker->A_copy;

  // Adjust ffty and fftz to use Hwork.  The ffty transforms are not
  // contiguous, so we have to make a separate ffty call for each
  // k-plane (count: rdimz).  If Hwork is fully filled, then the fftz
  // transforms are contiguous, so they can all be done with a single
  // call.
  ffty->AdjustInputDimensions(rdimy,Hwjstride,
                              ODTV_VECSIZE*embed_block_size);
  fftz->AdjustInputDimensions(rdimz,Hwkstride,
                              ODTV_VECSIZE*embed_block_size*cdimy);

  const OC_INDEX chunk_size = (cdimx+thread_count-1)/thread_count;
  const OC_INDEX istart = chunk_size * thread_number;
  const OC_INDEX istop = (istart+chunk_size<cdimx ? istart+chunk_size : cdimx);

  for(OC_INDEX i1=istart;i1<istop;i1+=embed_block_size) {
    const OC_INDEX i2 = (i1+embed_block_size<istop
                         ? i1+embed_block_size : istop);
    const OC_INDEX ispan = i2 - i1;
    OC_INDEX i,j,k;

    // Copy data from carr into Hwork scratch space.  Do the forward
    // FFT-y on each z-plane as it is loaded.
    ffty->AdjustArrayCount(ODTV_VECSIZE*ispan);
    for(k=0;k<rdimz;++k) {
      for(j=0;j<rdimy;++j) {
        OC_INDEX Hindex = k*Hwkstride + j*Hwjstride;
        OC_INDEX cindex = ODTV_VECSIZE*ODTV_COMPLEXSIZE*i1
          + k*ckstride + j*cjstride;
#if OC_USE_SSE
        // Prime cache for _next_ j
        _mm_prefetch((const char *)(carr+cindex+cjstride),_MM_HINT_T0);
        _mm_prefetch((const char *)(carr+cindex+cjstride+6),_MM_HINT_T0);
#endif
        for(OC_INDEX q=0;q<ODTV_VECSIZE*ODTV_COMPLEXSIZE*ispan;q+=6) {
          // Use fact that ODTV_COMPLEXSIZE*ODTV_COMPLEXSIZE == 6
          Hwork[Hindex + q]     = carr[cindex + q];
          Hwork[Hindex + q + 1] = carr[cindex + q + 1];
          Hwork[Hindex + q + 2] = carr[cindex + q + 2];
          Hwork[Hindex + q + 3] = carr[cindex + q + 3];
          Hwork[Hindex + q + 4] = carr[cindex + q + 4];
          Hwork[Hindex + q + 5] = carr[cindex + q + 5];
        }
      }
      ffty->ForwardFFT(Hwork + k*Hwkstride);
    }

    // Forward FFT-z on Hwork.  If Hwork is filled (i.e., ispan ==
    // embed_block_size) then this takes one call.  Otherwise, we
    // need to loop though j.
    if(ispan == embed_block_size) {
      fftz->AdjustArrayCount(ODTV_VECSIZE*ispan*cdimy);
      fftz->ForwardFFT(Hwork);
    } else {
      fftz->AdjustArrayCount(ODTV_VECSIZE*ispan);
      for(j=0;j<cdimy;++j) {
        fftz->ForwardFFT(Hwork + j*Hwjstride);
      }
    }

    // Do matrix-vector multiply ("convolution") for block
    const OC_INDEX ajstride = adimx;
    const OC_INDEX akstride = ajstride*adimy;
    for(OC_INDEX k1=0;k1<adimz;++k1) {
      for(OC_INDEX j1=0;j1<adimy;++j1) {
        OC_INDEX Aindex = k1*akstride  + j1*ajstride + i1;
        OC_INDEX Hindex = k1*Hwkstride + j1*Hwjstride;
#if OC_USE_SSE
        // Prime cache for _next_ j
        _mm_prefetch((const char*)(Acopy+Aindex+ajstride),_MM_HINT_T0);
        _mm_prefetch((const char*)(Acopy+Aindex+ajstride+6),_MM_HINT_T0);
#endif
        for(i=0;i<ispan;++i) {
          const Oxs_Demag::A_coefs& Aref = Acopy[Aindex+i];
          { // j>=0, k>=0
            const OC_INDEX index = Hindex + (ODTV_COMPLEXSIZE*ODTV_VECSIZE)*i;
            OXS_FFT_REAL_TYPE Hx_re = Hwork[index];
            OXS_FFT_REAL_TYPE Hy_re = Hwork[index+2];
            OXS_FFT_REAL_TYPE Hz_re = Hwork[index+4];
            Hwork[index]   = Aref.A00*Hx_re + Aref.A01*Hy_re + Aref.A02*Hz_re;
            Hwork[index+2] = Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
            Hwork[index+4] = Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;

            OXS_FFT_REAL_TYPE Hx_im = Hwork[index+1];
            OXS_FFT_REAL_TYPE Hy_im = Hwork[index+3];
            OXS_FFT_REAL_TYPE Hz_im = Hwork[index+5];
            Hwork[index+1] = Aref.A00*Hx_im + Aref.A01*Hy_im + Aref.A02*Hz_im;
            Hwork[index+3] = Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
            Hwork[index+5] = Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
          const OC_INDEX j2 = cdimy-j1;
          if(adimy<=j2 && j2<cdimy) {
            // j<0, k>=0
            // Flip signs on a01 and a12 as compared to the j>=0
            // case because a01 and a12 are odd in y.
            const OC_INDEX index = k1*Hwkstride + (cdimy-j1)*Hwjstride
              + ODTV_COMPLEXSIZE*ODTV_VECSIZE*i;
            OXS_FFT_REAL_TYPE Hx_re = Hwork[index];
            OXS_FFT_REAL_TYPE Hy_re = Hwork[index+2];
            OXS_FFT_REAL_TYPE Hz_re = Hwork[index+4];
            Hwork[index]   =  Aref.A00*Hx_re - Aref.A01*Hy_re + Aref.A02*Hz_re;
            Hwork[index+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
            Hwork[index+4] =  Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;
            
            OXS_FFT_REAL_TYPE Hx_im = Hwork[index+1];
            OXS_FFT_REAL_TYPE Hy_im = Hwork[index+3];
            OXS_FFT_REAL_TYPE Hz_im = Hwork[index+5];
            Hwork[index+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im + Aref.A02*Hz_im;
            Hwork[index+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
            Hwork[index+5] =  Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
          const OC_INDEX k2 = cdimz-k1;
          if(adimz<=k2 && k2<cdimz) {
            // j>=0, k<0
            // Flip signs on a02 and a12 as compared to the k>=0, j>=0 case
            // because a02 and a12 are odd in z.
            const OC_INDEX index = (cdimz-k1)*Hwkstride + j1*Hwjstride
              + ODTV_COMPLEXSIZE*ODTV_VECSIZE*i;
            OXS_FFT_REAL_TYPE Hx_re = Hwork[index];
            OXS_FFT_REAL_TYPE Hy_re = Hwork[index+2];
            OXS_FFT_REAL_TYPE Hz_re = Hwork[index+4];
            Hwork[index]   =  Aref.A00*Hx_re + Aref.A01*Hy_re - Aref.A02*Hz_re;
            Hwork[index+2] =  Aref.A01*Hx_re + Aref.A11*Hy_re - Aref.A12*Hz_re;
            Hwork[index+4] = -Aref.A02*Hx_re - Aref.A12*Hy_re + Aref.A22*Hz_re;

            OXS_FFT_REAL_TYPE Hx_im = Hwork[index+1];
            OXS_FFT_REAL_TYPE Hy_im = Hwork[index+3];
            OXS_FFT_REAL_TYPE Hz_im = Hwork[index+5];
            Hwork[index+1] =  Aref.A00*Hx_im + Aref.A01*Hy_im - Aref.A02*Hz_im;
            Hwork[index+3] =  Aref.A01*Hx_im + Aref.A11*Hy_im - Aref.A12*Hz_im;
            Hwork[index+5] = -Aref.A02*Hx_im - Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
          if(adimy<=j2 && j2<cdimy && adimz<=k2 && k2<cdimz) {
            // j<0, k<0
            // Flip signs on a01 and a02 as compared to the k>=0, j>=0 case
            // because a01 is odd in y and even in z,
            //     and a02 is odd in z and even in y.
            // No change to a12 because it is odd in both y and z.
            const OC_INDEX index = (cdimz-k1)*Hwkstride + (cdimy-j1)*Hwjstride
              + ODTV_COMPLEXSIZE*ODTV_VECSIZE*i;
            OXS_FFT_REAL_TYPE Hx_re = Hwork[index];
            OXS_FFT_REAL_TYPE Hy_re = Hwork[index+2];
            OXS_FFT_REAL_TYPE Hz_re = Hwork[index+4];
            Hwork[index]   =  Aref.A00*Hx_re - Aref.A01*Hy_re - Aref.A02*Hz_re;
            Hwork[index+2] = -Aref.A01*Hx_re + Aref.A11*Hy_re + Aref.A12*Hz_re;
            Hwork[index+4] = -Aref.A02*Hx_re + Aref.A12*Hy_re + Aref.A22*Hz_re;

            OXS_FFT_REAL_TYPE Hx_im = Hwork[index+1];
            OXS_FFT_REAL_TYPE Hy_im = Hwork[index+3];
            OXS_FFT_REAL_TYPE Hz_im = Hwork[index+5];
            Hwork[index+1] =  Aref.A00*Hx_im - Aref.A01*Hy_im - Aref.A02*Hz_im;
            Hwork[index+3] = -Aref.A01*Hx_im + Aref.A11*Hy_im + Aref.A12*Hz_im;
            Hwork[index+5] = -Aref.A02*Hx_im + Aref.A12*Hy_im + Aref.A22*Hz_im;
          }
        } // for(i)
      } // for(j1)
    } // for(k1)

    // Inverse FFT-z on Hwork
    if(ispan == embed_block_size) {
      fftz->InverseFFT(Hwork);
    } else {
      for(j=0;j<cdimy;++j) {
        fftz->InverseFFT(Hwork + j*Hwjstride);
      }
    }

    // Inverse FFT-y on Hwork, and copy back to carr
    for(k=0;k<rdimz;++k) {
      ffty->InverseFFT(Hwork + k*Hwkstride);
      for(j=0;j<rdimy;++j) {
        OC_INDEX cindex = ODTV_VECSIZE*ODTV_COMPLEXSIZE*i1
          + k*ckstride + j*cjstride;
        OC_INDEX Hindex = k*Hwkstride + j*Hwjstride;
#if 1 || !OC_USE_SSE
        for(OC_INDEX q=0;q<ODTV_VECSIZE*ODTV_COMPLEXSIZE*ispan;q+=6) {
          // Use fact that ODTV_COMPLEXSIZE*ODTV_COMPLEXSIZE == 6
          carr[cindex + q]     = Hwork[Hindex + q];
          carr[cindex + q + 1] = Hwork[Hindex + q + 1];
          carr[cindex + q + 2] = Hwork[Hindex + q + 2];
          carr[cindex + q + 3] = Hwork[Hindex + q + 3];
          carr[cindex + q + 4] = Hwork[Hindex + q + 4];
          carr[cindex + q + 5] = Hwork[Hindex + q + 5];
        }
#else
        // In practice _mm_stream_pd code below is rather slow.
        // Best guess is that embed_size (and hence ispan) is 1
        // for large cdimy*cdimz, so that each pass here only puts
        // out 6 doubles; since there are 8 doubles to a cache line
        // write combining doesn't happen, so the cache doesn't get
        // bypassed after all.
        for(OC_INDEX q=0;q<ODTV_VECSIZE*ODTV_COMPLEXSIZE*ispan;q+=6) {
          // Use fact that ODTV_COMPLEXSIZE*ODTV_COMPLEXSIZE == 6
          // Use fact that both carr and Hwork are 16-byte aligned
          _mm_stream_pd(carr+cindex+q  ,_mm_load_pd(Hwork+Hindex+q));
          _mm_stream_pd(carr+cindex+q+2,_mm_load_pd(Hwork+Hindex+q+2));
          _mm_stream_pd(carr+cindex+q+4,_mm_load_pd(Hwork+Hindex+q+4));
          /// _mm_stream_pd bypasses cache
        }
#endif
      }
    }

  } // for(i1)
}
#endif // USE_FFT_YZ_CONVOLVE
// asdf //////////////////////////////


void Oxs_Demag::ComputeEnergy
(const Oxs_SimState& state,
 Oxs_ComputeEnergyData& oced
 ) const
{
  // (Re)-initialize mesh coefficient array if mesh has changed.
  if(mesh_id != state.mesh->Id()) {
    mesh_id = 0; // Safety
    FillCoefficientArrays(state.mesh);
    mesh_id = state.mesh->Id();
  }

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  // Fill Mtemp with Ms[]*spin[].  The plan is to eventually
  // roll this step into the forward FFT routine.
  assert(rdimx*rdimy*rdimz == Ms.Size());

  const OC_INDEX rxdim = ODTV_VECSIZE*rdimx;
  const OC_INDEX cxdim = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx;
  const OC_INDEX rxydim = rxdim*rdimy;
  const OC_INDEX cxydim = cxdim*cdimy;

  // Calculate x- and y-axis FFTs of Mtemp.
  OC_INT4m ithread;
  {
    OXS_FFT_REAL_TYPE *Hxfrm=0;
    OC_INDEX Hxfrm_kstride = 0;
    if(!USE_FFT_YZ_CONVOLVE || cdimz<2) {
      Hxfrm = Hxfrm_base.GetArrBase();
      Hxfrm_kstride = cxydim;
    } else {
      // For yz-convolve code.  This has a smaller footprint
      Hxfrm_base_yz.SetSize(2*ODTV_VECSIZE*cdimx*rdimy*rdimz);
      Hxfrm = Hxfrm_base_yz.GetArrBase();
      Hxfrm_kstride = cxdim*rdimy;
    }

#if REPORT_TIME
  fftxforwardtime.Start();
#endif // REPORT_TIME

    vector<_Oxs_DemagFFTxThread> fftx_thread;
    fftx_thread.resize(MaxThreadCount);

    for(ithread=0;ithread<MaxThreadCount;++ithread) {
      fftx_thread[ithread].spin = &spin;
      fftx_thread[ithread].Ms   = &Ms;
      fftx_thread[ithread].rarr = Mtemp;
      fftx_thread[ithread].carr = Hxfrm;
      fftx_thread[ithread].locker_info.Set(rdimx,rdimy,rdimz,
                                           cdimx,cdimy,cdimz,
                                           embed_block_size,
                                           embed_yzblock_size,
                                           MakeLockerName());
      fftx_thread[ithread].spin_xdim  = rdimx;
      fftx_thread[ithread].spin_xydim = rdimx*rdimy;

      fftx_thread[ithread].j_dim = rdimy;
      fftx_thread[ithread].j_rstride = rxdim;
      fftx_thread[ithread].j_cstride = cxdim;

      fftx_thread[ithread].k_rstride = rxydim;
      fftx_thread[ithread].k_cstride = Hxfrm_kstride;

      fftx_thread[ithread].jk_max = rdimy*rdimz;

      fftx_thread[ithread].direction = _Oxs_DemagFFTxThread::FORWARD;
      if(ithread>0) threadtree.Launch(fftx_thread[ithread],0);
    }
    threadtree.LaunchRoot(fftx_thread[0],0);
#if REPORT_TIME
    fftxforwardtime.Stop();
#endif // REPORT_TIME
  }

  if(cdimz<2) {
#if REPORT_TIME
    convtime.Start();
#endif // REPORT_TIME
    {
      OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base.GetArrBase();
      vector<_Oxs_DemagFFTyConvolveThread> ffty_thread;
      ffty_thread.resize(MaxThreadCount);

      _Oxs_DemagFFTyConvolveThread::job_control.Init(cdimx,MaxThreadCount,1);

      for(ithread=0;ithread<MaxThreadCount;++ithread) {
        ffty_thread[ithread].Hxfrm = Hxfrm;
        ffty_thread[ithread].A = A;
        ffty_thread[ithread].locker_info.Set(rdimx,rdimy,rdimz,
                                             cdimx,cdimy,cdimz,
                                             embed_block_size,
                                             embed_yzblock_size,
                                             MakeLockerName());
        ffty_thread[ithread].embed_block_size = embed_block_size;
        ffty_thread[ithread].jstride = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx;
        ffty_thread[ithread].ajstride = adimx;
        ffty_thread[ithread].i_dim = cdimx;
        ffty_thread[ithread].rdimy = rdimy;
        ffty_thread[ithread].adimy = adimy;
        ffty_thread[ithread].cdimy = cdimy;

        if(ithread>0) threadtree.Launch(ffty_thread[ithread],0);
      }
      threadtree.LaunchRoot(ffty_thread[0],0);
    }
#if REPORT_TIME
    convtime.Stop();
#endif // REPORT_TIME

  } else { // cdimz>=2

#if !USE_FFT_YZ_CONVOLVE // qwerty
#if REPORT_TIME
    fftyforwardtime.Start();
#endif // REPORT_TIME
    {
      OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base.GetArrBase();
      vector<_Oxs_DemagFFTyThread> ffty_thread;
      ffty_thread.resize(MaxThreadCount);

      _Oxs_DemagFFTyThread::job_control.Init(cdimx*ODTV_VECSIZE*rdimz,
                                             MaxThreadCount,16);

      for(ithread=0;ithread<MaxThreadCount;++ithread) {
        ffty_thread[ithread].carr = Hxfrm;
        ffty_thread[ithread].locker_info.Set(rdimx,rdimy,rdimz,
                                             cdimx,cdimy,cdimz,
                                             embed_block_size,
                                             embed_yzblock_size,
                                             MakeLockerName());
        ffty_thread[ithread].k_stride = cxydim;
        ffty_thread[ithread].k_dim = rdimz;
        ffty_thread[ithread].i_dim = cdimx*ODTV_VECSIZE;
        ffty_thread[ithread].direction = _Oxs_DemagFFTyThread::FORWARD;
        if(ithread>0) threadtree.Launch(ffty_thread[ithread],0);
      }
      threadtree.LaunchRoot(ffty_thread[0],0);
    }
#if REPORT_TIME
    fftyforwardtime.Stop();
#endif // REPORT_TIME

    // Do z-axis FFTs with embedded "convolution" operations.
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
    {
      const OC_INDEX  jstride = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx;
      const OC_INDEX  kstride = jstride*cdimy;
      const OC_INDEX ajstride = adimx;
      const OC_INDEX akstride = ajstride*adimy;
      OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base.GetArrBase();

      // Multi-thread
      vector<_Oxs_DemagFFTzConvolveThread> fftzconv;
      fftzconv.resize(MaxThreadCount);

      _Oxs_DemagFFTzConvolveThread::job_control.Init(adimy,MaxThreadCount,1);

      for(ithread=0;ithread<MaxThreadCount;++ithread) {
        fftzconv[ithread].Hxfrm = Hxfrm;
        fftzconv[ithread].A = A;
        fftzconv[ithread].locker_info.Set(rdimx,rdimy,rdimz,
                                          cdimx,cdimy,cdimz,
                                          embed_block_size,
                                          embed_yzblock_size,
                                          MakeLockerName());
        fftzconv[ithread].thread_count = MaxThreadCount;

        fftzconv[ithread].cdimx = cdimx;
        fftzconv[ithread].cdimy = cdimy;
        fftzconv[ithread].cdimz = cdimz;
        fftzconv[ithread].adimx = adimx;
        fftzconv[ithread].adimy = adimy;
        fftzconv[ithread].adimz = adimz;
        fftzconv[ithread].rdimz = rdimz;

        fftzconv[ithread].embed_block_size = embed_block_size;
        fftzconv[ithread].jstride = jstride;
        fftzconv[ithread].ajstride = ajstride;
        fftzconv[ithread].kstride = kstride;
        fftzconv[ithread].akstride = akstride;
        if(ithread>0) threadtree.Launch(fftzconv[ithread],0);
      }
      threadtree.LaunchRoot(fftzconv[0],0);
    }
#if REPORT_TIME
    convtime.Stop();
#endif // REPORT_TIME

    // Do inverse y- and x-axis FFTs, to complete transform back into
    // space domain.
#if REPORT_TIME
    fftyinversetime.Start();
#endif // REPORT_TIME
    {
      OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base.GetArrBase();
      vector<_Oxs_DemagFFTyThread> ffty_thread;
      ffty_thread.resize(MaxThreadCount);

      _Oxs_DemagFFTyThread::job_control.Init(cdimx*ODTV_VECSIZE*rdimz,
                                             MaxThreadCount,16);

      for(ithread=0;ithread<MaxThreadCount;++ithread) {
        ffty_thread[ithread].carr = Hxfrm;
        ffty_thread[ithread].locker_info.Set(rdimx,rdimy,rdimz,
                                             cdimx,cdimy,cdimz,
                                             embed_block_size,
                                             embed_yzblock_size,
                                             MakeLockerName());
        ffty_thread[ithread].k_stride = cxydim;
        ffty_thread[ithread].k_dim = rdimz;
        ffty_thread[ithread].i_dim = cdimx*ODTV_VECSIZE;
        ffty_thread[ithread].direction = _Oxs_DemagFFTyThread::INVERSE;
        if(ithread>0) threadtree.Launch(ffty_thread[ithread],0);
      }
      threadtree.LaunchRoot(ffty_thread[0],0);
    }
#if REPORT_TIME
    fftyinversetime.Stop();
#endif // REPORT_TIME

#else // USE_FFT_YZ_CONVOLVE

    // Do y+z-axis FFTs with embedded "convolution" operations.  Embed
    // "convolution" (really matrix-vector multiply A^*M^) inside
    // FFTs.  Previous to this, the full forward x-FFTs are computed.
    // Then, in this stage, do a limited number of y-axis and z-axis
    // forward FFTs, followed by the the convolution for the
    // corresponding elements, and after that the corresponding of
    // inverse y- and z-axis FFTs.  The number of y- and z-axis
    // forward and inverse FFTs to do in each sandwich is controlled
    // by the class member variable embed_block_size.
    //    NB: In this branch, the fftforwardtime and fftinversetime timer
    // variables measure the time for the x-axis transforms only.
    // The convtime timer variable includes not only the "convolution"
    // time, but also the wrapping y- and z-axis FFT times.

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
    {
      OXS_FFT_REAL_TYPE *Hxfrm = Hxfrm_base_yz.GetArrBase();

      // Multi-thread
      vector<_Oxs_DemagFFTyzConvolveThread> fftyzconv;
      fftyzconv.resize(MaxThreadCount);

      for(ithread=0;ithread<MaxThreadCount;++ithread) {
        fftyzconv[ithread].A = A;
        fftyzconv[ithread].carr = Hxfrm;
        fftyzconv[ithread].locker_info.Set(rdimx,rdimy,rdimz,
                                          cdimx,cdimy,cdimz,
                                          embed_block_size,
                                          embed_yzblock_size,
                                          MakeLockerName());

        fftyzconv[ithread].rdimx = rdimx;
        fftyzconv[ithread].rdimy = rdimy;
        fftyzconv[ithread].rdimz = rdimz;

        fftyzconv[ithread].cdimx = cdimx;
        fftyzconv[ithread].cdimy = cdimy;
        fftyzconv[ithread].cdimz = cdimz;

        fftyzconv[ithread].adimx = adimx;
        fftyzconv[ithread].adimy = adimy;
        fftyzconv[ithread].adimz = adimz;

        fftyzconv[ithread].thread_count = MaxThreadCount;

        fftyzconv[ithread].embed_block_size = embed_yzblock_size;
        if(ithread>0) threadtree.Launch(fftyzconv[ithread],0);
      }
      threadtree.LaunchRoot(fftyzconv[0],0);
    }
#if REPORT_TIME
    convtime.Stop();
#endif // REPORT_TIME

#endif // USE_FFT_YZ_CONVOLVE
  }  // cdimz<2
#if REPORT_TIME
  fftxinversetime.Start();
#endif // REPORT_TIME

  {
    OXS_FFT_REAL_TYPE *Hxfrm=0;
    OC_INDEX Hxfrm_kstride = 0;
    if(!USE_FFT_YZ_CONVOLVE || cdimz<2) {
      Hxfrm = Hxfrm_base.GetArrBase();
      Hxfrm_kstride = cxydim; // Doesn't matter?
    } else {
      // For yz-convolve code.  This has a smaller footprint
      Hxfrm = Hxfrm_base_yz.GetArrBase();
      Hxfrm_kstride = cxdim*rdimy;
    }

    vector<_Oxs_DemagiFFTxDotThread> fftx_thread;
    fftx_thread.resize(MaxThreadCount);

    _Oxs_DemagiFFTxDotThread::Init(); // Zeros energy_sum
    for(ithread=0;ithread<MaxThreadCount;++ithread) {
      fftx_thread[ithread].carr = Hxfrm;

      fftx_thread[ithread].spin_ptr = &spin;
      fftx_thread[ithread].Ms_ptr   = &Ms;
      fftx_thread[ithread].oced_ptr = &oced;
      fftx_thread[ithread].locker_info.Set(rdimx,rdimy,rdimz,
                                         cdimx,cdimy,cdimz,
                                         embed_block_size,
                                         embed_yzblock_size,
                                         MakeLockerName());
      fftx_thread[ithread].rdimx = rdimx;
      fftx_thread[ithread].j_dim = rdimy;
      fftx_thread[ithread].j_rstride = rxdim;
      fftx_thread[ithread].j_cstride = cxdim;
      
      fftx_thread[ithread].k_rstride = rxydim;
      fftx_thread[ithread].k_cstride = Hxfrm_kstride;

      fftx_thread[ithread].jk_max = rdimy*rdimz;

      if(ithread>0) threadtree.Launch(fftx_thread[ithread],0);
    }
    threadtree.LaunchRoot(fftx_thread[0],0);

    Nb_Xpfloat tempsum;
    _Oxs_DemagiFFTxDotThread::result_mutex.Lock();
    tempsum = _Oxs_DemagiFFTxDotThread::energy_sum;
    _Oxs_DemagiFFTxDotThread::result_mutex.Unlock();
    oced.energy_sum
      = static_cast<OC_REAL8m>(tempsum.GetValue() * state.mesh->Volume(0));
    /// All cells have same volume in an Oxs_RectangularMesh.
  }

#if REPORT_TIME
  fftxinversetime.Stop();
#endif // REPORT_TIME
}

#endif // OOMMF_THREADS
