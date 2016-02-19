/* FILE: MF_magnetoResistance.cc                 -*-Mode: c++-*-
 * version 1.1.1
 *
 * Class allows calculations of resistance of selected areas of the junction
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
#include "MF_MagnetoResistance.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

// Oxs_Ext registration support
OXS_EXT_REGISTER(MF_MagnetoResistance);

/* End includes */


// Constructor
MF_MagnetoResistance::MF_MagnetoResistance(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    mesh_id(0)
{
  // Process arguments
  RA_p = GetRealInitValue("RA_p",0.);
  RA_ap = GetRealInitValue("RA_ap",0.);

area_x = GetRealInitValue("x",0.);
area_y = GetRealInitValue("y",0.);
delta_x = GetRealInitValue("dx",0.);
delta_y = GetRealInitValue("dy",0.);

area_x1 = GetRealInitValue("x1",0.);
area_y1 = GetRealInitValue("y1",0.);
delta_x1 = GetRealInitValue("dx1",0.);
delta_y1 = GetRealInitValue("dy1",0.);

area_x2 = GetRealInitValue("x2",0.);
area_y2 = GetRealInitValue("y2",0.);
delta_x2 = GetRealInitValue("dx2",0.);
delta_y2 = GetRealInitValue("dy2",0.);

area_x3 = GetRealInitValue("x3",0.);
area_y3 = GetRealInitValue("y3",0.);
delta_x3 = GetRealInitValue("dx3",0.);
delta_y3 = GetRealInitValue("dy3",0.);
  // First boundary.  Import parameter string should be a 10 element
  // list of name-value pairs:
  //         NAME         VALUE
  //         atlas        atlas reference
  //         region       region name
  //         scalarfield  scalar field reference
  //         scalarvalue  scalar field level surface reference value
  //         scalarside   either "+" or "-", denoting inner surface side
  CheckInitValueParamCount("surface1",10);
  vector<String> surface1;
  FindRequiredInitValue("surface1",surface1);
  OC_BOOL badinput1=0;
  String error1;
  if(surface1[0].compare("atlas")!=0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[0]
      + String("\"; should be \"atlas\"");
  } else if(surface1[2].compare("region")      != 0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[2]
      + String("\"; should be \"region\"");
  } else if(surface1[4].compare("scalarfield") != 0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[4]
      + String("\"; should be \"scalarfield\"");
  } else if(surface1[6].compare("scalarvalue") != 0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[6]
      + String("\"; should be \"scalarvalue\"");
  } else if(surface1[8].compare("scalarside")  != 0) {
    badinput1 = 1;
    error1 = String("Bad name \"") + surface1[8]
      + String("\"; should be \"scalarside\"");
  } else {
    Nb_SplitList param_strings;
    vector<String> params;
    param_strings.Split(surface1[1].c_str());
    param_strings.FillParams(params);
    OXS_GET_EXT_OBJECT(params,Oxs_Atlas,atlas1);
    region1 = surface1[3];
    param_strings.Split(surface1[5].c_str());
    param_strings.FillParams(params);
    OXS_GET_EXT_OBJECT(params,Oxs_ScalarField,bdry1);
    bdry1_value = Nb_Atof(surface1[7].c_str());
    bdry1_side = surface1[9];
    if(atlas1->GetRegionId(region1) == -1) {
      badinput1 = 1;
      error1 = String("Unrecognized region \"")
        + region1 + String("\"");
    } else if(bdry1_side.compare("+")!=0 && bdry1_side.compare("-")!=0) {
      badinput1 = 1;
      error1 = String("Bad scalarside string \"")
        + bdry1_side + String("\"; should be \"+\" or \"-\"");
    }
  }
  DeleteInitValue("surface1");

  // Second boundary.  Import parameter string same format
  // as for first boundary.
  CheckInitValueParamCount("surface2",10);
  vector<String> surface2;
  FindRequiredInitValue("surface2",surface2);
  OC_BOOL badinput2=0;
  String error2;
  if(surface2[0].compare("atlas")!=0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[0]
      + String("\"; should be \"atlas\"");
  } else if(surface2[2].compare("region")      != 0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[2]
      + String("\"; should be \"region\"");
  } else if(surface2[4].compare("scalarfield") != 0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[4]
      + String("\"; should be \"scalarfield\"");
  } else if(surface2[6].compare("scalarvalue") != 0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[6]
      + String("\"; should be \"scalarvalue\"");
  } else if(surface2[8].compare("scalarside")  != 0) {
    badinput2 = 1;
    error2 = String("Bad name \"") + surface2[8]
      + String("\"; should be \"scalarside\"");
  } else {
    Nb_SplitList param_strings;
    vector<String> params;
    param_strings.Split(surface2[1].c_str());
    param_strings.FillParams(params);
    OXS_GET_EXT_OBJECT(params,Oxs_Atlas,atlas2);
    region2 = surface2[3];
    param_strings.Split(surface2[5].c_str());
    param_strings.FillParams(params);
    OXS_GET_EXT_OBJECT(params,Oxs_ScalarField,bdry2);
    bdry2_value = Nb_Atof(surface2[7].c_str());
    bdry2_side = surface2[9];
    if(atlas2->GetRegionId(region2) == -1) {
      badinput2 = 1;
      error2 = String("Unrecognized region \"")
        + region2 + String("\"");
    } else if(bdry2_side.compare("+")!=0 && bdry2_side.compare("-")!=0) {
      badinput2 = 1;
      error2 = String("Bad scalarside string \"")
        + bdry2_side + String("\"; should be \"+\" or \"-\"");
    }
  }
  DeleteInitValue("surface2");

  if(badinput1 || badinput2) {
    String msg=
      String("Bad surface parameter block, inside "
             "Oxs_TwoSurfaceExchange mif Specify block for object ")
      +  String(InstanceName()) + String(":");
    if(badinput1) msg += String(" surface1 block is bad: ") + error1;
    if(badinput1 && badinput2) msg += String(", ");
    if(badinput2) msg += String(" surface2 block is bad: ") + error2;
    msg += String(".");
    throw Oxs_ExtError(msg.c_str());
  }
MR_output.Setup(this,InstanceName(),"Total resistance","Ohm",0,
     &MF_MagnetoResistance::UpdateDerivedOutputs);
mr_area_output.Setup(this,InstanceName(),"Area0 resistance","Ohm",0,
     &MF_MagnetoResistance::UpdateDerivedOutputs);
mr_area1_output.Setup(this,InstanceName(),"Area1 resistance","Ohm",0,
     &MF_MagnetoResistance::UpdateDerivedOutputs);
mr_area2_output.Setup(this,InstanceName(),"Area2 resistance","Ohm",0,
     &MF_MagnetoResistance::UpdateDerivedOutputs);
mr_area3_output.Setup(this,InstanceName(),"Area3 resistance","Ohm",0,
     &MF_MagnetoResistance::UpdateDerivedOutputs);
conductance_output.Setup(this,InstanceName(),"Channel conductance","1/Ohm",0,
     &MF_MagnetoResistance::UpdateDerivedOutputs);

  VerifyAllInitArgsUsed();
}

MF_MagnetoResistance::~MF_MagnetoResistance()
{}

OC_BOOL MF_MagnetoResistance::Init()
{
MR_output.Register(director,-5);
mr_area_output.Register(director,-5);
mr_area1_output.Register(director,-5);
mr_area2_output.Register(director,-5);
mr_area3_output.Register(director,-5);
conductance_output.Register(director,-5);
  mesh_id = 0;
  links.clear();
  return Oxs_Energy::Init();
}

void MF_MagnetoResistance::FillLinkList
(const Oxs_RectangularMesh* mesh) const
{ // Queries the mesh with each surface spec,
  // and then pairs up cells between surfaces.
  // Each cell on the first surface is paired up with
  // the closest cell from the second surface.  Note
  // the asymmetry in this pairing procedure---generally,
  // one will probably want to make the smaller surface
  // the "first" surface.  Consider these pictures:
  //
  //          22222222222222 <-- Surface 2
  //              ||||||           
  //              ||||||  <-- Links
  //              ||||||         
  //              111111  <-- Surface 1
  //
  //  as compared to
  //
  //          11111111111111 <-- Surface 1
  //           \\\||||||///        
  //            \\||||||// <-- Links
  //             \||||||/        
  //              222222  <-- Surface 2
  //
  // In the second picture, all the outer cells in the top
  // surface get paired up with the outer elements in the
  // lower surface.

  // Get cell lists for each surface
  vector<OC_INDEX> bdry1_cells,bdry2_cells;
  mesh->BoundaryList(*atlas1,region1,*bdry1,bdry1_value,bdry1_side,
                     bdry1_cells);
  mesh->BoundaryList(*atlas2,region2,*bdry2,bdry2_value,bdry2_side,
                     bdry2_cells);
  if(bdry1_cells.empty() || bdry2_cells.empty()) {
    String msg =
      String("Empty bdry/surface list with current mesh"
             " detected in Oxs_TwoSurfaceExchange::FillLinkList()"
             " routine of object ") + String(InstanceName());
    throw Oxs_ExtError(msg.c_str());
  }

  // For each cell in bdry1_cells, find closest cell in bdry2_cells,
  // and store this pair in "links".
  links.clear();  // Throw away previous links, if any.
  vector<OC_INDEX>::iterator it1 = bdry1_cells.begin();
  while(it1!=bdry1_cells.end()) {
    ThreeVector cell1;
    mesh->Center(*it1,cell1);
    vector<OC_INDEX>::iterator it2 = bdry2_cells.begin();
    ThreeVector vtemp;
    mesh->Center(*it2,vtemp);
    vtemp -= cell1;
    OC_REAL8m min_distsq = vtemp.MagSq();
    OC_INDEX min_index = *it2;
    while( (++it2) != bdry2_cells.end()) {
      mesh->Center(*it2,vtemp);
      vtemp -= cell1;
      OC_REAL8m distsq = vtemp.MagSq();
      if(distsq<min_distsq) {
        min_distsq = distsq;
        min_index = *it2;
      }
    }
    if(min_distsq>0.) {
      // The sigma coef's need to be weighted by the inverse
      // of the thickness of the compuational cell in the
      // direction of the link.  This is necessary so that
      // the total surface (interfacial) exchange energy is
      // independent of the discretization size, as the
      // discretization size is reduces to 0.  (As the
      // computational cell size is reduced, the surface
      // energy is squeezed into an ever thinner layer of
      // cells along the surface.)
      ThreeVector wtemp;
      mesh->Center(min_index,wtemp);
      wtemp -= cell1;
      OC_REAL8m cellwgt = wtemp.x*mesh->EdgeLengthX()
                     + wtemp.y*mesh->EdgeLengthY()
                     + wtemp.z*mesh->EdgeLengthZ();
      if(cellwgt>0.) { // Safety
	cellwgt = sqrt(min_distsq)/cellwgt;
	// min_distsq _should_ be wtemp.MagSq(), so
	// cellwgt is |wtemp|/<wtemp,celldims>, i.e.,
	// < wtemp/|wtemp| , celldims >^{-1}
      }

      MF_MagnetoResistanceLinkParams ltemp;
      ltemp.index1 = *it1;    ltemp.index2 = min_index;
      //ltemp.exch_coef1 = (init_sigma + init_sigma2) * cellwgt;
      //ltemp.exch_coef2 = -init_sigma2 * cellwgt;
      // NB: The init_sigma and init_sigma2 references above
      //  will likely be replaced in the future with a call
      //  to a user-defined function taking as imports the
      //  center locations of the two cells in the link pair.
ltemp.conductance = 0.;
      links.push_back(ltemp);
    }
    ++it1;
  }
}

void MF_MagnetoResistance::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);

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
    FillLinkList(mesh);
    mesh_id=mesh->Id();
  }

  // Zero entire energy and field meshes
  OC_INDEX size=mesh->Size();
  OC_INDEX index;
  for(index=0;index<size;index++) energy[index]=0.;
  for(index=0;index<size;index++) field[index].Set(0.,0.,0.);

  // Iterate through link list and accumulate energies and fields
  OC_REAL8m mindot = 1;
  OC_REAL8m hcoef = 2.0/MU0;
  vector<MF_MagnetoResistanceLinkParams>::const_iterator it
    = links.begin();
  for(it=links.begin();it!=links.end();++it) {
    OC_INDEX i = it->index1;
    OC_INDEX j = it->index2;
    if(Ms_inverse[i]==0. || Ms_inverse[j]==0.) continue; // Ms=0; skip
    OC_REAL8m ecoef1 = it->exch_coef1;
    OC_REAL8m ecoef2 = it->exch_coef2;
    ThreeVector mdiff = spin[i]-spin[j];
    OC_REAL8m dot = spin[i] * spin[j];
    if(dot<mindot) mindot = dot;
    OC_REAL8m temp = 0.5*mdiff.MagSq();
    OC_REAL8m elink = (ecoef1 + ecoef2*temp)*temp; // Energy density
    energy[i] += elink;
    energy[j] += elink;
    mdiff *= hcoef*(ecoef1+2*ecoef2*temp);
    field[i] += -1*Ms_inverse[i]*mdiff;
    field[j] +=    Ms_inverse[j]*mdiff;
  }

