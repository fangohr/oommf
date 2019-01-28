/* FILE: edgecorranisotropy.cc            -*-Mode: c++-*-
 *
 * Edge Correction Anisotropy, derived from Oxs_Energy class.
 *
 */

#include <assert.h>
#include <string>

#include "oc.h"
#include "nb.h"
#include "vf.h"

#include "arrayscalarfield.h"
#include "director.h"
#include "threevector.h"
#include "simstate.h"
#include "ext.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "uniformvectorfield.h"
#include "util.h"
#include "rectangularmesh.h"
#include "edgecorranisotropy.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

#if 0 && !defined(NDEBUG)
# define DEBUG_PRINT
#endif // NDEBUG

#ifdef DEBUG_PRINT
# include <stdio.h>
# include <string.h>
#endif

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_EdgeCorrAnisotropy);

/* End includes */

// Debug routine
#ifdef DEBUG_PRINT
void PrintMeshValue_Real
(
 const char* msg,const char* fmt,
 const Oxs_RectangularMesh* mesh,
 const Oxs_MeshValue<OC_REAL8m>& val
)
{
  const OC_INDEX xdim  = mesh->DimX();
  const OC_INDEX ydim  = mesh->DimY();
  const OC_INDEX zdim  = mesh->DimZ();

  OC_INDEX i,j,k;

  char eqline[] = "==================================="
    "=============================================";

  int msglen = strlen(msg);
  int prelen = (77 - msglen)/2;
  int postlen = 77 - prelen - msglen;
  if(prelen<1) prelen = 1;
  if(postlen<1) postlen = 1;

  printf("%1.*s %1.*s %1.*s\n",prelen,eqline,msglen,msg,postlen,eqline);
  for(k=0;k<zdim;++k) {
    printf("k=%d\n",k);
    for(j=ydim-1;j>=0;--j) {
      for(i=0;i<xdim;++i) {
        printf(fmt,val[mesh->Index(i,j,k)]);
      }
      printf("\n");
    }
    if(k+1<zdim) {
      printf(" --------------------------------------------------------\n");
    }      
  }
  printf("%79.79s\n",eqline);
}
void PrintMeshValue_int
(
 const char* msg,const char* fmt,
 const Oxs_RectangularMesh* mesh,
 const Oxs_MeshValue<int>& val
)
{
  const OC_INDEX xdim  = mesh->DimX();
  const OC_INDEX ydim  = mesh->DimY();
  const OC_INDEX zdim  = mesh->DimZ();

  OC_INDEX i,j,k;

  char eqline[] = "==================================="
    "=============================================";

  int msglen = strlen(msg);
  int prelen = (77 - msglen)/2;
  int postlen = 77 - prelen - msglen;
  if(prelen<1) prelen = 1;
  if(postlen<1) postlen = 1;

  printf("%1.*s %1.*s %1.*s\n",prelen,eqline,msglen,msg,postlen,eqline);
  for(k=0;k<zdim;++k) {
    printf("k=%d\n",k);
    for(j=ydim-1;j>=0;--j) {
      for(i=0;i<xdim;++i) {
        printf(fmt,val[mesh->Index(i,j,k)]);
      }
      printf("\n");
    }
    if(k+1<zdim) {
      printf(" --------------------------------------------------------\n");
    }      
  }
  printf("%79.79s\n",eqline);
}
#endif // DEBUG_PRINT


