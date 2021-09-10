/* FILE: kl_pbc_util.h            -*-Mode: c++-*- 
 * See the header file fo more...
 */
// KL(m) 

#include <float.h>        // For FLT_MAX

#include "vf.h"           // tensor buffer file I/O
#include "oxswarn.h"      // for messages
#include "kl_pbc_util.h"
#include "demagcoef.h"    // we use its functions Oxs_CalculateSDA<ij>

/* End includes */

// Revision information, set via CVS keyword substitution
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1.2 $",
   "$Date: 2015/09/29 20:55:45 $",
   "$Author: donahue $",
   "Kristof M. Lebecki (lebecki(ot)ifpan.edu.pl)");

// const OC_REAL8m eps=1e-10;
const OC_REAL8m eps=OC_REAL8_EPSILON; // error used for comparisons: x==0.0 <-> fabs(x)<eps 

// We set these values fixed. 
//  First, because MD proved analythically their value.
//  Second, we anyhow assume at some point value of error_p_dip, 
//  namely in the function Oxs_CalculateSDA_dip_err_accumulated.
const OC_REAL4m error_p_img= 3.0;
const OC_REAL4m error_p_dip=-7.0;

// Helper functions to shorten the code for large integer (positive) power.
inline OC_REALWIDE pow2(OC_REALWIDE x)
{ return x*x; }
inline OC_REALWIDE pow3(OC_REALWIDE x)
{ return x*x*x; }
inline OC_REALWIDE pow4(OC_REALWIDE x)
{ return x*x*x*x; }
inline OC_REALWIDE pow5(OC_REALWIDE x)
{ return x*x*x*x*x; }

// Often I write e.g. sqrt(X*X*X*X*X) instead of pow(X,2.5)
// Reason: for speed-up (30 times faster on IEEE-754 processors).

// Wrappers for OOMMF 20181207 API update.
// Note: The Oxs_CalculateNxx and company routines use double-double
//       arithmetic, which makes them more accurate but much slower than
//       the older Oxs_CalculateSDA00 (and company) routines which the
//       replace.  There is also a scaling difference, which is
//       accounted for in the wrappers.
#if OOMMF_API_INDEX >= 20181207
inline OC_REALWIDE Oxs_CalculateSDA00(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
                                      OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return static_cast<OC_REALWIDE>(Oxs_CalculateNxx(x,y,z,dx,dy,dz).Hi())*(4*PI*dx*dy*dz); }
inline OC_REALWIDE Oxs_CalculateSDA11(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
                                      OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return static_cast<OC_REALWIDE>(Oxs_CalculateNyy(x,y,z,dx,dy,dz).Hi())*(4*PI*dx*dy*dz); }
inline OC_REALWIDE Oxs_CalculateSDA22(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
                                      OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return static_cast<OC_REALWIDE>(Oxs_CalculateNzz(x,y,z,dx,dy,dz).Hi())*(4*PI*dx*dy*dz); }
inline OC_REALWIDE Oxs_CalculateSDA01(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
                                      OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return static_cast<OC_REALWIDE>(Oxs_CalculateNxy(x,y,z,dx,dy,dz).Hi())*(4*PI*dx*dy*dz); }
inline OC_REALWIDE Oxs_CalculateSDA02(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
                                      OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return static_cast<OC_REALWIDE>(Oxs_CalculateNxz(x,y,z,dx,dy,dz).Hi())*(4*PI*dx*dy*dz); }
inline OC_REALWIDE Oxs_CalculateSDA12(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
                                      OC_REALWIDE dx,OC_REALWIDE dy,OC_REALWIDE dz)
{ return static_cast<OC_REALWIDE>(Oxs_CalculateNyz(x,y,z,dx,dy,dz).Hi())*(4*PI*dx*dy*dz); }
#endif

// Functions for calculating the sphere / dipole N-values
// These functions can be succesfully used as a far-distance approximation
//   (replacement) for exact cube-cube Newell functions. 
// Following functions are taken from paper [1], eq. (15).
// They have to be multiplied by scale_dip_rel, see 
//   comment about "scale multiplicative ready" (scale-ready).
inline OC_REALWIDE Oxs_CalculateSDA00_dip(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // This is Nxx term (or N00) from eq. (15) without the mul-factor
  return (2.*x*x-y*y-z*z) / (3.*sqrt( pow5(x*x+y*y+z*z) ));
}
inline OC_REALWIDE Oxs_CalculateSDA11_dip(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z)
{ return Oxs_CalculateSDA00_dip(y,x,z); }
inline OC_REALWIDE Oxs_CalculateSDA22_dip(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z)
{ return Oxs_CalculateSDA00_dip(z,y,x); }
//
inline OC_REALWIDE Oxs_CalculateSDA01_dip(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z)
{ // This is Nxy term (or N01) from eq. (15) without the mul-factor
  return x*y / sqrt( pow5(x*x+y*y+z*z) );
}
inline OC_REALWIDE Oxs_CalculateSDA02_dip(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z)
{ return Oxs_CalculateSDA01_dip(x,z,y); }
inline OC_REALWIDE Oxs_CalculateSDA12_dip(OC_REALWIDE x, OC_REALWIDE y, OC_REALWIDE z)
{ return Oxs_CalculateSDA01_dip(y,z,x); }

// Functions to calculate the approximated tails in infinities.
// Following functions reflect summation {from -\infty to -n_max} 
//  plus {from +n_max to +\infty}. For instance:
//   Oxs_CalculateSDA00_tails ~=   \sum _-\infty ^-n_max Oxs_CalculateSDA00_dip
//                            +\sum _+n_max ^-\infty Oxs_CalculateSDA00_dip
// These functions are scale-ready.
// See [2].(A2.6), (A2.7).
// Acording to Mike's advice they require better shape:
// "with the exception of Nzz^cont, the formulas in (A2.6) are 
//  numerically rather poor.  Nxy^cont is especially bad"
OC_REALWIDE Oxs_CalculateSDA00_tails(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
			OC_REALWIDE zPeriod, OC_INDEX n_max)
{ 
  const OC_REALWIDE nmin05=static_cast<OC_REALWIDE>(n_max)-0.5;	
  // thus, n_max = nmin_05+0.5; or n_max -> nmin05+0.5;	
  if(fabs(x-y)>eps){ // i.e. x<>y
    return ( \
	((-pow2(x) + pow2(y))*(2 + ((z - nmin05*zPeriod)* \
	          (pow2(y) + pow2(z - nmin05*zPeriod) +  \
	            (2*pow4(x))/(pow2(x) - pow2(y))))/ \
	        sqrt(pow3(pow2(x) + pow2(y) + pow2(z - nmin05*zPeriod))) -  \
	       ((z + nmin05*zPeriod)*(pow2(y) + pow2(z + nmin05*zPeriod) +  \
	            (2*pow4(x))/(pow2(x) - pow2(y))))/ \
	        sqrt(pow3(pow2(x) + pow2(y) + pow2(z + nmin05*zPeriod)))))/ \
	   (zPeriod*pow2(pow2(x) + pow2(y))) \
    );          
  } else { // i.e. x==y
    return ( \
	(-((z - nmin05*zPeriod)/ \
	        sqrt(pow3(2*pow2(x) + pow2(z - nmin05*zPeriod)))) +  \
	     (z + nmin05*zPeriod)/sqrt(pow3(2*pow2(x) + pow2(z + nmin05*zPeriod))) \
	     )/(2.*zPeriod) \
	    );          
  }
}

OC_REALWIDE Oxs_CalculateSDA01_tails(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
			OC_REALWIDE zPeriod, OC_INDEX n_max)
{ 
  const OC_REALWIDE nmin05=static_cast<OC_REALWIDE>(n_max)-0.5;	
  if(fabs(x)>eps||fabs(y)>eps){ // i.e. NOT( x==y==0 )
    return ( \
	-((x*y*(4/(pow2(x) + pow2(y)) +  \
	         ((z - nmin05*zPeriod)* \
	            (1 + (2*(pow2(x) + pow2(y) + pow2(z - nmin05*zPeriod)))/ \
	               (pow2(x) + pow2(y))))/ \
	          sqrt(pow3(pow2(x) + pow2(y) + pow2(z - nmin05*zPeriod))) -  \
	         ((z + nmin05*zPeriod)* \
	            (1 + (2*(pow2(x) + pow2(y) + pow2(z + nmin05*zPeriod)))/ \
	               (pow2(x) + pow2(y))))/ \
	          sqrt(pow3(pow2(x) + pow2(y) + pow2(z + nmin05*zPeriod)))))/ \
	     (zPeriod*(pow2(x) + pow2(y))))         \
    );
  } else { // i.e. x==y==0	  
    return (0);
  }
}

