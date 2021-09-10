/* FILE: simpledemag.cc            -*-Mode: c++-*-
 *
 * Average H demag field across rectangular cells.  Simple
 * implementation that does not take advantage of any memory
 * saving or calculation speed improvements possible using
 * symmetries in interaction tensor or realness of data in
 * spatial domain.
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
#include "simpledemag.h"
#include "demagcoef.h"
#include "fft.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_SimpleDemag);

/* End includes */

// Helper function
OC_INDEX Oxs_SimpleDemag::NextPowerOfTwo(OC_INDEX n) const
{ // Returns first power of two >= n
  OC_INDEX m=1;
  while(m<n) m*=2;
  return m;
}

// Constructor
Oxs_SimpleDemag::Oxs_SimpleDemag(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    xdim(0),ydim(0),zdim(0),totalsize(0),
    pxdim(0),pydim(0),pzdim(0),ptotalsize(0),
    mesh_id(0),energy_density_error_estimate(-1),
    A00(NULL),A01(NULL),A02(NULL),A11(NULL),A12(NULL),A22(NULL),
    Mx(NULL),My(NULL),Mz(NULL),Hcomp(NULL)
{
  VerifyAllInitArgsUsed();
}

OC_BOOL Oxs_SimpleDemag::Init()
{
  mesh_id = 0;
  energy_density_error_estimate = -1;
  ReleaseMemory();
  return Oxs_Energy::Init();
}

void Oxs_SimpleDemag::ReleaseMemory() const
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

void Oxs_SimpleDemag::FillCoefficientArrays(const Oxs_Mesh* genmesh) const
{ // This routine is conceptually const.
  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(genmesh);
  if(mesh==NULL) {
    String msg = String("Object ")
      + String(genmesh->InstanceName())
      + String(" is not a (non-periodic) rectangular mesh.");
    throw Oxs_ExtError(msg.c_str());
  }
  // Note: It probably would not be too hard to add support
  // for periodic rectangular meshes.  See files demag.* and
  // demagcoef.* for ideas.

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
    String msg = String("OC_INDEX overflow in ") + String(InstanceName())
      + String(": Product xdim*ydim*zdim too big"
               " to fit in a OC_INDEX variable");
    throw Oxs_ExtError(msg.c_str());
  }
  pxdim = NextPowerOfTwo(2*xdim);
  pydim = NextPowerOfTwo(2*ydim);
  pzdim = NextPowerOfTwo(2*zdim);
  ptotalsize=pxdim*pydim*pzdim;
  if(ptotalsize < pxdim || ptotalsize < pydim || ptotalsize < pzdim) {
    // Partial overflow check
    String msg = String("OC_INDEX overflow in ") + String(InstanceName())
      + String(": Product pxdim*pydim*pzdim too big"
               " to fit in a OC_INDEX variable");
    throw Oxs_ExtError(msg.c_str());
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
    throw Oxs_ExtError(msg.c_str());
  }

  // Initialize interaction matrices to zeros
  OC_INDEX pindex;
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
  OC_INDEX i,j,k;
  OC_INDEX pxydim=pxdim*pydim;
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
  const OC_REALWIDE scale = -1.;
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef = scale
      * static_cast<OC_REALWIDE>(Oxs_CalculateNxx(x,y,z,dx,dy,dz).Hi());
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
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef = scale
      * static_cast<OC_REALWIDE>(Oxs_CalculateNyy(x,y,z,dx,dy,dz).Hi());
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
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef = scale
      * static_cast<OC_REALWIDE>(Oxs_CalculateNzz(x,y,z,dx,dy,dz).Hi());
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
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef = scale
      * static_cast<OC_REALWIDE>(Oxs_CalculateNxy(x,y,z,dx,dy,dz).Hi());
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
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef = scale
      * static_cast<OC_REALWIDE>(Oxs_CalculateNxz(x,y,z,dx,dy,dz).Hi());
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
  }
  for(k=0;k<zdim;k++) for(j=0;j<ydim;j++) for(i=0;i<xdim;i++) {
    OC_REALWIDE x = dx*i;
    OC_REALWIDE y = dy*j;
    OC_REALWIDE z = dz*k;
    OC_REALWIDE coef = scale
      * static_cast<OC_REALWIDE>(Oxs_CalculateNyz(x,y,z,dx,dy,dz).Hi());
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
  }

  // Transform interaction matrices into frequency domain.
  fft.Forward(pxdim,pydim,pzdim,A00);
  fft.Forward(pxdim,pydim,pzdim,A01);
  fft.Forward(pxdim,pydim,pzdim,A02);
  fft.Forward(pxdim,pydim,pzdim,A11);
  fft.Forward(pxdim,pydim,pzdim,A12);
  fft.Forward(pxdim,pydim,pzdim,A22);
}