// Helper method that computes Ms_coarse and anisotropy corrections
void Oxs_EdgeCorrAnisotropy::ComputeAnisotropyCorrections
( // Imports
 Oxs_Director* director,     // Application director
 OC_INDEX subcount_x,   // Fine cells counts inside one coarse cell
 OC_INDEX subcount_y,
 OC_INDEX subcount_z,
 OC_INDEX supercount_x, // Coarse cell counts used in correction
 OC_INDEX supercount_y,
 OC_INDEX supercount_z,
 Oxs_Energy* cDemag, // Demag object for coarse working mesh
 Oxs_Energy* fDemag, // Demag object for fine working mesh
 const Oxs_RectangularMesh* cmesh, // Coarse mesh
 const Oxs_ScalarField* Msinit,    // Ms initializer
 // Exports
 Oxs_MeshValue<OC_REAL8m>& Ms_coarse, // Averaged Ms, on coarse grid
 Oxs_MeshValue<ThreeVector>& Ncorr_diag,    // Correction terms; see
 Oxs_MeshValue<ThreeVector>& Ncorr_offdiag  // header file for details.
 )
{
  // NOTE: Unless you want total demag coefficient thrashing on the
  // demag objects for the working meshes, cDemag and fDemag had better
  // point to distinct objects.
  assert(cDemag != fDemag);

  OC_INDEX i,j,k;

  // Coarse mesh dimensions
  const OC_INDEX cxdim  = cmesh->DimX();
  const OC_INDEX cydim  = cmesh->DimY();
  const OC_INDEX czdim  = cmesh->DimZ();
  const OC_INDEX cxydim = cxdim * cydim;

  // Create fine mesh
  Oxs_OwnedPointer<Oxs_RectangularMesh> fmesh;
  cmesh->MakeRefinedMesh("Oxs_EdgeCorrAnistropy_FineMesh",
                         subcount_x,subcount_y,subcount_z,fmesh);
  const OC_INDEX fxdim  = fmesh->DimX();
  const OC_INDEX fydim  = fmesh->DimY();
  const OC_INDEX fzdim  = fmesh->DimZ();
  const OC_INDEX fxydim = fxdim * fydim;

  // Ms on fine mesh
  Oxs_MeshValue<OC_REAL8m> Ms_fine;
  Msinit->FillMeshValue(fmesh.GetPtr(),Ms_fine);

#ifdef DEBUG_PRINT
  PrintMeshValue_Real("Ms_fine"," %1.0f",fmesh.GetPtr(),Ms_fine);
#endif // DEBUG_PRINT

  // Boolean array to keep track of which coarse cells are not
  // uniformly filled, and hence require correction.
  Oxs_MeshValue<int> cell_needs_correction;
  cell_needs_correction.AdjustSize(cmesh);

  // Compute averaged Ms values for coarse mesh
  Ms_coarse.AdjustSize(cmesh);
  const OC_REAL8m cellscale
    = 1.0/static_cast<OC_REAL8m>(subcount_x*subcount_y*subcount_z);
  OC_INDEX ic,jc,kc;
  for(kc=0;kc<czdim;++kc) {
    OC_INDEX koff = kc*subcount_z;
    for(jc=0;jc<cydim;++jc) {
      OC_INDEX joff = jc*subcount_y;
      for(ic=0;ic<cxdim;++ic) {
        OC_INDEX ioff = ic*subcount_x;
        OC_REAL8m sum = 0.0;
        OC_REAL8m corrcheckval = Ms_fine[fmesh->Index(ioff,joff,koff)];
        int ncorr = 0; // Needs correction check; correction
        // is not needed if all cells in block have same Ms
        for(k=0;k<subcount_z;++k) {
          for(j=0;j<subcount_y;++j) {
            for(i=0;i<subcount_x;++i) {
              OC_REAL8m Msfval = Ms_fine[fmesh->Index(ioff+i,joff+j,koff+k)];
              sum += Msfval;
              if(fabs(Msfval-corrcheckval)
                 >16*OC_REAL8_EPSILON*(fabs(Msfval)+fabs(corrcheckval))) {
                ncorr = 1;
              }
            }
          }
        }
        OC_INDEX cindex = cmesh->Index(ic,jc,kc);
        if(ncorr) {
          cell_needs_correction[cindex] = 1;
          OC_REAL8m val = sum * cellscale;
          Ms_coarse[cindex] = val;
        } else {
          cell_needs_correction[cindex] = 0;
          Ms_coarse[cindex] = corrcheckval;
        }
      }
    }
  }

#ifdef DEBUG_PRINT
  PrintMeshValue_Real("Ms_coarse"," %4.2f",cmesh,Ms_coarse);
  PrintMeshValue_int("cell_needs_correction"," %4d",cmesh,
                     cell_needs_correction);
#endif // DEBUG_PRINT

  // Create appropriately sized working meshes
  Oxs_ThreeVector coarse_cellsize;
  coarse_cellsize.x = cmesh->EdgeLengthX();
  coarse_cellsize.y = cmesh->EdgeLengthY();
  coarse_cellsize.z = cmesh->EdgeLengthZ();

  Oxs_Box range;
  range.Set(0,coarse_cellsize.x*supercount_x,
            0,coarse_cellsize.y*supercount_y,
            0,coarse_cellsize.z*supercount_z);


  Oxs_OwnedPointer<Oxs_RectangularMesh> cwmesh;
  cwmesh.SetAsOwner(new Oxs_RectangularMesh("EdgeCorrMeshCoarse",director,
                                            "",coarse_cellsize,range));
  Oxs_OwnedPointer<Oxs_RectangularMesh> fwmesh;
  cwmesh->MakeRefinedMesh("EdgeCorrMeshFine",
                          subcount_x,subcount_y,subcount_z,fwmesh);
  OC_INDEX fwxdim  = fwmesh->DimX();
  OC_INDEX fwxydim = fwmesh->DimY()*fwxdim;

  // Working Ms arrays, coarse and fine.
  Oxs_MeshValue<OC_REAL8m> cwMs;  cwMs.AdjustSize(cwmesh.GetPtr());
  Oxs_MeshValue<OC_REAL8m> fwMs;  fwMs.AdjustSize(fwmesh.GetPtr());

  // Uniformly aligned spin arrays, for convenience use with
  // state spin arrays.
  Oxs_MeshValue<ThreeVector> cwspin_x;  cwspin_x.AdjustSize(cwmesh.GetPtr());
  Oxs_MeshValue<ThreeVector> cwspin_y;  cwspin_y.AdjustSize(cwmesh.GetPtr());
  Oxs_MeshValue<ThreeVector> cwspin_z;  cwspin_z.AdjustSize(cwmesh.GetPtr());
  OC_INDEX cwsize = cwmesh->Size();
  for(i=0;i<cwsize;++i) {
    cwspin_x[i].Set(1.,0.,0.);
    cwspin_y[i].Set(0.,1.,0.);
    cwspin_z[i].Set(0.,0.,1.);
  }
  Oxs_MeshValue<ThreeVector> fwspin_x;  fwspin_x.AdjustSize(fwmesh.GetPtr());
  Oxs_MeshValue<ThreeVector> fwspin_y;  fwspin_y.AdjustSize(fwmesh.GetPtr());
  Oxs_MeshValue<ThreeVector> fwspin_z;  fwspin_z.AdjustSize(fwmesh.GetPtr());
  OC_INDEX fwsize = fwmesh->Size();
  for(i=0;i<fwsize;++i) {
    fwspin_x[i].Set(1.,0.,0.);
    fwspin_y[i].Set(0.,1.,0.);
    fwspin_z[i].Set(0.,0.,1.);
  }

  // Working states, coarse and fine
  Oxs_SimState cwstate;
  cwstate.mesh = cwmesh.GetPtr();
  cwstate.Ms = &cwMs;
  Oxs_MeshValue<OC_REAL8m> cwenergy;     // Buffer space for oed object in
  Oxs_MeshValue<ThreeVector> cwHfield; // GetEnergyData call
  const OC_INDEX ccenter_windex   // Center index
    = cwmesh->Index(cwmesh->DimX()/2,cwmesh->DimY()/2,cwmesh->DimZ()/2);

  Oxs_SimState fwstate;
  fwstate.mesh = fwmesh.GetPtr();
  fwstate.Ms = &fwMs;
  Oxs_MeshValue<OC_REAL8m> fwenergy;     // Buffer space for oed object in
  Oxs_MeshValue<ThreeVector> fwHfield; // GetEnergyData call
  const OC_INDEX fcenter_windex   // Center index; actually, first cell in
    = fwmesh->Index((cwmesh->DimX()/2)*subcount_x,  /// block corresponding
                    (cwmesh->DimY()/2)*subcount_y,  /// to center block in
                    (cwmesh->DimZ()/2)*subcount_z); /// coarse mesh

  // Indexing through coarse grid, compute and fill Ncorr arrays
  Ncorr_diag.AdjustSize(cmesh);
  Ncorr_offdiag.AdjustSize(cmesh);
  OC_INDEX total_coarse_size = cmesh->Size();
  for(OC_INDEX cindex=0;cindex<total_coarse_size;++cindex) {
    //   Does coarse grid index need correction?

    if(Ms_coarse[cindex]==0.0) {
      Ncorr_diag[cindex].x
        = Ncorr_diag[cindex].y
        = Ncorr_diag[cindex].z
        = Ncorr_offdiag[cindex].x
        = Ncorr_offdiag[cindex].y
        = Ncorr_offdiag[cindex].z
        = 0.0;
      continue;
    }

    ThreeVector Hdiffx,Hdiffy,Hdiffz;

    // Yes.  Fill cwMs and fwMs
    // with appropriate subrange from Ms_fine and Ms_coarse
    OC_INDEX kcoarse = cindex/cxydim;
    OC_INDEX jcoarse = (cindex - kcoarse*cxydim)/cxdim;
    OC_INDEX icoarse = cindex - kcoarse*cxydim - jcoarse*cxdim;

    OC_INDEX kfine = kcoarse * subcount_z;
    OC_INDEX jfine = jcoarse * subcount_y;
    OC_INDEX ifine = icoarse * subcount_x;

    // Fill coarse submesh arrays
    OC_INDEX iw,jw,kw,windex;
    OC_INDEX kmin = -supercount_z/2;
    OC_INDEX kmax = kmin + supercount_z;
    OC_INDEX jmin = -supercount_y/2;
    OC_INDEX jmax = jmin + supercount_y;
    OC_INDEX imin = -supercount_x/2;
    OC_INDEX imax = imin + supercount_x;
    windex = 0;
    int docorr = 0;
    for(kw = kmin; kw<kmax; ++kw) {
      k = kcoarse + kw;
      for(jw = jmin; jw<jmax; ++jw) {
        j = jcoarse + jw;
        for(iw= imin; iw<imax; ++iw) {
          i = icoarse + iw;
          if(i<0 || i>=cxdim || j<0 || j>=cydim || k<0 || k>=czdim) {
            cwMs[windex] = 0.0;
          } else {
            OC_INDEX tindex = i+j*cxdim+k*cxydim;
            cwMs[windex] = Ms_coarse[tindex];
            if(cell_needs_correction[tindex]) docorr = 1;
          }
          ++windex;
        }
      }
    }
    if(!docorr) {
      // All coarse cells in working neighborhood are full;
      // set anisotropy correction to 0
      Ncorr_diag[cindex].x
        = Ncorr_diag[cindex].y
        = Ncorr_diag[cindex].z
        = Ncorr_offdiag[cindex].x
        = Ncorr_offdiag[cindex].y
        = Ncorr_offdiag[cindex].z
        = 0.0;
      continue;
    }

    // Fill fine submesh arrays
    kmin = -(supercount_z/2)*subcount_z;
    kmax = kmin + supercount_z*subcount_z;
    jmin = -(supercount_y/2)*subcount_y;
    jmax = jmin + supercount_y*subcount_y;
    imin = -(supercount_x/2)*subcount_x;
    imax = imin + supercount_x*subcount_x;
    windex = 0;
    for(kw = kmin; kw<kmax; ++kw) {
      k = kfine + kw;
      for(jw = jmin; jw<jmax; ++jw) {
        j = jfine + jw;
        for(iw= imin; iw<imax; ++iw) {
          i = ifine + iw;
          if(i<0 || i>=fxdim || j<0 || j>=fydim || k<0 || k>=fzdim) {
            fwMs[windex] = 0.0;
          } else {
            OC_REAL8m tMs = Ms_fine[i+j*fxdim+k*fxydim];
            fwMs[windex] = tMs;
          }
          ++windex;
        }
      }
    }

#ifdef DEBUG_PRINT
    printf("+++ Coarse mesh index (%2d,%2d,%2d) +++\n",icoarse,jcoarse,kcoarse);
    PrintMeshValue_Real("cwMs"," %7.2f",cwmesh.GetPtr(),cwMs);
    PrintMeshValue_Real("fwMs"," %7.2f",fwmesh.GetPtr(),fwMs);
#endif // DEBUG_PRINT


    //     Fill (swap) cwstate and fwstate spin in x, y, and z directions
    //       Call coarse demag to compute H with current spin direction
    //       Call fine   demag to compute H with current spin direction
    //       Determine correction for coarse grid index

    // spins in x-direction
    {
      cwstate.spin.Swap(cwspin_x);  // Spins in work states point in
      fwstate.spin.Swap(fwspin_x);  // x direction

      Oxs_EnergyData cwoed(cwstate);
      cwoed.energy_buffer = &cwenergy;
      cwoed.field_buffer  = &cwHfield;

      Oxs_EnergyData fwoed(fwstate);
      fwoed.energy_buffer = &fwenergy;
      fwoed.field_buffer  = &fwHfield;

      cDemag->GetEnergyData(cwstate,cwoed);
      fDemag->GetEnergyData(fwstate,fwoed);

      cwstate.spin.Swap(cwspin_x);  // Restore x pointing spins back to
      fwstate.spin.Swap(fwspin_x);  // c/fwspin storage.

      // Coarse field is just field at center of coarse mesh
      ThreeVector Hcoarse = (*cwoed.field)[ccenter_windex];

      // Fine field is average field across fine cells coincident
      // with center coarse cell
      ThreeVector Hfine(0.,0.,0.);
      for(k=0;k<subcount_z;++k) {
        for(j=0;j<subcount_y;++j) {
          for(i=0;i<subcount_x;++i) {
            windex = fcenter_windex + k*fwxydim + j*fwxdim + i;
#if 0 && defined(DEBUG_PRINT)
            printf(" windex (%3d,%3d,%3d)->%6d  fwMs=%g, H=(%g,%g,%g)\n",
                   i,j,k,windex,fwMs[windex],
                   (*fwoed.field)[windex].x,(*fwoed.field)[windex].y,
                   (*fwoed.field)[windex].z);
#endif
            Hfine += fwMs[windex] * (*fwoed.field)[windex];
          }
        }
      }
      Hfine *= 1.0/(cwMs[ccenter_windex]*subcount_x*subcount_y*subcount_z);
      Hdiffx = Hfine - Hcoarse;
    }

    // spins in y-direction
    {
      cwstate.spin.Swap(cwspin_y);  // Spins in work states point in
      fwstate.spin.Swap(fwspin_y);  // x direction

      Oxs_EnergyData cwoed(cwstate);
      cwoed.energy_buffer = &cwenergy;
      cwoed.field_buffer  = &cwHfield;

      Oxs_EnergyData fwoed(fwstate);
      fwoed.energy_buffer = &fwenergy;
      fwoed.field_buffer  = &fwHfield;

      cDemag->GetEnergyData(cwstate,cwoed);
      fDemag->GetEnergyData(fwstate,fwoed);

      cwstate.spin.Swap(cwspin_y);  // Restore x pointing spins back to
      fwstate.spin.Swap(fwspin_y);  // c/fwspin storage.

      // Coarse field is just field at center of coarse mesh
      ThreeVector Hcoarse = (*cwoed.field)[ccenter_windex];

      // Fine field is average field across fine cells coincident
      // with center coarse cell
      ThreeVector Hfine(0.,0.,0.);
      for(k=0;k<subcount_z;++k) {
        for(j=0;j<subcount_y;++j) {
          for(i=0;i<subcount_x;++i) {
            windex = fcenter_windex + k*fwxydim + j*fwxdim + i;
            Hfine += fwMs[windex] * (*fwoed.field)[windex];
          }
        }
      }
      Hfine *= 1.0/(cwMs[ccenter_windex]*subcount_x*subcount_y*subcount_z);
      Hdiffy = Hfine - Hcoarse;
    }

    // spins in z-direction
    {
      cwstate.spin.Swap(cwspin_z);  // Spins in work states point in
      fwstate.spin.Swap(fwspin_z);  // x direction

      Oxs_EnergyData cwoed(cwstate);
      cwoed.energy_buffer = &cwenergy;
      cwoed.field_buffer  = &cwHfield;

      Oxs_EnergyData fwoed(fwstate);
      fwoed.energy_buffer = &fwenergy;
      fwoed.field_buffer  = &fwHfield;

      cDemag->GetEnergyData(cwstate,cwoed);
      fDemag->GetEnergyData(fwstate,fwoed);

      cwstate.spin.Swap(cwspin_z);  // Restore x pointing spins back to
      fwstate.spin.Swap(fwspin_z);  // c/fwspin storage.

      // Coarse field is just field at center of coarse mesh
      ThreeVector Hcoarse = (*cwoed.field)[ccenter_windex];

      // Fine field is average field across fine cells coincident
      // with center coarse cell
      ThreeVector Hfine(0.,0.,0.);
      for(k=0;k<subcount_z;++k) {
        for(j=0;j<subcount_y;++j) {
          for(i=0;i<subcount_x;++i) {
            windex = fcenter_windex + k*fwxydim + j*fwxdim + i;
            Hfine += fwMs[windex] * (*fwoed.field)[windex];
          }
        }
      }
      Hfine *= 1.0/(cwMs[ccenter_windex]*subcount_x*subcount_y*subcount_z);
      Hdiffz = Hfine - Hcoarse;
    }

#ifdef DEBUG_PRINT
    printf("Coarse index (%2d,%2d,%2d) ---\n",icoarse,jcoarse,kcoarse);
    printf(" Hdiff = %21.15g %21.15g %21.15g\n"
           "         %21.15g %21.15g %21.15g\n"
           "         %21.15g %21.15g %21.15g\n\n",
           Hdiffx.x,Hdiffy.x,Hdiffz.x,
           Hdiffx.y,Hdiffy.y,Hdiffz.y,
           Hdiffx.z,Hdiffy.z,Hdiffz.z);
    fflush(stdout);
#endif // DEBUG_PRINT

    // Use H data to compute compensatory demag interaction tensor
    OC_REAL8m& Nxx = Ncorr_diag[cindex].x;
    OC_REAL8m& Nyy = Ncorr_diag[cindex].y;
    OC_REAL8m& Nzz = Ncorr_diag[cindex].z;
    OC_REAL8m& Nxy = Ncorr_offdiag[cindex].x;
    OC_REAL8m& Nxz = Ncorr_offdiag[cindex].y;
    OC_REAL8m& Nyz = Ncorr_offdiag[cindex].z;
    Nxx = Hdiffx.x;
    Nyy = Hdiffy.y;
    Nzz = Hdiffz.z;

    // The tensor should be symmetric, e.g., Hdiffx.y should equal
    // Hdiffy.x, but use the average anyway.  The assert checks
    // below require relative error smaller than 1e-8, which sounds
    // like a reasonable value but has no theoretical or empirical
    // reasoning behind it.
    Nxy = 0.5*(Hdiffx.y+Hdiffy.x);
    assert(fabs(Hdiffx.y-Hdiffy.x) <= 1e-8*fabs(Nxy));
    Nxz = 0.5*(Hdiffx.z+Hdiffz.x);
    assert(fabs(Hdiffx.z-Hdiffz.x) <= 1e-8*fabs(Nxz));
    Nyz = 0.5*(Hdiffy.z+Hdiffz.y);
    assert(fabs(Hdiffy.z-Hdiffz.y) <= 1e-8*fabs(Nyz));
  }

}

