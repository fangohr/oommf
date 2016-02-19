/* FILE: rungekuttaevolve.h                 -*-Mode: c++-*-
 *
 * Concrete evolver class, using Runge-Kutta steps
 *
 */

#ifndef _OXS_RUNGEKUTTAEVOLVE
#define _OXS_RUNGEKUTTAEVOLVE

#include <vector>

#include "timeevolver.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "scalarfield.h"
#include "output.h"

/* End includes */

#if REPORT_TIME
# ifndef REPORT_TIME_RKDEVEL
#  define REPORT_TIME_RKDEVEL 1
# endif
#endif
#ifndef REPORT_TIME_RKDEVEL
# define REPORT_TIME_RKDEVEL 0
#endif

#if OOMMF_THREADS
  class _Oxs_RKEvolve_Step_ThreadA; // Forward references
  class _Oxs_RKEvolve_RKFBase54_ThreadB;
  class _Oxs_RKEvolve_RKFBase54_ThreadC;
  class _Oxs_RKEvolve_RKFBase54_ThreadD;
#endif

// Zhang damping is a damping term added to LLG that depends directly on
// the local magnetization configuration.  See
//
//   Shufeng Zhang and Steven S.-L. Zhang, "Generalization of the
//   Landau-Lifshitz-Gilbert equation for conducting ferromagnetics,"
//   Physical Review Letters, 102, 086601 (2009).
//
// and also NOTES VI, 1-June-2012, p 40-41.
#ifndef ZHANG_DAMPING
# define ZHANG_DAMPING 0
#endif

// Tserkovnyak damping is a damping term added to LLG that depends directly on
// the local magnetization configuration.  See
//
//   Y. Tserkovnyak, E.M. Hankiewicz, G. Vignale, "Transverse spin diffusion in ferromagnets,"
//   Physical Review B, 79, 094415 (2009).
//
#ifndef TSERKOVNYAK_DAMPING
# define TSERKOVNYAK_DAMPING 0
#endif

#if OOMMF_THREADS && (ZHANG_DAMPING || TSERKOVNYAK_DAMPING)
# error Zhang and Tserkovnyak damping not supported for threaded builds
#endif

class Oxs_RungeKuttaEvolve:public Oxs_TimeEvolver {
private:
#if REPORT_TIME_RKDEVEL
  mutable vector<Nb_StopWatch> timer;
  struct TimerCounts {
  public:
    OC_INT4m pass_count;
    size_t bytes;
    String name;
    TimerCounts() : pass_count(0), bytes(0) {}
    void Reset() { pass_count = 0; bytes = 0; }
  };
  mutable vector<TimerCounts> timer_counts;
#endif

  mutable OC_UINT4m mesh_id;     // Used by gamma and alpha meshvalues to
  void UpdateMeshArrays(const Oxs_Mesh*);   /// track changes in mesh.

  // Base step size control parameters
  OC_REAL8m min_timestep;           // Seconds
  OC_REAL8m max_timestep;           // Seconds

  const OC_REAL8m max_step_decrease;        // Safety size adjusment
  const OC_REAL8m max_step_increase_limit;  // bounds.
  const OC_REAL8m max_step_increase_adj_ratio;
  OC_REAL8m max_step_increase;
  /// NOTE: These bounds do not include step_headroom, which
  /// is applied at the end.

  // Error-based step size control parameters.  Each may be disabled
  // by setting to -1.  There is an additional step size control that
  // insures that energy is monotonically non-increasing (up to
  // estimated rounding error).
  OC_REAL8m allowed_error_rate;  // Step size is adjusted so
  /// that the estimated maximum error (across all spins) divided
  /// by the step size is smaller than this value.  The units
  /// internally are radians per second, converted from the value
  /// specified in the input MIF file, which is in deg/sec.

  OC_REAL8m allowed_absolute_step_error; // Similar to allowed_error_rate,
  /// but without the step size adjustment.  Internal units are
  /// radians; MIF input units are degrees.

  OC_REAL8m allowed_relative_step_error; // Step size is adjusted so that
  /// the estimated maximum error (across all spins) divided by
  /// [maximum dm/dt (across all spins) * step size] is smaller than
  /// this value.  This value is non-dimensional, representing the
  /// allowed relative (proportional) error, presumably in (0,1).

  OC_REAL8m expected_energy_precision; // Expected relative energy
  /// precision.

