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
 */

#include "pbc_demag.h"
#if !OOMMF_THREADS

#include "pbc_util.h"

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
#include "demagcoef.h"


OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(PBC_Demag_2D);

/* End includes */

#ifndef VERBOSE_DEBUG
#define VERBOSE_DEBUG 0
#endif

// Size of threevector.  This macro is defined for code legibility
// and maintenance; it should always be "3".
#define ODTV_VECSIZE 3

// Size of complex value, in real units.  This macro is defined for code
// legibility and maintenance; it should always be "2".
#define ODTV_COMPLEXSIZE 2

// Constructor

PBC_Demag_2D::PBC_Demag_2D(
        const char* name, // Child instance id
        Oxs_Director* newdtr, // App director
        const char* argstr) // MIF input block parameters
: Oxs_Energy(name, newdtr, argstr),
rdimx(0), rdimy(0), rdimz(0),
cdimx(0), cdimy(0), cdimz(0),
adimx(0), adimy(0), adimz(0),
mesh_id(0), A(0), Hxfrm(0), Mtemp(0),
embed_convolution(0), embed_block_size(0),
tensor_file_name(""), pbc_2d_error(0),
asymptotic_radius(0), dipolar_radius(0),
asymptotic_radius_sq(0), dipolar_radius_sq(0),
xdim(0), ydim(0), zdim(0), sample_repeat_nx(0), sample_repeat_ny(0),
Npbc_diag(NULL), Npbc_offdiag(NULL) {


    tensor_file_name = GetStringInitValue("tensor_file_name", "");
    pbc_2d_error = GetRealInitValue("tensor_error", 1e-10);
    asymptotic_radius = GetRealInitValue("asymptotic_radius", 32.0);
    dipolar_radius = GetRealInitValue("dipolar_radius", 10000.0);
    sample_repeat_nx = GetIntInitValue("sample_repeat_nx", -1);
    sample_repeat_ny = GetIntInitValue("sample_repeat_ny", -1);

    asymptotic_radius_sq = asymptotic_radius*asymptotic_radius;
    dipolar_radius_sq = dipolar_radius*dipolar_radius;

    VerifyAllInitArgsUsed();
}

PBC_Demag_2D::~PBC_Demag_2D() {

#if REPORT_TIME
    Oc_TimeVal cpu, wall;

    inittime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...   init%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    fftforwardtime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...  f-fft%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }
    fftinversetime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...  i-fft%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    fftxforwardtime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ... f-fftx%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }
    fftxinversetime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ... i-fftx%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    fftyforwardtime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ... f-ffty%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }
    fftyinversetime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ... i-ffty%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    convtime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...   conv%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    dottime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...    dot%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

#endif // REPORT_TIME
    ReleaseMemory();
}

