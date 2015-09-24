/* FILE: affinetransformscalarfield.cc      -*-Mode: c++-*-
 *
 * Scalar field object, derived from Oxs_ScalarField class,
 * that applies an affine transformation to the output of
 * another scalar field object.
 *
 */

#include "oc.h"

#include "util.h"
#include "threevector.h"
#include "meshvalue.h"
#include "director.h"
#include "affinetransformscalarfield.h"

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_AffineTransformScalarField);

/* End includes */

// Constructor
Oxs_AffineTransformScalarField::Oxs_AffineTransformScalarField(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ScalarField(name,newdtr,argstr)
{
  // Get scalar field object
  OXS_GET_INIT_EXT_OBJECT("field",Oxs_ScalarField,field);

  // Get offset
  offset = GetRealInitValue("offset",0.0);

  // Get multiplier
  multiplier = GetRealInitValue("multiplier",1.0);

  // Invert?
  OC_BOOL inverse = GetIntInitValue("inverse",0);
  if(inverse) {
    OC_REAL8m test = OC_MAX(1.0,offset);
    if(multiplier<1 && test>=DBL_MAX*multiplier) {
      throw Oxs_ExtError(this,
	 "Unable to invert multiplier; numeric overflow detected.");
    }
    multiplier = 1.0/multiplier;
    offset = -1*offset*multiplier;
  }

  VerifyAllInitArgsUsed();
}

Oxs_AffineTransformScalarField::~Oxs_AffineTransformScalarField()
{}

OC_REAL8m
Oxs_AffineTransformScalarField::Value
(const ThreeVector& pt) const
{
  // Evaluate field at pt
  OC_REAL8m val = field->Value(pt);

  // Apply transform to val
  return multiplier*val+offset;
}

void
Oxs_AffineTransformScalarField::FillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultFillMeshValue(mesh,array);
}

void
Oxs_AffineTransformScalarField::IncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultIncrMeshValue(mesh,array);
}

void
Oxs_AffineTransformScalarField::MultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  DefaultMultMeshValue(mesh,array);
}