OC_REALWIDE Oxs_CalculateSDA02_tails(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
			OC_REALWIDE zPeriod, OC_INDEX n_max)
{ 
  const OC_REALWIDE nmin05=static_cast<OC_REALWIDE>(n_max)-0.5;	
  return ( \
	-((x*(-1./sqrt(pow3(pow2(x) + pow2(y) + pow2(z - nmin05*zPeriod))) +  \
	         1./sqrt(pow3(pow2(x) + pow2(y) + pow2(z + nmin05*zPeriod)))))/ \
	     zPeriod) \
  );
}

OC_REALWIDE Oxs_CalculateSDA22_tails(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
			OC_REALWIDE zPeriod, OC_INDEX n_max)
{ 
  const OC_REALWIDE nmin05=static_cast<OC_REALWIDE>(n_max)-0.5;	
  return ( \
	-((-((z - nmin05*zPeriod)/ \
	          sqrt(pow3(pow2(x) + pow2(y) + pow2(z - nmin05*zPeriod)))) +  \
	       (z + nmin05*zPeriod)/ \
	        sqrt(pow3(pow2(x) + pow2(y) + pow2(z + nmin05*zPeriod))))/zPeriod)       \
  );
}

inline OC_REALWIDE Oxs_CalculateSDA11_tails(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
			OC_REALWIDE zPeriod, OC_INDEX n_max)
{ return Oxs_CalculateSDA00_tails(y,x,z,zPeriod,n_max); }
        
inline OC_REALWIDE Oxs_CalculateSDA12_tails(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
			OC_REALWIDE zPeriod, OC_INDEX n_max)
{ return Oxs_CalculateSDA02_tails(y,x,z,zPeriod,n_max); }

// helper functions
inline OC_REAL4m powI4(const OC_INDEX n) {
  return static_cast<OC_REAL4m>(n)*static_cast<OC_REAL4m>(n)
        *static_cast<OC_REAL4m>(n)*static_cast<OC_REAL4m>(n);
}
inline OC_REAL4m powI6(const OC_INDEX n) {
  return powI4(n)*static_cast<OC_REAL4m>(n)*static_cast<OC_REAL4m>(n);
}
inline OC_REAL4m powR3(const OC_REALWIDE x) {
  return static_cast<OC_REAL4m>(x)*static_cast<OC_REAL4m>(x)*static_cast<OC_REAL4m>(x);
}
inline OC_REAL4m powR5(const OC_REALWIDE x) {
  return powR3(x)*static_cast<OC_REAL4m>(x)*static_cast<OC_REAL4m>(x);
}
inline OC_REAL4m powR7(const OC_REALWIDE x) {
  return powR5(x)*static_cast<OC_REAL4m>(x)*static_cast<OC_REAL4m>(x);
}

// The functions Oxs_CalculateSDAxx_tails return results with an error.
//  This error can be estimated "from top" by two, values, see [2].(A2.3):
//  - by the cont-error (nonnegative).
//  - by the dip-error (positive).
// These two errors should be added together to form the total tail error.
// All these are "scale multiplicative ready".
//
// These errors come out of the eq. [2].(A2.9), if you multiply it by 
//   |scale|.
OC_REAL4m Oxs_CalculateSDA00_cont_err(OC_REALWIDE /*x*/,OC_REALWIDE /*y*/,OC_REALWIDE /*z*/,
			        OC_REALWIDE zPeriod, OC_INDEX n) { 
  return 1. / (4.*powR3(zPeriod)*powI4(n));
}			        
inline OC_REAL4m Oxs_CalculateSDA11_cont_err(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
			        OC_REALWIDE zPeriod, OC_INDEX n) { 
  return Oxs_CalculateSDA00_cont_err(y, x, z, zPeriod, n);
}			        
OC_REAL4m Oxs_CalculateSDA22_cont_err(OC_REALWIDE /*x*/,OC_REALWIDE /*y*/,OC_REALWIDE /*z*/,
			        OC_REALWIDE zPeriod, OC_INDEX n) { 
  return 1 / (2.*powR3(zPeriod)*powI4(n));
}			        
OC_REAL4m Oxs_CalculateSDA01_cont_err(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE /*z*/,
			        OC_REALWIDE zPeriod, OC_INDEX n) { 
  return 5.*static_cast<OC_REAL4m>(x)*static_cast<OC_REAL4m>(y) \
           / (4.*powR5(zPeriod)*powI6(n));
}			        
OC_REAL4m Oxs_CalculateSDA02_cont_err(OC_REALWIDE x,OC_REALWIDE /*y*/,OC_REALWIDE z,
			        OC_REALWIDE zPeriod, OC_INDEX n) { 
  return 5.*static_cast<OC_REAL4m>(x)*static_cast<OC_REAL4m>(z) \
           / (powR5(zPeriod)*powI6(n));
}			        
inline OC_REAL4m Oxs_CalculateSDA12_cont_err(OC_REALWIDE x,OC_REALWIDE y,OC_REALWIDE z,
			        OC_REALWIDE zPeriod, OC_INDEX n) { 
  return Oxs_CalculateSDA02_cont_err(y, x, z, zPeriod, n);
}			        

// These errors come out of the eq. [2].(A2.1),(A2.2), if you multiply it by 
//   |scale| and sum over (accumulate) up to the infinity. The summation to +\inf
//   and -\inf is here included.
// This value has to be further multipled by the 'A' parametr from [2].(A2.1), 
//   (A2.2). Beside it, it is "scale multiplicative ready".
OC_REAL4m Oxs_CalculateSDA_dip_err_accumulated(OC_REALWIDE zPeriod, OC_INDEX n) { 
  return 4.*PI / (3.*powR7(zPeriod)*powI6(n));
}			        

// Helper function
OC_INDEX NextPowerOfTwo(OC_INDEX n)
{ // Returns first power of two >= n
  OC_INDEX m=1;
  while(m<n) m*=2;
  return m;
}

// Some constants used in the class
const String title_1[exec_order_MAX]=   \
  {"Demagnetization tensor diagonal elements: Nxx, Nyy, Nzz",     // _SimpleDemag
   "Demagnetization tensor elements: Nxx, Nxy, Nxz"};		  // _Demag
const String title_2[exec_order_MAX]=\
  {"Demagnetization tensor off-diagonal elements: Nxy, Nxz, Nyz", // _SimpleDemag
   "Demagnetization tensor elements: Nyy, Nyz, Nzz"};		  // _Demag
      
// how many progress steps. 
// This is determined by the progress.tcl script requirement
const OC_INDEX progress_steps = 100;  

// how large real number I can imagine
// important *only* for statistics initiatialization of "min" values
const OC_REAL4m MAX_REAL = FLT_MAX;
          
// DemagPBC class

