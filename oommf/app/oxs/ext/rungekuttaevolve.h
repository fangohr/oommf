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
#include "rectangularmesh.h"
#include "meshvalue.h"
#include "scalarfield.h"
#include "output.h"

/* End includes */

#define REPORT_TIME_RKDEVEL 0

#if REPORT_TIME
# ifndef REPORT_TIME_RKDEVEL
#  define REPORT_TIME_RKDEVEL 1
# endif
#endif
#ifndef REPORT_TIME_RKDEVEL
# define REPORT_TIME_RKDEVEL 0
#endif

// Zhang damping is a damping term added to LLG that depends directly on
// the local magnetization configuration.  See
//
//   Shufeng Zhang and Steven S.-L. Zhang, "Generalization of the
//   Landau-Lifshitz-Gilbert equation for conducting ferromagnetics,"
//   Physical Review Letters, 102, 086601 (2009).
//
// The above paper provides implicit formula using the Gilbert
// formulation of LLG.  The OOMMF code implements an explicit version
// that differs from the Zhang formula by terms of order alpha.|D| and
// |D|^2m where D is the differential operator defined in the above PRL
// paper.  See NOTES VI, 1-June-2012, pp 40-41 for details, and also
// NOTES VII, 18-Apr-2016, pp 130-131 for information on computing
// dE/dt.

// Tserkovnyak damping is a damping term added to LLG that depends directly on
// the local magnetization configuration.  See
//
//   Y. Tserkovnyak, E.M. Hankiewicz, G. Vignale, "Transverse spin
//   diffusion in ferromagnets," Physical Review B, 79, 094415 (2009).
//
// NOT IMPLEMENTED

// Baryakhtar damping, see
//
//   "Phenomenological description of the nonlocal magnetization
//   relaxation in magnonics, spintronics, and domain-wall dynamics,"
//   Weiwei Wang, Mykola Dvornik, Marc-Antonio Bisotti, Dmitri
//   Chernyshenko, Marijan Beg, Maximilian Albert, Arne Vansteenkiste,
//   Bartel V. Waeyenberge, Andriy N. Kuchko, Volodymyr V. Kruglyak, and
//   Hans Fangohr, Physical Review B, 92, 054430 (10 pages) (2015).
//
// Refer in particular to equations (17) and (18).  See also NOTES VII,
// 18-Apr-2016, pp 130-131.

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
  // Three convenience functions
  inline void RKTIME_START(int i) {
    timer[i].Start();
  }
  inline void RKTIME_STOP(int i,const char* name) {
    timer[i].Stop();
    ++timer_counts[i].pass_count;
    timer_counts[i].name = name;
  }
  inline void RKTIME_STOP(int i,const char* name,OC_INDEX bytes) {
    timer[i].Stop();
    ++timer_counts[i].pass_count;
    timer_counts[i].name = name;
    timer_counts[i].bytes += bytes;
  }
