/* FILE: uniformscalarfield.cc     -*-Mode: c++-*-
 *
 * Uniform scalar field object, derived from Oxs_ScalarField
 * class.
 *
 */

#include "oc.h"
#include "nb.h"

#include "uniformscalarfield.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_UniformScalarField);

/* End includes */


// Constructor
Oxs_UniformScalarField::Oxs_UniformScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr)
{
  // Process arguments.
  value = GetRealInitValue("value");
  VerifyAllInitArgsUsed();
}

OC_REAL8m
Oxs_UniformScalarField::Value(const ThreeVector& /* pt */) const
{
  return value;
}

void
Oxs_UniformScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  array.AdjustSize(mesh);
  OC_INDEX i,size=mesh->Size();
  for(i=0;i<size;i++) array[i]=value;
}

void
Oxs_UniformScalarField::IncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  if(array.Size() != mesh->Size()) {
    Oxs_ExtError("Import array not initialized in "
		   "Oxs_UniformScalarField::DefaultIncrMeshValue");
  }
  OC_INDEX i,size=mesh->Size();
  for(i=0;i<size;i++) array[i] += value;
}

void
Oxs_UniformScalarField::MultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  if(array.Size() != mesh->Size()) {
    Oxs_ExtError("Import array not initialized in "
		   "Oxs_UniformScalarField::DefaultMultMeshValue");
  }
  OC_INDEX i,size=mesh->Size();
  for(i=0;i<size;i++) array[i] *= value;
}

