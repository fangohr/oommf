/* FILE: exchange6ngbr.cc            -*-Mode: c++-*-
 *
 * 6 neighbor exchange energy on rectangular mesh,
 * derived from Oxs_Energy class.
 *
 */

#include <cstdio> // For some versions of g++ on Windows, if stdio.h is
/// #include'd after a C++-style header, then printf format modifiers
/// like OC_INDEX_MOD and OC_INT4m_MOD aren't handled properly.

#include <string>

#include "atlas.h"
#include "nb.h"
#include "key.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "oxswarn.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "exchange6ngbr.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_Exchange6Ngbr);

/* End includes */

// Revision information, set via CVS keyword substitution
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1.54 $",
   "$Date: 2015/01/29 21:33:45 $",
   "$Author: donahue $",
   "Michael J. Donahue (michael.donahue@nist.gov)");

// Constructor
Oxs_Exchange6Ngbr::Oxs_Exchange6Ngbr(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ChunkEnergy(name,newdtr,argstr),
    coef_size(0), coef(NULL), max_abscoef(0),
    mesh_id(0), energy_density_error_estimate(-1)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("atlas",Oxs_Atlas,atlas);
  atlaskey.Set(atlas.GetPtr());
  /// Dependency lock is held until *this is deleted.

  // Determine number of regions, and check that the
  // count lies within the allowed range.
  coef_size = atlas->GetRegionCount();
  if(coef_size<1) {
    String msg = String("Oxs_Atlas object ")
      + atlas->InstanceName()
      + String(" must contain at least one region.");

    throw Oxs_ExtError(msg.c_str());
  }
  const OC_INDEX sqrootbits = (sizeof(OC_INDEX)*8-1)/2;
  // Note assumption that OC_INDEX is a signed type.
  // Also, sizeof(OC_INDEX)*8-1 is odd, so sqrootbits is shy
  // by half a bit.  We adjust for this below by multiplying
  // by a rational approximation to sqrt(2).
  OC_INDEX coef_check = 1;
  for(OC_INDEX ibit=0;ibit<sqrootbits;++ibit) coef_check *= 2;
#if OC_INDEX_WIDTH == 1
    coef_check = 11;  // Table look-up <g>
#elif OC_INDEX_WIDTH < 5
    // Use sqrt(2) > 239/169
    coef_check = (coef_check*239+168)/169 - 1;
#else
    // Use sqrt(2) > 275807/195025.  This is accurate for
    // sizeof(OC_INDEX) <= 9, but will round low above that.
    coef_check = (coef_check*275807+195024)/195025 - 1;
#endif // OC_INDEX_WIDTH
  const OC_INDEX max_coef_size = coef_check;
  if(coef_size>max_coef_size) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
		  "Oxs_Atlas object %.1000s has too many regions: %lu"
                  " (max allowed: %lu)",
                  atlas->InstanceName(),
                  (unsigned long)coef_size,
                  (unsigned long)max_coef_size);
      throw Oxs_ExtError(this,buf);
  }

  // Determine coef matrix fill type
  String typestr;
  vector<String> params;
  OC_REAL8m default_coef = 0.0;
  OC_BOOL has_A = HasInitValue("A");
  OC_BOOL has_lex = HasInitValue("lex");
  if(has_A && has_lex) {
    throw Oxs_ExtError(this,"Invalid exchange coefficient request:"
			 " both A and lex specified; only one should"
			 " be given.");
  } else if(has_lex) {
    excoeftype = LEX_TYPE;
    typestr = "lex";
    default_coef = GetRealInitValue("default_lex",0.0);
    FindRequiredInitValue("lex",params);
  } else {
    excoeftype = A_TYPE;
    typestr = "A";
    default_coef = GetRealInitValue("default_A",0.0);
    FindRequiredInitValue("A",params);
  }
  if(params.empty()) {
    String msg = String("Empty parameter list for key \"")
      + typestr + String("\"");
    throw Oxs_ExtError(this,msg);
  }
  if(params.size()%3!=0) {
      char buf[4096];
      Oc_Snprintf(buf,sizeof(buf),
		  "Number of elements in %.80s sub-list must be"
		  " divisible by 3"
		  " (actual sub-list size: %u)",
		  typestr.c_str(),(unsigned int)params.size());
      throw Oxs_ExtError(this,buf);
  }

  // Allocate A matrix.  Because raw pointers are used, a memory leak
  // would occur if an uncaught exception occurred beyond this point.
  // So, we put the whole bit in a try-catch block.
  try {
    coef = new OC_REAL8m*[coef_size];
    coef[0] = NULL; // Safety, in case next alloc throws an exception
    coef[0] = new OC_REAL8m[coef_size*coef_size];
    OC_INDEX ic;
    for(ic=1;ic<coef_size;ic++) coef[ic] = coef[ic-1] + coef_size;

    // Fill A/lex matrix
    max_abscoef=0.0;
    for(ic=0;ic<coef_size*coef_size;ic++) coef[0][ic] = default_coef;
    for(OC_INDEX ip=0;static_cast<size_t>(ip)<params.size();ip+=3) {
      OC_INDEX i1 = atlas->GetRegionId(params[ip]);
      OC_INDEX i2 = atlas->GetRegionId(params[ip+1]);
      if(i1<0 || i2<0) {
        // Unknown region(s) requested
        char buf[4096];
        char* cptr=buf;
        if(i1<0) {
          char item[96];  // Safety
          item[80]='\0';
          Oc_Snprintf(item,sizeof(item),"%.82s",params[ip].c_str());
          if(item[80]!='\0') strcpy(item+80,"..."); // Overflow
          Oc_Snprintf(buf,sizeof(buf),
                      "First entry in %.80s[%" OC_INDEX_MOD "d] sub-list,"
                      " \"%.85s\", is not a known region in atlas"
                      " \"%.1000s\".  ",
                      typestr.c_str(),ip/3,item,
                      atlas->InstanceName());
          cptr += strlen(buf);
        }
        if(i2<0) {
          char item[96];  // Safety
          item[80]='\0';
          Oc_Snprintf(item,sizeof(item),"%.82s",params[ip+1].c_str());
          if(item[80]!='\0') strcpy(item+80,"..."); // Overflow
          Oc_Snprintf(cptr,sizeof(buf)-(cptr-buf),
                      "Second entry in %.80s[%ld] sub-list, \"%.85s\","
                      " is not a known region in atlas \"%.1000s\".  ",
                      typestr.c_str(),long(ip/3),item,
                      atlas->InstanceName());
        }
        String msg = String(buf);
        msg += String("Known regions:");
        vector<String> regions;
        atlas->GetRegionList(regions);
        for(OC_INDEX j=0;static_cast<size_t>(j)<regions.size();++j) {
          msg += String(" \n");
          msg += regions[j];
        }
        throw Oxs_ExtError(this,msg);
      }
      OC_BOOL err;
      OC_REAL8m coefpair = Nb_Atof(params[ip+2].c_str(),err);
      if(err) {
        char item[96];  // Safety
        item[80]='\0';
        Oc_Snprintf(item,sizeof(item),"%.82s",params[ip+2].c_str());
        if(item[80]!='\0') strcpy(item+80,"..."); // Overflow
        char buf[4096];
        Oc_Snprintf(buf,sizeof(buf),
                    "Third entry in %.80s[%ld] sub-list, \"%.85s\","
                    " is not a valid floating point number.",
                    typestr.c_str(),long(ip/3),item);
        throw Oxs_ExtError(this,buf);
      }
      coef[i1][i2]=coefpair;
      coef[i2][i1]=coefpair; // coef should be symmetric
      if(fabs(coefpair)>max_abscoef) max_abscoef=coefpair;
    }
    DeleteInitValue(typestr);

    VerifyAllInitArgsUsed();

    // Setup outputs
    maxspinangle_output.Setup(this,InstanceName(),"Max Spin Ang","deg",1,
                              &Oxs_Exchange6Ngbr::UpdateDerivedOutputs);
    maxspinangle_output.Register(director,0);
    stage_maxspinangle_output.Setup(this,InstanceName(),
                                    "Stage Max Spin Ang","deg",1,
                                    &Oxs_Exchange6Ngbr::UpdateDerivedOutputs);
    stage_maxspinangle_output.Register(director,0);
    run_maxspinangle_output.Setup(this,InstanceName(),
                                  "Run Max Spin Ang","deg",1,
                                  &Oxs_Exchange6Ngbr::UpdateDerivedOutputs);
    run_maxspinangle_output.Register(director,0);
  }
  catch(...) {
    if(coef_size>0 && coef!=NULL) { // Release coef memory
      delete[] coef[0]; delete[] coef;
      coef = NULL;
      coef_size = 0; // Safety
    }
    throw;
  }

}