#if 0 // The following block has no effect, because all the variables are local.  -mjd
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



void MF_MagnetoResistance::UpdateDerivedOutputs(const Oxs_SimState& state)
{
  MR_output.cache.state_id = mr_area_output.cache.state_id =
mr_area1_output.cache.state_id =
mr_area2_output.cache.state_id =
mr_area3_output.cache.state_id = 0;  // Mark change in progress

const Oxs_MeshValue<ThreeVector>& spin_ = state.spin;
vector<MF_MagnetoResistanceLinkParams>::const_iterator it = links.begin();

const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);
Oxs_MeshValue<ThreeVector>& con = conductance_output.cache.value;
con.AdjustSize(mesh);

OC_REAL8m conductance = 0.;
OC_REAL8m p = 0.;
OC_REAL8m pom = 0.;
OC_REAL8m dummy_value;

Rs_p = RA_p/(mesh->EdgeLengthX()*mesh->EdgeLengthY());
Rs_ap = RA_ap/(mesh->EdgeLengthX()*mesh->EdgeLengthY());
for(it=links.begin();it!=links.end();++it)
{
    OC_INDEX i = it->index1;
    OC_INDEX j = it->index2;

p=(spin_[i].x*spin_[j].x+spin_[i].y*spin_[j].y+spin_[i].z*spin_[j].z)/(sqrt(pow(spin_[i].x,2)+pow(spin_[i].y,2)+pow(spin_[i].z,2))*sqrt(pow(spin_[j].x,2)+pow(spin_[j].y,2)+pow(spin_[j].z,2)));
//it->conductance= 1/(r_p+(r_ap-r_p)/2*(1-p)); read only structure 
pom = 1/(Rs_p+(Rs_ap-Rs_p)/2*(1-p));
conductance += pom;
con[i].Set(0.0,0.0,pom);
con[j].Set(0.0,0.0,pom);

}

