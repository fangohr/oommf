/* FILE: edgecorranisotropy.h            -*-Mode: c++-*-
 *
 * Edge Correctin Anisotropy, derived from Oxs_Energy class.
 *
 */

#ifndef _OXS_EDGECORRANISOTROPY
#define _OXS_EDGECORRANISOTROPY

#include <vector>
#include <string>

#include "nb.h"
#include "threevector.h"
#include "util.h"
#include "energy.h"
#include "key.h"
#include "simstate.h"
#include "mesh.h"
#include "meshvalue.h"
#include "scalarfield.h"
#include "vectorfield.h"

/* End includes */

class Oxs_EdgeCorrAnisotropy
  : public Oxs_Energy, public Oxs_EnergyPreconditionerSupport {
private:
  Oxs_OwnedPointer<Oxs_RectangularMesh> coarse_mesh;
  Oxs_ConstKey<Oxs_RectangularMesh> coarse_mesh_key;

  Oxs_MeshValue<OC_REAL8m> Ms_coarse;  // Saturation magnetization

  // Anisotropy correction for the demag term is handled via the
  // difference between the demag interaction coefficient tensors
  // N_fine_mesh - N_coarse_mesh.  At each point, this tensor is a
  // 3x3 symmetric matrix, with diagonal components Nxx, Nyy, Nzz
  // and off-diagonal components Nxy Nxz Nyz.  This information is
  // stored using a kludge of two ThreeVectors, with components in
  // the just mentioned order.
  // NOTE: The Ncorr values here are actually (Nfine-Ncoarse)*(-1*Ms).
  Oxs_MeshValue<ThreeVector> Ncorr_diag;
  Oxs_MeshValue<ThreeVector> Ncorr_offdiag;

  // Helper function for Ms_coarse and Ncorr_* initialization.
  static void ComputeAnisotropyCorrections
  ( // Imports
   Oxs_Director* director,  // App director
   OC_INDEX subcount_x,   // Fine cells counts inside one coarse cell
   OC_INDEX subcount_y,
   OC_INDEX subcount_z,
   OC_INDEX supercount_x, // Coarse cell counts used in correction
   OC_INDEX supercount_y,
   OC_INDEX supercount_z,
   Oxs_Energy* cDemag,  // Demag object for coarse working mesh
   Oxs_Energy* fDemag, // (Different) demag object for fine working mesh
   const Oxs_RectangularMesh* cmesh, // Coarse mesh
   const Oxs_ScalarField* Ms_fine_init, // Ms initializer for fine mesh
   // Exports
   Oxs_MeshValue<OC_REAL8m>& Ms_coarse, // Averaged Ms, on coarse grid
   Oxs_MeshValue<ThreeVector>& Ncorr_diag,    // Correction terms; see
   Oxs_MeshValue<ThreeVector>& Ncorr_offdiag  // header file for details.
   );

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
                         Oxs_EnergyData& oed) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_EdgeCorrAnisotropy(const char* name,  // Child instance id
                         Oxs_Director* newdtr, // App director
                         const char* argstr);  // MIF input block parameters
  virtual ~Oxs_EdgeCorrAnisotropy() {}
  virtual OC_BOOL Init();

  // Optional interface for conjugate-gradient evolver.
  virtual int IncrementPreconditioner(PreconditionerData& pcd);
};

#endif // _OXS_EDGECORRANISOTROPY