  OC_REAL8m reject_goal,reject_ratio;
  OC_REAL8m min_step_headroom,max_step_headroom;
  OC_REAL8m step_headroom; // Safety margin used in step size adjustment

  // Spatially variable Landau-Lifschitz-Gilbert gyromagnetic ratio
  // and damping coefficients.
  OC_BOOL do_precess;  // If false, then do pure damping
  OC_BOOL allow_signed_gamma; // If false, then force gamma negative
  enum GammaStyle { GS_INVALID, GS_LL, GS_G }; // Landau-Lifshitz or Gilbert
  GammaStyle gamma_style;
  Oxs_OwnedPointer<Oxs_ScalarField> gamma_init;
  mutable Oxs_MeshValue<OC_REAL8m> gamma;

  Oxs_OwnedPointer<Oxs_ScalarField> alpha_init;
  mutable Oxs_MeshValue<OC_REAL8m> alpha;

#if ZHANG_DAMPING
  // Zhang damping; use 0.0 (default) to disable.  The support member
  // variables mesh_* are initialized inside the ::UpdateMeshArrays
  // member function.
  OC_REAL8m zhang_damping;
  OC_INDEX mesh_ispan, mesh_jspan, mesh_kspan;
  OC_REAL8m mesh_delx_inverse, mesh_dely_inverse, mesh_delz_inverse;
  void Compute_delm (const Oxs_SimState& state_,  // Import
                     const OC_INDEX mi,           // Import
                     const OC_INDEX mj,           // Import
                     const OC_INDEX mk,           // Import
                     const OC_INDEX i,            // Import
                     ThreeVector& D1,             // Export; 1st row of Dab
                     ThreeVector& D2,             // Export; 2nd row of Dab
                     ThreeVector& D3) const;      // Export; 3rd row of Dab
#endif // ZHANG_DAMPING

#if TSERKOVNYAK_DAMPING

#endif // TSERKOVNYAK_DAMPING


  // The next timestep is based on the error from the last step.  If
  // there is no last step (either because this is the first step,
  // or because the last state handled by this routine is different
  // from the incoming current_state), then timestep is calculated
  // so that max_dm_dt * timestep = start_dm, or timestep = start_dt,
  // whichever is smaller.  Either can be disabled by setting <0.
  OC_REAL8m start_dm;
  OC_REAL8m start_dt;

  // Stepsize control for first step of each stage after the first.
  // Choices are to use start conditions (start_dm and/or start_dt),
  // use continuation from end of previous stage, or to automatically
  // select between the two methods depending on whether or not the
  // energy appears to be continuous across the stage boundary.
  enum StageInitStepControl { SISC_INVALID, SISC_START_COND,
			      SISC_CONTINUOUS, SISC_AUTO };
  StageInitStepControl stage_init_step_control;

  // Data cached from last state
  OC_UINT4m energy_state_id;
  Oxs_MeshValue<OC_REAL8m> energy;
  OC_REAL8m next_timestep;

  // Outputs
  void UpdateDerivedOutputs(const Oxs_SimState& state,
                            const Oxs_SimState* prevstate);
  void UpdateDerivedOutputs(const Oxs_SimState& state) {
    UpdateDerivedOutputs(state,NULL);
  }
  Oxs_ScalarOutput<Oxs_RungeKuttaEvolve> max_dm_dt_output;
  Oxs_ScalarOutput<Oxs_RungeKuttaEvolve> dE_dt_output;
  Oxs_ScalarOutput<Oxs_RungeKuttaEvolve> delta_E_output;
  Oxs_VectorFieldOutput<Oxs_RungeKuttaEvolve> dm_dt_output;
  Oxs_VectorFieldOutput<Oxs_RungeKuttaEvolve> mxH_output;

  // Scratch space
  Oxs_MeshValue<OC_REAL8m> temp_energy;
  Oxs_MeshValue<ThreeVector> vtmpA;
  Oxs_MeshValue<ThreeVector> vtmpB;
  Oxs_MeshValue<ThreeVector> vtmpC;
  Oxs_MeshValue<ThreeVector> vtmpD;

  // Utility functions
  void CheckCache(const Oxs_SimState& cstate);

