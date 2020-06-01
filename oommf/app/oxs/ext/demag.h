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

#if OOMMF_THREADS
  friend class Oxs_FFTLocker;
  friend class _Oxs_DemagFFTxThread;
  friend class _Oxs_DemagiFFTxDotThread;
  friend class _Oxs_DemagFFTyzConvolveThread;
#endif // OOMMF_THREADS

public:
  // Sun CC, Forte Developer 7 C++ 5.4 2002/03/09, complains inside
  // Oxs_FFTLocker about A_coefs not being accessible if this declaration
  // is inside private: block.  OK, fine...
  // More recently, this struct is referenced by thread classes in
  // demag-threaded.cc
  struct A_coefs {
  public:
    OXS_FFT_REAL_TYPE A00;
    OXS_FFT_REAL_TYPE A01;
    OXS_FFT_REAL_TYPE A02;
    OXS_FFT_REAL_TYPE A11;
    OXS_FFT_REAL_TYPE A12;
    OXS_FFT_REAL_TYPE A22;
    A_coefs() {}
    A_coefs(OXS_FFT_REAL_TYPE iA00,OXS_FFT_REAL_TYPE iA01,
            OXS_FFT_REAL_TYPE iA02,OXS_FFT_REAL_TYPE iA11,
            OXS_FFT_REAL_TYPE iA12,OXS_FFT_REAL_TYPE iA22)
      : A00(iA00), A01(iA01), A02(iA02), A11(iA11), A12(iA12), A22(iA22) {}
    A_coefs& operator+=(const A_coefs& other) {
      A00 += other.A00;   A01 += other.A01;   A02 += other.A02;
      A11 += other.A11;   A12 += other.A12;   A22 += other.A22;
      return *this;
    }
  };

#if OOMMF_THREADS
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
    OC_INDEX Hxfrm_jstride;
    OC_INDEX Hxfrm_kstride;
    OC_INDEX embed_block_size;
    String name; // In use, we just set this to
    /// "InstanceName() + this_addr", so that each
    /// Oxs_Demag object gets a separate locker.
    /// See Oxs_Demag member function MakeLockerName().

    Oxs_FFTLocker_Info()
      : rdimx(0), rdimy(0), rdimz(0),
        cdimx(0), cdimy(0), cdimz(0),
        Hxfrm_jstride(0),Hxfrm_kstride(0),
        embed_block_size(0) {}

    Oxs_FFTLocker_Info(OC_INDEX in_rdimx,OC_INDEX in_rdimy,OC_INDEX in_rdimz,
                       OC_INDEX in_cdimx,OC_INDEX in_cdimy,OC_INDEX in_cdimz,
                       OC_INDEX in_Hxfrm_jstride,OC_INDEX in_Hxfrm_kstride,
                       OC_INDEX in_embed_block_size,
                       const char* in_name)
      : rdimx(in_rdimx), rdimy(in_rdimy), rdimz(in_rdimz),
        cdimx(in_cdimx), cdimy(in_cdimy), cdimz(in_cdimz),
        Hxfrm_jstride(in_Hxfrm_jstride),Hxfrm_kstride(in_Hxfrm_kstride),
        embed_block_size(in_embed_block_size), name(in_name) {}

    void Set(OC_INDEX in_rdimx,OC_INDEX in_rdimy,OC_INDEX in_rdimz,
             OC_INDEX in_cdimx,OC_INDEX in_cdimy,OC_INDEX in_cdimz,
             OC_INDEX in_Hxfrm_jstride,OC_INDEX in_Hxfrm_kstride,
             OC_INDEX in_embed_block_size,
             const String& in_name) {
      rdimx = in_rdimx;      rdimy = in_rdimy;      rdimz = in_rdimz;
      cdimx = in_cdimx;      cdimy = in_cdimy;      cdimz = in_cdimz;
      Hxfrm_jstride = in_Hxfrm_jstride;
      Hxfrm_kstride = in_Hxfrm_kstride;
      embed_block_size = in_embed_block_size;
      name = in_name;
    }
    // Default copy constructor and assignment operator are okay.
  };
#endif // OOMMF_THREADS

private:
#if REPORT_TIME
  mutable Nb_Timer inittime;

  mutable Nb_Timer fftforwardtime;
  mutable Nb_Timer fftxforwardtime;
  mutable Nb_Timer fftyforwardtime;

  mutable Nb_Timer fftinversetime;
  mutable Nb_Timer fftxinversetime;
  mutable Nb_Timer fftyinversetime;

  mutable Nb_Timer convtime;
  mutable Nb_Timer dottime;

  enum { dvltimer_number = 10 };
  mutable Nb_Timer dvltimer[dvltimer_number];
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


#if OOMMF_THREADS
  mutable OC_INDEX Hxfrm_jstride;
  mutable OC_INDEX Hxfrm_kstride;
  mutable Oxs_FFTLocker_Info locker_info; // Wraps most of the above
  /// info, for easy access by threads.
