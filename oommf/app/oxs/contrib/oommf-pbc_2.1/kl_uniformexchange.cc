/* FILE: kl_uniformexchange.cc            -*-Mode: c++-*- 
 *
 * Uniform 6 neighbor exchange energy on rectangular mesh,
 * derived from Oxs_Energy class.
 * File based on: uniformexchange.cc 
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
#include "kl_uniformexchange.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Klm_UniformExchange);

/* End includes */

#undef SKIP_PROBLEM_CODE
/// The 8.0 release of the Intel icc/ia64/Lintel compiler (and perhaps
/// others) gets hung up trying to compile the code wrapped up below in
/// the "SKIP_PROBLEM_CODE" #ifdef sections, at least when "-O3"
/// optimization is enabled.  These blocks provide features that are
/// undocumented at this time (April-2004), so for now we just disable
/// compiling of these blocks.  Remove the above #define to enable
/// compilation.

// Constructor
Klm_UniformExchange::Klm_UniformExchange(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Energy(name,newdtr,argstr),
    excoeftype(A_UNKNOWN), A(-1.), lex(-1.),
    kernel(NGBR_UNKNOWN), mesh_id(0)
{
  // Process arguments
  BOOL has_A = HasInitValue("A");
  BOOL has_lex = HasInitValue("lex");
  if(has_A && has_lex) {
    throw Oxs_Ext::Error(this,"Invalid exchange coefficient request:"
			 " both A and lex specified; only one should"
			 " be given.");
  } else if(has_lex) {
    lex = GetRealInitValue("lex");
    excoeftype = LEX_TYPE;
  } else {
    A = GetRealInitValue("A");
    excoeftype = A_TYPE;
  }

  String kernel_request = GetStringInitValue("kernel","6ngbr");
  if(kernel_request.compare("6ngbrfree")==0) {
    kernel = NGBR_6_FREE;
  } else if(kernel_request.compare("6ngbrmirror")==0 ||
	    kernel_request.compare("6ngbr")==0) {
    kernel = NGBR_6_MIRROR;
  } else if(kernel_request.compare("6ngbrmirror_std")==0) {
    kernel = NGBR_6_MIRROR_STD;
  } else if(kernel_request.compare("6ngbrbigangmirror")==0) {
    kernel = NGBR_6_BIGANG_MIRROR;
  } else if(kernel_request.compare("6ngbrzd2")==0) {
    kernel = NGBR_6_ZD2;
  } else if(kernel_request.compare("6ngbrzperiod")==0) {
    // KL(m)
    kernel = NGBR_6_Z_PERIOD;    
  } else if(kernel_request.compare("12ngbrfree")==0) {
    kernel = NGBR_12_FREE;
  } else if(kernel_request.compare("12ngbrzd1")==0 ||
	    kernel_request.compare("12ngbr")==0) {
    kernel = NGBR_12_ZD1;
  } else if(kernel_request.compare("12ngbrzd1b")==0 ||
	    kernel_request.compare("12ngbrb")==0) {
    kernel = NGBR_12_ZD1B;
  } else if(kernel_request.compare("12ngbrmirror")==0) {
    kernel = NGBR_12_MIRROR;
  } else if(kernel_request.compare("26ngbr")==0) {
    kernel = NGBR_26;
  } else {
    String msg=String("Invalid kernel request: ")
      + kernel_request
      + String("\n Should be one of 6ngbr, 6ngbrfree,"
	       " 12ngbr, 12ngbrfree, 12ngbrmirror, or 26ngbr.");
    throw Oxs_Ext::Error(this,msg.c_str());
  }

/*  
  if(A_TYPE != excoeftype && NGBR_6_MIRROR != kernel) {
    throw Oxs_Ext::Error(this,"Invalid exchange coefficient+kernel"
			 " combination; lex specification currently"
			 " only supported using 6ngbr kernel.");
  }
*/
  
// KL(m)
  if(A_TYPE != excoeftype)
    if(NGBR_6_MIRROR != kernel && NGBR_6_Z_PERIOD != kernel) {
    throw Oxs_Ext::Error(this,"Invalid exchange coefficient+kernel"
			 " combination; lex specification currently"
			 " only supported using kernels: 6ngbr or 6ngbrzperiod.");
  }
  
  VerifyAllInitArgsUsed();
}

Klm_UniformExchange::~Klm_UniformExchange()
{}

BOOL Klm_UniformExchange::Init()
{
  mesh_id = 0;
  xcoef.Free();
  ycoef.Free();
  zcoef.Free();
  return Oxs_Energy::Init();
}

void
Klm_UniformExchange::CalcEnergy6NgbrZD2
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  const REAL8m hcoef = -2/MU0;
  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
        UINT4m i = mesh->Index(x,y,z); // Get base linear address

        REAL8m Msii = Ms_inverse[i];
        if(Msii == 0.0) {
          energy[i]=0.0;
          field[i].Set(0.,0.,0.);
          continue;
        }

        ThreeVector base = spin[i];
        ThreeVector sum(0.,0.,0.);
        if(x>0) {
          UINT4m j = i-1;
          if(Ms_inverse[j]!=0.0) sum = (spin[j] - base);
        }
        if(x<xdim-1) {
          UINT4m j = i+1;
          if(Ms_inverse[j]!=0.0) {
            sum += (spin[j] - base);
          }
        }
	if(xdim>2) { // m''=0 boundary correction
	  if(x==0 || x==xdim-2) {
	    UINT4m j = i+1;
	    if(Ms_inverse[j]!=0.0) {
	      sum -= 0.5*(spin[j] - base);
	    }
	  }
	  if(x==1 || x==xdim-1) {
	    UINT4m j = i-1;
	    if(Ms_inverse[j]!=0.0) {
	      sum -= 0.5*(spin[j] - base);
	    }
	  }
	}
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0) {
          UINT4m j = i-xdim;
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }
        if(y<ydim-1) {
          UINT4m j = i+xdim;
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j] - base);
          }
        }
	if(ydim>2) { // m''=0 boundary correction
	  if(y==0 || y==ydim-2) {
	    UINT4m j = i+xdim;
	    if(Ms_inverse[j]!=0.0) {
	      temp -= 0.5*(spin[j] - base);
	    }
	  }
	  if(y==1 || y==ydim-1) {
	    UINT4m j = i-xdim;
	    if(Ms_inverse[j]!=0.0) {
	      temp -= 0.5*(spin[j] - base);
	    }
	  }
	}
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0) {
          UINT4m j = i-xydim;
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }
        if(z<zdim-1) {
          UINT4m j = i+xydim;
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j]- base);
          }
        }
	if(zdim>2) {  // m''=0 boundary correction
	  if(z==0 || z==zdim-2) {
	    UINT4m j = i+xydim;
	    if(Ms_inverse[j]!=0.0) {
	      temp -= 0.5*(spin[j] - base);
	    }
	  }
	  if(z==1 || z==zdim-1) {
	    UINT4m j = i-xydim;
	    if(Ms_inverse[j]!=0.0) {
	      temp -= 0.5*(spin[j] - base);
	    }
	  }
	}
        sum += wgtz*temp;

        field[i] = (hcoef*Msii) * sum;
        energy[i] = (sum * base);
      }
    }
  }
}

void
Klm_UniformExchange::CalcEnergy6NgbrFree
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  const REAL8m hcoef = -2/MU0;
  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
        UINT4m i = mesh->Index(x,y,z); // Get base linear address

        REAL8m Msii = Ms_inverse[i];
        if(Msii == 0.0) {
          energy[i]=0.0;
          field[i].Set(0.,0.,0.);
          continue;
        }

        ThreeVector base = spin[i];
        ThreeVector sum(0.,0.,0.);
        if(x>0) {
          UINT4m j = i-1;
          if(Ms_inverse[j]!=0.0) sum = (spin[j] - base);
        }
        if(x<xdim-1) {
          UINT4m j = i+1;
          if(Ms_inverse[j]!=0.0) {
            sum += (spin[j] - base);
          }
        }
	if(xdim>2) { // Free boundary correction
	  if(x==0 || x==xdim-2) {
	    UINT4m j = i+1;
	    if(Ms_inverse[j]!=0.0) {
	      sum += 0.5*(spin[j] - base);
	    }
	  }
	  if(x==1 || x==xdim-1) {
	    UINT4m j = i-1;
	    if(Ms_inverse[j]!=0.0) {
	      sum += 0.5*(spin[j] - base);
	    }
	  }
	}
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0) {
          UINT4m j = i-xdim;
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }
        if(y<ydim-1) {
          UINT4m j = i+xdim;
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j] - base);
          }
        }
	if(ydim>2) { // Free boundary correction
	  if(y==0 || y==ydim-2) {
	    UINT4m j = i+xdim;
	    if(Ms_inverse[j]!=0.0) {
	      temp += 0.5*(spin[j] - base);
	    }
	  }
	  if(y==1 || y==ydim-1) {
	    UINT4m j = i-xdim;
	    if(Ms_inverse[j]!=0.0) {
	      temp += 0.5*(spin[j] - base);
	    }
	  }
	}
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0) {
          UINT4m j = i-xydim;
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }
        if(z<zdim-1) {
          UINT4m j = i+xydim;
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j]- base);
          }
        }
	if(zdim>2) { // Free boundary correction
	  if(z==0 || z==zdim-2) {
	    UINT4m j = i+xydim;
	    if(Ms_inverse[j]!=0.0) {
	      temp += 0.5*(spin[j] - base);
	    }
	  }
	  if(z==1 || z==zdim-1) {
	    UINT4m j = i-xydim;
	    if(Ms_inverse[j]!=0.0) {
	      temp += 0.5*(spin[j] - base);
	    }
	  }
	}
        sum += wgtz*temp;

        field[i] = (hcoef*Msii) * sum;
        energy[i] = (sum * base);
      }
    }
  }
}