#else
  inline void RKTIME_START(int) {}
  inline void RKTIME_STOP(int,const char*) {}
  inline void RKTIME_STOP(int,const char*,OC_INDEX) {}
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


  // DMDT_STANDARD is standard LLG; others are damping variants
  enum DmDtStyle { DMDT_INVALID, DMDT_STANDARD, DMDT_ZHANG, DMDT_BARYAKHTAR };
  DmDtStyle dmdt_style;

  // Variant damping support member variables mesh_* are initialized
  // inside the ::UpdateMeshArrays member function.  These require a
  // rectangular mesh.
  OC_INDEX mesh_ispan, mesh_jspan, mesh_kspan;

  // Zhang damping
  Oxs_OwnedPointer<Oxs_ScalarField> zhang_damping_init;
  mutable Oxs_MeshValue<OC_REAL8m> zhang_damping;
  mutable Oxs_MeshValue<ThreeVector> zhang_delm; // Array to hold
  /// Zhang delm results.  This is needed because state advance
  /// code may overwrite old spin data on the fly.

  // Baryakhtar damping
  Oxs_OwnedPointer<Oxs_ScalarField> baryakhtar_sigma_init;
  mutable Oxs_MeshValue<OC_REAL8m> baryakhtar_sigma;
  mutable Oxs_MeshValue<ThreeVector> D2Hperp; // Array to hold
  /// Laplacian Hperp for in-progress dm/dt computation.
  OC_REAL8m mesh_delx, mesh_dely, mesh_delz;

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
		   Oxs_SimState& new_state) const;
  // Export new state has time index from old_state + h,
  // and spins from old state + mstep*dm_dt and re-normalized.

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
#define RKStepFuncSig(NAME)                              \
  void NAME (                                            \
     OC_REAL8m stepsize,                                 \
     Oxs_ConstKey<Oxs_SimState> current_state,           \
     const Oxs_MeshValue<ThreeVector>& current_dm_dt,    \
     Oxs_Key<Oxs_SimState>& next_state,                  \
     OC_REAL8m& error_estimate,                          \
     OC_REAL8m& global_error_order,                      \
     OC_REAL8m& norm_error,                              \
     OC_BOOL& new_energy_and_dmdt_computed)

  // Functions that calculate a single RK step
  template <typename DMDT> RKStepFuncSig(TakeRungeKuttaStep2);
  template <typename DMDT> RKStepFuncSig(TakeRungeKuttaStep2Heun);
  template <typename DMDT> RKStepFuncSig(TakeRungeKuttaStep4);
  template <typename DMDT> RKStepFuncSig(TakeRungeKuttaFehlbergStep54);
  template <typename DMDT> RKStepFuncSig(TakeRungeKuttaFehlbergStep54M);
  template <typename DMDT> RKStepFuncSig(TakeRungeKuttaFehlbergStep54S);

  // Pointer set at runtime during instance initialization
  // to one of the above functions single RK step functions.
  RKStepFuncSig((Oxs_RungeKuttaEvolve::* rkstep_ptr));

  // Utility code used by the TakeRungeKuttaFehlbergStep54* routines.
  enum RKF_SubType { RKF_INVALID, RK547FC, RK547FM, RK547FS };
  template <typename DMDT>
  void RungeKuttaFehlbergBase54(RKF_SubType method,
			   OC_REAL8m stepsize,
			   Oxs_ConstKey<Oxs_SimState> current_state,
			   const Oxs_MeshValue<ThreeVector>& current_dm_dt,
			   Oxs_Key<Oxs_SimState>& next_state,
			   OC_REAL8m& error_estimate,
			   OC_REAL8m& global_error_order,
			   OC_REAL8m& norm_error,
			   OC_BOOL& new_energy_and_dmdt_computed);

  OC_REAL8m PositiveTimestepBound(OC_REAL8m max_dm_dt);
  // Computes estimate on minimal timestep that will move at least one
  // spin an amount perceptible to the floating point representation.

  template <typename DMDT> void Calculate_dm_dt
  (const Oxs_SimState& state_,
   const Oxs_MeshValue<ThreeVector>& mxH_,
   OC_REAL8m pE_pt_,
   Oxs_MeshValue<ThreeVector>& dm_dt_,
   OC_REAL8m& max_dm_dt_,OC_REAL8m& dE_dt_,OC_REAL8m& min_timestep_);
  /// Imports: state_, mxH_, pE_pt
  /// Exports: dm_dt_, max_dm_dt_, dE_dt_, min_timestep_

  // Member function pointer to one of the templated Calculate_dm_dt
  // member functions
  void (Oxs_RungeKuttaEvolve::* calculate_dm_dt_ptr)
  (const Oxs_SimState&,const Oxs_MeshValue<ThreeVector>&,OC_REAL8m,
   Oxs_MeshValue<ThreeVector>&,OC_REAL8m&,OC_REAL8m&,OC_REAL8m&);

  // Initialize rkstep_ptr and calculate_dm_dt_ptr
  template <typename DMDT> void AssignFuncPtrs(String rk_method);

  // Declare but leave undefined copy constructor and assignment operator
  Oxs_RungeKuttaEvolve(const Oxs_RungeKuttaEvolve&);
  Oxs_RungeKuttaEvolve& operator=(const Oxs_RungeKuttaEvolve&);

  //////////////////////////////////////////////////////////////////////
  // Templated versions of dm_dt + state advance code
  template <typename DMDT,typename ADVANCE>
  void Compute_dm_dt_core_and_Advance(const Oxs_SimState* workstate,
                                      DMDT& dmdt,
                                      ADVANCE& advance)
  {
    Compute_dm_dt_core_and_Advance_helper<DMDT,ADVANCE>
      (workstate,dmdt,advance);
  }

  template <typename DMDT,typename ADVANCE>
  void Compute_dm_dt_and_Advance(const Oxs_SimState* workstate,
                                 DMDT& dmdt,
                                 ADVANCE& advance,
                                 OC_REAL8m& max_dm_dt,
                                 OC_REAL8m& pE_pM_sum)
  {
    Compute_dm_dt_and_Advance_helper<DMDT,ADVANCE>
      (workstate,dmdt,advance,max_dm_dt,pE_pM_sum);
  }

  // Helper classes for standard DMDT damping //////////////////////////
  template <typename DMDT,typename ADVANCE>
  class Compute_dm_dt_core_and_Advance_helper {
    // Helper class supports partial specialization.  (Template
    // functions in C++ don't support partial specialization.)
  public:
    Compute_dm_dt_core_and_Advance_helper
      (const Oxs_SimState* workstate,
       DMDT& dmdt,
       ADVANCE& advance)
    {
      const OC_INDEX meshsize = workstate->mesh->Size();
      dmdt.InitializeCore();
      advance.Initialize();
      for(OC_INDEX i=0;i<meshsize;++i) {
        dmdt.ComputeCore(i);
        advance.AdvanceSpins(i);
      }
      dmdt.FinalizeCore();
      advance.Finalize();
    }
  private:
    // Disable default copy and assignment members
    Compute_dm_dt_core_and_Advance_helper
      (Compute_dm_dt_core_and_Advance_helper const&);
    Compute_dm_dt_core_and_Advance_helper&
      operator=(Compute_dm_dt_core_and_Advance_helper const&);
  };

  template <typename DMDT,typename ADVANCE>
  class Compute_dm_dt_and_Advance_helper {
    // Helper class supports partial specialization.  (Template
    // functions in C++ don't support partial specialization.)
  public:
    Compute_dm_dt_and_Advance_helper
     (const Oxs_SimState* workstate,
      DMDT& dmdt,
      ADVANCE& advance,
      OC_REAL8m& max_dm_dt,
      OC_REAL8m& pE_pM_sum)
    {
      const OC_INDEX meshsize = workstate->mesh->Size();
      dmdt.Initialize();
      advance.Initialize();
      for(OC_INDEX i=0;i<meshsize;++i) {
        dmdt.Compute(i,max_dm_dt,pE_pM_sum);
        advance.AdvanceSpins(i);
      }
      dmdt.Finalize(max_dm_dt,pE_pM_sum);
      advance.Finalize();
    }
  private:
    // Disable default copy and assignment members
    Compute_dm_dt_and_Advance_helper
      (Compute_dm_dt_and_Advance_helper const&);
    Compute_dm_dt_and_Advance_helper&
      operator=(Compute_dm_dt_and_Advance_helper const&);
  };

  class Standard_dmdt {
  public:
    Standard_dmdt(const Oxs_RungeKuttaEvolve* rke,
                  const Oxs_SimState& work_state,
                  const Oxs_MeshValue<ThreeVector>& import_mxH,
                  Oxs_MeshValue<ThreeVector>& export_dm_dt)
      // NOTE: import_mxH and import_dm_dt may be same array.
      :  mesh(work_state.mesh),
         gamma(rke->gamma), alpha(rke->alpha), Ms(*(work_state.Ms)),
         spin(work_state.spin),
         mxH(import_mxH), dm_dt(export_dm_dt),
         do_precess(rke->do_precess)
    {}


    void InitializeCore() { dm_dt.AdjustSize(mesh); }

    inline void ComputeCore(OC_INDEX j) {
      // Doesn't compute max_dm_dt or pE_pM
      if(Ms[j]==0) {
        dm_dt[j].Set(0.0,0.0,0.0);
      } else {
        // Note: mxH may be same as dm_dt
        ThreeVector scratch_precess
          = (do_precess ? mxH[j] : ThreeVector(0,0,0));
        ThreeVector scratch_damp = spin[j];
        scratch_damp ^= mxH[j]; // (mx(mxH))
        scratch_precess.Accum(alpha[j],scratch_damp); // mxH + alpha.(mx(mxH))

        // Adding directly into mxH may be slightly more stable than
        // multiplying gamma.mxH first and then summing.
        dm_dt[j] = gamma[j]*scratch_precess;
      }
    }

    void FinalizeCore() {}

    void Initialize() { dm_dt.AdjustSize(mesh); }

    inline void Compute(OC_INDEX j,
                        OC_REAL8m& max_dm_dt_sq,
                        OC_REAL8m& pE_pM_sum) {
      if(Ms[j]==0) {
        dm_dt[j].Set(0.0,0.0,0.0);
      } else {
        // Note: mxH may be same as dm_dt
        const ThreeVector torque = mxH[j];
        ThreeVector scratch_precess
          = (do_precess ? mxH[j] : ThreeVector(0,0,0));
        OC_REAL8m dm_dt_sq = (do_precess ? 1.0 : 0.0);

        const OC_REAL8m a = alpha[j];
        dm_dt_sq += a*a;
        OC_REAL8m mxH_sq = torque.MagSq();

        const OC_REAL8m g = gamma[j];
        dm_dt_sq *= mxH_sq * g * g;
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
        pE_pM_sum -= mxH_sq * Ms[j] * mesh->Volume(j) * a * g;

        ThreeVector scratch_damp = spin[j];
        scratch_damp ^= torque; // (mx(mxH))
        scratch_precess.Accum(a,scratch_damp); // mxH + alpha.(mx(mxH))

        // Adding directly into mxH may be slightly more stable than
        // multiplying gamma.mxH first and then summing.
        dm_dt[j] = g*scratch_precess;
      }
    }

    void Finalize(OC_REAL8m& /* max_dm_dt_sq */,OC_REAL8m& pE_pM_sum) {
      pE_pM_sum *= -1*MU0;
    }

  private:
    const Oxs_Mesh* mesh;
    const Oxs_MeshValue<OC_REAL8m>& gamma;
    const Oxs_MeshValue<OC_REAL8m>& alpha;
    const Oxs_MeshValue<OC_REAL8m>& Ms;
    const Oxs_MeshValue<ThreeVector>& spin;
    const Oxs_MeshValue<ThreeVector>& mxH;
    Oxs_MeshValue<ThreeVector>& dm_dt;
    const OC_BOOL do_precess;

    // Disable default copy and assignment members
    Standard_dmdt(Standard_dmdt const&);
    Standard_dmdt& operator=(Standard_dmdt const&);
  };


  // Baryakhtar damping; see
  //
  //   "Phenomenological description of the nonlocal magnetization
  //   relaxation in magnonics, spintronics, and domain-wall dynamics,"
  //   Weiwei Wang, Mykola Dvornik, Marc-Antonio Bisotti, Dmitri
  //   Chernyshenko, Marijan Beg, Maximilian Albert, Arne Vansteenkiste,
  //   Bartel V. Waeyenberge, Andriy N. Kuchko, Volodymyr V. Kruglyak, and
  //   Hans Fangohr, Physical Review B, 92, 054430 (10 pages) (2015).
  //
  // Refer in particular to equations (17) and (18).
  //
  // This code requires a rectangular mesh
  //
  // If mxH and dm_dt arrays are the same (likely), then as dm_dt is
  // filled mxH is destroyed.  But we need mxH data to compute
  // Laplacian Hperp.  In principle, one could cache a few rows of
  // mxH before those rows are overwritten with dm_dt, but for a first
  // draft we just compute the entire Laplacian Hperp up front and store
  // the results in the D2Hperp array.
  class Baryakhtar_dmdt {
  public:
    Baryakhtar_dmdt(const Oxs_RungeKuttaEvolve* rke,
                  const Oxs_SimState& work_state,
                  const Oxs_MeshValue<ThreeVector>& import_mxH,
                  Oxs_MeshValue<ThreeVector>& import_dm_dt)
      // NOTE: import_mxH and import_dm_dt may be same array.
      :  mesh(work_state.mesh),
         gamma(rke->gamma), alpha(rke->alpha),
         baryakhtar_sigma(rke->baryakhtar_sigma),
         Ms(*(work_state.Ms)),
         spin(work_state.spin),
         mxH(import_mxH), dm_dt(import_dm_dt),
         D2Hperp(rke->D2Hperp),
         do_precess(rke->do_precess)
    {}

    // Code to fill the internal D2Hperp array for Baryakhtar damping
    void Compute_LaplacianHperpChunk(OC_INT4m /* thread_number*/,
                                     OC_INDEX nstart,OC_INDEX nstop)
    { // Note: The code below assumes mxHxm = Hperp = H - <H,m>m/<m,m>,
      //       which relies on |m|=1.
      if(nstop<=nstart) return; // Nothing to do

      assert(D2Hperp.CheckMesh(mesh));

      const Oxs_CommonRectangularMesh* rectmesh
        = dynamic_cast<const Oxs_CommonRectangularMesh*>(mesh);

      const OC_REAL8m mesh_delx_inverse_sq
        = 1.0/(rectmesh->EdgeLengthX()*rectmesh->EdgeLengthX());
      const OC_REAL8m mesh_dely_inverse_sq
        = 1.0/(rectmesh->EdgeLengthY()*rectmesh->EdgeLengthY());
      const OC_REAL8m mesh_delz_inverse_sq
        = 1.0/(rectmesh->EdgeLengthZ()*rectmesh->EdgeLengthZ());

      Oxs_RectangularMeshCoords coords(rectmesh); // Local copy
      const OC_INDEX& mesh_xspan = coords.xdim;   // Convenience labels
      const OC_INDEX& mesh_yspan = coords.ydim;
      const OC_INDEX& mesh_zspan = coords.zdim;
      const OC_INDEX& yoff = coords.xdim;
      const OC_INDEX& zoff = coords.xydim;
      OC_INDEX& mi = coords.x;
      OC_INDEX& mj = coords.y;
      OC_INDEX& mk = coords.z;
      OC_INDEX&  n = coords.index;

      assert(0<=nstart && nstop<=mesh_xspan*mesh_yspan*mesh_zspan);

      coords.SetIndex(nstart);
      do {
        // At interior points Laplacian H is computed using three point,
        // second order (in cell size h) discrete approximations to
        // second derivatives.  These data are not available for cells
        // on part boundaries --- the three point stencil falls off the
        // edge of the part.  The best three point estimate is to use
        // the estimate from the point next in; this estimate is first
        // order in h.  Empirically, however, we find that except for
        // rectangular shapes, using that estimate leads the time step
        // size control to collapse the time step so severely that
        // forward progress halts.  OTOH, one can instead just record
        // affected second derivatives as 0.0.  In the various tests I
        // have run (see oommf/app/examples/baryakhtar.mif with
        // different "shape" options) the overall result does not appear
        // to be affected.  This is the approach provided in the first
        // #if block below.  Alternatively, one can record a scaled-down
        // value of the first order estimate (i.e., a convex combination
        // of the first order estimate with zero).  This method is
        // provided in the second #if block, where the constant
        // "edge_scale" controls the scaling.  For elliptical shapes
        // edge_scale <= 0.125 does not appear to suffer stepsize
        // collapse, but edge_scale >= 0.1875 does.  The more extreme
        // "sphere" shape in baryakhtar.mif, which is designed to have a
        // single cell protrude along the axis-parallel diameters,
        // suffers cell size collapse at edge_scale = 0.125, but
        // apparently not at edge_scale = 0.0625.  The latter case was
        // run for simulation time = 4 ns, and compared against the
        // first #if block results (effectively edge_scale = 0.0).  The
        // my component values at t = 4 ns agreed through 4 decimal
        // places.
        if(Ms[n]==0.0) {
          D2Hperp[n].Set(0.,0.,0.);
        } else {
          // Compute Laplacian Hperp at index n
          ThreeVector Hperp = mxH[n];
          Hperp ^= spin[n];
#if 1 // d^2 H/dn^2 set to 0 along bdry
          ThreeVector d2H_dx2;
          if(mi>0 && Ms[n-1]!=0.0 && mi+1<mesh_xspan && Ms[n+1]!=0.0) {
            // Only compute d2H_dx2 if data is available on both sides
            d2H_dx2 = (mxH[n+1] ^ spin[n+1])
              + (mxH[n-1] ^ spin[n-1]) - 2*Hperp;
            d2H_dx2 *= mesh_delx_inverse_sq;
          }

          ThreeVector d2H_dy2;
          if(mj>0 && Ms[n-yoff]!=0.0 && mj+1<mesh_yspan && Ms[n+yoff]!=0.0) {
            d2H_dy2 = (mxH[n+yoff] ^ spin[n+yoff])
              + (mxH[n-yoff] ^ spin[n-yoff]) - 2*Hperp;
            d2H_dy2 *= mesh_dely_inverse_sq;
          }

          ThreeVector d2H_dz2;
          if(mk>0 && Ms[n-zoff]!=0.0 && mk+1<mesh_zspan && Ms[n+zoff]!=0.0) {
            d2H_dz2 = (mxH[n+zoff] ^ spin[n+zoff])
              + (mxH[n-zoff] ^ spin[n-zoff]) - 2*Hperp;
            d2H_dz2 *= mesh_delz_inverse_sq;
          }
#else // Along bdry, d^2 H/dn^2 set to downscaled value (edge_scale)
      // of that computed one cell in from boundary.
          const OC_REAL8m edge_scale = 0.0625;
          ThreeVector d2H_dx2;
          if(mi>0 && Ms[n-1]!=0.0) {
            // Left side available
            if(mi+1<mesh_xspan && Ms[n+1]!=0.0) {
              // Right side available.  This is usual case
              d2H_dx2 = (mxH[n+1] ^ spin[n+1])
                + (mxH[n-1] ^ spin[n-1]) - 2*Hperp;
            } else {
              // Left side is available but right is not.  If n-2 is available
              // then use O(h) estimate to dx2
              if(mi>1 && Ms[n-2]!=0.0) {
                d2H_dx2 = (mxH[n-2] ^ spin[n-2]) + Hperp
                  - 2*(mxH[n-1] ^ spin[n-1]);
                d2H_dx2 *= edge_scale;
              }
            }
          } else {
            // Left side is not available.  If n+1 and n+2 are available
            // then use O(h) estimate to dx2
            if(mi+2<mesh_xspan && Ms[n+1]!=0.0 && Ms[n+2]!=0.0) {
              d2H_dx2 = (mxH[n+2] ^ spin[n+2]) + Hperp
                - 2*(mxH[n+1] ^ spin[n+1]);
              d2H_dx2 *= edge_scale;
            }
          }
          d2H_dx2 *= mesh_delx_inverse_sq;

          ThreeVector d2H_dy2;
          if(mj>0 && Ms[n-yoff]!=0.0) {
            // "Left" side available
            if(mj+1<mesh_yspan && Ms[n+yoff]!=0.0) {
              // "Right" side available.  This is usual case
              d2H_dy2 = (mxH[n+yoff] ^ spin[n+yoff])
                + (mxH[n-yoff] ^ spin[n-yoff]) - 2*Hperp;
            } else {
              // Left side is available but right is not.  If n-2*yoff is
              // available then use O(h) estimate to dy2
              if(mj>1 && Ms[n-2*yoff]!=0.0) {
                d2H_dy2 = (mxH[n-2*yoff] ^ spin[n-2*yoff]) + Hperp
                  - 2*(mxH[n-yoff] ^ spin[n-yoff]);
                d2H_dy2 *= edge_scale;
              }
            }
          } else {
            // Left side is not available.  If n+yoff and n+2*yoff are
            // available then use O(h) estimate to dy2
            if(mj+2<mesh_yspan && Ms[n+yoff]!=0.0 && Ms[n+2*yoff]!=0.0) {
              d2H_dy2 = (mxH[n+2*yoff] ^ spin[n+2*yoff]) + Hperp
                - 2*(mxH[n+yoff] ^ spin[n+yoff]);
              d2H_dy2 *= edge_scale;
            }
          }
          d2H_dy2 *= mesh_dely_inverse_sq;

          ThreeVector d2H_dz2;
          if(mk>0 && Ms[n-zoff]!=0.0) {
            // "Left" side available
            if(mk+1<mesh_zspan && Ms[n+zoff]!=0.0) {
              // "Right" side available.  This is usual case
              d2H_dz2 = (mxH[n+zoff] ^ spin[n+zoff])
                + (mxH[n-zoff] ^ spin[n-zoff]) - 2*Hperp;
            } else {
              // Left side is available but right is not.  If n-2*zoff is
              // available then use O(h) estimate to dz2
              if(mk>1 && Ms[n-2*zoff]!=0.0) {
                d2H_dz2 = (mxH[n-2*zoff] ^ spin[n-2*zoff]) + Hperp
                  - 2*(mxH[n-zoff] ^ spin[n-zoff]);
                d2H_dz2 *= edge_scale;
              }
            }
          } else {
            // Left side is not available.  If n+zoff and n+2*zoff are
            // available then use O(h) estimate to dz2
            if(mk+2<mesh_zspan && Ms[n+zoff]!=0.0 && Ms[n+2*zoff]!=0.0) {
              d2H_dz2 = (mxH[n+2*zoff] ^ spin[n+2*zoff]) + Hperp
                - 2*(mxH[n+zoff] ^ spin[n+zoff]);
              d2H_dz2 *= edge_scale;
            }
          }
          d2H_dz2 *= mesh_delz_inverse_sq;
#endif
          D2Hperp[n] = d2H_dx2 + d2H_dy2 + d2H_dz2;
        }

      } while(coords.AdvanceIndex()<nstop);
    }

    void Compute_LaplacianHperp() {
      // The following call expands to multi-threaded
      // operation if OOMMF_THREADS macro is true.
      D2Hperp.AdjustSize(mesh);
      Oxs_RunMemberThreaded<Baryakhtar_dmdt,OC_REAL8m>
        (*this,&Baryakhtar_dmdt::Compute_LaplacianHperpChunk,Ms);
    }

    void InitializeCore() {
      dm_dt.AdjustSize(mesh); 
      Compute_LaplacianHperp();
    }

    inline void ComputeCore(OC_INDEX j) {
      // Doesn't compute max_dm_dt or pE_pM
      if(Ms[j]==0) {
        dm_dt[j].Set(0.0,0.0,0.0);
      } else {
        // Note: mxH may be same as dm_dt
        const ThreeVector torque = mxH[j];
        ThreeVector scratch_precess
          = (do_precess ? mxH[j] : ThreeVector(0,0,0));

        // Standard LLG damping
        ThreeVector scratch_damp = spin[j];
        scratch_damp ^= torque; // (mx(mxH))
        scratch_precess.Accum(alpha[j],scratch_damp); // mxH + alpha.(mx(mxH))

        // Baryakhtar damping term
        ThreeVector baryakhtar_damp = spin[j];
        baryakhtar_damp ^= D2Hperp[j];
        baryakhtar_damp ^= spin[j];
        scratch_precess.Accum(baryakhtar_sigma[j],baryakhtar_damp);
        /// mxH + alpha.(mx(mxH)) + sigma.(m x Hperp x m)

        /// NB: Computational cost may be reduced by integrating the
        /// Baryakhtar term more directly into the standard damping
        /// term, however the above approach may be more stable.

        dm_dt[j] = gamma[j]*scratch_precess;
      }
    }

    void FinalizeCore() {}

    void Initialize() {
      dm_dt.AdjustSize(mesh); 
      Compute_LaplacianHperp();
    }

    inline void Compute(OC_INDEX j,
                        OC_REAL8m& max_dm_dt_sq,
                        OC_REAL8m& pE_pM_sum) {
      if(Ms[j]==0) {
        dm_dt[j].Set(0.0,0.0,0.0);
      } else {
        // Note: mxH may be same as dm_dt
        const ThreeVector torque = mxH[j];
        ThreeVector scratch_precess
          = (do_precess ? mxH[j] : ThreeVector(0,0,0));

        const OC_REAL8m a = alpha[j];
        OC_REAL8m pE_pM = a * torque.MagSq();

        // Standard LLG damping
        ThreeVector scratch_damp = spin[j];
        scratch_damp ^= torque; // (mx(mxH))
        scratch_precess.Accum(a,scratch_damp); // mxH + alpha.(mx(mxH))

        // Baryakhtar damping term
        const OC_REAL8m sigma = baryakhtar_sigma[j];
        ThreeVector baryakhtar_damp = spin[j];
        baryakhtar_damp ^= D2Hperp[j]; // m x D2Hperp

        const OC_REAL8m g = gamma[j];
        pE_pM -= sigma*(baryakhtar_damp*torque);
        pE_pM_sum += g * Ms[j] * mesh->Volume(j) * pE_pM;

        baryakhtar_damp ^= spin[j];
        scratch_precess.Accum(sigma,baryakhtar_damp);
        /// mxH + alpha.(mx(mxH)) + sigma.(m x D2Hperp x m)

        /// NB: Computational cost may be reduced by integrating the
        /// Baryakhtar term more directly into the standard damping
        /// term, however the above approach may be more stable.

        // Adding directly into mxH may be slightly more stable than
        // multiplying gamma.mxH first and then summing.
        dm_dt[j] = g*scratch_precess;

        const OC_REAL8m dm_dt_sq = dm_dt[j].MagSq();
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }

      }
    }

    void Finalize(OC_REAL8m& /* max_dm_dt_sq */,OC_REAL8m& pE_pM_sum) {
      pE_pM_sum *= MU0;
    }

  private:
    const Oxs_Mesh* mesh;
    const Oxs_MeshValue<OC_REAL8m>& gamma;
    const Oxs_MeshValue<OC_REAL8m>& alpha;
    const Oxs_MeshValue<OC_REAL8m>& baryakhtar_sigma;
    const Oxs_MeshValue<OC_REAL8m>& Ms;
    const Oxs_MeshValue<ThreeVector>& spin;
    const Oxs_MeshValue<ThreeVector>& mxH;
    Oxs_MeshValue<ThreeVector>& dm_dt;
    Oxs_MeshValue<ThreeVector>& D2Hperp;
    const OC_BOOL do_precess;

    // Disable default copy and assignment members
    Baryakhtar_dmdt(Baryakhtar_dmdt const&);
    Baryakhtar_dmdt& operator=(Baryakhtar_dmdt const&);
  };


  // Zhang damping; see
  //
  //   Shufeng Zhang and Steven S.-L. Zhang, "Generalization of the
  //   Landau-Lifshitz-Gilbert equation for conducting ferromagnetics,"
  //   Physical Review Letters, 102, 086601 (2009).
  //
  // See also NOTES VI, 1-June-2012, pp 40-41 and NOTES VII,
  // 18-Apr-2016, pp 130-131.
  //
  // This code requires a rectangular mesh
  //
  // The state advance code may overwrite the state spin array on the
  // fly, which means that the delm computation buried within the Zhang
  // damping term must be computed upfront, before the state advance
  // code starts.  In principle, one could cache a few rows of old_spin
  // before those rows are overwritten with new_spin data, but for a
  // first draft we just do the entire Zhang delm computation up front
  // and store the results in the zhang_delm array.
  class Zhang_dmdt {
  public:
    Zhang_dmdt(const Oxs_RungeKuttaEvolve* rke,
               const Oxs_SimState& work_state,
               const Oxs_MeshValue<ThreeVector>& import_mxH,
               Oxs_MeshValue<ThreeVector>& export_dm_dt)
      // NOTE: import_mxH and import_dm_dt may be same array.
      :  mesh(work_state.mesh),
         gamma(rke->gamma),
         alpha(rke->alpha),
         zhang_damping(rke->zhang_damping),
         Ms(*(work_state.Ms)),
         spin(work_state.spin),
         mxH(import_mxH),
         zhang_delm(rke->zhang_delm),
         dm_dt(export_dm_dt),
         do_precess(rke->do_precess)
    {}

    // Code to fill the internal zhang_delm array (= D.(mxH)) for Zhang
    // damping.
    void Compute_ZhangDelmChunk(OC_INT4m /* thread_number*/,
                                OC_INDEX nstart,OC_INDEX nstop)
    {
      if(nstop<=nstart) return; // Nothing to do

      assert(zhang_delm.CheckMesh(mesh));

      const Oxs_CommonRectangularMesh* rectmesh
        = dynamic_cast<const Oxs_CommonRectangularMesh*>(mesh);

      const OC_REAL8m idelx = 1.0/rectmesh->EdgeLengthX(); // Local copies
      const OC_REAL8m idely = 1.0/rectmesh->EdgeLengthY();
      const OC_REAL8m idelz = 1.0/rectmesh->EdgeLengthZ();

      Oxs_RectangularMeshCoords coords(rectmesh); // Local copy
      const OC_INDEX& mesh_xspan = coords.xdim;   // Convenience labels
      const OC_INDEX& mesh_yspan = coords.ydim;
      const OC_INDEX& mesh_zspan = coords.zdim;
      const OC_INDEX& yoff = coords.xdim;
      const OC_INDEX& zoff = coords.xydim;
      OC_INDEX& mi = coords.x;
      OC_INDEX& mj = coords.y;
      OC_INDEX& mk = coords.z;
      OC_INDEX&  n = coords.index;

      assert(0<=nstart && nstop<=mesh_xspan*mesh_yspan*mesh_zspan);

      coords.SetIndex(nstart);
      do {
        if(Ms[n]==0.0) {
          zhang_delm[n].Set(0.,0.,0.);
        } else {

          // Compute dm/dx
          ThreeVector dm_dx;
          if(mi+1<mesh_xspan && Ms[n+1]!=0.0) dm_dx  = spin[n+1];
          else                                dm_dx  = spin[n];
          if(mi>0 && Ms[n-1]!=0.0)            dm_dx -= spin[n-1];
          else                                dm_dx -= spin[n];
          dm_dx *= 0.5*idelx;  // The 0.5 factor is correct in all cases;
          /// if mi is at an edge, then the dm_dn=0 boundary constraint is
          /// implemented by reflecting the edge spin across the boundary.  We
          /// might want to add some code here to check boundaries interior to
          /// the simulation, i.e., spots where Ms=0.
          dm_dx ^= spin[n];  // Prepare for Dab computation

          // Compute dm/dy
          ThreeVector dm_dy;
          if(mj+1<mesh_yspan && Ms[n+yoff]!=0.0) dm_dy  = spin[n+yoff];
          else                                   dm_dy  = spin[n];
          if(mj>0 && Ms[n-yoff]!=0.0)            dm_dy -= spin[n-yoff];
          else                                   dm_dy -= spin[n];
          dm_dy *= 0.5*idely;  // 0.5 factor is correct in all cases, as above.
          dm_dy ^= spin[n];  // Prepare for Dab computation

          // Compute dm/dz
          ThreeVector dm_dz;
          if(mk+1<mesh_zspan && Ms[n+zoff]!=0.0) dm_dz  = spin[n+zoff];
          else                                   dm_dz  = spin[n];
          if(mk>0 && Ms[n-zoff]!=0.0)            dm_dz -= spin[n-zoff];
          else                                   dm_dz -= spin[n];
          dm_dz *= 0.5*idelz;  // 0.5 factor is correct in all cases, as above.
          dm_dz ^= spin[n];  // Prepare for Dab computation

          ThreeVector D1(dm_dx.x*dm_dx.x + dm_dy.x*dm_dy.x + dm_dz.x*dm_dz.x,
                         dm_dx.x*dm_dx.y + dm_dy.x*dm_dy.y + dm_dz.x*dm_dz.y,
                         dm_dx.x*dm_dx.z + dm_dy.x*dm_dy.z + dm_dz.x*dm_dz.z);
          ThreeVector D2(D1.y,
                         dm_dx.y*dm_dx.y + dm_dy.y*dm_dy.y + dm_dz.y*dm_dz.y,
                         dm_dx.y*dm_dx.z + dm_dy.y*dm_dy.z + dm_dz.y*dm_dz.z);
          ThreeVector D3(D1.z,
                         D2.z,
                         dm_dx.z*dm_dx.z + dm_dy.z*dm_dy.z + dm_dz.z*dm_dz.z);

          zhang_delm[n].Set(D1*mxH[n],D2*mxH[n],D3*mxH[n]);
        }
      } while(coords.AdvanceIndex()<nstop);
    }

    void Compute_ZhangDelm() {
      // The following call expands to multi-threaded
      // operation if OOMMF_THREADS macro is true.
      zhang_delm.AdjustSize(mesh);
      Oxs_RunMemberThreaded<Zhang_dmdt,OC_REAL8m>
        (*this,&Zhang_dmdt::Compute_ZhangDelmChunk,Ms);
    }

    void InitializeCore() {
      dm_dt.AdjustSize(mesh); 
      Compute_ZhangDelm();
    }

    inline void ComputeCore(OC_INDEX j) {
      // Doesn't compute max_dm_dt or pE_pM
      if(Ms[j]==0) {
        dm_dt[j].Set(0.0,0.0,0.0);
      } else {
        // Note: mxH may be same as dm_dt
        const ThreeVector torque = mxH[j];
        ThreeVector scratch_precess
          = (do_precess ? mxH[j] : ThreeVector(0,0,0));

        // Standard LLG damping
        ThreeVector scratch_damp = spin[j];
        scratch_damp ^= torque; // (mx(mxH))
        scratch_precess.Accum(alpha[j],scratch_damp); // mxH + alpha.(mx(mxH))

        // Zhang damping term
        ThreeVector zhang_term = spin[j];
        zhang_term ^= zhang_delm[j];  // m x (D.(mxH))
        scratch_precess.Accum(zhang_damping[j],zhang_term);
        /// Computational cost may be reduced by one cross product by
        /// integrating the Zhang term more directly into the standard
        /// damping term, however this way may be more stable.

        // Adding directly into mxH may be slightly more stable than
        // multiplying gamma.mxH first and then summing.
        dm_dt[j] = gamma[j]*scratch_precess;
      }
    }

    void FinalizeCore() {}

    void Initialize() {
      dm_dt.AdjustSize(mesh); 
      Compute_ZhangDelm();
    }

    inline void Compute(OC_INDEX j,
                        OC_REAL8m& max_dm_dt_sq,
                        OC_REAL8m& pE_pM_sum) {
      if(Ms[j]==0) {
        dm_dt[j].Set(0.0,0.0,0.0);
      } else {
        // Note: mxH may be same as dm_dt
        const ThreeVector torque = mxH[j];
        ThreeVector scratch_precess
          = (do_precess ? mxH[j] : ThreeVector(0,0,0));

        const OC_REAL8m a = alpha[j];
        OC_REAL8m pE_pM = a * torque.MagSq();

        // Standard LLG damping
        ThreeVector scratch_damp = spin[j];
        scratch_damp ^= torque; // (mx(mxH))
        scratch_precess.Accum(a,scratch_damp); // mxH + alpha.(mx(mxH))

        // Zhang damping term
        const OC_REAL8m zd = zhang_damping[j];
        ThreeVector zhang_term = zhang_delm[j]; // D.(mxH)

        const OC_REAL8m g = gamma[j];
        pE_pM += zd*(zhang_term*torque);
        pE_pM_sum += g * Ms[j] * mesh->Volume(j) * pE_pM;

        zhang_term ^= spin[j];
        scratch_precess.Accum(-zd,zhang_term);
        /// Computational cost may be reduced by one cross product by
        /// integrating the Zhang term more directly into the standard
        /// damping term, however this way may be more stable.

        // Adding directly into mxH may be slightly more stable than
        // multiplying gamma.mxH first and then summing.
        dm_dt[j] = g*scratch_precess;

        const OC_REAL8m dm_dt_sq = dm_dt[j].MagSq();
        if(dm_dt_sq>max_dm_dt_sq) {
          max_dm_dt_sq = dm_dt_sq;
        }
      }
    }

    void Finalize(OC_REAL8m& /* max_dm_dt_sq */,OC_REAL8m& pE_pM_sum) {
      pE_pM_sum *= MU0;
    }

  private:
    const Oxs_Mesh* mesh;
    const Oxs_MeshValue<OC_REAL8m>& gamma;
    const Oxs_MeshValue<OC_REAL8m>& alpha;
    const Oxs_MeshValue<OC_REAL8m>& zhang_damping;
    const Oxs_MeshValue<OC_REAL8m>& Ms;
    const Oxs_MeshValue<ThreeVector>& spin;
    const Oxs_MeshValue<ThreeVector>& mxH;
    Oxs_MeshValue<ThreeVector>& zhang_delm;
    Oxs_MeshValue<ThreeVector>& dm_dt;
    const OC_BOOL do_precess;

    // Disable default copy and assignment members
    Zhang_dmdt(Zhang_dmdt const&);
    Zhang_dmdt& operator=(Zhang_dmdt const&);
  };



  class RKF54_AdvanceState_Base {
  public:
    RKF54_AdvanceState_Base
    (const Oxs_SimState& import_old_state,
     Oxs_SimState& import_new_state,
     const OC_REAL8m import_hstep,
     const OC_REAL8m import_mstep)
      : mstep(import_mstep),
        old_spin(import_old_state.spin),
        new_spin(import_new_state.spin),
        old_state(import_old_state),
        new_state(import_new_state),
        hstep(import_hstep)
    {}
    inline void Initialize() {
      // Note: Should be run by solitary thread
      new_state.ClearDerivedData();
      new_spin.AdjustSize(old_state.mesh);
    }
    inline void Finalize() {
      // Adjust time and iteration fields in new_state
      // Note: Should be run by solitary thread
      new_state.last_timestep=hstep;
      if(old_state.stage_number != new_state.stage_number) {
        // New stage
        new_state.stage_start_time = old_state.stage_start_time
          + old_state.stage_elapsed_time;
        new_state.stage_elapsed_time = new_state.last_timestep;
      } else {
        new_state.stage_start_time = old_state.stage_start_time;
        new_state.stage_elapsed_time = old_state.stage_elapsed_time
          + new_state.last_timestep;
      }
      // Don't touch iteration counts. (?!)  The problem is that one call
      // to Oxs_RungeKuttaEvolve::Step() takes 2 half-steps, and it is the
      // result from these half-steps that are used as the export state.
      // If we increment the iteration count each time through here, then
      // the iteration count goes up by 2 for each call to Step().  So
      // instead, we leave iteration count at whatever value was filled
      // in by the Oxs_RungeKuttaEvolve::NegotiateTimeStep() method.
    }

  protected:
    const OC_REAL8m mstep;
    const Oxs_MeshValue<ThreeVector>& old_spin;
    Oxs_MeshValue<ThreeVector>& new_spin;

  private:
    const Oxs_SimState& old_state;
    Oxs_SimState& new_state;
    const OC_REAL8m hstep;

    // Diable default copy and assignment members
    RKF54_AdvanceState_Base(RKF54_AdvanceState_Base const&);
    RKF54_AdvanceState_Base& operator=(RKF54_AdvanceState_Base const&);
  };


  class RKF54_AdvanceState_A : public RKF54_AdvanceState_Base {
  public:
    RKF54_AdvanceState_A
    (const Oxs_SimState& import_old_state,
     Oxs_SimState& import_new_state,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_A,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_B,
     const OC_REAL8m import_b21,
     const OC_REAL8m import_b22,
     const OC_REAL8m import_hstep,
     const OC_REAL8m import_mstep)
      : RKF54_AdvanceState_Base(import_old_state,
                                import_new_state,
                                import_hstep,import_mstep),
      dm_dt_A(import_dm_dt_A),
      dm_dt_B(import_dm_dt_B),
      b21(import_b21), b22(import_b22)
    {}
    inline void AdvanceSpins(OC_INDEX j) {
      ThreeVector vtmp = b21*dm_dt_A[j] + b22*dm_dt_B[j];
      vtmp *= mstep;
      vtmp += old_spin[j];
      vtmp.MakeUnit();
      new_spin[j] = vtmp;
    }
  private:
    const Oxs_MeshValue<ThreeVector>& dm_dt_A;
    const Oxs_MeshValue<ThreeVector>& dm_dt_B;
    const OC_REAL8m b21;
    const OC_REAL8m b22;

    // Diable default copy and assignment members
    RKF54_AdvanceState_A(RKF54_AdvanceState_A const&);
    RKF54_AdvanceState_A& operator=(RKF54_AdvanceState_A const&);
  };

  class RKF54_AdvanceState_B : public RKF54_AdvanceState_Base {
  public:
    RKF54_AdvanceState_B
    (const Oxs_SimState& import_old_state,
     Oxs_SimState& import_new_state,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_A,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_B,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_C,
     const OC_REAL8m import_b31,
     const OC_REAL8m import_b32,
     const OC_REAL8m import_b33,
     const OC_REAL8m import_hstep,
     const OC_REAL8m import_mstep)
      : RKF54_AdvanceState_Base(import_old_state,
                                import_new_state,
                                import_hstep,import_mstep),
        dm_dt_A(import_dm_dt_A),
        dm_dt_B(import_dm_dt_B),
        dm_dt_C(import_dm_dt_C),
        b31(import_b31), b32(import_b32),b33(import_b33)
    {}
    inline void AdvanceSpins(OC_INDEX j) {
      ThreeVector vtmp
        = b31*dm_dt_A[j] + b32*dm_dt_B[j] + b33*dm_dt_C[j];
      vtmp *= mstep;
      vtmp += old_spin[j];
      vtmp.MakeUnit();
      new_spin[j] = vtmp;
    }

  private:
    const Oxs_MeshValue<ThreeVector>& dm_dt_A;
    const Oxs_MeshValue<ThreeVector>& dm_dt_B;
    const Oxs_MeshValue<ThreeVector>& dm_dt_C;
    const OC_REAL8m b31;
    const OC_REAL8m b32;
    const OC_REAL8m b33;

    // Diable default copy and assignment members
    RKF54_AdvanceState_B(RKF54_AdvanceState_B const&);
    RKF54_AdvanceState_B& operator=(RKF54_AdvanceState_B const&);
  };

  class RKF54_AdvanceState_C : public RKF54_AdvanceState_Base {
  public:
    RKF54_AdvanceState_C
    (const Oxs_SimState& import_old_state,
     Oxs_SimState& import_new_state,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_A,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_B,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_C,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_D,
     const OC_REAL8m import_b41,
     const OC_REAL8m import_b42,
     const OC_REAL8m import_b43,
     const OC_REAL8m import_b44,
     const OC_REAL8m import_hstep,
     const OC_REAL8m import_mstep)
      : RKF54_AdvanceState_Base(import_old_state,
                                import_new_state,
                                import_hstep,import_mstep),
        dm_dt_A(import_dm_dt_A),
        dm_dt_B(import_dm_dt_B),
        dm_dt_C(import_dm_dt_C),
        dm_dt_D(import_dm_dt_D),
        b41(import_b41), b42(import_b42),
        b43(import_b43), b44(import_b44)
    {}
    inline void AdvanceSpins(OC_INDEX j) {
      ThreeVector vtmp
        = b41*dm_dt_A[j] + b42*dm_dt_B[j]
        + b43*dm_dt_C[j] + b44*dm_dt_D[j];
      vtmp *= mstep;
      vtmp += old_spin[j];
      vtmp.MakeUnit();
      new_spin[j] = vtmp;
    }

  private:
    const Oxs_MeshValue<ThreeVector>& dm_dt_A;
    const Oxs_MeshValue<ThreeVector>& dm_dt_B;
    const Oxs_MeshValue<ThreeVector>& dm_dt_C;
    const Oxs_MeshValue<ThreeVector>& dm_dt_D;
    const OC_REAL8m b41;
    const OC_REAL8m b42;
    const OC_REAL8m b43;
    const OC_REAL8m b44;

    // Diable default copy and assignment members
    RKF54_AdvanceState_C(RKF54_AdvanceState_C const&);
    RKF54_AdvanceState_C& operator=(RKF54_AdvanceState_C const&);
  };

  class RKF54_AdvanceState_D : public RKF54_AdvanceState_Base {
  public:
    RKF54_AdvanceState_D
    (const Oxs_SimState& import_old_state,
     Oxs_SimState& import_new_state,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_A,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_B,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_C,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_D,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_E,
     const OC_REAL8m import_b51,
     const OC_REAL8m import_b52,
     const OC_REAL8m import_b53,
     const OC_REAL8m import_b54,
     const OC_REAL8m import_b55,
     const OC_REAL8m import_hstep,
     const OC_REAL8m import_mstep)
      : RKF54_AdvanceState_Base(import_old_state,
                                import_new_state,
                                import_hstep,import_mstep),
        dm_dt_A(import_dm_dt_A),
        dm_dt_B(import_dm_dt_B),
        dm_dt_C(import_dm_dt_C),
        dm_dt_D(import_dm_dt_D),
        dm_dt_E(import_dm_dt_E),
        b51(import_b51), b52(import_b52),
        b53(import_b53), b54(import_b54),
        b55(import_b55)
    {}
    inline void AdvanceSpins(OC_INDEX j) {
      ThreeVector vtmp
        = b51*dm_dt_A[j] + b52*dm_dt_B[j]
        + b53*dm_dt_C[j] + b54*dm_dt_D[j] + b55*dm_dt_E[j];
      vtmp *= mstep;
      vtmp += old_spin[j];
      vtmp.MakeUnit();
      new_spin[j] = vtmp;
    }

  private:
    const Oxs_MeshValue<ThreeVector>& dm_dt_A;
    const Oxs_MeshValue<ThreeVector>& dm_dt_B;
    const Oxs_MeshValue<ThreeVector>& dm_dt_C;
    const Oxs_MeshValue<ThreeVector>& dm_dt_D;
    const Oxs_MeshValue<ThreeVector>& dm_dt_E;
    const OC_REAL8m b51;
    const OC_REAL8m b52;
    const OC_REAL8m b53;
    const OC_REAL8m b54;
    const OC_REAL8m b55;

    // Diable default copy and assignment members
    RKF54_AdvanceState_D(RKF54_AdvanceState_D const&);
    RKF54_AdvanceState_D& operator=(RKF54_AdvanceState_D const&);
  };

  class RKF54_AdvanceState_E : public RKF54_AdvanceState_Base {
  public:
    RKF54_AdvanceState_E
    (const Oxs_SimState& import_old_state,
     Oxs_SimState& import_new_state,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_A,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_B,
     Oxs_MeshValue<ThreeVector>& biport_dm_dt_C, // NB: non-const
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_D,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_E,
     const OC_REAL8m import_b61, // Note: b62==0.0
     const OC_REAL8m import_b63,
     const OC_REAL8m import_b64,
     const OC_REAL8m import_b65,
     const OC_REAL8m import_b66,
     const OC_REAL8m import_dc3,
     const OC_REAL8m import_dc6,
     const OC_REAL8m import_hstep,
     const OC_REAL8m import_mstep,
     OC_REAL8m& norm_error)
      : RKF54_AdvanceState_Base(import_old_state,
                                import_new_state,
                                import_hstep,import_mstep),
        dm_dt_A(import_dm_dt_A),
        dm_dt_B(import_dm_dt_B),
        dm_dt_C(biport_dm_dt_C),
        dm_dt_D(import_dm_dt_D),
        dm_dt_E(import_dm_dt_E),
        b61(import_b61), // Note: b62==0.0
        b63(import_b63), b64(import_b64),
        b65(import_b65), b66(import_b66),
        dc3(import_dc3), dc6(import_dc6),
        min_normsq(DBL_MAX), max_normsq(0.0),
        export_norm_error(norm_error)
    {}
    inline void Initialize() {
      RKF54_AdvanceState_Base::Initialize();
      min_normsq = DBL_MAX;
      max_normsq = 0.0;
    }
    inline void AdvanceSpins(OC_INDEX j) {
      ThreeVector vtmp6 = dm_dt_B[j];
      ThreeVector vtmp3 = dm_dt_C[j];
      dm_dt_C[j] = dc6*vtmp6 + dc3*vtmp3;
      vtmp6 *= b66;
      vtmp6 += b63*vtmp3
        + b61*dm_dt_A[j]
        + b64*dm_dt_D[j]
        + b65*dm_dt_E[j];
      vtmp6 *= mstep;
      vtmp6 += old_spin[j];
      OC_REAL8m magsq = vtmp6.MakeUnit();
      new_spin[j] = vtmp6;
      if(magsq<min_normsq) min_normsq=magsq;
      if(magsq>max_normsq) max_normsq=magsq;
    }
    inline void Finalize() {
      export_norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                                 1.0 - sqrt(min_normsq));
      RKF54_AdvanceState_Base::Finalize();
    }

  private:
    const Oxs_MeshValue<ThreeVector>& dm_dt_A;
    const Oxs_MeshValue<ThreeVector>& dm_dt_B;
    Oxs_MeshValue<ThreeVector>& dm_dt_C; // NB: non-const
    const Oxs_MeshValue<ThreeVector>& dm_dt_D;
    const Oxs_MeshValue<ThreeVector>& dm_dt_E;
    const OC_REAL8m b61;
    const OC_REAL8m b63; // Note: b62==0.0
    const OC_REAL8m b64;
    const OC_REAL8m b65;
    const OC_REAL8m b66;
    const OC_REAL8m dc3;
    const OC_REAL8m dc6;
    OC_REAL8m min_normsq, max_normsq;
    OC_REAL8m& export_norm_error;

    // Diable default copy and assignment members
    RKF54_AdvanceState_E(RKF54_AdvanceState_E const&);
    RKF54_AdvanceState_E& operator=(RKF54_AdvanceState_E const&);
  };


  // NOTE: RKF54_AdvanceState_F does NOT inherit from
  // RKF54_AdvanceState_Base (primarily because RKF54_AdvanceState_F
  // doesn't use states).
  class RKF54_AdvanceState_F {
  public:
    RKF54_AdvanceState_F
    (const Oxs_MeshValue<ThreeVector>& import_dm_dt_A,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_B,
     Oxs_MeshValue<ThreeVector>& biport_dm_dt_C, // NB: non-const
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_D,
     const Oxs_MeshValue<ThreeVector>& import_dm_dt_E,
     const OC_REAL8m import_dc1,
     const OC_REAL8m import_dc4,
     const OC_REAL8m import_dc5,
     const OC_REAL8m import_dc7,
     OC_REAL8m& max_dD_sq)
      : dm_dt_A(import_dm_dt_A),
        dm_dt_B(import_dm_dt_B),
        dm_dt_C(biport_dm_dt_C),
        dm_dt_D(import_dm_dt_D),
        dm_dt_E(import_dm_dt_E),
        dc1(import_dc1),
        dc4(import_dc4),
        dc5(import_dc5),
        dc7(import_dc7),
        max_dm_dt_C_sq(0.0),
        export_max_dD_sq(max_dD_sq)
    {}
    inline void Initialize() {
      max_dm_dt_C_sq = 0.0;
    }
    inline void AdvanceSpins(OC_INDEX j) {
      dm_dt_C[j] += dc1*dm_dt_A[j]
        + dc4*dm_dt_D[j]
        + dc5*dm_dt_E[j]
        + dc7*dm_dt_B[j];
      // NB: No spin advancement
      OC_REAL8m magsq = dm_dt_C[j].MagSq();
      if(magsq>max_dm_dt_C_sq) max_dm_dt_C_sq = magsq;
    }
    inline void Finalize() {
      export_max_dD_sq = max_dm_dt_C_sq;
    }

  private:
    const Oxs_MeshValue<ThreeVector>& dm_dt_A;
    const Oxs_MeshValue<ThreeVector>& dm_dt_B;
    Oxs_MeshValue<ThreeVector>& dm_dt_C; // NB: non-const
    const Oxs_MeshValue<ThreeVector>& dm_dt_D;
    const Oxs_MeshValue<ThreeVector>& dm_dt_E;
    const OC_REAL8m dc1;
    const OC_REAL8m dc4;
    const OC_REAL8m dc5;
    const OC_REAL8m dc7;
    OC_REAL8m max_dm_dt_C_sq;
    OC_REAL8m& export_max_dD_sq;

    // Diable default copy and assignment members
    RKF54_AdvanceState_F(RKF54_AdvanceState_F const&);
    RKF54_AdvanceState_F& operator=(RKF54_AdvanceState_F const&);
  };


  // Templated versions of dm_dt + state advance code
  //////////////////////////////////////////////////////////////////////

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
