/* FILE: cubicanisotropy.h            -*-Mode: c++-*-
 *
 * Cubic Anisotropy, derived from Oxs_Energy class.
 */

#ifndef _OXS_CUBICANISOTROPY
#define _OXS_CUBICANISOTROPY

#include "nb.h"
#include "threevector.h"
#include "chunkenergy.h"
#include "energy.h"
#include "key.h"
#include "simstate.h"
#include "mesh.h"
#include "meshvalue.h"
#include "scalarfield.h"
#include "vectorfield.h"

/* End includes */

class Oxs_CubicAnisotropy
  : public Oxs_ChunkEnergy, public Oxs_EnergyPreconditionerSupport {
private:
  enum AnisotropyCoefType {
    ANIS_UNKNOWN, K1_TYPE, Ha_TYPE
  } aniscoeftype;

  Oxs_OwnedPointer<Oxs_ScalarField> K1_init;
  Oxs_OwnedPointer<Oxs_ScalarField> Ha_init;
  Oxs_OwnedPointer<Oxs_VectorField> axis1_init;
  Oxs_OwnedPointer<Oxs_VectorField> axis2_init;
  mutable OC_UINT4m mesh_id;
  mutable Oxs_MeshValue<OC_REAL8m> K1;
  mutable Oxs_MeshValue<OC_REAL8m> Ha;
  mutable Oxs_MeshValue<ThreeVector> axis1;
  mutable Oxs_MeshValue<ThreeVector> axis2;
  /// K1, Ha, axis1 and axis2 are cached values filled by corresponding
  /// *_init members when a change in mesh is detected.

  mutable OC_REAL8m max_K1;  // Max K1 magnitude. Used for energy
                             // density error estimate.

  // It is not uncommon for the anisotropy to be specified by uniform
  // fields.  In this case, memory traffic can be significantly
  // reduced, which may be helpful in parallelized code.  The
  // variables uniform_K1/axis_value are valid iff the corresponding
  // boolean is true.
  OC_BOOL K1_is_uniform;
  OC_BOOL Ha_is_uniform;
  OC_BOOL axis1_is_uniform;
  OC_BOOL axis2_is_uniform;
  OC_REAL8m uniform_K1_value;
  OC_REAL8m uniform_Ha_value;
  ThreeVector uniform_axis1_value;
  ThreeVector uniform_axis2_value;

protected:
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const {
    GetEnergyAlt(state,oed);
  }

  virtual void ComputeEnergy(const Oxs_SimState& state,
                             Oxs_ComputeEnergyData& oced) const {
    ComputeEnergyAlt(state,oced);
  }

  virtual void ComputeEnergyChunkInitialize
  (const Oxs_SimState& state,
   Oxs_ComputeEnergyDataThreaded& ocedt,
   Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& thread_ocedtaux,
   int number_of_threads) const;

  virtual void ComputeEnergyChunk(const Oxs_SimState& state,
                                  Oxs_ComputeEnergyDataThreaded& ocedt,
                                  Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
                                  OC_INDEX node_start,OC_INDEX node_stop,
                                  int threadnumber) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  Oxs_CubicAnisotropy(const char* name,  // Child instance id
		      Oxs_Director* newdtr, // App director
		      const char* argstr);  // MIF input block parameters

  virtual ~Oxs_CubicAnisotropy() {}
  virtual OC_BOOL Init();

  // Optional interface for conjugate-gradient evolver.
  virtual int IncrementPreconditioner(PreconditionerData& pcd);
};


#endif // _OXS_CUBICANISOTROPY
