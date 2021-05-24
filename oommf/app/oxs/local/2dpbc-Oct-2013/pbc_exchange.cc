/* FILE: pbc_exchange.cc            -*-Mode: c++-*-
 * modified periodicexchange.cc
 *
 * Uniform 6 neighbor exchange energy on rectangular mesh,
 * derived from Oxs_Energy class.
 *
 */

#include <string>

#include "nb.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

#include "pbc_exchange.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(PBC_Exchange_2D);

/* End includes */

// Constructor
PBC_Exchange_2D::PBC_Exchange_2D(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr), A(0.)
{
  // Process arguments
  A = GetRealInitValue("A");
  VerifyAllInitArgsUsed();
}

PBC_Exchange_2D::~PBC_Exchange_2D()
{}

OC_BOOL PBC_Exchange_2D::Init()
{
  return Oxs_Energy::Init();
}


//In direction x and y use periodic boundary condition.
//In direction z use mirror boundary condition.
void
PBC_Exchange_2D::CalcEnergy6NgbrPBC_2D
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<OC_REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  OC_INDEX   xdim = mesh->DimX();
  OC_INDEX   ydim = mesh->DimY();
  OC_INDEX   zdim = mesh->DimZ();
  OC_INDEX  xydim =  xdim*ydim;
  // OC_INDEX xyzdim = xydim*zdim;

  const OC_REAL8m hcoef = -2/MU0;
  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(OC_INDEX z=0;z<zdim;z++) {
    for(OC_INDEX y=0;y<ydim;y++) {
      for(OC_INDEX x=0;x<xdim;x++) {
        OC_INDEX i = mesh->Index(x,y,z); // Get base linear address

        OC_REAL8m Msii = Ms_inverse[i];
        if(Msii == 0.0) {
          energy[i]=0.0;
          field[i].Set(0.,0.,0.);
          continue;
          }

        OC_INDEX j=0;
        ThreeVector base = spin[i];
        ThreeVector sum(0.,0.,0.);
        if(x>0) j = i - 1;  //j=mesh->Index(x-1,y,z)
        else    j = i - 1 + xdim;  //x==0,j=Index(xdim-1,y,z);
        if(Ms_inverse[j]!=0.0) sum  = (spin[j] - base);
        if(x<xdim-1) j = i + 1;
        else         j = i + 1 - xdim;
        if(Ms_inverse[j]!=0.0) sum += (spin[j] - base);
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0) j = i - xdim;
        else    j = i - xdim + xydim;
        if(Ms_inverse[j]!=0.0) temp  = (spin[j] - base);
        if(y<ydim-1) j = i + xdim;
        else         j = i + xdim - xydim;
        if(Ms_inverse[j]!=0.0) temp += (spin[j] - base);
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0){
	      j = i - xydim;
            if(Ms_inverse[j]!=0.0) temp  = (spin[j] - base);
          }
        if(z<zdim-1){
            j = i + xydim;
            if(Ms_inverse[j]!=0.0) temp += (spin[j]- base);
          }
        sum += wgtz*temp;
        field[i] = (hcoef*Msii) * sum;
        energy[i] = (sum * base);
      }
    }
  }
}



void PBC_Exchange_2D::GetEnergy
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
    throw Oxs_Ext::Error(this,"Import mesh to"
                         " PBC_Exchange_2D::GetEnergy()"
                         " is not an Oxs_RectangularMesh object.");
  }

  CalcEnergy6NgbrPBC_2D(spin,Ms_inverse,mesh,energy,field);
}
