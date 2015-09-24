/* FILE: scalarfield.h      -*-Mode: c++-*-
 *
 * Abstract Oxs_ScalarField class, derives from Oxs_Ext.
 *
 */

#ifndef _OXS_SCALARFIELD
#define _OXS_SCALARFIELD

#include "ext.h"
#include "threevector.h"
#include "mesh.h"
#include "meshvalue.h"

/* End includes */

////////////////////////////////////////////////////////////////////////
// Oxs_ScalarField (generic Oxs_Ext scalar field) class
class Oxs_ScalarField : public Oxs_Ext {
protected:
  // Disable copy constructor and assignment operator by
  // declaring them here and never providing a definition.
  Oxs_ScalarField(const Oxs_ScalarField&);
  Oxs_ScalarField& operator=(const Oxs_ScalarField&);

  // Child support
  void DefaultFillMeshValue(const Oxs_Mesh* mesh,
			    Oxs_MeshValue<OC_REAL8m>& array) const;
  void DefaultIncrMeshValue(const Oxs_Mesh* mesh,
			    Oxs_MeshValue<OC_REAL8m>& array) const;
  void DefaultMultMeshValue(const Oxs_Mesh* mesh,
			    Oxs_MeshValue<OC_REAL8m>& array) const;

  // Constructors
  Oxs_ScalarField(const char* name,
		  Oxs_Director* newdtr);
  Oxs_ScalarField(const char* name,Oxs_Director* newdtr,
		  const char* argstr);

public:
  virtual ~Oxs_ScalarField() {}

  virtual OC_REAL8m Value(const ThreeVector& pt) const =0;

  virtual void FillMeshValue(const Oxs_Mesh* mesh,
			     Oxs_MeshValue<OC_REAL8m>& array) const =0;
  virtual void IncrMeshValue(const Oxs_Mesh* mesh,
			     Oxs_MeshValue<OC_REAL8m>& array) const =0;
  virtual void MultMeshValue(const Oxs_Mesh* mesh,
			     Oxs_MeshValue<OC_REAL8m>& array) const =0;

};


#endif // _OXS_SCALARFIELD
