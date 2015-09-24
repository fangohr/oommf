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

  virtual OC_BOOL
  Step(const Oxs_MinDriver* driver,
       Oxs_ConstKey<Oxs_SimState> current_state,
       const Oxs_DriverStepInfo& step_info,
       Oxs_Key<Oxs_SimState>& next_state) = 0;
  // Returns true if step was successful, false if
  // unable to step as requested.  The evolver object
  // is responsible for calling driver->FillState()
  // to fill next_state as needed.

};

#endif // _OXS_MINEVOLVER
