/* FILE: vecmagscalarfield.cc     -*-Mode: c++-*-
 *
 * Scalar field constructed from a vector field.
 * This class is derived from Oxs_ScalarField class.
 *
 */

#include "oc.h"
#include "nb.h"

#include "vecmagscalarfield.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_VecMagScalarField);

/* End includes */


// Constructor
Oxs_VecMagScalarField::Oxs_VecMagScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr)
{
  // Process arguments
  offset = GetRealInitValue("offset",0.0);
  multiplier = GetRealInitValue("multiplier",1.0);
  OXS_GET_INIT_EXT_OBJECT("field",Oxs_VectorField,vecfield);
  VerifyAllInitArgsUsed();
}

OC_REAL8m
Oxs_VecMagScalarField::Value
(const ThreeVector& pt) const
{
  ThreeVector vec;
  vecfield->Value(pt,vec);
  return multiplier*sqrt(vec.MagSq())+offset;
}

void
Oxs_VecMagScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultFillMeshValue(mesh,array);
}

void
Oxs_VecMagScalarField::IncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultIncrMeshValue(mesh,array);
}

void
Oxs_VecMagScalarField::MultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultMultMeshValue(mesh,array);
}

