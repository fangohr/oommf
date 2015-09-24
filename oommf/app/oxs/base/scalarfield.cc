/* FILE: scalarfield.cc      -*-Mode: c++-*-
 *
 * Abstract Oxs_ScalarField class, derives from Oxs_Ext.
 *
 */

#include "ext.h"
#include "director.h"
#include "scalarfield.h"

/* End includes */

////////////////////////////////////////////////////////////////////////
// Oxs_ScalarField (generic Oxs_Ext scalar field) class

// Constructors
Oxs_ScalarField::Oxs_ScalarField
( const char* name,     // Child instance id
  Oxs_Director* newdtr  // App director
  ) : Oxs_Ext(name,newdtr)
{}

Oxs_ScalarField::Oxs_ScalarField
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Ext(name,newdtr,argstr)
{}

// Child support
void
Oxs_ScalarField::DefaultFillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  array.AdjustSize(mesh);
  OC_INDEX i,size=mesh->Size();
  for(i=0;i<size;i++) {
    ThreeVector pt;
    mesh->Center(i,pt);
    array[i]=Value(pt);
  }
}

void
Oxs_ScalarField::DefaultIncrMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  if(array.Size() != mesh->Size()) {
    Oxs_ExtError("Import array not initialized"
		   " in Oxs_ScalarField::DefaultIncrMeshValue");
  }
  OC_INDEX i,size=mesh->Size();
  for(i=0;i<size;i++) {
    ThreeVector pt;
    mesh->Center(i,pt);
    array[i] += Value(pt);
  }
}

void
Oxs_ScalarField::DefaultMultMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& array) const
{
  if(array.Size() != mesh->Size()) {
    Oxs_ExtError("Import array not initialized"
		   " in Oxs_ScalarField::DefaultMultMeshValue");
  }
  OC_INDEX i,size=mesh->Size();
  for(i=0;i<size;i++) {
    ThreeVector pt;
    mesh->Center(i,pt);
    array[i] *= Value(pt);
  }
}


