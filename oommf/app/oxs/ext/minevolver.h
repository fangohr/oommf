/* FILE: minevolver.h                 -*-Mode: c++-*-
 *
 * Abstract minimization evolver class
 *
 */

#ifndef _OXS_MINEVOLVER
#define _OXS_MINEVOLVER

#include "evolver.h"

/* End includes */

class Oxs_MinDriver; // Forward references
struct Oxs_DriverStepInfo;

class Oxs_MinEvolver:public Oxs_Evolver {
protected:
  Oxs_MinEvolver(const char* name,     // Child instance id
                 Oxs_Director* newdtr)  // App director
    : Oxs_Evolver(name,newdtr) {}
  Oxs_MinEvolver(const char* name,
                 Oxs_Director* newdtr,
                 const char* argstr)      // MIF block argument string
    : Oxs_Evolver(name,newdtr,argstr) {}
public:
  virtual ~Oxs_MinEvolver() {}
  virtual OC_BOOL Init() { return Oxs_Evolver::Init(); }

  virtual OC_BOOL
  InitNewStage(const Oxs_MinDriver* /* driver */,
               Oxs_ConstKey<Oxs_SimState> /* state */,
               Oxs_ConstKey<Oxs_SimState> /* prevstate */) { return 1; }
  /// Default implementation is a NOP.  Children may override.


  // There are two versions of the Step routine, one (Step) where
  // next_state is an Oxs_Key<Oxs_SimState>& import/export value, and
  // one (TryStep) where next_state is an Oxs_ConstKey<Oxs_SimState>&
  // export-only value.  In the former, the caller initializes
  // next_state with a fresh state by calling
  // director->GetNewSimulationState(), which leaves new_state with a
  // write lock.  The latter interface is more recent; with it the
  // Step code should make its own calls to
  // director->GetNewSimulationState() to get working states as
  // needed, and pass the "next" state back to the caller through the
  // next_state reference.  This is more flexible, and should be
  // preferred by new code.  In particular, it allows the
  // evolver::Step() routine to return a reference to a pre-existing
  // state held in an Oxs_ConstKey<Oxs_SimState>, without having to
  // invoke a const_cast<> operation.  Be aware, however, that this
  // newer interface involves a transfer of "ownership" of the state.
  // In the original Oxs_Key interface the caller created next_state,
  // passed it to Step(), received it back, and ultimately dispose of
  // it.  In the newer Oxs_ConstKey interface, the Step() routine
  // creates next_state and passes back to the caller.  But this is
  // what the Oxs_Key class is designed for, so it should work fine.
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
  Step(const Oxs_MinDriver* /* driver */,
       Oxs_ConstKey<Oxs_SimState> /* current_state */,
       const Oxs_DriverStepInfo& /* step_info */,
       Oxs_Key<Oxs_SimState>& /* next_state */)
  {
    throw Oxs_ExtError(this,
          "Programming error: Implementation of"
          " Oxs_MinEvolver::Step(const Oxs_MinDriver*,Oxs_ConstKey<Oxs_SimState>,Oxs_Key<Oxs_SimState>&)"
          " not provided.");
    return 0;
  }

  virtual OC_BOOL
  TryStep(const Oxs_MinDriver* driver,
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

#endif // _OXS_MINEVOLVER