void Oxs_SimpleDemag::ComputeEnergy
(const Oxs_SimState& state,
 Oxs_ComputeEnergyData& oced
 ) const
{
  // This routine was originally coded to the older
  // Oxs_Energy::GetEnergy interface.  The conversion to the
  // ::ComputeEnergy interface is serviceable, but could be made more
  // efficient; in particular, Hdemag is typically not individually
  // requested, so rather than fill the H output array, as each
  // inverse x-axis FFT is computed that x-strip could be used to fill
  // mxH and energy accum arrays.

  // (Re)-initialize mesh coefficient array if mesh has changed.
  if(mesh_id != state.mesh->Id()) {
    mesh_id = 0; // Safety
    FillCoefficientArrays(state.mesh);
    mesh_id = state.mesh->Id();
    energy_density_error_estimate
      = 0.5*OC_REAL8m_EPSILON*MU0*state.max_absMs*state.max_absMs
      *log(double(ptotalsize))/log(2.);
  }
  oced.energy_density_error_estimate = energy_density_error_estimate;

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  Oxs_MeshValue<ThreeVector>& field
    = *(oced.H != 0 ? oced.H : oced.scratch_H);
  field.AdjustSize(state.mesh);

  // Calculate FFT of Mx, My and Mz
  OC_INDEX xydim=xdim*ydim;
  OC_INDEX pxydim=pxdim*pydim;
  OC_INDEX index,pindex;
  OC_INDEX i,j,k;
  for(pindex=0;pindex<ptotalsize;pindex++) Mx[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) My[pindex].Set(0.,0.);
  for(pindex=0;pindex<ptotalsize;pindex++) Mz[pindex].Set(0.,0.);
  for(k=0;k<zdim;k++) {
    for(j=0;j<ydim;j++) {
      for(i=0;i<xdim;i++) {
        index  = i+j*xdim+k*xydim;
        pindex = i+j*pxdim+k*pxydim;
        OC_REAL8m scale = Ms[index];
        const ThreeVector& vec = spin[index];
        Mx[pindex].Set(scale*(vec.x),0);
        My[pindex].Set(scale*(vec.y),0);
        Mz[pindex].Set(scale*(vec.z),0);
      }
    }
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

  // Calculate pointwise energy density: -0.5*MU0*<M,H>, and other terms
  // as requested.
  assert(oced.H==0 || oced.H == &field);
  const OC_REAL8m emult = -0.5 * MU0;
  Oxs_Energy::SUMTYPE esum = 0.0;
  for(index=0;index<totalsize;index++) {
    OC_REAL8m ei = emult * Ms[index] * (spin[index]*field[index]);
    ThreeVector torque = spin[index]^field[index];
    esum += ei;
    if(oced.H_accum)           (*oced.H_accum)[index] += field[index];
    if(oced.energy)             (*oced.energy)[index]  = ei;
    if(oced.energy_accum) (*oced.energy_accum)[index] += ei;
    if(oced.mxH)                   (*oced.mxH)[index]  = torque;
    if(oced.mxH_accum)       (*oced.mxH_accum)[index] += torque;
    // If oced.H != NULL then field points to oced.H and so oced.H is
    // already filled.
  }
  oced.energy_sum = esum * state.mesh->Volume(0);
  /// All cells have same volume in an Oxs_RectangularMesh.

  oced.pE_pt = 0.0;
}
