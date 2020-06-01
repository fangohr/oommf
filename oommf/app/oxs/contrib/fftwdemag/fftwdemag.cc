/* FILE: fftwdemag.cc            -*-Mode: c++-*-
 *
 * Average H demag field across rectangular cells.  The formulae used
 * are reduced forms of equations in A. J. Newell, W. Williams,
 * D. J. Dunlop, "A Generalization of the Demagnetizing Tensor for
 * Nonuniform Magnetization," Journal of Geophysical Research - Solid
 * Earth 98, 9551-9555 (1993).
 *
 * This implementation uses the 3rd party FFTW library to perform the
 * forward/inverse FFT's.
 *
 */

#include <limits.h>
#include <string>

#include "exterror.h"
#include "rectangularmesh.h"
#include "demagcoef.h"

#include "fftwdemag.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(FFTW_Demag);

/* End includes */

// Constructor
FFTW_Demag::FFTW_Demag(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    xdim(0),ydim(0),zdim(0),
    lxdim(0),lydim(0),lzdim(0),
    vxdim(0),vydim(0),vzdim(0),
    rvxdim(0),rvydim(0),rvzdim(0),
    axdim(0),aydim(0),azdim(0),
    vystride(0),vzstride(0),vtotalsize(0),
    rvystride(0),rvzstride(0),rvtotalsize(0),
    aystride(0),azstride(0),atotalsize(0),
    mesh_id(0),
    A00(0),A01(0),A02(0),A11(0),A12(0),A22(0),
    V1(0),V2(0),V3(0),rV1(0),rV2(0),rV3(0),
    forward_plan(NULL),backward_plan(NULL),
    initmem(0)
{
  String kernel_fill_routine
    = GetStringInitValue("kernel_fill_routine","fast");
  if(kernel_fill_routine.compare("standard")==0) {
    initmem = &FFTW_Demag::StandardInitializeMemory;
  } else if(kernel_fill_routine.compare("fast")==0) {
    initmem = &FFTW_Demag::FastInitializeMemory;
  } else {
    String msg = String("Invalid kernel_fill_routine request: ");
    msg += kernel_fill_routine;
    throw Oxs_ExtError(this,msg);
  }
  VerifyAllInitArgsUsed();
}

OC_BOOL FFTW_Demag::Init()
{
  mesh_id = 0;
  ReleaseMemory();
  return Oxs_Energy::Init();
}

void FFTW_Demag::ReleaseMemory() const
{ // Conceptually const
  if(forward_plan!=NULL)  fftw_destroy_plan(forward_plan);
  if(backward_plan!=NULL) fftw_destroy_plan(backward_plan);
  forward_plan=backward_plan=NULL;

  if(V1!=0) fftw_free(V1); // V1/2/3 is allocated as one big array
  V1=V2=V3=0;
  rV1=rV2=rV3=0;

  if(A00!=NULL) delete[] A00;
  if(A11!=NULL) delete[] A11;
  if(A22!=NULL) delete[] A22;
  if(A01!=NULL) delete[] A01;
  if(A02!=NULL) delete[] A02;
  if(A12!=NULL) delete[] A12;
  A00=A11=A22=A01=A02=A12=0;

  xdim=ydim=zdim=0;
  lxdim=lydim=lzdim=0;
  vxdim=vydim=vzdim=vystride=vzstride=vtotalsize=0;
  rvxdim=rvydim=rvzdim=rvystride=rvzstride=rvtotalsize=0;
  axdim=aydim=azdim=aystride=azstride=atotalsize=0;
}

void FFTW_Demag::StandardInitializeMemory(const Oxs_Mesh* genmesh) const
{ // This routine is conceptually const.
  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(genmesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(genmesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(msg.c_str());
  }

  // Clean-up from previous allocation, if any.
  ReleaseMemory();

  // Dimensional variables:
  //  xdim,  ydim,  zdim : Mesh (problem) dimensions.
  // lxdim, lydim, lzdim : Logical dimensions for FFT.
  // vxdim, vydim, vzdim : Physical dimensions For V1/2/3 arrays,
  //                       with complex-type elements.
  // axdim, aydim, azdim : Physical dimensions for A?? arrays.
  // When treated as real arrays, rV1/2/3 have dimensions
  // 2*vxdim, vydim, vzdim.

  // Space domain dimensions
  xdim = mesh->DimX();
  ydim = mesh->DimY();
  zdim = mesh->DimZ();
  if(xdim==0 || ydim==0 || zdim==0) return; // Empty mesh!

  // Logical frequency space dimensions (complex data)
  lxdim = 2*xdim;
  if(ydim>1) lydim = 2*ydim;
  else       lydim = 1;
  if(zdim>1) lzdim = 2*zdim;
  else       lzdim = 1;

  // Physical array dimensions, for complex data
  vxdim = (lxdim/2) + 1;
  vydim = lydim;
  vzdim = lzdim;
  vystride = vxdim;
  vzstride = vystride * vydim;
  vtotalsize = vzstride * vzdim;
  if(vtotalsize < vxdim || vtotalsize < vydim || vtotalsize < vzdim) {
    // Partial overflow check
    String msg = String("OC_INDEX overflow in ") + String(InstanceName())
      + String(": Product vxdim*vydim*vzdim too big"
               " to fit in a OC_INDEX variable");
    throw Oxs_ExtError(msg.c_str());
  }
  V1  = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex)
					       *vtotalsize*3));
  V2  = V1 + vtotalsize;
  V3  = V2 + vtotalsize;

  rV1 = static_cast<double*>(static_cast<void*>(V1));
  rV2 = static_cast<double*>(static_cast<void*>(V2));
  rV3 = static_cast<double*>(static_cast<void*>(V3));
  rvxdim = 2*vxdim;  rvydim = vydim;  rvzdim = vzdim;
  rvystride = 2*vystride;
  rvzstride = 2*vzstride;
  rvtotalsize = 2*vtotalsize;

  // Allocate memory for interaction matrices.  These are real-valued,
  // and because of symmetries we only need to store the first octant
  axdim = OC_MAX(1,lxdim/2+1);
  aydim = OC_MAX(1,lydim/2+1);
  azdim = OC_MAX(1,lzdim/2+1);
  aystride = axdim;
  azstride = aystride * aydim;
  atotalsize = azstride * azdim;
  A00 = new double[atotalsize];
  A11 = new double[atotalsize];
  A22 = new double[atotalsize];
  A01 = new double[atotalsize];
  A02 = new double[atotalsize];
  A12 = new double[atotalsize];
  if(V1==NULL
     || A00==0 || A11==0 || A22==0
     || A01==0 || A02==0 || A12==0) {
    throw Oxs_ExtError("Insufficient memory in FFTW_Demag constructor.");
  }

  // Setup plans on V1/V2/V3.  We specify the dimensions in reverse
  // order, to account for the difference between column-major order
  // (Oxs_MeshValue arrays) and row-major order (FFTW arrays).
