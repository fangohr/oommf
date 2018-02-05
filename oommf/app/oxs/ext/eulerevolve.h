/* FILE: eulerevolve.h                 -*-Mode: c++-*-
 *
 * Concrete evolver class, using simple forward Euler steps
 *
 */

#ifndef _OXS_EULEREVOLVE
#define _OXS_EULEREVOLVE

#include "energy.h"
#include "key.h"
#include "output.h"
#include "timeevolver.h"

/* End includes */

class Oxs_EulerEvolve:public Oxs_TimeEvolver {
private:
  // Base step size control parameters
  OC_REAL8m min_timestep;           // Seconds
  OC_REAL8m max_timestep;           // Seconds

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

  OC_REAL8m step_headroom; // The 3 control parameters above can be
  /// used to estimate the step size that would just fit the control
  /// requirements.  Because this is only an estimate, if the step size
  /// is actually set to that value there is a good chance that the
  /// requirement will not be met.  So instead, we leave some headroom
  /// by setting the step size to the computed value multiplied by
  /// step_headroom.  This is a non-dimensional quantity, which should
  /// be in the range (0,1).

  // The following evolution constants are uniform for now.  These
  // should be changed to arrays in the future.
  OC_REAL8m gamma;  // Landau-Lifschitz gyromagnetic ratio
  OC_REAL8m alpha;  // Landau-Lifschitz damping coef
  OC_BOOL do_precess;  // If false, then do pure damping

  // The next timestep is based on the error from the last step.  If
  // there is no last step (either because this is the first step,
  // or because the last state handled by this routine is different
  // from the incoming current_state), then timestep is calculated
  // so that max_dm_dt * timestep = start_dm.
  OC_REAL8m start_dm;

  // Stepsize control
  const OC_REAL8m max_step_increase;
  const OC_REAL8m max_step_decrease;
  OC_BOOL CheckError(OC_REAL8m global_error_order,
                  OC_REAL8m error,
                  OC_REAL8m stepsize,
                  OC_REAL8m reference_stepsize,
                  OC_REAL8m max_dm_dt,
                  OC_REAL8m& new_stepsize);

  // Data cached from last state
  OC_UINT4m energy_state_id;
  Oxs_MeshValue<OC_REAL8m> energy;
  OC_REAL8m next_timestep;

  // Outputs
  void UpdateDerivedOutputs(const Oxs_SimState&);
  Oxs_ScalarOutput<Oxs_EulerEvolve> max_dm_dt_output;
  Oxs_ScalarOutput<Oxs_EulerEvolve> dE_dt_output;
  Oxs_ScalarOutput<Oxs_EulerEvolve> delta_E_output;
  Oxs_VectorFieldOutput<Oxs_EulerEvolve> dm_dt_output;
  Oxs_VectorFieldOutput<Oxs_EulerEvolve> mxH_output;

  // Scratch space
  Oxs_MeshValue<OC_REAL8m> new_energy;
  Oxs_MeshValue<ThreeVector> new_dm_dt;
  Oxs_MeshValue<ThreeVector> new_H;

  OC_REAL8m PositiveTimestepBound(OC_REAL8m max_dm_dt);

  void Calculate_dm_dt
  (const Oxs_Mesh& mesh_,
   const Oxs_MeshValue<OC_REAL8m>& Ms_,
   const Oxs_MeshValue<ThreeVector>& mxH_,
   const Oxs_MeshValue<ThreeVector>& spin_,
   OC_REAL8m pE_pt_,
   Oxs_MeshValue<ThreeVector>& dm_dt_,
   OC_REAL8m& max_dm_dt_,OC_REAL8m& dE_dt_,OC_REAL8m& timestep_lower_bound_);
  /// Imports: mesh_, Ms_, mxH_, spin_, pE_pt
  /// Exports: dm_dt_, max_dm_dt_, dE_dt_, timestep_lower_bound_

  inline void Calculate_dm_dt_i
  (const Oxs_Mesh& mesh,
   const Oxs_MeshValue<OC_REAL8m>& Ms,
   const Oxs_MeshValue<ThreeVector>& mxH,
   const Oxs_MeshValue<ThreeVector>& spin,
   Oxs_MeshValue<ThreeVector>& dm_dt,
   OC_REAL8m& max_dm_dt_sq,
   Oxs_Energy::SUMTYPE& dE_dt_sum,
   const OC_REAL8m coef1, /* (do_precess ? -1*gamma : 0.0 ) */
   const OC_REAL8m coef2, /* alpha * gamma */
   const OC_INDEX i) {
    if(Ms[i]==0) {
      dm_dt[i].Set(0.0,0.0,0.0);
    } else {
      dE_dt_sum += mxH[i].MagSq() * Ms[i] * mesh.Volume(i);
      ThreeVector scratch = mxH[i];
      scratch ^= spin[i];
      scratch *= coef2;
      scratch.Accum(coef1,mxH[i]);
      OC_REAL8m dm_dt_sq = scratch.MagSq();
      dm_dt[i] = scratch;
      if(dm_dt_sq>max_dm_dt_sq) {
        max_dm_dt_sq=dm_dt_sq;
      }
    }
  }


public:
  virtual const char* ClassName() const; // ClassName() is
  /// automatically generated by the OXS_EXT_REGISTER macro.
  virtual OC_BOOL Init();
  Oxs_EulerEvolve(const char* name,     // Child instance id
		 Oxs_Director* newdtr, // App director
		 const char* argstr);  // MIF input block parameters
  virtual ~Oxs_EulerEvolve();

  virtual  OC_BOOL
  Step(const Oxs_TimeDriver* driver,
       Oxs_ConstKey<Oxs_SimState> current_state,
       const Oxs_DriverStepInfo& step_info,
       Oxs_Key<Oxs_SimState>& next_state);
  // Returns true if step was successful, false if
  // unable to step as requested.
};

#endif // _OXS_EULEREVOLVE
