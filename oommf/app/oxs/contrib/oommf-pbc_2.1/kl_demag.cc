/* FILE: demag.cc            -*-Mode: c++-*- 
 * version 2.0
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
#include "kl_demag.h"           // KL(m)
// KL(m) "demagcoef.h" still needed here
// This is Demag-specific
#include "demagcoef.h"  
#include "fft.h"
#include "kl_pbc_util.h"        // KL(m) file with PBC utilities

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Klm_Demag_PBC);

/* End includes */

// Helper function
UINT4m Klm_Demag_PBC::NextPowerOfTwo(UINT4m n) const
{ // Returns first power of two >= n
  UINT4m m=1;
  while(m<n) m*=2;
  return m;
}

// Constructor
Klm_Demag_PBC::Klm_Demag_PBC(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
  
    // KL(m)
     zPeriod_param(0.0)
    ,error_A_img_diag(0.0),   error_A_dip_diag(0.0)
    ,error_A_img_OFFdiag(0.0),error_A_dip_OFFdiag(0.0)
    ,err_ratio_cont_to_N(0.0),max_no_sum_elements(0)
    ,no_of_sum_elements(0),include_inf_tails(1)
    ,tensor_file_name(""),tensor_file_suffix(""),progress_script("")
//    ,turbo("")
    ,compute_stats(0),
    // KL(m) end
  
    rdimx(0),rdimy(0),rdimz(0),
    cdimx(0),cdimy(0),cdimz(0),cstridey(0),cstridez(0),
    mesh_id(0),
    adimx(0),adimy(0),adimz(0),astridey(0),astridez(0),
    A00(NULL),A01(NULL),A02(NULL),A11(NULL),A12(NULL),A22(NULL),
    xcomp(NULL),ycomp(NULL),zcomp(NULL),fillcoefs(0)
{
	
// KL(m), PBC works _only_ with FillCoefficientArraysStandard	
// Thus, the initialization part is slowed down.	
/* 
  String kernel_fill_routine
    = GetStringInitValue("kernel_fill_routine","fast");
  if(kernel_fill_routine.compare("standard")==0) {
*/	  
    fillcoefs = &Klm_Demag_PBC::FillCoefficientArraysStandard;
/*    
  } else if(kernel_fill_routine.compare("fast")==0) {
    fillcoefs = &Klm_Demag_PBC::FillCoefficientArraysFast;
  } else {
    String msg = String("Invalid kernel_fill_routine request: ");
    msg += kernel_fill_routine;
    throw Oxs_Ext::Error(this,msg);    
  }
*/  

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
  // *** to turbo argument ***
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

Klm_Demag_PBC::~Klm_Demag_PBC() {
#if REPORT_TIME
  Oc_TimeVal cpu,wall;
  ffttime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...   fft%7.2f cpu /%7.2f wall,"
            " (%s)\n",
            double(cpu),double(wall),InstanceName());
    convtime.GetTimes(cpu,wall);
    fprintf(stderr,"      subtime ...  conv%7.2f cpu /%7.2f wall,"
            " (%s)\n",
            double(cpu),double(wall),InstanceName());
    dottime.GetTimes(cpu,wall);
    fprintf(stderr,"      subtime ...   dot%7.2f cpu /%7.2f wall,"
            " (%s)\n",
            double(cpu),double(wall),InstanceName());
  }
#endif // REPORT_TIME
  ReleaseMemory();
}

BOOL Klm_Demag_PBC::Init()
{
#if REPORT_TIME
  Oc_TimeVal cpu,wall;
  ffttime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      subtime ...   fft%7.2f cpu /%7.2f wall,"
            " subtime module %s\n",
            double(cpu),double(wall),InstanceName());
    convtime.GetTimes(cpu,wall);
    fprintf(stderr,"              ...  conv%7.2f cpu /%7.2f wall,"
            " subtime module %s\n",
            double(cpu),double(wall),InstanceName());
    dottime.GetTimes(cpu,wall);
    fprintf(stderr,"      subtime ...   dot%7.2f cpu /%7.2f wall,"
            " (%s)\n",
            double(cpu),double(wall),InstanceName());
  }
  ffttime.Reset();
  convtime.Reset();
  dottime.Reset();
#endif // REPORT_TIME
  mesh_id = 0;
  ReleaseMemory();
  return Oxs_Energy::Init();
}

void Klm_Demag_PBC::ReleaseMemory() const
{ // Conceptually const
  if(xcomp!=NULL) { delete[] xcomp; xcomp=NULL; }
  if(ycomp!=NULL) { delete[] ycomp; ycomp=NULL; }
  if(zcomp!=NULL) { delete[] zcomp; zcomp=NULL; }
  if(A00!=NULL) { delete[] A00; A00=NULL; }
  if(A01!=NULL) { delete[] A01; A01=NULL; }
  if(A02!=NULL) { delete[] A02; A02=NULL; }
  if(A11!=NULL) { delete[] A11; A11=NULL; }
  if(A12!=NULL) { delete[] A12; A12=NULL; }
  if(A22!=NULL) { delete[] A22; A22=NULL; }
  rdimx=rdimy=rdimz=0;
  cdimx=cdimy=cdimz=0;
  cstridey=cstridez=0;
  adimx=adimy=adimz=0;
  astridey=astridez=0;
}

