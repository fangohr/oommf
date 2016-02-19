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
  : Oxs_Energy(name,newdtr,argstr),
    hscale(1.0), number_of_stages(0)
{
  // Process arguments
  String cmdoptreq = GetStringInitValue("script_args",
					"stage stage_time total_time");
  command_options.push_back(Nb_TclCommandLineOption("stage",1));
  command_options.push_back(Nb_TclCommandLineOption("stage_time",1));
  command_options.push_back(Nb_TclCommandLineOption("total_time",1));
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


void Oxs_ScriptUZeeman::GetEnergy
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 ) const
{
  OC_INDEX size = state.mesh->Size();
  if(size<1) return;

  // Use supplied buffer space, and reflect that use in oed.
  oed.energy = oed.energy_buffer;
  oed.field = oed.field_buffer;
  Oxs_MeshValue<OC_REAL8m>& energy = *oed.energy_buffer;
  Oxs_MeshValue<ThreeVector>& field = *oed.field_buffer;

  ThreeVector H,dHdt;
  GetAppliedField(state,H,dHdt);

  OC_INDEX i=0;
  do {
    field[i] = H;
    ++i;
  } while(i<size);

  ThreeVector vtemp(H);
  vtemp *= -MU0; // Constant so that
                /// vtemp * spin * Ms = cell energy density
  Nb_Xpfloat Msumx,Msumy,Msumz;
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state.Ms);
  const Oxs_Mesh* mesh = state.mesh;
  i=0;
  do {
    OC_REAL8m cellscale = Ms[i]*mesh->Volume(i);
    Msumx += cellscale * spin[i].x;
    Msumy += cellscale * spin[i].y;
    Msumz += cellscale * spin[i].z;
    energy[i] = Ms[i] * (spin[i]*vtemp);
    ++i;
  } while(i<size);
  Msumx *= dHdt.x;
  Msumy *= dHdt.y;
  Msumz *= dHdt.z;
  // Some compilers (xlC) choke on
  //  oed.pE_pt = -MU0*(Msumx + Msumy + Msumz);
  // so spell it out:
  Msumx += Msumy;
  Msumx += Msumz;
  Msumx *= -MU0;
  oed.pE_pt = Msumx.GetValue();
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