Oxs_Exchange6Ngbr::~Oxs_Exchange6Ngbr()
{
  if(coef_size>0 && coef!=NULL) {
    delete[] coef[0];
    delete[] coef;
  }
}

OC_BOOL Oxs_Exchange6Ngbr::Init()
{
  mesh_id = 0;
  region_id.Release();
  energy_density_error_estimate = -1;
  return Oxs_Energy::Init();
}

void Oxs_Exchange6Ngbr::CalcEnergyA
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int threadnumber
 ) const
{
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);

  // Downcast mesh
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg =
      String("Import mesh (\"")
      + String(state.mesh->InstanceName())
      + String("\") to Oxs_Exchange6Ngbr::GetEnergyA()"
             " routine of object \"") + String(InstanceName())
      + String("\" is not a rectangular mesh object.");
    throw Oxs_ExtError(msg.c_str());
  }

  // If periodic, collect data for distance determination
  // Periodic boundaries?
  int xperiodic=0, yperiodic=0, zperiodic=0;
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh!=NULL) {
    xperiodic = pmesh->IsPeriodicX();
    yperiodic = pmesh->IsPeriodicY();
    zperiodic = pmesh->IsPeriodicZ();
  }

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  OC_REAL8m wgtx = -1.0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -1.0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -1.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  OC_REAL8m hcoef = -2/MU0;

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  // Note: For maxangle calculation, it suffices to check
  // spin[j]-spin[i] for j>i.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
    while(x<xstop) {
      ThreeVector base = spin[i];
      OC_REAL8m Msii = Ms_inverse[i];
      if(0.0 == Msii) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }
      OC_REAL8m* Arow = coef[region_id[i]];
      ThreeVector sum(0.,0.,0.);
      if(z>0 || zperiodic) {
        OC_INDEX j = i-xydim;
        if(z==0) j += xyzdim;
        OC_REAL8m Apair = Arow[region_id[j]];
        if(Apair!=0 && Ms_inverse[j]!=0.0) {
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += Apair*wgtz*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(y>0 || yperiodic) {
        OC_INDEX j = i-xdim;
        if(y==0) j += xydim;
        OC_REAL8m Apair = Arow[region_id[j]];
        if(Apair!=0.0 && Ms_inverse[j]!=0.0) {
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += Apair*wgty*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x>0 || xperiodic) {
        OC_INDEX j = i-1;
        if(x==0) j += xdim;
        OC_REAL8m Apair = Arow[region_id[j]];
        if(Apair!=0.0 && Ms_inverse[j]!=0.0) {
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += Apair*wgtx*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x<xdim-1 || xperiodic) {
        OC_INDEX j = i+1;
        if(x==xdim-1) j -= xdim;
        OC_REAL8m Apair = Arow[region_id[j]];
        if(Ms_inverse[j]!=0.0) sum += Apair*wgtx*(spin[j] - base);
      }
      if(y<ydim-1 || yperiodic) {
        OC_INDEX j = i+xdim;
        if(y==ydim-1) j -= xydim;
        OC_REAL8m Apair = Arow[region_id[j]];
        if(Ms_inverse[j]!=0.0) sum += Apair*wgty*(spin[j] - base);
      }
      if(z<zdim-1 || zperiodic) {
        OC_INDEX j = i+xydim;
        if(z==zdim-1) j -= xyzdim;
        OC_REAL8m Apair = Arow[region_id[j]];
        if(Ms_inverse[j]!=0.0) sum += Apair*wgtz*(spin[j]- base);
      }

      OC_REAL8m ei = base.x*sum.x + base.y*sum.y + base.z*sum.z;
      OC_REAL8m hmult = hcoef*Msii;
      sum.x *= hmult;  sum.y *= hmult;   sum.z *= hmult;
      OC_REAL8m tx = base.y*sum.z - base.z*sum.y;
      OC_REAL8m ty = base.z*sum.x - base.x*sum.z;
      OC_REAL8m tz = base.x*sum.y - base.y*sum.x;

      energy_sum += ei;
      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)       (*ocedt.H)[i] = sum;
      if(ocedt.H_accum) (*ocedt.H_accum)[i] += sum;
      if(ocedt.mxH)       (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
      if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
      ++i;   ++x;
    }
    x=0;
    if((++y)>=ydim) {
      y=0;
      ++z;
    }
  }

  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0);
  /// All cells have same volume in an Oxs_RectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}


void Oxs_Exchange6Ngbr::CalcEnergyLex
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int threadnumber
 ) const
{
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  // Downcast mesh
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg =
      String("Import mesh (\"")
      + String(state.mesh->InstanceName())
      + String("\") to Oxs_Exchange6Ngbr::GetEnergyLex()"
             " routine of object \"") + String(InstanceName())
      + String("\" is not a rectangular mesh object.");
    throw Oxs_ExtError(msg.c_str());
  }

  // If periodic, collect data for distance determination
  // Periodic boundaries?
  int xperiodic=0, yperiodic=0, zperiodic=0;
  const Oxs_PeriodicRectangularMesh* pmesh
    = dynamic_cast<const Oxs_PeriodicRectangularMesh*>(mesh);
  if(pmesh!=NULL) {
    xperiodic = pmesh->IsPeriodicX();
    yperiodic = pmesh->IsPeriodicY();
    zperiodic = pmesh->IsPeriodicZ();
  }

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  OC_REAL8m wgtx = 1.0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = 1.0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = 1.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  // Note: For maxangle calculation, it suffices to check
  // spin[j]-spin[i] for j>i, or j<i, or various mixes of the two.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
    while(x<xstop) {
      if(0 == Ms[i]) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }
      const OC_REAL8m emult = (-0.5*MU0)*Ms[i];

      ThreeVector base = spin[i];

      OC_REAL8m* lexrow = coef[region_id[i]];
      ThreeVector sum(0.,0.,0.);
      if(z>0 || zperiodic) {
        OC_INDEX j = i-xydim;
        if(z==0) j += xyzdim;
        OC_REAL8m lexpair = lexrow[region_id[j]];
        if(lexpair!=0.0 && Ms[j]!=0.0) {
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += Ms[j]*lexpair*lexpair*wgtz*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(y>0 || yperiodic) {
        OC_INDEX j = i-xdim;
        if(y==0) y+= xydim;
        OC_REAL8m lexpair = lexrow[region_id[j]];
        if(lexpair!=0.0 && Ms[j]!=0.0) {
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += Ms[j]*lexpair*lexpair*wgty*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x>0 || xperiodic) {
        OC_INDEX j = i-1;
        if(x==0) j += xdim;
        OC_REAL8m lexpair = lexrow[region_id[j]];
        if(lexpair!=0.0 && Ms[j]!=0.0) {
          ThreeVector diff = (spin[j] - base);
          OC_REAL8m dot = diff.MagSq();
          sum += Ms[j]*lexpair*lexpair*wgtx*diff;
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x<xdim-1 || xperiodic) {
        OC_INDEX j = i+1;
        if(x==xdim-1) j -= xdim;
        OC_REAL8m lexpair = lexrow[region_id[j]];
        sum += Ms[j]*lexpair*lexpair*wgtx*(spin[j] - base);
      }
      if(y<ydim-1 || yperiodic) {
        OC_INDEX j = i+xdim;
        if(y==ydim-1) j -= xydim;
        OC_REAL8m lexpair = lexrow[region_id[j]];
        sum += Ms[j]*lexpair*lexpair*wgty*(spin[j] - base);
      }
      if(z<zdim-1 || zperiodic) {
        OC_INDEX j = i+xydim;
        if(z==zdim-1) j -= xyzdim;
        OC_REAL8m lexpair = lexrow[region_id[j]];
        sum += Ms[j]*lexpair*lexpair*wgtz*(spin[j]- base);
      }

      OC_REAL8m ei = emult*(base.x*sum.x + base.y*sum.y + base.z*sum.z);
      OC_REAL8m tx = base.y*sum.z - base.z*sum.y;
      OC_REAL8m ty = base.z*sum.x - base.x*sum.z;
      OC_REAL8m tz = base.x*sum.y - base.y*sum.x;

      energy_sum += ei;
      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)       (*ocedt.H)[i] = sum;
      if(ocedt.H_accum) (*ocedt.H_accum)[i] += sum;
      if(ocedt.mxH)       (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
      if(ocedt.mxH_accum) (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
      ++i;   ++x;
    }
    x=0;
    if((++y)>=ydim) {
      y=0;
      ++z;
    }
  }

  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0);
  /// All cells have same volume in an Oxs_RectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}


void Oxs_Exchange6Ngbr::ComputeEnergyChunkInitialize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& /* thread_ocedtaux */,
 int number_of_threads) const
{
  if(maxdot.size() != (vector<OC_REAL8m>::size_type)number_of_threads) {
    maxdot.resize(number_of_threads);
  }
  for(int i=0;i<number_of_threads;++i) {
    maxdot[i] = 0.0; // Minimum possible value for (m_i-m_j).MagSq()
  }

  if(mesh_id !=  state.mesh->Id() || !atlaskey.SameState()) {
    // Setup region mapping.  NB: At a lower level, this may potentially
    // involve calls back into the Tcl interpreter.  Per Tcl spec, only
    // the thread originating the interpreter is allowed to make calls
    // into it, so only threadnumber == 0 can do this processing.  The
    // Oxs_ChunkEnergy interface guarantees that this initialization
    // routine is only run on thread 0.
    const Oxs_CommonRectangularMesh* mesh
      = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
    if(mesh==NULL) {
      String msg=String("Object ")
        + String(state.mesh->InstanceName())
        + String(" is not a rectangular mesh.");
      throw Oxs_ExtError(this,msg);
    }
    region_id.AdjustSize(mesh);
    ThreeVector location;
    const OC_INDEX size = mesh->Size();
    for(OC_INDEX i=0;i<size;i++) {
      mesh->Center(i,location);
      if((region_id[i] = atlas->GetRegionId(location))<0) {
        String msg = String("Import mesh to Oxs_Exchange6Ngbr::GetEnergy()"
                            " routine of object ")
          + String(InstanceName())
          + String(" has points outside atlas ")
          + String(atlas->InstanceName());
        throw Oxs_ExtError(msg.c_str());
      }
    }
    atlaskey.Set(atlas.GetPtr());

    OC_REAL8m minedge = OC_REAL8m_MAX;
    if(mesh->DimX() > 1) {
      minedge = mesh->EdgeLengthX();
    }
    if(mesh->DimY() > 1 && mesh->EdgeLengthY()<minedge) {
      minedge = mesh->EdgeLengthY();
    }
    if(mesh->DimZ() > 1 && mesh->EdgeLengthZ()<minedge) {
      minedge = mesh->EdgeLengthZ();
    }
    OC_REAL8m working_A = 0.0;
    if(excoeftype == A_TYPE) {
      working_A = max_abscoef;
    } else if(excoeftype == LEX_TYPE) {
      OC_REAL8m lexMs = max_abscoef*state.max_absMs;
      if(lexMs>0) {
        working_A = 0.5*MU0*lexMs*lexMs;
      }
    } else {
      throw Oxs_ExtError(this,"Unsupported ExchangeCoefType.");
    }
    energy_density_error_estimate
      = 16*OC_REAL8m_EPSILON*working_A/minedge/minedge;
    // Worse case prefactor should be larger than 16, but in practice
    // error is probably smaller.
    mesh_id = state.mesh->Id();
  }
  ocedt.energy_density_error_estimate = energy_density_error_estimate;
}


void Oxs_Exchange6Ngbr::ComputeEnergyChunkFinalize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& /* ocedt */,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>&
    /* thread_ocedtaux */,
 int number_of_threads) const
{
  // Set max angle data
  OC_REAL8m total_maxdot = 0.0;
  for(int i=0;i<number_of_threads;++i) {
    if(maxdot[i]>total_maxdot) total_maxdot = maxdot[i];
  }
  const OC_REAL8m arg = 0.5*sqrt(total_maxdot);
  const OC_REAL8m maxang = ( arg >= 1.0 ? 180.0 : asin(arg)*(360.0/PI));

  OC_REAL8m dummy_value;
  String msa_name = MaxSpinAngleStateName();
  if(state.GetDerivedData(msa_name,dummy_value)) {
    // Ideally, energy values would never be computed more than once
    // for any one state, but in practice it seems inevitable that
    // such will occur on occasion.  For example, suppose a user
    // requests output on a state obtained by a stage crossing (as
    // opposed to a state obtained through a normal intrastage step);
    // a subsequent ::Step operation will re-compute the energies
    // because not all the information needed by the step transition
    // machinery is cached from an energy computation.  Even user
    // output requests on post-::Step states is problematic if some of
    // the requested output is not cached as part of the step
    // proceedings.  A warning is put into place below for debugging
    // purposes, but in general an error is raised only if results
    // from the recomputation are different than originally.
#ifndef NDEBUG
    static Oxs_WarningMessage maxangleset(3);
    maxangleset.Send(revision_info,OC_STRINGIFY(__LINE__),
                     "Programming error?"
                     " Oxs_Exchange6Ngbr max spin angle set twice.");
#endif
    // Max angle is computed by taking acos of the dot product
    // of neighboring spin vectors.  The relative error can be
    // quite large if the spins are nearly parallel.  The proper
    // error comparison is between the cos of the two values.
    // See NOTES VI, 6-Sep-2012, p71.
    OC_REAL8m diff = (dummy_value-maxang)*(PI/180.);
    OC_REAL8m sum  = (dummy_value+maxang)*(PI/180.);
    if(sum > PI ) sum = 2*PI - sum;
    if(fabs(diff*sum)>8*OC_REAL8_EPSILON) {
      char errbuf[1024];
      Oc_Snprintf(errbuf,sizeof(errbuf),
                  "Programming error:"
                  " Oxs_Exchange6Ngbr max spin angle set to"
                  " two different values;"
                  " orig val=%#.17g, new val=%#.17g",
                  dummy_value,maxang);
      throw Oxs_ExtError(this,errbuf);
    }
  } else {
    state.AddDerivedData(msa_name,maxang);
  }

  // Run and stage angle data depend on data from the previous state.
  // In the case that the energy (and hence max stage and run angle)
  // for the current state was computed previously, then the previous
  // state may have been dropped.  So, compute and save run and stage
  // angle data iff they are not already computed.

  // Check stage and run max angle data from previous state
  const Oxs_SimState* oldstate = NULL;
  OC_REAL8m stage_maxang = -1;
  OC_REAL8m run_maxang = -1;
  String smsa_name = StageMaxSpinAngleStateName();
  String rmsa_name = RunMaxSpinAngleStateName();
  if(state.previous_state_id &&
     0 != (oldstate
      = director->FindExistingSimulationState(state.previous_state_id)) ) {
    if(oldstate->stage_number != state.stage_number) {
      stage_maxang = 0.0;
    } else {
      if(oldstate->GetDerivedData(smsa_name,dummy_value)) {
        stage_maxang = dummy_value;
      }
    }
    if(oldstate->GetDerivedData(rmsa_name,dummy_value)) {
      run_maxang = dummy_value;
    }
  }
  if(stage_maxang<maxang) stage_maxang = maxang;
  if(run_maxang<maxang)   run_maxang = maxang;

  // Stage max angle data
  if(!state.GetDerivedData(smsa_name,dummy_value)) {
    state.AddDerivedData(smsa_name,stage_maxang);
  }

  // Run max angle data
  if(!state.GetDerivedData(rmsa_name,dummy_value)) {
    state.AddDerivedData(rmsa_name,run_maxang);
  }
}


