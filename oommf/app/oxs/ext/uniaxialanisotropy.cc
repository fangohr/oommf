/* FILE: uniaxialanisotropy.cc            -*-Mode: c++-*-
 *
 * Uniaxial Anisotropy, derived from Oxs_Energy class.
 *
 */

#include <limits>
#include <string>

#include "oc.h"
#include "nb.h"
#include "threevector.h"
#include "director.h"
#include "simstate.h"
#include "ext.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "uniformscalarfield.h"
#include "uniformvectorfield.h"
#include "rectangularmesh.h"  // For QUAD-style integration
#include "uniaxialanisotropy.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_UniaxialAnisotropy);

/* End includes */

// Constructor
Oxs_UniaxialAnisotropy::Oxs_UniaxialAnisotropy(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ChunkEnergy(name,newdtr,argstr),
    aniscoeftype(ANIS_UNKNOWN), mesh_id(0),
    max_K1(-1.0),
    K1_is_uniform(0), Ha_is_uniform(0), axis_is_uniform(0),
    uniform_K1_value(0.0), uniform_Ha_value(0.0),
    integration_method(UNKNOWN_INTEG),
    has_multscript(0), number_of_stages(0),
    mult_state_id(0), mult(1.0), dmult(0.0)
{
  // Process arguments
  OC_BOOL has_K1 = HasInitValue("K1");
  OC_BOOL has_Ha = HasInitValue("Ha");
  if(has_K1 && has_Ha) {
    throw Oxs_ExtError(this,"Invalid anisotropy coefficient request:"
			 " both K1 and Ha specified; only one should"
			 " be given.");
  } else if(has_K1) {
    OXS_GET_INIT_EXT_OBJECT("K1",Oxs_ScalarField,K1_init);
    Oxs_UniformScalarField* tmpK1ptr
      = dynamic_cast<Oxs_UniformScalarField*>(K1_init.GetPtr());
    if(tmpK1ptr) {
      // Initialization is via a uniform field; set up uniform
      // K1 variables.
      K1_is_uniform = 1;
      uniform_K1_value = tmpK1ptr->SoleValue();
    }
    aniscoeftype = K1_TYPE;
  } else {
    OXS_GET_INIT_EXT_OBJECT("Ha",Oxs_ScalarField,Ha_init);
    Oxs_UniformScalarField* tmpHaptr
      = dynamic_cast<Oxs_UniformScalarField*>(Ha_init.GetPtr());
    if(tmpHaptr) {
      // Initialization is via a uniform field; set up uniform
      // Ha variables.
      Ha_is_uniform = 1;
      uniform_Ha_value = tmpHaptr->SoleValue();
    }
    aniscoeftype = Ha_TYPE;
  }

  OXS_GET_INIT_EXT_OBJECT("axis",Oxs_VectorField,axis_init);
  Oxs_UniformVectorField* tmpaxisptr
    = dynamic_cast<Oxs_UniformVectorField*>(axis_init.GetPtr());
  if(tmpaxisptr) {
    // Initialization is via a uniform field.  For convenience,
    // modify the size of the field components to norm 1, as
    // required for the axis specification.  This allows the
    // user to specify the axis direction as, for example, {1,1,1},
    // as opposed to {0.57735027,0.57735027,0.57735027}, or
    //
    //      Specify Oxs_UniformVectorField {
    //        norm 1
    //        vector { 1 1 1 }
    //    }
    // Also setup uniform axis variables
    tmpaxisptr->SetMag(1.0);
    axis_is_uniform = 1;
    uniform_axis_value = tmpaxisptr->SoleValue();
  }


  String integration_request = GetStringInitValue("integration","rect");
  if(integration_request.compare("rect")==0) {
    integration_method = RECT_INTEG;
  } else if(integration_request.compare("quad")==0) {
    integration_method = QUAD_INTEG;
  } else {
    String msg=String("Invalid integration request: ")
      + integration_request
      + String("\n Should be either \"rect\" or \"quad\".");
    throw Oxs_ExtError(this,msg.c_str());
  }

  has_multscript = HasInitValue("multscript");
  String cmdoptreq;
  String runscript;
  if(has_multscript) {
    if(integration_method != RECT_INTEG) {
      String msg=String("Invalid option selection: Integration type \"")
        + integration_request
        + String("\" not supported for use with \"multscript\"");
      throw Oxs_ExtError(this,msg.c_str());
    }
    cmdoptreq = GetStringInitValue("multscript_args",
                                   "stage stage_time total_time");
    runscript = GetStringInitValue("multscript");
  }
  number_of_stages = GetUIntInitValue("stage_count",0);
  /// Default number_of_stages is 0, i.e., no preference.

  VerifyAllInitArgsUsed();

  // Run SetBaseCommand *after* VerifyAllInitArgsUsed(); this produces
  // a more intelligible error message in case a command line argument
  // error is really due to a misspelled parameter label.
  if(has_multscript) {
    command_options.push_back(Nb_TclCommandLineOption("stage",1));
    command_options.push_back(Nb_TclCommandLineOption("stage_time",1));
    command_options.push_back(Nb_TclCommandLineOption("total_time",1));
    cmd.SetBaseCommand(InstanceName(),
		       director->GetMifInterp(),
		       runscript,
		       Nb_ParseTclCommandLineRequest(InstanceName(),
                                                     command_options,
                                                     cmdoptreq));
  }
}

OC_BOOL Oxs_UniaxialAnisotropy::Init()
{
  mesh_id = 0;
  K1.Release();
  Ha.Release();
  axis.Release();

  max_K1 = -1.0;
  mult_state_id = 0;
  mult = 1.0;
  dmult = 0.0;

  return Oxs_ChunkEnergy::Init();
}

void
Oxs_UniaxialAnisotropy::StageRequestCount
(unsigned int& min,
 unsigned int& max) const
{
  if(number_of_stages==0) {
    min=0; max=UINT_MAX;
  } else {
    min = max = number_of_stages;
  }
}

void
Oxs_UniaxialAnisotropy::GetMultiplier
(const Oxs_SimState& state, // import
 OC_REAL8m& mult_,           // export
 OC_REAL8m& dmult_           // export
 ) const
{
  if(!has_multscript) {
    mult_ = 1.0;
    dmult_ = 0.0;
    return;
  }

  // Run script
  cmd.SaveInterpResult();
  int index;
  if((index = command_options[0].position)>=0) { // stage
    cmd.SetCommandArg(index,state.stage_number);
  }
  if((index = command_options[1].position)>=0) { // stage_time
    cmd.SetCommandArg(index,state.stage_elapsed_time);
  }
  if((index = command_options[2].position)>=0) { // total_time
    cmd.SetCommandArg(index,
		      state.stage_start_time+state.stage_elapsed_time);
  }
  cmd.Eval();
  if(cmd.GetResultListSize()!=2) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
		"Script return list wrong size: %d values (should be 2)",
		cmd.GetResultListSize());
    cmd.DiscardInterpResult();
    throw Oxs_ExtError(this,buf);
  }

  // Convert script string values to OC_REAL8m's.
  cmd.GetResultListItem(0,mult_);
  cmd.GetResultListItem(1,dmult_);

  cmd.RestoreInterpResult();
}