#if OC_INDEX_WIDTH > OC_INT_WIDTH
  if(lxdim>INT_MAX || lydim>INT_MAX || lzdim>INT_MAX) {
    throw Oxs_ExtError("Integer overflow in FFTW_Demag constructor.");
  }
#endif
  int pn[3] = { static_cast<int>(lzdim), static_cast<int>(lydim),
                static_cast<int>(lxdim)};
  forward_plan  = fftw_plan_many_dft_r2c(3,pn,3,
					 rV1,NULL,1,rvtotalsize,
					 V1,NULL,1,vtotalsize,
					 FFTW_MEASURE);
  backward_plan = fftw_plan_many_dft_c2r(3,pn,3,
					 V1,NULL,1,vtotalsize,
					 rV1,NULL,1,rvtotalsize,
					 FFTW_MEASURE);
  if(forward_plan==NULL || backward_plan==NULL) {
    throw Oxs_ExtError("FFTW plan initialization failed");
  }


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

  // Fill V1/2/3 with A00/11/22 non-zero space-domain values
  OC_INDEX index;
  OC_INDEX i,j,k;
  OC_REALWIDE dx = mesh->EdgeLengthX();
  OC_REALWIDE dy = mesh->EdgeLengthY();
  OC_REALWIDE dz = mesh->EdgeLengthZ();
  // For demag calculation, all that matters is the relative
  // sizes of dx, dy and dz.  To help insure we don't run
  // outside floating point range, rescale these values so
  // largest is 1.0
  OC_REALWIDE maxedge=dx;
  if(dy>maxedge) maxedge=dy;
  if(dz>maxedge) maxedge=dz;
  dx/=maxedge; dy/=maxedge; dz/=maxedge;

  // The FFTW routines don't include the DFT transform scaling,
  // so throw the scaling into the demag coefs.
  OC_REALWIDE scale *= -1.0/static_cast<OC_REALWIDE>(lxdim*lydim*lzdim);

  for(index=0;index<3*rvtotalsize;index++) rV1[index] = 0.0;
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef=scale*Oxs_CalculateNxx(x,y,z,dx,dy,dz);
    rV1[i+j*rvystride+k*rvzstride] = coef;
    if(i>0) rV1[(lxdim-i)+j*rvystride+k*rvzstride] = coef;
    if(j>0) rV1[i+(lydim-j)*rvystride+k*rvzstride] = coef;
    if(k>0) rV1[i+j*rvystride+(lzdim-k)*rvzstride] = coef;
    if(i>0 && j>0)
      rV1[(lxdim-i)+(lydim-j)*rvystride+k*rvzstride] = coef;
    if(i>0 && k>0)
      rV1[(lxdim-i)+j*rvystride+(lzdim-k)*rvzstride] = coef;
    if(j>0 && k>0)
      rV1[i+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
    if(i>0 && j>0 && k>0)
      rV1[(lxdim-i)+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef=scale*Oxs_CalculateNyy(x,y,z,dx,dy,dz);
    rV2[i+j*rvystride+k*rvzstride] = coef;
    if(i>0) rV2[(lxdim-i)+j*rvystride+k*rvzstride] = coef;
    if(j>0) rV2[i+(lydim-j)*rvystride+k*rvzstride] = coef;
    if(k>0) rV2[i+j*rvystride+(lzdim-k)*rvzstride] = coef;
    if(i>0 && j>0)
      rV2[(lxdim-i)+(lydim-j)*rvystride+k*rvzstride] = coef;
    if(i>0 && k>0)				    
      rV2[(lxdim-i)+j*rvystride+(lzdim-k)*rvzstride] = coef;
    if(j>0 && k>0)				    
      rV2[i+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
    if(i>0 && j>0 && k>0)
      rV2[(lxdim-i)+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef=scale*Oxs_CalculateNzz(x,y,z,dx,dy,dz);
    rV3[i+j*rvystride+k*rvzstride] = coef;
    if(i>0) rV3[(lxdim-i)+j*rvystride+k*rvzstride] = coef;
    if(j>0) rV3[i+(lydim-j)*rvystride+k*rvzstride] = coef;
    if(k>0) rV3[i+j*rvystride+(lzdim-k)*rvzstride] = coef;
    if(i>0 && j>0)
      rV3[(lxdim-i)+(lydim-j)*rvystride+k*rvzstride] = coef;
    if(i>0 && k>0)				    
      rV3[(lxdim-i)+j*rvystride+(lzdim-k)*rvzstride] = coef;
    if(j>0 && k>0)				    
      rV3[i+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
    if(i>0 && j>0 && k>0)
      rV3[(lxdim-i)+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
  }
  fftw_execute(forward_plan); // Take FFT of A00/11/22
  for(k=0;k<azdim;k++) for(j=0;j<aydim;j++) for(i=0;i<axdim;i++) {
    // Copy frequency data from V1/2/3 into A00/11/22.
    // Because of symmetries, V1/2/3 are real-valued.
    OC_INDEX aindex = i+j*aystride+k*azstride;
    OC_INDEX vindex = i+j*vystride+k*vzstride;
    A00[aindex] = V1[vindex][0];
    A11[aindex] = V2[vindex][0];
    A22[aindex] = V3[vindex][0];
  }

  // Repeat for A01/02/12
  for(index=0;index<3*rvtotalsize;index++) rV1[index] = 0.0;
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef=scale*Oxs_CalculateNxy(x,y,z,dx,dy,dz);
    rV1[i+j*rvystride+k*rvzstride] = coef;
    if(i>0) rV1[(lxdim-i)+j*rvystride+k*rvzstride] = -coef;
    if(j>0) rV1[i+(lydim-j)*rvystride+k*rvzstride] = -coef;
    if(k>0) rV1[i+j*rvystride+(lzdim-k)*rvzstride] =  coef;
    if(i>0 && j>0)
      rV1[(lxdim-i)+(lydim-j)*rvystride+k*rvzstride] =  coef;
    if(i>0 && k>0)				    
      rV1[(lxdim-i)+j*rvystride+(lzdim-k)*rvzstride] = -coef;
    if(j>0 && k>0)				    
      rV1[i+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = -coef;
    if(i>0 && j>0 && k>0)
      rV1[(lxdim-i)+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef=scale*Oxs_CalculateNxz(x,y,z,dx,dy,dz);
    rV2[i+j*rvystride+k*rvzstride] = coef;
    if(i>0) rV2[(lxdim-i)+j*rvystride+k*rvzstride] = -coef;
    if(j>0) rV2[i+(lydim-j)*rvystride+k*rvzstride] =  coef;
    if(k>0) rV2[i+j*rvystride+(lzdim-k)*rvzstride] = -coef;
    if(i>0 && j>0)
      rV2[(lxdim-i)+(lydim-j)*rvystride+k*rvzstride] = -coef;
    if(i>0 && k>0)				    
      rV2[(lxdim-i)+j*rvystride+(lzdim-k)*rvzstride] =  coef;
    if(j>0 && k>0)				    
      rV2[i+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = -coef;
    if(i>0 && j>0 && k>0)
      rV2[(lxdim-i)+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef=scale*Oxs_CalculateNyz(x,y,z,dx,dy,dz);
    rV3[i+j*rvystride+k*rvzstride] = coef;
    if(i>0) rV3[(lxdim-i)+j*rvystride+k*rvzstride] =  coef;
    if(j>0) rV3[i+(lydim-j)*rvystride+k*rvzstride] = -coef;
    if(k>0) rV3[i+j*rvystride+(lzdim-k)*rvzstride] = -coef;
    if(i>0 && j>0)
      rV3[(lxdim-i)+(lydim-j)*rvystride+k*rvzstride] = -coef;
    if(i>0 && k>0)				    
      rV3[(lxdim-i)+j*rvystride+(lzdim-k)*rvzstride] = -coef;
    if(j>0 && k>0)				    
      rV3[i+(lydim-j)*rvystride+(lzdim-k)*rvzstride] =  coef;
    if(i>0 && j>0 && k>0)
      rV3[(lxdim-i)+(lydim-j)*rvystride+(lzdim-k)*rvzstride] = coef;
  }
  fftw_execute(forward_plan);
  for(k=0;k<azdim;k++) for(j=0;j<aydim;j++) for(i=0;i<axdim;i++) {
    OC_INDEX aindex = i+j*aystride+k*azstride;
    OC_INDEX vindex = i+j*vystride+k*vzstride;
    A01[aindex] = V1[vindex][0];
    A02[aindex] = V2[vindex][0];
    A12[aindex] = V3[vindex][0];
  }

#ifdef FUBAR
/**/
  printf("\n ------- FFTW_Demag STANDARD ------\n");
  printf("\n Index     A00        A11        A22\n");
  for(k=0;k<azdim;k++) for(j=0;j<aydim;j++) for(i=0;i<axdim;i++) {
    OC_INDEX aindex = i+j*aystride+k*azstride;
    printf("%2d %2d %2d  %#10.4g  %#10.4g  %#10.4g\n",
	   i,j,k,A00[aindex],A11[aindex],A22[aindex]);
  }
  printf("\n Index     A01        A02        A12\n");
  for(k=0;k<azdim;k++) for(j=0;j<aydim;j++) for(i=0;i<axdim;i++) {
    OC_INDEX aindex = i+j*aystride+k*azstride;
    printf("%2d %2d %2d  %#10.4g  %#10.4g  %#10.4g\n",
	   i,j,k,A01[aindex],A02[aindex],A12[aindex]);
  }
  printf("\n ^^^^^^^ FFTW_Demag STANDARD ^^^^^^\n");
/**/
#endif // FUBAR
}

void FFTW_Demag::FastInitializeMemory(const Oxs_Mesh* genmesh) const
{ // This routine is conceptually const.
  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(genmesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(genmesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(msg.c_str());
  }

  // Clean-up from previous allocation, if any.
  ReleaseMemory();

  // Dimensional variables:
  //  xdim,  ydim,  zdim : Mesh (problem) dimensions.
  // lxdim, lydim, lzdim : Logical dimensions for FFT.
  // vxdim, vydim, vzdim : Physical dimensions For V1/2/3 arrays,
  //                       with complex-type elements.
  // axdim, aydim, azdim : Physical dimensions for A?? arrays.
  // When treated as real arrays, rV1/2/3 have dimensions
  // 2*vxdim, vydim, vzdim.

  // Space domain dimensions
  xdim = mesh->DimX();
  ydim = mesh->DimY();
  zdim = mesh->DimZ();
  if(xdim==0 || ydim==0 || zdim==0) return; // Empty mesh!

  // Logical frequency space dimensions (complex data)
  lxdim = 2*xdim;
  if(ydim>1) lydim = 2*ydim;
  else       lydim = 1;
  if(zdim>1) lzdim = 2*zdim;
  else       lzdim = 1;

  // Physical array dimensions, for complex data
  vxdim = (lxdim/2) + 1;
  vydim = lydim;
  vzdim = lzdim;
  vystride = vxdim;
  vzstride = vystride * vydim;
  vtotalsize = vzstride * vzdim;
  if(vtotalsize < vxdim || vtotalsize < vydim || vtotalsize < vzdim) {
    // Partial overflow check
    String msg = String("OC_INDEX overflow in ") + String(InstanceName())
      + String(": Product vxdim*vydim*vzdim too big"
               " to fit in a OC_INDEX variable");
    throw Oxs_ExtError(msg.c_str());
  }
  V1  = static_cast<fftw_complex*>(fftw_malloc(sizeof(fftw_complex)
					       *vtotalsize*3));
  V2  = V1 + vtotalsize;
  V3  = V2 + vtotalsize;

  rV1 = static_cast<double*>(static_cast<void*>(V1));
  rV2 = static_cast<double*>(static_cast<void*>(V2));
  rV3 = static_cast<double*>(static_cast<void*>(V3));
  rvxdim = 2*vxdim;  rvydim = vydim;  rvzdim = vzdim;
  rvystride = 2*vystride;
  rvzstride = 2*vzstride;
  rvtotalsize = 2*vtotalsize;

  // Allocate memory for interaction matrices.  These are real-valued,
  // and because of symmetries we only need to store the first octant
  axdim = OC_MAX(1,lxdim/2+1);
  aydim = OC_MAX(1,lydim/2+1);
  azdim = OC_MAX(1,lzdim/2+1);
  aystride = axdim;
  azstride = aystride * aydim;
  atotalsize = azstride * azdim;
  A00 = new double[atotalsize];
  A11 = new double[atotalsize];
  A22 = new double[atotalsize];
  A01 = new double[atotalsize];
  A02 = new double[atotalsize];
  A12 = new double[atotalsize];
  if(V1==NULL
     || A00==0 || A11==0 || A22==0
     || A01==0 || A02==0 || A12==0) {
    throw Oxs_ExtError("Insufficient memory in FFTW_Demag constructor.");
  }

  // Setup plans on V1/V2/V3.  We specify the dimensions in reverse
  // order, to account for the difference between column-major order
  // (Oxs_MeshValue arrays) and row-major order (FFTW arrays).
#if OC_INDEX_WIDTH > OC_INT_WIDTH
  if(lxdim>INT_MAX || lydim>INT_MAX || lzdim>INT_MAX) {
    throw Oxs_ExtError("Integer overflow in FFTW_Demag constructor.");
  }
#endif
  int pn[3] = { static_cast<int>(lzdim), static_cast<int>(lydim),
                static_cast<int>(lxdim)};
  forward_plan  = fftw_plan_many_dft_r2c(3,pn,3,
					 rV1,NULL,1,rvtotalsize,
					 V1,NULL,1,vtotalsize,
					 FFTW_MEASURE);
  backward_plan = fftw_plan_many_dft_c2r(3,pn,3,
					 V1,NULL,1,vtotalsize,
					 rV1,NULL,1,rvtotalsize,
					 FFTW_MEASURE);
  if(forward_plan==NULL || backward_plan==NULL) {
    throw Oxs_ExtError("FFTW plan initialization failed");
  }

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
  //  where fs = -1/(lxdim*lydim*lzdim)

  OC_INDEX index,i,j,k,istop,jstop,kstop;

  OC_REALWIDE dx = mesh->EdgeLengthX();
  OC_REALWIDE dy = mesh->EdgeLengthY();
  OC_REALWIDE dz = mesh->EdgeLengthZ();
  // For demag calculation, all that matters is the relative
  // size of dx, dy and dz.  To help insure we don't run
  // outside floating point range, rescale these values so
  // largest is 1.0
  OC_REALWIDE maxedge=dx;
  if(dy>maxedge) maxedge=dy;
  if(dz>maxedge) maxedge=dz;
  dx/=maxedge; dy/=maxedge; dz/=maxedge;

  OC_REALWIDE scale = 1.0/(4*PI*dx*dy*dz);

  // Also throw in FFT scaling.  This allows the "NoScale" FFT routines
  // to be used.
  scale *= 1.0/(lxdim*lydim*lzdim);

  for(index=0;index<3*rvtotalsize;++index) rV1[index]=0.0;

  // Calculate Nxx, Nyy and Nzz in first octant.
  // Step 1: Evaluate f at each cell site.  Offset by (-dx,-dy,-dz)
  //  so we can do 2nd derivative operations "in-place".
  kstop=1; if(zdim>1) kstop=zdim+2;
  jstop=1; if(ydim>1) jstop=ydim+2;
  istop=1; if(xdim>1) istop=xdim+2;
  for(k=0;k<kstop;k++) {
    OC_INDEX kindex = k*rvzstride;
    OC_REALWIDE z = 0.0;
    if(k==0) z = -dz;
    else if(k>1) z = dz*(k-1);
    for(j=0;j<jstop;j++) {
      OC_INDEX jkindex = kindex + j*rvystride;
      OC_REALWIDE y = 0.0;
      if(j==0) y = -dy;
      else if(j>1) y = dy*(j-1);
      for(i=0;i<istop;i++) {
	index = i+jkindex;
#ifndef NDEBUG
	if(index>=rvtotalsize) {
	  String msg = String("Programming error: array index out-of-bounds.");
	  throw Oxs_ExtError(this,msg);
	}
#endif // NDEBUG
	OC_REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rV1[index] = scale*Oxs_Newell_f(x,y,z);
	rV2[index] = scale*Oxs_Newell_f(y,x,z);
	rV3[index] = scale*Oxs_Newell_f(z,y,x);
      }
    }
  }

  // Step 2a: Do d^2/dz^2
  if(kstop==1) {
    // Only 1 layer in z-direction of f stored in r?comp arrays.
    for(j=0;j<jstop;j++) {
      OC_INDEX jkindex = j*rvystride;
      OC_REALWIDE y = 0.0;
      if(j==0) y = -dy;
      else if(j>1) y = dy*(j-1);
      for(i=0;i<istop;i++) {
	index = i+jkindex;
	OC_REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rV1[index] -= scale*Oxs_Newell_f(x,y,0);
	rV1[index] *= 2;
	rV2[index] -= scale*Oxs_Newell_f(y,x,0);
	rV2[index] *= 2;
	rV3[index] -= scale*Oxs_Newell_f(0,y,x);
	rV3[index] *= 2;
	/// f is even in z, so for example
	/// f(x,y,-dz) - 2f(x,y,0) + f(x,y,dz)
	///     =  2( f(x,y,-dz) - f(x,y,0) )
      }
    }
  } else {
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<jstop;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] += rV1[index+2*rvzstride];
	  rV1[index] -= 2*rV1[index+rvzstride];
	  rV2[index] += rV2[index+2*rvzstride];
	  rV2[index] -= 2*rV2[index+rvzstride];
	  rV3[index] += rV3[index+2*rvzstride];
	  rV3[index] -= 2*rV3[index+rvzstride];
	}
      }
    }
    for(k=zdim;k<kstop;k++) { // Zero-fill overhang
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<jstop;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] = 0.0;
	  rV2[index] = 0.0;
	  rV3[index] = 0.0;
	}
      }
    }
  }
  // Step 2b: Do d^2/dy^2
  if(jstop==1) {
    // Only 1 layer in y-direction of f stored in r?comp arrays.
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      OC_REALWIDE z = dz*k;
      for(i=0;i<istop;i++) {
	index = i+kindex;
	OC_REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rV1[index] -= scale * 
	  ((Oxs_Newell_f(x,0,z-dz)+Oxs_Newell_f(x,0,z+dz))
           -2*Oxs_Newell_f(x,0,z));
	rV1[index] *= 2;
	rV2[index] -= scale * 
	  ((Oxs_Newell_f(0,x,z-dz)+Oxs_Newell_f(0,x,z+dz))
           -2*Oxs_Newell_f(0,x,z));
	rV2[index] *= 2;
	rV3[index] -= scale * 
	  ((Oxs_Newell_f(z-dz,0,x)+Oxs_Newell_f(z+dz,0,x))
           -2*Oxs_Newell_f(z,0,x));
	rV3[index] *= 2;
	/// f is even in y
      }
    }
  } else {
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<ydim;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] += rV1[index+2*rvystride];
	  rV1[index] -= 2*rV1[index+rvystride];
	  rV2[index] += rV2[index+2*rvystride];
	  rV2[index] -= 2*rV2[index+rvystride];
	  rV3[index] += rV3[index+2*rvystride];
	  rV3[index] -= 2*rV3[index+rvystride];
	}
      }
    }
    for(k=0;k<zdim;k++) { // Zero-fill overhang
      OC_INDEX kindex = k*rvzstride;
      for(j=ydim;j<jstop;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] = 0.0;
	  rV2[index] = 0.0;
	  rV3[index] = 0.0;
	}
      }
    }
  }
  // Step 2c: Do d^2/dx^2
  if(istop==1) {
    // Only 1 layer in x-direction of f stored in r?comp arrays.
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      OC_REALWIDE z = dz*k;
      for(j=0;j<ydim;j++) {
	index = kindex + j*rvystride;
	OC_REALWIDE y = dy*j;
	rV1[index] -= scale * 
	  ((4*Oxs_Newell_f(0,y,z)
	    +Oxs_Newell_f(0,y+dy,z+dz)+Oxs_Newell_f(0,y-dy,z+dz)
	    +Oxs_Newell_f(0,y+dy,z-dz)+Oxs_Newell_f(0,y-dy,z-dz))
	   -2*(Oxs_Newell_f(0,y+dy,z)+Oxs_Newell_f(0,y-dy,z)
	       +Oxs_Newell_f(0,y,z+dz)+Oxs_Newell_f(0,y,z-dz)));
	rV1[index] *= 2;
	rV2[index] -= scale * 
	  ((4*Oxs_Newell_f(y,0,z)
	    +Oxs_Newell_f(y+dy,0,z+dz)+Oxs_Newell_f(y-dy,0,z+dz)
	    +Oxs_Newell_f(y+dy,0,z-dz)+Oxs_Newell_f(y-dy,0,z-dz))
	   -2*(Oxs_Newell_f(y+dy,0,z)+Oxs_Newell_f(y-dy,0,z)
	       +Oxs_Newell_f(y,0,z+dz)+Oxs_Newell_f(y,0,z-dz)));
	rV2[index] *= 2;
	rV3[index] -= scale * 
	  ((4*Oxs_Newell_f(z,y,0)
	    +Oxs_Newell_f(z+dz,y+dy,0)+Oxs_Newell_f(z+dz,y-dy,0)
	    +Oxs_Newell_f(z-dz,y+dy,0)+Oxs_Newell_f(z-dz,y-dy,0))
	   -2*(Oxs_Newell_f(z,y+dy,0)+Oxs_Newell_f(z,y-dy,0)
	       +Oxs_Newell_f(z+dz,y,0)+Oxs_Newell_f(z-dz,y,0)));
	rV3[index] *= 2;
	/// f is even in x.
      }
    }
  } else {
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<ydim;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<xdim;i++) {
	  index = i+jkindex;
	  rV1[index] += rV1[index+2];
	  rV1[index] -= 2*rV1[index+1];
	  rV2[index] += rV2[index+2];
	  rV2[index] -= 2*rV2[index+1];
	  rV3[index] += rV3[index+2];
	  rV3[index] -= 2*rV3[index+1];
	}
      }
    }
    for(k=0;k<zdim;k++) { // Zero-fill overhang
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<ydim;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=xdim;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] = 0.0;
	  rV2[index] = 0.0;
	  rV3[index] = 0.0;
	}
      }
    }
  }

  // Step 3: Use symmetries to reflect into other octants.
  for(k=0;k<zdim;k++) {
    OC_INDEX kindex = k*rvzstride;
    for(j=0;j<ydim;j++) {
      OC_INDEX jkindex = kindex + j*rvystride;
      for(i=0;i<xdim;i++) {
	index = i+jkindex;
	OC_REALWIDE xtemp = rV1[index];
	OC_REALWIDE ytemp = rV2[index];
	OC_REALWIDE ztemp = rV3[index];
	if(i>0) {
	  OC_INDEX tindex = (lxdim-i)+j*rvystride+k*rvzstride;
	  rV1[tindex]=xtemp;
	  rV2[tindex]=ytemp;
	  rV3[tindex]=ztemp;
	}
	if(j>0) {
	  OC_INDEX tindex = i+(lydim-j)*rvystride+k*rvzstride;
	  rV1[tindex]=xtemp;
	  rV2[tindex]=ytemp;
	  rV3[tindex]=ztemp;
	}
	if(k>0) {
	  OC_INDEX tindex = i+j*rvystride+(lzdim-k)*rvzstride;
	  rV1[tindex]=xtemp;
	  rV2[tindex]=ytemp;
	  rV3[tindex]=ztemp;
	}
	if(i>0 && j>0) {
	  OC_INDEX tindex = (lxdim-i)+(lydim-j)*rvystride+k*rvzstride;
	  rV1[tindex]=xtemp;
	  rV2[tindex]=ytemp;
	  rV3[tindex]=ztemp;
	}
	if(i>0 && k>0) {
	  OC_INDEX tindex = (lxdim-i)+j*rvystride+(lzdim-k)*rvzstride;
	  rV1[tindex]=xtemp;
	  rV2[tindex]=ytemp;
	  rV3[tindex]=ztemp;
	}
	if(j>0 && k>0) {
	  OC_INDEX tindex = i+(lydim-j)*rvystride+(lzdim-k)*rvzstride;
	  rV1[tindex]=xtemp;
	  rV2[tindex]=ytemp;
	  rV3[tindex]=ztemp;
	}
	if(i>0 && j>0 && k>0) {
	  OC_INDEX tindex = (lxdim-i)+(lydim-j)*rvystride+(lzdim-k)*rvzstride;
	  rV1[tindex]=xtemp;
	  rV2[tindex]=ytemp;
	  rV3[tindex]=ztemp;
	}
      }
    }
  }

  // Special "SelfDemag" code may be more accurate at index 0,0,0
 rV1[0] = -Oxs_SelfDemagNx(dx,dy,dz)/static_cast<double>(lxdim*lydim*lzdim);
 rV2[0] = -Oxs_SelfDemagNy(dx,dy,dz)/static_cast<double>(lxdim*lydim*lzdim);
 rV3[0] = -Oxs_SelfDemagNz(dx,dy,dz)/static_cast<double>(lxdim*lydim*lzdim);

  // Step 4: Transform into frequency domain.
  fftw_execute(forward_plan);

  // Step 5: Copy frequency data from V1/2/3 into A00/11/22.
  // Because of symmetries, V1/2/3 are real-valued, and we need
  // only store 1/8th of the data (the first octant).
  for(k=0;k<azdim;k++) for(j=0;j<aydim;j++) for(i=0;i<axdim;i++) {
    OC_INDEX aindex = i+j*aystride+k*azstride;
    OC_INDEX vindex = i+j*vystride+k*vzstride;
    A00[aindex] = V1[vindex][0];
    A11[aindex] = V2[vindex][0];
    A22[aindex] = V3[vindex][0];
  }

  // Repeat for Nxy, Nxz and Nyz. //////////////////////////////////////
  // Step 1: Evaluate g at each cell site.  Offset by (-dx,-dy,-dz)
  for(index=0;index<3*rvtotalsize;++index) rV1[index]=0.0;
  for(k=0;k<kstop;k++) {
    OC_INDEX kindex = k*rvzstride;
    OC_REALWIDE z = 0.0;
    if(k==0) z = -dz;
    else if(k>1) z = dz*(k-1);
    for(j=0;j<jstop;j++) {
      OC_INDEX jkindex = kindex + j*rvystride;
      OC_REALWIDE y = 0.0;
      if(j==0) y = -dy;
      else if(j>1) y = dy*(j-1);
      for(i=0;i<istop;i++) {
	index = i+jkindex;
	OC_REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rV1[index] = scale*Oxs_Newell_g(x,y,z);
	rV2[index] = scale*Oxs_Newell_g(x,z,y);
	rV3[index] = scale*Oxs_Newell_g(y,z,x);
      }
    }
  }

  // Step 2a: Do d^2/dz^2
  if(kstop==1) {
    // Only 1 layer in z-direction of g stored in r?comp arrays.
    for(j=0;j<jstop;j++) {
      OC_INDEX jkindex = j*rvystride;
      OC_REALWIDE y = 0.0;
      if(j==0) y = -dy;
      else if(j>1) y = dy*(j-1);
      for(i=0;i<istop;i++) {
	index = i+jkindex;
	OC_REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rV1[index] -= scale*Oxs_Newell_g(x,y,0);
	rV1[index] *= 2;
	// NOTE: g is even in z.
	// If zdim==1, then Nxz and Nyz are identically zero.
      }
    }
  } else {
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<jstop;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] += rV1[index+2*rvzstride];
	  rV1[index] -= 2*rV1[index+rvzstride];
	  rV2[index] += rV2[index+2*rvzstride];
	  rV2[index] -= 2*rV2[index+rvzstride];
	  rV3[index] += rV3[index+2*rvzstride];
	  rV3[index] -= 2*rV3[index+rvzstride];
	}
      }
    }
    for(k=zdim;k<kstop;k++) { // Zero-fill overhang
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<jstop;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] = 0.0;
	  rV2[index] = 0.0;
	  rV3[index] = 0.0;
	}
      }
    }
  }
  // Step 2b: Do d^2/dy^2
  if(jstop==1) {
    // Only 1 layer in y-direction of f stored in r?comp arrays.
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      OC_REALWIDE z = dz*k;
      for(i=0;i<istop;i++) {
	index = i+kindex;
	OC_REALWIDE x = 0.0;
	if(i==0) x = -dx;
	else if(i>1) x = dx*(i-1);
	rV2[index] -= scale * 
	  ((Oxs_Newell_g(x,z-dz,0)+Oxs_Newell_g(x,z+dz,0))
           -2*Oxs_Newell_g(x,z,0));
	rV2[index] *= 2;
	// Note: g is even in its third argument.
	// If ydim==1, then Nxy and Nyz are identically zero.
      }
    }
  } else {
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<ydim;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] += rV1[index+2*rvystride];
	  rV1[index] -= 2*rV1[index+rvystride];
	  rV2[index] += rV2[index+2*rvystride];
	  rV2[index] -= 2*rV2[index+rvystride];
	  rV3[index] += rV3[index+2*rvystride];
	  rV3[index] -= 2*rV3[index+rvystride];
	}
      }
    }
    for(k=0;k<zdim;k++) { // Zero-fill overhang
      OC_INDEX kindex = k*rvzstride;
      for(j=ydim;j<jstop;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] = 0.0;
	  rV2[index] = 0.0;
	  rV3[index] = 0.0;
	}
      }
    }
  }
  // Step 2c: Do d^2/dx^2
  if(istop==1) {
    // Only 1 layer in x-direction of f stored in r?comp arrays.
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      OC_REALWIDE z = dz*k;
      for(j=0;j<ydim;j++) {
	index = kindex + j*rvystride;
	OC_REALWIDE y = dy*j;
	rV3[index] -= scale * 
	  ((4*Oxs_Newell_g(y,z,0)
	    +Oxs_Newell_g(y+dy,z+dz,0)+Oxs_Newell_g(y-dy,z+dz,0)
	    +Oxs_Newell_g(y+dy,z-dz,0)+Oxs_Newell_g(y-dy,z-dz,0))
	   -2*(Oxs_Newell_g(y+dy,z,0)+Oxs_Newell_g(y-dy,z,0)
	       +Oxs_Newell_g(y,z+dz,0)+Oxs_Newell_g(y,z-dz,0)));
	rV3[index] *= 2;
	// Note: g is even in its third argument.
	// If xdim==1, then Nxy and Nxz are identically zero.
      }
    }
  } else {
    for(k=0;k<zdim;k++) {
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<ydim;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=0;i<xdim;i++) {
	  index = i+jkindex;
	  rV1[index] += rV1[index+2];
	  rV1[index] -= 2*rV1[index+1];
	  rV2[index] += rV2[index+2];
	  rV2[index] -= 2*rV2[index+1];
	  rV3[index] += rV3[index+2];
	  rV3[index] -= 2*rV3[index+1];
	}
      }
    }
    for(k=0;k<zdim;k++) { // Zero-fill overhang
      OC_INDEX kindex = k*rvzstride;
      for(j=0;j<ydim;j++) {
	OC_INDEX jkindex = kindex + j*rvystride;
	for(i=xdim;i<istop;i++) {
	  index = i+jkindex;
	  rV1[index] = 0.0;
	  rV2[index] = 0.0;
	  rV3[index] = 0.0;
	}
      }
    }
  }

  // Step 3: Use symmetries to reflect into other octants.
  //     Also, at each coordinate plane, set to 0.0 any term
  //     which is odd across that boundary.  It should already
  //     be close to 0, but will likely be slightly off due to
  //     rounding errors.
  for(k=0;k<zdim;k++) {
    OC_INDEX kindex = k*rvzstride;
    for(j=0;j<ydim;j++) {
      OC_INDEX jkindex = kindex + j*rvystride;
      for(i=0;i<xdim;i++) {
	index = i+jkindex;

	if(i==0 || j==0) rV1[index]=0.0;
	if(i==0 || k==0) rV2[index]=0.0;
	if(j==0 || k==0) rV3[index]=0.0;

	OC_REALWIDE xtemp = rV1[index];
	OC_REALWIDE ytemp = rV2[index];
	OC_REALWIDE ztemp = rV3[index];
	if(i>0) {
	  OC_INDEX tindex = (lxdim-i)+j*rvystride+k*rvzstride;
	  rV1[tindex] = -1*xtemp;
	  rV2[tindex] = -1*ytemp;
	  rV3[tindex] =    ztemp;
	}
	if(j>0) {
	  OC_INDEX tindex = i+(lydim-j)*rvystride+k*rvzstride;
	  rV1[tindex] = -1*xtemp;
	  rV2[tindex] =    ytemp;
	  rV3[tindex] = -1*ztemp;
	}
	if(k>0) {
	  OC_INDEX tindex = i+j*rvystride+(lzdim-k)*rvzstride;
	  rV1[tindex] =    xtemp;
	  rV2[tindex] = -1*ytemp;
	  rV3[tindex] = -1*ztemp;
	}
	if(i>0 && j>0) {
	  OC_INDEX tindex = (lxdim-i)+(lydim-j)*rvystride+k*rvzstride;
	  rV1[tindex] =    xtemp;
	  rV2[tindex] = -1*ytemp;
	  rV3[tindex] = -1*ztemp;
	}
	if(i>0 && k>0) {
	  OC_INDEX tindex = (lxdim-i)+j*rvystride+(lzdim-k)*rvzstride;
	  rV1[tindex] = -1*xtemp;
	  rV2[tindex] =    ytemp;
	  rV3[tindex] = -1*ztemp;
	}
	if(j>0 && k>0) {
	  OC_INDEX tindex = i+(lydim-j)*rvystride+(lzdim-k)*rvzstride;
	  rV1[tindex] = -1*xtemp;
	  rV2[tindex] = -1*ytemp;
	  rV3[tindex] =    ztemp;
	}
	if(i>0 && j>0 && k>0) {
	  OC_INDEX tindex = (lxdim-i)+(lydim-j)*rvystride+(lzdim-k)*rvzstride;
	  rV1[tindex]=xtemp;
	  rV2[tindex]=ytemp;
	  rV3[tindex]=ztemp;
	}
      }
    }
  }

  // Step 4: Transform into frequency domain.
  fftw_execute(forward_plan);

  // Step 5: Copy frequency data from V1/2/3 into A01/02/12.
  // Because of symmetries, V1/2/3 are real-valued, and we need
  // only store 1/8th of the data (the first octant).
  for(k=0;k<azdim;k++) for(j=0;j<aydim;j++) for(i=0;i<axdim;i++) {
    OC_INDEX aindex = i+j*aystride+k*azstride;
    OC_INDEX vindex = i+j*vystride+k*vzstride;
    A01[aindex] = V1[vindex][0];
    A02[aindex] = V2[vindex][0];
    A12[aindex] = V3[vindex][0];
  }