  void AdjustState(OC_REAL8m hstep,
		   OC_REAL8m mstep,
		   const Oxs_SimState& old_state,
		   const Oxs_MeshValue<ThreeVector>& dm_dt,
		   Oxs_SimState& new_state,
		   OC_REAL8m& norm_error) const;
  // Export new state has time index from old_state + h,
  // and spins from old state + mstep*dm_dt and re-normalized.

#if OOMMF_THREADS
  void AdjustState
  (OC_REAL8m hstep,
   OC_REAL8m mstep,
   const Oxs_SimState& old_state,
   vector<OC_REAL8m>& b,
   vector<const Oxs_MeshValue<ThreeVector>*>& A,
   Oxs_MeshValue<ThreeVector>& kn, // target = b*source
   Oxs_SimState& new_state,
   OC_REAL8m& norm_error) const;
  // Same as preceding AdjustState, except that dm_dt is an export,
  // which is filled via kn[i] = \sum_j b[j]*A[j][i] 

  void AdjustState
  (OC_REAL8m hstep,
   OC_REAL8m mstep,
   const Oxs_SimState& old_state,
   const Oxs_MeshValue<ThreeVector>& partial_sum,
   vector<OC_REAL8m>& b,
   vector<const Oxs_MeshValue<ThreeVector>*>& A,
   Oxs_SimState& new_state,
   OC_REAL8m& norm_error) const;
  // Same as preceding AdjustState, except that kn is not
  // exported, but is computed for internal use and computed
  // as
  //     kn[i] = partial_sum[i] + \sum_j b[j]*A[j][i] 

#endif // OOMMF_THREADS


  void UpdateTimeFields(const Oxs_SimState& cstate,
			Oxs_SimState& nstate,
			OC_REAL8m stepsize) const;

  void NegotiateTimeStep(const Oxs_TimeDriver* driver,
			 const Oxs_SimState&  cstate,
			 Oxs_SimState& nstate,
			 OC_REAL8m stepsize,
			 OC_BOOL use_start_cond,
			 OC_BOOL& forcestep,
			 OC_BOOL& driver_set_step) const;

  OC_BOOL CheckError(OC_REAL8m global_error_order,OC_REAL8m error,
		  OC_REAL8m stepsize,OC_REAL8m reference_stepsize,
		  OC_REAL8m max_dm_dt,OC_REAL8m& new_stepsize);
  /// Returns 1 if step is good, 0 if error is too large.
  /// Export new_stepsize is set to suggested stepsize
  /// for next step.

  OC_REAL8m MaxDiff(const Oxs_MeshValue<ThreeVector>& vecA,
		 const Oxs_MeshValue<ThreeVector>& vecB);
  /// Returns maximum difference between vectors in corresponding
  /// positions in two vector fields.

  void AdjustStepHeadroom(OC_INT4m step_reject);
  /// step_reject should be 0 or 1, reflecting whether the current
  /// step was rejected or not.  This routine updates reject_ratio
  /// and adjusts step_headroom appropriately.

  void ComputeEnergyChange(const Oxs_Mesh* mesh,
                           const Oxs_MeshValue<OC_REAL8m>& current_energy,
                           const Oxs_MeshValue<OC_REAL8m>& candidate_energy,
                           OC_REAL8m& dE,OC_REAL8m& var_dE,OC_REAL8m& total_E);
  /// Computes cellwise difference between energies, and variance.
  /// Export total_E is "current" energy (used for stepsize control).


  // Stepper routines:  If routine needs to compute the energy
  // at the new (final) state, then it should store the final
  // energy results in temp_energy, mxH in mxH_output.cache,
  // and dm_dt into the vtmpA scratch array, fill
  // the "Timestep lower bound", "Max dm/dt", "dE/dt", and
  // "pE/pt" derived data fields in nstate, and set the export
  // value new_energy_and_dmdt_computed true.  Otherwise the export
  // value should be set false, and the client routine is responsible
  // for obtaining these values as necessary.  (If possible, it is
  // better to let the client compute these values, because the
  // client may be able to defer computation until it has decided
  // whether or not to keep the step.)

