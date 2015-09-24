/* FILE: simpledemag.cc            -*-Mode: c++-*- 
 * version 2.0
 *
 * Average H demag field across rectangular cells.  Simple
 * implementation that does not take advantage of any memory
 * saving or calculation speed improvements possible using
 * symmetries in interaction tensor or realness of data in
 * spatial domain.
 *
 * This file is based on appriopriate Oxs_SimpleDemag file, which was written
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

#include <string>

#include "nb.h"
#include "director.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

#include "rectangularmesh.h"
#include "kl_simpledemag.h"     // KL(m)
// KL(m) "demagcoef.h" is not needed any more, we use it in PBC utilities
// This is SimpleDemag-specific
//#include "demagcoef.h"  
#include "fft.h"
#include "kl_pbc_util.h"        // KL(m) file with PBC utilities


OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Klm_SimpleDemag_PBC);

/* End includes */

// Helper function
UINT4m Klm_SimpleDemag_PBC::NextPowerOfTwo(UINT4m n) const
{ // Returns first power of two >= n
  UINT4m m=1;
  while(m<n) m*=2;
  return m;
}

// Constructor
Klm_SimpleDemag_PBC::Klm_SimpleDemag_PBC(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    // KL(m)
     zPeriod_param(0.0)
    ,error_A_img_diag(0.0),error_A_dip_diag(0.0)
    ,error_A_img_OFFdiag(0.0),error_A_dip_OFFdiag(0.0)
    ,err_ratio_cont_to_N(0.0),max_no_sum_elements(0)
    ,no_of_sum_elements(0),include_inf_tails(1)
    ,tensor_file_name(""),tensor_file_suffix(""),progress_script("")
    ,turbo("")
    ,compute_stats(0),
    // KL(m) end
    xdim(0),ydim(0),zdim(0),totalsize(0),
    pxdim(0),pydim(0),pzdim(0),ptotalsize(0),mesh_id(0),
    A00(NULL),A01(NULL),A02(NULL),A11(NULL),A12(NULL),A22(NULL),
    Mx(NULL),My(NULL),Mz(NULL),Hcomp(NULL)
{
  // KL(m), process arguments
  INT4m i_tmp;
  // Repetition distance for z-direction. In meters.
  // Value of zero will be later replaced by the z-dimension.
  zPeriod_param = GetRealInitValue("z_period",0.0);
  if(zPeriod_param<0.0) {
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
		"\nInvalid parameter value:"
		" Specified \"z_period\" is %g (should be >=0.0)",
		zPeriod_param);
    throw Oxs_Ext::Error(this,buf);
  }
  // Fitted accurancy/error (absolute) of the calculations: "image" & "dipole".
  // The errors are fitted with following function (r is the distance cell-cell):
  //      A * r^p
  // You should put a value here basing on previous results analysis.
  // Different values for diagonal (like nXX) and off-diag elements (like nYZ)
  error_A_img_diag    = GetRealInitValue("error_A_img_diag",    2.2e-17);
  error_A_dip_diag    = GetRealInitValue("error_A_dip_diag",    2.3e-2);
  error_A_img_OFFdiag = GetRealInitValue("error_A_img_OFFdiag", 1.2e-17);
  error_A_dip_OFFdiag = GetRealInitValue("error_A_dip_OFFdiag", 1.1e-2);
  if(error_A_img_diag    <= 0.0 ||
     error_A_dip_diag    <= 0.0 ||
     error_A_img_OFFdiag <= 0.0 ||
     error_A_dip_OFFdiag <= 0.0 ) {
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
		"\nInvalid parameter value. Following cases should never happen:\n"
		"    error_A_img_diag    <= 0.0 ||\n"
		"    error_A_dip_diag    <= 0.0 ||\n"
		"    error_A_img_OFFdiag <= 0.0 ||\n"
		"    error_A_dip_OFFdiag <= 0.0   \n" 
		"Specified values are as following (in the same order):\n",
		     error_A_img_diag   ,
		     error_A_dip_diag   ,
		     error_A_img_OFFdiag,
		     error_A_dip_OFFdiag);
    throw Oxs_Ext::Error(this,buf);
  }  
  // The summation will be done until the *cont* error is below a certain 
  // fraction of the actual tensor value:
  //     error_inf_tail < err_ratio_cont_to_N * N
  err_ratio_cont_to_N = GetRealInitValue("fidelity_level",1e-10);  
  if(err_ratio_cont_to_N<=0.0) {
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
		"\nInvalid parameter value:"
		" Specified \"fidelity_level\" is %g (should be >0.0)",
		err_ratio_cont_to_N);
    throw Oxs_Ext::Error(this,buf);
  }
  // Safety stopper in that case
  max_no_sum_elements = GetIntInitValue("max_no_sum_elements",100000);
  // Fixed number of repetitions in the summation.
  // As a default, instead of this criteria, number of repetitions is variable.
  // basing on the err_ratio_cont_to_N.
  no_of_sum_elements = GetIntInitValue("no_of_sum_elements",0);
  // Include inifinite tails computation?
  // This is evidently for debugging purposes only. Default: YES!
  i_tmp = GetUIntInitValue("include_inf_tails",1);
  if(i_tmp>1) {
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
		"\nInvalid parameter value:"
		" Specified \"include_inf_tails\" is %d (should be 0 or 1)",
		i_tmp);
    throw Oxs_Ext::Error(this,buf);
  }
  include_inf_tails = i_tmp;
  // Tensor file storage.
  tensor_file_name = GetStringInitValue("tensor_file_name", "");
  tensor_file_suffix = GetStringInitValue("tensor_file_suffix", ".ovf");
  // Script name for showing the progress. With full path.
  // If epmty/not-present: no progress show.
  progress_script = GetStringInitValue("progress_script", "");
  // Want to speed up the calculations by reducing the z-size of the evaluated tensor?
  // This can be done only for the case when zdim=NextPowerOfTwo(zim)
  // Choosing 'always' will cause error in other cases
  // Default: 'sometimes' (i.e. when possible)
  turbo = GetStringInitValue("turbo", "sometimes");
  if( turbo.compare("sometimes")!=0 &&
      turbo.compare("never")!=0 &&
      turbo.compare("always")!=0 ) {
    String msg=String("Invalid value of \"turbo\" parameter: ")
      + turbo
      + String("\n Should be one of sometimes, always, never.");
    throw Oxs_Ext::Error(this,msg.c_str());
  }
  // to perform deeper statistics, or not?
  i_tmp = GetUIntInitValue("compute_stats",0);
  if(i_tmp>1) {
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
		"\nInvalid parameter value:"
		" Specified \"compute_stats\" is %d (should be 0 or 1)",
		i_tmp);
    throw Oxs_Ext::Error(this,buf);
  }
  compute_stats = i_tmp;
  // KL(m) end
  VerifyAllInitArgsUsed();
}

