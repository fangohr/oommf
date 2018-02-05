/* FILE: demag-threaded.cc            -*-Mode: c++-*-
 *
 * Threaded version of demag.cc (q.v.).
 */

#include "demag.h"  // Includes definition of OOMMF_THREADS macro
#include "oxsarray.h"  // Oxs_3DArray

#ifndef USE_MADVISE
# define USE_MADVISE 0
#endif

#if USE_MADVISE
# include <sys/mman.h>
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
(const Oxs_Demag::Oxs_FFTLocker_Info& info,
 int in_threadnumber)
  : fftx_scratch(0), fftx_scratch_base(0),
    fftx_scratch_size(0), fftx_scratch_base_size(0),
    ffty_Hwork(0), ffty_Hwork_base(0),
    ffty_Hwork_zstride(0), ffty_Hwork_xstride(0),
    ffty_Hwork_size(0), ffty_Hwork_base_size(0),
    fftz_Hwork(0), fftz_Hwork_base(0),
    fftz_Hwork_zstride(0),
    fftz_Hwork_size(0), fftz_Hwork_base_size(0),
    threadnumber(in_threadnumber)
{
  OC_INDEX alignoff;  // Used for aligning data blocks to cache lines

  // Check import data
  assert(info.rdimx>0 && info.rdimy>0 && info.rdimz>0 &&
         info.cdimx>0 && info.cdimy>0 && info.cdimz>0 &&
         info.embed_block_size>0);
         
  // FFT objects
  fftx.SetDimensions(info.rdimx,
                     (info.cdimx==1 ? 1 : 2*(info.cdimx-1)), info.rdimy);
  ffty.SetDimensions(info.rdimy, info.cdimy,
                     ODTV_COMPLEXSIZE*ODTV_VECSIZE*info.cdimx,
                     ODTV_VECSIZE*info.cdimx);
  fftz.SetDimensions(info.rdimz, info.cdimz,
                     ODTV_COMPLEXSIZE*ODTV_VECSIZE*info.cdimx*info.cdimy,
                     ODTV_VECSIZE*info.cdimx*info.cdimy);

  // Workspace for inverse x-FFTs.  This space is sized to allow use by
  // the larger real-to-complex forward FFT, but is used by the inverse
  // complex-to-real FFT.
  fftx_scratch_size = (info.cdimx+1)*ODTV_COMPLEXSIZE*ODTV_VECSIZE;
  /// Add 1 to allow match with SSE2 alignment of spin and Ms arrays.
  fftx_scratch_base_size = fftx_scratch_size*sizeof(OXS_FFT_REAL_TYPE)
    + OC_CACHE_LINESIZE - 1;
  fftx_scratch_base = static_cast<char*>
    (Oc_AllocThreadLocal(fftx_scratch_base_size));
  alignoff = reinterpret_cast<OC_UINDEX>(fftx_scratch_base)
    % OC_CACHE_LINESIZE;
  if(alignoff>0) {
    alignoff = OC_CACHE_LINESIZE - alignoff;
  }
  fftx_scratch
    = reinterpret_cast<OXS_FFT_REAL_TYPE*>(fftx_scratch_base + alignoff);

  // Workspace for yz-FFTs + convolve
  OC_INDEX cache_real_gcd
    = Nb_Gcd(int(sizeof(OXS_FFT_REAL_TYPE)),OC_CACHE_LINESIZE);
  OC_INDEX roundcount = OC_CACHE_LINESIZE / cache_real_gcd;
  /// roundcount is the smallest number of OXS_FFT_REAL_TYPE variables
  /// that fill out an integer count of full cache lines.  If
  /// sizeof(OXS_FFT_REAL_TYPE) divides OC_CACHE_LINESIZE (which is
  /// usually the case unless OXS_FFT_REAL_TYPE is long double on a
  /// 32-bit machine), then roundcount is just
  /// OC_CACHE_LINESIZE/sizeof(OXS_FFT_REAL_TYPE).  For example, if
  /// OC_CACHE_LINESIZE=64 bytes and sizeof(OXS_FFT_REAL_TYPE) is 8,
  /// then roundcount will be 8 (8*8=64 bytes=1 cache line).  But
  /// otherwise, if say sizeof(OXS_FFT_REAL_TYPE) is 10 or 12
  /// (respectively), then roundcount will be 32 (32*10=320 bytes=5
  /// cache lines) or 16 (16*12=192 bytes=3 cache lines).  We may want
  /// to decide in these latter cases if the extra space is justified.

  OC_INDEX cacheroundcount = sizeof(OXS_FFT_REAL_TYPE) / cache_real_gcd;
  /// cacheroundcount is the number of cache lines enclosed in roundcount
  /// OXS_FFT_REAL_TYPEs.

  // Add padding to align z rows to cache lines
  ffty_Hwork_zstride = ODTV_VECSIZE*ODTV_COMPLEXSIZE*info.cdimy;
  if(ffty_Hwork_zstride%roundcount) {
    ffty_Hwork_zstride += roundcount - (ffty_Hwork_zstride%roundcount);
  }

  // Compute xstride, with padding to protect against cache thrash
  // when accessing along x.
  ffty_Hwork_xstride = ffty_Hwork_zstride*info.rdimz;
  OC_INDEX xstride_cache_lines
    = (sizeof(OXS_FFT_REAL_TYPE)*ffty_Hwork_xstride)/OC_CACHE_LINESIZE;
  if(xstride_cache_lines%2==0 && cacheroundcount%2==1) {
    ffty_Hwork_xstride += roundcount;
  }

  ffty_Hwork_size = ffty_Hwork_xstride * info.embed_block_size;
  ffty_Hwork_base_size = ffty_Hwork_size*sizeof(OXS_FFT_REAL_TYPE)
    + OC_CACHE_LINESIZE - 1;
  ffty_Hwork_base = static_cast<char*>
    (Oc_AllocThreadLocal(ffty_Hwork_base_size));
  // Point ffty_Hwork to aligned spot in ffty_Hwork_base
  alignoff = reinterpret_cast<OC_UINDEX>(ffty_Hwork_base) % OC_CACHE_LINESIZE;
  if(alignoff>0) {
    alignoff = OC_CACHE_LINESIZE - alignoff;
  }
  ffty_Hwork
    = reinterpret_cast<OXS_FFT_REAL_TYPE*>(ffty_Hwork_base + alignoff);

  // Buffer for z-axis FFTs.  The buffer is double-sized (the leading
  // "2" in the first line of the fftz_Hwork_zstride assignment) to
  // allow, at each y-pass, z-ffts at both y and cdimy-y offsets.
  // This is adjusted so that a whole number of cache lines fits in
  // each half (lcm(a,b) = a*b/gcd(a,b)).  This guarantees that each
  // row is itself a whole number of cache lines --- which allows
  // AVX-512 instructions to be used, for example.  (Assuming
  // cacheline_mod=8, then fftz_Hwork_zstride = 48 reals = 3+3 64-byte
  // cache lines.)  Some additional padding is added to reduce cache
  // thrashing, which is markedly important for processors that have a
  // L1 data cache associativity 4-way or less.
  fftz_Hwork_zstride = 2*ODTV_VECSIZE*ODTV_COMPLEXSIZE*roundcount;
  fftz_Hwork_zstride /= Nb_Gcd(OC_INDEX(ODTV_VECSIZE*ODTV_COMPLEXSIZE),
                               roundcount);
  OC_INDEX zstride_cache_lines
    = (sizeof(OXS_FFT_REAL_TYPE)*fftz_Hwork_zstride)/OC_CACHE_LINESIZE;
  if(zstride_cache_lines%2==0 && cacheroundcount%2==1) {
    fftz_Hwork_zstride += roundcount;
  }

  fftz_Hwork_size = fftz_Hwork_zstride*info.cdimz;
  fftz_Hwork_base_size = fftz_Hwork_size*sizeof(OXS_FFT_REAL_TYPE)
    + OC_CACHE_LINESIZE - 1;
  fftz_Hwork_base = static_cast<char*>
    (Oc_AllocThreadLocal(fftz_Hwork_base_size));
  // Point fftz_Hwork to aligned spot in fftz_Hwork_base
  OC_INDEX alignoffz = reinterpret_cast<OC_UINDEX>(fftz_Hwork_base)
    % OC_CACHE_LINESIZE;
  if(alignoffz>0) {
    alignoffz = OC_CACHE_LINESIZE - alignoffz;
  }
  fftz_Hwork
    = reinterpret_cast<OXS_FFT_REAL_TYPE*>(fftz_Hwork_base + alignoffz);

#if !defined(NDEBUG)
  for(OC_INDEX izz=0;izz<fftz_Hwork_base_size;++izz) {
    // Fill workspace with NaNs, to detect uninitialized use.
    reinterpret_cast<unsigned char&>(fftz_Hwork_base[izz]) = 255u;
  }
#endif

#if (VERBOSE_DEBUG && !defined(NDEBUG))
  if(threadnumber==0) { // All threads use same sizes; just print one.
    fprintf(stderr,"Thread buffer sizes: "
            "fftx_scratch=%.2f KB, ffty_Hwork=%.2f KB, fftz_Hwork=%.2f KB\n",
            fftx_scratch_base_size/1024.,ffty_Hwork_base_size/1024.,
            fftz_Hwork_base_size/1024.);
    fprintf(stderr,"Thread buffer strides (bytes): "
            "ffty_Hwork_zstride=%ld, ffty_Hwork_xstride=%ld,"
            " fftz_Hwork_zstride=%ld\n",
            long(sizeof(OXS_FFT_REAL_TYPE)*ffty_Hwork_zstride),
            long(sizeof(OXS_FFT_REAL_TYPE)*ffty_Hwork_xstride),
            long(sizeof(OXS_FFT_REAL_TYPE)*fftz_Hwork_zstride));
  }
#endif

#if USE_MADVISE
  // Note: madvise requires address to be page-aligned.
  OC_INDEX workpage = reinterpret_cast<OC_UINDEX>(ffty_Hwork)%OC_PAGESIZE;
  if(workpage>0) workpage = OC_PAGESIZE - workpage;
  madvise(ffty_Hwork+workpage,ffty_Hwork_size-workpage,MADV_RANDOM);
  OC_INDEX workpagez = reinterpret_cast<OC_UINDEX>(fftz_Hwork)%OC_PAGESIZE;
  if(workpagez>0) workpagez = OC_PAGESIZE - workpagez;
  madvise(fftz_Hwork+workpagez,fftz_Hwork_size-workpagez,MADV_RANDOM);
#endif // USE_MADVISE
}

Oxs_Demag::Oxs_FFTLocker::~Oxs_FFTLocker()
{
  if(fftx_scratch_base) Oc_FreeThreadLocal(fftx_scratch_base,
                                           fftx_scratch_base_size);
  if(fftz_Hwork_base)   Oc_FreeThreadLocal(fftz_Hwork_base,
                                           fftz_Hwork_base_size);
  if(ffty_Hwork_base)   Oc_FreeThreadLocal(ffty_Hwork_base,
                                           ffty_Hwork_base_size);
}

