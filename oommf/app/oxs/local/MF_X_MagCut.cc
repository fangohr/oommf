/* FILE: MF_X_MagCut.cc                 -*-Mode: c++-*-
 * version 1.1.1
 *
 * Class allows cutting magnetisation value in x direction of selected area
 *  
 * author: Marek Frankowski
 * contact: mfrankow[at]agh.edu.pl
 * website: layer.uci.agh.edu.pl/M.Frankowski
 *
 * This code is public domain work based on other public domains contributions
 */

#include "nb.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "oxswarn.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "MF_X_MagCut.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

// Oxs_Ext registration support
OXS_EXT_REGISTER(MF_X_MagCut);

/* End includes */


// Constructor
MF_X_MagCut::MF_X_MagCut(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    mesh_id(0)
{
  // Process arguments
area_x = GetIntInitValue("x",0);
area_y = GetIntInitValue("y",0);
area_z = GetIntInitValue("z",0);

delta_x = GetIntInitValue("dx",0);
delta_y = GetIntInitValue("dy",0);
delta_z = GetIntInitValue("dz",0);

m_area_x_output.Setup(this,InstanceName(),"area mx","",0,
     &MF_X_MagCut::UpdateDerivedOutputs);

  VerifyAllInitArgsUsed();
}

MF_X_MagCut::~MF_X_MagCut()
{}

OC_BOOL MF_X_MagCut::Init()
{
m_area_x_output.Register(director,-5);
  mesh_id = 0;
  return Oxs_Energy::Init();
}

void MF_X_MagCut::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{

  // const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  // const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg =
      String("Import mesh to Oxs_TwoSurfaceExchange::GetEnergy()"
             " routine of object ") + String(InstanceName())
      + String(" is not an Oxs_RectangularMesh object.");
    throw Oxs_ExtError(msg.c_str());
  }
  // If mesh has changed, re-pick link selections
  if(mesh_id !=  mesh->Id()) {
    //llLinkList(mesh);
    mesh_id=mesh->Id();
  }

  // Zero entire energy and field meshes
  OC_INDEX size=mesh->Size();
  OC_INDEX index;
  for(index=0;index<size;index++) energy[index]=0.;
  for(index=0;index<size;index++) field[index].Set(0.,0.,0.);

#if 0 // The following block has no effect, because all the variables are local.  -mjd
  OC_REAL8m mindot = 1;
  OC_REAL8m hcoef = 2.0/MU0;

  // Set maxang data
  OC_REAL8m maxang;
  // Handle extremal cases separately in case spins drift off S2
  if(mindot >  1 - 8*OC_REAL8_EPSILON)      maxang = 0.0;
  else if(mindot < -1 + 8*OC_REAL8_EPSILON) maxang = 180.0;
  else                                   maxang = acos(mindot)*(180.0/PI);

  OC_REAL8m dummy_value;

  const Oxs_SimState* oldstate = NULL;
  OC_REAL8m stage_maxang = -1;
  OC_REAL8m run_maxang = -1;
#endif // NOP
}



void MF_X_MagCut::UpdateDerivedOutputs(const Oxs_SimState& state)
{
m_area_x_output.cache.state_id = 0;

const Oxs_MeshValue<ThreeVector>& spin_ = state.spin;

const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);

const Oxs_MeshValue<OC_REAL8m>& Ms_ = *(state.Ms);

OC_INDEX pom;
OC_REAL8m tmp1;
OC_REAL8m counter = 0.;

ThreeVector tmp;
tmp1 = 0.;
for (int i1=area_x; i1 <= area_x +delta_x; i1++)
{
	for (int i2=area_y; i2 <= area_y + delta_y; i2++)
	{
			for (int i3=area_z; i3 <= area_z + delta_z; i3++)
			{
				pom = mesh->Index(i1,i2,i3);
			if ( Ms_[pom] == 0 )
				continue;
			tmp = spin_[pom];

			tmp1+=tmp.x*Ms_[pom];
			counter+=Ms_[pom];
			}
		
	}
}

m_area_x_output.cache.value = tmp1/counter;

m_area_x_output.cache.state_id = state.Id();

}
