/* FILE: cgevolve.h                 -*-Mode: c++-*-
 *
 * Concrete minimization evolver class, using conjugate gradients
 *
 */

#ifndef _OXS_CGEVOLVE
#define _OXS_CGEVOLVE

#include "energy.h"
#include "minevolver.h"
#include "key.h"
#include "output.h"

/* End includes */

#if REPORT_TIME
# ifndef REPORT_TIME_CGDEVEL
#  define REPORT_TIME_CGDEVEL 1
# endif
#endif
#ifndef REPORT_TIME_CGDEVEL
# define REPORT_TIME_CGDEVEL 0
#endif

class Oxs_CGEvolve:public Oxs_MinEvolver {
private:

  // Originally, the user-specified "energy_precision" setting was
  // used directly to obtain an estimate of error in the computed
  // energy.  Since OOMMF API 20170801 this has been superseded by
  // error estimates coming from individual energy terms themselves.
  // The user controlled error knob has been retained, however,
  // through the "energy_error_adj" member variable.  This value
  // scales the error estimate coming from the energy computation,
  // where energy_error_adj is set to energy_precision/1e-14, where
  // 1e-14 is the default value for energy_precision.
  OC_REAL8m energy_error_adj;

#if REPORT_TIME
  mutable Nb_StopWatch steponlytime;
#if REPORT_TIME_CGDEVEL
  mutable Nb_StopWatch energyobjtime;
  mutable Nb_StopWatch basepttime;
  mutable Nb_StopWatch findbrackettime;
  mutable Nb_StopWatch findlinemintime;
  mutable Nb_StopWatch fillbrackettime;
  mutable Nb_StopWatch getenergyandmxHxmtime;
  mutable vector<Nb_StopWatch> timer;
  struct TimerCounts {
  public:
    OC_INT4m pass_count;
    size_t bytes;
    OC_INT4m throw_away_count;
    TimerCounts() : pass_count(0), bytes(0), throw_away_count(2) {}
    void Reset() { pass_count = 0; bytes = 0; }
    void Increment(Nb_StopWatch& timer,size_t add_bytes) {
      bytes += add_bytes;
      if(++pass_count == throw_away_count) {
        bytes=0;
        timer.Reset();
      }
    }
  };
  mutable vector<TimerCounts> timer_counts;
#endif // REPORT_TIME_CGDEVEL
#endif

  // Counts
  OC_UINT4m step_attempt_count; // Set to 0 in Init()
  OC_UINT4m energy_calc_count;
  OC_UINT4m cycle_count;      // Total # of directions selected
  OC_UINT4m cycle_sub_count;  // # of cycles since last gradient reset
  OC_UINT4m bracket_count;
  OC_UINT4m line_minimum_count;
  OC_UINT4m conjugate_cycle_count;

  OC_UINT4m gradient_reset_count; // Re-initialize minimization
  /// direction from gradient (as opposed to conjugate gradient)
  /// when cycle_sub_count>=gradient_reset_count.
  /// Set from user data: "gradient_reset_count"

  OC_REAL8m gradient_reset_score,gradient_reset_wgt,
    gradient_reset_angle_cotangent,gradient_reset_trigger;

  OC_REAL8m kludge_adjust_angle_cos; // If angle between new direction
  /// and mxHxm is bigger than this angle, then call ::KludgeDirection
  /// member function to adjust new direction to match this angle.

  // Internal data structures
  struct Basept_Data {
    OC_BOOL valid; // Set this to zero to mark data invalid
    OC_UINT4m id;
    OC_UINT4m stage;
    OC_REAL8m total_energy;
    Oxs_MeshValue<ThreeVector> direction;
    OC_REAL8m direction_max_mag; // sup-norm: max_k sqrt(direction[k]^2)
    OC_REAL8m direction_norm;    // L2-norm: sqrt(\sum_k direction[k]^2)

