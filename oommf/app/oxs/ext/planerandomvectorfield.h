/* FILE: planerandomvectorfield.h      -*-Mode: c++-*-
 *
 * Vector field object, random in a specified plane.
 * Derived from Oxs_VectorField class.
 *
 */

#ifndef _OXS_PLANERANDOMVECTORFIELD
#define _OXS_PLANERANDOMVECTORFIELD

#include <map>

#include "oc.h"

#include "threevector.h"
#include "vectorfield.h"

/* End includes */

class Oxs_PlaneRandomVectorField:public Oxs_VectorField {
private:
  OC_REAL8m min_norm;
  OC_REAL8m max_norm;
  OC_REAL8m minsq;
  OC_REAL8m maxsq;
  Oxs_OwnedPointer<Oxs_VectorField> plane_normal_field;

  // Caching
  Oxs_OwnedPointer<Oxs_Mesh> cache_mesh;
  Oxs_MeshValue<ThreeVector> results_cache;
  OC_BOOL use_cache;

  // Optional base field
  Oxs_OwnedPointer<Oxs_VectorField> base_field;

  // Util function
  ThreeVector CreatePlaneVector(ThreeVector plane_normal) const;

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.

  Oxs_PlaneRandomVectorField
  (const char* name,     // Child instance id
   Oxs_Director* newdtr, // App director
   const char* argstr);  // MIF input block parameters

  virtual ~Oxs_PlaneRandomVectorField() {}

  virtual void Value(const ThreeVector& pt,ThreeVector& value) const;

  virtual void FillMeshValue(const Oxs_Mesh* mesh,
			     Oxs_MeshValue<ThreeVector>& array) const;
};


#endif // _OXS_PLANERANDOMVECTORFIELD

