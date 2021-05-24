/* FILE: fixedzeeman.cc           -*-Mode: c++-*-
 *
 * Fixed (in time) Zeeman energy/field, derived from Oxs_Energy class.
 *
 */

#include "oc.h"
#include "nb.h"
#include "threevector.h"
#include "energy.h"
#include "simstate.h"
#include "mesh.h"
#include "meshvalue.h"
#include "vectorfield.h"
#include "fixedzeeman.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_FixedZeeman);

/* End includes */


// Constructor
Oxs_FixedZeeman::Oxs_FixedZeeman(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ChunkEnergy(name,newdtr,argstr), mesh_id(0),
    energy_density_error_estimate(-1.0)
{
  // Process arguments
  field_mult = GetRealInitValue("multiplier",1.0);
  OXS_GET_INIT_EXT_OBJECT("field",Oxs_VectorField,fixedfield_init);
  VerifyAllInitArgsUsed();
}

OC_BOOL Oxs_FixedZeeman::Init()
{
  mesh_id = 0;
  energy_density_error_estimate = -1.0;
  fixedfield.Release();
  return Oxs_Energy::Init();
}

void Oxs_FixedZeeman::ComputeEnergyChunkInitialize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& /* thread_ocedtaux */,
 int /* number_of_threads */) const
{
  if(mesh_id != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id = 0;
    fixedfield_init->FillMeshValue(state.mesh,fixedfield);
    if(field_mult!=1.0) {
      const OC_INDEX size = fixedfield.Size();
      for(OC_INDEX i=0;i<size;i++) fixedfield[i] *= field_mult;
    }
    mesh_id = state.mesh->Id();

    // Energy density error estimate
    const OC_INDEX size = fixedfield.Size();
    OC_REAL8m max_factorsq = 0.0;
    const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
    for(OC_INDEX i=0;i<size;i++) {
      OC_REAL8m test = fixedfield[i].MagSq()*Ms[i]*Ms[i];
      if(test>max_factorsq) max_factorsq = test;
    }
    energy_density_error_estimate
      = 4*OC_REAL8m_EPSILON*MU0*sqrt(max_factorsq);
  }
  ocedt.energy_density_error_estimate
    = energy_density_error_estimate;
}


void Oxs_FixedZeeman::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int /* threadnumber */
 ) const
{
  if(node_start>=node_stop) return; // Nothing to do

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m cell_volume;
  if(state.mesh->HasUniformCellVolumes(cell_volume)) {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      Oxs_ThreeVector& hi = fixedfield[i];

      OC_REAL8m ei = -MU0*Ms[i]*(hi*spin[i]);;
      energy_sum += ei;

      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)       (*ocedt.H)[i] = hi;
      if(ocedt.H_accum) (*ocedt.H_accum)[i] += hi;
      if(ocedt.mxH_accum || ocedt.mxH) {
        Oxs_ThreeVector ti = spin[i] ^ hi;
        if(ocedt.mxH)       (*ocedt.mxH)[i] = ti;
        if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ti;
      }
    }
    energy_sum *= cell_volume;
  } else {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      Oxs_ThreeVector& hi = fixedfield[i];

      OC_REAL8m ei = -MU0*Ms[i]*(hi*spin[i]);;
      energy_sum += (ei*state.mesh->Volume(i));

      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)       (*ocedt.H)[i] = hi;
      if(ocedt.H_accum) (*ocedt.H_accum)[i] += hi;
      if(ocedt.mxH_accum || ocedt.mxH) {
        Oxs_ThreeVector ti = spin[i] ^ hi;
        if(ocedt.mxH)       (*ocedt.mxH)[i] = ti;
        if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ti;
      }
    }
  }
  ocedtaux.energy_total_accum += energy_sum;
}


// Optional interface for conjugate-gradient evolver.
// For details on this code, see NOTES VI, 21-July-2011, pp 10-11.
OC_INT4m
Oxs_FixedZeeman::IncrementPreconditioner(PreconditionerData& /* pcd */)
{
  // Nothing to do --- bilinear part of this term is 0.
  return 1;
}
