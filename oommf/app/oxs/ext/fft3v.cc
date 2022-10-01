/* File: fft3v.cc
 *
 * Classes for real-to-complex and complex-to-real FFT of three
 * dimensional arrays of three vectors.
 *
 * For "standalone" testing, both this file and the header fft3v.h
 * are needed.  Then compile with a line like
 *
 *   g++ -O3 -DSTANDALONE fft3v.cc -o fft3vtest
 *
 * Refer to the "int main(int argc,char** argv)" section at the
 * bottom of this file for test details.
 */

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

#ifndef STANDALONE

#define OXS_FFT_TRY_XP 0
/// Optional; set to either 0 or 1

# include "nb.h"  // For constants PI and SQRT1_2
# include "oxsexcept.h" // OXS_THROW

#if OXS_FFT_TRY_XP
# include "xp.h"  // Double-double calculations
typedef Xp_DoubleDouble OXS_FFT_XP_TYPE;
# define OXS_FFT_XP_PI Xp_DoubleDouble::DD_PI
#endif

#else // STANDALONE
////////////////////////////////////////////////////////////////////////
// Lightweight versions of the OOMMF support library:
#include <cstdio>
#include <string>
#include <cstdarg>  // va_list and friends
#include <cstdlib>

using namespace std;

#define WIDE_PI  OC_REALWIDE(3.141592653589793238462643383279502884L)
#define	WIDE_SQRT1_2   OC_REALWIDE(0.7071067811865475244008443621048490393L)

// Xp double-double functions not ported to STANDALONE
#define OXS_FFT_TRY_XP 0

typedef int OC_INT4m;
typedef long OC_INDEX;
typedef unsigned long OC_UINDEX;
typedef double OC_REAL8m;
typedef double OC_REALWIDE;
typedef std::string String;

#define OXS_THROW(x,buf) throw(buf)

void
Oc_Snprintf(char *str, size_t n, const char *format, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr,format);
  vsnprintf(str,n,format,arg_ptr);
  va_end(arg_ptr);
}

void* Oc_AllocThreadLocal(size_t size)
{
  void* foo = malloc(size);
  if(!foo) { throw("Out of memory."); }
  return foo;
}
void Oc_FreeThreadLocal(void* start,size_t /* size */)
{
  if(!start) return; // Free of NULL pointer always allowed.
  free(start);
}

#endif // STANDALONE
////////////////////////////////////////////////////////////////////////

#include "fft3v.h"

/* End includes */

#define OXS_FFT_SQRT1_2 OXS_FFT_REAL_TYPE(WIDE_SQRT1_2)
/// Get the right width

// Size of threevector, in real units.  This macro is defined for code
// legibility and maintenance; it should always be "3".
#define OFTV_VECSIZE 3

// Size of complex value, in real units.  This macro is defined for code
// legibility and maintenance; it should always be "2".
#define OFTV_COMPLEXSIZE 2

// Macros used in Oxs_FFTStrided class.  Arrays are processed in pieces
// of size OFS_ARRAY_BLOCKSIZE (in complex units).  This significantly
// speeds up processing of wide arrays, presumably by increasing cache
// locality at the lower levels of FFT processing.
//   The OFS_ARRAY_MAXBLOCKSIZE is used to fit left-over array bits.
// At the array ends, the piece size is adjusted to match the full
// array size.  Narrow array passes aren't processed efficiently,
// padding up is preferred, as long as it doesn't go past
// OFS_ARRAY_MAXBLOCKSIZE.
// NOTE: For some reason, threaded code runs noticeable faster with
//  smaller OFS_ARRAY_BLOCKSIZE, at least on Intel Core2 Quad boxes.
//  Perhaps this has to do with interleaving memory access with
//  computation, at reduced memory bandwidth?
#ifndef OFS_ARRAY_BLOCKSIZE
# if OOMMF_THREADS
#  define OFS_ARRAY_BLOCKSIZE 8
# else
#  define OFS_ARRAY_BLOCKSIZE 32
# endif
#else
#endif // OFS_ARRAY_BLOCKSIZE

//
#ifndef OFS_ARRAY_MAXBLOCKSIZE
# if (OFS_ARRAY_BLOCKSIZE <= 24)
#  define OFS_ARRAY_MAXBLOCKSIZE (2*OFS_ARRAY_BLOCKSIZE)
# elif (OFS_ARRAY_BLOCKSIZE<48)
#  define OFS_ARRAY_MAXBLOCKSIZE 48
# else
#  define OFS_ARRAY_MAXBLOCKSIZE OFS_ARRAY_BLOCKSIZE
# endif
#endif

#if (OFS_ARRAY_MAXBLOCKSIZE < OFS_ARRAY_BLOCKSIZE)
error "OFS_ARRAY_MAXBLOCKSIZE < OFS_ARRAY_BLOCKSIZE"
#endif


#define TRY6 // For development purposes.  Select FFT size 8
/// code to use in Oxs_FFTStrided class.

////////////////////////////////////////////////////////////////////////
// Class Oxs_FFT1DThreeVector //////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

OC_INDEX Oxs_FFT1DThreeVector::GetNextPowerOfTwo(OC_INDEX n,OC_INT4m& logsize)
{ // Returns first power of two >= n
  // This is a private function, for use by power-of-two
  // code.  The client interface is RecommendSize(), which
  // at present just calls this routine, but may be adapted
  // in the future to a mixed-radix code.
  OC_INDEX m=1;
  logsize=0;
  while(m<n) {
    m*=2; ++logsize;
    if(m<1) {
      char msgbuf[1024];
      Oc_Snprintf(msgbuf,sizeof(msgbuf),
                  ": Import n=%ld too big",static_cast<long int>(n));
      String msg =
        String("OC_INDEX overflow in Oxs_FFT1DThreeVector::GetNextPowerOfTwo")
        + String(msgbuf);
      OXS_THROW(Oxs_BadParameter,msg);
    }
  }
  return m;
}

OC_INDEX Oxs_FFT1DThreeVector::RecommendSize(OC_INDEX size)
{ // Returns first power of two >= size
  OC_INT4m dummy;
  return GetNextPowerOfTwo(size,dummy);
}

void Oxs_FFT1DThreeVector::FreeMemory()
{
  if(UReals)         delete[] UReals;
  UReals=0;
  if(UForwardRadix4) delete[] UForwardRadix4;
  UForwardRadix4=0;
  if(ptsRadix4)      delete[] ptsRadix4;
  ptsRadix4=0;
  if(bitreverse)     delete[] bitreverse;
  bitreverse=0;
  if(scratch)        delete[] scratch;
  scratch=0;
#if 0
  if(workbuffer)     delete[] workbuffer;
  workbuffer=0;
#else
  if(workbuffer) {
    Oc_FreeThreadLocal(workbuffer,
                       sizeof(OXS_FFT_REAL_TYPE)*workbuffer_size);
  }
  workbuffer=0;
#endif
  scratch_size = 0;
  workbuffer_size = 0;
}

void Oxs_FFT1DThreeVector::FillRootsOfUnity()
{
  // NB: Any changes to memory size or layout in this routine
  // must be reflected in Oxs_FFT1DThreeVector::Dup().

  OC_INDEX i,j,k;

  if(UReals)         delete[] UReals;
  UReals=0;
  if(UForwardRadix4) delete[] UForwardRadix4;
  UForwardRadix4=0;

  if(fftsize<16) return; // Size 8 and smaller transforms
  // use hard-coded roots.

  UReals = new OXS_FFT_REAL_TYPE[2*fftsize];

  // Size of UForwardRadix4 array, in complex units, is
  //  fftsize - 3*n/2 - 10,     if log_2(fftsize) is even, or
  //  fftsize - 3*(n-1)/2 - 11, if log_2(fftsize) is odd
  // or, double that in real units

  //   If fftsize<=8, then both the complex transform and the unpacking
  // of the real transform use hard-coded roots.  For fftsize==16, the
  // complex transform uses hard-coded roots, but the real transform
  // uses the UReals array.  For fftsize>16, both UReals and
  // UForwardRadix4 arrays are used.
  OC_INDEX UfR4_csize = fftsize - 3*(log2fftsize/2)
    - 10 - (log2fftsize%2);
  if(fftsize>16) UForwardRadix4 = new OXS_FFT_REAL_TYPE[2*UfR4_csize];

  // Compute base roots for real transform.  These all lie in lower
  // half of complex plane, so imaginary parts are non-positive for
  // all roots.  To make code easier to follow, compute roots in
  // the first quadrant, and adjust signs as necessary.
#if OXS_FFT_TRY_XP
  OXS_FFT_XP_TYPE theta_real_base = OXS_FFT_XP_PI/OXS_FFT_XP_TYPE(fftsize);
#else
  OXS_FFT_REAL_TYPE theta_real_base = WIDE_PI/fftsize;
#endif
  for(i=1;i<fftsize/4;++i) {
#if OXS_FFT_TRY_XP
    OXS_FFT_XP_TYPE theta = i * theta_real_base;
    OXS_FFT_XP_TYPE ddst,ddct;   SinCos(theta,ddst,ddct);
    OXS_FFT_REAL_TYPE st,ct;
    ddst.DownConvert(st);   ddct.DownConvert(ct);
#else
    OXS_FFT_REAL_TYPE theta = i * theta_real_base;
    OXS_FFT_REAL_TYPE st = sin(theta);
    OXS_FFT_REAL_TYPE ct = cos(theta);
#endif
    UReals[2*i]               =    ct;
    UReals[2*i+1]             = -1*st;
    UReals[2*(fftsize/2-i)]   =    st;
    UReals[2*(fftsize/2-i)+1] = -1*ct;
    UReals[2*(fftsize/2+i)]   = -1*st;
    UReals[2*(fftsize/2+i)+1] = -1*ct;
    UReals[2*(fftsize-i)]     = -1*ct;
    UReals[2*(fftsize-i)+1]   = -1*st;
  }
  UReals[0]               =  1.0;
  UReals[1]               =  0.0;
  UReals[fftsize]         =  0.0;
  UReals[fftsize+1]       = -1.0;
  UReals[fftsize/2]       =    OXS_FFT_SQRT1_2;
  UReals[fftsize/2 + 1]   = -1*OXS_FFT_SQRT1_2;
  UReals[3*fftsize/2]     = -1*OXS_FFT_SQRT1_2;
  UReals[3*fftsize/2 + 1] = -1*OXS_FFT_SQRT1_2;

  if(fftsize<32) return;

  // Using values computed for UReals, fill in UForwardRadix4 array.
  // UForwardRadix4 is first filled with roots of unity down to and
  // including N=64 if log2fftsize if even, or N=128 if log2fftsize if
  // odd.
  j=0;
  for(i=1;i<=fftsize/(64*(1+log2fftsize%2));i*=4) {
    for(k=i;k<fftsize/4;k+=i) { // Don't store 1's (i.e., k==0)
      UForwardRadix4[j++] = UReals[8*k];     // Real part
      UForwardRadix4[j++] = UReals[8*k+1];   // Imaginary part
      UForwardRadix4[j++] = UReals[4*k];
      UForwardRadix4[j++] = UReals[4*k+1];
      if(6*k<fftsize) {
        UForwardRadix4[j++] = UReals[12*k];
        UForwardRadix4[j++] = UReals[12*k+1];
      } else {
        UForwardRadix4[j++] =    UReals[4*fftsize-12*k];
        UForwardRadix4[j++] = -1*UReals[4*fftsize-12*k+1];
      }
    }
  }
  if(log2fftsize%2) {
    // Append 32nd roots of unity sub-array
    OC_INDEX basestride32 = fftsize/8;
    for(k=1;k<16;++k) { // Don't store 1's (i.e., k==0)
      UForwardRadix4[j++] = UReals[k*basestride32];
      UForwardRadix4[j++] = UReals[k*basestride32+1];
    }
  }
  assert(j == 2*UfR4_csize);
}

void Oxs_FFT1DThreeVector::FillPreorderTraversalStateArray()
{ // See mjd's NOTES III, 30-June-2003, p78.
  if(ptsRadix4) delete[] ptsRadix4;
  ptsRadix4 = 0;

  if(fftsize<64) return; // ptsRadix4 array only used for
  /// complex FFT's of size 64 and larger.

  OC_INDEX ptsRadix4_size = fftsize/((1+log2fftsize%2)*64);
  ptsRadix4 = new PreorderTraversalState[ptsRadix4_size+1];
  /// Extra "+1" is to hold terminating 0 marker

  OC_INDEX URadix4_size = fftsize - 3*(log2fftsize/2) - 10
    - 16*(log2fftsize%2);
  // Radix4 portion of UForwardRadix4.
  URadix4_size *= 2; // Measure in terms of reals.

  OC_INDEX i,j;
  for(i=0;i<ptsRadix4_size;++i) {
    ptsRadix4[i].stride=32*(1+log2fftsize%2);
    ptsRadix4[i].uoff=4+log2fftsize%2; // Temporarily store
    /// log_2(stride/2), for use below in calculating uoff
  }
  for(j=4;j<=ptsRadix4_size;j*=4) {
    for(i=0;i<ptsRadix4_size;i+=j) {
      ptsRadix4[i].stride*=4;
      ptsRadix4[i].uoff+=2;
    }
  }
  for(i=0;i<ptsRadix4_size;++i) {
    OC_INDEX rs  = ptsRadix4[i].stride; // "real" (versus complex) stride
    ptsRadix4[i].stride *= OFTV_VECSIZE;
    OC_INDEX k = ptsRadix4[i].uoff;
    ptsRadix4[i].uoff = (2*fftsize + 6 + 3*k) - (4*rs + 3*log2fftsize);
    /// The 3*k term is because we don't store the k==0 terms in
    /// UForwardRadix4. (!?)
    assert(0<=ptsRadix4[i].uoff && ptsRadix4[i].uoff<URadix4_size-2);
  }
  ptsRadix4[ptsRadix4_size].stride=0; // End-of-array mark
  ptsRadix4[ptsRadix4_size].uoff=0;   // Safety
}

void Oxs_FFT1DThreeVector::FillBitReversalArray()
{
  if(bitreverse) delete[] bitreverse;
  bitreverse = 0;

  if(fftsize<32) return; // Bit reversal for fftsize<=16 is hard-coded.

  bitreverse = new OC_INDEX[fftsize];

  // The following code relies heavily on fftsize being a power-of-2
  bitreverse[0] = 0;

  // For sanity, work with unsigned types
  const OC_UINDEX ufftsize = static_cast<OC_UINDEX>(fftsize);
  OC_UINDEX k,n;
  OC_UINDEX mask = 0x0F;  mask = ~mask;
  for(k=1,n=ufftsize>>1;k<ufftsize;k++) {
    // At each step, n is bit-reversed pattern of k
#ifdef OLDE_BITREVERSE_CODE
    if((k&mask) == (n&mask) ) {
      // k and n are in same 16-block, so during final 16-fly
      // processing step both will be held in local variables
      // at the same time.  Therefore we don't need to swap
      // them, just write them directly into the appropriate
      // locations.  This can only happen if the number of
      // bits (log2fftsize) is 6 or less (i.e., fftsize<=64).
      bitreverse[k] = OFTV_VECSIZE*static_cast<OC_INDEX>(2*n);
    } else if(n<k) { // Swap
      bitreverse[k]= -1*OFTV_VECSIZE*static_cast<OC_INDEX>(2*n);
    } else { // k<n; No swap at i=k; hold for swap at i=n
      bitreverse[k] = OFTV_VECSIZE*static_cast<OC_INDEX>(2*k);
    }
#else // OLDE_BITREVERSE_CODE
    if((k&mask) == (n&mask) ) {
      // k and n are in same 16-block, so during final 16-fly
      // processing step both will be held in local variables
      // at the same time.  Therefore we don't need to swap
      // them, just write them directly into the appropriate
      // locations.  This can only happen if the number of
      // bits (log2fftsize) is 6 or less (i.e., fftsize<=64).
      bitreverse[k] = -1*OFTV_VECSIZE*static_cast<OC_INDEX>(2*n);
    } else if(n<k) { // Swap
      bitreverse[k]= OFTV_VECSIZE*static_cast<OC_INDEX>(2*n);
    } else { // k<n; No swap at i=k; hold for swap at i=n
      bitreverse[k] = 0;
    }
#endif // OLDE_BITREVERSE_CODE

    // Calculate next n.  We do this by manually adding 1 to the
    // leftmost bit, and carrying to the right.
    OC_UINDEX m = ufftsize>>1;
    while(m>0 && n&m) { n-=m; m>>=1; }
    n+=m;
  }
}

void
Oxs_FFT1DThreeVector::AllocScratchSpace(OC_INDEX size)
{ // Allocate scratch space, and initialize to zero.
  if(scratch) delete[] scratch;
  scratch=0;
  scratch_size = 0;
  if(size>0) {
    scratch_size = size;
    scratch = new OXS_FFT_REAL_TYPE[size];
    for(OC_INDEX i=0;i<size;++i) scratch[i] = 0.0;
  }
}

Oxs_FFT1DThreeVector::Oxs_FFT1DThreeVector() :
  ForwardPtr(0),InversePtr(0),         // Dummy initial values
  arrcount(0), rsize(0), rstride(0), fftsize(0),
  log2fftsize(-1),
  UReals(0),UForwardRadix4(0),
  ptsRadix4(0),bitreverse(0),
  scratch_size(0),scratch(0),
  workbuffer_size(0),workbuffer(0)
{}

void Oxs_FFT1DThreeVector::Dup(const Oxs_FFT1DThreeVector& other)
{
  OC_INDEX i;
  FreeMemory();

  arrcount     = other.arrcount;
  rsize        = other.rsize;
  rstride      = other.rstride;
  fftsize      = other.fftsize;
  log2fftsize  = other.log2fftsize;

  if(other.UReals != 0) {
    UReals = new OXS_FFT_REAL_TYPE[2*fftsize];
    for(i=0;i<2*fftsize;++i) UReals[i] = other.UReals[i];
  }

  if(other.UForwardRadix4 != 0) {
    OC_INDEX UfR4_csize = fftsize - 3*(log2fftsize/2)
      - 10 - (log2fftsize%2);
    OC_INDEX ufrsize = 2*UfR4_csize;
    UForwardRadix4 = new OXS_FFT_REAL_TYPE[ufrsize];
    for(i=0;i<ufrsize;++i) UForwardRadix4[i] = other.UForwardRadix4[i];
  }

  if(other.ptsRadix4 != 0) {
    OC_INDEX ptsRadix4_size = fftsize/((1+log2fftsize%2)*64);
    OC_INDEX pr4size = ptsRadix4_size+1;
    ptsRadix4 = new PreorderTraversalState[pr4size];
    for(i=0;i<pr4size;++i)  ptsRadix4[i] = other.ptsRadix4[i];
  }

  if(other.bitreverse !=0) {
    bitreverse = new OC_INDEX[fftsize];
    for(i=0;i<fftsize;++i) bitreverse[i] = other.bitreverse[i];
  }

  // An alternative way to set Forward/InversePtr and scratch
  // is to call AssignTransformPointers.
  ForwardPtr = other.ForwardPtr;
  InversePtr = other.InversePtr;

  if(other.scratch !=0 && other.scratch_size>0) {
    scratch_size = other.scratch_size;
    scratch = new OXS_FFT_REAL_TYPE[scratch_size];
    for(i=0;i<scratch_size;++i) scratch[i] = other.scratch[i];
  }

  if(other.workbuffer != 0) {
    workbuffer_size = other.workbuffer_size;
#if 0
    workbuffer = new OXS_FFT_REAL_TYPE[workbuffer_size];
#else
    workbuffer = static_cast<OXS_FFT_REAL_TYPE*>
      (Oc_AllocThreadLocal(sizeof(OXS_FFT_REAL_TYPE)*workbuffer_size));
#endif
    for(i=0;i<workbuffer_size;++i) workbuffer[i] = other.workbuffer[i];
  }

}


void Oxs_FFT1DThreeVector::AssignTransformPointers()
{
  if(scratch) delete[] scratch;
  scratch=0;  scratch_size = 0;
  if(workbuffer) {
#if 0
    delete[] workbuffer;
#else
    Oc_FreeThreadLocal(workbuffer,sizeof(OXS_FFT_REAL_TYPE)*workbuffer_size);
#endif
  }
  workbuffer=0;  workbuffer_size=0;

  // Assign Forward/Inverse routine pointers
  switch(log2fftsize) {
    case -1: // Special case; rsize==csize==1
      ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize0;
      InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize0;
      break;
    case 0: // csize=2
      ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize1;
      InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize1;
      break;
    case 1:
      AllocScratchSpace(2*2*OFTV_VECSIZE);
      ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize2;
      InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize2;
      break;
    case 2:
      AllocScratchSpace(4*2*OFTV_VECSIZE);
      if(rsize>fftsize) {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize4;
      } else {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize4ZP;
        // This "ZP" form is provided, but doesn't appear to improve
        // speed over plain variant.  Presumably at this size the code is
        // completely limited by memory access.
      }
      InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize4;
      break;
    case 3:
      // The Size8 codes include built-in "ZP" handling
      ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize8;
      InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize8;
      break;
    case 4:
      if(rsize>fftsize) {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize16;
        InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize16;
      } else {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize16ZP;
        InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize16ZP;
      }
      break;
    case 5:
      AllocScratchSpace(16*2*OFTV_VECSIZE);
      if(rsize>fftsize) {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize32;
        InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize32;
      } else {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize32ZP;
        InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize32ZP;
      }
      break;
    case 6:
      AllocScratchSpace(16*2*OFTV_VECSIZE);
      if(rsize>fftsize) {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize64;
        InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize64;
      } else {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize64ZP;
        InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize64ZP;
      }
      break;
    default:
      AllocScratchSpace(16*2*OFTV_VECSIZE);
      if(rsize>fftsize) {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTRadix4;
        InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTRadix4;
      } else {
        ForwardPtr = &Oxs_FFT1DThreeVector::ForwardRealToComplexFFTRadix4ZP;
        InversePtr = &Oxs_FFT1DThreeVector::InverseComplexToRealFFTRadix4ZP;
      }
      break;
  }

  workbuffer_size = OFTV_VECSIZE*(fftsize+1)*OFTV_COMPLEXSIZE;
#if 0
  workbuffer = new OXS_FFT_REAL_TYPE[workbuffer_size];
#else
  workbuffer = static_cast<OXS_FFT_REAL_TYPE*>
    (Oc_AllocThreadLocal(sizeof(OXS_FFT_REAL_TYPE)*workbuffer_size));
#endif

}

void Oxs_FFT1DThreeVector::SetDimensions
(OC_INDEX import_rsize,
 OC_INDEX import_csize,
 OC_INDEX import_array_count)
{
  FreeMemory();

  rsize = import_rsize;
  fftsize = import_csize/2;
  arrcount = import_array_count;
  rstride = OFTV_VECSIZE*import_rsize;

  if(import_csize==1) {
    log2fftsize=-1; // Coded value
  } else {
    OC_INDEX checksize = GetNextPowerOfTwo(fftsize,log2fftsize);
    if(2*fftsize!=import_csize || fftsize!=checksize) {
      OXS_THROW(Oxs_BadParameter,
	"Illegal csize import to Oxs_FFT1DThreeVector::SetDimensions().");
    }
  }

  if(import_csize<import_rsize) {
    OXS_THROW(Oxs_BadParameter,
      "Invalid Oxs_FFT1DThreeVector::SetDimensions() call: csize<rsize.");
  }
  if(import_rsize<1 || import_csize<1 || import_array_count<1) {
    OXS_THROW(Oxs_BadParameter,
	      "Illegal import to Oxs_FFT1DThreeVector::SetDimensions().");
  }

  FillRootsOfUnity();
  FillPreorderTraversalStateArray();
  FillBitReversalArray();

  AssignTransformPointers();
}

void
Oxs_FFT1DThreeVector::AdjustInputDimensions
(OC_INDEX new_rsize,
 OC_INDEX new_array_count)
{
  OC_INDEX csize = (fftsize>0 ? 2*fftsize : 1);
  if(new_rsize<1 || new_array_count<1) {
    OXS_THROW(Oxs_BadParameter,
      "Illegal import to Oxs_FFT1DThreeVector::AdjustInputDimensions().");
  }
  if(new_rsize>csize) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
	"Invalid Oxs_FFT1DThreeVector::AdjustInputDimensions() call:"
	" new_rsize=%d > csize=%d.",new_rsize,csize);
    OXS_THROW(Oxs_BadParameter,buf);
  }
  rsize = new_rsize;
  arrcount = new_array_count;
  rstride = OFTV_VECSIZE*new_rsize;
  AssignTransformPointers(); // Transform size doesn't change,
  /// but *ZP code selection might.
}

Oxs_FFT1DThreeVector::~Oxs_FFT1DThreeVector()
{
  FreeMemory();
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTRadix4
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{ // NOTE: The bit-reversal code in this routine
  // is only valid for fftsize>=128.
  // The following Oc_Nop is a workaround for an optimization bug in the
  // version of gcc 4.2.1 that ships alongside Mac OS X Lion.
  const OC_INDEX block32_count = Oc_Nop(4*(log2fftsize%2));
  const OC_INDEX block16_count = block32_count + 4;
  const OC_INDEX cstride = 2*(fftsize+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {
    {
      // Copy current row of rarr_in to carr_out, with zero padding.
      // NOTE: It is probably possible to speed computation by a
      // few percent by embedding this processing with the first
      // radix-4 pass.
      OC_INDEX i;
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      if(mult_base!=NULL) {
        const OXS_FFT_REAL_TYPE* mult = mult_base;
        mult_base += rstride/OFTV_VECSIZE;
        for(i=0;i<istop-5;i+=6) {
          carr_out[i]   = (*mult)*rarr_in[i];
          carr_out[i+2] = (*mult)*rarr_in[i+1];
          carr_out[i+4] = (*mult)*rarr_in[i+2];
          ++mult;
          carr_out[i+1] = (*mult)*rarr_in[i+3];
          carr_out[i+3] = (*mult)*rarr_in[i+4];
          carr_out[i+5] = (*mult)*rarr_in[i+5];
          ++mult;
        }
        if(i<istop) {
          carr_out[i]   = (*mult)*rarr_in[i];
          carr_out[i+1] = 0.0;
          carr_out[i+2] = (*mult)*rarr_in[i+1];
          carr_out[i+3] = 0.0;
          carr_out[i+4] = (*mult)*rarr_in[i+2];
          carr_out[i+5] = 0.0;
          i += 6;
        }
      } else {
        for(i=0;i<istop-5;i+=6) {
          carr_out[i]   = rarr_in[i];
          carr_out[i+2] = rarr_in[i+1];
          carr_out[i+4] = rarr_in[i+2];
          carr_out[i+1] = rarr_in[i+3];
          carr_out[i+3] = rarr_in[i+4];
          carr_out[i+5] = rarr_in[i+5];
        }
        if(i<istop) {
          carr_out[i]   = rarr_in[i];
          carr_out[i+1] = 0.0;
          carr_out[i+2] = rarr_in[i+1];
          carr_out[i+3] = 0.0;
          carr_out[i+4] = rarr_in[i+2];
          carr_out[i+5] = 0.0;
          i += 6;
        }
      }
      while(i<6*fftsize) carr_out[i++] = 0.0;
    }
    OXS_FFT_REAL_TYPE* v = carr_out; // Working vector.

    // Do power-of-4 blocks, with preorder traversal/tree walk
    OC_INDEX offset = 0;
    const PreorderTraversalState* sptr = ptsRadix4;
    do {
      // Stride is measured in units of OXS_FFT_REAL_TYPE's.
      OC_INDEX stride = sptr->stride;
      OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;
      do {
        // Do power-of-4 butterflies with "stride" at position v + 3*offset
        OXS_FFT_REAL_TYPE *const va = v + OFTV_VECSIZE*offset;
        OXS_FFT_REAL_TYPE *const vb = va + stride;
        OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
        OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
        OC_INDEX i;
        for(i=0;i<6;i+=2) {
          // w^0
          OXS_FFT_REAL_TYPE x0 = va[i];
          OXS_FFT_REAL_TYPE y0 = va[i+1];
          OXS_FFT_REAL_TYPE x1 = vb[i];
          OXS_FFT_REAL_TYPE y1 = vb[i+1];
          OXS_FFT_REAL_TYPE x2 = vc[i];
          OXS_FFT_REAL_TYPE x3 = vd[i];

          OXS_FFT_REAL_TYPE y2 = vc[i+1];

          OXS_FFT_REAL_TYPE sx1 = x0 + x2;
          OXS_FFT_REAL_TYPE dx1 = x0 - x2;

          OXS_FFT_REAL_TYPE sx2 = x1 + x3;
          OXS_FFT_REAL_TYPE dx2 = x1 - x3;

          OXS_FFT_REAL_TYPE y3 = vd[i+1];

          OXS_FFT_REAL_TYPE sy1 = y0 + y2;
          OXS_FFT_REAL_TYPE dy1 = y0 - y2;
          OXS_FFT_REAL_TYPE sy2 = y1 + y3;
          OXS_FFT_REAL_TYPE dy2 = y1 - y3;

          va[i]   = sx1 + sx2;
          va[i+1] = sy1 + sy2;
          vb[i]   = sx1 - sx2;
          vb[i+1] = sy1 - sy2;
          vc[i]   = dx1 + dy2;
          vc[i+1] = dy1 - dx2;
          vd[i]   = dx1 - dy2;
          vd[i+1] = dy1 + dx2;
        }
        while(i<stride) {
          for(OC_INDEX j=i;j-i<6;j+=2) {
            OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
            OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
            OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
            OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
            OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
            OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

            OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
            OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
            dx1 -= x2;    // dx1 := R0
#define dx2 x1
            dx2 -= x3;    // dx2 := R1

#define y2 x2
            y2 = vc[j+1]; // y2 := R2
#define y3 x3
            y3 = vd[j+1]; // y3 := R3

            va[j]              = sx1 + sx2;
#define txa sx1
            txa -= sx2;     // txa := R4

#define sy1 sx2
            sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
            dy1 -= y2;      // dy1 := R6
            OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
            dy2 -= y3;      // dy2 := R7

#define mx1 y3
            mx1 = U[0];   // mx1 := R3

            va[j+1]              = sy1 + sy2;
#define tya sy1
            tya -= sy2;     // tya := R5

#define mx1_txa y2
            mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
            mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
            my1 = U[1];    // my1 := R8

#define my1_tya tya
            my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
            my1_txa *= my1; // my1_txa := R4

            vb[j]     = mx1_txa - my1_tya;
            vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
            txb = dx1 + dy2;  // txb := R2
#define txc dx1
            txc -= dy2;       // txc := R0
#define tyb mx1_tya
            tyb = dy1 - dx2;  // tyb := R3
#define tyc dy1
            tyc += dx2;       // tyc := R6

#define mx2 my1_txa
            mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
            mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
            mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
            my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
            my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
            my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
            Cx -= my2_tyb; // Cx := R1
            vc[j]   = Cx;

#define mx3 mx2
            mx3 = U[4];  // mx3 := R4
#define my3 my2
            my3 = U[5];  // my3 := R5

            OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
            OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
            OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
            OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
            OXS_FFT_REAL_TYPE Cy = mx2_tyb + my2_txb; // Cy := R2
            OXS_FFT_REAL_TYPE Dx = mx3_txc - my3_tyc;
            OXS_FFT_REAL_TYPE Dy = my3_txc + mx3_tyc;

            vc[j+1] = Cy;
            vd[j]   = Dx;
            vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
          }
          i+=6;
          U+=6;
        }
        stride /= 4;
      } while(stride>48);
      if(block32_count>0) {
        { // i=0
          OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*offset;
          for(OC_INDEX j1=0 ; j1<OFTV_VECSIZE*256 ; j1+=OFTV_VECSIZE*64) {
            for(OC_INDEX j2=j1 ; j2<j1+2*OFTV_VECSIZE ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+OFTV_VECSIZE*16];
              OXS_FFT_REAL_TYPE cy0 = va[j2+OFTV_VECSIZE*16+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+OFTV_VECSIZE*32];
              OXS_FFT_REAL_TYPE ay1 = va[j2+OFTV_VECSIZE*32+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+OFTV_VECSIZE*48];
              OXS_FFT_REAL_TYPE cy1 = va[j2+OFTV_VECSIZE*48+1];
              va[j2]                    = ax0 + ax1;
              va[j2+1]                  = ay0 + ay1;
              va[j2+OFTV_VECSIZE*32]   = ax0 - ax1;
              va[j2+OFTV_VECSIZE*32+1] = ay0 - ay1;

              va[j2+OFTV_VECSIZE*16]   = cx0 + cx1;
              va[j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
              va[j2+OFTV_VECSIZE*48]   = cy0 - cy1;
              va[j2+OFTV_VECSIZE*48+1] = cx1 - cx0;
            }
          }
        }
        // NB: U currently points to the tail of the UForwardRadix4 array,
        // which is a subsegment holding the 32nd roots of unity.  That
        // subsegment doesn't contain the (1,0) root, so the access below
        // is to U - 2.
        for(OC_INDEX i=2;i<16;i+=2) {
          OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*(offset + i);
          const OXS_FFT_REAL_TYPE amx = U[i-2];
          const OXS_FFT_REAL_TYPE amy = U[i-1];
          for(OC_INDEX j1=0;j1<OFTV_VECSIZE*256;j1+=OFTV_VECSIZE*64) {
            for(OC_INDEX j2=j1 ; j2<j1+2*OFTV_VECSIZE ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+OFTV_VECSIZE*16];
              OXS_FFT_REAL_TYPE cy0 = va[j2+OFTV_VECSIZE*16+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+OFTV_VECSIZE*32];
              OXS_FFT_REAL_TYPE ay1 = va[j2+OFTV_VECSIZE*32+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+OFTV_VECSIZE*48];
              OXS_FFT_REAL_TYPE cy1 = va[j2+OFTV_VECSIZE*48+1];

              OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
              va[j2]        = ax0 + ax1;
              va[j2+1]      = ay0 + ay1;
              OXS_FFT_REAL_TYPE adify = ay0 - ay1;

              va[j2+OFTV_VECSIZE*32]   = amx*adifx - amy*adify;
              va[j2+OFTV_VECSIZE*32+1] = amx*adify + amy*adifx;

              OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
              va[j2+OFTV_VECSIZE*16]   = cx0 + cx1;
              va[j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
              OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

              va[j2+OFTV_VECSIZE*48]   = amx*cdify + amy*cdifx;
              va[j2+OFTV_VECSIZE*48+1] = amy*cdify - amx*cdifx;
            }
          }
        }
      }

      OC_INDEX i = offset/2;
      OC_INDEX k=block16_count;
      do {
        // Bottom level 16-passes, with built-in bit-flip shuffling
        const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
        const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

        OXS_FFT_REAL_TYPE* const bv0 = v + 6*i;

        OC_INDEX j;

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE a0x = bv0[j];
          OXS_FFT_REAL_TYPE a0y = bv0[j+1];
          OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
          OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

          OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
          OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
          OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
          OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

          OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
          OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
          OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
          OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

          OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
          OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
          OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
          OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

          scratch[j]        = aS0x + aS1x;
          scratch[j+1]      = aS0y + aS1y;
          scratch[j+3*8]    = aS0x - aS1x;
          scratch[j+1+3*8]  = aS0y - aS1y;

          scratch[j+3*16]   = aD0x + aD1y;
          scratch[j+1+3*16] = aD0y - aD1x;
          scratch[j+3*24]   = aD0x - aD1y;
          scratch[j+1+3*24] = aD0y + aD1x;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
          OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
          OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
          OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

          OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
          OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
          OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
          OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

          OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
          OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
          OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
          OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

          OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
          OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
          OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
          OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

          OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
          scratch[j+3*2]    = bS0x + bS1x;
          scratch[j+1+3*2]  = bS0y + bS1y;
          OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

          scratch[j+3*10]   = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*10] = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
          OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
          OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
          OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

          scratch[j+3*18]   = tUb2x*alphax + tUb2y*alphay;
          scratch[j+1+3*18] = tUb2y*alphax - tUb2x*alphay;
          scratch[j+3*26]   = tUb3x*alphay + tUb3y*alphax;
          scratch[j+1+3*26] = tUb3y*alphay - tUb3x*alphax;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
          OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
          OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
          OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

          OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
          OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
          OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
          OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

          OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
          OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
          OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
          OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

          OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
          OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
          OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
          OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

          OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
          OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
          OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
          OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

          scratch[j+3*4]    = cS1x + cS0x;
          scratch[j+1+3*4]  = cS0y + cS1y;
          scratch[j+3*12]   = cS0y - cS1y;
          scratch[j+1+3*12] = cS1x - cS0x;

          scratch[j+3*20]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*20] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j+3*28]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*28] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
          OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
          OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
          OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

          OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
          OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
          OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
          OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

          OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
          OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
          OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
          OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

          OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
          OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
          OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
          OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

          OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
          scratch[j+3*6]    = dS1x + dS0x;
          scratch[j+1+3*6]  = dS0y + dS1y;
          OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

          scratch[j+3*14]   = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*14] = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
          OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
          OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
          OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

          scratch[j+3*22]   = tUd2x*alphay + tUd2y*alphax;
          scratch[j+1+3*22] = tUd2y*alphay - tUd2x*alphax;
          scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
          scratch[j+1+3*30] = tUd3y*alphax - tUd3x*alphay;
        }

        // Bit reversal
        OC_INDEX ja;
        OXS_FFT_REAL_TYPE* w[16];
        for(ja=0;ja<16;++ja) {
          const OC_INDEX bs = 2*OFTV_VECSIZE;
          w[ja] = bv0+bs*ja;
          if(bitreverse[i+ja]>0) {
            // Swap
            memcpy(bv0+bs*ja,v+bitreverse[i+ja],bs*sizeof(OXS_FFT_REAL_TYPE));
            w[ja] = v+bitreverse[i+ja];
          } // Else, no swap.
        }

        // Do 4 lower (bottom/final) -level dragonflies
        for(ja=0;ja<16;ja+=4) {
          OC_INDEX off = 6*ja;
          OXS_FFT_REAL_TYPE const * const sv = scratch + off;
          for(OC_INDEX jb=0;jb<6;jb+=2) {
            OXS_FFT_REAL_TYPE Uax = sv[jb];
            OXS_FFT_REAL_TYPE Uay = sv[jb+1];
            OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
            OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

            OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
            OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
            OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
            OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

            OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
            OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
            OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
            OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

            OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
            OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
            OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
            OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

            *(w[ja]+jb)     = BaSx + BbSx;
            *(w[ja]+jb+1)   = BaSy + BbSy;
            *(w[ja+1]+jb)   = BaSx - BbSx;
            *(w[ja+1]+jb+1) = BaSy - BbSy;

            *(w[ja+2]+jb)   = BaDx + BbDy;
            *(w[ja+2]+jb+1) = BaDy - BbDx;
            *(w[ja+3]+jb)   = BaDx - BbDy;
            *(w[ja+3]+jb+1) = BaDy + BbDx;
          }
        }

        i += 16;
      } while((--k)>0);
      offset = 2*i;
      ++sptr;
    } while(sptr->stride>0);

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      v[2*OFTV_VECSIZE*fftsize+j]   = v[j] - v[j+1];
      v[2*OFTV_VECSIZE*fftsize+1+j] = 0.0;
      v[j] += v[j+1];
      v[j+1] = 0.0;
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*fftsize;
    for(OC_INDEX i=2;i<fftsize;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = 0.5*UReals[i];
      const OXS_FFT_REAL_TYPE wy = 0.5*UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = 0.5*(Ax + Bx);
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = 0.5*(Ay - By);
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+j]   = Sx + C1;
        v[k1+j+1] = C2 + Dy;
        v[k2+j]   = Sx - C1;
        v[k2+j+1] = C2 - Dy;
      }
    }
    v[OFTV_VECSIZE*fftsize+1] *= -1;
    v[OFTV_VECSIZE*fftsize+3] *= -1;
    v[OFTV_VECSIZE*fftsize+5] *= -1;
  }
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTRadix4
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{ // NOTE: The bit-reversal code in this routine
  // is only valid for fftsize>=128.
  // The following Oc_Nop is a workaround for an optimization bug in the
  // version of gcc 4.2.1 that ships alongside Mac OS X Lion.
  const OC_INDEX block32_count = Oc_Nop(4*(log2fftsize%2));
  const OC_INDEX block16_count = block32_count + 4;
  const OC_INDEX cstride = 2*(fftsize+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {
    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = v[j];
      OXS_FFT_REAL_TYPE tmpBx0 = v[2*OFTV_VECSIZE*fftsize+j];
      v[j]   = 0.5*(tmpAx0 + tmpBx0);
      v[j+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[j+1] and v[2*OFTV_VECSIZE*fftsize+j+1] = 0.0
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*fftsize;
    for(OC_INDEX i=2;i<fftsize;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = 0.5*UReals[i];
      const OXS_FFT_REAL_TYPE wy = 0.5*UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = 0.5*(Ax + Bx);
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = 0.5*(Ay - By);
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = Sx + C1;
        v[k1+j+1] = C2 + Dy;
        v[k2+j]   = Sx - C1;
        v[k2+j+1] = C2 - Dy;
      }
    }
    v[OFTV_VECSIZE*fftsize+1] *= -1;
    v[OFTV_VECSIZE*fftsize+3] *= -1;
    v[OFTV_VECSIZE*fftsize+5] *= -1;

    // Do power-of-4 blocks, with preorder traversal/tree walk
    OC_INDEX offset = 0;
    const PreorderTraversalState* sptr = ptsRadix4;
    do {
      // Stride is measured in units of OXS_FFT_REAL_TYPE's.
      OC_INDEX stride = sptr->stride;
      OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;
      // Note: The inverse roots are the complex conjugates of the
      // forward roots.
      do {
        // Do power-of-4 butterflies with "stride" at position v + 3*offset
        OXS_FFT_REAL_TYPE *const va = v + OFTV_VECSIZE*offset;
        OXS_FFT_REAL_TYPE *const vb = va + stride;
        OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
        OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
        OC_INDEX i;
        for(i=0;i<6;i+=2) {
          // w^0
          OXS_FFT_REAL_TYPE x0 = va[i];  OXS_FFT_REAL_TYPE y0 = va[i+1];
          OXS_FFT_REAL_TYPE x1 = vb[i];  OXS_FFT_REAL_TYPE y1 = vb[i+1];
          OXS_FFT_REAL_TYPE x2 = vc[i];
          OXS_FFT_REAL_TYPE x3 = vd[i];

          OXS_FFT_REAL_TYPE y2 = vc[i+1];

          OXS_FFT_REAL_TYPE sx1 = x0 + x2;
          OXS_FFT_REAL_TYPE dx1 = x0 - x2;

          OXS_FFT_REAL_TYPE sx2 = x1 + x3;
          OXS_FFT_REAL_TYPE dx2 = x1 - x3;

          OXS_FFT_REAL_TYPE y3 = vd[i+1];

          OXS_FFT_REAL_TYPE sy1 = y0 + y2;
          OXS_FFT_REAL_TYPE dy1 = y0 - y2;
          OXS_FFT_REAL_TYPE sy2 = y1 + y3;
          OXS_FFT_REAL_TYPE dy2 = y1 - y3;

          va[i] = sx1 + sx2;  va[i+1] = sy1 + sy2;
          vb[i] = sx1 - sx2;  vb[i+1] = sy1 - sy2;
          vc[i] = dx1 - dy2;  vc[i+1] = dy1 + dx2;
          vd[i] = dx1 + dy2;  vd[i+1] = dy1 - dx2;
        }
        while(i<stride) {
          for(OC_INDEX j=i;j-i<6;j+=2) {
            OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
            OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
            OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
            OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
            OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
            OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

            OXS_FFT_REAL_TYPE sx1 = x0 + x2; // sx1 := R4
            OXS_FFT_REAL_TYPE sx2 = x1 + x3; // sx2 := R5
#define dx1 x0
            dx1 -= x2;    // dx1 := R0
#define dx2 x1
            dx2 -= x3;    // dx2 := R1

#define y2 x2
            y2 = vc[j+1]; // y2 := R2
#define y3 x3
            y3 = vd[j+1]; // y3 := R3

            va[j]              = sx1 + sx2;
#define txa sx1
            txa -= sx2;     // txa := R4

#define sy1 sx2
            sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
            dy1 -= y2;      // dy1 := R6
            OXS_FFT_REAL_TYPE sy2 = y1 + y3; // sy2 := R8
#define dy2 y1
            dy2 -= y3;      // dy2 := R7

#define mx1 y3
            mx1 = U[0];   // mx1 := R3

            va[j+1]              = sy1 + sy2;
#define tya sy1
            tya -= sy2;     // tya := R5

#define mx1_txa y2
            mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
            mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
            my1 = U[1];    // my1 := R8

#define my1_tya tya
            my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
            my1_txa *= my1; // my1_txa := R4

            vb[j]     = mx1_txa + my1_tya;
            vb[j+1]   = mx1_tya - my1_txa;

#define txb mx1_txa
            txb = dx1 - dy2;  // txb := R2
#define txc dx1
            txc += dy2;       // txc := R0
#define tyb mx1_tya
            tyb = dy1 + dx2;  // tyb := R3
#define tyc dy1
            tyc -= dx2;       // tyc := R6

#define mx2 my1_txa
            mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
            mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
            mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
            my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
            my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
            my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
            Cx += my2_tyb; // Cx := R1
            vc[j]   = Cx;

#define mx3 mx2
            mx3 = U[4];  // mx3 := R4
#define my3 my2
            my3 = U[5];  // my3 := R5

            OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
            OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
            OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
            OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
            OXS_FFT_REAL_TYPE Cy = mx2_tyb - my2_txb; // Cy := R2
            OXS_FFT_REAL_TYPE Dx = mx3_txc + my3_tyc;
            OXS_FFT_REAL_TYPE Dy = mx3_tyc - my3_txc;

            vc[j+1] = Cy;
            vd[j]   = Dx;
            vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
          }
          i+=6;
          U+=6;
        }
        stride /= 4;
      } while(stride>48);
      if(block32_count>0) {
        { // i=0
          OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*offset;
          for(OC_INDEX j1=0 ; j1<OFTV_VECSIZE*256 ; j1+=OFTV_VECSIZE*64) {
            for(OC_INDEX j2=0 ; j2<2*OFTV_VECSIZE ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j1+j2];
              OXS_FFT_REAL_TYPE ay0 = va[j1+j2+1];
              OXS_FFT_REAL_TYPE ax1 = va[j1+j2+OFTV_VECSIZE*32];
              OXS_FFT_REAL_TYPE ay1 = va[j1+j2+OFTV_VECSIZE*32+1];

              va[j1+j2]                    = ax0 + ax1;
              va[j1+j2+1]                  = ay0 + ay1;
              va[j1+j2+OFTV_VECSIZE*32]   = ax0 - ax1;
              va[j1+j2+OFTV_VECSIZE*32+1] = ay0 - ay1;

              OXS_FFT_REAL_TYPE cx0 = va[j1+j2+OFTV_VECSIZE*16];
              OXS_FFT_REAL_TYPE cy0 = va[j1+j2+OFTV_VECSIZE*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j1+j2+OFTV_VECSIZE*48];
              OXS_FFT_REAL_TYPE cy1 = va[j1+j2+OFTV_VECSIZE*48+1];

              va[j1+j2+OFTV_VECSIZE*16]   = cx0 + cx1;
              va[j1+j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
              va[j1+j2+OFTV_VECSIZE*48]   = cy1 - cy0;
              va[j1+j2+OFTV_VECSIZE*48+1] = cx0 - cx1;
            }
          }
        }
        // NB: U currently points to the tail of the UForwardRadix4 array,
        // which is a subsegment holding the 32nd roots of unity.  That
        // subsegment doesn't contain the (1,0) root, so the access below
        // is to U - 2.  Again, the inverse roots are the complex conjugates
        // of the forward roots.
        for(OC_INDEX i=2;i<16;i+=2) {
          OXS_FFT_REAL_TYPE* const va
            = v + OFTV_VECSIZE*offset + OFTV_VECSIZE*i;
          const OXS_FFT_REAL_TYPE amx = U[i-2];
          const OXS_FFT_REAL_TYPE amy = U[i-1];
          for(OC_INDEX j1=0;j1<OFTV_VECSIZE*256;j1+=OFTV_VECSIZE*64) {
            for(OC_INDEX j2=j1 ; j2<j1+6 ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+OFTV_VECSIZE*16];
              OXS_FFT_REAL_TYPE cy0 = va[j2+OFTV_VECSIZE*16+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+OFTV_VECSIZE*32];
              OXS_FFT_REAL_TYPE ay1 = va[j2+OFTV_VECSIZE*32+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+OFTV_VECSIZE*48];
              OXS_FFT_REAL_TYPE cy1 = va[j2+OFTV_VECSIZE*48+1];

              OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
              va[j2]        = ax0 + ax1;
              va[j2+1]      = ay0 + ay1;
              OXS_FFT_REAL_TYPE adify = ay0 - ay1;

              va[j2+OFTV_VECSIZE*32]   = amx*adifx + amy*adify;
              va[j2+OFTV_VECSIZE*32+1] = amx*adify - amy*adifx;

              OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
              va[j2+OFTV_VECSIZE*16]   = cx0 + cx1;
              va[j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
              OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

              va[j2+OFTV_VECSIZE*48]   = amy*cdifx - amx*cdify;
              va[j2+OFTV_VECSIZE*48+1] = amy*cdify + amx*cdifx;
            }
          }
        }
      }

      OC_INDEX i = offset/2;
      OC_INDEX k=block16_count;
      do {
        // Bottom level 16-passes, with built-in bit-flip shuffling
        const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
        const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

        OXS_FFT_REAL_TYPE* const bv0 = v + 6*i;

        OC_INDEX j;

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE a0x = bv0[j];
          OXS_FFT_REAL_TYPE a0y = bv0[j+1];
          OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
          OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

          OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
          OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
          OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
          OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

          OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
          OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
          OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
          OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

          OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
          OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
          OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
          OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

          scratch[j]        = aS0x + aS1x;
          scratch[j+1]      = aS0y + aS1y;
          scratch[j+3*8]    = aS0x - aS1x;
          scratch[j+1+3*8]  = aS0y - aS1y;

          scratch[j+3*16]   = aD0x - aD1y;
          scratch[j+1+3*16] = aD0y + aD1x;
          scratch[j+3*24]   = aD0x + aD1y;
          scratch[j+1+3*24] = aD0y - aD1x;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
          OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
          OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
          OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

          OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
          OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
          OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
          OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

          OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
          OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
          OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
          OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

          OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
          OXS_FFT_REAL_TYPE bD1x = b1x - b3x;

          OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
          OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

          OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
          scratch[j+3*2]    = bS0x + bS1x;
          scratch[j+1+3*2]  = bS0y + bS1y;
          OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

          scratch[j+3*10]   = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*10] = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
          OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
          OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
          OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

          scratch[j+3*18]   = tUb2x*alphax - tUb2y*alphay;
          scratch[j+1+3*18] = tUb2y*alphax + tUb2x*alphay;
          scratch[j+3*26]   = tUb3x*alphay - tUb3y*alphax;
          scratch[j+1+3*26] = tUb3y*alphay + tUb3x*alphax;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
          OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
          OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
          OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

          OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
          OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
          OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
          OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

          OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
          OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
          OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
          OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

          OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
          OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
          OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
          OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

          OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
          OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
          OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
          OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

          scratch[j+3*4]    = cS0x + cS1x;
          scratch[j+1+3*4]  = cS0y + cS1y;
          scratch[j+3*12]   = cS1y - cS0y;
          scratch[j+1+3*12] = cS0x - cS1x;

          scratch[j+3*20]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*20] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j+3*28]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*28] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
          OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
          OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
          OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

          OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
          OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
          OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
          OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

          OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
          OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
          OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
          OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

          OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
          OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
          OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
          OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

          OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
          scratch[j+3*6]    = dS0x + dS1x;
          scratch[j+1+3*6]  = dS0y + dS1y;
          OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

          scratch[j+3*14]   = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*14] = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
          OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
          OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
          OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

          scratch[j+3*22]   = tUd2x*alphay - tUd2y*alphax;
          scratch[j+1+3*22] = tUd2y*alphay + tUd2x*alphax;
          scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
          scratch[j+1+3*30] = tUd3x*alphay - tUd3y*alphax;
        }


        // Bit reversal
        OC_INDEX ja;
        OXS_FFT_REAL_TYPE* w[16];
        for(ja=0;ja<16;++ja) {
          const OC_INDEX bs = 2*OFTV_VECSIZE;
          w[ja] = bv0+bs*ja;
          if(bitreverse[i+ja]>0) {
            // Swap
            memcpy(bv0+bs*ja,v+bitreverse[i+ja],bs*sizeof(OXS_FFT_REAL_TYPE));
            w[ja] = v+bitreverse[i+ja];
          } // Else, no swap.
        }

        // Do 4 lower (bottom/final) -level dragonflies
        for(ja=0;ja<16;ja+=4) {
          OC_INDEX off = 6*ja;
          OXS_FFT_REAL_TYPE const * const sv = scratch + off;
          for(OC_INDEX jb=0;jb<6;jb+=2) {
            OXS_FFT_REAL_TYPE Uax = sv[jb];
            OXS_FFT_REAL_TYPE Uay = sv[jb+1];
            OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
            OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

            OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
            OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
            OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
            OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

            OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
            OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
            OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
            OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

            OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
            OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
            OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
            OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

            *(w[ja]+jb)     = BaSx + BbSx;
            *(w[ja]+jb+1)   = BaSy + BbSy;
            *(w[ja+1]+jb)   = BaSx - BbSx;
            *(w[ja+1]+jb+1) = BaSy - BbSy;

            *(w[ja+2]+jb)   = BaDx - BbDy;
            *(w[ja+2]+jb+1) = BaDy + BbDx;
            *(w[ja+3]+jb)   = BaDx + BbDy;
            *(w[ja+3]+jb+1) = BaDy - BbDx;
          }
        }

        i += 16;
      } while((--k)>0);
      offset = 2*i;
      ++sptr;
    } while(sptr->stride>0);

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      OC_INDEX i;
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-5;i+=6) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTRadix4ZP
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{ // Version of ForwardRealToComplexFFTRadix4 which assumes that
  // rsize<=fftsize, i.e., that there is at least two-times
  // zero padding.
  // NOTE: The bit-reversal code in this routine is only valid
  // for fftsize>=128.
  // The following Oc_Nop is a workaround for an optimization bug in the
  // version of gcc 4.2.1 that ships alongside Mac OS X Lion.
  const OC_INDEX block32_count = Oc_Nop(4*(log2fftsize%2));
  const OC_INDEX block16_count = block32_count + 4;
  const OC_INDEX cstride = 2*(fftsize+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {
    {
      // Copy current row of rarr_in to carr_out, with zero padding.
      // NOTE: It is probably possible to speed computation a
      // few percent by embedding this processing with the first
      // radix-4 pass.
      OC_INDEX i;
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      if(mult_base!=NULL) {
        const OXS_FFT_REAL_TYPE* mult = mult_base;
        mult_base += rstride/OFTV_VECSIZE;
        for(i=0;i<istop-5;i+=6) {
          carr_out[i]   = (*mult)*rarr_in[i];
          carr_out[i+2] = (*mult)*rarr_in[i+1];
          carr_out[i+4] = (*mult)*rarr_in[i+2];
          ++mult;
          carr_out[i+1] = (*mult)*rarr_in[i+3];
          carr_out[i+3] = (*mult)*rarr_in[i+4];
          carr_out[i+5] = (*mult)*rarr_in[i+5];
          ++mult;
        }
        if(i<istop) {
          carr_out[i]   = (*mult)*rarr_in[i];
          carr_out[i+1] = 0.0;
          carr_out[i+2] = (*mult)*rarr_in[i+1];
          carr_out[i+3] = 0.0;
          carr_out[i+4] = (*mult)*rarr_in[i+2];
          carr_out[i+5] = 0.0;
          i += 6;
        }
      } else {
        for(i=0;i<istop-5;i+=6) {
          carr_out[i]   = rarr_in[i];
          carr_out[i+2] = rarr_in[i+1];
          carr_out[i+4] = rarr_in[i+2];
          carr_out[i+1] = rarr_in[i+3];
          carr_out[i+3] = rarr_in[i+4];
          carr_out[i+5] = rarr_in[i+5];
        }
        if(i<istop) {
          carr_out[i]   = rarr_in[i];
          carr_out[i+1] = 0.0;
          carr_out[i+2] = rarr_in[i+1];
          carr_out[i+3] = 0.0;
          carr_out[i+4] = rarr_in[i+2];
          carr_out[i+5] = 0.0;
          i += 6;
        }
      }
      while(i<OFTV_VECSIZE*fftsize) carr_out[i++] = 0.0;
    }

    OXS_FFT_REAL_TYPE* v = carr_out; // Working vector.

    // Do power-of-4 blocks, with preorder traversal/tree walk
    OC_INDEX offset = 0;
    const PreorderTraversalState* sptr = ptsRadix4;

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    OC_INDEX stride = sptr->stride;
    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;

    {
      // Do power-of-4 butterflies with "stride" at position v + 3*offset
      OXS_FFT_REAL_TYPE *const va = v;
      OXS_FFT_REAL_TYPE *const vb = va + stride;
      OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
      OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
      OC_INDEX i;
      for(i=0;i<2*OFTV_VECSIZE;i+=2) {
        // w^0
        OXS_FFT_REAL_TYPE x0 = va[i];
        OXS_FFT_REAL_TYPE y0 = va[i+1];
        OXS_FFT_REAL_TYPE x1 = vb[i];
        OXS_FFT_REAL_TYPE y1 = vb[i+1];
        va[i]   = x0 + x1;        va[i+1] = y0 + y1;
        vb[i]   = x0 - x1;        vb[i+1] = y0 - y1;
        vc[i]   = x0 + y1;        vc[i+1] = y0 - x1;
        vd[i]   = x0 - y1;        vd[i+1] = y0 + x1;
      }
      while(i<stride) {
        for(OC_INDEX j=i;j-i<6;j+=2) {
          OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
          OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
          OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
          OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7

          va[j]                 = x0 + x1;
          OXS_FFT_REAL_TYPE txa = x0 - x1;     // txa := R4

          OXS_FFT_REAL_TYPE mx1 = U[0];   // mx1 := R3

          va[j+1]               = y0 + y1;
          OXS_FFT_REAL_TYPE tya = y0 - y1;     // tya := R5

          OXS_FFT_REAL_TYPE mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
          mx1_tya *= tya;    // mx1_tya := R3

          OXS_FFT_REAL_TYPE my1 = U[1];    // my1 := R8

#define my1_tya tya
          my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
          my1_txa *= my1; // my1_txa := R4

          vb[j]     = mx1_txa - my1_tya;
          vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
          txb = x0 + y1;  // txb := R2
#define txc x0
          txc -= y1;       // txc := R0
#define tyb mx1_tya
          tyb = y0 - x1;  // tyb := R3
#define tyc y0
          tyc += x1;       // tyc := R6

#define mx2 my1_txa
          mx2     = U[2]; //     mx2 := R4
#define mx2_tyb y1
          mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb x1
          mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
          my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
          my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
          my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
          Cx -= my2_tyb; // Cx := R1
          vc[j]   = Cx;

#define mx3 mx2
          mx3 = U[4];  // mx3 := R4
#define my3 my2
          my3 = U[5];  // my3 := R5

          OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
          OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
          OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
          OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0

          vc[j+1] = mx2_tyb + my2_txb;
          vd[j]   = mx3_txc - my3_tyc;
          vd[j+1] = my3_txc + mx3_tyc;

#undef mx1_tya
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
        }
        i+=6;
        U+=6;
      }
      stride /= 4;
    }

    do {

      if(offset>0) {
        // Stride is measured in units of OXS_FFT_REAL_TYPE's.
        stride = sptr->stride;
        U = UForwardRadix4 + sptr->uoff;
      }

      while(stride>48) {
        // Do power-of-4 butterflies with "stride" at position v + 3*offset
        OXS_FFT_REAL_TYPE *const va = v + OFTV_VECSIZE*offset;
        OXS_FFT_REAL_TYPE *const vb = va + stride;
        OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
        OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
        OC_INDEX i;
        for(i=0;i<6;i+=2) {
          // w^0
          OXS_FFT_REAL_TYPE x0 = va[i];
          OXS_FFT_REAL_TYPE y0 = va[i+1];
          OXS_FFT_REAL_TYPE x1 = vb[i];
          OXS_FFT_REAL_TYPE y1 = vb[i+1];
          OXS_FFT_REAL_TYPE x2 = vc[i];
          OXS_FFT_REAL_TYPE x3 = vd[i];

          OXS_FFT_REAL_TYPE y2 = vc[i+1];

          OXS_FFT_REAL_TYPE sx1 = x0 + x2;
          OXS_FFT_REAL_TYPE dx1 = x0 - x2;

          OXS_FFT_REAL_TYPE sx2 = x1 + x3;
          OXS_FFT_REAL_TYPE dx2 = x1 - x3;

          OXS_FFT_REAL_TYPE y3 = vd[i+1];

          OXS_FFT_REAL_TYPE sy1 = y0 + y2;
          OXS_FFT_REAL_TYPE dy1 = y0 - y2;
          OXS_FFT_REAL_TYPE sy2 = y1 + y3;
          OXS_FFT_REAL_TYPE dy2 = y1 - y3;

          va[i]   = sx1 + sx2;
          va[i+1] = sy1 + sy2;
          vb[i]   = sx1 - sx2;
          vb[i+1] = sy1 - sy2;
          vc[i]   = dx1 + dy2;
          vc[i+1] = dy1 - dx2;
          vd[i]   = dx1 - dy2;
          vd[i+1] = dy1 + dx2;
        }
        while(i<stride) {
          for(OC_INDEX j=i;j-i<6;j+=2) {
            OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
            OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
            OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
            OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
            OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
            OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

            OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
            OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
            dx1 -= x2;    // dx1 := R0
#define dx2 x1
            dx2 -= x3;    // dx2 := R1

#define y2 x2
            y2 = vc[j+1]; // y2 := R2
#define y3 x3
            y3 = vd[j+1]; // y3 := R3

            va[j]              = sx1 + sx2;
#define txa sx1
            txa -= sx2;     // txa := R4

#define sy1 sx2
            sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
            dy1 -= y2;      // dy1 := R6
            OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
            dy2 -= y3;      // dy2 := R7

#define mx1 y3
            mx1 = U[0];   // mx1 := R3

            va[j+1]              = sy1 + sy2;
#define tya sy1
            tya -= sy2;     // tya := R5

#define mx1_txa y2
            mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
            mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
            my1 = U[1];    // my1 := R8

#define my1_tya tya
            my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
            my1_txa *= my1; // my1_txa := R4

            vb[j]     = mx1_txa - my1_tya;
            vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
            txb = dx1 + dy2;  // txb := R2
#define txc dx1
            txc -= dy2;       // txc := R0
#define tyb mx1_tya
            tyb = dy1 - dx2;  // tyb := R3
#define tyc dy1
            tyc += dx2;       // tyc := R6

#define mx2 my1_txa
            mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
            mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
            mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
            my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
            my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
            my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
            Cx -= my2_tyb; // Cx := R1
            vc[j]   = Cx;

#define mx3 mx2
            mx3 = U[4];  // mx3 := R4
#define my3 my2
            my3 = U[5];  // my3 := R5

            OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
            OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
            OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
            OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
            OXS_FFT_REAL_TYPE Cy = mx2_tyb + my2_txb; // Cy := R2
            OXS_FFT_REAL_TYPE Dx = mx3_txc - my3_tyc;
            OXS_FFT_REAL_TYPE Dy = my3_txc + mx3_tyc;

            vc[j+1] = Cy;
            vd[j]   = Dx;
            vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
          }
          i+=6;
          U+=6;
        }
        stride /= 4;
      }
      if(block32_count>0) {
        { // i=0
          OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*offset;
          for(OC_INDEX j1=0 ; j1<OFTV_VECSIZE*256 ; j1+=OFTV_VECSIZE*64) {
            for(OC_INDEX j2=j1 ; j2<j1+2*OFTV_VECSIZE ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+OFTV_VECSIZE*16];
              OXS_FFT_REAL_TYPE cy0 = va[j2+OFTV_VECSIZE*16+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+OFTV_VECSIZE*32];
              OXS_FFT_REAL_TYPE ay1 = va[j2+OFTV_VECSIZE*32+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+OFTV_VECSIZE*48];
              OXS_FFT_REAL_TYPE cy1 = va[j2+OFTV_VECSIZE*48+1];
              va[j2]                    = ax0 + ax1;
              va[j2+1]                  = ay0 + ay1;
              va[j2+OFTV_VECSIZE*32]   = ax0 - ax1;
              va[j2+OFTV_VECSIZE*32+1] = ay0 - ay1;

              va[j2+OFTV_VECSIZE*16]   = cx0 + cx1;
              va[j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
              va[j2+OFTV_VECSIZE*48]   = cy0 - cy1;
              va[j2+OFTV_VECSIZE*48+1] = cx1 - cx0;
            }
          }
        }
        // NB: U currently points to the tail of the UForwardRadix4 array,
        // which is a subsegment holding the 32nd roots of unity.  That
        // subsegment doesn't contain the (1,0) root, so the access below
        // is to U - 2.
        for(OC_INDEX i=2;i<16;i+=2) {
          OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*(offset + i);
          const OXS_FFT_REAL_TYPE amx = U[i-2];
          const OXS_FFT_REAL_TYPE amy = U[i-1];
          for(OC_INDEX j1=0;j1<OFTV_VECSIZE*256;j1+=OFTV_VECSIZE*64) {
            for(OC_INDEX j2=j1 ; j2<j1+2*OFTV_VECSIZE ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+OFTV_VECSIZE*16];
              OXS_FFT_REAL_TYPE cy0 = va[j2+OFTV_VECSIZE*16+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+OFTV_VECSIZE*32];
              OXS_FFT_REAL_TYPE ay1 = va[j2+OFTV_VECSIZE*32+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+OFTV_VECSIZE*48];
              OXS_FFT_REAL_TYPE cy1 = va[j2+OFTV_VECSIZE*48+1];

              OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
              va[j2]        = ax0 + ax1;
              va[j2+1]      = ay0 + ay1;
              OXS_FFT_REAL_TYPE adify = ay0 - ay1;

              va[j2+OFTV_VECSIZE*32]   = amx*adifx - amy*adify;
              va[j2+OFTV_VECSIZE*32+1] = amx*adify + amy*adifx;

              OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
              va[j2+OFTV_VECSIZE*16]   = cx0 + cx1;
              va[j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
              OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

              va[j2+OFTV_VECSIZE*48]   = amx*cdify + amy*cdifx;
              va[j2+OFTV_VECSIZE*48+1] = amy*cdify - amx*cdifx;
            }
          }
        }
      }

      OC_INDEX i = offset/2;
      OC_INDEX k=block16_count;
      do {
        // Bottom level 16-passes, with built-in bit-flip shuffling
        const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
        const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

        OXS_FFT_REAL_TYPE* const bv0 = v + 6*i;

        OC_INDEX j;

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE a0x = bv0[j];
          OXS_FFT_REAL_TYPE a0y = bv0[j+1];
          OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
          OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

          OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
          OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
          OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
          OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

          OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
          OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
          OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
          OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

          OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
          OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
          OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
          OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

          scratch[j]        = aS0x + aS1x;
          scratch[j+1]      = aS0y + aS1y;
          scratch[j+3*8]    = aS0x - aS1x;
          scratch[j+1+3*8]  = aS0y - aS1y;

          scratch[j+3*16]   = aD0x + aD1y;
          scratch[j+1+3*16] = aD0y - aD1x;
          scratch[j+3*24]   = aD0x - aD1y;
          scratch[j+1+3*24] = aD0y + aD1x;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
          OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
          OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
          OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

          OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
          OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
          OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
          OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

          OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
          OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
          OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
          OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

          OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
          OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
          OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
          OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

          OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
          scratch[j+3*2]    = bS0x + bS1x;
          scratch[j+1+3*2]  = bS0y + bS1y;
          OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

          scratch[j+3*10]   = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*10] = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
          OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
          OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
          OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

          scratch[j+3*18]   = tUb2x*alphax + tUb2y*alphay;
          scratch[j+1+3*18] = tUb2y*alphax - tUb2x*alphay;
          scratch[j+3*26]   = tUb3x*alphay + tUb3y*alphax;
          scratch[j+1+3*26] = tUb3y*alphay - tUb3x*alphax;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
          OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
          OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
          OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

          OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
          OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
          OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
          OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

          OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
          OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
          OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
          OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

          OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
          OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
          OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
          OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

          OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
          OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
          OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
          OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

          scratch[j+3*4]    = cS1x + cS0x;
          scratch[j+1+3*4]  = cS0y + cS1y;
          scratch[j+3*12]   = cS0y - cS1y;
          scratch[j+1+3*12] = cS1x - cS0x;

          scratch[j+3*20]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*20] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j+3*28]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*28] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
          OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
          OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
          OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

          OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
          OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
          OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
          OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

          OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
          OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
          OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
          OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

          OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
          OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
          OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
          OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

          OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
          scratch[j+3*6]    = dS1x + dS0x;
          scratch[j+1+3*6]  = dS0y + dS1y;
          OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

          scratch[j+3*14]   = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*14] = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
          OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
          OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
          OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

          scratch[j+3*22]   = tUd2x*alphay + tUd2y*alphax;
          scratch[j+1+3*22] = tUd2y*alphay - tUd2x*alphax;
          scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
          scratch[j+1+3*30] = tUd3y*alphax - tUd3x*alphay;
        }

        // Bit reversal
        OC_INDEX ja;
        OXS_FFT_REAL_TYPE* w[16];
        for(ja=0;ja<16;++ja) {
          const OC_INDEX bs = 2*OFTV_VECSIZE;
          w[ja] = bv0+bs*ja;
          if(bitreverse[i+ja]>0) {
            // Swap
            memcpy(bv0+bs*ja,v+bitreverse[i+ja],bs*sizeof(OXS_FFT_REAL_TYPE));
            w[ja] = v+bitreverse[i+ja];
          } // Else, no swap.
        }

        // Do 4 lower (bottom/final) -level dragonflies
        for(ja=0;ja<16;ja+=4) {
          OC_INDEX off = 6*ja;
          OXS_FFT_REAL_TYPE const * const sv = scratch + off;
          for(OC_INDEX jb=0;jb<6;jb+=2) {
            OXS_FFT_REAL_TYPE Uax = sv[jb];
            OXS_FFT_REAL_TYPE Uay = sv[jb+1];
            OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
            OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

            OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
            OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
            OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
            OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

            OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
            OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
            OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
            OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

            OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
            OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
            OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
            OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

            *(w[ja]+jb)     = BaSx + BbSx;
            *(w[ja]+jb+1)   = BaSy + BbSy;
            *(w[ja+1]+jb)   = BaSx - BbSx;
            *(w[ja+1]+jb+1) = BaSy - BbSy;

            *(w[ja+2]+jb)   = BaDx + BbDy;
            *(w[ja+2]+jb+1) = BaDy - BbDx;
            *(w[ja+3]+jb)   = BaDx - BbDy;
            *(w[ja+3]+jb+1) = BaDy + BbDx;
          }
        }

        i += 16;
      } while((--k)>0);
      offset = 2*i;
      ++sptr;
    } while(sptr->stride>0);

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      v[2*OFTV_VECSIZE*fftsize+j]   = v[j] - v[j+1];
      v[2*OFTV_VECSIZE*fftsize+1+j] = 0.0;
      v[j] += v[j+1];
      v[j+1] = 0.0;
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*fftsize;
    for(OC_INDEX i=2;i<fftsize;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = 0.5*UReals[i];
      const OXS_FFT_REAL_TYPE wy = 0.5*UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = 0.5*(Ax + Bx);
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = 0.5*(Ay - By);
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+j]   = Sx + C1;
        v[k1+j+1] = C2 + Dy;
        v[k2+j]   = Sx - C1;
        v[k2+j+1] = C2 - Dy;
      }
    }
    v[OFTV_VECSIZE*fftsize+1] *= -1;
    v[OFTV_VECSIZE*fftsize+3] *= -1;
    v[OFTV_VECSIZE*fftsize+5] *= -1;
  }
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTRadix4ZP
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{ // Version of InverseComplexToRealFFTRadix4 which assumes that
  // rsize<=fftsize, i.e., that the last half of the transform
  // is to be thrown away.
  // NOTE: The bit-reversal code in this routine
  // is only valid for fftsize>=128.
  assert(workbuffer != 0);
  // The following Oc_Nop is a workaround for an optimization bug in the
  // version of gcc 4.2.1 that ships alongside Mac OS X Lion.
  const OC_INDEX block32_count = Oc_Nop(4*(log2fftsize%2));
  const OC_INDEX block16_count = block32_count + 4;
  const OC_INDEX cstride = 2*(fftsize+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {
#if 1
    OXS_FFT_REAL_TYPE* const v = workbuffer; // Working vector.
#else
    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.
#endif

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    // Copy from carr_in to workbuffer; this should reduce memory
    // write traffic by reducing the number of dirty cache lines.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = carr_in[j];
      OXS_FFT_REAL_TYPE tmpBx0 = carr_in[2*OFTV_VECSIZE*fftsize+j];
      v[j]   = 0.5*(tmpAx0 + tmpBx0);
      v[j+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[j+1] and v[2*OFTV_VECSIZE*fftsize+j+1] = 0.0
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*fftsize;
    for(OC_INDEX i=2;i<fftsize;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = 0.5*UReals[i];
      const OXS_FFT_REAL_TYPE wy = 0.5*UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = carr_in[k1+j];
        const OXS_FFT_REAL_TYPE Ay = carr_in[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = carr_in[k2+j];
        const OXS_FFT_REAL_TYPE By = carr_in[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = 0.5*(Ax + Bx);
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = 0.5*(Ay - By);
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = Sx + C1;
        v[k1+j+1] = C2 + Dy;
        v[k2+j]   = Sx - C1;
        v[k2+j+1] = C2 - Dy;
      }
    }
    v[OFTV_VECSIZE*fftsize]   =    carr_in[OFTV_VECSIZE*fftsize];
    v[OFTV_VECSIZE*fftsize+1] = -1*carr_in[OFTV_VECSIZE*fftsize+1];
    v[OFTV_VECSIZE*fftsize+2] =    carr_in[OFTV_VECSIZE*fftsize+2];
    v[OFTV_VECSIZE*fftsize+3] = -1*carr_in[OFTV_VECSIZE*fftsize+3];
    v[OFTV_VECSIZE*fftsize+4] =    carr_in[OFTV_VECSIZE*fftsize+4];
    v[OFTV_VECSIZE*fftsize+5] = -1*carr_in[OFTV_VECSIZE*fftsize+5];

    // Do power-of-4 blocks, with preorder traversal/tree walk
    OC_INDEX offset = 0;
    const PreorderTraversalState* sptr = ptsRadix4;
    do {
      // Stride is measured in units of OXS_FFT_REAL_TYPE's.
      OC_INDEX stride = sptr->stride;
      OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;
      // Note: The inverse roots are the complex conjugates of the
      // forward roots.
      do {
        // Do power-of-4 butterflies with "stride" at position v + 3*offset
        OXS_FFT_REAL_TYPE *const va = v + OFTV_VECSIZE*offset;
        OXS_FFT_REAL_TYPE *const vb = va + stride;
        OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
        OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
        OC_INDEX i;
        for(i=0;i<6;i+=2) {
          // w^0
          OXS_FFT_REAL_TYPE x0 = va[i];  OXS_FFT_REAL_TYPE y0 = va[i+1];
          OXS_FFT_REAL_TYPE x1 = vb[i];  OXS_FFT_REAL_TYPE y1 = vb[i+1];
          OXS_FFT_REAL_TYPE x2 = vc[i];
          OXS_FFT_REAL_TYPE x3 = vd[i];

          OXS_FFT_REAL_TYPE y2 = vc[i+1];

          OXS_FFT_REAL_TYPE sx1 = x0 + x2;
          OXS_FFT_REAL_TYPE dx1 = x0 - x2;

          OXS_FFT_REAL_TYPE sx2 = x1 + x3;
          OXS_FFT_REAL_TYPE dx2 = x1 - x3;

          OXS_FFT_REAL_TYPE y3 = vd[i+1];

          OXS_FFT_REAL_TYPE sy1 = y0 + y2;
          OXS_FFT_REAL_TYPE dy1 = y0 - y2;
          OXS_FFT_REAL_TYPE sy2 = y1 + y3;
          OXS_FFT_REAL_TYPE dy2 = y1 - y3;

          va[i] = sx1 + sx2;  va[i+1] = sy1 + sy2;
          vb[i] = sx1 - sx2;  vb[i+1] = sy1 - sy2;
          vc[i] = dx1 - dy2;  vc[i+1] = dy1 + dx2;
          vd[i] = dx1 + dy2;  vd[i+1] = dy1 - dx2;
        }
        while(i<stride) {
          for(OC_INDEX j=i;j-i<6;j+=2) {
            OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
            OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
            OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
            OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
            OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
            OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

            OXS_FFT_REAL_TYPE sx1 = x0 + x2; // sx1 := R4
            OXS_FFT_REAL_TYPE sx2 = x1 + x3; // sx2 := R5
#define dx1 x0
            dx1 -= x2;    // dx1 := R0
#define dx2 x1
            dx2 -= x3;    // dx2 := R1

#define y2 x2
            y2 = vc[j+1]; // y2 := R2
#define y3 x3
            y3 = vd[j+1]; // y3 := R3

            va[j]              = sx1 + sx2;
#define txa sx1
            txa -= sx2;     // txa := R4

#define sy1 sx2
            sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
            dy1 -= y2;      // dy1 := R6
            OXS_FFT_REAL_TYPE sy2 = y1 + y3; // sy2 := R8
#define dy2 y1
            dy2 -= y3;      // dy2 := R7

#define mx1 y3
            mx1 = U[0];   // mx1 := R3

            va[j+1]              = sy1 + sy2;
#define tya sy1
            tya -= sy2;     // tya := R5

#define mx1_txa y2
            mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
            mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
            my1 = U[1];    // my1 := R8

#define my1_tya tya
            my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
            my1_txa *= my1; // my1_txa := R4

            vb[j]     = mx1_txa + my1_tya;
            vb[j+1]   = mx1_tya - my1_txa;

#define txb mx1_txa
            txb = dx1 - dy2;  // txb := R2
#define txc dx1
            txc += dy2;       // txc := R0
#define tyb mx1_tya
            tyb = dy1 + dx2;  // tyb := R3
#define tyc dy1
            tyc -= dx2;       // tyc := R6

#define mx2 my1_txa
            mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
            mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
            mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
            my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
            my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
            my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
            Cx += my2_tyb; // Cx := R1
            vc[j]   = Cx;

#define mx3 mx2
            mx3 = U[4];  // mx3 := R4
#define my3 my2
            my3 = U[5];  // my3 := R5

            OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
            OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
            OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
            OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
            OXS_FFT_REAL_TYPE Cy = mx2_tyb - my2_txb; // Cy := R2
            OXS_FFT_REAL_TYPE Dx = mx3_txc + my3_tyc;
            OXS_FFT_REAL_TYPE Dy = mx3_tyc - my3_txc;

            vc[j+1] = Cy;
            vd[j]   = Dx;
            vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
          }
          i+=6;
          U+=6;
        }
        stride /= 4;
      } while(stride>48);
      if(block32_count>0) {
        { // i=0
          OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*offset;
          for(OC_INDEX j1=0 ; j1<OFTV_VECSIZE*256 ; j1+=OFTV_VECSIZE*64) {
            for(OC_INDEX j2=0 ; j2<2*OFTV_VECSIZE ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j1+j2];
              OXS_FFT_REAL_TYPE ay0 = va[j1+j2+1];
              OXS_FFT_REAL_TYPE ax1 = va[j1+j2+OFTV_VECSIZE*32];
              OXS_FFT_REAL_TYPE ay1 = va[j1+j2+OFTV_VECSIZE*32+1];

              va[j1+j2]                    = ax0 + ax1;
              va[j1+j2+1]                  = ay0 + ay1;
              va[j1+j2+OFTV_VECSIZE*32]   = ax0 - ax1;
              va[j1+j2+OFTV_VECSIZE*32+1] = ay0 - ay1;

              OXS_FFT_REAL_TYPE cx0 = va[j1+j2+OFTV_VECSIZE*16];
              OXS_FFT_REAL_TYPE cy0 = va[j1+j2+OFTV_VECSIZE*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j1+j2+OFTV_VECSIZE*48];
              OXS_FFT_REAL_TYPE cy1 = va[j1+j2+OFTV_VECSIZE*48+1];

              va[j1+j2+OFTV_VECSIZE*16]   = cx0 + cx1;
              va[j1+j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
              va[j1+j2+OFTV_VECSIZE*48]   = cy1 - cy0;
              va[j1+j2+OFTV_VECSIZE*48+1] = cx0 - cx1;
            }
          }
        }
        // NB: U currently points to the tail of the UForwardRadix4 array,
        // which is a subsegment holding the 32nd roots of unity.  That
        // subsegment doesn't contain the (1,0) root, so the access below
        // is to U - 2.  Again, the inverse roots are the complex conjugates
        // of the forward roots.
        for(OC_INDEX i=2;i<16;i+=2) {
          OXS_FFT_REAL_TYPE* const va
            = v + OFTV_VECSIZE*offset + OFTV_VECSIZE*i;
          const OXS_FFT_REAL_TYPE amx = U[i-2];
          const OXS_FFT_REAL_TYPE amy = U[i-1];
          for(OC_INDEX j1=0;j1<OFTV_VECSIZE*256;j1+=OFTV_VECSIZE*64) {
            for(OC_INDEX j2=j1 ; j2<j1+6 ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+OFTV_VECSIZE*16];
              OXS_FFT_REAL_TYPE cy0 = va[j2+OFTV_VECSIZE*16+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+OFTV_VECSIZE*32];
              OXS_FFT_REAL_TYPE ay1 = va[j2+OFTV_VECSIZE*32+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+OFTV_VECSIZE*48];
              OXS_FFT_REAL_TYPE cy1 = va[j2+OFTV_VECSIZE*48+1];

              OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
              va[j2]        = ax0 + ax1;
              va[j2+1]      = ay0 + ay1;
              OXS_FFT_REAL_TYPE adify = ay0 - ay1;

              va[j2+OFTV_VECSIZE*32]   = amx*adifx + amy*adify;
              va[j2+OFTV_VECSIZE*32+1] = amx*adify - amy*adifx;

              OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
              va[j2+OFTV_VECSIZE*16]   = cx0 + cx1;
              va[j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
              OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

              va[j2+OFTV_VECSIZE*48]   = amy*cdifx - amx*cdify;
              va[j2+OFTV_VECSIZE*48+1] = amy*cdify + amx*cdifx;
            }
          }
        }
      }

      OC_INDEX i = offset/2;
      OC_INDEX k=block16_count;
      do {
        // Bottom level 16-passes, with built-in bit-flip shuffling
        const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
        const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

        OXS_FFT_REAL_TYPE* const bv0 = v + 6*i;

        OC_INDEX j;

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE a0x = bv0[j];
          OXS_FFT_REAL_TYPE a0y = bv0[j+1];
          OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
          OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

          OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
          OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
          OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
          OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

          OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
          OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
          OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
          OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

          OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
          OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
          OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
          OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

          scratch[j]        = aS0x + aS1x;
          scratch[j+1]      = aS0y + aS1y;
          scratch[j+3*8]    = aS0x - aS1x;
          scratch[j+1+3*8]  = aS0y - aS1y;

          scratch[j+3*16]   = aD0x - aD1y;
          scratch[j+1+3*16] = aD0y + aD1x;
          scratch[j+3*24]   = aD0x + aD1y;
          scratch[j+1+3*24] = aD0y - aD1x;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
          OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
          OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
          OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

          OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
          OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
          OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
          OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

          OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
          OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
          OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
          OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

          OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
          OXS_FFT_REAL_TYPE bD1x = b1x - b3x;

          OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
          OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

          OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
          scratch[j+3*2]    = bS0x + bS1x;
          scratch[j+1+3*2]  = bS0y + bS1y;
          OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

          scratch[j+3*10]   = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*10] = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
          OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
          OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
          OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

          scratch[j+3*18]   = tUb2x*alphax - tUb2y*alphay;
          scratch[j+1+3*18] = tUb2y*alphax + tUb2x*alphay;
          scratch[j+3*26]   = tUb3x*alphay - tUb3y*alphax;
          scratch[j+1+3*26] = tUb3y*alphay + tUb3x*alphax;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
          OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
          OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
          OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

          OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
          OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
          OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
          OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

          OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
          OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
          OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
          OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

          OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
          OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
          OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
          OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

          OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
          OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
          OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
          OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

          scratch[j+3*4]    = cS0x + cS1x;
          scratch[j+1+3*4]  = cS0y + cS1y;
          scratch[j+3*12]   = cS1y - cS0y;
          scratch[j+1+3*12] = cS0x - cS1x;

          scratch[j+3*20]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*20] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j+3*28]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*28] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        }

        for(j=0;j<6;j+=2) {
          OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
          OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
          OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
          OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

          OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
          OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
          OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
          OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

          OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
          OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
          OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
          OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

          OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
          OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
          OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
          OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

          OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
          scratch[j+3*6]    = dS0x + dS1x;
          scratch[j+1+3*6]  = dS0y + dS1y;
          OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

          scratch[j+3*14]   = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
          scratch[j+1+3*14] = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
          OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
          OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
          OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

          scratch[j+3*22]   = tUd2x*alphay - tUd2y*alphax;
          scratch[j+1+3*22] = tUd2y*alphay + tUd2x*alphax;
          scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
          scratch[j+1+3*30] = tUd3x*alphay - tUd3y*alphax;
        }


        // Bit reversal.  This "...ZP" variant skips reversals
        // that affect what ends up in the upper half of the
        // results, since those are to be thrown away.
        OC_INDEX ja;
        OXS_FFT_REAL_TYPE* w[16];
        if(2*i<fftsize) {
          for(ja=0;ja<16;ja+=2) {
            const OC_INDEX bs = 2*OFTV_VECSIZE;
            w[ja] = bv0+bs*ja;
            if(bitreverse[i+ja]>0) {
              // Swap
              memcpy(bv0+bs*ja,v+bitreverse[i+ja],bs*sizeof(OXS_FFT_REAL_TYPE));
              w[ja] = v+bitreverse[i+ja];
            } // Else, no swap.
          }
        } else {
          for(ja=0;ja<16;ja+=2) w[ja] = v+bitreverse[i+ja];
        }

        // Do 4 lower (bottom/final) -level dragonflies
        // This "...ZP" variant assumes that the last half of the
        // results are to be thrown away, or equivalently, that
        // the pre-reversal odd (ja%2==1) results are not used.
        for(ja=0;ja<16;ja+=4) {
          OC_INDEX off = 6*ja;
          OXS_FFT_REAL_TYPE const * const sv = scratch + off;
          for(OC_INDEX jb=0;jb<6;jb+=2) {
            OXS_FFT_REAL_TYPE Uax = sv[jb];
            OXS_FFT_REAL_TYPE Uay = sv[jb+1];
            OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
            OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

            OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
            OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
            OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
            OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

            OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
            OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
            OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
            OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

            OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
            OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
            OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
            OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

            *(w[ja]+jb)     = BaSx + BbSx;
            *(w[ja]+jb+1)   = BaSy + BbSy;
            *(w[ja+2]+jb)   = BaDx - BbDy;
            *(w[ja+2]+jb+1) = BaDy + BbDx;
          }
        }

        i += 16;
      } while((--k)>0);
      offset = 2*i;
      ++sptr;
    } while(sptr->stride>0);

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest).
    {
      OC_INDEX i;
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-5;i+=6) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize64
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{
#define RTNFFTSIZE 64
  const OC_INDEX block16_count = 4;
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {
    {
      // Copy current row of rarr_in to carr_out, with zero padding.
      OC_INDEX i;
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      if(mult_base!=NULL) {
        const OXS_FFT_REAL_TYPE* mult = mult_base;
        mult_base += rstride/OFTV_VECSIZE;
        for(i=0;i<istop-5;i+=6) {
          carr_out[i]   = (*mult)*rarr_in[i];
          carr_out[i+2] = (*mult)*rarr_in[i+1];
          carr_out[i+4] = (*mult)*rarr_in[i+2];
          ++mult;
          carr_out[i+1] = (*mult)*rarr_in[i+3];
          carr_out[i+3] = (*mult)*rarr_in[i+4];
          carr_out[i+5] = (*mult)*rarr_in[i+5];
          ++mult;
        }
        if(i<istop) {
          carr_out[i]   = (*mult)*rarr_in[i];
          carr_out[i+1] = 0.0;
          carr_out[i+2] = (*mult)*rarr_in[i+1];
          carr_out[i+3] = 0.0;
          carr_out[i+4] = (*mult)*rarr_in[i+2];
          carr_out[i+5] = 0.0;
          i += 6;
        }
      } else {
        for(i=0;i<istop-5;i+=6) {
          carr_out[i]   = rarr_in[i];
          carr_out[i+2] = rarr_in[i+1];
          carr_out[i+4] = rarr_in[i+2];
          carr_out[i+1] = rarr_in[i+3];
          carr_out[i+3] = rarr_in[i+4];
          carr_out[i+5] = rarr_in[i+5];
        }
        if(i<istop) {
          carr_out[i]   = rarr_in[i];
          carr_out[i+1] = 0.0;
          carr_out[i+2] = rarr_in[i+1];
          carr_out[i+3] = 0.0;
          carr_out[i+4] = rarr_in[i+2];
          carr_out[i+5] = 0.0;
          i += 6;
        }
      }
      while(i<2*OFTV_VECSIZE*RTNFFTSIZE) carr_out[i++] = 0.0;
    }
    OXS_FFT_REAL_TYPE* v = carr_out; // Working vector.

    const PreorderTraversalState* sptr = ptsRadix4;

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    const OC_INDEX stride = sptr->stride;
    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;

    // Do power-of-4 butterflies with "stride" == 64/4
    OXS_FFT_REAL_TYPE *const va = v;
    OXS_FFT_REAL_TYPE *const vb = va + stride;
    OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
    OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      // w^0
      OXS_FFT_REAL_TYPE x0 = va[i];
      OXS_FFT_REAL_TYPE y0 = va[i+1];
      OXS_FFT_REAL_TYPE x1 = vb[i];
      OXS_FFT_REAL_TYPE y1 = vb[i+1];
      OXS_FFT_REAL_TYPE x2 = vc[i];
      OXS_FFT_REAL_TYPE x3 = vd[i];

      OXS_FFT_REAL_TYPE y2 = vc[i+1];

      OXS_FFT_REAL_TYPE sx1 = x0 + x2;
      OXS_FFT_REAL_TYPE dx1 = x0 - x2;

      OXS_FFT_REAL_TYPE sx2 = x1 + x3;
      OXS_FFT_REAL_TYPE dx2 = x1 - x3;

      OXS_FFT_REAL_TYPE y3 = vd[i+1];

      OXS_FFT_REAL_TYPE sy1 = y0 + y2;
      OXS_FFT_REAL_TYPE dy1 = y0 - y2;
      OXS_FFT_REAL_TYPE sy2 = y1 + y3;
      OXS_FFT_REAL_TYPE dy2 = y1 - y3;

      va[i]   = sx1 + sx2;
      va[i+1] = sy1 + sy2;
      vb[i]   = sx1 - sx2;
      vb[i+1] = sy1 - sy2;
      vc[i]   = dx1 + dy2;
      vc[i+1] = dy1 - dx2;
      vd[i]   = dx1 - dy2;
      vd[i+1] = dy1 + dx2;
    }
    while(i<stride) {
      for(OC_INDEX j=i;j-i<2*OFTV_VECSIZE;j+=2) {
        OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
        OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
        OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
        OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
        OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
        OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

        OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
        OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
        dx1 -= x2;    // dx1 := R0
#define dx2 x1
        dx2 -= x3;    // dx2 := R1

#define y2 x2
        y2 = vc[j+1]; // y2 := R2
#define y3 x3
        y3 = vd[j+1]; // y3 := R3

        va[j]              = sx1 + sx2;
#define txa sx1
        txa -= sx2;     // txa := R4

#define sy1 sx2
        sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
        dy1 -= y2;      // dy1 := R6
        OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
        dy2 -= y3;      // dy2 := R7

#define mx1 y3
        mx1 = U[0];   // mx1 := R3

        va[j+1]              = sy1 + sy2;
#define tya sy1
        tya -= sy2;     // tya := R5

#define mx1_txa y2
        mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
        mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
        my1 = U[1];    // my1 := R8

#define my1_tya tya
        my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
        my1_txa *= my1; // my1_txa := R4

        vb[j]     = mx1_txa - my1_tya;
        vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
        txb = dx1 + dy2;  // txb := R2
#define txc dx1
        txc -= dy2;       // txc := R0
#define tyb mx1_tya
        tyb = dy1 - dx2;  // tyb := R3
#define tyc dy1
        tyc += dx2;       // tyc := R6

#define mx2 my1_txa
        mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
        mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
        mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
        my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
        my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
        my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
        Cx -= my2_tyb; // Cx := R1
        vc[j]   = Cx;

#define mx3 mx2
        mx3 = U[4];  // mx3 := R4
#define my3 my2
        my3 = U[5];  // my3 := R5

        OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
        OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
        OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
        OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
        OXS_FFT_REAL_TYPE Cy = mx2_tyb + my2_txb; // Cy := R2
        OXS_FFT_REAL_TYPE Dx = mx3_txc - my3_tyc;
        OXS_FFT_REAL_TYPE Dy = my3_txc + mx3_tyc;

        vc[j+1] = Cy;
        vd[j]   = Dx;
        vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
      }
      i+=6;
      U+=6;
    }

    i = 0;
    OC_INDEX k = block16_count;
    do {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + 6*i;

      OC_INDEX j;

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
        OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j]        = aS0x + aS1x;
        scratch[j+1]      = aS0y + aS1y;
        scratch[j+3*8]    = aS0x - aS1x;
        scratch[j+1+3*8]  = aS0y - aS1y;

        scratch[j+3*16]   = aD0x + aD1y;
        scratch[j+1+3*16] = aD0y - aD1x;
        scratch[j+3*24]   = aD0x - aD1y;
        scratch[j+1+3*24] = aD0y + aD1x;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
        OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
        OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j+3*2]    = bS0x + bS1x;
        scratch[j+1+3*2]  = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j+3*10]   = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*10] = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

        scratch[j+3*18]   = tUb2x*alphax + tUb2y*alphay;
        scratch[j+1+3*18] = tUb2y*alphax - tUb2x*alphay;
        scratch[j+3*26]   = tUb3x*alphay + tUb3y*alphax;
        scratch[j+1+3*26] = tUb3y*alphay - tUb3x*alphax;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
        OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
        OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
        OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
        OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

        scratch[j+3*4]    = cS1x + cS0x;
        scratch[j+1+3*4]  = cS0y + cS1y;
        scratch[j+3*12]   = cS0y - cS1y;
        scratch[j+1+3*12] = cS1x - cS0x;

        scratch[j+3*20]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*20] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+3*28]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*28] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
        OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
        OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

        OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
        OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
        OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
        OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

        OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
        scratch[j+3*6]    = dS1x + dS0x;
        scratch[j+1+3*6]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

        scratch[j+3*14]   = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*14] = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
        OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

        scratch[j+3*22]   = tUd2x*alphay + tUd2y*alphax;
        scratch[j+1+3*22] = tUd2y*alphay - tUd2x*alphax;
        scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j+1+3*30] = tUd3y*alphax - tUd3x*alphay;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      for(ja=0;ja<16;++ja) {
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        w[ja] = bv0+bs*ja;
        OC_INDEX br = bitreverse[i+ja];
        if(br>0) { // Swap
          memcpy(w[ja],v + br,bs*sizeof(OXS_FFT_REAL_TYPE));
          w[ja] = v + br;
        } else if(br<0) {
          w[ja] = v - br;
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OC_INDEX off = 6*ja;
        OXS_FFT_REAL_TYPE const * const sv = scratch + off;
        for(OC_INDEX jb=0;jb<6;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb];
          OXS_FFT_REAL_TYPE Uay = sv[jb+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
          OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
          OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
          OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx + BbDy;
          *(w[ja+2]+jb+1) = BaDy - BbDx;
          *(w[ja+3]+jb)   = BaDx - BbDy;
          *(w[ja+3]+jb+1) = BaDy + BbDx;
        }
      }

      i += 16;
    } while((--k)>0);

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      v[2*OFTV_VECSIZE*RTNFFTSIZE+j]   = v[j] - v[j+1];
      v[2*OFTV_VECSIZE*RTNFFTSIZE+1+j] = 0.0;
      v[j] += v[j+1];
      v[j+1] = 0.0;
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(OC_INDEX ia=2;ia<RTNFFTSIZE;ia+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[ia];
      const OXS_FFT_REAL_TYPE wy = UReals[ia+1];
      for(OC_INDEX ja=0;ja<2*OFTV_VECSIZE;ja+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+ja];
        const OXS_FFT_REAL_TYPE Ay = v[k1+ja+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+ja];
        const OXS_FFT_REAL_TYPE By = v[k2+ja+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+ja]   = 0.5*(Sx + C1);
        v[k1+ja+1] = 0.5*(C2 + Dy);
        v[k2+ja]   = 0.5*(Sx - C1);
        v[k2+ja+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize64
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{
#define RTNFFTSIZE 64
  const OC_INDEX block16_count = 4;
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {
    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = v[j];
      OXS_FFT_REAL_TYPE tmpBx0 = v[2*OFTV_VECSIZE*RTNFFTSIZE+j];
      v[j]   = 0.5*(tmpAx0 + tmpBx0);
      v[j+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[j+1] and v[2*OFTV_VECSIZE*RTNFFTSIZE+j+1] = 0.0
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    OC_INDEX i;
    for(i=2;i<RTNFFTSIZE;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[i];
      const OXS_FFT_REAL_TYPE wy = UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

    const PreorderTraversalState* sptr = ptsRadix4;

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    OC_INDEX stride = sptr->stride;

    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;
    // Note: The inverse roots are the complex conjugates of the
    // forward roots.

    // Do power-of-4 butterflies with "stride" == 64/4
    OXS_FFT_REAL_TYPE *const va = v;
    OXS_FFT_REAL_TYPE *const vb = va + stride;
    OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
    OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
    for(i=0;i<6;i+=2) {
      // w^0
      OXS_FFT_REAL_TYPE x0 = va[i];
      OXS_FFT_REAL_TYPE y0 = va[i+1];
      OXS_FFT_REAL_TYPE x1 = vb[i];
      OXS_FFT_REAL_TYPE y1 = vb[i+1];
      OXS_FFT_REAL_TYPE x2 = vc[i];
      OXS_FFT_REAL_TYPE x3 = vd[i];

      OXS_FFT_REAL_TYPE y2 = vc[i+1];

      OXS_FFT_REAL_TYPE sx1 = x0 + x2;
      OXS_FFT_REAL_TYPE dx1 = x0 - x2;

      OXS_FFT_REAL_TYPE sx2 = x1 + x3;
      OXS_FFT_REAL_TYPE dx2 = x1 - x3;

      OXS_FFT_REAL_TYPE y3 = vd[i+1];

      OXS_FFT_REAL_TYPE sy1 = y0 + y2;
      OXS_FFT_REAL_TYPE dy1 = y0 - y2;
      OXS_FFT_REAL_TYPE sy2 = y1 + y3;
      OXS_FFT_REAL_TYPE dy2 = y1 - y3;

      va[i] = sx1 + sx2;  va[i+1] = sy1 + sy2;
      vb[i] = sx1 - sx2;  vb[i+1] = sy1 - sy2;
      vc[i] = dx1 - dy2;  vc[i+1] = dy1 + dx2;
      vd[i] = dx1 + dy2;  vd[i+1] = dy1 - dx2;
    }
    while(i<stride) {
      for(OC_INDEX j=i;j-i<6;j+=2) {
        OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
        OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
        OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
        OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
        OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
        OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

        OXS_FFT_REAL_TYPE sx1 = x0 + x2; // sx1 := R4
        OXS_FFT_REAL_TYPE sx2 = x1 + x3; // sx2 := R5
#define dx1 x0
        dx1 -= x2;    // dx1 := R0
#define dx2 x1
        dx2 -= x3;    // dx2 := R1

#define y2 x2
        y2 = vc[j+1]; // y2 := R2
#define y3 x3
        y3 = vd[j+1]; // y3 := R3

        va[j]              = sx1 + sx2;
#define txa sx1
        txa -= sx2;     // txa := R4

#define sy1 sx2
        sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
        dy1 -= y2;      // dy1 := R6
        OXS_FFT_REAL_TYPE sy2 = y1 + y3; // sy2 := R8
#define dy2 y1
        dy2 -= y3;      // dy2 := R7

#define mx1 y3
        mx1 = U[0];   // mx1 := R3

        va[j+1]              = sy1 + sy2;
#define tya sy1
        tya -= sy2;     // tya := R5

#define mx1_txa y2
        mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
        mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
        my1 = U[1];    // my1 := R8

#define my1_tya tya
        my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
        my1_txa *= my1; // my1_txa := R4

        vb[j]     = mx1_txa + my1_tya;
        vb[j+1]   = mx1_tya - my1_txa;

#define txb mx1_txa
        txb = dx1 - dy2;  // txb := R2
#define txc dx1
        txc += dy2;       // txc := R0
#define tyb mx1_tya
        tyb = dy1 + dx2;  // tyb := R3
#define tyc dy1
        tyc -= dx2;       // tyc := R6

#define mx2 my1_txa
        mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
        mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
        mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
        my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
        my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
        my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
        Cx += my2_tyb; // Cx := R1
        vc[j]   = Cx;

#define mx3 mx2
        mx3 = U[4];  // mx3 := R4
#define my3 my2
        my3 = U[5];  // my3 := R5

        OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
        OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
        OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
        OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
        OXS_FFT_REAL_TYPE Cy = mx2_tyb - my2_txb; // Cy := R2
        OXS_FFT_REAL_TYPE Dx = mx3_txc + my3_tyc;
        OXS_FFT_REAL_TYPE Dy = mx3_tyc - my3_txc;

        vc[j+1] = Cy;
        vd[j]   = Dx;
        vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
      }
      i+=6;
      U+=6;
    }

    i = 0;
    OC_INDEX k=block16_count;
    do {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + 6*i;

      OC_INDEX j;

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
        OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j]        = aS0x + aS1x;
        scratch[j+1]      = aS0y + aS1y;
        scratch[j+3*8]    = aS0x - aS1x;
        scratch[j+1+3*8]  = aS0y - aS1y;

        scratch[j+3*16]   = aD0x - aD1y;
        scratch[j+1+3*16] = aD0y + aD1x;
        scratch[j+3*24]   = aD0x + aD1y;
        scratch[j+1+3*24] = aD0y - aD1x;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
        OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
        OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;

        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j+3*2]    = bS0x + bS1x;
        scratch[j+1+3*2]  = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j+3*10]   = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*10] = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

        scratch[j+3*18]   = tUb2x*alphax - tUb2y*alphay;
        scratch[j+1+3*18] = tUb2y*alphax + tUb2x*alphay;
        scratch[j+3*26]   = tUb3x*alphay - tUb3y*alphax;
        scratch[j+1+3*26] = tUb3y*alphay + tUb3x*alphax;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
        OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
        OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
        OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
        OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

        scratch[j+3*4]    = cS0x + cS1x;
        scratch[j+1+3*4]  = cS0y + cS1y;
        scratch[j+3*12]   = cS1y - cS0y;
        scratch[j+1+3*12] = cS0x - cS1x;

        scratch[j+3*20]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*20] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+3*28]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*28] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
        OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
        OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

        OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
        OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
        OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
        OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

        OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
        scratch[j+3*6]    = dS0x + dS1x;
        scratch[j+1+3*6]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

        scratch[j+3*14]   = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*14] = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
        OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

        scratch[j+3*22]   = tUd2x*alphay - tUd2y*alphax;
        scratch[j+1+3*22] = tUd2y*alphay + tUd2x*alphax;
        scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j+1+3*30] = tUd3x*alphay - tUd3y*alphax;
      }


      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      for(ja=0;ja<16;++ja) {
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        w[ja] = bv0+bs*ja;
        if(bitreverse[i+ja]!=0) {
          if(bitreverse[i+ja]>0) { // Swap
            memcpy(w[ja],v + bitreverse[i+ja],bs*sizeof(OXS_FFT_REAL_TYPE));
            w[ja] = v + bitreverse[i+ja];
          } else {
            w[ja] = v - bitreverse[i+ja];
          }
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OC_INDEX off = 6*ja;
        OXS_FFT_REAL_TYPE const * const sv = scratch + off;
        for(OC_INDEX jb=0;jb<6;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb];
          OXS_FFT_REAL_TYPE Uay = sv[jb+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
          OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
          OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
          OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx - BbDy;
          *(w[ja+2]+jb+1) = BaDy + BbDx;
          *(w[ja+3]+jb)   = BaDx + BbDy;
          *(w[ja+3]+jb+1) = BaDy - BbDx;
        }
      }

      i += 16;
    } while((--k)>0);

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-5;i+=6) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize64ZP
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{ // Version of ForwardRealToComplexFFTSize64 that assumes
  // rsize<=fftsize (=csize/2).
#define RTNFFTSIZE 64
  const OC_INDEX block16_count = 4;
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {
    {
      // Copy current row of rarr_in to carr_out, with zero padding.
      OC_INDEX i;
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      if(mult_base!=NULL) {
        const OXS_FFT_REAL_TYPE* mult = mult_base;
        mult_base += rstride/OFTV_VECSIZE;
        for(i=0;i<istop-5;i+=6) {
          carr_out[i]   = (*mult)*rarr_in[i];
          carr_out[i+2] = (*mult)*rarr_in[i+1];
          carr_out[i+4] = (*mult)*rarr_in[i+2];
          ++mult;
          carr_out[i+1] = (*mult)*rarr_in[i+3];
          carr_out[i+3] = (*mult)*rarr_in[i+4];
          carr_out[i+5] = (*mult)*rarr_in[i+5];
          ++mult;
        }
        if(i<istop) {
          carr_out[i]   = (*mult)*rarr_in[i];
          carr_out[i+1] = 0.0;
          carr_out[i+2] = (*mult)*rarr_in[i+1];
          carr_out[i+3] = 0.0;
          carr_out[i+4] = (*mult)*rarr_in[i+2];
          carr_out[i+5] = 0.0;
          i += 6;
        }
      } else {
        for(i=0;i<istop-5;i+=6) {
          carr_out[i]   = rarr_in[i];
          carr_out[i+2] = rarr_in[i+1];
          carr_out[i+4] = rarr_in[i+2];
          carr_out[i+1] = rarr_in[i+3];
          carr_out[i+3] = rarr_in[i+4];
          carr_out[i+5] = rarr_in[i+5];
        }
        if(i<istop) {
          carr_out[i]   = rarr_in[i];
          carr_out[i+1] = 0.0;
          carr_out[i+2] = rarr_in[i+1];
          carr_out[i+3] = 0.0;
          carr_out[i+4] = rarr_in[i+2];
          carr_out[i+5] = 0.0;
          i += 6;
        }
      }
      while(i<OFTV_VECSIZE*RTNFFTSIZE) carr_out[i++] = 0.0;
    }
    OXS_FFT_REAL_TYPE* v = carr_out; // Working vector.

    const PreorderTraversalState* sptr = ptsRadix4;

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    const OC_INDEX stride = sptr->stride;
    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;

    // Do power-of-4 butterflies with "stride" == 64/4
    OXS_FFT_REAL_TYPE *const va = v;
    OXS_FFT_REAL_TYPE *const vb = va + stride;
    OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
    OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      // w^0
      OXS_FFT_REAL_TYPE x0 = va[i];
      OXS_FFT_REAL_TYPE y0 = va[i+1];
      OXS_FFT_REAL_TYPE x1 = vb[i];
      OXS_FFT_REAL_TYPE y1 = vb[i+1];
      va[i]   = x0 + x1;        va[i+1] = y0 + y1;
      vb[i]   = x0 - x1;        vb[i+1] = y0 - y1;
      vc[i]   = x0 + y1;        vc[i+1] = y0 - x1;
      vd[i]   = x0 - y1;        vd[i+1] = y0 + x1;
    }
    while(i<stride) {
      for(OC_INDEX j=i;j-i<2*OFTV_VECSIZE;j+=2) {
        OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
        OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
        OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
        OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7

        va[j]                 = x0 + x1;
        OXS_FFT_REAL_TYPE txa = x0 - x1;     // txa := R4

        OXS_FFT_REAL_TYPE mx1 = U[0];   // mx1 := R3

        va[j+1]               = y0 + y1;
        OXS_FFT_REAL_TYPE tya = y0 - y1;     // tya := R5

        OXS_FFT_REAL_TYPE mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
        mx1_tya *= tya;    // mx1_tya := R3

        OXS_FFT_REAL_TYPE my1 = U[1];    // my1 := R8

#define my1_tya tya
        my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
        my1_txa *= my1; // my1_txa := R4

        vb[j]     = mx1_txa - my1_tya;
        vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
        txb = x0 + y1;  // txb := R2
#define txc x0
        txc -= y1;       // txc := R0
#define tyb mx1_tya
        tyb = y0 - x1;  // tyb := R3
#define tyc y0
        tyc += x1;       // tyc := R6

#define mx2 my1_txa
        mx2     = U[2]; //     mx2 := R4
#define mx2_tyb y1
        mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb x1
        mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
        my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
        my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
        my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
        Cx -= my2_tyb; // Cx := R1
        vc[j]   = Cx;

#define mx3 mx2
        mx3 = U[4];  // mx3 := R4
#define my3 my2
        my3 = U[5];  // my3 := R5

        OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
        OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
        OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
        OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0

        vc[j+1] = mx2_tyb + my2_txb;
        vd[j]   = mx3_txc - my3_tyc;
        vd[j+1] = my3_txc + mx3_tyc;

#undef mx1_tya
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
      }
      i+=6;
      U+=6;
    }

    i = 0;
    OC_INDEX k = block16_count;
    do {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + 6*i;

      OC_INDEX j;

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
        OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j]        = aS0x + aS1x;
        scratch[j+1]      = aS0y + aS1y;
        scratch[j+3*8]    = aS0x - aS1x;
        scratch[j+1+3*8]  = aS0y - aS1y;

        scratch[j+3*16]   = aD0x + aD1y;
        scratch[j+1+3*16] = aD0y - aD1x;
        scratch[j+3*24]   = aD0x - aD1y;
        scratch[j+1+3*24] = aD0y + aD1x;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
        OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
        OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j+3*2]    = bS0x + bS1x;
        scratch[j+1+3*2]  = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j+3*10]   = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*10] = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

        scratch[j+3*18]   = tUb2x*alphax + tUb2y*alphay;
        scratch[j+1+3*18] = tUb2y*alphax - tUb2x*alphay;
        scratch[j+3*26]   = tUb3x*alphay + tUb3y*alphax;
        scratch[j+1+3*26] = tUb3y*alphay - tUb3x*alphax;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
        OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
        OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
        OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
        OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

        scratch[j+3*4]    = cS1x + cS0x;
        scratch[j+1+3*4]  = cS0y + cS1y;
        scratch[j+3*12]   = cS0y - cS1y;
        scratch[j+1+3*12] = cS1x - cS0x;

        scratch[j+3*20]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*20] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+3*28]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*28] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
        OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
        OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

        OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
        OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
        OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
        OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

        OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
        scratch[j+3*6]    = dS1x + dS0x;
        scratch[j+1+3*6]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

        scratch[j+3*14]   = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*14] = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
        OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

        scratch[j+3*22]   = tUd2x*alphay + tUd2y*alphax;
        scratch[j+1+3*22] = tUd2y*alphay - tUd2x*alphax;
        scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j+1+3*30] = tUd3y*alphax - tUd3x*alphay;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      for(ja=0;ja<16;++ja) {
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        w[ja] = bv0+bs*ja;
        OC_INDEX br = bitreverse[i+ja];
        if(br>0) { // Swap
          memcpy(w[ja],v + br,bs*sizeof(OXS_FFT_REAL_TYPE));
          w[ja] = v + br;
        } else if(br<0) {
          w[ja] = v - br;
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OC_INDEX off = 6*ja;
        OXS_FFT_REAL_TYPE const * const sv = scratch + off;
        for(OC_INDEX jb=0;jb<6;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb];
          OXS_FFT_REAL_TYPE Uay = sv[jb+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
          OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
          OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
          OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx + BbDy;
          *(w[ja+2]+jb+1) = BaDy - BbDx;
          *(w[ja+3]+jb)   = BaDx - BbDy;
          *(w[ja+3]+jb+1) = BaDy + BbDx;
        }
      }

      i += 16;
    } while((--k)>0);

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      v[2*OFTV_VECSIZE*RTNFFTSIZE+j]   = v[j] - v[j+1];
      v[2*OFTV_VECSIZE*RTNFFTSIZE+1+j] = 0.0;
      v[j] += v[j+1];
      v[j+1] = 0.0;
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(OC_INDEX ia=2;ia<RTNFFTSIZE;ia+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[ia];
      const OXS_FFT_REAL_TYPE wy = UReals[ia+1];
      for(OC_INDEX ja=0;ja<2*OFTV_VECSIZE;ja+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+ja];
        const OXS_FFT_REAL_TYPE Ay = v[k1+ja+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+ja];
        const OXS_FFT_REAL_TYPE By = v[k2+ja+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+ja]   = 0.5*(Sx + C1);
        v[k1+ja+1] = 0.5*(C2 + Dy);
        v[k2+ja]   = 0.5*(Sx - C1);
        v[k2+ja+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize64ZP
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{ // Version of InverseComplexToRealFFT64 which assumes that
  // rsize<=fftsize, i.e., that the last half of the transform
  // is to be thrown away.
#define RTNFFTSIZE 64
  const OC_INDEX block16_count = 4;
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {
    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = v[j];
      OXS_FFT_REAL_TYPE tmpBx0 = v[2*OFTV_VECSIZE*RTNFFTSIZE+j];
      v[j]   = 0.5*(tmpAx0 + tmpBx0);
      v[j+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[j+1] and v[2*OFTV_VECSIZE*RTNFFTSIZE+j+1] = 0.0
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    OC_INDEX i;
    for(i=2;i<RTNFFTSIZE;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[i];
      const OXS_FFT_REAL_TYPE wy = UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

    const PreorderTraversalState* sptr = ptsRadix4;

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    OC_INDEX stride = sptr->stride;

    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;
    // Note: The inverse roots are the complex conjugates of the
    // forward roots.

    // Do power-of-4 butterflies with "stride" == 64/4
    OXS_FFT_REAL_TYPE *const va = v;
    OXS_FFT_REAL_TYPE *const vb = va + stride;
    OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
    OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
    for(i=0;i<6;i+=2) {
      // w^0
      OXS_FFT_REAL_TYPE x0 = va[i];
      OXS_FFT_REAL_TYPE y0 = va[i+1];
      OXS_FFT_REAL_TYPE x1 = vb[i];
      OXS_FFT_REAL_TYPE y1 = vb[i+1];
      OXS_FFT_REAL_TYPE x2 = vc[i];
      OXS_FFT_REAL_TYPE x3 = vd[i];

      OXS_FFT_REAL_TYPE y2 = vc[i+1];

      OXS_FFT_REAL_TYPE sx1 = x0 + x2;
      OXS_FFT_REAL_TYPE dx1 = x0 - x2;

      OXS_FFT_REAL_TYPE sx2 = x1 + x3;
      OXS_FFT_REAL_TYPE dx2 = x1 - x3;

      OXS_FFT_REAL_TYPE y3 = vd[i+1];

      OXS_FFT_REAL_TYPE sy1 = y0 + y2;
      OXS_FFT_REAL_TYPE dy1 = y0 - y2;
      OXS_FFT_REAL_TYPE sy2 = y1 + y3;
      OXS_FFT_REAL_TYPE dy2 = y1 - y3;

      va[i] = sx1 + sx2;  va[i+1] = sy1 + sy2;
      vb[i] = sx1 - sx2;  vb[i+1] = sy1 - sy2;
      vc[i] = dx1 - dy2;  vc[i+1] = dy1 + dx2;
      vd[i] = dx1 + dy2;  vd[i+1] = dy1 - dx2;
    }
    while(i<stride) {
      for(OC_INDEX j=i;j-i<6;j+=2) {
        OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
        OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
        OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
        OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
        OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
        OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

        OXS_FFT_REAL_TYPE sx1 = x0 + x2; // sx1 := R4
        OXS_FFT_REAL_TYPE sx2 = x1 + x3; // sx2 := R5
#define dx1 x0
        dx1 -= x2;    // dx1 := R0
#define dx2 x1
        dx2 -= x3;    // dx2 := R1

#define y2 x2
        y2 = vc[j+1]; // y2 := R2
#define y3 x3
        y3 = vd[j+1]; // y3 := R3

        va[j]              = sx1 + sx2;
#define txa sx1
        txa -= sx2;     // txa := R4

#define sy1 sx2
        sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
        dy1 -= y2;      // dy1 := R6
        OXS_FFT_REAL_TYPE sy2 = y1 + y3; // sy2 := R8
#define dy2 y1
        dy2 -= y3;      // dy2 := R7

#define mx1 y3
        mx1 = U[0];   // mx1 := R3

        va[j+1]              = sy1 + sy2;
#define tya sy1
        tya -= sy2;     // tya := R5

#define mx1_txa y2
        mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
        mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
        my1 = U[1];    // my1 := R8

#define my1_tya tya
        my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
        my1_txa *= my1; // my1_txa := R4

        vb[j]     = mx1_txa + my1_tya;
        vb[j+1]   = mx1_tya - my1_txa;

#define txb mx1_txa
        txb = dx1 - dy2;  // txb := R2
#define txc dx1
        txc += dy2;       // txc := R0
#define tyb mx1_tya
        tyb = dy1 + dx2;  // tyb := R3
#define tyc dy1
        tyc -= dx2;       // tyc := R6

#define mx2 my1_txa
        mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
        mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
        mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
        my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
        my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
        my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
        Cx += my2_tyb; // Cx := R1
        vc[j]   = Cx;

#define mx3 mx2
        mx3 = U[4];  // mx3 := R4
#define my3 my2
        my3 = U[5];  // my3 := R5

        OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
        OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
        OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
        OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
        OXS_FFT_REAL_TYPE Cy = mx2_tyb - my2_txb; // Cy := R2
        OXS_FFT_REAL_TYPE Dx = mx3_txc + my3_tyc;
        OXS_FFT_REAL_TYPE Dy = mx3_tyc - my3_txc;

        vc[j+1] = Cy;
        vd[j]   = Dx;
        vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
      }
      i+=6;
      U+=6;
    }

    i = 0;
    OC_INDEX k=block16_count;
    do {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + 6*i;

      OC_INDEX j;

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
        OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j]        = aS0x + aS1x;
        scratch[j+1]      = aS0y + aS1y;
        scratch[j+3*8]    = aS0x - aS1x;
        scratch[j+1+3*8]  = aS0y - aS1y;

        scratch[j+3*16]   = aD0x - aD1y;
        scratch[j+1+3*16] = aD0y + aD1x;
        scratch[j+3*24]   = aD0x + aD1y;
        scratch[j+1+3*24] = aD0y - aD1x;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
        OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
        OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;

        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j+3*2]    = bS0x + bS1x;
        scratch[j+1+3*2]  = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j+3*10]   = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*10] = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

        scratch[j+3*18]   = tUb2x*alphax - tUb2y*alphay;
        scratch[j+1+3*18] = tUb2y*alphax + tUb2x*alphay;
        scratch[j+3*26]   = tUb3x*alphay - tUb3y*alphax;
        scratch[j+1+3*26] = tUb3y*alphay + tUb3x*alphax;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
        OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
        OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
        OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
        OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

        scratch[j+3*4]    = cS0x + cS1x;
        scratch[j+1+3*4]  = cS0y + cS1y;
        scratch[j+3*12]   = cS1y - cS0y;
        scratch[j+1+3*12] = cS0x - cS1x;

        scratch[j+3*20]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*20] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+3*28]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*28] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
        OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
        OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

        OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
        OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
        OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
        OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

        OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
        scratch[j+3*6]    = dS0x + dS1x;
        scratch[j+1+3*6]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

        scratch[j+3*14]   = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*14] = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
        OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

        scratch[j+3*22]   = tUd2x*alphay - tUd2y*alphax;
        scratch[j+1+3*22] = tUd2y*alphay + tUd2x*alphax;
        scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j+1+3*30] = tUd3x*alphay - tUd3y*alphax;
      }


      // Bit reversal.  This "...ZP" variant skips reversals
      // that affect what ends up in the upper half of the
      // results, since those are to be thrown away.
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i<RTNFFTSIZE/2) {
        for(ja=0;ja<16;ja+=2) {
          const OC_INDEX bs = 2*OFTV_VECSIZE;
          w[ja] = bv0+bs*ja;
          if(bitreverse[i+ja]!=0) {
            if(bitreverse[i+ja]>0) { // Swap
              memcpy(w[ja],v + bitreverse[i+ja],bs*sizeof(OXS_FFT_REAL_TYPE));
              w[ja] = v + bitreverse[i+ja];
            } else {
              w[ja] = v - bitreverse[i+ja];
            }
          }
        }
      } else {
        for(ja=0;ja<16;ja+=2) w[ja] = v+bitreverse[i+ja];
      }

      // Do 4 lower (bottom/final) -level dragonflies
      // This "...ZP" variant assumes that the last half of the
      // results are to be thrown away, or equivalently, that
      // the pre-reversal odd (ja%2==1) results are not used.
      for(ja=0;ja<16;ja+=4) {
        OC_INDEX off = 6*ja;
        OXS_FFT_REAL_TYPE const * const sv = scratch + off;
        for(OC_INDEX jb=0;jb<6;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb];
          OXS_FFT_REAL_TYPE Uay = sv[jb+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
          OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
          OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
          OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+2]+jb)   = BaDx - BbDy;
          *(w[ja+2]+jb+1) = BaDy + BbDx;
        }
      }

      i += 16;
    } while((--k)>0);

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-5;i+=6) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize32
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{
#define RTNFFTSIZE 32
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {

    // Copy current row of rarr_in to carr_out, with zero padding.
    OC_INDEX i;
    const OC_INDEX istop = OFTV_VECSIZE*rsize;
    if(mult_base!=NULL) {
      const OXS_FFT_REAL_TYPE* mult = mult_base;
      mult_base += rstride/OFTV_VECSIZE;
      for(i=0;i<istop-5;i+=6) {
        carr_out[i]   = (*mult)*rarr_in[i];
        carr_out[i+2] = (*mult)*rarr_in[i+1];
        carr_out[i+4] = (*mult)*rarr_in[i+2];
        ++mult;
        carr_out[i+1] = (*mult)*rarr_in[i+3];
        carr_out[i+3] = (*mult)*rarr_in[i+4];
        carr_out[i+5] = (*mult)*rarr_in[i+5];
        ++mult;
      }
      if(i<istop) {
        carr_out[i]   = (*mult)*rarr_in[i];
        carr_out[i+1] = 0.0;
        carr_out[i+2] = (*mult)*rarr_in[i+1];
        carr_out[i+3] = 0.0;
        carr_out[i+4] = (*mult)*rarr_in[i+2];
        carr_out[i+5] = 0.0;
        i += 6;
      }
    } else {
      for(i=0;i<istop-5;i+=6) {
        carr_out[i]   = rarr_in[i];
        carr_out[i+2] = rarr_in[i+1];
        carr_out[i+4] = rarr_in[i+2];
        carr_out[i+1] = rarr_in[i+3];
        carr_out[i+3] = rarr_in[i+4];
        carr_out[i+5] = rarr_in[i+5];
      }
      if(i<istop) {
        carr_out[i]   = rarr_in[i];
        carr_out[i+1] = 0.0;
        carr_out[i+2] = rarr_in[i+1];
        carr_out[i+3] = 0.0;
        carr_out[i+4] = rarr_in[i+2];
        carr_out[i+5] = 0.0;
        i += 6;
      }
    }
    while(i<2*OFTV_VECSIZE*RTNFFTSIZE)carr_out[i++] = 0.0;

    // Top-level 32-block pass
    OXS_FFT_REAL_TYPE* v = carr_out; // Working vector.
    { // i=0
      for(OC_INDEX j2=0 ; j2<2*OFTV_VECSIZE ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = v[j2];
        OXS_FFT_REAL_TYPE ay0 = v[j2+1];
        OXS_FFT_REAL_TYPE cx0 = v[j2+OFTV_VECSIZE*16];
        OXS_FFT_REAL_TYPE cy0 = v[j2+OFTV_VECSIZE*16+1];
        OXS_FFT_REAL_TYPE ax1 = v[j2+OFTV_VECSIZE*32];
        OXS_FFT_REAL_TYPE ay1 = v[j2+OFTV_VECSIZE*32+1];
        OXS_FFT_REAL_TYPE cx1 = v[j2+OFTV_VECSIZE*48];
        OXS_FFT_REAL_TYPE cy1 = v[j2+OFTV_VECSIZE*48+1];
        v[j2]                    = ax0 + ax1;
        v[j2+1]                  = ay0 + ay1;
        v[j2+OFTV_VECSIZE*32]   = ax0 - ax1;
        v[j2+OFTV_VECSIZE*32+1] = ay0 - ay1;
        v[j2+OFTV_VECSIZE*16]   = cx0 + cx1;
        v[j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
        v[j2+OFTV_VECSIZE*48]   = cy0 - cy1;
        v[j2+OFTV_VECSIZE*48+1] = cx1 - cx0;
      }
    }

    OXS_FFT_REAL_TYPE const *U = UForwardRadix4;
    // NB: U points to the tail of the UForwardRadix4 array,
    // which is a segment holding the 32nd roots of unity.  That
    // segment doesn't contain the (1,0) root, so the access below
    // is to U - 2.
    for(i=2;i<16;i+=2) {
      OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*i;
      const OXS_FFT_REAL_TYPE amx = U[i-2];
      const OXS_FFT_REAL_TYPE amy = U[i-1];
      for(OC_INDEX j2=0 ; j2<2*OFTV_VECSIZE ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = va[j2];
        OXS_FFT_REAL_TYPE ay0 = va[j2+1];
        OXS_FFT_REAL_TYPE ax1 = va[j2+OFTV_VECSIZE*32];
        OXS_FFT_REAL_TYPE ay1 = va[j2+OFTV_VECSIZE*32+1];

        OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
        va[j2]        = ax0 + ax1;
        va[j2+1]      = ay0 + ay1;
        OXS_FFT_REAL_TYPE adify = ay0 - ay1;
        va[j2+OFTV_VECSIZE*32]   = amx*adifx - amy*adify;
        va[j2+OFTV_VECSIZE*32+1] = amx*adify + amy*adifx;

        OXS_FFT_REAL_TYPE cx0 = va[j2+OFTV_VECSIZE*16];
        OXS_FFT_REAL_TYPE cy0 = va[j2+OFTV_VECSIZE*16+1];
        OXS_FFT_REAL_TYPE cx1 = va[j2+OFTV_VECSIZE*48];
        OXS_FFT_REAL_TYPE cy1 = va[j2+OFTV_VECSIZE*48+1];

        OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
        va[j2+OFTV_VECSIZE*16]   = cx0 + cx1;
        va[j2+OFTV_VECSIZE*16+1] = cy0 + cy1;
        OXS_FFT_REAL_TYPE cdify = cy0 - cy1;
        va[j2+OFTV_VECSIZE*48]   = amx*cdify + amy*cdifx;
        va[j2+OFTV_VECSIZE*48+1] = amy*cdify - amx*cdifx;
      }
    }

    for(i=0;i<32;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + 2*OFTV_VECSIZE*i;

      OC_INDEX j;

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
        OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j]        = aS0x + aS1x;
        scratch[j+1]      = aS0y + aS1y;
        scratch[j+3*8]    = aS0x - aS1x;
        scratch[j+1+3*8]  = aS0y - aS1y;

        scratch[j+3*16]   = aD0x + aD1y;
        scratch[j+1+3*16] = aD0y - aD1x;
        scratch[j+3*24]   = aD0x - aD1y;
        scratch[j+1+3*24] = aD0y + aD1x;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
        OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
        OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j+3*2]    = bS0x + bS1x;
        scratch[j+1+3*2]  = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j+3*10]   = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*10] = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

        scratch[j+3*18]   = tUb2x*alphax + tUb2y*alphay;
        scratch[j+1+3*18] = tUb2y*alphax - tUb2x*alphay;
        scratch[j+3*26]   = tUb3x*alphay + tUb3y*alphax;
        scratch[j+1+3*26] = tUb3y*alphay - tUb3x*alphax;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
        OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
        OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
        OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
        OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

        scratch[j+3*4]    = cS1x + cS0x;
        scratch[j+1+3*4]  = cS0y + cS1y;
        scratch[j+3*12]   = cS0y - cS1y;
        scratch[j+1+3*12] = cS1x - cS0x;

        scratch[j+3*20]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*20] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+3*28]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*28] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
        OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
        OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

        OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
        OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
        OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
        OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

        OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
        scratch[j+3*6]    = dS1x + dS0x;
        scratch[j+1+3*6]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

        scratch[j+3*14]   = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*14] = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
        OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

        scratch[j+3*22]   = tUd2x*alphay + tUd2y*alphax;
        scratch[j+1+3*22] = tUd2y*alphay - tUd2x*alphax;
        scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j+1+3*30] = tUd3y*alphax - tUd3x*alphay;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i==0) {
        // Lower pass; no swaps, two pair interchanges
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        for(ja=0;ja<16;++ja) w[ja] = bv0+bs*ja;
        w[2]  = bv0 + bs*8;
        w[6]  = bv0 + bs*12;
        w[8]  = bv0 + bs*2;
        w[12] = bv0 + bs*6;
      } else {
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        for(ja=0;ja<16;ja+=2) {
          // Upper pass; all even offsets swap.
          //  Odd offsets don't swap, but might interchange
          OC_INDEX br0 = bitreverse[i+ja];
          memcpy(bv0+bs*ja,v + br0,bs*sizeof(OXS_FFT_REAL_TYPE));
          w[ja] = v + br0;

          OC_INDEX br1 = bitreverse[i+ja+1];
          if(br1==0) w[ja+1] = bv0+bs*(ja+1);
          else       w[ja+1] = v - br1;
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OC_INDEX off = 6*ja;
        OXS_FFT_REAL_TYPE const * const sv = scratch + off;
        for(OC_INDEX jb=0;jb<6;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb];
          OXS_FFT_REAL_TYPE Uay = sv[jb+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
          OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
          OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
          OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx + BbDy;
          *(w[ja+2]+jb+1) = BaDy - BbDx;
          *(w[ja+3]+jb)   = BaDx - BbDy;
          *(w[ja+3]+jb+1) = BaDy + BbDx;
        }
      }

    }

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      v[2*OFTV_VECSIZE*RTNFFTSIZE+j]   = v[j] - v[j+1];
      v[2*OFTV_VECSIZE*RTNFFTSIZE+1+j] = 0.0;
      v[j] += v[j+1];
      v[j+1] = 0.0;
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(OC_INDEX ia=2;ia<RTNFFTSIZE;ia+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[ia];
      const OXS_FFT_REAL_TYPE wy = UReals[ia+1];
      for(OC_INDEX ja=0;ja<2*OFTV_VECSIZE;ja+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+ja];
        const OXS_FFT_REAL_TYPE Ay = v[k1+ja+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+ja];
        const OXS_FFT_REAL_TYPE By = v[k2+ja+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+ja]   = 0.5*(Sx + C1);
        v[k1+ja+1] = 0.5*(C2 + Dy);
        v[k2+ja]   = 0.5*(Sx - C1);
        v[k2+ja+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*32+1] *= -1;
    v[OFTV_VECSIZE*32+3] *= -1;
    v[OFTV_VECSIZE*32+5] *= -1;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize32
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{
#define RTNFFTSIZE 32
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = v[i];
      OXS_FFT_REAL_TYPE tmpBx0 = v[2*OFTV_VECSIZE*RTNFFTSIZE+i];
      v[i]   = 0.5*(tmpAx0 + tmpBx0);
      v[i+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[i+1] and v[2*OFTV_VECSIZE*RTNFFTSIZE+i+1] = 0.0
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(i=2;i<RTNFFTSIZE;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[i];
      const OXS_FFT_REAL_TYPE wy = UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

    // Top-level 32-block pass
    { // i=0
      for(OC_INDEX j=0 ; j<2*OFTV_VECSIZE ; j+=2) {
        OXS_FFT_REAL_TYPE ax0 = v[j];
        OXS_FFT_REAL_TYPE ay0 = v[j+1];
        OXS_FFT_REAL_TYPE ax1 = v[j+OFTV_VECSIZE*32];
        OXS_FFT_REAL_TYPE ay1 = v[j+OFTV_VECSIZE*32+1];
        v[j]                    = ax0 + ax1;
        v[j+1]                  = ay0 + ay1;
        v[j+OFTV_VECSIZE*32]   = ax0 - ax1;
        v[j+OFTV_VECSIZE*32+1] = ay0 - ay1;
        OXS_FFT_REAL_TYPE cx0 = v[j+OFTV_VECSIZE*16];
        OXS_FFT_REAL_TYPE cy0 = v[j+OFTV_VECSIZE*16+1];
        OXS_FFT_REAL_TYPE cx1 = v[j+OFTV_VECSIZE*48];
        OXS_FFT_REAL_TYPE cy1 = v[j+OFTV_VECSIZE*48+1];
        v[j+OFTV_VECSIZE*16]   = cx0 + cx1;
        v[j+OFTV_VECSIZE*16+1] = cy0 + cy1;
        v[j+OFTV_VECSIZE*48]   = cy1 - cy0;
        v[j+OFTV_VECSIZE*48+1] = cx0 - cx1;
      }
    }

    OXS_FFT_REAL_TYPE const *U = UForwardRadix4;
    // U points to the tail of the UForwardRadix4 array, which is a
    // segment holding the 32nd roots of unity.  That segment doesn't
    // contain the (1,0) root, so the access below is to U - 2.  Again,
    // the inverse roots are the complex conjugates of the forward
    // roots.
    for(i=2;i<16;i+=2) {
      OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*i;
      const OXS_FFT_REAL_TYPE amx = U[i-2];
      const OXS_FFT_REAL_TYPE amy = U[i-1];
      for(OC_INDEX j=0 ; j<2*OFTV_VECSIZE ; j+=2) {
        OXS_FFT_REAL_TYPE ax0 = va[j];
        OXS_FFT_REAL_TYPE ay0 = va[j+1];
        OXS_FFT_REAL_TYPE ax1 = va[j+2*OFTV_VECSIZE*16];
        OXS_FFT_REAL_TYPE ay1 = va[j+2*OFTV_VECSIZE*16+1];

        OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
        va[j]        = ax0 + ax1;
        va[j+1]      = ay0 + ay1;
        OXS_FFT_REAL_TYPE adify = ay0 - ay1;
        va[j+2*OFTV_VECSIZE*16]   = amx*adifx + amy*adify;
        va[j+2*OFTV_VECSIZE*16+1] = amx*adify - amy*adifx;

        OXS_FFT_REAL_TYPE cx0 = va[j+2*OFTV_VECSIZE*8];
        OXS_FFT_REAL_TYPE cy0 = va[j+2*OFTV_VECSIZE*8+1];
        OXS_FFT_REAL_TYPE cx1 = va[j+2*OFTV_VECSIZE*24];
        OXS_FFT_REAL_TYPE cy1 = va[j+2*OFTV_VECSIZE*24+1];

        OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
        va[j+2*OFTV_VECSIZE*8]   = cx0 + cx1;
        va[j+2*OFTV_VECSIZE*8+1] = cy0 + cy1;
        OXS_FFT_REAL_TYPE cdify = cy0 - cy1;
        va[j+2*OFTV_VECSIZE*24]   = amy*cdifx - amx*cdify;
        va[j+2*OFTV_VECSIZE*24+1] = amy*cdify + amx*cdifx;
      }
    }


    for(i=0;i<32;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + 2*OFTV_VECSIZE*i;

      OC_INDEX j;

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
        OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j]        = aS0x + aS1x;
        scratch[j+1]      = aS0y + aS1y;
        scratch[j+3*8]    = aS0x - aS1x;
        scratch[j+1+3*8]  = aS0y - aS1y;

        scratch[j+3*16]   = aD0x - aD1y;
        scratch[j+1+3*16] = aD0y + aD1x;
        scratch[j+3*24]   = aD0x + aD1y;
        scratch[j+1+3*24] = aD0y - aD1x;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
        OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
        OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;

        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j+3*2]    = bS0x + bS1x;
        scratch[j+1+3*2]  = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j+3*10]   = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*10] = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

        scratch[j+3*18]   = tUb2x*alphax - tUb2y*alphay;
        scratch[j+1+3*18] = tUb2y*alphax + tUb2x*alphay;
        scratch[j+3*26]   = tUb3x*alphay - tUb3y*alphax;
        scratch[j+1+3*26] = tUb3y*alphay + tUb3x*alphax;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
        OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
        OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
        OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
        OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

        scratch[j+3*4]    = cS0x + cS1x;
        scratch[j+1+3*4]  = cS0y + cS1y;
        scratch[j+3*12]   = cS1y - cS0y;
        scratch[j+1+3*12] = cS0x - cS1x;

        scratch[j+3*20]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*20] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+3*28]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*28] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
        OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
        OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

        OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
        OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
        OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
        OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

        OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
        scratch[j+3*6]    = dS0x + dS1x;
        scratch[j+1+3*6]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

        scratch[j+3*14]   = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*14] = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
        OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

        scratch[j+3*22]   = tUd2x*alphay - tUd2y*alphax;
        scratch[j+1+3*22] = tUd2y*alphay + tUd2x*alphax;
        scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j+1+3*30] = tUd3x*alphay - tUd3y*alphax;
      }


      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i==0) {
        // Lower pass; no swaps, two pair interchanges
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        for(ja=0;ja<16;++ja) w[ja] = bv0+bs*ja;
        w[2]  = bv0 + bs*8;
        w[6]  = bv0 + bs*12;
        w[8]  = bv0 + bs*2;
        w[12] = bv0 + bs*6;
      } else {
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        for(ja=0;ja<16;ja+=2) {
          // Upper pass; all even offsets swap.
          //  Odd offsets don't swap, but might interchange
          OC_INDEX br0 = bitreverse[i+ja];
          memcpy(bv0+bs*ja,v + br0,bs*sizeof(OXS_FFT_REAL_TYPE));
          w[ja] = v + br0;
          OC_INDEX br1 = bitreverse[i+ja+1];
          if(br1==0) w[ja+1] = bv0+bs*(ja+1);
          else       w[ja+1] = v - br1;
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OC_INDEX off = 6*ja;
        OXS_FFT_REAL_TYPE const * const sv = scratch + off;
        for(OC_INDEX jb=0;jb<6;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb];
          OXS_FFT_REAL_TYPE Uay = sv[jb+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
          OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
          OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
          OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx - BbDy;
          *(w[ja+2]+jb+1) = BaDy + BbDx;
          *(w[ja+3]+jb)   = BaDx + BbDy;
          *(w[ja+3]+jb+1) = BaDy - BbDx;
        }
      }

    }

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-2*OFTV_VECSIZE+1;i+=2*OFTV_VECSIZE) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize32ZP
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{ // Version of ForwardRealToComplexFFTSize32 that assumes
  // rsize<=fftsize (=csize/2).
#define RTNFFTSIZE 32
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {

    // Copy current row of rarr_in to carr_out, with zero padding.
    OC_INDEX i;
    const OC_INDEX istop = OFTV_VECSIZE*rsize;
    if(mult_base!=NULL) {
      const OXS_FFT_REAL_TYPE* mult = mult_base;
      mult_base += rstride/OFTV_VECSIZE;
      for(i=0;i<istop-5;i+=6) {
        carr_out[i]   = (*mult)*rarr_in[i];
        carr_out[i+2] = (*mult)*rarr_in[i+1];
        carr_out[i+4] = (*mult)*rarr_in[i+2];
        ++mult;
        carr_out[i+1] = (*mult)*rarr_in[i+3];
        carr_out[i+3] = (*mult)*rarr_in[i+4];
        carr_out[i+5] = (*mult)*rarr_in[i+5];
        ++mult;
      }
      if(i<istop) {
        carr_out[i]   = (*mult)*rarr_in[i];
        carr_out[i+1] = 0.0;
        carr_out[i+2] = (*mult)*rarr_in[i+1];
        carr_out[i+3] = 0.0;
        carr_out[i+4] = (*mult)*rarr_in[i+2];
        carr_out[i+5] = 0.0;
        i += 6;
      }
    } else {
      for(i=0;i<istop-5;i+=6) {
        carr_out[i]   = rarr_in[i];
        carr_out[i+2] = rarr_in[i+1];
        carr_out[i+4] = rarr_in[i+2];
        carr_out[i+1] = rarr_in[i+3];
        carr_out[i+3] = rarr_in[i+4];
        carr_out[i+5] = rarr_in[i+5];
      }
      if(i<istop) {
        carr_out[i]   = rarr_in[i];
        carr_out[i+1] = 0.0;
        carr_out[i+2] = rarr_in[i+1];
        carr_out[i+3] = 0.0;
        carr_out[i+4] = rarr_in[i+2];
        carr_out[i+5] = 0.0;
        i += 6;
      }
    }
    while(i<OFTV_VECSIZE*RTNFFTSIZE)carr_out[i++] = 0.0;

    // Top-level 32-block pass
    OXS_FFT_REAL_TYPE* v = carr_out; // Working vector.
    { // i=0
      for(OC_INDEX j2=0 ; j2<2*OFTV_VECSIZE ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = v[j2];
        OXS_FFT_REAL_TYPE ay0 = v[j2+1];
        OXS_FFT_REAL_TYPE cx0 = v[j2+OFTV_VECSIZE*16];
        OXS_FFT_REAL_TYPE cy0 = v[j2+OFTV_VECSIZE*16+1];
        v[j2+OFTV_VECSIZE*32]   =  ax0;
        v[j2+OFTV_VECSIZE*32+1] =  ay0;
        v[j2+OFTV_VECSIZE*48]   =  cy0;
        v[j2+OFTV_VECSIZE*48+1] = -cx0;
      }
    }

    OXS_FFT_REAL_TYPE const *U = UForwardRadix4;
    // NB: U points to the tail of the UForwardRadix4 array,
    // which is a segment holding the 32nd roots of unity.  That
    // segment doesn't contain the (1,0) root, so the access below
    // is to U - 2.
    for(i=2;i<16;i+=2) {
      OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*i;
      const OXS_FFT_REAL_TYPE amx = U[i-2];
      const OXS_FFT_REAL_TYPE amy = U[i-1];
      for(OC_INDEX j2=0 ; j2<2*OFTV_VECSIZE ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = va[j2];
        OXS_FFT_REAL_TYPE ay0 = va[j2+1];
        OXS_FFT_REAL_TYPE cx0 = va[j2+OFTV_VECSIZE*16];
        OXS_FFT_REAL_TYPE cy0 = va[j2+OFTV_VECSIZE*16+1];
        va[j2+OFTV_VECSIZE*32]   = amx*ax0 - amy*ay0;
        va[j2+OFTV_VECSIZE*32+1] = amx*ay0 + amy*ax0;
        va[j2+OFTV_VECSIZE*48]   = amx*cy0 + amy*cx0;
        va[j2+OFTV_VECSIZE*48+1] = amy*cy0 - amx*cx0;
      }
    }

    for(i=0;i<32;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + 2*OFTV_VECSIZE*i;

      OC_INDEX j;

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
        OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j]        = aS0x + aS1x;
        scratch[j+1]      = aS0y + aS1y;
        scratch[j+3*8]    = aS0x - aS1x;
        scratch[j+1+3*8]  = aS0y - aS1y;

        scratch[j+3*16]   = aD0x + aD1y;
        scratch[j+1+3*16] = aD0y - aD1x;
        scratch[j+3*24]   = aD0x - aD1y;
        scratch[j+1+3*24] = aD0y + aD1x;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
        OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
        OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j+3*2]    = bS0x + bS1x;
        scratch[j+1+3*2]  = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j+3*10]   = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*10] = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

        scratch[j+3*18]   = tUb2x*alphax + tUb2y*alphay;
        scratch[j+1+3*18] = tUb2y*alphax - tUb2x*alphay;
        scratch[j+3*26]   = tUb3x*alphay + tUb3y*alphax;
        scratch[j+1+3*26] = tUb3y*alphay - tUb3x*alphax;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
        OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
        OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
        OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
        OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

        scratch[j+3*4]    = cS1x + cS0x;
        scratch[j+1+3*4]  = cS0y + cS1y;
        scratch[j+3*12]   = cS0y - cS1y;
        scratch[j+1+3*12] = cS1x - cS0x;

        scratch[j+3*20]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*20] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+3*28]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*28] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
        OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
        OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

        OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
        OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
        OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
        OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

        OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
        scratch[j+3*6]    = dS1x + dS0x;
        scratch[j+1+3*6]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

        scratch[j+3*14]   = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*14] = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
        OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

        scratch[j+3*22]   = tUd2x*alphay + tUd2y*alphax;
        scratch[j+1+3*22] = tUd2y*alphay - tUd2x*alphax;
        scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j+1+3*30] = tUd3y*alphax - tUd3x*alphay;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i==0) {
        // Lower pass; no swaps, two pair interchanges
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        for(ja=0;ja<16;++ja) w[ja] = bv0+bs*ja;
        w[2]  = bv0 + bs*8;
        w[6]  = bv0 + bs*12;
        w[8]  = bv0 + bs*2;
        w[12] = bv0 + bs*6;
      } else {
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        for(ja=0;ja<16;ja+=2) {
          // Upper pass; all even offsets swap.
          //  Odd offsets don't swap, but might interchange
          OC_INDEX br0 = bitreverse[i+ja];
          memcpy(bv0+bs*ja,v + br0,bs*sizeof(OXS_FFT_REAL_TYPE));
          w[ja] = v + br0;

          OC_INDEX br1 = bitreverse[i+ja+1];
          if(br1==0) w[ja+1] = bv0+bs*(ja+1);
          else       w[ja+1] = v - br1;
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OC_INDEX off = 6*ja;
        OXS_FFT_REAL_TYPE const * const sv = scratch + off;
        for(OC_INDEX jb=0;jb<6;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb];
          OXS_FFT_REAL_TYPE Uay = sv[jb+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
          OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
          OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
          OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx + BbDy;
          *(w[ja+2]+jb+1) = BaDy - BbDx;
          *(w[ja+3]+jb)   = BaDx - BbDy;
          *(w[ja+3]+jb+1) = BaDy + BbDx;
        }
      }

    }

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
      v[2*OFTV_VECSIZE*RTNFFTSIZE+j]   = v[j] - v[j+1];
      v[2*OFTV_VECSIZE*RTNFFTSIZE+1+j] = 0.0;
      v[j] += v[j+1];
      v[j+1] = 0.0;
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(OC_INDEX ia=2;ia<RTNFFTSIZE;ia+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[ia];
      const OXS_FFT_REAL_TYPE wy = UReals[ia+1];
      for(OC_INDEX ja=0;ja<2*OFTV_VECSIZE;ja+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+ja];
        const OXS_FFT_REAL_TYPE Ay = v[k1+ja+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+ja];
        const OXS_FFT_REAL_TYPE By = v[k2+ja+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+ja]   = 0.5*(Sx + C1);
        v[k1+ja+1] = 0.5*(C2 + Dy);
        v[k2+ja]   = 0.5*(Sx - C1);
        v[k2+ja+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*32+1] *= -1;
    v[OFTV_VECSIZE*32+3] *= -1;
    v[OFTV_VECSIZE*32+5] *= -1;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize32ZP
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{ // Version of InverseComplexToRealFFT32 which assumes that
  // rsize<=fftsize, i.e., that the last half of the transform
  // is to be thrown away.
#define RTNFFTSIZE 32
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = v[i];
      OXS_FFT_REAL_TYPE tmpBx0 = v[2*OFTV_VECSIZE*RTNFFTSIZE+i];
      v[i]   = 0.5*(tmpAx0 + tmpBx0);
      v[i+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[i+1] and v[2*OFTV_VECSIZE*RTNFFTSIZE+i+1] = 0.0
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(i=2;i<RTNFFTSIZE;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[i];
      const OXS_FFT_REAL_TYPE wy = UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

    // Top-level 32-block pass
    { // i=0
      for(OC_INDEX j=0 ; j<2*OFTV_VECSIZE ; j+=2) {
        OXS_FFT_REAL_TYPE ax0 = v[j];
        OXS_FFT_REAL_TYPE ay0 = v[j+1];
        OXS_FFT_REAL_TYPE ax1 = v[j+OFTV_VECSIZE*32];
        OXS_FFT_REAL_TYPE ay1 = v[j+OFTV_VECSIZE*32+1];
        v[j]                    = ax0 + ax1;
        v[j+1]                  = ay0 + ay1;
        v[j+OFTV_VECSIZE*32]   = ax0 - ax1;
        v[j+OFTV_VECSIZE*32+1] = ay0 - ay1;
        OXS_FFT_REAL_TYPE cx0 = v[j+OFTV_VECSIZE*16];
        OXS_FFT_REAL_TYPE cy0 = v[j+OFTV_VECSIZE*16+1];
        OXS_FFT_REAL_TYPE cx1 = v[j+OFTV_VECSIZE*48];
        OXS_FFT_REAL_TYPE cy1 = v[j+OFTV_VECSIZE*48+1];
        v[j+OFTV_VECSIZE*16]   = cx0 + cx1;
        v[j+OFTV_VECSIZE*16+1] = cy0 + cy1;
        v[j+OFTV_VECSIZE*48]   = cy1 - cy0;
        v[j+OFTV_VECSIZE*48+1] = cx0 - cx1;
      }
    }

    OXS_FFT_REAL_TYPE const *U = UForwardRadix4;
    // U points to the tail of the UForwardRadix4 array, which is a
    // segment holding the 32nd roots of unity.  That segment doesn't
    // contain the (1,0) root, so the access below is to U - 2.  Again,
    // the inverse roots are the complex conjugates of the forward
    // roots.
    for(i=2;i<16;i+=2) {
      OXS_FFT_REAL_TYPE* const va = v + OFTV_VECSIZE*i;
      const OXS_FFT_REAL_TYPE amx = U[i-2];
      const OXS_FFT_REAL_TYPE amy = U[i-1];
      for(OC_INDEX j=0 ; j<2*OFTV_VECSIZE ; j+=2) {
        OXS_FFT_REAL_TYPE ax0 = va[j];
        OXS_FFT_REAL_TYPE ay0 = va[j+1];
        OXS_FFT_REAL_TYPE ax1 = va[j+2*OFTV_VECSIZE*16];
        OXS_FFT_REAL_TYPE ay1 = va[j+2*OFTV_VECSIZE*16+1];

        OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
        va[j]        = ax0 + ax1;
        va[j+1]      = ay0 + ay1;
        OXS_FFT_REAL_TYPE adify = ay0 - ay1;
        va[j+2*OFTV_VECSIZE*16]   = amx*adifx + amy*adify;
        va[j+2*OFTV_VECSIZE*16+1] = amx*adify - amy*adifx;

        OXS_FFT_REAL_TYPE cx0 = va[j+2*OFTV_VECSIZE*8];
        OXS_FFT_REAL_TYPE cy0 = va[j+2*OFTV_VECSIZE*8+1];
        OXS_FFT_REAL_TYPE cx1 = va[j+2*OFTV_VECSIZE*24];
        OXS_FFT_REAL_TYPE cy1 = va[j+2*OFTV_VECSIZE*24+1];

        OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
        va[j+2*OFTV_VECSIZE*8]   = cx0 + cx1;
        va[j+2*OFTV_VECSIZE*8+1] = cy0 + cy1;
        OXS_FFT_REAL_TYPE cdify = cy0 - cy1;
        va[j+2*OFTV_VECSIZE*24]   = amy*cdifx - amx*cdify;
        va[j+2*OFTV_VECSIZE*24+1] = amy*cdify + amx*cdifx;
      }
    }


    for(i=0;i<32;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + 2*OFTV_VECSIZE*i;

      OC_INDEX j;

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j+0+3*16];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+3*16];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j+0+3*8];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+3*8];
        OXS_FFT_REAL_TYPE a3x = bv0[j+0+3*24];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+3*24];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j]        = aS0x + aS1x;
        scratch[j+1]      = aS0y + aS1y;
        scratch[j+3*8]    = aS0x - aS1x;
        scratch[j+1+3*8]  = aS0y - aS1y;

        scratch[j+3*16]   = aD0x - aD1y;
        scratch[j+1+3*16] = aD0y + aD1x;
        scratch[j+3*24]   = aD0x + aD1y;
        scratch[j+1+3*24] = aD0y - aD1x;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j+3*2];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+3*2];
        OXS_FFT_REAL_TYPE b2x = bv0[j+3*18];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+3*18];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j+3*10];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+3*10];
        OXS_FFT_REAL_TYPE b3x = bv0[j+3*26];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+3*26];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;

        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j+3*2]    = bS0x + bS1x;
        scratch[j+1+3*2]  = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j+3*10]   = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*10] = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

        scratch[j+3*18]   = tUb2x*alphax - tUb2y*alphay;
        scratch[j+1+3*18] = tUb2y*alphax + tUb2x*alphay;
        scratch[j+3*26]   = tUb3x*alphay - tUb3y*alphax;
        scratch[j+1+3*26] = tUb3y*alphay + tUb3x*alphax;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j+3*4];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+3*4];
        OXS_FFT_REAL_TYPE c2x = bv0[j+3*20];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+3*20];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j+3*12];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+3*12];
        OXS_FFT_REAL_TYPE c3x = bv0[j+3*28];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+3*28];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
        OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
        OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

        scratch[j+3*4]    = cS0x + cS1x;
        scratch[j+1+3*4]  = cS0y + cS1y;
        scratch[j+3*12]   = cS1y - cS0y;
        scratch[j+1+3*12] = cS0x - cS1x;

        scratch[j+3*20]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*20] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j+3*28]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*28] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j+3*6];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+3*6];
        OXS_FFT_REAL_TYPE d2x = bv0[j+3*22];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+3*22];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j+3*14];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+3*14];
        OXS_FFT_REAL_TYPE d3x = bv0[j+3*30];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+3*30];

        OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
        OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
        OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
        OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

        OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
        scratch[j+3*6]    = dS0x + dS1x;
        scratch[j+1+3*6]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

        scratch[j+3*14]   = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
        scratch[j+1+3*14] = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
        OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

        scratch[j+3*22]   = tUd2x*alphay - tUd2y*alphax;
        scratch[j+1+3*22] = tUd2y*alphay + tUd2x*alphax;
        scratch[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j+1+3*30] = tUd3x*alphay - tUd3y*alphax;
      }


      // Bit reversal.  This "...ZP" variant skips reversals
      // that affect what ends up in the upper half of the
      // results, since those are to be thrown away.
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i==0) {
        // Lower pass; no swaps, two pair interchanges
        const OC_INDEX bs = 2*OFTV_VECSIZE;
        for(ja=0;ja<16;ja+=2) w[ja] = bv0+bs*ja;
        w[2]  = bv0 + bs*8;
        w[6]  = bv0 + bs*12;
        w[8]  = bv0 + bs*2;
        w[12] = bv0 + bs*6;
      } else {
        for(ja=0;ja<16;ja+=2) w[ja] = v+bitreverse[i+ja];
      }

      // Do 4 lower (bottom/final) -level dragonflies
      // This "...ZP" variant assumes that the last half of the
      // results are to be thrown away, or equivalently, that
      // the pre-reversal odd (ja%2==1) results are not used.
      for(ja=0;ja<16;ja+=4) {
        OC_INDEX off = 6*ja;
        OXS_FFT_REAL_TYPE const * const sv = scratch + off;
        for(OC_INDEX jb=0;jb<6;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb];
          OXS_FFT_REAL_TYPE Uay = sv[jb+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb+3*4];
          OXS_FFT_REAL_TYPE Ucy = sv[jb+3*4+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb+3*2];
          OXS_FFT_REAL_TYPE Uby = sv[jb+3*2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb+3*6];
          OXS_FFT_REAL_TYPE Udy = sv[jb+3*6+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+2]+jb)   = BaDx - BbDy;
          *(w[ja+2]+jb+1) = BaDy + BbDx;
        }
      }

    }

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-2*OFTV_VECSIZE+1;i+=2*OFTV_VECSIZE) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize16
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{
#define RTNFFTSIZE 16
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {

    // Copy current row of rarr_in to carr_out, with zero padding.
    OC_INDEX i;
    const OC_INDEX istop = OFTV_VECSIZE*rsize;
    if(mult_base!=NULL) {
      const OXS_FFT_REAL_TYPE* mult = mult_base;
      mult_base += rstride/OFTV_VECSIZE;
      for(i=0;i<istop-5;i+=6) {
        carr_out[i]   = (*mult)*rarr_in[i];
        carr_out[i+2] = (*mult)*rarr_in[i+1];
        carr_out[i+4] = (*mult)*rarr_in[i+2];
        ++mult;
        carr_out[i+1] = (*mult)*rarr_in[i+3];
        carr_out[i+3] = (*mult)*rarr_in[i+4];
        carr_out[i+5] = (*mult)*rarr_in[i+5];
        ++mult;
      }
      if(i<istop) {
        carr_out[i]   = (*mult)*rarr_in[i];
        carr_out[i+1] = 0.0;
        carr_out[i+2] = (*mult)*rarr_in[i+1];
        carr_out[i+3] = 0.0;
        carr_out[i+4] = (*mult)*rarr_in[i+2];
        carr_out[i+5] = 0.0;
        i += 6;
      }
    } else {
      for(i=0;i<istop-5;i+=6) {
        carr_out[i]   = rarr_in[i];
        carr_out[i+2] = rarr_in[i+1];
        carr_out[i+4] = rarr_in[i+2];
        carr_out[i+1] = rarr_in[i+3];
        carr_out[i+3] = rarr_in[i+4];
        carr_out[i+5] = rarr_in[i+5];
      }
      if(i<istop) {
        carr_out[i]   = rarr_in[i];
        carr_out[i+1] = 0.0;
        carr_out[i+2] = rarr_in[i+1];
        carr_out[i+3] = 0.0;
        carr_out[i+4] = rarr_in[i+2];
        carr_out[i+5] = 0.0;
        i += 6;
      }
    }
    while(i<2*OFTV_VECSIZE*RTNFFTSIZE) carr_out[i++] = 0.0;

    OXS_FFT_REAL_TYPE* v = carr_out; // Working vector.

    // Top level 16-pass, with bit-flip shuffling
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

    OC_INDEX j;
    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE a0x = v[j];
      OXS_FFT_REAL_TYPE a0y = v[j+1];
      OXS_FFT_REAL_TYPE a2x = v[j+0+3*16];
      OXS_FFT_REAL_TYPE a2y = v[j+1+3*16];

      OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
      OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
      OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
      OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

      OXS_FFT_REAL_TYPE a1x = v[j+0+3*8];
      OXS_FFT_REAL_TYPE a1y = v[j+1+3*8];
      OXS_FFT_REAL_TYPE a3x = v[j+0+3*24];
      OXS_FFT_REAL_TYPE a3y = v[j+1+3*24];

      OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
      OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
      OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
      OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

      v[j]        = aS0x + aS1x;
      v[j+1]      = aS0y + aS1y;
      v[j+3*8]    = aS0x - aS1x;
      v[j+1+3*8]  = aS0y - aS1y;

      v[j+3*16]   = aD0x + aD1y;
      v[j+1+3*16] = aD0y - aD1x;
      v[j+3*24]   = aD0x - aD1y;
      v[j+1+3*24] = aD0y + aD1x;
    }

    for(j=0;j<6;j+=2) {
      OXS_FFT_REAL_TYPE b0x = v[j+3*2];
      OXS_FFT_REAL_TYPE b0y = v[j+1+3*2];
      OXS_FFT_REAL_TYPE b2x = v[j+3*18];
      OXS_FFT_REAL_TYPE b2y = v[j+1+3*18];

      OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
      OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
      OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
      OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

      OXS_FFT_REAL_TYPE b1x = v[j+3*10];
      OXS_FFT_REAL_TYPE b1y = v[j+1+3*10];
      OXS_FFT_REAL_TYPE b3x = v[j+3*26];
      OXS_FFT_REAL_TYPE b3y = v[j+1+3*26];

      OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
      OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
      OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
      OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

      OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
      v[j+3*2]    = bS0x + bS1x;
      v[j+1+3*2]  = bS0y + bS1y;
      OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

      v[j+3*10]   = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
      v[j+1+3*10] = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
      OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
      OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
      OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

      v[j+3*18]   = tUb2x*alphax + tUb2y*alphay;
      v[j+1+3*18] = tUb2y*alphax - tUb2x*alphay;
      v[j+3*26]   = tUb3x*alphay + tUb3y*alphax;
      v[j+1+3*26] = tUb3y*alphay - tUb3x*alphax;
    }

    for(j=0;j<6;j+=2) {
      OXS_FFT_REAL_TYPE d0x = v[j+3*6];
      OXS_FFT_REAL_TYPE d0y = v[j+1+3*6];
      OXS_FFT_REAL_TYPE d2x = v[j+3*22];
      OXS_FFT_REAL_TYPE d2y = v[j+1+3*22];

      OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
      OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
      OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
      OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

      OXS_FFT_REAL_TYPE d1x = v[j+3*14];
      OXS_FFT_REAL_TYPE d1y = v[j+1+3*14];
      OXS_FFT_REAL_TYPE d3x = v[j+3*30];
      OXS_FFT_REAL_TYPE d3y = v[j+1+3*30];

      OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
      OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
      OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
      OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

      OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
      v[j+3*6]    = dS1x + dS0x;
      v[j+1+3*6]  = dS0y + dS1y;
      OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

      v[j+3*14]   = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
      v[j+1+3*14] = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
      OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
      OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
      OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

      v[j+3*22]   = tUd2x*alphay + tUd2y*alphax;
      v[j+1+3*22] = tUd2y*alphay - tUd2x*alphax;
      v[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
      v[j+1+3*30] = tUd3y*alphax - tUd3x*alphay;
    }

    for(j=0;j<6;j+=2) {
      OXS_FFT_REAL_TYPE c0x = v[j+3*4];
      OXS_FFT_REAL_TYPE c0y = v[j+1+3*4];
      OXS_FFT_REAL_TYPE c2x = v[j+3*20];
      OXS_FFT_REAL_TYPE c2y = v[j+1+3*20];

      OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
      OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
      OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
      OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

      OXS_FFT_REAL_TYPE c1x = v[j+3*12];
      OXS_FFT_REAL_TYPE c1y = v[j+1+3*12];
      OXS_FFT_REAL_TYPE c3x = v[j+3*28];
      OXS_FFT_REAL_TYPE c3y = v[j+1+3*28];

      OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
      OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
      OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
      OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

      v[j+3*4]    = cS1x + cS0x;
      v[j+1+3*4]  = cS0y + cS1y;
      v[j+3*12]   = cS0y - cS1y;
      v[j+1+3*12] = cS1x - cS0x;

      OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
      OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
      OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
      OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

      v[j+3*20]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
      v[j+1+3*20] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
      v[j+3*28]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      v[j+1+3*28] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
    }

    // Do 4 lower (bottom/final) -level dragonflies
    for(OC_INDEX ja=0;ja<4*24;ja+=24) {
      OXS_FFT_REAL_TYPE* const bv = v + ja;
      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE Uax = bv[j];
        OXS_FFT_REAL_TYPE Uay = bv[j+1];
        OXS_FFT_REAL_TYPE Ucx = bv[j+3*4];
        OXS_FFT_REAL_TYPE Ucy = bv[j+3*4+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = bv[j+3*2];
        OXS_FFT_REAL_TYPE Uby = bv[j+3*2+1];
        OXS_FFT_REAL_TYPE Udx = bv[j+3*6];
        OXS_FFT_REAL_TYPE Udy = bv[j+3*6+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;

        bv[j]       = BaSx + BbSx;
        bv[j+1]     = BaSy + BbSy;
        bv[j+3*2]   = BaSx - BbSx;
        bv[j+3*2+1] = BaSy - BbSy;

        bv[j+3*4]   = BaDx + BbDy;
        bv[j+3*4+1] = BaDy - BbDx;
        bv[j+3*6]   = BaDx - BbDy;
        bv[j+3*6+1] = BaDy + BbDx;
      }
    }

    // Bit reversal, hard coded.  Perhaps this can be
    // moved above into the last dragonflies.
    const OC_INDEX bs = 2*OFTV_VECSIZE;
    OXS_FFT_REAL_TYPE swap_area[bs];
    //  1 <->  8
    memcpy(swap_area,v+bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+bs,v+8*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+8*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  2 <->  4
    memcpy(swap_area,v+2*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+2*bs,v+4*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+4*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));


    //  3 <-> 12
    memcpy(swap_area,v+3*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+3*bs,v+12*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+12*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  5 <-> 10
    memcpy(swap_area,v+5*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+5*bs,v+10*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+10*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  7 <-> 14
    memcpy(swap_area,v+7*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+7*bs,v+14*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+14*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    // 11 <-> 13
    memcpy(swap_area,v+11*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+11*bs,v+13*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+13*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      v[2*OFTV_VECSIZE*RTNFFTSIZE+j]   = v[j] - v[j+1];
      v[2*OFTV_VECSIZE*RTNFFTSIZE+1+j] = 0.0;
      v[j] += v[j+1];
      v[j+1] = 0.0;
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(i=2;i<RTNFFTSIZE;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[i];
      const OXS_FFT_REAL_TYPE wy = UReals[i+1];
      for(j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize16
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{
#define RTNFFTSIZE 16
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = v[i];
      OXS_FFT_REAL_TYPE tmpBx0 = v[2*OFTV_VECSIZE*RTNFFTSIZE+i];
      v[i]   = 0.5*(tmpAx0 + tmpBx0);
      v[i+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[i+1] and v[2*OFTV_VECSIZE*RTNFFTSIZE+i+1] = 0.0
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(i=2;i<RTNFFTSIZE;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[i];
      const OXS_FFT_REAL_TYPE wy = UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

    // Top level 16-pass, with bit-flip shuffling
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

    OC_INDEX j;
    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE a0x = v[j];
      OXS_FFT_REAL_TYPE a0y = v[j+1];
      OXS_FFT_REAL_TYPE a2x = v[j+0+3*16];
      OXS_FFT_REAL_TYPE a2y = v[j+1+3*16];

      OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
      OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
      OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
      OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

      OXS_FFT_REAL_TYPE a1x = v[j+0+3*8];
      OXS_FFT_REAL_TYPE a1y = v[j+1+3*8];
      OXS_FFT_REAL_TYPE a3x = v[j+0+3*24];
      OXS_FFT_REAL_TYPE a3y = v[j+1+3*24];

      OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
      OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
      OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
      OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

      v[j]        = aS0x + aS1x;
      v[j+1]      = aS0y + aS1y;
      v[j+3*8]    = aS0x - aS1x;
      v[j+1+3*8]  = aS0y - aS1y;
      v[j+3*16]   = aD0x - aD1y;
      v[j+1+3*16] = aD0y + aD1x;
      v[j+3*24]   = aD0x + aD1y;
      v[j+1+3*24] = aD0y - aD1x;
    }

    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE b0x = v[j+3*2];
      OXS_FFT_REAL_TYPE b0y = v[j+1+3*2];
      OXS_FFT_REAL_TYPE b2x = v[j+3*18];
      OXS_FFT_REAL_TYPE b2y = v[j+1+3*18];

      OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
      OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
      OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
      OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

      OXS_FFT_REAL_TYPE b1x = v[j+3*10];
      OXS_FFT_REAL_TYPE b1y = v[j+1+3*10];
      OXS_FFT_REAL_TYPE b3x = v[j+3*26];
      OXS_FFT_REAL_TYPE b3y = v[j+1+3*26];

      OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
      OXS_FFT_REAL_TYPE bD1x = b1x - b3x;

      OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
      OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

      OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
      v[j+3*2]    = bS0x + bS1x;
      v[j+1+3*2]  = bS0y + bS1y;
      OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

      v[j+3*10]   = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
      v[j+1+3*10] = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
      OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
      OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
      OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

      v[j+3*18]   = tUb2x*alphax - tUb2y*alphay;
      v[j+1+3*18] = tUb2y*alphax + tUb2x*alphay;
      v[j+3*26]   = tUb3x*alphay - tUb3y*alphax;
      v[j+1+3*26] = tUb3y*alphay + tUb3x*alphax;
    }

    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE d0x = v[j+3*6];
      OXS_FFT_REAL_TYPE d0y = v[j+1+3*6];
      OXS_FFT_REAL_TYPE d2x = v[j+3*22];
      OXS_FFT_REAL_TYPE d2y = v[j+1+3*22];

      OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
      OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
      OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
      OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

      OXS_FFT_REAL_TYPE d1x = v[j+3*14];
      OXS_FFT_REAL_TYPE d1y = v[j+1+3*14];
      OXS_FFT_REAL_TYPE d3x = v[j+3*30];
      OXS_FFT_REAL_TYPE d3y = v[j+1+3*30];

      OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
      OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
      OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
      OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

      OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
      v[j+3*6]    = dS0x + dS1x;
      v[j+1+3*6]  = dS0y + dS1y;
      OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

      v[j+3*14]   = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
      v[j+1+3*14] = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
      OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
      OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
      OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

      v[j+3*22]   = tUd2x*alphay - tUd2y*alphax;
      v[j+1+3*22] = tUd2y*alphay + tUd2x*alphax;
      v[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
      v[j+1+3*30] = tUd3x*alphay - tUd3y*alphax;
    }

    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE c0x = v[j+3*4];
      OXS_FFT_REAL_TYPE c0y = v[j+1+3*4];
      OXS_FFT_REAL_TYPE c2x = v[j+3*20];
      OXS_FFT_REAL_TYPE c2y = v[j+1+3*20];

      OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
      OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
      OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
      OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

      OXS_FFT_REAL_TYPE c1x = v[j+3*12];
      OXS_FFT_REAL_TYPE c1y = v[j+1+3*12];
      OXS_FFT_REAL_TYPE c3x = v[j+3*28];
      OXS_FFT_REAL_TYPE c3y = v[j+1+3*28];

      OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
      OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
      OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
      OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

      v[j+3*4]    = cS0x + cS1x;
      v[j+1+3*4]  = cS0y + cS1y;
      v[j+3*12]   = cS1y - cS0y;
      v[j+1+3*12] = cS0x - cS1x;

      OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
      OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
      OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
      OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

      v[j+3*20]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
      v[j+1+3*20] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
      v[j+3*28]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
      v[j+1+3*28] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
    }

    // Do 4 lower (bottom/final) -level dragonflies
    for(OC_INDEX ja=0;ja<4*24;ja+=24) {
      OXS_FFT_REAL_TYPE* const bv = v + ja;
      for(OC_INDEX jb=0;jb<6;jb+=2) {
        OXS_FFT_REAL_TYPE Uax = bv[jb];
        OXS_FFT_REAL_TYPE Uay = bv[jb+1];
        OXS_FFT_REAL_TYPE Ucx = bv[jb+3*4];
        OXS_FFT_REAL_TYPE Ucy = bv[jb+3*4+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = bv[jb+3*2];
        OXS_FFT_REAL_TYPE Uby = bv[jb+3*2+1];
        OXS_FFT_REAL_TYPE Udx = bv[jb+3*6];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;

        OXS_FFT_REAL_TYPE Udy = bv[jb+3*6+1];

        bv[jb]       = BaSx + BbSx;
        bv[jb+3*2]   = BaSx - BbSx;

        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        bv[jb+1]     = BaSy + BbSy;
        bv[jb+3*2+1] = BaSy - BbSy;
        bv[jb+3*4]   = BaDx - BbDy;
        bv[jb+3*4+1] = BaDy + BbDx;
        bv[jb+3*6]   = BaDx + BbDy;
        bv[jb+3*6+1] = BaDy - BbDx;
      }
    }

    // Bit reversal, hard coded.  Perhaps this can be
    // moved above into the last dragonflies.
    const OC_INDEX bs = 2*OFTV_VECSIZE;
    OXS_FFT_REAL_TYPE swap_area[bs];
    //  1 <->  8
    memcpy(swap_area,v+bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+bs,v+8*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+8*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  2 <->  4
    memcpy(swap_area,v+2*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+2*bs,v+4*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+4*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));


    //  3 <-> 12
    memcpy(swap_area,v+3*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+3*bs,v+12*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+12*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  5 <-> 10
    memcpy(swap_area,v+5*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+5*bs,v+10*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+10*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  7 <-> 14
    memcpy(swap_area,v+7*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+7*bs,v+14*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+14*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    // 11 <-> 13
    memcpy(swap_area,v+11*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+11*bs,v+13*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+13*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-2*OFTV_VECSIZE+1;i+=2*OFTV_VECSIZE) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize16ZP
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{ // Version of ForwardRealToComplexFFTSize16 that assumes
  // rsize<=fftsize (= csize/2 = 16).
#define RTNFFTSIZE 16
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {

    // Copy current row of rarr_in to carr_out, with zero padding.
    OC_INDEX i;
    const OC_INDEX istop = OFTV_VECSIZE*rsize;
    if(mult_base!=NULL) {
      const OXS_FFT_REAL_TYPE* mult = mult_base;
      mult_base += rstride/OFTV_VECSIZE;
      for(i=0;i<istop-5;i+=6) {
        carr_out[i]   = (*mult)*rarr_in[i];
        carr_out[i+2] = (*mult)*rarr_in[i+1];
        carr_out[i+4] = (*mult)*rarr_in[i+2];
        ++mult;
        carr_out[i+1] = (*mult)*rarr_in[i+3];
        carr_out[i+3] = (*mult)*rarr_in[i+4];
        carr_out[i+5] = (*mult)*rarr_in[i+5];
        ++mult;
      }
      if(i<istop) {
        carr_out[i]   = (*mult)*rarr_in[i];
        carr_out[i+1] = 0.0;
        carr_out[i+2] = (*mult)*rarr_in[i+1];
        carr_out[i+3] = 0.0;
        carr_out[i+4] = (*mult)*rarr_in[i+2];
        carr_out[i+5] = 0.0;
        i += 6;
      }
    } else {
      for(i=0;i<istop-5;i+=6) {
        carr_out[i]   = rarr_in[i];
        carr_out[i+2] = rarr_in[i+1];
        carr_out[i+4] = rarr_in[i+2];
        carr_out[i+1] = rarr_in[i+3];
        carr_out[i+3] = rarr_in[i+4];
        carr_out[i+5] = rarr_in[i+5];
      }
      if(i<istop) {
        carr_out[i]   = rarr_in[i];
        carr_out[i+1] = 0.0;
        carr_out[i+2] = rarr_in[i+1];
        carr_out[i+3] = 0.0;
        carr_out[i+4] = rarr_in[i+2];
        carr_out[i+5] = 0.0;
        i += 6;
      }
    }
    while(i<OFTV_VECSIZE*RTNFFTSIZE) carr_out[i++] = 0.0;

    OXS_FFT_REAL_TYPE* v = carr_out; // Working vector.

    // Top level 16-pass, with bit-flip shuffling
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

    OC_INDEX j;
    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE a0x = v[j];
      OXS_FFT_REAL_TYPE a0y = v[j+1];
      OXS_FFT_REAL_TYPE a1x = v[j+0+3*8];
      OXS_FFT_REAL_TYPE a1y = v[j+1+3*8];
      v[j]        = a0x + a1x;      v[j+1]      = a0y + a1y;
      v[j+3*8]    = a0x - a1x;      v[j+1+3*8]  = a0y - a1y;
      v[j+3*16]   = a0x + a1y;      v[j+1+3*16] = a0y - a1x;
      v[j+3*24]   = a0x - a1y;      v[j+1+3*24] = a0y + a1x;
    }

    for(j=0;j<6;j+=2) {
      OXS_FFT_REAL_TYPE b0x = v[j+3*2];
      OXS_FFT_REAL_TYPE b0y = v[j+1+3*2];
      OXS_FFT_REAL_TYPE b1x = v[j+3*10];
      OXS_FFT_REAL_TYPE b1y = v[j+1+3*10];

      OXS_FFT_REAL_TYPE tUb1x = b0x - b1x;
      v[j+3*2]    = b0x + b1x;
      v[j+1+3*2]  = b0y + b1y;
      OXS_FFT_REAL_TYPE tUb1y = b0y - b1y;

      v[j+3*10]   = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
      v[j+1+3*10] = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUb2x = b0x + b1y;
      OXS_FFT_REAL_TYPE tUb3x = b0x - b1y;
      OXS_FFT_REAL_TYPE tUb2y = b0y - b1x;
      OXS_FFT_REAL_TYPE tUb3y = b0y + b1x;

      v[j+3*18]   = tUb2x*alphax + tUb2y*alphay;
      v[j+1+3*18] = tUb2y*alphax - tUb2x*alphay;
      v[j+3*26]   = tUb3x*alphay + tUb3y*alphax;
      v[j+1+3*26] = tUb3y*alphay - tUb3x*alphax;
    }


    for(j=0;j<6;j+=2) {
      OXS_FFT_REAL_TYPE c0x = v[j+3*4];
      OXS_FFT_REAL_TYPE c0y = v[j+1+3*4];
      OXS_FFT_REAL_TYPE c1x = v[j+3*12];
      OXS_FFT_REAL_TYPE c1y = v[j+1+3*12];

      v[j+3*4]    = c1x + c0x;
      v[j+1+3*4]  = c0y + c1y;
      v[j+3*12]   = c0y - c1y;
      v[j+1+3*12] = c1x - c0x;

      OXS_FFT_REAL_TYPE tUc2x = c1y + c0x;
      OXS_FFT_REAL_TYPE tUc3x = c1y - c0x;
      OXS_FFT_REAL_TYPE tUc2y = c0y - c1x;
      OXS_FFT_REAL_TYPE tUc3y = c0y + c1x;

      v[j+3*20]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
      v[j+1+3*20] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
      v[j+3*28]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      v[j+1+3*28] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
    }

    for(j=0;j<6;j+=2) {
      OXS_FFT_REAL_TYPE d0x = v[j+3*6];
      OXS_FFT_REAL_TYPE d0y = v[j+1+3*6];
      OXS_FFT_REAL_TYPE d1x = v[j+3*14];
      OXS_FFT_REAL_TYPE d1y = v[j+1+3*14];

      OXS_FFT_REAL_TYPE tUd1x = d1x - d0x;
      v[j+3*6]    = d1x + d0x;
      v[j+1+3*6]  = d0y + d1y;
      OXS_FFT_REAL_TYPE tUd1y = d0y - d1y;

      v[j+3*14]   = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
      v[j+1+3*14] = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUd2x =  d1y + d0x;
      OXS_FFT_REAL_TYPE tUd3x =  d1y - d0x;
      OXS_FFT_REAL_TYPE tUd2y =  d0y - d1x;
      OXS_FFT_REAL_TYPE tUd3y =  d0y + d1x;

      v[j+3*22]   =  tUd2x*alphay + tUd2y*alphax;
      v[j+1+3*22] =  tUd2y*alphay - tUd2x*alphax;
      v[j+3*30]   =  tUd3x*alphax - tUd3y*alphay;
      v[j+1+3*30] = -tUd3y*alphax - tUd3x*alphay;
    }

    // Do 4 lower (bottom/final) -level dragonflies
    for(OC_INDEX ja=0;ja<4*24;ja+=24) {
      OXS_FFT_REAL_TYPE* const bv = v + ja;
      for(j=0;j<6;j+=2) {
        OXS_FFT_REAL_TYPE Uax = bv[j];
        OXS_FFT_REAL_TYPE Uay = bv[j+1];
        OXS_FFT_REAL_TYPE Ucx = bv[j+3*4];
        OXS_FFT_REAL_TYPE Ucy = bv[j+3*4+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = bv[j+3*2];
        OXS_FFT_REAL_TYPE Uby = bv[j+3*2+1];
        OXS_FFT_REAL_TYPE Udx = bv[j+3*6];
        OXS_FFT_REAL_TYPE Udy = bv[j+3*6+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;

        bv[j]       = BaSx + BbSx;
        bv[j+1]     = BaSy + BbSy;
        bv[j+3*2]   = BaSx - BbSx;
        bv[j+3*2+1] = BaSy - BbSy;

        bv[j+3*4]   = BaDx + BbDy;
        bv[j+3*4+1] = BaDy - BbDx;
        bv[j+3*6]   = BaDx - BbDy;
        bv[j+3*6+1] = BaDy + BbDx;
      }
    }

    // Bit reversal, hard coded.  Perhaps this can be
    // moved above into the last dragonflies.
    const OC_INDEX bs = 2*OFTV_VECSIZE;
    OXS_FFT_REAL_TYPE swap_area[bs];
    //  1 <->  8
    memcpy(swap_area,v+bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+bs,v+8*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+8*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  2 <->  4
    memcpy(swap_area,v+2*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+2*bs,v+4*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+4*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));


    //  3 <-> 12
    memcpy(swap_area,v+3*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+3*bs,v+12*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+12*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  5 <-> 10
    memcpy(swap_area,v+5*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+5*bs,v+10*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+10*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  7 <-> 14
    memcpy(swap_area,v+7*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+7*bs,v+14*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+14*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    // 11 <-> 13
    memcpy(swap_area,v+11*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+11*bs,v+13*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+13*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      v[2*OFTV_VECSIZE*RTNFFTSIZE+j]   = v[j] - v[j+1];
      v[2*OFTV_VECSIZE*RTNFFTSIZE+1+j] = 0.0;
      v[j] += v[j+1];
      v[j+1] = 0.0;
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(i=2;i<RTNFFTSIZE;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[i];
      const OXS_FFT_REAL_TYPE wy = UReals[i+1];
      for(j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize16ZP
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{ // Version of InverseComplexToRealFFT16 which assumes that
  // rsize<=fftsize, i.e., that the last half of the transform
  // is to be thrown away.
#define RTNFFTSIZE 16
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = v[i];
      OXS_FFT_REAL_TYPE tmpBx0 = v[2*OFTV_VECSIZE*RTNFFTSIZE+i];
      v[i]   = 0.5*(tmpAx0 + tmpBx0);
      v[i+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[i+1] and v[2*OFTV_VECSIZE*RTNFFTSIZE+i+1] = 0.0
    }
    OC_INDEX k1 = 0;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE;
    for(i=2;i<RTNFFTSIZE;i+=2) {
      k1 += 2*OFTV_VECSIZE;
      k2 -= 2*OFTV_VECSIZE;
      const OXS_FFT_REAL_TYPE wx = UReals[i];
      const OXS_FFT_REAL_TYPE wy = UReals[i+1];
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

    // Top level 16-pass, with bit-flip shuffling
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

    OC_INDEX j;
    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE a0x = v[j];
      OXS_FFT_REAL_TYPE a0y = v[j+1];
      OXS_FFT_REAL_TYPE a2x = v[j+0+3*16];
      OXS_FFT_REAL_TYPE a2y = v[j+1+3*16];

      OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
      OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
      OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
      OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

      OXS_FFT_REAL_TYPE a1x = v[j+0+3*8];
      OXS_FFT_REAL_TYPE a1y = v[j+1+3*8];
      OXS_FFT_REAL_TYPE a3x = v[j+0+3*24];
      OXS_FFT_REAL_TYPE a3y = v[j+1+3*24];

      OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
      OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
      OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
      OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

      v[j]        = aS0x + aS1x;
      v[j+1]      = aS0y + aS1y;
      v[j+3*8]    = aS0x - aS1x;
      v[j+1+3*8]  = aS0y - aS1y;
      v[j+3*16]   = aD0x - aD1y;
      v[j+1+3*16] = aD0y + aD1x;
      v[j+3*24]   = aD0x + aD1y;
      v[j+1+3*24] = aD0y - aD1x;
    }

    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE b0x = v[j+3*2];
      OXS_FFT_REAL_TYPE b0y = v[j+1+3*2];
      OXS_FFT_REAL_TYPE b2x = v[j+3*18];
      OXS_FFT_REAL_TYPE b2y = v[j+1+3*18];

      OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
      OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
      OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
      OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

      OXS_FFT_REAL_TYPE b1x = v[j+3*10];
      OXS_FFT_REAL_TYPE b1y = v[j+1+3*10];
      OXS_FFT_REAL_TYPE b3x = v[j+3*26];
      OXS_FFT_REAL_TYPE b3y = v[j+1+3*26];

      OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
      OXS_FFT_REAL_TYPE bD1x = b1x - b3x;

      OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
      OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

      OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
      v[j+3*2]    = bS0x + bS1x;
      v[j+1+3*2]  = bS0y + bS1y;
      OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

      v[j+3*10]   = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
      v[j+1+3*10] = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
      OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
      OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
      OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

      v[j+3*18]   = tUb2x*alphax - tUb2y*alphay;
      v[j+1+3*18] = tUb2y*alphax + tUb2x*alphay;
      v[j+3*26]   = tUb3x*alphay - tUb3y*alphax;
      v[j+1+3*26] = tUb3y*alphay + tUb3x*alphax;
    }

    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE d0x = v[j+3*6];
      OXS_FFT_REAL_TYPE d0y = v[j+1+3*6];
      OXS_FFT_REAL_TYPE d2x = v[j+3*22];
      OXS_FFT_REAL_TYPE d2y = v[j+1+3*22];

      OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
      OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
      OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
      OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

      OXS_FFT_REAL_TYPE d1x = v[j+3*14];
      OXS_FFT_REAL_TYPE d1y = v[j+1+3*14];
      OXS_FFT_REAL_TYPE d3x = v[j+3*30];
      OXS_FFT_REAL_TYPE d3y = v[j+1+3*30];

      OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
      OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
      OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
      OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

      OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
      v[j+3*6]    = dS0x + dS1x;
      v[j+1+3*6]  = dS0y + dS1y;
      OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

      v[j+3*14]   = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
      v[j+1+3*14] = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
      OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
      OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
      OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

      v[j+3*22]   = tUd2x*alphay - tUd2y*alphax;
      v[j+1+3*22] = tUd2y*alphay + tUd2x*alphax;
      v[j+3*30]   = tUd3x*alphax + tUd3y*alphay;
      v[j+1+3*30] = tUd3x*alphay - tUd3y*alphax;
    }

    for(j=0;j<2*OFTV_VECSIZE;j+=2) {
      OXS_FFT_REAL_TYPE c0x = v[j+3*4];
      OXS_FFT_REAL_TYPE c0y = v[j+1+3*4];
      OXS_FFT_REAL_TYPE c2x = v[j+3*20];
      OXS_FFT_REAL_TYPE c2y = v[j+1+3*20];

      OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
      OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
      OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
      OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

      OXS_FFT_REAL_TYPE c1x = v[j+3*12];
      OXS_FFT_REAL_TYPE c1y = v[j+1+3*12];
      OXS_FFT_REAL_TYPE c3x = v[j+3*28];
      OXS_FFT_REAL_TYPE c3y = v[j+1+3*28];

      OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
      OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
      OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
      OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

      v[j+3*4]    = cS0x + cS1x;
      v[j+1+3*4]  = cS0y + cS1y;
      v[j+3*12]   = cS1y - cS0y;
      v[j+1+3*12] = cS0x - cS1x;

      OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
      OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
      OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
      OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

      v[j+3*20]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
      v[j+1+3*20] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
      v[j+3*28]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
      v[j+1+3*28] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
    }

    // Do 4 lower (bottom/final) -level dragonflies
    // This "...ZP" variant assumes that the last half of the
    // results are to be thrown away, or equivalently, that
    // the pre-reversal odd (ja%2==1) results are not used.
    for(OC_INDEX ja=0;ja<4*24;ja+=24) {
      OXS_FFT_REAL_TYPE* const bv = v + ja;
      for(OC_INDEX jb=0;jb<6;jb+=2) {
        OXS_FFT_REAL_TYPE Uax = bv[jb];
        OXS_FFT_REAL_TYPE Uay = bv[jb+1];
        OXS_FFT_REAL_TYPE Ucx = bv[jb+3*4];
        OXS_FFT_REAL_TYPE Ucy = bv[jb+3*4+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = bv[jb+3*2];
        OXS_FFT_REAL_TYPE Uby = bv[jb+3*2+1];
        OXS_FFT_REAL_TYPE Udx = bv[jb+3*6];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;

        OXS_FFT_REAL_TYPE Udy = bv[jb+3*6+1];


        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        bv[jb]       = BaSx + BbSx;
        bv[jb+1]     = BaSy + BbSy;
        bv[jb+3*4]   = BaDx - BbDy;
        bv[jb+3*4+1] = BaDy + BbDx;
      }
    }

    // Bit reversal.  This "...ZP" variant skips reversals that affect
    // what ends up in the upper half of the results, since those are to
    // be thrown away.  Perhaps this can be moved above into the last
    // dragonflies?

    const OC_INDEX bs = 2*OFTV_VECSIZE;
    OXS_FFT_REAL_TYPE swap_area[bs];
    //  8 -> 1
    memcpy(v+bs,v+8*bs,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  2 <-> 4
    memcpy(swap_area,v+2*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+2*bs,v+4*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+4*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));


    //  12 -> 3
    memcpy(v+3*bs,v+12*bs,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  10 -> 5
    memcpy(v+5*bs,v+10*bs,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  14 -> 7
    memcpy(v+7*bs,v+14*bs,bs*sizeof(OXS_FFT_REAL_TYPE));

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-2*OFTV_VECSIZE+1;i+=2*OFTV_VECSIZE) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize8
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{ // See NOTES III, p103-109 (24-Oct-2003).
  // Includes built-in "ZP" handling
#define RTNFFTSIZE 8
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_out; // Working vector.

    // Copy current row of rarr_in to v, with zero padding,
    // and embedded top-level butterfly operation.
    const OC_INDEX istop = OFTV_VECSIZE*rsize;
    OC_INDEX i=0;

    if(mult_base!=NULL) {
      const OXS_FFT_REAL_TYPE* mult = mult_base;
      mult_base += rstride/OFTV_VECSIZE;
      while(i < istop - 2*OFTV_VECSIZE - OFTV_VECSIZE*RTNFFTSIZE + 1) {
        OXS_FFT_REAL_TYPE mult_a = mult[0];
        OXS_FFT_REAL_TYPE mult_b = mult[RTNFFTSIZE];
        OXS_FFT_REAL_TYPE a0 = mult_a*rarr_in[i];
        OXS_FFT_REAL_TYPE b0 = mult_b*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE];
        v[i]                           = a0 + b0;
        v[i + OFTV_VECSIZE*RTNFFTSIZE] = a0 - b0;

        OXS_FFT_REAL_TYPE a1 = mult_a*rarr_in[i + 1];
        OXS_FFT_REAL_TYPE b1 = mult_b*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 1];
        v[i + 2]                           = a1 + b1;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 2] = a1 - b1;

        OXS_FFT_REAL_TYPE a2 = mult_a*rarr_in[i + 2];
        OXS_FFT_REAL_TYPE b2 = mult_b*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 2];
        v[i + 4]                           = a2 + b2;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 4] = a2 - b2;

        OXS_FFT_REAL_TYPE mult_c = mult[1];
        OXS_FFT_REAL_TYPE mult_d = mult[RTNFFTSIZE + 1];
        OXS_FFT_REAL_TYPE a3 = mult_c*rarr_in[i + 3];
        OXS_FFT_REAL_TYPE b3 = mult_d*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 3];
        v[i + 1]                           = a3 + b3;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 1] = a3 - b3;

        OXS_FFT_REAL_TYPE a4 = mult_c*rarr_in[i + 4];
        OXS_FFT_REAL_TYPE b4 = mult_d*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 4];
        v[i + 3]                           = a4 + b4;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 3] = a4 - b4;

        OXS_FFT_REAL_TYPE a5 = mult_c*rarr_in[i + 5];
        OXS_FFT_REAL_TYPE b5 = mult_d*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 5];
        v[i + 5]                           = a5 + b5;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 5] = a5 - b5;

        i += 2*OFTV_VECSIZE;
        mult += 2;
      }
      if(i < istop - OFTV_VECSIZE*RTNFFTSIZE) {
        OXS_FFT_REAL_TYPE mult_a = mult[0];
        OXS_FFT_REAL_TYPE mult_b = mult[RTNFFTSIZE];
        OXS_FFT_REAL_TYPE a0 = mult_a*rarr_in[i];
        OXS_FFT_REAL_TYPE b0 = mult_b*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE];
        v[i]                           = a0 + b0;
        v[i + OFTV_VECSIZE*RTNFFTSIZE] = a0 - b0;

        OXS_FFT_REAL_TYPE a1 = mult_a*rarr_in[i + 1];
        OXS_FFT_REAL_TYPE b1 = mult_b*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 1];
        v[i + 2]                           = a1 + b1;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 2] = a1 - b1;

        OXS_FFT_REAL_TYPE a2 = mult_a*rarr_in[i + 2];
        OXS_FFT_REAL_TYPE b2 = mult_b*rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 2];
        v[i + 4]                           = a2 + b2;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 4] = a2 - b2;

        OXS_FFT_REAL_TYPE mult_c = mult[1];
        v[i + 1]                             =
          v[i + OFTV_VECSIZE*RTNFFTSIZE + 1] = mult_c*rarr_in[i + 3];

        v[i + 3]                           =
          v[i + OFTV_VECSIZE*RTNFFTSIZE + 3] = mult_c*rarr_in[i + 4];

        v[i + 5]                           =
          v[i + OFTV_VECSIZE*RTNFFTSIZE + 5] = mult_c*rarr_in[i + 5];

        i += 2*OFTV_VECSIZE;
        mult += 2;
      }

      while(i < istop - 2*OFTV_VECSIZE + 1
            && i<OFTV_VECSIZE*RTNFFTSIZE) {
        OXS_FFT_REAL_TYPE mult_a = mult[0];
        OXS_FFT_REAL_TYPE mult_c = mult[1];
        v[i]   = v[i + OFTV_VECSIZE*RTNFFTSIZE]     = mult_a*rarr_in[i];
        v[i+2] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 2] = mult_a*rarr_in[i+1];
        v[i+4] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 4] = mult_a*rarr_in[i+2];
        v[i+1] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 1] = mult_c*rarr_in[i+3];
        v[i+3] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 3] = mult_c*rarr_in[i+4];
        v[i+5] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 5] = mult_c*rarr_in[i+5];
        i += 2*OFTV_VECSIZE;
        mult += 2;
      }

      if(i < istop && i<OFTV_VECSIZE*RTNFFTSIZE) {
        OXS_FFT_REAL_TYPE mult_a = mult[0];
        v[i]   = v[i + OFTV_VECSIZE*RTNFFTSIZE]     = mult_a*rarr_in[i];
        v[i+1] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 1] = 0;
        v[i+2] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 2] = mult_a*rarr_in[i+1];
        v[i+3] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 3] = 0;
        v[i+4] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 4] = mult_a*rarr_in[i+2];
        v[i+5] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 5] = 0;
        i += 2*OFTV_VECSIZE;
      }
    } else {
      while(i < istop - 2*OFTV_VECSIZE - OFTV_VECSIZE*RTNFFTSIZE + 1) {
        OXS_FFT_REAL_TYPE a0 = rarr_in[i];
        OXS_FFT_REAL_TYPE b0 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE];
        v[i]                           = a0 + b0;
        v[i + OFTV_VECSIZE*RTNFFTSIZE] = a0 - b0;

        OXS_FFT_REAL_TYPE a1 = rarr_in[i + 1];
        OXS_FFT_REAL_TYPE b1 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 1];
        v[i + 2]                           = a1 + b1;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 2] = a1 - b1;

        OXS_FFT_REAL_TYPE a2 = rarr_in[i + 2];
        OXS_FFT_REAL_TYPE b2 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 2];
        v[i + 4]                           = a2 + b2;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 4] = a2 - b2;

        OXS_FFT_REAL_TYPE a3 = rarr_in[i + 3];
        OXS_FFT_REAL_TYPE b3 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 3];
        v[i + 1]                           = a3 + b3;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 1] = a3 - b3;

        OXS_FFT_REAL_TYPE a4 = rarr_in[i + 4];
        OXS_FFT_REAL_TYPE b4 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 4];
        v[i + 3]                           = a4 + b4;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 3] = a4 - b4;

        OXS_FFT_REAL_TYPE a5 = rarr_in[i + 5];
        OXS_FFT_REAL_TYPE b5 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 5];
        v[i + 5]                           = a5 + b5;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 5] = a5 - b5;

        i += 2*OFTV_VECSIZE;
      }
      if(i < istop - OFTV_VECSIZE*RTNFFTSIZE) {
        OXS_FFT_REAL_TYPE a0 = rarr_in[i];
        OXS_FFT_REAL_TYPE b0 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE];
        v[i]                           = a0 + b0;
        v[i + OFTV_VECSIZE*RTNFFTSIZE] = a0 - b0;

        OXS_FFT_REAL_TYPE a1 = rarr_in[i + 1];
        OXS_FFT_REAL_TYPE b1 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 1];
        v[i + 2]                           = a1 + b1;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 2] = a1 - b1;

        OXS_FFT_REAL_TYPE a2 = rarr_in[i + 2];
        OXS_FFT_REAL_TYPE b2 = rarr_in[i + OFTV_VECSIZE*RTNFFTSIZE + 2];
        v[i + 4]                           = a2 + b2;
        v[i + OFTV_VECSIZE*RTNFFTSIZE + 4] = a2 - b2;

        v[i + 1]                           =
          v[i + OFTV_VECSIZE*RTNFFTSIZE + 1] = rarr_in[i + 3];

        v[i + 3]                           =
          v[i + OFTV_VECSIZE*RTNFFTSIZE + 3] = rarr_in[i + 4];

        v[i + 5]                           =
          v[i + OFTV_VECSIZE*RTNFFTSIZE + 5] = rarr_in[i + 5];

        i += 2*OFTV_VECSIZE;
      }

      while(i < istop - 2*OFTV_VECSIZE + 1
            && i<OFTV_VECSIZE*RTNFFTSIZE) {
        v[i]   = v[i + OFTV_VECSIZE*RTNFFTSIZE]     = rarr_in[i];
        v[i+2] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 2] = rarr_in[i+1];
        v[i+4] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 4] = rarr_in[i+2];
        v[i+1] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 1] = rarr_in[i+3];
        v[i+3] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 3] = rarr_in[i+4];
        v[i+5] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 5] = rarr_in[i+5];
        i += 2*OFTV_VECSIZE;
      }

      if(i < istop && i<OFTV_VECSIZE*RTNFFTSIZE) {
        v[i]   = v[i + OFTV_VECSIZE*RTNFFTSIZE]     = rarr_in[i];
        v[i+1] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 1] = 0;
        v[i+2] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 2] = rarr_in[i+1];
        v[i+3] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 3] = 0;
        v[i+4] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 4] = rarr_in[i+2];
        v[i+5] = v[i + OFTV_VECSIZE*RTNFFTSIZE + 5] = 0;
        i += 2*OFTV_VECSIZE;
      }
    }
    while(i<OFTV_VECSIZE*RTNFFTSIZE) {
      v[i]   = v[i + OFTV_VECSIZE*RTNFFTSIZE] = 0;
      ++i;
    }

    // Second level butterflies.
    for(i=0;i<4*OFTV_VECSIZE;i++) {
      OXS_FFT_REAL_TYPE a = v[i];
      OXS_FFT_REAL_TYPE b = v[i+4*OFTV_VECSIZE];
      v[i]                 = a + b;
      v[i+4*OFTV_VECSIZE] = a - b;
    }
    for(i=4*2*OFTV_VECSIZE;i<6*2*OFTV_VECSIZE;i+=2) {
      OXS_FFT_REAL_TYPE ax = v[i];
      OXS_FFT_REAL_TYPE ay = v[i+1];
      OXS_FFT_REAL_TYPE bx = v[i+2*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE by = v[i+2*2*OFTV_VECSIZE+1];
      v[i]                     = ax + by;
      v[i+1]                   = ay - bx;
      v[i+2*2*OFTV_VECSIZE]   = ax - by;
      v[i+2*2*OFTV_VECSIZE+1] = ay + bx;
    }

    // Bottom level butterflies, with multiplication by U
    for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==0,1
      OXS_FFT_REAL_TYPE ax = v[i];
      OXS_FFT_REAL_TYPE bx = v[i+2*OFTV_VECSIZE];
      v[i]                 = ax + bx;
      v[i+2*OFTV_VECSIZE] = ax - bx;

      OXS_FFT_REAL_TYPE ay = v[i+1];
      OXS_FFT_REAL_TYPE by = v[i+2*OFTV_VECSIZE+1];
      v[i+1]                 = ay + by;
      v[i+2*OFTV_VECSIZE+1] = ay - by;
    }
    for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==2,3
      OXS_FFT_REAL_TYPE ax = v[i+2*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE ay = v[i+2*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE bx = v[i+3*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE by = v[i+3*2*OFTV_VECSIZE+1];

      v[i+2*2*OFTV_VECSIZE]   = ax + by;
      v[i+2*2*OFTV_VECSIZE+1] = ay - bx;
      v[i+3*2*OFTV_VECSIZE]   = ax - by;
      v[i+3*2*OFTV_VECSIZE+1] = ay + bx;
    }
    for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==4,5
      OXS_FFT_REAL_TYPE ax = v[i+4*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE ay = v[i+4*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE bx = v[i+5*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE by = v[i+5*2*OFTV_VECSIZE+1];

      OXS_FFT_REAL_TYPE tx = OXS_FFT_SQRT1_2*(bx+by);
      OXS_FFT_REAL_TYPE ty = OXS_FFT_SQRT1_2*(by-bx);

      v[i+4*2*OFTV_VECSIZE]   = ax + tx;
      v[i+4*2*OFTV_VECSIZE+1] = ay + ty;
      v[i+5*2*OFTV_VECSIZE]   = ax - tx;
      v[i+5*2*OFTV_VECSIZE+1] = ay - ty;
    }
    for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==6,7
      OXS_FFT_REAL_TYPE ax = v[i+6*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE ay = v[i+6*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE bx = v[i+7*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE by = v[i+7*2*OFTV_VECSIZE+1];

      OXS_FFT_REAL_TYPE tx = OXS_FFT_SQRT1_2*(bx+by);
      OXS_FFT_REAL_TYPE ty = OXS_FFT_SQRT1_2*(by-bx);

      v[i+6*2*OFTV_VECSIZE]   = ax + ty;
      v[i+6*2*OFTV_VECSIZE+1] = ay - tx;
      v[i+7*2*OFTV_VECSIZE]   = ax - ty;
      v[i+7*2*OFTV_VECSIZE+1] = ay + tx;
    }

    // Bit reversal, hard coded.  Perhaps this can be
    // moved above into the last butterflies.
    const OC_INDEX bs = 2*OFTV_VECSIZE;
    OXS_FFT_REAL_TYPE swap_area[bs];
    //  1 <->  4
    memcpy(swap_area,v+bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+bs,v+4*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+4*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    //  3 <-> 6
    memcpy(swap_area,v+3*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+3*bs,v+6*bs,bs*sizeof(OXS_FFT_REAL_TYPE));
    memcpy(v+6*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    // Unpack reals.  See NOTES IV, 14-Mar-2005, p42.
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      v[2*OFTV_VECSIZE*RTNFFTSIZE+i]   = v[i] - v[i+1];
      v[2*OFTV_VECSIZE*RTNFFTSIZE+1+i] = 0.0;
      v[i] += v[i+1];
      v[i+1] = 0.0;
    }
    OC_INDEX k1 = 2*OFTV_VECSIZE;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE - 2*OFTV_VECSIZE;
    { // i==2
      const OXS_FFT_REAL_TYPE wx =  alphax;
      const OXS_FFT_REAL_TYPE wy = -alphay;
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    k1 += 2*OFTV_VECSIZE;
    k2 -= 2*OFTV_VECSIZE;
    { // i==4
      const OXS_FFT_REAL_TYPE wx =  OXS_FFT_SQRT1_2;
      const OXS_FFT_REAL_TYPE wy = -OXS_FFT_SQRT1_2;
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    k1 += 2*OFTV_VECSIZE;
    k2 -= 2*OFTV_VECSIZE;
    { // i==6
      const OXS_FFT_REAL_TYPE wx =  alphay;
      const OXS_FFT_REAL_TYPE wy = -alphax;
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wx*Sy + wy*Dx;
        const OXS_FFT_REAL_TYPE C2 = wy*Sy - wx*Dx;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }

    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize8
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{ // Includes built-in "ZP" handling
#define RTNFFTSIZE 8
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      OXS_FFT_REAL_TYPE tmpAx0 = v[i];
      OXS_FFT_REAL_TYPE tmpBx0 = v[2*OFTV_VECSIZE*RTNFFTSIZE+i];
      v[i]   = 0.5*(tmpAx0 + tmpBx0);
      v[i+1] = 0.5*(tmpAx0 - tmpBx0);
      // Assume v[i+1] and v[2*OFTV_VECSIZE*RTNFFTSIZE+i+1] = 0.0
    }
    OC_INDEX k1 = 2*OFTV_VECSIZE;
    OC_INDEX k2 = 2*OFTV_VECSIZE*RTNFFTSIZE - 2*OFTV_VECSIZE;
    { // i==2
      const OXS_FFT_REAL_TYPE wx =  alphax;
      const OXS_FFT_REAL_TYPE wy = -alphay;
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    k1 += 2*OFTV_VECSIZE;
    k2 -= 2*OFTV_VECSIZE;
    { // i==4
      const OXS_FFT_REAL_TYPE wx =  OXS_FFT_SQRT1_2;
      const OXS_FFT_REAL_TYPE wy = -OXS_FFT_SQRT1_2;
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }
    k1 += 2*OFTV_VECSIZE;
    k2 -= 2*OFTV_VECSIZE;
    { // i==6
      const OXS_FFT_REAL_TYPE wx =  alphay;
      const OXS_FFT_REAL_TYPE wy = -alphax;
      for(OC_INDEX j=0;j<2*OFTV_VECSIZE;j+=2) {
        const OXS_FFT_REAL_TYPE Ax = v[k1+j];
        const OXS_FFT_REAL_TYPE Ay = v[k1+j+1];
        const OXS_FFT_REAL_TYPE Bx = v[k2+j];
        const OXS_FFT_REAL_TYPE By = v[k2+j+1];
        const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
        const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
        const OXS_FFT_REAL_TYPE Sy = Ay + By;
        const OXS_FFT_REAL_TYPE Dy = Ay - By;
        const OXS_FFT_REAL_TYPE C1 = wy*Dx - wx*Sy;
        const OXS_FFT_REAL_TYPE C2 = wx*Dx + wy*Sy;
        v[k1+j]   = 0.5*(Sx + C1);
        v[k1+j+1] = 0.5*(C2 + Dy);
        v[k2+j]   = 0.5*(Sx - C1);
        v[k2+j+1] = 0.5*(C2 - Dy);
      }
    }

    v[OFTV_VECSIZE*RTNFFTSIZE+1] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+3] *= -1;
    v[OFTV_VECSIZE*RTNFFTSIZE+5] *= -1;

    // Top-level butterfly operation
    for(i=0;i<4*2*OFTV_VECSIZE;i++) {
      OXS_FFT_REAL_TYPE a = v[i];
      OXS_FFT_REAL_TYPE b = v[i+4*2*OFTV_VECSIZE];
      v[i]                   = a + b;
      v[i+4*2*OFTV_VECSIZE] = a - b;
    }

    // Second level butterflies.
    for(i=0;i<4*OFTV_VECSIZE;i++) {
      OXS_FFT_REAL_TYPE a = v[i];
      OXS_FFT_REAL_TYPE b = v[i+4*OFTV_VECSIZE];
      v[i]                 = a + b;
      v[i+4*OFTV_VECSIZE] = a - b;
    }
    for(i=4*2*OFTV_VECSIZE;i<6*2*OFTV_VECSIZE;i+=2) {
      OXS_FFT_REAL_TYPE ax = v[i];
      OXS_FFT_REAL_TYPE ay = v[i+1];
      OXS_FFT_REAL_TYPE bx = v[i+2*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE by = v[i+2*2*OFTV_VECSIZE+1];
      v[i]                     = ax - by;
      v[i+1]                   = ay + bx;
      v[i+2*2*OFTV_VECSIZE]   = ax + by;
      v[i+2*2*OFTV_VECSIZE+1] = ay - bx;
    }

    // Bottom level butterflies, with multiplication by U
    // Bit-reversal is embedded.
    if(rsize>RTNFFTSIZE) {
      const OC_INDEX bs = 2*OFTV_VECSIZE;
      OXS_FFT_REAL_TYPE swap_area[bs]; // Temp space for bit-reversal

      for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==0,1
        OXS_FFT_REAL_TYPE ax = v[i];
        OXS_FFT_REAL_TYPE bx = v[i+2*OFTV_VECSIZE];
        v[i]                 = ax + bx;
        swap_area[i]         = ax - bx; // Bit-reversal, 1<->4

        OXS_FFT_REAL_TYPE ay = v[i+1];
        OXS_FFT_REAL_TYPE by = v[i+2*OFTV_VECSIZE+1];
        v[i+1]               = ay + by;
        swap_area[i+1]       = ay - by;
      }

      for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==4,5
        OXS_FFT_REAL_TYPE ax = v[i+4*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE ay = v[i+4*2*OFTV_VECSIZE+1];
        OXS_FFT_REAL_TYPE bx = v[i+5*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE by = v[i+5*2*OFTV_VECSIZE+1];

        OXS_FFT_REAL_TYPE tx = OXS_FFT_SQRT1_2*(bx-by);
        OXS_FFT_REAL_TYPE ty = OXS_FFT_SQRT1_2*(bx+by);

        v[i+  2*OFTV_VECSIZE]   = ax + tx; // Bit-reversal, 1<->4
        v[i+  2*OFTV_VECSIZE+1] = ay + ty;
        v[i+5*2*OFTV_VECSIZE]   = ax - tx;
        v[i+5*2*OFTV_VECSIZE+1] = ay - ty;
      }
      // Finish bit-reversal 1<->4
      memcpy(v+4*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

      for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==2,3
        OXS_FFT_REAL_TYPE ax = v[i+2*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE ay = v[i+2*2*OFTV_VECSIZE+1];
        OXS_FFT_REAL_TYPE bx = v[i+3*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE by = v[i+3*2*OFTV_VECSIZE+1];

        v[i+2*2*OFTV_VECSIZE]   = ax - by;
        v[i+2*2*OFTV_VECSIZE+1] = ay + bx;
        swap_area[i]   = ax + by; // Bit-reversal, 3<->6
        swap_area[i+1] = ay - bx;
      }

      for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==6,7
        OXS_FFT_REAL_TYPE ax = v[i+6*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE ay = v[i+6*2*OFTV_VECSIZE+1];
        OXS_FFT_REAL_TYPE bx = v[i+7*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE by = v[i+7*2*OFTV_VECSIZE+1];

        OXS_FFT_REAL_TYPE tx = OXS_FFT_SQRT1_2*(bx-by);
        OXS_FFT_REAL_TYPE ty = OXS_FFT_SQRT1_2*(bx+by);

        v[i+3*2*OFTV_VECSIZE]   = ax - ty; // Bit-reversal, 3<->6
        v[i+3*2*OFTV_VECSIZE+1] = ay + tx;
        v[i+7*2*OFTV_VECSIZE]   = ax + ty;
        v[i+7*2*OFTV_VECSIZE+1] = ay - tx;
      }
      // Finish bit-reversal 3<->6
      memcpy(v+6*bs,swap_area,bs*sizeof(OXS_FFT_REAL_TYPE));

    } else {
      // rsize<=fftsize, which implies we don't care about the
      // last half of the results.  For this last butterfly
      // pass, that means we don't need to compute or store
      // values in the "odd" (%2==1) offsets.  The bit reversal
      // is hard-coded into here, namely 4->1 and 6->3
      for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==0,1
        OXS_FFT_REAL_TYPE ax = v[i];
        OXS_FFT_REAL_TYPE bx = v[i+2*OFTV_VECSIZE];
        v[i]                 = ax + bx;

        OXS_FFT_REAL_TYPE ay = v[i+1];
        OXS_FFT_REAL_TYPE by = v[i+2*OFTV_VECSIZE+1];
        v[i+1]                 = ay + by;
      }
      for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==2,3
        OXS_FFT_REAL_TYPE ax = v[i+2*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE ay = v[i+2*2*OFTV_VECSIZE+1];
        OXS_FFT_REAL_TYPE bx = v[i+3*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE by = v[i+3*2*OFTV_VECSIZE+1];

        v[i+2*2*OFTV_VECSIZE]   = ax - by;
        v[i+2*2*OFTV_VECSIZE+1] = ay + bx;
      }
      for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==4,5
        OXS_FFT_REAL_TYPE ax = v[i+4*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE ay = v[i+4*2*OFTV_VECSIZE+1];
        OXS_FFT_REAL_TYPE bx = v[i+5*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE by = v[i+5*2*OFTV_VECSIZE+1];

        OXS_FFT_REAL_TYPE tx = OXS_FFT_SQRT1_2*(bx-by);
        OXS_FFT_REAL_TYPE ty = OXS_FFT_SQRT1_2*(bx+by);

        v[i+2*OFTV_VECSIZE]   = ax + tx; // Bit reversal, 4->1
        v[i+2*OFTV_VECSIZE+1] = ay + ty;
      }
      for(i=0;i<2*OFTV_VECSIZE;i+=2) { // i==6,7
        OXS_FFT_REAL_TYPE ax = v[i+6*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE ay = v[i+6*2*OFTV_VECSIZE+1];
        OXS_FFT_REAL_TYPE bx = v[i+7*2*OFTV_VECSIZE];
        OXS_FFT_REAL_TYPE by = v[i+7*2*OFTV_VECSIZE+1];

        OXS_FFT_REAL_TYPE tx = OXS_FFT_SQRT1_2*(bx-by);
        OXS_FFT_REAL_TYPE ty = OXS_FFT_SQRT1_2*(bx+by);

        v[i+3*2*OFTV_VECSIZE]   = ax - ty; // Bit reversal, 6->3
        v[i+3*2*OFTV_VECSIZE+1] = ay + tx;
      }
    }

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-2*OFTV_VECSIZE+1;i+=2*OFTV_VECSIZE) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize4
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{
#define RTNFFTSIZE 4
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_out; // Working vector.

    // Copy current row of rarr_in to scratch.  Scratch
    // space is zero-filled during initialization, so
    // zero padding is automatic.
    OC_INDEX i;
    const OC_INDEX istop = OFTV_VECSIZE*rsize;
    if(mult_base!=NULL) {
      const OXS_FFT_REAL_TYPE* mult = mult_base;
      mult_base += rstride/OFTV_VECSIZE;
      for(i=0;i<istop-5;i+=6) {
        scratch[i]   = (*mult)*rarr_in[i];
        scratch[i+2] = (*mult)*rarr_in[i+1];
        scratch[i+4] = (*mult)*rarr_in[i+2];
        ++mult;
        scratch[i+1] = (*mult)*rarr_in[i+3];
        scratch[i+3] = (*mult)*rarr_in[i+4];
        scratch[i+5] = (*mult)*rarr_in[i+5];
        ++mult;
      }
      if(i<istop) {
        scratch[i]   = (*mult)*rarr_in[i];
        scratch[i+2] = (*mult)*rarr_in[i+1];
        scratch[i+4] = (*mult)*rarr_in[i+2];
      }
    } else {
      for(i=0;i<istop-5;i+=6) {
        scratch[i]   = rarr_in[i];
        scratch[i+2] = rarr_in[i+1];
        scratch[i+4] = rarr_in[i+2];
        scratch[i+1] = rarr_in[i+3];
        scratch[i+3] = rarr_in[i+4];
        scratch[i+5] = rarr_in[i+5];
      }
      if(i<istop) {
        scratch[i]   = rarr_in[i];
        scratch[i+2] = rarr_in[i+1];
        scratch[i+4] = rarr_in[i+2];
      }
    }

    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      // Compute unscaled 4-size FFT and store result in v = carr_out
      OXS_FFT_REAL_TYPE s1x
        = scratch[i]                 + scratch[i+2*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE d1x
        = scratch[i]                 - scratch[i+2*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE s2x
        = scratch[i+2*OFTV_VECSIZE] + scratch[i+3*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE d2x
        = scratch[i+2*OFTV_VECSIZE] - scratch[i+3*2*OFTV_VECSIZE];
      v[i]                   = s1x+s2x;
      v[i+2*2*OFTV_VECSIZE] = s1x-s2x;

      OXS_FFT_REAL_TYPE s1y
        = scratch[i+1]                 + scratch[i+2*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE d1y
        = scratch[i+1]                 - scratch[i+2*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE s2y
        = scratch[i+2*OFTV_VECSIZE+1] + scratch[i+3*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE d2y
        = scratch[i+2*OFTV_VECSIZE+1] - scratch[i+3*2*OFTV_VECSIZE+1];
      v[i+1]                   = s1y+s2y;
      v[i+2*2*OFTV_VECSIZE+1] = s1y-s2y;

      v[i+  2*OFTV_VECSIZE+1] = d1y - d2x;
      v[i+3*2*OFTV_VECSIZE+1] = d1y + d2x;
      v[i+  2*OFTV_VECSIZE]   = d1x + d2y;
      v[i+3*2*OFTV_VECSIZE]   = d1x - d2y;

      // Unpack.  See NOTES IV, 14-Mar-2005, p42.
      v[i+RTNFFTSIZE*2*OFTV_VECSIZE]   = v[i] - v[i+1];
      v[i+RTNFFTSIZE*2*OFTV_VECSIZE+1] = 0.0;
      v[i]  += v[i+1];
      v[i+1] = 0.0;

      const OXS_FFT_REAL_TYPE Ax = v[i+  2*OFTV_VECSIZE];
      const OXS_FFT_REAL_TYPE Ay = v[i+  2*OFTV_VECSIZE+1];
      const OXS_FFT_REAL_TYPE Bx = v[i+3*2*OFTV_VECSIZE];
      const OXS_FFT_REAL_TYPE By = v[i+3*2*OFTV_VECSIZE+1];
      const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
      const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
      const OXS_FFT_REAL_TYPE Sy = Ay + By;
      const OXS_FFT_REAL_TYPE Dy = Ay - By;
      const OXS_FFT_REAL_TYPE C1 = (Sy - Dx)*OXS_FFT_SQRT1_2;
      const OXS_FFT_REAL_TYPE C2 = (Sy + Dx)*-1*OXS_FFT_SQRT1_2;

      v[i+  2*OFTV_VECSIZE]   = 0.5*(Sx + C1);
      v[i+  2*OFTV_VECSIZE+1] = 0.5*(C2 + Dy);
      v[i+3*2*OFTV_VECSIZE]   = 0.5*(Sx - C1);
      v[i+3*2*OFTV_VECSIZE+1] = 0.5*(C2 - Dy);

      v[i+2*2*OFTV_VECSIZE+1] *= -1;
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize4ZP
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{ // Version of ForwardRealToComplexFFTSize4 that assumes
  // rsize<=fftsize (= csize/2 = 4).
#define RTNFFTSIZE 4
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_out; // Working vector.

    // Copy current row of rarr_in to scratch.  Scratch
    // space is zero-filled during initialization, so
    // zero padding is automatic.
    OC_INDEX i;
    const OC_INDEX istop = OFTV_VECSIZE*rsize;
    if(mult_base!=NULL) {
      const OXS_FFT_REAL_TYPE* mult = mult_base;
      mult_base += rstride/OFTV_VECSIZE;
      for(i=0;i<istop-5;i+=6) {
        scratch[i]   = (*mult)*rarr_in[i];
        scratch[i+2] = (*mult)*rarr_in[i+1];
        scratch[i+4] = (*mult)*rarr_in[i+2];
        ++mult;
        scratch[i+1] = (*mult)*rarr_in[i+3];
        scratch[i+3] = (*mult)*rarr_in[i+4];
        scratch[i+5] = (*mult)*rarr_in[i+5];
        ++mult;
      }
      if(i<istop) {
        scratch[i]   = (*mult)*rarr_in[i];
        scratch[i+2] = (*mult)*rarr_in[i+1];
        scratch[i+4] = (*mult)*rarr_in[i+2];
      }
    } else {
      for(i=0;i<istop-5;i+=6) {
        scratch[i]   = rarr_in[i];
        scratch[i+2] = rarr_in[i+1];
        scratch[i+4] = rarr_in[i+2];
        scratch[i+1] = rarr_in[i+3];
        scratch[i+3] = rarr_in[i+4];
        scratch[i+5] = rarr_in[i+5];
      }
      if(i<istop) {
        scratch[i]   = rarr_in[i];
        scratch[i+2] = rarr_in[i+1];
        scratch[i+4] = rarr_in[i+2];
      }
    }

    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      // Compute unscaled 4-size FFT and store result in v = carr_out
      const OXS_FFT_REAL_TYPE a1x = scratch[i];
      const OXS_FFT_REAL_TYPE a1y = scratch[i+1];
      const OXS_FFT_REAL_TYPE a2x = scratch[i+2*OFTV_VECSIZE];
      const OXS_FFT_REAL_TYPE a2y = scratch[i+2*OFTV_VECSIZE+1];
      v[i]                     = a1x + a2x;
      v[i+1]                   = a1y + a2y;
      v[i+  2*OFTV_VECSIZE]   = a1x + a2y;
      v[i+  2*OFTV_VECSIZE+1] = a1y - a2x;
      v[i+2*2*OFTV_VECSIZE]   = a1x - a2x;
      v[i+2*2*OFTV_VECSIZE+1] = a1y - a2y;
      v[i+3*2*OFTV_VECSIZE]   = a1x - a2y;
      v[i+3*2*OFTV_VECSIZE+1] = a1y + a2x;

      // Unpack.  See NOTES IV, 14-Mar-2005, p42.
      v[i+RTNFFTSIZE*2*OFTV_VECSIZE]   = v[i] - v[i+1];
      v[i+RTNFFTSIZE*2*OFTV_VECSIZE+1] = 0.0;
      v[i]  += v[i+1];
      v[i+1] = 0.0;

      const OXS_FFT_REAL_TYPE Ax = v[i+  2*OFTV_VECSIZE];
      const OXS_FFT_REAL_TYPE Ay = v[i+  2*OFTV_VECSIZE+1];
      const OXS_FFT_REAL_TYPE Bx = v[i+3*2*OFTV_VECSIZE];
      const OXS_FFT_REAL_TYPE By = v[i+3*2*OFTV_VECSIZE+1];
      const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
      const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
      const OXS_FFT_REAL_TYPE Sy = Ay + By;
      const OXS_FFT_REAL_TYPE Dy = Ay - By;
      const OXS_FFT_REAL_TYPE C1 = (Sy - Dx)*OXS_FFT_SQRT1_2;
      const OXS_FFT_REAL_TYPE C2 = (Sy + Dx)*-1*OXS_FFT_SQRT1_2;

      v[i+  2*OFTV_VECSIZE]   = 0.5*(Sx + C1);
      v[i+  2*OFTV_VECSIZE+1] = 0.5*(C2 + Dy);
      v[i+3*2*OFTV_VECSIZE]   = 0.5*(Sx - C1);
      v[i+3*2*OFTV_VECSIZE+1] = 0.5*(C2 - Dy);

      v[i+2*2*OFTV_VECSIZE+1] *= -1;
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize4
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{
#define RTNFFTSIZE 4
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      // Repack for real transform.  See NOTES IV, 14-Mar-2005, p42.
      OXS_FFT_REAL_TYPE tmpAx0 = v[i];
      OXS_FFT_REAL_TYPE tmpBx0 = v[i+RTNFFTSIZE*2*OFTV_VECSIZE];
      v[i]   = 0.5*(tmpAx0 + tmpBx0);
      v[i+1] = 0.5*(tmpAx0 - tmpBx0);

      const OXS_FFT_REAL_TYPE Ax = v[i+  2*OFTV_VECSIZE];
      const OXS_FFT_REAL_TYPE Ay = v[i+  2*OFTV_VECSIZE+1];
      const OXS_FFT_REAL_TYPE Bx = v[i+3*2*OFTV_VECSIZE];
      const OXS_FFT_REAL_TYPE By = v[i+3*2*OFTV_VECSIZE+1];
      const OXS_FFT_REAL_TYPE Sx = Ax + Bx;
      const OXS_FFT_REAL_TYPE Dx = Ax - Bx;
      const OXS_FFT_REAL_TYPE Sy = Ay + By;
      const OXS_FFT_REAL_TYPE Dy = Ay - By;
      const OXS_FFT_REAL_TYPE C1 = (Dx + Sy)*-1*OXS_FFT_SQRT1_2;
      const OXS_FFT_REAL_TYPE C2 = (Dx - Sy)*OXS_FFT_SQRT1_2;

      v[i+2*OFTV_VECSIZE]     = 0.5*(Sx + C1);
      v[i+2*OFTV_VECSIZE+1]   = 0.5*(C2 + Dy);
      v[i+3*2*OFTV_VECSIZE]   = 0.5*(Sx - C1);
      v[i+3*2*OFTV_VECSIZE+1] = 0.5*(C2 - Dy);

      v[i+2*2*OFTV_VECSIZE+1] *= -1;

      // Compute unscaled 4-size iFFT, in-place
      OXS_FFT_REAL_TYPE s1x = v[i] + v[i+2*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE d1x = v[i] - v[i+2*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE s2x = v[i+2*OFTV_VECSIZE] + v[i+3*2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE d2x = v[i+2*OFTV_VECSIZE] - v[i+3*2*OFTV_VECSIZE];
      v[i]                   = s1x+s2x;
      v[i+2*2*OFTV_VECSIZE] = s1x-s2x;

      OXS_FFT_REAL_TYPE s1y = v[i+1]                 + v[i+2*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE d1y = v[i+1]                 - v[i+2*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE s2y = v[i+2*OFTV_VECSIZE+1] + v[i+3*2*OFTV_VECSIZE+1];
      OXS_FFT_REAL_TYPE d2y = v[i+2*OFTV_VECSIZE+1] - v[i+3*2*OFTV_VECSIZE+1];
      v[i+1]                   = s1y+s2y;
      v[i+2*2*OFTV_VECSIZE+1] = s1y-s2y;

      v[i+  2*OFTV_VECSIZE+1] = d1y + d2x;
      v[i+3*2*OFTV_VECSIZE+1] = d1y - d2x;
      v[i+  2*OFTV_VECSIZE]   = d1x - d2y;
      v[i+3*2*OFTV_VECSIZE]   = d1x + d2y;
    }

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-2*OFTV_VECSIZE+1;i+=2*OFTV_VECSIZE) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }

  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize2
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{
#define RTNFFTSIZE 2
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_in+=rstride,carr_out+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_out; // Working vector.

    // Copy current row of rarr_in to scratch.  Scratch
    // space is zero-filled during initialization, so
    // zero padding is automatic.
    OC_INDEX i;
    const OC_INDEX istop = OFTV_VECSIZE*rsize;
    if(mult_base!=NULL) {
      const OXS_FFT_REAL_TYPE* mult = mult_base;
      mult_base += rstride/OFTV_VECSIZE;
      for(i=0;i<istop-5;i+=6) {
        scratch[i]   = (*mult)*rarr_in[i];
        scratch[i+2] = (*mult)*rarr_in[i+1];
        scratch[i+4] = (*mult)*rarr_in[i+2];
        ++mult;
        scratch[i+1] = (*mult)*rarr_in[i+3];
        scratch[i+3] = (*mult)*rarr_in[i+4];
        scratch[i+5] = (*mult)*rarr_in[i+5];
        ++mult;
      }
      if(i<istop) {
        scratch[i]   = (*mult)*rarr_in[i];
        scratch[i+2] = (*mult)*rarr_in[i+1];
        scratch[i+4] = (*mult)*rarr_in[i+2];
      }
    } else {
      for(i=0;i<istop-5;i+=6) {
        scratch[i]   = rarr_in[i];
        scratch[i+2] = rarr_in[i+1];
        scratch[i+4] = rarr_in[i+2];
        scratch[i+1] = rarr_in[i+3];
        scratch[i+3] = rarr_in[i+4];
        scratch[i+5] = rarr_in[i+5];
      }
      if(i<istop) {
        scratch[i]   = rarr_in[i];
        scratch[i+2] = rarr_in[i+1];
        scratch[i+4] = rarr_in[i+2];
      }
    }

    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      // Compute unscaled 2-size FFT and store result in v = carr_out.
      // Include complex/real unpacking
      OXS_FFT_REAL_TYPE Ax = scratch[i]   + scratch[i+2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE Ay = scratch[i+1] + scratch[i+2*OFTV_VECSIZE+1];
      v[i]   = Ax + Ay;
      v[i+1] = 0.0;
      v[i+2*2*OFTV_VECSIZE]   = Ax - Ay;
      v[i+2*2*OFTV_VECSIZE+1] = 0.0;
      v[i+  2*OFTV_VECSIZE]   = scratch[i]   - scratch[i+2*OFTV_VECSIZE];
      v[i+  2*OFTV_VECSIZE+1] = scratch[i+2*OFTV_VECSIZE+1] - scratch[i+1];
    }

  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize2
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{
#define RTNFFTSIZE 2
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.
    OC_INDEX i;
    for(i=0;i<2*OFTV_VECSIZE;i+=2) {
      // Unpack and do unscaled 2-size FFT, inplace.
      OXS_FFT_REAL_TYPE tAx = v[i];
      OXS_FFT_REAL_TYPE tAy = v[i+2*2*OFTV_VECSIZE];
      // Assume v[i+1] and v[2*OFTV_VECSIZE*RTNFFTSIZE+i+1] = 0.0

      OXS_FFT_REAL_TYPE Ax = 0.5*(tAx+tAy);
      OXS_FFT_REAL_TYPE Ay = 0.5*(tAx-tAy);
      OXS_FFT_REAL_TYPE Bx = v[i+2*OFTV_VECSIZE];
      OXS_FFT_REAL_TYPE By = v[i+2*OFTV_VECSIZE+1];

      v[i]   = Ax + Bx;
      v[i+1] = Ay - By;
      v[i+2*OFTV_VECSIZE]   = Ax - Bx;
      v[i+2*OFTV_VECSIZE+1] = Ay + By;
    }

    // Copy current row of carr_out to rarr_in, ignoring data outside
    // rsize (which is not assumed to be zero, but rather only of no
    // interest.
    {
      const OC_INDEX istop = OFTV_VECSIZE*rsize;
      for(i=0;i<istop-2*OFTV_VECSIZE+1;i+=2*OFTV_VECSIZE) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
        rarr_out[i+3] = v[i+1];
        rarr_out[i+4] = v[i+3];
        rarr_out[i+5] = v[i+5];
      }
      if(i<istop) {
        rarr_out[i]   = v[i];
        rarr_out[i+1] = v[i+2];
        rarr_out[i+2] = v[i+4];
      }
    }

  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize1
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{
#define RTNFFTSIZE 1
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;
  if(mult_base!=NULL) {
    for(OC_INDEX row=0;row<arrcount;
        ++row,rarr_in+=rstride,carr_out+=cstride,
          mult_base += rstride/OFTV_VECSIZE) {
      OXS_FFT_REAL_TYPE* const v = carr_out; // Working vector.

      // Compute FFT.  This is nominally a size 1 complex FFT,
      // plus unpacking, or equivalently, a FFT of size 2 of
      // a real sequence.
      // NB: rsize must be either 1 or 2
      if(rsize==1) {
        OXS_FFT_REAL_TYPE mult_a = mult_base[0];
        v[ 6] = v[0] = mult_a*rarr_in[0];   v[ 7] = v[1] = 0.0;
        v[ 8] = v[2] = mult_a*rarr_in[1];   v[ 9] = v[3] = 0.0;
        v[10] = v[4] = mult_a*rarr_in[2];   v[11] = v[5] = 0.0;
      } else {
        // rsize == 2
        OXS_FFT_REAL_TYPE mult_a = mult_base[0];
        OXS_FFT_REAL_TYPE mult_b = mult_base[1];

        OXS_FFT_REAL_TYPE x0 = mult_a * rarr_in[0];
        OXS_FFT_REAL_TYPE y0 = mult_b * rarr_in[3];
        v[0] = x0 + y0;    v[1] = 0.0;
        v[6] = x0 - y0;    v[7] = 0.0;

        OXS_FFT_REAL_TYPE x1 = mult_a * rarr_in[1];
        OXS_FFT_REAL_TYPE y1 = mult_b * rarr_in[4];
        v[2] = x1 + y1;    v[3] = 0.0;
        v[8] = x1 - y1;    v[9] = 0.0;

        OXS_FFT_REAL_TYPE x2 = mult_a * rarr_in[2];
        OXS_FFT_REAL_TYPE y2 = mult_b * rarr_in[5];
        v[4]  = x2 + y2;   v[5]  = 0.0;
        v[10] = x2 - y2;   v[11] = 0.0;
      }
    }
  } else { // mult_base == NULL
    for(OC_INDEX row=0;row<arrcount;
        ++row,rarr_in+=rstride,carr_out+=cstride) {

      OXS_FFT_REAL_TYPE* const v = carr_out; // Working vector.

      // Compute FFT.  This is nominally a size 1 complex FFT,
      // plus unpacking, or equivalently, a FFT of size 2 of
      // a real sequence.
      // NB: rsize must be either 1 or 2
      if(rsize==1) {
        v[0] = rarr_in[0];    v[1] = 0.0;
        v[6] = rarr_in[0];    v[7] = 0.0;

        v[2] = rarr_in[1];    v[3] = 0.0;
        v[8] = rarr_in[1];    v[9] = 0.0;

        v[4]  = rarr_in[2];   v[5]  = 0.0;
        v[10] = rarr_in[2];   v[11] = 0.0;
      } else {
        // rsize == 2
        v[0] = rarr_in[0] + rarr_in[3];    v[1] = 0.0;
        v[6] = rarr_in[0] - rarr_in[3];    v[7] = 0.0;

        v[2] = rarr_in[1] + rarr_in[4];    v[3] = 0.0;
        v[8] = rarr_in[1] - rarr_in[4];    v[9] = 0.0;

        v[4]  = rarr_in[2] + rarr_in[5];   v[5]  = 0.0;
        v[10] = rarr_in[2] - rarr_in[5];   v[11] = 0.0;
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize1
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{
#define RTNFFTSIZE 1
  const OC_INDEX cstride = 2*(RTNFFTSIZE+1)*OFTV_VECSIZE;

  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=rstride,carr_in+=cstride) {

    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.

    // Compute iFFT.  This is nominally a size 1 complex iFFT,
    // plus unpacking, or equivalently, an iFFT of size 2 of
    // a real sequence.
    // NB: rsize must be either 1 or 2
    if(rsize==1) {
      rarr_out[0] = 0.5*(v[0] + v[6]);
      rarr_out[1] = 0.5*(v[2] + v[8]);
      rarr_out[2] = 0.5*(v[4] + v[10]);
    } else {
      // rsize == 2
      rarr_out[0] = 0.5*(v[0] + v[6]);
      rarr_out[3] = 0.5*(v[0] - v[6]);

      rarr_out[1] = 0.5*(v[2] + v[8]);
      rarr_out[4] = 0.5*(v[2] - v[8]);

      rarr_out[2] = 0.5*(v[4] + v[10]);
      rarr_out[5] = 0.5*(v[4] - v[10]);
    }

  }

#undef RTNFFTSIZE
}

void
Oxs_FFT1DThreeVector::ForwardRealToComplexFFTSize0
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out,
 const OXS_FFT_REAL_TYPE* mult_base) const
{ // Special case, rsize = csize = 1;
    if(mult_base!=NULL) {
      for(OC_INDEX row=0;row<arrcount;
          ++row,rarr_in+=OFTV_VECSIZE,carr_out+=2*OFTV_VECSIZE,
            mult_base += rstride/OFTV_VECSIZE) {
        OXS_FFT_REAL_TYPE* const v = carr_out; // Working vector.
        v[0] = mult_base[0]*rarr_in[0];   v[1] = 0.0;
        v[2] = mult_base[0]*rarr_in[1];   v[3] = 0.0;
        v[4] = mult_base[0]*rarr_in[2];   v[5] = 0.0;
      }
    } else {
      for(OC_INDEX row=0;row<arrcount;
          ++row,rarr_in+=OFTV_VECSIZE,carr_out+=2*OFTV_VECSIZE) {
        OXS_FFT_REAL_TYPE* const v = carr_out; // Working vector.
        v[0] = rarr_in[0];   v[1] = 0.0;
        v[2] = rarr_in[1];   v[3] = 0.0;
        v[4] = rarr_in[2];   v[5] = 0.0;
      }
    }
}

void
Oxs_FFT1DThreeVector::InverseComplexToRealFFTSize0
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{ // Special case, rsize = csize = 1;
  for(OC_INDEX row=0;row<arrcount;
      ++row,rarr_out+=OFTV_VECSIZE,carr_in+=2*OFTV_VECSIZE) {
    OXS_FFT_REAL_TYPE* const v = carr_in; // Working vector.
    rarr_out[0] = v[0];
    rarr_out[1] = v[2];
    rarr_out[2] = v[4];
    // Assume v[1] == v[3] == v[5] == 0.0
  }
}

////////////////////////////////////////////////////////////////////////
// Class Oxs_FFTStrided ////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

/* class Oxs_FFTStrided
 *
 * Class for in-place, complex-to-complex FFT, where consecutive complex
 * values are separated by a fixed stride.  An array of these sequences
 * can be transformed with a single call, provided that the same index
 * entries are adjacent in memory, i.e.,
 *
 *         -- memory index increases by 1 -->
 *
 *   |     a0_x  a0_y  b0_x  b0_y  c0_x c0_y ...
 *  mem    a1_x  a1_y  b1_x  b1_y  c1_x c1_y ...
 * index   ...
 *  +K     aj_x  aj_y  bj_x  bj_y  cj_x cj_y ...
 *   |     ...
 *   v
 *
 * Here the complex FFT is taken of the sequence (a), and also (b), (c),
 * ..., where the stride between a0 and a1 (and likewise (b0 and b1, c0
 * and c1, ak and ak+1, etc.) is fixed size K.  (The stride is actually
 * measured in OXS_FFT_REAL_TYPE units, which is half the size of a
 * complex item.)
 *
 * Implicit zero padding is also supported.
 *
 * This class is primarily intended for compound use to construct
 * multi-dimensional FFT's.
 */

OC_INDEX Oxs_FFTStrided::GetNextPowerOfTwo(OC_INDEX n,OC_INT4m& logsize)
{ // Returns first power of two >= n
  // This is a private function, for use by power-of-two
  // code.  The client interface is RecommendSize(), which
  // at present just calls this routine, but may be adapted
  // in the future to a mixed-radix code.
  OC_INDEX m=1;
  logsize=0;
  while(m<n) {
    m*=2; ++logsize;
    if(m<1) {
      char msgbuf[1024];
      Oc_Snprintf(msgbuf,sizeof(msgbuf),
                  ": Import n=%ld too big",static_cast<long int>(n));
      String msg =
        String("OC_INDEX overflow in Oxs_FFTStrided::GetNextPowerOfTwo")
        + String(msgbuf);
      OXS_THROW(Oxs_BadParameter,msg);
    }
  }
  return m;
}

OC_INDEX Oxs_FFTStrided::RecommendSize(OC_INDEX size)
{ // Returns first power of two >= size
  OC_INT4m dummy;
  return GetNextPowerOfTwo(size,dummy);
}

void Oxs_FFTStrided::FreeMemory()
{
  if(UForwardRadix4) delete[] UForwardRadix4;
  UForwardRadix4=0;
  if(ptsRadix4)      delete[] ptsRadix4;
  ptsRadix4=0;
  if(bitreverse)     delete[] bitreverse;
  bitreverse=0;
  if(scratch)        delete[] scratch;
  scratch=0;
  scratch_size = 0;
}

void Oxs_FFTStrided::FillRootsOfUnity()
{
  // NB: Any changes to memory size or layout in this routine
  // must be reflected in Oxs_FFT1DThreeVector::Dup().

  OC_INDEX i,j,k;

  if(UForwardRadix4) delete[] UForwardRadix4;
  UForwardRadix4=0;

  if(fftsize<32) return; // Size 16 and smaller transforms
  // use hard-coded roots.

  if(fftsize==32) {
    // Size 32 uses radix-2 roots only.  For convenience,
    // the radix-2 roots are tagged on to the end of the
    // UForwardRadix4 array, which is a misnomer for these
    // roots, especially in the fftsize==32 case, where no
    // radix-4 roots are stored all!
    OXS_FFT_REAL_TYPE theta_real_base = 2*WIDE_PI/fftsize;
    UForwardRadix4 = new OXS_FFT_REAL_TYPE[14];
    for(i=1;i<4;++i) {
      OXS_FFT_REAL_TYPE theta = i * theta_real_base;
      OXS_FFT_REAL_TYPE ct = cos(theta);
      OXS_FFT_REAL_TYPE st = sin(theta);
      UForwardRadix4[2*(i-1)]   =    ct;  // Real part
      UForwardRadix4[2*(i-1)+1] = -1*st;  // Imaginary part
      UForwardRadix4[2*(7-i)]   =    st;
      UForwardRadix4[2*(7-i)+1] = -1*ct;
    }
    UForwardRadix4[6] =    OXS_FFT_SQRT1_2;
    UForwardRadix4[7] = -1*OXS_FFT_SQRT1_2;
    return;
  }

  // Size of UForwardRadix4 array, in complex units, is
  //  fftsize - 3*n/2 - 10,     if n=log_2(fftsize) is even, or
  //  fftsize - 3*(n-1)/2 - 11, if n=log_2(fftsize) is odd
  // or, double that in real units
  OC_INDEX UfR4_csize = fftsize - 3*(log2fftsize/2)
    - 10 - (log2fftsize%2);
  UForwardRadix4 = new OXS_FFT_REAL_TYPE[2*UfR4_csize];

  // Compute base roots for real transform.  These all lie in lower half
  // of complex plane, so imaginary parts are non-positive for all
  // roots.  To make code easier to follow, compute roots in the first
  // quadrant, and adjust signs as necessary.
  // NB: Values are stored in the "by use" order, namely w^2, w, w^3.
  // So the first pass (base roots) are stored at offset +2.
  OXS_FFT_REAL_TYPE theta_real_base = 2*WIDE_PI/fftsize;
  for(i=1;i<fftsize/8;++i) {
    OXS_FFT_REAL_TYPE theta = i * theta_real_base;
    OXS_FFT_REAL_TYPE ct = cos(theta);
    OXS_FFT_REAL_TYPE st = sin(theta);
    UForwardRadix4[6*(i-1)+2]   =    ct;  // Real part
    UForwardRadix4[6*(i-1)+1+2] = -1*st;  // Imaginary part
    UForwardRadix4[6*(fftsize/4-i-1)+2]   =    st;
    UForwardRadix4[6*(fftsize/4-i-1)+1+2] = -1*ct;
  }
  UForwardRadix4[6*(fftsize/8-1)+2]   =    OXS_FFT_SQRT1_2;
  UForwardRadix4[6*(fftsize/8-1)+1+2] = -1*OXS_FFT_SQRT1_2;

  // UForwardRadix4 is filled with roots of unity down to and including
  // N=64 if log2fftsize if even, or N=128 if log2fftsize if odd.
  j=0;
  for(i=1;i<=fftsize/(64*(1+log2fftsize%2));i*=4) {
    for(k=i;k<fftsize/4;k+=i) { // Don't store 1's (i.e., k==0)
      if(2*k<fftsize/4) {
        UForwardRadix4[j++] = UForwardRadix4[6*(2*k-1)+2];
        UForwardRadix4[j++] = UForwardRadix4[6*(2*k-1)+1+2];
      } else if(2*k==fftsize/4) {
        UForwardRadix4[j++] =  0;
        UForwardRadix4[j++] = -1;
      } else {
        UForwardRadix4[j++] = -1*UForwardRadix4[6*(fftsize/2-2*k-1)+2];
        UForwardRadix4[j++] =    UForwardRadix4[6*(fftsize/2-2*k-1)+1+2];
      }
      if(i==1) j+=2; // w^k already stored
      else {
        UForwardRadix4[j++] = UForwardRadix4[6*(k-1)+2];   // Real
        UForwardRadix4[j++] = UForwardRadix4[6*(k-1)+1+2]; // Imaginary
        /// The "-1" in "k-1" reflects the fact that the
        /// w^0 term isn't stored.
      }
      if(3*k<fftsize/4) {
        UForwardRadix4[j++] = UForwardRadix4[6*(3*k-1)+2];
        UForwardRadix4[j++] = UForwardRadix4[6*(3*k-1)+1+2];
      } else if(3*k<fftsize/2) {
        UForwardRadix4[j++] = -1*UForwardRadix4[6*(fftsize/2-3*k-1)+2];
        UForwardRadix4[j++] =    UForwardRadix4[6*(fftsize/2-3*k-1)+1+2];
      } else {
        UForwardRadix4[j++] = -1*UForwardRadix4[6*(3*k-fftsize/2-1)+2];
        UForwardRadix4[j++] = -1*UForwardRadix4[6*(3*k-fftsize/2-1)+1+2];
      }
    }
  }
  if(log2fftsize%2) {
    // Append 32nd roots of unity sub-array
    OC_INDEX basestride32 = fftsize/32;
    for(k=1;k<8;++k) { // Don't store 1's (i.e., k==0)
      UForwardRadix4[j++] = UForwardRadix4[6*(k*basestride32-1)+2];
      UForwardRadix4[j++] = UForwardRadix4[6*(k*basestride32-1)+1+2];
    }
    UForwardRadix4[j++] =  0;
    UForwardRadix4[j++] = -1;
    for(k=9;k<16;++k) {
      OC_INDEX koff = fftsize/2-k*basestride32-1;
      UForwardRadix4[j++] = -1*UForwardRadix4[6*koff+2];
      UForwardRadix4[j++] =    UForwardRadix4[6*koff+1+2];
    }
  }

  assert(j == 2*UfR4_csize);
}

void Oxs_FFTStrided::FillPreorderTraversalStateArray()
{ // See mjd's NOTES III, 30-June-2003, p78.
  if(ptsRadix4) delete[] ptsRadix4;
  ptsRadix4 = 0;

  if(fftsize<64) return; // ptsRadix4 array only used for
  /// FFT's of size 64 and larger.

  OC_INDEX ptsRadix4_size = fftsize/((1+log2fftsize%2)*64);
  ptsRadix4 = new PreorderTraversalState[ptsRadix4_size+1];
  /// Extra "+1" is to hold terminating 0 marker

  OC_INDEX URadix4_size = fftsize - 3*(log2fftsize/2) - 10
    - 16*(log2fftsize%2);
  // Radix4 portion of UForwardRadix4.
  URadix4_size *= 2; // Measure in terms of reals.

  OC_INDEX i,j;
  for(i=0;i<ptsRadix4_size;++i) {
    ptsRadix4[i].stride=32*(1+log2fftsize%2);
    ptsRadix4[i].uoff=4+log2fftsize%2; // Temporarily store
    /// log_2(stride/2), for use below in calculating uoff
  }
  for(j=4;j<=ptsRadix4_size;j*=4) {
    for(i=0;i<ptsRadix4_size;i+=j) {
      ptsRadix4[i].stride*=4;
      ptsRadix4[i].uoff+=2;
    }
  }
  for(i=0;i<ptsRadix4_size;++i) {
    OC_INDEX rs  = ptsRadix4[i].stride; // "real" (versus complex) stride
    ptsRadix4[i].stride = (rs/2)*rstride; // Note: rstride might be odd
    OC_INDEX k = ptsRadix4[i].uoff;
    ptsRadix4[i].uoff = (2*fftsize + 6 + 3*k) - (4*rs + 3*log2fftsize);
    /// The 3*k term is because we don't store the k==0 terms in
    /// UForwardRadix4. (!?)
    assert(0<=ptsRadix4[i].uoff && ptsRadix4[i].uoff<URadix4_size-2);
  }
  ptsRadix4[ptsRadix4_size].stride=0; // End-of-array mark
  ptsRadix4[ptsRadix4_size].uoff=0;   // Safety
}

void Oxs_FFTStrided::FillBitReversalArray()
{
  if(bitreverse) delete[] bitreverse;
  bitreverse = 0;

  if(fftsize<32) return; // Bit reversal for fftsize<=16 is hard-coded.

  bitreverse = new OC_INDEX[fftsize];

  // The following code relies heavily on fftsize being a power-of-2
  bitreverse[0] = 0;

  // For sanity, work with unsigned types
  const OC_UINDEX ufftsize = static_cast<OC_UINDEX>(fftsize);
  OC_UINDEX k,n;
  OC_UINDEX mask = 0x0F;  mask = ~mask;
  for(k=1,n=ufftsize>>1;k<ufftsize;k++) {
    // At each step, n is bit-reversed pattern of k
    if((k&mask) == (n&mask) ) {
      // k and n are in same 16-block, so during final 16-fly
      // processing step both will be held in local variables
      // at the same time.  Therefore we don't need to swap
      // them, just write them directly into the appropriate
      // locations.  This can only happen if the number of
      // bits (log2fftsize) is 6 or less (i.e., fftsize<=64).
      bitreverse[k] = rstride*static_cast<OC_INDEX>(n);
    } else if(n<k) { // Swap
      bitreverse[k]= rstride*static_cast<OC_INDEX>(n);
    } else { // k<n; No swap at i=k; hold for swap at i=n
      bitreverse[k] = rstride*static_cast<OC_INDEX>(k);
    }

    // Calculate next n.  We do this by manually adding 1 to the
    // leftmost bit, and carrying to the right.
    OC_UINDEX m = ufftsize>>1;
    while(m>0 && n&m) { n-=m; m>>=1; }
    n+=m;
  }
}

void
Oxs_FFTStrided::AllocScratchSpace(OC_INDEX size)
{ // Allocate scratch space, and initialize to zero.
  if(scratch) delete[] scratch;
  scratch=0;  scratch_size = 0;
  if(size>0) {
    scratch_size = size;
    scratch = new OXS_FFT_REAL_TYPE[size];
    for(OC_INDEX i=0;i<size;++i) scratch[i] = 0.0;
  }
}

Oxs_FFTStrided::Oxs_FFTStrided() :
  ForwardPtr(0),InversePtr(0),         // Dummy initial values
  arrcount(0),csize_base(0),rstride(0),
  fftsize(0),log2fftsize(-1),
  UForwardRadix4(0),ptsRadix4(0),bitreverse(0),
  scratch_size(0),scratch(0)
{}

Oxs_FFTStrided::~Oxs_FFTStrided()
{
  FreeMemory();
}

void Oxs_FFTStrided::Dup(const Oxs_FFTStrided& other)
{
  OC_INDEX i;
  FreeMemory();

  arrcount    = other.arrcount;
  csize_base  = other.csize_base;
  rstride     = other.rstride;
  fftsize     = other.fftsize;
  log2fftsize = other.log2fftsize;


  if(other.UForwardRadix4 != 0) {
    OC_INDEX UfR4_csize = fftsize - 3*(log2fftsize/2)
      - 10 - (log2fftsize%2);
    OC_INDEX radsize = 2*UfR4_csize;
    if(fftsize==32) {
      radsize = 14;
    }
    UForwardRadix4 = new OXS_FFT_REAL_TYPE[radsize];
    for(i=0;i<radsize;++i) UForwardRadix4[i] = other.UForwardRadix4[i];
  }

  if(other.ptsRadix4 != 0) {
    OC_INDEX ptsRadix4_size = fftsize/((1+log2fftsize%2)*64);
    OC_INDEX pr4size = ptsRadix4_size+1;
    ptsRadix4 = new PreorderTraversalState[pr4size];
    for(i=0;i<pr4size;++i)  ptsRadix4[i] = other.ptsRadix4[i];
  }

  if(other.bitreverse !=0) {
    bitreverse = new OC_INDEX[fftsize];
    for(i=0;i<fftsize;++i) bitreverse[i] = other.bitreverse[i];
  }

  // An alternative way to set Forward/InversePtr and scratch
  // is to call AssignTransformPointers.
  ForwardPtr = other.ForwardPtr;
  InversePtr = other.InversePtr;
  if(other.scratch !=0 && other.scratch_size>0) {
    scratch_size = other.scratch_size;
    scratch = new OXS_FFT_REAL_TYPE[scratch_size];
    for(i=0;i<scratch_size;++i) scratch[i] = other.scratch[i];
  }

}

void Oxs_FFTStrided::AssignTransformPointers()
{
  if(scratch) delete[] scratch;
  scratch=0;  scratch_size = 0;

  // Assign Forward/Inverse routine pointers
  switch(log2fftsize) {
  case 0: // fftsize == 1
    ForwardPtr = &Oxs_FFTStrided::FFTNop;
    InversePtr = &Oxs_FFTStrided::FFTNop;
    break;
  case 1: // fftsize == 2
    if(2*csize_base>fftsize) {
      ForwardPtr = &Oxs_FFTStrided::FFTSize2; // Forward and inverse
      InversePtr = &Oxs_FFTStrided::FFTSize2; // are the same.
    } else {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize2ZP;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize2ZP;
    }
    break;
  case 2: // fftsize == 4
    AllocScratchSpace(4*2*OFS_ARRAY_MAXBLOCKSIZE);
    if(2*csize_base>fftsize) {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize4;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize4;
    } else {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize4ZP;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize4ZP;
    }
    break;
  case 3: // fftsize == 8
    AllocScratchSpace(8*2*OFS_ARRAY_MAXBLOCKSIZE);
    if(2*csize_base>fftsize) {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize8;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize8;
    } else {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize8ZP;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize8ZP;
    }
    break;
  case 4: // fftsize == 16
    AllocScratchSpace(16*2*OFS_ARRAY_MAXBLOCKSIZE);
    if(2*csize_base>fftsize) {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize16;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize16;
    } else {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize16ZP;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize16ZP;
    }
    break;
  case 5: // fftsize == 32
    AllocScratchSpace(16*2*OFS_ARRAY_MAXBLOCKSIZE);
    if(2*csize_base>fftsize) {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize32;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize32;
    } else {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize32ZP;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize32ZP;
    }
    break;
  case 6: // fftsize == 64
    AllocScratchSpace(16*2*OFS_ARRAY_MAXBLOCKSIZE);
    if(2*csize_base>fftsize) {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize64;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize64;
    } else {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTSize64ZP;
      InversePtr = &Oxs_FFTStrided::InverseFFTSize64ZP;
    }
    break;
  default: // fftsize > 64
    AllocScratchSpace(16*2*OFS_ARRAY_MAXBLOCKSIZE);
    if(2*csize_base>fftsize) {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTRadix4;
      InversePtr = &Oxs_FFTStrided::InverseFFTRadix4;
    } else {
      ForwardPtr = &Oxs_FFTStrided::ForwardFFTRadix4ZP;
      InversePtr = &Oxs_FFTStrided::InverseFFTRadix4ZP;
    }
    break;
  }
}

void Oxs_FFTStrided::SetDimensions
(OC_INDEX import_csize_base,
 OC_INDEX import_csize_zp,
 OC_INDEX import_rstride,
 OC_INDEX import_array_count)
{
  csize_base = import_csize_base;
  fftsize = import_csize_zp;
  rstride = import_rstride;
  arrcount = import_array_count;

  OC_INDEX checksize = GetNextPowerOfTwo(fftsize,log2fftsize);
  if(fftsize!=checksize) {
    OXS_THROW(Oxs_BadParameter,
	      "Illegal csize_zp import to Oxs_FFTStrided::SetDimensions().");
  }

  if(import_csize_zp<import_csize_base) {
    OXS_THROW(Oxs_BadParameter,
      "Invalid Oxs_FFTStrided::SetDimensions() call: csize_zp<csize_base.");
  }

  if(import_rstride<2*import_array_count) {
    OXS_THROW(Oxs_BadParameter,
      "Invalid Oxs_FFTStrided::SetDimensions() call: rstride<2*array_count.");
  }

  if(import_csize_base<1 || import_array_count<1) {
    OXS_THROW(Oxs_BadParameter,
	      "Illegal import to Oxs_FFTStrided::SetDimensions().");
  }

  FillRootsOfUnity();
  FillPreorderTraversalStateArray();
  FillBitReversalArray();

  AssignTransformPointers();
}

void Oxs_FFTStrided::AdjustInputDimensions
(OC_INDEX new_csize_base,
 OC_INDEX new_rstride,
 OC_INDEX new_array_count)
{
  if(new_csize_base<1 || new_array_count<1) {
    OXS_THROW(Oxs_BadParameter,
	      "Illegal import to Oxs_FFTStrided::AdjustInputDimensions().");
  }
  if(new_rstride<2*new_array_count) {
    OXS_THROW(Oxs_BadParameter,
      "Invalid Oxs_FFTStrided::AdjustInputDimensions() call:"
	      " new_rstride<2*new_array_count.");
  }
  if(new_csize_base>fftsize) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid Oxs_FFTStrided::AdjustInputDimensions() call:"
                " new_csize_base (=%d) > csize_zp (=%d).",
                new_csize_base,fftsize);
    OXS_THROW(Oxs_BadParameter,String(buf));
  }

  csize_base = new_csize_base;
  arrcount   = new_array_count;

  if(new_rstride!=rstride) {
    rstride = new_rstride;
    FillPreorderTraversalStateArray();
    FillBitReversalArray();
  }

  AssignTransformPointers(); // Transform size doesn't change,
  /// but *ZP code selection might.
}

void
Oxs_FFTStrided::ForwardFFTRadix4
(OXS_FFT_REAL_TYPE* arr) const
{ // NOTE: The bit-reversal code in this routine
  // is only valid for fftsize>=128.
  // The following Oc_Nop is a workaround for an optimization bug in the
  // version of gcc 4.2.1 that ships alongside Mac OS X Lion.
  const OC_INDEX block32_count = Oc_Nop(4*(log2fftsize%2));
  const OC_INDEX block16_count = block32_count + 4;

  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    OXS_FFT_REAL_TYPE* const v = arr + 2*block;

    // Fill out zero-padding
    {
      for(OC_INDEX i=csize_base;i<fftsize;++i) {
        OXS_FFT_REAL_TYPE* w = v + i*rstride;
        for(OC_INDEX j=0;j<bw;++j) w[j] = 0.0;
      }
    }

    // Do power-of-4 blocks, with preorder traversal/tree walk
    OC_INDEX offset = 0;
    const PreorderTraversalState* sptr = ptsRadix4;
    do {
      // Stride is measured in units of OXS_FFT_REAL_TYPE's.
      OC_INDEX stride = sptr->stride;
      OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;
      do {
        // Do power-of-4 butterflies with "stride" at
        // position v + rstride*offset
        OXS_FFT_REAL_TYPE *const va = v + rstride*offset;
        OXS_FFT_REAL_TYPE *const vb = va + stride;
        OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
        OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
        OC_INDEX i;
        for(i=0;i<bw;i+=2) {
          // w^0
          OXS_FFT_REAL_TYPE x0 = va[i];
          OXS_FFT_REAL_TYPE y0 = va[i+1];
          OXS_FFT_REAL_TYPE x1 = vb[i];
          OXS_FFT_REAL_TYPE y1 = vb[i+1];
          OXS_FFT_REAL_TYPE x2 = vc[i];
          OXS_FFT_REAL_TYPE x3 = vd[i];

          OXS_FFT_REAL_TYPE y2 = vc[i+1];

          OXS_FFT_REAL_TYPE sx1 = x0 + x2;
          OXS_FFT_REAL_TYPE dx1 = x0 - x2;

          OXS_FFT_REAL_TYPE sx2 = x1 + x3;
          OXS_FFT_REAL_TYPE dx2 = x1 - x3;

          OXS_FFT_REAL_TYPE y3 = vd[i+1];

          OXS_FFT_REAL_TYPE sy1 = y0 + y2;
          OXS_FFT_REAL_TYPE dy1 = y0 - y2;
          OXS_FFT_REAL_TYPE sy2 = y1 + y3;
          OXS_FFT_REAL_TYPE dy2 = y1 - y3;

          va[i]   = sx1 + sx2;
          va[i+1] = sy1 + sy2;
          vb[i]   = sx1 - sx2;
          vb[i+1] = sy1 - sy2;
          vc[i]   = dx1 + dy2;
          vc[i+1] = dy1 - dx2;
          vd[i]   = dx1 - dy2;
          vd[i+1] = dy1 + dx2;
        }
        i = rstride;
        while(i<stride) {
          for(OC_INDEX j=i;j-i<bw;j+=2) {
            OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
            OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
            OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
            OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
            OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
            OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

            OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
            OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
            dx1 -= x2;    // dx1 := R0
#define dx2 x1
            dx2 -= x3;    // dx2 := R1

#define y2 x2
            y2 = vc[j+1]; // y2 := R2
#define y3 x3
            y3 = vd[j+1]; // y3 := R3

            va[j]              = sx1 + sx2;
#define txa sx1
            txa -= sx2;     // txa := R4

#define sy1 sx2
            sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
            dy1 -= y2;      // dy1 := R6
            OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
            dy2 -= y3;      // dy2 := R7

#define mx1 y3
            mx1 = U[0];   // mx1 := R3

            va[j+1]              = sy1 + sy2;
#define tya sy1
            tya -= sy2;     // tya := R5

#define mx1_txa y2
            mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
            mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
            my1 = U[1];    // my1 := R8

#define my1_tya tya
            my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
            my1_txa *= my1; // my1_txa := R4

            vb[j]     = mx1_txa - my1_tya;
            vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
            txb = dx1 + dy2;  // txb := R2
#define txc dx1
            txc -= dy2;       // txc := R0
#define tyb mx1_tya
            tyb = dy1 - dx2;  // tyb := R3
#define tyc dy1
            tyc += dx2;       // tyc := R6

#define mx2 my1_txa
            mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
            mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
            mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
            my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
            my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
            my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
            Cx -= my2_tyb; // Cx := R1
            vc[j]   = Cx;

#define mx3 mx2
            mx3 = U[4];  // mx3 := R4
#define my3 my2
            my3 = U[5];  // my3 := R5

            OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
            OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
            OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
            OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
            OXS_FFT_REAL_TYPE Cy = mx2_tyb + my2_txb; // Cy := R2
            OXS_FFT_REAL_TYPE Dx = mx3_txc - my3_tyc;
            OXS_FFT_REAL_TYPE Dy = my3_txc + mx3_tyc;

            vc[j+1] = Cy;
            vd[j]   = Dx;
            vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
          }
          i+=rstride;
          U+=6;
        }
        stride /= 4;
      } while(stride>8*rstride);

      if(block32_count>0) {
        { // i=0
          OXS_FFT_REAL_TYPE* const va = v + rstride*offset;
          for(OC_INDEX j1=0 ; j1<rstride*128 ; j1+=rstride*32) {
            for(OC_INDEX j2=j1 ; j2<j1+bw ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
              OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
              OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
              OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];
              va[j2]                    = ax0 + ax1;
              va[j2+1]                  = ay0 + ay1;
              va[j2+rstride*16]   = ax0 - ax1;
              va[j2+rstride*16+1] = ay0 - ay1;
              va[j2+rstride*8]   = cx0 + cx1;
              va[j2+rstride*8+1] = cy0 + cy1;
              va[j2+rstride*24]   = cy0 - cy1;
              va[j2+rstride*24+1] = cx1 - cx0;
            }
          }
        }
        // NB: U currently points to the tail of the UForwardRadix4 array,
        // which is a subsegment holding the 32nd roots of unity.  That
        // subsegment doesn't contain the (1,0) root, so the access below
        // is to U - 2.
        for(OC_INDEX i=1;i<8;++i) {
          OXS_FFT_REAL_TYPE* const va = v + rstride*(offset + i);
          const OXS_FFT_REAL_TYPE amx = U[2*i-2];
          const OXS_FFT_REAL_TYPE amy = U[2*i-1];
          for(OC_INDEX j1=0;j1<rstride*128;j1+=rstride*32) {
            for(OC_INDEX j2=j1 ; j2<j1+bw ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
              OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
              OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
              OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];

              OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
              va[j2]        = ax0 + ax1;
              va[j2+1]      = ay0 + ay1;
              OXS_FFT_REAL_TYPE adify = ay0 - ay1;

              va[j2+rstride*16]   = amx*adifx - amy*adify;
              va[j2+rstride*16+1] = amx*adify + amy*adifx;

              OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
              va[j2+rstride*8]   = cx0 + cx1;
              va[j2+rstride*8+1] = cy0 + cy1;
              OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

              va[j2+rstride*24]   = amx*cdify + amy*cdifx;
              va[j2+rstride*24+1] = amy*cdify - amx*cdifx;
            }
          }
        }
      }

      OC_INDEX i = offset;
      OC_INDEX k=block16_count;
      do {
        // Bottom level 16-passes, with built-in bit-flip shuffling
        const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
        const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

        OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

        OC_INDEX j;

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE a0x = bv0[j];
          OXS_FFT_REAL_TYPE a0y = bv0[j+1];
          OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
          OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

          OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
          OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
          OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
          OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

          OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
          OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
          OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
          OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

          OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
          OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
          OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
          OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

          scratch[j*16]        = aS0x + aS1x;
          scratch[j*16+1]      = aS0y + aS1y;
          scratch[j*16+2*4]    = aS0x - aS1x;
          scratch[j*16+2*4+1]  = aS0y - aS1y;

          scratch[j*16+2*8]    = aD0x + aD1y;
          scratch[j*16+2*8+1]  = aD0y - aD1x;
          scratch[j*16+2*12]   = aD0x - aD1y;
          scratch[j*16+2*12+1] = aD0y + aD1x;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
          OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
          OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
          OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

          OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
          OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
          OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
          OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

          OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
          OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
          OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
          OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

          OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
          OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
          OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
          OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

          OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
          scratch[j*16+2]    = bS0x + bS1x;
          scratch[j*16+2+1]    = bS0y + bS1y;
          OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

          scratch[j*16+2*5]  = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*5+1]  = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
          OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
          OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
          OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

          scratch[j*16+2*9]  = tUb2x*alphax + tUb2y*alphay;
          scratch[j*16+2*9+1]  = tUb2y*alphax - tUb2x*alphay;
          scratch[j*16+2*13] = tUb3x*alphay + tUb3y*alphax;
          scratch[j*16+2*13+1] = tUb3y*alphay - tUb3x*alphax;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
          OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
          OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
          OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

          OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
          OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
          OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
          OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

          OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
          OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
          OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
          OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

          OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
          OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
          OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
          OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

          OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
          OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
          OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
          OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

          scratch[j*16+2*2]  = cS1x + cS0x;
          scratch[j*16+2*2+1]  = cS0y + cS1y;
          scratch[j*16+2*6]  = cS0y - cS1y;
          scratch[j*16+2*6+1]  = cS1x - cS0x;

          scratch[j*16+2*10] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*10+1] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*14] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*14+1] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
          OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
          OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
          OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

          OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
          OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
          OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
          OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

          OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
          OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
          OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
          OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

          OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
          OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
          OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
          OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

          OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
          scratch[j*16+2*3]  = dS1x + dS0x;
          scratch[j*16+2*3+1]  = dS0y + dS1y;
          OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

          scratch[j*16+2*7]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*7+1]  = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
          OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
          OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
          OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

          scratch[j*16+2*11] = tUd2x*alphay + tUd2y*alphax;
          scratch[j*16+2*11+1] = tUd2y*alphay - tUd2x*alphax;
          scratch[j*16+2*15] = tUd3x*alphax + tUd3y*alphay;
          scratch[j*16+2*15+1] = tUd3y*alphax - tUd3x*alphay;
        }

        // Bit reversal
        OC_INDEX ja;
        OXS_FFT_REAL_TYPE* w[16];
        for(ja=0;ja<16;++ja) {
          OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
          w[ja] = wtmp;
          if(wtmp<bv0) {
            // Swap
            memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
          }
        }

        // Do 4 lower (bottom/final) -level dragonflies
        for(ja=0;ja<16;ja+=4) {
          OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
          for(OC_INDEX jb=0;jb<bw;jb+=2) {
            OXS_FFT_REAL_TYPE Uax = sv[jb*16];
            OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
            OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
            OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

            OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
            OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
            OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
            OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

            OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
            OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
            OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
            OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

            OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
            OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
            OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
            OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

            *(w[ja]+jb)     = BaSx + BbSx;
            *(w[ja]+jb+1)   = BaSy + BbSy;
            *(w[ja+1]+jb)   = BaSx - BbSx;
            *(w[ja+1]+jb+1) = BaSy - BbSy;

            *(w[ja+2]+jb)   = BaDx + BbDy;
            *(w[ja+2]+jb+1) = BaDy - BbDx;
            *(w[ja+3]+jb)   = BaDx - BbDy;
            *(w[ja+3]+jb+1) = BaDy + BbDx;
          }
        }

        i += 16;
      } while((--k)>0);
      offset = i;
      ++sptr;
    } while(sptr->stride>0);
  }
}

void
Oxs_FFTStrided::InverseFFTRadix4
(OXS_FFT_REAL_TYPE* arr) const
{ // NOTE: The bit-reversal code in this routine
  // is only valid for fftsize>=128.
  // The following Oc_Nop is a workaround for an optimization bug in the
  // version of gcc 4.2.1 that ships alongside Mac OS X Lion.
  const OC_INDEX block32_count = Oc_Nop(4*(log2fftsize%2));
  const OC_INDEX block16_count = block32_count + 4;

  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    // Do power-of-4 blocks, with preorder traversal/tree walk
    OC_INDEX offset = 0;
    const PreorderTraversalState* sptr = ptsRadix4;
    OXS_FFT_REAL_TYPE* v = arr +2*block; // Working vector.
    do {
      // Stride is measured in units of OXS_FFT_REAL_TYPE's.
      OC_INDEX stride = sptr->stride;
      OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;
      do {
        // Do power-of-4 butterflies with "stride" at
        // position v + rstride*offset
        OXS_FFT_REAL_TYPE *const va = v + rstride*offset;
        OXS_FFT_REAL_TYPE *const vb = va + stride;
        OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
        OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
        OC_INDEX i;
        for(i=0;i<bw;i+=2) {
          // w^0
          OXS_FFT_REAL_TYPE x0 = va[i];
          OXS_FFT_REAL_TYPE y0 = va[i+1];
          OXS_FFT_REAL_TYPE x1 = vb[i];
          OXS_FFT_REAL_TYPE y1 = vb[i+1];
          OXS_FFT_REAL_TYPE x2 = vc[i];
          OXS_FFT_REAL_TYPE x3 = vd[i];

          OXS_FFT_REAL_TYPE y2 = vc[i+1];

          OXS_FFT_REAL_TYPE sx1 = x0 + x2;
          OXS_FFT_REAL_TYPE dx1 = x0 - x2;

          OXS_FFT_REAL_TYPE sx2 = x1 + x3;
          OXS_FFT_REAL_TYPE dx2 = x1 - x3;

          OXS_FFT_REAL_TYPE y3 = vd[i+1];

          OXS_FFT_REAL_TYPE sy1 = y0 + y2;
          OXS_FFT_REAL_TYPE dy1 = y0 - y2;
          OXS_FFT_REAL_TYPE sy2 = y1 + y3;
          OXS_FFT_REAL_TYPE dy2 = y1 - y3;

          va[i]   = sx1 + sx2;        va[i+1] = sy1 + sy2;
          vb[i]   = sx1 - sx2;        vb[i+1] = sy1 - sy2;
          vc[i]   = dx1 - dy2;        vc[i+1] = dy1 + dx2;
          vd[i]   = dx1 + dy2;        vd[i+1] = dy1 - dx2;
        }
        i = rstride;
        while(i<stride) {
          for(OC_INDEX j=i;j-i<bw;j+=2) {
            OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
            OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
            OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
            OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
            OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
            OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

            OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
            OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
            dx1 -= x2;    // dx1 := R0
#define dx2 x1
            dx2 -= x3;    // dx2 := R1

#define y2 x2
            y2 = vc[j+1]; // y2 := R2
#define y3 x3
            y3 = vd[j+1]; // y3 := R3

            va[j]              = sx1 + sx2;
#define txa sx1
            txa -= sx2;     // txa := R4

#define sy1 sx2
            sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
            dy1 -= y2;      // dy1 := R6
            OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
            dy2 -= y3;      // dy2 := R7

#define mx1 y3
            mx1 = U[0];   // mx1 := R3

            va[j+1]              = sy1 + sy2;
#define tya sy1
            tya -= sy2;     // tya := R5

#define mx1_txa y2
            mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
            mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
            my1 = U[1];    // my1 := R8

#define my1_tya tya
            my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
            my1_txa *= my1; // my1_txa := R4

            vb[j]     = mx1_txa + my1_tya;
            vb[j+1]   = mx1_tya - my1_txa;

#define txb mx1_txa
            txb = dx1 - dy2;  // txb := R2
#define txc dx1
            txc += dy2;       // txc := R0
#define tyb mx1_tya
            tyb = dy1 + dx2;  // tyb := R3
#define tyc dy1
            tyc -= dx2;       // tyc := R6

#define mx2 my1_txa
            mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
            mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
            mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
            my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
            my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
            my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
            Cx += my2_tyb; // Cx := R1
            vc[j]   = Cx;

#define mx3 mx2
            mx3 = U[4];  // mx3 := R4
#define my3 my2
            my3 = U[5];  // my3 := R5

            OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
            OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
            OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
            OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
            OXS_FFT_REAL_TYPE Cy = mx2_tyb - my2_txb; // Cy := R2
            OXS_FFT_REAL_TYPE Dx = mx3_txc + my3_tyc;
            OXS_FFT_REAL_TYPE Dy = mx3_tyc - my3_txc;

            vc[j+1] = Cy;
            vd[j]   = Dx;
            vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
          }
          i+=rstride;
          U+=6;
        }
        stride /= 4;
      } while(stride>8*rstride);

      if(block32_count>0) {
        { // i=0
          OXS_FFT_REAL_TYPE* const va = v + rstride*offset;
          for(OC_INDEX j1=0 ; j1<rstride*128 ; j1+=rstride*32) {
            for(OC_INDEX j2=j1 ; j2<j1+bw ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
              OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
              OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
              OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];
              va[j2]              = ax0 + ax1;
              va[j2+1]            = ay0 + ay1;
              va[j2+rstride*16]   = ax0 - ax1;
              va[j2+rstride*16+1] = ay0 - ay1;
              va[j2+rstride*8]    = cx0 + cx1;
              va[j2+rstride*8+1]  = cy0 + cy1;
              va[j2+rstride*24]   = cy1 - cy0;
              va[j2+rstride*24+1] = cx0 - cx1;
            }
          }
        }
        // NB: U currently points to the tail of the UForwardRadix4 array,
        // which is a subsegment holding the 32nd roots of unity.  That
        // subsegment doesn't contain the (1,0) root, so the access below
        // is to U - 2.
        for(OC_INDEX i=1;i<8;++i) {
          OXS_FFT_REAL_TYPE* const va = v + rstride*(offset + i);
          const OXS_FFT_REAL_TYPE amx = U[2*i-2];
          const OXS_FFT_REAL_TYPE amy = U[2*i-1];
          for(OC_INDEX j1=0;j1<rstride*128;j1+=rstride*32) {
            for(OC_INDEX j2=j1 ; j2<j1+bw ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
              OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
              OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
              OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];

              OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
              va[j2]        = ax0 + ax1;
              va[j2+1]      = ay0 + ay1;
              OXS_FFT_REAL_TYPE adify = ay0 - ay1;

              va[j2+rstride*16]   = amx*adifx + amy*adify;
              va[j2+rstride*16+1] = amx*adify - amy*adifx;

              OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
              va[j2+rstride*8]   = cx0 + cx1;
              va[j2+rstride*8+1] = cy0 + cy1;
              OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

              va[j2+rstride*24]   = amy*cdifx - amx*cdify;
              va[j2+rstride*24+1] = amy*cdify + amx*cdifx;
            }
          }
        }
      }

      OC_INDEX i = offset;
      OC_INDEX k = block16_count;
      do {
        // Bottom level 16-passes, with built-in bit-flip shuffling
        const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
        const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

        OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

        OC_INDEX j;

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE a0x = bv0[j];
          OXS_FFT_REAL_TYPE a0y = bv0[j+1];
          OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
          OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

          OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
          OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
          OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
          OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

          OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
          OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
          OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
          OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

          OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
          OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
          OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
          OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

          scratch[j*16]        = aS0x + aS1x;
          scratch[j*16+1]      = aS0y + aS1y;
          scratch[j*16+2*4]    = aS0x - aS1x;
          scratch[j*16+2*4+1]  = aS0y - aS1y;

          scratch[j*16+2*8]    = aD0x - aD1y;
          scratch[j*16+2*8+1]  = aD0y + aD1x;
          scratch[j*16+2*12]   = aD0x + aD1y;
          scratch[j*16+2*12+1] = aD0y - aD1x;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
          OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
          OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
          OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

          OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
          OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
          OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
          OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

          OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
          OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
          OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
          OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

          OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
          OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
          OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
          OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

          OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
          scratch[j*16+2]      = bS0x + bS1x;
          scratch[j*16+2+1]    = bS0y + bS1y;
          OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

          scratch[j*16+2*5]    = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*5+1]  = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
          OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
          OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
          OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

          scratch[j*16+2*9]    = tUb2x*alphax - tUb2y*alphay;
          scratch[j*16+2*9+1]  = tUb2y*alphax + tUb2x*alphay;
          scratch[j*16+2*13]   = tUb3x*alphay - tUb3y*alphax;
          scratch[j*16+2*13+1] = tUb3y*alphay + tUb3x*alphax;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
          OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
          OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
          OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

          OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
          OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
          OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
          OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

          OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
          OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
          OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
          OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

          OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
          OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
          OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
          OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

          OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
          OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
          OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
          OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

          scratch[j*16+2*2]    = cS0x + cS1x;
          scratch[j*16+2*2+1]  = cS0y + cS1y;
          scratch[j*16+2*6]    = cS1y - cS0y;
          scratch[j*16+2*6+1]  = cS0x - cS1x;

          scratch[j*16+2*10]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*10+1] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*14]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*14+1] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
          OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
          OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
          OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

          OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
          OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
          OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
          OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

          OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
          OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
          OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
          OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

          OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
          OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
          OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
          OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

          OXS_FFT_REAL_TYPE tUd1x   = dS0x - dS1x;
          scratch[j*16+2*3]    = dS0x + dS1x;
          scratch[j*16+2*3+1]  = dS0y + dS1y;
          OXS_FFT_REAL_TYPE tUd1y   = dS1y - dS0y;

          scratch[j*16+2*7]    = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*7+1]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
          OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
          OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
          OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

          scratch[j*16+2*11]   = tUd2x*alphay - tUd2y*alphax;
          scratch[j*16+2*11+1] = tUd2y*alphay + tUd2x*alphax;
          scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
          scratch[j*16+2*15+1] = tUd3x*alphay - tUd3y*alphax;
        }

        // Bit reversal
        OC_INDEX ja;
        OXS_FFT_REAL_TYPE* w[16];
        for(ja=0;ja<16;++ja) {
          OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
          w[ja] = wtmp;
          if(wtmp<bv0) {
            // Swap
            memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
          }
        }

        // Do 4 lower (bottom/final) -level dragonflies
        for(ja=0;ja<16;ja+=4) {
          OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
          for(OC_INDEX jb=0;jb<bw;jb+=2) {
            OXS_FFT_REAL_TYPE Uax = sv[jb*16];
            OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
            OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
            OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

            OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
            OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
            OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
            OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

            OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
            OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
            OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
            OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

            OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
            OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
            OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
            OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

            *(w[ja]+jb)     = BaSx + BbSx;
            *(w[ja]+jb+1)   = BaSy + BbSy;
            *(w[ja+1]+jb)   = BaSx - BbSx;
            *(w[ja+1]+jb+1) = BaSy - BbSy;

            *(w[ja+2]+jb)   = BaDx - BbDy;
            *(w[ja+2]+jb+1) = BaDy + BbDx;
            *(w[ja+3]+jb)   = BaDx + BbDy;
            *(w[ja+3]+jb+1) = BaDy - BbDx;
          }
        }

        i += 16;
      } while((--k)>0);
      offset = i;
      ++sptr;
    } while(sptr->stride>0);
  }
}

void
Oxs_FFTStrided::ForwardFFTRadix4ZP
(OXS_FFT_REAL_TYPE* arr) const
{ // Version of ForwardFFTRadix4 which assumes that
  // 2*csize_base<=fftsize, i.e., that there is at least
  // two-times zero padding.
  // NOTE: The bit-reversal code in this routine is only valid
  // for fftsize>=128.
  // The following Oc_Nop is a workaround for an optimization bug in the
  // version of gcc 4.2.1 that ships alongside Mac OS X Lion.
  const OC_INDEX block32_count = Oc_Nop(4*(log2fftsize%2));
  const OC_INDEX block16_count = block32_count + 4;

  OC_INDEX bw = OFS_ARRAY_BLOCKSIZE + (arrcount % OFS_ARRAY_BLOCKSIZE);
  if(bw>OFS_ARRAY_MAXBLOCKSIZE) bw -= OFS_ARRAY_BLOCKSIZE;
  if(bw>arrcount) bw=arrcount;
  bw *= 2;

  OC_INDEX block=0;
  while(block<2*arrcount) {
    // Do power-of-4 blocks, with preorder traversal/tree walk
    OXS_FFT_REAL_TYPE* const v = arr + block;

    // Fill out zero-padding
    {
      for(OC_INDEX i=csize_base;i<fftsize/2;++i) {
        OXS_FFT_REAL_TYPE* w = v + i*rstride;
        for(OC_INDEX j=0;j<bw;++j) w[j] = 0.0;
      }
    }

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    OC_INDEX offset = 0;
    const PreorderTraversalState* sptr = ptsRadix4;
    OC_INDEX stride = sptr->stride;
    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;

    {
      // Do power-of-4 butterflies with "stride" at
      // position v + rstride*offset
      OXS_FFT_REAL_TYPE *const va = v + rstride*offset;
      OXS_FFT_REAL_TYPE *const vb = va + stride;
      OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
      OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
      OC_INDEX i;
      for(i=0;i<bw;i+=2) {
        // w^0
        OXS_FFT_REAL_TYPE x0 = va[i];
        OXS_FFT_REAL_TYPE y0 = va[i+1];
        OXS_FFT_REAL_TYPE x1 = vb[i];
        OXS_FFT_REAL_TYPE y1 = vb[i+1];
        va[i]   = x0 + x1;      va[i+1] = y0 + y1;
        vb[i]   = x0 - x1;      vb[i+1] = y0 - y1;
        vc[i]   = x0 + y1;      vc[i+1] = y0 - x1;
        vd[i]   = x0 - y1;      vd[i+1] = y0 + x1;
      }
      i = rstride;
      while(i<stride) {
        for(OC_INDEX j=i;j-i<bw;j+=2) {
          OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
          OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
          OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
          OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7

          va[j]                 = x0 + x1;
          OXS_FFT_REAL_TYPE txa = x0 - x1;     // txa := R4

          OXS_FFT_REAL_TYPE mx1 = U[0];   // mx1 := R3

          va[j+1]               = y0 + y1;
          OXS_FFT_REAL_TYPE tya = y0 - y1;     // tya := R5

          OXS_FFT_REAL_TYPE mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
          mx1_tya *= tya;    // mx1_tya := R3

          OXS_FFT_REAL_TYPE my1 = U[1];    // my1 := R8

#define my1_tya tya
          my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
          my1_txa *= my1; // my1_txa := R4

          vb[j]     = mx1_txa - my1_tya;
          vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
          txb = x0 + y1;  // txb := R2
#define txc x0
          txc -= y1;       // txc := R0
#define tyb mx1_tya
          tyb = y0 - x1;  // tyb := R3
#define tyc y0
          tyc += x1;       // tyc := R6

#define mx2 my1_txa
          mx2     = U[2]; //     mx2 := R4
#define mx2_tyb y1
          mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb x1
          mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
          my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
          my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
          my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
          Cx -= my2_tyb; // Cx := R1
          vc[j]   = Cx;

#define mx3 mx2
          mx3 = U[4];  // mx3 := R4
#define my3 my2
          my3 = U[5];  // my3 := R5

          OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
          OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
          OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
          OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0

          vc[j+1] = mx2_tyb + my2_txb;
          vd[j]   = mx3_txc - my3_tyc;
          vd[j+1] = my3_txc + mx3_tyc;

#undef mx1_tya
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
        }
        i+=rstride;
        U+=6;
      }
      stride /= 4;
    }


    do {

      if(offset>0) {
        // Stride is measured in units of OXS_FFT_REAL_TYPE's.
        stride = sptr->stride;
        U = UForwardRadix4 + sptr->uoff;
      }

      while(stride>8*rstride) {
        // Do power-of-4 butterflies with "stride" at
        // position v + rstride*offset
        OXS_FFT_REAL_TYPE *const va = v + rstride*offset;
        OXS_FFT_REAL_TYPE *const vb = va + stride;
        OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
        OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
        OC_INDEX i;
        for(i=0;i<bw;i+=2) {
          // w^0
          OXS_FFT_REAL_TYPE x0 = va[i];
          OXS_FFT_REAL_TYPE y0 = va[i+1];
          OXS_FFT_REAL_TYPE x1 = vb[i];
          OXS_FFT_REAL_TYPE y1 = vb[i+1];
          OXS_FFT_REAL_TYPE x2 = vc[i];
          OXS_FFT_REAL_TYPE x3 = vd[i];

          OXS_FFT_REAL_TYPE y2 = vc[i+1];

          OXS_FFT_REAL_TYPE sx1 = x0 + x2;
          OXS_FFT_REAL_TYPE dx1 = x0 - x2;

          OXS_FFT_REAL_TYPE sx2 = x1 + x3;
          OXS_FFT_REAL_TYPE dx2 = x1 - x3;

          OXS_FFT_REAL_TYPE y3 = vd[i+1];

          OXS_FFT_REAL_TYPE sy1 = y0 + y2;
          OXS_FFT_REAL_TYPE dy1 = y0 - y2;
          OXS_FFT_REAL_TYPE sy2 = y1 + y3;
          OXS_FFT_REAL_TYPE dy2 = y1 - y3;

          va[i]   = sx1 + sx2;
          va[i+1] = sy1 + sy2;
          vb[i]   = sx1 - sx2;
          vb[i+1] = sy1 - sy2;
          vc[i]   = dx1 + dy2;
          vc[i+1] = dy1 - dx2;
          vd[i]   = dx1 - dy2;
          vd[i+1] = dy1 + dx2;
        }
        i = rstride;
        while(i<stride) {
          for(OC_INDEX j=i;j-i<bw;j+=2) {
            OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
            OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
            OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
            OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
            OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
            OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

            OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
            OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
            dx1 -= x2;    // dx1 := R0
#define dx2 x1
            dx2 -= x3;    // dx2 := R1

#define y2 x2
            y2 = vc[j+1]; // y2 := R2
#define y3 x3
            y3 = vd[j+1]; // y3 := R3

            va[j]              = sx1 + sx2;
#define txa sx1
            txa -= sx2;     // txa := R4

#define sy1 sx2
            sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
            dy1 -= y2;      // dy1 := R6
            OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
            dy2 -= y3;      // dy2 := R7

#define mx1 y3
            mx1 = U[0];   // mx1 := R3

            va[j+1]              = sy1 + sy2;
#define tya sy1
            tya -= sy2;     // tya := R5

#define mx1_txa y2
            mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
            mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
            my1 = U[1];    // my1 := R8

#define my1_tya tya
            my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
            my1_txa *= my1; // my1_txa := R4

            vb[j]     = mx1_txa - my1_tya;
            vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
            txb = dx1 + dy2;  // txb := R2
#define txc dx1
            txc -= dy2;       // txc := R0
#define tyb mx1_tya
            tyb = dy1 - dx2;  // tyb := R3
#define tyc dy1
            tyc += dx2;       // tyc := R6

#define mx2 my1_txa
            mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
            mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
            mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
            my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
            my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
            my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
            Cx -= my2_tyb; // Cx := R1
            vc[j]   = Cx;

#define mx3 mx2
            mx3 = U[4];  // mx3 := R4
#define my3 my2
            my3 = U[5];  // my3 := R5

            OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
            OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
            OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
            OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
            OXS_FFT_REAL_TYPE Cy = mx2_tyb + my2_txb; // Cy := R2
            OXS_FFT_REAL_TYPE Dx = mx3_txc - my3_tyc;
            OXS_FFT_REAL_TYPE Dy = my3_txc + mx3_tyc;

            vc[j+1] = Cy;
            vd[j]   = Dx;
            vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
          }
          i+=rstride;
          U+=6;
        }
        stride /= 4;
      }

      if(block32_count>0) {
        { // i=0
          OXS_FFT_REAL_TYPE* const va = v + rstride*offset;
          for(OC_INDEX j1=0 ; j1<rstride*128 ; j1+=rstride*32) {
            for(OC_INDEX j2=j1 ; j2<j1+bw ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
              OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
              OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
              OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];
              va[j2]                    = ax0 + ax1;
              va[j2+1]                  = ay0 + ay1;
              va[j2+rstride*16]   = ax0 - ax1;
              va[j2+rstride*16+1] = ay0 - ay1;
              va[j2+rstride*8]   = cx0 + cx1;
              va[j2+rstride*8+1] = cy0 + cy1;
              va[j2+rstride*24]   = cy0 - cy1;
              va[j2+rstride*24+1] = cx1 - cx0;
            }
          }
        }
        // NB: U currently points to the tail of the UForwardRadix4 array,
        // which is a subsegment holding the 32nd roots of unity.  That
        // subsegment doesn't contain the (1,0) root, so the access below
        // is to U - 2.
        for(OC_INDEX i=1;i<8;++i) {
          OXS_FFT_REAL_TYPE* const va = v + rstride*(offset + i);
          const OXS_FFT_REAL_TYPE amx = U[2*i-2];
          const OXS_FFT_REAL_TYPE amy = U[2*i-1];
          for(OC_INDEX j1=0;j1<rstride*128;j1+=rstride*32) {
            for(OC_INDEX j2=j1 ; j2<j1+bw ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
              OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
              OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
              OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];

              OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
              va[j2]        = ax0 + ax1;
              va[j2+1]      = ay0 + ay1;
              OXS_FFT_REAL_TYPE adify = ay0 - ay1;

              va[j2+rstride*16]   = amx*adifx - amy*adify;
              va[j2+rstride*16+1] = amx*adify + amy*adifx;

              OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
              va[j2+rstride*8]   = cx0 + cx1;
              va[j2+rstride*8+1] = cy0 + cy1;
              OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

              va[j2+rstride*24]   = amx*cdify + amy*cdifx;
              va[j2+rstride*24+1] = amy*cdify - amx*cdifx;
            }
          }
        }
      }

      OC_INDEX i = offset;
      OC_INDEX k=block16_count;
      do {
        // Bottom level 16-passes, with built-in bit-flip shuffling
        const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
        const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

        OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

        OC_INDEX j;

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE a0x = bv0[j];
          OXS_FFT_REAL_TYPE a0y = bv0[j+1];
          OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
          OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

          OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
          OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
          OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
          OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

          OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
          OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
          OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
          OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

          OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
          OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
          OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
          OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

          scratch[j*16]        = aS0x + aS1x;
          scratch[j*16+1]      = aS0y + aS1y;
          scratch[j*16+2*4]    = aS0x - aS1x;
          scratch[j*16+2*4+1]  = aS0y - aS1y;

          scratch[j*16+2*8]    = aD0x + aD1y;
          scratch[j*16+2*8+1]  = aD0y - aD1x;
          scratch[j*16+2*12]   = aD0x - aD1y;
          scratch[j*16+2*12+1] = aD0y + aD1x;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
          OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
          OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
          OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

          OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
          OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
          OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
          OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

          OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
          OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
          OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
          OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

          OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
          OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
          OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
          OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

          OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
          scratch[j*16+2]      = bS0x + bS1x;
          scratch[j*16+2+1]    = bS0y + bS1y;
          OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

          scratch[j*16+2*5]    = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*5+1]  = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
          OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
          OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
          OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

          scratch[j*16+2*9]    = tUb2x*alphax + tUb2y*alphay;
          scratch[j*16+2*9+1]  = tUb2y*alphax - tUb2x*alphay;
          scratch[j*16+2*13]   = tUb3x*alphay + tUb3y*alphax;
          scratch[j*16+2*13+1] = tUb3y*alphay - tUb3x*alphax;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
          OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
          OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
          OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

          OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
          OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
          OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
          OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

          OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
          OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
          OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
          OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

          OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
          OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
          OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
          OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

          OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
          OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
          OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
          OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

          scratch[j*16+2*2]    = cS1x + cS0x;
          scratch[j*16+2*2+1]  = cS0y + cS1y;
          scratch[j*16+2*6]    = cS0y - cS1y;
          scratch[j*16+2*6+1]  = cS1x - cS0x;

          scratch[j*16+2*10]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*10+1] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*14]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*14+1] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
          OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
          OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
          OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

          OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
          OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
          OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
          OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

          OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
          OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
          OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
          OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

          OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
          OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
          OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
          OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

          OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
          scratch[j*16+2*3]    = dS1x + dS0x;
          scratch[j*16+2*3+1]  = dS0y + dS1y;
          OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

          scratch[j*16+2*7]    = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*7+1]  = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
          OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
          OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
          OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

          scratch[j*16+2*11]   = tUd2x*alphay + tUd2y*alphax;
          scratch[j*16+2*11+1] = tUd2y*alphay - tUd2x*alphax;
          scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
          scratch[j*16+2*15+1] = tUd3y*alphax - tUd3x*alphay;
        }

        // Bit reversal
        OC_INDEX ja;
        OXS_FFT_REAL_TYPE* w[16];
        for(ja=0;ja<16;++ja) {
          OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
          w[ja] = wtmp;
          if(wtmp<bv0) {
            // Swap
            memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
          }
        }

        // Do 4 lower (bottom/final) -level dragonflies
        for(ja=0;ja<16;ja+=4) {
          OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
          for(OC_INDEX jb=0;jb<bw;jb+=2) {
            OXS_FFT_REAL_TYPE Uax = sv[jb*16];
            OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
            OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
            OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

            OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
            OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
            OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
            OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

            OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
            OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
            OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
            OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

            OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
            OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
            OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
            OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

            *(w[ja]+jb)     = BaSx + BbSx;
            *(w[ja]+jb+1)   = BaSy + BbSy;
            *(w[ja+1]+jb)   = BaSx - BbSx;
            *(w[ja+1]+jb+1) = BaSy - BbSy;

            *(w[ja+2]+jb)   = BaDx + BbDy;
            *(w[ja+2]+jb+1) = BaDy - BbDx;
            *(w[ja+3]+jb)   = BaDx - BbDy;
            *(w[ja+3]+jb+1) = BaDy + BbDx;
          }
        }

        i += 16;
      } while((--k)>0);
      offset = i;
      ++sptr;
    } while(sptr->stride>0);

    // Update for next block
    block += bw;
    bw = 2*OFS_ARRAY_BLOCKSIZE;
  }
}

void
Oxs_FFTStrided::InverseFFTRadix4ZP
(OXS_FFT_REAL_TYPE* arr) const
{ // Version of InverseFFTRadix4 which assumes that
  // 2*csize_base<=fftsize, i.e., that there is at least
  // two-times zero padding.
  // NOTE: The bit-reversal code in this routine is only valid
  // for fftsize>=128.
  // The following Oc_Nop is a workaround for an optimization bug in the
  // version of gcc 4.2.1 that ships alongside Mac OS X Lion.
  const OC_INDEX block32_count = Oc_Nop(4*(log2fftsize%2));
  const OC_INDEX block16_count = block32_count + 4;

  OC_INDEX bw = OFS_ARRAY_BLOCKSIZE + (arrcount % OFS_ARRAY_BLOCKSIZE);
  if(bw>OFS_ARRAY_MAXBLOCKSIZE) bw -= OFS_ARRAY_BLOCKSIZE;
  if(bw>arrcount) bw=arrcount;
  bw *= 2;

  OC_INDEX block=0;
  while(block<2*arrcount) {
    // Do power-of-4 blocks, with preorder traversal/tree walk
    OC_INDEX offset = 0;
    const PreorderTraversalState* sptr = ptsRadix4;
    OXS_FFT_REAL_TYPE* v = arr + block; // Working vector.
    do {
      // Stride is measured in units of OXS_FFT_REAL_TYPE's.
      OC_INDEX stride = sptr->stride;
      OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;
      do {
        // Do power-of-4 butterflies with "stride" at
        // position v + rstride*offset
        OXS_FFT_REAL_TYPE *const va = v + rstride*offset;
        OXS_FFT_REAL_TYPE *const vb = va + stride;
        OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
        OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
        OC_INDEX i;
        for(i=0;i<bw;i+=2) {
          // w^0
          OXS_FFT_REAL_TYPE x0 = va[i];
          OXS_FFT_REAL_TYPE y0 = va[i+1];
          OXS_FFT_REAL_TYPE x1 = vb[i];
          OXS_FFT_REAL_TYPE y1 = vb[i+1];
          OXS_FFT_REAL_TYPE x2 = vc[i];
          OXS_FFT_REAL_TYPE x3 = vd[i];

          OXS_FFT_REAL_TYPE y2 = vc[i+1];

          OXS_FFT_REAL_TYPE sx1 = x0 + x2;
          OXS_FFT_REAL_TYPE dx1 = x0 - x2;

          OXS_FFT_REAL_TYPE sx2 = x1 + x3;
          OXS_FFT_REAL_TYPE dx2 = x1 - x3;

          OXS_FFT_REAL_TYPE y3 = vd[i+1];

          OXS_FFT_REAL_TYPE sy1 = y0 + y2;
          OXS_FFT_REAL_TYPE dy1 = y0 - y2;
          OXS_FFT_REAL_TYPE sy2 = y1 + y3;
          OXS_FFT_REAL_TYPE dy2 = y1 - y3;

          va[i]   = sx1 + sx2;        va[i+1] = sy1 + sy2;
          vb[i]   = sx1 - sx2;        vb[i+1] = sy1 - sy2;
          vc[i]   = dx1 - dy2;        vc[i+1] = dy1 + dx2;
          vd[i]   = dx1 + dy2;        vd[i+1] = dy1 - dx2;
        }
        i = rstride;
        while(i<stride) {
          for(OC_INDEX j=i;j-i<bw;j+=2) {
            OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
            OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
            OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
            OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
            OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
            OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

            OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
            OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
            dx1 -= x2;    // dx1 := R0
#define dx2 x1
            dx2 -= x3;    // dx2 := R1

#define y2 x2
            y2 = vc[j+1]; // y2 := R2
#define y3 x3
            y3 = vd[j+1]; // y3 := R3

            va[j]              = sx1 + sx2;
#define txa sx1
            txa -= sx2;     // txa := R4

#define sy1 sx2
            sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
            dy1 -= y2;      // dy1 := R6
            OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
            dy2 -= y3;      // dy2 := R7

#define mx1 y3
            mx1 = U[0];   // mx1 := R3

            va[j+1]              = sy1 + sy2;
#define tya sy1
            tya -= sy2;     // tya := R5

#define mx1_txa y2
            mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
            mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
            my1 = U[1];    // my1 := R8

#define my1_tya tya
            my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
            my1_txa *= my1; // my1_txa := R4

            vb[j]     = mx1_txa + my1_tya;
            vb[j+1]   = mx1_tya - my1_txa;

#define txb mx1_txa
            txb = dx1 - dy2;  // txb := R2
#define txc dx1
            txc += dy2;       // txc := R0
#define tyb mx1_tya
            tyb = dy1 + dx2;  // tyb := R3
#define tyc dy1
            tyc -= dx2;       // tyc := R6

#define mx2 my1_txa
            mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
            mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
            mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
            my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
            my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
            my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
            Cx += my2_tyb; // Cx := R1
            vc[j]   = Cx;

#define mx3 mx2
            mx3 = U[4];  // mx3 := R4
#define my3 my2
            my3 = U[5];  // my3 := R5

            OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
            OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
            OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
            OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
            OXS_FFT_REAL_TYPE Cy = mx2_tyb - my2_txb; // Cy := R2
            OXS_FFT_REAL_TYPE Dx = mx3_txc + my3_tyc;
            OXS_FFT_REAL_TYPE Dy = mx3_tyc - my3_txc;

            vc[j+1] = Cy;
            vd[j]   = Dx;
            vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
          }
          i+=rstride;
          U+=6;
        }
        stride /= 4;
      } while(stride>8*rstride);

      if(block32_count>0) {
        { // i=0
          OXS_FFT_REAL_TYPE* const va = v + rstride*offset;
          for(OC_INDEX j1=0 ; j1<rstride*128 ; j1+=rstride*32) {
            for(OC_INDEX j2=j1 ; j2<j1+bw ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
              OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
              OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
              OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];
              va[j2]              = ax0 + ax1;
              va[j2+1]            = ay0 + ay1;
              va[j2+rstride*16]   = ax0 - ax1;
              va[j2+rstride*16+1] = ay0 - ay1;
              va[j2+rstride*8]    = cx0 + cx1;
              va[j2+rstride*8+1]  = cy0 + cy1;
              va[j2+rstride*24]   = cy1 - cy0;
              va[j2+rstride*24+1] = cx0 - cx1;
            }
          }
        }
        // NB: U currently points to the tail of the UForwardRadix4 array,
        // which is a subsegment holding the 32nd roots of unity.  That
        // subsegment doesn't contain the (1,0) root, so the access below
        // is to U - 2.
        for(OC_INDEX i=1;i<8;++i) {
          OXS_FFT_REAL_TYPE* const va = v + rstride*(offset + i);
          const OXS_FFT_REAL_TYPE amx = U[2*i-2];
          const OXS_FFT_REAL_TYPE amy = U[2*i-1];
          for(OC_INDEX j1=0;j1<rstride*128;j1+=rstride*32) {
            for(OC_INDEX j2=j1 ; j2<j1+bw ; j2+=2) {
              OXS_FFT_REAL_TYPE ax0 = va[j2];
              OXS_FFT_REAL_TYPE ay0 = va[j2+1];
              OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
              OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
              OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
              OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];
              OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
              OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];

              OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
              va[j2]        = ax0 + ax1;
              va[j2+1]      = ay0 + ay1;
              OXS_FFT_REAL_TYPE adify = ay0 - ay1;

              va[j2+rstride*16]   = amx*adifx + amy*adify;
              va[j2+rstride*16+1] = amx*adify - amy*adifx;

              OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
              va[j2+rstride*8]   = cx0 + cx1;
              va[j2+rstride*8+1] = cy0 + cy1;
              OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

              va[j2+rstride*24]   = amy*cdifx - amx*cdify;
              va[j2+rstride*24+1] = amy*cdify + amx*cdifx;
            }
          }
        }
      }

      OC_INDEX i = offset;
      OC_INDEX k = block16_count;
      do {
        // Bottom level 16-passes, with built-in bit-flip shuffling
        const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
        const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

        OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

        OC_INDEX j;

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE a0x = bv0[j];
          OXS_FFT_REAL_TYPE a0y = bv0[j+1];
          OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
          OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

          OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
          OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
          OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
          OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

          OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
          OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
          OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
          OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

          OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
          OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
          OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
          OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

          scratch[j*16]        = aS0x + aS1x;
          scratch[j*16+1]      = aS0y + aS1y;
          scratch[j*16+2*4]    = aS0x - aS1x;
          scratch[j*16+2*4+1]  = aS0y - aS1y;

          scratch[j*16+2*8]    = aD0x - aD1y;
          scratch[j*16+2*8+1]  = aD0y + aD1x;
          scratch[j*16+2*12]   = aD0x + aD1y;
          scratch[j*16+2*12+1] = aD0y - aD1x;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
          OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
          OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
          OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

          OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
          OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
          OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
          OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

          OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
          OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
          OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
          OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

          OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
          OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
          OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
          OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

          OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
          scratch[j*16+2]         = bS0x + bS1x;
          scratch[j*16+2+1]       = bS0y + bS1y;
          OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

          scratch[j*16+2*5]       = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*5+1]     = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
          OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
          OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
          OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

          scratch[j*16+2*9]    = tUb2x*alphax - tUb2y*alphay;
          scratch[j*16+2*9+1]  = tUb2y*alphax + tUb2x*alphay;
          scratch[j*16+2*13]   = tUb3x*alphay - tUb3y*alphax;
          scratch[j*16+2*13+1] = tUb3y*alphay + tUb3x*alphax;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
          OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
          OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
          OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

          OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
          OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
          OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
          OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

          OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
          OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
          OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
          OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

          OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
          OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
          OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
          OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

          OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
          OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
          OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
          OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

          scratch[j*16+2*2]    = cS0x + cS1x;
          scratch[j*16+2*2+1]  = cS0y + cS1y;
          scratch[j*16+2*6]    = cS1y - cS0y;
          scratch[j*16+2*6+1]  = cS0x - cS1x;

          scratch[j*16+2*10]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*10+1] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*14]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*14+1] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        }

        for(j=0;j<bw;j+=2) {
          OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
          OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
          OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
          OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

          OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
          OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
          OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
          OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

          OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
          OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
          OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
          OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

          OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
          OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
          OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
          OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

          OXS_FFT_REAL_TYPE tUd1x   = dS0x - dS1x;
          scratch[j*16+2*3] = dS0x + dS1x;
          scratch[j*16+2*3+1] = dS0y + dS1y;
          OXS_FFT_REAL_TYPE tUd1y   = dS1y - dS0y;

          scratch[j*16+2*7]  = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
          scratch[j*16+2*7+1]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

          OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
          OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
          OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
          OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

          scratch[j*16+2*11] = tUd2x*alphay - tUd2y*alphax;
          scratch[j*16+2*11+1] = tUd2y*alphay + tUd2x*alphax;
          scratch[j*16+2*15] = tUd3x*alphax + tUd3y*alphay;
          scratch[j*16+2*15+1] = tUd3x*alphay - tUd3y*alphax;
        }

        // Bit reversal.  This "...ZP" variant skips reversals
        // that affect what ends up in the upper half of the
        // results, since those are to be thrown away.
        OC_INDEX ja;
        OXS_FFT_REAL_TYPE* w[16];
        if(2*i<fftsize) {
          for(ja=0;ja<16;++ja) {
            OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
            w[ja] = wtmp;
            if(wtmp<bv0) {
              // Swap
              memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
            }
          }
        } else {
          for(ja=0;ja<16;ja+=2) w[ja] = v + bitreverse[i+ja];
        }

        // Do 4 lower (bottom/final) -level dragonflies
        // This "...ZP" variant assumes that the last half of the
        // results are to be thrown away, or equivalently, that
        // the pre-reversal odd (ja%2==1) results are not used.
        for(ja=0;ja<16;ja+=4) {
          OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
          for(OC_INDEX jb=0;jb<bw;jb+=2) {
            OXS_FFT_REAL_TYPE Uax = sv[jb*16];
            OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
            OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
            OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

            OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
            OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
            OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
            OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

            OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
            OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
            OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
            OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

            OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
            OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
            OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
            OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

            *(w[ja]+jb)     = BaSx + BbSx;
            *(w[ja]+jb+1)   = BaSy + BbSy;
            *(w[ja+2]+jb)   = BaDx - BbDy;
            *(w[ja+2]+jb+1) = BaDy + BbDx;
          }
        }

        i += 16;
      } while((--k)>0);
      offset = i;
      ++sptr;
    } while(sptr->stride>0);

    // Update for next block
    block += bw;
    bw = 2*OFS_ARRAY_BLOCKSIZE;
  }
}

void
Oxs_FFTStrided::ForwardFFTSize64
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 64

  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    // Do power-of-4 blocks, with preorder traversal/tree walk
    OXS_FFT_REAL_TYPE* const v = arr + 2*block;

    // Fill out zero-padding
    OC_INDEX i;
    for(i=csize_base;i<RTNFFTSIZE;++i) {
      OXS_FFT_REAL_TYPE* w = v + i*rstride;
      for(OC_INDEX j=0;j<bw;++j) w[j] = 0.0;
    }

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    const PreorderTraversalState* sptr = ptsRadix4;
    OC_INDEX stride = sptr->stride;
    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;

    // Do top-level power-of-4 butterflies
    OXS_FFT_REAL_TYPE *const va = v;
    OXS_FFT_REAL_TYPE *const vb = va + stride;
    OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
    OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
    for(i=0;i<bw;i+=2) {
      // w^0
      OXS_FFT_REAL_TYPE x0 = va[i];
      OXS_FFT_REAL_TYPE y0 = va[i+1];
      OXS_FFT_REAL_TYPE x1 = vb[i];
      OXS_FFT_REAL_TYPE y1 = vb[i+1];
      OXS_FFT_REAL_TYPE x2 = vc[i];
      OXS_FFT_REAL_TYPE x3 = vd[i];

      OXS_FFT_REAL_TYPE y2 = vc[i+1];

      OXS_FFT_REAL_TYPE sx1 = x0 + x2;
      OXS_FFT_REAL_TYPE dx1 = x0 - x2;

      OXS_FFT_REAL_TYPE sx2 = x1 + x3;
      OXS_FFT_REAL_TYPE dx2 = x1 - x3;

      OXS_FFT_REAL_TYPE y3 = vd[i+1];

      OXS_FFT_REAL_TYPE sy1 = y0 + y2;
      OXS_FFT_REAL_TYPE dy1 = y0 - y2;
      OXS_FFT_REAL_TYPE sy2 = y1 + y3;
      OXS_FFT_REAL_TYPE dy2 = y1 - y3;

      va[i]   = sx1 + sx2;
      va[i+1] = sy1 + sy2;
      vb[i]   = sx1 - sx2;
      vb[i+1] = sy1 - sy2;
      vc[i]   = dx1 + dy2;
      vc[i+1] = dy1 - dx2;
      vd[i]   = dx1 - dy2;
      vd[i+1] = dy1 + dx2;
    }
    i = rstride;
    while(i<stride) {
      for(OC_INDEX j=i;j-i<bw;j+=2) {
        OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
        OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
        OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
        OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
        OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
        OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

        OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
        OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
        dx1 -= x2;    // dx1 := R0
#define dx2 x1
        dx2 -= x3;    // dx2 := R1

#define y2 x2
        y2 = vc[j+1]; // y2 := R2
#define y3 x3
        y3 = vd[j+1]; // y3 := R3

        va[j]              = sx1 + sx2;
#define txa sx1
        txa -= sx2;     // txa := R4

#define sy1 sx2
        sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
        dy1 -= y2;      // dy1 := R6
        OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
        dy2 -= y3;      // dy2 := R7

#define mx1 y3
        mx1 = U[0];   // mx1 := R3

        va[j+1]              = sy1 + sy2;
#define tya sy1
        tya -= sy2;     // tya := R5

#define mx1_txa y2
        mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
        mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
        my1 = U[1];    // my1 := R8

#define my1_tya tya
        my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
        my1_txa *= my1; // my1_txa := R4

        vb[j]     = mx1_txa - my1_tya;
        vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
        txb = dx1 + dy2;  // txb := R2
#define txc dx1
        txc -= dy2;       // txc := R0
#define tyb mx1_tya
        tyb = dy1 - dx2;  // tyb := R3
#define tyc dy1
        tyc += dx2;       // tyc := R6

#define mx2 my1_txa
        mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
        mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
        mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
        my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
        my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
        my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
        Cx -= my2_tyb; // Cx := R1
        vc[j]   = Cx;

#define mx3 mx2
        mx3 = U[4];  // mx3 := R4
#define my3 my2
        my3 = U[5];  // my3 := R5

        OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
        OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
        OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
        OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
        OXS_FFT_REAL_TYPE Cy = mx2_tyb + my2_txb; // Cy := R2
        OXS_FFT_REAL_TYPE Dx = mx3_txc - my3_tyc;
        OXS_FFT_REAL_TYPE Dy = my3_txc + mx3_tyc;

        vc[j+1] = Cy;
        vd[j]   = Dx;
        vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
      }
      i+=rstride;
      U+=6;
    }


    for(i=0;i<64;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

      OC_INDEX j;

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
        OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j*16]        = aS0x + aS1x;
        scratch[j*16+1]      = aS0y + aS1y;
        scratch[j*16+2*4]    = aS0x - aS1x;
        scratch[j*16+2*4+1]  = aS0y - aS1y;

        scratch[j*16+2*8]    = aD0x + aD1y;
        scratch[j*16+2*8+1]  = aD0y - aD1x;
        scratch[j*16+2*12]   = aD0x - aD1y;
        scratch[j*16+2*12+1] = aD0y + aD1x;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
        OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
        OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j*16+2]      = bS0x + bS1x;
        scratch[j*16+2+1]    = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j*16+2*5]    = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*5+1]  = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

        scratch[j*16+2*9]    = tUb2x*alphax + tUb2y*alphay;
        scratch[j*16+2*9+1]  = tUb2y*alphax - tUb2x*alphay;
        scratch[j*16+2*13]   = tUb3x*alphay + tUb3y*alphax;
        scratch[j*16+2*13+1] = tUb3y*alphay - tUb3x*alphax;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
        OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
        OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
        OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
        OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

        scratch[j*16+2*2]    = cS1x + cS0x;
        scratch[j*16+2*2+1]  = cS0y + cS1y;
        scratch[j*16+2*6]    = cS0y - cS1y;
        scratch[j*16+2*6+1]  = cS1x - cS0x;

        scratch[j*16+2*10]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*10+1] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14+1] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
        OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
        OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

        OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
        OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
        OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
        OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

        OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
        scratch[j*16+2*3]    = dS1x + dS0x;
        scratch[j*16+2*3+1]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

        scratch[j*16+2*7]    = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*7+1]  = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
        OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

        scratch[j*16+2*11]   = tUd2x*alphay + tUd2y*alphax;
        scratch[j*16+2*11+1] = tUd2y*alphay - tUd2x*alphax;
        scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j*16+2*15+1] = tUd3y*alphax - tUd3x*alphay;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      for(ja=0;ja<16;++ja) {
        OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
        w[ja] = wtmp;
        if(wtmp<bv0) {
          // Swap
          memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
        for(OC_INDEX jb=0;jb<bw;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb*16];
          OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
          OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
          OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
          OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx + BbDy;
          *(w[ja+2]+jb+1) = BaDy - BbDx;
          *(w[ja+3]+jb)   = BaDx - BbDy;
          *(w[ja+3]+jb+1) = BaDy + BbDx;
        }
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize64
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 64
  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    // Do power-of-4 blocks, with preorder traversal/tree walk
    const PreorderTraversalState* sptr = ptsRadix4;
    OXS_FFT_REAL_TYPE* const v = arr + 2*block;

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    OC_INDEX stride = sptr->stride;
    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;

    // Do top-level power-of-4 butterflies
    OXS_FFT_REAL_TYPE *const va = v;
    OXS_FFT_REAL_TYPE *const vb = va + stride;
    OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
    OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
    OC_INDEX i;
    for(i=0;i<bw;i+=2) {
      // w^0
      OXS_FFT_REAL_TYPE x0 = va[i];
      OXS_FFT_REAL_TYPE y0 = va[i+1];
      OXS_FFT_REAL_TYPE x1 = vb[i];
      OXS_FFT_REAL_TYPE y1 = vb[i+1];
      OXS_FFT_REAL_TYPE x2 = vc[i];
      OXS_FFT_REAL_TYPE x3 = vd[i];

      OXS_FFT_REAL_TYPE y2 = vc[i+1];

      OXS_FFT_REAL_TYPE sx1 = x0 + x2;
      OXS_FFT_REAL_TYPE dx1 = x0 - x2;

      OXS_FFT_REAL_TYPE sx2 = x1 + x3;
      OXS_FFT_REAL_TYPE dx2 = x1 - x3;

      OXS_FFT_REAL_TYPE y3 = vd[i+1];

      OXS_FFT_REAL_TYPE sy1 = y0 + y2;
      OXS_FFT_REAL_TYPE dy1 = y0 - y2;
      OXS_FFT_REAL_TYPE sy2 = y1 + y3;
      OXS_FFT_REAL_TYPE dy2 = y1 - y3;

      va[i]   = sx1 + sx2;        va[i+1] = sy1 + sy2;
      vb[i]   = sx1 - sx2;        vb[i+1] = sy1 - sy2;
      vc[i]   = dx1 - dy2;        vc[i+1] = dy1 + dx2;
      vd[i]   = dx1 + dy2;        vd[i+1] = dy1 - dx2;
    }
    i = rstride;
    while(i<stride) {
      for(OC_INDEX j=i;j-i<bw;j+=2) {
        OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
        OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
        OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
        OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
        OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
        OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

        OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
        OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
        dx1 -= x2;    // dx1 := R0
#define dx2 x1
        dx2 -= x3;    // dx2 := R1

#define y2 x2
        y2 = vc[j+1]; // y2 := R2
#define y3 x3
        y3 = vd[j+1]; // y3 := R3

        va[j]              = sx1 + sx2;
#define txa sx1
        txa -= sx2;     // txa := R4

#define sy1 sx2
        sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
        dy1 -= y2;      // dy1 := R6
        OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
        dy2 -= y3;      // dy2 := R7

#define mx1 y3
        mx1 = U[0];   // mx1 := R3

        va[j+1]              = sy1 + sy2;
#define tya sy1
        tya -= sy2;     // tya := R5

#define mx1_txa y2
        mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
        mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
        my1 = U[1];    // my1 := R8

#define my1_tya tya
        my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
        my1_txa *= my1; // my1_txa := R4

        vb[j]     = mx1_txa + my1_tya;
        vb[j+1]   = mx1_tya - my1_txa;

#define txb mx1_txa
        txb = dx1 - dy2;  // txb := R2
#define txc dx1
        txc += dy2;       // txc := R0
#define tyb mx1_tya
        tyb = dy1 + dx2;  // tyb := R3
#define tyc dy1
        tyc -= dx2;       // tyc := R6

#define mx2 my1_txa
        mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
        mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
        mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
        my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
        my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
        my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
        Cx += my2_tyb; // Cx := R1
        vc[j]   = Cx;

#define mx3 mx2
        mx3 = U[4];  // mx3 := R4
#define my3 my2
        my3 = U[5];  // my3 := R5

        OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
        OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
        OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
        OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
        OXS_FFT_REAL_TYPE Cy = mx2_tyb - my2_txb; // Cy := R2
        OXS_FFT_REAL_TYPE Dx = mx3_txc + my3_tyc;
        OXS_FFT_REAL_TYPE Dy = mx3_tyc - my3_txc;

        vc[j+1] = Cy;
        vd[j]   = Dx;
        vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
      }
      i+=rstride;
      U+=6;
    }


    for(i=0;i<64;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

      OC_INDEX j;

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
        OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j*16]                 = aS0x + aS1x;
        scratch[j*16+1]               = aS0y + aS1y;
        scratch[j*16+2*4]  = aS0x - aS1x;
        scratch[j*16+2*4+1]  = aS0y - aS1y;

        scratch[j*16+2*8]  = aD0x - aD1y;
        scratch[j*16+2*8+1]  = aD0y + aD1x;
        scratch[j*16+2*12] = aD0x + aD1y;
        scratch[j*16+2*12+1] = aD0y - aD1x;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
        OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
        OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j*16+2]      = bS0x + bS1x;
        scratch[j*16+2+1]    = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j*16+2*5]    = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*5+1]  = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

        scratch[j*16+2*9]    = tUb2x*alphax - tUb2y*alphay;
        scratch[j*16+2*9+1]  = tUb2y*alphax + tUb2x*alphay;
        scratch[j*16+2*13]   = tUb3x*alphay - tUb3y*alphax;
        scratch[j*16+2*13+1] = tUb3y*alphay + tUb3x*alphax;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
        OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
        OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
        OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
        OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

        scratch[j*16+2*2]    = cS0x + cS1x;
        scratch[j*16+2*2+1]  = cS0y + cS1y;
        scratch[j*16+2*6]    = cS1y - cS0y;
        scratch[j*16+2*6+1]  = cS0x - cS1x;

        scratch[j*16+2*10]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*10+1] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14+1] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
        OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
        OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

        OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
        OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
        OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
        OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

        OXS_FFT_REAL_TYPE tUd1x   = dS0x - dS1x;
        scratch[j*16+2*3]    = dS0x + dS1x;
        scratch[j*16+2*3+1]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y   = dS1y - dS0y;

        scratch[j*16+2*7]    = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*7+1]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
        OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

        scratch[j*16+2*11]   = tUd2x*alphay - tUd2y*alphax;
        scratch[j*16+2*11+1] = tUd2y*alphay + tUd2x*alphax;
        scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j*16+2*15+1] = tUd3x*alphay - tUd3y*alphax;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      for(ja=0;ja<16;++ja) {
        OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
        w[ja] = wtmp;
        if(wtmp<bv0) {
          // Swap
          memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
        for(OC_INDEX jb=0;jb<bw;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb*16];
          OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
          OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
          OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
          OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx - BbDy;
          *(w[ja+2]+jb+1) = BaDy + BbDx;
          *(w[ja+3]+jb)   = BaDx + BbDy;
          *(w[ja+3]+jb+1) = BaDy - BbDx;
        }
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize64ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 64

  OC_INDEX bw = OFS_ARRAY_BLOCKSIZE + (arrcount % OFS_ARRAY_BLOCKSIZE);
  if(bw>OFS_ARRAY_MAXBLOCKSIZE) bw -= OFS_ARRAY_BLOCKSIZE;
  if(bw>arrcount) bw=arrcount;
  bw *= 2;

  OC_INDEX block=0;
  while(block<2*arrcount) {
    // Do power-of-4 blocks, with preorder traversal/tree walk
    OXS_FFT_REAL_TYPE* const v = arr + block;

    // Fill out zero-padding
    OC_INDEX i;
    for(i=csize_base;i<RTNFFTSIZE/2;++i) {
      OXS_FFT_REAL_TYPE* w = v + i*rstride;
      for(OC_INDEX j=0;j<bw;++j) w[j] = 0.0;
    }

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    const PreorderTraversalState* sptr = ptsRadix4;
    OC_INDEX stride = sptr->stride;
    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;

    // Do top-level power-of-4 butterflies
    OXS_FFT_REAL_TYPE *const va = v;
    OXS_FFT_REAL_TYPE *const vb = va + stride;
    OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
    OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
    for(i=0;i<bw;i+=2) {
      // w^0
      OXS_FFT_REAL_TYPE x0 = va[i];
      OXS_FFT_REAL_TYPE y0 = va[i+1];
      OXS_FFT_REAL_TYPE x1 = vb[i];
      OXS_FFT_REAL_TYPE y1 = vb[i+1];
      va[i]   = x0 + x1;      va[i+1] = y0 + y1;
      vb[i]   = x0 - x1;      vb[i+1] = y0 - y1;
      vc[i]   = x0 + y1;      vc[i+1] = y0 - x1;
      vd[i]   = x0 - y1;      vd[i+1] = y0 + x1;
    }
    i = rstride;
    while(i<stride) {
      for(OC_INDEX j=i;j-i<bw;j+=2) {
        OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
        OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
        OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
        OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7

        va[j]                 = x0 + x1;
        OXS_FFT_REAL_TYPE txa = x0 - x1;     // txa := R4

        OXS_FFT_REAL_TYPE mx1 = U[0];   // mx1 := R3

        va[j+1]               = y0 + y1;
        OXS_FFT_REAL_TYPE tya = y0 - y1;     // tya := R5

        OXS_FFT_REAL_TYPE mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
        mx1_tya *= tya;    // mx1_tya := R3

        OXS_FFT_REAL_TYPE my1 = U[1];    // my1 := R8

#define my1_tya tya
        my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
        my1_txa *= my1; // my1_txa := R4

        vb[j]     = mx1_txa - my1_tya;
        vb[j+1]   = my1_txa + mx1_tya;

#define txb mx1_txa
        txb = x0 + y1;  // txb := R2
#define txc x0
        txc -= y1;       // txc := R0
#define tyb mx1_tya
        tyb = y0 - x1;  // tyb := R3
#define tyc y0
        tyc += x1;       // tyc := R6

#define mx2 my1_txa
        mx2     = U[2]; //     mx2 := R4
#define mx2_tyb y1
        mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb x1
        mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
        my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
        my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
        my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
        Cx -= my2_tyb; // Cx := R1
        vc[j]   = Cx;

#define mx3 mx2
        mx3 = U[4];  // mx3 := R4
#define my3 my2
        my3 = U[5];  // my3 := R5

        OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
        OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
        OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
        OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0

        vc[j+1] = mx2_tyb + my2_txb;
        vd[j]   = mx3_txc - my3_tyc;
        vd[j+1] = my3_txc + mx3_tyc;

#undef mx1_tya
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
      }
      i+=rstride;
      U+=6;
    }


    for(i=0;i<64;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

      OC_INDEX j;

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
        OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j*16]        = aS0x + aS1x;
        scratch[j*16+1]      = aS0y + aS1y;
        scratch[j*16+2*4]    = aS0x - aS1x;
        scratch[j*16+2*4+1]  = aS0y - aS1y;

        scratch[j*16+2*8]    = aD0x + aD1y;
        scratch[j*16+2*8+1]  = aD0y - aD1x;
        scratch[j*16+2*12]   = aD0x - aD1y;
        scratch[j*16+2*12+1] = aD0y + aD1x;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
        OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
        OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j*16+2]    = bS0x + bS1x;
        scratch[j*16+2+1]    = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j*16+2*5]  = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*5+1]  = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

        scratch[j*16+2*9]  = tUb2x*alphax + tUb2y*alphay;
        scratch[j*16+2*9+1]  = tUb2y*alphax - tUb2x*alphay;
        scratch[j*16+2*13] = tUb3x*alphay + tUb3y*alphax;
        scratch[j*16+2*13+1] = tUb3y*alphay - tUb3x*alphax;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
        OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
        OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
        OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
        OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

        scratch[j*16+2*2]    = cS1x + cS0x;
        scratch[j*16+2*2+1]  = cS0y + cS1y;
        scratch[j*16+2*6]    = cS0y - cS1y;
        scratch[j*16+2*6+1]  = cS1x - cS0x;

        scratch[j*16+2*10]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*10+1] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14+1] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
        OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
        OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

        OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
        OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
        OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
        OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

        OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
        scratch[j*16+2*3]    = dS1x + dS0x;
        scratch[j*16+2*3+1]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

        scratch[j*16+2*7]    = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*7+1]  = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
        OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

        scratch[j*16+2*11]   = tUd2x*alphay + tUd2y*alphax;
        scratch[j*16+2*11+1] = tUd2y*alphay - tUd2x*alphax;
        scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j*16+2*15+1] = tUd3y*alphax - tUd3x*alphay;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      for(ja=0;ja<16;++ja) {
        OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
        w[ja] = wtmp;
        if(wtmp<bv0) {
          // Swap
          memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
        for(OC_INDEX jb=0;jb<bw;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb*16];
          OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
          OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
          OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
          OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx + BbDy;
          *(w[ja+2]+jb+1) = BaDy - BbDx;
          *(w[ja+3]+jb)   = BaDx - BbDy;
          *(w[ja+3]+jb+1) = BaDy + BbDx;
        }
      }
    }

    // Update for next block
    block += bw;
    bw = 2*OFS_ARRAY_BLOCKSIZE;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize64ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 64

  OC_INDEX bw = OFS_ARRAY_BLOCKSIZE + (arrcount % OFS_ARRAY_BLOCKSIZE);
  if(bw>OFS_ARRAY_MAXBLOCKSIZE) bw -= OFS_ARRAY_BLOCKSIZE;
  if(bw>arrcount) bw=arrcount;
  bw *= 2;

  OC_INDEX block=0;
  while(block<2*arrcount) {
    // Do power-of-4 blocks, with preorder traversal/tree walk
    const PreorderTraversalState* sptr = ptsRadix4;
    OXS_FFT_REAL_TYPE* const v = arr + block;

    // Stride is measured in units of OXS_FFT_REAL_TYPE's.
    OC_INDEX stride = sptr->stride;
    OXS_FFT_REAL_TYPE const *U = UForwardRadix4 + sptr->uoff;

    // Do top-level power-of-4 butterflies
    OXS_FFT_REAL_TYPE *const va = v;
    OXS_FFT_REAL_TYPE *const vb = va + stride;
    OXS_FFT_REAL_TYPE *const vc = va + 2*stride;
    OXS_FFT_REAL_TYPE *const vd = va + 3*stride;
    OC_INDEX i;
    for(i=0;i<bw;i+=2) {
      // w^0
      OXS_FFT_REAL_TYPE x0 = va[i];
      OXS_FFT_REAL_TYPE y0 = va[i+1];
      OXS_FFT_REAL_TYPE x1 = vb[i];
      OXS_FFT_REAL_TYPE y1 = vb[i+1];
      OXS_FFT_REAL_TYPE x2 = vc[i];
      OXS_FFT_REAL_TYPE x3 = vd[i];

      OXS_FFT_REAL_TYPE y2 = vc[i+1];

      OXS_FFT_REAL_TYPE sx1 = x0 + x2;
      OXS_FFT_REAL_TYPE dx1 = x0 - x2;

      OXS_FFT_REAL_TYPE sx2 = x1 + x3;
      OXS_FFT_REAL_TYPE dx2 = x1 - x3;

      OXS_FFT_REAL_TYPE y3 = vd[i+1];

      OXS_FFT_REAL_TYPE sy1 = y0 + y2;
      OXS_FFT_REAL_TYPE dy1 = y0 - y2;
      OXS_FFT_REAL_TYPE sy2 = y1 + y3;
      OXS_FFT_REAL_TYPE dy2 = y1 - y3;

      va[i]   = sx1 + sx2;        va[i+1] = sy1 + sy2;
      vb[i]   = sx1 - sx2;        vb[i+1] = sy1 - sy2;
      vc[i]   = dx1 - dy2;        vc[i+1] = dy1 + dx2;
      vd[i]   = dx1 + dy2;        vd[i+1] = dy1 - dx2;
    }
    i = rstride;
    while(i<stride) {
      for(OC_INDEX j=i;j-i<bw;j+=2) {
        OXS_FFT_REAL_TYPE x0 = va[j];    // x0 := R0
        OXS_FFT_REAL_TYPE y0 = va[j+1];  // y0 := R6
        OXS_FFT_REAL_TYPE x1 = vb[j];    // x1 := R1
        OXS_FFT_REAL_TYPE y1 = vb[j+1];  // y1 := R7
        OXS_FFT_REAL_TYPE x2 = vc[j];    // x2 := R2
        OXS_FFT_REAL_TYPE x3 = vd[j];    // x3 := R3

        OXS_FFT_REAL_TYPE sx1 = x0 + x2;           // sx1 := R4
        OXS_FFT_REAL_TYPE sx2 = x1 + x3;           // sx2 := R5
#define dx1 x0
        dx1 -= x2;    // dx1 := R0
#define dx2 x1
        dx2 -= x3;    // dx2 := R1

#define y2 x2
        y2 = vc[j+1]; // y2 := R2
#define y3 x3
        y3 = vd[j+1]; // y3 := R3

        va[j]              = sx1 + sx2;
#define txa sx1
        txa -= sx2;     // txa := R4

#define sy1 sx2
        sy1 = y0 + y2;  // sy1 := R5
#define dy1 y0
        dy1 -= y2;      // dy1 := R6
        OXS_FFT_REAL_TYPE         sy2 = y1 + y3;      // sy2 := R8
#define dy2 y1
        dy2 -= y3;      // dy2 := R7

#define mx1 y3
        mx1 = U[0];   // mx1 := R3

        va[j+1]              = sy1 + sy2;
#define tya sy1
        tya -= sy2;     // tya := R5

#define mx1_txa y2
        mx1_txa = mx1*txa; // mx1_txa := R2
#define mx1_tya mx1
        mx1_tya *= tya;    // mx1_tya := R3

#define my1 sy2
        my1 = U[1];    // my1 := R8

#define my1_tya tya
        my1_tya *= my1; // my1_tya := R5
#define my1_txa txa
        my1_txa *= my1; // my1_txa := R4

        vb[j]     = mx1_txa + my1_tya;
        vb[j+1]   = mx1_tya - my1_txa;

#define txb mx1_txa
        txb = dx1 - dy2;  // txb := R2
#define txc dx1
        txc += dy2;       // txc := R0
#define tyb mx1_tya
        tyb = dy1 + dx2;  // tyb := R3
#define tyc dy1
        tyc -= dx2;       // tyc := R6

#define mx2 my1_txa
        mx2     = U[2]; //     mx2 := R4
#define mx2_tyb dy2
        mx2_tyb = tyb*mx2;  // mx2_tvb := R7
#define mx2_txb dx2
        mx2_txb = txb*mx2;  // mx2_txb := R1
#define my2 my1_tya
        my2     = U[3]; //     my2 := R5
#define my2_tyb tyb
        my2_tyb *= my2;     // my2_tyb := R3
#define my2_txb txb
        my2_txb *= my2;     // my2_txb := R2

#define Cx mx2_txb
        Cx += my2_tyb; // Cx := R1
        vc[j]   = Cx;

#define mx3 mx2
        mx3 = U[4];  // mx3 := R4
#define my3 my2
        my3 = U[5];  // my3 := R5

        OXS_FFT_REAL_TYPE mx3_txc = txc * mx3;    // mx3_txc := R3
        OXS_FFT_REAL_TYPE mx3_tyc = tyc * mx3;    // mx3_tyc := R4
        OXS_FFT_REAL_TYPE my3_tyc = tyc * my3;    // my3_tyc := R6
        OXS_FFT_REAL_TYPE my3_txc = txc * my3;    // my3_txc := R0
        OXS_FFT_REAL_TYPE Cy = mx2_tyb - my2_txb; // Cy := R2
        OXS_FFT_REAL_TYPE Dx = mx3_txc + my3_tyc;
        OXS_FFT_REAL_TYPE Dy = mx3_tyc - my3_txc;

        vc[j+1] = Cy;
        vd[j]   = Dx;
        vd[j+1] = Dy;
#undef dx1
#undef dx2
#undef y2
#undef y3
#undef txa
#undef sy1
#undef dy1
#undef dy2
#undef mx1
#undef tya
#undef mx1_txa
#undef mx1_tya
#undef my1
#undef my1_tya
#undef my1_txa
#undef txb
#undef txc
#undef tyb
#undef tyc
#undef mx2
#undef mx2_tyb
#undef mx2_txb
#undef my2
#undef my2_tyb
#undef my2_txb
#undef Cx
#undef mx3
#undef my3
      }
      i+=rstride;
      U+=6;
    }


    for(i=0;i<64;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

      OC_INDEX j;

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
        OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j*16]        = aS0x + aS1x;
        scratch[j*16+1]      = aS0y + aS1y;
        scratch[j*16+2*4]    = aS0x - aS1x;
        scratch[j*16+2*4+1]  = aS0y - aS1y;

        scratch[j*16+2*8]    = aD0x - aD1y;
        scratch[j*16+2*8+1]  = aD0y + aD1x;
        scratch[j*16+2*12]   = aD0x + aD1y;
        scratch[j*16+2*12+1] = aD0y - aD1x;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
        OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
        OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j*16+2]      = bS0x + bS1x;
        scratch[j*16+2+1]    = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j*16+2*5]    = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*5+1]  = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

        scratch[j*16+2*9]    = tUb2x*alphax - tUb2y*alphay;
        scratch[j*16+2*9+1]  = tUb2y*alphax + tUb2x*alphay;
        scratch[j*16+2*13]   = tUb3x*alphay - tUb3y*alphax;
        scratch[j*16+2*13+1] = tUb3y*alphay + tUb3x*alphax;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
        OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
        OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
        OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
        OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

        scratch[j*16+2*2]    = cS0x + cS1x;
        scratch[j*16+2*2+1]  = cS0y + cS1y;
        scratch[j*16+2*6]    = cS1y - cS0y;
        scratch[j*16+2*6+1]  = cS0x - cS1x;

        scratch[j*16+2*10]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*10+1] = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14+1] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
        OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
        OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

        OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
        OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
        OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
        OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

        OXS_FFT_REAL_TYPE tUd1x   = dS0x - dS1x;
        scratch[j*16+2*3]    = dS0x + dS1x;
        scratch[j*16+2*3+1]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y   = dS1y - dS0y;

        scratch[j*16+2*7]    = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*7+1]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD0x + dD1y;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
        OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

        scratch[j*16+2*11]   = tUd2x*alphay - tUd2y*alphax;
        scratch[j*16+2*11+1] = tUd2y*alphay + tUd2x*alphax;
        scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j*16+2*15+1] = tUd3x*alphay - tUd3y*alphax;
      }

      // Bit reversal.  This "...ZP" variant skips reversals
      // that affect what ends up in the upper half of the
      // results, since those are to be thrown away.
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i<RTNFFTSIZE/2) {
        for(ja=0;ja<16;++ja) {
          OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
          w[ja] = wtmp;
          if(wtmp<bv0) {
            // Swap
            memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
          }
        }
      } else {
        for(ja=0;ja<16;ja+=2) w[ja] = v+bitreverse[i+ja];
      }

      // Do 4 lower (bottom/final) -level dragonflies
      // This "...ZP" variant assumes that the last half of the
      // results are to be thrown away, or equivalently, that
      // the pre-reversal odd (ja%2==1) results are not used.
      for(ja=0;ja<16;ja+=4) {
        OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
        for(OC_INDEX jb=0;jb<bw;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb*16];
          OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
          OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
          OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
          OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+2]+jb)   = BaDx - BbDy;
          *(w[ja+2]+jb+1) = BaDy + BbDx;
        }
      }
    }

    // Update for next block
    block += bw;
    bw = 2*OFS_ARRAY_BLOCKSIZE;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize32
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 32
  OC_INDEX i;
  OXS_FFT_REAL_TYPE* v; // Working vector.

  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    // Top-level 32-block pass
    v = arr + 2*block;  // Working vector

    // Fill out zero-padding
    for(i=csize_base;i<RTNFFTSIZE;++i) {
      OXS_FFT_REAL_TYPE* w = v + i*rstride;
      for(OC_INDEX j=0;j<bw;++j) w[j] = 0.0;
    }

    { // i=0
      for(OC_INDEX j2=0 ; j2<bw ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = v[j2];
        OXS_FFT_REAL_TYPE ay0 = v[j2+1];
        OXS_FFT_REAL_TYPE cx0 = v[j2+rstride*8];
        OXS_FFT_REAL_TYPE cy0 = v[j2+rstride*8+1];
        OXS_FFT_REAL_TYPE ax1 = v[j2+rstride*16];
        OXS_FFT_REAL_TYPE ay1 = v[j2+rstride*16+1];
        OXS_FFT_REAL_TYPE cx1 = v[j2+rstride*24];
        OXS_FFT_REAL_TYPE cy1 = v[j2+rstride*24+1];
        v[j2]              = ax0 + ax1;
        v[j2+1]            = ay0 + ay1;
        v[j2+rstride*16]   = ax0 - ax1;
        v[j2+rstride*16+1] = ay0 - ay1;
        v[j2+rstride*8]    = cx0 + cx1;
        v[j2+rstride*8+1]  = cy0 + cy1;
        v[j2+rstride*24]   = cy0 - cy1;
        v[j2+rstride*24+1] = cx1 - cx0;
      }
    }

    OXS_FFT_REAL_TYPE const * const U = UForwardRadix4;
    // NB: U points to the tail of the UForwardRadix4 array,
    // which is a segment holding the 32nd roots of unity.  That
    // segment doesn't contain the (1,0) root, so the access below
    // is to U - 2.
    for(i=1;i<8;++i) {
      OXS_FFT_REAL_TYPE* const va = v + rstride*i;
      const OXS_FFT_REAL_TYPE amx = U[2*i-2];
      const OXS_FFT_REAL_TYPE amy = U[2*i-1];
      for(OC_INDEX j2=0 ; j2<bw ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = va[j2];
        OXS_FFT_REAL_TYPE ay0 = va[j2+1];
        OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
        OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];

        OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
        va[j2]                  = ax0 + ax1;
        va[j2+1]                = ay0 + ay1;
        OXS_FFT_REAL_TYPE adify = ay0 - ay1;
        va[j2+rstride*16]   = amx*adifx - amy*adify;
        va[j2+rstride*16+1] = amx*adify + amy*adifx;

        OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
        OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
        OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
        OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];

        OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
        va[j2+rstride*8]   = cx0 + cx1;
        va[j2+rstride*8+1] = cy0 + cy1;
        OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

        va[j2+rstride*24]   = amx*cdify + amy*cdifx;
        va[j2+rstride*24+1] = amy*cdify - amx*cdifx;
      }
    }

    for(i=0;i<32;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

      OC_INDEX j;

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
        OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j*16]        = aS0x + aS1x;
        scratch[j*16+1]      = aS0y + aS1y;
        scratch[j*16+2*4]    = aS0x - aS1x;
        scratch[j*16+2*4+1]  = aS0y - aS1y;

        scratch[j*16+2*8]    = aD0x + aD1y;
        scratch[j*16+2*8+1]  = aD0y - aD1x;
        scratch[j*16+2*12]   = aD0x - aD1y;
        scratch[j*16+2*12+1] = aD0y + aD1x;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
        OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
        OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j*16+2]      = bS0x + bS1x;
        scratch[j*16+2+1]    = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j*16+2*5]    = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*5+1]  = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

        scratch[j*16+2*9]    = tUb2x*alphax + tUb2y*alphay;
        scratch[j*16+2*9+1]  = tUb2y*alphax - tUb2x*alphay;
        scratch[j*16+2*13]   = tUb3x*alphay + tUb3y*alphax;
        scratch[j*16+2*13+1] = tUb3y*alphay - tUb3x*alphax;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
        OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
        OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
        OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
        OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

        scratch[j*16+2*2]    = cS1x + cS0x;
        scratch[j*16+2*2+1]  = cS0y + cS1y;
        scratch[j*16+2*6]    = cS0y - cS1y;
        scratch[j*16+2*6+1]  = cS1x - cS0x;

        scratch[j*16+2*10]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*10+1] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14+1] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
        OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
        OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

        OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
        OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
        OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
        OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

        OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
        scratch[j*16+2*3]    = dS1x + dS0x;
        scratch[j*16+2*3+1]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

        scratch[j*16+2*7]    = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*7+1]  = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
        OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

        scratch[j*16+2*11]   = tUd2x*alphay + tUd2y*alphax;
        scratch[j*16+2*11+1] = tUd2y*alphay - tUd2x*alphax;
        scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j*16+2*15+1] = tUd3y*alphax - tUd3x*alphay;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i==0) {
        // Lower pass; no swaps, two pair interchanges
        for(ja=0;ja<16;++ja) w[ja] = v + bitreverse[i+ja];
      } else {
        for(ja=0;ja<16;ja+=2) {
          // Upper pass; all even offsets swap.
          //  Odd offsets don't swap, but might interchange
          OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
          memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
          w[ja]   = wtmp;
          w[ja+1] =  v + bitreverse[i+ja+1];
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
        for(OC_INDEX jb=0;jb<bw;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb*16];
          OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
          OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
          OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
          OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx + BbDy;
          *(w[ja+2]+jb+1) = BaDy - BbDx;
          *(w[ja+3]+jb)   = BaDx - BbDy;
          *(w[ja+3]+jb+1) = BaDy + BbDx;
        }
      }

    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize32
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 32
  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    // Top-level 32-block pass
    OXS_FFT_REAL_TYPE* const v = arr + 2*block;  // Working vector
    { // i=0
      for(OC_INDEX j2=0 ; j2<bw ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = v[j2];
        OXS_FFT_REAL_TYPE ay0 = v[j2+1];
        OXS_FFT_REAL_TYPE cx0 = v[j2+rstride*8];
        OXS_FFT_REAL_TYPE cy0 = v[j2+rstride*8+1];
        OXS_FFT_REAL_TYPE ax1 = v[j2+rstride*16];
        OXS_FFT_REAL_TYPE ay1 = v[j2+rstride*16+1];
        OXS_FFT_REAL_TYPE cx1 = v[j2+rstride*24];
        OXS_FFT_REAL_TYPE cy1 = v[j2+rstride*24+1];
        v[j2]              = ax0 + ax1;
        v[j2+1]            = ay0 + ay1;
        v[j2+rstride*16]   = ax0 - ax1;
        v[j2+rstride*16+1] = ay0 - ay1;
        v[j2+rstride*8]    = cx0 + cx1;
        v[j2+rstride*8+1]  = cy0 + cy1;
        v[j2+rstride*24]   = cy1 - cy0;
        v[j2+rstride*24+1] = cx0 - cx1;
      }
    }

    OXS_FFT_REAL_TYPE const *U = UForwardRadix4;
    // NB: U points to the tail of the UForwardRadix4 array,
    // which is a segment holding the 32nd roots of unity.  That
    // segment doesn't contain the (1,0) root, so the access below
    // is to U - 2.
    OC_INDEX i;
    for(i=1;i<8;++i) {
      OXS_FFT_REAL_TYPE* const va = v + rstride*i;
      const OXS_FFT_REAL_TYPE amx = U[2*i-2];
      const OXS_FFT_REAL_TYPE amy = U[2*i-1];
      for(OC_INDEX j2=0 ; j2<bw ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = va[j2];
        OXS_FFT_REAL_TYPE ay0 = va[j2+1];
        OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
        OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];

        OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
        va[j2]        = ax0 + ax1;
        va[j2+1]      = ay0 + ay1;
        OXS_FFT_REAL_TYPE adify = ay0 - ay1;
        va[j2+rstride*16]   = amx*adifx + amy*adify;
        va[j2+rstride*16+1] = amx*adify - amy*adifx;

        OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
        OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
        OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
        OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];

        OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
        va[j2+rstride*8]        = cx0 + cx1;
        va[j2+rstride*8+1]      = cy0 + cy1;
        OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

        va[j2+rstride*24]   = amy*cdifx - amx*cdify;
        va[j2+rstride*24+1] = amy*cdify + amx*cdifx;
      }
    }

    for(i=0;i<32;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

      OC_INDEX j;

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
        OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j*16]        = aS0x + aS1x;
        scratch[j*16+1]      = aS0y + aS1y;
        scratch[j*16+2*4]    = aS0x - aS1x;
        scratch[j*16+2*4+1]  = aS0y - aS1y;

        scratch[j*16+2*8]    = aD0x - aD1y;
        scratch[j*16+2*8+1]  = aD0y + aD1x;
        scratch[j*16+2*12]   = aD0x + aD1y;
        scratch[j*16+2*12+1] = aD0y - aD1x;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
        OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
        OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j*16+2]         = bS0x + bS1x;
        scratch[j*16+2+1]       = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j*16+2*5]    = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*5+1]  = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

        scratch[j*16+2*9]    = tUb2x*alphax - tUb2y*alphay;
        scratch[j*16+2*9+1]  = tUb2y*alphax + tUb2x*alphay;
        scratch[j*16+2*13]   = tUb3x*alphay - tUb3y*alphax;
        scratch[j*16+2*13+1] = tUb3y*alphay + tUb3x*alphax;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
        OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
        OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
        OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
        OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

        scratch[j*16+2*2]    = cS0x + cS1x;
        scratch[j*16+2*2+1]  = cS0y + cS1y;
        scratch[j*16+2*6]    = cS1y - cS0y;
        scratch[j*16+2*6+1]  = cS0x - cS1x;

        scratch[j*16+2*10]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*10+1] = (tUc2x + tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14+1] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
        OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
        OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

        OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
        OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
        OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
        OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

        OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
        scratch[j*16+2*3]    = dS0x + dS1x;
        scratch[j*16+2*3+1]  = dS1y + dS0y;
        OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

        scratch[j*16+2*7]    = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*7+1]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
        OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

        scratch[j*16+2*11]   = tUd2x*alphay - tUd2y*alphax;
        scratch[j*16+2*11+1] = tUd2y*alphay + tUd2x*alphax;
        scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j*16+2*15+1] = tUd3x*alphay - tUd3y*alphax;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i==0) {
        // Lower pass; no swaps, two pair interchanges
        for(ja=0;ja<16;++ja) w[ja] = v + bitreverse[i+ja];
      } else {
        for(ja=0;ja<16;ja+=2) {
          // Upper pass; all even offsets swap.
          //  Odd offsets don't swap, but might interchange
          OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
          memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
          w[ja]   = wtmp;
          w[ja+1] =  v + bitreverse[i+ja+1];
        }
      }

      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
        for(OC_INDEX jb=0;jb<bw;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb*16];
          OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
          OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
          OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
          OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          *(w[ja]+jb)     = BaSx + BbSx;
          *(w[ja]+jb+1)   = BaSy + BbSy;
          *(w[ja+1]+jb)   = BaSx - BbSx;
          *(w[ja+1]+jb+1) = BaSy - BbSy;

          *(w[ja+2]+jb)   = BaDx - BbDy;
          *(w[ja+2]+jb+1) = BaDy + BbDx;
          *(w[ja+3]+jb)   = BaDx + BbDy;
          *(w[ja+3]+jb+1) = BaDy - BbDx;
        }
      }
    }
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize32ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 32
  OC_INDEX i;
  OXS_FFT_REAL_TYPE* v; // Working vector.

  OC_INDEX bw = OFS_ARRAY_BLOCKSIZE + (arrcount % OFS_ARRAY_BLOCKSIZE);
  if(bw>OFS_ARRAY_MAXBLOCKSIZE) bw -= OFS_ARRAY_BLOCKSIZE;
  if(bw>arrcount) bw=arrcount;
  bw *= 2;

  OC_INDEX block=0;
  while(block<2*arrcount) {
    // Top-level 32-block pass
    v = arr + block;  // Working vector

    // Fill out zero-padding
    for(i=csize_base;i<RTNFFTSIZE/2;++i) {
      OXS_FFT_REAL_TYPE* w = v + i*rstride;
      for(OC_INDEX j=0;j<bw;++j) w[j] = 0.0;
    }

    { // i=0
      for(OC_INDEX j2=0 ; j2<bw ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = v[j2];
        OXS_FFT_REAL_TYPE ay0 = v[j2+1];
        OXS_FFT_REAL_TYPE cx0 = v[j2+rstride*8];
        OXS_FFT_REAL_TYPE cy0 = v[j2+rstride*8+1];
        v[j2]              = ax0;
        v[j2+1]            = ay0;
        v[j2+rstride*16]   = ax0;
        v[j2+rstride*16+1] = ay0;
        v[j2+rstride*8]    = cx0;
        v[j2+rstride*8+1]  = cy0;
        v[j2+rstride*24]   = cy0;
        v[j2+rstride*24+1] = -cx0;
      }
    }

    OXS_FFT_REAL_TYPE const * const U = UForwardRadix4;
    // NB: U points to the tail of the UForwardRadix4 array,
    // which is a segment holding the 32nd roots of unity.  That
    // segment doesn't contain the (1,0) root, so the access below
    // is to U - 2.
    for(i=1;i<8;++i) {
      OXS_FFT_REAL_TYPE* const va = v + rstride*i;
      const OXS_FFT_REAL_TYPE amx = U[2*i-2];
      const OXS_FFT_REAL_TYPE amy = U[2*i-1];
      for(OC_INDEX j2=0 ; j2<bw ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = va[j2];
        OXS_FFT_REAL_TYPE ay0 = va[j2+1];
        OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
        OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
        va[j2+rstride*16]   = amx*ax0 - amy*ay0;
        va[j2+rstride*16+1] = amx*ay0 + amy*ax0;
        va[j2+rstride*24]   = amx*cy0 + amy*cx0;
        va[j2+rstride*24+1] = amy*cy0 - amx*cx0;
      }
    }

    for(i=0;i<32;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

      OC_INDEX j;

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
        OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j*16]        = aS0x + aS1x;
        scratch[j*16+1]      = aS0y + aS1y;
        scratch[j*16+2*4]    = aS0x - aS1x;
        scratch[j*16+2*4+1]  = aS0y - aS1y;

        scratch[j*16+2*8]    = aD0x + aD1y;
        scratch[j*16+2*8+1]  = aD0y - aD1x;
        scratch[j*16+2*12]   = aD0x - aD1y;
        scratch[j*16+2*12+1] = aD0y + aD1x;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
        OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
        OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j*16+2]      = bS0x + bS1x;
        scratch[j*16+2+1]    = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j*16+2*5]    = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*5+1]  = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

        scratch[j*16+2*9]    = tUb2x*alphax + tUb2y*alphay;
        scratch[j*16+2*9+1]  = tUb2y*alphax - tUb2x*alphay;
        scratch[j*16+2*13]   = tUb3x*alphay + tUb3y*alphax;
        scratch[j*16+2*13+1] = tUb3y*alphay - tUb3x*alphax;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
        OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
        OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
        OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
        OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

        scratch[j*16+2*2]    = cS1x + cS0x;
        scratch[j*16+2*2+1]  = cS0y + cS1y;
        scratch[j*16+2*6]    = cS0y - cS1y;
        scratch[j*16+2*6+1]  = cS1x - cS0x;

        scratch[j*16+2*10]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*10+1] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14+1] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
        OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
        OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

        OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
        OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
        OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
        OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

        OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
        scratch[j*16+2*3]    = dS1x + dS0x;
        scratch[j*16+2*3+1]  = dS0y + dS1y;
        OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

        scratch[j*16+2*7]    = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*7+1]  = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
        OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

        scratch[j*16+2*11]   = tUd2x*alphay + tUd2y*alphax;
        scratch[j*16+2*11+1] = tUd2y*alphay - tUd2x*alphax;
        scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j*16+2*15+1] = tUd3y*alphax - tUd3x*alphay;
      }

      // Bit reversal
      OC_INDEX ja;
      OXS_FFT_REAL_TYPE* w[16];
      if(i==0) {
        // Lower pass; no swaps, two pair interchanges
        for(ja=0;ja<16;++ja) w[ja] = v + bitreverse[i+ja];
      } else {
        for(ja=0;ja<16;ja+=2) {
          // Upper pass; all even offsets swap.
          //  Odd offsets don't swap, but might interchange
          OXS_FFT_REAL_TYPE* const wtmp = v + bitreverse[i+ja];
          memcpy(bv0+rstride*ja,wtmp,bw*sizeof(OXS_FFT_REAL_TYPE));
          w[ja]   = wtmp;
          w[ja+1] =  v + bitreverse[i+ja+1];
        }
      }


      // Do 4 lower (bottom/final) -level dragonflies
      for(ja=0;ja<16;ja+=4) {
        OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
        OXS_FFT_REAL_TYPE * const wa = w[ja];
        OXS_FFT_REAL_TYPE * const wb = w[ja+1];
        OXS_FFT_REAL_TYPE * const wc = w[ja+2];
        OXS_FFT_REAL_TYPE * const wd = w[ja+3];
        for(OC_INDEX jb=0;jb<bw;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb*16];
          OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
          OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
          OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
          OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          wa[jb]   = BaSx + BbSx;
          wa[jb+1] = BaSy + BbSy;
          wb[jb]   = BaSx - BbSx;
          wb[jb+1] = BaSy - BbSy;

          wc[jb]   = BaDx + BbDy;
          wc[jb+1] = BaDy - BbDx;
          wd[jb]   = BaDx - BbDy;
          wd[jb+1] = BaDy + BbDx;
        }
      }

    }

    // Update for next block
    block += bw;
    bw = 2*OFS_ARRAY_BLOCKSIZE;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize32ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 32
  OC_INDEX bw = OFS_ARRAY_BLOCKSIZE + (arrcount % OFS_ARRAY_BLOCKSIZE);
  if(bw>OFS_ARRAY_MAXBLOCKSIZE) bw -= OFS_ARRAY_BLOCKSIZE;
  if(bw>arrcount) bw=arrcount;
  bw *= 2;

  OC_INDEX block=0;
  while(block<2*arrcount) {
    // Top-level 32-block pass
    OXS_FFT_REAL_TYPE* const v = arr + block;  // Working vector
    { // i=0
      for(OC_INDEX j2=0 ; j2<bw ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = v[j2];
        OXS_FFT_REAL_TYPE ay0 = v[j2+1];
        OXS_FFT_REAL_TYPE cx0 = v[j2+rstride*8];
        OXS_FFT_REAL_TYPE cy0 = v[j2+rstride*8+1];
        OXS_FFT_REAL_TYPE ax1 = v[j2+rstride*16];
        OXS_FFT_REAL_TYPE ay1 = v[j2+rstride*16+1];
        OXS_FFT_REAL_TYPE cx1 = v[j2+rstride*24];
        OXS_FFT_REAL_TYPE cy1 = v[j2+rstride*24+1];
        v[j2]              = ax0 + ax1;
        v[j2+1]            = ay0 + ay1;
        v[j2+rstride*16]   = ax0 - ax1;
        v[j2+rstride*16+1] = ay0 - ay1;
        v[j2+rstride*8]    = cx0 + cx1;
        v[j2+rstride*8+1]  = cy0 + cy1;
        v[j2+rstride*24]   = cy1 - cy0;
        v[j2+rstride*24+1] = cx0 - cx1;
      }
    }

    OXS_FFT_REAL_TYPE const *U = UForwardRadix4;
    // NB: U points to the tail of the UForwardRadix4 array,
    // which is a segment holding the 32nd roots of unity.  That
    // segment doesn't contain the (1,0) root, so the access below
    // is to U - 2.
    OC_INDEX i;
    for(i=1;i<8;++i) {
      OXS_FFT_REAL_TYPE* const va = v + rstride*i;
      const OXS_FFT_REAL_TYPE amx = U[2*i-2];
      const OXS_FFT_REAL_TYPE amy = U[2*i-1];
      for(OC_INDEX j2=0 ; j2<bw ; j2+=2) {
        OXS_FFT_REAL_TYPE ax0 = va[j2];
        OXS_FFT_REAL_TYPE ay0 = va[j2+1];
        OXS_FFT_REAL_TYPE ax1 = va[j2+rstride*16];
        OXS_FFT_REAL_TYPE ay1 = va[j2+rstride*16+1];

        OXS_FFT_REAL_TYPE adifx = ax0 - ax1;
        va[j2]        = ax0 + ax1;
        va[j2+1]      = ay0 + ay1;
        OXS_FFT_REAL_TYPE adify = ay0 - ay1;
        va[j2+rstride*16]   = amx*adifx + amy*adify;
        va[j2+rstride*16+1] = amx*adify - amy*adifx;

        OXS_FFT_REAL_TYPE cx0 = va[j2+rstride*8];
        OXS_FFT_REAL_TYPE cy0 = va[j2+rstride*8+1];
        OXS_FFT_REAL_TYPE cx1 = va[j2+rstride*24];
        OXS_FFT_REAL_TYPE cy1 = va[j2+rstride*24+1];

        OXS_FFT_REAL_TYPE cdifx = cx0 - cx1;
        va[j2+rstride*8]        = cx0 + cx1;
        va[j2+rstride*8+1]      = cy0 + cy1;
        OXS_FFT_REAL_TYPE cdify = cy0 - cy1;

        va[j2+rstride*24]   = amy*cdifx - amx*cdify;
        va[j2+rstride*24+1] = amy*cdify + amx*cdifx;
      }
    }

    for(i=0;i<32;i+=16) {
      // Bottom level 16-passes, with built-in bit-flip shuffling
      const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
      const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

      OXS_FFT_REAL_TYPE* const bv0 = v + rstride*i;

      OC_INDEX j;

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE a0x = bv0[j];
        OXS_FFT_REAL_TYPE a0y = bv0[j+1];
        OXS_FFT_REAL_TYPE a2x = bv0[j  +rstride*8];
        OXS_FFT_REAL_TYPE a2y = bv0[j+1+rstride*8];

        OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
        OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
        OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
        OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

        OXS_FFT_REAL_TYPE a1x = bv0[j  +rstride*4];
        OXS_FFT_REAL_TYPE a1y = bv0[j+1+rstride*4];
        OXS_FFT_REAL_TYPE a3x = bv0[j  +rstride*12];
        OXS_FFT_REAL_TYPE a3y = bv0[j+1+rstride*12];

        OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
        OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
        OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
        OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

        scratch[j*16]        = aS0x + aS1x;
        scratch[j*16+1]      = aS0y + aS1y;
        scratch[j*16+2*4]    = aS0x - aS1x;
        scratch[j*16+2*4+1]  = aS0y - aS1y;

        scratch[j*16+2*8]    = aD0x - aD1y;
        scratch[j*16+2*8+1]  = aD0y + aD1x;
        scratch[j*16+2*12]   = aD0x + aD1y;
        scratch[j*16+2*12+1] = aD0y - aD1x;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE b0x = bv0[j  +rstride];
        OXS_FFT_REAL_TYPE b0y = bv0[j+1+rstride];
        OXS_FFT_REAL_TYPE b2x = bv0[j  +rstride*9];
        OXS_FFT_REAL_TYPE b2y = bv0[j+1+rstride*9];

        OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
        OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
        OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
        OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

        OXS_FFT_REAL_TYPE b1x = bv0[j  +rstride*5];
        OXS_FFT_REAL_TYPE b1y = bv0[j+1+rstride*5];
        OXS_FFT_REAL_TYPE b3x = bv0[j  +rstride*13];
        OXS_FFT_REAL_TYPE b3y = bv0[j+1+rstride*13];

        OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
        OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
        OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
        OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

        OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
        scratch[j*16+2]      = bS0x + bS1x;
        scratch[j*16+2+1]    = bS0y + bS1y;
        OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

        scratch[j*16+2*5]    = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*5+1]  = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
        OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
        OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
        OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

        scratch[j*16+2*9]    = tUb2x*alphax - tUb2y*alphay;
        scratch[j*16+2*9+1]  = tUb2y*alphax + tUb2x*alphay;
        scratch[j*16+2*13]   = tUb3x*alphay - tUb3y*alphax;
        scratch[j*16+2*13+1] = tUb3y*alphay + tUb3x*alphax;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE c0x = bv0[j  +rstride*2];
        OXS_FFT_REAL_TYPE c0y = bv0[j+1+rstride*2];
        OXS_FFT_REAL_TYPE c2x = bv0[j  +rstride*10];
        OXS_FFT_REAL_TYPE c2y = bv0[j+1+rstride*10];

        OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
        OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
        OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
        OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

        OXS_FFT_REAL_TYPE c1x = bv0[j  +rstride*6];
        OXS_FFT_REAL_TYPE c1y = bv0[j+1+rstride*6];
        OXS_FFT_REAL_TYPE c3x = bv0[j  +rstride*14];
        OXS_FFT_REAL_TYPE c3y = bv0[j+1+rstride*14];

        OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
        OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
        OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
        OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

        OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
        OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
        OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
        OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

        scratch[j*16+2*2]    = cS0x + cS1x;
        scratch[j*16+2*2+1]  = cS0y + cS1y;
        scratch[j*16+2*6]    = cS1y - cS0y;
        scratch[j*16+2*6+1]  = cS0x - cS1x;

        scratch[j*16+2*10]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*10+1] = (tUc2x + tUc2y)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*14+1] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      }

      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE d0x = bv0[j  +rstride*3];
        OXS_FFT_REAL_TYPE d0y = bv0[j+1+rstride*3];
        OXS_FFT_REAL_TYPE d2x = bv0[j  +rstride*11];
        OXS_FFT_REAL_TYPE d2y = bv0[j+1+rstride*11];

        OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
        OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
        OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
        OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

        OXS_FFT_REAL_TYPE d1x = bv0[j  +rstride*7];
        OXS_FFT_REAL_TYPE d1y = bv0[j+1+rstride*7];
        OXS_FFT_REAL_TYPE d3x = bv0[j  +rstride*15];
        OXS_FFT_REAL_TYPE d3y = bv0[j+1+rstride*15];

        OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
        OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
        OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
        OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

        OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
        scratch[j*16+2*3]    = dS0x + dS1x;
        scratch[j*16+2*3+1]  = dS1y + dS0y;
        OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

        scratch[j*16+2*7]    = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
        scratch[j*16+2*7+1]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

        OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
        OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
        OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
        OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

        scratch[j*16+2*11]   = tUd2x*alphay - tUd2y*alphax;
        scratch[j*16+2*11+1] = tUd2y*alphay + tUd2x*alphax;
        scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
        scratch[j*16+2*15+1] = tUd3x*alphay - tUd3y*alphax;
      }

      // Do 4 lower (bottom/final) -level dragonflies, with built-in bit
      // reversal.  This "...ZP" variant skips reversals and storage
      // that affect what ends up in the upper half of the results,
      // since those are to be thrown away.  (Or equivalently, that the
      // pre-reversal odd (ja%2==1) results are not used.)
      for(OC_INDEX ja=0;ja<16;ja+=4) {
        OXS_FFT_REAL_TYPE const * const sv = scratch + 2*ja;
        OXS_FFT_REAL_TYPE * const w0 = v + bitreverse[i+ja];
        OXS_FFT_REAL_TYPE * const w2 = v + bitreverse[i+ja+2];
        for(OC_INDEX jb=0;jb<bw;jb+=2) {
          OXS_FFT_REAL_TYPE Uax = sv[jb*16];
          OXS_FFT_REAL_TYPE Uay = sv[jb*16+1];
          OXS_FFT_REAL_TYPE Ucx = sv[jb*16+2*2];
          OXS_FFT_REAL_TYPE Ucy = sv[jb*16+2*2+1];

          OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
          OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
          OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
          OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

          OXS_FFT_REAL_TYPE Ubx = sv[jb*16+2];
          OXS_FFT_REAL_TYPE Uby = sv[jb*16+2+1];
          OXS_FFT_REAL_TYPE Udx = sv[jb*16+2*3];
          OXS_FFT_REAL_TYPE Udy = sv[jb*16+2*3+1];

          OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
          OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
          OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
          OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

          w0[jb]   = BaSx + BbSx;
          w0[jb+1] = BaSy + BbSy;
          w2[jb]   = BaDx - BbDy;
          w2[jb+1] = BaDy + BbDx;
        }
      }

    }

    // Update for next block
    block += bw;
    bw = 2*OFS_ARRAY_BLOCKSIZE;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize16
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 16
  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    OXS_FFT_REAL_TYPE* const v = arr + 2*block;  // Working vector

    // Fill out zero-padding
    OC_INDEX i,j;
    for(i=csize_base;i<RTNFFTSIZE;++i) {
      OXS_FFT_REAL_TYPE* w = v + i*rstride;
      for(j=0;j<bw;++j) w[j] = 0.0;
    }

    // Top level 16-pass
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE a0x = v[j];
      OXS_FFT_REAL_TYPE a0y = v[j+1];
      OXS_FFT_REAL_TYPE a2x = v[j  +rstride*8];
      OXS_FFT_REAL_TYPE a2y = v[j+1+rstride*8];

      OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
      OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
      OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
      OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

      OXS_FFT_REAL_TYPE a1x = v[j  +rstride*4];
      OXS_FFT_REAL_TYPE a1y = v[j+1+rstride*4];
      OXS_FFT_REAL_TYPE a3x = v[j  +rstride*12];
      OXS_FFT_REAL_TYPE a3y = v[j+1+rstride*12];

      OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
      OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
      OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
      OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

      scratch[j*16]        = aS0x + aS1x;
      scratch[j*16+1]      = aS0y + aS1y;
      scratch[j*16+2*4]    = aS0x - aS1x;
      scratch[j*16+2*4+1]  = aS0y - aS1y;

      scratch[j*16+2*8]    = aD0x + aD1y;
      scratch[j*16+2*8+1]  = aD0y - aD1x;
      scratch[j*16+2*12]   = aD0x - aD1y;
      scratch[j*16+2*12+1] = aD0y + aD1x;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE b0x = v[j  +rstride];
      OXS_FFT_REAL_TYPE b0y = v[j+1+rstride];
      OXS_FFT_REAL_TYPE b2x = v[j  +rstride*9];
      OXS_FFT_REAL_TYPE b2y = v[j+1+rstride*9];

      OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
      OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
      OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
      OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

      OXS_FFT_REAL_TYPE b1x = v[j  +rstride*5];
      OXS_FFT_REAL_TYPE b1y = v[j+1+rstride*5];
      OXS_FFT_REAL_TYPE b3x = v[j  +rstride*13];
      OXS_FFT_REAL_TYPE b3y = v[j+1+rstride*13];

      OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
      OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
      OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
      OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

      OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
      scratch[j*16+2]      = bS0x + bS1x;
      scratch[j*16+2+1]    = bS0y + bS1y;
      OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

      scratch[j*16+2*5]    = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*5+1]  = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUb2x = bD0x + bD1y;
      OXS_FFT_REAL_TYPE tUb3x = bD0x - bD1y;
      OXS_FFT_REAL_TYPE tUb2y = bD0y - bD1x;
      OXS_FFT_REAL_TYPE tUb3y = bD0y + bD1x;

      scratch[j*16+2*9]    = tUb2x*alphax + tUb2y*alphay;
      scratch[j*16+2*9+1]  = tUb2y*alphax - tUb2x*alphay;
      scratch[j*16+2*13]   = tUb3x*alphay + tUb3y*alphax;
      scratch[j*16+2*13+1] = tUb3y*alphay - tUb3x*alphax;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE c0x = v[j  +rstride*2];
      OXS_FFT_REAL_TYPE c0y = v[j+1+rstride*2];
      OXS_FFT_REAL_TYPE c2x = v[j  +rstride*10];
      OXS_FFT_REAL_TYPE c2y = v[j+1+rstride*10];

      OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
      OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
      OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
      OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

      OXS_FFT_REAL_TYPE c1x = v[j  +rstride*6];
      OXS_FFT_REAL_TYPE c1y = v[j+1+rstride*6];
      OXS_FFT_REAL_TYPE c3x = v[j  +rstride*14];
      OXS_FFT_REAL_TYPE c3y = v[j+1+rstride*14];

      OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
      OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
      OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
      OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

      OXS_FFT_REAL_TYPE tUc2x = cD1y + cD0x;
      OXS_FFT_REAL_TYPE tUc3x = cD1y - cD0x;
      OXS_FFT_REAL_TYPE tUc2y = cD0y - cD1x;
      OXS_FFT_REAL_TYPE tUc3y = cD0y + cD1x;

      scratch[j*16+2*2]    = cS1x + cS0x;
      scratch[j*16+2*2+1]  = cS0y + cS1y;
      scratch[j*16+2*6]    = cS0y - cS1y;
      scratch[j*16+2*6+1]  = cS1x - cS0x;

      scratch[j*16+2*10]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*10+1] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*14]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*14+1] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE d0x = v[j  +rstride*3];
      OXS_FFT_REAL_TYPE d0y = v[j+1+rstride*3];
      OXS_FFT_REAL_TYPE d2x = v[j  +rstride*11];
      OXS_FFT_REAL_TYPE d2y = v[j+1+rstride*11];

      OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
      OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
      OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
      OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

      OXS_FFT_REAL_TYPE d1x = v[j  +rstride*7];
      OXS_FFT_REAL_TYPE d1y = v[j+1+rstride*7];
      OXS_FFT_REAL_TYPE d3x = v[j  +rstride*15];
      OXS_FFT_REAL_TYPE d3y = v[j+1+rstride*15];

      OXS_FFT_REAL_TYPE dS1x = d3x + d1x;
      OXS_FFT_REAL_TYPE dD1x = d3x - d1x;
      OXS_FFT_REAL_TYPE dS1y = d1y + d3y;
      OXS_FFT_REAL_TYPE dD1y = d1y - d3y;

      OXS_FFT_REAL_TYPE tUd1x = dS1x - dS0x;
      scratch[j*16+2*3]    = dS1x + dS0x;
      scratch[j*16+2*3+1]  = dS0y + dS1y;
      OXS_FFT_REAL_TYPE tUd1y = dS0y - dS1y;

      scratch[j*16+2*7]    = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*7+1]  = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
      OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
      OXS_FFT_REAL_TYPE tUd2y = dD1x + dD0y;
      OXS_FFT_REAL_TYPE tUd3y = dD1x - dD0y;

      scratch[j*16+2*11]   = tUd2x*alphay + tUd2y*alphax;
      scratch[j*16+2*11+1] = tUd2y*alphay - tUd2x*alphax;
      scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
      scratch[j*16+2*15+1] = tUd3y*alphax - tUd3x*alphay;
    }

    // Do 4 lower (bottom/final) -level dragonflies,
    // with embedded, hard coded bit reversals:
    //
    //                  1 <->  8
    //                  2 <->  4
    //                  3 <-> 12
    //                  5 <-> 10
    //                  7 <-> 14
    //                 11 <-> 13
    //
    { // Block 0-3
      OXS_FFT_REAL_TYPE const * const sv = scratch;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[j]              = BaSx + BbSx;  //  0->0
        v[j+1]            = BaSy + BbSy;
        v[8*rstride+j]    = BaSx - BbSx;  //  1->8
        v[8*rstride+j+1]  = BaSy - BbSy;

        v[4*rstride+j]    = BaDx + BbDy;  //  2->4
        v[4*rstride+j+1]  = BaDy - BbDx;
        v[12*rstride+j]   = BaDx - BbDy;  //  3->12
        v[12*rstride+j+1] = BaDy + BbDx;
      }
    }

    { // Block 4-7
      OXS_FFT_REAL_TYPE const * const sv = scratch + 4*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[2*rstride+j]    = BaSx + BbSx;  //  4->2
        v[2*rstride+j+1]  = BaSy + BbSy;
        v[10*rstride+j]   = BaSx - BbSx;  //  5->10
        v[10*rstride+j+1] = BaSy - BbSy;

        v[6*rstride+j]    = BaDx + BbDy;  //  6->6
        v[6*rstride+j+1]  = BaDy - BbDx;
        v[14*rstride+j]   = BaDx - BbDy;  //  7->14
        v[14*rstride+j+1] = BaDy + BbDx;
      }
    }

    { // Block 8-11
      OXS_FFT_REAL_TYPE const * const sv = scratch + 8*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[rstride+j]      = BaSx + BbSx;  //  8->1
        v[rstride+j+1]    = BaSy + BbSy;
        v[9*rstride+j]    = BaSx - BbSx;  //  9->9
        v[9*rstride+j+1]  = BaSy - BbSy;

        v[5*rstride+j]    = BaDx + BbDy;  // 10->5
        v[5*rstride+j+1]  = BaDy - BbDx;
        v[13*rstride+j]   = BaDx - BbDy;  // 11->13
        v[13*rstride+j+1] = BaDy + BbDx;
      }
    }

    { // Block 12-15
      OXS_FFT_REAL_TYPE const * const sv = scratch + 12*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[3*rstride+j]    = BaSx + BbSx;  // 12->3
        v[3*rstride+j+1]  = BaSy + BbSy;
        v[11*rstride+j]   = BaSx - BbSx;  // 13->11
        v[11*rstride+j+1] = BaSy - BbSy;

        v[7*rstride+j]    = BaDx + BbDy;  // 14->7
        v[7*rstride+j+1]  = BaDy - BbDx;
        v[15*rstride+j]   = BaDx - BbDy;  // 15->15
        v[15*rstride+j+1] = BaDy + BbDx;
      }
    }

  }

#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize16
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 16
  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    OXS_FFT_REAL_TYPE* const v = arr + 2*block;  // Working vector

    // Top level 16-pass
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

    OC_INDEX j;
    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE a0x = v[j];
      OXS_FFT_REAL_TYPE a0y = v[j+1];
      OXS_FFT_REAL_TYPE a2x = v[j  +rstride*8];
      OXS_FFT_REAL_TYPE a2y = v[j+1+rstride*8];

      OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
      OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
      OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
      OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

      OXS_FFT_REAL_TYPE a1x = v[j  +rstride*4];
      OXS_FFT_REAL_TYPE a1y = v[j+1+rstride*4];
      OXS_FFT_REAL_TYPE a3x = v[j  +rstride*12];
      OXS_FFT_REAL_TYPE a3y = v[j+1+rstride*12];

      OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
      OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
      OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
      OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

      scratch[j*16]         = aS0x + aS1x;
      scratch[j*16+1]       = aS0y + aS1y;
      scratch[j*16+2*4]  = aS0x - aS1x;
      scratch[j*16+2*4+1]  = aS0y - aS1y;

      scratch[j*16+2*8]  = aD0x - aD1y;
      scratch[j*16+2*8+1]  = aD0y + aD1x;
      scratch[j*16+2*12] = aD0x + aD1y;
      scratch[j*16+2*12+1] = aD0y - aD1x;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE b0x = v[j  +rstride];
      OXS_FFT_REAL_TYPE b0y = v[j+1+rstride];
      OXS_FFT_REAL_TYPE b2x = v[j  +rstride*9];
      OXS_FFT_REAL_TYPE b2y = v[j+1+rstride*9];

      OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
      OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
      OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
      OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

      OXS_FFT_REAL_TYPE b1x = v[j  +rstride*5];
      OXS_FFT_REAL_TYPE b1y = v[j+1+rstride*5];
      OXS_FFT_REAL_TYPE b3x = v[j  +rstride*13];
      OXS_FFT_REAL_TYPE b3y = v[j+1+rstride*13];

      OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
      OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
      OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
      OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

      OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
      scratch[j*16+2]      = bS0x + bS1x;
      scratch[j*16+2+1]    = bS0y + bS1y;
      OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

      scratch[j*16+2*5]    = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*5+1]  = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
      OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
      OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
      OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

      scratch[j*16+2*9]    = tUb2x*alphax - tUb2y*alphay;
      scratch[j*16+2*9+1]  = tUb2y*alphax + tUb2x*alphay;
      scratch[j*16+2*13]   = tUb3x*alphay - tUb3y*alphax;
      scratch[j*16+2*13+1] = tUb3y*alphay + tUb3x*alphax;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE c0x = v[j  +rstride*2];
      OXS_FFT_REAL_TYPE c0y = v[j+1+rstride*2];
      OXS_FFT_REAL_TYPE c2x = v[j  +rstride*10];
      OXS_FFT_REAL_TYPE c2y = v[j+1+rstride*10];

      OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
      OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
      OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
      OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

      OXS_FFT_REAL_TYPE c1x = v[j  +rstride*6];
      OXS_FFT_REAL_TYPE c1y = v[j+1+rstride*6];
      OXS_FFT_REAL_TYPE c3x = v[j  +rstride*14];
      OXS_FFT_REAL_TYPE c3y = v[j+1+rstride*14];

      OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
      OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
      OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
      OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

      OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
      OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
      OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
      OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

      scratch[j*16+2*2]    = cS0x + cS1x;
      scratch[j*16+2*2+1]  = cS0y + cS1y;
      scratch[j*16+2*6]    = cS1y - cS0y;
      scratch[j*16+2*6+1]  = cS0x - cS1x;

      scratch[j*16+2*10]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*10+1] = (tUc2x + tUc2y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*14]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*14+1] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE d0x = v[j  +rstride*3];
      OXS_FFT_REAL_TYPE d0y = v[j+1+rstride*3];
      OXS_FFT_REAL_TYPE d2x = v[j  +rstride*11];
      OXS_FFT_REAL_TYPE d2y = v[j+1+rstride*11];

      OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
      OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
      OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
      OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

      OXS_FFT_REAL_TYPE d1x = v[j  +rstride*7];
      OXS_FFT_REAL_TYPE d1y = v[j+1+rstride*7];
      OXS_FFT_REAL_TYPE d3x = v[j  +rstride*15];
      OXS_FFT_REAL_TYPE d3y = v[j+1+rstride*15];

      OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
      OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
      OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
      OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

      OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
      scratch[j*16+2*3]    = dS0x + dS1x;
      scratch[j*16+2*3+1]  = dS1y + dS0y;
      OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

      scratch[j*16+2*7]    = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*7+1]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
      OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
      OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
      OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

      scratch[j*16+2*11]   = tUd2x*alphay - tUd2y*alphax;
      scratch[j*16+2*11+1] = tUd2y*alphay + tUd2x*alphax;
      scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
      scratch[j*16+2*15+1] = tUd3x*alphay - tUd3y*alphax;
    }

    // Do 4 lower (bottom/final) -level dragonflies,
    // with embedded, hard coded bit reversals:
    //
    //                  1 <->  8
    //                  2 <->  4
    //                  3 <-> 12
    //                  5 <-> 10
    //                  7 <-> 14
    //                 11 <-> 13
    //
    { // Block 0-3
      OXS_FFT_REAL_TYPE const * const sv = scratch;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[j]              = BaSx + BbSx;  //  0->0
        v[j+1]            = BaSy + BbSy;
        v[8*rstride+j]    = BaSx - BbSx;  //  1->8
        v[8*rstride+j+1]  = BaSy - BbSy;

        v[4*rstride+j]    = BaDx - BbDy;  //  2->4
        v[4*rstride+j+1]  = BaDy + BbDx;
        v[12*rstride+j]   = BaDx + BbDy;  //  3->12
        v[12*rstride+j+1] = BaDy - BbDx;
      }
    }

    { // Block 4-7
      OXS_FFT_REAL_TYPE const * const sv = scratch + 4*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[2*rstride+j]    = BaSx + BbSx;  //  4->2
        v[2*rstride+j+1]  = BaSy + BbSy;
        v[10*rstride+j]   = BaSx - BbSx;  //  5->10
        v[10*rstride+j+1] = BaSy - BbSy;

        v[6*rstride+j]    = BaDx - BbDy;  //  6->6
        v[6*rstride+j+1]  = BaDy + BbDx;
        v[14*rstride+j]   = BaDx + BbDy;  //  7->14
        v[14*rstride+j+1] = BaDy - BbDx;
      }
    }

    { // Block 8-11
      OXS_FFT_REAL_TYPE const * const sv = scratch + 8*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[rstride+j]      = BaSx + BbSx;  //  8->1
        v[rstride+j+1]    = BaSy + BbSy;
        v[9*rstride+j]    = BaSx - BbSx;  //  9->9
        v[9*rstride+j+1]  = BaSy - BbSy;

        v[5*rstride+j]    = BaDx - BbDy;  // 10->5
        v[5*rstride+j+1]  = BaDy + BbDx;
        v[13*rstride+j]   = BaDx + BbDy;  // 11->13
        v[13*rstride+j+1] = BaDy - BbDx;
      }
    }

    { // Block 12-15
      OXS_FFT_REAL_TYPE const * const sv = scratch + 12*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[3*rstride+j]    = BaSx + BbSx;  // 12->3
        v[3*rstride+j+1]  = BaSy + BbSy;
        v[11*rstride+j]   = BaSx - BbSx;  // 13->11
        v[11*rstride+j+1] = BaSy - BbSy;

        v[7*rstride+j]    = BaDx - BbDy;  // 14->7
        v[7*rstride+j+1]  = BaDy + BbDx;
        v[15*rstride+j]   = BaDx + BbDy;  // 15->15
        v[15*rstride+j+1] = BaDy - BbDx;
      }
    }

  }

#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize16ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 16

  OC_INDEX bw = OFS_ARRAY_BLOCKSIZE + (arrcount % OFS_ARRAY_BLOCKSIZE);
  if(bw>OFS_ARRAY_MAXBLOCKSIZE) bw -= OFS_ARRAY_BLOCKSIZE;
  if(bw>arrcount) bw=arrcount;
  bw *= 2;

  OC_INDEX block=0;
  while(block<2*arrcount) {
    OXS_FFT_REAL_TYPE* const v = arr + block;  // Working vector

    // Fill out zero-padding
    OC_INDEX i,j;
    for(i=csize_base;i<RTNFFTSIZE/2;++i) {
      OXS_FFT_REAL_TYPE* w = v + i*rstride;
      for(j=0;j<bw;++j) w[j] = 0.0;
    }

    // Top level 16-pass
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE a0x = v[j];
      OXS_FFT_REAL_TYPE a0y = v[j+1];
      OXS_FFT_REAL_TYPE a1x = v[j  +rstride*4];
      OXS_FFT_REAL_TYPE a1y = v[j+1+rstride*4];
      scratch[j*16]      = a0x + a1x;   scratch[j*16+1]      = a0y + a1y;
      scratch[j*16+2*4]  = a0x - a1x;   scratch[j*16+2*4+1]  = a0y - a1y;
      scratch[j*16+2*8]  = a0x + a1y;   scratch[j*16+2*8+1]  = a0y - a1x;
      scratch[j*16+2*12] = a0x - a1y;   scratch[j*16+2*12+1] = a0y + a1x;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE b0x = v[j  +rstride];
      OXS_FFT_REAL_TYPE b0y = v[j+1+rstride];
      OXS_FFT_REAL_TYPE b1x = v[j  +rstride*5];
      OXS_FFT_REAL_TYPE b1y = v[j+1+rstride*5];

      OXS_FFT_REAL_TYPE tUb1x = b0x - b1x;
      scratch[j*16  +2]    = b0x + b1x;
      scratch[j*16+1+2]    = b0y + b1y;
      OXS_FFT_REAL_TYPE tUb1y = b0y - b1y;

      scratch[j*16  +2*5]  = (tUb1x + tUb1y)*OXS_FFT_SQRT1_2;
      scratch[j*16+1+2*5]  = (tUb1y - tUb1x)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUb2x = b0x + b1y;
      OXS_FFT_REAL_TYPE tUb3x = b0x - b1y;
      OXS_FFT_REAL_TYPE tUb2y = b0y - b1x;
      OXS_FFT_REAL_TYPE tUb3y = b0y + b1x;

      scratch[j*16+2*9]    = tUb2x*alphax + tUb2y*alphay;
      scratch[j*16+2*9+1]  = tUb2y*alphax - tUb2x*alphay;
      scratch[j*16+2*13]   = tUb3x*alphay + tUb3y*alphax;
      scratch[j*16+2*13+1] = tUb3y*alphay - tUb3x*alphax;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE c0x = v[j  +rstride*2];
      OXS_FFT_REAL_TYPE c0y = v[j+1+rstride*2];
      OXS_FFT_REAL_TYPE c1x = v[j  +rstride*6];
      OXS_FFT_REAL_TYPE c1y = v[j+1+rstride*6];

      scratch[j*16+2*2]    = c1x + c0x;
      scratch[j*16+2*2+1]  = c0y + c1y;
      scratch[j*16+2*6]    = c0y - c1y;
      scratch[j*16+2*6+1]  = c1x - c0x;

      OXS_FFT_REAL_TYPE tUc2x = c1y + c0x;
      OXS_FFT_REAL_TYPE tUc3x = c1y - c0x;
      OXS_FFT_REAL_TYPE tUc2y = c0y - c1x;
      OXS_FFT_REAL_TYPE tUc3y = c0y + c1x;

      scratch[j*16+2*10]   = (tUc2y + tUc2x)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*10+1] = (tUc2y - tUc2x)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*14]   = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*14+1] = (tUc3x - tUc3y)*OXS_FFT_SQRT1_2;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE d0x = v[j  +rstride*3];
      OXS_FFT_REAL_TYPE d0y = v[j+1+rstride*3];
      OXS_FFT_REAL_TYPE d1x = v[j  +rstride*7];
      OXS_FFT_REAL_TYPE d1y = v[j+1+rstride*7];

      OXS_FFT_REAL_TYPE tUd1x = d1x - d0x;
      scratch[j*16+2*3]    = d1x + d0x;
      scratch[j*16+2*3+1]  = d0y + d1y;
      OXS_FFT_REAL_TYPE tUd1y = d0y - d1y;

      scratch[j*16+2*7]    = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*7+1]  = (tUd1x - tUd1y)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUd2x =  d1y + d0x;
      OXS_FFT_REAL_TYPE tUd3x =  d1y - d0x;
      OXS_FFT_REAL_TYPE tUd2y =  d0y - d1x;
      OXS_FFT_REAL_TYPE tUd3y =  d1x + d0y;

      scratch[j*16+2*11]   =  tUd2x*alphay + tUd2y*alphax;
      scratch[j*16+2*11+1] =  tUd2y*alphay - tUd2x*alphax;
      scratch[j*16+2*15]   =  tUd3x*alphax - tUd3y*alphay;
      scratch[j*16+2*15+1] = -tUd3y*alphax - tUd3x*alphay;
    }

    // Do 4 lower (bottom/final) -level dragonflies,
    // with embedded, hard coded bit reversals:
    //
    //                  1 <->  8
    //                  2 <->  4
    //                  3 <-> 12
    //                  5 <-> 10
    //                  7 <-> 14
    //                 11 <-> 13
    //
    { // Block 0-3
      OXS_FFT_REAL_TYPE const * const sv = scratch;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[j]              = BaSx + BbSx;  //  0->0
        v[j+1]            = BaSy + BbSy;
        v[8*rstride+j]    = BaSx - BbSx;  //  1->8
        v[8*rstride+j+1]  = BaSy - BbSy;

        v[4*rstride+j]    = BaDx + BbDy;  //  2->4
        v[4*rstride+j+1]  = BaDy - BbDx;
        v[12*rstride+j]   = BaDx - BbDy;  //  3->12
        v[12*rstride+j+1] = BaDy + BbDx;
      }
    }

    { // Block 4-7
      OXS_FFT_REAL_TYPE const * const sv = scratch + 4*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[2*rstride+j]    = BaSx + BbSx;  //  4->2
        v[2*rstride+j+1]  = BaSy + BbSy;
        v[10*rstride+j]   = BaSx - BbSx;  //  5->10
        v[10*rstride+j+1] = BaSy - BbSy;

        v[6*rstride+j]    = BaDx + BbDy;  //  6->6
        v[6*rstride+j+1]  = BaDy - BbDx;
        v[14*rstride+j]   = BaDx - BbDy;  //  7->14
        v[14*rstride+j+1] = BaDy + BbDx;
      }
    }

    { // Block 8-11
      OXS_FFT_REAL_TYPE const * const sv = scratch + 8*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[rstride+j]      = BaSx + BbSx;  //  8->1
        v[rstride+j+1]    = BaSy + BbSy;
        v[9*rstride+j]    = BaSx - BbSx;  //  9->9
        v[9*rstride+j+1]  = BaSy - BbSy;

        v[5*rstride+j]    = BaDx + BbDy;  // 10->5
        v[5*rstride+j+1]  = BaDy - BbDx;
        v[13*rstride+j]   = BaDx - BbDy;  // 11->13
        v[13*rstride+j+1] = BaDy + BbDx;
      }
    }

    { // Block 12-15
      OXS_FFT_REAL_TYPE const * const sv = scratch + 12*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[3*rstride+j]    = BaSx + BbSx;  // 12->3
        v[3*rstride+j+1]  = BaSy + BbSy;
        v[11*rstride+j]   = BaSx - BbSx;  // 13->11
        v[11*rstride+j+1] = BaSy - BbSy;

        v[7*rstride+j]    = BaDx + BbDy;  // 14->7
        v[7*rstride+j+1]  = BaDy - BbDx;
        v[15*rstride+j]   = BaDx - BbDy;  // 15->15
        v[15*rstride+j+1] = BaDy + BbDx;
      }
    }

    // Update for next block
    block += bw;
    bw = 2*OFS_ARRAY_BLOCKSIZE;
  }

#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize16ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 16
  OC_INDEX bw = OFS_ARRAY_BLOCKSIZE + (arrcount % OFS_ARRAY_BLOCKSIZE);
  if(bw>OFS_ARRAY_MAXBLOCKSIZE) bw -= OFS_ARRAY_BLOCKSIZE;
  if(bw>arrcount) bw=arrcount;
  bw *= 2;

  OC_INDEX block=0;
  while(block<2*arrcount) {
    OXS_FFT_REAL_TYPE* const v = arr + block;  // Working vector

    // Top level 16-pass
    const OXS_FFT_REAL_TYPE alphax =  0.923879532511286756128L; // cos(pi/8)
    const OXS_FFT_REAL_TYPE alphay =  0.382683432365089771728L; // sin(pi/8)

    OC_INDEX j;
    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE a0x = v[j];
      OXS_FFT_REAL_TYPE a0y = v[j+1];
      OXS_FFT_REAL_TYPE a2x = v[j  +rstride*8];
      OXS_FFT_REAL_TYPE a2y = v[j+1+rstride*8];

      OXS_FFT_REAL_TYPE aS0x = a0x + a2x;
      OXS_FFT_REAL_TYPE aD0x = a0x - a2x;
      OXS_FFT_REAL_TYPE aS0y = a0y + a2y;
      OXS_FFT_REAL_TYPE aD0y = a0y - a2y;

      OXS_FFT_REAL_TYPE a1x = v[j  +rstride*4];
      OXS_FFT_REAL_TYPE a1y = v[j+1+rstride*4];
      OXS_FFT_REAL_TYPE a3x = v[j  +rstride*12];
      OXS_FFT_REAL_TYPE a3y = v[j+1+rstride*12];

      OXS_FFT_REAL_TYPE aS1x = a1x + a3x;
      OXS_FFT_REAL_TYPE aD1x = a1x - a3x;
      OXS_FFT_REAL_TYPE aS1y = a1y + a3y;
      OXS_FFT_REAL_TYPE aD1y = a1y - a3y;

      scratch[j*16]        = aS0x + aS1x;
      scratch[j*16+1]      = aS0y + aS1y;
      scratch[j*16+2*4]    = aS0x - aS1x;
      scratch[j*16+2*4+1]  = aS0y - aS1y;

      scratch[j*16+2*8]    = aD0x - aD1y;
      scratch[j*16+2*8+1]  = aD0y + aD1x;
      scratch[j*16+2*12]   = aD0x + aD1y;
      scratch[j*16+2*12+1] = aD0y - aD1x;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE b0x = v[j  +rstride];
      OXS_FFT_REAL_TYPE b0y = v[j+1+rstride];
      OXS_FFT_REAL_TYPE b2x = v[j  +rstride*9];
      OXS_FFT_REAL_TYPE b2y = v[j+1+rstride*9];

      OXS_FFT_REAL_TYPE bS0x = b0x + b2x;
      OXS_FFT_REAL_TYPE bD0x = b0x - b2x;
      OXS_FFT_REAL_TYPE bS0y = b0y + b2y;
      OXS_FFT_REAL_TYPE bD0y = b0y - b2y;

      OXS_FFT_REAL_TYPE b1x = v[j  +rstride*5];
      OXS_FFT_REAL_TYPE b1y = v[j+1+rstride*5];
      OXS_FFT_REAL_TYPE b3x = v[j  +rstride*13];
      OXS_FFT_REAL_TYPE b3y = v[j+1+rstride*13];

      OXS_FFT_REAL_TYPE bS1x = b1x + b3x;
      OXS_FFT_REAL_TYPE bD1x = b1x - b3x;
      OXS_FFT_REAL_TYPE bS1y = b1y + b3y;
      OXS_FFT_REAL_TYPE bD1y = b1y - b3y;

      OXS_FFT_REAL_TYPE tUb1x = bS0x - bS1x;
      scratch[j*16+2]      = bS0x + bS1x;
      scratch[j*16+2+1]    = bS0y + bS1y;
      OXS_FFT_REAL_TYPE tUb1y = bS0y - bS1y;

      scratch[j*16+2*5]    = (tUb1x - tUb1y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*5+1]  = (tUb1y + tUb1x)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUb2x = bD0x - bD1y;
      OXS_FFT_REAL_TYPE tUb3x = bD0x + bD1y;
      OXS_FFT_REAL_TYPE tUb2y = bD0y + bD1x;
      OXS_FFT_REAL_TYPE tUb3y = bD0y - bD1x;

      scratch[j*16+2*9]    = tUb2x*alphax - tUb2y*alphay;
      scratch[j*16+2*9+1]  = tUb2y*alphax + tUb2x*alphay;
      scratch[j*16+2*13]   = tUb3x*alphay - tUb3y*alphax;
      scratch[j*16+2*13+1] = tUb3y*alphay + tUb3x*alphax;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE c0x = v[j  +rstride*2];
      OXS_FFT_REAL_TYPE c0y = v[j+1+rstride*2];
      OXS_FFT_REAL_TYPE c2x = v[j  +rstride*10];
      OXS_FFT_REAL_TYPE c2y = v[j+1+rstride*10];

      OXS_FFT_REAL_TYPE cS0x = c0x + c2x;
      OXS_FFT_REAL_TYPE cD0x = c0x - c2x;
      OXS_FFT_REAL_TYPE cS0y = c0y + c2y;
      OXS_FFT_REAL_TYPE cD0y = c0y - c2y;

      OXS_FFT_REAL_TYPE c1x = v[j  +rstride*6];
      OXS_FFT_REAL_TYPE c1y = v[j+1+rstride*6];
      OXS_FFT_REAL_TYPE c3x = v[j  +rstride*14];
      OXS_FFT_REAL_TYPE c3y = v[j+1+rstride*14];

      OXS_FFT_REAL_TYPE cS1x = c1x + c3x;
      OXS_FFT_REAL_TYPE cD1x = c1x - c3x;
      OXS_FFT_REAL_TYPE cS1y = c1y + c3y;
      OXS_FFT_REAL_TYPE cD1y = c1y - c3y;

      OXS_FFT_REAL_TYPE tUc2x = cD0x - cD1y;
      OXS_FFT_REAL_TYPE tUc3x = cD0x + cD1y;
      OXS_FFT_REAL_TYPE tUc2y = cD0y + cD1x;
      OXS_FFT_REAL_TYPE tUc3y = cD1x - cD0y;

      scratch[j*16+2*2]    = cS0x + cS1x;
      scratch[j*16+2*2+1]  = cS0y + cS1y;
      scratch[j*16+2*6]    = cS1y - cS0y;
      scratch[j*16+2*6+1]  = cS0x - cS1x;

      scratch[j*16+2*10]   = (tUc2x - tUc2y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*10+1] = (tUc2x + tUc2y)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*14]   = (tUc3y - tUc3x)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*14+1] = (tUc3x + tUc3y)*OXS_FFT_SQRT1_2;
    }

    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE d0x = v[j  +rstride*3];
      OXS_FFT_REAL_TYPE d0y = v[j+1+rstride*3];
      OXS_FFT_REAL_TYPE d2x = v[j  +rstride*11];
      OXS_FFT_REAL_TYPE d2y = v[j+1+rstride*11];

      OXS_FFT_REAL_TYPE dS0x = d0x + d2x;
      OXS_FFT_REAL_TYPE dD0x = d0x - d2x;
      OXS_FFT_REAL_TYPE dS0y = d0y + d2y;
      OXS_FFT_REAL_TYPE dD0y = d0y - d2y;

      OXS_FFT_REAL_TYPE d1x = v[j  +rstride*7];
      OXS_FFT_REAL_TYPE d1y = v[j+1+rstride*7];
      OXS_FFT_REAL_TYPE d3x = v[j  +rstride*15];
      OXS_FFT_REAL_TYPE d3y = v[j+1+rstride*15];

      OXS_FFT_REAL_TYPE dS1x = d1x + d3x;
      OXS_FFT_REAL_TYPE dD1x = d1x - d3x;
      OXS_FFT_REAL_TYPE dS1y = d3y + d1y;
      OXS_FFT_REAL_TYPE dD1y = d3y - d1y;

      OXS_FFT_REAL_TYPE tUd1x = dS0x - dS1x;
      scratch[j*16+2*3]    = dS0x + dS1x;
      scratch[j*16+2*3+1]  = dS1y + dS0y;
      OXS_FFT_REAL_TYPE tUd1y = dS1y - dS0y;

      scratch[j*16+2*7]    = (tUd1y - tUd1x)*OXS_FFT_SQRT1_2;
      scratch[j*16+2*7+1]  = (tUd1x + tUd1y)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE tUd2x = dD1y + dD0x;
      OXS_FFT_REAL_TYPE tUd3x = dD1y - dD0x;
      OXS_FFT_REAL_TYPE tUd2y = dD0y + dD1x;
      OXS_FFT_REAL_TYPE tUd3y = dD0y - dD1x;

      scratch[j*16+2*11]   = tUd2x*alphay - tUd2y*alphax;
      scratch[j*16+2*11+1] = tUd2y*alphay + tUd2x*alphax;
      scratch[j*16+2*15]   = tUd3x*alphax + tUd3y*alphay;
      scratch[j*16+2*15+1] = tUd3x*alphay - tUd3y*alphax;
    }

    // Do 4 lower (bottom/final) -level dragonflies,
    // with embedded, hard coded bit reversals:
    //
    //                  1 <->  8
    //                  2 <->  4
    //                  3 <-> 12
    //                  5 <-> 10
    //                  7 <-> 14
    //                 11 <-> 13
    //
    // This "...ZP" variant assumes that the last half of the
    // results are to be thrown away, or equivalently, that
    // the pre-reversal odd (ja%2==1) results are not used.
    //
    { // Block 0-3
      OXS_FFT_REAL_TYPE const * const sv = scratch;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[j]              = BaSx + BbSx;  //  0->0
        v[j+1]            = BaSy + BbSy;
        v[4*rstride+j]    = BaDx - BbDy;  //  2->4
        v[4*rstride+j+1]  = BaDy + BbDx;
      }
    }

    { // Block 4-7
      OXS_FFT_REAL_TYPE const * const sv = scratch + 4*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[2*rstride+j]    = BaSx + BbSx;  //  4->2
        v[2*rstride+j+1]  = BaSy + BbSy;
        v[6*rstride+j]    = BaDx - BbDy;  //  6->6
        v[6*rstride+j+1]  = BaDy + BbDx;
      }
    }

    { // Block 8-11
      OXS_FFT_REAL_TYPE const * const sv = scratch + 8*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[rstride+j]      = BaSx + BbSx;  //  8->1
        v[rstride+j+1]    = BaSy + BbSy;
        v[5*rstride+j]    = BaDx - BbDy;  // 10->5
        v[5*rstride+j+1]  = BaDy + BbDx;
      }
    }

    { // Block 12-15
      OXS_FFT_REAL_TYPE const * const sv = scratch + 12*2;
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE Uax = sv[j*16];
        OXS_FFT_REAL_TYPE Uay = sv[j*16+1];
        OXS_FFT_REAL_TYPE Ucx = sv[j*16+2*2];
        OXS_FFT_REAL_TYPE Ucy = sv[j*16+2*2+1];

        OXS_FFT_REAL_TYPE BaSx = Uax + Ucx;
        OXS_FFT_REAL_TYPE BaDx = Uax - Ucx;
        OXS_FFT_REAL_TYPE BaSy = Uay + Ucy;
        OXS_FFT_REAL_TYPE BaDy = Uay - Ucy;

        OXS_FFT_REAL_TYPE Ubx = sv[j*16+2];
        OXS_FFT_REAL_TYPE Uby = sv[j*16+2+1];
        OXS_FFT_REAL_TYPE Udx = sv[j*16+2*3];
        OXS_FFT_REAL_TYPE Udy = sv[j*16+2*3+1];

        OXS_FFT_REAL_TYPE BbSx = Ubx + Udx;
        OXS_FFT_REAL_TYPE BbDx = Ubx - Udx;
        OXS_FFT_REAL_TYPE BbSy = Uby + Udy;
        OXS_FFT_REAL_TYPE BbDy = Uby - Udy;

        v[3*rstride+j]    = BaSx + BbSx;  // 12->3
        v[3*rstride+j+1]  = BaSy + BbSy;
        v[7*rstride+j]    = BaDx - BbDy;  // 14->7
        v[7*rstride+j+1]  = BaDy + BbDx;
      }
    }

    // Update for next block
    block += bw;
    bw = 2*OFS_ARRAY_BLOCKSIZE;
  }

#undef RTNFFTSIZE
}

#ifdef TRY1
void
Oxs_FFTStrided::ForwardFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    OXS_FFT_REAL_TYPE* const v = arr + 2*block;  // Working vector
    OC_INDEX i,j;

    // Load scratch array while processing top-level butterfly
    if(csize_base>=RTNFFTSIZE/2) {
      for(i=0;i<csize_base-RTNFFTSIZE/2;++i) {
        for(j=0;j<bw;++j) {
          OXS_FFT_REAL_TYPE a = v[ i   *rstride+j];
          OXS_FFT_REAL_TYPE b = v[(i+RTNFFTSIZE/2)*rstride+j];
          scratch[i*bw+j]     = a + b;
          scratch[(i+RTNFFTSIZE/2)*bw+j] = a - b;
        }
      }
      while(i<RTNFFTSIZE/2) {
        for(j=0;j<bw;++j) {
          scratch[i*bw+j] = scratch[(i+RTNFFTSIZE/2)*bw+j]
            = v[i*rstride+j];
        }
        ++i;
      }
    } else { // csize_base<RTNFFTSIZE/2
      for(i=0;i<csize_base;++i) {
        for(j=0;j<bw;++j) {
          scratch[i*bw+j] = scratch[(i+RTNFFTSIZE/2)*bw+j]
            = v[i*rstride+j];
        }
      }
      while(i<RTNFFTSIZE/2) {
        for(j=0;j<bw;++j) {
          scratch[i*bw+j]=scratch[(i+RTNFFTSIZE/2)*bw+j]=0;
        }
        ++i;
      }
    }

    // Second level butterflies
    for(i=0;i<2;++i) {
      for(j=0;j<bw;++j) {
        OXS_FFT_REAL_TYPE a = scratch[ i   *bw+j];
        OXS_FFT_REAL_TYPE b = scratch[(i+2)*bw+j];
        scratch[i*bw+j]     = a + b;
        scratch[(i+2)*bw+j] = a - b;
      }
    }
    for(i=4;i<6;++i) {
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE ax = scratch[ i   *bw+j];
        OXS_FFT_REAL_TYPE ay = scratch[ i   *bw+j + 1];
        OXS_FFT_REAL_TYPE bx = scratch[(i+2)*bw+j];
        OXS_FFT_REAL_TYPE by = scratch[(i+2)*bw+j + 1];
        scratch[ i   *bw+j]     = ax + by;
        scratch[ i   *bw+j + 1] = ay - bx;
        scratch[(i+2)*bw+j]     = ax - by;
        scratch[(i+2)*bw+j + 1] = ay + bx;
      }
    }

    // Bottom level butterflies with embedded bit reversal
    { // i=0,1:   0->0, 1->4.
      for(j=0;j<bw;++j) {
        OXS_FFT_REAL_TYPE a = scratch[j];
        OXS_FFT_REAL_TYPE b = scratch[bw+j];
        v[j]               = a + b;
        v[4*rstride+j]     = a - b;
      }
    }

    { // i=2,3:   2->2, 3->6.
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE ax = scratch[2*bw+j];
        OXS_FFT_REAL_TYPE ay = scratch[2*bw+j+1];
        OXS_FFT_REAL_TYPE bx = scratch[3*bw+j];
        OXS_FFT_REAL_TYPE by = scratch[3*bw+j+1];
        v[2*rstride+j]    =  ax + by;
        v[2*rstride+j+1]  =  ay - bx;
        v[6*rstride+j]    =  ax - by;
        v[6*rstride+j+1]  =  ay + bx;
      }
    }

    { // i=4,5:   4->1, 5->5.
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE ax = scratch[4*bw+j];
        OXS_FFT_REAL_TYPE ay = scratch[4*bw+j+1];
        OXS_FFT_REAL_TYPE bx = scratch[5*bw+j];
        OXS_FFT_REAL_TYPE by = scratch[5*bw+j+1];
        OXS_FFT_REAL_TYPE tx = (by+bx)*OXS_FFT_SQRT1_2;
        OXS_FFT_REAL_TYPE ty = (by-bx)*OXS_FFT_SQRT1_2;
        v[  rstride+j]    = ax + tx;
        v[  rstride+j+1]  = ay + ty;
        v[5*rstride+j]    = ax - tx;
        v[5*rstride+j+1]  = ay - ty;
      }
    }

    { // i=6,7:   6->3, 7->7.
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE ax = scratch[6*bw+j];
        OXS_FFT_REAL_TYPE ay = scratch[6*bw+j+1];
        OXS_FFT_REAL_TYPE bx = scratch[7*bw+j];
        OXS_FFT_REAL_TYPE by = scratch[7*bw+j+1];
        OXS_FFT_REAL_TYPE tx = (by+bx)*OXS_FFT_SQRT1_2;
        OXS_FFT_REAL_TYPE ty = (by-bx)*OXS_FFT_SQRT1_2;
        v[3*rstride+j]    = ax + ty;
        v[3*rstride+j+1]  = ay - tx;
        v[7*rstride+j]    = ax - ty;
        v[7*rstride+j+1]  = ay + tx;
      }
    }

  }

#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    OXS_FFT_REAL_TYPE* const v = arr + 2*block;  // Working vector
    OC_INDEX i,j;

    // Top-level butterfly, with embedded copy from v to scratch
    for(i=0;i<RTNFFTSIZE/2;++i) {
      for(j=0;j<bw;++j) {
        OXS_FFT_REAL_TYPE a = v[ i   *rstride+j];
        OXS_FFT_REAL_TYPE b = v[(i+RTNFFTSIZE/2)*rstride+j];
        scratch[i*bw+j]     = a + b;
        scratch[(i+RTNFFTSIZE/2)*bw+j] = a - b;
      }
    }

    // Second level butterflies
    for(i=0;i<2;++i) {
      for(j=0;j<bw;++j) {
        OXS_FFT_REAL_TYPE a = scratch[ i   *bw+j];
        OXS_FFT_REAL_TYPE b = scratch[(i+2)*bw+j];
        scratch[i*bw+j]     = a + b;
        scratch[(i+2)*bw+j] = a - b;
      }
    }
    for(i=4;i<6;++i) {
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE ax = scratch[ i   *bw+j];
        OXS_FFT_REAL_TYPE ay = scratch[ i   *bw+j + 1];
        OXS_FFT_REAL_TYPE bx = scratch[(i+2)*bw+j];
        OXS_FFT_REAL_TYPE by = scratch[(i+2)*bw+j + 1];
        scratch[ i   *bw+j]     = ax - by;
        scratch[ i   *bw+j + 1] = ay + bx;
        scratch[(i+2)*bw+j]     = ax + by;
        scratch[(i+2)*bw+j + 1] = ay - bx;
      }
    }

    // Bottom level butterflies with embedded bit reversal
    { // i=0,1:   0->0, 1->4.
      for(j=0;j<bw;++j) {
        OXS_FFT_REAL_TYPE a = scratch[j];
        OXS_FFT_REAL_TYPE b = scratch[bw+j];
        v[j]               = a + b;
        v[4*rstride+j]     = a - b;
      }
    }

    { // i=2,3:   2->2, 3->6.
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE ax = scratch[2*bw+j];
        OXS_FFT_REAL_TYPE ay = scratch[2*bw+j+1];
        OXS_FFT_REAL_TYPE bx = scratch[3*bw+j];
        OXS_FFT_REAL_TYPE by = scratch[3*bw+j+1];
        v[2*rstride+j]    =  ax - by;
        v[2*rstride+j+1]  =  ay + bx;
        v[6*rstride+j]    =  ax + by;
        v[6*rstride+j+1]  =  ay - bx;
      }
    }

    { // i=4,5:   4->1, 5->5.
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE ax = scratch[4*bw+j];
        OXS_FFT_REAL_TYPE ay = scratch[4*bw+j+1];
        OXS_FFT_REAL_TYPE bx = scratch[5*bw+j];
        OXS_FFT_REAL_TYPE by = scratch[5*bw+j+1];
        OXS_FFT_REAL_TYPE tx = (bx-by)*OXS_FFT_SQRT1_2;
        OXS_FFT_REAL_TYPE ty = (bx+by)*OXS_FFT_SQRT1_2;
        v[  rstride+j]    = ax + tx;
        v[  rstride+j+1]  = ay + ty;
        v[5*rstride+j]    = ax - tx;
        v[5*rstride+j+1]  = ay - ty;
      }
    }

    { // i=6,7:   6->3, 7->7.
      for(j=0;j<bw;j+=2) {
        OXS_FFT_REAL_TYPE ax = scratch[6*bw+j];
        OXS_FFT_REAL_TYPE ay = scratch[6*bw+j+1];
        OXS_FFT_REAL_TYPE bx = scratch[7*bw+j];
        OXS_FFT_REAL_TYPE by = scratch[7*bw+j+1];
        OXS_FFT_REAL_TYPE tx = (bx+by)*OXS_FFT_SQRT1_2;
        OXS_FFT_REAL_TYPE ty = (bx-by)*OXS_FFT_SQRT1_2;
        v[3*rstride+j]    = ax - tx;
        v[3*rstride+j+1]  = ay + ty;
        v[7*rstride+j]    = ax + tx;
        v[7*rstride+j+1]  = ay - ty;
      }
    }

  }

#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{}

void
Oxs_FFTStrided::InverseFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{}

#endif // TRY1

#ifdef TRY2
void
Oxs_FFTStrided::ForwardFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array
    for(j=0;j<csize_base;++j) {
      scratch[2*j]   = v[j*rstride];
      scratch[2*j+1] = v[j*rstride+1];
    }

    // Do upper two levels of butterflies
    OXS_FFT_REAL_TYPE s1x = scratch[0] + scratch[8];
    OXS_FFT_REAL_TYPE d1x = scratch[0] - scratch[8];
    OXS_FFT_REAL_TYPE s1y = scratch[1] + scratch[9];
    OXS_FFT_REAL_TYPE d1y = scratch[1] - scratch[9];

    OXS_FFT_REAL_TYPE s2x = scratch[2] + scratch[10];
    OXS_FFT_REAL_TYPE d2x = scratch[2] - scratch[10];
    OXS_FFT_REAL_TYPE s2y = scratch[3] + scratch[11];
    OXS_FFT_REAL_TYPE d2y = scratch[3] - scratch[11];

    OXS_FFT_REAL_TYPE s3x = scratch[4] + scratch[12];
    OXS_FFT_REAL_TYPE d3x = scratch[4] - scratch[12];
    OXS_FFT_REAL_TYPE s3y = scratch[5] + scratch[13];
    OXS_FFT_REAL_TYPE d3y = scratch[5] - scratch[13];

    OXS_FFT_REAL_TYPE s4x = scratch[6] + scratch[14];
    OXS_FFT_REAL_TYPE d4x = scratch[6] - scratch[14];
    OXS_FFT_REAL_TYPE s4y = scratch[7] + scratch[15];
    OXS_FFT_REAL_TYPE d4y = scratch[7] - scratch[15];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2y + E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2y - E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2y + T2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2y - T2x)*OXS_FFT_SQRT1_2;

    // Do bottom level butterfly and write back to
    // v, with embedded bit reversal
    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;
    v[4*rstride]    = A1x - A2x;   // 1 -> 4
    v[4*rstride+1]  = A1y - A2y;

    v[2*rstride]    = B1x + B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y - B2x;
    v[6*rstride]    = B1x - B2y;   // 3 -> 6
    v[6*rstride+1]  = B1y + B2x;

    v[rstride]      = E1x + bE2x;   // 4 -> 1
    v[rstride+1]    = E1y + bE2y;
    v[5*rstride]    = E1x - bE2x;   // 5 -> 5
    v[5*rstride+1]  = E1y - bE2y;

    v[3*rstride]    = T1x + bT2y;   // 6 -> 3
    v[3*rstride+1]  = T1y - bT2x;
    v[7*rstride]    = T1x - bT2y;   // 7 -> 7
    v[7*rstride+1]  = T1y + bT2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Do upper two levels of butterflies
    OXS_FFT_REAL_TYPE s1x = v[0] + v[4*rstride];
    OXS_FFT_REAL_TYPE d1x = v[0] - v[4*rstride];
    OXS_FFT_REAL_TYPE s1y = v[1] + v[4*rstride+1];
    OXS_FFT_REAL_TYPE d1y = v[1] - v[4*rstride+1];

    OXS_FFT_REAL_TYPE s2x = v[rstride]   + v[5*rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]   - v[5*rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1] + v[5*rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1] - v[5*rstride+1];

    OXS_FFT_REAL_TYPE s3x = v[2*rstride]   + v[6*rstride];
    OXS_FFT_REAL_TYPE d3x = v[2*rstride]   - v[6*rstride];
    OXS_FFT_REAL_TYPE s3y = v[2*rstride+1] + v[6*rstride+1];
    OXS_FFT_REAL_TYPE d3y = v[2*rstride+1] - v[6*rstride+1];

    OXS_FFT_REAL_TYPE s4x = v[3*rstride]   + v[7*rstride];
    OXS_FFT_REAL_TYPE d4x = v[3*rstride]   - v[7*rstride];
    OXS_FFT_REAL_TYPE s4y = v[3*rstride+1] + v[7*rstride+1];
    OXS_FFT_REAL_TYPE d4y = v[3*rstride+1] - v[7*rstride+1];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2x - E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2x + E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2x - T2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2x + T2y)*OXS_FFT_SQRT1_2;

    // Do bottom level butterfly and write back to
    // v, with embedded bit reversal
    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;
    v[4*rstride]    = A1x - A2x;   // 1 -> 4
    v[4*rstride+1]  = A1y - A2y;

    v[2*rstride]    = B1x - B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y + B2x;
    v[6*rstride]    = B1x + B2y;   // 3 -> 6
    v[6*rstride+1]  = B1y - B2x;

    v[rstride]      = T1x + bT2x;   // 4 -> 1
    v[rstride+1]    = T1y + bT2y;
    v[5*rstride]    = T1x - bT2x;   // 5 -> 5
    v[5*rstride+1]  = T1y - bT2y;

    v[3*rstride]    = E1x - bE2y;   // 6 -> 3
    v[3*rstride+1]  = E1y + bE2x;
    v[7*rstride]    = E1x + bE2y;   // 7 -> 7
    v[7*rstride+1]  = E1y - bE2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{}

void
Oxs_FFTStrided::InverseFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{}

#endif // TRY2

#ifdef TRY3
void
Oxs_FFTStrided::ForwardFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array
    for(j=0;j<csize_base;++j) {
      scratch[2*j]   = v[j*rstride];
      scratch[2*j+1] = v[j*rstride+1];
    }

    // Do upper two levels of butterflies
    OXS_FFT_REAL_TYPE s1x = scratch[0] + scratch[8];
    OXS_FFT_REAL_TYPE d1x = scratch[0] - scratch[8];
    OXS_FFT_REAL_TYPE s1y = scratch[1] + scratch[9];
    OXS_FFT_REAL_TYPE d1y = scratch[1] - scratch[9];

    OXS_FFT_REAL_TYPE s3x = scratch[4] + scratch[12];
    OXS_FFT_REAL_TYPE d3x = scratch[4] - scratch[12];
    OXS_FFT_REAL_TYPE s3y = scratch[5] + scratch[13];
    OXS_FFT_REAL_TYPE d3y = scratch[5] - scratch[13];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE s2x = scratch[2] + scratch[10];
    OXS_FFT_REAL_TYPE d2x = scratch[2] - scratch[10];
    OXS_FFT_REAL_TYPE s2y = scratch[3] + scratch[11];
    OXS_FFT_REAL_TYPE d2y = scratch[3] - scratch[11];

    OXS_FFT_REAL_TYPE s4x = scratch[6] + scratch[14];
    OXS_FFT_REAL_TYPE d4x = scratch[6] - scratch[14];
    OXS_FFT_REAL_TYPE s4y = scratch[7] + scratch[15];
    OXS_FFT_REAL_TYPE d4y = scratch[7] - scratch[15];

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[4*rstride]    = A1x - A2x;   // 1 -> 4
    v[4*rstride+1]  = A1y - A2y;

    v[2*rstride]    = B1x + B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y - B2x;

    v[6*rstride]    = B1x - B2y;   // 3 -> 6
    v[6*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2y + E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2y - E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2y + T2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2y - T2x)*OXS_FFT_SQRT1_2;

    // Do bottom level butterfly and write back to
    // v, with embedded bit reversal
    v[rstride]      = E1x + bE2x;   // 4 -> 1
    v[rstride+1]    = E1y + bE2y;

    v[5*rstride]    = E1x - bE2x;   // 5 -> 5
    v[5*rstride+1]  = E1y - bE2y;

    v[3*rstride]    = T1x + bT2y;   // 6 -> 3
    v[3*rstride+1]  = T1y - bT2x;

    v[7*rstride]    = T1x - bT2y;   // 7 -> 7
    v[7*rstride+1]  = T1y + bT2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Do upper two levels of butterflies
    OXS_FFT_REAL_TYPE s1x = v[0] + v[4*rstride];
    OXS_FFT_REAL_TYPE d1x = v[0] - v[4*rstride];
    OXS_FFT_REAL_TYPE s1y = v[1] + v[4*rstride+1];
    OXS_FFT_REAL_TYPE d1y = v[1] - v[4*rstride+1];

    OXS_FFT_REAL_TYPE s3x = v[2*rstride]   + v[6*rstride];
    OXS_FFT_REAL_TYPE d3x = v[2*rstride]   - v[6*rstride];
    OXS_FFT_REAL_TYPE s3y = v[2*rstride+1] + v[6*rstride+1];
    OXS_FFT_REAL_TYPE d3y = v[2*rstride+1] - v[6*rstride+1];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE s2x = v[rstride]   + v[5*rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]   - v[5*rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1] + v[5*rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1] - v[5*rstride+1];

    OXS_FFT_REAL_TYPE s4x = v[3*rstride]   + v[7*rstride];
    OXS_FFT_REAL_TYPE d4x = v[3*rstride]   - v[7*rstride];
    OXS_FFT_REAL_TYPE s4y = v[3*rstride+1] + v[7*rstride+1];
    OXS_FFT_REAL_TYPE d4y = v[3*rstride+1] - v[7*rstride+1];

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[4*rstride]    = A1x - A2x;   // 1 -> 4
    v[4*rstride+1]  = A1y - A2y;

    v[2*rstride]    = B1x - B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y + B2x;

    v[6*rstride]    = B1x + B2y;   // 3 -> 6
    v[6*rstride+1]  = B1y - B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2x - E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2x + E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2x - T2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2x + T2y)*OXS_FFT_SQRT1_2;

    // Do bottom level butterfly and write back to
    // v, with embedded bit reversal
    v[rstride]      = T1x + bT2x;   // 4 -> 1
    v[rstride+1]    = T1y + bT2y;

    v[5*rstride]    = T1x - bT2x;   // 5 -> 5
    v[5*rstride+1]  = T1y - bT2y;

    v[3*rstride]    = E1x - bE2y;   // 6 -> 3
    v[3*rstride+1]  = E1y + bE2x;

    v[7*rstride]    = E1x + bE2y;   // 7 -> 7
    v[7*rstride+1]  = E1y - bE2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  OXS_FFT_REAL_TYPE const & ax = scratch[0];
  OXS_FFT_REAL_TYPE const & ay = scratch[1];
  OXS_FFT_REAL_TYPE const & bx = scratch[2];
  OXS_FFT_REAL_TYPE const & by = scratch[3];
  OXS_FFT_REAL_TYPE const & cx = scratch[4];
  OXS_FFT_REAL_TYPE const & cy = scratch[5];
  OXS_FFT_REAL_TYPE const & dx = scratch[6];
  OXS_FFT_REAL_TYPE const & dy = scratch[7];

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array
    for(j=0;j<csize_base;++j) {
      scratch[2*j]   = v[j*rstride];
      scratch[2*j+1] = v[j*rstride+1];
    }

    // Do butterflies plus embedded bit reversal
    OXS_FFT_REAL_TYPE A1x = ax + cx;
    OXS_FFT_REAL_TYPE B1x = ax - cx;
    OXS_FFT_REAL_TYPE A1y = ay + cy;
    OXS_FFT_REAL_TYPE B1y = ay - cy;

    OXS_FFT_REAL_TYPE E1x = ax + cy;
    OXS_FFT_REAL_TYPE T1x = ax - cy;
    OXS_FFT_REAL_TYPE E1y = ay - cx;
    OXS_FFT_REAL_TYPE T1y = ay + cx;

    OXS_FFT_REAL_TYPE A2x = bx + dx;
    OXS_FFT_REAL_TYPE B2x = bx - dx;
    OXS_FFT_REAL_TYPE A2y = by + dy;
    OXS_FFT_REAL_TYPE B2y = by - dy;

    OXS_FFT_REAL_TYPE E2x = bx + dy;
    OXS_FFT_REAL_TYPE T2x = bx - dy;
    OXS_FFT_REAL_TYPE E2y = by - dx;
    OXS_FFT_REAL_TYPE T2y = by + dx;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[4*rstride]    = A1x - A2x;   // 1 -> 4
    v[4*rstride+1]  = A1y - A2y;

    v[2*rstride]    = B1x + B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y - B2x;

    v[6*rstride]    = B1x - B2y;   // 3 -> 6
    v[6*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE bE2x = (E2y + E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2y - E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2y + T2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2y - T2x)*OXS_FFT_SQRT1_2;

    v[rstride]      = E1x + bE2x;   // 4 -> 1
    v[rstride+1]    = E1y + bE2y;

    v[5*rstride]    = E1x - bE2x;   // 5 -> 5
    v[5*rstride+1]  = E1y - bE2y;

    v[3*rstride]    = T1x + bT2y;   // 6 -> 3
    v[3*rstride+1]  = T1y - bT2x;

    v[7*rstride]    = T1x - bT2y;   // 7 -> 7
    v[7*rstride+1]  = T1y + bT2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    OXS_FFT_REAL_TYPE s1x = v[0] + v[4*rstride];
    OXS_FFT_REAL_TYPE d1x = v[0] - v[4*rstride];
    OXS_FFT_REAL_TYPE s1y = v[1] + v[4*rstride+1];
    OXS_FFT_REAL_TYPE d1y = v[1] - v[4*rstride+1];

    OXS_FFT_REAL_TYPE s3x = v[2*rstride]   + v[6*rstride];
    OXS_FFT_REAL_TYPE d3x = v[2*rstride]   - v[6*rstride];
    OXS_FFT_REAL_TYPE s3y = v[2*rstride+1] + v[6*rstride+1];
    OXS_FFT_REAL_TYPE d3y = v[2*rstride+1] - v[6*rstride+1];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE s2x = v[rstride]     + v[5*rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]     - v[5*rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1]   + v[5*rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1]   - v[5*rstride+1];

    OXS_FFT_REAL_TYPE s4x = v[3*rstride]   + v[7*rstride];
    OXS_FFT_REAL_TYPE d4x = v[3*rstride]   - v[7*rstride];
    OXS_FFT_REAL_TYPE s4y = v[3*rstride+1] + v[7*rstride+1];
    OXS_FFT_REAL_TYPE d4y = v[3*rstride+1] - v[7*rstride+1];

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[2*rstride]    = B1x - B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2x - E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2x + E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2x - T2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2x + T2y)*OXS_FFT_SQRT1_2;

    v[rstride]      = T1x + bT2x;   // 4 -> 1
    v[rstride+1]    = T1y + bT2y;

    v[3*rstride]    = E1x - bE2y;   // 6 -> 3
    v[3*rstride+1]  = E1y + bE2x;
  }
#undef RTNFFTSIZE
}
#endif // TRY3

#ifdef TRY4
void
Oxs_FFTStrided::ForwardFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8

  if(csize_base<=RTNFFTSIZE/2) {
    ForwardFFTSize8ZP(arr);
    return;
  }

  const OC_INDEX voff = rstride*(RTNFFTSIZE/2);

  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    OXS_FFT_REAL_TYPE* const va = arr + 2*block;  // Working vectors
    OC_INDEX i,j;

    // Load scratch array while processing top-level butterfly
    // Assumes csize_base > RTNFFTSIZE/2.
    assert(csize_base > RTNFFTSIZE/2);
    for(i=0;i<csize_base-RTNFFTSIZE/2;++i) {
      OXS_FFT_REAL_TYPE* const st = scratch + 2*i;
      OXS_FFT_REAL_TYPE* const vt = va + i*rstride;
      for(j=0;j<bw;j+=2) {
        OC_INDEX joff = j*RTNFFTSIZE;
        OXS_FFT_REAL_TYPE ax = vt[j];
        OXS_FFT_REAL_TYPE ay = vt[j+1];
        OXS_FFT_REAL_TYPE bx = vt[j+voff];
        OXS_FFT_REAL_TYPE by = vt[j+voff+1];
        st[joff]              = ax + bx;
        st[joff+1]            = ay + by;
        st[joff+RTNFFTSIZE]   = ax - bx;
        st[joff+RTNFFTSIZE+1] = ay - by;
      }
    }
    while(i<RTNFFTSIZE/2) {
      OXS_FFT_REAL_TYPE* const st = scratch + 2*i;
      OXS_FFT_REAL_TYPE* const vt = va + i*rstride;
      for(j=0;j<bw;j+=2) {
        OC_INDEX joff = j*RTNFFTSIZE;
        st[joff]   = st[RTNFFTSIZE+joff]   = vt[j];
        st[joff+1] = st[RTNFFTSIZE+joff+1] = vt[j+1];
      }
      ++i;
    }

    // Second and third level butterflies, with
    // embedded bit-reversal
    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE* const st = scratch + j*RTNFFTSIZE;
      OXS_FFT_REAL_TYPE* const vt = va+j;
      // i = 0 through 3
      OXS_FFT_REAL_TYPE x0 = st[0];
      OXS_FFT_REAL_TYPE y0 = st[1];
      OXS_FFT_REAL_TYPE x2 = st[4];
      OXS_FFT_REAL_TYPE y2 = st[4+1];

      OXS_FFT_REAL_TYPE a0 = x0 + x2;
      OXS_FFT_REAL_TYPE a2 = x0 - x2;
      OXS_FFT_REAL_TYPE b0 = y0 + y2;
      OXS_FFT_REAL_TYPE b2 = y0 - y2;

      OXS_FFT_REAL_TYPE x1 = st[2];
      OXS_FFT_REAL_TYPE y1 = st[2+1];
      OXS_FFT_REAL_TYPE x3 = st[6];
      OXS_FFT_REAL_TYPE y3 = st[6+1];

      OXS_FFT_REAL_TYPE a1 = x1 + x3;
      OXS_FFT_REAL_TYPE a3 = x1 - x3;
      OXS_FFT_REAL_TYPE b1 = y1 + y3;
      OXS_FFT_REAL_TYPE b3 = y1 - y3;

      vt[0]                = a0 + a1;  // 0 -> 0
      vt[1]                = b0 + b1;
      vt[voff]             = a0 - a1;  // 1 -> 4
      vt[voff+1]           = b0 - b1;

      vt[     2*rstride]   = a2 + b3;  // 2 -> 2
      vt[     2*rstride+1] = b2 - a3;
      vt[voff+2*rstride]   = a2 - b3;  // 3 -> 6
      vt[voff+2*rstride+1] = b2 + a3;

      // i = 4 through 7
      OXS_FFT_REAL_TYPE x5 = st[10];
      OXS_FFT_REAL_TYPE y5 = st[10+1];
      OXS_FFT_REAL_TYPE x7 = st[14];
      OXS_FFT_REAL_TYPE y7 = st[14+1];

      OXS_FFT_REAL_TYPE a5 = x5 + y7;
      OXS_FFT_REAL_TYPE a7 = x5 - y7;

      OXS_FFT_REAL_TYPE b5 = y5 - x7;
      OXS_FFT_REAL_TYPE b7 = y5 + x7;

      OXS_FFT_REAL_TYPE cx = (b5+a5)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE cy = (b5-a5)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE dx = (b7+a7)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE dy = (b7-a7)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE x4 = st[8];
      OXS_FFT_REAL_TYPE y6 = st[12+1];

      OXS_FFT_REAL_TYPE a4 = x4 + y6;
      OXS_FFT_REAL_TYPE a6 = x4 - y6;

      OXS_FFT_REAL_TYPE y4 = st[8+1];
      OXS_FFT_REAL_TYPE x6 = st[12];

      OXS_FFT_REAL_TYPE b4 = y4 - x6;
      OXS_FFT_REAL_TYPE b6 = y4 + x6;

      vt[       rstride]    = a4 + cx; // 4 -> 1
      vt[       rstride+1]  = b4 + cy;
      vt[voff  +rstride]    = a4 - cx; // 5 -> 5
      vt[voff  +rstride+1]  = b4 - cy;
      vt[     3*rstride]    = a6 + dy; // 6 -> 3
      vt[     3*rstride+1]  = b6 - dx;
      vt[voff+3*rstride]    = a6 - dy; // 7 -> 7
      vt[voff+3*rstride+1]  = b6 + dx;
    }
  }

#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8

  if(csize_base<=RTNFFTSIZE/2) {
    InverseFFTSize8ZP(arr);
    return;
  }

  const OC_INDEX voff = rstride*(RTNFFTSIZE/2);

  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    OXS_FFT_REAL_TYPE* const va = arr + 2*block;  // Working vectors
    OC_INDEX i,j;

    // Load scratch array while processing top-level butterfly
    // Assumes csize_base > RTNFFTSIZE/2.
    assert(csize_base > RTNFFTSIZE/2);
    for(i=0;i<RTNFFTSIZE/2;++i) {
      OXS_FFT_REAL_TYPE* const st = scratch + 2*i;
      OXS_FFT_REAL_TYPE* const vt = va + i*rstride;
      for(j=0;j<bw;j+=2) {
        OC_INDEX joff = j*RTNFFTSIZE;
        OXS_FFT_REAL_TYPE ax = vt[j];
        OXS_FFT_REAL_TYPE ay = vt[j+1];
        OXS_FFT_REAL_TYPE bx = vt[j+voff];
        OXS_FFT_REAL_TYPE by = vt[j+voff+1];
        st[joff]              = ax + bx;
        st[joff+1]            = ay + by;
        st[joff+RTNFFTSIZE]   = ax - bx;
        st[joff+RTNFFTSIZE+1] = ay - by;
      }
    }

    // Second and third level butterflies, with
    // embedded bit-reversal
    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE* const st = scratch + j*RTNFFTSIZE;
      OXS_FFT_REAL_TYPE* const vt = va+j;
      // i = 0 through 3
      OXS_FFT_REAL_TYPE x0 = st[0];
      OXS_FFT_REAL_TYPE y0 = st[1];
      OXS_FFT_REAL_TYPE x2 = st[4];
      OXS_FFT_REAL_TYPE y2 = st[4+1];

      OXS_FFT_REAL_TYPE a0 = x0 + x2;
      OXS_FFT_REAL_TYPE a2 = x0 - x2;
      OXS_FFT_REAL_TYPE b0 = y0 + y2;
      OXS_FFT_REAL_TYPE b2 = y0 - y2;

      OXS_FFT_REAL_TYPE x1 = st[2];
      OXS_FFT_REAL_TYPE y1 = st[2+1];
      OXS_FFT_REAL_TYPE x3 = st[6];
      OXS_FFT_REAL_TYPE y3 = st[6+1];

      OXS_FFT_REAL_TYPE a1 = x1 + x3;
      OXS_FFT_REAL_TYPE a3 = x1 - x3;
      OXS_FFT_REAL_TYPE b1 = y1 + y3;
      OXS_FFT_REAL_TYPE b3 = y1 - y3;

      vt[0]                = a0 + a1;  // 0 -> 0
      vt[1]                = b0 + b1;
      vt[voff]             = a0 - a1;  // 1 -> 4
      vt[voff+1]           = b0 - b1;

      vt[     2*rstride]   = a2 - b3;  // 2 -> 2
      vt[     2*rstride+1] = b2 + a3;
      vt[voff+2*rstride]   = a2 + b3;  // 3 -> 6
      vt[voff+2*rstride+1] = b2 - a3;

      // i = 4 through 7
      OXS_FFT_REAL_TYPE x5 = st[10];
      OXS_FFT_REAL_TYPE y5 = st[10+1];
      OXS_FFT_REAL_TYPE x7 = st[14];
      OXS_FFT_REAL_TYPE y7 = st[14+1];

      OXS_FFT_REAL_TYPE a5 = x5 - y7;
      OXS_FFT_REAL_TYPE a7 = x5 + y7;

      OXS_FFT_REAL_TYPE b5 = y5 + x7;
      OXS_FFT_REAL_TYPE b7 = y5 - x7;

      OXS_FFT_REAL_TYPE cx = (a5-b5)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE cy = (a5+b5)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE dx = (a7-b7)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE dy = (a7+b7)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE x4 = st[8];
      OXS_FFT_REAL_TYPE y6 = st[12+1];

      OXS_FFT_REAL_TYPE a4 = x4 - y6;
      OXS_FFT_REAL_TYPE a6 = x4 + y6;

      OXS_FFT_REAL_TYPE y4 = st[8+1];
      OXS_FFT_REAL_TYPE x6 = st[12];

      OXS_FFT_REAL_TYPE b4 = y4 + x6;
      OXS_FFT_REAL_TYPE b6 = y4 - x6;

      vt[       rstride]    = a4 + cx; // 4 -> 1
      vt[       rstride+1]  = b4 + cy;
      vt[voff  +rstride]    = a4 - cx; // 5 -> 5
      vt[voff  +rstride+1]  = b4 - cy;
      vt[     3*rstride]    = a6 - dy; // 6 -> 3
      vt[     3*rstride+1]  = b6 + dx;
      vt[voff+3*rstride]    = a6 + dy; // 7 -> 7
      vt[voff+3*rstride+1]  = b6 - dx;
    }
  }

#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  OXS_FFT_REAL_TYPE const & ax = scratch[0];
  OXS_FFT_REAL_TYPE const & ay = scratch[1];
  OXS_FFT_REAL_TYPE const & bx = scratch[2];
  OXS_FFT_REAL_TYPE const & by = scratch[3];
  OXS_FFT_REAL_TYPE const & cx = scratch[4];
  OXS_FFT_REAL_TYPE const & cy = scratch[5];
  OXS_FFT_REAL_TYPE const & dx = scratch[6];
  OXS_FFT_REAL_TYPE const & dy = scratch[7];

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array
    for(j=0;j<csize_base;++j) {
      scratch[2*j]   = v[j*rstride];
      scratch[2*j+1] = v[j*rstride+1];
    }

    // Do butterflies plus embedded bit reversal
    OXS_FFT_REAL_TYPE A1x = ax + cx;
    OXS_FFT_REAL_TYPE B1x = ax - cx;
    OXS_FFT_REAL_TYPE A1y = ay + cy;
    OXS_FFT_REAL_TYPE B1y = ay - cy;

    OXS_FFT_REAL_TYPE E1x = ax + cy;
    OXS_FFT_REAL_TYPE T1x = ax - cy;
    OXS_FFT_REAL_TYPE E1y = ay - cx;
    OXS_FFT_REAL_TYPE T1y = ay + cx;

    OXS_FFT_REAL_TYPE A2x = bx + dx;
    OXS_FFT_REAL_TYPE B2x = bx - dx;
    OXS_FFT_REAL_TYPE A2y = by + dy;
    OXS_FFT_REAL_TYPE B2y = by - dy;

    OXS_FFT_REAL_TYPE E2x = bx + dy;
    OXS_FFT_REAL_TYPE T2x = bx - dy;
    OXS_FFT_REAL_TYPE E2y = by - dx;
    OXS_FFT_REAL_TYPE T2y = by + dx;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[4*rstride]    = A1x - A2x;   // 1 -> 4
    v[4*rstride+1]  = A1y - A2y;

    v[2*rstride]    = B1x + B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y - B2x;

    v[6*rstride]    = B1x - B2y;   // 3 -> 6
    v[6*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE bE2x = (E2y + E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2y - E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2y + T2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2y - T2x)*OXS_FFT_SQRT1_2;

    v[rstride]      = E1x + bE2x;   // 4 -> 1
    v[rstride+1]    = E1y + bE2y;

    v[5*rstride]    = E1x - bE2x;   // 5 -> 5
    v[5*rstride+1]  = E1y - bE2y;

    v[3*rstride]    = T1x + bT2y;   // 6 -> 3
    v[3*rstride+1]  = T1y - bT2x;

    v[7*rstride]    = T1x - bT2y;   // 7 -> 7
    v[7*rstride+1]  = T1y + bT2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    OXS_FFT_REAL_TYPE s1x = v[0] + v[4*rstride];
    OXS_FFT_REAL_TYPE d1x = v[0] - v[4*rstride];
    OXS_FFT_REAL_TYPE s1y = v[1] + v[4*rstride+1];
    OXS_FFT_REAL_TYPE d1y = v[1] - v[4*rstride+1];

    OXS_FFT_REAL_TYPE s3x = v[2*rstride]   + v[6*rstride];
    OXS_FFT_REAL_TYPE d3x = v[2*rstride]   - v[6*rstride];
    OXS_FFT_REAL_TYPE s3y = v[2*rstride+1] + v[6*rstride+1];
    OXS_FFT_REAL_TYPE d3y = v[2*rstride+1] - v[6*rstride+1];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE s2x = v[rstride]     + v[5*rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]     - v[5*rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1]   + v[5*rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1]   - v[5*rstride+1];

    OXS_FFT_REAL_TYPE s4x = v[3*rstride]   + v[7*rstride];
    OXS_FFT_REAL_TYPE d4x = v[3*rstride]   - v[7*rstride];
    OXS_FFT_REAL_TYPE s4y = v[3*rstride+1] + v[7*rstride+1];
    OXS_FFT_REAL_TYPE d4y = v[3*rstride+1] - v[7*rstride+1];

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[2*rstride]    = B1x - B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2x - E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2x + E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2x - T2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2x + T2y)*OXS_FFT_SQRT1_2;

    v[rstride]      = T1x + bT2x;   // 4 -> 1
    v[rstride+1]    = T1y + bT2y;

    v[3*rstride]    = E1x - bE2y;   // 6 -> 3
    v[3*rstride+1]  = E1y + bE2x;
  }
#undef RTNFFTSIZE
}
#endif // TRY4

#ifdef TRY5
void
Oxs_FFTStrided::ForwardFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8

  if(csize_base<=RTNFFTSIZE/2) {
    ForwardFFTSize8ZP(arr);
    return;
  }

  const OC_INDEX voff = rstride*(RTNFFTSIZE/2);

  for(OC_INDEX block=0;block<arrcount;block+=OFS_ARRAY_BLOCKSIZE) {
    const OC_INDEX bw = 2*(OFS_ARRAY_BLOCKSIZE>arrcount-block
                      ? arrcount-block : OFS_ARRAY_BLOCKSIZE);

    OXS_FFT_REAL_TYPE* const va = arr + 2*block;  // Working vectors
    OC_INDEX i,j;

    // Load scratch array while processing top-level butterfly
    // Assumes csize_base > RTNFFTSIZE/2.
    assert(csize_base > RTNFFTSIZE/2);
    for(i=0;i<csize_base-RTNFFTSIZE/2;++i) {
      OXS_FFT_REAL_TYPE* const st = scratch + 2*i;
      OXS_FFT_REAL_TYPE* const vt = va + i*rstride;
      for(j=0;j<bw;j+=2) {
        OC_INDEX joff = j*RTNFFTSIZE;
        OXS_FFT_REAL_TYPE ax = vt[j];
        OXS_FFT_REAL_TYPE ay = vt[j+1];
        OXS_FFT_REAL_TYPE bx = vt[j+voff];
        OXS_FFT_REAL_TYPE by = vt[j+voff+1];
        st[joff]              = ax + bx;
        st[joff+1]            = ay + by;
        st[joff+RTNFFTSIZE]   = ax - bx;
        st[joff+RTNFFTSIZE+1] = ay - by;
      }
    }
    while(i<RTNFFTSIZE/2) {
      OXS_FFT_REAL_TYPE* const st = scratch + 2*i;
      OXS_FFT_REAL_TYPE* const vt = va + i*rstride;
      for(j=0;j<bw;j+=2) {
        OC_INDEX joff = j*RTNFFTSIZE;
        st[joff]   = st[RTNFFTSIZE+joff]   = vt[j];
        st[joff+1] = st[RTNFFTSIZE+joff+1] = vt[j+1];
      }
      ++i;
    }

    // Second and third level butterflies, with
    // embedded bit-reversal
    for(j=0;j<bw;j+=2) {
      OXS_FFT_REAL_TYPE* const st = scratch + j*RTNFFTSIZE;
      OXS_FFT_REAL_TYPE* const vt = va+j;
      // i = 0 through 3
      OXS_FFT_REAL_TYPE x0 = st[0];
      OXS_FFT_REAL_TYPE y0 = st[1];
      OXS_FFT_REAL_TYPE x2 = st[4];
      OXS_FFT_REAL_TYPE y2 = st[4+1];

      OXS_FFT_REAL_TYPE a0 = x0 + x2;
      OXS_FFT_REAL_TYPE a2 = x0 - x2;
      OXS_FFT_REAL_TYPE b0 = y0 + y2;
      OXS_FFT_REAL_TYPE b2 = y0 - y2;

      OXS_FFT_REAL_TYPE x1 = st[2];
      OXS_FFT_REAL_TYPE y1 = st[2+1];
      OXS_FFT_REAL_TYPE x3 = st[6];
      OXS_FFT_REAL_TYPE y3 = st[6+1];

      OXS_FFT_REAL_TYPE a1 = x1 + x3;
      OXS_FFT_REAL_TYPE a3 = x1 - x3;
      OXS_FFT_REAL_TYPE b1 = y1 + y3;
      OXS_FFT_REAL_TYPE b3 = y1 - y3;

      vt[0]                = a0 + a1;  // 0 -> 0
      vt[1]                = b0 + b1;
      vt[voff]             = a0 - a1;  // 1 -> 4
      vt[voff+1]           = b0 - b1;

      vt[     2*rstride]   = a2 + b3;  // 2 -> 2
      vt[     2*rstride+1] = b2 - a3;
      vt[voff+2*rstride]   = a2 - b3;  // 3 -> 6
      vt[voff+2*rstride+1] = b2 + a3;

      // i = 4 through 7
      OXS_FFT_REAL_TYPE x5 = st[10];
      OXS_FFT_REAL_TYPE y5 = st[10+1];
      OXS_FFT_REAL_TYPE x7 = st[14];
      OXS_FFT_REAL_TYPE y7 = st[14+1];

      OXS_FFT_REAL_TYPE a5 = x5 + y7;
      OXS_FFT_REAL_TYPE a7 = x5 - y7;

      OXS_FFT_REAL_TYPE b5 = y5 - x7;
      OXS_FFT_REAL_TYPE b7 = y5 + x7;

      OXS_FFT_REAL_TYPE cx = (b5+a5)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE cy = (b5-a5)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE dx = (b7+a7)*OXS_FFT_SQRT1_2;
      OXS_FFT_REAL_TYPE dy = (b7-a7)*OXS_FFT_SQRT1_2;

      OXS_FFT_REAL_TYPE x4 = st[8];
      OXS_FFT_REAL_TYPE y6 = st[12+1];

      OXS_FFT_REAL_TYPE a4 = x4 + y6;
      OXS_FFT_REAL_TYPE a6 = x4 - y6;

      OXS_FFT_REAL_TYPE y4 = st[8+1];
      OXS_FFT_REAL_TYPE x6 = st[12];

      OXS_FFT_REAL_TYPE b4 = y4 - x6;
      OXS_FFT_REAL_TYPE b6 = y4 + x6;

      vt[       rstride]    = a4 + cx; // 4 -> 1
      vt[       rstride+1]  = b4 + cy;
      vt[voff  +rstride]    = a4 - cx; // 5 -> 5
      vt[voff  +rstride+1]  = b4 - cy;
      vt[     3*rstride]    = a6 + dy; // 6 -> 3
      vt[     3*rstride+1]  = b6 - dx;
      vt[voff+3*rstride]    = a6 - dy; // 7 -> 7
      vt[voff+3*rstride+1]  = b6 + dx;
    }
  }

#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  const OC_INDEX voff = rstride*(RTNFFTSIZE/2);

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Do upper two levels of butterflies
    OXS_FFT_REAL_TYPE s1x = v[0] + v[voff];
    OXS_FFT_REAL_TYPE d1x = v[0] - v[voff];
    OXS_FFT_REAL_TYPE s1y = v[1] + v[voff+1];
    OXS_FFT_REAL_TYPE d1y = v[1] - v[voff+1];

    OXS_FFT_REAL_TYPE s3x = v[2*rstride]   + v[voff+2*rstride];
    OXS_FFT_REAL_TYPE d3x = v[2*rstride]   - v[voff+2*rstride];
    OXS_FFT_REAL_TYPE s3y = v[2*rstride+1] + v[voff+2*rstride+1];
    OXS_FFT_REAL_TYPE d3y = v[2*rstride+1] - v[voff+2*rstride+1];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE s2x = v[rstride]   + v[voff+rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]   - v[voff+rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1] + v[voff+rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1] - v[voff+rstride+1];

    OXS_FFT_REAL_TYPE s4x = v[3*rstride]   + v[voff+3*rstride];
    OXS_FFT_REAL_TYPE d4x = v[3*rstride]   - v[voff+3*rstride];
    OXS_FFT_REAL_TYPE s4y = v[3*rstride+1] + v[voff+3*rstride+1];
    OXS_FFT_REAL_TYPE d4y = v[3*rstride+1] - v[voff+3*rstride+1];

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]                 = A1x + A2x;   // 0 -> 0
    v[1]                 = A1y + A2y;

    v[voff]              = A1x - A2x;   // 1 -> 4
    v[voff+1]            = A1y - A2y;

    v[2*rstride]         = B1x - B2y;   // 2 -> 2
    v[2*rstride+1]       = B1y + B2x;

    v[voff+2*rstride]    = B1x + B2y;   // 3 -> 6
    v[voff+2*rstride+1]  = B1y - B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2x - E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2x + E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2x - T2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2x + T2y)*OXS_FFT_SQRT1_2;

    // Do bottom level butterfly and write back to
    // v, with embedded bit reversal
    v[rstride]           = T1x + bT2x;   // 4 -> 1
    v[rstride+1]         = T1y + bT2y;

    v[voff+rstride]      = T1x - bT2x;   // 5 -> 5
    v[voff+rstride+1]    = T1y - bT2y;

    v[3*rstride]         = E1x - bE2y;   // 6 -> 3
    v[3*rstride+1]       = E1y + bE2x;

    v[voff+3*rstride]    = E1x + bE2y;   // 7 -> 7
    v[voff+3*rstride+1]  = E1y - bE2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  OXS_FFT_REAL_TYPE const & ax = scratch[0];
  OXS_FFT_REAL_TYPE const & ay = scratch[1];
  OXS_FFT_REAL_TYPE const & bx = scratch[2];
  OXS_FFT_REAL_TYPE const & by = scratch[3];
  OXS_FFT_REAL_TYPE const & cx = scratch[4];
  OXS_FFT_REAL_TYPE const & cy = scratch[5];
  OXS_FFT_REAL_TYPE const & dx = scratch[6];
  OXS_FFT_REAL_TYPE const & dy = scratch[7];

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array
    for(j=0;j<csize_base;++j) {
      scratch[2*j]   = v[j*rstride];
      scratch[2*j+1] = v[j*rstride+1];
    }

    // Do butterflies plus embedded bit reversal
    OXS_FFT_REAL_TYPE A1x = ax + cx;
    OXS_FFT_REAL_TYPE B1x = ax - cx;
    OXS_FFT_REAL_TYPE A1y = ay + cy;
    OXS_FFT_REAL_TYPE B1y = ay - cy;

    OXS_FFT_REAL_TYPE E1x = ax + cy;
    OXS_FFT_REAL_TYPE T1x = ax - cy;
    OXS_FFT_REAL_TYPE E1y = ay - cx;
    OXS_FFT_REAL_TYPE T1y = ay + cx;

    OXS_FFT_REAL_TYPE A2x = bx + dx;
    OXS_FFT_REAL_TYPE B2x = bx - dx;
    OXS_FFT_REAL_TYPE A2y = by + dy;
    OXS_FFT_REAL_TYPE B2y = by - dy;

    OXS_FFT_REAL_TYPE E2x = bx + dy;
    OXS_FFT_REAL_TYPE T2x = bx - dy;
    OXS_FFT_REAL_TYPE E2y = by - dx;
    OXS_FFT_REAL_TYPE T2y = by + dx;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[4*rstride]    = A1x - A2x;   // 1 -> 4
    v[4*rstride+1]  = A1y - A2y;

    v[2*rstride]    = B1x + B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y - B2x;

    v[6*rstride]    = B1x - B2y;   // 3 -> 6
    v[6*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE bE2x = (E2y + E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2y - E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2y + T2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2y - T2x)*OXS_FFT_SQRT1_2;

    v[rstride]      = E1x + bE2x;   // 4 -> 1
    v[rstride+1]    = E1y + bE2y;

    v[5*rstride]    = E1x - bE2x;   // 5 -> 5
    v[5*rstride+1]  = E1y - bE2y;

    v[3*rstride]    = T1x + bT2y;   // 6 -> 3
    v[3*rstride+1]  = T1y - bT2x;

    v[7*rstride]    = T1x - bT2y;   // 7 -> 7
    v[7*rstride+1]  = T1y + bT2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    OXS_FFT_REAL_TYPE s1x = v[0] + v[4*rstride];
    OXS_FFT_REAL_TYPE d1x = v[0] - v[4*rstride];
    OXS_FFT_REAL_TYPE s1y = v[1] + v[4*rstride+1];
    OXS_FFT_REAL_TYPE d1y = v[1] - v[4*rstride+1];

    OXS_FFT_REAL_TYPE s3x = v[2*rstride]   + v[6*rstride];
    OXS_FFT_REAL_TYPE d3x = v[2*rstride]   - v[6*rstride];
    OXS_FFT_REAL_TYPE s3y = v[2*rstride+1] + v[6*rstride+1];
    OXS_FFT_REAL_TYPE d3y = v[2*rstride+1] - v[6*rstride+1];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE s2x = v[rstride]     + v[5*rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]     - v[5*rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1]   + v[5*rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1]   - v[5*rstride+1];

    OXS_FFT_REAL_TYPE s4x = v[3*rstride]   + v[7*rstride];
    OXS_FFT_REAL_TYPE d4x = v[3*rstride]   - v[7*rstride];
    OXS_FFT_REAL_TYPE s4y = v[3*rstride+1] + v[7*rstride+1];
    OXS_FFT_REAL_TYPE d4y = v[3*rstride+1] - v[7*rstride+1];

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[2*rstride]    = B1x - B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2x - E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2x + E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2x - T2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2x + T2y)*OXS_FFT_SQRT1_2;

    v[rstride]      = T1x + bT2x;   // 4 -> 1
    v[rstride+1]    = T1y + bT2y;

    v[3*rstride]    = E1x - bE2y;   // 6 -> 3
    v[3*rstride+1]  = E1y + bE2x;
  }
#undef RTNFFTSIZE
}
#endif // TRY5

#ifdef TRY6
void
Oxs_FFTStrided::ForwardFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8

  if(csize_base<=RTNFFTSIZE/2) {
    ForwardFFTSize8ZP(arr);
    return;
  }

  OXS_FFT_REAL_TYPE const & s1x = scratch[0];
  OXS_FFT_REAL_TYPE const & s1y = scratch[1];
  OXS_FFT_REAL_TYPE const & s2x = scratch[2];
  OXS_FFT_REAL_TYPE const & s2y = scratch[3];
  OXS_FFT_REAL_TYPE const & s3x = scratch[4];
  OXS_FFT_REAL_TYPE const & s3y = scratch[5];
  OXS_FFT_REAL_TYPE const & s4x = scratch[6];
  OXS_FFT_REAL_TYPE const & s4y = scratch[7];

  OXS_FFT_REAL_TYPE const & d1x = scratch[8];
  OXS_FFT_REAL_TYPE const & d1y = scratch[9];
  OXS_FFT_REAL_TYPE const & d2x = scratch[10];
  OXS_FFT_REAL_TYPE const & d2y = scratch[11];
  OXS_FFT_REAL_TYPE const & d3x = scratch[12];
  OXS_FFT_REAL_TYPE const & d3y = scratch[13];
  OXS_FFT_REAL_TYPE const & d4x = scratch[14];
  OXS_FFT_REAL_TYPE const & d4y = scratch[15];

  const OC_INDEX voff = rstride*(RTNFFTSIZE/2);

  for(OC_INDEX i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array while processing top-level butterfly
    // Assumes csize_base > RTNFFTSIZE/2.
    assert(csize_base > RTNFFTSIZE/2);
    OC_INDEX j;
    for(j=0;j<csize_base-RTNFFTSIZE/2;++j) {
      OXS_FFT_REAL_TYPE ax = v[j*rstride];
      OXS_FFT_REAL_TYPE ay = v[j*rstride+1];
      OXS_FFT_REAL_TYPE bx = v[j*rstride+voff];
      OXS_FFT_REAL_TYPE by = v[j*rstride+voff+1];
      scratch[2*j]              = ax + bx;
      scratch[2*j+1]            = ay + by;
      scratch[2*j+RTNFFTSIZE]   = ax - bx;
      scratch[2*j+RTNFFTSIZE+1] = ay - by;
    }
    while(j<RTNFFTSIZE/2) {
      scratch[2*j]   = scratch[2*j+RTNFFTSIZE]   = v[j*rstride];
      scratch[2*j+1] = scratch[2*j+RTNFFTSIZE+1] = v[j*rstride+1];
      ++j;
    }

    OXS_FFT_REAL_TYPE* const vb = v + voff;  // Working vector

    // Do second and third level of butterflies,
    // and write out with embedded bit-reversal
    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]             = A1x + A2x;   // 0 -> 0
    v[1]             = A1y + A2y;
    vb[0]            = A1x - A2x;   // 1 -> 4
    vb[1]            = A1y - A2y;

    v[2*rstride]     = B1x + B2y;   // 2 -> 2
    v[2*rstride+1]   = B1y - B2x;
    vb[2*rstride]    = B1x - B2y;   // 3 -> 6
    vb[2*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2y + E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2y - E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2y + T2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2y - T2x)*OXS_FFT_SQRT1_2;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    v[rstride]       = E1x + bE2x;   // 4 -> 1
    v[rstride+1]     = E1y + bE2y;

    vb[rstride]      = E1x - bE2x;   // 5 -> 5
    vb[rstride+1]    = E1y - bE2y;

    v[3*rstride]     = T1x + bT2y;   // 6 -> 3
    v[3*rstride+1]   = T1y - bT2x;

    vb[3*rstride]    = T1x - bT2y;   // 7 -> 7
    vb[3*rstride+1]  = T1y + bT2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  const OC_INDEX voff = rstride*(RTNFFTSIZE/2);

  for(OC_INDEX i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector
    OXS_FFT_REAL_TYPE* const vb = arr + 2*i + voff;

    // Do upper two levels of butterflies
    OXS_FFT_REAL_TYPE s1x = v[0] + vb[0];
    OXS_FFT_REAL_TYPE d1x = v[0] - vb[0];
    OXS_FFT_REAL_TYPE s1y = v[1] + vb[1];
    OXS_FFT_REAL_TYPE d1y = v[1] - vb[1];

    OXS_FFT_REAL_TYPE s3x = v[2*rstride]   + vb[2*rstride];
    OXS_FFT_REAL_TYPE d3x = v[2*rstride]   - vb[2*rstride];
    OXS_FFT_REAL_TYPE s3y = v[2*rstride+1] + vb[2*rstride+1];
    OXS_FFT_REAL_TYPE d3y = v[2*rstride+1] - vb[2*rstride+1];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE s2x = v[rstride]   + vb[rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]   - vb[rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1] + vb[rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1] - vb[rstride+1];

    OXS_FFT_REAL_TYPE s4x = v[3*rstride]   + vb[3*rstride];
    OXS_FFT_REAL_TYPE d4x = v[3*rstride]   - vb[3*rstride];
    OXS_FFT_REAL_TYPE s4y = v[3*rstride+1] + vb[3*rstride+1];
    OXS_FFT_REAL_TYPE d4y = v[3*rstride+1] - vb[3*rstride+1];

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]             = A1x + A2x;   // 0 -> 0
    v[1]             = A1y + A2y;

    vb[0]            = A1x - A2x;   // 1 -> 4
    vb[1]            = A1y - A2y;

    v[2*rstride]     = B1x - B2y;   // 2 -> 2
    v[2*rstride+1]   = B1y + B2x;

    vb[2*rstride]    = B1x + B2y;   // 3 -> 6
    vb[2*rstride+1]  = B1y - B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2x - E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2x + E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2x - T2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2x + T2y)*OXS_FFT_SQRT1_2;

    // Do bottom level butterfly and write back to
    // v, with embedded bit reversal
    v[rstride]       = T1x + bT2x;   // 4 -> 1
    v[rstride+1]     = T1y + bT2y;

    vb[rstride]      = T1x - bT2x;   // 5 -> 5
    vb[rstride+1]    = T1y - bT2y;

    v[3*rstride]     = E1x - bE2y;   // 6 -> 3
    v[3*rstride+1]   = E1y + bE2x;

    vb[3*rstride]    = E1x + bE2y;   // 7 -> 7
    vb[3*rstride+1]  = E1y - bE2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i,j;

  OXS_FFT_REAL_TYPE const & ax = scratch[0];
  OXS_FFT_REAL_TYPE const & ay = scratch[1];
  OXS_FFT_REAL_TYPE const & bx = scratch[2];
  OXS_FFT_REAL_TYPE const & by = scratch[3];
  OXS_FFT_REAL_TYPE const & cx = scratch[4];
  OXS_FFT_REAL_TYPE const & cy = scratch[5];
  OXS_FFT_REAL_TYPE const & dx = scratch[6];
  OXS_FFT_REAL_TYPE const & dy = scratch[7];

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array
    for(j=0;j<csize_base;++j) {
      scratch[2*j]   = v[j*rstride];
      scratch[2*j+1] = v[j*rstride+1];
    }

    // Do butterflies plus embedded bit reversal
    OXS_FFT_REAL_TYPE A1x = ax + cx;
    OXS_FFT_REAL_TYPE B1x = ax - cx;
    OXS_FFT_REAL_TYPE A1y = ay + cy;
    OXS_FFT_REAL_TYPE B1y = ay - cy;

    OXS_FFT_REAL_TYPE E1x = ax + cy;
    OXS_FFT_REAL_TYPE T1x = ax - cy;
    OXS_FFT_REAL_TYPE E1y = ay - cx;
    OXS_FFT_REAL_TYPE T1y = ay + cx;

    OXS_FFT_REAL_TYPE A2x = bx + dx;
    OXS_FFT_REAL_TYPE B2x = bx - dx;
    OXS_FFT_REAL_TYPE A2y = by + dy;
    OXS_FFT_REAL_TYPE B2y = by - dy;

    OXS_FFT_REAL_TYPE E2x = bx + dy;
    OXS_FFT_REAL_TYPE T2x = bx - dy;
    OXS_FFT_REAL_TYPE E2y = by - dx;
    OXS_FFT_REAL_TYPE T2y = by + dx;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[4*rstride]    = A1x - A2x;   // 1 -> 4
    v[4*rstride+1]  = A1y - A2y;

    v[2*rstride]    = B1x + B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y - B2x;

    v[6*rstride]    = B1x - B2y;   // 3 -> 6
    v[6*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE bE2x = (E2y + E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2y - E2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2y + T2x)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2y - T2x)*OXS_FFT_SQRT1_2;

    v[rstride]      = E1x + bE2x;   // 4 -> 1
    v[rstride+1]    = E1y + bE2y;

    v[5*rstride]    = E1x - bE2x;   // 5 -> 5
    v[5*rstride+1]  = E1y - bE2y;

    v[3*rstride]    = T1x + bT2y;   // 6 -> 3
    v[3*rstride+1]  = T1y - bT2x;

    v[7*rstride]    = T1x - bT2y;   // 7 -> 7
    v[7*rstride+1]  = T1y + bT2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize8ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 8
  OC_INDEX i;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    OXS_FFT_REAL_TYPE s1x = v[0] + v[4*rstride];
    OXS_FFT_REAL_TYPE d1x = v[0] - v[4*rstride];
    OXS_FFT_REAL_TYPE s1y = v[1] + v[4*rstride+1];
    OXS_FFT_REAL_TYPE d1y = v[1] - v[4*rstride+1];

    OXS_FFT_REAL_TYPE s3x = v[2*rstride]   + v[6*rstride];
    OXS_FFT_REAL_TYPE d3x = v[2*rstride]   - v[6*rstride];
    OXS_FFT_REAL_TYPE s3y = v[2*rstride+1] + v[6*rstride+1];
    OXS_FFT_REAL_TYPE d3y = v[2*rstride+1] - v[6*rstride+1];

    OXS_FFT_REAL_TYPE A1x = s1x + s3x;
    OXS_FFT_REAL_TYPE B1x = s1x - s3x;
    OXS_FFT_REAL_TYPE A1y = s1y + s3y;
    OXS_FFT_REAL_TYPE B1y = s1y - s3y;

    OXS_FFT_REAL_TYPE E1x = d1x + d3y;
    OXS_FFT_REAL_TYPE T1x = d1x - d3y;
    OXS_FFT_REAL_TYPE E1y = d1y - d3x;
    OXS_FFT_REAL_TYPE T1y = d1y + d3x;

    OXS_FFT_REAL_TYPE s2x = v[rstride]     + v[5*rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]     - v[5*rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1]   + v[5*rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1]   - v[5*rstride+1];

    OXS_FFT_REAL_TYPE s4x = v[3*rstride]   + v[7*rstride];
    OXS_FFT_REAL_TYPE d4x = v[3*rstride]   - v[7*rstride];
    OXS_FFT_REAL_TYPE s4y = v[3*rstride+1] + v[7*rstride+1];
    OXS_FFT_REAL_TYPE d4y = v[3*rstride+1] - v[7*rstride+1];

    OXS_FFT_REAL_TYPE A2x = s2x + s4x;
    OXS_FFT_REAL_TYPE B2x = s2x - s4x;
    OXS_FFT_REAL_TYPE A2y = s2y + s4y;
    OXS_FFT_REAL_TYPE B2y = s2y - s4y;

    v[0]            = A1x + A2x;   // 0 -> 0
    v[1]            = A1y + A2y;

    v[2*rstride]    = B1x - B2y;   // 2 -> 2
    v[2*rstride+1]  = B1y + B2x;

    OXS_FFT_REAL_TYPE E2x = d2x + d4y;
    OXS_FFT_REAL_TYPE T2x = d2x - d4y;
    OXS_FFT_REAL_TYPE E2y = d2y - d4x;
    OXS_FFT_REAL_TYPE T2y = d2y + d4x;

    OXS_FFT_REAL_TYPE bE2x = (E2x - E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bE2y = (E2x + E2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2x = (T2x - T2y)*OXS_FFT_SQRT1_2;
    OXS_FFT_REAL_TYPE bT2y = (T2x + T2y)*OXS_FFT_SQRT1_2;

    v[rstride]      = T1x + bT2x;   // 4 -> 1
    v[rstride+1]    = T1y + bT2y;

    v[3*rstride]    = E1x - bE2y;   // 6 -> 3
    v[3*rstride+1]  = E1y + bE2x;
  }
#undef RTNFFTSIZE
}
#endif // TRY6

void
Oxs_FFTStrided::ForwardFFTSize4
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 4
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array
    for(j=0;j<csize_base;++j) {
      scratch[2*j]   = v[j*rstride];
      scratch[2*j+1] = v[j*rstride+1];
    }

    OXS_FFT_REAL_TYPE s1x = scratch[0] + scratch[4];
    OXS_FFT_REAL_TYPE d1x = scratch[0] - scratch[4];
    OXS_FFT_REAL_TYPE s1y = scratch[1] + scratch[5];
    OXS_FFT_REAL_TYPE d1y = scratch[1] - scratch[5];

    OXS_FFT_REAL_TYPE s2x = scratch[2] + scratch[6];
    OXS_FFT_REAL_TYPE d2x = scratch[2] - scratch[6];
    OXS_FFT_REAL_TYPE s2y = scratch[3] + scratch[7];
    OXS_FFT_REAL_TYPE d2y = scratch[3] - scratch[7];


    v[0]            = s1x + s2x;
    v[1]            = s1y + s2y;

    v[rstride]      = d1x + d2y;
    v[rstride+1]    = d1y - d2x;

    v[2*rstride]    = s1x - s2x;
    v[2*rstride+1]  = s1y - s2y;

    v[3*rstride]    = d1x - d2y;
    v[3*rstride+1]  = d1y + d2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize4
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 4
  OC_INDEX i;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    OXS_FFT_REAL_TYPE s1x = v[0] + v[2*rstride];
    OXS_FFT_REAL_TYPE d1x = v[0] - v[2*rstride];
    OXS_FFT_REAL_TYPE s1y = v[1] + v[2*rstride+1];
    OXS_FFT_REAL_TYPE d1y = v[1] - v[2*rstride+1];

    OXS_FFT_REAL_TYPE s2x = v[rstride]   + v[3*rstride];
    OXS_FFT_REAL_TYPE d2x = v[rstride]   - v[3*rstride];
    OXS_FFT_REAL_TYPE s2y = v[rstride+1] + v[3*rstride+1];
    OXS_FFT_REAL_TYPE d2y = v[rstride+1] - v[3*rstride+1];

    v[0]            = s1x + s2x;
    v[1]            = s1y + s2y;

    v[rstride]      = d1x - d2y;
    v[rstride+1]    = d1y + d2x;

    v[2*rstride]    = s1x - s2x;
    v[2*rstride+1]  = s1y - s2y;

    v[3*rstride]    = d1x + d2y;
    v[3*rstride+1]  = d1y - d2x;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::ForwardFFTSize4ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 4
  OC_INDEX i,j;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector

    // Load scratch array
    for(j=0;j<csize_base;++j) {
      scratch[2*j]   = v[j*rstride];
      scratch[2*j+1] = v[j*rstride+1];
    }

    OXS_FFT_REAL_TYPE const & ax = scratch[0];
    OXS_FFT_REAL_TYPE const & ay = scratch[1];
    OXS_FFT_REAL_TYPE const & bx = scratch[2];
    OXS_FFT_REAL_TYPE const & by = scratch[3];

    v[0]            = ax + bx;
    v[1]            = ay + by;
    v[rstride]      = ax + by;
    v[rstride+1]    = ay - bx;
    v[2*rstride]    = ax - bx;
    v[2*rstride+1]  = ay - by;
    v[3*rstride]    = ax - by;
    v[3*rstride+1]  = ay + bx;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::InverseFFTSize4ZP
(OXS_FFT_REAL_TYPE* arr) const
{
#define RTNFFTSIZE 4
  OC_INDEX i;

  for(i=0;i<arrcount;++i) {
    OXS_FFT_REAL_TYPE* const v = arr + 2*i;  // Working vector
                 
    OXS_FFT_REAL_TYPE ax  = v[0] + v[2*rstride]   + v[rstride]   + v[3*rstride];
    OXS_FFT_REAL_TYPE bx  = v[0] - v[2*rstride]   - v[rstride+1] + v[3*rstride+1];
    OXS_FFT_REAL_TYPE ay  = v[1] + v[2*rstride+1] + v[rstride+1] + v[3*rstride+1];
    OXS_FFT_REAL_TYPE by  = v[1] - v[2*rstride+1] + v[rstride]   - v[3*rstride];

    v[0]         = ax;
    v[1]         = ay;
    v[rstride]   = bx;
    v[rstride+1] = by;
  }
#undef RTNFFTSIZE
}

void
Oxs_FFTStrided::FFTSize2
(OXS_FFT_REAL_TYPE* arr) const
{ // Same for forward and inverse.  *Assumes* csize_base==2.
  assert(csize_base == 2);
  OXS_FFT_REAL_TYPE* const v = arr;
  const OC_INDEX istop = 2*arrcount;
  OC_INDEX i=0;
  if(arrcount%2) {
    OXS_FFT_REAL_TYPE ax = v[0];
    OXS_FFT_REAL_TYPE ay = v[1];
    OXS_FFT_REAL_TYPE bx = v[rstride];
    OXS_FFT_REAL_TYPE by = v[rstride+1];
    v[0]         = ax + bx;
    v[1]         = ay + by;
    v[rstride]   = ax - bx;
    v[rstride+1] = ay - by;
    i+=2;
  }
  while(i<istop) {
    OXS_FFT_REAL_TYPE ax = v[i];
    OXS_FFT_REAL_TYPE ay = v[i+1];
    OXS_FFT_REAL_TYPE cx = v[i+2];
    OXS_FFT_REAL_TYPE cy = v[i+3];

    OXS_FFT_REAL_TYPE bx = v[i+rstride];
    OXS_FFT_REAL_TYPE by = v[i+rstride+1];
    OXS_FFT_REAL_TYPE dx = v[i+rstride+2];
    OXS_FFT_REAL_TYPE dy = v[i+rstride+3];

    v[i]           = ax + bx;
    v[i+1]         = ay + by;
    v[i+2]         = cx + dx;
    v[i+3]         = cy + dy;

    v[i+rstride]   = ax - bx;
    v[i+rstride+1] = ay - by;
    v[i+rstride+2] = cx - dx;
    v[i+rstride+3] = cy - dy;

    i+=4;
  }
}

void
Oxs_FFTStrided::ForwardFFTSize2ZP
(OXS_FFT_REAL_TYPE* arr) const
{ // Assumes csize_base == 1
  assert(csize_base == 1);
  memcpy(arr+rstride,arr,2*arrcount*sizeof(OXS_FFT_REAL_TYPE));
}

void
Oxs_FFTStrided::InverseFFTSize2ZP
(OXS_FFT_REAL_TYPE* arr) const
{ // Assumes csize_base == 1
  assert(csize_base == 1);
  OXS_FFT_REAL_TYPE* const v = arr;
  for(OC_INDEX i=0;i<arrcount;++i) {
    v[2*i]   += v[2*i+rstride];
    v[2*i+1] += v[2*i+rstride+1];
  }
}

void
Oxs_FFTStrided::FFTNop
(OXS_FFT_REAL_TYPE* /* arr */) const
{} // NOP


////////////////////////////////////////////////////////////////////////
// Class Oxs_FFT3DThreeVector //////////////////////////////////////////
////////////////////////////////////////////////////////////////////////

/* Class Oxs_FFT3DThreeVector
 *
 * Class for real-to-complex and complex-to-real FFT of a 3D array
 * of three vectors.
 *
 */

void
Oxs_FFT3DThreeVector::RecommendDimensions
(OC_INDEX rdim1,OC_INDEX rdim2,OC_INDEX rdim3,
 OC_INDEX& cdim1,OC_INDEX& cdim2,OC_INDEX& cdim3)
{
  OC_INDEX csize1 = Oxs_FFT1DThreeVector::RecommendSize(rdim1);
  OC_INDEX csize2 = Oxs_FFTStrided::RecommendSize(rdim2);
  OC_INDEX csize3 = Oxs_FFTStrided::RecommendSize(rdim3);
  // Note to self: Should probably refactor the Oxs_FFT1DThreeVector
  // and Oxs_FFTStrided classes to have RecommendDimensions rather
  // than RecommendSize, because the cdim1 computation below relies on
  // effectively "private" knowledge about the relationship between
  // csize1 and cdim1.
  cdim1 = (csize1/2)+1; // Integer truncation handles csize1=1 case.
  cdim2 = csize2;
  cdim3 = csize3;
}

#if !OC_INDEX_CHECKS && OC_INT4m_WIDTH != OC_INDEX_WIDTH
// Wrapper for backward compatibility
void
Oxs_FFT3DThreeVector::RecommendDimensions
(OC_INT4m rdim1,OC_INT4m rdim2,OC_INT4m rdim3,
 OC_INT4m& cdim1,OC_INT4m& cdim2,OC_INT4m& cdim3)
{
  OC_INDEX t1,t2,t3;
  RecommendDimensions(rdim1,rdim2,rdim3,t1,t2,t3);
  cdim1 = OC_INT4m(t1);  cdim2 = OC_INT4m(t2);  cdim3 = OC_INT4m(t3);
  if(cdim1 != t1 || cdim2 != t2 || cdim3 != t3) {
    // OC_INT4m can hold values up to 2^31-1, so this branch shouldn't
    // happen.
    OXS_THROW(Oxs_BadParameter,
              "OC_INT4m overflow in Oxs_FFT3DThreeVector::RecommendDimensions().");
  }
}
#endif

Oxs_FFT3DThreeVector::Oxs_FFT3DThreeVector()
  : rdim1(0), rdim2(0), rdim3(0),
    cdim1(0), cdim2(0), cdim3(0),
    rxydim(0), cxydim_rs(0)
{}

Oxs_FFT3DThreeVector::~Oxs_FFT3DThreeVector()
{}

void Oxs_FFT3DThreeVector::SetDimensions
(OC_INDEX in_rdim1,OC_INDEX in_rdim2,OC_INDEX in_rdim3,
 OC_INDEX in_cdim1,OC_INDEX in_cdim2,OC_INDEX in_cdim3)
{
  rdim1 = in_rdim1;
  rdim2 = in_rdim2;
  rdim3 = in_rdim3;
  cdim1 = in_cdim1;
  cdim2 = in_cdim2;
  cdim3 = in_cdim3;

  rxydim = OFTV_VECSIZE*rdim1*rdim2;
  cxydim_rs = OFTV_COMPLEXSIZE*OFTV_VECSIZE*cdim1*cdim2;

  fftx.SetDimensions(rdim1, (cdim1==1 ? 1 : 2*(cdim1-1)), rdim2);
  ffty.SetDimensions(rdim2, cdim2,
		     OFTV_COMPLEXSIZE*OFTV_VECSIZE*cdim1,
		     OFTV_VECSIZE*cdim1);
  fftz.SetDimensions(rdim3, cdim3,
		     OFTV_COMPLEXSIZE*OFTV_VECSIZE*cdim1*cdim2,
		     OFTV_VECSIZE*cdim1*cdim2);
}

void Oxs_FFT3DThreeVector::AdjustInputDimensions
(OC_INDEX new_rdim1,OC_INDEX new_rdim2,OC_INDEX new_rdim3)
{
  fftx.AdjustInputDimensions(new_rdim1,new_rdim2);
  ffty.AdjustInputDimensions(new_rdim2,
			     OFTV_COMPLEXSIZE*OFTV_VECSIZE*cdim1,
			     OFTV_VECSIZE*cdim1);
  fftz.AdjustInputDimensions(new_rdim3,
			     OFTV_COMPLEXSIZE*OFTV_VECSIZE*cdim1*cdim2,
			     OFTV_VECSIZE*cdim1*cdim2);

  rdim1 = new_rdim1;
  rdim2 = new_rdim2;
  rdim3 = new_rdim3;

  rxydim = OFTV_VECSIZE*rdim1*rdim2;
}

void
Oxs_FFT3DThreeVector::ForwardRealToComplexFFT
(const OXS_FFT_REAL_TYPE* rarr_in,
 OXS_FFT_REAL_TYPE* carr_out) const
{
  OC_INDEX k;
  for(k=0;k<rdim3;++k) {
    // Dim1 transforms in plane k
    fftx.ForwardRealToComplexFFT(rarr_in+k*rxydim,carr_out+k*cxydim_rs);

    // Dim2 transforms in plane k
    ffty.ForwardFFT(carr_out+k*cxydim_rs);
  }
  fftz.ForwardFFT(carr_out); // Dim3 transforms
}

void
Oxs_FFT3DThreeVector::InverseComplexToRealFFT
(OXS_FFT_REAL_TYPE* carr_in,
 OXS_FFT_REAL_TYPE* rarr_out) const
{
  fftz.InverseFFT(carr_in); // Dim3 transforms

  OC_INDEX k;
  for(k=0;k<rdim3;++k) {
    // Dim2 transforms
    ffty.InverseFFT(carr_in+k*cxydim_rs);

    // Dim1 transforms
    fftx.InverseComplexToRealFFT(carr_in+k*cxydim_rs,rarr_out+k*rxydim);
  }
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

#ifdef STANDALONE
// Stand-alone code for testing and development

void Usage()
{
  fprintf(stderr,"Usage: fft3vtest N\n");
  exit(1);
}

#include <cctype>

int main(int argc,char** argv)
{
  if(argc!=2) Usage();
  OC_INDEX N = atoi(argv[1]);
  OC_INDEX M = Oxs_FFT1DThreeVector::RecommendSize(N);
#ifdef ZEROPAD
  M*=2;
#endif
  OC_INDEX rarr_size = 3*N;
  OC_INDEX carr_size = 3*((M/2)+1)*2;

  OXS_FFT_REAL_TYPE* orig = new OXS_FFT_REAL_TYPE[rarr_size];
  OXS_FFT_REAL_TYPE* rarr = new OXS_FFT_REAL_TYPE[rarr_size];
  OXS_FFT_REAL_TYPE* carr = new OXS_FFT_REAL_TYPE[carr_size];

  printf("carr_size: %ld\n",carr_size);

  Oxs_FFT1DThreeVector fftx;
  fftx.SetDimensions(N,M,1);

  Oxs_FFTStrided ffty,fftz;
  ffty.SetDimensions(1,1,6*M,3*M);
  fftz.SetDimensions(1,1,6*M,3*M);

  OXS_FFT_REAL_TYPE scale = fftx.GetScaling()
                           *ffty.GetScaling()*fftz.GetScaling();

  for(OC_INDEX i=0;i<rarr_size;++i) rarr[i] = 0.0; // Safety init

#if 0
  srand(13);
  for(OC_INDEX i=0;i<rarr_size;++i) {
    orig[i]= OXS_FFT_REAL_TYPE(rand())/OXS_FFT_REAL_TYPE(RAND_MAX);
    rarr[i] = scale*orig[i];
  }
#endif
#if 1
  OC_INDEX istart = 0;
  for(OC_INDEX i=istart;i<=N/2;++i) {
    OXS_FFT_REAL_TYPE x = i;
    x *= 2.0/OXS_FFT_REAL_TYPE(N);
    // At this point, 0 <= x <= 1
#if 1
    x *= 1000.0;
#else
    x *= N/2;
#endif
    if(x<1.0) x = 1.0;  // Bound inverse
    orig[3*i]= 1.0/fabs(x*x*x);
    rarr[3*i] = scale*orig[3*i];
    if(i>0) {
      orig[3*(N-i)] = orig[3*i];  // Make it even
      rarr[3*(N-i)] = rarr[3*i];
    }
    // 3*i+1 and 3*i+2 are zeroed during "safety" initialization
  }

#endif
#if 0
  OC_INDEX istart = 0;
  for(OC_INDEX i=istart;i<=N/2;++i) {
    OXS_FFT_REAL_TYPE x = i;
    x *= 2.0/OXS_FFT_REAL_TYPE(N);
    // At this point, 0 <= x <= 1
    x *= 4;
    orig[3*i]= exp(-1*x*x);
    rarr[3*i] = scale*orig[3*i];
    if(i>0) {
      orig[3*(N-i)] = orig[3*i];  // Make it even
      rarr[3*(N-i)] = rarr[3*i];
    }
    // 3*i+1 and 3*i+2 are zeroed during "safety" initialization
  }
#endif

#ifdef VERBOSE
  for(OC_INDEX i=0;i<rarr_size;i+=3) {
    printf("orig[%2d] = %#15.8g  %#15.8g %#15.8g\n",
           i/3,orig[i],orig[i+1],orig[i+2]);
  }
  printf("\n");
#endif

  fftx.ForwardRealToComplexFFT(rarr,carr);
  ffty.ForwardFFT(carr);
  fftz.ForwardFFT(carr);

#ifdef VERBOSE
  for(OC_INDEX i=0;i<carr_size;i+=6) {
    printf("carr[%2d] = "
           "(%#15.8g,%#15.8g) (%#15.8g,%#15.8g) (%#15.8g,%#15.8g)\n",
           i/6,carr[i],carr[i+1],carr[i+2],carr[i+3],carr[i+4],carr[i+5]);
  }
#endif

#ifdef WASH_IMAGINARY
  // Wash imaginary part.  Intended for use with even rarr,
  // since in that case the transform should be real.
  for(OC_INDEX i=1;i<carr_size;i+=2) {
    carr[i] = 0.0;
  }
  printf("Imaginary part of image set to zero.\n");
#endif // WASH_IMAGINARY

  fftz.InverseFFT(carr);
  ffty.InverseFFT(carr);
  fftx.InverseComplexToRealFFT(carr,rarr);

  OXS_FFT_REAL_TYPE max_error = -1;
  OC_INDEX index_max_error = -1;

  OXS_FFT_REAL_TYPE max_rel_error = -1;
  OXS_FFT_REAL_TYPE check_size = 1;
  OC_INDEX index_max_rel_error = -1;

  for(OC_INDEX i=0;i<rarr_size;++i) {
    OXS_FFT_REAL_TYPE error = fabs(rarr[i] - orig[i]);
    if(error>max_error) {
      max_error = error;
      index_max_error = i;
    }
    if(orig[i]==0.0) continue;
    if(error*check_size>max_rel_error*fabs(orig[i])) {
      max_rel_error = error;
      check_size = fabs(orig[i]);
      index_max_rel_error = i;
    }
  }
  printf("Max     error: @%9d : %25.15e + %25.15e\n",
         index_max_error,orig[index_max_error],
         rarr[index_max_error]-orig[index_max_error]);

  printf("Max rel error: @%9d : %25.15e + %25.15e\n",
         index_max_rel_error,orig[index_max_rel_error],
         rarr[index_max_rel_error]-orig[index_max_rel_error]);

  delete[] carr;
  delete[] rarr;
  delete[] orig;

  return 0;
}

#endif // STANDALONE
