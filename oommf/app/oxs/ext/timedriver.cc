/* FILE: timedriver.cc            -*-Mode: c++-*-
 *
 * Example concrete Oxs_Driver class.
 *
 */

#include <string>

#include "nb.h"
#include "timedriver.h"
#include "director.h"
#include "simstate.h"
#include "timeevolver.h"
#include "key.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_TimeDriver);

/* End includes */

// Constructor
Oxs_TimeDriver::Oxs_TimeDriver(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_Driver(name,newdtr,argstr), max_dm_dt_obj_ptr(NULL)
{
  // Process arguments
  OXS_GET_INIT_EXT_OBJECT("evolver",Oxs_TimeEvolver,evolver_obj);
  evolver_key.Set(evolver_obj.GetPtr());
  // Dependency lock on Oxs_TimeEvolver object is
  // held until *this is destroyed.

  if(!HasInitValue("stopping_dm_dt")) {
    stopping_dm_dt.push_back(0.0); // Default is no control
  } else {
    GetGroupedRealListInitValue("stopping_dm_dt",stopping_dm_dt);
  }

  if(!HasInitValue("stopping_time")) {
    stopping_time.push_back(0.0); // Default is no control
  } else {
    GetGroupedRealListInitValue("stopping_time",stopping_time);
  }

  VerifyAllInitArgsUsed();

  last_timestep_output.Setup(
           this,InstanceName(),"Last time step","s",0,
	   &Oxs_TimeDriver::Fill__last_timestep_output);
  simulation_time_output.Setup(
	   this,InstanceName(),"Simulation time","s",0,
	   &Oxs_TimeDriver::Fill__simulation_time_output);

  last_timestep_output.Register(director,0);
  simulation_time_output.Register(director,0);

  // Reserve space for initial state (see GetInitialState() below)
  director->ReserveSimulationStateRequest(1);
}

Oxs_ConstKey<Oxs_SimState>
Oxs_TimeDriver::GetInitialState() const
{
  Oxs_Key<Oxs_SimState> initial_state;
  director->GetNewSimulationState(initial_state);
  Oxs_SimState& istate = initial_state.GetWriteReference();
  SetStartValues(istate);
  initial_state.GetReadReference();  // Release write lock.
  /// The read lock will be automatically released when the
  /// key "initial_state" is destroyed.
  return initial_state;
}


OC_BOOL Oxs_TimeDriver::Init()
{ 
  Oxs_Driver::Init();  // Run init routine in parent.
  /// This will call Oxs_TimeDriver::GetInitialState().

  // Get pointer to output object providing max dm/dt data
  const Oxs_TimeEvolver* evolver = evolver_key.GetPtr();
  if(evolver==NULL) {
    throw Oxs_ExtError(this,"PROGRAMMING ERROR: No evolver found?");
  }
  String output_name = String(evolver->InstanceName());
  output_name += String(":Max dm/dt");
  max_dm_dt_obj_ptr
    =  director->FindOutputObjectExact(output_name.c_str());
  if(max_dm_dt_obj_ptr==NULL) {
    throw Oxs_ExtError(this,"Unable to identify unique"
                         " Max dm/dt output object");
  }

  return 1;
}

Oxs_TimeDriver::~Oxs_TimeDriver()
{}

void Oxs_TimeDriver::StageRequestCount
(unsigned int& min,
 unsigned int& max) const
{ // Number of stages wanted by driver

  Oxs_Driver::StageRequestCount(min,max);

  unsigned int count = static_cast<OC_UINT4m>(stopping_dm_dt.size());
  if(count>min) min=count;
  if(count>1 && count<max) max=count;
  // Treat length 1 lists as imposing no upper constraint.

  count =  static_cast<OC_UINT4m>(stopping_time.size());
  if(count>min) min=count;
  if(count>1 && count<max) max=count;
  // Treat length 1 lists as imposing no upper constraint.
}

OC_BOOL
Oxs_TimeDriver::ChildIsStageDone(const Oxs_SimState& state) const
{
  OC_UINT4m stage_index = state.stage_number;

  // Stage time check
  OC_REAL8m stop_time=0.;
  if(stage_index >= stopping_time.size()) {
    stop_time = stopping_time[stopping_time.size()-1];
  } else {
    stop_time = stopping_time[stage_index];
  }
  if(stop_time>0.0
     && stop_time-state.stage_elapsed_time<=stop_time*OC_REAL8_EPSILON*2) {
    return 1; // Stage done
  }

  // dm_dt check
  Tcl_Interp* mif_interp = director->GetMifInterp();
  if(max_dm_dt_obj_ptr==NULL ||
     max_dm_dt_obj_ptr->Output(&state,mif_interp,0,NULL) != TCL_OK) {
    String msg=String("Unable to obtain Max dm/dt output: ");
    if(max_dm_dt_obj_ptr==NULL) {
      msg += String("PROGRAMMING ERROR: max_dm_dt_obj_ptr not set."
		    " Driver Init() probably not called.");
    } else {
      msg += String(Tcl_GetStringResult(mif_interp));
    }
    throw Oxs_ExtError(this,msg.c_str());
  }
  OC_BOOL err;
  OC_REAL8m max_dm_dt = Nb_Atof(Tcl_GetStringResult(mif_interp),err);
  if(err) {
    String msg=String("Error detected in StageDone method"
		      " --- Invalid Max dm/dt output: ");
    msg += String(Tcl_GetStringResult(mif_interp));
    throw Oxs_ExtError(this,msg.c_str());
  }
  OC_REAL8m stop_dm_dt=0.;
  if(stage_index >= stopping_dm_dt.size()) {
    stop_dm_dt = stopping_dm_dt[stopping_dm_dt.size()-1];
  } else {
    stop_dm_dt = stopping_dm_dt[stage_index];
  }
  if(stop_dm_dt>0.0 && max_dm_dt <= stop_dm_dt) {
    return 1; // Stage done
  }

  // If control gets here, then stage not done
  return 0;
}

OC_BOOL
Oxs_TimeDriver::ChildIsRunDone(const Oxs_SimState& /* state */) const
{
  // No child-specific checks at this time...
  return 0; // Run not done
}

void Oxs_TimeDriver::FillStateSupplemental(Oxs_SimState& work_state) const
{
  OC_REAL8m work_step = work_state.last_timestep;
  OC_REAL8m base_time = work_state.stage_elapsed_time - work_step;

  // Insure that step does not go past stage stopping time
  OC_UINT4m stop_index = work_state.stage_number;
  OC_REAL8m stop_value=0.0;
  if(stop_index >= stopping_time.size()) {
    stop_value = stopping_time[stopping_time.size()-1];
  } else {
    stop_value = stopping_time[stop_index];
  }
  if(stop_value>0.0) {
    OC_REAL8m timediff = stop_value-work_state.stage_elapsed_time;
    if(timediff<=0) { // Over step
      // In the degenerate case where dm_dt=0, work_step will be
      // large (==1) and work_state.stage_elapsed_time will also
      // be large.  In that case, timediff will be numerically
      // poor because stop_value << work_state.stage_elapsed_time.
      // Check for this, and adjust sums accordingly.
      if(work_step>stop_value) { // Degenerate case
        work_step -= work_state.stage_elapsed_time;
        work_step += stop_value;
      } else {                   // Normal case
        work_step += timediff;
      }
      if(work_step<=0.0) work_step = stop_value*OC_REAL8_EPSILON; // Safety
      work_state.last_timestep = work_step;
      work_state.stage_elapsed_time = stop_value;
    } else if(timediff < 2*stop_value*OC_REAL8_EPSILON) {
      // Under step, but close enough for government work
      work_state.last_timestep += timediff;
      work_state.stage_elapsed_time = stop_value;
    } else if(0.25*work_step>timediff) {
      // Getting close to stage boundary.  Foreshorten.
      OC_REAL8m tempstep = (3*work_step+timediff)*0.25;
      work_state.last_timestep = tempstep;
      work_state.stage_elapsed_time = base_time+tempstep;
    }
  }
}

OC_BOOL
Oxs_TimeDriver::Step
(Oxs_ConstKey<Oxs_SimState> base_state,
 const Oxs_DriverStepInfo& stepinfo,
 Oxs_Key<Oxs_SimState>& next_state)
{ // Returns true if step was successful, false if
  // unable to step as requested.

  // Put write lock on evolver in order to get a non-const
  // pointer.  Use a temporary variable, temp_key, so
  // write lock is automatically removed when temp_key
  // is destroyed.
  Oxs_Key<Oxs_TimeEvolver> temp_key = evolver_key;
  Oxs_TimeEvolver& evolver = temp_key.GetWriteReference();
  return evolver.Step(this,base_state,stepinfo,next_state);
}

OC_BOOL
Oxs_TimeDriver::InitNewStage
(Oxs_ConstKey<Oxs_SimState> state,
 Oxs_ConstKey<Oxs_SimState> prevstate)
{
  // Put write lock on evolver in order to get a non-const
  // pointer.  Use a temporary variable, temp_key, so
  // write lock is automatically removed when temp_key
  // is destroyed.
  Oxs_Key<Oxs_TimeEvolver> temp_key = evolver_key;
  Oxs_TimeEvolver& evolver = temp_key.GetWriteReference();
  return evolver.InitNewStage(this,state,prevstate);
}


////////////////////////////////////////////////////////////////////////
// State-based outputs, maintained by the driver.  These are
// conceptually public, but are specified private to force
// clients to use the output_map interface in Oxs_Director.

#define OSO_FUNC(NAME) \
void Oxs_TimeDriver::Fill__##NAME##_output(const Oxs_SimState& state) \
{ NAME##_output.cache.state_id=state.Id(); \
  NAME##_output.cache.value=state.NAME; }

OSO_FUNC(last_timestep)

void
Oxs_TimeDriver::Fill__simulation_time_output(const Oxs_SimState& state)
{
  simulation_time_output.cache.state_id = state.Id();
  simulation_time_output.cache.value =
    state.stage_start_time + state.stage_elapsed_time;
}