// Constructor
DemagPBC::DemagPBC( 
      const Oxs_Mesh*	genmesh_param,
      const OC_REAL4m 	zPeriod_param,
      const OC_REAL4m 	error_A_img_diag_param   ,
      const OC_REAL4m 	error_A_dip_diag_param   ,
      const OC_REAL4m 	error_A_img_OFFdiag_param,
      const OC_REAL4m 	error_A_dip_OFFdiag_param,
      const OC_REAL4m 	err_ratio_cont_to_N_param,
      const OC_INDEX 	max_no_sum_elements_param,
      const OC_INDEX 	no_of_sum_elements_param,
      const OC_BOOL 	include_inf_tails_param,
      const String 	tensor_file_name_param,
      const String 	tensor_file_suffix_param,
      const String 	progress_script_param,
      const T_exec_order exec_order_param,
      const OC_BOOL        compute_stats_param) 
      : genmesh(NULL),mesh(NULL),zPeriod(0.0),
        // the error fitting funtion has a form err==A*r^p
        // But we need here 4*PI*dx*dy*dz*err^2, i.e. we will store internally 
        // modified parameters: (4*PI*dx*dy*dz*A*A) * exp( (2*p) * log(r) )
        error_A2modif_img_diag   	(0.0),
        error_A2modif_dip_diag   	(0.0),
        error_A2modif_img_OFFdiag	(0.0),
        error_A2modif_dip_OFFdiag	(0.0),
        error_A_dip_diag		(error_A_dip_diag_param),
        error_A_dip_OFFdiag		(error_A_dip_OFFdiag_param),
	err_ratio_cont_to_N		(err_ratio_cont_to_N_param),
	max_no_sum_elements		(max_no_sum_elements_param),
	no_of_sum_elements		(no_of_sum_elements_param),
	include_inf_tails		(include_inf_tails_param),
        tensor_file_suffix		(tensor_file_suffix_param),
        exec_order 			(exec_order_param),
        compute_stats                   (compute_stats_param),
        //
        max_celldistance_img_diag(   pow(    error_A_dip_diag_param/error_A_img_diag_param,
                                          1/(error_p_img-error_p_dip) )),
        max_celldistance_img_OFFdiag(pow(    error_A_dip_OFFdiag_param/error_A_img_OFFdiag_param,
                                          1/(error_p_img-error_p_dip) )),
        //        
        xdim(0),ydim(0),zdim(0),xydim(0),totalsize(0),
        dx(0.0),dy(0.0),dz(0.0),
        tensor_rw(NOTHING),tensor_file_prefix(""),important_parameters(""),
        progress_script			(progress_script_param),
        interp(NULL),
        show_progress(0),progress_cnt(0),cell_cnt(0),
        // n_max and other tables are initialized in the constructor body below
        tensor_buffer(NULL)
{ 
  // highest possible OC_INDEX value
  OC_INDEX MAX_INDEX = OC_INDEX(OC_UINDEX(-1)/2);
  // members that could not be initialized in the initializer list	
  n_max[n00]=0.0; 
  n_max[n11]=0.0; 
  n_max[n22]=0.0; 
  n_max[n01]=0.0; 
  n_max[n02]=0.0; 
  n_max[n12]=0.0; 
  n_min[n00]=MAX_INDEX;
  n_min[n11]=MAX_INDEX; 
  n_min[n22]=MAX_INDEX; 
  n_min[n01]=MAX_INDEX; 
  n_min[n02]=MAX_INDEX; 
  n_min[n12]=MAX_INDEX; 
  n_sum[n00]=0.0; 
  n_sum[n11]=0.0; 
  n_sum[n22]=0.0; 
  n_sum[n01]=0.0; 
  n_sum[n02]=0.0; 
  n_sum[n12]=0.0; 
  err_max[n00]=0.0; 
  err_max[n11]=0.0; 
  err_max[n22]=0.0; 
  err_max[n01]=0.0; 
  err_max[n02]=0.0; 
  err_max[n12]=0.0; 
  err_min[n00]=MAX_REAL; 
  err_min[n11]=MAX_REAL; 
  err_min[n22]=MAX_REAL; 
  err_min[n01]=MAX_REAL; 
  err_min[n02]=MAX_REAL; 
  err_min[n12]=MAX_REAL; 
  err_sum[n00]=0.0; 
  err_sum[n11]=0.0; 
  err_sum[n22]=0.0; 
  err_sum[n01]=0.0; 
  err_sum[n02]=0.0; 
  err_sum[n12]=0.0; 
  err_sum_ratio_max[n00]=0.0; 
  err_sum_ratio_max[n11]=0.0; 
  err_sum_ratio_max[n22]=0.0; 
  err_sum_ratio_max[n01]=0.0; 
  err_sum_ratio_max[n02]=0.0; 
  err_sum_ratio_max[n12]=0.0; 
  err_sum_ratio_min[n00]=MAX_REAL;
  err_sum_ratio_min[n11]=MAX_REAL; 
  err_sum_ratio_min[n22]=MAX_REAL; 
  err_sum_ratio_min[n01]=MAX_REAL; 
  err_sum_ratio_min[n02]=MAX_REAL; 
  err_sum_ratio_min[n12]=MAX_REAL; 
  err_sum_ratio_sum[n00]=0.0; 
  err_sum_ratio_sum[n11]=0.0; 
  err_sum_ratio_sum[n22]=0.0; 
  err_sum_ratio_sum[n01]=0.0; 
  err_sum_ratio_sum[n02]=0.0; 
  err_sum_ratio_sum[n12]=0.0; 
  
	
  // copying parameters
  
// Is it normal that class memebers "const" can be changed inside the constructor?
// This only concerns pointers to other classes...

  // genmesh
  genmesh = genmesh_param;
  mesh = dynamic_cast<const Oxs_RectangularMesh*>(genmesh);
  if(mesh==NULL) {
    String msg = String("Object ")
      + String(genmesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_Ext::Error(msg.c_str());
  }
  
  // Dimension variables dependent on the mesh
  xdim = mesh->DimX();
  ydim = mesh->DimY();
  zdim = mesh->DimZ();
  xydim=xdim*ydim;
  totalsize=xdim*ydim*zdim;
  //
  dx = mesh->EdgeLengthX();
  dy = mesh->EdgeLengthY();
  dz = mesh->EdgeLengthZ();
  // For demag calculation, all that matters is the relative
  // sizes of dx, dy and dz.  To help insure we don't run
  // outside floating point range, rescale these values so
  // largest is 1.0
  OC_REALWIDE maxedge=dx;
  if(dy>maxedge) maxedge=dy;
  if(dz>maxedge) maxedge=dz;
  dx/=maxedge; dy/=maxedge; dz/=maxedge;

  // error fitting function parameters
  // the error fitting funtion has a form err==A*r^p
  // But we need here 4*PI*dx*dy*dz*err^2, i.e. we will store internally 
  // modified parameters: (4*PI*dx*dy*dz*A*A) * exp( (2*p) * log(r) )
  error_A2modif_img_diag    = pow2(4.*PI*dx*dy*dz*error_A_img_diag_param);
  error_A2modif_dip_diag    = pow2(4.*PI*dx*dy*dz*error_A_dip_diag_param);
  error_A2modif_img_OFFdiag = pow2(4.*PI*dx*dy*dz*error_A_img_OFFdiag_param);
  error_A2modif_dip_OFFdiag = pow2(4.*PI*dx*dy*dz*error_A_dip_OFFdiag_param);
    
  // zPeriod
  // It depends on dimension variables!
  if(zPeriod_param==0) { // we take zdim in that case (atlas length)
    zPeriod = static_cast<OC_REAL8m>(zdim) * dz;
  } else { // we take parameter value in that case
    zPeriod = static_cast<OC_REAL8m>(zPeriod_param / (mesh->EdgeLengthZ())) * dz;
  }
  if(zPeriod_param<(static_cast<OC_REAL8m>(zdim) * (mesh->EdgeLengthZ())) && 
     zPeriod_param!=0.0) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf), "Strange parameter value:"
  	    " Specified \"zPeriod\" is %g (should be zero or >= %g, i.e. the z-length)\n"
	      "I cannot imagine why you have done it, but as you wish...\n"
  	    ,zPeriod_param, static_cast<OC_REAL8m>(zdim) * (mesh->EdgeLengthZ())
	  );
    static Oxs_WarningMessage foo(1);
    foo.Send(revision_info,OC_STRINGIFY(__LINE__), buf);	  
  }
  
  // tensor_file_name
  // tensor_file_name is not needed at global level
  // Instead we set up here other variables
  if(tensor_file_name_param=="")
    tensor_rw = NOTHING;
  else
    tensor_rw = WRITE;
  // Full file prefix
  if(tensor_file_name_param!="") {
    if(tensor_file_name_param.substr( tensor_file_name_param.length()-1,
	      	                      tensor_file_name_param.length()-1)=="/") {
      // it is a directory, we create a file name basing on it
      char buf[1024];
      // zPeriod is present in the file name as an integer value. But all internal calculations
      // are done with its floating point precision. This floating value should be also present
      // in the Title line of this file. 
      // So even small changes in this input parametr will cause the file to be re-computed :)
      Oc_Snprintf(buf,sizeof(buf),"t-%d-%d-%d-%d-",xdim,ydim,zdim,
      				  static_cast<OC_INDEX>(OC_ROUND(zPeriod/dz)));
      tensor_file_prefix = tensor_file_name_param + buf; 	      	                      
    } else
      // it is just a file name
      tensor_file_prefix = tensor_file_name_param;
    // important input parameters
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),". zP=%g,eaid=%g,eadd=%g,eaio=%g,eado=%g,erctN=%g,mnse=%d,nose=%d,iit=%d",
    				   zPeriod_param,
    				         error_A_img_diag_param,
    				                 error_A_dip_diag_param,
    				                         error_A_img_OFFdiag_param,
    				                                 error_A_dip_OFFdiag_param,
                                                          	         err_ratio_cont_to_N_param,
    				                                                  max_no_sum_elements_param,
 		                                                                          no_of_sum_elements_param,
    			                                                                          include_inf_tails_param
        	);
    important_parameters = buf;
  }
  
  // progress_script
  if(progress_script_param!="") show_progress=1;
  else show_progress=0;
}  

