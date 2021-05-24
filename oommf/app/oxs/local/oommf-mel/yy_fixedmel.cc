/* FILE: fixedmel.cc           -*-Mode: c++-*-
 *
 * OOMMF magnetoelastic coupling extension module.
 * YY_FixedMEL class.
 * Calculates fixed MEL field/energy for a simulation.
 *
 * Release ver. 1.0.1 (2015-03-03)
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

#include "yy_fixedmel.h"
#include "yy_mel_util.h"

/* End includes */

// Oxs_Ext registration support
OXS_EXT_REGISTER(YY_FixedMEL);

// Constructor
YY_FixedMEL::YY_FixedMEL(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr), mesh_id(0)
{
  // Process arguments
  field_mult = GetRealInitValue("multiplier",1.0);

  // Generate MELCoef initializer
  OXS_GET_INIT_EXT_OBJECT("B1",Oxs_ScalarField,MELCoef1_init);
  OXS_GET_INIT_EXT_OBJECT("B2",Oxs_ScalarField,MELCoef2_init);

  // Select displacement or strain for input
  SelectElasticityInputType();

  if(use_u) {
    OXS_GET_INIT_EXT_OBJECT("u_field",Oxs_VectorField,fixedu_init);
  } else {  // use_e
    OXS_GET_INIT_EXT_OBJECT("e_diag_field",Oxs_VectorField,fixede_diag_init);
    OXS_GET_INIT_EXT_OBJECT("e_offdiag_field",Oxs_VectorField,fixede_offdiag_init);
  }

  VerifyAllInitArgsUsed();
}

OC_BOOL YY_FixedMEL::Init()
{
  mesh_id = 0;
  MELField.Release();
  return Oxs_Energy::Init();
}

void YY_FixedMEL::GetEnergy
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

    MELField.SetMELCoef(state,MELCoef1_init,MELCoef2_init);

    if(use_u) {
      MELField.SetDisplacement(state,fixedu_init);
    } else {  // use_e
      MELField.SetStrain(state,fixede_diag_init,fixede_offdiag_init);
    }

    mesh_id = state.mesh->Id();
  }

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  MELField.CalculateMELField(state, field_mult, field, energy);
  max_field = MELField.GetMaxField();

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
}

void YY_FixedMEL::SelectElasticityInputType()
{
  // Sets several flags for elasticity input.
  // Whether you use displacement or strain
  use_u = HasInitValue("u_field");
  use_e = HasInitValue("e_diag_field") && HasInitValue("e_offdiag_field");

  if( (use_u && use_e) || (!use_u && !use_e)) {
    const char *cptr =
      "Initialization string specifies both displacement"
      " and strain as the elasticity input. They are mutually"
      " exclusive. Select only one. For strain, you need"
      " both diagonal and off-diagonal fields.";
    throw Oxs_ExtError(this,cptr);
  }
}