// Constructor
Oxs_EdgeCorrAnisotropy::Oxs_EdgeCorrAnisotropy(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr)
{
  // Process arguments

  // Create full coarse mesh, and set read lock
  OXS_GET_INIT_EXT_OBJECT("mesh",Oxs_RectangularMesh,coarse_mesh);
  coarse_mesh_key.Set(coarse_mesh.GetPtr());  // Sets a dep lock
  const Oxs_RectangularMesh& cmesh = coarse_mesh_key.GetReadReference();

  // Program logic for setting Ms_coarse and Ncorr values:
  //   The Ms_coarse, Ncorr_diag and Ncorr_offdiag Oxs_MeshValue
  //   arrays can be set either from a file ("readN" init value) or by
  //   computation ("supercount" et al. init values).  If "readN" is
  //   given and "supercount" is not, then the Ms and Ncorrs will be
  //   filled from file, with an exception thrown on error.  If both
  //   "readN" and "supercount" are given, then Ncorrs will be filled
  //   from file if possible, but if the readN file cannot be opened,
  //   then Ncorrs will be computed instead, without any warning
  //   message.  If "readN" is not given but "supercount" is, then
  //   Ms and Ncorrs will be computed.  It is a fatal error if neither
  //   "readN" nor "supercount" are specified in the MIF file.
  //
  //   The "saveN" init value is ignored if Ms and Ncorrs were filled
  //   from file.  It is not considered an error to specify "saveN"
  //   w/o also specifying "supercount", even though in that case
  //   "saveN" is superfluous.
  //
  //   If Ms and Ncorr are read from file, then the "Ms",
  //   "supercount", "subcount", and "demag" init values are ignored.

  // Input Ncorr values from a file?
  OC_BOOL Ncorr_set_from_file = 0;
  String readN = GetStringInitValue("readN","");
  if(readN.length()>0) {
    Nb_FileChannel channel;
    String Nfilename = readN;
    if(Nfilename.find('.') == String::npos) Nfilename += String(".ovf");
    OC_BOOL open_success = 1;
    try {
      channel.Open(Nfilename.c_str(),"r");
    } catch(...) {
      // Can't open file.  Silently ignore and fall back to direct
      // computation.
      open_success = 0;
    }
    if(open_success) {
      Vf_Ovf20FileHeader fileheader;
      fileheader.ReadHeader(channel);

      // Check that input file is size compatible with current problem
      if(fileheader.meshtype.Value() != vf_ovf20mesh_rectangular) {
        String errmsg = String("Read edge correction data file \"")
          + Nfilename + String("\" is not of rectangular mesh type.");
        throw Oxs_ExtError(this,errmsg);
      }
      if(fileheader.xnodes.Value() != cmesh.DimX() ||
         fileheader.ynodes.Value() != cmesh.DimY() ||
         fileheader.znodes.Value() != cmesh.DimZ()) {
        String errmsg
          = String("Dimensions of read edge correction data file \"")
          + Nfilename + String("\" do not match problem dimensions.");
        throw Oxs_ExtError(this,errmsg);
      }
      if(fileheader.valuedim.Value() != 7) {
        const char* errfmt = "Read edge correction data file \"%s\" has"
          " wrong value dimension (should be 7, not %d).";
        Oc_AutoBuf errbuf(strlen(errfmt)+Nfilename.size()+256);
        Oc_Snprintf(errbuf,errbuf.GetLength(),errfmt,Nfilename.c_str(),
                    fileheader.valuedim.Value());
        throw Oxs_ExtError(this,errbuf);
      }

      // Size and fill import data arrays
      Ms_coarse.AdjustSize(&cmesh);
      Ncorr_diag.AdjustSize(&cmesh);
      Ncorr_offdiag.AdjustSize(&cmesh);
      vector<Vf_Ovf20VecArray> data_array;
      data_array.push_back(Vf_Ovf20VecArray(3,Ncorr_diag.Size(),
              reinterpret_cast<OC_REAL8m*>(Ncorr_diag.GetPtr())));
      data_array.push_back(Vf_Ovf20VecArray(3,Ncorr_offdiag.Size(),
              reinterpret_cast<OC_REAL8m*>(Ncorr_offdiag.GetPtr())));
      data_array.push_back(Vf_Ovf20VecArray(1,Ms_coarse.Size(),
                                            Ms_coarse.GetPtr()));
      fileheader.ReadData(channel,data_array);
      channel.Close();
      Ncorr_set_from_file = 1;
    }
  }

  OC_INDEX supercount_x=0, supercount_y=0, supercount_z=0;
  OC_INDEX subcount_x=0,   subcount_y=0,   subcount_z=0;
  if(Ncorr_set_from_file) {
    DeleteInitValue("Ms");
    DeleteInitValue("supercount");
    DeleteInitValue("subcount");
    DeleteInitValue("demag");
  } else {
    // Fill Ms_coarse, Ncorr_diag, and Ncorr_offdiag

    // Scalar field used for filling fine mesh
    Oxs_OwnedPointer<Oxs_ScalarField> Ms_fine_init;
    OXS_GET_INIT_EXT_OBJECT("Ms",Oxs_ScalarField,Ms_fine_init);

    ThreeVector vsup = GetThreeVectorInitValue("supercount");
    if(vsup.x != floor(vsup.x) || vsup.x<1.0 ||
       vsup.y != floor(vsup.y) || vsup.y<1.0 ||
       vsup.z != floor(vsup.z) || vsup.z<1.0) {
      throw Oxs_ExtError(this,"Inputs to supercount must be"
                           " positive integers.");
    }
    supercount_x = static_cast<OC_INDEX>(floor(vsup.x));
    supercount_y = static_cast<OC_INDEX>(floor(vsup.y));
    supercount_z = static_cast<OC_INDEX>(floor(vsup.z));

    ThreeVector vsub = GetThreeVectorInitValue("subcount");
    if(vsub.x != floor(vsub.x) || vsub.x<1.0 ||
       vsub.y != floor(vsub.y) || vsub.y<1.0 ||
       vsub.z != floor(vsub.z) || vsub.z<1.0) {
      throw Oxs_ExtError(this,"Inputs to subcount must be"
                         " positive integers.");
    }
    subcount_x = static_cast<OC_INDEX>(floor(vsub.x));
    subcount_y = static_cast<OC_INDEX>(floor(vsub.y));
    subcount_z = static_cast<OC_INDEX>(floor(vsub.z));

    // Create two demag objects, one for coarse working mesh
    // and one for fine working mesh.
    String demag_init = GetStringInitValue("demag","");
    Oxs_OwnedPointer<Oxs_Energy> cDemag;
    {
      vector<String> demag_spec;
      Nb_SplitList demag_pieces;
      String coarse_demag_init = demag_init;
      if(coarse_demag_init.size()==0) {
        coarse_demag_init = String("Oxs_Demag:ECA_Coarse {}");
      }
      demag_pieces.Split(coarse_demag_init);
      demag_pieces.FillParams(demag_spec);
      OXS_GET_EXT_OBJECT(demag_spec,Oxs_Energy,cDemag);
    }
    Oxs_OwnedPointer<Oxs_Energy> fDemag;
    {
      vector<String> demag_spec;
      Nb_SplitList demag_pieces;
      String fine_demag_init = demag_init;
      if(fine_demag_init.size()==0) {
        fine_demag_init = String("Oxs_Demag:ECA_Fine {}");
      }
      demag_pieces.Split(fine_demag_init);
      demag_pieces.FillParams(demag_spec);
      OXS_GET_EXT_OBJECT(demag_spec,Oxs_Energy,fDemag);
    }
    ComputeAnisotropyCorrections(director,
                                 subcount_x,subcount_y,subcount_z,
                                 supercount_x,supercount_y,supercount_z,
                                 cDemag.GetPtr(),fDemag.GetPtr(),
                                 coarse_mesh_key.GetPtr(),
                                 Ms_fine_init.GetPtr(),
                                 Ms_coarse,Ncorr_diag,Ncorr_offdiag);
  }
  String saveN = GetStringInitValue("saveN","");
  if(saveN.length()>0 && !Ncorr_set_from_file) {
    Vf_Ovf20FileHeader fileheader;
    coarse_mesh_key.GetPtr()->DumpGeometry(fileheader,
                                           vf_ovf20mesh_rectangular);
    Oc_AutoBuf title(1000);
    Oc_Snprintf(title.GetStr(),title.GetLength(),
                "Oxs_EdgeCorrAnisotropy initialization data:"
                " %d x %d x %d / %d x %d x %d",
                (int)supercount_x,(int)supercount_y,(int)supercount_z,
                (int)subcount_x,(int)subcount_y,(int)subcount_z);
    fileheader.title.Set(title.GetStr());
    fileheader.valuedim.Set(7);  // 7 component field
    fileheader.valuelabels.SetFromString("Nxx Nyy Nzz Nxy Nxz Nyz Ms");
    fileheader.valueunits.SetFromString("{} {} {} {} {} {} A/m");
    fileheader.desc.Set(String("Oxs_EdgeCorrAnisotropy adjustment"
                               " anisotropy coefficients:"
                               " Nxx, Nyy, Nzz, Nxy, Nxz, Nyz, Ms"));
    fileheader.ovfversion = vf_ovf_latest;
    String errors;
    if(!fileheader.IsValid(errors)) {
      errors = String("Oxs_EdgeCorrAnisotropy constructor"
                      " failed to create a valid OVF fileheader: ")
        + errors;
      OXS_THROW(Oxs_ProgramLogicError,errors);
    }

    vector<Vf_Ovf20VecArrayConst> data_array;
    data_array.push_back(Vf_Ovf20VecArrayConst(3,Ncorr_diag.Size(),
          reinterpret_cast<const OC_REAL8m*>(Ncorr_diag.GetPtr())));
    data_array.push_back(Vf_Ovf20VecArrayConst(3,Ncorr_offdiag.Size(),
          reinterpret_cast<const OC_REAL8m*>(Ncorr_offdiag.GetPtr())));
    data_array.push_back(Vf_Ovf20VecArrayConst(1,Ms_coarse.Size(),
                                               Ms_coarse.GetPtr()));

#if 0 // DEBUG
    {
      OC_INDEX check_index = cmesh.Index(16,67,0);
      ThreeVector check_value = Ncorr_diag[check_index];
      fprintf(stderr,"Check (%2d,%2d,%2d): (%25.17g, %25.17g, %25.17g)",
              16,67,0,check_value.x,check_value.y,check_value.z);
      check_value = Ncorr_offdiag[check_index];
      fprintf(stderr," (%25.17g, %25.17g, %25.17g)\n",
              check_value.x,check_value.y,check_value.z);

      check_index = cmesh.Index(21,59,0);
      check_value = Ncorr_diag[check_index];
      fprintf(stderr,"Check (%2d,%2d,%2d): (%25.17g, %25.17g, %25.17g)",
              21,59,0,check_value.x,check_value.y,check_value.z);
      check_value = Ncorr_offdiag[check_index];
      fprintf(stderr," (%25.17g, %25.17g, %25.17g)\n",
              check_value.x,check_value.y,check_value.z);
    }
#endif // DEBUG

    String Nfilename = saveN;
    if(Nfilename.find('.') == String::npos) Nfilename += String(".ovf");
    Nb_FileChannel channel(Nfilename.c_str(),"w");
    fileheader.WriteHeader(channel);
    fileheader.WriteData(channel,vf_obin8,0,0,data_array);
    channel.Close();
  }

  // Create Oxs_ScalarField object to hold Ms values on coarse mesh.
  // This is then passed off to the director to hold.
  String cMs_name = String("Oxs_ArrayScalarField:")
    + GetStringInitValue("coarsename",String("coarseMs"));
  Oxs_Box bbox;
  cmesh.GetBoundingBox(bbox);
  Oxs_ArrayScalarField* cMs
    = new Oxs_ArrayScalarField(cMs_name.c_str(),director,"");
  Oxs_OwnedPointer<Oxs_Ext> cMs_holder;
  cMs_holder.SetAsOwner(static_cast<Oxs_Ext*>(cMs)); // Handles clean-up
  const OC_INDEX imax_coarse = cmesh.DimX();
  const OC_INDEX jmax_coarse = cmesh.DimY();
  const OC_INDEX kmax_coarse = cmesh.DimZ();
  cMs->InitArray(imax_coarse,jmax_coarse,kmax_coarse,bbox);
  for(OC_INDEX k=0;k<kmax_coarse;++k) {
    for(OC_INDEX j=0;j<jmax_coarse;++j) {
      for(OC_INDEX i=0;i<imax_coarse;++i) {
        cMs->SetArrayElt(i,j,k,Ms_coarse[cmesh.Index(i,j,k)]);
      }
    }
  }
  director->StoreExtObject(cMs_holder); // Transfer ownership to director.
  /// This also puts cMs on the director's ExtObject list.

  VerifyAllInitArgsUsed();
}

