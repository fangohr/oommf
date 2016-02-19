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

  struct Best_Point {
    Oxs_ConstKey<Oxs_SimState> key;
    OC_REAL8m offset; // Offset from basept
    OC_REAL8m Ep;
    OC_REAL8m grad_norm;
    Oxs_MeshValue<OC_REAL8m> energy;
    Oxs_MeshValue<ThreeVector> mxHxm;
    OC_BOOL is_line_minimum;
    void Init() {
      key.Release();
      offset=0.0;
      Ep=0.0;
      grad_norm=0.0;
      energy.Release();
      mxHxm.Release();
      is_line_minimum=0;
    }
    Best_Point() { Init(); }
  } bestpt;

  struct Bracket {
    OC_REAL8m offset;  // Location is basept + offset*basedir
    OC_REAL8m E;       // Energy relative to best_state
    OC_REAL8m Ep;      // Derivative of E in direction basedir.
    OC_REAL8m grad_norm; // Weighted L2-norm of mxHxm.  By Cauchy-Schwartz,
    /// |Ep| < mu0*basept.direction_norm * grad_norm
    void Init() { offset=0.0; E=0.0; Ep=0.0; grad_norm=0.0; }
    Bracket() { Init(); }
  };

  Bracket extra_bracket;
  struct Bracket_Data;
  friend struct Bracket_Data; // Allow access to struct Bracket.

  struct Bracket_Data {
    OC_BOOL min_bracketed;
    OC_BOOL min_found;
    OC_REAL8m minstep;  // Fixed from user data: "minimum_bracket_step"
    OC_REAL8m maxstep;  // Fixed from user data: "maximum_bracket_step"
    OC_REAL8m energy_precision; // From user data: "energy_precision"
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
      min_bracketed=min_found=0;
      scaled_minstep=scaled_maxstep=start_step=stop_span=0.0;
      last_min_reduction_ratio=0.0;
      next_to_last_min_reduction_ratio=0.0;
      left.Init();
      right.Init();
    }
    Bracket_Data()
      : minstep(-1.), maxstep(-1.),energy_precision(-1),
        angle_precision(-1),
        relative_minspan(-1) { Init(); }
  } bracket;

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

  // Temp space
  OC_UINT4m temp_id;
  Oxs_MeshValue<OC_REAL8m> temp_energy;
  Oxs_MeshValue<ThreeVector> temp_mxHxm;

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
  (const Oxs_SimState* state, // Import
   OC_REAL8m time_offset,     // Import
   OC_REAL8m &relenergy,      // Export
   OC_REAL8m &derivative,     // Export
   OC_REAL8m &grad_norm);     // Export

  static OC_REAL8m EstimateQuadraticMinimum
  (OC_REAL8m wgt,
   OC_REAL8m h,
   OC_REAL8m f0,    // f(0)
   OC_REAL8m fh,    // f(h)
   OC_REAL8m fp0,   // f'(0)
   OC_REAL8m fph);  // f'(h)

  void FillBracket(OC_REAL8m offset,
		   const Oxs_SimState* oldstate,
		   Oxs_Key<Oxs_SimState>& statekey,
		   Bracket& endpt);

  void UpdateBrackets(Oxs_ConstKey<Oxs_SimState> tstate_key,
		      const Bracket& tbracket,
		      OC_BOOL force_bestpt);

  void FindBracketStep(const Oxs_SimState* oldstate,
		       Oxs_Key<Oxs_SimState>& statekey);

  void FindLineMinimumStep(const Oxs_SimState* oldstate,
			   Oxs_Key<Oxs_SimState>& statekey);

  void AdjustDirection(const Oxs_SimState* cstate,OC_REAL8m gamma,
                       OC_REAL8m& maxmagsq,Nb_Xpfloat& work_normsumsq,
                       Nb_Xpfloat& work_gradsumsq,Nb_Xpfloat& work_Ep);

  void KludgeDirection(const Oxs_SimState* cstate,OC_REAL8m kludge,
                       OC_REAL8m& maxmagsq,Nb_Xpfloat& work_normsumsq,
                       Nb_Xpfloat& work_gradsumsq,Nb_Xpfloat& work_Ep);

  void SetBasePoint(Oxs_ConstKey<Oxs_SimState> cstate_key);

  void RuffleBasePoint(const Oxs_SimState* oldstate,
                       Oxs_Key<Oxs_SimState>& statekey);

  void NudgeBestpt(const Oxs_SimState* oldstate,
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
  Step(const Oxs_MinDriver* driver,
       Oxs_ConstKey<Oxs_SimState> current_state,
       const Oxs_DriverStepInfo& step_info,
       Oxs_Key<Oxs_SimState>& next_state);
  // Returns true if step was successful, false if
  // unable to step as requested.
};

#endif // _OXS_CGEVOLVE
