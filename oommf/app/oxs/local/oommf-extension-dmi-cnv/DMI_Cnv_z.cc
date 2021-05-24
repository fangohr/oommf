/* FILE: DMI_Cnv_z.cc            -*-Mode: c++-*-
 *
 * Dzyaloshinskii-Moriya energy for the Cnv crystallographic class [1]:
 *
 * $w_\text{dmi} = D ( L_{xz}^{(x)} + L_{yz}^{(y)} )
 *
 * This extension works both with and without periodic boundary conditions.
 *
 * Extension and modification by David Cortes-Ortuno, Marijan Beg and Hans Fangohr (University of Southampton and European XFEL GmbH) of Oxs_DMexchange6ngbr.h [2].
 *
 * Developed as a part of OpenDreamKit Horizon 2020 European Research Infrastructure
 * project (676541), and the EPSRC Programme grant on Skyrmionics (EP/N032128/1).
 *
 * [1] A. N. Bogdanov and D. A. Yablonskii. Zh. Eksp. Teor. Fiz. 95, 178-182 (1989).
 * [2] Rohart, S., & Thiaville, A. Physical Review B, 88, 184422 (2013).
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
#include "DMI_Cnv_z.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_DMI_Cnv_z);

/* End includes */


// Constructor
Oxs_DMI_Cnv_z::Oxs_DMI_Cnv_z(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    A_size(0), D(NULL), 
    xperiodic(0), yperiodic(0), zperiodic(0),
    mesh_id(0)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("atlas",Oxs_Atlas,atlas);
  atlaskey.Set(atlas.GetPtr());
  /// Dependency lock is held until *this is deleted.

  // Check for optional default_D parameter; default is 0.
  OC_REAL8m default_D = GetRealInitValue("default_D",0.0);

  // Allocate A matrix.  Because raw pointers are used, a memory
  // leak will occur if an exception is thrown inside this constructor.
  A_size = atlas->GetRegionCount();
  if(A_size<1) {
    String msg = String("Oxs_Atlas object ")
      + atlas->InstanceName()
      + String(" must contain at least one region.");

    throw Oxs_Ext::Error(msg.c_str());
  }
  D = new OC_REAL8m*[A_size];
  D[0] = new OC_REAL8m[A_size*A_size];
  OC_INDEX i;
  for(i=1;i<A_size;i++) D[i] = D[i-1] + A_size;
  for(i=0;i<A_size*A_size;i++) D[0][i] = default_D;

  // Fill D matrix
  vector<String> params;
  FindRequiredInitValue("D",params);
  if(params.empty()) {
    throw Oxs_Ext::Error(this,"Empty parameter list for key \"D\"");
  }
  if(params.size()%3!=0) {
      char buf[512];
      Oc_Snprintf(buf,sizeof(buf),
		  "Number of elements in D sub-list must be"
		  " divisible by 3"
		  " (actual sub-list size: %u)",
		  (unsigned int)params.size());
      throw Oxs_Ext::Error(this,buf);
  }
  for(i=0;i<params.size();i+=3) {
    OC_INT4m i1 = atlas->GetRegionId(params[i]);
    OC_INT4m i2 = atlas->GetRegionId(params[i+1]);
    if(i1<0 || i2<0) {
      char buf[4096];
      char* cptr=buf;
      if(i1<0) {
	Oc_Snprintf(buf,sizeof(buf),
		    "First entry in D[%u] sub-list, \"%s\","
		    " is not a known region in atlas \"%s\".  ",
		    i/3,params[i].c_str(),atlas->InstanceName());
	cptr += strlen(buf);
      }
      if(i2<0) {
	Oc_Snprintf(cptr,sizeof(buf)-(cptr-buf),
		    "Second entry in D[%u] sub-list, \"%s\","
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
    OC_REAL8m Dpair = Nb_Atof(params[i+2].c_str(),err);
    if(err) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
		  "Third entry in D[%u] sub-list, \"%s\","
		  " is not a valid floating point number.",
		  i/3,params[i+2].c_str());
      throw Oxs_Ext::Error(this,buf);
    }
    D[i1][i2]=Dpair;
    D[i2][i1]=Dpair; // D should be symmetric
  }
  DeleteInitValue("D");

  VerifyAllInitArgsUsed();
}

Oxs_DMI_Cnv_z::~Oxs_DMI_Cnv_z()
{
  if(A_size>0 && D!=NULL) {
    delete[] D[0];
    delete[] D;
  }
}

OC_BOOL Oxs_DMI_Cnv_z::Init()
{
  mesh_id = 0;
  region_id.Release();
  return Oxs_Energy::Init();
}

void Oxs_DMI_Cnv_z::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  // See if mesh and/or atlas has changed.
  if(mesh_id !=  state.mesh->Id() || !atlaskey.SameState()) {
    // Setup region mapping
    mesh_id = 0; // Safety
    OC_INDEX size = state.mesh->Size();
    region_id.AdjustSize(state.mesh);
    ThreeVector location;
    for(OC_INDEX i=0;i<size;i++) {
      state.mesh->Center(i,location);
      if((region_id[i] = atlas->GetRegionId(location))<0) {
	String msg = String("Import mesh to Oxs_DMI_Cnv_z::GetEnergy()"
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

  // Check periodicity --------------------------------------------------------
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(state.mesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(this,msg);
  }

  const Oxs_RectangularMesh* rmesh
    = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh!=NULL) {
    // Rectangular, periodic mesh
    xperiodic = pmesh->IsPeriodicX();
    yperiodic = pmesh->IsPeriodicY();
    zperiodic = pmesh->IsPeriodicZ();
  } else if (rmesh!=NULL) {
    xperiodic=0; yperiodic=0; zperiodic=0;
  } else {
    String msg=String("Unknown mesh type: \"")
      + String(ClassName())
      + String("\".");
    throw Oxs_ExtError(this,msg.c_str());
  }
  // --------------------------------------------------------------------------

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim * ydim;
  // OC_INDEX xyzdim = xdim * ydim * zdim;

  OC_REAL8m wgtx = 1.0/(mesh->EdgeLengthX());
  OC_REAL8m wgty = 1.0/(mesh->EdgeLengthY());
  //OC_REAL8m wgtz = -1.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

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
        OC_REAL8m* Drow = D[region_id[i]];
        ThreeVector sum(0.,0.,0.);
        ThreeVector zu(0.,0.,1.);
        OC_INDEX j;

        if(y > 0 || yperiodic) {  // y- direction
          if(y > 0) {
            j = i - xdim;
          } else if (yperiodic) {
            j = i - xdim + xydim;
          }
          if(Ms_inverse[j] != 0.0) {
            OC_REAL8m Dpair = Drow[region_id[j]];
            ThreeVector uij(0.,-1.,0);
            sum += 0.5 * Dpair * wgty * ((zu ^ uij) ^ spin[j]);
          }
        }

        if(x > 0 || xperiodic) {  // x- direction
          if(x > 0) {
            j = i - 1;        // j = mesh->Index(x-1,y,z)
          } else if (xperiodic) {
            j = i - 1 + xdim; // x == 0, j = Index(xdim-1,y,z);
          }
          if(Ms_inverse[j] != 0.0) {
            OC_REAL8m Dpair = Drow[region_id[j]];
            ThreeVector uij(-1.,0.,0);
            sum += 0.5 * Dpair * wgtx * ((zu ^ uij) ^ spin[j]);
          }
        }

        if(y < ydim - 1 || yperiodic) {  // y+ direction
          if (y < ydim-1) {
            j = i + xdim;
          } else if (yperiodic) {
            j = i + xdim - xydim;
          }
          if(Ms_inverse[j] != 0.0) {
            OC_REAL8m Dpair = Drow[region_id[j]];
            ThreeVector uij(0.,1.,0);
            sum += 0.5 * Dpair * wgty * ((zu ^ uij) ^ spin[j]);
          }
        }

        if(x < xdim-1 || xperiodic) {  // x+ direction
          if (x < xdim-1) {
            j = i + 1;
          } else if (xperiodic) {
            j = i + 1 - xdim;
          }
          if (Ms_inverse[j] != 0.0) {
            OC_REAL8m Dpair = Drow[region_id[j]];
            ThreeVector uij(1.,0.,0);
            sum += 0.5 * Dpair * wgtx * ((zu ^ uij) ^ spin[j]);
          }
        }
	
	    field[i]  = (hcoef * Msii) * sum;
	    energy[i] = (sum * base);
      }
    }
  }
}