void Oxs_UniaxialAnisotropy::RectIntegEnergy
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop
 ) const
{
  const Oxs_Mesh* mesh = state.mesh;
  const Oxs_MeshValue<OC_REAL8m>& Ms         = *(state.Ms);
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse = *(state.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;

  Nb_Xpfloat energy_sum = 0.0;
  Nb_Xpfloat pE_pt_sum = 0.0;

  OC_REAL8m k = uniform_K1_value;
  OC_REAL8m field_mult = uniform_Ha_value;
  ThreeVector unifaxis = uniform_axis_value;

  OC_REAL8m scaling  = mult;  // Copy from class mutables.  These are
  OC_REAL8m dscaling = dmult; // set from main thread once per state.

  for(OC_INDEX i=node_start;i<node_stop;++i) {
    if(aniscoeftype == K1_TYPE) {
      if(!K1_is_uniform) k = K1[i];
      field_mult = (2.0/MU0)*k*Ms_inverse[i];
    } else {
      if(!Ha_is_uniform) field_mult = Ha[i];
      k = 0.5*MU0*field_mult*Ms[i];
    }
    if(k==0.0 || field_mult == 0.0) { // Includes Ms==0.0 case
      if(ocedt.energy) (*ocedt.energy)[i] = 0.0;
      if(ocedt.H)      (*ocedt.H)[i].Set(0.,0.,0.);
      if(ocedt.mxH)    (*ocedt.mxH)[i].Set(0.,0.,0.);
      continue;
    }

    const ThreeVector& axisi = (axis_is_uniform ? unifaxis : axis[i]);
    if(k<=0) {
      // Easy plane (hard axis)
      // NOTE: This division is based on the value of k as specified by
      //       K1[i], irrespective of the scaling multiplier.  This is
      //       because we don't want the energy formula for a cell to
      //       hop back and forth between the two (easy vs. hard)
      //       representations over the lifetime of the simulation.

      // Split up computation of dot product to improve data flow. If
      // we don't do this manually then the compiler may do this in a
      // way that is not consistent between loops, leading to small
      // rounding differences.
      OC_REAL8m dot = spin[i].x*axisi.x;
      OC_REAL8m ty = FMA_Block(spin[i].z,axisi.x,-spin[i].x*axisi.z);
      dot = FMA_Block(spin[i].z,axisi.z,dot);
      OC_REAL8m tx = FMA_Block(spin[i].y,axisi.z,-spin[i].z*axisi.y);
      dot = FMA_Block(spin[i].y,axisi.y,dot);
      OC_REAL8m tz = FMA_Block(spin[i].x,axisi.y,-spin[i].y*axisi.x);
      const OC_REAL8m Hscale = scaling*field_mult*dot;

      if(ocedt.mxH)       {
        (*ocedt.mxH)[i] = ThreeVector(Hscale*tx,Hscale*ty,Hscale*tz);
      }
      if(ocedt.mxH_accum) {
        (*ocedt.mxH_accum)[i].Accum(Hscale,ThreeVector(tx,ty,tz));
      }

      const ThreeVector H = Hscale*axisi;
      const OC_REAL8m mkdotsq = -k*dot*dot;
      const OC_REAL8m ei = scaling*mkdotsq;
      const OC_REAL8m vol = mesh->Volume(i);
      Nb_XpfloatDualAccum(energy_sum,ei*vol,
                          pE_pt_sum,dscaling*mkdotsq*vol);
      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)       (*ocedt.H)[i] = H;
      if(ocedt.H_accum) (*ocedt.H_accum)[i] += H;

    } else {
      // Easy axis case.  For improved accuracy, we want to report
      // energy as -k*(dot*dot-1), where dot = axis * spin.  But
      // dot*dot-1 suffers from bad loss of precision if spin is
      // nearly parallel to axis.  The are a couple of ways around
      // this.  Recall that both spin and axis are unit vectors.
      // Then from the cross product:
      //            (axis x spin)^2 = 1 - dot*dot
      // The cross product requires 6 mults and 3 adds, and
      // the norm squared takes 3 mult and 2 adds
      //            => 9 mults + 5 adds.
      // Another option is to use
      //            (axis - spin)^2 = 2*(1-dot)
      //     so  1 - dot*dot = t*(2-t)
      //                where t = 0.5*(axis-spin)^2.
      // The op count here is
      //            => 5 mults + 6 adds.
      // Another advantage to the second approach is you get 'dot', as
      // opposed to dot*dot, which saves a sqrt if dot is needed.  The
      // downside is that if axis and spin are anti-parallel, then you
      // want to use (axis+spin)^2 rather than (axis-spin)^2.  I did
      // some single-spin test runs and the performance of the two
      // methods was about the same.  Below we use the cross-product
      // formulation. -mjd, 28-Jan-2001

      // Split up computation of dot product to improve data flow. If
      // we don't do this manually then the compiler may do this in a
      // way that is not consistent between loops, leading to small
      // rounding differences.
      OC_REAL8m dot = spin[i].x*axisi.x;
      const OC_REAL8m ty = FMA_Block(spin[i].z,axisi.x,-spin[i].x*axisi.z);
      dot = FMA_Block(spin[i].z,axisi.z,dot);
      const OC_REAL8m tx = FMA_Block(spin[i].y,axisi.z,-spin[i].z*axisi.y);
      dot = FMA_Block(spin[i].y,axisi.y,dot);
      const OC_REAL8m tz = FMA_Block(spin[i].x,axisi.y,-spin[i].y*axisi.x);

      const OC_REAL8m ktsq = k*(FMA_Block(tx,tx,FMA_Block(ty,ty,tz*tz)));
      const OC_REAL8m ei = scaling*ktsq;

      const OC_REAL8m Hscale = scaling*field_mult*dot;

      const OC_REAL8m vol = mesh->Volume(i);
      Nb_XpfloatDualAccum(energy_sum,ei*vol,
                          pE_pt_sum,dscaling*ktsq*vol);
      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)            (*ocedt.H)[i] = Hscale*axisi;
      if(ocedt.H_accum)      (*ocedt.H_accum)[i].Accum(Hscale,axisi);
      if(ocedt.mxH) {
        (*ocedt.mxH)[i] = ThreeVector(Hscale*tx,Hscale*ty,Hscale*tz);
      }
      if(ocedt.mxH_accum) {
        (*ocedt.mxH_accum)[i].Accum(Hscale,ThreeVector(tx,ty,tz));
      }
    }
  }
  ocedtaux.energy_total_accum += energy_sum;
  ocedtaux.pE_pt_accum += pE_pt_sum;
}

