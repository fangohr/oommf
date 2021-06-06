/* FILE: uniformexchange.cc            -*-Mode: c++-*-
 *
 * Uniform 6 neighbor exchange energy on rectangular mesh,
 * derived from Oxs_Energy class.
 *
 */

#include <cctype>

#include <string>

#include "nb.h"
#include "director.h"
#include "mesh.h"
#include "meshvalue.h"
#include "oxswarn.h"
#include "simstate.h"
#include "threevector.h"
#include "rectangularmesh.h"
#include "uniformexchange.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_UniformExchange);

/* End includes */

// Revision information, set via CVS keyword substitution
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1.75 $",
   "$Date: 2016/03/04 22:08:58 $",
   "$Author: donahue $",
   "Michael J. Donahue (michael.donahue@nist.gov)");

#undef SKIP_PROBLEM_CODE
/// The 8.0 release of the Intel icc/ia64/Lintel compiler (and perhaps
/// others) gets hung up trying to compile the code wrapped up below in
/// the "SKIP_PROBLEM_CODE" #ifdef sections, at least when "-O3"
/// optimization is enabled.  These blocks provide features that are
/// undocumented at this time (April-2004), so for now we just disable
/// compiling of these blocks.  Remove the above #define to enable
/// compilation.

// Constructor
Oxs_UniformExchange::Oxs_UniformExchange(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ChunkEnergy(name,newdtr,argstr),
    excoeftype(A_UNKNOWN), A(-1.), lex(-1.),
    kernel(NGBR_UNKNOWN), 
    xperiodic(0),yperiodic(0),zperiodic(0),
    mesh_id(0), energy_density_error_estimate(-1)
{
  // Process arguments
  OC_BOOL has_A = HasInitValue("A");
  OC_BOOL has_lex = HasInitValue("lex");
  if(has_A && has_lex) {
    throw Oxs_ExtError(this,"Invalid exchange coefficient request:"
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
  } else if(kernel_request.compare("6ngbralt")==0) {
    kernel = NGBR_6_ALT;
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
      + String("\n Should be one of 6ngbr, 6ngbrfree, 6ngbralt,"
	       " 12ngbr, 12ngbrfree, 12ngbrmirror, or 26ngbr.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  if(A_TYPE != excoeftype && NGBR_6_MIRROR != kernel) {
    throw Oxs_ExtError(this,"Invalid exchange coefficient+kernel"
			 " combination; lex specification currently"
			 " only supported using 6ngbr kernel.");
  }

  VerifyAllInitArgsUsed();

  // Setup outputs
  maxspinangle_output.Setup(this,InstanceName(),"Max Spin Ang","deg",1,
			    &Oxs_UniformExchange::UpdateDerivedOutputs);
  maxspinangle_output.Register(director,0);
  stage_maxspinangle_output.Setup(this,InstanceName(),"Stage Max Spin Ang",
                            "deg",1,
			    &Oxs_UniformExchange::UpdateDerivedOutputs);
  stage_maxspinangle_output.Register(director,0);
  run_maxspinangle_output.Setup(this,InstanceName(),"Run Max Spin Ang","deg",1,
			    &Oxs_UniformExchange::UpdateDerivedOutputs);
  run_maxspinangle_output.Register(director,0);
}

Oxs_UniformExchange::~Oxs_UniformExchange()
{}

OC_BOOL Oxs_UniformExchange::Init()
{
  mesh_id = 0;
  xcoef.Free();
  ycoef.Free();
  zcoef.Free();
  energy_density_error_estimate = -1;
  return Oxs_ChunkEnergy::Init();
}

void
Oxs_UniformExchange::CalcEnergy6NgbrFree
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_CommonRectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{
#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  const OC_REAL8m hcoef = -2/MU0;
  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  // Note: For  maxangle calculation, it suffices
  // to check spin[j]-spin[i] for j>i.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      OC_REAL8m Msii = Ms_inverse[i];
      if(Msii == 0.0) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      ThreeVector base = spin[i];
      ThreeVector sum(0.,0.,0.);
      if(x>0) {
        OC_INDEX j = i-1;
        if(Ms_inverse[j]!=0.0) sum = (spin[j] - base);
      }
      if(x==0 && xperiodic) {
        OC_INDEX j = i+xdim-1;
        if(Ms_inverse[j]!=0.0) sum = (spin[j] - base);
      }
      if(x<xdim-1 || (x==xdim-1 && xperiodic)) {
        OC_INDEX j = i+1;
        if(x==xdim-1) j -= xdim;
        if(Ms_inverse[j]!=0.0) {
          ThreeVector diff = spin[j] - base;
          sum += diff;
          OC_REAL8m dot = diff.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(!xperiodic && xdim>2) { // Free boundary correction
        if(x==0 || x==xdim-2) {
          OC_INDEX j = i+1;
          if(Ms_inverse[j]!=0.0) {
            sum += 0.5*(spin[j] - base);
          }
        }
        if(x==1 || x==xdim-1) {
          OC_INDEX j = i-1;
          if(Ms_inverse[j]!=0.0) {
            sum += 0.5*(spin[j] - base);
          }
        }
      }
      sum *= wgtx;

      ThreeVector temp(0.,0.,0.);
      if(y>0) {
        OC_INDEX j = i-xdim;
        if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
      }
      if(y==0 && yperiodic) {
        OC_INDEX j = i-xdim+xydim;
        if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
      }
      if(y<ydim-1 || (y==ydim-1 && yperiodic)) {
        OC_INDEX j = i+xdim;
        if(y==ydim-1) j -= xydim;
        if(Ms_inverse[j]!=0.0) {
          ThreeVector diff = spin[j] - base;
          temp += diff;
          OC_REAL8m dot = diff.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(!yperiodic && ydim>2) { // Free boundary correction
        if(y==0 || y==ydim-2) {
          OC_INDEX j = i+xdim;
          if(Ms_inverse[j]!=0.0) {
            temp += 0.5*(spin[j] - base);
          }
        }
        if(y==1 || y==ydim-1) {
          OC_INDEX j = i-xdim;
          if(Ms_inverse[j]!=0.0) {
            temp += 0.5*(spin[j] - base);
          }
        }
      }
      sum += wgty*temp;

      temp.Set(0.,0.,0.);
      if(z>0) {
        OC_INDEX j = i-xydim;
        if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
      }
      if(z==0 && zperiodic) {
        OC_INDEX j = i-xydim+xyzdim;
        if(Ms_inverse[j]!=0.0) temp = (spin[j] - base);
      }
      if(z<zdim-1 || (z==zdim-1 && zperiodic)) {
        OC_INDEX j = i+xydim;
        if(z==zdim-1) j -= xyzdim;
        if(Ms_inverse[j]!=0.0) {
          ThreeVector diff = spin[j] - base;
          temp += diff;
          OC_REAL8m dot = diff.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(!zperiodic && zdim>2) { // Free boundary correction
        if(z==0 || z==zdim-2) {
          OC_INDEX j = i+xydim;
          if(Ms_inverse[j]!=0.0) {
            temp += 0.5*(spin[j] - base);
          }
        }
        if(z==1 || z==zdim-1) {
          OC_INDEX j = i-xydim;
          if(Ms_inverse[j]!=0.0) {
            temp += 0.5*(spin[j] - base);
          }
        }
      }
      sum += wgtz*temp;

      OC_REAL8m ei = base.x*sum.x + base.y*sum.y + base.z*sum.z;
      sum.x *= hcoef*Msii; sum.y *= hcoef*Msii; sum.z *= hcoef*Msii;
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
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

void
Oxs_UniformExchange::CalcEnergy6NgbrMirror
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_CommonRectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{

#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  OC_REAL8m thread_maxdot_x = thread_maxdot;
  OC_REAL8m thread_maxdot_y = thread_maxdot;
  OC_REAL8m thread_maxdot_z = thread_maxdot;
  // Note: For maxangle calculation, it suffices to check
  // spin[j]-spin[i] for j>i, or j<i, or various mixes of the two.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
    ThreeVector xlast(0.,0.,0.);
    if(x>0 || (x==0 && xperiodic)) {
      OC_INDEX j = i-1;
      if(x==0) j += xdim;
      if(Ms_inverse[j]!=0.0) xlast = (spin[j]-spin[i]);
    }
    while(x<xstop) {
      if(0 == Ms_inverse[i]) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        xlast.Set(0.,0.,0.);
      } else {
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
        Oc_Nop(Ms_inverse[i]); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
        /// (others?) has an optimization bug that segfaults in the
        /// following code block.  Problem appears to be with the
        /// -fstrict-aliasing option (see demag-threaded.cc, line 1664),
        /// which is thrown in by -fast on Mac OS X/x86.
#endif
        const OC_REAL8m hmult = (-2/MU0) * Ms_inverse[i];

        ThreeVector base = spin[i];
        ThreeVector sum = xlast;
        if(x<xdim-1 || (x==xdim-1 && xperiodic)) {
          OC_INDEX j = i + 1;
          if(x==xdim-1) j -= xdim;
          if(Ms_inverse[j]!=0) {
            xlast = (base - spin[j]);
            sum -= xlast;
            OC_REAL8m dot = xlast.MagSq();
            if(dot>thread_maxdot_x) thread_maxdot_x = dot;
          }
        }
        sum *= wgtx;

        ThreeVector tempy(0.,0.,0.);
        if(y>0 || (y==0 && yperiodic)) {
          OC_INDEX j = i-xdim;
          if(y==0) j += xydim;
          if(Ms_inverse[j]!=0.0) {
            tempy = (spin[j] - base);
            OC_REAL8m dot = tempy.MagSq();
            if(dot>thread_maxdot_y) thread_maxdot_y = dot;
          }
        }
        if(y<ydim-1 || (y==ydim-1 && yperiodic)) {
          OC_INDEX j = i+xdim;
          if(y==ydim-1) j -= xydim;
          if(Ms_inverse[j]!=0.0) {
            tempy += (spin[j] - base);
          }
        }
        sum += wgty*tempy;

        ThreeVector tempz(0.,0.,0.);
        if(z>0 || (z==0 && zperiodic)) {
          OC_INDEX j = i-xydim;
          if(z==0) j+= xyzdim;
          if(Ms_inverse[j]!=0.0) {
            tempz = (spin[j] - base);
            OC_REAL8m dot = tempz.MagSq();
            if(dot>thread_maxdot_z) thread_maxdot_z = dot;
          }
        }
        if(z<zdim-1 || (z==zdim-1 && zperiodic)) {
          OC_INDEX j = i+xydim;
          if(z==zdim-1) j -= xyzdim;
          if(Ms_inverse[j]!=0.0) {
            tempz += (spin[j] - base);
          }
        }
        sum += wgtz*tempz;

        OC_REAL8m ei = base.x*sum.x + base.y*sum.y + base.z*sum.z;
        sum.x *= hmult;  sum.y *= hmult;   sum.z *= hmult;

        // The following computation of mxH is wasted if ocedt.mxH and
        // ocedt.mxH_accum are NULL (as happens if one is using the
        // old-style GetEnergy interface), but timing tests imply that
        // the loss in that case is small, but the win is significant
        // for the case where ocedt.mxH_accum is non-NULL (as happens
        // if one is using the new-style
        // ComputeEnergy/AccumEnergyAndTorque interface).
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
      }
      ++i;
      ++x;
    }
    x=0;
    if((++y)>=ydim) {
      y=0;
      ++z;
    }
  }
  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0); 
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  if(thread_maxdot_x>thread_maxdot) thread_maxdot = thread_maxdot_x;
  if(thread_maxdot_y>thread_maxdot) thread_maxdot = thread_maxdot_y;
  if(thread_maxdot_z>thread_maxdot) thread_maxdot = thread_maxdot_z;
  maxdot[threadnumber] = thread_maxdot;
}

void
Oxs_UniformExchange::CalcEnergy6NgbrMirror_lex
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms,
 const Oxs_CommonRectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{

#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  OC_REAL8m wgtx = lex*lex/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = lex*lex/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = lex*lex/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

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
    ThreeVector xlast(0.,0.,0.);
    if(x>0 || (x==0 && xperiodic)) {
      OC_INDEX j = i-1;
      if(x==0) j += xdim;
      if(Ms[j]!=0.0) xlast = (spin[j]-spin[i]);
    }
    while(x<xstop) {
      if(0 == Ms[i]) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        xlast.Set(0.,0.,0.);
      } else {
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
        Oc_Nop(Ms[i]); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
        /// (others?) has an optimization bug that segfaults in the
        /// following code block.  Problem appears to be with the
        /// -fstrict-aliasing option (see demag-threaded.cc, line 1664),
        /// which is thrown in by -fast on Mac OS X/x86.
#endif
	const OC_REAL8m emult = (-0.5*MU0)*Ms[i];

        ThreeVector base = spin[i];
        ThreeVector sum = xlast;
        if(x>0) sum *= Ms[i-1];
        if(x==0 && xperiodic) sum *= Ms[i-1+xdim];
        if(x<xdim-1 || (x==xdim-1 && xperiodic)) {
          OC_INDEX j = i + 1;
          if(x==xdim-1) j -= xdim;
          if(Ms[j]!=0) {
            xlast = (base - spin[j]);
            OC_REAL8m dot = xlast.MagSq();
            sum -= Ms[j]*xlast;
            if(dot>thread_maxdot) thread_maxdot = dot;
          }
        }
        sum *= wgtx;

        ThreeVector temp(0.,0.,0.);
        if(y>0 || (y==0 && yperiodic)) {
          OC_INDEX j = i-xdim;
          if(y==0) j += xydim;
          if(Ms[j]!=0.0) {
            temp = spin[j] - base;
            OC_REAL8m dot = temp.MagSq();
            if(dot>thread_maxdot) thread_maxdot = dot;
            temp *= Ms[j];
          }
        }
        if(y<ydim-1 || (y==ydim-1 && yperiodic)) {
          OC_INDEX j = i+xdim;
          if(y==ydim-1) j -= xydim;
          if(Ms[j]!=0.0) {
            temp += Ms[j]*(spin[j] - base);
          }
        }
        sum += wgty*temp;

        temp.Set(0.,0.,0.);
        if(z>0 || (z==0 && zperiodic)) {
          OC_INDEX j = i-xydim;
          if(z==0) j+= xyzdim;
          if(Ms[j]!=0.0) {
            temp = spin[j] - base;
            OC_REAL8m dot = temp.MagSq();
            if(dot>thread_maxdot) thread_maxdot = dot;
            temp *= Ms[j];
          }
        }
        if(z<zdim-1 || (z==zdim-1 && zperiodic)) {
          OC_INDEX j = i+xydim;
          if(z==zdim-1) j -= xyzdim;
          if(Ms[j]!=0.0) {
            temp += Ms[j]*(spin[j]- base);
          }
        }
        sum += wgtz*temp;

        OC_REAL8m ei = emult * (base.x*sum.x + base.y*sum.y + base.z*sum.z);
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
      }
      ++i;
      ++x;
    }
    x=0;
    if((++y)>=ydim) {
      y=0;
      ++z;
    }
  }
  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0); 
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

void
Oxs_UniformExchange::CalcEnergy6NgbrMirrorStd
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_CommonRectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{ // This is the same as ::CalcEnergy6NgbrMirror,
  // except that the spin at cell i is *not* subtracted
  // from the field at spin i.  This is the "canonical"
  // or "standard" way that exchange energy is usually
  // written in the literature.  The numerics of this
  // formulation are inferior to the other in the case
  // where spin i and neighboring spin j are nearly
  // parallel.

#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

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
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      if(0 == Ms_inverse[i]) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      const OC_REAL8m hmult = (-2/MU0) * Ms_inverse[i];

      ThreeVector sum(0.,0.,0.);
      if(x>0 || (x==0 && xperiodic)) {
        OC_INDEX j = i-1;
        if(x==0) j += xdim;
        if(Ms_inverse[j]!=0.0) {
          sum = spin[j];
          OC_REAL8m dot = (spin[j]-spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x<xdim-1 || (x==xdim-1 && xperiodic)) {
        OC_INDEX j = i+1;
        if(x==xdim-1) j -= xdim;
        if(Ms_inverse[j]!=0.0) {
          sum += spin[j];
        }
      }
      sum *= wgtx;

      ThreeVector temp(0.,0.,0.);
      if(y>0 || (y==0 && yperiodic)) {
        OC_INDEX j = i-xdim;
        if(y==0) j += xydim;
        if(Ms_inverse[j]!=0.0) {
          temp = spin[j];
          OC_REAL8m dot = (spin[j]-spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(y<ydim-1 || (y==ydim-1 && yperiodic)) {
        OC_INDEX j = i+xdim;
        if(y==ydim-1) j -= xydim;
        if(Ms_inverse[j]!=0.0) {
          temp += spin[j];
        }
      }
      sum += wgty*temp;

      temp.Set(0.,0.,0.);
      if(z>0 || (z==0 && zperiodic)) {
        OC_INDEX j = i-xydim;
        if(z==0) j += xyzdim;
        if(Ms_inverse[j]!=0.0) {
          temp = spin[j];
          OC_REAL8m dot = (spin[j]-spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(z<zdim-1 || (z==zdim-1 && zperiodic)) {
        OC_INDEX j = i+xydim;
        if(z==zdim-1) j -= xyzdim;
        if(Ms_inverse[j]!=0.0) {
          temp += spin[j];
        }
      }
      sum += wgtz*temp;

      OC_REAL8m ei = spin[i].x*sum.x + spin[i].y*sum.y + spin[i].z*sum.z;
      sum.x *= hmult;  sum.y *= hmult;   sum.z *= hmult;
      OC_REAL8m tx = spin[i].y*sum.z - spin[i].z*sum.y;
      OC_REAL8m ty = spin[i].z*sum.x - spin[i].x*sum.z;
      OC_REAL8m tz = spin[i].x*sum.y - spin[i].y*sum.x;

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
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

OC_REAL8m
Oxs_UniformExchange::ComputeAngle
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
  return 2*asin(0.5*d.Mag());
}

void
Oxs_UniformExchange::CalcEnergy6NgbrBigAngMirror
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_CommonRectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{
#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  const OC_REAL8m hcoef = -2/MU0;
  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

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
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      OC_REAL8m Msii = Ms_inverse[i];
      if(Msii == 0.0) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      ThreeVector base = spin[i];

      ThreeVector sum(0.,0.,0.);
      OC_REAL8m Esum = 0.0;
      if(x>0 || (x==0 && xperiodic)) {
        OC_INDEX j = i-1;
        if(x==0) j += xdim;
        if(Ms_inverse[j]!=0.0) {
          OC_REAL8m theta = ComputeAngle(spin[j],base);
          OC_REAL8m sintheta = sin(theta);
          Esum = theta*theta;
          ThreeVector vtmp = spin[j] - base;
          OC_REAL8m dot = vtmp.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
          if(sintheta>0.0) vtmp *= theta/sintheta;
          sum = vtmp;
        }
      }
      if(x<xdim-1 || (x==xdim-1 && xperiodic)) {
        OC_INDEX j = i+1;
        if(x==xdim-1) j -= xdim;
        if(Ms_inverse[j]!=0.0) {
          OC_REAL8m theta = ComputeAngle(spin[j],base);
          OC_REAL8m sintheta = sin(theta);
          Esum += theta*theta;
          ThreeVector vtmp = spin[j] - base;
          if(sintheta>0.0) vtmp *= theta/sintheta;
          sum += vtmp;
        }
      }
      Esum *= wgtx;
      sum *= wgtx;

      ThreeVector temp(0.,0.,0.);
      OC_REAL8m Etemp = 0.0;
      if(y>0 || (y==0 && yperiodic)) {
        OC_INDEX j = i-xdim;
        if(y==0) j += xydim;
        if(Ms_inverse[j]!=0.0) {
          OC_REAL8m theta = ComputeAngle(spin[j],base);
          OC_REAL8m sintheta = sin(theta);
          Etemp = theta*theta;
          ThreeVector vtmp = spin[j] - base;
          OC_REAL8m dot = vtmp.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
          if(sintheta>0.0) vtmp *= theta/sintheta;
          temp = vtmp;
        }
      }
      if(y<ydim-1 || (y==ydim-1 && yperiodic)) {
        OC_INDEX j = i+xdim;
        if(y==ydim-1) j -= xydim;
        if(Ms_inverse[j]!=0.0) {
          OC_REAL8m theta = ComputeAngle(spin[j],base);
          OC_REAL8m sintheta = sin(theta);
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
      if(z>0 || (z==0 && zperiodic)) {
        OC_INDEX j = i-xydim;
        if(z==0) j += xyzdim;
        if(Ms_inverse[j]!=0.0) {
          OC_REAL8m theta = ComputeAngle(spin[j],base);
          OC_REAL8m sintheta = sin(theta);
          Etemp = theta*theta;
          ThreeVector vtmp = spin[j] - base;
          OC_REAL8m dot = vtmp.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
          if(sintheta>0.0) vtmp *= theta/sintheta;
          temp = vtmp;
        }
      }
      if(z<zdim-1 || (z==zdim-1 && zperiodic)) {
        OC_INDEX j = i+xydim;
        if(z==zdim-1) j -= xyzdim;
        if(Ms_inverse[j]!=0.0) {
          OC_REAL8m theta = ComputeAngle(spin[j],base);
          OC_REAL8m sintheta = sin(theta);
          Etemp += theta*theta;
          ThreeVector vtmp = spin[j] - base;
          if(sintheta>0.0) vtmp *= theta/sintheta;
          temp += vtmp;
        }
      }
      Esum += wgtz*Etemp;
      sum += wgtz*temp;

      OC_REAL8m ei = -0.5 * Esum;
      sum.x *= (hcoef*Msii); sum.y *= (hcoef*Msii); sum.z *= (hcoef*Msii);
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
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

void
Oxs_UniformExchange::CalcEnergy6NgbrZD2
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_CommonRectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{
#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  const OC_REAL8m hcoef = -2/MU0;
  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

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
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      OC_REAL8m Msii = Ms_inverse[i];
      if(Msii == 0.0) {
        if(ocedt.energy) (*ocedt.energy)[i]=0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      ThreeVector base = spin[i];
      ThreeVector sum(0.,0.,0.);
      if(x>0 || (x==0 && xperiodic)) {
        OC_INDEX j = i-1;
        if(x==0) j += xdim;
        if(Ms_inverse[j]!=0.0) {
          sum = (spin[j] - base);
          OC_REAL8m dot = sum.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x<xdim-1 || (x==xdim-1 && xperiodic)) {
        OC_INDEX j = i+1;
        if(x==xdim-1) j -= xdim;
        if(Ms_inverse[j]!=0.0) {
          sum += (spin[j] - base);
        }
      }
      if(xdim>2 && !xperiodic) { // m''=0 boundary correction
        if(x==0 || x==xdim-2) {
          OC_INDEX j = i+1;
          if(Ms_inverse[j]!=0.0) {
            sum -= 0.5*(spin[j] - base);
          }
        }
        if(x==1 || x==xdim-1) {
          OC_INDEX j = i-1;
          if(Ms_inverse[j]!=0.0) {
            sum -= 0.5*(spin[j] - base);
          }
        }
      }
      sum *= wgtx;

      ThreeVector temp(0.,0.,0.);
      if(y>0 || (y==0 && yperiodic)) {
        OC_INDEX j = i-xdim;
        if(y==0) j += xydim;
        if(Ms_inverse[j]!=0.0) {
          temp = (spin[j] - base);
          OC_REAL8m dot = temp.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(y<ydim-1 || (y==ydim-1 && yperiodic)) {
        OC_INDEX j = i+xdim;
        if(y==ydim-1) j -= xydim;
        if(Ms_inverse[j]!=0.0) {
          temp += (spin[j] - base);
        }
      }
      if(ydim>2 && !yperiodic) { // m''=0 boundary correction
        if(y==0 || y==ydim-2) {
          OC_INDEX j = i+xdim;
          if(Ms_inverse[j]!=0.0) {
            temp -= 0.5*(spin[j] - base);
          }
        }
        if(y==1 || y==ydim-1) {
          OC_INDEX j = i-xdim;
          if(Ms_inverse[j]!=0.0) {
            temp -= 0.5*(spin[j] - base);
          }
        }
      }
      sum += wgty*temp;

      temp.Set(0.,0.,0.);
      if(z>0 || (z==0 && zperiodic)) {
        OC_INDEX j = i-xydim;
        if(z==0) j += xyzdim;
        if(Ms_inverse[j]!=0.0) {
          temp = (spin[j] - base);
          OC_REAL8m dot = temp.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(z<zdim-1 || (z==zdim-1 && zperiodic)) {
        OC_INDEX j = i+xydim;
        if(z==zdim-1) j -= xyzdim;
        if(Ms_inverse[j]!=0.0) {
          temp += (spin[j] - base);
        }
      }
      if(zdim>2 && !zperiodic) {  // m''=0 boundary correction
        if(z==0 || z==zdim-2) {
          OC_INDEX j = i+xydim;
          if(Ms_inverse[j]!=0.0) {
            temp -= 0.5*(spin[j] - base);
          }
        }
        if(z==1 || z==zdim-1) {
          OC_INDEX j = i-xydim;
          if(Ms_inverse[j]!=0.0) {
            temp -= 0.5*(spin[j] - base);
          }
        }
      }
      sum += wgtz*temp;

      OC_REAL8m ei = base.x*sum.x + base.y*sum.y + base.z*sum.z;
      sum.x *= (hcoef*Msii); sum.y *= (hcoef*Msii); sum.z *= (hcoef*Msii);
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
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

void
Oxs_UniformExchange::CalcEnergy6NgbrAlt
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_CommonRectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{ // Exchange computation based on \int (grad(m))^2 rather than
  // \int m.grad^2(m), with O(h^2) energy density approximation
  // in edge cells.  See NOTES VII, 2-Mar-2016, pp 120-125.
  // STILL TO DO:
  //  (1) The code below applies corrections to edges along
  //      simulation boundaries, but not interior edges formed
  //      by Ms=0 cells.
  //  (2) Handle cases where dim = 2 (and check dim=3 is OK)

#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif
  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  OC_REAL8m thread_maxdot_x = thread_maxdot;
  OC_REAL8m thread_maxdot_y = thread_maxdot;
  OC_REAL8m thread_maxdot_z = thread_maxdot;
  // Note: For maxangle calculation, it suffices to check
  // spin[j]-spin[i] for j>i, or j<i, or various mixes of the two.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
    ThreeVector xlast(0.,0.,0.);
    if(x>0 || (x==0 && xperiodic)) {
      OC_INDEX j = i-1;
      if(x==0) j += xdim;
      if(Ms_inverse[j]!=0.0) {
        xlast = (spin[j]-spin[i]);
        if(!xperiodic && (x==1 || x==xdim-1)) {
          xlast *= 25./24.;
        }
      }
    }
    while(x<xstop) {
      if(0 == Ms_inverse[i]) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        xlast.Set(0.,0.,0.);
      } else {
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
        Oc_Nop(Ms_inverse[i]); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
        /// (others?) has an optimization bug that segfaults in the
        /// following code block.  Problem appears to be with the
        /// -fstrict-aliasing option (see demag-threaded.cc, line 1664),
        /// which is thrown in by -fast on Mac OS X/x86.
#endif
        const OC_REAL8m hmult = (-2/MU0) * Ms_inverse[i];

        ThreeVector base = spin[i];
        ThreeVector sum = xlast;
        if(x<xdim-1 || (x==xdim-1 && xperiodic)) {
          OC_INDEX j = i + 1;
          if(x==xdim-1) j -= xdim;
          if(Ms_inverse[j]!=0) {
            xlast = (base - spin[j]);
            OC_REAL8m dot = xlast.MagSq();
            if(dot>thread_maxdot_x) thread_maxdot_x = dot;
            if(!xperiodic && (x==0 || x==xdim-2)) {
              xlast *= 25./24.;
            }
            sum -= xlast;
          }
        }
        sum *= wgtx;

        ThreeVector tempy(0.,0.,0.);
        if(y>0 || (y==0 && yperiodic)) {
          OC_INDEX j = i-xdim;
          if(y==0) j += xydim;
          if(Ms_inverse[j]!=0.0) {
            tempy = (spin[j] - base);
            OC_REAL8m dot = tempy.MagSq();
            if(dot>thread_maxdot_y) thread_maxdot_y = dot;
            if(!yperiodic && (y==1 || y==ydim-1)) {
              tempy *= 25./24.;
            }
          }
        }
        if(y<ydim-1 || (y==ydim-1 && yperiodic)) {
          OC_INDEX j = i+xdim;
          if(y==ydim-1) j -= xydim;
          if(Ms_inverse[j]!=0.0) {
            if(!yperiodic && (y==0 || y==ydim-2)) {
              tempy += 25./24.*(spin[j] - base);
            } else {
              tempy += (spin[j] - base);
            }
          }
        }
        sum += wgty*tempy;

        ThreeVector tempz(0.,0.,0.);
        if(z>0 || (z==0 && zperiodic)) {
          OC_INDEX j = i-xydim;
          if(z==0) j+= xyzdim;
          if(Ms_inverse[j]!=0.0) {
            tempz = (spin[j] - base);
            OC_REAL8m dot = tempz.MagSq();
            if(dot>thread_maxdot_z) thread_maxdot_z = dot;
            if(!zperiodic && (z==1 || z==zdim-1)) {
              tempz *= 25./24.;
            }
          }
        }
        if(z<zdim-1 || (z==zdim-1 && zperiodic)) {
          OC_INDEX j = i+xydim;
          if(z==zdim-1) j -= xyzdim;
          if(Ms_inverse[j]!=0.0) {
            if(!zperiodic && (z==0 || z==zdim-2)) {
              tempz += 25./24.*(spin[j] - base);
            } else {
              tempz += (spin[j] - base);
            }
          }
        }
        sum += wgtz*tempz;

        OC_REAL8m ei = base.x*sum.x + base.y*sum.y + base.z*sum.z;
        sum.x *= hmult;  sum.y *= hmult;   sum.z *= hmult;

        // The following computation of mxH is wasted if ocedt.mxH and
        // ocedt.mxH_accum are NULL (as happens if one is using the
        // old-style GetEnergy interface), but timing tests imply that
        // the loss in that case is small, but the win is significant
        // for the case where ocedt.mxH_accum is non-NULL (as happens
        // if one is using the new-style
        // ComputeEnergy/AccumEnergyAndTorque interface).
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
      }
      ++i;
      ++x;
    }
    x=0;
    if((++y)>=ydim) {
      y=0;
      ++z;
    }
  }
  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0); 
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  if(thread_maxdot_x>thread_maxdot) thread_maxdot = thread_maxdot_x;
  if(thread_maxdot_y>thread_maxdot) thread_maxdot = thread_maxdot_y;
  if(thread_maxdot_z>thread_maxdot) thread_maxdot = thread_maxdot_z;
  maxdot[threadnumber] = thread_maxdot;
}

#ifdef SKIP_PROBLEM_CODE
void
Oxs_UniformExchange::CalcEnergy12NgbrFree
(const Oxs_MeshValue<ThreeVector>&,
 const Oxs_MeshValue<OC_REAL8m>&,
 const Oxs_RectangularMesh*,
 Oxs_ComputeEnergyDataThreaded&,
 OC_INDEX,OC_INDEX,int) const
{
  throw Oxs_ExtError(this,"Oxs_UniformExchange::CalcEnergy12NgbrFree()"
		       " not supported in this build.  See file"
		       " oommf/app/oxs/ext/uniformexchange.cc for details.");
}

void
Oxs_UniformExchange::CalcEnergy12NgbrZD1
(const Oxs_MeshValue<ThreeVector>&,
 const Oxs_MeshValue<OC_REAL8m>&,
 const Oxs_RectangularMesh*,
 Oxs_ComputeEnergyDataThreaded&,
 OC_INDEX,OC_INDEX,int) const
{
  throw Oxs_ExtError(this,"Oxs_UniformExchange::CalcEnergy12NgbrZD1()"
		       " not supported in this build.  See file"
		       " oommf/app/oxs/ext/uniformexchange.cc for details.");
}

#else // SKIP_PROBLEM_CODE

void
Oxs_UniformExchange::CalcEnergy12NgbrFree
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{
#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;

  const OC_REAL8m hcoef = -2/MU0;
  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX()*1152.);
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY()*1152.);
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ()*1152.);

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  // Note: For maxangle calculation, it suffices to check
  // spin[j]-spin[i] for j>i, or j<i, or various mixes of the two.

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
    throw Oxs_ExtError(this,buf);
  }
  // Note: Integral weights ai, bj, and ck are derived in mjd
  // NOTES II, 5-Aug-2002, p178-181.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_REAL8m ck=1.0;
  if(z==0 || z==zdim-1) ck=26./24.;
  if(z==1 || z==zdim-2) ck=21./24.;
  if(z==2 || z==zdim-3) ck=25./24.;

  OC_REAL8m bj=1.0;
  if(y==0 || y==ydim-1) bj=26./24.;
  if(y==1 || y==ydim-2) bj=21./24.;
  if(y==2 || y==ydim-3) bj=25./24.;

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      OC_REAL8m Msii = Ms_inverse[i];
      if(Msii == 0.0) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      OC_REAL8m ai=1.0;
      if(x==0 || x==xdim-1) ai=26./24.;
      if(x==1 || x==xdim-2) ai=21./24.;
      if(x==2 || x==xdim-3) ai=25./24.;

      ThreeVector sum,tsum;
      sum.Set(0.,0.,0.);

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
      if(x>0 && Ms_inverse[i-1]!=0.0) {
        OC_REAL8m dot = (spin[i-1]-spin[i]).MagSq();
        if(dot>thread_maxdot) thread_maxdot = dot;
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
      if(y>0 && Ms_inverse[i-xdim]!=0.0) {
        OC_REAL8m dot = (spin[i-xdim] - spin[i]).MagSq();
        if(dot>thread_maxdot) thread_maxdot = dot;
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
      if(z>0 && Ms_inverse[i-xydim]!=0.0) {
        OC_REAL8m dot = (spin[i-xydim] - spin[i]).MagSq();
        if(dot>thread_maxdot) thread_maxdot = dot;
      }
      sum += (ai*bj*wgtz)*tsum; // ck built into above coefficients.

      OC_REAL8m ei = spin[i].x*sum.x + spin[i].y*sum.y + spin[i].z*sum.z;
      sum.x *= hcoef*Msii; sum.y *= hcoef*Msii; sum.z *= hcoef*Msii;
      OC_REAL8m tx = spin[i].y*sum.z - spin[i].z*sum.y;
      OC_REAL8m ty = spin[i].z*sum.x - spin[i].x*sum.z;
      OC_REAL8m tz = spin[i].x*sum.y - spin[i].y*sum.x;

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
    bj=1.0;
    if((++y)<ydim) {
      if(y==0 || y==ydim-1) bj=26./24.;
      if(y==1 || y==ydim-2) bj=21./24.;
      if(y==2 || y==ydim-3) bj=25./24.;
    } else {
      y=0;
      bj=26./24.;
      ck=1.0;
      ++z;
      if(z==0 || z==zdim-1) ck=26./24.;
      if(z==1 || z==zdim-2) ck=21./24.;
      if(z==2 || z==zdim-3) ck=25./24.;
    }
  }
  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0); 
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

void
Oxs_UniformExchange::CalcEnergy12NgbrZD1
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{
#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;

  const OC_REAL8m hcoef = -2/MU0;
  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX()*240768.);
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY()*240768.);
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ()*240768.);

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  // Note: For maxangle calculation, it suffices to check
  // spin[j]-spin[i] for j>i, or j<i, or various mixes of the two.

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
    throw Oxs_ExtError(this,buf);
  }
  // Note: Integral weights ai, bj, and ck are derived in mjd
  // NOTES II, 5-Aug-2002, p178-181.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_REAL8m ck=1.0;
  if(zdim>1) {
    if(z==0 || z==zdim-1) ck=26./24.;
    if(z==1 || z==zdim-2) ck=21./24.;
    if(z==2 || z==zdim-3) ck=25./24.;
  }

  OC_REAL8m bj=1.0;
  if(ydim>1) {
    if(y==0 || y==ydim-1) bj=26./24.;
    if(y==1 || y==ydim-2) bj=21./24.;
    if(y==2 || y==ydim-3) bj=25./24.;
  }

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      OC_REAL8m Msii = Ms_inverse[i];
      if(Msii == 0.0) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      OC_REAL8m ai=1.0;
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
        if(x>0 && Ms_inverse[i-1]!=0.0) {
          OC_REAL8m dot = (spin[i-1]-spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
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
        if(y>0 && Ms_inverse[i-xdim]!=0.0) {
          OC_REAL8m dot = (spin[i-xdim]-spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
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
        if(z>0 && Ms_inverse[i-xydim]!=0.0) {
          OC_REAL8m dot = (spin[i-xydim]-spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
        sum += (ai*bj*wgtz)*tsum; // ck built into above coefficients.
      }
      /// asdf SLOW CODE? ///
      OC_REAL8m ei = spin[i].x*sum.x + spin[i].y*sum.y + spin[i].z*sum.z;
      sum.x *= hcoef*Msii; sum.y *= hcoef*Msii; sum.z *= hcoef*Msii;
      OC_REAL8m tx = spin[i].y*sum.z - spin[i].z*sum.y;
      OC_REAL8m ty = spin[i].z*sum.x - spin[i].x*sum.z;
      OC_REAL8m tz = spin[i].x*sum.y - spin[i].y*sum.x;

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
    bj=1.0;
    if((++y)<ydim) {
      if(y==0 || y==ydim-1) bj=26./24.;
      if(y==1 || y==ydim-2) bj=21./24.;
      if(y==2 || y==ydim-3) bj=25./24.;
    } else {
      y=0;
      bj=26./24.;
      ck=1.0;
      ++z;
      if(z==0 || z==zdim-1) ck=26./24.;
      if(z==1 || z==zdim-2) ck=21./24.;
      if(z==2 || z==zdim-3) ck=25./24.;
    }
  }
  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0); 
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}
#endif // SKIP_PROBLEM_CODE

void
Oxs_UniformExchange::InitCoef_12NgbrZD1
(OC_INDEX size,
 OC_REAL8m wgt[3],
 Nb_2DArrayWrapper<OC_REAL8m>& coef) const
{
  if(size==0) {
    coef.Free();
    return;
  }

  if(size>10) size=10; // 10x10 is largest special coefficient
  /// array ever needed.

  OC_INDEX i,j;

  // Allocate memory
  OC_INDEX tsize1,tsize2;
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
    throw Oxs_ExtError(this,buf);
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
    OC_REAL8m tmp = (coef(i,j) + coef(j,i))/2.0;
    coef(i,j) = coef(j,i) = tmp;
  }

  // For improved numerics, adjust diagonal elements so rows
  // are zero-sum
  for(i=0;i<size;i++) {
    OC_REAL8m rowsum=0.0;
    for(j=0;j<size;j++) rowsum += coef(i,j);
    coef(i,i) -= rowsum;
  }

  // That's all folks!
}

void
Oxs_UniformExchange::CalcEnergy12NgbrZD1B
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{
  // See mjd's NOTES III, 7/30-Jan-2003, p27-35
  // EXTRA NOTE: Beware that "field" computed here is
  //  -grad(E)/(mu_0*cellvolume), which is not a good representation
  //  for H.  See mjd's NOTES II, 9-Aug-2002, p183.

#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;

  const OC_REAL8m hcoef = -2/MU0;
  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());

  Nb_Xpfloat energy_sum = 0.0;
  OC_REAL8m thread_maxdot = maxdot[threadnumber];
  // Note: For maxangle calculation, it suffices to check
  // spin[j]-spin[i] for j>i, or j<i, or various mixes of the two.

  OC_INDEX x,y,z;
  mesh->GetCoords(node_start,x,y,z);

  OC_INDEX zoff = z;        // "Distance" from boundary
  OC_INDEX zcoef_index = z; // Index into zcoef array
  if(zdim-1<2*z) { // Backside
    zoff = zdim - 1 - z;
    zcoef_index = OC_MIN(10,zdim) - 1 - zoff;
  }
  OC_REAL8m ck=1.0;
  if(zoff<3) ck = zinteg[zoff];  // Integration weight
  /// Note: zcoef_index is only valid if zoff<5

  OC_INDEX yoff = y;
  OC_INDEX ycoef_index = y;
  if(ydim-1<2*y) {
    yoff = ydim - 1 - y;
    ycoef_index = OC_MIN(10,ydim) - 1 - yoff;
  }
  OC_REAL8m bj=1.0;
  if(yoff<3) bj = yinteg[yoff];
  /// Note: ycoef_index is only valid if yoff<5

  OC_INDEX i = node_start;
  while(i<node_stop) {
    OC_INDEX xstop = xdim;
    if(xdim-x>node_stop-i) xstop = x + (node_stop-i);
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      OC_REAL8m Msii = Ms_inverse[i];
      if(Msii == 0.0) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      OC_INDEX xoff = x;
      OC_INDEX xcoef_index = x;
      if(xdim-1<2*x) {
        xoff = xdim - 1 - x;
        xcoef_index = OC_MIN(10,xdim) - 1 - xoff;
      }
      OC_REAL8m ai=1.0;
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
          OC_INT4m jmin = -1 * static_cast<OC_INT4m>(OC_MIN(2,x));
          OC_INT4m jmax = static_cast<OC_INT4m>(OC_MIN(3,xdim-x));
          for(OC_INT4m j=jmin;j<jmax;j++) {
            tsum += xcoef(xcoef_index,xcoef_index+j)*spin[i+j];
          }
        }
        if(x>0 && Ms_inverse[i-1]!=0.0) {
          OC_REAL8m dot = (spin[i-1]-spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
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
          OC_INT4m jmin = -1*static_cast<OC_INT4m>(OC_MIN(2,y));
          OC_INT4m jmax = static_cast<OC_INT4m>(OC_MIN(3,ydim-y));
          for(OC_INT4m j=jmin;j<jmax;j++) {
            tsum += ycoef(ycoef_index,ycoef_index+j)*spin[i+j*xdim];
          }
        }
        if(y>0 && Ms_inverse[i-xdim]!=0.0) {
          OC_REAL8m dot = (spin[i-xdim] - spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
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
          OC_INT4m jmin = -1*static_cast<OC_INT4m>(OC_MIN(2,z));
          OC_INT4m jmax = static_cast<OC_INT4m>(OC_MIN(3,zdim-z));
          for(OC_INT4m j=jmin;j<jmax;j++) {
            tsum += zcoef(zcoef_index,zcoef_index+j)*spin[i+j*xydim];
          }
        }
        if(z>0 && Ms_inverse[i-xydim]!=0.0) {
          OC_REAL8m dot = (spin[i-xydim]-spin[i]).MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
        sum += (ai*bj*wgtz)*tsum; // ck built into above coefficients.
      }

      OC_REAL8m ei = spin[i].x*sum.x + spin[i].y*sum.y + spin[i].z*sum.z;
      sum.x *= hcoef*Msii; sum.y *= hcoef*Msii; sum.z *= hcoef*Msii;
      OC_REAL8m tx = spin[i].y*sum.z - spin[i].z*sum.y;
      OC_REAL8m ty = spin[i].z*sum.x - spin[i].x*sum.z;
      OC_REAL8m tz = spin[i].x*sum.y - spin[i].y*sum.x;

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
    if((++y)<ydim) {
      yoff = y;
      ycoef_index = y;
      if(ydim-1<2*y) {
        yoff = ydim - 1 - y;
        ycoef_index = OC_MIN(10,ydim) - 1 - yoff;
      }
      bj=1.0;
      if(yoff<3) bj = yinteg[yoff];
      /// Note: ycoef_index is only valid if yoff<5
    } else {
      ycoef_index = yoff = y = 0;
      bj = yinteg[yoff];
      /// Note: ycoef_index is only valid if yoff<5

      ++z;
      zoff = z;        // "Distance" from boundary
      zcoef_index = z; // Index into zcoef array
      if(zdim-1<2*z) { // Backside
        zoff = zdim - 1 - z;
        zcoef_index = OC_MIN(10,zdim) - 1 - zoff;
      }
      ck=1.0;
      if(zoff<3) ck = zinteg[zoff];  // Integration weight
      /// Note: zcoef_index is only valid if zoff<5
    }
  }
  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0); 
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

void
Oxs_UniformExchange::CalcEnergy12NgbrMirror
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_CommonRectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{
#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;
  OC_INDEX xyzdim = xdim*ydim*zdim;

  if((xperiodic && xdim<2) ||
     (yperiodic && ydim<2) ||
     (zperiodic && zdim<2)) {
    throw Oxs_ExtError(this,"Bad parameters:"
     " 12ngbrmirror exchange kernel requires dimension size of"
     " at least 2 in each periodic direction.");
  }

  const OC_REAL8m hcoef = -2/MU0;

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
  OC_REAL8m wgtx = -A/(mesh->EdgeLengthX()*mesh->EdgeLengthX()*12.);
  OC_REAL8m wgty = -A/(mesh->EdgeLengthY()*mesh->EdgeLengthY()*12.);
  OC_REAL8m wgtz = -A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ()*12.);

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
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      OC_REAL8m Msii = Ms_inverse[i];
      if(Msii == 0.0) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      ThreeVector base = spin[i];

      ThreeVector sum_inner(0.,0.,0.);
      ThreeVector sum_outer(0.,0.,0.);
      if(x > 1 || xperiodic) {
        OC_INDEX j1 =  i - 1;  if(x<1) j1 += xdim;
        OC_INDEX j2 =  i - 2;  if(x<2) j2 += xdim;
        // Note: If xperiodic, then xdim >= 2.
        if(Ms_inverse[j1]!=0.0) {
          sum_inner = spin[j1] - base;
          OC_REAL8m dot = sum_inner.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
        if(Ms_inverse[j2]!=0.0) sum_outer = spin[j1] - spin[j2];
      } else if(x == 1) {
        OC_INDEX j = i - 1;
        if(Ms_inverse[j]!=0.0) {
          sum_inner = spin[j] - base;
          OC_REAL8m dot = sum_inner.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      } else if(xdim > 1) {
        // x==0.  Add edge correction.
        OC_INDEX j = i + 1;
        if(Ms_inverse[j]!=0.0) sum_outer = base - spin[j];
      }
      if(x+2 < xdim || xperiodic) {
        // Note: We have to compare 'x+2<xdim', NOT 'x<xdim-2',
        // because x and xdim are unsigned, and xdim may be
        // as small as 1.
        OC_INDEX j1 = i + 1;  if(x+1>=xdim) j1 -= xdim;
        OC_INDEX j2 = i + 2;  if(x+2>=xdim) j2 -= xdim;
        // Note: If xperiodic, then xdim >= 2.
        if(Ms_inverse[j1]!=0.0) sum_inner += spin[j1] - base;
        if(Ms_inverse[j2]!=0.0) sum_outer += spin[j1] - spin[j2];
      } else if(x+2 == xdim) {
        OC_INDEX j = i + 1;
        if(Ms_inverse[j]!=0.0) sum_inner += spin[j] - base;
      } else if(xdim > 1) {
        // x==xdim-1.  Add edge correction.
        OC_INDEX j = i - 1;
        if(Ms_inverse[j]!=0.0) sum_outer += base - spin[j];
      }
      sum_inner *= wgtx;
      sum_outer *= wgtx;

      ThreeVector temp_inner(0.,0.,0.);
      ThreeVector temp_outer(0.,0.,0.);
      if(y > 1 || yperiodic) {
        OC_INDEX j1 = i - xdim;    if(y<1) j1 += xydim;
        OC_INDEX j2 = i - 2*xdim;  if(y<2) j2 += xydim;
        // Note: If yperiodic, then ydim >= 2.
        if(Ms_inverse[j1]!=0.0) {
          temp_inner = spin[j1] - base;
          OC_REAL8m dot = temp_inner.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
        if(Ms_inverse[j2]!=0.0) temp_outer = spin[j1] - spin[j2];
      } else if(y == 1) {
        OC_INDEX j = i - xdim;
        if(Ms_inverse[j]!=0.0) {
          temp_inner = spin[j] - base;
          OC_REAL8m dot = temp_inner.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      } else if(ydim > 1) {
        // y==0.  Add edge correction.
        OC_INDEX j = i + xdim;
        if(Ms_inverse[j]!=0.0) temp_outer = base - spin[j];
      }
      if(y+2 < ydim || yperiodic) {
        OC_INDEX j1 = i + xdim;    if(y+1>=ydim) j1 -= xydim;
        OC_INDEX j2 = i + 2*xdim;  if(y+2>=ydim) j2 -= xydim;
        // Note: If yperiodic, then ydim >= 2.
        if(Ms_inverse[j1]!=0.0) temp_inner += spin[j1] - base;
        if(Ms_inverse[j2]!=0.0) temp_outer += spin[j1] - spin[j2];
      } else if(y+2 == ydim) {
        OC_INDEX j = i + xdim;
        if(Ms_inverse[j]!=0.0) temp_inner += spin[j] - base;
      } else if(ydim > 1) {
        // y==ydim-1.  Add edge correction.
        OC_INDEX j = i - xdim;
        if(Ms_inverse[j]!=0.0) temp_outer += base - spin[j];
      }
      sum_inner += wgty*temp_inner;
      sum_outer += wgty*temp_outer;

      temp_inner.Set(0.,0.,0.);
      temp_outer.Set(0.,0.,0.);
      if(z > 1 || zperiodic) {
        OC_INDEX j1 = i - xydim;    if(z<1) j1 += xyzdim;
        OC_INDEX j2 = i - 2*xydim;  if(z<2) j2 += xyzdim;
        // Note: If zperiodic, then zdim >= 2.
        if(Ms_inverse[j1]!=0.0) {
          temp_inner = spin[j1] - base;
          OC_REAL8m dot = temp_inner.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
        if(Ms_inverse[j2]!=0.0) temp_outer = spin[j1] - spin[j2];
      } else if(z == 1) {
        OC_INDEX j = i - xydim;
        if(Ms_inverse[j]!=0.0) {
          temp_inner = spin[j] - base;
          OC_REAL8m dot = temp_inner.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      } else if(zdim > 1) {
        // z==0.  Add edge correction.
        OC_INDEX j = i + xydim;
        if(Ms_inverse[j]!=0.0) temp_outer = base - spin[j];
      }
      if(z+2 < zdim || zperiodic) {
        OC_INDEX j1 = i + xydim;    if(z+1>=zdim) j1 -= xyzdim;
        OC_INDEX j2 = i + 2*xydim;  if(z+2>=zdim) j2 -= xyzdim;
        // Note: If zperiodic, then zdim >= 2.
        if(Ms_inverse[j1]!=0.0) temp_inner += spin[j1] - base;
        if(Ms_inverse[j2]!=0.0) temp_outer += spin[j1] - spin[j2];
      } else if(z+2 == zdim) {
        OC_INDEX j = i + xydim;
        if(Ms_inverse[j]!=0.0) temp_inner += spin[j] - base;
      } else if(zdim > 1) {
        // z==zdim-1.  Add edge correction.
        OC_INDEX j = i - xydim;
        if(Ms_inverse[j]!=0.0) temp_outer += base - spin[j];
      }
      sum_inner += wgtz*temp_inner;
      sum_outer += wgtz*temp_outer;

      ThreeVector sum = 15*sum_inner + sum_outer;

      OC_REAL8m ei = base.x*sum.x + base.y*sum.y + base.z*sum.z;
      sum.x *= hcoef*Msii; sum.y *= hcoef*Msii; sum.z *= hcoef*Msii;
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
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

void
Oxs_UniformExchange::CalcEnergy26Ngbr
(const Oxs_MeshValue<ThreeVector>& spin,
 const Oxs_MeshValue<OC_REAL8m>& Ms_inverse,
 const Oxs_RectangularMesh* mesh,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int threadnumber) const
{
#ifndef NDEBUG
  if(node_stop>mesh->Size() || node_start>node_stop) {
    throw Oxs_ExtError(this,"Programming error:"
                       " Invalid node_start/node_stop values");
  }
#endif

  OC_INDEX xdim = mesh->DimX();
  OC_INDEX ydim = mesh->DimY();
  OC_INDEX zdim = mesh->DimZ();
  OC_INDEX xydim = xdim*ydim;

  const OC_REAL8m hcoef = -2/MU0;

  // Construct term weightings.  See mjd's NOTES I, 23-Oct-1997,
  // pp 168-171.
  OC_REAL8m delxsq = mesh->EdgeLengthX()*mesh->EdgeLengthX();
  OC_REAL8m delysq = mesh->EdgeLengthY()*mesh->EdgeLengthY();
  OC_REAL8m delzsq = mesh->EdgeLengthZ()*mesh->EdgeLengthZ();
  OC_REAL8m delisum = (1.0/delxsq)+(1.0/delysq)+(1.0/delzsq);
  OC_REAL8m Amult = A/18.0;

  // Nearest neighbor weights.  These are all zero for cubic cells.
  // Might want to implement special handling in the algorithm
  // below for this common case.
  OC_REAL8m wgt_x=0.,wgt_y=0.,wgt_z=0.;
  if(delxsq!=delysq || delxsq!=delzsq || delysq!=delzsq) {
    wgt_x = 4*Amult*(delisum-3.0/delxsq);
    wgt_y = 4*Amult*(delisum-3.0/delysq);
    wgt_z = 4*Amult*(delisum-3.0/delzsq);
  }

  // 2nd nearest neighbor weights.
  OC_REAL8m wgt_xy = 2*Amult*(-delisum + 3.0/(2*delzsq));
  OC_REAL8m wgt_xz = 2*Amult*(-delisum + 3.0/(2*delysq));
  OC_REAL8m wgt_yz = 2*Amult*(-delisum + 3.0/(2*delxsq));

  // Corner neighbor weight.
  OC_REAL8m wgt_corner = -0.5*Amult*delisum;

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
#if defined(__GNUC__) && __GNUC__ == 4 \
  && defined(__GNUC_MINOR__) && __GNUC_MINOR__ <= 1
    Oc_Nop(xstop); // i686-apple-darwin9-gcc-4.0.1 (OS X Leopard)
    /// (others?) has an optimization bug that segfaults in the
    /// following code block.  Problem is probably with -fstrict-alias
    /// (see demag-threaded.cc, line 1664), but this was not checked
    /// explicitly.
#endif
    while(x<xstop) {
      OC_REAL8m Msii = Ms_inverse[i];
      if(Msii == 0.0) {
        if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
        if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
        if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
        ++i;   ++x;
        continue;
      }

      ThreeVector base = spin[i];

      // Note: Intel C++ compiler version 11.0 on IA-64 fumbles at
      //   high optimization levels if "i-1" and "i+1" below are
      //   replaced with a temp variable, e.g.,
      //      OC_INDEX j = i-1;
      //      if(Ms_inverse[j]!=0.0) { ... }
      //   The compiled code segfaults core at runtime.

      // Nearest x neighbors.  At most 2.
      ThreeVector sum_x(0.,0.,0.);
      if(x>0) {
        if(Ms_inverse[i-1]!=0.0) {
          sum_x = (spin[i-1] - base);
          OC_REAL8m dot = sum_x.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(x<xdim-1) {
        if(Ms_inverse[i+1]!=0.0) sum_x += (spin[i+1] - base);
      }
      sum_x *= wgt_x;

      // Nearest y neighbors.  At most 2.
      ThreeVector sum_y(0.,0.,0.);
      if(y>0) {
        OC_INDEX j = i-xdim;
        if(Ms_inverse[j]!=0.0) {
          sum_y = (spin[j] - base);
          OC_REAL8m dot = sum_y.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(y<ydim-1) {
        OC_INDEX j = i+xdim;
        if(Ms_inverse[j]!=0.0) sum_y += (spin[j] - base);
      }
      sum_y *= wgt_y;

      // Nearest z neighbors.  At most 2.
      ThreeVector sum_z(0.,0.,0.);
      if(z>0) {
        OC_INDEX j = i-xydim;
        if(Ms_inverse[j]!=0.0) {
          sum_z = (spin[j] - base);
          OC_REAL8m dot = sum_z.MagSq();
          if(dot>thread_maxdot) thread_maxdot = dot;
        }
      }
      if(z<zdim-1) {
        OC_INDEX j = i+xydim;
        if(Ms_inverse[j]!=0.0) sum_z += (spin[j] - base);
      }
      sum_z *= wgt_z;

      // xy-neighbors.  At most 4.
      OC_INDEX j;
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
      OC_INDEX iz=i;
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


      /// asdf SLOW CODE? ///
      OC_REAL8m ei = base.x*sum.x + base.y*sum.y + base.z*sum.z;
      sum.x *= hcoef*Msii; sum.y *= hcoef*Msii; sum.z *= hcoef*Msii;
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
  /// All cells have same volume in an Oxs_CommonRectangularMesh.

  maxdot[threadnumber] = thread_maxdot;
}

void Oxs_UniformExchange::ComputeEnergyChunkInitialize
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

  // (Re)-initialize coefficients if mesh has changed.
  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(state.mesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(this,msg);
  }
  if(mesh_id != mesh->Id()) {
    mesh_id = mesh->Id();
    if(kernel == NGBR_12_ZD1B) {
      const OC_INDEX xdim = mesh->DimX();
      const OC_INDEX ydim = mesh->DimY();
      const OC_INDEX zdim = mesh->DimZ();
      if((1<xdim && xdim<5) || (1<ydim && ydim<5)
         || (1<zdim && zdim<5)) {
        char buf[1024];
        Oc_Snprintf(buf,sizeof(buf),
                    "Each dimension must be ==1 or >=5 for 12ngbrZD1b kernel."
                    " (Actual dimensions: xdim=%u, ydim=%u, zdim=%u.)",
                    xdim,ydim,zdim);
        throw Oxs_ExtError(this,buf);
      }
      InitCoef_12NgbrZD1(xdim,xinteg,xcoef);
      InitCoef_12NgbrZD1(ydim,yinteg,ycoef);
      InitCoef_12NgbrZD1(zdim,zinteg,zcoef);
    }
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
      working_A = A;
    } else if(excoeftype == LEX_TYPE) {
      OC_REAL8m lexMs = lex*state.max_absMs;
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
  }
  ocedt.energy_density_error_estimate = energy_density_error_estimate;
}

void Oxs_UniformExchange::ComputeEnergyChunkFinalize
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
    // output requests on post ::Step states is problematic if some of
    // the requested output is not cached as part of the step
    // proceedings.  A warning is put into place below for debugging
    // purposes, but in general an error is raised only if results
    // from the recomputation are different than originally.
#ifndef NDEBUG
    static Oxs_WarningMessage maxangleset(3);
    maxangleset.Send(revision_info,OC_STRINGIFY(__LINE__),
                     "Programming inefficiency?"
                     " Oxs_UniformExchange max spin angle set twice.");
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
                  " Oxs_UniformExchange max spin angle set to"
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

void Oxs_UniformExchange::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,
 OC_INDEX node_stop,
 int threadnumber
 ) const
{
  const OC_INDEX size = state.mesh->Size();
  if(size<1) {
    return;
  }

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);

  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(state.mesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(this,msg);
  }

  // Check periodicity.  Note that the following kernels have not been
  // upgraded to supported periodic meshes:
  //   NGBR_12_FREE, NGBR_12_ZD1, NGBR_12_ZD1B, NGBR_26
  // This is checked for and reported in the individual arms of the
  // kernel if-test below.
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

  // Note: Might want to consider subclassing exchange energies,
  //  to replace this if-block with virtual function pointers.
  if(kernel == NGBR_6_FREE) {
    CalcEnergy6NgbrFree(spin,Ms_inverse,mesh,ocedt,ocedtaux,
                        node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_6_MIRROR) {
    if(A_TYPE != excoeftype) {
      CalcEnergy6NgbrMirror_lex(spin,Ms,mesh,ocedt,ocedtaux,
                                node_start,node_stop,threadnumber);
    } else {
      CalcEnergy6NgbrMirror(spin,Ms_inverse,mesh,ocedt,ocedtaux,
                            node_start,node_stop,threadnumber);
    }
  } else if(kernel == NGBR_6_MIRROR_STD) {
    CalcEnergy6NgbrMirrorStd(spin,Ms_inverse,mesh,ocedt,ocedtaux,
                             node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_6_BIGANG_MIRROR) {
    CalcEnergy6NgbrBigAngMirror(spin,Ms_inverse,mesh,ocedt,ocedtaux,
                                node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_6_ZD2) {
    CalcEnergy6NgbrZD2(spin,Ms_inverse,mesh,ocedt,ocedtaux,
                       node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_6_ALT) {
    CalcEnergy6NgbrAlt(spin,Ms_inverse,mesh,ocedt,ocedtaux,
                       node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_12_FREE) {
    if(rmesh==NULL) {
      String msg=String("Requested exchange kernel \"12ngbrfree\""
                        " requires a non-periodic rectangular mesh.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    CalcEnergy12NgbrFree(spin,Ms_inverse,rmesh,ocedt,ocedtaux,
                         node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_12_ZD1) {
    if(rmesh==NULL) {
      String msg=String("Requested exchange kernel \"12ngbr\"/\"12ngbrzd1\""
                        " requires a non-periodic rectangular mesh.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    CalcEnergy12NgbrZD1(spin,Ms_inverse,rmesh,ocedt,ocedtaux,
                        node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_12_ZD1B) {
    if(rmesh==NULL) {
      String msg=String("Requested exchange kernel"
                        " \"12ngbrb\"/\"12ngbrzd1b\""
                        " requires a non-periodic rectangular mesh.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    CalcEnergy12NgbrZD1B(spin,Ms_inverse,rmesh,ocedt,ocedtaux,
                         node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_12_MIRROR) {
    CalcEnergy12NgbrMirror(spin,Ms_inverse,mesh,ocedt,ocedtaux,
                           node_start,node_stop,threadnumber);
  } else if(kernel == NGBR_26) {
    if(rmesh==NULL) {
      String msg=String("Requested exchange kernel \"26ngbr\""
                        " requires a non-periodic rectangular mesh.");
      throw Oxs_ExtError(this,msg.c_str());
    }
    CalcEnergy26Ngbr(spin,Ms_inverse,rmesh,ocedt,ocedtaux,
                     node_start,node_stop,threadnumber);
  } else {
    throw Oxs_ExtError(this,"Invalid kernel type detected."
                         "  (Programming error)");
  }

}

void Oxs_UniformExchange::UpdateDerivedOutputs(const Oxs_SimState& state)
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
    maxspinangle_output.cache.value = -2.222;
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
Oxs_UniformExchange::IncrementPreconditioner(PreconditionerData& pcd)
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

  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  const Oxs_CommonRectangularMesh* mesh
    = dynamic_cast<const Oxs_CommonRectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    String msg=String("Object ")
      + String(state.mesh->InstanceName())
      + String(" is not a rectangular mesh.");
    throw Oxs_ExtError(this,msg);
  }

  // Check periodicity.  Note that the following kernels have not been
  // upgraded to supported periodic meshes:
  //   NGBR_12_FREE, NGBR_12_ZD1, NGBR_12_ZD1B, NGBR_26
  // This is checked for and reported in the individual arms of the
  // kernel if-test below.
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

  OC_REAL8m wgtx, wgty, wgtz;
  if(A_TYPE == excoeftype) {
    wgtx = 2*A/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
    wgty = 2*A/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
    wgtz = 2*A/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());
  } else if(LEX_TYPE == excoeftype) {
    wgtx = lex*lex*MU0/(mesh->EdgeLengthX()*mesh->EdgeLengthX());
    wgty = lex*lex*MU0/(mesh->EdgeLengthY()*mesh->EdgeLengthY());
    wgtz = lex*lex*MU0/(mesh->EdgeLengthZ()*mesh->EdgeLengthZ());
  } else {
    throw Oxs_ExtError(this,"Invalid ExchangeCoefType.");
  }

  OC_INDEX i = -1;
  int success_code = 0;
  if(kernel == NGBR_6_MIRROR) {
    for(OC_INDEX z=0;z<zdim;++z) {
      for(OC_INDEX y=0;y<ydim;++y) {
        for(OC_INDEX x=0;x<xdim;++x) {
          ++i;
          if(0.0 == Ms[i]) continue; // Ignore cells with Ms == 0.
          OC_REAL8m sum = 0.0;
          if(A_TYPE == excoeftype) {
            // x ngbrs
            if(x>0) {
              if(0.0 != Ms[i-1]) sum += wgtx; 
            } else if(xperiodic) {
              if(0.0 != Ms[i-1+xdim]) sum += wgtx; 
            }
            if(x<xdim-1) {
              if(0.0 != Ms[i+1]) sum += wgtx; 
            } else if(xperiodic) {
              if(0.0 != Ms[i+1-xdim]) sum += wgtx; 
            }
            // y ngbrs
            if(y>0) {
              if(0.0 != Ms[i-xdim]) sum += wgty;
            } else if(yperiodic) {
              if(0.0 != Ms[i-xdim+xydim]) sum += wgty;
            }
            if(y<ydim-1) {
              if(0.0 != Ms[i+xdim]) sum += wgty;
            } else if(yperiodic) {
              if(0.0 != Ms[i+xdim-xydim]) sum += wgty;
            }
            // z ngbrs
            if(z>0) {
              if(0.0 != Ms[i-xydim]) sum += wgtz;
            } else if(zperiodic) {
              if(0.0 != Ms[i-xydim+xyzdim]) sum += wgtz;
            }
            if(z<zdim-1) {
              if(0.0 != Ms[i+xydim]) sum += wgtz;
            } else if(zperiodic) {
              if(0.0 != Ms[i+xydim-xyzdim]) sum += wgtz;
            }
            sum /= Ms[i];
          } else { // LEX_TYPE == excoeftype
            // x ngbrs
            OC_REAL8m xsum=0.0;
            if(x>0)            xsum += Ms[i-1]; 
            else if(xperiodic) xsum += Ms[i-1+xdim]; 
            if(x<xdim-1)       xsum += Ms[i+1]; 
            else if(xperiodic) xsum += Ms[i+1-xdim]; 
            xsum *= wgtx;
            // y ngbrs
            OC_REAL8m ysum=0.0;
            if(y>0)            ysum += Ms[i-xdim];
            else if(yperiodic) ysum += Ms[i-xdim+xydim];
            if(y<ydim-1)       ysum += Ms[i+xdim];
            else if(yperiodic) ysum += Ms[i+xdim-xydim];
            ysum *= wgty;
            // z ngbrs
            OC_REAL8m zsum=0.0;
            if(z>0)            zsum += Ms[i-xydim];
            else if(zperiodic) zsum += Ms[i-xydim+xyzdim];
            if(z<zdim-1)       zsum += Ms[i+xydim];
            else if(zperiodic) zsum += Ms[i+xydim-xyzdim];
            zsum *= wgtz;
            sum = xsum + ysum + zsum;
          }
          val[i].x += sum;
          val[i].y += sum;
          val[i].z += sum;
        }
      }
    }
    success_code = 1;
  } else {
    // No preconditioning support for this kernel
    success_code = 0; // Safety
  }

  return success_code;
}