void DemagPBC::Init() const
{ 
  // Clean-up from previous allocation, if any.
  ReleaseMemory();
  
  // Tensor file storage initialization
  if(tensor_rw==WRITE) {
    // In this case we will need the tensor_buffer
    tensor_buffer.AdjustSize(mesh);
    if(tensor_buffer.Size()!=mesh->Size())
      throw Oxs_Ext::Error("Error allocating space for the tensor_buffer");
    // try to read it
    if(1==load_tensor_file(tensor_file_prefix+"2"+tensor_file_suffix, 
                           title_2[exec_order]+important_parameters)) {
      if(1==load_tensor_file(tensor_file_prefix+"1"+tensor_file_suffix,
                             title_1[exec_order]+important_parameters)) {
        // we can use the old file (it is valid).
        tensor_rw = READ;
        // BTW, we have already loaded the tensor with diagonal elements
      }
    }
  }
  
  // script initialization
  if(show_progress==1 && tensor_rw!=READ) {
  }
  
  // Progress initialization
  // variables, constants
//  progress_cnt    = 0; we do it in the constructor!
  // script initialization
  if(show_progress==1 && tensor_rw!=READ) {
    // in case of READ we do not progress anything
    interp=Oc_GlobalInterpreter();
    if(interp==NULL)
      throw Oxs_Ext::Error("Tcl interpretor not initialized");
    int errcode = TCL_OK;
    // now, we start the progress-script
    errcode = Tcl_Eval(interp,Oc_AutoBuf(String("set klmchan [open \"|wish ")
                                         + progress_script + String("\" w+]")));
    if(errcode==TCL_OK) {
      // Configure process channel, and double-check launch success
      errcode = Tcl_Eval(interp,Oc_AutoBuf("\n\
        fconfigure $klmchan -blocking 0 -buffering line\n\
        set kl_pbc_check_loop 10\n\
        while {[set kl_pbc_check_count [gets $klmchan kl_pbc_check_value]]<1} {\n\
           if {$kl_pbc_check_count<0 && [eof $klmchan]} {\n\
              fconfigure $klmchan -blocking 1\n\
              close $klmchan\n\
              error {Error launching progress script}\n\
           }\n\
           if {[incr kl_pbc_check_loop -1]<0} {\n\
              catch {close $klmchan}\n\
              error {Timeout launching progress script}\n\
           }\n\
           after 1000\n\
        }\n\
        if {[string compare \"kl_progress_OK\" $kl_pbc_check_value]!=0} {\n\
           catch {close $klmchan}\n\
           error \"Bad progress script check string: -->$kl_pbc_check_value<--\"\n\
        }\n\
      "));
    }
    if(errcode!=TCL_OK) {
      static Oxs_WarningMessage foo(-1);
      foo.Send(revision_info,OC_STRINGIFY(__LINE__),
               (String("Failure launching progress script \"")
                + progress_script + String("\": ")
                +Tcl_GetStringResult(interp)).c_str());
      show_progress=0;
    }
  }
}

// error of the *img* computation done for teXX element,
// with r inter-cell distance.
// r2=r^2, the result is also error^2
OC_REAL4m DemagPBC::Oxs_CalculateSDAerrIMG_2(const OC_REALWIDE r2, 
                                      const T_tens_el teXX) const
{ 
  // the error fitting funtion has a form err==A*r^p
  // But we compute here (4*PI*dx*dy*dz)^2*err^2 == (4*PI*dx*dy*dz)^2*A^2 * r^2p == 
  //                     A2modif * exp(2*p*log(r)) == A2modif * exp(p*log(r^2)), 
  // where modified parameter: A2modif==(4*PI*dx*dy*dz*A)^2;
  if(compute_stats) {
    if(teXX==n00 || teXX==n11 || teXX==n22)
      // diagonal elements
      return error_A2modif_img_diag    * exp(error_p_img*log(r2));
    else
      // OFF-diagonal elements
      return error_A2modif_img_OFFdiag * exp(error_p_img*log(r2));
  } else
    return 0.0;
}
// similarly for *dip* method
OC_REAL4m DemagPBC::Oxs_CalculateSDAerrDIP_2(const OC_REALWIDE r2, 
                                      const T_tens_el teXX) const
{ 
  if(compute_stats) {
    if(teXX==n00 || teXX==n11 || teXX==n22)
      // diagonal elements
      return error_A2modif_dip_diag    * exp(error_p_dip*log(r2));
    else
      // OFF-diagonal elements
      return error_A2modif_dip_OFFdiag * exp(error_p_dip*log(r2));
  } else
    return 0.0;
}

// Dumps the tensor_buffer into a file
void DemagPBC::save_tensor_file( const String file_name,
                                 const String title,
                                 const char* desc) const
{
  const OC_BOOL write_headers = 1;
  const char* valueunit = "{}";
  const char* meshtype = genmesh->NaturalOutputType();
  const char* datatype  = "binary";
  const char* precision = "8";
//  const char* datatype  = "text";
//  const char* precision = "%.20g";
  //
  genmesh->WriteOvfFile(file_name.c_str(),
     	  write_headers,
		    title.c_str(),desc,
		    valueunit,meshtype,datatype,precision,
		    &tensor_buffer,NULL);
}

// helper function that replaces all instancies of "what" by "by"
String replace_substr( const String str,
                       const String replace_what,
                       const String replace_by)
{
  String tmp = str;
  OC_INDEX i = tmp.find(replace_what);
  while(i != (OC_INDEX)String::npos) { // casting to avoid compiler warnings
    tmp = tmp.replace(i, replace_what.length(), replace_by);
    i = tmp.find(replace_what);
  }
  return tmp;	
}                          

// Fills the tensor_buffer from a file
OC_BOOL DemagPBC::load_tensor_file( const String file_name, 
                                 const String title) const