void
Klm_UniformExchange::CalcEnergy6NgbrMirror
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
        UINT4m i = mesh->Index(x,y,z); // Get base linear address

	if(0 == Ms_inverse[i]) {
	    energy[i] = 0.0;
	    field[i].Set(0.,0.,0.);
	    continue;
	}
	const REAL8m hmult = (-2/MU0) * Ms_inverse[i];

        ThreeVector base = spin[i];

        ThreeVector sum(0.,0.,0.);
        if(x>0) {
          UINT4m j = i-1;
          if(Ms_inverse[j]!=0.0) sum = (spin[j] - base);
        }
        if(x<xdim-1) {
          UINT4m j = i+1;
          if(Ms_inverse[j]!=0.0) {
            sum += (spin[j] - base);
          }
        }
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0) {
          UINT4m j = i-xdim;
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }
        if(y<ydim-1) {
          UINT4m j = i+xdim;
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j] - base);
          }
        }
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0) {
          UINT4m j = i-xydim;
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }
        if(z<zdim-1) {
          UINT4m j = i+xydim;
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j]- base);
          }
        }
        sum += wgtz*temp;

        energy[i] = sum * base;
        field[i] = hmult * sum;
      }
    }
  }
}

//kl(m) based on CalcEnergy6NgbrMirror
void
Klm_UniformExchange::CalcEnergy6NgbrZPeriodicCond
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
        UINT4m i = mesh->Index(x,y,z); // Get base linear address

	if(0 == Ms_inverse[i]) {
	    energy[i] = 0.0;
	    field[i].Set(0.,0.,0.);
	    continue;
	}
	const REAL8m hmult = (-2/MU0) * Ms_inverse[i];

        ThreeVector base = spin[i];

        ThreeVector sum(0.,0.,0.);
        if(x>0) {
          UINT4m j = i-1;
          if(Ms_inverse[j]!=0.0) sum = (spin[j] - base);
        }
        if(x<xdim-1) {
          UINT4m j = i+1;
          if(Ms_inverse[j]!=0.0) {
            sum += (spin[j] - base);
          }
        }
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0) {
          UINT4m j = i-xdim;
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }
        if(y<ydim-1) {
          UINT4m j = i+xdim;
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j] - base);
          }
        }
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0) {
          UINT4m j = i-xydim;
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }
        //kl(m)
        if(z==0) {
          UINT4m j = i+(zdim-1)*xydim; // The neighbor 'on the other side'; Index(x,y,zdim-1)
          if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
        }        
        if(z<zdim-1) {
          UINT4m j = i+xydim;
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j]- base);
          }
        }
        //kl(m)
        if(z==zdim-1) {
          UINT4m j = i%xydim; // The neighbor 'on the other side'; Index(x,y,0)
          if(Ms_inverse[j]!=0.0) {
            temp += (spin[j]- base);
          }
        }        
        sum += wgtz*temp;

        energy[i] = sum * base;
        field[i] = hmult * sum;
      }
    }
  }
} //kl(m)

void
Klm_UniformExchange::CalcEnergy6NgbrMirror_lex
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  REAL8m wgtx = lex*lex/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = lex*lex/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = lex*lex/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
        UINT4m i = mesh->Index(x,y,z); // Get base linear address

	if(0 == Ms[i]) {
	    energy[i] = 0.0;
	    field[i].Set(0.,0.,0.);
	    continue;
	}
	const REAL8m emult = (-0.5*MU0)*Ms[i];

        ThreeVector base = spin[i];

        ThreeVector sum(0.,0.,0.);
        if(x>0) {
          UINT4m j = i-1;
          sum = Ms[j]*(spin[j] - base);
        }
        if(x<xdim-1) {
          UINT4m j = i+1;
	  sum += Ms[j]*(spin[j] - base);
        }
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0) {
          UINT4m j = i-xdim;
          temp = Ms[j]*(spin[j] - base);
        }
        if(y<ydim-1) {
          UINT4m j = i+xdim;
	  temp += Ms[j]*(spin[j] - base);
        }
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0) {
          UINT4m j = i-xydim;
          temp = Ms[j]*(spin[j] - base);
        }
        if(z<zdim-1) {
          UINT4m j = i+xydim;
	  temp += Ms[j]*(spin[j]- base);
        }
        sum += wgtz*temp;

        field[i] = sum;
        energy[i] = emult * (sum * base);
      }
    }
  }
}

//kl(m) based on CalcEnergy6NgbrMirror_lex
void
Klm_UniformExchange::CalcEnergy6NgbrZPeriodicCond_lex
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  REAL8m wgtx = lex*lex/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = lex*lex/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = lex*lex/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
        UINT4m i = mesh->Index(x,y,z); // Get base linear address

	if(0 == Ms[i]) {
	    energy[i] = 0.0;
	    field[i].Set(0.,0.,0.);
	    continue;
	}
	const REAL8m emult = (-0.5*MU0)*Ms[i];

        ThreeVector base = spin[i];

        ThreeVector sum(0.,0.,0.);
        if(x>0) {
          UINT4m j = i-1;
          sum = Ms[j]*(spin[j] - base);
        }
        if(x<xdim-1) {
          UINT4m j = i+1;
	  sum += Ms[j]*(spin[j] - base);
        }
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0) {
          UINT4m j = i-xdim;
          temp = Ms[j]*(spin[j] - base);
        }
        if(y<ydim-1) {
          UINT4m j = i+xdim;
	  temp += Ms[j]*(spin[j] - base);
        }
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0) {
          UINT4m j = i-xydim;
          temp = Ms[j]*(spin[j] - base);
        }
        //kl(m)
        if(z==0) {
          UINT4m j = i+(zdim-1)*xydim; // The neighbor 'on the other side'; Index(x,y,zdim-1)
          temp = Ms[j]*(spin[j] - base);
        }             
        if(z<zdim-1) {
          UINT4m j = i+xydim;
	  temp += Ms[j]*(spin[j]- base);
        }
        //kl(m)
        if(z==zdim-1) {
          UINT4m j = i%xydim; // The neighbor 'on the other side'; Index(x,y,0)
	  temp += Ms[j]*(spin[j]- base);
        }          
        sum += wgtz*temp;

        field[i] = sum;
        energy[i] = emult * (sum * base);
      }
    }
  }
}

void
Klm_UniformExchange::CalcEnergy6NgbrMirrorStd
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{ // This is the same as ::CalcEnergy6NgbrMirror,
  // except that the spin at cell i is *not* subtracted
  // from the field at spin i.  This is the "canonical"
  // or "standard" way that exchange energy is usually
  // written in the literature.  The numerics of this
  // formulation are inferior to the other, in the case
  // where spin i and neighboring spin j are nearly
  // parallel.
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
        UINT4m i = mesh->Index(x,y,z); // Get base linear address

	if(0 == Ms_inverse[i]) {
	    energy[i] = 0.0;
	    field[i].Set(0.,0.,0.);
	    continue;
	}
	const REAL8m hmult = (-2/MU0) * Ms_inverse[i];

        ThreeVector sum(0.,0.,0.);
        if(x>0) {
          UINT4m j = i-1;
          if(Ms_inverse[j]!=0.0) sum = spin[j];
        }
        if(x<xdim-1) {
          UINT4m j = i+1;
          if(Ms_inverse[j]!=0.0) {
            sum += spin[j];
          }
        }
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0) {
          UINT4m j = i-xdim;
          if(Ms_inverse[j]!=0.0) temp = spin[j];
        }
        if(y<ydim-1) {
          UINT4m j = i+xdim;
          if(Ms_inverse[j]!=0.0) {
            temp += spin[j];
          }
        }
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0) {
          UINT4m j = i-xydim;
          if(Ms_inverse[j]!=0.0) temp = spin[j];
        }
        if(z<zdim-1) {
          UINT4m j = i+xydim;
          if(Ms_inverse[j]!=0.0) {
            temp += spin[j];
          }
        }
        sum += wgtz*temp;

        energy[i] = sum * spin[i];
        field[i] = hmult * sum;
      }
    }
  }
}

REAL8m
Klm_UniformExchange::ComputeAngle
(const ThreeVector& u1,
 const ThreeVector& u2) const
{ // Compute angle between unit vectors u1 and u2.
  // Note 1: The return value is in the range [0,pi/2],
  //         and in particular does not distinguish between
  //         positive and negative angles.  Thus,
  //         ComputeAngle(v1,v2) = ComputeAngle(v2,v1).
  // Note 2: asin(u1 x u2) will return an angle in the range
  //         -pi/2 to pi/2, and so fails in the case where the
  //         angle is larger than pi/2.  Instead, we compute
  //         the arcsin of half the angle, and then double.
  ThreeVector d = u1 - u2;
  return 2*asin(0.5*sqrt(d.MagSq()));
}

