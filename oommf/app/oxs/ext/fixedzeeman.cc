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
  : Oxs_Energy(name,newdtr,argstr), mesh_id(0)
{
  // Process arguments
  field_mult = GetRealInitValue("multiplier",1.0);
  OXS_GET_INIT_EXT_OBJECT("field",Oxs_VectorField,fixedfield_init);
  VerifyAllInitArgsUsed();
}

OC_BOOL Oxs_FixedZeeman::Init()
{
  mesh_id = 0;
  fixedfield.Release();
  return Oxs_Energy::Init();
}

void Oxs_FixedZeeman::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  OC_INDEX size = state.mesh->Size();
  if(size<1) return; // Nothing to do
  if(mesh_id != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id = 0;
    fixedfield_init->FillMeshValue(state.mesh,fixedfield);
    if(field_mult!=1.0) {
      for(OC_INDEX i=0;i<size;i++) fixedfield[i] *= field_mult;
    }
    mesh_id = state.mesh->Id();
  }

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  OC_INDEX i=0;
  do {
    field[i] = fixedfield[i];
    energy[i] = -MU0*Ms[i]*(fixedfield[i]*spin[i]);
    ++i;
  } while(i<size);
}

// Optional interface for conjugate-gradient evolver.
// For details on this code, see NOTES VI, 21-July-2011, pp 10-11.
OC_INT4m
Oxs_FixedZeeman::IncrementPreconditioner(PreconditionerData& /* pcd */)
{
  // Nothing to do --- bilinear part of this term is 0.
  return 1;
}