// Result: succesfull (1) or not (0) load process
// For a success the file_mesh has to be *exactly* the same as the current one
// Title must be appriopriate, too.
// As in demag evaluation only relative size is important, in future one can 
// code all necessary shifting (offsets) and re-scaling to support *similar* meshes.
// As for now it is not implemented.
{
  Vf_Mesh* file_mesh;
  // Create and load Vf_Mesh object with file data
  // This line produces a message when the file is not present
  // But this message goes only to stdout? So, it should not be 
  //  a great problem. If it were, well either check for the file
  //  presence first, or what else?
  Vf_FileInput* vffi=Vf_FileInput::NewReader(file_name.c_str());
  if(vffi==NULL) 
    return 0; // probably no file present
  try { // we must ensure Delete of vfii
    file_mesh = vffi->NewMesh();
  } catch(...) {
    delete vffi;
    return 0;
  }
  delete vffi;
  
  if(file_mesh==NULL) 
    return 0; //Unable to create Vf_Mesh object on input file

  try { // we must ensure Delete of file_mesh 
    // title
    // due to different implementations we ignore "e-0" in the title.
    // e.x "3.2e-0017" will be changed into "3.2e-17"
    if( replace_substr(String(file_mesh->GetTitle()),"e-0","e-") 
      !=replace_substr(title,"e-0","e-") ) 
    {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
    		"\nInappriopriate title:\n"
    		"%s\n"
    		"%s\n"
    		,String(file_mesh->GetTitle()).c_str()
        ,title.c_str());
      throw Oxs_Ext::Error(buf);
    }
    // mesh size
    if(static_cast<OC_INDEX>(file_mesh->GetSize())!=mesh->Size()) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
    		"\nDifferent mesh sizes:\n"
    		"%d != %d\n"
    		,static_cast<OC_INDEX>(file_mesh->GetSize())
        ,mesh->Size());
      throw Oxs_Ext::Error(buf);
    }
    if(tensor_buffer.Size() != mesh->Size())
      tensor_buffer.AdjustSize(mesh); // safety, should never happen
      
    // Cellsize
    Nb_Vec3<OC_REAL4> file_dim= file_mesh->GetApproximateCellDimensions();   
    const OC_REAL4 edgeX = static_cast<OC_REAL4>(mesh->EdgeLengthX());
    const OC_REAL4 edgeY = static_cast<OC_REAL4>(mesh->EdgeLengthY());
    const OC_REAL4 edgeZ = static_cast<OC_REAL4>(mesh->EdgeLengthZ());    
    if( fabs(edgeX - file_dim.x)>OC_REAL4_EPSILON*edgeX ||
        fabs(edgeY - file_dim.y)>OC_REAL4_EPSILON*edgeY ||
        fabs(edgeZ - file_dim.z)>OC_REAL4_EPSILON*edgeZ ) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
    		"\nDifferent cell sizes:\n"
    		"x: %g, %g\n"
    		"y: %g, %g\n"
    		"z: %g, %g\n"
    		,edgeX, file_dim.x
    		,edgeY, file_dim.y
    		,edgeZ, file_dim.z);
      throw Oxs_Ext::Error(buf);
    }
      
    // mesh boundaries
    // file
    Nb_BoundingBox<OC_REAL8m> file_range;
    file_mesh->GetPreciseRange(file_range); // including boundary
    OC_REAL8m fxMin, fyMin, fzMin, fxMax, fyMax, fzMax;
    file_range.GetMinPt(fxMin, fyMin, fzMin);
    file_range.GetMaxPt(fxMax, fyMax, fzMax);
    // actual
    Oxs_Box bbox;    
    mesh->GetBoundingBox(bbox);
    // check
    if( fabs(fxMin - bbox.GetMinX())>OC_REAL8_EPSILON*fabs(fxMin) ||
        fabs(fyMin - bbox.GetMinY())>OC_REAL8_EPSILON*fabs(fyMin) ||
        fabs(fzMin - bbox.GetMinZ())>OC_REAL8_EPSILON*fabs(fzMin) ||
        fabs(fxMax - bbox.GetMaxX())>OC_REAL8_EPSILON*fabs(fxMax) ||
        fabs(fyMax - bbox.GetMaxY())>OC_REAL8_EPSILON*fabs(fyMax) ||
        fabs(fzMax - bbox.GetMaxZ())>OC_REAL8_EPSILON*fabs(fzMax) ) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
    		"\nDifferent mesh boundaries:\n"
    		"x_min: %g, %g\n"
    		"y_min: %g, %g\n"
    		"z_min: %g, %g\n"
    		"x_max: %g, %g\n"
    		"y_max: %g, %g\n"
    		"z_max: %g, %g\n"
    		,fxMin, bbox.GetMinX()
    		,fyMin, bbox.GetMinY()
    		,fzMin, bbox.GetMinZ()
    		,fxMax, bbox.GetMaxX()
    		,fyMax, bbox.GetMaxY()
    		,fzMax, bbox.GetMaxZ() );
      throw Oxs_Ext::Error(buf);
    }
    
    // number of cells in every direction
    // This checking is redundant, but if you are writing re-scaling code
    // you can use it...
    /* const OC_INDEX f_noX = \
        static_cast<OC_INDEX>(OC_ROUND(file_range.GetWidth() /file_dim.x));
    const OC_INDEX f_noY = \
        static_cast<OC_INDEX>(OC_ROUND(file_range.GetHeight()/file_dim.y));
    const OC_INDEX f_noZ = \
        static_cast<OC_INDEX>(OC_ROUND(file_range.GetDepth() /file_dim.z));
    const OC_INDEX noX = \
        static_cast<OC_INDEX>(OC_ROUND((bbox.GetMaxX()-bbox.GetMinX())/edgeX));
    const OC_INDEX noY = \
        static_cast<OC_INDEX>(OC_ROUND((bbox.GetMaxY()-bbox.GetMinY())/edgeY));
    const OC_INDEX noZ = \
        static_cast<OC_INDEX>(OC_ROUND((bbox.GetMaxZ()-bbox.GetMinZ())/edgeZ));
    if( f_noX!=noX || f_noY!=noY || f_noZ!=noZ ) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
    		"\nDifferent number of cells in every direction:\n"
    		"x: %d!=%d (%g, %g)\n"
    		"y: %d!=%d (%g, %g)\n"
    		"z: %d!=%d (%g, %g)\n"
    		,f_noX, noX
    		,f_noY, noY
    		,f_noZ, noZ
    		,file_range.GetWidth(), static_cast<OC_REAL8>(bbox.GetMaxX()-bbox.GetMinX())
        ,file_range.GetHeight(),static_cast<OC_REAL8>(bbox.GetMaxY()-bbox.GetMinY())
        ,file_range.GetDepth(), static_cast<OC_REAL8>(bbox.GetMaxZ()-bbox.GetMinZ()));
      throw Oxs_Ext::Error(buf);
    } */
    
    // Fill the buffer
    OC_INDEX size = tensor_buffer.Size();
    for(OC_INDEX i=0;i<size;i++) {
      ThreeVector location;
      mesh->Center(i,location);
      Nb_Vec3<OC_REAL8> pos(location.x,location.y,location.z);
      Nb_LocatedVector<OC_REAL8> lv;
      file_mesh->FindPreciseClosest(pos,lv);
      ThreeVector value(lv.value.x,lv.value.y,lv.value.z);
      tensor_buffer[i] = value * file_mesh->GetValueMultiplier();
    }
  // catch structure copied from oxscmds.cc::200
  } catch (Oxs_Ext::Error& err) {
    /* print-out the reason "while old tensor file is wrong" for test purpose */
    //   this prints the message only to the console
    static Oxs_WarningMessage foo(9, NB_MSG_INFO);
    //   this prints the message to the current window
    // static Oxs_WarningMessage foo(9);
    foo.Send(revision_info,OC_STRINGIFY(__LINE__),
          (String("Mesh in the tensor file is inappriopriate: \n")
          +String(err)+String("\n")).c_str()); 
    /**/
    delete file_mesh;
    return 0; 
  } catch(...) {
    static Oxs_WarningMessage foo(9);
    foo.Send(revision_info,OC_STRINGIFY(__LINE__),
    	     "Some problems in the read-out of the tensor file\n");
    delete file_mesh;
    return 0;
  }
  delete file_mesh;
  return 1; // OK, successful load
}

void DemagPBC::middle_load_or_save_tensor_file() const
{
  if(tensor_rw==WRITE)
    // filling up the tensor
    // dumping the tensor
    save_tensor_file(tensor_file_prefix+"1"+tensor_file_suffix,
                     title_1[exec_order]+important_parameters, NULL);
  if(tensor_rw==READ)
    load_tensor_file(tensor_file_prefix+"2"+tensor_file_suffix,
                     title_2[exec_order]+important_parameters);
}