BOOL PBC_Demag_2D::Init() {


#if REPORT_TIME
    Oc_TimeVal cpu, wall;

    inittime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...   init%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    fftforwardtime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...  f-fft%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }
    fftinversetime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...  i-fft%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    fftxforwardtime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ... f-fftx%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }
    fftxinversetime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ... i-fftx%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    fftyforwardtime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ... f-ffty%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }
    fftyinversetime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ... i-ffty%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    convtime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...   conv%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
    }

    dottime.GetTimes(cpu, wall);
    if (double(wall) > 0.0) {
        fprintf(stderr, "      subtime ...    dot%7.2f cpu /%7.2f wall,"
                " (%s)\n",
                double(cpu), double(wall), InstanceName());
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

void PBC_Demag_2D::ReleaseMemory() const { // Conceptually const
    
    if (A != 0) {
        delete[] A;
        A = 0;
    }
    if (Hxfrm != 0) {
        delete[] Hxfrm;
        Hxfrm = 0;
    }
    if (Mtemp != 0) {
        delete[] Mtemp;
        Mtemp = 0;
    }
    rdimx = rdimy = rdimz = 0;
    cdimx = cdimy = cdimz = 0;
    adimx = adimy = adimz = 0;


}

void PBC_Demag_2D::FillCoefficientArrays(const Oxs_Mesh* genmesh) const { // This routine is conceptually const.

    const Oxs_RectangularMesh* mesh
            = dynamic_cast<const Oxs_RectangularMesh*> (genmesh);
    if (mesh == NULL) {
        String msg = String("Object ")
                + String(genmesh->InstanceName())
                + String(" is not a rectangular mesh.");
        throw Oxs_Ext::Error(this, msg);
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
    if (rdimx == 0 || rdimy == 0 || rdimz == 0) return; // Empty mesh!


    Mtemp = new OXS_FFT_REAL_TYPE[ODTV_VECSIZE * rdimx * rdimy * rdimz];
    /// Temporary space to hold Ms[]*m[].  The plan is to make this space
    /// unnecessary by introducing FFT functions that can take Ms as input
    /// and do the multiplication on the fly.

    // Initialize fft object.  If a dimension equals 1, then zero
    // padding is not required.  Otherwise, zero pad to at least
    // twice the dimension.

    Oxs_FFT3DThreeVector::RecommendDimensions((rdimx == 1 ? 1 : 2 * rdimx),
            (rdimy == 1 ? 1 : 2 * rdimy),
            (rdimz == 1 ? 1 : 2 * rdimz),
            cdimx, cdimy, cdimz);
    INT4m xfrm_size = ODTV_VECSIZE * 2 * cdimx * cdimy * cdimz;
    // "ODTV_VECSIZE" here is because we work with arrays if ThreeVectors,
    // and "2" because these are complex (as opposed to real)
    // quantities.
    if (xfrm_size < cdimx || xfrm_size < cdimy || xfrm_size < cdimz) {
        // Partial overflow check
        String msg =
                String("INT4m overflow in ")
                + String(InstanceName())
                + String(": Product 2*cdimx*cdimy*cdimz too big"
                " to fit in a INT4m variable");
        throw Oxs_ExtError(this, msg);
    }


    // The following 3 statements are cribbed from
    // Oxs_FFT3DThreeVector::SetDimensions().  The corresponding
    // code using that class is
    //
    //  Oxs_FFT3DThreeVector fft;
    //  fft.SetDimensions(rdimx,rdimy,rdimz,cdimx,cdimy,cdimz);
    //  fft.GetLogicalDimensions(ldimx,ldimy,ldimz);
    //
    fftx.SetDimensions(rdimx, (cdimx == 1 ? 1 : 2 * (cdimx - 1)), rdimy);
    ffty.SetDimensions(rdimy, cdimy,
            ODTV_COMPLEXSIZE * ODTV_VECSIZE*cdimx,
            ODTV_VECSIZE * cdimx);
    fftz.SetDimensions(rdimz, cdimz,
            ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy,
            ODTV_VECSIZE * cdimx * cdimy);
    INT4m ldimx, ldimy, ldimz; // Logical dimensions
    // The following 3 statements are cribbed from
    // Oxs_FFT3DThreeVector::GetLogicalDimensions()
    ldimx = fftx.GetLogicalDimension();
    ldimy = ffty.GetLogicalDimension();
    ldimz = fftz.GetLogicalDimension();

    adimx = (ldimx / 2) + 1;
    adimy = (ldimy / 2) + 1;
    adimz = (ldimz / 2) + 1;

#if VERBOSE_DEBUG && !defined(NDEBUG)
    fprintf(stderr, "RDIMS: (%d,%d,%d)\n", rdimx, rdimy, rdimz); /**/
    fprintf(stderr, "CDIMS: (%d,%d,%d)\n", cdimx, cdimy, cdimz); /**/
    fprintf(stderr, "LDIMS: (%d,%d,%d)\n", ldimx, ldimy, ldimz); /**/
    fprintf(stderr, "ADIMS: (%d,%d,%d)\n", adimx, adimy, adimz); /**/
#endif // NDEBUG



    INT4m sstridey = ODTV_VECSIZE*ldimx;
    INT4m sstridez = sstridey*ldimy;



    // Dimension of array necessary to hold 3 sets of full interaction
    // coefficients in real space:
    INT4m scratch_size = ODTV_VECSIZE * ldimx * ldimy * ldimz;
    if (scratch_size < ldimx || scratch_size < ldimy || scratch_size < ldimz) {
        // Partial overflow check
        String msg =
                String("INT4m overflow in ")
                + String(InstanceName())
                + String(": Product 3*8*rdimx*rdimy*rdimz too big"
                " to fit in a INT4m variable");
        throw Oxs_ExtError(this, msg);
    }

    // Allocate memory for FFT xfrm target H, and scratch space
    // for computing interaction coefficients
    Hxfrm = new OXS_FFT_REAL_TYPE[xfrm_size];
    OXS_FFT_REAL_TYPE* scratch = new OXS_FFT_REAL_TYPE[scratch_size];

    if (Hxfrm == NULL || scratch == NULL) {
        // Safety check for those machines on which new[] doesn't throw
        // BadAlloc.
        String msg = String("Insufficient memory in Demag setup.");
        throw Oxs_ExtError(this, msg);
    }
    
    UINT4m index, i, j, k;
    
    for (k = 0; k < ldimz; ++k) {
        for (j = 0; j < ldimy; ++j) {
            for (i = 0; i < ldimx; ++i) {
                index = ODTV_VECSIZE * ((k * ldimy + j) * ldimx + i);
                scratch[index]=0;
                scratch[index + 1]=0;
                scratch[index + 2]=0;
            }
        }
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


    

    REALWIDE dx = mesh->EdgeLengthX();
    REALWIDE dy = mesh->EdgeLengthY();
    REALWIDE dz = mesh->EdgeLengthZ();
    // For demag calculation, all that matters is the relative
    // sizes of dx, dy and dz.  To help insure we don't run
    // outside floating point range, rescale these values so
    // largest is 1.0
    REALWIDE maxedge = dx;
    if (dy > maxedge) maxedge = dy;
    if (dz > maxedge) maxedge = dz;
    dx /= maxedge;
    dy /= maxedge;
    dz /= maxedge;

    // REALWIDE scale = -1./(4*PI*dx*dy*dz);
    REALWIDE scale = -1 * fftx.GetScaling() * ffty.GetScaling() * fftz.GetScaling();
    
    
    
 
    

    for (k = 0; k < rdimz; k++) {
        INT4m kindex = k*sstridez;
        for (j = 0; j < rdimy; j++) {
            INT4m jkindex = kindex + j*sstridey;
            for (i = 0; i < rdimx; i++) {
                INT4m index = ODTV_VECSIZE * i + jkindex;

                REALWIDE tmpA00 = scale * GetTensorFromBuffer(xx, i, j, k);
                REALWIDE tmpA01 = scale * GetTensorFromBuffer(xy, i, j, k);
                REALWIDE tmpA02 = scale * GetTensorFromBuffer(xz, i, j, k);

                scratch[index] = tmpA00;
                scratch[index + 1] = tmpA01;
                scratch[index + 2] = tmpA02;

                if (i > 0) {
                    INT4m tindex = ODTV_VECSIZE * (ldimx - i) + j * sstridey + k*sstridez;
                    scratch[tindex] = tmpA00;
                    scratch[tindex + 1] = -1 * tmpA01;
                    scratch[tindex + 2] = -1 * tmpA02;
                }
                if (j > 0) {
                    INT4m tindex = ODTV_VECSIZE * i + (ldimy - j) * sstridey + k*sstridez;
                    scratch[tindex] = tmpA00;
                    scratch[tindex + 1] = -1 * tmpA01;
                    scratch[tindex + 2] = tmpA02;
                }
                if (k > 0) {
                    INT4m tindex = ODTV_VECSIZE * i + j * sstridey + (ldimz - k) * sstridez;
                    scratch[tindex] = tmpA00;
                    scratch[tindex + 1] = tmpA01;
                    scratch[tindex + 2] = -1 * tmpA02;
                }
                if (i > 0 && j > 0) {
                    INT4m tindex
                            = ODTV_VECSIZE * (ldimx - i) + (ldimy - j) * sstridey + k*sstridez;
                    scratch[tindex] = tmpA00;
                    scratch[tindex + 1] = tmpA01;
                    scratch[tindex + 2] = -1 * tmpA02;
                }
                if (i > 0 && k > 0) {
                    INT4m tindex
                            = ODTV_VECSIZE * (ldimx - i) + j * sstridey + (ldimz - k) * sstridez;
                    scratch[tindex] = tmpA00;
                    scratch[tindex + 1] = -1 * tmpA01;
                    scratch[tindex + 2] = tmpA02;
                }
                if (j > 0 && k > 0) {
                    INT4m tindex
                            = ODTV_VECSIZE * i + (ldimy - j) * sstridey + (ldimz - k) * sstridez;
                    scratch[tindex] = tmpA00;
                    scratch[tindex + 1] = -1 * tmpA01;
                    scratch[tindex + 2] = -1 * tmpA02;
                }
                if (i > 0 && j > 0 && k > 0) {
                    INT4m tindex
                            = ODTV_VECSIZE * (ldimx - i) + (ldimy - j) * sstridey + (ldimz - k) * sstridez;
                    scratch[tindex] = tmpA00;
                    scratch[tindex + 1] = tmpA01;
                    scratch[tindex + 2] = tmpA02;
                }
            }
        }
    }


   #if VERBOSE_DEBUG && !defined(NDEBUG)
    for (k = 0; k < ldimz; ++k) {
        for (j = 0; j < ldimy; ++j) {
            for (i = 0; i < ldimx; ++i) {
                INT4m index = ODTV_VECSIZE * ((k * ldimy + j) * ldimx + i);
                printf("A00[%02d][%02d][%02d] = %#25.12f\n",
                        i, j, k, 0.5 * scratch[index]);
                printf("A01[%02d][%02d][%02d] = %#25.12f\n",
                        i, j, k, 0.5 * scratch[index + 1]);
                printf("A02[%02d][%02d][%02d] = %#25.12f\n",
                        i, j, k, 0.5 * scratch[index + 2]);
            }
        }
    }
    fflush(stdout);
   #endif // NDEBUG





    // Transform into frequency domain.  These lines are cribbed
    // from the corresponding code in Oxs_FFT3DThreeVector.
    // Note: Using an Oxs_FFT3DThreeVector fft object, this would be just
    //    fft.AdjustInputDimensions(ldimx,ldimy,ldimz);
    //    fft.ForwardRealToComplexFFT(scratch,Hxfrm);
    //    fft.AdjustInputDimensions(rdimx,rdimy,rdimz); // Safety
    {
        fftx.AdjustInputDimensions(ldimx, ldimy);
        ffty.AdjustInputDimensions(ldimy,
                ODTV_COMPLEXSIZE * ODTV_VECSIZE*cdimx,
                ODTV_VECSIZE * cdimx);
        fftz.AdjustInputDimensions(ldimz,
                ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy,
                ODTV_VECSIZE * cdimx * cdimy);

        INT4m rxydim = ODTV_VECSIZE * ldimx*ldimy;
        INT4m cxydim = ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy;

        for (INT4m m = 0; m < ldimz; ++m) {
            // x-direction transforms in plane "m"
            fftx.ForwardRealToComplexFFT(scratch + m*rxydim, Hxfrm + m * cxydim);

            // y-direction transforms in plane "m"
            ffty.ForwardFFT(Hxfrm + m * cxydim);
        }
        fftz.ForwardFFT(Hxfrm); // z-direction transforms

        fftx.AdjustInputDimensions(rdimx, rdimy); // Safety
        ffty.AdjustInputDimensions(rdimy,
                ODTV_COMPLEXSIZE * ODTV_VECSIZE*cdimx,
                ODTV_VECSIZE * cdimx);
        fftz.AdjustInputDimensions(rdimz,
                ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy,
                ODTV_VECSIZE * cdimx * cdimy);

    }

    // Copy results from scratch into A00, A01, and A02.  We only need
    // store 1/8th of the results because of symmetries.
    INT4m astridey = adimx;
    INT4m astridez = astridey*adimy;
    INT4m a_size = astridez*adimz;
    A = new A_coefs[a_size];

    INT4m cstridey = 2 * ODTV_VECSIZE*cdimx; // "2" for complex data
    INT4m cstridez = cstridey*cdimy;
    for (k = 0; k < adimz; k++) for (j = 0; j < adimy; j++) for (i = 0; i < adimx; i++) {
                INT4m aindex = i + j * astridey + k*astridez;
                INT4m hindex = 2 * ODTV_VECSIZE * i + j * cstridey + k*cstridez;
                A[aindex].A00 = Hxfrm[hindex]; // A00
                A[aindex].A01 = Hxfrm[hindex + 2]; // A01
                A[aindex].A02 = Hxfrm[hindex + 4]; // A02
                // The A## values are all real-valued, so we only need to pull the
                // real parts out of Hxfrm, which are stored in the even offsets.
            }







    // Repeat for A11, A12 and A22. //////////////////////////////////////
    for (k = 0; k < rdimz; k++) {
        INT4m kindex = k*sstridez;
        for (j = 0; j < rdimy; j++) {
            INT4m jkindex = kindex + j*sstridey;
            for (i = 0; i < rdimx; i++) {
                INT4m index = ODTV_VECSIZE * i + jkindex;



                REALWIDE tmpA11 = scale * GetTensorFromBuffer(yy, i, j, k);
                REALWIDE tmpA12 = scale * GetTensorFromBuffer(yz, i, j, k);
                REALWIDE tmpA22 = scale * GetTensorFromBuffer(zz, i, j, k);


                scratch[index] = tmpA11;
                scratch[index + 1] = tmpA12;
                scratch[index + 2] = tmpA22;
                if (i > 0) {
                    INT4m tindex = ODTV_VECSIZE * (ldimx - i) + j * sstridey + k*sstridez;
                    scratch[tindex] = tmpA11;
                    scratch[tindex + 1] = tmpA12;
                    scratch[tindex + 2] = tmpA22;
                }
                if (j > 0) {
                    INT4m tindex = ODTV_VECSIZE * i + (ldimy - j) * sstridey + k*sstridez;
                    scratch[tindex] = tmpA11;
                    scratch[tindex + 1] = -1 * tmpA12;
                    scratch[tindex + 2] = tmpA22;
                }
                if (k > 0) {
                    INT4m tindex = ODTV_VECSIZE * i + j * sstridey + (ldimz - k) * sstridez;
                    scratch[tindex] = tmpA11;
                    scratch[tindex + 1] = -1 * tmpA12;
                    scratch[tindex + 2] = tmpA22;
                }
                if (i > 0 && j > 0) {
                    INT4m tindex
                            = ODTV_VECSIZE * (ldimx - i) + (ldimy - j) * sstridey + k*sstridez;
                    scratch[tindex] = tmpA11;
                    scratch[tindex + 1] = -1 * tmpA12;
                    scratch[tindex + 2] = tmpA22;
                }
                if (i > 0 && k > 0) {
                    INT4m tindex
                            = ODTV_VECSIZE * (ldimx - i) + j * sstridey + (ldimz - k) * sstridez;
                    scratch[tindex] = tmpA11;
                    scratch[tindex + 1] = -1 * tmpA12;
                    scratch[tindex + 2] = tmpA22;
                }
                if (j > 0 && k > 0) {
                    INT4m tindex
                            = ODTV_VECSIZE * i + (ldimy - j) * sstridey + (ldimz - k) * sstridez;
                    scratch[tindex] = tmpA11;
                    scratch[tindex + 1] = tmpA12;
                    scratch[tindex + 2] = tmpA22;
                }
                if (i > 0 && j > 0 && k > 0) {
                    INT4m tindex
                            = ODTV_VECSIZE * (ldimx - i) + (ldimy - j) * sstridey + (ldimz - k) * sstridez;
                    scratch[tindex] = tmpA11;
                    scratch[tindex + 1] = tmpA12;
                    scratch[tindex + 2] = tmpA22;
                }
            }
        }
    }


  #if VERBOSE_DEBUG && !defined(NDEBUG)
  for(k=0;k<ldimz;++k) {
    for(j=0;j<ldimy;++j) {
      for(i=0;i<ldimx;++i) {
        INT4m index = ODTV_VECSIZE*((k*ldimy+j)*ldimx+i);
        printf("A11[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index]);
        printf("A12[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index+1]);
        printf("A22[%02d][%02d][%02d] = %#25.12f\n",
               i,j,k,0.5*scratch[index+2]);
      }
    }
  }
  fflush(stdout);
#endif // NDEBUG
    
    // Transform into frequency domain.  These lines are cribbed
    // from the corresponding code in Oxs_FFT3DThreeVector.
    // Note: Using an Oxs_FFT3DThreeVector fft object, this would be just
    //    fft.AdjustInputDimensions(ldimx,ldimy,ldimz);
    //    fft.ForwardRealToComplexFFT(scratch,Hxfrm);
    //    fft.AdjustInputDimensions(rdimx,rdimy,rdimz); // Safety
    {
        fftx.AdjustInputDimensions(ldimx, ldimy);
        ffty.AdjustInputDimensions(ldimy,
                ODTV_COMPLEXSIZE * ODTV_VECSIZE*cdimx,
                ODTV_VECSIZE * cdimx);
        fftz.AdjustInputDimensions(ldimz,
                ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy,
                ODTV_VECSIZE * cdimx * cdimy);

        INT4m rxydim = ODTV_VECSIZE * ldimx*ldimy;
        INT4m cxydim = ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy;

        for (INT4m m = 0; m < ldimz; ++m) {
            // x-direction transforms in plane "m"
            fftx.ForwardRealToComplexFFT(scratch + m*rxydim, Hxfrm + m * cxydim);

            // y-direction transforms in plane "m"
            ffty.ForwardFFT(Hxfrm + m * cxydim);
        }
        fftz.ForwardFFT(Hxfrm); // z-direction transforms

        fftx.AdjustInputDimensions(rdimx, rdimy); // Safety
        ffty.AdjustInputDimensions(rdimy,
                ODTV_COMPLEXSIZE * ODTV_VECSIZE*cdimx,
                ODTV_VECSIZE * cdimx);
        fftz.AdjustInputDimensions(rdimz,
                ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy,
                ODTV_VECSIZE * cdimx * cdimy);

    }

    // At this point we no longer need the "scratch" array, so release it.
    
    delete[] scratch;

    // Copy results from scratch into A11, A12, and A22.  We only need
    // store 1/8th of the results because of symmetries.
    for (k = 0; k < adimz; k++) for (j = 0; j < adimy; j++) for (i = 0; i < adimx; i++) {
                INT4m aindex = i + j * astridey + k*astridez;
                INT4m hindex = 2 * ODTV_VECSIZE * i + j * cstridey + k*cstridez;
                A[aindex].A11 = Hxfrm[hindex]; // A11
                A[aindex].A12 = Hxfrm[hindex + 2]; // A12
                A[aindex].A22 = Hxfrm[hindex + 4]; // A22
                // The A## values are all real-valued, so we only need to pull the
                // real parts out of Hxfrm, which are stored in the even offsets.
            }
    // Do we want to embed "convolution" computation inside z-axis FFTs?
    // If so, setup control variables.
    INT4m footprint
            = ODTV_COMPLEXSIZE * ODTV_VECSIZE * sizeof (OXS_FFT_REAL_TYPE) // Data
            + sizeof (A_coefs) // Interaction matrix
            + 2 * ODTV_COMPLEXSIZE * sizeof (OXS_FFT_REAL_TYPE); // Roots of unity
    footprint *= cdimz;
    INT4m trialsize = cache_size / (2 * footprint); // "2" is fudge factor
    if (trialsize > cdimx) trialsize = cdimx;
    if (cdimz > 1 && trialsize > 4) {
        // Note: If cdimz==1, then the z-axis FFT is a nop, so there is
        // nothing to embed the "convolution" with and we are better off
        // using the non-embedded code.
        embed_convolution = 1;
        embed_block_size = trialsize;
    } else {
        embed_convolution = 0;
        embed_block_size = 0; // A cry for help...
    }

#if REPORT_TIME
    inittime.Stop();
#endif // REPORT_TIME

}

void PBC_Demag_2D::GetEnergy
(const Oxs_SimState& state,
        Oxs_EnergyData& oed
        ) const {

    

    // (Re)-initialize mesh coefficient array if mesh has changed.
    if (mesh_id != state.mesh->Id()) {
        mesh_id = 0; // Safety
        
        
        const Oxs_RectangularMesh* mesh
            = dynamic_cast<const Oxs_RectangularMesh*> (state.mesh);
        
        LoadPbcDemagTensor(mesh);
        if (!load_from_file_success) {
            CalculateDemagTensors(mesh);
            SavePbcDemagTensor(state.mesh);
        }
        FillCoefficientArrays(state.mesh);
        mesh_id = state.mesh->Id();
    }

    const Oxs_MeshValue<ThreeVector>& spin = state.spin;
    const Oxs_MeshValue<REAL8m>& Ms = *(state.Ms);

    // Use supplied buffer space, and reflect that use in oed.
    oed.energy = oed.energy_buffer;
    oed.field = oed.field_buffer;
    Oxs_MeshValue<REAL8m>& energy = *oed.energy_buffer;
    Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;
    energy.AdjustSize(state.mesh);
    field.AdjustSize(state.mesh);

    // Fill Mtemp with Ms[]*spin[].  The plan is to eventually
    // roll this step into the forward FFT routine.
    const INT4m rsize = static_cast<INT4m> (Ms.Size());
    assert(rdimx*rdimy*rdimz == rsize);

    INT4m i, j, k;

    for (i = 0; i < rsize; ++i) {
        REAL8m scale = Ms[i];
        const ThreeVector& vec = spin[i];
        Mtemp[3 * i] = scale * vec.x;
        Mtemp[3 * i + 1] = scale * vec.y;
        Mtemp[3 * i + 2] = scale * vec.z;
    }

    if (!embed_convolution) {
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
            INT4m rxydim = ODTV_VECSIZE * rdimx*rdimy;
            INT4m cxydim = ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy;
            for (INT4m m = 0; m < rdimz; ++m) {
                // x-direction transforms in plane "m"
                fftx.ForwardRealToComplexFFT(Mtemp + m*rxydim, Hxfrm + m * cxydim);
                // y-direction transforms in plane "m"
                ffty.ForwardFFT(Hxfrm + m * cxydim);
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



        const INT4m jstride = ODTV_COMPLEXSIZE * ODTV_VECSIZE*cdimx;
        const INT4m kstride = jstride*cdimy;
        const INT4m ajstride = adimx;
        const INT4m akstride = ajstride*adimy;
        for (k = 0; k < adimz; ++k) {
            // k>=0
            INT4m kindex = k*kstride;
            INT4m akindex = k*akstride;
            for (j = 0; j < adimy; ++j) {
                // j>=0, k>=0
                INT4m jindex = kindex + j*jstride;
                INT4m ajindex = akindex + j*ajstride;
                for (i = 0; i < cdimx; ++i) {
                    INT4m index = ODTV_COMPLEXSIZE * ODTV_VECSIZE * i + jindex;
                    OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
                    OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index + 1];
                    OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index + 2];
                    OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index + 3];
                    OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index + 4];
                    OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index + 5];

                    const A_coefs& Aref = A[ajindex + i];

                    Hxfrm[index] = Aref.A00 * Hx_re + Aref.A01 * Hy_re + Aref.A02*Hz_re;
                    Hxfrm[index + 1] = Aref.A00 * Hx_im + Aref.A01 * Hy_im + Aref.A02*Hz_im;
                    Hxfrm[index + 2] = Aref.A01 * Hx_re + Aref.A11 * Hy_re + Aref.A12*Hz_re;
                    Hxfrm[index + 3] = Aref.A01 * Hx_im + Aref.A11 * Hy_im + Aref.A12*Hz_im;
                    Hxfrm[index + 4] = Aref.A02 * Hx_re + Aref.A12 * Hy_re + Aref.A22*Hz_re;
                    Hxfrm[index + 5] = Aref.A02 * Hx_im + Aref.A12 * Hy_im + Aref.A22*Hz_im;
                }
            }
            for (j = adimy; j < cdimy; ++j) {
                // j<0, k>=0
                INT4m jindex = kindex + j*jstride;
                INT4m ajindex = akindex + (cdimy - j) * ajstride;
                for (i = 0; i < cdimx; ++i) {
                    INT4m index = ODTV_COMPLEXSIZE * ODTV_VECSIZE * i + jindex;
                    OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
                    OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index + 1];
                    OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index + 2];
                    OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index + 3];
                    OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index + 4];
                    OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index + 5];

                    const A_coefs& Aref = A[ajindex + i];

                    // Flip signs on a01 and a12 as compared to the j>=0
                    // case because a01 and a12 are odd in y.
                    Hxfrm[index] = Aref.A00 * Hx_re - Aref.A01 * Hy_re + Aref.A02*Hz_re;
                    Hxfrm[index + 1] = Aref.A00 * Hx_im - Aref.A01 * Hy_im + Aref.A02*Hz_im;
                    Hxfrm[index + 2] = -Aref.A01 * Hx_re + Aref.A11 * Hy_re - Aref.A12*Hz_re;
                    Hxfrm[index + 3] = -Aref.A01 * Hx_im + Aref.A11 * Hy_im - Aref.A12*Hz_im;
                    Hxfrm[index + 4] = Aref.A02 * Hx_re - Aref.A12 * Hy_re + Aref.A22*Hz_re;
                    Hxfrm[index + 5] = Aref.A02 * Hx_im - Aref.A12 * Hy_im + Aref.A22*Hz_im;
                }
            }
        }
        for (k = adimz; k < cdimz; ++k) {
            // k<0
            INT4m kindex = k*kstride;
            INT4m akindex = (cdimz - k) * akstride;
            for (j = 0; j < adimy; ++j) {
                // j>=0, k<0
                INT4m jindex = kindex + j*jstride;
                INT4m ajindex = akindex + j*ajstride;
                for (i = 0; i < cdimx; ++i) {
                    INT4m index = ODTV_COMPLEXSIZE * ODTV_VECSIZE * i + jindex;
                    OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
                    OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index + 1];
                    OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index + 2];
                    OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index + 3];
                    OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index + 4];
                    OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index + 5];

                    const A_coefs& Aref = A[ajindex + i];

                    // Flip signs on a02 and a12 as compared to the k>=0, j>=0 case
                    // because a02 and a12 are odd in z.
                    Hxfrm[index] = Aref.A00 * Hx_re + Aref.A01 * Hy_re - Aref.A02*Hz_re;
                    Hxfrm[index + 1] = Aref.A00 * Hx_im + Aref.A01 * Hy_im - Aref.A02*Hz_im;
                    Hxfrm[index + 2] = Aref.A01 * Hx_re + Aref.A11 * Hy_re - Aref.A12*Hz_re;
                    Hxfrm[index + 3] = Aref.A01 * Hx_im + Aref.A11 * Hy_im - Aref.A12*Hz_im;
                    Hxfrm[index + 4] = -Aref.A02 * Hx_re - Aref.A12 * Hy_re + Aref.A22*Hz_re;
                    Hxfrm[index + 5] = -Aref.A02 * Hx_im - Aref.A12 * Hy_im + Aref.A22*Hz_im;
                }
            }
            for (j = adimy; j < cdimy; ++j) {
                // j<0, k<0
                INT4m jindex = kindex + j*jstride;
                INT4m ajindex = akindex + (cdimy - j) * ajstride;
                for (i = 0; i < cdimx; ++i) {
                    INT4m index = ODTV_COMPLEXSIZE * ODTV_VECSIZE * i + jindex;
                    OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
                    OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index + 1];
                    OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index + 2];
                    OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index + 3];
                    OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index + 4];
                    OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index + 5];

                    const A_coefs& Aref = A[ajindex + i];

                    // Flip signs on a01 and a02 as compared to the k>=0, j>=0 case
                    // because a01 is odd in y and even in z,
                    //     and a02 is odd in z and even in y.
                    // No change to a12 because it is odd in both y and z.
                    Hxfrm[index] = Aref.A00 * Hx_re - Aref.A01 * Hy_re - Aref.A02*Hz_re;
                    Hxfrm[index + 1] = Aref.A00 * Hx_im - Aref.A01 * Hy_im - Aref.A02*Hz_im;
                    Hxfrm[index + 2] = -Aref.A01 * Hx_re + Aref.A11 * Hy_re + Aref.A12*Hz_re;
                    Hxfrm[index + 3] = -Aref.A01 * Hx_im + Aref.A11 * Hy_im + Aref.A12*Hz_im;
                    Hxfrm[index + 4] = -Aref.A02 * Hx_re + Aref.A12 * Hy_re + Aref.A22*Hz_re;
                    Hxfrm[index + 5] = -Aref.A02 * Hx_im + Aref.A12 * Hy_im + Aref.A22*Hz_im;
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
            INT4m m;
            INT4m rxydim = ODTV_VECSIZE * rdimx*rdimy;
            INT4m cxydim = ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy;
            assert(3*sizeof(OXS_FFT_REAL_TYPE)<=sizeof(ThreeVector));
            OXS_FFT_REAL_TYPE* fptr
                    = static_cast<OXS_FFT_REAL_TYPE*> (static_cast<void*> (&field[0]));
            fftz.InverseFFT(Hxfrm); // z-direction transforms
            for (m = 0; m < rdimz; ++m) {
                // y-direction transforms
                ffty.InverseFFT(Hxfrm + m * cxydim);
                // x-direction transforms
                fftx.InverseComplexToRealFFT(Hxfrm + m*cxydim, fptr + m * rxydim);
            }

            if (3 * sizeof (OXS_FFT_REAL_TYPE)<sizeof (ThreeVector)) {
                // The fftx.InverseComplexToRealFFT calls above assume the
                // target is an array of OXS_FFT_REAL_TYPE.  If ThreeVector is
                // not tightly packed, then this assumption is false; however we
                // can correct the problem by expanding the results in-place.
                // The only setting I know of where ThreeVector doesn't tight
                // pack is under the Borland bcc32 compiler on Windows x86 with
                // OXS_FFT_REAL_TYPE equal to "long double".  In that case
                // sizeof(long double) == 10, but sizeof(ThreeVector) == 36.
                for (m = rsize - 1; m >= 0; --m) {
                    ThreeVector temp(fptr[ODTV_VECSIZE * m], fptr[ODTV_VECSIZE * m + 1], fptr[ODTV_VECSIZE * m + 2]);
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
            INT4m rxydim = ODTV_VECSIZE * rdimx*rdimy;
            INT4m cxydim = ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy;
            for (INT4m m = 0; m < rdimz; ++m) {
                // x-direction transforms in plane "m"
#if REPORT_TIME
                fftxforwardtime.Start();
#endif // REPORT_TIME
                fftx.ForwardRealToComplexFFT(Mtemp + m*rxydim, Hxfrm + m * cxydim);
#if REPORT_TIME
                fftxforwardtime.Stop();
                fftyforwardtime.Start();
#endif // REPORT_TIME
                // y-direction transforms in plane "m"
                ffty.ForwardFFT(Hxfrm + m * cxydim);
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
        const INT4m jstride = ODTV_COMPLEXSIZE * ODTV_VECSIZE*cdimx;
        const INT4m kstride = jstride*cdimy;
        const INT4m ajstride = adimx;
        const INT4m akstride = ajstride*adimy;

        for (j = 0; j < adimy; ++j) {
            // j>=0
            INT4m jindex = j*jstride;
            INT4m ajindex = j*ajstride;
            fftz.AdjustArrayCount(ODTV_VECSIZE * embed_block_size);
            for (INT4m m = 0; m < cdimx; m += embed_block_size) {
                // Do one block of forward z-direction transforms
                INT4m istop = m + embed_block_size;
                if (embed_block_size > cdimx - m) {
                    // Partial block
                    fftz.AdjustArrayCount(ODTV_VECSIZE * (cdimx - m));
                    istop = cdimx;
                }
                fftz.ForwardFFT(Hxfrm + jindex + m * ODTV_COMPLEXSIZE * ODTV_VECSIZE);
                // Do matrix-vector multiply ("convolution") for block
                for (k = 0; k < adimz; ++k) {
                    // j>=0, k>=0
                    INT4m kindex = jindex + k*kstride;
                    INT4m akindex = ajindex + k*akstride;
                    for (i = m; i < istop; ++i) {
                        INT4m index = ODTV_COMPLEXSIZE * ODTV_VECSIZE * i + kindex;
                        OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
                        OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index + 1];
                        OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index + 2];
                        OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index + 3];
                        OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index + 4];
                        OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index + 5];

                        const A_coefs& Aref = A[akindex + i];

                        Hxfrm[index] = Aref.A00 * Hx_re + Aref.A01 * Hy_re + Aref.A02*Hz_re;
                        Hxfrm[index + 1] = Aref.A00 * Hx_im + Aref.A01 * Hy_im + Aref.A02*Hz_im;
                        Hxfrm[index + 2] = Aref.A01 * Hx_re + Aref.A11 * Hy_re + Aref.A12*Hz_re;
                        Hxfrm[index + 3] = Aref.A01 * Hx_im + Aref.A11 * Hy_im + Aref.A12*Hz_im;
                        Hxfrm[index + 4] = Aref.A02 * Hx_re + Aref.A12 * Hy_re + Aref.A22*Hz_re;
                        Hxfrm[index + 5] = Aref.A02 * Hx_im + Aref.A12 * Hy_im + Aref.A22*Hz_im;
                    }
                }
                for (k = adimz; k < cdimz; ++k) {
                    // j>=0, k<0
                    INT4m kindex = jindex + k*kstride;
                    INT4m akindex = ajindex + (cdimz - k) * akstride;
                    for (i = m; i < istop; ++i) {
                        INT4m index = ODTV_COMPLEXSIZE * ODTV_VECSIZE * i + kindex;
                        OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
                        OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index + 1];
                        OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index + 2];
                        OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index + 3];
                        OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index + 4];
                        OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index + 5];

                        const A_coefs& Aref = A[akindex + i];

                        // Flip signs on a02 and a12 as compared to the k>=0, j>=0 case
                        // because a02 and a12 are odd in z.
                        Hxfrm[index] = Aref.A00 * Hx_re + Aref.A01 * Hy_re - Aref.A02*Hz_re;
                        Hxfrm[index + 1] = Aref.A00 * Hx_im + Aref.A01 * Hy_im - Aref.A02*Hz_im;
                        Hxfrm[index + 2] = Aref.A01 * Hx_re + Aref.A11 * Hy_re - Aref.A12*Hz_re;
                        Hxfrm[index + 3] = Aref.A01 * Hx_im + Aref.A11 * Hy_im - Aref.A12*Hz_im;
                        Hxfrm[index + 4] = -Aref.A02 * Hx_re - Aref.A12 * Hy_re + Aref.A22*Hz_re;
                        Hxfrm[index + 5] = -Aref.A02 * Hx_im - Aref.A12 * Hy_im + Aref.A22*Hz_im;
                    }
                }
                // Do inverse z-direction transforms for block
                fftz.InverseFFT(Hxfrm + jindex + m * ODTV_COMPLEXSIZE * ODTV_VECSIZE);
            }
        }
        for (j = adimy; j < cdimy; ++j) {
            // j<0
            INT4m jindex = j*jstride;
            INT4m ajindex = (cdimy - j) * ajstride;
            fftz.AdjustArrayCount(ODTV_VECSIZE * embed_block_size);
            for (INT4m m = 0; m < cdimx; m += embed_block_size) {
                // Do one block of forward z-direction transforms
                INT4m istop = m + embed_block_size;
                if (embed_block_size > cdimx - m) {
                    // Partial block
                    fftz.AdjustArrayCount(ODTV_VECSIZE * (cdimx - m));
                    istop = cdimx;
                }
                fftz.ForwardFFT(Hxfrm + jindex + m * ODTV_COMPLEXSIZE * ODTV_VECSIZE);
                // Do matrix-vector multiply ("convolution") for block
                for (k = 0; k < adimz; ++k) {
                    // j<0, k>=0
                    INT4m kindex = jindex + k*kstride;
                    INT4m akindex = ajindex + k*akstride;
                    for (i = m; i < istop; ++i) {
                        INT4m index = ODTV_COMPLEXSIZE * ODTV_VECSIZE * i + kindex;
                        OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
                        OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index + 1];
                        OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index + 2];
                        OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index + 3];
                        OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index + 4];
                        OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index + 5];

                        const A_coefs& Aref = A[akindex + i];

                        // Flip signs on a01 and a12 as compared to the j>=0
                        // case because a01 and a12 are odd in y.
                        Hxfrm[index] = Aref.A00 * Hx_re - Aref.A01 * Hy_re + Aref.A02*Hz_re;
                        Hxfrm[index + 1] = Aref.A00 * Hx_im - Aref.A01 * Hy_im + Aref.A02*Hz_im;
                        Hxfrm[index + 2] = -Aref.A01 * Hx_re + Aref.A11 * Hy_re - Aref.A12*Hz_re;
                        Hxfrm[index + 3] = -Aref.A01 * Hx_im + Aref.A11 * Hy_im - Aref.A12*Hz_im;
                        Hxfrm[index + 4] = Aref.A02 * Hx_re - Aref.A12 * Hy_re + Aref.A22*Hz_re;
                        Hxfrm[index + 5] = Aref.A02 * Hx_im - Aref.A12 * Hy_im + Aref.A22*Hz_im;
                    }
                }
                for (k = adimz; k < cdimz; ++k) {
                    // j<0, k<0
                    INT4m kindex = jindex + k*kstride;
                    INT4m akindex = ajindex + (cdimz - k) * akstride;
                    for (i = m; i < istop; ++i) {
                        INT4m index = ODTV_COMPLEXSIZE * ODTV_VECSIZE * i + kindex;
                        OXS_FFT_REAL_TYPE Hx_re = Hxfrm[index];
                        OXS_FFT_REAL_TYPE Hx_im = Hxfrm[index + 1];
                        OXS_FFT_REAL_TYPE Hy_re = Hxfrm[index + 2];
                        OXS_FFT_REAL_TYPE Hy_im = Hxfrm[index + 3];
                        OXS_FFT_REAL_TYPE Hz_re = Hxfrm[index + 4];
                        OXS_FFT_REAL_TYPE Hz_im = Hxfrm[index + 5];

                        const A_coefs& Aref = A[akindex + i];

                        // Flip signs on a01 and a02 as compared to the k>=0, j>=0 case
                        // because a01 is odd in y and even in z,
                        //     and a02 is odd in z and even in y.
                        // No change to a12 because it is odd in both y and z.
                        Hxfrm[index] = Aref.A00 * Hx_re - Aref.A01 * Hy_re - Aref.A02*Hz_re;
                        Hxfrm[index + 1] = Aref.A00 * Hx_im - Aref.A01 * Hy_im - Aref.A02*Hz_im;
                        Hxfrm[index + 2] = -Aref.A01 * Hx_re + Aref.A11 * Hy_re + Aref.A12*Hz_re;
                        Hxfrm[index + 3] = -Aref.A01 * Hx_im + Aref.A11 * Hy_im + Aref.A12*Hz_im;
                        Hxfrm[index + 4] = -Aref.A02 * Hx_re + Aref.A12 * Hy_re + Aref.A22*Hz_re;
                        Hxfrm[index + 5] = -Aref.A02 * Hx_im + Aref.A12 * Hy_im + Aref.A22*Hz_im;
                    }
                }
                // Do inverse z-direction transforms for block
                fftz.InverseFFT(Hxfrm + jindex + m * ODTV_COMPLEXSIZE * ODTV_VECSIZE);
            }
        }