void
Klm_UniformExchange::CalcEnergy6NgbrBigAngMirror
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  const REAL8m hcoef = -2/MU0;
  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
        UINT4m i = mesh->Index(x,y,z); // Get base linear address

        REAL8m Msii = Ms_inverse[i];
        if(Msii == 0.0) {
          energy[i]=0.0;
          field[i].Set(0.,0.,0.);
          continue;
        }

        ThreeVector base = spin[i];

        ThreeVector sum(0.,0.,0.);
        REAL8m Esum = 0.0;
        if(x>0) {
          UINT4m j = i-1;
          if(Ms_inverse[j]!=0.0) {
            REAL8m theta = ComputeAngle(spin[j],base);
            REAL8m sintheta = sin(theta);
            Esum = theta*theta;
            ThreeVector vtmp = spin[j] - base;
            if(sintheta>0.0) vtmp *= theta/sintheta;
            sum = vtmp;
          }
        }
        if(x<xdim-1) {
          UINT4m j = i+1;
          if(Ms_inverse[j]!=0.0) {
            REAL8m theta = ComputeAngle(spin[j],base);
            REAL8m sintheta = sin(theta);
            Esum += theta*theta;
            ThreeVector vtmp = spin[j] - base;
            if(sintheta>0.0) vtmp *= theta/sintheta;
            sum += vtmp;
          }
        }
        Esum *= wgtx;
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        REAL8m Etemp = 0.0;
        if(y>0) {
          UINT4m j = i-xdim;
          if(Ms_inverse[j]!=0.0) {
            REAL8m theta = ComputeAngle(spin[j],base);
            REAL8m sintheta = sin(theta);
            Etemp = theta*theta;
            ThreeVector vtmp = spin[j] - base;
            if(sintheta>0.0) vtmp *= theta/sintheta;
            temp = vtmp;
          }
        }
        if(y<ydim-1) {
          UINT4m j = i+xdim;
          if(Ms_inverse[j]!=0.0) {
            REAL8m theta = ComputeAngle(spin[j],base);
            REAL8m sintheta = sin(theta);
            Etemp += theta*theta;
            ThreeVector vtmp = spin[j] - base;
            if(sintheta>0.0) vtmp *= theta/sintheta;
            temp += vtmp;
          }
        }
        Esum += wgty*Etemp;
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        Etemp = 0.0;
        if(z>0) {
          UINT4m j = i-xydim;
          if(Ms_inverse[j]!=0.0) {
            REAL8m theta = ComputeAngle(spin[j],base);
            REAL8m sintheta = sin(theta);
            Etemp = theta*theta;
            ThreeVector vtmp = spin[j] - base;
            if(sintheta>0.0) vtmp *= theta/sintheta;
            temp = vtmp;
          }
        }
        if(z<zdim-1) {
          UINT4m j = i+xydim;
          if(Ms_inverse[j]!=0.0) {
            REAL8m theta = ComputeAngle(spin[j],base);
            REAL8m sintheta = sin(theta);
            Etemp += theta*theta;
            ThreeVector vtmp = spin[j] - base;
            if(sintheta>0.0) vtmp *= theta/sintheta;
            temp += vtmp;
          }
        }
        Esum += wgtz*Etemp;
        sum += wgtz*temp;

        field[i] = (hcoef*Msii) * sum;
        energy[i] = -0.5 * Esum;
      }
    }
  }
}

#ifdef SKIP_PROBLEM_CODE
void
Klm_UniformExchange::CalcEnergy12NgbrFree
(const Oxs_MeshValue<ThreeVector>&,
 const Oxs_MeshValue<REAL8m>&,
 const Oxs_RectangularMesh*,
 Oxs_MeshValue<REAL8m>&,
 Oxs_MeshValue<ThreeVector>&) const
{
  throw Oxs_Ext::Error(this,"Klm_UniformExchange::CalcEnergy12NgbrFree()"
		       " not supported in this build.  See file"
		       " oommf/app/oxs/ext/uniformexchange.cc for details.");
}

void
Klm_UniformExchange::CalcEnergy12NgbrZD1
(const Oxs_MeshValue<ThreeVector>&,
 const Oxs_MeshValue<REAL8m>&,
 const Oxs_RectangularMesh*,
 Oxs_MeshValue<REAL8m>&,
 Oxs_MeshValue<ThreeVector>&) const
{
  throw Oxs_Ext::Error(this,"Klm_UniformExchange::CalcEnergy12NgbrZD1()"
		       " not supported in this build.  See file"
		       " oommf/app/oxs/ext/uniformexchange.cc for details.");
}

#else // SKIP_PROBLEM_CODE

