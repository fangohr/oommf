/* FILE: kl_pbc_util.h            -*-Mode: c++-*-
 * version 2.0
 *
 * This file was inspired by the OOMMF code.
 * For the latest information about OOMMF software, see 
 * <URL:http://math.nist.gov/oommf/>.
 *
 * Defined here structures are used by the periodic boundary conditions (PBC) 
 * demagnetization modules (Klm_SimpleDemag_PBC, Klm_Demag_PBC).
 * N means demagnetization tensor (see paper of Newell [1])
 * You may also look our paper that will be hopefully soon available.
 [1] "A Generalization of the Demagnetizing Tensor for Nonuniform
     Magnetization," by Andrew J. Newell, Wyn Williams, and David
     J. Dunlop, Journal of Geophysical Research - Solid Earth, vol 98,
     p 9551-9555, June 1993.
 [2] "Periodic boundary conditions for demagnetization interactions 
     in micromagnetic simulations", Kristof M. Lebecki, Michael Donahue,
     and Marek Gutowski, to be published.
 
 In 1-2006 the Periodic Boundary Condition (PBC) functionality
 was introduced by me (Kristof Lebecki, nick KL(m)).
 With large help of Mike Donahue, of course.
 The (MD's) idea was, to expand the initialization part, where original
 interactions between cells were computed using formulae and
 functions defined in [1].
 PBC concept is based on the idea that interaction between a
 cell and an (inifinite) series of cells leads to an (infinite) 
 *summation* of the coeffitients for the interaction cellC-cellS,
 where cellC is our "central cell" and cellS belongs to the
 series of "repetitions" of an originall cellC:
   	cellS(n) = Zshift(cellC,n*zPeriod),  where
 	n = -Infinity, ..., -1, 0, 1, ..., +Infinity
 	zPeriod is a "repetition period"
 The target summation, \sum _{n=-Infinity} ^{+Infinity} N(cellC,cellS),
 (N is the demagnetization tensor) will be broken into three parts:
   1. "Image", shortcut *img*
      I will often denote it as cube-cube. 
      Central part (small values of |n|) will be calculated using 
      Newells cell-cell formulae. This part (area) is limited by the 
      cell-cell formulea, which lead to increasing computational errors 
      for large cell-cell distances. 
      This critical distance / radius is defined by parameters
      >>max_celldistance_img_*<<.
      I have evaluated this parameter for Wintel platform
      (8 byte double precision), and it apears to be in the
      order of 30. In the code this parameter might be calculated
      basing on error function parameters.
   2. "Dipole", shortcut *dip*
      Near distance cellS's (medium values of |n|) are evaluated
      using the point dipole estimation.
      This area is limited for large |n| by the case where we are
      able to introduce an *cont* _approximation_ of the *dipole*
      formula. Namely, for large |n|, the error introduced 
      by *cont* (error_inf_tail) can be compared to 
      the fraction (err_ratio_cont_to_N) of the actual tensor (N):
        error_inf_tail < err_ratio_cont_to_N * N
      err_ratio_cont_to_N will be often calle later as "fidelity level"
      If this condition is fulfilled, we can stop *dip* method
      and switch to the *cont* method.
      Somehow err_ratio_cont_to_N means "number of significant digits"
      in the result, e.g. 10^-8, means 8 significent digits.
   3. "Continous", shortcut *cont*
      Other names appearing int the code are: "Summation up to 
      the infinites" or "approximation".
      This approximation is choosen in such a nice way, that 
      we know the analytical formula for the summation with
      infinite upper (or lower) limit. We know also approximate
      formulas for error of this function.
      For this *approximation*, we have choosen a contintuous 1D
      dipole density.
      We will use formulae (functions) which give us ready
      results of the summation from an finite to infinite 
      limit. 
 Each part (area) will use following functions:
   1. CalculateSDA** (defined in the OOMMF libraries)
   2. CalculateSDA**_dip
   3. CalculateSDA00_tails
  Concluding, we will sum several elements using way "1", "2"
  and add to it "infinite tails" using funtions **_tails

 KL(m), 10-20-2005
 KL(m),  2-20-2008
   
Kristof M. Lebecki, 
Institute of Physics, Polish Academy of Sciences, dept. SL-3
Al. Lotnikow 32/46, 02-668 Warsaw, POLAND
phone: (48 22) 843 66 01 ext. 3193 / 2551
fax:   (48 22) 843 13 31
WWW:   www.ifpan.edu.pl/~lebecki
 */

#include "mesh.h"
#include "rectangularmesh.h"
#include "fft.h"		// just for Oxs_Complex definition

/* End includes */

