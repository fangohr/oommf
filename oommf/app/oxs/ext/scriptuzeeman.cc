/* FILE: scriptuzeeman.cc            -*-Mode: c++-*-
 *
 * Uniform Zeeman (applied field) energy, derived from Oxs_Energy class,
 * specified from a Tcl proc.
 *
 */

#include <limits>
#include <string>
#include <vector>

#include "oc.h"
#include "nb.h"

#include "director.h"
#include "scriptuzeeman.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_ScriptUZeeman);

/* End includes */


// Constructor
Oxs_ScriptUZeeman::Oxs_ScriptUZeeman(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_ChunkEnergy(name,newdtr,argstr),
    hscale(1.0), number_of_stages(0)
{
  // Process arguments
  String cmdoptreq = GetStringInitValue("script_args",
					"stage stage_time total_time");
  command_options.push_back(Nb_TclCommandLineOption("stage",1));
  command_options.push_back(Nb_TclCommandLineOption("stage_time",1));
  command_options.push_back(Nb_TclCommandLineOption("total_time",1));
  command_options.push_back(Nb_TclCommandLineOption("base_state_id",1));
  command_options.push_back(Nb_TclCommandLineOption("current_state_id",1));
  String runscript = GetStringInitValue("script");

  hscale = GetRealInitValue("multiplier",1.0);

  number_of_stages = GetUIntInitValue("stage_count",0);
  /// Default number_of_stages is 0, i.e., no preference.

  Bapp_output.Setup(this,InstanceName(),"B","mT",1,
		    &Oxs_ScriptUZeeman::Fill__Bapp_output);
  Bappx_output.Setup(this,InstanceName(),"Bx","mT",1,
		     &Oxs_ScriptUZeeman::Fill__Bapp_output);
  Bappy_output.Setup(this,InstanceName(),"By","mT",1,
		     &Oxs_ScriptUZeeman::Fill__Bapp_output);
  Bappz_output.Setup(this,InstanceName(),"Bz","mT",1,
		     &Oxs_ScriptUZeeman::Fill__Bapp_output);

  Bapp_output.Register(director,0);
  Bappx_output.Register(director,0);
  Bappy_output.Register(director,0);
  Bappz_output.Register(director,0);

  VerifyAllInitArgsUsed();

  // Run SetBaseCommand *after* VerifyAllInitArgsUsed(); this produces
  // a more intelligible error message in case a command line argument
  // error is really due to a misspelled parameter label.
  cmd.SetBaseCommand(InstanceName(),
		     director->GetMifInterp(),
		     runscript,
		     Nb_ParseTclCommandLineRequest(InstanceName(),
                                                   command_options,
                                                   cmdoptreq));
}

Oxs_ScriptUZeeman::~Oxs_ScriptUZeeman()
{}

OC_BOOL Oxs_ScriptUZeeman::Init()
{
  return Oxs_Energy::Init();
}

void
Oxs_ScriptUZeeman::StageRequestCount
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
Oxs_ScriptUZeeman::GetAppliedField
(const Oxs_SimState& state,
 ThreeVector& H,
 ThreeVector& dHdt
) const
{
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
  if((index = command_options[3].position)>=0) { // base_state_id
    const Oxs_SimState* base_state = director->FindBaseStepState(&state);
    cmd.SetCommandArg(index,base_state->Id());
  }
  if((index = command_options[4].position)>=0) { // current_state_id
    cmd.SetCommandArg(index,state.Id());
  }

  cmd.Eval();

  if(cmd.GetResultListSize()!=6) {
    String msg
      = String("Return script value is not a 6-tuple: ")
      + cmd.GetWholeResult();
    cmd.RestoreInterpResult();
    throw Oxs_ExtError(this,msg.c_str());
  }

  cmd.GetResultListItem(0,H.x);
  cmd.GetResultListItem(1,H.y);
  cmd.GetResultListItem(2,H.z);
  cmd.GetResultListItem(3,dHdt.x);
  cmd.GetResultListItem(4,dHdt.y);
  cmd.GetResultListItem(5,dHdt.z);
  H *= hscale;
  dHdt *= hscale;

  cmd.RestoreInterpResult();
}


void Oxs_ScriptUZeeman::ComputeEnergyChunkInitialize
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>& /* thread_ocedtaux */,
 int /* number_of_threads */) const
{
  GetAppliedField(state,H_work,dHdt_work);
  ocedt.energy_density_error_estimate
    = 4*OC_REAL8m_EPSILON*MU0*state.max_absMs*H_work.Mag();
}