BOOL Klm_SimpleDemag_PBC::Init()
{
  mesh_id = 0;
  ReleaseMemory();
  return Oxs_Energy::Init();
}

void Klm_SimpleDemag_PBC::ReleaseMemory() const
{ // Conceptually const
  if(Hcomp!=NULL) { delete[] Hcomp; Hcomp=NULL; }
  if(Mx!=NULL) { delete[] Mx; Mx=NULL; }
  if(My!=NULL) { delete[] My; My=NULL; }
  if(Mz!=NULL) { delete[] Mz; Mz=NULL; }
  if(A00!=NULL) { delete[] A00; A00=NULL; }
  if(A01!=NULL) { delete[] A01; A01=NULL; }
  if(A02!=NULL) { delete[] A02; A02=NULL; }
  if(A11!=NULL) { delete[] A11; A11=NULL; }
  if(A12!=NULL) { delete[] A12; A12=NULL; }
  if(A22!=NULL) { delete[] A22; A22=NULL; }
  xdim=ydim=zdim=totalsize=0;
  pxdim=pydim=pzdim=ptotalsize=0;
}

void Klm_SimpleDemag_PBC::FillCoefficientArrays(const Oxs_Mesh* genmesh) const
{ // This routine is conceptually const.
  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(genmesh);
  if(mesh==NULL) {
    String msg = String("Object ")
      + String(genmesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_Ext::Error(msg.c_str());
  }

  // Clean-up from previous allocation, if any.
  ReleaseMemory();

  // Fill dimension variables
  xdim = mesh->DimX();
  ydim = mesh->DimY();
  zdim = mesh->DimZ();
  totalsize=xdim*ydim*zdim;
  if(xdim==0 || ydim==0 || zdim==0) return; // Empty mesh!
  if(totalsize < xdim || totalsize < ydim || totalsize < zdim) {
    // Partial overflow check
    String msg = String("UINT4m overflow in ") + String(InstanceName())
      + String(": Product xdim*ydim*zdim too big"
               " to fit in a UINT4m variable");
    throw Oxs_Ext::Error(msg.c_str());
  }
  pxdim = NextPowerOfTwo(2*xdim);
  pydim = NextPowerOfTwo(2*ydim);
  
  // KL(m)
  // Regarding pzdim, its value depends on the turbo parameter
  BOOL turbo_used=0;
  if(  turbo.compare("sometimes")==0 && NextPowerOfTwo(zdim)==zdim 
    && zPeriod_param==0.0 ) {
    // turbo _can_ be ON
    // we don't multiply by 2, NextPowerOfTwo is also not necessary
    turbo_used=1;
    pzdim=zdim;
  } else if( turbo.compare("always")==0 ) {
    // turbo _must_ be ON
    if(NextPowerOfTwo(zdim) != zdim) {
      // necessary condition is not fulfilled
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
	    	"For TURBO you need the z-dimension (%d) to be"
	    	" power-of-2: %d\n",
	    	zdim, NextPowerOfTwo(zdim));
      throw Oxs_Ext::Error(buf);	     
    }
    if(zPeriod_param != 0.0) {
      // necessary condition is not fulfilled
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
	    	"For TURBO you need the zPeriod parameter (%g) to be"
	    	" equal to zero (0.0)\n", zPeriod_param);
      throw Oxs_Ext::Error(buf);	     
    }
    // turbo ON
    turbo_used=1;
    pzdim=zdim;
  } else {
    // Other cases, icluding "never"
    // turbo OFF
    turbo_used=0;
    pzdim=NextPowerOfTwo(2*zdim);
  }
  // KL(m) end
  
  ptotalsize=pxdim*pydim*pzdim;
  if(ptotalsize < pxdim || ptotalsize < pydim || ptotalsize < pzdim) {
    // Partial overflow check
    String msg = String("UINT4m overflow in ") + String(InstanceName())
      + String(": Product pxdim*pydim*pzdim too big"
               " to fit in a UINT4m variable");
    throw Oxs_Ext::Error(msg.c_str());
  }

  // Allocate memory for interaction matrices and magnetization components
  A00   = new Oxs_Complex[ptotalsize];
  A01   = new Oxs_Complex[ptotalsize];
  A02   = new Oxs_Complex[ptotalsize];
  A11   = new Oxs_Complex[ptotalsize];
  A12   = new Oxs_Complex[ptotalsize];
  A22   = new Oxs_Complex[ptotalsize];
  Mx    = new Oxs_Complex[ptotalsize];
  My    = new Oxs_Complex[ptotalsize];
  Mz    = new Oxs_Complex[ptotalsize];
  Hcomp = new Oxs_Complex[ptotalsize];
  if(Hcomp==NULL || Mx==NULL || My==NULL || Mz==NULL
     || A00==NULL || A01==NULL || A02==NULL
     || A11==NULL || A12==NULL || A22==NULL) {
    // Safety check for those machines on which new[] doesn't throw
    // BadAlloc.
    String msg = String("Insufficient memory in simpledemag constructor.");
    throw Oxs_Ext::Error(msg.c_str());
  }
  
  // Initialize interaction matrices to zeros
  UINT4m pindex;
  for(pindex=0;pindex<ptotalsize;pindex++) A00[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) A01[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) A02[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) A11[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) A12[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) A22[pindex].Set(0.,0.);
  
  // According (16) in Newell's paper, the demag field is given by
  //                        H = -N*M
  // where N is the "demagnetizing tensor," with components Nxx, Nxy,
  // etc.  With the '-1' in 'scale' we store '-N' instead of 'N',
  // so we don't have to multiply the output from the FFT + iFFT
  // by -1 in ConstMagField() below.

  // Fill interaction matrices with demag coefs from Newell's paper.
  // Note that A00, A11 and A22 are even in x,y and z.
  // A01 is odd in x and y, even in z.
  // A02 is odd in x and z, even in y.
  // A12 is odd in y and z, even in x.
  UINT4m i,j,k;
  UINT4m pxydim=pxdim*pydim;
  REALWIDE dx = mesh->EdgeLengthX();
  REALWIDE dy = mesh->EdgeLengthY();
  REALWIDE dz = mesh->EdgeLengthZ();
  // For demag calculation, all that matters is the relative
  // sizes of dx, dy and dz.  To help insure we don't run
  // outside floating point range, rescale these values so
  // largest is 1.0
  REALWIDE maxedge=dx;
  if(dy>maxedge) maxedge=dy;
  if(dz>maxedge) maxedge=dz;
  dx/=maxedge; dy/=maxedge; dz/=maxedge;
  REALWIDE scale = -1./(4*PI*dx*dy*dz);
  
  // KL(m)
  DemagPBC demag_pbc( genmesh, 
                      zPeriod_param,
		      error_A_img_diag   ,
		      error_A_dip_diag   ,
		      error_A_img_OFFdiag,
		      error_A_dip_OFFdiag,
		      err_ratio_cont_to_N,
		      max_no_sum_elements,
		      no_of_sum_elements,
		      include_inf_tails,
                      tensor_file_name,
                      tensor_file_suffix,
                      progress_script,
                      _SimpleDemag,
                      compute_stats
                    ); 
  demag_pbc.Init();                    
  
  // 
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    
    // KL(m)
    //REALWIDE x = dx*i;
    //REALWIDE y = dy*j;
    //REALWIDE z = dz*k;
    //REALWIDE coef=scale*CalculateSDA00(x,y,z,dx,dy,dz);
    REALWIDE coef = scale*demag_pbc.CalculateSDA(n00, i, j, k); 

    // KL(m)
    if(turbo_used==0) {
      // originall code. Large pzdim
      A00[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A00[(pxdim-i)+j*pxdim+k*pxydim].Set(coef,0.);
      if(j>0) A00[i+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(k>0) A00[i+j*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(i>0 && j>0)
        A00[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(i>0 && k>0)
        A00[(pxdim-i)+j*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(j>0 && k>0)
        A00[i+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(i>0 && j>0 && k>0)
        A00[(pxdim-i)+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
    } else {
      // smaller pzdim
      A00[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A00[(pxdim-i)+j*pxdim+k*pxydim].Set(coef,0.);
      if(j>0) A00[i+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(i>0 && j>0)
        A00[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
    }
    // KL(m) end
    
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    
    // KL(m)
    //REALWIDE x = dx*i;
    //REALWIDE y = dy*j;
    //REALWIDE z = dz*k;
    //REALWIDE coef=scale*CalculateSDA11(x,y,z,dx,dy,dz);
    REALWIDE coef = scale*demag_pbc.CalculateSDA(n11, i, j, k); 

    // KL(m)
    if(turbo_used==0) {
      // originall code. Large pzdim
      A11[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A11[(pxdim-i)+j*pxdim+k*pxydim].Set(coef,0.);
      if(j>0) A11[i+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(k>0) A11[i+j*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(i>0 && j>0)
        A11[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(i>0 && k>0)
        A11[(pxdim-i)+j*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(j>0 && k>0)
        A11[i+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(i>0 && j>0 && k>0)
        A11[(pxdim-i)+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
    } else {
      // smaller pzdim
      A11[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A11[(pxdim-i)+j*pxdim+k*pxydim].Set(coef,0.);
      if(j>0) A11[i+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(i>0 && j>0)
        A11[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
    }
    // KL(m) end
    
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    
    // KL(m)
    //REALWIDE x = dx*i;
    //REALWIDE y = dy*j;
    //REALWIDE z = dz*k;
    //REALWIDE coef=scale*CalculateSDA22(x,y,z,dx,dy,dz);
    REALWIDE coef = scale*demag_pbc.CalculateSDA(n22, i, j, k); 

    // KL(m)
    if(turbo_used==0) {
      // originall code. Large pzdim
      A22[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A22[(pxdim-i)+j*pxdim+k*pxydim].Set(coef,0.);
      if(j>0) A22[i+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(k>0) A22[i+j*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(i>0 && j>0)
        A22[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(i>0 && k>0)
        A22[(pxdim-i)+j*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(j>0 && k>0)
        A22[i+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(i>0 && j>0 && k>0)
        A22[(pxdim-i)+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
    } else {
      // smaller pzdim
      A22[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A22[(pxdim-i)+j*pxdim+k*pxydim].Set(coef,0.);
      if(j>0) A22[i+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(i>0 && j>0)
        A22[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
    }
    // KL(m) end
      
  }

  // KL(m)
  demag_pbc.middle_load_or_save_tensor_file();
  
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    
    // KL(m)
    //REALWIDE x = dx*i;
    //REALWIDE y = dy*j;
    //REALWIDE z = dz*k;
    //REALWIDE coef=scale*CalculateSDA01(x,y,z,dx,dy,dz);
    REALWIDE coef = scale*demag_pbc.CalculateSDA(n01, i, j, k); 

    // KL(m)
    if(turbo_used==0) {
      // originall code. Large pzdim
      A01[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A01[(pxdim-i)+j*pxdim+k*pxydim].Set(-coef,0.);
      if(j>0) A01[i+(pydim-j)*pxdim+k*pxydim].Set(-coef,0.);
      if(k>0) A01[i+j*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(i>0 && j>0)
        A01[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(i>0 && k>0)
        A01[(pxdim-i)+j*pxdim+(pzdim-k)*pxydim].Set(-coef,0.);
      if(j>0 && k>0)
        A01[i+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(-coef,0.);
      if(i>0 && j>0 && k>0)
        A01[(pxdim-i)+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
    } else {
      // smaller pzdim
      A01[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A01[(pxdim-i)+j*pxdim+k*pxydim].Set(-coef,0.);
      if(j>0) A01[i+(pydim-j)*pxdim+k*pxydim].Set(-coef,0.);
      if(i>0 && j>0)
        A01[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
    }
    // KL(m) end
      
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    
    // KL(m)
    //REALWIDE x = dx*i;
    //REALWIDE y = dy*j;
    //REALWIDE z = dz*k;
    //REALWIDE coef=scale*CalculateSDA02(x,y,z,dx,dy,dz);
    REALWIDE coef = scale*demag_pbc.CalculateSDA(n02, i, j, k); 

    // KL(m)
    if(turbo_used==0) {
      // originall code. Large pzdim
      A02[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A02[(pxdim-i)+j*pxdim+k*pxydim].Set(-coef,0.);
      if(j>0) A02[i+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(k>0) A02[i+j*pxdim+(pzdim-k)*pxydim].Set(-coef,0.);
      if(i>0 && j>0)
        A02[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(-coef,0.);
      if(i>0 && k>0)
        A02[(pxdim-i)+j*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(j>0 && k>0)
        A02[i+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(-coef,0.);
      if(i>0 && j>0 && k>0)
        A02[(pxdim-i)+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
    } else {
      // smaller pzdim
      A02[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A02[(pxdim-i)+j*pxdim+k*pxydim].Set(-coef,0.);
      if(j>0) A02[i+(pydim-j)*pxdim+k*pxydim].Set(coef,0.);
      if(i>0 && j>0)
        A02[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(-coef,0.);
    }
    // KL(m) end
      
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    
    // KL(m)
    //REALWIDE x = dx*i;
    //REALWIDE y = dy*j;
    //REALWIDE z = dz*k;
    //REALWIDE coef=scale*CalculateSDA12(x,y,z,dx,dy,dz);
    REALWIDE coef = scale*demag_pbc.CalculateSDA(n12, i, j, k); 

    // KL(m)
    if(turbo_used==0) {
      // originall code. Large pzdim
      A12[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A12[(pxdim-i)+j*pxdim+k*pxydim].Set(coef,0.);
      if(j>0) A12[i+(pydim-j)*pxdim+k*pxydim].Set(-coef,0.);
      if(k>0) A12[i+j*pxdim+(pzdim-k)*pxydim].Set(-coef,0.);
      if(i>0 && j>0)
        A12[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(-coef,0.);
      if(i>0 && k>0)
        A12[(pxdim-i)+j*pxdim+(pzdim-k)*pxydim].Set(-coef,0.);
      if(j>0 && k>0)
        A12[i+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
      if(i>0 && j>0 && k>0)
        A12[(pxdim-i)+(pydim-j)*pxdim+(pzdim-k)*pxydim].Set(coef,0.);
    } else {
      // smaller pzdim
      A12[i+j*pxdim+k*pxydim].Set(coef,0.);
      if(i>0) A12[(pxdim-i)+j*pxdim+k*pxydim].Set(coef,0.);
      if(j>0) A12[i+(pydim-j)*pxdim+k*pxydim].Set(-coef,0.);
      if(i>0 && j>0)
        A12[(pxdim-i)+(pydim-j)*pxdim+k*pxydim].Set(-coef,0.);
    }
    // KL(m) end
      
  }
  
  // KL(m)
  demag_pbc.Finalize();
      
  // Transform interaction matrices into frequency domain.
  fft.Forward(pxdim,pydim,pzdim,A00);
  fft.Forward(pxdim,pydim,pzdim,A01);
  fft.Forward(pxdim,pydim,pzdim,A02);
  fft.Forward(pxdim,pydim,pzdim,A11);
  fft.Forward(pxdim,pydim,pzdim,A12);
  fft.Forward(pxdim,pydim,pzdim,A22);
}

void Klm_SimpleDemag_PBC::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  // (Re)-initialize mesh coefficient array if mesh has changed.
  if(mesh_id != state.mesh->Id()) {
    mesh_id = 0; // Safety
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

  // Calculate FFT of Mx, My and Mz
  UINT4m xydim=xdim*ydim;
  UINT4m pxydim=pxdim*pydim;
  UINT4m index,pindex;
  UINT4m i,j,k;
  for(pindex=0;pindex<ptotalsize;pindex++) Mx[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) My[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) Mz[pindex].Set(0.,0.);
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    index  = i+j*xdim+k*xydim;
    pindex = i+j*pxdim+k*pxydim;
    REAL8m scale = Ms[index];
    const ThreeVector& vec = spin[index];
    Mx[pindex].Set(scale*(vec.x),0);
    My[pindex].Set(scale*(vec.y),0);
    Mz[pindex].Set(scale*(vec.z),0);
  }
  fft.Forward(pxdim,pydim,pzdim,Mx);
  fft.Forward(pxdim,pydim,pzdim,My);
  fft.Forward(pxdim,pydim,pzdim,Mz);
  
  // Calculate field components in frequency domain, then to iFFT
  // to transform back to space domain.

  // Hx component
  for(pindex=0;pindex<ptotalsize;pindex++) {
    Hcomp[pindex] = A00[pindex]*Mx[pindex]
      + A01[pindex]*My[pindex] + A02[pindex]*Mz[pindex];
  }
  fft.Inverse(pxdim,pydim,pzdim,Hcomp);
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    index  = i+j*xdim+k*xydim;
    pindex = i+j*pxdim+k*pxydim;
    field[index].x=Hcomp[pindex].real();
  }
  
  // Hy component
  for(pindex=0;pindex<ptotalsize;pindex++) {
    Hcomp[pindex] = A01[pindex]*Mx[pindex]
      + A11[pindex]*My[pindex] + A12[pindex]*Mz[pindex];
  }
  fft.Inverse(pxdim,pydim,pzdim,Hcomp);
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    index  = i+j*xdim+k*xydim;
    pindex = i+j*pxdim+k*pxydim;
    field[index].y=Hcomp[pindex].real();
  }
  
  // Hz component
  for(pindex=0;pindex<ptotalsize;pindex++) {
    Hcomp[pindex] = A02[pindex]*Mx[pindex]
      + A12[pindex]*My[pindex] + A22[pindex]*Mz[pindex];
  }
  fft.Inverse(pxdim,pydim,pzdim,Hcomp);
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    index  = i+j*xdim+k*xydim;
    pindex = i+j*pxdim+k*pxydim;
    field[index].z=Hcomp[pindex].real();
  }

  // Calculate pointwise energy density: -0.5*MU0*<M,H>
  REAL8m mult = -0.5 * MU0;
  for(index=0;index<totalsize;index++) {
    energy[index] = mult * Ms[index] * (spin[index]*field[index]);
  }

}

      
    
    
/*    
    if(((i==0 && j==1)||(i==1 && j==0)) && k==1 && n<5)
    // tuuuuu
    {
        char buf[1024];
        Oc_Snprintf(buf,sizeof(buf),"00\n"
	  "%d %d %d %d, *%d %d*\n"
	  "%.7g %.7g %.7g\n"
	  "%.7g %.7g %.7g\n"
	  "%.7g %.7g %.7g\n"
	  ,i,j,k,n
	  ,n_pos_img_switch,n_neg_img_switch
	  ,coef,coef,coef
	  ,coef_part_pos,coef_part_neg,sum_coef
          ,fabs(sum_coef/coef)
          ,fabs(summation_corrector/coef)
          ,summation_corrector
	  );
	static Oxs_WarningMessage foo(8);
        foo.Send(revision_info,OC_STRINGIFY(__LINE__), buf);
	//throw Oxs_Ext::Error(this,buf); 
    }
*/    

/*
    if( // zakres 0..19x0..19x0..49
//      (i==11&&j==13&&k==23)
//      ||
(i==2&&j==10&&k==19)
//||(i==13&&j==5&&k==3)||(i==19&&j==11&&k==12)||(i==17&&j==10&&k==36)||
//      (i==3&&j==13&&k==38)||(i==2&&j==8&&k==44)||(i==10&&j==12&&k==37)||(i==5&&j==19&&k==45) 
      )
    if( // slice, y==0, x=0,9,19,29,39
      j==0&&(i==0||i==9||i==19||i==29||i==39) )      
//    if( i!=0 && k!=0 )

//    if( coef!=0 && fabs(sum_coef/coef)>5e17 )
//        fabs(sum_coef/(coef+sum_coef+summation_corrector+infinity_tails))>200||
//        fabs(infinity_tails/(coef+sum_coef+summation_corrector+infinity_tails))>2e-3 )
//    if( tmp>1 )
    if( comp_err>1 )
    // tuuuuu
    {
        char buf[1024];
        Oc_Snprintf(buf,sizeof(buf),"00\n"
	  "%d %d %d\n"
	  "%.20g %.20g %.20g\n"
	  "%.20g %.20g %.20g\n"
	  "(%.20g) %.20g *%.20g*\n"
	  ,i,j,k
	  ,coef - sum_coef - summation_corrector - infinity_tails,sum_coef,infinity_tails
          ,error_img_dip_diag*fabs(coef-infinity_tails-sum_coef-summation_corrector) / fabs(coef)
          ,error_img_dip_diag*fabs(sum_coef) / fabs(coef)
          ,fabs(inf_tail_error/coef)
	  ,summation_corrector,tmp,coef
	  );
	static Oxs_WarningMessage foo(9);
        foo.Send(revision_info,OC_STRINGIFY(__LINE__), buf);
	//throw Oxs_Ext::Error(this,buf); 
    }
*/

/*
    if( comp_err>1e-2 )
    {
	    // tuuu
	char buf[1024];
        Oc_Snprintf(buf,sizeof(buf),
	  "%d %d %d \n"
	  "%g\n"
	  "%.4g\t%.4g\t%.4g\n"
	  "tensor:\t\t%.4g\t%.4g\n"
	  "\t\t\t\t%.4g\n"
	  "trace: %.4g, det: %.4g\n"
	  "%.4g\t%.4g\t%.4g\n"
	  "infty:\t\t%.4g\t%.4g\n"
	  "\t\t\t\t%.4g\n"
	  "infty_trace: %.4g\n"
	  "%.4g\t%.4g\t%.4g\n"
	  "sum:\t\t%.4g\t%.4g\n"
	  "\t\t\t\t%.4g\n"
	  "sum_trace: %.4g\n"
	  ,i,j,k
	  ,diag_total_abs_error
          ,A00[i+j*pxdim+k*pxydim].real()
          ,A01[i+j*pxdim+k*pxydim].real()
          ,A02[i+j*pxdim+k*pxydim].real()
          ,A11[i+j*pxdim+k*pxydim].real()
          ,A12[i+j*pxdim+k*pxydim].real()
          ,A22[i+j*pxdim+k*pxydim].real()
          ,A00[i+j*pxdim+k*pxydim].real()+A11[i+j*pxdim+k*pxydim].real()+A22[i+j*pxdim+k*pxydim].real()
          ,det
          ,A00_pbc_error[i+j*xdim+k*xydim].real()
          ,A01_pbc_error[i+j*xdim+k*xydim].real()
          ,A02_pbc_error[i+j*xdim+k*xydim].real()
          ,A11_pbc_error[i+j*xdim+k*xydim].real()
          ,A12_pbc_error[i+j*xdim+k*xydim].real()
          ,A22_pbc_error[i+j*xdim+k*xydim].real()
          ,A00_pbc_error[i+j*xdim+k*xydim].real()+A11_pbc_error[i+j*xdim+k*xydim].real()+A22_pbc_error[i+j*xdim+k*xydim].real()
          ,A00_pbc_error[i+j*xdim+k*xydim].imag()
          ,A01_pbc_error[i+j*xdim+k*xydim].imag()
          ,A02_pbc_error[i+j*xdim+k*xydim].imag()
          ,A11_pbc_error[i+j*xdim+k*xydim].imag()
          ,A12_pbc_error[i+j*xdim+k*xydim].imag()
          ,A22_pbc_error[i+j*xdim+k*xydim].imag()
          ,A00_pbc_error[i+j*xdim+k*xydim].imag()+A11_pbc_error[i+j*xdim+k*xydim].imag()+A22_pbc_error[i+j*xdim+k*xydim].imag()
	  );
	static Oxs_WarningMessage foo(9);
        foo.Send(revision_info,OC_STRINGIFY(__LINE__), buf);
        //throw Oxs_Ext::Error(this,buf); 
    }
*/