#if REPORT_TIME
        convtime.Stop();
#endif // REPORT_TIME

        // Do inverse y- and x-axis FFTs, to complete transform back into
        // space domain.
        {
            INT4m m;
            INT4m rxydim = ODTV_VECSIZE * rdimx*rdimy;
            INT4m cxydim = ODTV_COMPLEXSIZE * ODTV_VECSIZE * cdimx*cdimy;
            assert(3*sizeof(OXS_FFT_REAL_TYPE)<=sizeof(ThreeVector));
            OXS_FFT_REAL_TYPE* fptr
                    = static_cast<OXS_FFT_REAL_TYPE*> (static_cast<void*> (&field[0]));
            for (m = 0; m < rdimz; ++m) {
                // y-direction transforms
#if REPORT_TIME
                fftyinversetime.Start();
#endif // REPORT_TIME
                ffty.InverseFFT(Hxfrm + m * cxydim);
#if REPORT_TIME
                fftyinversetime.Stop();
                fftxinversetime.Start();
#endif // REPORT_TIME
                // x-direction transforms
                fftx.InverseComplexToRealFFT(Hxfrm + m*cxydim, fptr + m * rxydim);
#if REPORT_TIME
                fftxinversetime.Stop();
#endif // REPORT_TIME
            }

            if (3 * sizeof (OXS_FFT_REAL_TYPE)<sizeof (ThreeVector)) {
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
                for (m = rsize - 1; m >= 0; --m) {
                    ThreeVector temp(fptr[ODTV_VECSIZE * m], fptr[ODTV_VECSIZE * m + 1], fptr[ODTV_VECSIZE * m + 2]);
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
    const OXS_FFT_REAL_TYPE emult = -0.5 * MU0;
    for (i = 0; i < rsize; ++i) {
        OXS_FFT_REAL_TYPE dot = spin[i] * field[i];
        energy[i] = emult * dot * Ms[i];
    }
#if REPORT_TIME
    dottime.Stop();
#endif // REPORT_TIME

}

void PBC_Demag_2D::LoadPbcDemagTensor(
        const Oxs_RectangularMesh* mesh
        ) const {

    xdim = mesh->DimX();
    ydim = mesh->DimY();
    zdim = mesh->DimZ();

    Npbc_diag.AdjustSize(mesh);
    Npbc_offdiag.AdjustSize(mesh);

    load_from_file_success = 0;
    if (tensor_file_name.length() > 0) {

        String diagname = tensor_file_name;
        diagname += String("-diag.ovf");
        String offdiagname = tensor_file_name;
        offdiagname += String("-offdiag.ovf");

        Vf_FileInput* vffi = NULL;
        Vf_Mesh* file_mesh = NULL;

        // Ncorr_diag
        Nb_DString dummy;
        vffi = Vf_FileInput::NewReader(diagname.c_str(), &dummy);
        if (vffi != NULL) {
            file_mesh = vffi->NewMesh();
            delete vffi;
            if (file_mesh == NULL || !mesh->IsCompatible(file_mesh)) {
                if (file_mesh != NULL) delete file_mesh;
            } else {
                mesh->FillMeshValueExact(file_mesh, Npbc_diag);
                delete file_mesh;

                // Ncorr_offdiag
                vffi = Vf_FileInput::NewReader(offdiagname.c_str(), &dummy);
                if (vffi != NULL) {
                    file_mesh = vffi->NewMesh();
                    delete vffi;
                    if (file_mesh == NULL || !mesh->IsCompatible(file_mesh)) {
                        if (file_mesh != NULL) delete file_mesh;
                    } else {
                        mesh->FillMeshValueExact(file_mesh, Npbc_offdiag);
                        delete file_mesh;
                        load_from_file_success = 1;
                    }
                }
            }
        }
    }

}

void PBC_Demag_2D::SavePbcDemagTensor(
        const Oxs_Mesh * mesh
        ) const {

    if (tensor_file_name.length() > 0 && !load_from_file_success) {
        // Oxs_MeshValue<ThreeVector> Ncorr_diag;
        // Oxs_MeshValue<ThreeVector> Ncorr_offdiag;
        String diagname = tensor_file_name;
        diagname += String("-diag.ovf");
        String offdiagname = tensor_file_name;
        offdiagname += String("-offdiag.ovf");


        mesh->WriteOvf(diagname.c_str(), 1,
                "N-diag",
                "PBC_Demag_2D::DemagTensors:"
                " Nxx, Nyy, Nzz",
                "1", "rectangular", "binary", "8", &Npbc_diag, NULL);
        mesh->WriteOvf(offdiagname.c_str(), 1,
                "N-offdiag",
                "PBC_Demag_2D::DemagTensors:"
                " Nxy, Nxz, Nyz",
                "1", "rectangular", "binary", "8", &Npbc_offdiag, NULL);
    }

}

void PBC_Demag_2D::CalculateDemagTensors(
        const Oxs_RectangularMesh* mesh
        ) const {

    ReleaseMemory();

    Npbc_diag.AdjustSize(mesh);
    Npbc_offdiag.AdjustSize(mesh);

    REALWIDE dx = mesh->EdgeLengthX();
    REALWIDE dy = mesh->EdgeLengthY();
    REALWIDE dz = mesh->EdgeLengthZ();

    xdim = mesh->DimX();
    ydim = mesh->DimY();
    zdim = mesh->DimZ();
    UINT4m xydim = xdim*ydim;


    REALWIDE maxedge = dx;
    if (dy > maxedge) maxedge = dy;
    if (dz > maxedge) maxedge = dz;
    dx /= maxedge;
    dy /= maxedge;
    dz /= maxedge;


    REALWIDE x, y, z;

    int gxx, gyy, gzz;


    gxx = sample_repeat_nx;
    gyy = sample_repeat_ny;

    if (gxx >= 0 && gyy >= 0) {

        UINT4m index, i, j, k;

        for (k = 0; k < zdim; k++) {
            z = k*dz;
            for (j = 0; j < ydim; j++) {
                y = j*dy;
                for (i = 0; i < xdim; i++) {
                    x = i*dx;
                    index = k * xydim + j * xdim + i;
                    Npbc_diag[index].x = CalculateSingleTensorFinitely(xx, gxx, gyy, x, y, z, dx, dy, dz);
                    Npbc_diag[index].y = CalculateSingleTensorFinitely(yy, gxx, gyy, x, y, z, dx, dy, dz);
                    Npbc_diag[index].z = CalculateSingleTensorFinitely(zz, gxx, gyy, x, y, z, dx, dy, dz);
                    Npbc_offdiag[index].x = CalculateSingleTensorFinitely(xy, gxx, gyy, x, y, z, dx, dy, dz);
                    Npbc_offdiag[index].y = CalculateSingleTensorFinitely(xz, gxx, gyy, x, y, z, dx, dy, dz);
                    Npbc_offdiag[index].z = CalculateSingleTensorFinitely(yz, gxx, gyy, x, y, z, dx, dy, dz);
                }
            }
        }

    } else {
        gxx = FindG(xx, dx * dy*dz, xdim*dx, ydim * dy);
        gyy = FindG(yy, dx * dy*dz, xdim*dx, ydim * dy);
        gzz = FindG(zz, dx * dy*dz, xdim*dx, ydim * dy);


        UINT4m index, i, j, k;

        for (k = 0; k < zdim; k++) {
            z = k*dz;
            for (j = 0; j < ydim; j++) {
                y = j*dy;
                for (i = 0; i < xdim; i++) {
                    x = i*dx;
                    index = k * xydim + j * xdim + i;
                    Npbc_diag[index].x = CalculateSingleTensor(xx, gxx, x, y, z, dx, dy, dz);
                    Npbc_diag[index].y = CalculateSingleTensor(yy, gyy, x, y, z, dx, dy, dz);
                    //  Npbc_diag[index].z = CalculateSingleTensor(zz,gzz,x,y,z,dx,dy,dz);
                    Npbc_diag[index].z = -(Npbc_diag[index].x + Npbc_diag[index].y);
                    Npbc_offdiag[index].x = CalculateSingleTensor(xy, gxx, x, y, z, dx, dy, dz);
                    Npbc_offdiag[index].y = CalculateSingleTensor(xz, gxx, x, y, z, dx, dy, dz);
                    Npbc_offdiag[index].z = CalculateSingleTensor(yz, gxx, x, y, z, dx, dy, dz);
                }
            }
        }

        Npbc_diag[0].z += 1.0;


    }
    //printf("gxx=%d  gyy=%d  gzz=%d\n", gxx, gyy, gzz);



}

int PBC_Demag_2D::FindG(
        enum TensorComponent comp,
        REALWIDE v, REALWIDE Tx, REALWIDE Ty
        ) const {

    REALWIDE tmp;

    switch (comp) {
        case xy:
        case xz:
        case yz:
        case xx:
            tmp = v / (4 * PI * (Tx * Tx) * sqrt(Tx * Tx + Ty * Ty) * pbc_2d_error);
            return (int) pow(tmp, 1 / 3.0) + 1;
        case yy:
            tmp = v / (4 * PI * (Ty * Ty) * sqrt(Tx * Tx + Ty * Ty) * pbc_2d_error);
            return (int) pow(tmp, 1 / 3.0) + 1;
        case zz:
            tmp = v * sqrt(Tx * Tx + Ty * Ty) / (4 * PI * (Tx * Ty * Tx * Ty) * pbc_2d_error);
            return (int) pow(tmp, 1 / 3.0) + 1;
    }
}

REALWIDE PBC_Demag_2D::CalculateSingleTensor(
        enum TensorComponent comp, int g, REALWIDE x, REALWIDE y, REALWIDE z,
        REALWIDE a, REALWIDE b, REALWIDE c
        ) const {

    if ((comp == xy || comp == xz || comp == yz) && x * y == 0) return 0.0;

    REALWIDE Tx = xdim*a, Ty = ydim*b, cof1 = 1 / (4 * PI * a * b * c), cof2 = a * b * c / (4 * PI);
    REALWIDE* tmpx = new REALWIDE[2 * g + 2];
    REALWIDE* tmpy = new REALWIDE[2 * g + 1];
    REALWIDE tpx, tpy, radius_sq;
    for (int i = -g; i <= g; i++) {
        for (int j = -g; j <= g; j++) {
            tpx = x + i*Tx;
            tpy = y + j*Ty;
            radius_sq = tpx * tpx + tpy * tpy + z*z;
            if (radius_sq < asymptotic_radius_sq) {
                tmpy[j + g] = DemagTensorNormal(comp, tpx, tpy, z, a, b, c) * cof1;
            } else if (radius_sq < dipolar_radius_sq) {
                tmpy[j + g] = DemagTensorAsymptotic(comp, tpx, tpy, z, a, b, c);
            } else {
                //printf("%f\n", radius_sq);
                tmpy[j + g] = DemagTensorDipolar(comp, tpx, tpy, z) * cof2;
            }
        }
        tmpx[i + g] = AccurateSum(2 * g + 1, tmpy);
    }

    REALWIDE X0 = (g + 0.5) * Tx;
    REALWIDE Y0 = (g + 0.5) * Ty;

    tmpx[2 * g + 1] = DemagTensorInfinite(comp, x, y, z, X0, Y0) * cof2 / (Tx * Ty);

    REALWIDE result = AccurateSum(2 * g + 2, tmpx);

    delete[] tmpx;
    delete[] tmpy;

    return result;
}

REALWIDE PBC_Demag_2D::CalculateSingleTensorFinitely(
        enum TensorComponent comp, int gx, int gy, REALWIDE x, REALWIDE y, REALWIDE z,
        REALWIDE a, REALWIDE b, REALWIDE c
        ) const {

    if ((comp == xy || comp == xz || comp == yz) && x * y == 0) return 0.0;

    REALWIDE Tx = xdim*a, Ty = ydim*b, cof1 = 1 / (4 * PI * a * b * c), cof2 = a * b * c / (4 * PI);
    REALWIDE* tmpx = new REALWIDE[2 * gx + 1];
    REALWIDE* tmpy = new REALWIDE[2 * gy + 1];
    REALWIDE tpx, tpy, radius_sq;
    for (int i = -gx; i <= gx; i++) {
        for (int j = -gy; j <= gy; j++) {
            tpx = x + i*Tx;
            tpy = y + j*Ty;
            radius_sq = tpx * tpx + tpy * tpy + z*z;
            if (radius_sq < asymptotic_radius_sq) {
                tmpy[j + gy] = DemagTensorNormal(comp, tpx, tpy, z, a, b, c) * cof1;
            } else if (radius_sq < dipolar_radius_sq) {
                tmpy[j + gy] = DemagTensorAsymptotic(comp, tpx, tpy, z, a, b, c);
            } else {
                // printf("%f\n", radius_sq);
                tmpy[j + gy] = DemagTensorDipolar(comp, tpx, tpy, z) * cof2;
            }
        }
        tmpx[i + gx] = AccurateSum(2 * gy + 1, tmpy);
    }

    REALWIDE X0 = (gx + 0.5) * Tx;
    REALWIDE Y0 = (gy + 0.5) * Ty;

    //  tmpx[2 * gx + 1] = DemagTensorInfinite(comp, x, y, z, X0, Y0) * cof2 / (Tx * Ty);

    REALWIDE result = AccurateSum(2 * gx + 1, tmpx);

    delete[] tmpx;
    delete[] tmpy;

    return result;
}

REAL8m PBC_Demag_2D::GetTensorFromBuffer(
        enum TensorComponent comp, int i, int j, int k
        ) const {

    int index = xdim * ydim * k + xdim * j + i;

    switch (comp) {
        case xx:
            return Npbc_diag[index].x;
        case yy:
            return Npbc_diag[index].y;
        case zz:
            return Npbc_diag[index].z;
        case xy:
            return Npbc_offdiag[index].x;
        case xz:
            return Npbc_offdiag[index].y;
        case yz:
            return Npbc_offdiag[index].z;
    }

}



#endif // OOMMF_THREADS
