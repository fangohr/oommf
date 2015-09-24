/* FILE: demag.h            -*-Mode: c++-*-
 * version 2.0
 *
 * Average H demag field across rectangular cells.  This is a modified
 * version of the simpledemag class, which uses symmetries in the
 * interaction coefficients to reduce memory usage.
 *
 * This file is based on appriopriate Oxs_Demag file, which was written
 * by Mike Donahue as part of the OOMMF package.
 * For the latest information about OOMMF software, see 
 * <URL:http://math.nist.gov/oommf/>.
 *
 * In 2006 the Periodic Boundary Condition (PBC) functionality
 * was added by Kristof Lebecki (nick KL(m)).
 * With large help of Mike Donahue, of course.
 * See the kl_pbc_util.h file for more information.
 *
 */

#ifndef _KLM_DEMAG_PBC
#define _KLM_DEMAG_PBC

#include "energy.h"
#include "fft.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"

/* End includes */

class Klm_Demag_PBC:public Oxs_Energy {
private:
#if REPORT_TIME
  mutable Nb_StopWatch ffttime;
  mutable Nb_StopWatch convtime;
  mutable Nb_StopWatch dottime;
#endif // REPORT_TIME

  // KL(m) parameters
  // Repetition distance for z-direction. In cell meters.
  // If 0.0 (or absent): use sample z-length
  OC_REAL4m zPeriod_param;
  
  // Fitted accurancy/error (absolute) of the calculations: "image" & "dipole".
  // The errors are fitted with following function (r is the distance cell-cell):
  //      A * r^p
  // You should put a value here basing on previous results analysis.
  // Different values for diagonal (like nXX) and off-diag elements (like nYZ)
  OC_REAL4m error_A_img_diag;  
//  OC_REAL4m error_p_img_diag;  we fix it
  OC_REAL4m error_A_dip_diag;  
//  OC_REAL4m error_p_dip_diag;  
  OC_REAL4m error_A_img_OFFdiag;  
//  OC_REAL4m error_p_img_OFFdiag;  
  OC_REAL4m error_A_dip_OFFdiag;  
//  OC_REAL4m error_p_dip_OFFdiag;  
  
  // The summation will be done until the *cont* error is below a certain 
  // fraction of the actual tensor value:
  //     error_inf_tail < err_ratio_cont_to_N * N
  OC_REAL4m err_ratio_cont_to_N;  
  // Safety stopper in that case (for instance in the case where N==0)
  OC_INDEX max_no_sum_elements;  
  
  // Fixed number of repetitions in the summation.
  // If set (i.e, non-zero), then this criteria is used
  // instead of the err_ratio_cont_to_N checking
  OC_INDEX no_of_sum_elements;  
  // Include inifinite tails computation?
  // This is evidently for debugging purposes only
  OC_BOOL include_inf_tails;
  
  // Tensor file storage for future use.
  // The tensor will be kept in two files differing by ID (1 or 2). 
  // 1. If the tensor_file_name is a directory (ends with a slash "/"),
  //    then the files will we writen in this path 
  //    using a geometry-based pattern to distinguish them.
  // 2. Otherwise the files will be created basing on a scheme:
  //    tensor_file_name<ID>tensor_file_suffix
  // Allways the ID will be placed before tensor_file_suffix.
  String tensor_file_name;
  String tensor_file_suffix;
  // Script name for showing the progress. With full path.
  // If epmty/not-present: no progress show.
  String progress_script;
  // Want to speed up the calculations by reducing the z-size 
  // of the evaluated tensor?
  // This can be done only for the case when zdim=NextPowerOfTwo(zim)
  // Choosing 'always' will cause error in other cases
  // Default: 'sometimes' (i.e. when possible)
  // Only implemented for SimpleDemag, so far.
// String turbo; 
  // to perform deeper statistics, or not?
  OC_BOOL compute_stats;
  // End of parameters.
  // KL(m) end

  mutable OC_INDEX rdimx; // Natural size of real data
  mutable OC_INDEX rdimy; // Digital Mars compiler wants these as separate
  mutable OC_INDEX rdimz; //    statements, because of "mutable" keyword.
  mutable OC_INDEX cdimx; // Full size of complex data
  mutable OC_INDEX cdimy;
  mutable OC_INDEX cdimz;
  // 2*cdimx>=rdimx, cdimy>=rdimy, cdimz>=rdimz
  // cdim[xyz] should be powers of 2.
  mutable OC_INDEX cstridey; // Strides across complex data
  mutable OC_INDEX cstridez;
  // cstridey>=cdimx, cstridez>=cdimy*cstridey
  // The stride sizes for the real arrays are just double the
  // complex strides, except cstride1 and rstride1 are assumed
  // to be 1.  Total matrix size is effectively cdimz*cstridez
  // Oxs_Complex elements, or twice that many "double" elements.

  mutable OC_UINT4m mesh_id;

  // The A## arrays hold demag coefficients, transformed into
  // frequency domain.  These are held long term.  xcomp,
  // ycomp, and zcomp are used as temporary space, first to hold
  // the transforms of Mx, My, and Mz, then to store Hx, Hy, and
  // Hz.
  mutable OC_INDEX adimx;
  mutable OC_INDEX adimy;
  mutable OC_INDEX adimz;
  mutable OC_INDEX astridey;
  mutable OC_INDEX astridez;
  mutable OXS_COMPLEX_REAL_TYPE *A00;
  mutable OXS_COMPLEX_REAL_TYPE *A01;
  mutable OXS_COMPLEX_REAL_TYPE *A02;
  mutable OXS_COMPLEX_REAL_TYPE *A11;
  mutable OXS_COMPLEX_REAL_TYPE *A12;
  mutable OXS_COMPLEX_REAL_TYPE *A22;
  mutable Oxs_Complex *xcomp;
  mutable Oxs_Complex *ycomp;
  mutable Oxs_Complex *zcomp;
  mutable Oxs_FFT3D fft; // All transforms are same size, so we need
  /// only one Oxs_FFT3D object.

  void (Klm_Demag_PBC::*fillcoefs)(const Oxs_Mesh*) const;
  void FillCoefficientArraysFast(const Oxs_Mesh* mesh) const;
  void FillCoefficientArraysStandard(const Oxs_Mesh* mesh) const;
  /// The "standard" variant is simpler but slower, and is retained
  /// mainly for testing and development purposes.

  OC_INDEX NextPowerOfTwo(OC_INDEX n) const;  // Helper function
  void ReleaseMemory() const;

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Klm_Demag_PBC(const char* name,     // Child instance id
	    Oxs_Director* newdtr, // App director
	    const char* argstr);  // MIF input block parameters
  virtual ~Klm_Demag_PBC();
  virtual OC_BOOL Init();
};


#endif // _KLM_DEMAG_PBC