ThreeVector tmp;
tmp.x=tmp.y=tmp.z=0.;
area_conductance = 0.;
area_conductance1 = 0.;
area_conductance2 = 0.;
area_conductance3 = 0.;
OC_INDEX indeks,a,b,ii3 =0;
it = links.begin(); ++it; indeks = it->index1; // Compacting this
                                  // confuses some compilers -mjd
mesh->GetCoords(indeks,a,b,ii3);
for (int i1=area_x; i1 <= area_x +delta_x; i1++)
{
	for (int i2=area_y; i2 <= area_y + delta_y; i2++)
	{
		tmp = con[mesh->Index(i1,i2,ii3)];
		area_conductance += tmp.z;
	}
}
tmp.x=tmp.y=tmp.z=0.;
for (int i1=area_x1; i1 <= area_x1 +delta_x1; i1++)
{
	for (int i2=area_y1; i2 <= area_y1 + delta_y1; i2++)
	{
		tmp = con[mesh->Index(i1,i2,ii3)];
		area_conductance1 += tmp.z;
	}
}
tmp.x=tmp.y=tmp.z=0.;
for (int i1=area_x2; i1 <= area_x2 +delta_x2; i1++)
{
	for (int i2=area_y2; i2 <= area_y2 + delta_y2; i2++)
	{
		tmp = con[mesh->Index(i1,i2,ii3)];
		area_conductance2 += tmp.z;
	}
}
tmp.x=tmp.y=tmp.z=0.;
for (int i1=area_x3; i1 <= area_x3 +delta_x3; i1++)
{
	for (int i2=area_y3; i2 <= area_y3 + delta_y3; i2++)
	{
		tmp = con[mesh->Index(i1,i2,ii3)];
		area_conductance3 += tmp.z;
	}
}