void Oxs_UniaxialAnisotropy::ComputeEnergyChunkInitialize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& /* thread_ocedtaux */,
 int /* number_of_threads */) const
{
  if(mesh_id !=  state.mesh->Id()) {
    // This is either the first pass through, or else mesh has changed.
    // Initialize/update data fields.  NB: At a lower level, this may
    // potentially involve calls back into the Tcl interpreter.  Per Tcl
    // spec, only the thread originating the interpreter is allowed to
    // make calls into it.  The ComputeEnergyChunkInitialize member is
    // guaranteed to be called in the main thread.
    // Note: max_K1 might be initialized here or in
    // ::IncrementPreconditioner().
    if(aniscoeftype == K1_TYPE) {
      if(K1_is_uniform) {
        max_K1 = fabs(uniform_K1_value);
      } else {
        K1_init->FillMeshValue(state.mesh,K1);
        max_K1=0.0;
        const OC_INDEX size = state.mesh->Size();
        for(OC_INDEX i=0;i<size;i++) {
          OC_REAL8m test = fabs(K1[i]);
          if(test>max_K1) max_K1 = test;
        }
      }
    } else if(aniscoeftype == Ha_TYPE) {
      if(!Ha_is_uniform) Ha_init->FillMeshValue(state.mesh,Ha);
      max_K1=0.0;
      const OC_INDEX size = state.mesh->Size();
      const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
      OC_REAL8m tmpHa = uniform_Ha_value;
      for(OC_INDEX i=0;i<size;i++) {
        // Note: This code assumes that Ms doesn't change
        // for the lifetime of state.mesh.
        if(!Ha_is_uniform) tmpHa = Ha[i];
        OC_REAL8m test = fabs(tmpHa*Ms[i]);
        if(test>max_K1) max_K1 = test;
      }
      max_K1 *= 0.5*MU0;
    }
    if(!axis_is_uniform) {
      axis_init->FillMeshValue(state.mesh,axis);
      const OC_INDEX size = state.mesh->Size();
      for(OC_INDEX i=0;i<size;i++) {
        // Check that axis is a unit vector:
        const OC_REAL8m eps = 1e-14;
        if(axis[i].MagSq()<eps*eps) {
          throw Oxs_ExtError(this,"Invalid initialization detected:"
                             " Zero length anisotropy axis");
        } else {
          axis[i].MakeUnit();
        }
      }
    }
    mesh_id = state.mesh->Id();
  }
  if(has_multscript && mult_state_id !=  state.Id()) {
    // The processing to set mult and dmult involves calls into the Tcl
    // interpreter.  Per Tcl spec, only the thread originating the
    // interpreter is allowed to make calls into it, so only
    // threadnumber == 0 can do this processing.
    GetMultiplier(state,mult,dmult);
    mult_state_id = state.Id();
  }

  if(integration_method == QUAD_INTEG) {
    // Base evaluations are written into scratch space
    if(!ocedt.energy) {
      ocedt.scratch_energy->AdjustSize(state.mesh);
    }
    if(!ocedt.H) {
      ocedt.scratch_H->AdjustSize(state.mesh);
    }
  }

  ocedt.energy_density_error_estimate = 8*OC_REAL8m_EPSILON*max_K1;
}

