/* FILE: vectorfield.h      -*-Mode: c++-*-
 *
 * Abstract Oxs_VectorField class, derives from Oxs_Ext.
 *
 */

#ifndef _OXS_VECTORFIELD
#define _OXS_VECTORFIELD

#include "oc.h"

#include "ext.h"
#include "director.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "threevector.h"

/* End includes */

////////////////////////////////////////////////////////////////////////
// Oxs_VectorField (generic Oxs_Ext vector field) class
class Oxs_VectorField : public Oxs_Ext {
protected:
  // Disable copy constructor and assignment operator by
  // declaring them here and never providing a definition.
  Oxs_VectorField(const Oxs_VectorField&);
  Oxs_VectorField& operator=(const Oxs_VectorField&);

  // Child support
  void DefaultFillMeshValue(const Oxs_Mesh* mesh,
			    Oxs_MeshValue<ThreeVector>& array) const;

  // Constructors
  Oxs_VectorField(const char* name,
		  Oxs_Director* newdtr);
  Oxs_VectorField(const char* name,Oxs_Director* newdtr,
		  const char* argstr);


public:
  virtual ~Oxs_VectorField() {}

  virtual void Value(const ThreeVector& pt,ThreeVector& value) const =0;

  virtual void
  FillMeshValue(const Oxs_Mesh* mesh,
		Oxs_MeshValue<ThreeVector>& array) const =0;

  // FillMeshValueOrthogonal is analogous to FillMeshValue, except that
  // the export array should be filled with values orthogonal to the
  // orth_array import.  This is just a convenience function that calls
  // the virtual FillMeshValue member, and then checks that the
  // orthogonality relation holds at each point.  An exception will be
  // thrown if the orthogonality requirement is not met.
  void FillMeshValueOrthogonal(const Oxs_Mesh* mesh,
                               const Oxs_MeshValue<ThreeVector>& orth_array,
                               Oxs_MeshValue<ThreeVector>& array) const;

};


#endif // _OXS_VECTORFIELD
