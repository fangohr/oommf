/* FILE: maskvectorfield.h      -*-Mode: c++-*-
 *
 * Vector field object, derived from Oxs_VectorField class, that
 * multiplies a vector field pointwise by a scalar vector field (the
 * mask) to produce a new vector field.
 *
 */

#ifndef _OXS_MASKVECTORFIELD
#define _OXS_MASKVECTORFIELD

#include "oc.h"

#include "threevector.h"
#include "util.h"
#include "scalarfield.h"
#include "vectorfield.h"

/* End includes */

class Oxs_MaskVectorField:public Oxs_VectorField {
private:
  Oxs_OwnedPointer<Oxs_VectorField> field;
  Oxs_OwnedPointer<Oxs_ScalarField> mask;
public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.

  Oxs_MaskVectorField
  (const char* name,     // Child instance id
   Oxs_Director* newdtr, // App director
   const char* argstr);  // MIF input block parameters

  virtual ~Oxs_MaskVectorField();

  virtual void Value(const ThreeVector& pt,ThreeVector& value) const;

  virtual void FillMeshValue(const Oxs_Mesh* mesh,
			     Oxs_MeshValue<ThreeVector>& array) const;
};


#endif // _OXS_MASKVECTORFIELD
