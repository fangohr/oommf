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

  virtual OC_BOOL
  Step(const Oxs_TimeDriver* driver,
       Oxs_ConstKey<Oxs_SimState> current_state,
       const Oxs_DriverStepInfo& step_info,
       Oxs_Key<Oxs_SimState>& next_state) = 0;
  // Returns true if step was successful, false if
  // unable to step as requested.  The evolver object
  // is responsible for calling driver->FillState()
  // and driver->FillStateSupplemental() to fill
  // next_state as needed.
};

#endif // _OXS_TIMEEVOLVER