void
Klm_UniformExchange::CalcEnergy12NgbrFree
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  const REAL8m hcoef = -2/MU0;
  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX()*1152.);
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY()*1152.);
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ()*1152.);

  // See mjd's NOTES II, 12-Aug-2002, p186-192
  // EXTRA NOTE: Beware that "field" computed here is
  //  -grad(E)/(mu_0*cellvolume), which is not a good representation
  //  for H.  See mjd's NOTES II, 9-Aug-2002, p183.
  if((1<xdim && xdim<10) || (1<ydim && ydim<10) || (1<zdim && zdim<10)) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
		"Each dimension must be ==1 or >=10 for 12NgbrFree kernel."
		" (Actual dimensions: xdim=%d, ydim=%d, zdim=%d.)",
		xdim,ydim,zdim);
    throw Oxs_Ext::Error(this,buf);
  }
  // Note: Integral weights ai, bj, and ck are derived in mjd
  // NOTES II, 5-Aug-2002, p178-181.
  for(UINT4m z=0;z<zdim;z++) {
    REAL8m ck=1.0;
    if(z==0 || z==zdim-1) ck=26./24.;
    if(z==1 || z==zdim-2) ck=21./24.;
    if(z==2 || z==zdim-3) ck=25./24.;

    for(UINT4m y=0;y<ydim;y++) {
      REAL8m bj=1.0;
      if(y==0 || y==ydim-1) bj=26./24.;
      if(y==1 || y==ydim-2) bj=21./24.;
      if(y==2 || y==ydim-3) bj=25./24.;

      for(UINT4m x=0;x<xdim;x++) {
	UINT4m i = mesh->Index(x,y,z); // Get base linear address

	REAL8m Msii = Ms_inverse[i];
	if(Msii == 0.0) {
	  energy[i]=0.0;
	  field[i].Set(0.,0.,0.);
	  continue;
	}

	REAL8m ai=1.0;
	if(x==0 || x==xdim-1) ai=26./24.;
	if(x==1 || x==xdim-2) ai=21./24.;
	if(x==2 || x==xdim-3) ai=25./24.;

	ThreeVector sum,tsum;

	// Dxx
        if(xdim==1) {
          tsum.Set(0,0,0);
        } else if(x>4 && x<xdim-5) {
	  tsum  =    -96*spin[i-2];
	  tsum +=   1536*spin[i-1];
	  tsum +=  -2880*spin[i];
	  tsum +=   1536*spin[i+1];
	  tsum +=    -96*spin[i+2];
	} else if(x==0) {
	  tsum  =  -6125*spin[i];
	  tsum +=  11959*spin[i+1];
	  tsum +=  -8864*spin[i+2];
	  tsum +=   3613*spin[i+3];
	  tsum +=   -583*spin[i+4];
	} else if(x==1) {
	  tsum  =  11959*spin[i-1];
	  tsum += -25725*spin[i];
	  tsum +=  20078*spin[i+1];
	  tsum +=  -7425*spin[i+2];
	  tsum +=   1113*spin[i+3];
	} else if(x==2) {
	  tsum  =  -8864*spin[i-2];
	  tsum +=  20078*spin[i-1];
	  tsum += -17175*spin[i];
	  tsum +=   6752*spin[i+1];
	  tsum +=   -791*spin[i+2];
	} else if(x==3) {
	  tsum  =   3613*spin[i-3];
	  tsum +=  -7425*spin[i-2];
	  tsum +=   6752*spin[i-1];
	  tsum +=  -4545*spin[i];
	  tsum +=   1701*spin[i+1];
	  tsum +=    -96*spin[i+2];
	} else if(x==4) {
	  tsum  =   -583*spin[i-4];
	  tsum +=   1113*spin[i-3];
	  tsum +=   -791*spin[i-2];
	  tsum +=   1701*spin[i-1];
	  tsum +=  -2880*spin[i];
	  tsum +=   1536*spin[i+1];
	  tsum +=    -96*spin[i+2];
	} else if(x==xdim-5) {
	  tsum  =    -96*spin[i-2];
	  tsum +=   1536*spin[i-1];
	  tsum +=  -2880*spin[i];
	  tsum +=   1701*spin[i+1];
	  tsum +=   -791*spin[i+2];
	  tsum +=   1113*spin[i+3];
	  tsum +=   -583*spin[i+4];
	} else if(x==xdim-4) {
	  tsum  =    -96*spin[i-2];
	  tsum +=   1701*spin[i-1];
	  tsum +=  -4545*spin[i];
	  tsum +=   6752*spin[i+1];
	  tsum +=  -7425*spin[i+2];
	  tsum +=   3613*spin[i+3];
	} else if(x==xdim-3) {
	  tsum  =   -791*spin[i-2];
	  tsum +=   6752*spin[i-1];
	  tsum += -17175*spin[i];
	  tsum +=  20078*spin[i+1];
	  tsum +=  -8864*spin[i+2];
	} else if(x==xdim-2) {
	  tsum  =   1113*spin[i-3];
	  tsum +=  -7425*spin[i-2];
	  tsum +=  20078*spin[i-1];
	  tsum += -25725*spin[i];
	  tsum +=  11959*spin[i+1];
	} else if(x==xdim-1) {
	  tsum  =   -583*spin[i-4];
	  tsum +=   3613*spin[i-3];
	  tsum +=  -8864*spin[i-2];
	  tsum +=  11959*spin[i-1];
	  tsum +=  -6125*spin[i];
	}
	sum = (bj*ck*wgtx)*tsum; // ai built into above coefficients.

	// Dyy
        if(ydim==1) {
          tsum.Set(0,0,0);
        } else if(y>4 && y<ydim-5) {
	  tsum  =    -96*spin[i-2*xdim];
	  tsum +=   1536*spin[i-xdim];
	  tsum +=  -2880*spin[i];
	  tsum +=   1536*spin[i+xdim];
	  tsum +=    -96*spin[i+2*xdim];
	} else if(y==0) {
	  tsum  =  -6125*spin[i];
	  tsum +=  11959*spin[i+xdim];
	  tsum +=  -8864*spin[i+2*xdim];
	  tsum +=   3613*spin[i+3*xdim];
	  tsum +=   -583*spin[i+4*xdim];
	} else if(y==1) {
	  tsum  =  11959*spin[i-xdim];
	  tsum += -25725*spin[i];
	  tsum +=  20078*spin[i+xdim];
	  tsum +=  -7425*spin[i+2*xdim];
	  tsum +=   1113*spin[i+3*xdim];
	} else if(y==2) {
	  tsum  =  -8864*spin[i-2*xdim];
	  tsum +=  20078*spin[i-xdim];
	  tsum += -17175*spin[i];
	  tsum +=   6752*spin[i+xdim];
	  tsum +=   -791*spin[i+2*xdim];
	} else if(y==3) {
	  tsum  =   3613*spin[i-3*xdim];
	  tsum +=  -7425*spin[i-2*xdim];
	  tsum +=   6752*spin[i-xdim];
	  tsum +=  -4545*spin[i];
	  tsum +=   1701*spin[i+xdim];
	  tsum +=    -96*spin[i+2*xdim];
	} else if(y==4) {
	  tsum  =   -583*spin[i-4*xdim];
	  tsum +=   1113*spin[i-3*xdim];
	  tsum +=   -791*spin[i-2*xdim];
	  tsum +=   1701*spin[i-xdim];
	  tsum +=  -2880*spin[i];
	  tsum +=   1536*spin[i+xdim];
	  tsum +=    -96*spin[i+2*xdim];
	} else if(y==ydim-5) {
	  tsum  =    -96*spin[i-2*xdim];
	  tsum +=   1536*spin[i-xdim];
	  tsum +=  -2880*spin[i];
	  tsum +=   1701*spin[i+xdim];
	  tsum +=   -791*spin[i+2*xdim];
	  tsum +=   1113*spin[i+3*xdim];
	  tsum +=   -583*spin[i+4*xdim];
	} else if(y==ydim-4) {
	  tsum  =    -96*spin[i-2*xdim];
	  tsum +=   1701*spin[i-xdim];
	  tsum +=  -4545*spin[i];
	  tsum +=   6752*spin[i+xdim];
	  tsum +=  -7425*spin[i+2*xdim];
	  tsum +=   3613*spin[i+3*xdim];
	} else if(y==ydim-3) {
	  tsum  =   -791*spin[i-2*xdim];
	  tsum +=   6752*spin[i-xdim];
	  tsum += -17175*spin[i];
	  tsum +=  20078*spin[i+xdim];
	  tsum +=  -8864*spin[i+2*xdim];
	} else if(y==ydim-2) {
	  tsum  =   1113*spin[i-3*xdim];
	  tsum +=  -7425*spin[i-2*xdim];
	  tsum +=  20078*spin[i-xdim];
	  tsum += -25725*spin[i];
	  tsum +=  11959*spin[i+xdim];
	} else if(y==ydim-1) {
	  tsum  =   -583*spin[i-4*xdim];
	  tsum +=   3613*spin[i-3*xdim];
	  tsum +=  -8864*spin[i-2*xdim];
	  tsum +=  11959*spin[i-xdim];
	  tsum +=  -6125*spin[i];
	}
	sum += (ai*ck*wgty)*tsum; // bj built into above coefficients.

	// Dzz
        if(zdim==1) {
          tsum.Set(0,0,0);
        } else if(z>4 && z<zdim-5) {
	  tsum  =    -96*spin[i-2*xydim];
	  tsum +=   1536*spin[i-xydim];
	  tsum +=  -2880*spin[i];
	  tsum +=   1536*spin[i+xydim];
	  tsum +=    -96*spin[i+2*xydim];
	} else if(z==0) {
	  tsum  =  -6125*spin[i];
	  tsum +=  11959*spin[i+xydim];
	  tsum +=  -8864*spin[i+2*xydim];
	  tsum +=   3613*spin[i+3*xydim];
	  tsum +=   -583*spin[i+4*xydim];
	} else if(z==1) {
	  tsum  =  11959*spin[i-xydim];
	  tsum += -25725*spin[i];
	  tsum +=  20078*spin[i+xydim];
	  tsum +=  -7425*spin[i+2*xydim];
	  tsum +=   1113*spin[i+3*xydim];
	} else if(z==2) {
	  tsum  =  -8864*spin[i-2*xydim];
	  tsum +=  20078*spin[i-xydim];
	  tsum += -17175*spin[i];
	  tsum +=   6752*spin[i+xydim];
	  tsum +=   -791*spin[i+2*xydim];
	} else if(z==3) {
	  tsum  =   3613*spin[i-3*xydim];
	  tsum +=  -7425*spin[i-2*xydim];
	  tsum +=   6752*spin[i-xydim];
	  tsum +=  -4545*spin[i];
	  tsum +=   1701*spin[i+xydim];
	  tsum +=    -96*spin[i+2*xydim];
	} else if(z==4) {
	  tsum  =   -583*spin[i-4*xydim];
	  tsum +=   1113*spin[i-3*xydim];
	  tsum +=   -791*spin[i-2*xydim];
	  tsum +=   1701*spin[i-xydim];
	  tsum +=  -2880*spin[i];
	  tsum +=   1536*spin[i+xydim];
	  tsum +=    -96*spin[i+2*xydim];
	} else if(z==zdim-5) {
	  tsum  =    -96*spin[i-2*xydim];
	  tsum +=   1536*spin[i-xydim];
	  tsum +=  -2880*spin[i];
	  tsum +=   1701*spin[i+xydim];
	  tsum +=   -791*spin[i+2*xydim];
	  tsum +=   1113*spin[i+3*xydim];
	  tsum +=   -583*spin[i+4*xydim];
	} else if(z==zdim-4) {
	  tsum  =    -96*spin[i-2*xydim];
	  tsum +=   1701*spin[i-xydim];
	  tsum +=  -4545*spin[i];
	  tsum +=   6752*spin[i+xydim];
	  tsum +=  -7425*spin[i+2*xydim];
	  tsum +=   3613*spin[i+3*xydim];
	} else if(z==zdim-3) {
	  tsum  =   -791*spin[i-2*xydim];
	  tsum +=   6752*spin[i-xydim];
	  tsum += -17175*spin[i];
	  tsum +=  20078*spin[i+xydim];
	  tsum +=  -8864*spin[i+2*xydim];
	} else if(z==zdim-2) {
	  tsum  =   1113*spin[i-3*xydim];
	  tsum +=  -7425*spin[i-2*xydim];
	  tsum +=  20078*spin[i-xydim];
	  tsum += -25725*spin[i];
	  tsum +=  11959*spin[i+xydim];
	} else if(z==zdim-1) {
	  tsum  =   -583*spin[i-4*xydim];
	  tsum +=   3613*spin[i-3*xydim];
	  tsum +=  -8864*spin[i-2*xydim];
	  tsum +=  11959*spin[i-xydim];
	  tsum +=  -6125*spin[i];
	}
	sum += (ai*bj*wgtz)*tsum; // ck built into above coefficients.

	field[i] = (hcoef*Msii) * sum;
	energy[i] = (sum * spin[i]);
      }
    }
  }
}