    enum Conjugate_Method { FLETCHER_REEVES, POLAK_RIBIERE } method;
    /// "method" is fixed from user data.  Conjugate gradient
    /// direction is calculated via
    ///    direction[n] = mxHxm[n] + gamma*direction[n-1],
    /// where 'n' is the cycle count, and
    ///    gamma = mxHxm[n]^2/mxHxm[n-1]^2
    /// for Fletcher-Reeves method, or
    ///    gamma = (mxHxm[n]-mxHxm[n-1])*mxHxm[n]/mxHxm[n-1]^2
    /// for the Polak-Ribiere method.  Note that the mxHxm[n-1]
    /// array is only needed for the Polak-Ribiere method.
    /// If using the Fletcher-Reeves method, only the mxHxm^2
    /// scalar value needs to be saved between cycles, reducing
    /// memory requirements.
    Oxs_MeshValue<ThreeVector> mxHxm; // Use only if Polak-Ribiere
    OC_REAL8m g_sum_sq;  // (V[k].Ms[k].mxHxm[k])^2
    OC_REAL8m Ep;

    void Init() {
      valid=0; id=0; stage=0;
      total_energy=0.0;
      direction.Release();
      direction_max_mag=0.0;
      direction_norm=0.0;
      mxHxm.Release();
      g_sum_sq = 0.0;
      Ep = 0.0;
    }
    Basept_Data() : method(FLETCHER_REEVES) { Init(); }
  } basept;

  struct Bracket {
    Oxs_ConstKey<Oxs_SimState> key;
    Oxs_MeshValue<OC_REAL8m> energy;
    Oxs_MeshValue<ThreeVector> mxHxm;
    OC_REAL8m offset;  // Location is basept + offset*basedir
    OC_REAL8m E;       // (Total) energy relative to bestpt state
    OC_REAL8m E_error_estimate; // Estimated absolute error in E
    OC_REAL8m Ep;      // Derivative of E in direction basedir.
    OC_REAL8m grad_norm; // Weighted L2-norm of mxHxm.  By Cauchy-Schwartz,
    /// |Ep| < mu0*basept.direction_norm * grad_norm
    void Init() {
      key.Release();
      energy.Release();
      mxHxm.Release();
      offset = -1.0;
      E = E_error_estimate = Ep = grad_norm = 0.0;
    }
    void Clear () {
      key.Release(); // Puts key in INVALID state and set key ptr to 0.
      // Reserve memory in energy or mxHxm arrays
      offset = -1.0;
      E = E_error_estimate = Ep = grad_norm = 0.0;
    }
    void Swap(Bracket& other) {
      if(this == &other) return; // NOP
      key.Swap(other.key);
      energy.Swap(other.energy);
      mxHxm.Swap(other.mxHxm);
      OC_REAL8m tmpoff = offset; offset = other.offset; other.offset = tmpoff;
      OC_REAL8m tmpE   = E;           E = other.E;           other.E = tmpE;
      OC_REAL8m tmpEerr      = E_error_estimate;
      E_error_estimate       = other.E_error_estimate;
      other.E_error_estimate = tmpEerr;
      OC_REAL8m tmpEp  = Ep;         Ep = other.Ep;         other.Ep = tmpEp;
      OC_REAL8m tmpgrad = grad_norm;
      grad_norm         = other.grad_norm;
      other.grad_norm   = tmpgrad;
    }
    Bracket() { Init(); }
  };

  Bracket extra_bracket;
  struct Bracket_Data;
  friend struct Bracket_Data; // Allow access to struct Bracket.

  struct Bracket_Data {
    OC_BOOL min_bracketed;
    OC_BOOL min_found;
    OC_BOOL bad_Edata;  // true if E data appears noisy
    OC_UINT4m weak_bracket_count; // for current line direction
    OC_REAL8m minstep;  // Fixed from user data: "minimum_bracket_step"
    OC_REAL8m maxstep;  // Fixed from user data: "maximum_bracket_step"
    OC_REAL8m scaled_minstep;
    OC_REAL8m scaled_maxstep;
    OC_REAL8m start_step;
    OC_REAL8m angle_precision; // From user: "line_minimum_angle_precision"
    OC_REAL8m relative_minspan; // From user data: "line_minimum_relwidth"
    OC_REAL8m stop_span; // Calculated using relative_minspan and
    /// initial bracketing interval.
    OC_REAL8m last_min_reduction_ratio;
    OC_REAL8m next_to_last_min_reduction_ratio;
    Bracket left;
    Bracket right;
    void Init() {
      min_bracketed=min_found=bad_Edata=0;
      weak_bracket_count=0;
      scaled_minstep=scaled_maxstep=start_step=stop_span=0.0;
      last_min_reduction_ratio=0.0;
      next_to_last_min_reduction_ratio=0.0;
      left.Init();
      right.Init();
    }
    Bracket_Data()
      : minstep(-1.), maxstep(-1.),
        angle_precision(-1),
        relative_minspan(-1) { Init(); }
  } bracket;

