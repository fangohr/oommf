/* FILE: uniformvectorfield.cc     -*-Mode: c++-*-
 *
 * Uniform vector field object, derived from Oxs_VectorField class.
 *
 */

#include "oc.h"
#include "nb.h"

#include "uniformvectorfield.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_UniformVectorField);

/* End includes */


// Constructor
Oxs_UniformVectorField::Oxs_UniformVectorField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_VectorField(name,newdtr,argstr)
{
  // Process arguments
  vec = GetThreeVectorInitValue("vector");
  if(HasInitValue("norm")) {
    vec.SetMag(GetRealInitValue("norm"));
  }
  VerifyAllInitArgsUsed();
}

void
Oxs_UniformVectorField::Value
(const ThreeVector& /* pt */,
 ThreeVector& value) const
{
  value = vec;
}

void
Oxs_UniformVectorField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<ThreeVector>& array) const
{
  array.AdjustSize(mesh);
  OC_INDEX i,size=mesh->Size();
  for(i=0;i<size;i++) array[i]=vec;
}