  // One would like to declare the step functions and pointer
  // to same via typedef's, but the MS VC++ 6.0 (& others?)
  // compiler doesn't handle member function typedef's properly---
  // it produces __cdecl linkage rather than instance member
  // linkage.  Typedef's on pointers to member functions work
  // okay, just not typedef's on member functions themselves.
  // So, instead we use a #define, which is ugly but portable.
#define RKStepFuncSig(NAME) \
  void NAME (                                            \
     OC_REAL8m stepsize,                                    \
     Oxs_ConstKey<Oxs_SimState> current_state,           \
     const Oxs_MeshValue<ThreeVector>& current_dm_dt,    \
     Oxs_Key<Oxs_SimState>& next_state,                  \
     OC_REAL8m& error_estimate,                             \
     OC_REAL8m& global_error_order,                         \
     OC_REAL8m& norm_error,                                 \
     OC_BOOL& new_energy_and_dmdt_computed)

  // Functions that calculate a single RK step
  RKStepFuncSig(TakeRungeKuttaStep2);
  RKStepFuncSig(TakeRungeKuttaStep2Heun);
  RKStepFuncSig(TakeRungeKuttaStep4);
  RKStepFuncSig(TakeRungeKuttaFehlbergStep54);
  RKStepFuncSig(TakeRungeKuttaFehlbergStep54M);
  RKStepFuncSig(TakeRungeKuttaFehlbergStep54S);

  // Pointer set at runtime during instance initialization
  // to one of the above functions single RK step functions.
  RKStepFuncSig((Oxs_RungeKuttaEvolve::* rkstep_ptr));

  // Utility code used by the TakeRungeKuttaFehlbergStep54* routines.
  enum RKF_SubType { RKF_INVALID, RK547FC, RK547FM, RK547FS };
  void RungeKuttaFehlbergBase54(RKF_SubType method,
			   OC_REAL8m stepsize,
			   Oxs_ConstKey<Oxs_SimState> current_state,
			   const Oxs_MeshValue<ThreeVector>& current_dm_dt,
			   Oxs_Key<Oxs_SimState>& next_state,
			   OC_REAL8m& error_estimate,
			   OC_REAL8m& global_error_order,
			   OC_REAL8m& norm_error,
			   OC_BOOL& new_energy_and_dmdt_computed);

  static OC_REAL8m PositiveTimestepBound(OC_REAL8m max_dm_dt);
  // Computes estimate on minimal timestep that will move at least one
  // spin an amount perceptible to the floating point representation.

  void Calculate_dm_dt
  (const Oxs_SimState& state_,
   const Oxs_MeshValue<ThreeVector>& mxH_,
   OC_REAL8m pE_pt_,
   Oxs_MeshValue<ThreeVector>& dm_dt_,
   OC_REAL8m& max_dm_dt_,OC_REAL8m& dE_dt_,OC_REAL8m& min_timestep_);
  /// Imports: state_, mxH_, pE_pt
  /// Exports: dm_dt_, max_dm_dt_, dE_dt_, min_timestep_

#if OOMMF_THREADS
  // Thread-friendly version of Calculate_dm_dt breaks into
  // three parts: the Initialize and Finalize members here
  // and the Cmd member of _Oxs_RKEvolve_RKFBase54_ThreadB
  // (which is defined in rungekuttaevolve.cc).  Used in
  // ::RungeKuttaFehlbergBase54().
  void Initialize_Threaded_Calculate_dm_dt
  (const Oxs_SimState& base_state, // Import
   Oxs_SimState& work_state,       // Import and export
   const Oxs_MeshValue<ThreeVector>& mxH, // Import
   Oxs_MeshValue<ThreeVector>& dm_dt,     // Export; may be same as mxH
   _Oxs_RKEvolve_RKFBase54_ThreadB& thread_data, // Export
   OC_REAL8m mstep,
   OC_REAL8m b_dm_dt,
   const vector<OC_REAL8m>& b,
   vector<const Oxs_MeshValue<ThreeVector>*>& A,
   Oxs_MeshValue<ThreeVector>& kn); // Export kn = b*A
  /// Also sets spins in work_state to base_state.spin + mstep*(b*A)