void
Klm_UniformExchange::CalcEnergy12NgbrZD1
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  const REAL8m hcoef = -2/MU0;
  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX()*240768.);
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY()*240768.);
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ()*240768.);

  // See mjd's NOTES III, 7/30-Jan-2003, p27-35
  // EXTRA NOTE: Beware that "field" computed here is
  //  -grad(E)/(mu_0*cellvolume), which is not a good representation
  //  for H.  See mjd's NOTES II, 9-Aug-2002, p183.
  if((1<xdim && xdim<10) || (1<ydim && ydim<10)
     || (1<zdim && zdim<10)) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
	"Each dimension must be ==1 or >=10 for 12ngbrZD1 kernel."
	" (Actual dimensions: xdim=%d, ydim=%d, zdim=%d.)",
	xdim,ydim,zdim);
    throw Oxs_Ext::Error(this,buf);
  }
  // Note: Integral weights ai, bj, and ck are derived in mjd
  // NOTES II, 5-Aug-2002, p178-181.
  for(UINT4m z=0;z<zdim;z++) {
    REAL8m ck=1.0;
    if(zdim>1) {
      if(z==0 || z==zdim-1) ck=26./24.;
      if(z==1 || z==zdim-2) ck=21./24.;
      if(z==2 || z==zdim-3) ck=25./24.;
    }

    for(UINT4m y=0;y<ydim;y++) {
      REAL8m bj=1.0;
      if(ydim>1) {
	if(y==0 || y==ydim-1) bj=26./24.;
	if(y==1 || y==ydim-2) bj=21./24.;
	if(y==2 || y==ydim-3) bj=25./24.;
      }

      for(UINT4m x=0;x<xdim;x++) {
	UINT4m i = mesh->Index(x,y,z); // Get base linear address

	REAL8m Msii = Ms_inverse[i];
	if(Msii == 0.0) {
	  energy[i]=0.0;
	  field[i].Set(0.,0.,0.);
	  continue;
	}

	REAL8m ai=1.0;
	if(xdim>1) {
	  if(x==0 || x==xdim-1) ai=26./24.;
	  if(x==1 || x==xdim-2) ai=21./24.;
	  if(x==2 || x==xdim-3) ai=25./24.;
	}

	ThreeVector sum,tsum;
	sum.Set(0.,0.,0.);

	// Dxx
	if(xdim>1) {
	  if(x>4 && x<xdim-5) {
	    tsum  =  -20064*spin[i-2];
	    tsum +=  321024*spin[i-1];
	    tsum += -601920*spin[i];
	    tsum +=  321024*spin[i+1];
	    tsum +=  -20064*spin[i+2];
	  } else if(x==0) {
	    tsum  = -325703*spin[i];
	    tsum +=  353313*spin[i+1];
	    tsum +=  -27610*spin[i+2];
	  } else if(x==1) {
	    tsum  =  353313*spin[i-1];
	    tsum += -643747*spin[i];
	    tsum +=  309643*spin[i+1];
	    tsum +=  -19209*spin[i+2];
	  } else if(x==2) {
	    tsum  =  -27610*spin[i-2];
	    tsum +=  309643*spin[i-1];
	    tsum += -589263*spin[i];
	    tsum +=  327712*spin[i+1];
	    tsum +=  -20482*spin[i+2];
	  } else if(x==3) {
	    tsum  =  -19209*spin[i-2];
	    tsum +=  327712*spin[i-1];
	    tsum += -609463*spin[i];
	    tsum +=  321024*spin[i+1];
	    tsum +=  -20064*spin[i+2];
	  } else if(x==4) {
	    tsum  =  -20482*spin[i-2];
	    tsum +=  321024*spin[i-1];
	    tsum += -601502*spin[i];
	    tsum +=  321024*spin[i+1];
	    tsum +=  -20064*spin[i+2];
	  } else if(x==xdim-5) {
	    tsum  =  -20064*spin[i-2];
	    tsum +=  321024*spin[i-1];
	    tsum += -601502*spin[i];
	    tsum +=  321024*spin[i+1];
	    tsum +=  -20482*spin[i+2];
	  } else if(x==xdim-4) {
	    tsum  =  -20064*spin[i-2];
	    tsum +=  321024*spin[i-1];
	    tsum += -609463*spin[i];
	    tsum +=  327712*spin[i+1];
	    tsum +=  -19209*spin[i+2];
	  } else if(x==xdim-3) {
	    tsum  =  -20482*spin[i-2];
	    tsum +=  327712*spin[i-1];
	    tsum += -589263*spin[i];
	    tsum +=  309643*spin[i+1];
	    tsum +=  -27610*spin[i+2];
	  } else if(x==xdim-2) {
	    tsum  =  -19209*spin[i-2];
	    tsum +=  309643*spin[i-1];
	    tsum += -643747*spin[i];
	    tsum +=  353313*spin[i+1];
	  } else if(x==xdim-1) {
	    tsum  =  -27610*spin[i-2];
	    tsum +=  353313*spin[i-1];
	    tsum += -325703*spin[i];
	  }
	  sum = (bj*ck*wgtx)*tsum; // ai built into above coefficients.
	}

	// Dyy
	if(ydim>1) {
	  if(y>4 && y<ydim-5) {
	    tsum  =  -20064*spin[i-2*xdim];
	    tsum +=  321024*spin[i-xdim];
	    tsum += -601920*spin[i];
	    tsum +=  321024*spin[i+xdim];
	    tsum +=  -20064*spin[i+2*xdim];
	  } else if(y==0) {
	    tsum  = -325703*spin[i];
	    tsum +=  353313*spin[i+xdim];
	    tsum +=  -27610*spin[i+2*xdim];
	  } else if(y==1) {
	    tsum  =  353313*spin[i-xdim];
	    tsum += -643747*spin[i];
	    tsum +=  309643*spin[i+xdim];
	    tsum +=  -19209*spin[i+2*xdim];
	  } else if(y==2) {
	    tsum  =  -27610*spin[i-2*xdim];
	    tsum +=  309643*spin[i-xdim];
	    tsum += -589263*spin[i];
	    tsum +=  327712*spin[i+xdim];
	    tsum +=  -20482*spin[i+2*xdim];
	  } else if(y==3) {
	    tsum  =  -19209*spin[i-2*xdim];
	    tsum +=  327712*spin[i-xdim];
	    tsum += -609463*spin[i];
	    tsum +=  321024*spin[i+xdim];
	    tsum +=  -20064*spin[i+2*xdim];
	  } else if(y==4) {
	    tsum  =  -20482*spin[i-2*xdim];
	    tsum +=  321024*spin[i-xdim];
	    tsum += -601502*spin[i];
	    tsum +=  321024*spin[i+xdim];
	    tsum +=  -20064*spin[i+2*xdim];
	  } else if(y==ydim-5) {
	    tsum  =  -20064*spin[i-2*xdim];
	    tsum +=  321024*spin[i-xdim];
	    tsum += -601502*spin[i];
	    tsum +=  321024*spin[i+xdim];
	    tsum +=  -20482*spin[i+2*xdim];
	  } else if(y==ydim-4) {
	    tsum  =  -20064*spin[i-2*xdim];
	    tsum +=  321024*spin[i-xdim];
	    tsum += -609463*spin[i];
	    tsum +=  327712*spin[i+xdim];
	    tsum +=  -19209*spin[i+2*xdim];
	  } else if(y==ydim-3) {
	    tsum  =  -20482*spin[i-2*xdim];
	    tsum +=  327712*spin[i-xdim];
	    tsum += -589263*spin[i];
	    tsum +=  309643*spin[i+xdim];
	    tsum +=  -27610*spin[i+2*xdim];
	  } else if(y==ydim-2) {
	    tsum  =  -19209*spin[i-2*xdim];
	    tsum +=  309643*spin[i-xdim];
	    tsum += -643747*spin[i];
	    tsum +=  353313*spin[i+xdim];
	  } else if(y==ydim-1) {
	    tsum  =  -27610*spin[i-2*xdim];
	    tsum +=  353313*spin[i-xdim];
	    tsum += -325703*spin[i];
	  }
	  sum += (ai*ck*wgty)*tsum; // bj built into above coefficients.
	}

	// Dzz
	if(zdim>1) {
	  if(z>4 && z<zdim-5) {
	    tsum  =  -20064*spin[i-2*xydim];
	    tsum +=  321024*spin[i-xydim];
	    tsum += -601920*spin[i];
	    tsum +=  321024*spin[i+xydim];
	    tsum +=  -20064*spin[i+2*xydim];
	  } else if(z==0) {
	    tsum  = -325703*spin[i];
	    tsum +=  353313*spin[i+xydim];
	    tsum +=  -27610*spin[i+2*xydim];
	  } else if(z==1) {
	    tsum  =  353313*spin[i-xydim];
	    tsum += -643747*spin[i];
	    tsum +=  309643*spin[i+xydim];
	    tsum +=  -19209*spin[i+2*xydim];
	  } else if(z==2) {
	    tsum  =  -27610*spin[i-2*xydim];
	    tsum +=  309643*spin[i-xydim];
	    tsum += -589263*spin[i];
	    tsum +=  327712*spin[i+xydim];
	    tsum +=  -20482*spin[i+2*xydim];
	  } else if(z==3) {
	    tsum  =  -19209*spin[i-2*xydim];
	    tsum +=  327712*spin[i-xydim];
	    tsum += -609463*spin[i];
	    tsum +=  321024*spin[i+xydim];
	    tsum +=  -20064*spin[i+2*xydim];
	  } else if(z==4) {
	    tsum  =  -20482*spin[i-2*xydim];
	    tsum +=  321024*spin[i-xydim];
	    tsum += -601502*spin[i];
	    tsum +=  321024*spin[i+xydim];
	    tsum +=  -20064*spin[i+2*xydim];
	  } else if(z==zdim-5) {
	    tsum  =  -20064*spin[i-2*xydim];
	    tsum +=  321024*spin[i-xydim];
	    tsum += -601502*spin[i];
	    tsum +=  321024*spin[i+xydim];
	    tsum +=  -20482*spin[i+2*xydim];
	  } else if(z==zdim-4) {
	    tsum  =  -20064*spin[i-2*xydim];
	    tsum +=  321024*spin[i-xydim];
	    tsum += -609463*spin[i];
	    tsum +=  327712*spin[i+xydim];
	    tsum +=  -19209*spin[i+2*xydim];
	  } else if(z==zdim-3) {
	    tsum  =  -20482*spin[i-2*xydim];
	    tsum +=  327712*spin[i-xydim];
	    tsum += -589263*spin[i];
	    tsum +=  309643*spin[i+xydim];
	    tsum +=  -27610*spin[i+2*xydim];
	  } else if(z==zdim-2) {
	    tsum  =  -19209*spin[i-2*xydim];
	    tsum +=  309643*spin[i-xydim];
	    tsum += -643747*spin[i];
	    tsum +=  353313*spin[i+xydim];
	  } else if(z==zdim-1) {
	    tsum  =  -27610*spin[i-2*xydim];
	    tsum +=  353313*spin[i-xydim];
	    tsum += -325703*spin[i];
	  }
	  sum += (ai*bj*wgtz)*tsum; // ck built into above coefficients.
	}

	field[i] = (hcoef*Msii) * sum;
	energy[i] = (sum * spin[i]);
      }
    }
  }
}
#endif // SKIP_PROBLEM_CODE

