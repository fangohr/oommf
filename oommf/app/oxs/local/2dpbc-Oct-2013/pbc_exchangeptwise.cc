/* FILE: pbc_exchangeptwise.cc            -*-Mode: c++-*-
 *
 * 6 neighbor exchange energy on rectangular mesh, with
 * exchange coefficient A specified on a cell-by-cell
 * basis.  The effective coefficient between two neighboring
 * cells is calculated on the fly via 2.A_1.A_2/(A_1+A_2)
 *
 */

#include <string>

#include "atlas.h"
#include "nb.h"
#include "key.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "pbc_exchangeptwise.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(PBC_ExchangePtwise_2D);

/* End includes */


// Constructor
PBC_ExchangePtwise_2D::PBC_ExchangePtwise_2D(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr), mesh_id(0)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("A",Oxs_ScalarField,A_init);
  VerifyAllInitArgsUsed();
}

PBC_ExchangePtwise_2D::~PBC_ExchangePtwise_2D(){}

OC_BOOL PBC_ExchangePtwise_2D::Init()
{
  mesh_id = 0;
  A.Release();
  return Oxs_Energy::Init();
}

void PBC_ExchangePtwise_2D::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg = String("Import mesh to PBC_ExchangePtwise_2D::GetEnergy()"
                        " routine of object ")
      + String(InstanceName())
      + String(" is not an Oxs_RectangularMesh object.");
    throw Oxs_Ext::Error(msg.c_str());
  }

  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;

  if(static_cast<OC_UINDEX>(mesh_id) != state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    mesh_id = 0;
    A_init->FillMeshValue(state.mesh,A);
    // Check that we won't have any divide-by-zero problems
    // when we use A.
    for(OC_INDEX z=0;z<zdim;z++) {
      for(OC_INDEX y=0;y<ydim;y++) {
	for(OC_INDEX x=0;x<xdim;x++) {
	  OC_INDEX i = mesh->Index(x,y,z); // Get base linear address
	  OC_REAL8m Ai = A[i];
	  for(OC_INDEX dir=0;dir<3;dir++) { // 0=>x, 1=>y, 2=>z
	    OC_INDEX j=0,x2=x,y2=y,z2=z;
	    switch(dir) {
	      case 0:  if(x+1<xdim) { j=i+1;     x2+=1; } break;
	      case 1:  if(y+1<ydim) { j=i+xdim;  y2+=1; } break;
	      default: if(z+1<zdim) { j=i+xydim; z2+=1; } break;
	    }
	    if(j==0) continue; // Edge case; skip
	    OC_REAL8m Aj=A[j];
	    if(fabs(Ai+Aj)<1.0
	       && fabs(Ai*Aj)>(DBL_MAX/2.)*(fabs(Ai+Aj))) {
	      char buf[1024];
	      Oc_Snprintf(buf,sizeof(buf),
		 "Exchange coefficient A initialization in"
		 " PBC_ExchangePtwise_2D::GetEnergy()"
		 " routine of object %s yields infinite value"
		 " for A: A(%u,%u,%u)=%g, A(%u,%u,%u)=%g\n",
		 InstanceName(),x,y,z,Ai,x2,y2,z2,Aj);
	      throw Oxs_Ext::Error(buf);
	    }
	  }
	} // x++
      } // y++
    } // z++
    mesh_id = state.mesh->Id();
  } // mesh_id != state.mesh->Id()

  // First loop through and zero out entire energy and field
  // meshes.  This could be done inside the main body of the
  // loop, but it makes the logic a bit more complicated.
  OC_INDEX size = state.mesh->Size();
  OC_INDEX index;
  for(index=0;index<size;index++) energy[index]=0.;
  for(index=0;index<size;index++) field[index].Set(0.,0.,0.);

  // Now run through mesh and accumulate energies and fields.
  OC_REAL8m wgtx = -1.0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -1.0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -1.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  OC_REAL8m hcoef = -2.0/MU0;

  // Exchange energy density is
  //
  //     A.(1-m_i*m_j)/d^2                      (1)
  //
  // where A is effective exchange coefficient, and d is
  // the distance between the centers of cells i and j.  In
  // this code effective A is calculated from A_i and A_j
  // via A = 2.A_i.A_j/(A_i+A_j).  This energy density
  // is accumated at both cells i & j.
  //   The fields are
  //
  //     H_i = (2/MU0).[A/(Ms_i.d^2)].m_j       (2)
  //     H_j = (2/MU0).[A/(Ms_j.d^2)].m_i
  //
  // where the lead factor of 2 comes because in the total
  // energy expression, (1) occurs twice, once for cell i
  // and once for cell j.

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

          OC_REAL8m Ai = A[i];
          OC_INDEX j=0;
          ThreeVector base = spin[i];
          ThreeVector sum(0.,0.,0.);

          if(x>0) j = i - 1;  //j=mesh->Index(x-1,y,z)
          else    j = i - 1 + xdim;  //x==0,j=Index(xdim-1,y,z);
          OC_REAL8m acoef = 2*wgtx*((Ai*A[j])/(Ai+A[j]));
          if(Ms_inverse[j]!=0.0) sum += (spin[j] - base)*acoef;

          if(x<xdim-1) j = i + 1;
          else         j = i + 1 - xdim;
          acoef = 2*wgtx*((Ai*A[j])/(Ai+A[j]));
          if(Ms_inverse[j]!=0.0) sum += (spin[j] - base)*acoef;


          if(y>0) j = i - xdim;
          else    j = i - xdim + xydim;
          acoef = 2*wgty*((Ai*A[j])/(Ai+A[j]));
          if(Ms_inverse[j]!=0.0) sum += (spin[j] - base)*acoef;

          if(y<ydim-1) j = i + xdim;
          else         j = i + xdim - xydim;
          acoef = 2*wgty*((Ai*A[j])/(Ai+A[j]));
          if(Ms_inverse[j]!=0.0) sum += (spin[j] - base)*acoef;


          if(z>0){
        	  j = i - xydim;
        	  acoef = 2*wgtz*((Ai*A[j])/(Ai+A[j]));
              if(Ms_inverse[j]!=0.0) sum+= (spin[j] - base)*acoef;
            }
          if(z<zdim-1){
              j = i + xydim;
              acoef = 2*wgtz*((Ai*A[j])/(Ai+A[j]));
              if(Ms_inverse[j]!=0.0) sum += (spin[j]- base)*acoef;
          }
          field[i] = (hcoef*Msii) * sum;
          energy[i] = (sum * base);
        }
      }
    }

}

