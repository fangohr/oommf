/* FILE: maskvectorfield.cc      -*-Mode: c++-*-
 *
 * Vector field object, derived from Oxs_VectorField class,
 * that applies an affine transformation to the output of
 * another vector field object.
 *
 */

#include "oc.h"

#include "util.h"
#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "maskvectorfield.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_MaskVectorField);

/* End includes */

// Constructor
Oxs_MaskVectorField::Oxs_MaskVectorField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_VectorField(name,newdtr,argstr)
{
  // Get vector field object
  OXS_GET_INIT_EXT_OBJECT("field",Oxs_VectorField,field);

  // Get "mask" scalar field object
  OXS_GET_INIT_EXT_OBJECT("mask",Oxs_ScalarField,mask);

  VerifyAllInitArgsUsed();
}

Oxs_MaskVectorField::~Oxs_MaskVectorField()
{}

void
Oxs_MaskVectorField::Value
(const ThreeVector& pt,ThreeVector& value) const
{
  field->Value(pt,value);
  value *= mask->Value(pt);
}

void 
Oxs_MaskVectorField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<ThreeVector>& array) const
{
  array.AdjustSize(mesh); // Safety
  field->FillMeshValue(mesh,array);
  OC_INDEX size = mesh->Size();
  for(OC_INDEX i=0;i<size;i++) {
    ThreeVector pt;
    mesh->Center(i,pt);
    array[i] *= mask->Value(pt);
  }
}