if ( area_conductance == 0.0 )
{
	mr_area_output.cache.value = -1;	
}
else
{
	mr_area_output.cache.value = 1/area_conductance ;
}

if ( area_conductance1 == 0.0 )
{
	mr_area1_output.cache.value = -1;	
}
else
{
	mr_area1_output.cache.value = 1/area_conductance1 ;
}

if ( area_conductance2 == 0.0 )
{
	mr_area2_output.cache.value = -1;	
}
else
{
	mr_area2_output.cache.value = 1/area_conductance2 ;
}

if ( area_conductance3 == 0.0 )
{
	mr_area3_output.cache.value = -1;	
}
else
{
	mr_area3_output.cache.value = 1/area_conductance3 ;
}

if ( conductance == 0 )
{
throw Oxs_Ext::Error(this,
			   "CYY_STTEvolve::UpdateDerviedOutputs:"
			   " Only one layer detected.");	

}
conductance_output.cache.state_id=state.Id();
MR_output.cache.value = 1/conductance;
  
if(!state.GetDerivedData("magnetoresistance",dummy_value)) {
      state.AddDerivedData("magnetoresistance",MR_output.cache.value);
}
MR_output.cache.state_id = state.Id();
mr_area_output.cache.state_id = state.Id();
mr_area1_output.cache.state_id = state.Id();
mr_area2_output.cache.state_id = state.Id();
mr_area3_output.cache.state_id = state.Id();

}
