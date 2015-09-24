/* FILE: vectorfield.cc      -*-Mode: c++-*-
 *
 * Abstract Oxs_VectorField class, derives from Oxs_Ext.
 *
 */

#include "vectorfield.h"

/* End includes */

////////////////////////////////////////////////////////////////////////
// Oxs_VectorField (generic Oxs_Ext vector field) class

// Constructors
Oxs_VectorField::Oxs_VectorField
( const char* name,     // Child instance id
  Oxs_Director* newdtr  // App director
  ) : Oxs_Ext(name,newdtr)
{}

Oxs_VectorField::Oxs_VectorField
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Ext(name,newdtr,argstr)
{}

void
Oxs_VectorField::DefaultFillMeshValue
(const Oxs_Mesh* mesh,
 Oxs_MeshValue<ThreeVector>& array) const
{
  array.AdjustSize(mesh);
  OC_INDEX i,size=mesh->Size();
  for(i=0;i<size;i++) {
    ThreeVector pt;
    mesh->Center(i,pt);
    Value(pt,array[i]);
  }
}

void
Oxs_VectorField::FillMeshValueOrthogonal
(const Oxs_Mesh* mesh,
 const Oxs_MeshValue<ThreeVector>& orth_array,
 Oxs_MeshValue<ThreeVector>& array) const
{
  FillMeshValue(mesh,array);
  if(!orth_array.CheckMesh(mesh) || !array.CheckMesh(mesh)) {
    String msg=
      String("Mesh size incompatibility detected in"
	     " Oxs_VectorField::FillMeshValueOrthogonal()"
	     " method of instance ")
      + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }
  OC_INDEX i,size=mesh->Size();
  const OC_REAL8m eps_sq = 1e-28;
  for(i=0;i<size;i++) {
    OC_REAL8m dot = orth_array[i]*array[i];
    OC_REAL8m comp = eps_sq*orth_array[i].MagSq()*array[i].MagSq();
    if(dot*dot >= comp) {
      String msg= String("Orthogonality requirement failed in"
                         " Oxs_VectorField::FillMeshValueOrthogonal()"
                         " method of instance ")
	+ String(InstanceName());
      if(strcmp("Oxs_RandomVectorField",ClassName())==0) {
        msg += String(".\nTry using Oxs_PlaneRandomVectorField instead.");
      }
      throw Oxs_ExtError(msg.c_str());
    }
  }
}
