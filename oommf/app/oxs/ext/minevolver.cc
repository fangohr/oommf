/* FILE: minevolver.cc                 -*-Mode: c++-*-
 *
 * Abstract minimization evolver class
 *
 */

#include "director.h"
#include "minevolver.h"

#include "energy.h"

/* End includes */

// OOMMF 20201225 API interface. Children are encouraged to use this
// interface rather than accessing Oxs_ComputeEnergies directly, as this
// allows greater interface forward flexibility (which is especially
// important for third party extensions).
void Oxs_MinEvolver::GetEnergies
(const Oxs_ComputeEnergiesImports& ocei,
 Oxs_ComputeEnergiesExports& ocee)
{
  Oxs_ComputeEnergies(ocei,ocee);
}

OC_BOOL Oxs_MinEvolver::Init()
{
  // Advertise "well known quantities" available for attaching to
  // Oxs_SimStates. Attachment is handled in the GetEnergies() routine.
  director->SetWellKnownQuantityLabel
    (Oxs_Director::WellKnownQuantity::total_energy_density,
     DataName("Total energy density"));
  director->SetWellKnownQuantityLabel
    (Oxs_Director::WellKnownQuantity::total_H,
     DataName("Total field"));
  director->SetWellKnownQuantityLabel
    (Oxs_Director::WellKnownQuantity::total_mxH,
     DataName("mxH"));
  director->SetWellKnownQuantityLabel
    (Oxs_Director::WellKnownQuantity::total_mxHxm,
     DataName("mxHxm"));

  return Oxs_Evolver::Init();
}
