/* FILE: mindriver.cc            -*-Mode: c++-*-
 *
 * Example minimization Oxs_Driver class.
 *
 */

#include <string>

#include "nb.h"
#include "mindriver.h"
#include "director.h"
#include "simstate.h"
#include "minevolver.h"
#include "key.h"
#include "mesh.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_MinDriver);

/* End includes */

// Constructor
Oxs_MinDriver::Oxs_MinDriver(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Driver(name,newdtr,argstr),
    max_mxHxm_output_obj_ptr(NULL), total_energy_output_obj_ptr(NULL)
{
  // Process arguments
  // Get evolver name specification
  OXS_GET_INIT_EXT_OBJECT("evolver",Oxs_MinEvolver,evolver_obj);
  evolver_key.Set(evolver_obj.GetPtr());
  // Dependency lock on Oxs_MinEvolver object is
  // held until *this is destroyed.

  if(!HasInitValue("stopping_mxHxm")) {
    stopping_mxHxm.push_back(0.0); // Default is no control
  } else {
    GetGroupedRealListInitValue("stopping_mxHxm",stopping_mxHxm);
  }

  VerifyAllInitArgsUsed();

  // Reserve space for initial state (see GetInitialState() below)
  director->ReserveSimulationStateRequest(1);
}

Oxs_ConstKey<Oxs_SimState>
Oxs_MinDriver::GetInitialState() const
{
  Oxs_Key<Oxs_SimState> initial_state;
  director->GetNewSimulationState(initial_state);
  Oxs_SimState& istate = initial_state.GetWriteReference();
  SetStartValues(istate);
  istate.stage_start_time      = -1.;  // Dummy value
  istate.stage_elapsed_time    = -1.;  // Dummy value
  istate.last_timestep         = -1.;  // Dummy value
  const Oxs_SimState& fstate = initial_state.GetReadReference();
  /// Release write lock. The read lock will be automatically
  /// released when the key "initial_state" is destroyed.
  fstate.AddDerivedData("Last energy",0.);
  return initial_state;
}


OC_BOOL Oxs_MinDriver::Init()
{ 
  Oxs_Driver::Init();  // Run init routine in parent.
  /// This will call Oxs_MinDriver::GetInitialState().

  // Get pointer to output object providing max mxHxm data
  const Oxs_MinEvolver* evolver = evolver_key.GetPtr();
  if(evolver==NULL) {
    throw Oxs_ExtError(this,"PROGRAMMING ERROR: No evolver found?");
  }

  String output_name = String(evolver->InstanceName());
  output_name += String(":Max mxHxm");
  max_mxHxm_output_obj_ptr
    =  director->FindOutputObjectExact(output_name.c_str());
  if(max_mxHxm_output_obj_ptr==NULL) {
    String msg = String("Unable to identify unique \"")
      + output_name + String("\" object.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  // Get pointer to output object providing total energy
  output_name = String(evolver->InstanceName());
  output_name += String(":Total energy");
  total_energy_output_obj_ptr
    =  director->FindOutputObjectExact(output_name.c_str());
  if(total_energy_output_obj_ptr==NULL) {
    String msg = String("Unable to identify unique \"")
      + output_name + String("\" object.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  return 1;
}

Oxs_MinDriver::~Oxs_MinDriver()
{}


void Oxs_MinDriver::StageRequestCount
(unsigned int& min,
 unsigned int& max) const
{ // Number of stages wanted by driver

  Oxs_Driver::StageRequestCount(min,max);

  unsigned int count = static_cast<unsigned int>(stopping_mxHxm.size());
  if(count>min) min=count;
  if(count>1 && count<max) max=count;
  // Treat length 1 lists as imposing no upper constraint.
}

OC_BOOL
Oxs_MinDriver::ChildIsStageDone(const Oxs_SimState& state) const
{
  OC_UINT4m stage_index = state.stage_number;

  // Max mxHxm check
  Tcl_Interp* mif_interp = director->GetMifInterp();
  if(max_mxHxm_output_obj_ptr == NULL ||
     max_mxHxm_output_obj_ptr->Output(&state,mif_interp,0,NULL) != TCL_OK) {
    String msg=String("Unable to obtain Max mxHxm output: ");
    if(max_mxHxm_output_obj_ptr==NULL) {
      msg += String("PROGRAMMING ERROR: max_mxHxm_output_obj_ptr not set."
		    " Driver Init() probably not called.");
    } else {
      msg += String(Tcl_GetStringResult(mif_interp));
    }
    throw Oxs_ExtError(this,msg.c_str());
  }
  OC_BOOL err;
  OC_REAL8m max_mxHxm = Nb_Atof(Tcl_GetStringResult(mif_interp),err);
  if(err) {
    String msg=String("Error detected in StageDone method"
		      " --- Invalid Max dm/dt output: ");
    msg += String(Tcl_GetStringResult(mif_interp));
    throw Oxs_ExtError(this,msg.c_str());
  }
  OC_REAL8m stop_mxHxm=0.;
  if(stage_index >= stopping_mxHxm.size()) {
    stop_mxHxm = stopping_mxHxm[stopping_mxHxm.size()-1];
  } else {
    stop_mxHxm = stopping_mxHxm[stage_index];
  }
  if(stop_mxHxm>0.0 && max_mxHxm <= stop_mxHxm) {
    return 1; // Stage done
  }

  // If control gets here, then stage not done
  return 0;
}

OC_BOOL
Oxs_MinDriver::ChildIsRunDone(const Oxs_SimState& /* state */) const
{
  // No child-specific checks at this time...
  return 0; // Run not done
}

void Oxs_MinDriver::FillStateMemberData
(const Oxs_SimState& old_state,
 Oxs_SimState& new_state) const
{
  Oxs_Driver::FillStateMemberData(old_state,new_state);

  // Copy over values from old_state for time indexes, since
  // with mindriver time is not advanced.  Perhaps the time index
  // variables should be moved over to the WOO area in simstate?
  new_state.stage_start_time   = old_state.stage_start_time;
  new_state.stage_elapsed_time = old_state.stage_elapsed_time;
  new_state.last_timestep      = old_state.last_timestep;
}

OC_REAL8m
Oxs_MinDriver::GetTotalEnergy(const Oxs_SimState& tstate) const
{
  OC_REAL8m tenergy;
  if(!tstate.GetDerivedData("Total energy",tenergy)) {
    // Energy field not filled in tstate.  Try calling output
    // object, which should call necessary update functions.
    Tcl_Interp* mif_interp = director->GetMifInterp();
    if(total_energy_output_obj_ptr == NULL ||
       total_energy_output_obj_ptr->Output(&tstate,mif_interp,0,NULL)
       != TCL_OK) {
      String msg =
	String("Error in Oxs_MinDriver::GetTotalEnergy:"
	       " Unable to obtain \"Total energy\" output: ");
      if(total_energy_output_obj_ptr==NULL) {
	msg += String("PROGRAMMING ERROR:"
		      " total_energy_output_obj_ptr not set."
		      " Driver Init() probably not called.");
      } else {
	msg += String(Tcl_GetStringResult(mif_interp));
      }
      throw Oxs_ExtError(this,msg.c_str());
    }
    OC_BOOL err;
    tenergy = Nb_Atof(Tcl_GetStringResult(mif_interp),err);
    if(err) {
      String msg=String("Error detected in Oxs_MinDriver::GetTotalEnergy"
			" --- Invalid \"Total energy\" output: ");
      msg += String(Tcl_GetStringResult(mif_interp));
      throw Oxs_ExtError(this,msg.c_str());
    }
  }
  return tenergy;
 }

void Oxs_MinDriver::FillStateDerivedData
(const Oxs_SimState& old_state,
 const Oxs_SimState& new_state) const
{
  Oxs_Driver::FillStateDerivedData(old_state,new_state);

  // Keep track of energy from previous state.  This is used
  // by "Delta E" output.
  OC_REAL8m old_energy = GetTotalEnergy(old_state);
  new_state.AddDerivedData("Last energy",old_energy);
}

void Oxs_MinDriver::FillNewStageStateDerivedData
(const Oxs_SimState& old_state,
 int new_stage_number,
 const Oxs_SimState& new_state) const
{
  Oxs_Driver::FillNewStageStateDerivedData(old_state,new_stage_number,
                                           new_state);
  OC_REAL8m old_energy = GetTotalEnergy(old_state);
  new_state.AddDerivedData("Last energy",old_energy);
}

OC_BOOL
Oxs_MinDriver::Step
(Oxs_ConstKey<Oxs_SimState> base_state,
 const Oxs_DriverStepInfo& stepinfo,
 Oxs_ConstKey<Oxs_SimState>& next_state)
{ // Returns true if step was successful, false if
  // unable to step as requested.

  // Put write lock on evolver in order to get a non-const
  // pointer.  Use a temporary variable, temp_key, so
  // write lock is automatically removed when temp_key
  // is destroyed.
  Oxs_Key<Oxs_MinEvolver> temp_key = evolver_key;
  Oxs_MinEvolver& evolver = temp_key.GetWriteReference();
  return evolver.TryStep(this,base_state,stepinfo,next_state);
}

OC_BOOL
Oxs_MinDriver::InitNewStage
(Oxs_ConstKey<Oxs_SimState> state,
 Oxs_ConstKey<Oxs_SimState> prevstate)
{
  // Put write lock on evolver in order to get a non-const
  // pointer.  Use a temporary variable, temp_key, so
  // write lock is automatically removed when temp_key
  // is destroyed.
  Oxs_Key<Oxs_MinEvolver> temp_key = evolver_key;
  Oxs_MinEvolver& evolver = temp_key.GetWriteReference();
  return evolver.InitNewStage(this,state,prevstate);
}