#ifdef FUBAR
/**/
  printf("\n ------- FFTW_Demag FAST ------\n");
  printf(" Index     A00        A11        A22\n");
  for(k=0;k<azdim;k++) for(j=0;j<aydim;j++) for(i=0;i<axdim;i++) {
    OC_INDEX aindex = i+j*aystride+k*azstride;
    printf("%2d %2d %2d  %#10.4g  %#10.4g  %#10.4g\n",
	   i,j,k,A00[aindex],A11[aindex],A22[aindex]);
  }
  printf("\n Index     A01        A02        A12\n");
  for(k=0;k<azdim;k++) for(j=0;j<aydim;j++) for(i=0;i<axdim;i++) {
    OC_INDEX aindex = i+j*aystride+k*azstride;
    printf("%2d %2d %2d  %#10.4g  %#10.4g  %#10.4g\n",
	   i,j,k,A01[aindex],A02[aindex],A12[aindex]);
  }
  printf("\n ^^^^^^^ FFTW_Demag FAST ^^^^^^\n");
/**/
#endif // FUBAR
}


void FFTW_Demag::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  // (Re)-initialize mesh coefficient array if mesh has changed,
  // and set FFTW plans
  if(mesh_id != state.mesh->Id()) {
    mesh_id = 0; // Safety
    (this->*initmem)(state.mesh);
    mesh_id = state.mesh->Id();
  }

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  // Fill rV1/2/3 with Mx/y/z, zero pad, and take FFT.
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX i,j,k;
  OC_INDEX index,rvindex,rvstop;
  for(k=0;k<zdim;++k) {
    OC_INDEX koff = k*xydim;
    OC_INDEX rvkoff = k*rvzstride;
    for(j=0;j<ydim;++j) {
      index = j*xdim+koff;
      rvindex = j*rvystride+rvkoff;
      rvstop = rvindex + rvxdim;
      for(i=0;i<xdim;++i,++index,++rvindex) {
	OC_REAL8m scale = Ms[index];
	const ThreeVector& vec = spin[index];
	rV1[rvindex] = scale*vec.x;
	rV2[rvindex] = scale*vec.y;
	rV3[rvindex] = scale*vec.z;
      }
      while(rvindex<rvstop) {
	rV1[rvindex] = rV2[rvindex] = rV3[rvindex] = 0.0;
	++rvindex;
      }
    }
    rvindex = ydim*rvystride+rvkoff;
    rvstop = rvkoff + rvystride*rvydim;
    while(rvindex<rvstop) {
	rV1[rvindex] = rV2[rvindex] = rV3[rvindex] = 0.0;
	++rvindex;
    }
  }
  rvindex = zdim*rvzstride;
  while(rvindex<rvtotalsize) {
    rV1[rvindex] = rV2[rvindex] = rV3[rvindex] = 0.0;
    ++rvindex;
  }
  fftw_execute(forward_plan);
  
  // Calculate field components in frequency domain, then do iFFT
  // to transform back to space domain.
  for(k=0;k<lzdim;k++) {
    int aksign = -1;  OC_INDEX ak = lzdim - k;
    if(ak>=k) { ak = k; aksign = 1; }

    for(j=0;j<lydim;j++) {
      int ajsign = -1;  OC_INDEX aj = lydim - j;
      if(aj>=j) { aj = j; ajsign = 1; }

      for(i=0;i<lxdim/2+1;i++) {
	OC_INDEX aindex = i+aj*aystride+ak*azstride;
	const double a00r = A00[aindex];
	const double a11r = A11[aindex];
	const double a22r = A22[aindex];
	const double a01r = A01[aindex]*ajsign;
	const double a02r = A02[aindex]*aksign;
	const double a12r = A12[aindex]*ajsign*aksign;

	index = i + j*vystride + k*vzstride;
	const double mxr = V1[index][0];
	const double mxi = V1[index][1];
	const double myr = V2[index][0];
	const double myi = V2[index][1];
	const double mzr = V3[index][0];
	const double mzi = V3[index][1];

	// Compute H and store in V
	V1[index][0] = a00r*mxr + a01r*myr + a02r*mzr;
	V1[index][1] = a00r*mxi + a01r*myi + a02r*mzi;
	V2[index][0] = a01r*mxr + a11r*myr + a12r*mzr;
	V2[index][1] = a01r*mxi + a11r*myi + a12r*mzi;
	V3[index][0] = a02r*mxr + a12r*myr + a22r*mzr;
	V3[index][1] = a02r*mxi + a12r*myi + a22r*mzi;
	
      }
    }
  }

  // Transform H to space domain and copy into field array
  fftw_execute(backward_plan);
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    index  = i+j*xdim+k*xydim;
    rvindex = i+j*rvystride+k*rvzstride;
    field[index].x = rV1[rvindex];
    field[index].y = rV2[rvindex];
    field[index].z = rV3[rvindex];
  }

  // Calculate pointwise energy density: -0.5*MU0*<M,H>
  OC_REAL8m mult = -0.5 * MU0;
  OC_REAL8m totalsize = xdim*ydim*zdim;
  for(index=0;index<totalsize;index++) {
    energy[index] = mult * Ms[index] * (spin[index]*field[index]);
  }

}