OC_REAL8m DemagPBC::GetTensorElementFromBuffer(
    const OC_INDEX el_idx, const T_tens_el teXX) const 
{
  switch(exec_order) {
    case _SimpleDemag:
      switch(teXX) {
        case n00:
             return tensor_buffer[el_idx].x;
        case n11:
             return tensor_buffer[el_idx].y;
        case n22:
             return tensor_buffer[el_idx].z;
        case n01:
             return tensor_buffer[el_idx].x;
        case n02:
             return tensor_buffer[el_idx].y;
        case n12:
             return tensor_buffer[el_idx].z;
      }
    case _Demag:
      switch(teXX) { // now, the nXX order is different!
        case n00:
             return tensor_buffer[el_idx].x;
        case n01:
             return tensor_buffer[el_idx].y;
        case n02:
             return tensor_buffer[el_idx].z;
        case n11:
             return tensor_buffer[el_idx].x;
        case n12:
             return tensor_buffer[el_idx].y;
        case n22:
             return tensor_buffer[el_idx].z;
      }
  }
  // this should never happen, as we evaluated all possible cases...
  String msg = String("Error in function GetTensorElementFromBuffer in ")
    + String(genmesh->InstanceName());
  throw Oxs_Ext::Error(msg.c_str());
  return 0;
}
// conceptually const
void DemagPBC::PutTensorElementToBuffer(
    const OC_REAL8m elem, const OC_INDEX el_idx, const T_tens_el teXX) const
{    
  switch(exec_order) {
    case _SimpleDemag:
      switch(teXX) { 
        case n00:
             tensor_buffer[el_idx].x = elem;
             break;
        case n11:
             tensor_buffer[el_idx].y = elem;
             break;
        case n22:
             tensor_buffer[el_idx].z = elem;
             break;
        case n01:
             tensor_buffer[el_idx].x = elem;
             break;
        case n02:
             tensor_buffer[el_idx].y = elem;
             break;
        case n12:
             tensor_buffer[el_idx].z = elem;
             break;
      }
      break;
    case _Demag:
      switch(teXX) { // now, the nXX order is different!
        case n00:
             tensor_buffer[el_idx].x = elem;
             break;
        case n01:
             tensor_buffer[el_idx].y = elem;
             break;
        case n02:
             tensor_buffer[el_idx].z = elem;
             break;
        case n11:
             tensor_buffer[el_idx].x = elem;
             break;
        case n12:
             tensor_buffer[el_idx].y = elem;
             break;
        case n22:
             tensor_buffer[el_idx].z = elem;
             break;
      }
  }
}

// Function to calculate the tensor element (coef in originall simpleDemag)
// Inputs are various variables like position (ijk), size (dx...), etc.
// Most of those variables are taken from the class body.