void
Klm_UniformExchange::InitCoef_12NgbrZD1
(UINT4m size,
 REAL8m wgt[3],
 Nb_2DArrayWrapper<REAL8m>& coef) const
{
  if(size==0) {
    coef.Free();
    return;
  }

  if(size>10) size=10; // 10x10 is largest special coefficient
  /// array ever needed.

  UINT4m i,j;

  // Allocate memory
  UINT4m tsize1,tsize2;
  coef.GetSize(tsize1,tsize2);
  if(tsize1!=size || tsize2!=size) {
    coef.Free();
    coef.SetSize(size,size);
  }

  // Wipe slate
  for(i=0;i<size;i++) for(j=0;j<size;j++) coef(i,j)=0.0;

  // Note: Integration weights wgt are derived in mjd
  //   NOTES II, 5-Aug-2002, p178-181.  See also NOTES III,
  //   8-May-2003, p56-57.  For dim>=3, the integration is O(h^4)
  //   (global) accurate.  The dim==1 and dim==2 cases are local
  //   O(h^3) accurate (how does one define global accuracy when
  //   there is only 1 or 2 points and the integration limits
  //   depend on h?) in general, but if the integrand f has f'=0
  //   on the boundaries (or f' left bdry = f' right bdry), then
  //   these two cases are in fact O(h^5) locally accurate.
  switch(size) {
  case 0: case 1: case 2: wgt[0]=wgt[1]=wgt[2]=1.0; break;
  case 3:
    wgt[0]=27./24.; wgt[1]=18./24.; wgt[2]=27./24.;
    break;
  case 4:
    wgt[0]=26./24.; wgt[1]=22./24.; wgt[2]=22./24.;
    break;
  case 5:
    wgt[0]=26./24.; wgt[1]=21./24.; wgt[2]=26./24.;
    break;
  default:
    wgt[0]=26./24.; wgt[1]=21./24.; wgt[2]=25./24.;
    break;
  }

  // Size check
  if(size==1) {
    coef(0,0) = 1.0;
    return;
  }
  if(size<5) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
		"Dimension %u too small;"
		" must be 1 or >4 for 12ngbrZD1 kernel.",
		size);
    throw Oxs_Ext::Error(this,buf);
  }

  // Preliminary fill.  See mjd's NOTES III, 29-Jan-2003 p32.
  coef(0,0) =  coef(size-1,size-1) =  -59. /  38.;  // Bdry nodes
  coef(0,1) =  coef(size-1,size-2) =   64. /  38.;
  coef(0,2) =  coef(size-1,size-3) =   -5. /  38.;
  coef(1,0) =  coef(size-2,size-1) =  335. / 264.;
  coef(1,1) =  coef(size-2,size-2) = -669. / 264.;
  coef(1,2) =  coef(size-2,size-3) =  357. / 264.;
  coef(1,3) =  coef(size-2,size-4) =  -23. / 264.;
  for(i=2;i<size-2;i++) {
    coef(i,i-2) = coef(i,i+2) =  -1. / 12.;     // Interior nodes
    coef(i,i-1) = coef(i,i+1) =  16. / 12.;
    coef(i,i)                 = -30. / 12.;
  }

  // Multiply rows by integration weights.  See mjd's NOTES II,
  // 5-Aug-2002 p178, and 8-Aug-2002 p180.
  if(size>2) {
    for(j=0;j<size;j++) coef(0,j)      *= wgt[0];
    for(j=0;j<size;j++) coef(1,j)      *= wgt[1];
    for(j=0;j<size;j++) coef(size-1,j) *= wgt[0];
  }
  if(size>3) for(j=0;j<size;j++) coef(size-2,j) *= wgt[1];
  if(size>4) for(j=0;j<size;j++) coef(2,j)      *= wgt[2];
  if(size>5) for(j=0;j<size;j++) coef(size-3,j) *= wgt[2];
  // For size>=6, interior nodes weights are 1.0.

  // Symmetrize
  for(i=0;i<size;i++) for(j=i+1;j<size;j++) {
    REAL8m tmp = (coef(i,j) + coef(j,i))/2.0;
    coef(i,j) = coef(j,i) = tmp;
  }

  // For improved numerics, adjust diagonal elements so rows
  // are zero-sum
  for(i=0;i<size;i++) {
    REAL8m rowsum=0.0;
    for(j=0;j<size;j++) rowsum += coef(i,j);
    coef(i,i) -= rowsum;
  }

  // That's all folks!
}

void
Klm_UniformExchange::CalcEnergy12NgbrZD1B
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  // See mjd's NOTES III, 7/30-Jan-2003, p27-35
  // EXTRA NOTE: Beware that "field" computed here is
  //  -grad(E)/(mu_0*cellvolume), which is not a good representation
  //  for H.  See mjd's NOTES II, 9-Aug-2002, p183.
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;
  if((1<xdim && xdim<5) || (1<ydim && ydim<5)
     || (1<zdim && zdim<5)) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
	"Each dimension must be ==1 or >=5 for 12ngbrZD1b kernel."
	" (Actual dimensions: xdim=%u, ydim=%u, zdim=%u.)",
	xdim,ydim,zdim);
    throw Oxs_Ext::Error(this,buf);
  }

  // (Re)-initialize coefficients if mesh has changed.
  if(mesh_id != mesh->Id()) {
    mesh_id = 0; // Safety
    InitCoef_12NgbrZD1(xdim,xinteg,xcoef);
    InitCoef_12NgbrZD1(ydim,yinteg,ycoef);
    InitCoef_12NgbrZD1(zdim,zinteg,zcoef);
    mesh_id = mesh->Id();
  }

  const REAL8m hcoef = -2/MU0;
  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  for(UINT4m z=0;z<zdim;z++) {
    UINT4m zoff = z;        // "Distance" from boundary
    UINT4m zcoef_index = z; // Index into zcoef array
    if(zdim-1<2*z) { // Backside
      zoff = zdim - 1 - z;
      zcoef_index = OC_MIN(10,zdim) - 1 - zoff;
    }
    REAL8m ck=1.0;
    if(zoff<3) ck = zinteg[zoff];  // Integration weight
    /// Note: zcoef_index is only valid if zoff<5

    for(UINT4m y=0;y<ydim;y++) {
      UINT4m yoff = y;
      UINT4m ycoef_index = y;
      if(ydim-1<2*y) {
	yoff = ydim - 1 - y;
	ycoef_index = OC_MIN(10,ydim) - 1 - yoff;
      }
      REAL8m bj=1.0;
      if(yoff<3) bj = yinteg[yoff];
      /// Note: ycoef_index is only valid if yoff<5

      for(UINT4m x=0;x<xdim;x++) {
	UINT4m i = mesh->Index(x,y,z); // Get base linear address
	REAL8m Msii = Ms_inverse[i];
	if(Msii == 0.0) {
	  energy[i]=0.0;
	  field[i].Set(0.,0.,0.);
	  continue;
	}

	UINT4m xoff = x;
	UINT4m xcoef_index = x;
	if(xdim-1<2*x) {
	  xoff = xdim - 1 - x;
	  xcoef_index = OC_MIN(10,xdim) - 1 - xoff;
	}
	REAL8m ai=1.0;
	if(xoff<3) ai = xinteg[xoff];
	/// Note: xcoef_index is only valid if xoff<5

	ThreeVector sum,tsum;
	sum.Set(0.,0.,0.);

	// Dxx
	if(xdim>1) {
	  if(xoff>4) {
	    tsum  = ( -1./12.)*spin[i-2];
	    tsum += ( 16./12.)*spin[i-1];
	    tsum += (-30./12.)*spin[i];
	    tsum += ( 16./12.)*spin[i+1];
	    tsum += ( -1./12.)*spin[i+2];
	  } else {
	    tsum.Set(0.,0.,0.);
	    INT4m jmin = -1 * static_cast<INT4m>(OC_MIN(2,x));
	    INT4m jmax = static_cast<INT4m>(OC_MIN(3,xdim-x));
	    for(INT4m j=jmin;j<jmax;j++) {
	      tsum += xcoef(xcoef_index,xcoef_index+j)*spin[i+j];
	    }
	  }
	  sum = (bj*ck*wgtx)*tsum; // ai built into above coefficients.
	}

	// Dyy
	if(ydim>1) {
	  if(yoff>4) {
	    tsum  = ( -1./12.)*spin[i-2*xdim];
	    tsum += ( 16./12.)*spin[i-xdim];
	    tsum += (-30./12.)*spin[i];
	    tsum += ( 16./12.)*spin[i+xdim];
	    tsum += ( -1./12.)*spin[i+2*xdim];
	  } else {
	    tsum.Set(0.,0.,0.);
	    INT4m jmin = -1*static_cast<INT4m>(OC_MIN(2,y));
	    INT4m jmax = static_cast<INT4m>(OC_MIN(3,ydim-y));
	    for(INT4m j=jmin;j<jmax;j++) {
	      tsum += ycoef(ycoef_index,ycoef_index+j)*spin[i+j*xdim];
	    }
	  }
	  sum += (ai*ck*wgty)*tsum; // bj built into above coefficients.
	}

	// Dzz
	if(zdim>1) {
	  if(zoff>4) {
	    tsum  = ( -1./12.)*spin[i-2*xydim];
	    tsum += ( 16./12.)*spin[i-xydim];
	    tsum += (-30./12.)*spin[i];
	    tsum += ( 16./12.)*spin[i+xydim];
	    tsum += ( -1./12.)*spin[i+2*xydim];
	  } else {
	    tsum.Set(0.,0.,0.);
	    INT4m jmin = -1*static_cast<INT4m>(OC_MIN(2,z));
	    INT4m jmax = static_cast<INT4m>(OC_MIN(3,zdim-z));
	    for(INT4m j=jmin;j<jmax;j++) {
	      tsum += zcoef(zcoef_index,zcoef_index+j)*spin[i+j*xydim];
	    }
	  }
	  sum += (ai*bj*wgtz)*tsum; // ck built into above coefficients.
	}

	field[i] = (hcoef*Msii) * sum;
	energy[i] = (sum * spin[i]);
      }
    }
  }
}

