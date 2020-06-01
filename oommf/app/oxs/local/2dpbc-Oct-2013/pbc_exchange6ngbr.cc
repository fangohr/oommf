/* FILE: pbc_exchange6ngbr.cc            -*-Mode: c++-*-
 * 
 * modified from exchange6ngbr.cc 
 *
 * 6 neighbor exchange energy on rectangular mesh,
 * derived from Oxs_Energy class.
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
#include "pbc_exchange6ngbr.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(PBC_Exchange6Ngbr_2D);

/* End includes */


// Constructor
PBC_Exchange6Ngbr_2D::PBC_Exchange6Ngbr_2D(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    A_size(0), A(NULL), mesh_id(0)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("atlas",Oxs_Atlas,atlas);
  atlaskey.Set(atlas.GetPtr());
  /// Dependency lock is held until *this is deleted.

  // Check for optional default_A parameter; default is 0.
  OC_REAL8m default_A = GetRealInitValue("default_A",0.0);

  // Allocate A matrix.  Because raw pointers are used, a memory
  // leak will occur if an exception is thrown inside this constructor.
  A_size = atlas->GetRegionCount();
  if(A_size<1) {
    String msg = String("Oxs_Atlas object ")
      + atlas->InstanceName()
      + String(" must contain at least one region.");

    throw Oxs_Ext::Error(msg.c_str());
  }
  A = new OC_REAL8m*[A_size];
  A[0] = new OC_REAL8m[A_size*A_size];
  OC_INDEX i;
  for(i=1;i<A_size;i++) A[i] = A[i-1] + A_size;
  for(i=0;i<A_size*A_size;i++) A[0][i] = default_A;

  // Fill A matrix
  vector<String> params;
  FindRequiredInitValue("A",params);
  if(params.empty()) {
    throw Oxs_Ext::Error(this,"Empty parameter list for key \"A\"");
  }
  if(params.size()%3!=0) {
      char buf[512];
      Oc_Snprintf(buf,sizeof(buf),
		  "Number of elements in A sub-list must be"
		  " divisible by 3"
		  " (actual sub-list size: %u)",
		  (unsigned int)params.size());
      throw Oxs_Ext::Error(this,buf);
  }
  for(i=0;i<static_cast<OC_INDEX>(params.size());i+=3) {
    OC_INT4m i1 = atlas->GetRegionId(params[i]);
    OC_INT4m i2 = atlas->GetRegionId(params[i+1]);
    if(i1<0 || i2<0) {
      char buf[4096];
      char* cptr=buf;
      if(i1<0) {
	Oc_Snprintf(buf,sizeof(buf),
		    "First entry in A[%u] sub-list, \"%s\","
		    " is not a known region in atlas \"%s\".  ",
		    i/3,params[i].c_str(),atlas->InstanceName());
	cptr += strlen(buf);
      }
      if(i2<0) {
	Oc_Snprintf(cptr,sizeof(buf)-(cptr-buf),
		    "Second entry in A[%u] sub-list, \"%s\","
		    " is not a known region in atlas \"%s\".  ",
		    i/3,params[i+1].c_str(),atlas->InstanceName());
      }
      String msg = String(buf);
      msg += String("Known regions:");
      vector<String> regions;
      atlas->GetRegionList(regions);
      for(unsigned int j=0;j<regions.size();++j) {
	msg += String(" \n");
	msg += regions[j];
      }
      throw Oxs_Ext::Error(this,msg);
    }
    OC_BOOL err;
    OC_REAL8m Apair = Nb_Atof(params[i+2].c_str(),err);
    if(err) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
		  "Third entry in A[%u] sub-list, \"%s\","
		  " is not a valid floating point number.",
		  i/3,params[i+2].c_str());
      throw Oxs_Ext::Error(this,buf);
    }
    A[i1][i2]=Apair;
    A[i2][i1]=Apair; // A should be symmetric
  }
  DeleteInitValue("A");

  VerifyAllInitArgsUsed();
}

PBC_Exchange6Ngbr_2D::~PBC_Exchange6Ngbr_2D()
{
  if(A_size>0 && A!=NULL) {
    delete[] A[0];
    delete[] A;
  }
}

OC_BOOL PBC_Exchange6Ngbr_2D::Init()
{
  mesh_id = 0;
  region_id.Release();
  return Oxs_Energy::Init();
}

void PBC_Exchange6Ngbr_2D::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  // See if mesh and/or atlas has changed.
  if(static_cast<OC_UINDEX>(mesh_id) !=  state.mesh->Id()
     || !atlaskey.SameState()) {
    // Setup region mapping
    mesh_id = 0; // Safety
    OC_INDEX size = state.mesh->Size();
    region_id.AdjustSize(state.mesh);
    ThreeVector location;
    for(OC_INDEX i=0;i<size;i++) {
      state.mesh->Center(i,location);
      if((region_id[i] = atlas->GetRegionId(location))<0) {
	String msg = String("Import mesh to PBC_Exchange6Ngbr_2D::GetEnergy()"
                            " routine of object ")
          + String(InstanceName())
	  + String(" has points outside atlas ")
	  + String(atlas->InstanceName());
	throw Oxs_Ext::Error(msg.c_str());
      }
    }
    mesh_id = state.mesh->Id();
  }
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
    String msg = String("Import mesh to PBC_Exchange6Ngbr_2D::GetEnergy()"
                        " routine of object ")
      + String(InstanceName())
      + String(" is not an Oxs_RectangularMesh object.");
    throw Oxs_Ext::Error(msg.c_str());
  }

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;

  OC_REAL8m wgtx = -1.0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -1.0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -1.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  OC_REAL8m hcoef = -2/MU0;

  for(OC_INDEX z=0;z<zdim;z++) {
    for(OC_INDEX y=0;y<ydim;y++) {
      for(OC_INDEX x=0;x<xdim;x++) {
	OC_INDEX i = mesh->Index(x,y,z); // Get base linear address
	ThreeVector base = spin[i];
	OC_REAL8m Msii = Ms_inverse[i];
	if(Msii == 0.0) {
	  energy[i]=0.0;
	  field[i].Set(0.,0.,0.);
	  continue;
	}
	OC_REAL8m* Arow = A[region_id[i]];
	ThreeVector sum(0.,0.,0.);
	if(z>0) {
	  OC_INDEX j = i-xydim;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgtz*(spin[j] - base);
	}
	if(z<zdim-1) {
	  OC_INDEX j = i+xydim;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgtz*(spin[j]- base);
	}



	if(x>0) {
	  OC_INDEX j = i-1;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgtx*(spin[j] - base);
	}else{//x==0
	  OC_INDEX j = i - 1 + xdim;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgtx*(spin[j] - base);    
    }

	if(x<xdim-1) {
	  OC_INDEX j = i+1;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgtx*(spin[j] - base);
	}else{//x==xdim-1
	  OC_INDEX j = i + 1 - xdim;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgtx*(spin[j] - base);    
    }



	if(y>0) {
	  OC_INDEX j = i-xdim;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgty*(spin[j] - base);
	}else{//y==0
	  OC_INDEX j = i - xdim + xydim;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgtx*(spin[j] - base);    
    }

	if(y<ydim-1) {
	  OC_INDEX j = i+xdim;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgty*(spin[j] - base);
	}else{//y==ydim-1
	  OC_INDEX j = i + xdim - xydim;
	  OC_REAL8m Apair = Arow[region_id[j]];
	  if(Ms_inverse[j]!=0.0) sum += Apair*wgtx*(spin[j] - base);    
    }

	field[i] = (hcoef*Msii) * sum;
	energy[i] = (sum * base);
      }
    }
  }
}