  struct Best_Point {
    OC_UINT4m state_id;
    const Bracket* bracket;
    OC_BOOL is_line_minimum;
    void Init() {
      state_id = 0;
      bracket = 0;
      is_line_minimum=0;
    }
    Best_Point() { Init(); }
    void SetBracket(const Bracket& endpt) {
      bracket = &endpt;
      state_id = endpt.key.ObjectId();
    }
  } bestpt;

  // Preconditioner support.  This is only used inside the
  // SetBasePoint routine.  See NOTES VI, 18-Jul-2011, pp. 6-9.
  Oxs_EnergyPreconditionerSupport::Preconditioner_Type preconditioner_type;
  OC_REAL8m preconditioner_weight; // In range [0,1]
  Oxs_MeshValue<ThreeVector> preconditioner_Ms_V;   // C^-2 * Ms * V
  Oxs_MeshValue<ThreeVector> preconditioner_Ms2_V2; // C^-2 * Ms^2 * V^2
  Oxs_MeshValue<OC_REAL8m> Ms_V;                    // Ms * V
  OC_UINT4m preconditioner_mesh_id;
  OC_REAL8m sum_error_estimate;  // OC_REAL8m_EPSILON*sqrt(# of cells)
  void InitializePreconditioner(const Oxs_SimState* state);
  // The above arrays, preconditioner_Ms_V, preconditioner_Ms2_V2, and
  // Ms_V all depend on Ms and volume V.  These are all set inside
  // InitializePreconditioner, which is only called when a change in the
  // state->mesh is detected.  Therefore, the code assumes that cell Ms
  // and volume don't change without a change in the mesh.  If this
  // assumption becomes untrue, then modifications will be necessary.

  // Scratch space
  Oxs_MeshValue<OC_REAL8m> scratch_energy;
  Oxs_MeshValue<ThreeVector> scratch_field;

  // Field outputs
  void UpdateDerivedFieldOutputs(const Oxs_SimState& state);
  Oxs_VectorFieldOutput<Oxs_CGEvolve> total_H_field_output;
  Oxs_VectorFieldOutput<Oxs_CGEvolve> mxHxm_output;
  Oxs_ScalarFieldOutput<Oxs_CGEvolve> total_energy_density_output;

  // Scalar outputs
  void UpdateDerivedOutputs(const Oxs_SimState&);
  Oxs_ScalarOutput<Oxs_CGEvolve> energy_calc_count_output;
  Oxs_ScalarOutput<Oxs_CGEvolve> cycle_count_output;
  Oxs_ScalarOutput<Oxs_CGEvolve> cycle_sub_count_output;
  Oxs_ScalarOutput<Oxs_CGEvolve> max_mxHxm_output;
  Oxs_ScalarOutput<Oxs_CGEvolve> delta_E_output;
  Oxs_ScalarOutput<Oxs_CGEvolve> total_energy_output;
  Oxs_ScalarOutput<Oxs_CGEvolve> bracket_count_output;
  Oxs_ScalarOutput<Oxs_CGEvolve> line_min_count_output;
  Oxs_ScalarOutput<Oxs_CGEvolve> conjugate_cycle_count_output;

  void GetEnergyAndmxHxm
  (const Oxs_SimState* state,          // Import
   Oxs_MeshValue<OC_REAL8m>& energy,   // Export
   Oxs_MeshValue<ThreeVector>& mxHxm,  // Export
   Oxs_MeshValue<ThreeVector>* Hptr);  // Export