OC_BOOL Oxs_EdgeCorrAnisotropy::Init()
{
  return Oxs_Energy::Init();
}

void Oxs_EdgeCorrAnisotropy::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  OC_INDEX size = state.mesh->Size();
  if(coarse_mesh->Id() != state.mesh->Id()) {
    // State mesh must be the coarse mesh used during object construction.
    String msg = String("State mesh from driver is not the same"
                        " mesh used to initialize object ");
    msg += String(InstanceName());
    msg += String(".");
    throw Oxs_ExtError(this,msg);
  }

  for(OC_INDEX i=0;i<size;++i) {
    OC_REAL8m Msat = Ms[i];
    if(Msat==0.0) {
      energy[i]=0.0;
      field[i].Set(0.,0.,0.);
      continue;
    }
    // N correction kludge. N has -1*Msat built-in.
    // See also notes in edgecorranisotropy.h.
    const OC_REAL8m& Nxx = Ncorr_diag[i].x;
    const OC_REAL8m& Nyy = Ncorr_diag[i].y;
    const OC_REAL8m& Nzz = Ncorr_diag[i].z;
    const OC_REAL8m& Nxy = Ncorr_offdiag[i].x;
    const OC_REAL8m& Nxz = Ncorr_offdiag[i].y;
    const OC_REAL8m& Nyz = Ncorr_offdiag[i].z;

    const ThreeVector& m = spin[i];

    field[i].x = (Nxx*m.x + Nxy*m.y + Nxz*m.z);
    field[i].y = (Nxy*m.x + Nyy*m.y + Nyz*m.z);
    field[i].z = (Nxz*m.x + Nyz*m.y + Nzz*m.z);

    energy[i] = -0.5*MU0*Msat*(field[i]*m);
  }
}