void
Klm_UniformExchange::CalcEnergy12NgbrMirror
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  const REAL8m hcoef = -2/MU0;

  // Second derivative weighting matrix is (-1 16 -30 16 -1)/12.
  // Introduce 2 row mirror boundary, i.e., if edge is
  //
  //           |
  //           | a  b  c  d ...
  //           |
  // use
  //           |
  //      b  a | a  b  c  d ...
  //           |
  //
  REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX()*12.);
  REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY()*12.);
  REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ()*12.);
  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
	UINT4m i = mesh->Index(x,y,z); // Get base linear address

	REAL8m Msii = Ms_inverse[i];
	if(Msii == 0.0) {
	  energy[i]=0.0;
	  field[i].Set(0.,0.,0.);
	  continue;
	}

	ThreeVector base = spin[i];

	ThreeVector sum_inner(0.,0.,0.);
	ThreeVector sum_outer(0.,0.,0.);
	if(x > 1) {
	  UINT4m j1 =  i - 1;
	  UINT4m j2 = j1 - 1;
	  if(Ms_inverse[j1]!=0.0) sum_inner = spin[j1] - base;
	  if(Ms_inverse[j2]!=0.0) sum_outer = spin[j1] - spin[j2];
	} else if(x == 1) {
	  UINT4m j = i - 1;
	  if(Ms_inverse[j]!=0.0) sum_inner = spin[j] - base;
	} else if(xdim > 1) {
	  // x==0.  Add edge correction.
	  UINT4m j = i + 1;
	  if(Ms_inverse[j]!=0.0) sum_outer = base - spin[j];
	}
	if(x+2 < xdim) {
	  // Note: We have to compare 'x+2<xdim', NOT 'x<xdim-2',
	  // because x and xdim are unsigned, and xdim may be
	  // as small as 1.
	  UINT4m j1 =  i + 1;
	  UINT4m j2 = j1 + 1;
	  if(Ms_inverse[j1]!=0.0) sum_inner += spin[j1] - base;
	  if(Ms_inverse[j2]!=0.0) sum_outer += spin[j1] - spin[j2];
	} else if(x+2 == xdim) {
	  UINT4m j = i + 1;
	  if(Ms_inverse[j]!=0.0) sum_inner += spin[j] - base;
	} else if(xdim > 1) {
	  // x==xdim-1.  Add edge correction.
	  UINT4m j = i - 1;
	  if(Ms_inverse[j]!=0.0) sum_outer += base - spin[j];
	}
	sum_inner *= wgtx;
	sum_outer *= wgtx;

	ThreeVector temp_inner(0.,0.,0.);
	ThreeVector temp_outer(0.,0.,0.);
	if(y > 1) {
	  UINT4m j1 =  i - xdim;
	  UINT4m j2 = j1 - xdim;
	  if(Ms_inverse[j1]!=0.0) temp_inner = spin[j1] - base;
	  if(Ms_inverse[j2]!=0.0) temp_outer = spin[j1] - spin[j2];
	} else if(y == 1) {
	  UINT4m j = i - xdim;
	  if(Ms_inverse[j]!=0.0) temp_inner = spin[j] - base;
	} else if(ydim > 1) {
	  // y==0.  Add edge correction.
	  UINT4m j = i + xdim;
	  if(Ms_inverse[j]!=0.0) temp_outer = base - spin[j];
	}
	if(y+2 < ydim) {
	  UINT4m j1 =  i + xdim;
	  UINT4m j2 = j1 + xdim;
	  if(Ms_inverse[j1]!=0.0) temp_inner += spin[j1] - base;
	  if(Ms_inverse[j2]!=0.0) temp_outer += spin[j1] - spin[j2];
	} else if(y+2 == ydim) {
	  UINT4m j = i + xdim;
	  if(Ms_inverse[j]!=0.0) temp_inner += spin[j] - base;
	} else if(ydim > 1) {
	  // y==ydim-1.  Add edge correction.
	  UINT4m j = i - xdim;
	  if(Ms_inverse[j]!=0.0) temp_outer += base - spin[j];
	}
	sum_inner += wgty*temp_inner;
	sum_outer += wgty*temp_outer;

	temp_inner.Set(0.,0.,0.);
	temp_outer.Set(0.,0.,0.);
	if(z > 1) {
	  UINT4m j1 =  i - xydim;
	  UINT4m j2 = j1 - xydim;
	  if(Ms_inverse[j1]!=0.0) temp_inner = spin[j1] - base;
	  if(Ms_inverse[j2]!=0.0) temp_outer = spin[j1] - spin[j2];
	} else if(z == 1) {
	  UINT4m j = i - xydim;
	  if(Ms_inverse[j]!=0.0) temp_inner = spin[j] - base;
	} else if(zdim > 1) {
	  // z==0.  Add edge correction.
	  UINT4m j = i + xydim;
	  if(Ms_inverse[j]!=0.0) temp_outer = base - spin[j];
	}
	if(z+2 < zdim) {
	  UINT4m j1 =  i + xydim;
	  UINT4m j2 = j1 + xydim;
	  if(Ms_inverse[j1]!=0.0) temp_inner += spin[j1] - base;
	  if(Ms_inverse[j2]!=0.0) temp_outer += spin[j1] - spin[j2];
	} else if(z+2 == zdim) {
	  UINT4m j = i + xydim;
	  if(Ms_inverse[j]!=0.0) temp_inner += spin[j] - base;
	} else if(zdim > 1) {
	  // z==zdim-1.  Add edge correction.
	  UINT4m j = i - xydim;
	  if(Ms_inverse[j]!=0.0) temp_outer += base - spin[j];
	}
	sum_inner += wgtz*temp_inner;
	sum_outer += wgtz*temp_outer;

	sum_inner *= 15;
	sum_inner += sum_outer;

	field[i] = (hcoef*Msii) * sum_inner;
	energy[i] = (sum_inner * base);
      }
    }
  }
}