void Oxs_Exchange6Ngbr::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int threadnumber
 ) const
{
#ifndef NDEBUG
  if(node_stop>state.mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  if(excoeftype == LEX_TYPE) {
    CalcEnergyLex(state,ocedt,ocedtaux,node_start,node_stop,threadnumber);
  } else {
    CalcEnergyA(state,ocedt,ocedtaux,node_start,node_stop,threadnumber);
  }

}

void Oxs_Exchange6Ngbr::UpdateDerivedOutputs(const Oxs_SimState& state)
{
  maxspinangle_output.cache.state_id
    = stage_maxspinangle_output.cache.state_id
    = run_maxspinangle_output.cache.state_id
    = 0;  // Mark change in progress

  String dummy_name = MaxSpinAngleStateName();
  if(!state.GetDerivedData(dummy_name.c_str(),
                           maxspinangle_output.cache.value)) {
    // Error; This should always be set.  For now, just set the value to
    // -1, but in the future should consider throwing an exception.
    maxspinangle_output.cache.value = -1.0;
  }

  String stage_dummy_name = StageMaxSpinAngleStateName();
  if(!state.GetDerivedData(stage_dummy_name.c_str(),
                           stage_maxspinangle_output.cache.value)) {
    // Error; This should always be set.  For now, just set the value to
    // -1, but in the future should consider throwing an exception.
    stage_maxspinangle_output.cache.value = -1.0;
  }

  String run_dummy_name = RunMaxSpinAngleStateName();
  if(!state.GetDerivedData(run_dummy_name.c_str(),
                           run_maxspinangle_output.cache.value)) {
    // Error; This should always be set.  For now, just set the value to
    // -1, but in the future should consider throwing an exception.
    run_maxspinangle_output.cache.value = -1.0;
  }

  maxspinangle_output.cache.state_id
    = stage_maxspinangle_output.cache.state_id
    = run_maxspinangle_output.cache.state_id
    = state.Id();
}

// Optional interface for conjugate-gradient evolver.
// NOTE: At present, not all of the kernels support preconditioning.
// For details on this code, see NOTES VI, 21-July-2011, pp 10-11.
OC_INT4m
Oxs_Exchange6Ngbr::IncrementPreconditioner(PreconditionerData& pcd)
{
  if(!pcd.state || !pcd.val) {
    throw Oxs_ExtError(this,
         "Import to IncrementPreconditioner not properly initialized.");
  }

  const Oxs_SimState& state = *(pcd.state);
  const OC_INDEX size = state.mesh->Size();

  Oxs_MeshValue<ThreeVector>& val = *(pcd.val);
  if(val.Size() != size) {
    throw Oxs_ExtError(this,
         "Import to IncrementPreconditioner not properly initialized.");
  }

  if(pcd.type != DIAGONAL) return 0; // Unsupported preconditioning type

  if(size<1) return 1; // Nothing to do

  if(mesh_id !=  state.mesh->Id() || !atlaskey.SameState()) {
    // Setup region mapping.
    region_id.AdjustSize(state.mesh);
    ThreeVector location;
    for(OC_INDEX i=0;i<size;i++) {
      state.mesh->Center(i,location);
      if((region_id[i] = atlas->GetRegionId(location))<0) {
        String msg
          = String("Import mesh to"
                   " Oxs_Exchange6Ngbr::IncrementPreconditioner()"
                   " routine of object ")
          + String(InstanceName())
          + String(" has points outside atlas ")
          + String(atlas->InstanceName());
        throw Oxs_ExtError(msg.c_str());
      }
    }
    atlaskey.Set(atlas.GetPtr());
    mesh_id = state.mesh->Id();
  }

  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(state.mesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(this,msg);
  }

  // Check periodicity.
  int xperiodic=0, yperiodic=0, zperiodic=0;
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

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  if(excoeftype == LEX_TYPE) {
    OC_REAL8m wgtx = MU0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
    OC_REAL8m wgty = MU0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
    OC_REAL8m wgtz = MU0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());
    OC_INDEX i = -1;
    for(OC_INDEX z=0;z<zdim;++z) {
      for(OC_INDEX y=0;y<ydim;++y) {
        for(OC_INDEX x=0;x<xdim;++x) {
          ++i;
          if(0.0 == Ms[i]) continue; // Ignore cells with Ms == 0.
          OC_REAL8m* lexrow = coef[region_id[i]];
          OC_REAL8m sum = 0.0;
          if(z>0 || zperiodic) {
            OC_INDEX j = i-xydim;
            if(z==0.0) j += xyzdim;
            OC_REAL8m lexpair = lexrow[region_id[j]];
            sum += Ms[j]*lexpair*lexpair*wgtz;
          }
          if(y>0 || yperiodic) {
            OC_INDEX j = i-xdim;
            if(y==0.0) y+= xydim;
            OC_REAL8m lexpair = lexrow[region_id[j]];
            sum += Ms[j]*lexpair*lexpair*wgty;
          }
          if(x>0 || xperiodic) {
            OC_INDEX j = i-1;
            if(x==0) j += xdim;
            OC_REAL8m lexpair = lexrow[region_id[j]];
            sum += Ms[j]*lexpair*lexpair*wgtx;
          }
          if(x<xdim-1 || xperiodic) {
            OC_INDEX j = i+1;
            if(x==xdim-1) j -= xdim;
            OC_REAL8m lexpair = lexrow[region_id[j]];
            sum += Ms[j]*lexpair*lexpair*wgtx;
          }
          if(y<ydim-1 || yperiodic) {
            OC_INDEX j = i+xdim;
            if(y==ydim-1) j -= xydim;
            OC_REAL8m lexpair = lexrow[region_id[j]];
            sum += Ms[j]*lexpair*lexpair*wgty;
          }
          if(z<zdim-1 || zperiodic) {
            OC_INDEX j = i+xydim;
            if(z==zdim-1) j -= xyzdim;
            OC_REAL8m lexpair = lexrow[region_id[j]];
            sum += Ms[j]*lexpair*lexpair*wgtz;
          }
          val[i].x += sum;
          val[i].y += sum;
          val[i].z += sum;
        }
      }
    }
  } else { // A_TYPE
    OC_REAL8m wgtx = 2.0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
    OC_REAL8m wgty = 2.0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
    OC_REAL8m wgtz = 2.0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());
    OC_INDEX i = -1;
    for(OC_INDEX z=0;z<zdim;++z) {
      for(OC_INDEX y=0;y<ydim;++y) {
        for(OC_INDEX x=0;x<xdim;++x) {
          ++i;
          if(0.0 == Ms[i]) continue; // Ignore cells with Ms == 0.
          OC_REAL8m* Arow = coef[region_id[i]];
          OC_REAL8m sum = 0.0;
          if(z>0 || zperiodic) {
            OC_INDEX j = i-xydim;
            if(z==0) j += xyzdim;
              OC_REAL8m Apair = Arow[region_id[j]];
              if(Apair!=0.0 && Ms[j]!=0.0) {
                sum += Apair*wgtz;
              }
          }
          if(y>0 || yperiodic) {
            OC_INDEX j = i-xdim;
            if(y==0) j += xydim;
            OC_REAL8m Apair = Arow[region_id[j]];
            if(Apair!=0.0 && Ms[j]!=0.0) {
              sum += Apair*wgty;
            }
          }
          if(x>0 || xperiodic) {
            OC_INDEX j = i-1;
            if(x==0) j += xdim;
            OC_REAL8m Apair = Arow[region_id[j]];
            if(Apair!=0.0 && Ms[j]!=0.0) {
              sum += Apair*wgtx;
            }
          }
          if(x<xdim-1 || xperiodic) {
            OC_INDEX j = i+1;
            if(x==xdim-1) j -= xdim;
            OC_REAL8m Apair = Arow[region_id[j]];
            if(Apair!=0.0 && Ms[j]!=0.0) {
              sum += Apair*wgtx;
            }
          }
          if(y<ydim-1 || yperiodic) {
            OC_INDEX j = i+xdim;
            if(y==ydim-1) j -= xydim;
            OC_REAL8m Apair = Arow[region_id[j]];
            if(Apair!=0.0 && Ms[j]!=0.0) {
              sum += Apair*wgty;
            }
          }
          if(z<zdim-1 || zperiodic) {
            OC_INDEX j = i+xydim;
            if(z==zdim-1) j -= xyzdim;
            OC_REAL8m Apair = Arow[region_id[j]];
            if(Apair!=0.0 && Ms[j]!=0.0) {
              sum += Apair*wgtz;
            }
          }
          sum /= Ms[i];
          val[i].x += sum;
          val[i].y += sum;
          val[i].z += sum;
        }
      }
    }
  }

  return 1;
}