////////////////////////////////////////////////////////////////////////
// Oxs_Demag Constructor
Oxs_Demag::Oxs_Demag(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
#if REPORT_TIME
    inittime("init"),fftforwardtime("f-fft"),
    fftxforwardtime("f-fftx"),fftyforwardtime("f-ffty"),
    fftinversetime("i-fft"),fftxinversetime("i-fftx"),
    fftyinversetime("i-ffty"),
    convtime("conv"),dottime("dot"),
#endif // REPORT_TIME
    rdimx(0),rdimy(0),rdimz(0),cdimx(0),cdimy(0),cdimz(0),
    adimx(0),adimy(0),adimz(0),
    xperiodic(0),yperiodic(0),zperiodic(0),
    coarse_grid(0),coarse_block_x(1),coarse_block_y(1),coarse_block_z(1),
    mesh_id(0),energy_density_error_estimate(-1),
    asymptotic_radius(-1),
    MaxThreadCount(Oc_GetMaxThreadCount()),
    embed_block_size(0)
{
  asymptotic_radius = GetRealInitValue("asymptotic_radius",32.0);
  /// Units of (dx*dy*dz)^(1/3) (geometric mean of cell dimensions),
  /// which is the appropriate scale for estimating the numeric error in
  /// the demag analytic formulae evaluation.  See NOTES V, 15-Mar-2011,
  /// pp 185-193.
  ///   Value of -1 disables use of asymptotic approximation on
  /// non-periodic grids.  For periodic grids zero or negative values
  /// for asymptotic_radius are reset to half the width of the
  /// simulation window in the periodic dimenions.  This may be
  /// counterintuitive, so it might be better to disallow or modify the
  /// behavior in the periodic setting.

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

  // Compute on coarse grid?
  if(HasInitValue("coarse_grid")) {
    ThreeVector blockvec = GetThreeVectorInitValue("coarse_grid");
    coarse_block_x = static_cast<int>(blockvec.x);
    coarse_block_y = static_cast<int>(blockvec.y);
    coarse_block_z = static_cast<int>(blockvec.z);
    if(coarse_block_x<1 || coarse_block_y<1 || coarse_block_z<1
       || static_cast<OC_REAL8m>(coarse_block_x) != blockvec.x
       || static_cast<OC_REAL8m>(coarse_block_y) != blockvec.y
       || static_cast<OC_REAL8m>(coarse_block_z) != blockvec.z) {
      throw Oxs_ExtError(this,"Invalid values for coarse_grid entry: "
                         "Each value should be a positive integer.");
    }
    if(coarse_block_x>1 || coarse_block_y>1 || coarse_block_z>1) {
      coarse_grid = 1; // Enable coarse grid
    }
  }

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
  energy_density_error_estimate = -1;
  ReleaseMemory();
  return Oxs_Energy::Init();
}

void Oxs_Demag::ReleaseMemory() const
{ // Conceptually const
  A.Free();
  Hxfrm_base.Free();
  rdimx=rdimy=rdimz=0;
  cdimx=cdimy=cdimz=0;
  adimx=adimy=adimz=0;
}

// Thread for computing Dx2 of f.  This is the first component
// of the D6f computation
class _Oxs_FillCoefficientArraysDx2fThread : public Oxs_ThreadRunObj {
public:
  OC_REALWIDE (*f)(OC_REALWIDE,OC_REALWIDE,OC_REALWIDE);
  Oxs_3DArray<OC_REALWIDE>& arr;
  OC_REALWIDE dx,dy,dz;
  int number_of_threads;

  _Oxs_FillCoefficientArraysDx2fThread
  (OC_REALWIDE (*g)(OC_REALWIDE,OC_REALWIDE,OC_REALWIDE),
   Oxs_3DArray<OC_REALWIDE>& iarr,
   OC_REALWIDE idx,OC_REALWIDE idy,OC_REALWIDE idz,
   int inumber_of_threads)
    : f(g),arr(iarr),
      dx(idx), dy(idy), dz(idz),
      number_of_threads(inumber_of_threads) {}

  void Cmd(int threadnumber, void*);

private:
    // Declare but don't define default constructors and
    // assignment operator.
    _Oxs_FillCoefficientArraysDx2fThread();
    _Oxs_FillCoefficientArraysDx2fThread
       (const _Oxs_FillCoefficientArraysDx2fThread&);
    _Oxs_FillCoefficientArraysDx2fThread&
       operator=(const _Oxs_FillCoefficientArraysDx2fThread&);
};

void _Oxs_FillCoefficientArraysDx2fThread::Cmd(int threadnumber,void*)
{
  // Hand out jobs across the base yz-plane.
  // Select job by threadnumber:
  const OC_INDEX xdim = arr.GetDim1();
  const OC_INDEX ydim = arr.GetDim2();
  const OC_INDEX zdim = arr.GetDim3();

  const OC_INDEX jobsize = ydim*zdim;
  OC_INDEX bitesize = jobsize/number_of_threads;
  OC_INDEX leftover = jobsize - bitesize*number_of_threads;
  const OC_INDEX jobstart = threadnumber*bitesize
    + (threadnumber*leftover)/number_of_threads;
  const OC_INDEX jobstop = (threadnumber+1)*bitesize
    + ((threadnumber+1)*leftover)/number_of_threads;

  const OC_INDEX kstart = jobstart / ydim;
  const OC_INDEX jstart = jobstart - ydim*kstart;
  const OC_INDEX kstop  = jobstop / ydim;
  const OC_INDEX jstop  = jobstop - ydim*kstop;

  for(OC_INDEX k=kstart;k<=kstop;++k) {
    const OC_INDEX ja = (k!=kstart ? 0 : jstart);
    const OC_INDEX jb = (k!=kstop  ? ydim : jstop);
    for(OC_INDEX j=ja;j<jb;++j) {
      OC_REALWIDE a0 = f(0.0,(j-1)*dy,(k-1)*dz);
      OC_REALWIDE b0 = a0 - f(-1*dx,(j-1)*dy,(k-1)*dz);
      for(OC_INDEX i=0;i<xdim;++i) {
        OC_REALWIDE a1 = f((i+1)*dx,(j-1)*dy,(k-1)*dz);
        OC_REALWIDE b1 = a1 - a0;  a0 = a1;
        arr(i,j,k) = b1 - b0; b0 = b1;
      }
    }
  }
}

// Thread for computing Dy2z2 of f.  This is the second and third
// components of the D6f computation.
class _Oxs_FillCoefficientArraysDy2z2Thread : public Oxs_ThreadRunObj {
public:
  Oxs_3DArray<OC_REALWIDE>& arr;
  int number_of_threads;

  _Oxs_FillCoefficientArraysDy2z2Thread
  (Oxs_3DArray<OC_REALWIDE>& iarr,int inumber_of_threads)
    : arr(iarr),
      number_of_threads(inumber_of_threads) {}

  void Cmd(int threadnumber, void*);

private:
    // Declare but don't define default constructors and
    // assignment operator.
    _Oxs_FillCoefficientArraysDy2z2Thread();
    _Oxs_FillCoefficientArraysDy2z2Thread
       (const _Oxs_FillCoefficientArraysDy2z2Thread&);
    _Oxs_FillCoefficientArraysDy2z2Thread&
       operator=(const _Oxs_FillCoefficientArraysDy2z2Thread&);
};

void _Oxs_FillCoefficientArraysDy2z2Thread::Cmd(int threadnumber,void*)
{
  const OC_INDEX xdim = arr.GetDim1();
  const OC_INDEX ydim = arr.GetDim2()-2;
  const OC_INDEX zdim = arr.GetDim3()-2;

  // Select job by threadnumber:
  OC_INDEX bitesize = xdim/number_of_threads;
  OC_INDEX leftover = xdim - bitesize*number_of_threads;
  const OC_INDEX istart = threadnumber*bitesize
    + (threadnumber*leftover)/number_of_threads;
  const OC_INDEX istop = (threadnumber+1)*bitesize
    + ((threadnumber+1)*leftover)/number_of_threads;

  // Compute D2y.  Block a little bit on j.
  const OC_INDEX jstop = ydim - ydim%4;
  for(OC_INDEX k=0;k<zdim+2;++k) {
    for(OC_INDEX j=0;j<jstop;j+=4) {
      for(OC_INDEX i=istart;i<istop;++i) {
        arr(i,j,k)  =  arr(i,j+1,k) - arr(i,j,k);
        arr(i,j,k) += (arr(i,j+1,k) - arr(i,j+2,k));
        arr(i,j+1,k)  =  arr(i,j+2,k) - arr(i,j+1,k);
        arr(i,j+1,k) += (arr(i,j+2,k) - arr(i,j+3,k));
        arr(i,j+2,k)  =  arr(i,j+3,k) - arr(i,j+2,k);
        arr(i,j+2,k) += (arr(i,j+3,k) - arr(i,j+4,k));
        arr(i,j+3,k)  =  arr(i,j+4,k) - arr(i,j+3,k);
        arr(i,j+3,k) += (arr(i,j+4,k) - arr(i,j+5,k));
      }
    }
    for(OC_INDEX j=jstop;j<ydim;++j) {
      for(OC_INDEX i=istart;i<istop;++i) {
        arr(i,j,k) = arr(i,j+1,k) - arr(i,j,k);
        arr(i,j,k) += (arr(i,j+1,k) - arr(i,j+2,k));
      }
    }
  }
  // Compute D2z.  Block a little bit on k.
  const OC_INDEX kstop = zdim - zdim%4;
  for(OC_INDEX j=0;j<ydim;++j) {
    for(OC_INDEX k=0;k<kstop;k+=4) {
      for(OC_INDEX i=istart;i<istop;++i) {
        arr(i,j,k)  =  arr(i,j,k+1) - arr(i,j,k);
        arr(i,j,k) += (arr(i,j,k+1) - arr(i,j,k+2));
        arr(i,j,k+1)  =  arr(i,j,k+2) - arr(i,j,k+1);
        arr(i,j,k+1) += (arr(i,j,k+2) - arr(i,j,k+3));
        arr(i,j,k+2)  =  arr(i,j,k+3) - arr(i,j,k+2);
        arr(i,j,k+2) += (arr(i,j,k+3) - arr(i,j,k+4));
        arr(i,j,k+3)  =  arr(i,j,k+4) - arr(i,j,k+3);
        arr(i,j,k+3) += (arr(i,j,k+4) - arr(i,j,k+5));
      }
    }
    for(OC_INDEX k=kstop;k<zdim;++k) {
      for(OC_INDEX i=istart;i<istop;++i) {
        arr(i,j,k) = arr(i,j,k+1) - arr(i,j,k);
        arr(i,j,k) += (arr(i,j,k+1) - arr(i,j,k+2));
      }
    }
  }
}

// Given a function f, and an array arr pre-sized to dimensions
// xdim, ydim+2, zdim+2, computes D6[f] = D2z[D2y[D2x[f]]] across
// the range [0,xdim-1] x [0,ydim-1] x [0,zdim-1].  The results are
// stored in arr.  The outer planes j=ydim,ydim+1 and k=zdim,zdim+1
// are used as workspace.
void ComputeD6f(OC_REALWIDE (*f)(OC_REALWIDE,OC_REALWIDE,OC_REALWIDE),
                Oxs_3DArray<OC_REALWIDE>& arr,
                Oxs_ThreadTree& threadtree,
                OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz,
                int maxthreadcount)
{
  // Compute f values and Dx2
  _Oxs_FillCoefficientArraysDx2fThread dx2f(f,arr,dx,dy,dz,maxthreadcount);
  threadtree.LaunchTree(dx2f,0);

  // Compute Dy2 and Dz2
#if 1 // Multi-thread
  _Oxs_FillCoefficientArraysDy2z2Thread dy2z2(arr,maxthreadcount);
  threadtree.LaunchTree(dy2z2,0);
#else // Single-thread
  _Oxs_FillCoefficientArraysDy2z2Thread dy2z2(arr,1);
  dy2z2.Cmd(0,0);
#endif
}


class _Oxs_FillCoefficientArraysAsympThread : public Oxs_ThreadRunObj {
public:
  Oxs_StripedArray<Oxs_Demag::A_coefs>& A;
  const OC_INDEX rdimx;
  const OC_INDEX rdimy;
  const OC_INDEX rdimz;
  const OC_INDEX adimx;
  const OC_INDEX adimy;
  const OC_INDEX adimz;
  const int number_of_threads;
  const OC_REAL8m dx;
  const OC_REAL8m dy;
  const OC_REAL8m dz;
  const OXS_DEMAG_REAL_ASYMP scaled_arad_sq;
  const OXS_FFT_REAL_TYPE fft_scaling;

  _Oxs_FillCoefficientArraysAsympThread
  (Oxs_StripedArray<Oxs_Demag::A_coefs>& iA,
   OC_INDEX irdimx,OC_INDEX irdimy,OC_INDEX irdimz,
   OC_INDEX iadimx,OC_INDEX iadimy,OC_INDEX iadimz,
   int inumber_of_threads,
   OC_REAL8m idx,OC_REAL8m idy,OC_REAL8m idz,
   OXS_DEMAG_REAL_ASYMP iscaled_arad_sq,
   OXS_FFT_REAL_TYPE ifft_scaling)
    : A(iA),
      rdimx(irdimx), rdimy(irdimy), rdimz(irdimz),
      adimx(iadimx), adimy(iadimy), adimz(iadimz),
      number_of_threads(inumber_of_threads),
      dx(idx), dy(idy), dz(idz),
      scaled_arad_sq(iscaled_arad_sq),
      fft_scaling(ifft_scaling) {}

  void Cmd(int threadnumber, void*);

private:
    // Declare but don't define default constructors and
    // assignment operator.
    _Oxs_FillCoefficientArraysAsympThread();
    _Oxs_FillCoefficientArraysAsympThread
       (const _Oxs_FillCoefficientArraysAsympThread&);
    _Oxs_FillCoefficientArraysAsympThread&
       operator=(const _Oxs_FillCoefficientArraysAsympThread&);
};

void _Oxs_FillCoefficientArraysAsympThread::Cmd(int threadnumber,
                                                void* /* data */)
{
  Oxs_DemagNxxAsymptotic ANxx(dx,dy,dz);
  Oxs_DemagNxyAsymptotic ANxy(dx,dy,dz);
  Oxs_DemagNxzAsymptotic ANxz(dx,dy,dz);
  Oxs_DemagNyyAsymptotic ANyy(dx,dy,dz);
  Oxs_DemagNyzAsymptotic ANyz(dx,dy,dz);
  Oxs_DemagNzzAsymptotic ANzz(dx,dy,dz);

  OXS_DEMAG_REAL_ASYMP ytmp
    = static_cast<OXS_DEMAG_REAL_ASYMP>(rdimy)*dy;
  const OXS_DEMAG_REAL_ASYMP ytestsq = ytmp*ytmp;

  const OC_INDEX astridez = adimy;
  const OC_INDEX astridex = adimz*astridez;

  // The i range for jobs runs across 0 <= i < rdimx, so hand out jobs
  // across this range.  Note that the memory is sliced to threads
  // across the range 0 <= i < adimx, and typically adimx > rdimx, so
  // the simple assignment done here is not efficient wrt memory
  // accessing.  However, this routine is called only once per run,
  // during initialization, so it probably isn't worth spending the time
  // to optimize it --- moreover, this code may be compute rather than
  // memory bandwidth bound.

  // Select job by threadnumber:
  OC_INDEX bitesize = rdimx/number_of_threads;
  OC_INDEX leftover = rdimx - bitesize*number_of_threads;
  const OC_INDEX istart = threadnumber*bitesize
    + (threadnumber*leftover)/number_of_threads;
  const OC_INDEX istop = (threadnumber+1)*bitesize
    + ((threadnumber+1)*leftover)/number_of_threads;

  for(OC_INDEX i=istart;i<istop;++i) {
    OC_INDEX iindex = i*astridex;
    OXS_DEMAG_REAL_ASYMP x = dx*i;
    OXS_DEMAG_REAL_ASYMP xsq = x*x;
    for(OC_INDEX k=0;k<rdimz;++k) {
      OC_INDEX ikindex = iindex + k*astridez;
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      OXS_DEMAG_REAL_ASYMP zsq = z*z;

      OC_INDEX jstart = 0;
      OXS_DEMAG_REAL_ASYMP test = scaled_arad_sq-xsq-zsq;
      if(test>0.0) {
        if(test>ytestsq) {
          jstart = rdimy+1;  // No asymptotic
        } else {
          jstart = static_cast<OC_INDEX>(Oc_Ceil(Oc_Sqrt(test)/dy));
        }
      }
      for(OC_INDEX j=jstart;j<rdimy;++j) {
        OC_INDEX index = j+ikindex;
        OXS_DEMAG_REAL_ASYMP y = dy*j;
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

class _Oxs_FillCoefficientArraysPBCxThread : public Oxs_ThreadRunObj {
public:
  Oxs_StripedArray<Oxs_Demag::A_coefs>& A;
  const OC_INDEX rdimx;
  const OC_INDEX rdimy;
  const OC_INDEX rdimz;
  const OC_INDEX astridex;
  const OC_INDEX astridez;
  const int number_of_threads;
  const OC_REAL8m dx;
  const OC_REAL8m dy;
  const OC_REAL8m dz;
  const OXS_DEMAG_REAL_ASYMP scaled_arad;
  const OXS_FFT_REAL_TYPE fft_scaling;

  _Oxs_FillCoefficientArraysPBCxThread
  (Oxs_StripedArray<Oxs_Demag::A_coefs>& iA,
   OC_INDEX irdimx,OC_INDEX irdimy,OC_INDEX irdimz,
   OC_INDEX iastridex,OC_INDEX iastridez,
   int inumber_of_threads,
   OC_REAL8m idx,OC_REAL8m idy,OC_REAL8m idz,
   OXS_DEMAG_REAL_ASYMP iscaled_arad,
   OXS_FFT_REAL_TYPE ifft_scaling)
    : A(iA),
      rdimx(irdimx), rdimy(irdimy), rdimz(irdimz),
      astridex(iastridex), astridez(iastridez),
      number_of_threads(inumber_of_threads),
      dx(idx), dy(idy), dz(idz),
      scaled_arad(iscaled_arad),
      fft_scaling(ifft_scaling) {}

  void Cmd(int threadnumber, void*);
private:
    // Declare but don't define default constructors and
    // assignment operator.
    _Oxs_FillCoefficientArraysPBCxThread();
    _Oxs_FillCoefficientArraysPBCxThread
       (const _Oxs_FillCoefficientArraysPBCxThread&);
    _Oxs_FillCoefficientArraysPBCxThread&
       operator=(const _Oxs_FillCoefficientArraysPBCxThread&);
};

void _Oxs_FillCoefficientArraysPBCxThread::Cmd(int threadnumber,
                                               void* /* data */)
{
  Oxs_DemagPeriodicX pbctensor(dx,dy,dz,rdimx*dx,scaled_arad);

  // The i range for jobs runs across 0 <= i <= rdimx/2, so hand out
  // jobs across this range.  Note that the memory is sliced to threads
  // across the range 0 <= i < adimx, and typically adimx > rdimx, so
  // the simple assignment done here is not efficient wrt memory
  // accessing.  However, this routine is called only once per run,
  // during initialization, so it probably isn't worth spending the time
  // to optimize it --- moreover, this code may be compute rather than
  // memory bandwidth bound.

  // Select job by threadnumber:
  const OC_INDEX ibound = (rdimx/2 + 1);
  OC_INDEX bitesize = ibound/number_of_threads;
  OC_INDEX leftover = ibound - bitesize*number_of_threads;
  const OC_INDEX istart = threadnumber*bitesize
    + (threadnumber*leftover)/number_of_threads;
  const OC_INDEX istop = (threadnumber+1)*bitesize
    + ((threadnumber+1)*leftover)/number_of_threads;

  for(OC_INDEX i=istart;i<istop;++i) {
    OXS_DEMAG_REAL_ASYMP x = dx*i;
    for(OC_INDEX k=0;k<rdimz;++k) {
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(OC_INDEX j=0;j<rdimy;++j) {
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz, Nyy, Nyz, Nzz;
        pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
        pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
        Oxs_Demag::A_coefs Atmp(fft_scaling*Nxx,fft_scaling*Nxy,
                                fft_scaling*Nxz,fft_scaling*Nyy,
                                fft_scaling*Nyz,fft_scaling*Nzz);
        A[i*astridex + k*astridez + j] = Atmp;
        if(0<i && 2*i<rdimx) {
          // Interior point.  Reflect results from left half to
          // right half.  Note that wrt x, Nxy and Nxz are odd,
          // the other terms are even.
          Atmp.A01 *= -1.0;
          Atmp.A02 *= -1.0;
          A[(rdimx-i)*astridex + k*astridez + j] = Atmp;
        }
      }
    }
  }
}

class _Oxs_FillCoefficientArraysPBCyThread : public Oxs_ThreadRunObj {
public:
  Oxs_StripedArray<Oxs_Demag::A_coefs>& A;
  const OC_INDEX rdimx;
  const OC_INDEX rdimy;
  const OC_INDEX rdimz;
  const OC_INDEX astridex;
  const OC_INDEX astridez;
  const int number_of_threads;
  const OC_REAL8m dx;
  const OC_REAL8m dy;
  const OC_REAL8m dz;
  const OXS_DEMAG_REAL_ASYMP scaled_arad;
  const OXS_FFT_REAL_TYPE fft_scaling;

  _Oxs_FillCoefficientArraysPBCyThread
  (Oxs_StripedArray<Oxs_Demag::A_coefs>& iA,
   OC_INDEX irdimx,OC_INDEX irdimy,OC_INDEX irdimz,
   OC_INDEX iastridex,OC_INDEX iastridez,
   int inumber_of_threads,
   OC_REAL8m idx,OC_REAL8m idy,OC_REAL8m idz,
   OXS_DEMAG_REAL_ASYMP iscaled_arad,
   OXS_FFT_REAL_TYPE ifft_scaling)
    : A(iA),
      rdimx(irdimx), rdimy(irdimy), rdimz(irdimz),
      astridex(iastridex), astridez(iastridez),
      number_of_threads(inumber_of_threads),
      dx(idx), dy(idy), dz(idz),
      scaled_arad(iscaled_arad),
      fft_scaling(ifft_scaling) {}

  void Cmd(int threadnumber, void*);

private:
    // Declare but don't define default constructors and
    // assignment operator.
    _Oxs_FillCoefficientArraysPBCyThread();
    _Oxs_FillCoefficientArraysPBCyThread
       (const _Oxs_FillCoefficientArraysPBCyThread&);
    _Oxs_FillCoefficientArraysPBCyThread&
       operator=(const _Oxs_FillCoefficientArraysPBCyThread&);
};

void _Oxs_FillCoefficientArraysPBCyThread::Cmd(int threadnumber,
                                               void* /* data */)
{
  Oxs_DemagPeriodicY pbctensor(dx,dy,dz,rdimy*dy,scaled_arad);

  // Select job by threadnumber:
  OC_INDEX bitesize = rdimx/number_of_threads;
  OC_INDEX leftover = rdimx - bitesize*number_of_threads;
  const OC_INDEX istart = threadnumber*bitesize
    + (threadnumber*leftover)/number_of_threads;
  const OC_INDEX istop = (threadnumber+1)*bitesize
    + ((threadnumber+1)*leftover)/number_of_threads;

  for(OC_INDEX i=istart;i<istop;++i) {
    OXS_DEMAG_REAL_ASYMP x = dx*i;
    for(OC_INDEX k=0;k<rdimz;++k) {
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      OC_INDEX ikoff = i*astridex + k*astridez;
      for(OC_INDEX j=0;j<=(rdimy/2);++j) {
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz, Nyy, Nyz, Nzz;
        pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
        pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
        Oxs_Demag::A_coefs Atmp(fft_scaling*Nxx,fft_scaling*Nxy,
                                fft_scaling*Nxz,fft_scaling*Nyy,
                                fft_scaling*Nyz,fft_scaling*Nzz);
        A[ikoff + j] = Atmp;
        if(0<j && 2*j<rdimy) {
          // Interior point.  Reflect results from lower half to
          // upper half.  Note that wrt y, Nxy and Nyz are odd,
          // the other terms are even.
          Atmp.A01 *= -1.0;
          Atmp.A12 *= -1.0;
          A[ikoff + (rdimy-j)] = Atmp;
        }
      }
    }
  }
}


class _Oxs_FillCoefficientArraysPBCzThread : public Oxs_ThreadRunObj {
public:
  Oxs_StripedArray<Oxs_Demag::A_coefs>& A;
  const OC_INDEX rdimx;
  const OC_INDEX rdimy;
  const OC_INDEX rdimz;
  const OC_INDEX astridex;
  const OC_INDEX astridez;
  const int number_of_threads;
  const OC_REAL8m dx;
  const OC_REAL8m dy;
  const OC_REAL8m dz;
  const OXS_DEMAG_REAL_ASYMP scaled_arad;
  const OXS_FFT_REAL_TYPE fft_scaling;

  _Oxs_FillCoefficientArraysPBCzThread
  (Oxs_StripedArray<Oxs_Demag::A_coefs>& iA,
   OC_INDEX irdimx,OC_INDEX irdimy,OC_INDEX irdimz,
   OC_INDEX iastridex,OC_INDEX iastridez,
   int inumber_of_threads,
   OC_REAL8m idx,OC_REAL8m idy,OC_REAL8m idz,
   OXS_DEMAG_REAL_ASYMP iscaled_arad,
   OXS_FFT_REAL_TYPE ifft_scaling)
    : A(iA),
      rdimx(irdimx), rdimy(irdimy), rdimz(irdimz),
      astridex(iastridex), astridez(iastridez),
      number_of_threads(inumber_of_threads),
      dx(idx), dy(idy), dz(idz),
      scaled_arad(iscaled_arad),
      fft_scaling(ifft_scaling) {}

  void Cmd(int threadnumber, void*);

private:
    // Declare but don't define default constructors and
    // assignment operator.
    _Oxs_FillCoefficientArraysPBCzThread();
    _Oxs_FillCoefficientArraysPBCzThread
       (const _Oxs_FillCoefficientArraysPBCzThread&);
    _Oxs_FillCoefficientArraysPBCzThread&
       operator=(const _Oxs_FillCoefficientArraysPBCzThread&);
};

void _Oxs_FillCoefficientArraysPBCzThread::Cmd(int threadnumber,
                                               void* /* data */)
{
  Oxs_DemagPeriodicZ pbctensor(dx,dy,dz,rdimz*dz,scaled_arad);

  // Select job by threadnumber:
  OC_INDEX bitesize = rdimx/number_of_threads;
  OC_INDEX leftover = rdimx - bitesize*number_of_threads;
  const OC_INDEX istart = threadnumber*bitesize
    + (threadnumber*leftover)/number_of_threads;
  const OC_INDEX istop = (threadnumber+1)*bitesize
    + ((threadnumber+1)*leftover)/number_of_threads;

  for(OC_INDEX i=istart;i<istop;++i) {
    OXS_DEMAG_REAL_ASYMP x = dx*i;
    for(OC_INDEX k=0;k<=(rdimz/2);++k) {
      OXS_DEMAG_REAL_ASYMP z = dz*k;
      for(OC_INDEX j=0;j<rdimy;++j) {
        OXS_DEMAG_REAL_ASYMP y = dy*j;
        OXS_DEMAG_REAL_ASYMP Nxx, Nxy, Nxz, Nyy, Nyz, Nzz;
        pbctensor.NxxNxyNxz(x,y,z,Nxx,Nxy,Nxz);
        pbctensor.NyyNyzNzz(x,y,z,Nyy,Nyz,Nzz);
        Oxs_Demag::A_coefs Atmp(fft_scaling*Nxx,fft_scaling*Nxy,
                                fft_scaling*Nxz,fft_scaling*Nyy,
                                fft_scaling*Nyz,fft_scaling*Nzz);
        A[i*astridex + k*astridez + j] = Atmp;
        if(0<k && 2*k<rdimz) {
          // Interior point.  Reflect results from bottom half to
          // top half.  Note that wrt z, Nxz and Nyz are odd, the
          // other terms are even.
          Atmp.A02 *= -1.0;
          Atmp.A12 *= -1.0;
          A[i*astridex + (rdimz-k)*astridez + j] = Atmp;
        }
      }
    }
  }
}


class _Oxs_FillCoefficientArraysFFTxThread : public Oxs_ThreadRunObj {
public:
  Oxs_StripedArray<Oxs_Demag::A_coefs>& A;
  const OC_INDEX rdimx;
  const OC_INDEX rdimy;
  const OC_INDEX rdimz;
  const OC_INDEX adimx;
  const OC_INDEX adimy;
  const OC_INDEX adimz;
  const OC_INDEX ldimx;
  const int number_of_threads;

  _Oxs_FillCoefficientArraysFFTxThread
  (Oxs_StripedArray<Oxs_Demag::A_coefs>& iA,
   OC_INDEX irdimx,OC_INDEX irdimy,OC_INDEX irdimz,
   OC_INDEX iadimx,OC_INDEX iadimy,OC_INDEX iadimz,
   OC_INDEX ildimx,
   int inumber_of_threads)
    : A(iA),
      rdimx(irdimx), rdimy(irdimy), rdimz(irdimz),
      adimx(iadimx), adimy(iadimy), adimz(iadimz),
      ldimx(ildimx),
      number_of_threads(inumber_of_threads) {}

  void Cmd(int threadnumber, void*);

private:
    // Declare but don't define default constructors and
    // assignment operator.
    _Oxs_FillCoefficientArraysFFTxThread();
    _Oxs_FillCoefficientArraysFFTxThread
       (const _Oxs_FillCoefficientArraysFFTxThread&);
    _Oxs_FillCoefficientArraysFFTxThread&
       operator=(const _Oxs_FillCoefficientArraysFFTxThread&);
};

void _Oxs_FillCoefficientArraysFFTxThread::Cmd(int threadnumber,
                                               void* /* data */)
{
  // Compute FFTs in x-direction
  Oxs_FFT1DThreeVector workfft;
  workfft.SetDimensions(rdimx,ldimx,1);
  const OC_INDEX sourcedim = ODTV_VECSIZE*rdimx;
  vector<OXS_FFT_REAL_TYPE> sourcebuf(2*sourcedim);
  const OC_INDEX targetdim = 2*ODTV_VECSIZE*adimx;
  vector<OXS_FFT_REAL_TYPE> targetbuf(2*targetdim);

  const OC_INDEX astridez = adimy;
  const OC_INDEX astridex = adimz*astridez;

  // Hand out jobs across the base yz-plane.  At present tensor A is
  // tightly packed, but don't assume that in case astridez is padded
  // in the future so that each y row starts on a cache boundary (to
  // play nice with AVX-512).
  // Select job by threadnumber:
  const OC_INDEX jobsize = rdimy*rdimz;
  OC_INDEX bitesize = jobsize/number_of_threads;
  OC_INDEX leftover = jobsize - bitesize*number_of_threads;
  const OC_INDEX jobstart = threadnumber*bitesize
    + (threadnumber*leftover)/number_of_threads;
  const OC_INDEX jobstop = (threadnumber+1)*bitesize
    + ((threadnumber+1)*leftover)/number_of_threads;

  const OC_INDEX kstart = jobstart / rdimy;
  const OC_INDEX jstart = jobstart - rdimy*kstart;
  const OC_INDEX kstop  = jobstop / rdimy;
  const OC_INDEX jstop  = jobstop - rdimy*kstop;

  for(OC_INDEX k=kstart;k<=kstop;++k) {
    const OC_INDEX ja = (k!=kstart ? 0 : jstart);
    const OC_INDEX jb = (k!=kstop  ? adimy : jstop);
    for(OC_INDEX j=ja;j<jb;++j) {
      const OC_INDEX aoff = k*astridez +j;
      // Fill sourcebuf
      for(OC_INDEX i=0;i<rdimx;++i) {
        sourcebuf[ODTV_VECSIZE*i]   = A[aoff + i*astridex].A00;
        sourcebuf[ODTV_VECSIZE*i+1] = A[aoff + i*astridex].A11;
        sourcebuf[ODTV_VECSIZE*i+2] = A[aoff + i*astridex].A22;
        sourcebuf[sourcedim+ODTV_VECSIZE*i]   = A[aoff + i*astridex].A01;
        sourcebuf[sourcedim+ODTV_VECSIZE*i+1] = A[aoff + i*astridex].A02;
        sourcebuf[sourcedim+ODTV_VECSIZE*i+2] = A[aoff + i*astridex].A12;
      }
      // A01 and A02 are odd wrt x, all other components are even.
      sourcebuf[0] *= 0.5; sourcebuf[1] *= 0.5; sourcebuf[2] *= 0.5;
      sourcebuf[sourcedim] =  sourcebuf[sourcedim+1] = 0.0;
      sourcebuf[sourcedim+2] *= 0.5;

      // Forward FFTs
      workfft.ForwardRealToComplexFFT(&(sourcebuf[0]),&(targetbuf[0]));
      workfft.ForwardRealToComplexFFT(&(sourcebuf[sourcedim]),
                                      &(targetbuf[targetdim]));

      // Copy back from targetbuf.  A00, A11, A22 and A12 are even wrt
      // x, so for those terms take the real component of the
      // transformed data.  A01 and A02 are odd wrt x, so take the
      // imaginary component for those two terms.
      for(OC_INDEX i=0;i<adimx;++i) {
        A[aoff + i*astridex].A00 = targetbuf[2*ODTV_VECSIZE*i];
        A[aoff + i*astridex].A11 = targetbuf[2*ODTV_VECSIZE*i+2];
        A[aoff + i*astridex].A22 = targetbuf[2*ODTV_VECSIZE*i+4];
        A[aoff + i*astridex].A01 = targetbuf[targetdim + 2*ODTV_VECSIZE*i+1];
        A[aoff + i*astridex].A02 = targetbuf[targetdim + 2*ODTV_VECSIZE*i+3];
        A[aoff + i*astridex].A12 = targetbuf[targetdim + 2*ODTV_VECSIZE*i+4];
      }
    }
  }
}

class _Oxs_FillCoefficientArraysFFTyzThread : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<Oxs_Demag::A_coefs> job_basket;

  Oxs_StripedArray<Oxs_Demag::A_coefs>& A;
  const OC_INDEX rdimy;
  const OC_INDEX rdimz;
  const OC_INDEX adimy;
  const OC_INDEX adimz;
  const OC_INDEX ldimy;
  const OC_INDEX ldimz;

  _Oxs_FillCoefficientArraysFFTyzThread
  (Oxs_StripedArray<Oxs_Demag::A_coefs>& iA,
   OC_INDEX irdimy,OC_INDEX irdimz,
   OC_INDEX iadimy,OC_INDEX iadimz,
   OC_INDEX ildimy,OC_INDEX ildimz)
    : A(iA),
      rdimy(irdimy), rdimz(irdimz),
      adimy(iadimy), adimz(iadimz),
      ldimy(ildimy), ldimz(ildimz) {}

  void Cmd(int threadnumber, void*);

private:
    // Declare but don't define default constructors and
    // assignment operator.
    _Oxs_FillCoefficientArraysFFTyzThread();
    _Oxs_FillCoefficientArraysFFTyzThread
       (const _Oxs_FillCoefficientArraysFFTyzThread&);
    _Oxs_FillCoefficientArraysFFTyzThread&
       operator=(const _Oxs_FillCoefficientArraysFFTyzThread&);
};

Oxs_JobControl<Oxs_Demag::A_coefs>
_Oxs_FillCoefficientArraysFFTyzThread::job_basket;

void _Oxs_FillCoefficientArraysFFTyzThread::Cmd(int threadnumber,
                                                void* /* data */)
{
  const OC_INDEX astridez = adimy;
  const OC_INDEX astridex = adimz*astridez;

  Oxs_FFT1DThreeVector workffty;
  workffty.SetDimensions(rdimy,ldimy,1);
  const OC_INDEX sourcedimy = ODTV_VECSIZE*rdimy;
  vector<OXS_FFT_REAL_TYPE> sourcebufy(2*sourcedimy);
  const OC_INDEX targetdimy = 2*ODTV_VECSIZE*adimy;
  vector<OXS_FFT_REAL_TYPE> targetbufy(2*targetdimy);

  Oxs_FFT1DThreeVector workfftz;
  workfftz.SetDimensions(rdimz,ldimz,1);
  const OC_INDEX sourcedimz = ODTV_VECSIZE*rdimz;
  vector<OXS_FFT_REAL_TYPE> sourcebufz(2*sourcedimz);
  const OC_INDEX targetdimz = 2*ODTV_VECSIZE*adimz;
  vector<OXS_FFT_REAL_TYPE> targetbufz(2*targetdimz);

  while(1) {
    OC_INDEX nstart,nstop;
    job_basket.GetJob(threadnumber,nstart,nstop);
    if(nstart >= nstop) break; // No more jobs
    OC_INDEX istart = nstart / astridex;
    OC_INDEX istop  = nstop  / astridex;

    // Loop through yz-planes
    for(OC_INDEX i=istart;i<istop;++i) {
      const OC_INDEX aioff = i*astridex;

      // Do all FFTs in y-direction for x-plane i.
      for(OC_INDEX k=0;k<rdimz;++k) {
        const OC_INDEX aoff = aioff + k*astridez;
        // Fill sourcebuf
        for(OC_INDEX j=0;j<rdimy;++j) {
          const OC_INDEX joff = ODTV_VECSIZE*j;
          const OC_INDEX ajoff = aoff + j;
          sourcebufy[joff]   = A[ajoff].A00;
          sourcebufy[joff+1] = A[ajoff].A11;
          sourcebufy[joff+2] = A[ajoff].A22;
          sourcebufy[sourcedimy+joff]   = A[ajoff].A01;
          sourcebufy[sourcedimy+joff+1] = A[ajoff].A02;
          sourcebufy[sourcedimy+joff+2] = A[ajoff].A12;
        }
        // A01 and A12 are odd wrt y, all other components are even.
        sourcebufy[0]*=0.5; sourcebufy[1]*=0.5; sourcebufy[2]*=0.5;
        sourcebufy[sourcedimy]    = 0.0;
        sourcebufy[sourcedimy+1] *= 0.5;
        sourcebufy[sourcedimy+2]  = 0.0;

        // Forward FFTs
        workffty.ForwardRealToComplexFFT(&(sourcebufy[0]),&(targetbufy[0]));
        workffty.ForwardRealToComplexFFT(&(sourcebufy[sourcedimy]),
                                         &(targetbufy[targetdimy]));


        // Copy back from targetbuf.  A00, A11, A22 and A02 are even
        // wrt y, so for those terms take the real component of the
        // transformed data.  A01 and A12 are odd wrt y, so take the
        // imaginary component for those two terms.
        for(OC_INDEX j=0;j<adimy;++j) {
          const OC_INDEX joff = 2*ODTV_VECSIZE*j;
          const OC_INDEX ajoff = aoff + j;
          A[ajoff].A00 = targetbufy[joff];
          A[ajoff].A11 = targetbufy[joff+2];
          A[ajoff].A22 = targetbufy[joff+4];
          A[ajoff].A01 = targetbufy[targetdimy+joff+1];
          A[ajoff].A02 = targetbufy[targetdimy+joff+2];
          A[ajoff].A12 = targetbufy[targetdimy+joff+5];
        }
      }

      // Do all FFTs in z-direction for x-plane i.
      for(OC_INDEX j=0;j<adimy;++j) {
        const OC_INDEX aoff = aioff + j;
        // Fill sourcebuf
        for(OC_INDEX k=0;k<rdimz;++k) {
          const OC_INDEX koff = ODTV_VECSIZE*k;
          const OC_INDEX akoff = aoff + k*astridez;
          sourcebufz[koff]   = A[akoff].A00;
          sourcebufz[koff+1] = A[akoff].A11;
          sourcebufz[koff+2] = A[akoff].A22;
          sourcebufz[sourcedimz+koff]   = A[akoff].A01;
          sourcebufz[sourcedimz+koff+1] = A[akoff].A02;
          sourcebufz[sourcedimz+koff+2] = A[akoff].A12;
        }
        // A02 and A12 are odd wrt z, all other components are even.
        sourcebufz[0]*=0.5; sourcebufz[1]*=0.5; sourcebufz[2]*=0.5;
        sourcebufz[sourcedimz] *= 0.5;
        sourcebufz[sourcedimz+1] = sourcebufz[sourcedimz+2] = 0.0;

        // Foward FFTs
        workfftz.ForwardRealToComplexFFT(&(sourcebufz[0]),
                                         &(targetbufz[0]));
        workfftz.ForwardRealToComplexFFT(&(sourcebufz[sourcedimz]),
                                         &(targetbufz[targetdimz]));

        // Copy back from targetbuf.  Since A00, A11, A22 and A01 are
        // even wrt z, take the real component of the transformed data
        // for those terms.  A02 and A12 are odd wrt z, so take the
        // imaginary component for those two terms.  The "8*" factor
        // accounts for the zero-padded/symmetry conversion for all
        // the x, y and z transforms.  See NOTES VII, 1-May-2015, p95
        // for details.  Additionally, each odd symmetry induces a
        // factor of sqrt(-1); each of the off-diagonal terms has odd
        // symmetry across two axes, so the end result in each case is
        // a real value but with a -1 factor.
        for(OC_INDEX k=0;k<adimz;++k) {
          const OC_INDEX koff = 2*ODTV_VECSIZE*k;
          const OC_INDEX akoff = aoff + k*astridez;
          A[akoff].A00 =  8*targetbufz[koff];
          A[akoff].A11 =  8*targetbufz[koff+2];
          A[akoff].A22 =  8*targetbufz[koff+4];
          A[akoff].A01 = -8*targetbufz[targetdimz+koff];
          A[akoff].A02 = -8*targetbufz[targetdimz+koff+3];
          A[akoff].A12 = -8*targetbufz[targetdimz+koff+5];
        }
      }
    }
  }
}


void Oxs_Demag::FillCoefficientArrays(const Oxs_Mesh* genmesh) const
{ // This routine is conceptually const.
#if REPORT_TIME
  dvltimer[0].name = "FillCoefs";
  dvltimer[0].Start();
#endif // REPORT_TIME
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
  if(coarse_grid) {
    // If coarse block dimensions doesn't exactly divide mesh
    // dimensions, then round up for additional zero padding.
    rdimx = (rdimx + coarse_block_x - 1)/coarse_block_x;
    rdimy = (rdimy + coarse_block_y - 1)/coarse_block_y;
    rdimz = (rdimz + coarse_block_z - 1)/coarse_block_z;
    if((xperiodic && rdimx*coarse_block_x != mesh->DimX())
       || (yperiodic && rdimy*coarse_block_y != mesh->DimY())
       || (zperiodic && rdimz*coarse_block_z != mesh->DimZ())) {
      throw Oxs_ExtError(this,
             "Coarse block dimensions must divide periodic dimensions");
    }
  }

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

  // Compute block size for "convolution" embedded with inner FFT's.
  {
    OC_INDEX footprint =
      // y-FFT buffer
      ODTV_COMPLEXSIZE*ODTV_VECSIZE*sizeof(OXS_FFT_REAL_TYPE)*cdimy*rdimz
      // z-FFT buffer
      + ODTV_COMPLEXSIZE*ODTV_VECSIZE*sizeof(OXS_FFT_REAL_TYPE)*cdimz*8;
    embed_block_size = 1; // Default
    if(footprint > 2*cache_size) {
      // Footprint too big to fit in cache; fallback for efficient
      // transposing copy
      OC_INDEX datum_size
        = ODTV_VECSIZE*ODTV_COMPLEXSIZE*sizeof(OXS_FFT_REAL_TYPE);
      embed_block_size = datum_size * OC_CACHE_LINESIZE
        / (Nb_Gcd(datum_size,OC_INDEX(OC_CACHE_LINESIZE))*datum_size);
      if(embed_block_size<1) embed_block_size = 1; // Safety
    }
#if (VERBOSE_DEBUG && !defined(NDEBUG))
    fprintf(stderr,"Cache=%g KB, footprint=%g KB, embed_block_size=%ld\n",
            cache_size/1024.,footprint/1024.,long(embed_block_size));
#endif
  }

  // Routine-local, dummy copies of FFT objects used to determine
  // logical dimensions and scaling.  These objects could be replaced
  // with static calls that reported this information, without initialize
  // full FFT objects including roots of unity.
  //
  // In the threaded code, each thread creates its own FFT instances,
  // using keyword "Oxs_DemagFFTObject" to Oxs_ThreadRunObj::local_locker.
  Oxs_FFT1DThreeVector fftx;
  Oxs_FFTStrided ffty;
  Oxs_FFTStrided fftz;
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

  // Access strides for Hxfrm.  Adjust these so that each row starts on
  // a cache line, and make each dimension odd to protect against cache
  // thrash.
  {
    OC_INDEX cacheline_mod = OC_CACHE_LINESIZE/sizeof(OXS_FFT_REAL_TYPE);
    /// Number of reals in a single cache line; used for aligning rows to
    /// cache line boundaries.
    Hxfrm_jstride = ODTV_COMPLEXSIZE*ODTV_VECSIZE*cdimx;
    if(Hxfrm_jstride%cacheline_mod != 0) {
      Hxfrm_jstride += cacheline_mod - (Hxfrm_jstride%cacheline_mod);
    }
    if((Hxfrm_jstride/cacheline_mod)%2 == 0) Hxfrm_jstride += cacheline_mod;
    Hxfrm_kstride = Hxfrm_jstride*(rdimy + (rdimy+1)%2);
  }

  // Dimension and stride info for thread lockers
  locker_info.Set(rdimx,rdimy,rdimz,cdimx,cdimy,cdimz,
                  Hxfrm_jstride,Hxfrm_kstride,embed_block_size,
                  MakeLockerName());

  // FFT scaling
  // Note: Since H = -N*M, and by convention with the rest of this code,
  // we store "-N" instead of "N" so we don't have to multiply the
  // output from the FFT + iFFT by -1 in GetEnergy() below.
  const OXS_FFT_REAL_TYPE fft_scaling = -1 *
               fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();

#if (VERBOSE_DEBUG && !defined(NDEBUG))
  fprintf(stderr,"RDIMS: (%ld,%ld,%ld)\n",
          (long)rdimx,(long)rdimy,(long)rdimz);
  fprintf(stderr,"CDIMS: (%ld,%ld,%ld)\n",
          (long)cdimx,(long)cdimy,(long)cdimz);
  fprintf(stderr,"LDIMS: (%ld,%ld,%ld)\n",
          (long)ldimx,(long)ldimy,(long)ldimz);
  fprintf(stderr,"ADIMS: (%ld,%ld,%ld)\n",
          (long)adimx,(long)adimy,(long)adimz);
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

  OC_INDEX astridez = adimy;
  OC_INDEX astridex = astridez*adimz;
  OC_INDEX asize = astridex*adimx;
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
  if(coarse_grid) {
    dx *= coarse_block_x;
    dy *= coarse_block_y;
    dz *= coarse_block_z;
  }
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
#if REPORT_TIME
    dvltimer[1].name="initA_anal";
    dvltimer[1].Start();
#endif // REPORT_TIME

    // Tensor scale factor and FFT scaling.  This allows the "NoScale" FFT
    // routines to be used.  NB: There is effectively a "-1" built into
    // the differencing sections below, because we compute d^6/dx^2 dy^2
    // dz^2 instead of -d^6/dx^2 dy^2 dz^2 as required.
    OC_REALWIDE scale = fftx.GetScaling() * ffty.GetScaling()
      * fftz.GetScaling() / (4*WIDE_PI*dx*dy*dz);

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
    ComputeD6f(Oxs_Newell_f_xx,workspace,threadtree,dx,dy,dz,MaxThreadCount);
    for(i=0;i<wdim1;i++) {
      for(k=0;k<wdim3;k++) {
        for(j=0;j<wdim2;j++) {
          A[i*astridex + k*astridez + j].A00 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_g_xy,workspace,threadtree,dx,dy,dz,MaxThreadCount);
    for(i=0;i<wdim1;i++) {
      for(k=0;k<wdim3;k++) {
        for(j=0;j<wdim2;j++) {
          A[i*astridex + k*astridez + j].A01 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_g_xz,workspace,threadtree,dx,dy,dz,MaxThreadCount);
    for(i=0;i<wdim1;i++) {
      for(k=0;k<wdim3;k++) {
        for(j=0;j<wdim2;j++) {
          A[i*astridex + k*astridez + j].A02 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_f_yy,workspace,threadtree,dx,dy,dz,MaxThreadCount);
    for(i=0;i<wdim1;i++) {
      for(k=0;k<wdim3;k++) {
        for(j=0;j<wdim2;j++) {
          A[i*astridex + k*astridez + j].A11 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_g_yz,workspace,threadtree,dx,dy,dz,MaxThreadCount);
    for(i=0;i<wdim1;i++) {
      for(k=0;k<wdim3;k++) {
        for(j=0;j<wdim2;j++) {
          A[i*astridex + k*astridez + j].A12 = scale*workspace(i,j,k);
        }
      }
    }
    ComputeD6f(Oxs_Newell_f_zz,workspace,threadtree,dx,dy,dz,MaxThreadCount);
    for(i=0;i<wdim1;i++) {
      for(k=0;k<wdim3;k++) {
        for(j=0;j<wdim2;j++) {
          A[i*astridex + k*astridez + j].A22 = scale*workspace(i,j,k);
        }
      }
    }
#if REPORT_TIME
    dvltimer[1].Stop(wdim1*wdim2*wdim3*sizeof(A_coefs));
#endif // REPORT_TIME

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
#if REPORT_TIME
    dvltimer[2].name = "initA_asymp";
    dvltimer[2].Start();
#endif // REPORT_TIME
    if(scaled_arad>=0.0) {
      // Note that all distances here are in "reduced" units,
      // scaled so that dx, dy, and dz are either small integers
      // or else close to 1.0.
      OXS_DEMAG_REAL_ASYMP scaled_arad_sq = scaled_arad*scaled_arad;
      _Oxs_FillCoefficientArraysAsympThread
        asymp_thread(A,rdimx,rdimy,rdimz,adimx,adimy,adimz,MaxThreadCount,
                     dx,dy,dz,scaled_arad_sq,fft_scaling);
      threadtree.LaunchTree(asymp_thread,0);
    }
#if REPORT_TIME
    dvltimer[2].Stop((rdimx*rdimy*rdimz
                      -OC_INDEX(ceil(4./3.*PI*scaled_arad*scaled_arad
                                     *scaled_arad/(dx*dy*dz))))
                     *sizeof(A_coefs));
#endif // REPORT_TIME

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
  }

  // Step 2.6: If periodic boundaries selected, compute periodic tensors
  // instead.
  // NOTE THAT CODE BELOW CURRENTLY ONLY SUPPORTS 1D PERIODIC!!!
  // NOTE 2: Keep this code in sync with that in
  //         Oxs_Demag::IncrementPreconditioner
  if(xperiodic) {
#if REPORT_TIME
    dvltimer[1].name="initA_pbcx";
    dvltimer[1].Start();
#endif // REPORT_TIME
    _Oxs_FillCoefficientArraysPBCxThread
      pbcx_thread(A,rdimx,rdimy,rdimz,astridex,astridez,MaxThreadCount,
                  dx,dy,dz,scaled_arad,fft_scaling);
    threadtree.LaunchTree(pbcx_thread,0);
#if REPORT_TIME
    dvltimer[1].Stop();
#endif // REPORT_TIME
  }
  if(yperiodic) {
#if REPORT_TIME
    dvltimer[1].name="initA_pbcy";
    dvltimer[1].Start();
#endif // REPORT_TIME
    _Oxs_FillCoefficientArraysPBCyThread
      pbcy_thread(A,rdimx,rdimy,rdimz,astridex,astridez,MaxThreadCount,
                  dx,dy,dz,scaled_arad,fft_scaling);
    threadtree.LaunchTree(pbcy_thread,0);
#if REPORT_TIME
    dvltimer[1].Stop();
#endif // REPORT_TIME
  }
  if(zperiodic) {
#if REPORT_TIME
    dvltimer[1].name="initA_pbcz";
    dvltimer[1].Start();
#endif // REPORT_TIME
    _Oxs_FillCoefficientArraysPBCzThread
      pbcz_thread(A,rdimx,rdimy,rdimz,astridex,astridez,MaxThreadCount,
                  dx,dy,dz,scaled_arad,fft_scaling);
    threadtree.LaunchTree(pbcz_thread,0);
#if REPORT_TIME
    dvltimer[1].Stop();
#endif // REPORT_TIME
  }

  // Step 3: Do FFTs.  We only need store 1/8th of the results because
  //   of symmetries.  In this computation, make use of the relationship
  //   between FFTs of symmetric (even or odd) sequences and zero-padded
  //   sequences, as discussed in NOTES VII, 1-May-2015, p95.  In this
  //   regard, note that ldim is always >= 2*rdim, so ldim/2<rdim
  //   never happens, hence the sequence midpoint=ldim/2 can always be
  //   taken as zero.
  // Note: In this code STL vector objects are used to acquire scratch
  //  buffer space, using the &(arr[0]) idiom.  If using C++-11, the
  //  arr.data() member function could be used instead.
  {
#if REPORT_TIME
    dvltimer[3].name = "initA_fftx";
    dvltimer[3].Start();
#endif // REPORT_TIME
    _Oxs_FillCoefficientArraysFFTxThread
        fftx_thread(A,rdimx,rdimy,rdimz,adimx,adimy,adimz,
                    ldimx,MaxThreadCount);
    threadtree.LaunchTree(fftx_thread,0);
#if REPORT_TIME
    dvltimer[3].Stop(sizeof(A_coefs)*(rdimx+adimx)*rdimy*rdimz);
#endif // REPORT_TIME

#if REPORT_TIME
    dvltimer[4].name = "initA_fftyz";
    dvltimer[4].Start();
#endif // REPORT_TIME
    // FFTs in y and z directions.
    _Oxs_FillCoefficientArraysFFTyzThread::job_basket.Init(MaxThreadCount,
                                                           &A,adimy*adimz);
    _Oxs_FillCoefficientArraysFFTyzThread
      fftyz_thread(A,rdimy,rdimz,adimy,adimz,ldimy,ldimz);
    threadtree.LaunchTree(fftyz_thread,0);
#if REPORT_TIME
    dvltimer[4].Stop(sizeof(A_coefs)*adimx
                     *((rdimy+adimy)*rdimz+adimy*(rdimz+adimz)));
#endif // REPORT_TIME
  }

#if REPORT_TIME
  inittime.Stop();
#endif // REPORT_TIME
#if REPORT_TIME
  dvltimer[0].Stop();
#endif // REPORT_TIME
}


class _Oxs_DemagFFTxThread : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<OC_REAL8m> job_basket;

  const Oxs_MeshValue<ThreeVector>* spin;
  const Oxs_MeshValue<OC_REAL8m>* Ms;

  OXS_FFT_REAL_TYPE* carr;

  const Oxs_Demag::Oxs_FFTLocker_Info* locker_info;

  _Oxs_DemagFFTxThread()
    : spin(0), Ms(0), carr(0), locker_info(0) {}
  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_DemagFFTxThread::job_basket;

void _Oxs_DemagFFTxThread::Cmd(int threadnumber, void* /* data */)
{
  // Thread local storage
  Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info->name);
  if(!foo) {
    // Oxs_FFTLocker object not constructed
    foo = new Oxs_Demag::Oxs_FFTLocker(*locker_info,threadnumber);
    local_locker.AddItem(locker_info->name,foo);
  }
  Oxs_Demag::Oxs_FFTLocker* locker
    = dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo);
  if(!locker) {
    Oxs_ThreadError::SetError(String("Error in"
       "_Oxs_DemagFFTxThread::Cmd(): locker downcast failed."));
    return;
  }
  Oxs_FFT1DThreeVector& fftx = locker->fftx;

  // The Ms_?stride and carr+?stride are all offsets in units of OC_REAL8m.
  // Note that the Ms_?stride values can be used also with the spin array,
  // but in that case the effective offset unit becomes Oxs_ThreeVectors.
  const OC_INDEX Ms_jstride = locker_info->rdimx;
  const OC_INDEX Ms_kstride = Ms_jstride * locker_info->rdimy;
  const OC_INDEX carr_jstride = locker_info->Hxfrm_jstride;
  const OC_INDEX carr_kstride = locker_info->Hxfrm_kstride;

  const OC_INDEX j_dim = locker_info->rdimy;

  // OXS_FFT_REAL_TYPE* const scratch = (locker->fftx_scratch);
  fftx.AdjustArrayCount(1); // If carr is padded along x axis then the
  /// array facility in Oxs_FFT1DThreeVector can't be used.

  while(1) {
    OC_INDEX nstart,nstop;
    job_basket.GetJob(threadnumber,nstart,nstop);
    if(nstart >= nstop) break; // No more jobs

    OC_INDEX kstart = nstart/Ms_kstride;
    OC_INDEX jstart = nstart - kstart*Ms_kstride;

    OC_INDEX kstop  =  nstop/Ms_kstride;
    OC_INDEX jstop  =  nstop -  kstop*Ms_kstride;

    // Clean-up for endpoints in dead zone
    if(jstart>=j_dim) {
      jstart = 0; ++kstart;
    }
    if(jstop>j_dim) {
      jstop = j_dim;
    }
    if(kstart>kstop) continue;  // Whole range is in dead zone

#if (3*OC_REAL8m_WIDTH) != OC_THREE_VECTOR_REAL8m_WIDTH
# error Oxs_ThreeVector not tightly packed.
#endif
#ifndef NDEBUG
    // The call below to ForwardRealToComplexFFT puns the
    // Oxs_ThreeVector array spin into an OC_REAL8m array.  For this
    // to work the Oxs_ThreeVector class must be packed tight.
    if(sizeof(ThreeVector) != 3*sizeof(OC_REAL8m)) {
      Oxs_ThreadError::SetError(String("Error in"
         "_Oxs_DemagFFTxThread::Cmd(): Oxs_ThreeVector not tightly packed."));
      return;
    }
#endif // NDEBUG

    for(OC_INDEX k=kstart;k<=kstop;++k) {
      OC_INDEX j_line_stop = j_dim;
      if(k==kstop) j_line_stop = jstop;
      for(OC_INDEX j=jstart; j<j_line_stop; ++j) {
        const OC_INDEX jk_in = j*Ms_jstride + k*Ms_kstride;
        const OC_INDEX jk_out = j*carr_jstride + k*carr_kstride;
        fftx.ForwardRealToComplexFFT
          (reinterpret_cast<const OC_REAL8m*>(&((*spin)[jk_in].x)), // CHEAT
           carr+jk_out,
           reinterpret_cast<const OC_REAL8m*>(&((*Ms)[jk_in]))); // CHEAT
      }
      jstart=0;
    }
  }
}

class _Oxs_DemagiFFTxDotThread : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<OC_REAL8m> job_basket;

  OXS_FFT_REAL_TYPE* carr;
  const Oxs_MeshValue<ThreeVector> *spin_ptr;
  const Oxs_MeshValue<OC_REAL8m> *Ms_ptr;
  Oxs_ComputeEnergyData* oced_ptr;

  const Oxs_Demag::Oxs_FFTLocker_Info* locker_info;

  Nb_ArrayWrapper<Nb_Xpfloat> energy_sum; // Per thread export.
  // Note: Some (especially 32-bit compilers) don't do 16-byte
  // alignment for SSE __m128d data type in std::vector, so use
  // Nb_ArrayWrapper instead.

  _Oxs_DemagiFFTxDotThread(int threadcount)
    : carr(0),
      spin_ptr(0), Ms_ptr(0), oced_ptr(0),
      locker_info(0) {
    energy_sum.SetSize(OC_INDEX(threadcount),Nb_Xpfloat::Alignment());
#if NB_XPFLOAT_USE_SSE // Check alignment
    assert(size_t(energy_sum.GetPtr()) % sizeof(Nb_Xpfloat) == 0);
#endif
    for(OC_INDEX i=0;i<threadcount;++i) {
      energy_sum[i] = 0.0;
    }
  }
  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_DemagiFFTxDotThread::job_basket;

void _Oxs_DemagiFFTxDotThread::Cmd(int threadnumber, void* /* data */)
{
  // Thread local storage
  Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info->name);
  if(!foo) {
    // Oxs_FFTLocker object not constructed
    foo = new Oxs_Demag::Oxs_FFTLocker(*locker_info,threadnumber);
    local_locker.AddItem(locker_info->name,foo);
  }
  Oxs_Demag::Oxs_FFTLocker* locker
    = dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo);
  if(!locker) {
    Oxs_ThreadError::SetError(String("Error in"
       "_Oxs_DemagiFFTxDotThread::Cmd(): locker downcast failed."));
    return;
  }
  Oxs_FFT1DThreeVector& fftx = locker->fftx;

  // The Ms_?stride and carr+?stride are all offsets in units of OC_REAL8m.
  // Note that the Ms_?stride values can be used also with the spin array,
  // but in that case the effective offset unit becomes Oxs_ThreeVectors.
  const OC_INDEX Ms_jstride = locker_info->rdimx;
  const OC_INDEX Ms_kstride = Ms_jstride * locker_info->rdimy;
  const OC_INDEX carr_jstride = locker_info->Hxfrm_jstride;
  const OC_INDEX carr_kstride = locker_info->Hxfrm_kstride;
  const OC_INDEX i_dim = locker_info->rdimx;
  const OC_INDEX j_dim = locker_info->rdimy;

  const OXS_FFT_REAL_TYPE emult =  -0.5 * MU0;

  const Oxs_MeshValue<ThreeVector>& spin = *spin_ptr;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *Ms_ptr;
  Oxs_ComputeEnergyData& oced = *oced_ptr;

  Nb_Xpfloat energy_sumA(0.0), energy_sumB(0.0);

  fftx.AdjustArrayCount(1);

  while(1) {
    OC_INDEX nstart,nstop;
    job_basket.GetJob(threadnumber,nstart,nstop);
    if(nstart >= nstop) break; // No more jobs
    OC_INDEX kstart = nstart/Ms_kstride;
    OC_INDEX jstart = nstart - kstart*Ms_kstride;

    OC_INDEX kstop  =  nstop/Ms_kstride;
    OC_INDEX jstop  =  nstop -  kstop*Ms_kstride;

    // Clean-up for endpoints in dead zone
    if(jstart>=j_dim) {
      jstart = 0; ++kstart;
    }
    if(jstop>j_dim) {
      jstop = j_dim;
    }
    if(kstart>kstop) continue;  // Whole range is in dead zone

    for(OC_INDEX k=kstart;k<=kstop;++k,jstart=0) {
      OC_INDEX j_line_stop = j_dim;
      if(k==kstop) j_line_stop = jstop;
      if(jstart>=j_line_stop) continue;

      for(OC_INDEX j=jstart;j<j_line_stop;++j) {
        const OC_INDEX ioffset = (j*Ms_jstride+k*Ms_kstride);
        OXS_FFT_REAL_TYPE* scratch = (locker->fftx_scratch) + (ioffset%2);
        /// The base offsets of all import/export arrays are 16-byte
        /// aligned.  For SSE code, we want scratch to align with the
        /// working piece of these arrays, so offset by 1 if necessary.
        /// Note that fftx_scratch is allocated in the Oxs_FFTLocker
        /// constructor to size info.cdimx+1 >= info.rdimx+1 exactly to
        /// allow this.
        fftx.InverseComplexToRealFFT(carr+j*carr_jstride
                                     +k*carr_kstride,scratch);

        if(oced.H) {
          for(OC_INDEX i=0;i<i_dim;++i) {
            (*oced.H)[ioffset+i].Set(scratch[3*i],
                                     scratch[3*i+1],scratch[3*i+2]);
          }
        }

        if(oced.H_accum) {
          for(OC_INDEX i=0;i<i_dim;++i) {
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
            for(;i+STRIDE-1<i_dim;i+=STRIDE) {
              Oc_Duet mx,my,mz;
              Oxs_ThreeVectorPairLoadAligned(&(ispin[i]),mx,my,mz);
              
              Oc_Duet tHx,tHy,tHz;
              Oxs_ThreeVectorPairLoadAligned
                ((Oxs_ThreeVector*)(&(scratch[3*i])),tHx,tHy,tHz);

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
            for(;i+STRIDE-1<i_dim;i+=STRIDE) {
              Oc_Duet mx,my,mz;
              Oxs_ThreeVectorPairLoadAligned(&(ispin[i]),mx,my,mz);
              
              Oc_Duet tHx,tHy,tHz;
              Oxs_ThreeVectorPairLoadAligned
                ((Oxs_ThreeVector*)(&(scratch[3*i])),tHx,tHy,tHz);

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

          for(;i<i_dim;++i) {
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
          for(i=0;i<i_dim;++i) {
            const ThreeVector& m = spin[ioffset+i];
            const OXS_FFT_REAL_TYPE& tHx = scratch[3*i];
            const OXS_FFT_REAL_TYPE& tHy = scratch[3*i+1];
            const OXS_FFT_REAL_TYPE& tHz = scratch[3*i+2];

            OC_REAL8m ei = emult * Ms[ioffset+i]
              * (m.x * tHx + m.y * tHy + m.z * tHz);

            OC_REAL8m tx = m.y * tHz - m.z * tHy; // mxH
            OC_REAL8m ty = m.z * tHx - m.x * tHz;
            OC_REAL8m tz = m.x * tHy - m.y * tHx;

            energy_sumA += ei;

            if(oced.energy) {
              (*oced.energy)[ioffset + i] = ei;
            }
            if(oced.energy_accum) {
              (*oced.energy_accum)[ioffset + i] += ei;
            }
            if(oced.mxH) {
              (*oced.mxH)[ioffset + i] = ThreeVector(tx,ty,tz);
            }
            if(oced.mxH_accum) {
              (*oced.mxH_accum)[ioffset + i] += ThreeVector(tx,ty,tz);
            }

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
  energy_sum[OC_INDEX(threadnumber)] = energy_sumA;
}

class _Oxs_DemagFFTyzConvolveThread : public Oxs_ThreadRunObj {
public:
  // Note: The job basket is static, so only one "set" (tree) of this
  // class may be run at any one time.
  static Oxs_JobControl<Oxs_Demag::A_coefs> job_basket;

  const Oxs_StripedArray<Oxs_Demag::A_coefs>* A_ptr;
  OXS_FFT_REAL_TYPE* carr;

  const Oxs_Demag::Oxs_FFTLocker_Info* locker_info;

  OC_INDEX adimx,adimy,adimz;

  OC_INDEX thread_count;

  _Oxs_DemagFFTyzConvolveThread()
    : A_ptr(0), carr(0), locker_info(0),
      adimx(0),adimy(0),adimz(0),
      thread_count(0) {}

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<Oxs_Demag::A_coefs> _Oxs_DemagFFTyzConvolveThread::job_basket;

void _Oxs_DemagFFTyzConvolveThread::Cmd(int threadnumber, void* /* data */)
{
  // Access thread local storage
  Oxs_Demag::Oxs_FFTLocker* locker = 0;
  {
    Oxs_ThreadMapDataObject* foo = local_locker.GetItem(locker_info->name);
    if(!foo) {
      // Oxs_FFTLocker object not constructed
      foo = new Oxs_Demag::Oxs_FFTLocker(*locker_info,threadnumber);
      local_locker.AddItem(locker_info->name,foo);
    }
    locker = dynamic_cast<Oxs_Demag::Oxs_FFTLocker*>(foo);
    if(!locker) {
      Oxs_ThreadError::SetError(String("Error in"
         "_Oxs_DemagFFTyzConvolveThread::Cmd(): locker downcast failed."));
      return;
    }
  }

#if OXS_THREAD_TIMER_COUNT > 12
  Oxs_ThreadTimers* threadtimers = dynamic_cast<Oxs_ThreadTimers*>
    (local_locker.GetItem(String(OXS_THREAD_TIMER_NAME)));
  if(!threadtimers) {
      Oxs_ThreadError::SetError(String("Error in"
           "_Oxs_DemagFFTyzConvolveThread::::Cmd(): timers not found."));
      return;
  }
# define StartTimer(clock) threadtimers->timers[clock].Start()
# define StopTimer(clock) threadtimers->timers[clock].Stop()
#else
# define StartTimer(clock)
# define StopTimer(clock)
#endif

  StartTimer(0);

  Oxs_FFTStrided* const ffty = &(locker->ffty);
  Oxs_FFTStrided* const fftz = &(locker->fftz);

  const OC_INDEX cjstride = locker_info->Hxfrm_jstride;
  const OC_INDEX ckstride = locker_info->Hxfrm_kstride;

  OXS_FFT_REAL_TYPE* const Hywork = locker->ffty_Hwork;
  const OC_INDEX Hy_kstride = locker->ffty_Hwork_zstride;
  const OC_INDEX Hy_istride = locker->ffty_Hwork_xstride;

  const OC_INDEX rdimy = locker_info->rdimy;
  const OC_INDEX rdimz = locker_info->rdimz;
  const OC_INDEX cdimy = locker_info->cdimy;
  const OC_INDEX cdimz = locker_info->cdimz;
  const OC_INDEX embed_block_size = locker_info->embed_block_size;

  OXS_FFT_REAL_TYPE* const Hzwork = (cdimz>1 ? locker->fftz_Hwork : 0);
  const OC_INDEX Hz_kstride = (cdimz>1 ? locker->fftz_Hwork_zstride
                               : Hy_kstride);

#if OC_USE_SSE
  // Check alignment restrictions are met
  assert(reinterpret_cast<OC_UINDEX>(carr)%16==0);
  assert(reinterpret_cast<OC_UINDEX>(Hywork)%16==0);
  assert(reinterpret_cast<OC_UINDEX>(Hzwork)%16==0);
#endif

  const Oxs_Demag::A_coefs* A = A_ptr->GetArrBase();

  // Adjust ffty to use Hywork and fftz to use Hzwork
  ffty->AdjustInputDimensions(rdimy,ODTV_VECSIZE*ODTV_COMPLEXSIZE,
                              ODTV_VECSIZE);
  const OC_INDEX ypack
    = (cdimz>1
       ? OC_MIN(Hz_kstride/(ODTV_VECSIZE*ODTV_COMPLEXSIZE*2),adimy)
       : adimy);
  fftz->AdjustInputDimensions(rdimz,Hz_kstride,1); // 1 is dummy
#if (VERBOSE_DEBUG && !defined(NDEBUG))
  static int ypack_print_count = 0;
  if(threadnumber==0 && ypack_print_count==0) {
    fprintf(stderr,"Hz_kstride=%d, ypack=%d\n",int(Hz_kstride),int(ypack));
    ++ypack_print_count;
  }
#endif
  while(1) {
    OC_INDEX nstart,nstop;
    job_basket.GetJob(threadnumber,nstart,nstop);
    if(nstart >= nstop) break; // No more jobs
    OC_INDEX istart = nstart / (adimy*adimz);
    OC_INDEX istop  = nstop  / (adimy*adimz);

    for(OC_INDEX i1=istart;i1<istop;i1+=embed_block_size) {
      const OC_INDEX i2 = (i1+embed_block_size<istop
                         ? i1+embed_block_size : istop);
      const OC_INDEX ispan = i2 - i1;
      OC_INDEX i,j,k;

      // Copy data from carr into Hwork scratch space.  Do the forward
      // FFT-y on each xy-plane as it is loaded.
      StartTimer(1);
      for(k=0;k<rdimz;++k) {
        StartTimer(2);
        OXS_FFT_REAL_TYPE* const Hbase = Hywork + k*Hy_kstride;
        OXS_FFT_REAL_TYPE* const cbase = carr
          + ODTV_VECSIZE*ODTV_COMPLEXSIZE*i1 + k*ckstride;
        for(j=0;j<rdimy;++j) {
          OC_INDEX Hindex = j*ODTV_VECSIZE*ODTV_COMPLEXSIZE;
          OC_INDEX cindex = j*cjstride;
          for(i=0;i<ispan; ++i, cindex+=6, Hindex+=Hy_istride) {
            // Hbase[Hindex]     = cbase[cindex];
            Oc_Prefetch<Ocpd_T0>(Hbase+Hindex+2*ODTV_VECSIZE*ODTV_COMPLEXSIZE);
            Oc_Prefetch<Ocpd_NTA>(cbase+cindex+4*cjstride);
            Oc_Duet tmp0,tmp1,tmp2;
            tmp0.LoadAligned(cbase[cindex]);    // x
            tmp1.LoadAligned(cbase[cindex+2]);  // y
            tmp2.LoadAligned(cbase[cindex+4]);  // z
            tmp0.StoreAligned(Hbase[Hindex]);
            tmp1.StoreAligned(Hbase[Hindex+2]);
            tmp2.StoreAligned(Hbase[Hindex+4]);
          }
        }
        StopTimer(2);
        StartTimer(3);
        for(i=0;i<ispan; ++i) {
          ffty->ForwardFFT(Hbase + i*Hy_istride);
        }
        StopTimer(3);
      }
      StopTimer(1);

      StartTimer(4);
      for(i=0;i<ispan;++i) {
        OXS_FFT_REAL_TYPE* const Hybase = Hywork + i*Hy_istride;
        OXS_FFT_REAL_TYPE* const Hzbase = (cdimz>1 ? Hzwork : Hybase);

        for(OC_INDEX jstart=0;jstart<adimy;jstart+=ypack) {
          const OC_INDEX jstop = (jstart+ypack<adimy ? jstart+ypack : adimy);
          OC_INDEX jb0 = cdimy - jstop + 1;   if(jb0<adimy) jb0 = adimy;
          OC_INDEX jb1 = cdimy - jstart + 1;  if(jb1>cdimy) jb1 = cdimy;
          OC_INDEX mirror_offset = (jstop-jstart) + (jb1-jb0) - 1
            + (jstart==0 ? 1 : 0);
          /// The following copy operation tight packs data from
          /// Hywork into Hzwork.  Given ja in the "front" half
          /// (j<adimy), the corresponding jb in the back half is jb =
          /// cdimy - ja in Hywork, and jb = mirror_offset - ja in
          /// Hzwork.  Special handling is required to handle the case
          /// ja==0, because that element doesn't reflect.
          /// ja==adimy-1 doesn't reflect either if cdimy is even, but
          /// that just affects the length of the copied j-run.

          if(cdimz>1) {
            // Copy data into z-work area
            StartTimer(11);
            const OC_INDEX ypass_offset = 4*Hy_kstride;
            for(k=0;k<rdimz;++k) {
              Oc_Prefetch<Ocpd_T0>(Hzbase + (k+cdimz/2)*Hz_kstride);
              // Front side
              const OXS_FFT_REAL_TYPE* const Hytmpa = Hybase + k*Hy_kstride
                + ODTV_VECSIZE*ODTV_COMPLEXSIZE*jstart;
              OXS_FFT_REAL_TYPE* const Hztmpa = Hzbase + k*Hz_kstride;
              OXS_FFT_REAL_TYPE* const Hztmpb = Hztmpa
                + ODTV_VECSIZE*ODTV_COMPLEXSIZE*(jstop-jstart);
              // Oc_Prefetch<Ocpd_T0>(Hztmpa+(cdimz/2)*Hz_kstride);
              //Oc_Prefetch<Ocpd_T0>(Hztmpb+(cdimz/2)*Hz_kstride);
              for(j=0;j<ODTV_VECSIZE*ODTV_COMPLEXSIZE*(jstop-jstart); j += 6) {
                // Hztmpa[j] = Hytmpa[j];
                Oc_Duet tmp0,tmp1,tmp2;
                tmp0.LoadAligned(Hytmpa[j]);
                tmp1.LoadAligned(Hytmpa[j+2]);
                tmp2.LoadAligned(Hytmpa[j+4]);
                tmp0.StoreAligned(Hztmpa[j]);
                tmp1.StoreAligned(Hztmpa[j+2]);
                tmp2.StoreAligned(Hztmpa[j+4]);
              }
              // Back side
              const OXS_FFT_REAL_TYPE* const Hytmpb = Hybase + k*Hy_kstride
                + ODTV_VECSIZE*ODTV_COMPLEXSIZE*jb0;
              for(j=0;j<ODTV_VECSIZE*ODTV_COMPLEXSIZE*(jb1-jb0); j += 6) {
                // Hztmpb[j] = Hytmpb[j];
                Oc_Duet tmp0,tmp1,tmp2;
                tmp0.LoadAligned(Hytmpb[j]);
                tmp1.LoadAligned(Hytmpb[j+2]);
                tmp2.LoadAligned(Hytmpb[j+4]);
                tmp0.StoreAligned(Hztmpb[j]);
                tmp1.StoreAligned(Hztmpb[j+2]);
                tmp2.StoreAligned(Hztmpb[j+4]);
                Oc_Prefetch<Ocpd_T0>(Hytmpb+j+ypass_offset);
                Oc_Prefetch<Ocpd_T0>(Hytmpa+j+ypass_offset);
              }
            }
            StopTimer(11);
            StartTimer(5);
            // Forward FFT-z on Hwork.
            fftz->AdjustArrayCount(ODTV_VECSIZE*((jstop-jstart)+(jb1-jb0)));
            fftz->ForwardFFT(Hzbase);
            StopTimer(5);
          }

          StartTimer(6);
          // Do matrix-vector multiply ("convolution") for block
          const OC_INDEX astridek = adimy;
          const OC_INDEX astridei = astridek*adimz;
          const Oxs_Demag::A_coefs* Abase = A + (i1+i)*astridei;
          for(OC_INDEX ka=0;ka<adimz;++ka) {
            const Oxs_Demag::A_coefs* Atmp = Abase + ka*astridek;
            OXS_FFT_REAL_TYPE* const Htmpa = Hzbase + ka*Hz_kstride;
            OXS_FFT_REAL_TYPE* const Htmpb
              = (0<ka && ka<adimz-1 ? Hzbase + (cdimz-ka)*Hz_kstride : 0);
            
            for(OC_INDEX ja=jstart;ja<jstop;++ja) {
              Oc_Prefetch<Ocpd_NTA>(Atmp+(astridek+ja));
              const Oxs_Demag::A_coefs& Aref = Atmp[ja];
              Oc_Duet A_00(Aref.A00);   Oc_Duet A_01(Aref.A01);
              Oc_Duet A_02(Aref.A02);   Oc_Duet A_11(Aref.A11);
              Oc_Duet A_12(Aref.A12);   Oc_Duet A_22(Aref.A22);

              const OC_INDEX jaoff = ja - jstart;
              const OC_INDEX aindex = jaoff*ODTV_VECSIZE*ODTV_COMPLEXSIZE;
              const OC_INDEX jboff = (ja == 0 ? 0 : mirror_offset - jaoff);
              const OC_INDEX bindex = jboff*ODTV_VECSIZE*ODTV_COMPLEXSIZE;
              { // j>=0, k>=0
                Oc_Duet Hx,Hy,Hz;
                Hx.LoadAligned(Htmpa[aindex]);
                Hy.LoadAligned(Htmpa[aindex+2]);
                Hz.LoadAligned(Htmpa[aindex+4]);

                Oc_Duet tx(A_00*Hx);
                Oc_Duet ty(A_01*Hx);
                Oc_Duet tz(A_02*Hx);

                tx = Oc_FMA(A_01,Hy,tx);
                ty = Oc_FMA(A_11,Hy,ty);
                tz = Oc_FMA(A_12,Hy,tz);

                tx = Oc_FMA(A_02,Hz,tx);
                ty = Oc_FMA(A_12,Hz,ty);
                tz = Oc_FMA(A_22,Hz,tz);

                tx.StoreAligned(Htmpa[aindex]);
                ty.StoreAligned(Htmpa[aindex+2]);
                tz.StoreAligned(Htmpa[aindex+4]);
              }
              if(jboff>jaoff) {
                // j<0, k>=0
                // Flip signs on a01 and a12 as compared to the j>=0
                // case because a01 and a12 are odd in y.
                Oc_Duet Hx,Hy,Hz;
                Hx.LoadAligned(Htmpa[bindex]);
                Hy.LoadAligned(Htmpa[bindex+2]);
                Hz.LoadAligned(Htmpa[bindex+4]);

                Oc_Duet mA_01(A_01);  mA_01 *= -1;
                Oc_Duet mA_12(A_12);  mA_12 *= -1;

                Oc_Duet tx(A_00*Hx);
                Oc_Duet ty(mA_01*Hx);
                Oc_Duet tz(A_02*Hx);

                tx = Oc_FMA(mA_01,Hy,tx);
                ty = Oc_FMA(A_11,Hy,ty);
                tz = Oc_FMA(mA_12,Hy,tz);

                tx = Oc_FMA(A_02,Hz,tx);
                ty = Oc_FMA(mA_12,Hz,ty);
                tz = Oc_FMA(A_22,Hz,tz);

                tx.StoreAligned(Htmpa[bindex]);
                ty.StoreAligned(Htmpa[bindex+2]);
                tz.StoreAligned(Htmpa[bindex+4]);
              }
              if(Htmpb) {
                // j>=0, k<0
                // Flip signs on a02 and a12 as compared to the k>=0, j>=0 case
                // because a02 and a12 are odd in z.
                Oc_Duet Hx,Hy,Hz;
                Hx.LoadAligned(Htmpb[aindex]);
                Hy.LoadAligned(Htmpb[aindex+2]);
                Hz.LoadAligned(Htmpb[aindex+4]);

                Oc_Duet mA_02(A_02);  mA_02 *= -1;
                Oc_Duet mA_12(A_12);  mA_12 *= -1;

                Oc_Duet tx(A_00*Hx);
                Oc_Duet ty(A_01*Hx);
                Oc_Duet tz(mA_02*Hx);

                tx = Oc_FMA(A_01,Hy,tx);
                ty = Oc_FMA(A_11,Hy,ty);
                tz = Oc_FMA(mA_12,Hy,tz);

                tx = Oc_FMA(mA_02,Hz,tx);
                ty = Oc_FMA(mA_12,Hz,ty);
                tz = Oc_FMA(A_22,Hz,tz);

                tx.StoreAligned(Htmpb[aindex]);
                ty.StoreAligned(Htmpb[aindex+2]);
                tz.StoreAligned(Htmpb[aindex+4]);
              }
              if(Htmpb && jboff>jaoff) {
                // j<0, k<0
                // Flip signs on a01 and a02 as compared to the k>=0, j>=0 case
                // because a01 is odd in y and even in z,
                //     and a02 is odd in z and even in y.
                // No change to a12 because it is odd in both y and z.
                Oc_Duet Hx,Hy,Hz;
                Hx.LoadAligned(Htmpb[bindex]);
                Hy.LoadAligned(Htmpb[bindex+2]);
                Hz.LoadAligned(Htmpb[bindex+4]);

                Oc_Duet mA_01(A_01);  mA_01 *= -1;
                Oc_Duet mA_02(A_02);  mA_02 *= -1;

                Oc_Duet tx(A_00*Hx);
                Oc_Duet ty(mA_01*Hx);
                Oc_Duet tz(mA_02*Hx);

                tx = Oc_FMA(mA_01,Hy,tx);
                ty = Oc_FMA(A_11,Hy,ty);
                tz = Oc_FMA(A_12,Hy,tz);

                tx = Oc_FMA(mA_02,Hz,tx);
                ty = Oc_FMA(A_12,Hz,ty);
                tz = Oc_FMA(A_22,Hz,tz);

                tx.StoreAligned(Htmpb[bindex]);
                ty.StoreAligned(Htmpb[bindex+2]);
                tz.StoreAligned(Htmpb[bindex+4]);
              }
            } // for(ja)
          } // for(ka)
          StopTimer(6);

          if(cdimz>1) {
            // Forward FFT-z on Hwork.
            StartTimer(7);
            fftz->InverseFFT(Hzbase);
            StopTimer(7);
            // Copy data out of z-work area
            StartTimer(12);
            for(k=0;k<rdimz;++k) {
              // Front side
              OXS_FFT_REAL_TYPE* const Hytmpa = Hybase + k*Hy_kstride
                + ODTV_VECSIZE*ODTV_COMPLEXSIZE*jstart;
              const OXS_FFT_REAL_TYPE* const Hztmpa = Hzbase + k*Hz_kstride;
              for(j=0;j<ODTV_VECSIZE*ODTV_COMPLEXSIZE*(jstop-jstart); j += 6) {
                // Hytmpa[j] = Hztmpa[j];
                Oc_Duet tmp0,tmp1,tmp2;
                tmp0.LoadAligned(Hztmpa[j]);
                tmp0.StoreAligned(Hytmpa[j]);
                tmp1.LoadAligned(Hztmpa[j+2]);
                tmp1.StoreAligned(Hytmpa[j+2]);
                tmp2.LoadAligned(Hztmpa[j+4]);
                tmp2.StoreAligned(Hytmpa[j+4]);
              }
              // Back side
              OXS_FFT_REAL_TYPE* const Hytmpb = Hybase + k*Hy_kstride
                + ODTV_VECSIZE*ODTV_COMPLEXSIZE*jb0;
              const OXS_FFT_REAL_TYPE* const Hztmpb = Hztmpa
                + ODTV_VECSIZE*ODTV_COMPLEXSIZE*(jstop-jstart);
              for(j=0;j<ODTV_VECSIZE*ODTV_COMPLEXSIZE*(jb1-jb0); j += 6) {
                // Hytmpb[j] = Hztmpb[j];
                Oc_Duet tmp0,tmp1,tmp2;
                tmp0.LoadAligned(Hztmpb[j]);
                tmp0.StoreAligned(Hytmpb[j]);
                tmp1.LoadAligned(Hztmpb[j+2]);
                tmp1.StoreAligned(Hytmpb[j+2]);
                tmp2.LoadAligned(Hztmpb[j+4]);
                tmp2.StoreAligned(Hytmpb[j+4]);
              }
            } // k
            StopTimer(12);
          }

        } // jstart
      } // i
      StopTimer(4);

      // Inverse FFT-y on Hwork, and copy back to carr
      StartTimer(8);
      const OC_INDEX ileadstop = ispan - (ispan%4);
      for(k=0;k<rdimz;++k) {
        OXS_FFT_REAL_TYPE* const Hbase = Hywork + k*Hy_kstride;
        OXS_FFT_REAL_TYPE* const cbase = carr
          + ODTV_VECSIZE*ODTV_COMPLEXSIZE*i1 + k*ckstride;
        for(i=0;i<ileadstop;i+=4) {
          StartTimer(9);
          ffty->InverseFFT(Hbase + i*Hy_istride);
          ffty->InverseFFT(Hbase + (i+1)*Hy_istride);
          StopTimer(9);
          StartTimer(10);
          for(j=0;j<rdimy;++j) {
#if 0
            cbase[6*i+j*cjstride]   = Hbase[i*Hy_istride+6*j];
            cbase[6*i+j*cjstride+1] = Hbase[i*Hy_istride+6*j+1];
            cbase[6*i+j*cjstride+2] = Hbase[i*Hy_istride+6*j+2];
            cbase[6*i+j*cjstride+3] = Hbase[i*Hy_istride+6*j+3];
            cbase[6*i+j*cjstride+4] = Hbase[i*Hy_istride+6*j+4];
            cbase[6*i+j*cjstride+5] = Hbase[i*Hy_istride+6*j+5];
            cbase[6*(i+1)+j*cjstride]   = Hbase[(i+1)*Hy_istride+6*j];
            cbase[6*(i+1)+j*cjstride+1] = Hbase[(i+1)*Hy_istride+6*j+1];
#else
            // If the cbase target is not in cache, then StoreStream
            // will be somewhat faster than StoreAligned.  But it is
            // difficult to guarantee this, and if the target is in
            // cache then StoreStream is noticeably slower than
            // StoreAligned.
            Oc_Duet tmpA, tmpB, tmpC, tmpD;
            tmpA.LoadAligned(Hbase[i*Hy_istride+6*j]);
            tmpB.LoadAligned(Hbase[i*Hy_istride+6*j+2]);
            tmpC.LoadAligned(Hbase[i*Hy_istride+6*j+4]);
            tmpD.LoadAligned(Hbase[(i+1)*Hy_istride+6*j]);

            tmpA.StoreAligned(cbase[6*i+j*cjstride]);
            tmpB.StoreAligned(cbase[6*i+j*cjstride+2]);
            tmpC.StoreAligned(cbase[6*i+j*cjstride+4]);
            tmpD.StoreAligned(cbase[6*(i+1)+j*cjstride]);
#endif
          }
          StopTimer(10);
          StartTimer(9);
          ffty->InverseFFT(Hbase + (i+2)*Hy_istride);
          StopTimer(9);
          StartTimer(10);
          for(j=0;j<rdimy;++j) {
#if 0
            cbase[6*(i+1)+j*cjstride+2] = Hbase[(i+1)*Hy_istride+6*j+2];
            cbase[6*(i+1)+j*cjstride+3] = Hbase[(i+1)*Hy_istride+6*j+3];
            cbase[6*(i+1)+j*cjstride+4] = Hbase[(i+1)*Hy_istride+6*j+4];
            cbase[6*(i+1)+j*cjstride+5] = Hbase[(i+1)*Hy_istride+6*j+5];
            cbase[6*(i+2)+j*cjstride]   = Hbase[(i+2)*Hy_istride+6*j];
            cbase[6*(i+2)+j*cjstride+1] = Hbase[(i+2)*Hy_istride+6*j+1];
            cbase[6*(i+2)+j*cjstride+2] = Hbase[(i+2)*Hy_istride+6*j+2];
            cbase[6*(i+2)+j*cjstride+3] = Hbase[(i+2)*Hy_istride+6*j+3];
#else
            Oc_Duet tmpA, tmpB, tmpC, tmpD;
            tmpA.LoadAligned(Hbase[(i+1)*Hy_istride+6*j+2]);
            tmpB.LoadAligned(Hbase[(i+1)*Hy_istride+6*j+4]);
            tmpC.LoadAligned(Hbase[(i+2)*Hy_istride+6*j]);
            tmpD.LoadAligned(Hbase[(i+2)*Hy_istride+6*j+2]);

            tmpA.StoreAligned(cbase[6*(i+1)+j*cjstride+2]);
            tmpB.StoreAligned(cbase[6*(i+1)+j*cjstride+4]);
            tmpC.StoreAligned(cbase[6*(i+2)+j*cjstride]);
            tmpD.StoreAligned(cbase[6*(i+2)+j*cjstride+2]);
#endif
          }
          StopTimer(10);
          StartTimer(9);
          ffty->InverseFFT(Hbase + (i+3)*Hy_istride);
          StopTimer(9);
          StartTimer(10);
          for(j=0;j<rdimy;++j) {
#if 0
            cbase[6*(i+2)+j*cjstride+4] = Hbase[(i+2)*Hy_istride+6*j+4];
            cbase[6*(i+2)+j*cjstride+5] = Hbase[(i+2)*Hy_istride+6*j+5];
            cbase[6*(i+3)+j*cjstride]   = Hbase[(i+3)*Hy_istride+6*j];
            cbase[6*(i+3)+j*cjstride+1] = Hbase[(i+3)*Hy_istride+6*j+1];
            cbase[6*(i+3)+j*cjstride+2] = Hbase[(i+3)*Hy_istride+6*j+2];
            cbase[6*(i+3)+j*cjstride+3] = Hbase[(i+3)*Hy_istride+6*j+3];
            cbase[6*(i+3)+j*cjstride+4] = Hbase[(i+3)*Hy_istride+6*j+4];
            cbase[6*(i+3)+j*cjstride+5] = Hbase[(i+3)*Hy_istride+6*j+5];
#else
            Oc_Duet tmpA, tmpB, tmpC, tmpD;
            tmpA.LoadAligned(Hbase[(i+2)*Hy_istride+6*j+4]);
            tmpB.LoadAligned(Hbase[(i+3)*Hy_istride+6*j]);
            tmpC.LoadAligned(Hbase[(i+3)*Hy_istride+6*j+2]);
            tmpD.LoadAligned(Hbase[(i+3)*Hy_istride+6*j+4]);

            tmpA.StoreAligned(cbase[6*(i+2)+j*cjstride+4]);
            tmpB.StoreAligned(cbase[6*(i+3)+j*cjstride]);
            tmpC.StoreAligned(cbase[6*(i+3)+j*cjstride+2]);
            tmpD.StoreAligned(cbase[6*(i+3)+j*cjstride+4]);
#endif
          }
          StopTimer(10);
        }
        const OC_INDEX ifin=i;
        StartTimer(9);
        for(i=ifin;i<ispan; ++i) {
          ffty->InverseFFT(Hbase + i*Hy_istride);
        }
        StopTimer(9);
        StartTimer(10);
        for(j=0;j<rdimy;++j) {
          for(i=ifin;i<ispan; ++i) {
#if 0
            cbase[6*i+j*cjstride]   = Hbase[i*Hy_istride+6*j];
            cbase[6*i+j*cjstride+1] = Hbase[i*Hy_istride+6*j+1];
            cbase[6*i+j*cjstride+2] = Hbase[i*Hy_istride+6*j+2];
            cbase[6*i+j*cjstride+3] = Hbase[i*Hy_istride+6*j+3];
            cbase[6*i+j*cjstride+4] = Hbase[i*Hy_istride+6*j+4];
            cbase[6*i+j*cjstride+5] = Hbase[i*Hy_istride+6*j+5];
#else
            Oc_Duet tmpA, tmpB, tmpC, tmpD;
            tmpA.LoadAligned(Hbase[i*Hy_istride+6*j]);
            tmpB.LoadAligned(Hbase[i*Hy_istride+6*j+2]);
            tmpC.LoadAligned(Hbase[i*Hy_istride+6*j+4]);
            tmpA.StoreAligned(cbase[6*i+j*cjstride]);
            tmpB.StoreAligned(cbase[6*i+j*cjstride+2]);
            tmpC.StoreAligned(cbase[6*i+j*cjstride+4]);
#endif
          }
        } // j
        StopTimer(10);
      } // k
      StopTimer(8);
    } // for(i1)
  } // while(1)
  StopTimer(0);
}

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
    energy_density_error_estimate
      = 0.5*OC_REAL8m_EPSILON*MU0*state.max_absMs*state.max_absMs
      *(log(double(cdimx))+log(double(cdimy))+log(double(cdimz)))/log(2.);
  }
  oced.energy_density_error_estimate = energy_density_error_estimate;

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  assert(rdimx*rdimy*rdimz == Ms.Size());

  // Calculate forward x-axis FFTs
  {
    Hxfrm_base.SetSize(Hxfrm_kstride*rdimz);

#if REPORT_TIME
  fftxforwardtime.Start();
#endif // REPORT_TIME

    _Oxs_DemagFFTxThread fftx_thread;
    _Oxs_DemagFFTxThread::job_basket.Init(MaxThreadCount,
                                      Ms.GetArrayBlock(),Hxfrm_jstride);
    fftx_thread.spin = &spin;
    fftx_thread.Ms   = &Ms;
    fftx_thread.carr = Hxfrm_base.GetArrBase();
    fftx_thread.locker_info = &locker_info;

    threadtree.LaunchTree(fftx_thread,0);
#if REPORT_TIME
    fftxforwardtime.Stop();
#endif // REPORT_TIME
  }

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
  //
  // Calculate field components in frequency domain.  Make use of
  // realness and even/odd properties of interaction matrices Axx.
  // Note that in transform space only the x>=0 half-space is
  // stored.
  // Symmetries: A00, A11, A22 are even in each coordinate
  //             A01 is odd in x and y, even in z.
  //             A02 is odd in x and z, even in y.
  //             A12 is odd in y and z, even in x.
#if REPORT_TIME
  convtime.Start();
#endif // REPORT_TIME
  {
    assert(adimx>=cdimx);
    assert(cdimy-adimy<adimy);
    assert(cdimz-adimz<adimz);
    _Oxs_DemagFFTyzConvolveThread::job_basket.Init(MaxThreadCount,
                                                   &A,adimy*adimz);
    _Oxs_DemagFFTyzConvolveThread fftyzconv;
    fftyzconv.A_ptr = &A;
    fftyzconv.carr = Hxfrm_base.GetArrBase();
    fftyzconv.locker_info = &locker_info;
    fftyzconv.adimx=adimx; fftyzconv.adimy=adimy; fftyzconv.adimz=adimz;
    fftyzconv.thread_count = MaxThreadCount;
      
    threadtree.LaunchTree(fftyzconv,0);
  }
#if REPORT_TIME
  convtime.Stop();
#endif // REPORT_TIME

#if REPORT_TIME
  fftxinversetime.Start();
#endif // REPORT_TIME
  {
    _Oxs_DemagiFFTxDotThread ifftx_thread(MaxThreadCount);
    _Oxs_DemagiFFTxDotThread::job_basket.Init(MaxThreadCount,
                                              Ms.GetArrayBlock(),rdimx);
    ifftx_thread.carr = Hxfrm_base.GetArrBase();
    ifftx_thread.spin_ptr = &spin;
    ifftx_thread.Ms_ptr   = &Ms;
    ifftx_thread.oced_ptr = &oced;
    ifftx_thread.locker_info = &locker_info;
    threadtree.LaunchTree(ifftx_thread,0);

    Nb_Xpfloat tempsum = 0.0;
    for(OC_INDEX ithread=0;ithread<MaxThreadCount;++ithread) {
      tempsum += ifftx_thread.energy_sum[ithread];
    }
    oced.energy_sum = tempsum * state.mesh->Volume(0);
    /// All cells have same volume in an Oxs_RectangularMesh.
  }

#if REPORT_TIME
  fftxinversetime.Stop();
#endif // REPORT_TIME
}

#endif // OOMMF_THREADS