void Oxs_ScriptUZeeman::ComputeEnergyChunk
(const Oxs_SimState& state,
 Oxs_ComputeEnergyDataThreaded& ocedt,
 Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
 OC_INDEX node_start,OC_INDEX node_stop,
 int /* threadnumber */
 ) const
{
  OC_INDEX size = state.mesh->Size();
  if(size<1) return;

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  const Oxs_Mesh* mesh = state.mesh;
  ThreeVector    H = H_work;    // Local copies
  ThreeVector dHdt = dHdt_work;

  ThreeVector vtemp(H);
  vtemp *= -MU0; // Constant so that
                /// vtemp * spin * Ms = cell energy density
  Nb_Xpfloat Msumx = 0.0;
  Nb_Xpfloat Msumy = 0.0;
  Nb_Xpfloat Msumz = 0.0;
  Nb_Xpfloat energy_sum = 0.0;

  OC_REAL8m cell_volume;
  if(state.mesh->HasUniformCellVolumes(cell_volume)) {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      const ThreeVector&  m = spin[i];
      const OC_REAL8m cellvolume = mesh->Volume(i);
      OC_REAL8m cellscale = Ms[i]*cellvolume;
      Msumx += cellscale * m.x;
      Msumy += cellscale * m.y;
      Msumz += cellscale * m.z;
      OC_REAL8m ei  = Ms[i] * (m*vtemp);
      energy_sum += ei;
      OC_REAL8m tz = m.x*H.y;  // t = m x H
      OC_REAL8m ty = m.x*H.z;
      OC_REAL8m tx = m.y*H.z;
      tz -= m.y*H.x;    ty  = m.z*H.x - ty;    tx -= m.z*H.y;

      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)            (*ocedt.H)[i] = H;
      if(ocedt.H_accum)      (*ocedt.H_accum)[i] += H;
      if(ocedt.mxH)          (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
      if(ocedt.mxH_accum)    (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
    }
    energy_sum *= cell_volume;
  } else {
    for(OC_INDEX i=node_start;i<node_stop;++i) {
      const ThreeVector&  m = spin[i];
      const OC_REAL8m cellvolume = mesh->Volume(i);
      OC_REAL8m cellscale = Ms[i]*cellvolume;
      Msumx += cellscale * m.x;
      Msumy += cellscale * m.y;
      Msumz += cellscale * m.z;
      OC_REAL8m ei  = Ms[i] * (m*vtemp);
      energy_sum += ei * cellvolume;
      OC_REAL8m tz = m.x*H.y;  // t = m x H
      OC_REAL8m ty = m.x*H.z;
      OC_REAL8m tx = m.y*H.z;
      tz -= m.y*H.x;    ty  = m.z*H.x - ty;    tx -= m.z*H.y;

      if(ocedt.energy)       (*ocedt.energy)[i] = ei;
      if(ocedt.energy_accum) (*ocedt.energy_accum)[i] += ei;
      if(ocedt.H)            (*ocedt.H)[i] = H;
      if(ocedt.H_accum)      (*ocedt.H_accum)[i] += H;
      if(ocedt.mxH)          (*ocedt.mxH)[i] = ThreeVector(tx,ty,tz);
      if(ocedt.mxH_accum)    (*ocedt.mxH_accum)[i] += ThreeVector(tx,ty,tz);
    }
  }

  Msumx *= dHdt.x;
  Msumy *= dHdt.y;
  Msumz *= dHdt.z;
  // Some compilers (xlC) choke on
  //  oed.pE_pt = -MU0*(Msumx + Msumy + Msumz);
  // so spell it out:
  Msumx += Msumy;
  Msumx += Msumz;
  Msumx *= -MU0;

  ocedtaux.energy_total_accum += energy_sum;
  ocedtaux.pE_pt_accum += Msumx;
}

void
Oxs_ScriptUZeeman::Fill__Bapp_output(const Oxs_SimState& state)
{
  ThreeVector B,dHdt;
  GetAppliedField(state,B,dHdt);
  B *= MU0 * 1000; /// Report Bapp in mT

  if(Bapp_output.GetCacheRequestCount()>0) {
    Bapp_output.cache.state_id=0;
    Bapp_output.cache.value = sqrt(B.MagSq());
    Bapp_output.cache.state_id=state.Id();
  }

  if(Bappx_output.GetCacheRequestCount()>0) {
    Bappx_output.cache.state_id=0;
    Bappx_output.cache.value = B.x;
    Bappx_output.cache.state_id=state.Id();
  }

  if(Bappy_output.GetCacheRequestCount()>0) {
    Bappy_output.cache.state_id=0;
    Bappy_output.cache.value = B.y;
    Bappy_output.cache.state_id=state.Id();
  }

  if(Bappz_output.GetCacheRequestCount()>0) {
    Bappz_output.cache.state_id=0;
    Bappz_output.cache.value = B.z;
    Bappz_output.cache.state_id=state.Id();
  }

}