// Optional interface for conjugate-gradient evolver.
// For details, see NOTES VI, 28-July-2011, p 14.
OC_INT4m
Oxs_EdgeCorrAnisotropy::IncrementPreconditioner(PreconditionerData& pcd)
{
  if(!pcd.state || !pcd.val) {
    throw Oxs_ExtError(this,
         "Import to IncrementPreconditioner not properly initialized.");
  }

  const Oxs_SimState& state = *(pcd.state);
  const OC_INDEX size = state.mesh->Size();

  Oxs_MeshValue<ThreeVector>& val = *(pcd.val);
  if(val.Size() != size) {
    throw Oxs_ExtError(this,
         "Import to IncrementPreconditioner not properly initialized.");
  }

  if(pcd.type != DIAGONAL) return 0; // Unsupported preconditioning type

  if(size<1) return 1; // Nothing to do

  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  for(OC_INDEX i=0;i<size;++i) {
    if(Ms[i] == 0.0) continue; // No correction for cells with zero Ms
    const OC_REAL8m& Nxx = Ncorr_diag[i].x;
    const OC_REAL8m& Nyy = Ncorr_diag[i].y;
    const OC_REAL8m& Nzz = Ncorr_diag[i].z;
    OC_REAL8m maxval = Nxx;      // Compute shift necessary to
    if(Nyy>maxval) maxval = Nyy; // guarantee each element is
    if(Nzz>maxval) maxval = Nzz; // non-negative.
    if(maxval<=0.0) {
      val[i].x += -1*MU0*Nxx;
      val[i].y += -1*MU0*Nyy;
      val[i].z += -1*MU0*Nzz;
    } else {
      val[i].x += MU0*(maxval-Nxx);
      val[i].y += MU0*(maxval-Nyy);
      val[i].z += MU0*(maxval-Nzz);
    }
    /// If the above correction is unstable, might want to try
    /// increasing the offset.
  }

  return 1;
}