void
Klm_UniformExchange::CalcEnergy26Ngbr
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_MeshValue<REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>& field) const
{
  UINT4m xdim = mesh->DimX();
  UINT4m ydim = mesh->DimY();
  UINT4m zdim = mesh->DimZ();
  UINT4m xydim = xdim*ydim;

  const REAL8m hcoef = -2/MU0;

  // Construct term weightings.  See mjd's NOTES I, 23-Oct-1997,
  // pp 168-171.
  REAL8m delxsq = mesh->EdgeLengthX()*mesh->EdgeLengthX();
  REAL8m delysq = mesh->EdgeLengthY()*mesh->EdgeLengthY();
  REAL8m delzsq = mesh->EdgeLengthZ()*mesh->EdgeLengthZ();
  REAL8m delisum = (1.0/delxsq)+(1.0/delysq)+(1.0/delzsq);
  REAL8m Amult = A/18.0;

  // Nearest neighbor weights.  These are all zero for cubic cells.
  // Might want to implement special handling in the algorithm
  // below for this common case.
  REAL8m wgt_x=0.,wgt_y=0.,wgt_z=0.;
  if(delxsq!=delysq || delxsq!=delzsq || delysq!=delzsq) {
    wgt_x = 4*Amult*(delisum-3.0/delxsq);
    wgt_y = 4*Amult*(delisum-3.0/delysq);
    wgt_z = 4*Amult*(delisum-3.0/delzsq);
  }

  // 2nd nearest neighbor weights.
  REAL8m wgt_xy = 2*Amult*(-delisum + 3.0/(2*delzsq));
  REAL8m wgt_xz = 2*Amult*(-delisum + 3.0/(2*delysq));
  REAL8m wgt_yz = 2*Amult*(-delisum + 3.0/(2*delxsq));

  // Corner neighbor weight.
  REAL8m wgt_corner = -0.5*Amult*delisum;

  for(UINT4m z=0;z<zdim;z++) {
    for(UINT4m y=0;y<ydim;y++) {
      for(UINT4m x=0;x<xdim;x++) {
	UINT4m i = mesh->Index(x,y,z); // Get base linear address

	REAL8m Msii = Ms_inverse[i];
	if(Msii == 0.0) {
	  energy[i]=0.0;
	  field[i].Set(0.,0.,0.);
	  continue;
	}

	ThreeVector base = spin[i];

	// Nearest x neighbors.  At most 2.
	ThreeVector sum_x(0.,0.,0.);
	if(x>0) {
	  UINT4m j = i-1;
	  if(Ms_inverse[j]!=0.0) sum_x = (spin[j] - base);
	}
	if(x<xdim-1) {
	  UINT4m j = i+1;
	  if(Ms_inverse[j]!=0.0) sum_x += (spin[j] - base);
	}
	sum_x *= wgt_x;

	// Nearest y neighbors.  At most 2.
	ThreeVector sum_y(0.,0.,0.);
	if(y>0) {
	  UINT4m j = i-xdim;
	  if(Ms_inverse[j]!=0.0) sum_y = (spin[j] - base);
	}
	if(y<ydim-1) {
	  UINT4m j = i+xdim;
	  if(Ms_inverse[j]!=0.0) sum_y += (spin[j] - base);
	}
	sum_y *= wgt_y;

	// Nearest z neighbors.  At most 2.
	ThreeVector sum_z(0.,0.,0.);
	if(z>0) {
	  UINT4m j = i-xydim;
	  if(Ms_inverse[j]!=0.0) sum_z = (spin[j] - base);
	}
	if(z<zdim-1) {
	  UINT4m j = i+xydim;
	  if(Ms_inverse[j]!=0.0) sum_z += (spin[j] - base);
	}
	sum_z *= wgt_z;

	// xy-neighbors.  At most 4.
	UINT4m j;
	ThreeVector sum_xy;
	j = i;  if(x>0)      j -= 1;  if(y>0)      j -= xdim;
	if(Ms_inverse[j]!=0.0) sum_xy = (spin[j] - base);
	j = i;  if(x<xdim-1) j += 1;  if(y>0)      j -= xdim;
	if(Ms_inverse[j]!=0.0) sum_xy += (spin[j] - base);
	j = i;  if(x>0)      j -= 1;  if(y<ydim-1) j += xdim;
	if(Ms_inverse[j]!=0.0) sum_xy += (spin[j] - base);
	j = i;  if(x<xdim-1) j += 1;  if(y<ydim-1) j += xdim;
	if(Ms_inverse[j]!=0.0) sum_xy += (spin[j] - base);
	sum_xy *= wgt_xy;

	// xz-neighbors.  At most 4.
	ThreeVector sum_xz;
	j = i;  if(x>0)      j -= 1;  if(z>0)      j -= xydim;
	if(Ms_inverse[j]!=0.0) sum_xz = (spin[j] - base);
	j = i;  if(x<xdim-1) j += 1;  if(z>0)      j -= xydim;
	if(Ms_inverse[j]!=0.0) sum_xz += (spin[j] - base);
	j = i;  if(x>0)      j -= 1;  if(z<zdim-1) j += xydim;
	if(Ms_inverse[j]!=0.0) sum_xz += (spin[j] - base);
	j = i;  if(x<xdim-1) j += 1;  if(z<zdim-1) j += xydim;
	if(Ms_inverse[j]!=0.0) sum_xz += (spin[j] - base);
	sum_xz *= wgt_xz;

	// yz-neighbors.  At most 4.
	ThreeVector sum_yz;
	j = i;  if(y>0)      j -= xdim;  if(z>0)      j -= xydim;
	if(Ms_inverse[j]!=0.0) sum_yz = (spin[j] - base);
	j = i;  if(y<ydim-1) j += xdim;  if(z>0)      j -= xydim;
	if(Ms_inverse[j]!=0.0) sum_yz += (spin[j] - base);
	j = i;  if(y>0)      j -= xdim;  if(z<zdim-1) j += xydim;
	if(Ms_inverse[j]!=0.0) sum_yz += (spin[j] - base);
	j = i;  if(y<ydim-1) j += xdim;  if(z<zdim-1) j += xydim;
	if(Ms_inverse[j]!=0.0) sum_yz += (spin[j] - base);
	sum_yz *= wgt_yz;

	// Corner neighbors.  At most 8.
	ThreeVector sum_corner;
	UINT4m iz=i;
	if(z>0) iz -= xydim;
	j = iz;  if(x>0)      j -= 1;  if(y>0)      j -= xdim;
	if(Ms_inverse[j]!=0.0) sum_corner = (spin[j] - base);
	j = iz;  if(x<xdim-1) j += 1;  if(y>0)      j -= xdim;
	if(Ms_inverse[j]!=0.0) sum_corner += (spin[j] - base);
	j = iz;  if(x>0)      j -= 1;  if(y<ydim-1) j += xdim;
	if(Ms_inverse[j]!=0.0) sum_corner += (spin[j] - base);
	j = iz;  if(x<xdim-1) j += 1;  if(y<ydim-1) j += xdim;
	if(Ms_inverse[j]!=0.0) sum_corner += (spin[j] - base);
	iz=i;
	if(z<zdim-1) iz += xydim;
	j = iz;  if(x>0)      j -= 1;  if(y>0)      j -= xdim;
	if(Ms_inverse[j]!=0.0) sum_corner += (spin[j] - base);
	j = iz;  if(x<xdim-1) j += 1;  if(y>0)      j -= xdim;
	if(Ms_inverse[j]!=0.0) sum_corner += (spin[j] - base);
	j = iz;  if(x>0)      j -= 1;  if(y<ydim-1) j += xdim;
	if(Ms_inverse[j]!=0.0) sum_corner += (spin[j] - base);
	j = iz;  if(x<xdim-1) j += 1;  if(y<ydim-1) j += xdim;
	if(Ms_inverse[j]!=0.0) sum_corner += (spin[j] - base);
	sum_corner *= wgt_corner;

	// Combine terms
	ThreeVector sum
	  = sum_x  + sum_y  + sum_z
	  + sum_xy + sum_xz + sum_yz
	  + sum_corner;

	field[i] = (hcoef*Msii) * sum;
	energy[i] = (sum * base);
      }
    }
  }
}

void Klm_UniformExchange::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<REAL8m>& Ms = *(state.Ms);
  const Oxs_MeshValue<REAL8m>& Ms_inverse = *(state.Ms_inverse);

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    throw Oxs_Ext::Error(this,"Import mesh to"
                         " Klm_UniformExchange::GetEnergy()"
                         " is not an Oxs_RectangularMesh object.");
  }

  // Note: Might want to consider subclassing exchange energies,
  //  to replace this if-block with virtual function pointers.
  if(kernel == NGBR_6_FREE) {
    CalcEnergy6NgbrFree(spin,Ms_inverse,mesh,energy,field);
  } else if(kernel == NGBR_6_MIRROR) {
    if(A_TYPE != excoeftype) {
      CalcEnergy6NgbrMirror_lex(spin,Ms,mesh,energy,field);
    } else {
      CalcEnergy6NgbrMirror(spin,Ms_inverse,mesh,energy,field);
    }
  } else if(kernel == NGBR_6_MIRROR_STD) {
    CalcEnergy6NgbrMirrorStd(spin,Ms_inverse,mesh,energy,field);
  } else if(kernel == NGBR_6_BIGANG_MIRROR) {
    CalcEnergy6NgbrBigAngMirror(spin,Ms_inverse,mesh,energy,field);
  } else if(kernel == NGBR_6_ZD2) {
    CalcEnergy6NgbrZD2(spin,Ms_inverse,mesh,energy,field);
  } 
  
  // KL(m)
  else if(kernel == NGBR_6_Z_PERIOD) {
    if(A_TYPE != excoeftype) {
      CalcEnergy6NgbrZPeriodicCond_lex(spin,Ms,mesh,energy,field);
    } else {
      CalcEnergy6NgbrZPeriodicCond(spin,Ms_inverse,mesh,energy,field);
    }
    
  } else if(kernel == NGBR_12_FREE) {
    CalcEnergy12NgbrFree(spin,Ms_inverse,mesh,energy,field);
  } else if(kernel == NGBR_12_ZD1) {
    CalcEnergy12NgbrZD1(spin,Ms_inverse,mesh,energy,field);
  } else if(kernel == NGBR_12_ZD1B) {
    CalcEnergy12NgbrZD1B(spin,Ms_inverse,mesh,energy,field);
  } else if(kernel == NGBR_12_MIRROR) {
    CalcEnergy12NgbrMirror(spin,Ms_inverse,mesh,energy,field);
  } else if(kernel == NGBR_26) {
    CalcEnergy26Ngbr(spin,Ms_inverse,mesh,energy,field);
  } else {
    throw Oxs_Ext::Error(this,"Invalid kernel type detected."
                         "  (Programming error)");
  }

}
