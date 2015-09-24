/* FILE: linearscalarfield.cc     -*-Mode: c++-*-
 *
 * Linear scalar field, derived from Oxs_ScalarField class.
 *
 */

#include "oc.h"
#include "nb.h"

#include "linearscalarfield.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_LinearScalarField);

/* End includes */


// Constructor
Oxs_LinearScalarField::Oxs_LinearScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr)
{
  // Process arguments
  off = GetRealInitValue("offset",0.0);
  vec = GetThreeVectorInitValue("vector");
  if(HasInitValue("norm")) {
    vec.SetMag(GetRealInitValue("norm"));
  }
  VerifyAllInitArgsUsed();
}

OC_REAL8m
Oxs_LinearScalarField::Value
(const ThreeVector& pt) const
{
  return off+(vec*pt);
}

void
Oxs_LinearScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultFillMeshValue(mesh,array);
}

void
Oxs_LinearScalarField::IncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultIncrMeshValue(mesh,array);
}

void
Oxs_LinearScalarField::MultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultMultMeshValue(mesh,array);
}