// KL2.0 change (remove only inf_* ???)
// Inf_tails and sum_coeff are written on request to appr tables 
// (with index store_inf_sum_idx)
OC_REALWIDE DemagPBC::Oxs_CalculateSDA_PBC_really( \
    const OC_INDEX i, const OC_INDEX j, const OC_INDEX k, \
    const T_tens_el teXX) const
{ // conceptually const

  // For Nab, if a*b==0, thus Nab==0. This is is beacuse of symmetries of the N tensor (see Newell).
  // This is valid also for Nxz or Nyz, because its PBC repetitions are as a matter of fact non-zero,
  // but they are cancelled in the summation bacause of their anti-symmetricity.
  // So, we return zero in such a case.
  // Statistics are "not altered", thus we will not see zeros there caused by symmetries.
  // "not altered" means that the *average* will be accordingly lowered, due to the divisio at the end.
  // But the *mins* will reflect non-zero minimums.
  // This step is also *IMPORTANT*, because the n-index loop-braking will work now for sure.
  if((teXX==n01 && (i==0 || j==0)) ||
     (teXX==n02 && (i==0 || k==0)) ||
     (teXX==n12 && (j==0 || k==0))) {
    return 0.0; }
    
  // The above code uses antisymmetric property of Nij.
  // For code speed-up we could also use symmetric property of both Nii and Nij
  // (to shorten the summation by factor of 0.5). This is however not implemented, yet.

  const OC_REALWIDE x = dx*i;
  const OC_REALWIDE y = dy*j;
  const OC_REALWIDE z = dz*k;
  
  // OOMMF code uses not the demagnetization tensor, 'N', but rather
  //   (-4.*PI*dx*dy*dz)*N - at least this is what Oxs_CalculateSDA_PBC_really 
  //   functions are supposed to return.
  // So, we report everywhere here N multiplied by (-4.*PI*dx*dy*dz).
  //
  // That is why following factor from eq. (19) or (28) (Newell's papaper) 
  //   with additional minus sign is defined in OOMMF:
  //   const OC_REALWIDE scale = -1./(4.*PI*dx*dy*dz);
  //
  // Another factor comes from eq. (15) with additional minus sign. 
  //   This is scale factor for dipole-dipole approximation formula:
  //   const OC_REALWIDE scale_dip = (3.*dx*dy*dz)/(4.*PI);
  // As we prepare our results to be "scale multiplicative ready",
  //  it means they will be outside multiplied by scale (not scale_dip):
  //	- *img*  has to be not-multiplied (internally)
  //	- *dip*  has to be multiplied by scale_dip_rel = scale_dip / scale;
  //    - *cont* has to be not-multiplied (the errors as well)
  const OC_REALWIDE scale_dip_rel = -3.*pow2(dx*dy*dz);
  
  // This will be square of appriopriate max_celldistance_img_
  OC_REALWIDE  max_dist_SQR_XXX;
  if(teXX==n00 || teXX==n11 || teXX==n22)
    // diagonal elements
    max_dist_SQR_XXX= max_celldistance_img_diag*max_celldistance_img_diag;
  else
    // OFF-diagonal elements
    max_dist_SQR_XXX= max_celldistance_img_OFFdiag*max_celldistance_img_OFFdiag;
  
  // Now, we substitute depending on teXX:
  //  appriopriate functiones ...
  OC_REALWIDE \
    (*Oxs_CalculateSDAXX)(OC_REALWIDE,OC_REALWIDE,OC_REALWIDE,OC_REALWIDE,OC_REALWIDE,OC_REALWIDE) = NULL,
    (*Oxs_CalculateSDAXX_dip)(OC_REALWIDE,OC_REALWIDE,OC_REALWIDE)                        = NULL,
    (*Oxs_CalculateSDAXX_tails)(OC_REALWIDE,OC_REALWIDE,OC_REALWIDE,OC_REALWIDE,OC_INDEX)      = NULL;
  OC_REAL4m \
    (*Oxs_CalculateSDAXX_cont_err)(OC_REALWIDE,OC_REALWIDE,OC_REALWIDE,OC_REALWIDE,OC_INDEX)   = NULL;
  switch(teXX) {
    case n00: 
      Oxs_CalculateSDAXX =           Oxs_CalculateSDA00; 
      Oxs_CalculateSDAXX_dip =       Oxs_CalculateSDA00_dip; 
      Oxs_CalculateSDAXX_tails =     Oxs_CalculateSDA00_tails; 
      Oxs_CalculateSDAXX_cont_err =  Oxs_CalculateSDA00_cont_err; 
      break;
    case n11: 
      Oxs_CalculateSDAXX =           Oxs_CalculateSDA11; 
      Oxs_CalculateSDAXX_dip =       Oxs_CalculateSDA11_dip; 
      Oxs_CalculateSDAXX_tails =     Oxs_CalculateSDA11_tails; 
      Oxs_CalculateSDAXX_cont_err =  Oxs_CalculateSDA11_cont_err; 
      break;
    case n22: 
      Oxs_CalculateSDAXX =           Oxs_CalculateSDA22; 
      Oxs_CalculateSDAXX_dip =       Oxs_CalculateSDA22_dip; 
      Oxs_CalculateSDAXX_tails =     Oxs_CalculateSDA22_tails; 
      Oxs_CalculateSDAXX_cont_err =  Oxs_CalculateSDA22_cont_err; 
      break;
    case n01: 
      Oxs_CalculateSDAXX =           Oxs_CalculateSDA01; 
      Oxs_CalculateSDAXX_dip =       Oxs_CalculateSDA01_dip; 
      Oxs_CalculateSDAXX_tails =     Oxs_CalculateSDA01_tails; 
      Oxs_CalculateSDAXX_cont_err =  Oxs_CalculateSDA01_cont_err; 
      break;
    case n02: 
      Oxs_CalculateSDAXX =           Oxs_CalculateSDA02; 
      Oxs_CalculateSDAXX_dip =       Oxs_CalculateSDA02_dip; 
      Oxs_CalculateSDAXX_tails =     Oxs_CalculateSDA02_tails; 
      Oxs_CalculateSDAXX_cont_err =  Oxs_CalculateSDA02_cont_err; 
      break;
    case n12: 
      Oxs_CalculateSDAXX =           Oxs_CalculateSDA12; 
      Oxs_CalculateSDAXX_dip =       Oxs_CalculateSDA12_dip; 
      Oxs_CalculateSDAXX_tails =     Oxs_CalculateSDA12_tails; 
      Oxs_CalculateSDAXX_cont_err =  Oxs_CalculateSDA12_cont_err; 
      break;
  }
  
  OC_REALWIDE coef; // result of the function
  // this will be sum of *img*, *dip*, and *cont* repetitions
  
  // error^2 of the result, total and absolute. This will be sum-of-squares
  OC_REAL4m err2;  
  
  OC_REALWIDE x2y2=x*x+y*y; // this value will be repetitively used
  
  // 0_z-repetinion
  if(x2y2+z*z < max_dist_SQR_XXX)
  {
    coef = (*Oxs_CalculateSDAXX)(x,y,z,dx,dy,dz);               // not scaled
    err2 = Oxs_CalculateSDAerrIMG_2(x2y2+z*z,teXX);
  }
  else 
  {
    coef = scale_dip_rel * (*Oxs_CalculateSDAXX_dip)(x,y,z);    // scaled
    err2 = Oxs_CalculateSDAerrDIP_2(x2y2+z*z,teXX);
  }
  
  // n_z-repetinion (n and -n)
  // we sum from highest "coef" values to highest :(
  // I know, it should be opposite
  volatile OC_REALWIDE summation_corrector = 0.0; // For compensated summation
  OC_REAL4m inf_tail_error; // This is the error of the infinite tail
  OC_INDEX MAX_INDEX = OC_INDEX(OC_UINDEX(-1)/2); // for range overflow checking
  OC_INDEX n=0; // repetinion counter
  do 
  {
    n++; // we start with n==1
    // range overflow checking. Just to have a stopper somewhere.
    // Details are less important
    if(n>MAX_INDEX/3) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
                  "Range overflow, PBC repetition index reached %d\n", n);
      throw Oxs_Ext::Error(buf);
    }
    const OC_REALWIDE n_zPeriod=(static_cast<OC_REALWIDE>(n))*zPeriod;
    OC_REALWIDE part; // result for this +n and -n
    // first we evaluate z+n*zPeriod
    if(x2y2+(z+n_zPeriod)*(z+n_zPeriod) < max_dist_SQR_XXX) 
    {
      part = (*Oxs_CalculateSDAXX)(x,y,z+n_zPeriod,dx,dy,dz);           // not scaled  
      err2 += Oxs_CalculateSDAerrIMG_2(x2y2+(z+n_zPeriod)*(z+n_zPeriod),teXX);
    }
    else 
    { 
      part = scale_dip_rel * (*Oxs_CalculateSDAXX_dip)(x,y,z+n_zPeriod); // scaled
      err2 += Oxs_CalculateSDAerrDIP_2(x2y2+(z+n_zPeriod)*(z+n_zPeriod),teXX);
    }
    // Now, we evaluate z-n_zPeriod
    if(x2y2+(z-n_zPeriod)*(z-n_zPeriod) < max_dist_SQR_XXX) 
    {
      part+= (*Oxs_CalculateSDAXX)(x,y,z-n_zPeriod,dx,dy,dz);           // not scaled  
      err2 += Oxs_CalculateSDAerrIMG_2(x2y2+(z-n_zPeriod)*(z-n_zPeriod),teXX);
    }
    else 
    {
      part+= scale_dip_rel * (*Oxs_CalculateSDAXX_dip)(x,y,z-n_zPeriod); // scaled
      err2 += Oxs_CalculateSDAerrDIP_2(x2y2+(z-n_zPeriod)*(z-n_zPeriod),teXX);
    }
    // Add up using compensated summation.
    volatile OC_REALWIDE part_corr = part;           // this will be a summation corrector
    volatile OC_REALWIDE sum_corr = coef + part; // this will be a corrected sum
    part_corr -= sum_corr-coef;         
    summation_corrector += part_corr;
    coef = sum_corr; 
         
    // error of both infinite tails (positive)
    // this is |Ncont-Ndip|
    inf_tail_error = (*Oxs_CalculateSDAXX_cont_err)(x,y,z,zPeriod,n+1);
    // this is |Nimg-Ndip|
    if(teXX==n00 || teXX==n11 || teXX==n22)
          // diagonal elements
      inf_tail_error += error_A_dip_diag * \
                        Oxs_CalculateSDA_dip_err_accumulated(zPeriod,n+1);
    else  // off-diagonal elements
      inf_tail_error += error_A_dip_OFFdiag * \
                        Oxs_CalculateSDA_dip_err_accumulated(zPeriod,n+1);
  } while (
// such a test: 1 instead of N
//    (no_of_sum_elements==0 && err_ratio_cont_to_N<inf_tail_error
//                           && n<max_no_sum_elements) || // safety stopper, for instance when constantly coef==0
    (no_of_sum_elements==0                           // criterium based on err_ratio_cont_to_N (fidelity_level)
                           && fabs(coef+summation_corrector)*err_ratio_cont_to_N<inf_tail_error
                           && n<max_no_sum_elements) || // safety stopper, for instance when constantly coef==0
    (no_of_sum_elements!=0 && n< no_of_sum_elements) // criterium based on no_of_sum_elements
  );
  
  // "Infinite tails"
  const OC_REALWIDE infinity_tails = (*Oxs_CalculateSDAXX_tails)(x,y,z,zPeriod,n+1);
// till 16-2-2008 this line looked as following:
//  	(scale_dip_rel/3.) * (*Oxs_CalculateSDAXX_tailsOLD)(x,y,z,zPeriod,n+1); 
// The new for is corect with the paper and gives better result.
// (The bug might have been somewhere, I did not trace it)
//
// till 18-4-2007 this line looked as following:
//  	fabs(scale_dip_rel/3.) * (*Oxs_CalculateSDAXX_tails)(x,y,z,zPeriod,n+1); 
// This was apperently a bug.
  	
  // We add everything together:
  if(include_inf_tails)
    coef += summation_corrector + infinity_tails;
  else // not infinite tails. Debugging, probably
    coef += summation_corrector;

  // Statistics: errors, and not only
  
  // no of summation elements
  if(n_max[teXX]<static_cast<OC_REAL4m>(n)) n_max[teXX]=static_cast<OC_REAL4m>(n);
  if(n_min[teXX]>static_cast<OC_REAL4m>(n)) n_min[teXX]=static_cast<OC_REAL4m>(n);
  n_sum[teXX] += static_cast<OC_REAL4m>(n);

  if(compute_stats) {
    err2 += inf_tail_error*inf_tail_error;
      
    // Total errors
    if(err_max[teXX]<sqrt(err2)) err_max[teXX]=sqrt(err2);
    if(err_min[teXX]>sqrt(err2)) err_min[teXX]=sqrt(err2);
    // Instead of an average error a "total" error for the whole sample
    // is given. This total is computed as a \sum abs(error)
    err_sum[teXX] += sqrt(err2);
    
    // Summation compensation ratio
    const OC_REAL4m tmp = fabs(summation_corrector)/sqrt(err2);
    if(err_sum_ratio_max[teXX]<tmp) err_sum_ratio_max[teXX]=tmp;
    if(err_sum_ratio_min[teXX]>tmp) err_sum_ratio_min[teXX]=tmp;
    err_sum_ratio_sum[teXX] += tmp;
  }
  
  return coef;
}