// used ex. in the CalculateSDA_PBC_really function
// tensor element indicator
enum  T_tens_el { n00, n11, n22, n01, n02, n12 }; // !! update tens_el_MAX !!
const UINT2m tens_el_MAX=6; // number of elements !! keep this value up-to-date
/* This can _maybe_ solved _somehow_ as Mike advices:
You could do something like this:
        enum T_tens_el { n00, n11, n22, n01, n02, n12 };
        mutable UINT4m n_max[n12+1];
or put an unused marker at the end of the enum list:
        enum T_tens_el { n00, n11, n22, n01, n02, n12, tens_invalid };
        mutable UINT4m n_max[tens_invalid]; */

// tensor_file relation
enum FileAccessType { WRITE, READ, NOTHING };

// execution order in the *calling* *structure*
// _SimpleDemag means that first A00 are computed (for all cells), 
//	then A11, A22, A01, A02, and A12.
// _Demag means that first A00, A01, and A02 are computed (for all cells
//	*in parallel*), then A11, A12, A22.
// This order affects:
//	- tensor file storage (done during computations and in I/O methods)
enum  T_exec_order { _SimpleDemag, _Demag }; // !! update exec_order_MAX !!
const UINT2m exec_order_MAX=2; // number of elements 
                               // !! keep this value up-to-date

class DemagPBC {
  private:
    // Constructor parameters with event. small deviations
    // meshes, both are needed
    // declared const as such are here brought
    const Oxs_Mesh* genmesh;
    const Oxs_RectangularMesh* mesh;
    
    // "input" paramters
    
    // z-period
    // in fact const (IFC), but initialized inside the constructor body
    mutable REALWIDE zPeriod;   // in relative "dz" units
    
    // Fitted accurancy/error (absolute) of the calculations: "image" & "dipole".
    // The errors are fitted with following function (r is the distance cell-cell):
    //      A * r^p
    // Instead of 'A' we will store internally some modifications of them,
    // details are in CC file.
    mutable REAL4m error_A2modif_img_diag;    // IFC
    mutable REAL4m error_A2modif_dip_diag;
    mutable REAL4m error_A2modif_img_OFFdiag;  
    mutable REAL4m error_A2modif_dip_OFFdiag;  
    // inf-tail error evaluation
    const REAL4m error_A_dip_diag;  
    const REAL4m error_A_dip_OFFdiag;  
//    const REAL4m error_p_img_diag;  we fixed it
//    const REAL4m error_p_dip_diag;  
//    const REAL4m error_p_img_OFFdiag;  
//    const REAL4m error_p_dip_OFFdiag;  
  
    // The summation will be done until the *cont* error is below a certain 
    // fraction of the actual tensor value:
    //     error_inf_tail < err_ratio_cont_to_N * N
    const REAL4m err_ratio_cont_to_N;  
    // Safety stopper in that case
    const UINT4m max_no_sum_elements;  
    
    // Fixed number of repetitions in the summation.
    // If set (i.e, non-zero), then this criteria is used
    // instead of the err_ratio_cont_to_N checking
    const UINT4m no_of_sum_elements;  
    // Include inifinite tails computation?
    // This is evidently for debugging purposes only
    const BOOL include_inf_tails;
    
    const   String tensor_file_suffix;
    // progress script is not needed (to be seen globally). See below, as well...
    // cyclic convolutions are not as well
    const   T_exec_order exec_order;
    
    // te perform deeper statistics, or not?
    const BOOL compute_stats;
    
    // "input" paramters END    
    
    // Maximal relative cube-cube distance for "image" calculations 
    //  (according to exact cube-cube formalae). 
    // Above this radius dipole-dipole ("dipole") estimation is used.
    // They are calcualted basing on error_A_img_diag, etc. variables
    // Different values for diagonal (like nXX) and off-diag elements (like nYZ)
    const REAL4m max_celldistance_img_diag; 
    const REAL4m max_celldistance_img_OFFdiag; 
    
    // mesh parameters
    mutable UINT4m xdim, ydim, zdim; // natural size
    mutable UINT4m xydim;
    mutable UINT4m totalsize;
    // relative cell edge sizes
    mutable REALWIDE dx, dy, dz;
    
    // tensor_file-specific
    mutable FileAccessType tensor_rw; // read or write or else
    mutable String tensor_file_prefix;
    String important_parameters; // input parameters, those values may influence 
    				 // the saved tensor values. 
    				 // Not mutable as it is set once in the constructor.

    // progress-specific
    // Denominator, "how much to do", is determined by "totalsize"
    // This is the assumption: calling programm shall evaluate _all_ cells
    // in the constuctor-initialization-finalization-destructor cycle.
    const String progress_script;
    mutable Tcl_Interp* interp;       // for Tcl_Eval
    mutable BOOL show_progress;       // Yes or No 
    // Numerator, "actual done work", in units, say 0..100
    mutable UINT4m progress_cnt;      
    // the same, but now in units, 0..totalsize
    mutable UINT4m cell_cnt;      

// STATISTICS begin
    