  void Finalize_Threaded_Calculate_dm_dt
  (const Oxs_SimState& base_state, // Import
   const _Oxs_RKEvolve_RKFBase54_ThreadB& thread_data, // Import
   OC_REAL8m pE_pt,             // Import
   OC_REAL8m hstep,             // Import
   Oxs_SimState& work_state, // Export
   OC_REAL8m& max_dm_dt,        // Export
   OC_REAL8m& dE_dt,            // Export
   OC_REAL8m& min_timestep_export,  // Export
   OC_REAL8m& norm_error);      // Export
#endif // OOMMF_THREADS

#if OOMMF_THREADS
  // Thread-friendly version of Calculate_dm_dt breaks into
  // three parts: the Initialize and Finalize members here
  // and the Cmd member of _Oxs_RKEvolve_RKFBase54_ThreadC
  // (which is defined in rungekuttaevolve.cc).  Used in
  // ::RungeKuttaFehlbergBase54().
  void Initialize_Threaded_Calculate_dm_dt
  (const Oxs_SimState& base_state, // Import
   Oxs_SimState& work_state,       // Import and export
   const Oxs_MeshValue<ThreeVector>& mxH, // Import
   Oxs_MeshValue<ThreeVector>& dm_dt,     // Export; may be same as mxH
   _Oxs_RKEvolve_RKFBase54_ThreadC& thread_data, // Export
   OC_REAL8m mstep,
   OC_REAL8m b1_dm_dt,
   OC_REAL8m b2_dm_dt,
   const vector<OC_REAL8m>& b1,
   const vector<OC_REAL8m>& b2,
   vector<const Oxs_MeshValue<ThreeVector>*>& A,
   Oxs_MeshValue<ThreeVector>& kn); // Export kn = b2*A
  /// Also sets spins in work_state to base_state.spin + mstep*(b1*A)

  void Finalize_Threaded_Calculate_dm_dt
  (const Oxs_SimState& base_state, // Import
   const _Oxs_RKEvolve_RKFBase54_ThreadC& thread_data, // Import
   OC_REAL8m pE_pt,             // Import
   OC_REAL8m hstep,             // Import
   Oxs_SimState& work_state, // Export
   OC_REAL8m& max_dm_dt,        // Export
   OC_REAL8m& dE_dt,            // Export
   OC_REAL8m& min_timestep_export,  // Export
   OC_REAL8m& norm_error);      // Export
#endif // OOMMF_THREADS

#if OOMMF_THREADS
  // Thread-friendly version of Calculate_dm_dt breaks into
  // three parts: the Initialize and Finalize members here
  // and the Cmd member of _Oxs_RKEvolve_RKFBase54_ThreadD
  // (which is defined in rungekuttaevolve.cc).  Used in
  // ::RungeKuttaFehlbergBase54().
  void Initialize_Threaded_Calculate_dm_dt
  (const Oxs_SimState& work_state,        // Import
   const Oxs_MeshValue<ThreeVector>& mxH, // Import
   Oxs_MeshValue<ThreeVector>& dm_dt,     // Export
   _Oxs_RKEvolve_RKFBase54_ThreadD& thread_data, // Import and export
   OC_REAL8m dc7, // Import
   const Oxs_MeshValue<ThreeVector>& dD13456); // Import

  void Finalize_Threaded_Calculate_dm_dt
  (const _Oxs_RKEvolve_RKFBase54_ThreadD& thread_data, // Import
   OC_REAL8m pE_pt,          // Import
   OC_REAL8m& max_dm_dt,      // Export
   OC_REAL8m& dE_dt,          // Export
   OC_REAL8m& min_timestep_export,  // Export
   OC_REAL8m& max_dD_magsq);  // Export
#endif // OOMMF_THREADS

  // Declare but leave undefined copy constructor and assignment operator
  Oxs_RungeKuttaEvolve(const Oxs_RungeKuttaEvolve&);
  Oxs_RungeKuttaEvolve& operator=(const Oxs_RungeKuttaEvolve&);

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  virtual OC_BOOL Init();
  Oxs_RungeKuttaEvolve(const char* name,     // Child instance id
		       Oxs_Director* newdtr, // App director
		       const char* argstr);  // MIF input block parameters
  virtual ~Oxs_RungeKuttaEvolve();

  virtual OC_BOOL
  InitNewStage(const Oxs_TimeDriver* driver,
               Oxs_ConstKey<Oxs_SimState> state,
               Oxs_ConstKey<Oxs_SimState> prevstate);

  virtual  OC_BOOL
  Step(const Oxs_TimeDriver* driver,
       Oxs_ConstKey<Oxs_SimState> current_state,
       const Oxs_DriverStepInfo& step_info,
       Oxs_Key<Oxs_SimState>& next_state);
  // Returns true if step was successful, false if
  // unable to step as requested.
};

#endif // _OXS_RUNGEKUTTAEVOLVE