#endif // OOMMF_THREADS

  mutable OC_UINT4m mesh_id;
  mutable OC_REAL8m energy_density_error_estimate; // Cached value,
  /// initialized when mesh changes.

  // The A## arrays hold demag coefficients, transformed into
  // frequency domain.  These are held long term.  Due to
  // symmetries, only the first octant needs to be saved.
  //   The Hxfrm array is temporary space used primarily to hold
  // the transform image of the computed field.  It is also
  // used as intermediary storage for the A## values, which
  // for convenience are computed at full size before being
  // transfered to the 1/8-sized A## storage arrays.
  //   The frequency domain data (i.e., FFT images) are in truth arrays
  // of complex-valued three vectors, but are handled as REAL arrays of
  // six vectors --- with the exception of the transform of the A_coefs
  // data, which due to symmetry has imaginary part = 0 in both space
  // and frequency domains.
  mutable Oxs_StripedArray<A_coefs> A; // For single threaded code,
  /// indexing order or (x=i,y=j,z=k) increments i fastest and k
  /// slowest.  Multi-threaded code flips this so that j increments
  /// fastest and i slowest.

  // Max asymptotic method order
  int asymptotic_order;

  // Demag tensor computation accuracy
  OC_REAL8m demag_tensor_error;
  
#if !OOMMF_THREADS
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
  mutable Oxs_FFT1DThreeVector fftx;
  mutable Oxs_FFTStrided ffty;
  mutable Oxs_FFTStrided fftz;
  mutable OXS_FFT_REAL_TYPE *Hxfrm;
  // In the threaded code, the memory pointed to by Hxfrm
  // is managed by an Oxs_StripedArray object
  mutable OXS_FFT_REAL_TYPE *Mtemp;  // Temporary space to hold
  /// Ms[]*m[].  Not needed if using FFT routines that can take Ms as
  /// input and do the multiplication on the fly.
  mutable OC_BOOL embed_convolution; // Note: Always true in threaded version
#else // OOMMF_THREADS
  mutable Oxs_StripedArray<OXS_FFT_REAL_TYPE> Hxfrm_base;
  const int MaxThreadCount;
  mutable Oxs_ThreadTree threadtree;

  // Thread-specific data
  class Oxs_FFTLocker : public Oxs_ThreadMapDataObject {
  public:
    Oxs_FFT1DThreeVector fftx;
    Oxs_FFTStrided ffty;
    Oxs_FFTStrided fftz;

    OXS_FFT_REAL_TYPE* fftx_scratch;
    char*  fftx_scratch_base; // Block used for aligning fftx_scratch
    size_t fftx_scratch_size;
    size_t fftx_scratch_base_size;

    OXS_FFT_REAL_TYPE* ffty_Hwork;
    char*  ffty_Hwork_base; // Block used for aligning ffty_Hwork
    OC_INDEX ffty_Hwork_zstride;  // In OXS_FFT_REAL_TYPE units
    OC_INDEX ffty_Hwork_xstride;
    OC_INDEX ffty_Hwork_size;
    OC_INDEX ffty_Hwork_base_size; // For alignment; count in bytes

    OXS_FFT_REAL_TYPE* fftz_Hwork;
    char*  fftz_Hwork_base; // Block used for aligning fftz_Hwork
    OC_INDEX fftz_Hwork_zstride;  // In OXS_FFT_REAL_TYPE units
    OC_INDEX fftz_Hwork_size;
    OC_INDEX fftz_Hwork_base_size; // For alignment; count in bytes

    const int threadnumber;

    Oxs_FFTLocker(const Oxs_FFTLocker_Info& info,int in_threadnumber);
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

#endif // OOMMF_THREADS

  mutable OC_INDEX embed_block_size;
  OC_INDEX cache_size; // Cache size in bytes.  Used in selecting
                       // embed_block_size.

  OC_INT4m zero_self_demag;
  /// If zero_self_demag is true, then diag(1/3,1/3,1/3) is subtracted
  /// from the self-demag term.  In particular, for cubic cells this
  /// makes the self-demag field zero.  This will change the value
  /// computed for the demag energy by a constant amount, but since the
  /// demag field is changed by a multiple of m, the torque and
  /// therefore the magnetization dynamics are unaffected.

  String saveN;
  /// If non-empty, name of file to save demag tensor, as a six column
  /// OVF 2.0 file, with order Nxx Nxy Nxz Nyy Nyz Nzz.

  String saveN_fmt;  // File format for saveN
  
  void FillCoefficientArrays(const Oxs_Mesh* mesh) const;

  void ReleaseMemory() const;
  
protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const {
    GetEnergyAlt(state,oed);
  }

  virtual void ComputeEnergy(const Oxs_SimState& state,
                             Oxs_ComputeEnergyData& oced) const;

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

  // For debugging.  Don't use this if A is big!
  void DumpA(FILE* fptr) const;

};

inline bool operator==(const Oxs_Demag::A_coefs& lhs,
                       const Oxs_Demag::A_coefs& rhs) {
  return (lhs.A00 == rhs.A00) && (lhs.A01 == rhs.A01)
    && (lhs.A02 == rhs.A02) && (lhs.A11 == rhs.A11)
    && (lhs.A12 == rhs.A12) && (lhs.A22 == rhs.A22);
}

#endif // _OXS_DEMAG