    // Number of summations done. Maximal, average, and minimal
    mutable REAL4m n_max[tens_el_MAX];
    mutable REAL4m n_sum[tens_el_MAX]; // for arithmetic average
    mutable REAL4m n_min[tens_el_MAX];
    
    // Errors (absolute).
    // Equals to <error of the discrete sum> + <infinite tails error>
    // Instead of an average error a "total" error for the whole sample
    // is given. This total is computed as a \sum abs(error)
    mutable REAL4m err_max[tens_el_MAX];
    mutable REAL4m err_sum[tens_el_MAX]; // summary of abs(errors) for all tensor elements
    mutable REAL4m err_min[tens_el_MAX];
    
    // Summation error ratio.
    // During the *img* + *dip* summation, a "doubly compensated summation"
    // procedure is done. This procedure results in a special summation 
    // corrector. Here, we report ratio <summation corrector>/<error>
    // I am simply unsure how large mistakes are introduced by the fact 
    // that we sum from highest to lowest values. 
    // According to theory, we should do it in an opposite order...
    mutable REAL4m err_sum_ratio_max[tens_el_MAX];
    mutable REAL4m err_sum_ratio_sum[tens_el_MAX]; // for arithmetic average
    mutable REAL4m err_sum_ratio_min[tens_el_MAX];
    
// STATISTICS end

    // Tensor file storage buffer
    // Used in case, when (tensor_rw==WRITE || tensor_rw==READ)
    mutable Oxs_MeshValue<ThreeVector> tensor_buffer;  
    
    // methods ********************************
    
    void ReleaseMemory() const; // Conceptually const.  
    
    // Function to calculate the tensor element (coef in originall SimpleDemag)
    // Inputs are various variables like position (xyz), size (dx...), etc.
    // Some parameters are taken from the class body.
    // Inf_tails and sum_coeff are written on request to appr tables 
    // (with index store_inf_sum_idx)
    // conceptually const
    REALWIDE CalculateSDA_PBC_really( \
        const UINT4m i, const UINT4m j, const UINT4m k, \
        const T_tens_el teXX) const;
        
    // error of the *img* computation done for teXX element,
    // with r inter-cell distance.
    // r2=r^2, the result is also error^2,
    // result is positive
    REAL4m CalculateSDAerrIMG_2(const REALWIDE r2, 
                                const T_tens_el teXX) const;
    // similarly for *dip* method
    REAL4m CalculateSDAerrDIP_2(const REALWIDE r2, 
                                const T_tens_el teXX) const;
        
    // all tensor_buffer accesses should use following methods
    // (as Mike plans to change some OOMMF code somaday)
    REAL8m GetTensorElementFromBuffer(
        const UINT4m el_idx, const T_tens_el teXX) const;
    // conceptually const
    void PutTensorElementToBuffer(
        const REAL8m elem, const UINT4m el_idx, const T_tens_el teXX) const;
        
    // Upload/download routines
    // conceptually conts, changes the tensor_buffer
    BOOL load_tensor_file( const String file_name, 
                           const String title) const;
    // really const
    void save_tensor_file( const String file_name, 
                           const String title,
                           const char* desc) const;
                                                      
  public:
    // constructor
    // Mainly imports the parameters and does small evaluation of them,
    // initializes the variables
    DemagPBC( const Oxs_Mesh* genmesh,
              const REAL4m zPeriod_param,
	      const REAL4m error_A_img_diag   ,
	      const REAL4m error_A_dip_diag   ,
	      const REAL4m error_A_img_OFFdiag,
	      const REAL4m error_A_dip_OFFdiag,
//	      const REAL4m error_p_img_diag   ,
//	      const REAL4m error_p_dip_diag   ,
//	      const REAL4m error_p_img_OFFdiag,
//	      const REAL4m error_p_dip_OFFdiag,
	      const REAL4m err_ratio_cont_to_N,
	      const UINT4m max_no_sum_elements,
	      const UINT4m no_of_sum_elements,
	      const BOOL   include_inf_tails,
              const String tensor_file_name,
              const String tensor_file_suffix,
              const String progress_script,
              const T_exec_order exec_order,
              const BOOL   compute_stats
            );
    
    // Written separately as errors thorw here are better evaluated
    //  as those thrown from inside the constructor.
    // All memory allocation code is here.
    // conceptually const
    void Init() const;
    
    // computation of the tensor for the given cell
    // conceptually const                       
    REALWIDE CalculateSDA(const T_tens_el teXX,
                  const UINT4m i, const UINT4m j, const UINT4m k) const;
                  
    // Tensor_file upload/download.
    // Should be run in the middle of computations: after 00, 11, 22
    //  and before 01, 02, 12.
    // conceptually conts, changes the tensor_buffer
    void middle_load_or_save_tensor_file() const;
          
    // final step
    void Finalize() const;
        
    // destructor
    ~DemagPBC() { ReleaseMemory(); }
    
    
};