void Klm_Demag_PBC::FillCoefficientArraysFast(const Oxs_Mesh* genmesh) const
{ // This routine is conceptually const.

  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(genmesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(genmesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_Ext::Error(this,msg);
  }

  // Clean-up from previous allocation, if any.
  ReleaseMemory();

  // Fill dimension variables
  rdimx = mesh->DimX();
  rdimy = mesh->DimY();
  rdimz = mesh->DimZ();
  if(rdimx==0 || rdimy==0 || rdimz==0) return; // Empty mesh!

  cdimx = NextPowerOfTwo(2*rdimx)/2;
  if(rdimy>1) cdimy = NextPowerOfTwo(2*rdimy);
  else        cdimy = 1;
  if(rdimz>1) cdimz = NextPowerOfTwo(2*rdimz);
  else        cdimz = 1;
  cstridey = cdimx + 1;  // Pad by one to avoid cache line entanglement
  cstridez = cstridey*cdimy;

  UINT4m ctotalsize=cstridez*cdimz;
  UINT4m rtotalsize=2*ctotalsize;
  if(rtotalsize<2*cdimx || rtotalsize<cdimy || rtotalsize<cdimz) {
    // Partial overflow check
    String msg =
      String("UINT4m overflow in ")
      + String(InstanceName())
      + String(": Product cdimx*cdimy*cdimz too big"
               " to fit in a UINT4m variable");
    throw Oxs_Ext::Error(this,msg);
  }

  // Allocate memory for interaction matrices and magnetization components
  xcomp = new Oxs_Complex[ctotalsize];
  ycomp = new Oxs_Complex[ctotalsize];
  zcomp = new Oxs_Complex[ctotalsize];
  adimx=1+cdimx;
  adimy=1+cdimy/2;
  adimz=1+cdimz/2;
  astridey=adimx;
  astridez=adimy*astridey;
  UINT4m atotalsize=adimz*astridez;
  A00   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A01   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A02   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A11   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A12   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A22   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  if(xcomp==NULL || ycomp==NULL || zcomp==NULL
     || A00==NULL || A01==NULL || A02==NULL
     || A11==NULL || A12==NULL || A22==NULL) {
    // Safety check for those machines on which new[] doesn't throw
    // BadAlloc.
    String msg = String("Insufficient memory in Demag constructor.");
    throw Oxs_Ext::Error(this,msg);
  }

  REALWIDE *rxcomp = static_cast<REALWIDE*>(static_cast<void*>(xcomp));
  REALWIDE *rycomp = static_cast<REALWIDE*>(static_cast<void*>(ycomp));
  REALWIDE *rzcomp = static_cast<REALWIDE*>(static_cast<void*>(zcomp));
  UINT4m rstridey=2*cstridey;
  UINT4m rstridez=2*cstridez;

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
  // We use these symmetries to reduce storage requirements.  If
  // f is real and even, then f^ is also real and even.  If f
  // is real and odd, then f^ is (pure) imaginary and odd.
  // As a result, the transform of each of the Axx interaction
  // matrices will be real, with the same even/odd properties.
  //
  // Notation:  A00:=fs*Nxx, A01:=fs*Nxy, A02:=fs*Nxz,
  //                         A11:=fs*Nyy, A12:=fs*Nyz
  //                                      A22:=fs*Nzz
  //  where fs = -1/(2*cdimx*cdimy*cdimz)

  UINT4m index,i,j,k,istop,jstop,kstop;

  REALWIDE dx = mesh->EdgeLengthX();
  REALWIDE dy = mesh->EdgeLengthY();
  REALWIDE dz = mesh->EdgeLengthZ();
  // For demag calculation, all that matters is the relative
  // size of dx, dy and dz.  To help insure we don't run
  // outside floating point range, rescale these values so
  // largest is 1.0
  REALWIDE maxedge=dx;
  if(dy>maxedge) maxedge=dy;
  if(dz>maxedge) maxedge=dz;
  dx/=maxedge; dy/=maxedge; dz/=maxedge;

  REALWIDE scale = 1.0/(4*PI*dx*dy*dz);

  // Also throw in FFT scaling.  This allows the "NoScale" FFT routines
  // to be used.
  scale *= 1.0/(2*cdimx*cdimy*cdimz);

  for(index=0;index<ctotalsize;index++) {
    // Initialize temp space to zeros
    xcomp[index].Set(0.,0.);
    ycomp[index].Set(0.,0.);
    zcomp[index].Set(0.,0.);
  }

  // Calculate Nxx, Nyy and Nzz in first octant.
  // Step 1: Evaluate f at each cell site.  Offset by (-dx,-dy,-dz)
  //  so we can do 2nd derivative operations "in-place".
  kstop=1; if(rdimz>1) kstop=rdimz+2;
  jstop=1; if(rdimy>1) jstop=rdimy+2;
  istop=1; if(rdimx>1) istop=rdimx+2;
  for(k=0;k<kstop;k++) {
    UINT4m kindex = k*rstridez;
    REALWIDE z = 0.0;
    if(k==0) z = -dz;
    else if(k>1) z = dz*(k-1);
    for(j=0;j<jstop;j++) {
      UINT4m jkindex = kindex + j*rstridey;
      REALWIDE y = 0.0;
      if(j==0) y = -dy;
      else if(j>1) y = dy*(j-1);
      for(i=0;i<istop;i++) {
	index = i+jkindex;
#ifndef NDEBUG
	if(index>=rtotalsize) {
	  String msg = String("Programming error:"
                              " array index out-of-bounds.");
	  throw Oxs_Ext::Error(this,msg);
	}
#endif // NDEBUG
	REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rxcomp[index] = scale*Newell_f(x,y,z);
	rycomp[index] = scale*Newell_f(y,x,z);
	rzcomp[index] = scale*Newell_f(z,y,x);
      }
    }
  }

  // Step 2a: Do d^2/dz^2
  if(kstop==1) {
    // Only 1 layer in z-direction of f stored in r?comp arrays.
    for(j=0;j<jstop;j++) {
      UINT4m jkindex = j*rstridey;
      REALWIDE y = 0.0;
      if(j==0) y = -dy;
      else if(j>1) y = dy*(j-1);
      for(i=0;i<istop;i++) {
	index = i+jkindex;
	REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rxcomp[index] -= scale*Newell_f(x,y,0);
	rxcomp[index] *= 2;
	rycomp[index] -= scale*Newell_f(y,x,0);
	rycomp[index] *= 2;
	rzcomp[index] -= scale*Newell_f(0,y,x);
	rzcomp[index] *= 2;
	/// Use f is even in z, so for example
	/// f(x,y,-dz) - 2f(x,y,0) + f(x,y,dz)
	///     =  2( f(x,y,-dz) - f(x,y,0) )
      }
    }
  } else {
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      for(j=0;j<jstop;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] += rxcomp[index+2*rstridez];
	  rxcomp[index] -= 2*rxcomp[index+rstridez];
	  rycomp[index] += rycomp[index+2*rstridez];
	  rycomp[index] -= 2*rycomp[index+rstridez];
	  rzcomp[index] += rzcomp[index+2*rstridez];
	  rzcomp[index] -= 2*rzcomp[index+rstridez];
	}
      }
    }
    for(k=rdimz;k<kstop;k++) { // Zero-fill overhang
      UINT4m kindex = k*rstridez;
      for(j=0;j<jstop;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] = 0.0;
	  rycomp[index] = 0.0;
	  rzcomp[index] = 0.0;
	}
      }
    }
  }
  // Step 2b: Do d^2/dy^2
  if(jstop==1) {
    // Only 1 layer in y-direction of f stored in r?comp arrays.
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      REALWIDE z = dz*k;
      for(i=0;i<istop;i++) {
	index = i+kindex;
	REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rxcomp[index] -= scale * 
	  ((Newell_f(x,0,z-dz)+Newell_f(x,0,z+dz))-2*Newell_f(x,0,z));
	rxcomp[index] *= 2;
	rycomp[index] -= scale * 
	  ((Newell_f(0,x,z-dz)+Newell_f(0,x,z+dz))-2*Newell_f(0,x,z));
	rycomp[index] *= 2;
	rzcomp[index] -= scale * 
	  ((Newell_f(z-dz,0,x)+Newell_f(z+dz,0,x))-2*Newell_f(z,0,x));
	rzcomp[index] *= 2;
	/// Use f is even in y
      }
    }
  } else {
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      for(j=0;j<rdimy;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] += rxcomp[index+2*rstridey];
	  rxcomp[index] -= 2*rxcomp[index+rstridey];
	  rycomp[index] += rycomp[index+2*rstridey];
	  rycomp[index] -= 2*rycomp[index+rstridey];
	  rzcomp[index] += rzcomp[index+2*rstridey];
	  rzcomp[index] -= 2*rzcomp[index+rstridey];
	}
      }
    }
    for(k=0;k<rdimz;k++) { // Zero-fill overhang
      UINT4m kindex = k*rstridez;
      for(j=rdimy;j<jstop;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] = 0.0;
	  rycomp[index] = 0.0;
	  rzcomp[index] = 0.0;
	}
      }
    }
  }
  // Step 2c: Do d^2/dx^2
  if(istop==1) {
    // Only 1 layer in x-direction of f stored in r?comp arrays.
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      REALWIDE z = dz*k;
      for(j=0;j<rdimy;j++) {
	index = kindex + j*rstridey;
	REALWIDE y = dy*j;
	rxcomp[index] -= scale * 
	  ((4*Newell_f(0,y,z)
	    +Newell_f(0,y+dy,z+dz)+Newell_f(0,y-dy,z+dz)
	    +Newell_f(0,y+dy,z-dz)+Newell_f(0,y-dy,z-dz))
	   -2*(Newell_f(0,y+dy,z)+Newell_f(0,y-dy,z)
	       +Newell_f(0,y,z+dz)+Newell_f(0,y,z-dz)));
	rxcomp[index] *= 2;
	rycomp[index] -= scale * 
	  ((4*Newell_f(y,0,z)
	    +Newell_f(y+dy,0,z+dz)+Newell_f(y-dy,0,z+dz)
	    +Newell_f(y+dy,0,z-dz)+Newell_f(y-dy,0,z-dz))
	   -2*(Newell_f(y+dy,0,z)+Newell_f(y-dy,0,z)
	       +Newell_f(y,0,z+dz)+Newell_f(y,0,z-dz)));
	rycomp[index] *= 2;
	rzcomp[index] -= scale * 
	  ((4*Newell_f(z,y,0)
	    +Newell_f(z+dz,y+dy,0)+Newell_f(z+dz,y-dy,0)
	    +Newell_f(z-dz,y+dy,0)+Newell_f(z-dz,y-dy,0))
	   -2*(Newell_f(z,y+dy,0)+Newell_f(z,y-dy,0)
	       +Newell_f(z+dz,y,0)+Newell_f(z-dz,y,0)));
	rzcomp[index] *= 2;
	/// Use f is even in x.
      }
    }
  } else {
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      for(j=0;j<rdimy;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<rdimx;i++) {
	  index = i+jkindex;
	  rxcomp[index] += rxcomp[index+2];
	  rxcomp[index] -= 2*rxcomp[index+1];
	  rycomp[index] += rycomp[index+2];
	  rycomp[index] -= 2*rycomp[index+1];
	  rzcomp[index] += rzcomp[index+2];
	  rzcomp[index] -= 2*rzcomp[index+1];
	}
      }
    }
    for(k=0;k<rdimz;k++) { // Zero-fill overhang
      UINT4m kindex = k*rstridez;
      for(j=0;j<rdimy;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=rdimx;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] = 0.0;
	  rycomp[index] = 0.0;
	  rzcomp[index] = 0.0;
	}
      }
    }
  }

  // Step 3: Use symmetries to reflect into other octants.
  for(k=0;k<rdimz;k++) {
    UINT4m kindex = k*rstridez;
    for(j=0;j<rdimy;j++) {
      UINT4m jkindex = kindex + j*rstridey;
      for(i=0;i<rdimx;i++) {
	index = i+jkindex;
	REALWIDE xtemp = rxcomp[index];
	REALWIDE ytemp = rycomp[index];
	REALWIDE ztemp = rzcomp[index];
	if(i>0) {
	  UINT4m tindex = (2*cdimx-i)+j*rstridey+k*rstridez;
	  rxcomp[tindex]=xtemp;
	  rycomp[tindex]=ytemp;
	  rzcomp[tindex]=ztemp;
	}
	if(j>0) {
	  UINT4m tindex = i+(cdimy-j)*rstridey+k*rstridez;
	  rxcomp[tindex]=xtemp;
	  rycomp[tindex]=ytemp;
	  rzcomp[tindex]=ztemp;
	}
	if(k>0) {
	  UINT4m tindex = i+j*rstridey+(cdimz-k)*rstridez;
	  rxcomp[tindex]=xtemp;
	  rycomp[tindex]=ytemp;
	  rzcomp[tindex]=ztemp;
	}
	if(i>0 && j>0) {
	  UINT4m tindex = (2*cdimx-i)+(cdimy-j)*rstridey+k*rstridez;
	  rxcomp[tindex]=xtemp;
	  rycomp[tindex]=ytemp;
	  rzcomp[tindex]=ztemp;
	}
	if(i>0 && k>0) {
	  UINT4m tindex = (2*cdimx-i)+j*rstridey+(cdimz-k)*rstridez;
	  rxcomp[tindex]=xtemp;
	  rycomp[tindex]=ytemp;
	  rzcomp[tindex]=ztemp;
	}
	if(j>0 && k>0) {
	  UINT4m tindex = i+(cdimy-j)*rstridey+(cdimz-k)*rstridez;
	  rxcomp[tindex]=xtemp;
	  rycomp[tindex]=ytemp;
	  rzcomp[tindex]=ztemp;
	}
	if(i>0 && j>0 && k>0) {
	  UINT4m tindex = (2*cdimx-i)+(cdimy-j)*rstridey+(cdimz-k)*rstridez;
	  rxcomp[tindex]=xtemp;
	  rycomp[tindex]=ytemp;
	  rzcomp[tindex]=ztemp;
	}
      }
    }
  }

  // Special "SelfDemag" code may be more accurate at index 0,0,0
  REALWIDE sizescale = -2.0 * static_cast<REALWIDE>(cdimx*cdimy*cdimz);
  /// Careful... cdimx, cdimy, cdimz are unsigned, so don't try
  ///  -2*cdimx*cdimy*cdimz (although -2.0*cdimx*cdimy*cdimz is OK).
  rxcomp[0] = SelfDemagNx(dx,dy,dz)/sizescale;
  rycomp[0] = SelfDemagNy(dx,dy,dz)/sizescale;
  rzcomp[0] = SelfDemagNz(dx,dy,dz)/sizescale;

  // Step 4: Transform into frequency domain.
  fft.ForwardRealDataNoScale(xcomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(ycomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(zcomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);

  // Copy results from ?comp into A??.  We only need store 1/8th
  // of the results because of symmetries.
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx-1;i++) {
    UINT4m aindex = i+j*astridey+k*astridez;
    index = i+j*cstridey+k*cstridez;
    A00[aindex] = xcomp[index].real();
    A11[aindex] = ycomp[index].real();
    A22[aindex] = zcomp[index].real();
  }

  // Handle special packing for i=adimx-1 plane
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) {
    Oxs_Complex temp00 = fft.RetrievePackedIndex(xcomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    Oxs_Complex temp11 = fft.RetrievePackedIndex(ycomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    Oxs_Complex temp22 = fft.RetrievePackedIndex(zcomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    UINT4m aindex = (adimx-1)+j*astridey+k*astridez;
    A00[aindex] = temp00.real();
    A11[aindex] = temp11.real();
    A22[aindex] = temp22.real();
  }

  // Repeat for Nxy, Nxz and Nyz. //////////////////////////////////////
  // Step 1: Evaluate g at each cell site.  Offset by (-dx,-dy,-dz)
  //  so we can do 2nd derivative operations "in-place".
  for(index=0;index<ctotalsize;index++) {
    // Reset temp space to zeros
    xcomp[index].Set(0.,0.);
    ycomp[index].Set(0.,0.);
    zcomp[index].Set(0.,0.);
  }
  for(k=0;k<kstop;k++) {
    UINT4m kindex = k*rstridez;
    REALWIDE z = 0.0;
    if(k==0) z = -dz;
    else if(k>1) z = dz*(k-1);
    for(j=0;j<jstop;j++) {
      UINT4m jkindex = kindex + j*rstridey;
      REALWIDE y = 0.0;
      if(j==0) y = -dy;
      else if(j>1) y = dy*(j-1);
      for(i=0;i<istop;i++) {
	index = i+jkindex;
	REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rxcomp[index] = scale*Newell_g(x,y,z);
	rycomp[index] = scale*Newell_g(x,z,y);
	rzcomp[index] = scale*Newell_g(y,z,x);
      }
    }
  }

  // Step 2a: Do d^2/dz^2
  if(kstop==1) {
    // Only 1 layer in z-direction of g stored in r?comp arrays.
    for(j=0;j<jstop;j++) {
      UINT4m jkindex = j*rstridey;
      REALWIDE y = 0.0;
      if(j==0) y = -dy;
      else if(j>1) y = dy*(j-1);
      for(i=0;i<istop;i++) {
	index = i+jkindex;
	REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rxcomp[index] -= scale*Newell_g(x,y,0);
	rxcomp[index] *= 2;
	// NOTE: g is even in z.
	// If zdim==1, then rycomp and rzcomp, i.e., Nxz and Nyz,
	// are identically zero.  Don't bother setting the array
	// values here, because they get set to 0 in Step 3 below.
      }
    }
  } else {
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      for(j=0;j<jstop;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] += rxcomp[index+2*rstridez];
	  rxcomp[index] -= 2*rxcomp[index+rstridez];
	  rycomp[index] += rycomp[index+2*rstridez];
	  rycomp[index] -= 2*rycomp[index+rstridez];
	  rzcomp[index] += rzcomp[index+2*rstridez];
	  rzcomp[index] -= 2*rzcomp[index+rstridez];
	}
      }
    }
    for(k=rdimz;k<kstop;k++) { // Zero-fill overhang
      UINT4m kindex = k*rstridez;
      for(j=0;j<jstop;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] = 0.0;
	  rycomp[index] = 0.0;
	  rzcomp[index] = 0.0;
	}
      }
    }
  }
  // Step 2b: Do d^2/dy^2
  if(jstop==1) {
    // Only 1 layer in y-direction of f stored in r?comp arrays.
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      REALWIDE z = dz*k;
      for(i=0;i<istop;i++) {
	index = i+kindex;
	REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rycomp[index] -= scale * 
	  ((Newell_g(x,z-dz,0)+Newell_g(x,z+dz,0))-2*Newell_g(x,z,0));
	rycomp[index] *= 2;
	// Note: g is even in its third argument.
	// If ydim==1, then rxcomp and rzcomp, i.e., Nxy and Nyz,
	// are identically zero.  Don't bother setting the array
	// values here, because they get set to 0 in Step 3 below.
      }
    }
  } else {
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      for(j=0;j<rdimy;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] += rxcomp[index+2*rstridey];
	  rxcomp[index] -= 2*rxcomp[index+rstridey];
	  rycomp[index] += rycomp[index+2*rstridey];
	  rycomp[index] -= 2*rycomp[index+rstridey];
	  rzcomp[index] += rzcomp[index+2*rstridey];
	  rzcomp[index] -= 2*rzcomp[index+rstridey];
	}
      }
    }
    for(k=0;k<rdimz;k++) { // Zero-fill overhang
      UINT4m kindex = k*rstridez;
      for(j=rdimy;j<jstop;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] = 0.0;
	  rycomp[index] = 0.0;
	  rzcomp[index] = 0.0;
	}
      }
    }
  }
  // Step 2c: Do d^2/dx^2
  if(istop==1) {
    // Only 1 layer in x-direction of f stored in r?comp arrays.
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      REALWIDE z = dz*k;
      for(j=0;j<rdimy;j++) {
	index = kindex + j*rstridey;
	REALWIDE y = dy*j;
	rzcomp[index] -= scale * 
	  ((4*Newell_g(y,z,0)
	    +Newell_g(y+dy,z+dz,0)+Newell_g(y-dy,z+dz,0)
	    +Newell_g(y+dy,z-dz,0)+Newell_g(y-dy,z-dz,0))
	   -2*(Newell_g(y+dy,z,0)+Newell_g(y-dy,z,0)
	       +Newell_g(y,z+dz,0)+Newell_g(y,z-dz,0)));
	rzcomp[index] *= 2;
	// Note: g is even in its third argument.
	// If xdim==1, then rxcomp and rycomp, i.e., Nxy and Nxz,
	// are identically zero.  Don't bother setting the array
	// values here, because they get set to 0 in Step 3 below.
      }
    }
  } else {
    for(k=0;k<rdimz;k++) {
      UINT4m kindex = k*rstridez;
      for(j=0;j<rdimy;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=0;i<rdimx;i++) {
	  index = i+jkindex;
	  rxcomp[index] += rxcomp[index+2];
	  rxcomp[index] -= 2*rxcomp[index+1];
	  rycomp[index] += rycomp[index+2];
	  rycomp[index] -= 2*rycomp[index+1];
	  rzcomp[index] += rzcomp[index+2];
	  rzcomp[index] -= 2*rzcomp[index+1];
	}
      }
    }
    for(k=0;k<rdimz;k++) { // Zero-fill overhang
      UINT4m kindex = k*rstridez;
      for(j=0;j<rdimy;j++) {
	UINT4m jkindex = kindex + j*rstridey;
	for(i=rdimx;i<istop;i++) {
	  index = i+jkindex;
	  rxcomp[index] = 0.0;
	  rycomp[index] = 0.0;
	  rzcomp[index] = 0.0;
	}
      }
    }
  }

  // Step 3: Use symmetries to reflect into other octants.
  //     Also, at each coordinate plane, set to 0.0 any term
  //     which is odd across that boundary.  It should already
  //     be close to 0, but will likely be slightly off due to
  //     rounding errors.
  for(k=0;k<rdimz;k++) {
    UINT4m kindex = k*rstridez;
    for(j=0;j<rdimy;j++) {
      UINT4m jkindex = kindex + j*rstridey;
      for(i=0;i<rdimx;i++) {
	index = i+jkindex;

	if(i==0 || j==0) rxcomp[index]=0.0;
	if(i==0 || k==0) rycomp[index]=0.0;
	if(j==0 || k==0) rzcomp[index]=0.0;

	REALWIDE xtemp = rxcomp[index];
	REALWIDE ytemp = rycomp[index];
	REALWIDE ztemp = rzcomp[index];
	if(i>0) {
	  UINT4m tindex = (2*cdimx-i)+j*rstridey+k*rstridez;
	  rxcomp[tindex] = -1*xtemp;
	  rycomp[tindex] = -1*ytemp;
	  rzcomp[tindex] =    ztemp;
	}
	if(j>0) {
	  UINT4m tindex = i+(cdimy-j)*rstridey+k*rstridez;
	  rxcomp[tindex] = -1*xtemp;
	  rycomp[tindex] =    ytemp;
	  rzcomp[tindex] = -1*ztemp;
	}
	if(k>0) {
	  UINT4m tindex = i+j*rstridey+(cdimz-k)*rstridez;
	  rxcomp[tindex] =    xtemp;
	  rycomp[tindex] = -1*ytemp;
	  rzcomp[tindex] = -1*ztemp;
	}
	if(i>0 && j>0) {
	  UINT4m tindex = (2*cdimx-i)+(cdimy-j)*rstridey+k*rstridez;
	  rxcomp[tindex] =    xtemp;
	  rycomp[tindex] = -1*ytemp;
	  rzcomp[tindex] = -1*ztemp;
	}
	if(i>0 && k>0) {
	  UINT4m tindex = (2*cdimx-i)+j*rstridey+(cdimz-k)*rstridez;
	  rxcomp[tindex] = -1*xtemp;
	  rycomp[tindex] =    ytemp;
	  rzcomp[tindex] = -1*ztemp;
	}
	if(j>0 && k>0) {
	  UINT4m tindex = i+(cdimy-j)*rstridey+(cdimz-k)*rstridez;
	  rxcomp[tindex] = -1*xtemp;
	  rycomp[tindex] = -1*ytemp;
	  rzcomp[tindex] =    ztemp;
	}
	if(i>0 && j>0 && k>0) {
	  UINT4m tindex = (2*cdimx-i)+(cdimy-j)*rstridey+(cdimz-k)*rstridez;
	  rxcomp[tindex] = xtemp;
	  rycomp[tindex] = ytemp;
	  rzcomp[tindex] = ztemp;
	}
      }
    }
  }

  // Step 4: Transform into frequency domain.
  fft.ForwardRealDataNoScale(xcomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(ycomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(zcomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);

  // Copy results from ?comp into A??.  We only need store 1/8th
  // of the results because of symmetries.
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx-1;i++) {
    UINT4m aindex = i+j*astridey+k*astridez;
    index = i+j*cstridey+k*cstridez;
    A01[aindex] = xcomp[index].real();
    A02[aindex] = ycomp[index].real();
    A12[aindex] = zcomp[index].real();
  }
  // Handle special packing for i=adimx-1 plane
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) {
    Oxs_Complex temp01 = fft.RetrievePackedIndex(xcomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    Oxs_Complex temp02 = fft.RetrievePackedIndex(ycomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    Oxs_Complex temp12 = fft.RetrievePackedIndex(zcomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    UINT4m aindex = (adimx-1)+j*astridey+k*astridez;
    A01[aindex] = temp01.real();
    A02[aindex] = temp02.real();
    A12[aindex] = temp12.real();
  }
}

void Klm_Demag_PBC::FillCoefficientArraysStandard(const Oxs_Mesh* genmesh) const
{ // This routine is conceptually const.
  // This "OLD" variant is simpler but slower, and is retained
  // solely for testing and development purposes.

  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(genmesh);
  if(mesh==NULL) {
    String msg = String("Object ")
      + String(genmesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_Ext::Error(this,msg);
  }

  // Clean-up from previous allocation, if any.
  ReleaseMemory();

  // Fill dimension variables
  rdimx = mesh->DimX();
  rdimy = mesh->DimY();
  rdimz = mesh->DimZ();
  if(rdimx==0 || rdimy==0 || rdimz==0) return; // Empty mesh!

  cdimx = NextPowerOfTwo(2*rdimx)/2;
  if(rdimy>1) cdimy = NextPowerOfTwo(2*rdimy);
  else        cdimy = 1;
  if(rdimz>1) cdimz = NextPowerOfTwo(2*rdimz);
  else        cdimz = 1;
  cstridey = cdimx + 1;  // Pad by one to avoid cache line entanglement
  cstridez = cstridey*cdimy;

  UINT4m ctotalsize=cstridez*cdimz;
  UINT4m rtotalsize=2*ctotalsize;
  if(rtotalsize<2*cdimx || rtotalsize<cdimy || rtotalsize<cdimz) {
    // Partial overflow check
    String msg = String("UINT4m overflow in ") + String(InstanceName())
      + String(": Product cdimx*cdimy*cdimz too big"
               " to fit in a UINT4m variable");
    throw Oxs_Ext::Error(this,msg);
  }

  // Allocate memory for interaction matrices and magnetization components
  adimx=1+cdimx;
  adimy=1+cdimy/2;
  adimz=1+cdimz/2;
  astridey=adimx;
  astridez=adimy*astridey;
  UINT4m atotalsize=adimz*astridez;
  A00   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A01   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A02   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A11   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A12   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  A22   = new OXS_COMPLEX_REAL_TYPE[atotalsize];
  xcomp = new Oxs_Complex[ctotalsize];
  ycomp = new Oxs_Complex[ctotalsize];
  zcomp = new Oxs_Complex[ctotalsize];
  if(xcomp==NULL || ycomp==NULL || zcomp==NULL
     || A00==NULL || A01==NULL || A02==NULL
     || A11==NULL || A12==NULL || A22==NULL) {
    // Safety check for those machines on which new[] doesn't throw
    // BadAlloc.
    String msg = String("Insufficient memory in Demag constructor.");
    throw Oxs_Ext::Error(this,msg);
  }

  REALWIDE *rxcomp = static_cast<REALWIDE*>(static_cast<void*>(xcomp));
  REALWIDE *rycomp = static_cast<REALWIDE*>(static_cast<void*>(ycomp));
  REALWIDE *rzcomp = static_cast<REALWIDE*>(static_cast<void*>(zcomp));
  UINT4m rstridey=2*cstridey;
  UINT4m rstridez=2*cstridez;

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
  // We use these symmetries to reduce storage requirements.  If
  // f is real and even, then f^ is also real and even.  If f
  // is real and odd, then f^ is (pure) imaginary and odd.
  // As a result, the transform of each of the Axx interaction
  // matrices will be real, with the same even/odd properties.

  UINT4m index,i,j,k;

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
  // WARNING for developement: here is an important parameter 
  // distinguishing the Demag from the SimpleDemag code
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
                      _Demag,
                      compute_stats
                    ); 
  demag_pbc.Init();                    
  
  // Also throw in FFT scaling.  This allows the "NoScale" FFT routines
  // to be used.
  scale *= 1.0/(2*cdimx*cdimy*cdimz);

  for(index=0;index<ctotalsize;index++) {
    // Initialize temp space to 0
    xcomp[index].Set(0.,0.);
    ycomp[index].Set(0.,0.);
    zcomp[index].Set(0.,0.);
  }


#ifdef DUMP_COEF_TEST
fprintf(stderr,"Nxy(1,2,3,1,2,3)=%.17g   Nxy(10,1,1,1,2,3)=%.17g\n",
	(double)CalculateSDA01(1.,2.,3.,1.,2.,3.)/(4*PI*1.*2.*3.),
	(double)CalculateSDA01(10.,1.,1.,1.,2.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(-1,2,3,1,2,3)=%.17g   Nxy(10,1,-1,1,2,3)=%.17g\n",
	(double)CalculateSDA01(-1.,2.,3.,1.,2.,3.)/(4*PI*1.*2.*3.),
	(double)CalculateSDA01(10.,1.,-1.,1.,2.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(1,1,0,1,2,3)=%.17g   Nxy(1,1,0,2,1,3)=%.17g\n",
	(double)CalculateSDA01(1.,1.,0.,1.,2.,3.)/(4*PI*1.*2.*3.),
	(double)CalculateSDA01(1.,1.,0.,2.,1.,3.)/(4*PI*1.*2.*3.));
fprintf(stderr,"Nxy(-1,1,0,1,2,3)=%.17g   Nxy(1,-1,0,2,1,3)=%.17g\n",
	(double)CalculateSDA01(-1.,1.,0.,1.,2.,3.)/(4*PI*1.*2.*3.),
	(double)CalculateSDA01(1.,-1.,0.,2.,1.,3.)/(4*PI*1.*2.*3.));
#endif // DUMP_COEF_TEST

  for(k=0;k<rdimz;k++) for(j=0;j<rdimy;j++) for(i=0;i<rdimx;i++) {
    
    // KL(m)
    //REALWIDE x = dx*i;
    //REALWIDE y = dy*j;
    //REALWIDE z = dz*k;
    //REALWIDE a00=scale*CalculateSDA00(x,y,z,dx,dy,dz);
    //REALWIDE a01=scale*CalculateSDA01(x,y,z,dx,dy,dz);
    //REALWIDE a02=scale*CalculateSDA02(x,y,z,dx,dy,dz);
    REALWIDE a00= scale*demag_pbc.CalculateSDA(n00, i, j, k); 
    REALWIDE a01= scale*demag_pbc.CalculateSDA(n01, i, j, k); 
    REALWIDE a02= scale*demag_pbc.CalculateSDA(n02, i, j, k); 

    index = i+j*rstridey+k*rstridez;

    rxcomp[index]=a00;
    rycomp[index]=a01;
    rzcomp[index]=a02;

    if(i>0) {
      UINT4m tindex = (2*cdimx-i)+j*rstridey+k*rstridez;
      rxcomp[tindex]=a00;
      rycomp[tindex]=-a01;
      rzcomp[tindex]=-a02;
    }
    if(j>0) {
      UINT4m tindex = i+(cdimy-j)*rstridey+k*rstridez;
      rxcomp[tindex]=a00;
      rycomp[tindex]=-a01;
      rzcomp[tindex]=a02;
    }
    if(k>0) {
      UINT4m tindex = i+j*rstridey+(cdimz-k)*rstridez;
      rxcomp[tindex]=a00;
      rycomp[tindex]=a01;
      rzcomp[tindex]=-a02;
    }
    if(i>0 && j>0) {
      UINT4m tindex = (2*cdimx-i)+(cdimy-j)*rstridey+k*rstridez;
      rxcomp[tindex]=a00;
      rycomp[tindex]=a01;
      rzcomp[tindex]=-a02;
    }
    if(i>0 && k>0) {
      UINT4m tindex = (2*cdimx-i)+j*rstridey+(cdimz-k)*rstridez;
      rxcomp[tindex]=a00;
      rycomp[tindex]=-a01;
      rzcomp[tindex]=a02;
    }
    if(j>0 && k>0) {
      UINT4m tindex = i+(cdimy-j)*rstridey+(cdimz-k)*rstridez;
      rxcomp[tindex]=a00;
      rycomp[tindex]=-a01;
      rzcomp[tindex]=-a02;
    }
    if(i>0 && j>0 && k>0) {
      UINT4m tindex = (2*cdimx-i)+(cdimy-j)*rstridey+(cdimz-k)*rstridez;
      rxcomp[tindex]=a00;
      rycomp[tindex]=a01;
      rzcomp[tindex]=a02;
    }
  }

  // Transform into frequency domain.
  fft.ForwardRealDataNoScale(xcomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(ycomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(zcomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);

  // Copy results from ?comp into A??.  We only need store 1/8th
  // of the results because of symmetries.
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx-1;i++) {
    UINT4m aindex = i+j*astridey+k*astridez;
    index = i+j*cstridey+k*cstridez;
    A00[aindex] = xcomp[index].real();
    A01[aindex] = ycomp[index].real();
    A02[aindex] = zcomp[index].real();
  }
  // Handle special packing for i=adimx-1 plane
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) {
    Oxs_Complex temp00 = fft.RetrievePackedIndex(xcomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    Oxs_Complex temp01 = fft.RetrievePackedIndex(ycomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    Oxs_Complex temp02 = fft.RetrievePackedIndex(zcomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    UINT4m aindex = (adimx-1)+j*astridey+k*astridez;
    A00[aindex] = temp00.real();
    A01[aindex] = temp01.real();
    A02[aindex] = temp02.real();
  }
  
  // KL(m)
  demag_pbc.middle_load_or_save_tensor_file();

  // Repeat for A11, A12 and A22. //////////////////////////////////////
  for(index=0;index<ctotalsize;index++) {
    xcomp[index].Set(0.,0.);
    ycomp[index].Set(0.,0.);
    zcomp[index].Set(0.,0.);
  }

  for(k=0;k<rdimz;k++) for(j=0;j<rdimy;j++) for(i=0;i<rdimx;i++) {
    
    // KL(m)
    //REALWIDE x = dx*i;    REALWIDE y = dy*j;    REALWIDE z = dz*k;
    //REALWIDE a11=scale*CalculateSDA11(x,y,z,dx,dy,dz);
    //REALWIDE a12=scale*CalculateSDA12(x,y,z,dx,dy,dz);
    //REALWIDE a22=scale*CalculateSDA22(x,y,z,dx,dy,dz);
    REALWIDE a11= scale*demag_pbc.CalculateSDA(n11, i, j, k); 
    REALWIDE a12= scale*demag_pbc.CalculateSDA(n12, i, j, k); 
    REALWIDE a22= scale*demag_pbc.CalculateSDA(n22, i, j, k); 
    
    index = i+j*rstridey+k*rstridez;
    rxcomp[index]=a11;
    rycomp[index]=a12;
    rzcomp[index]=a22;
    if(i>0) {
      UINT4m tindex = (2*cdimx-i)+j*rstridey+k*rstridez;
      rxcomp[tindex]=a11;
      rycomp[tindex]=a12;
      rzcomp[tindex]=a22;
    }
    if(j>0) {
      UINT4m tindex = i+(cdimy-j)*rstridey+k*rstridez;
      rxcomp[tindex]=a11;
      rycomp[tindex]=-a12;
      rzcomp[tindex]=a22;
    }
    if(k>0) {
      UINT4m tindex = i+j*rstridey+(cdimz-k)*rstridez;
      rxcomp[tindex]=a11;
      rycomp[tindex]=-a12;
      rzcomp[tindex]=a22;
    }
    if(i>0 && j>0) {
      UINT4m tindex = (2*cdimx-i)+(cdimy-j)*rstridey+k*rstridez;
      rxcomp[tindex]=a11;
      rycomp[tindex]=-a12;
      rzcomp[tindex]=a22;
    }
    if(i>0 && k>0) {
      UINT4m tindex = (2*cdimx-i)+j*rstridey+(cdimz-k)*rstridez;
      rxcomp[tindex]=a11;
      rycomp[tindex]=-a12;
      rzcomp[tindex]=a22;
    }
    if(j>0 && k>0) {
      UINT4m tindex = i+(cdimy-j)*rstridey+(cdimz-k)*rstridez;
      rxcomp[tindex]=a11;
      rycomp[tindex]=a12;
      rzcomp[tindex]=a22;
    }
    if(i>0 && j>0 && k>0) {
      UINT4m tindex = (2*cdimx-i)+(cdimy-j)*rstridey+(cdimz-k)*rstridez;
      rxcomp[tindex]=a11;
      rycomp[tindex]=a12;
      rzcomp[tindex]=a22;
    }
  }

  fft.ForwardRealDataNoScale(xcomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(ycomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(zcomp,2*cdimx,cdimy,cdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);

  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx-1;i++) {
    UINT4m aindex = i+j*astridey+k*astridez;
    index = i+j*cstridey+k*cstridez;
    A11[aindex] = xcomp[index].real();
    A12[aindex] = ycomp[index].real();
    A22[aindex] = zcomp[index].real();
  }
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) { // Special packing case
    Oxs_Complex temp11 = fft.RetrievePackedIndex(xcomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    Oxs_Complex temp12 = fft.RetrievePackedIndex(ycomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    Oxs_Complex temp22 = fft.RetrievePackedIndex(zcomp,
		   cdimx,cdimy,cdimz,cstridey,cstridez,
		   adimx-1,j,k);
    UINT4m aindex = (adimx-1)+j*astridey+k*astridez;
    A11[aindex] = temp11.real();
    A12[aindex] = temp12.real();
    A22[aindex] = temp22.real();
  }
  
  // KL(m)
  // We do not know the tables containing real (not F transformed) coefficients
  demag_pbc.Finalize();

#ifdef FUBAR
/**/
  printf("\n Index     A00        A11        A22\n");
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx;i++) {
    UINT4m aindex = i+j*astridey+k*astridez;
    printf("%d %d %d  %#10.4g  %#10.4g  %#10.4g\n",
	   i,j,k,A00[aindex],A11[aindex],A22[aindex]);
  }
  printf("\n Index     A01        A02        A12\n");
  for(k=0;k<adimz;k++) for(j=0;j<adimy;j++) for(i=0;i<adimx;i++) {
    UINT4m aindex = i+j*astridey+k*astridez;
    printf("%d %d %d  %#10.4g  %#10.4g  %#10.4g\n",
	   i,j,k,A01[aindex],A02[aindex],A12[aindex]);
  }
/**/
#endif // FUBAR
}

void Klm_Demag_PBC::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  // (Re)-initialize mesh coefficient array if mesh has changed.
  if(mesh_id != state.mesh->Id()) {
    mesh_id = 0; // Safety
    (this->*fillcoefs)(state.mesh);
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
  UINT4m i,j,k;
  UINT4m mstridey=rdimx;  // Assume import mesh is tight packed
  UINT4m mstridez=rdimy*mstridey;
  UINT4m rstridey=2*cstridey;
  UINT4m rstridez=2*cstridez;
  OXS_COMPLEX_REAL_TYPE* rxcomp
    = static_cast<OXS_COMPLEX_REAL_TYPE*>(static_cast<void*>(xcomp));
  OXS_COMPLEX_REAL_TYPE* rycomp
    = static_cast<OXS_COMPLEX_REAL_TYPE*>(static_cast<void*>(ycomp));
  OXS_COMPLEX_REAL_TYPE* rzcomp
    = static_cast<OXS_COMPLEX_REAL_TYPE*>(static_cast<void*>(zcomp));

  for(k=0;k<rdimz;k++) for(j=0;j<rdimy;j++) {
    UINT4m mindex = j*mstridey+k*mstridez;
    UINT4m rindex = j*rstridey+k*rstridez;
    for(i=0;i<rdimx;i++) {
      REAL8m scale = Ms[mindex];
      const ThreeVector& vec = spin[mindex];
      ++mindex;
      rxcomp[rindex]=scale*vec.x;
      rycomp[rindex]=scale*vec.y;
      rzcomp[rindex]=scale*vec.z;
      ++rindex;
    }
  }

#if REPORT_TIME
  ffttime.Start();
#endif // REPORT_TIME
  fft.ForwardRealDataNoScale(xcomp,rdimx,rdimy,rdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(ycomp,rdimx,rdimy,rdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.ForwardRealDataNoScale(zcomp,rdimx,rdimy,rdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
#if REPORT_TIME
  ffttime.Stop();
#endif // REPORT_TIME

  // Calculate field components in frequency domain, then do iFFT to
  // transform back to space domain.  Make use of realness and even/odd
  // properties of interaction matrices Axx.
#if REPORT_TIME
  convtime.Start();
#endif // REPORT_TIME
  UINT4m block;
  for(block=0;block<4;block++) {
    UINT4m base_offset = 0;
    if(block%2==1) {
      if(cdimy==1) continue; // There is no second "j" block
      base_offset += cstridey*(cdimy/2);
    }
    if(block/2==1) {
      if(cdimz==1) continue; // There is no second "k" block
      base_offset += cstridez*(cdimz/2);
    }
    Oxs_Complex* xbase=xcomp+base_offset;
    Oxs_Complex* ybase=ycomp+base_offset;
    Oxs_Complex* zbase=zcomp+base_offset;
    int jsign=1; if(block%2==1) jsign=-1;
    int ksign=1; if(block/2==1) ksign=-1;
    Oxs_Complex Mx,My,Mz;
    OXS_COMPLEX_REAL_TYPE a00,a11,a22,a01,a02,a12;
    UINT4m aindex=0,cindex;
    k=0;
    do {
      // j==0, i==0
      cindex = k*cstridez;
      Mx = xbase[cindex];
      My = ybase[cindex];
      Mz = zbase[cindex];
      if(k==0) {
	aindex = 0;
	if(jsign != 1) aindex += (adimy-1)*astridey;
	if(ksign != 1) aindex += (adimz-1)*astridez;
	OXS_COMPLEX_REAL_TYPE a00b,a11b,a22b,a01b,a02b,a12b;
	a00  = A00[aindex];
	a00b = A00[aindex+cdimx];
	a11  = A11[aindex];
	a11b = A11[aindex+cdimx];
	a22  = A22[aindex];
	a22b = A22[aindex+cdimx];
	a01  = A01[aindex];
	a01b = A01[aindex+cdimx];
	a02  = A02[aindex];
	a02b = A02[aindex+cdimx];
	a12  = A12[aindex];
	a12b = A12[aindex+cdimx];
	xbase[cindex].re = a00 *Mx.re + a01 *My.re + a02 *Mz.re;  // Hx
	xbase[cindex].im = a00b*Mx.im + a01b*My.im + a02b*Mz.im;
	ybase[cindex].re = a01 *Mx.re + a11 *My.re + a12 *Mz.re;  // Hy
	ybase[cindex].im = a01b*Mx.im + a11b*My.im + a12b*Mz.im;
	zbase[cindex].re = a02 *Mx.re + a12 *My.re + a22 *Mz.re;  // Hz
	zbase[cindex].im = a02b*Mx.im + a12b*My.im + a22b*Mz.im;
      } else {
	// k>0
	if(ksign == 1) {
	  aindex = k*astridez;
	} else {
	  aindex = (adimz-1-k)*astridez+cdimx;
	}
	if(jsign != 1) aindex += (adimy-1)*astridey;
	a00 = A00[aindex];
	a11 = A11[aindex];
	a22 = A22[aindex];
	a01 = A01[aindex];
	a02 = ksign*A02[aindex];
	a12 = ksign*A12[aindex];
	xbase[cindex] = a00*Mx + a01*My + a02*Mz;  // Hx
	ybase[cindex] = a01*Mx + a11*My + a12*Mz;  // Hy
	zbase[cindex] = a02*Mx + a12*My + a22*Mz;  // Hz
      }
      // j==0, i>0
      if(ksign == 1) aindex = k*astridez;
      else           aindex = (adimz-1-k)*astridez;
      if(jsign != 1) aindex += (adimy-1)*astridey;
      for(i=1;i<cdimx;i++) {
	++cindex;
	Mx = xbase[cindex];
	My = ybase[cindex];
	Mz = zbase[cindex];
	++aindex;
	a00 = A00[aindex];
	a11 = A11[aindex];
	a22 = A22[aindex];
	a01 = A01[aindex];
	a02 = ksign*A02[aindex];
	a12 = ksign*A12[aindex];
	xbase[cindex] = a00*Mx + a01*My + a02*Mz;  // Hx
	ybase[cindex] = a01*Mx + a11*My + a12*Mz;  // Hy
	zbase[cindex] = a02*Mx + a12*My + a22*Mz;  // Hz
      }

      for(j=1;j<cdimy/2;j++) {
	// i==0
	cindex = j*cstridey+k*cstridez;
	Mx = xbase[cindex];
	My = ybase[cindex];
	Mz = zbase[cindex];
	if(ksign == 1) aindex = k*astridez;
	else           aindex = (adimz-1-k)*astridez;
	if(jsign == 1) aindex += j*astridey;
	else           aindex += (adimy-1-j)*astridey + cdimx;
	a00 = A00[aindex];
	a11 = A11[aindex];
	a22 = A22[aindex];
	a01 = jsign*A01[aindex];
	a02 = ksign*A02[aindex];
	a12 = jsign*ksign*A12[aindex];
	xbase[cindex] = a00*Mx + a01*My + a02*Mz;  // Hx
	ybase[cindex] = a01*Mx + a11*My + a12*Mz;  // Hy
	zbase[cindex] = a02*Mx + a12*My + a22*Mz;  // Hz
	if(jsign != 1) aindex -= cdimx;
	for(i=1;i<cdimx;i++) {
	  ++cindex;
	  Mx = xbase[cindex];
	  My = ybase[cindex];
	  Mz = zbase[cindex];
	  ++aindex;
	  a00 = A00[aindex];
	  a11 = A11[aindex];
	  a22 = A22[aindex];
	  a01 = jsign*A01[aindex];
	  a02 = ksign*A02[aindex];
	  a12 = jsign*ksign*A12[aindex];
	  xbase[cindex] = a00*Mx + a01*My + a02*Mz;  // Hx
	  ybase[cindex] = a01*Mx + a11*My + a12*Mz;  // Hy
	  zbase[cindex] = a02*Mx + a12*My + a22*Mz;  // Hz
	}
      }
    } while(++k<cdimz/2);  // For each block, do one pass through
    /// the above loop if cdimz is 1 or 2.  If cdimz is 1 then
    /// ksign will always be 1, because the upper half "blocks"
    /// are skipped.
  }
#if REPORT_TIME
  convtime.Stop();
#endif // REPORT_TIME

#if REPORT_TIME
  ffttime.Start();
#endif // REPORT_TIME
  fft.InverseRealDataNoScale(xcomp,rdimx,rdimy,rdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.InverseRealDataNoScale(ycomp,rdimx,rdimy,rdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
  fft.InverseRealDataNoScale(zcomp,rdimx,rdimy,rdimz,
			     cdimx,cdimy,cdimz,cstridey,cstridez);
#if REPORT_TIME
  ffttime.Stop();
#endif // REPORT_TIME

#if REPORT_TIME
  dottime.Start();
#endif // REPORT_TIME
  OXS_COMPLEX_REAL_TYPE mult =  -0.5 * MU0;
  for(k=0;k<rdimz;k++) for(j=0;j<rdimy;j++) {
    UINT4m mindex = j*mstridey+k*mstridez;
    UINT4m rindex = j*rstridey+k*rstridez;
    for(i=0;i<rdimx;i++) {
      field[mindex].x=rxcomp[rindex];
      field[mindex].y=rycomp[rindex];
      field[mindex].z=rzcomp[rindex];
      // Calculate also pointwise energy density: -0.5*MU0*<M,H>
      OXS_COMPLEX_REAL_TYPE dot = spin[mindex]*field[mindex];
      energy[mindex] = mult * Ms[mindex] * dot;
      ++mindex; ++rindex;
    }
  }
#if REPORT_TIME
  dottime.Stop();
#endif // REPORT_TIME
}
