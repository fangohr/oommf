/* FILE: timeevolver.h                 -*-Mode: c++-*-
 *
 * Abstract time evolver class
 *
 */

#ifndef _OXS_TIMEEVOLVER
#define _OXS_TIMEEVOLVER

#include "evolver.h"
#include "output.h"

/* End includes */

class Oxs_TimeDriver; // Forward references
struct Oxs_DriverStepInfo;

class Oxs_TimeEvolver:public Oxs_Evolver {
private:

  OC_UINT4m energy_calc_count; // Number of times GetEnergyDensity
  /// has been called in current problem run.

  Oxs_MeshValue<OC_REAL8m> temp_energy;     // Scratch space used by
  Oxs_MeshValue<ThreeVector> temp_field; // GetEnergyDensity().

  // Outputs maintained by this interface layer.  These are conceptually
  // public, but are specified private to force clients to use the
  // output_map interface.
  void UpdateEnergyOutputs(const Oxs_SimState&);
  void FillEnergyCalcCountOutput(const Oxs_SimState&);
  Oxs_ScalarOutput<Oxs_TimeEvolver> total_energy_output;
  Oxs_ScalarFieldOutput<Oxs_TimeEvolver> total_energy_density_output;
  Oxs_VectorFieldOutput<Oxs_TimeEvolver> total_field_output;
  Oxs_ScalarOutput<Oxs_TimeEvolver> energy_calc_count_output;

  // Disable copy constructor and assignment operator by declaring
  // them without defining them.
  Oxs_TimeEvolver(const Oxs_TimeEvolver&);
  Oxs_TimeEvolver& operator=(const Oxs_TimeEvolver&);

protected:

#if REPORT_TIME
  mutable Nb_StopWatch steponlytime;
#endif

  Oxs_TimeEvolver(const char* name,      // Child instance id
                 Oxs_Director* newdtr);  // App director
  Oxs_TimeEvolver(const char* name,
                 Oxs_Director* newdtr,
                 const char* argstr);      // MIF block argument string

  virtual OC_BOOL Init();  // All children of Oxs_TimeEvolver *must*
  /// call this function in their Init() routines.  The main purpose
  /// of this function is to initialize output variables.

  void GetEnergyDensity(const Oxs_SimState& state,
                        Oxs_MeshValue<OC_REAL8m>& energy,
                        Oxs_MeshValue<ThreeVector>* mxH_req,
                        Oxs_MeshValue<ThreeVector>* H_req,
                        OC_REAL8m& pE_pt,
                        OC_REAL8m& total_E);

  void GetEnergyDensity(const Oxs_SimState& state,
			Oxs_MeshValue<OC_REAL8m>& energy,
			Oxs_MeshValue<ThreeVector>* mxH_req,
			Oxs_MeshValue<ThreeVector>* H_req,
			OC_REAL8m& pE_pt) {
    // This interface for backwards compatibility
    OC_REAL8m dummy_E;
    GetEnergyDensity(state,energy,mxH_req,H_req,pE_pt,dummy_E);
  }

public:
  virtual ~Oxs_TimeEvolver();

  virtual OC_BOOL
  InitNewStage(const Oxs_TimeDriver* /* driver */,
               Oxs_ConstKey<Oxs_SimState> /* state */,
               Oxs_ConstKey<Oxs_SimState> /* prevstate */) { return 1; }
  /// Default implementation is a NOP.  Children may override.
  /// NOTE: prevstate may be "INVALID".  Children should check
  ///       before use.


  // There are two versions of the Step routine, one (Step) where
  // next_state is an Oxs_Key<Oxs_SimState>& import/export value, and
  // one (TryStep) where next_state is an Oxs_ConstKey<Oxs_SimState>&
  // export-only value.  In the former, the caller initializes
  // next_state with a fresh state by calling
  // director->GetNewSimulationState(), which leaves new_state with a
  // write lock.  The latter interface is more recent; with it the Step
  // code should make its own calls to director->GetNewSimulationState()
  // to get working states as needed, and pass the "next" state back to
  // the caller through the next_state reference.  This is more
  // flexible, and should be preferred by new code.  In particular, it
  // allows the evolver::TryStep() routine to return a reference to a
  // pre-existing state held in an Oxs_ConstKey<Oxs_SimState>, without
  // having to invoke a const_cast<> operation.  Be aware, however, that
  // this newer interface involves a transfer of "ownership" of the
  // state.  In the original Oxs_Key interface the caller created
  // next_state, passed it to Step(), received it back, and ultimately
  // disposed of it.  In the newer Oxs_ConstKey interface, the Step()
  // routine creates next_state and passes back to the caller.  But this
  // is what the Oxs_Key class is designed for, so it should work fine.
  //
  // In both cases, the return value is true if step was successful,
  // false if unable to step as requested.  Also, the evolver object
  // is responsible for calling driver->FillState() to fill next_state
  // as needed.
  //
  // As a migration aid, a default implementations are provided.  The
  // Oxs_Driver code calls the newer interface; child evolver classes
  // should  override exactly one of these.
  //
  // Note: We can't give the same name to both routines because of implicit
  // conversion of Oxs_Key<T> to Oxs_ConstKey<T>.
  //
  virtual OC_BOOL
  Step(const Oxs_TimeDriver* /* driver */,
       Oxs_ConstKey<Oxs_SimState> /* current_state */,
       const Oxs_DriverStepInfo& /* step_info */,
       Oxs_Key<Oxs_SimState>& /* next_state */)
 {
    throw Oxs_ExtError(this,
          "Programming error: Implementation of"
          " Oxs_TimeEvolver::Step(const Oxs_TimeDriver*,Oxs_ConstKey<Oxs_SimState>,Oxs_Key<Oxs_SimState>&)"
          " not provided.");
    return 0;
  }

  virtual OC_BOOL
  TryStep(const Oxs_TimeDriver* driver,
       Oxs_ConstKey<Oxs_SimState> current_state,
       const Oxs_DriverStepInfo& step_info,
       Oxs_ConstKey<Oxs_SimState>& next_state) {
    // Default implementation that wraps older Step() call.  New code
    // should override this implementation and ignore old Step()
    // interface.
    Oxs_Key<Oxs_SimState> temp_state;
    director->GetNewSimulationState(temp_state);
    OC_BOOL result = Step(driver,current_state,step_info,temp_state);
    temp_state.GetReadReference();
    next_state = temp_state;
    next_state.GetReadReference();
    return result;
  }
};

#endif // _OXS_TIMEEVOLVER