void Oxs_UniaxialAnisotropy::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int /* threadnumber */
 ) const
{
  assert(node_start<=node_stop && node_stop<=state.mesh->Size());

  if(integration_method != QUAD_INTEG) {
    RectIntegEnergy(state,ocedt,ocedtaux,node_start,node_stop);
    return;
  }

  // Otherwise, use QUAD_INTEG.  This implementation is not especially
  // efficient, and could presumably be sped up by a good bit if
  // needed.

  // The QUAD_INTEG code requires a rectangular mesh
  const Oxs_RectangularMesh* mesh
    = dynamic_cast<const Oxs_RectangularMesh*>(state.mesh);
  if(mesh==NULL) {
    throw Oxs_ExtError(this,
          "Import mesh to Oxs_UniaxialAnisotropy::GetEnergy()"
          " is not an Oxs_RectangularMesh object.  Quad integration"
          " method requires a rectangular mesh.  (Periodic"
          " rectangular meshes not presently supported.)");
  }

  // Do base evaluations, writing results into scratch space
  Oxs_ComputeEnergyDataThreaded work_ocedt(state);
  Oxs_ComputeEnergyDataThreadedAux work_ocedtaux;
  if(ocedt.energy) {
    work_ocedt.energy = work_ocedt.scratch_energy = ocedt.energy;
  } else {
    // Note: ocedt.scratch_energy resized in
    ocedt.scratch_energy->AdjustSize(state.mesh); // Thread-safe
    work_ocedt.energy = work_ocedt.scratch_energy = ocedt.scratch_energy;
  }
  if(ocedt.H) {
    work_ocedt.H      = work_ocedt.scratch_H      = ocedt.H;
  } else {
    ocedt.scratch_H->AdjustSize(state.mesh); // Thread-safe
    work_ocedt.H      = work_ocedt.scratch_H      = ocedt.scratch_H;
  }

  RectIntegEnergy(state,work_ocedt,work_ocedtaux,node_start,node_stop);

  // Edge correction if higher-order integration method requested.
  // See mjd's NOTES II, pp 178-181, Aug-2002.
  // NOTE: For short dimension lengths, all cells are modified.
  Oxs_MeshValue<OC_REAL8m>& energy = *work_ocedt.energy;
  Oxs_MeshValue<ThreeVector>& field = *work_ocedt.H;

  const OC_INDEX xdim = mesh->DimX();
  const OC_INDEX ydim = mesh->DimY();
  const OC_INDEX zdim = mesh->DimZ();
  const OC_INDEX xydim = xdim*ydim;
  OC_INDEX x,y,z;

  OC_INDEX xstart,ystart,zstart;
  mesh->GetCoords(node_start,xstart,ystart,zstart);
  OC_INDEX xstop,ystop,zstop;
  mesh->GetCoords(node_stop-1,xstop,ystop,zstop);


  // x-axis.  Note special case handling for short lengths.  Also note
  // very lazy handling of node start/stop conditions.  With luck, the
  // compiler can figure it out.  Otherwise, if we ever care to have
  // this code run fast, the condition handling should be re-written.
  if(xdim>=6) {
    for(z=zstart;z<=zstop;++z) {
      OC_INDEX ya = (z==zstart ? ystart : 0);
      OC_INDEX yb = (z==zstop  ? ystop : ydim-1);
      for(y=ya;y<=yb;++y) {
        OC_INDEX i = mesh->Index(0,y,z); // Get base linear address
        if(node_start<=i && i<node_stop) {
          energy[i]   *= 26./24.; // Left face
          field[i]    *= 26./24.;
        }
        if(node_start<=i+1 && i+1<node_stop) {
          energy[i+1] *= 21./24.;
          field[i+1]  *= 21./24.;
        }
        if(node_start<=i+2 && i+2<node_stop) {
          energy[i+2] *= 25./24.;
          field[i+2]  *= 25./24.;
        }

        i += xdim-3;
        if(node_start<=i && i<node_stop) {
          energy[i]   *= 25./24.;
          field[i]    *= 25./24.;
        }
        if(node_start<=i+1 && i+1<node_stop) {
          energy[i+1] *= 21./24.;
          field[i+1]  *= 21./24.;
        }
        if(node_start<=i+2 && i+2<node_stop) {
          energy[i+2] *= 26./24.; // Right face
          field[i+2]  *= 26./24.;
        }
      }
    }
  } else if(xdim==5) {
    for(z=zstart;z<=zstop;++z) {
      OC_INDEX ya = (z==zstart ? ystart : 0);
      OC_INDEX yb = (z==zstop  ? ystop : ydim-1);
      OC_INDEX i = mesh->Index(0,ya,z); // Get base linear address
      for(y=ya;y<=yb;++y) {
        if(node_start<=i && i<node_stop) {
          energy[i]   *= 26./24.;
          field[i]    *= 26./24.;
        }
        if(node_start<=i+1 && i+1<node_stop) {
          energy[i+1] *= 21./24.;
          field[i+1]  *= 21./24.;
        }
        if(node_start<=i+2 && i+2<node_stop) {
          energy[i+2] *= 26./24.;
          field[i+2]  *= 26./24.;
        }
        if(node_start<=i+3 && i+3<node_stop) {
          energy[i+3] *= 21./24.;
          field[i+3]  *= 21./24.;
        }
        if(node_start<=i+4 && i+4<node_stop) {
          energy[i+4] *= 26./24.;
          field[i+4]  *= 26./24.;
        }
        i += 5;
      }
    }
  } else if(xdim==4) {
    for(z=zstart;z<=zstop;++z) {
      OC_INDEX ya = (z==zstart ? ystart : 0);
      OC_INDEX yb = (z==zstop  ? ystop : ydim-1);
      OC_INDEX i = mesh->Index(0,ya,z); // Get base linear address
      for(y=ya;y<=yb;++y) {
        if(node_start<=i && i<node_stop) {
          energy[i]   *= 26./24.;
          field[i]    *= 26./24.;
        }
        if(node_start<=i+1 && i+1<node_stop) {
          energy[i+1] *= 22./24.;
          field[i+1]  *= 22./24.;
        }
        if(node_start<=i+2 && i+2<node_stop) {
          energy[i+2] *= 22./24.;
          field[i+2]  *= 22./24.;
        }
        if(node_start<=i+3 && i+3<node_stop) {
          energy[i+3] *= 26./24.;
          field[i+3]  *= 26./24.;
        }
        i += 4;
      }
    }
  } else if(xdim==3) {
    for(z=zstart;z<=zstop;++z) {
      OC_INDEX ya = (z==zstart ? ystart : 0);
      OC_INDEX yb = (z==zstop  ? ystop : ydim-1);
      OC_INDEX i = mesh->Index(0,ya,z); // Get base linear address
      for(y=ya;y<=yb;++y) {
        if(node_start<=i && i<node_stop) {
          energy[i]   *= 27./24.;
          field[i]    *= 27./24.;
        }
        if(node_start<=i+1 && i+1<node_stop) {
          energy[i+1] *= 18./24.;
          field[i+1]  *= 18./24.;
        }
        if(node_start<=i+2 && i+2<node_stop) {
          energy[i+2] *= 27./24.;
          field[i+2]  *= 27./24.;
        }
        i += 3;
      }
    }
  }
  // Quadratic fit requires 3 points, so no higher order method
  // available if xdim<3.

  // y-axis.  Note special case handling for short lengths.
  if(ydim>=6) {
    for(z=zstart;z<=zstop;++z) {
      // Front face
      OC_INDEX i = mesh->Index(0,0,z); // Get base linear address
      for(x=0;x<xdim;x++) { // y==0
        if(node_start<=i && i<node_stop) {
          energy[i] *= 26./24.;
          field[i]  *= 26./24.;
        }
        ++i;
      } // NB: At end of loop, i wraps around to next x-row.
      for(x=0;x<xdim;x++) { // y==1
        if(node_start<=i && i<node_stop) {
          energy[i] *= 21./24.;
          field[i]  *= 21./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==2
        if(node_start<=i && i<node_stop) {
          energy[i] *= 25./24.;
          field[i]  *= 25./24.;
        }
        ++i;
      }
      // Back face
      i = mesh->Index(0,ydim-3,z);
      for(x=0;x<xdim;x++) { // y==ydim-3
        if(node_start<=i && i<node_stop) {
          energy[i] *= 25./24.;
          field[i]  *= 25./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==ydim-2
        if(node_start<=i && i<node_stop) {
          energy[i] *= 21./24.;
          field[i]  *= 21./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==ydim-1
        if(node_start<=i && i<node_stop) {
          energy[i] *= 26./24.;
          field[i]  *= 26./24.;
        }
        ++i;
      }
    }
  } else if(ydim==5) {
    for(z=zstart;z<=zstop;++z) {
      OC_INDEX i = mesh->Index(0,0,z); // Get base linear address
      for(x=0;x<xdim;x++) { // y==0
        if(node_start<=i && i<node_stop) {
          energy[i] *= 26./24.;
          field[i]  *= 26./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==1
        if(node_start<=i && i<node_stop) {
          energy[i] *= 21./24.;
          field[i]  *= 21./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==2
        if(node_start<=i && i<node_stop) {
          energy[i] *= 26./24.;
          field[i]  *= 26./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==3
        if(node_start<=i && i<node_stop) {
          energy[i] *= 21./24.;
          field[i]  *= 21./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==4
        if(node_start<=i && i<node_stop) {
          energy[i] *= 26./24.;
          field[i]  *= 26./24.;
        }
        ++i;
      }
    }
  } else if(ydim==4) {
    for(z=zstart;z<=zstop;++z) {
      OC_INDEX i = mesh->Index(0,0,z); // Get base linear address
      for(x=0;x<xdim;x++) { // y==0
        if(node_start<=i && i<node_stop) {
          energy[i] *= 26./24.;
          field[i]  *= 26./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==1
        if(node_start<=i && i<node_stop) {
          energy[i] *= 22./24.;
          field[i]  *= 22./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==2
        if(node_start<=i && i<node_stop) {
          energy[i] *= 22./24.;
          field[i]  *= 22./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==3
        if(node_start<=i && i<node_stop) {
          energy[i] *= 26./24.;
          field[i]  *= 26./24.;
        }
        ++i;
      }
    }
  } else if(ydim==3) {
    for(z=zstart;z<=zstop;++z) {
      OC_INDEX i = mesh->Index(0,0,z); // Get base linear address
      for(x=0;x<xdim;x++) { // y==0
        if(node_start<=i && i<node_stop) {
          energy[i] *= 27./24.;
          field[i]  *= 27./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==1
        if(node_start<=i && i<node_stop) {
          energy[i] *= 18./24.;
          field[i]  *= 18./24.;
        }
        ++i;
      }
      for(x=0;x<xdim;x++) { // y==2
        if(node_start<=i && i<node_stop) {
          energy[i] *= 27./24.;
          field[i]  *= 27./24.;
        }
        ++i;
      }
    }
  }
  // Quadratic fit requires 3 points, so no higher order method
  // available if ydim<3.


  // z-axis.  Note special case handling for short lengths.
  if(zdim>=6) {
    // Bottom face, z==0
    OC_INDEX i = node_start;
    while(i<xydim && i<node_stop) { // z==0
      energy[i] *= 26./24.;
      field[i]  *= 26./24.;
      ++i;
    } // NB: At end of loop, i wraps around to next xy-plane.
    while(i<2*xydim && i<node_stop) { // z==1
      energy[i] *= 21./24.;
      field[i]  *= 21./24.;
      ++i;
    }
    while(i<3*xydim && i<node_stop) { // z==2
      energy[i] *= 25./24.;
      field[i]  *= 25./24.;
      ++i;
    }
    // Top face, z==zdim-1
    OC_INDEX itop = mesh->Index(0,0,zdim-3);
    i = (node_start<itop ? itop : node_start);
    while(i<itop+xydim && i<node_stop) { // z==zdim-3
      energy[i] *= 25./24.;
      field[i]  *= 25./24.;
      ++i;
    }
    while(i<itop+2*xydim && i<node_stop) { // z==zdim-2
      energy[i] *= 21./24.;
      field[i]  *= 21./24.;
      ++i;
    }
    while(i<node_stop) { // z==zdim-1
      energy[i] *= 26./24.;
      field[i]  *= 26./24.;
      ++i;
    }
  } else if(zdim==5) {
    OC_INDEX i = node_start;
    while(i<xydim && i<node_stop) { // z==0; bottom face
      energy[i] *= 26./24.;
      field[i]  *= 26./24.;
      ++i;
    }
    while(i<2*xydim && i<node_stop) { // z==1
      energy[i] *= 21./24.;
      field[i]  *= 21./24.;
      ++i;
    }
    while(i<3*xydim && i<node_stop) { // z==2
      energy[i] *= 26./24.;
      field[i]  *= 26./24.;
      ++i;
    }
    while(i<4*xydim && i<node_stop) { // z==3
      energy[i] *= 21./24.;
      field[i]  *= 21./24.;
      ++i;
    }
    while(i<node_stop) { // z==4; top face
      energy[i] *= 26./24.;
      field[i]  *= 26./24.;
      ++i;
    }
  } else if(zdim==4) {
    OC_INDEX i = node_start;
    while(i<xydim && i<node_stop) { // z==0; bottom face
      energy[i] *= 26./24.;
      field[i]  *= 26./24.;
      ++i;
    }
    while(i<2*xydim && i<node_stop) { // z==1
      energy[i] *= 22./24.;
      field[i]  *= 22./24.;
      ++i;
    }
    while(i<3*xydim && i<node_stop) { // z==2
      energy[i] *= 22./24.;
      field[i]  *= 22./24.;
      ++i;
    }
    while(i<node_stop) { // z==3; top face
      energy[i] *= 26./24.;
      field[i]  *= 26./24.;
      ++i;
    }
  } else if(zdim==3) {
    OC_INDEX i = node_start;
    while(i<xydim && i<node_stop) { // z==0; bottom face
      energy[i] *= 27./24.;
      field[i]  *= 27./24.;
      ++i;
    }
    while(i<2*xydim && i<node_stop) { // z==1
      energy[i] *= 18./24.;
      field[i]  *= 18./24.;
      ++i;
    }
    while(i<node_stop) { // z==2; top face
      energy[i] *= 27./24.;
      field[i]  *= 27./24.;
      ++i;
    }
  }
  // Quadratic fit requires 3 points, so no higher order method
  // available if zdim<3.

  // Copy from scratch to real buffers.
  Nb_Xpfloat energy_sum = 0.0;
  if(ocedt.energy_accum) {
    for(OC_INDEX i=node_start;i<node_stop;i++) {
      (*ocedt.energy_accum)[i] += energy[i];
      energy_sum += energy[i];
    }
  } else {
    for(OC_INDEX i=node_start;i<node_stop;i++) {
      energy_sum += energy[i];
    }
  }
  ocedtaux.energy_total_accum += energy_sum * mesh->Volume(0); // All cells
  /// have same volume in an Oxs_RectangularMesh.

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  if(ocedt.mxH_accum && ocedt.H_accum) {
    for(OC_INDEX i=node_start; i<node_stop; ++i) {
      (*ocedt.H_accum)[i] += field[i];
      ThreeVector temp = spin[i];
      temp ^= field[i];
      (*ocedt.mxH_accum)[i] += temp;
    }
  } else if(ocedt.mxH_accum) {
    for(OC_INDEX i=node_start; i<node_stop; ++i) {
      ThreeVector temp = spin[i];
      temp ^= field[i];
      (*ocedt.mxH_accum)[i] += temp;
    }
  } else if(ocedt.H_accum) {
    for(OC_INDEX i=node_start; i<node_stop; ++i) {
      (*ocedt.H_accum)[i] += field[i];
    }
  }
  if(ocedt.mxH) {
    for(OC_INDEX i=node_start; i<node_stop; ++i) {
      ThreeVector temp = spin[i];
      temp ^= field[i];
      (*ocedt.mxH)[i] = temp;
    }
  }
}

// Optional interface for conjugate-gradient evolver.
// For details on this code, see NOTES VI, 21-July-2011, pp 10-11.
OC_INT4m
Oxs_UniaxialAnisotropy::IncrementPreconditioner(PreconditionerData& pcd)
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

  if(has_multscript) return 0; // Unsupported option

  if(size<1) return 1; // Nothing to do

  if(mesh_id !=  state.mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.  Initialize/update data fields.
    // Note: max_K1 might be initialized here or in
    // ::ComputeEnergyChunkInitialize().
    if(aniscoeftype == K1_TYPE) {
      if(K1_is_uniform) {
        max_K1 = fabs(uniform_K1_value);
      } else {
        K1_init->FillMeshValue(state.mesh,K1);
        max_K1=0.0;
        for(OC_INDEX i=0;i<size;i++) {
          OC_REAL8m test = fabs(K1[i]);
          if(test>max_K1) max_K1 = test;
        }
      }
    } else if(aniscoeftype == Ha_TYPE) {
      if(!Ha_is_uniform) Ha_init->FillMeshValue(state.mesh,Ha);
      max_K1=0.0;
      const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
      OC_REAL8m tmpHa = uniform_Ha_value;
      for(OC_INDEX i=0;i<size;i++) {
        // Note: This code assumes that Ms doesn't change
        // for the lifetime of state.mesh.
        if(!Ha_is_uniform) tmpHa = Ha[i];
        OC_REAL8m test = fabs(tmpHa*Ms[i]);
        if(test>max_K1) max_K1 = test;
      }
      max_K1 *= 0.5*MU0;
    }
    if(!axis_is_uniform) {
      axis_init->FillMeshValue(state.mesh,axis);
      for(OC_INDEX i=0;i<size;i++) {
        // Check that axis is a unit vector:
        const OC_REAL8m eps = 1e-14;
        if(axis[i].MagSq()<eps*eps) {
          throw Oxs_ExtError(this,"Invalid initialization detected:"
                             " Zero length anisotropy axis");
        } else {
          axis[i].MakeUnit();
        }
      }
    }
    mesh_id = state.mesh->Id();
  }

  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);

  OC_REAL8m k = uniform_K1_value;
  OC_REAL8m h = uniform_Ha_value;
  ThreeVector unifaxis = uniform_axis_value;

  for(OC_INDEX i=0;i<size;++i) {
    if(Ms[i] == 0.0) continue;

    const ThreeVector& axisi = (axis_is_uniform ? unifaxis : axis[i]);
    if(aniscoeftype == K1_TYPE) {
      if(!K1_is_uniform) k = K1[i];
      if(k<=0) { // Easy plane (hard axis)
        val[i].x += -2*k*axisi.x*axisi.x/Ms[i];
        val[i].y += -2*k*axisi.y*axisi.y/Ms[i];
        val[i].z += -2*k*axisi.z*axisi.z/Ms[i];
      } else { // Easy axis (hard plane)
        val[i].x += 2*k*(1.0-axisi.x*axisi.x)/Ms[i];
        val[i].y += 2*k*(1.0-axisi.y*axisi.y)/Ms[i];
        val[i].z += 2*k*(1.0-axisi.z*axisi.z)/Ms[i];
      }
    } else {
      if(!Ha_is_uniform) h = Ha[i];
      if(h<=0) { // Easy plane (hard axis)
        val[i].x += -1*MU0*h*axisi.x*axisi.x;
        val[i].y += -1*MU0*h*axisi.y*axisi.y;
        val[i].z += -1*MU0*h*axisi.z*axisi.z;
      } else { // Easy axis (hard plane)
        val[i].x += MU0*h*(1.0-axisi.x*axisi.x);
        val[i].y += MU0*h*(1.0-axisi.y*axisi.y);
        val[i].z += MU0*h*(1.0-axisi.z*axisi.z);
      }
    }
  }

  return 1;
}
