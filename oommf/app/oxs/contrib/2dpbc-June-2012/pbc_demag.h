/* FILE: demag.h            -*-Mode: c++-*-
 *
 * Average H demag field across rectangular cells.  This is a modified
 * version of the simpledemag class, which uses symmetries in the
 * interaction coefficients to reduce memory usage.
 *
 */

#ifndef _PBC_DEMAG_2D
#define _PBC_DEMAG_2D

#include "oc.h"  // Includes OOMMF_THREADS macro in ocport.h
#include "energy.h"
#include "fft3v.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"

#include "pbc_util.h"

#if OOMMF_THREADS
#include "oxsthread.h"
#endif

/* End includes */

class PBC_Demag_2D : public Oxs_Energy {
    friend class Oxs_FFTLocker;
    friend class _PBC_DemagFFTxThread;
    friend class _PBC_DemagiFFTxDotThread;
    friend class _PBC_DemagFFTyThread;
    friend class _PBC_DemagFFTyConvolveThread;
    friend class _PBC_DemagFFTzConvolveThread;


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
#endif // REPORT_TIME

    mutable INT4m rdimx; // Natural size of real data
    mutable INT4m rdimy; // Digital Mars compiler wants these as separate
    mutable INT4m rdimz; //    statements, because of "mutable" keyword.
    mutable INT4m cdimx; // Full size of complex data
    mutable INT4m cdimy;
    mutable INT4m cdimz;
    // 2*(cdimx-1)>=rdimx, cdimy>=rdimy, cdimz>=rdimz
    // cdim-1 and cdim[yz] should be powers of 2.
    mutable INT4m adimx; // Dimensions of A## storage (see below).
    mutable INT4m adimy;
    mutable INT4m adimz;


    mutable UINT4m mesh_id;

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

    mutable OXS_FFT_REAL_TYPE *Hxfrm;


    // The A## arrays hold demag coefficients, transformed into
    // frequency domain.  These are held long term.  xcomp,
    // ycomp, and zcomp are used as temporary space, first to hold
    // the transforms of Mx, My, and Mz, then to store Hx, Hy, and
    // Hz.

    String tensor_file_name;
    mutable BOOL load_from_file_success;
    REAL8m pbc_2d_error;
    mutable UINT4m xdim;
    mutable UINT4m ydim;
    mutable UINT4m zdim;
    UINT4m sample_repeat_nx;
    UINT4m sample_repeat_ny;
    REAL8m asymptotic_radius;
    REAL8m dipolar_radius;
    REAL8m asymptotic_radius_sq;
    REAL8m dipolar_radius_sq;
    mutable Oxs_MeshValue<ThreeVector> Npbc_diag;
    mutable Oxs_MeshValue<ThreeVector> Npbc_offdiag;



    mutable OXS_FFT_REAL_TYPE *Mtemp; // Temporary space to hold
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
    mutable BOOL embed_convolution; // Note: Always true in threaded version
#else
    const int MaxThreadCount;
    mutable Oxs_ThreadTree threadtree;

    // FFT objects for non-threaded access
    mutable Oxs_FFT1DThreeVector fftx;
    mutable Oxs_FFTStrided ffty;
    mutable Oxs_FFTStrided fftz;

    // Thread-specific data

    struct Oxs_FFTLocker_Info {
    public:
        INT4m rdimx;
        INT4m rdimy;
        INT4m rdimz;
        INT4m cdimx;
        INT4m cdimy;
        INT4m cdimz;
        INT4m embed_block_size;
        String name; // In use, we just set this to
        /// "InstanceName() + this_addr", so that each
        /// Oxs_Demag object gets a separate locker.
        /// See Oxs_Demag member function MakeLockerName().

        Oxs_FFTLocker_Info()
        : rdimx(0), rdimy(0), rdimz(0),
        cdimx(0), cdimy(0), cdimz(0),
        embed_block_size(0) {
        }

        Oxs_FFTLocker_Info(INT4m in_rdimx, INT4m in_rdimy, INT4m in_rdimz,
                INT4m in_cdimx, INT4m in_cdimy, INT4m in_cdimz,
                INT4m in_embed_block_size, const char* in_name)
        : rdimx(in_rdimx), rdimy(in_rdimy), rdimz(in_rdimz),
        cdimx(in_cdimx), cdimy(in_cdimy), cdimz(in_cdimz),
        embed_block_size(in_embed_block_size), name(in_name) {
        }

        void Set(INT4m in_rdimx, INT4m in_rdimy, INT4m in_rdimz,
                INT4m in_cdimx, INT4m in_cdimy, INT4m in_cdimz,
                INT4m in_embed_block_size, const String & in_name) {
            rdimx = in_rdimx;
            rdimy = in_rdimy;
            rdimz = in_rdimz;
            cdimx = in_cdimx;
            cdimy = in_cdimy;
            cdimz = in_cdimz;
            embed_block_size = in_embed_block_size;
            name = in_name;
        }
        // Default copy constructor and assignment operator are okay.
    };

    class Oxs_FFTLocker : public Oxs_ThreadMapDataObject {
    public:
        Oxs_FFT1DThreeVector fftx;
        Oxs_FFTStrided ffty;
        Oxs_FFTStrided fftz;
        OXS_FFT_REAL_TYPE* ifftx_scratch;
        OXS_FFT_REAL_TYPE* fftz_Hwork;
        OXS_FFT_REAL_TYPE* fftyconvolve_Hwork;
        A_coefs* A_copy;
        size_t ifftx_scratch_size;
        size_t fftz_Hwork_size;
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
        Omf_AsciiPtr::PtrToAscii(this, abuf);
        String name = String(abuf.GetStr())
                + String(" : ") + String(InstanceName());
        return name;
    }

#endif


    mutable INT4m embed_block_size;
    INT4m cache_size; // Cache size in bytes.  Used to select
    // embed_block_size.

    void FillCoefficientArrays(const Oxs_Mesh* mesh) const;
    /// The "standard" variant is simpler but slower, and is retained
    /// mainly for testing and development purposes.

    void ReleaseMemory() const;
    double CalculateSingleTensor(enum TensorComponent comp,
            int g, double x, double y, double z, double a, double b, double c) const;
    double CalculateSingleTensorFinitely(enum TensorComponent comp,
            int gx, int gy, double x, double y, double z, double a, double b, double c) const;

    int FindG(enum TensorComponent comp, double v, double Tx, double Ty) const;
    void CalculateDemagTensors(const Oxs_RectangularMesh* mesh) const;
    void SavePbcDemagTensor(const Oxs_Mesh *mesh) const;
    void LoadPbcDemagTensor(const Oxs_RectangularMesh* mesh) const;
    REAL8m GetTensorFromBuffer(enum TensorComponent comp, int i, int j, int k) const;


protected:
#if !OOMMF_THREADS
    virtual void GetEnergy(const Oxs_SimState& state,
            Oxs_EnergyData& oed) const;
#else

    virtual void GetEnergy(const Oxs_SimState& state,
            Oxs_EnergyData& oed) const {
        GetEnergyAlt(state, oed);
    }

    virtual void ComputeEnergy(const Oxs_SimState& state,
            Oxs_ComputeEnergyData& oced) const;
#endif

public:
    virtual const char* ClassName() const; // ClassName() is
    /// automatically generated by the OXS_EXT_REGISTER macro.
    PBC_Demag_2D(const char* name, // Child instance id
            Oxs_Director* newdtr, // App director
            const char* argstr); // MIF input block parameters
    virtual ~PBC_Demag_2D();
    virtual BOOL Init();
};


#endif // _PBC_Demag_2D
