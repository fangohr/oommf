/* FILE: demag.h            -*-Mode: c++-*-
 *
 * Average H demag field across rectangular cells.  This is a modified
 * version of the simpledemag class, which uses symmetries in the
 * interaction coefficients to reduce memory usage.
 *
 * This code uses the Oxs_FFT3v classes to perform direct FFTs of the
 * import magnetization ThreeVectors.  This Oxs_Demag class is a
 * drop-in replacement for an older Oxs_Demag class that used the
 * scalar Oxs_FFT class.  That older class has been renamed
 * Oxs_DemagOld, and is contained in the demagold.* files.
 *
 * There are two .cc files providing definitions for routines in the
 * header file: demag.cc and demag-threaded.cc.  The first is provides
 * a non-threaded implementation of the routines, the second a
 * threaded version.  Exactly one of the .cc files is compiled into
 * the oxs executable, depending on the setting of the compiler macro
 * OOMMF_THREADS.
 *
 */

#ifndef _OXS_DEMAG
#define _OXS_DEMAG

#include "oc.h"  // Includes OOMMF_THREADS macro in ocport.h
#include "energy.h"
#include "fft3v.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"

#if OOMMF_THREADS
# include "oxsthread.h"
#endif

/* End includes */

class Oxs_Demag
  : public Oxs_Energy, public Oxs_EnergyPreconditionerSupport {

  friend class Oxs_FFTLocker;
  friend class _Oxs_DemagFFTxThread;
  friend class _Oxs_DemagiFFTxDotThread;
  friend class _Oxs_DemagFFTyThread;
  friend class _Oxs_DemagFFTyConvolveThread;
  friend class _Oxs_DemagFFTzConvolveThread;
  friend class _Oxs_DemagFFTyzConvolveThread;
public:
  // Sun CC, Forte Developer 7 C++ 5.4 2002/03/09, complains inside
  // Oxs_FFTLocker about A_coefs not being accessible if this declaration
  // is inside private: block.  OK, fine...
  struct A_coefs {
    OXS_FFT_REAL_TYPE A00;
    OXS_FFT_REAL_TYPE A01;
    OXS_FFT_REAL_TYPE A02;
    OXS_FFT_REAL_TYPE A11;
    OXS_FFT_REAL_TYPE A12;
    OXS_FFT_REAL_TYPE A22;
  };

  // Sun CC, Forte Developer 7 C++ 5.4 2002/03/09, complains inside
  // Oxs_FFTLocker about Oxs_FFTLocker_Info not being accessible if
  // this declaration is inside private: block.  OK, fine...
  // Thread-specific data
  struct Oxs_FFTLocker_Info {
  public:
    OC_INDEX rdimx;
    OC_INDEX rdimy;
    OC_INDEX rdimz;
    OC_INDEX cdimx;
    OC_INDEX cdimy;
    OC_INDEX cdimz;
    OC_INDEX embed_block_size;
    OC_INDEX embed_yzblock_size;
    String name; // In use, we just set this to
    /// "InstanceName() + this_addr", so that each
    /// Oxs_Demag object gets a separate locker.
    /// See Oxs_Demag member function MakeLockerName().

    Oxs_FFTLocker_Info()
      : rdimx(0), rdimy(0), rdimz(0),
        cdimx(0), cdimy(0), cdimz(0),
        embed_block_size(0), embed_yzblock_size(0) {}

    Oxs_FFTLocker_Info(OC_INDEX in_rdimx,OC_INDEX in_rdimy,OC_INDEX in_rdimz,
                       OC_INDEX in_cdimx,OC_INDEX in_cdimy,OC_INDEX in_cdimz,
                       OC_INDEX in_embed_block_size,
                       OC_INDEX in_embed_yzblock_size,
                       const char* in_name)
      : rdimx(in_rdimx), rdimy(in_rdimy), rdimz(in_rdimz),
        cdimx(in_cdimx), cdimy(in_cdimy), cdimz(in_cdimz),
        embed_block_size(in_embed_block_size),
        embed_yzblock_size(in_embed_yzblock_size), name(in_name) {}
    void Set(OC_INDEX in_rdimx,OC_INDEX in_rdimy,OC_INDEX in_rdimz,
             OC_INDEX in_cdimx,OC_INDEX in_cdimy,OC_INDEX in_cdimz,
             OC_INDEX in_embed_block_size,
             OC_INDEX in_embed_yzblock_size,
             const String& in_name) {
      rdimx = in_rdimx;      rdimy = in_rdimy;      rdimz = in_rdimz;
      cdimx = in_cdimx;      cdimy = in_cdimy;      cdimz = in_cdimz;
      embed_block_size = in_embed_block_size;
      embed_yzblock_size = in_embed_yzblock_size;
      name = in_name;
    }
    // Default copy constructor and assignment operator are okay.
  };

private:
#if REPORT_TIME
  mutable Nb_StopWatch inittime;

  mutable Nb_StopWatch fftforwardtime;
  mutable Nb_StopWatch fftxforwardtime;
  mutable Nb_StopWatch fftyforwardtime;

  mutable Nb_StopWatch fftinversetime;
  mutable Nb_StopWatch fftxinversetime;
  mutable Nb_StopWatch fftyinversetime;

  mutable Nb_StopWatch convtime;
  mutable Nb_StopWatch dottime;

  enum { dvltimer_number = 10 };
  mutable Nb_StopWatch dvltimer[dvltimer_number];
#endif // REPORT_TIME

  mutable OC_INDEX rdimx; // Natural size of real data
  mutable OC_INDEX rdimy; // Digital Mars compiler wants these as separate
  mutable OC_INDEX rdimz; //    statements, because of "mutable" keyword.
  mutable OC_INDEX cdimx; // Full size of complex data
  mutable OC_INDEX cdimy;
  mutable OC_INDEX cdimz;
  // 2*(cdimx-1)>=rdimx, cdimy>=rdimy, cdimz>=rdimz
  // cdimx-1 and cdim[yz] should be powers of 2.
  mutable OC_INDEX adimx; // Dimensions of A## storage (see below).
  mutable OC_INDEX adimy;
  mutable OC_INDEX adimz;

  mutable int xperiodic;  // If 0, then not periodic.  Otherwise,
  mutable int yperiodic;  // periodic in indicated direction.
  mutable int zperiodic;

  mutable OC_UINT4m mesh_id;

  // The A## arrays hold demag coefficients, transformed into
  // frequency domain.  These are held long term.  Due to
  // symmetries, only the first octant needs to be saved.
  //   The Hxfrm array is temporary space used primarily to hold
  // the transform image of the computed field.  It is also
  // used as intermediary storage for the A## values, which
  // for convenience are computed at full size before being
  // transfered to the 1/8-sized A## storage arrays.
  //   All of these arrays are actually arrays of complex-valued
  // three vectors, but are handled as simple REAL arrays.
  mutable A_coefs* A;

#if !OOMMF_THREADS
  mutable OXS_FFT_REAL_TYPE *Hxfrm;
#else
  // In the threaded code, the memory pointed to by Hxfrm
  // is managed by an Oxs_StripedArray object
  mutable Oxs_StripedArray<OXS_FFT_REAL_TYPE> Hxfrm_base;
  mutable Oxs_StripedArray<OXS_FFT_REAL_TYPE> Hxfrm_base_yz;
  /// Hxfrm_base_yz is used with the FFTyz+embedded convolution code
#endif

  OC_REAL8m asymptotic_radius;
  /// If >=0, then radius beyond which demag coefficients A_coefs
  /// are computed using asymptotic (dipolar and higher) approximation
  /// instead of Newell's analytic formulae.

  mutable OXS_FFT_REAL_TYPE *Mtemp;  // Temporary space to hold
  /// Ms[]*m[].  The plan is to make this space unnecessary
  /// by introducing FFT functions that can take Ms as input
  /// and do the multiplication on the fly.

  // Object to perform FFTs.  All transforms are the same size, so we
  // only need one Oxs_FFT3DThreeVector object.  (Note: A
  // multi-threaded version of this code might want to have a separate
  // Oxs_FFT3DThreeVector for each thread, so that the roots-of-unity
  // arrays in each thread are independent.  This may improve memory
  // access on multi-processor machines where each processor has its
  // own memory.)
   //   The embed_convolution boolean specifies whether or not to embed
  // the convolution (matrix-vector multiply in FFT transform space)
  // computation inside the fftz computation.  If true then
  // embed_block_size is the number of z-axis FFTs to perform about
  // each "convolution" (really matrix-vector multiply) computation.
  // These are set inside the FillCoefficientArrays member function,
  // and used inside GetEnergy.
#if !OOMMF_THREADS
  mutable Oxs_FFT1DThreeVector fftx;
  mutable Oxs_FFTStrided ffty;
  mutable Oxs_FFTStrided fftz;
  mutable OC_BOOL embed_convolution; // Note: Always true in threaded version
#else
  const int MaxThreadCount;
  mutable Oxs_ThreadTree threadtree;

  // FFT objects for non-threaded access
  mutable Oxs_FFT1DThreeVector fftx;
  mutable Oxs_FFTStrided ffty;
  mutable Oxs_FFTStrided fftz;

  class Oxs_FFTLocker : public Oxs_ThreadMapDataObject {
  public:
    Oxs_FFT1DThreeVector fftx;
    Oxs_FFTStrided ffty;
    Oxs_FFTStrided fftz;
    OXS_FFT_REAL_TYPE* ifftx_scratch;
    OXS_FFT_REAL_TYPE* fftz_Hwork;
    OXS_FFT_REAL_TYPE* fftyz_Hwork;
    char* fftyz_Hwork_base; // Block used for aligning fftyz_Hwork
    OXS_FFT_REAL_TYPE* fftyconvolve_Hwork;
    A_coefs* A_copy;
    size_t ifftx_scratch_size;
    size_t fftz_Hwork_size;
    size_t fftyz_Hwork_size;      // In OXS_FFT_REAL_TYPE units
    size_t fftyz_Hwork_base_size; // For alignment; count in bytes
    size_t fftyconvolve_Hwork_size;
    size_t A_copy_size;

    Oxs_FFTLocker(const Oxs_FFTLocker_Info& info);
    ~Oxs_FFTLocker();

  private:
    // Declare but don't define default constructors and
    // assignment operator.
    Oxs_FFTLocker();
    Oxs_FFTLocker(const Oxs_FFTLocker&);
    Oxs_FFTLocker& operator=(const Oxs_FFTLocker&);
  };

  String MakeLockerName() const {
    Oc_AutoBuf abuf;
    Omf_AsciiPtr::PtrToAscii(this,abuf);
    String name = String(abuf.GetStr())
      + String(" : ") + String(InstanceName());
    return name;
  }

#endif
  mutable OC_INDEX embed_block_size;
  mutable OC_INDEX embed_yzblock_size;
  OC_INDEX cache_size; // Cache size in bytes.  Used to select
                       // embed_block_size.

  OC_INT4m zero_self_demag;
  /// If zero_self_demag is true, then diag(1/3,1/3,1/3) is subtracted
  /// from the self-demag term.  In particular, for cubic cells this
  /// makes the self-demag field zero.  This will change the value
  /// computed for the demag energy by a constant amount, but since the
  /// demag field is changed by a multiple of m, the torque and
  /// therefore the magnetization dynamics are unaffected.

  void FillCoefficientArrays(const Oxs_Mesh* mesh) const;

  void ReleaseMemory() const;


protected:
#if !OOMMF_THREADS
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const;
#else
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const {
    GetEnergyAlt(state,oed);
  }

  virtual void ComputeEnergy(const Oxs_SimState& state,
                             Oxs_ComputeEnergyData& oced) const;
#endif
public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_Demag(const char* name,     // Child instance id
	    Oxs_Director* newdtr, // App director
	    const char* argstr);  // MIF input block parameters
  virtual ~Oxs_Demag();
  virtual OC_BOOL Init();

  // Optional interface for conjugate-gradient evolver.
  virtual OC_INT4m IncrementPreconditioner(PreconditionerData& pcd);
};


#endif // _OXS_DEMAG