OC_REALWIDE DemagPBC::Oxs_CalculateSDA(const T_tens_el teXX,
                  const OC_INDEX i, const OC_INDEX j, const OC_INDEX k) const
{ // conceptually const
  OC_REALWIDE coef;

  if(tensor_rw==NOTHING || tensor_rw==WRITE) {
    // we have to calculate it ...
    if(show_progress==1) { // progress
      cell_cnt++; // increase the cell counter
      // what can we report, 6 because we have six tensor elements
      OC_INDEX actual_cnt = (cell_cnt * progress_steps) / (totalsize * 6);
      if(actual_cnt > progress_cnt) {    // is it different from prev. value?
        progress_cnt = actual_cnt;
        char buf[100];
        sprintf(buf,"puts $klmchan %ld",(long)progress_cnt);
        int errcode = TCL_OK;
        errcode = Tcl_Eval(interp,buf);
        if(errcode!=TCL_OK)
          throw Oxs_Ext::Error((String("Tcl error\n")
                                    +Tcl_GetStringResult(interp)).c_str());
      }    
    }
    coef = Oxs_CalculateSDA_PBC_really(i, j, k, teXX);
        
    if(tensor_rw==WRITE)
      // filling up the tensor
      PutTensorElementToBuffer(coef, i+j*xdim+k*xydim, teXX);
  } else
    // we READ the tensor
    coef=GetTensorElementFromBuffer(i+j*xdim+k*xydim, teXX);
  
  return coef;
}                  

void DemagPBC::ReleaseMemory() const
{ // Conceptually const
  tensor_buffer.Release(); 
}

OC_REAL4m minimum6el(const OC_REAL4m *table) {
// shit, I hope it is ok. this table is: OC_REAL4m[tens_el_MAX]	
  OC_REAL4m tmp= table[n00];
  if(table[n11]<tmp) tmp= table[n11];
  if(table[n22]<tmp) tmp= table[n22];
  if(table[n01]<tmp) tmp= table[n01];
  if(table[n02]<tmp) tmp= table[n02];
  if(table[n12]<tmp) tmp= table[n12];
  return tmp;
}

OC_REAL4m maximum6el(const OC_REAL4m *table) {
  OC_REAL4m tmp= table[n00];
  if(table[n11]>tmp) tmp= table[n11];
  if(table[n22]>tmp) tmp= table[n22];
  if(table[n01]>tmp) tmp= table[n01];
  if(table[n02]>tmp) tmp= table[n02];
  if(table[n12]>tmp) tmp= table[n12];
  return tmp;
}

OC_REAL4m average6el(const OC_REAL4m *table) {
  OC_REAL4m tmp= 
    (table[n00]+table[n11]+table[n22]+table[n01]+table[n02]+table[n12])/6;
  return tmp;
}

OC_REAL4m sum6el(const OC_REAL4m *table) {
  OC_REAL4m tmp= 
    table[n00]+table[n11]+table[n22]+table[n01]+table[n02]+table[n12];
  return tmp;
}

void DemagPBC::Finalize() const
{
  // Statistics show buffer
  char buf_statistics[10024];
  // We write it to the file (only)
  if(tensor_rw==WRITE) {
    // Information show
    OC_REAL4m tmp = static_cast<OC_REAL4m>(totalsize);
    Oc_Snprintf(buf_statistics,sizeof(buf_statistics),
      "   ***   PBC statistics   ***\n"
          "max_celldistance_img_Diag=%g, max_celldistance_img_OffDiag=%g\n"
          "No of repetitions, n: %.6g (%.6g, %.6g) /average (minimum*, maximum)/\n"
          "Errors. Besides min & max, also a sum of all abs(error) is given.\n"
          "                 , e: %.6g (%.6g, %.6g) /sum_abs (minimum*, maximum)/\n"
          "Ratio <summation compensation>/<total error>\n"
          "                 , s: %.6g (%.6g, %.6g) /average (minimum*, maximum)/\n"
          "Below are tensor details of these values: /(XX, YY, ZZ)/ /(XY, XZ, YZ)/\n"
          "n_avg: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "n_min: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "n_max: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "e_sum_abs: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "e_min: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "e_max: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "s_avg: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "s_min: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "s_max: (%.6g, %.6g, %.6g) (%.6g, %.6g, %.6g)\n"
          "* minimum comment:\n"
          "These minimal values do not concern cases, where there are zeros because of symmetry.\n"
          "Thus it is possible that 'min'>'avg', because 'avg' takes these cases into account."
      ,max_celldistance_img_diag,max_celldistance_img_OFFdiag
      ,average6el(n_sum)/tmp, minimum6el(n_min),   maximum6el(n_max)
      ,sum6el(err_sum),       minimum6el(err_min), maximum6el(err_max)
      ,average6el(err_sum_ratio_sum)/tmp, minimum6el(err_sum_ratio_min)
                                        , maximum6el(err_sum_ratio_max)
                                        
      ,n_sum[n00]/tmp,n_sum[n11]/tmp,n_sum[n22]/tmp
      ,n_sum[n01]/tmp,n_sum[n02]/tmp,n_sum[n12]/tmp
      ,n_min[n00],n_min[n11],n_min[n22],n_min[n01],n_min[n02],n_min[n12]
      ,n_max[n00],n_max[n11],n_max[n22],n_max[n01],n_max[n02],n_max[n12]
                                        
      ,err_sum[n00],err_sum[n11],err_sum[n22],err_sum[n01],err_sum[n02],err_sum[n12]
      ,err_min[n00],err_min[n11],err_min[n22],err_min[n01],err_min[n02],err_min[n12]
      ,err_max[n00],err_max[n11],err_max[n22],err_max[n01],err_max[n02],err_max[n12]
      
      ,err_sum_ratio_sum[n00]/tmp,err_sum_ratio_sum[n11]/tmp,err_sum_ratio_sum[n22]/tmp
      ,err_sum_ratio_sum[n01]/tmp,err_sum_ratio_sum[n02]/tmp,err_sum_ratio_sum[n12]/tmp
      ,err_sum_ratio_min[n00],err_sum_ratio_min[n11],err_sum_ratio_min[n22]
      ,err_sum_ratio_min[n01],err_sum_ratio_min[n02],err_sum_ratio_min[n12]
      ,err_sum_ratio_max[n00],err_sum_ratio_max[n11],err_sum_ratio_max[n22]
      ,err_sum_ratio_max[n01],err_sum_ratio_max[n02],err_sum_ratio_max[n12]
    );	  
  }
    
  // dumping the tensor
  // as desription we give here the above statistics info
  if(tensor_rw==WRITE)
    save_tensor_file(tensor_file_prefix+"2"+tensor_file_suffix, 
                     title_2[exec_order]+important_parameters, buf_statistics);
	  
  // Progress closure
  if(show_progress==1 && tensor_rw!=READ) {
    int errcode = TCL_OK;
    errcode = Tcl_Eval(interp,Oc_AutoBuf("puts $klmchan exit"));
    if(errcode!=TCL_OK)
      throw Oxs_Ext::Error((String("Tcl error\n")
                                +Tcl_GetStringResult(interp)).c_str());
  }
  
  ReleaseMemory();
  
}

/*
if(i==3 && j==3 && k==3 && (teXX==n00 || teXX==n11 || teXX==n22) &&
(n==10000 || n==100 || n==1))         
{
        char buf[1024];
        
        Oc_Snprintf(buf,sizeof(buf),"...\n"
	  "%d %d\n"
	  "%.20g %.20g %.20g\n"
	  "%.20g\n"
	  ,n,teXX
	  ,sum_coef, part, scale_dip_rel * (*Oxs_CalculateSDAXX_dip)(x,y,z+n_zPeriod)
	  ,error_img_dip_diag * fabs(sum_coef)
	  );
//	throw Oxs_Ext::Error(buf);
        static Oxs_WarningMessage foo(9);
        foo.Send(revision_info,OC_STRINGIFY(__LINE__), buf);
}
    {
        char buf[1024];
        Oc_Snprintf(buf,sizeof(buf),"...\n"
	  "%s\n"
          "%d\n"
	  "%s\n"
	  ,str.c_str()
	  ,i
	  ,tmp.c_str()
	  );
	throw Oxs_Ext::Error(buf);
    }  
*/