  void GetRelativeEnergyAndDerivative
  (Oxs_ConstKey<Oxs_SimState> statekey, // Import
   OC_REAL8m time_offset,               // Import
   Bracket& endpt);                     // Export

  static OC_REAL8m EstimateQuadraticMinimum
  (OC_REAL8m wgt,
   OC_REAL8m h,
   OC_REAL8m f0,    // f(0)
   OC_REAL8m fh,    // f(h)
   OC_REAL8m fp0,   // f'(0)
   OC_REAL8m fph);  // f'(h)

  static OC_REAL8m FindCubicMinimum
  (OC_REAL8m Ediff, // f(1) - f(0)
   OC_REAL8m lEp,   // f'(0)
   OC_REAL8m rEp);  // f'(1)

  OC_REAL8m EstimateEnergySlack() const;
  // Uses basept and bracket data to make an estimate on rounding error
  // in energy computation.  If |E1-E2|<EstimateEnergySlack() then for
  // practical purposes E1 and E2 should be considered equal.

  OC_BOOL BadPrecisionTest
  (const Oxs_CGEvolve::Bracket& left,
   const Oxs_CGEvolve::Bracket& right,
   OC_REAL8m energy_slack);
  // Returns 0 if data looks OK, 1 otherwise.


  void InitializeWorkState(const Oxs_MinDriver* driver,
                           const Oxs_SimState* current_state,
                           Oxs_Key<Oxs_SimState>& work_state_key);

  void FillBracket(const Oxs_MinDriver* driver,
                   OC_REAL8m offset,
		   const Oxs_SimState* oldstate,
		   Oxs_Key<Oxs_SimState>& statekey,
		   Bracket& endpt);

  void UpdateBrackets(Bracket& tbracket,
		      OC_BOOL force_bestpt);

  void FindBracketStep(const Oxs_MinDriver* driver,
                       const Oxs_SimState* oldstate,
		       Oxs_Key<Oxs_SimState>& statekey);

  void FindLineMinimumStep(const Oxs_MinDriver* driver,
                           const Oxs_SimState* oldstate,
			   Oxs_Key<Oxs_SimState>& statekey);

  void AdjustDirection(const Oxs_SimState* cstate,OC_REAL8m gamma,
                       OC_REAL8m& maxmagsq,Nb_Xpfloat& work_normsumsq,
                       Nb_Xpfloat& work_gradsumsq,Nb_Xpfloat& work_Ep);

  void KludgeDirection(const Oxs_SimState* cstate,OC_REAL8m kludge,
                       OC_REAL8m& maxmagsq,Nb_Xpfloat& work_normsumsq,
                       Nb_Xpfloat& work_gradsumsq,Nb_Xpfloat& work_Ep);

  void SetBasePoint(Oxs_ConstKey<Oxs_SimState> cstate_key);

  void RuffleBasePoint(const Oxs_MinDriver* driver,
                       const Oxs_SimState* oldstate,
                       Oxs_Key<Oxs_SimState>& statekey);

  void NudgeBestpt(const Oxs_MinDriver* driver,
                   const Oxs_SimState* oldstate,
		   Oxs_Key<Oxs_SimState>& statekey);

public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  virtual OC_BOOL Init();
  Oxs_CGEvolve(const char* name,     // Child instance id
	       Oxs_Director* newdtr, // App director
	       const char* argstr);  // MIF input block parameters
  virtual ~Oxs_CGEvolve();

  virtual OC_BOOL
  InitNewStage(const Oxs_MinDriver* driver,
               Oxs_ConstKey<Oxs_SimState> state,
               Oxs_ConstKey<Oxs_SimState> prevstate);

  virtual OC_BOOL
  TryStep(const Oxs_MinDriver* driver,
          Oxs_ConstKey<Oxs_SimState> current_state,
          const Oxs_DriverStepInfo& step_info,
          Oxs_ConstKey<Oxs_SimState>& next_state);
  // Returns true if step was successful, false if
  // unable to step as requested.
};

#endif // _OXS_CGEVOLVE
