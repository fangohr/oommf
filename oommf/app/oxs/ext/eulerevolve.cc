/* FILE: eulerevolve.cc                 -*-Mode: c++-*-
 *
 * Concrete evolver class, using simple forward Euler steps
 *
 */

#include <float.h>

#include "nb.h"
#include "director.h"
#include "timedriver.h"
#include "simstate.h"
#include "eulerevolve.h"
#include "key.h"
#include "energy.h"		// Needed to make MSVC++ 5 happy

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_EulerEvolve);

/* End includes */

// Constructor
Oxs_EulerEvolve::Oxs_EulerEvolve(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_TimeEvolver(name,newdtr,argstr),
    max_step_increase(1.25), max_step_decrease(0.5),
    energy_state_id(0),next_timestep(0.)
{
  // Process arguments
  min_timestep=GetRealInitValue("min_timestep",0.);
  max_timestep=GetRealInitValue("max_timestep",1e-10);
  if(max_timestep<=0.0) {
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
		"Invalid parameter value:"
		" Specified max time step is %g (should be >0.)",
		static_cast<double>(max_timestep));
    throw Oxs_ExtError(this,buf);
  }

  allowed_error_rate = GetRealInitValue("error_rate",-1);
  if(allowed_error_rate>0.0) {
    allowed_error_rate *= PI*1e9/180.; // Convert from deg/ns to rad/s
  }
  allowed_absolute_step_error
    = GetRealInitValue("absolute_step_error",0.2);
  if(allowed_absolute_step_error>0.0) {
    allowed_absolute_step_error *= PI/180.; // Convert from deg to rad
  }
  allowed_relative_step_error
    = GetRealInitValue("relative_step_error",0.2);

  step_headroom = GetRealInitValue("step_headroom",0.85);
  if(step_headroom<=0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
			 " step_headroom value must be bigger than 0.");
  }

  alpha = GetRealInitValue("alpha",0.5);

  // User may specify either gamma_G (Gilbert) or
  // gamma_LL (Landau-Lifshitz).  Code uses "gamma"
  // which is LL form.
  if(HasInitValue("gamma_G") && HasInitValue("gamma_LL")) {
    throw Oxs_ExtError(this,"Invalid Specify block; "
			 "both gamma_G and gamma_LL specified.");
  } else if(HasInitValue("gamma_G")) {
    gamma = GetRealInitValue("gamma_G")/(1+alpha*alpha);
  } else if(HasInitValue("gamma_LL")) {
    gamma = GetRealInitValue("gamma_LL");
  } else {
    gamma = 2.211e5/(1+alpha*alpha);
  }
  gamma = fabs(gamma); // Force positive

  do_precess = GetIntInitValue("do_precess",1);

  start_dm = GetRealInitValue("start_dm",0.01);
  start_dm *= PI/180.; // Convert from deg to rad

  // Setup outputs
  max_dm_dt_output.Setup(this,InstanceName(),"Max dm/dt","deg/ns",0,
     &Oxs_EulerEvolve::UpdateDerivedOutputs);
  dE_dt_output.Setup(this,InstanceName(),"dE/dt","J/s",0,
     &Oxs_EulerEvolve::UpdateDerivedOutputs);
  delta_E_output.Setup(this,InstanceName(),"Delta E","J",0,
     &Oxs_EulerEvolve::UpdateDerivedOutputs);
  dm_dt_output.Setup(this,InstanceName(),"dm/dt","rad/s",1,
     &Oxs_EulerEvolve::UpdateDerivedOutputs);
  mxH_output.Setup(this,InstanceName(),"mxH","A/m",1,
     &Oxs_EulerEvolve::UpdateDerivedOutputs);

  max_dm_dt_output.Register(director,-5);
  dE_dt_output.Register(director,-5);
  delta_E_output.Register(director,-5);
  dm_dt_output.Register(director,-5);
  mxH_output.Register(director,-5);

  // dm_dt and mxH output caches are used for intermediate storage,
  // so enable caching.
  dm_dt_output.CacheRequestIncrement(1);
  mxH_output.CacheRequestIncrement(1);

  VerifyAllInitArgsUsed();
}

OC_BOOL Oxs_EulerEvolve::Init()
{
  Oxs_TimeEvolver::Init();

  energy_state_id=0;   // Mark as invalid state
  next_timestep=0.;    // Dummy value
  return 1;
}


Oxs_EulerEvolve::~Oxs_EulerEvolve()
{}


OC_REAL8m
Oxs_EulerEvolve::PositiveTimestepBound
(OC_REAL8m max_dm_dt)
{ // Computes an estimate on the minimum time needed to
  // change the magnetization state, subject to floating
  // points limits.  This code lifted out of
  // Oxs_RungeKuttaEvolve, q.v.
  OC_REAL8m min_timestep = DBL_MAX/64.;
  if(max_dm_dt>1 || OC_REAL8_EPSILON<min_timestep*max_dm_dt) {
    min_timestep = OC_REAL8_EPSILON/max_dm_dt;
    // A timestep of size min_timestep will be hopelessly lost
    // in roundoff error.  So increase a bit, based on an empirical
    // fudge factor.  This fudge factor can be tested by running a
    // problem with start_dm = 0.  If the evolver can't climb its
    // way out of the stepsize=0 hole, then this fudge factor is too
    // small.  So far, the most challenging examples have been
    // problems with small cells with nearly aligned spins, e.g., in
    // a remanent state with an external field is applied at t=0.
    // Damping ratio doesn't seem to have much effect, either way.
    min_timestep *= 64;
  } else {
    // Degenerate case: max_dm_dt_ must be exactly or very nearly
    // zero.  Punt.
    min_timestep = 1.0;
  }
  return min_timestep;
}

void Oxs_EulerEvolve::Calculate_dm_dt
(const Oxs_Mesh& mesh_,
 const Oxs_MeshValue<OC_REAL8m>& Ms_,
 const Oxs_MeshValue<ThreeVector>& mxH_,
 const Oxs_MeshValue<ThreeVector>& spin_,
 OC_REAL8m pE_pt_,
 Oxs_MeshValue<ThreeVector>& dm_dt_,
 OC_REAL8m& max_dm_dt_,OC_REAL8m& dE_dt_,OC_REAL8m& timestep_lower_bound_)
{ // Imports: mesh_, Ms_, mxH_, spin_, pE_pt_
  // Exports: dm_dt_, max_dm_dt_, dE_dt_, timestep_lower_bound_
  const OC_INDEX size = mesh_.Size(); // Assume all imports are compatible
  OC_REAL8m coef1 = -fabs(gamma);
  OC_REAL8m coef2 = -fabs(alpha);
  ThreeVector scratch;
  dm_dt_.AdjustSize(&mesh_);
  OC_INDEX i;
  for(i=0;i<size;i++) {
    if(Ms_[i]==0) {
      dm_dt_[i].Set(0.0,0.0,0.0);
    } else {
      scratch = mxH_[i];
      scratch *= coef1;
      if(do_precess) {
	dm_dt_[i] = scratch;
      } else {
	dm_dt_[i].Set(0.0,0.0,0.0);
      }
      scratch ^= spin_[i]; // -|gamma|((mxH)xm)
      scratch *= coef2;  // |alpha.gamma|((mxH)xm) = -|alpha.gamma|(mx(mxH))
      dm_dt_[i] += scratch;
    }
  }

  // Zero dm_dt at fixed spin sites
  UpdateFixedSpinList(&mesh_);
  const OC_INDEX fixed_count = GetFixedSpinCount();
  for(OC_INDEX j=0;j<fixed_count;j++) {
    dm_dt_[GetFixedSpin(j)].Set(0.,0.,0.); // What about pE_pt???
  }

  // Collect statistics
  OC_REAL8m max_dm_dt_sq=0.0;
  OC_REAL8m dE_dt_sum=0.0;
  for(i=0;i<size;i++) {
    OC_REAL8m dm_dt_sq = dm_dt_[i].MagSq();
    if(dm_dt_sq>0.0) {
      dE_dt_sum += mxH_[i].MagSq() * Ms_[i] * mesh_.Volume(i);
      if(dm_dt_sq>max_dm_dt_sq) {
	max_dm_dt_sq=dm_dt_sq;
      }
    }
  }
  max_dm_dt_ = sqrt(max_dm_dt_sq);
  dE_dt_ = -1 * MU0 * fabs(gamma*alpha) * dE_dt_sum + pE_pt_;
  /// The first term is (partial E/partial M)*dM/dt, the
  /// second term is (partial E/partial t)*dt/dt.  Note that,
  /// provided Ms_[i]>=0, that by constructions dE_dt_sum above
  /// is always non-negative, so dE_dt_ can only be made positive
  /// by positive pE_pt_.

  // Get bound on smallest stepsize that would actually
  // change spin new_max_dm_dt_index.  Rather than trying
  // to do this exactly, instead just require that
  // dm_dt*stepsize is bigger than OC_REAL8_EPSILON radian.
  // We assume here that |m| is about 1.0.
  timestep_lower_bound_ = PositiveTimestepBound(max_dm_dt_);

  return;
}


OC_BOOL
Oxs_EulerEvolve::CheckError
(OC_REAL8m global_error_order,
 OC_REAL8m error,
 OC_REAL8m stepsize,
 OC_REAL8m reference_stepsize,
 OC_REAL8m max_dm_dt,
 OC_REAL8m& new_stepsize)
{ // Returns 1 if step is good, 0 if error is too large.
  // Export new_stepsize is set to suggested stepsize
  // for next step.
  //
  // new_stepsize is initially filled with a relative stepsize
  // adjustment ratio, e.g., 1.0 means no change in stepsize.
  // At the end of this routine this ratio is multiplied by
  // stepsize to get the actual absolute stepsize.
  //
  // The import stepsize is the size (in seconds) of the actual
  // step.  The new_stepsize is computed from this based on the
  // various error estimates.  An upper bound is placed on the
  // size of new_stepsize relative to the imports stepsize and
  // reference_stepsize.  reference_stepsize has no effect if
  // it is smaller than max_step_increase*stepsize.  It is
  // usually used only in the case where the stepsize was
  // artificially reduced by, for example, a stage stopping
  // criterion.
  //
  // NOTE: This routine assumes the local error order is
  //     global_error_order + 1.
  //
  // NOTE: This routine lifted from rungekuttaevolve.cc.  Check
  //     there for updates.

  OC_BOOL good_step = 1;
  OC_BOOL error_checked=0;

  if(allowed_relative_step_error>=0. || allowed_error_rate>=0.) {
    // Determine tighter rate bound.
    OC_REAL8m rate_error = 0.0;
    if(allowed_relative_step_error<0.) {
      rate_error = allowed_error_rate;
    } else if(allowed_error_rate<0.) {
      rate_error = allowed_relative_step_error * max_dm_dt;
    } else {
      rate_error = allowed_relative_step_error * max_dm_dt;
      if(rate_error>allowed_error_rate) {
        rate_error = allowed_error_rate;
      }
    }
    rate_error *= stepsize;

    // Rate check
    if(error>rate_error) {
      good_step = 0;
      new_stepsize = pow(rate_error/error,1.0/global_error_order);
    } else {
      OC_REAL8m ratio = 0.125*DBL_MAX;
      if(error>=1 || rate_error<ratio*error) {
        OC_REAL8m test_ratio = rate_error/error;
        if(test_ratio<ratio) ratio = test_ratio;
      }
      new_stepsize = pow(ratio,1.0/global_error_order);
    }
    error_checked = 1;
  }

  // Absolute error check
  if(allowed_absolute_step_error>=0.0) {
    OC_REAL8m test_stepsize = 0.0;
    OC_REAL8m local_error_order = global_error_order + 1.0;
    if(error>allowed_absolute_step_error) {
      good_step = 0;
      test_stepsize = pow(allowed_absolute_step_error/error,
                          1.0/local_error_order);
    } else {
      OC_REAL8m ratio = 0.125*DBL_MAX;
      if(error>=1 || allowed_absolute_step_error<ratio*error) {
        OC_REAL8m test_ratio = allowed_absolute_step_error/error;
        if(test_ratio<ratio) ratio = test_ratio;
      }
      test_stepsize = pow(ratio,1.0/local_error_order);
    }
    if(!error_checked || test_stepsize<new_stepsize) {
      new_stepsize = test_stepsize;
    }
    error_checked = 1;
  }

  if(error_checked) {
    new_stepsize *= step_headroom;
    if(new_stepsize<max_step_decrease) {
      new_stepsize = max_step_decrease*stepsize;
    } else {
      new_stepsize *= stepsize;
      OC_REAL8m step_bound = stepsize * max_step_increase;
      const OC_REAL8m refrat = 0.85;  // Ad hoc value
      if(stepsize<reference_stepsize*refrat) {
        step_bound = OC_MIN(step_bound,reference_stepsize);
      } else if(stepsize<reference_stepsize) {
        OC_REAL8m ref_bound = reference_stepsize + (max_step_increase-1)
          *(stepsize-reference_stepsize*refrat)/(1-refrat);
        /// If stepsize = reference_stepsize*refrat,
        ///     then ref_bound = reference_stepsize
        /// If stepsize = reference_stepsize,
        ///     then ref_bound = step_bound
        /// Otherwise, linear interpolation on stepsize.
        step_bound = OC_MIN(step_bound,ref_bound);
      }
      if(new_stepsize>step_bound) new_stepsize = step_bound;
    }
  } else {
    new_stepsize = stepsize;
  }

  return good_step;
}

OC_BOOL
Oxs_EulerEvolve::Step(const Oxs_TimeDriver* driver,
		      Oxs_ConstKey<Oxs_SimState> current_state,
                      const Oxs_DriverStepInfo& /* step_info */,
		      Oxs_Key<Oxs_SimState>& next_state)
{
  OC_INDEX size,i; // Mesh size and indexing variable

  const Oxs_SimState& cstate = current_state.GetReadReference();

  // Do first part of next_state structure initialization.
  Oxs_SimState& workstate = next_state.GetWriteReference();
  driver->FillState(current_state.GetReadReference(),workstate);

  if(cstate.mesh->Id() != workstate.mesh->Id()) {
    throw Oxs_ExtError(this,
       "Oxs_EulerEvolve::Step: Oxs_Mesh not fixed across steps.");
  }

  if(cstate.Id() != workstate.previous_state_id) {
    throw Oxs_ExtError(this,
       "Oxs_EulerEvolve::Step: State continuity break detected.");
  }

  // Pull cached values out from cstate.
  // If cstate.Id() == energy_state_id, then cstate has been run
  // through either this method or UpdateDerivedOutputs.  Either
  // way, all derived state data should be stored in cstate,
  // except currently the "energy" mesh value array, which is
  // stored independently inside *this.  Eventually that should
  // probably be moved in some fashion into cstate too.
  if(energy_state_id != cstate.Id()) {
    // cached data out-of-date
    UpdateDerivedOutputs(cstate);
  }
  OC_BOOL cache_good = 1;
  OC_REAL8m max_dm_dt,dE_dt,delta_E,pE_pt;
  OC_REAL8m timestep_lower_bound;  // Smallest timestep that can actually
  /// change spin with max_dm_dt (due to OC_REAL8_EPSILON restrictions).
  /// The next timestep is based on the error from the last step.  If
  /// there is no last step (either because this is the first step,
  /// or because the last state handled by this routine is different
  /// from the incoming current_state), then timestep is calculated
  /// so that max_dm_dt * timestep = start_dm.

  cache_good &= cstate.GetDerivedData("Max dm/dt",max_dm_dt);
  cache_good &= cstate.GetDerivedData("dE/dt",dE_dt);
  cache_good &= cstate.GetDerivedData("Delta E",delta_E);
  cache_good &= cstate.GetDerivedData("pE/pt",pE_pt);
  cache_good &= cstate.GetDerivedData("Timestep lower bound",
				      timestep_lower_bound);
  cache_good &= (energy_state_id == cstate.Id());
  cache_good &= (dm_dt_output.cache.state_id == cstate.Id());

  if(!cache_good) {
    throw Oxs_ExtError(this,
       "Oxs_EulerEvolve::Step: Invalid data cache.");
  }

  const Oxs_MeshValue<ThreeVector>& dm_dt = dm_dt_output.cache.value;

  // Negotiate with driver over size of next step
  OC_REAL8m stepsize = next_timestep;

  if(stepsize<=0.0) {
    if(start_dm < sqrt(DBL_MAX/4) * max_dm_dt) {
      stepsize = start_dm / max_dm_dt;
    } else {
      stepsize = sqrt(DBL_MAX/4);
    }
  }

  // Insure step is not outside requested step bounds
  if(stepsize>max_timestep) stepsize = max_timestep;
  if(stepsize<min_timestep) stepsize = min_timestep;

  workstate.last_timestep=stepsize;
  if(stepsize<timestep_lower_bound) {
    workstate.last_timestep=timestep_lower_bound;
  }

  if(cstate.stage_number != workstate.stage_number) {
    // New stage
    workstate.stage_start_time = cstate.stage_start_time
                                + cstate.stage_elapsed_time;
    workstate.stage_elapsed_time = workstate.last_timestep;
  } else {
    workstate.stage_start_time = cstate.stage_start_time;
    workstate.stage_elapsed_time = cstate.stage_elapsed_time
                                  + workstate.last_timestep;
  }
  workstate.iteration_count = cstate.iteration_count + 1;
  workstate.stage_iteration_count = cstate.stage_iteration_count + 1;

  driver->FillStateSupplemental(workstate);
  OC_BOOL forcestep=0;
  if(workstate.last_timestep>stepsize ||
     workstate.last_timestep<=min_timestep) {
    // Either driver wants to force this stepsize,
    // or else suggested stepsize is smaller than
    // timestep_lower_bound and/or min_timestep.
    forcestep=1;
  }
  stepsize = workstate.last_timestep;

  // Put new spin configuration in next_state
  workstate.spin.AdjustSize(workstate.mesh); // Safety
  size = workstate.spin.Size();
  ThreeVector tempspin;
  for(i=0;i<size;++i) {
    tempspin = dm_dt[i];
    tempspin *= stepsize;

    // For improved accuracy, adjust step vector so that
    // to first order m0 + adjusted_step = v/|v| where
    // v = m0 + step.
    OC_REAL8m adj = 0.5 * tempspin.MagSq();
    tempspin -= adj*cstate.spin[i];
    tempspin *= 1.0/(1.0+adj);

    tempspin += cstate.spin[i];
    tempspin.MakeUnit();
    workstate.spin[i] = tempspin;
  }
  const Oxs_SimState& nstate
    = next_state.GetReadReference();  // Release write lock

  //  Calculate delta E
  OC_REAL8m new_pE_pt;
  GetEnergyDensity(nstate,new_energy,
		   &mxH_output.cache.value,
		   NULL,new_pE_pt);
  mxH_output.cache.state_id=nstate.Id();
  const Oxs_MeshValue<ThreeVector>& mxH = mxH_output.cache.value;

  OC_REAL8m dE=0.0;
  OC_REAL8m var_dE=0.0;
  OC_REAL8m total_E=0.0;
  for(i=0;i<size;++i) {
    OC_REAL8m vol = nstate.mesh->Volume(i);
    OC_REAL8m e = energy[i];
    total_E += e * vol;
    OC_REAL8m new_e = new_energy[i];
    dE += (new_e - e) * vol;
    var_dE += (new_e*new_e + e*e)*vol*vol;
  }
  var_dE *= 256*OC_REAL8_EPSILON*OC_REAL8_EPSILON; // Variance, assuming
  /// error in each energy[i] term is independent, uniformly
  /// distributed, 0-mean, with range +/- 16*OC_REAL8_EPSILON*energy[i].
  /// It would probably be better to get an error estimate directly
  /// from each energy term.

  // Get error estimate.  See step size adjustment discussion in MJD
  // Notes II, p72 (18-Jan-2001).  Basically, estimate the error as the
  // difference between the obtained step endpoint and what would have
  // been obtained if a 2nd order Heun step had been taken.

  OC_REAL8m new_max_dm_dt,new_dE_dt,new_timestep_lower_bound;
  Calculate_dm_dt(*(nstate.mesh),*(nstate.Ms),
		  mxH,nstate.spin,new_pE_pt,new_dm_dt,
		  new_max_dm_dt,new_dE_dt,new_timestep_lower_bound);

  OC_REAL8m max_error=0;
  for(i=0;i<size;++i) {
    ThreeVector temp = dm_dt[i];
    temp -= new_dm_dt[i];
    OC_REAL8m temp_error = temp.MagSq();
    if(temp_error>max_error) max_error = temp_error;
  }
  max_error = sqrt(max_error)/2.0; // Actual (local) error
  /// estimate is max_error * stepsize

  // Energy check control
#ifdef FOO
  OC_REAL8m expected_dE = 0.5 * (dE_dt+new_dE_dt) * stepsize;
  OC_REAL8m dE_error = dE - expected_dE;
  OC_REAL8m max_allowed_dE = expected_dE + 0.25*fabs(expected_dE);
  max_allowed_dE += OC_REAL8_EPSILON*fabs(total_E);
  max_allowed_dE += 2*sqrt(var_dE);
#else
  OC_REAL8m max_allowed_dE = 0.5 * (pE_pt+new_pE_pt) * stepsize
    + OC_MAX(OC_REAL8_EPSILON*fabs(total_E),2*sqrt(var_dE));
  /// The above says essentially that the spin adjustment can
  /// increase the energy by only as much as pE/pt allows; in
  /// the absence of pE/pt, the energy should decrease.  I
  /// think this may be problematic, if at the start of a stage
  /// the spins are near equilibrium, and the applied field is
  /// ramping up slowly.  In this case there won't be much "give"
  /// in the spin configuration with respect to pE/pm.  But I
  /// haven't seen an example of this yet, so we'll wait and see.
  /// -mjd, 27-July-2001.
#endif

  // Check step and adjust next_timestep.  The relative error
  // check is a bit fudged, because rather than limiting the
  // relative error uniformly across the sample, we limit it
  // only at the position that has the maximum absolute error
  // (i.e., max_error is max *absolute* error).
  OC_REAL8m suggested_step;
  OC_BOOL goodstep = CheckError(1,max_error*stepsize,
                             stepsize, stepsize,
                             max_dm_dt,suggested_step);
  if(dE>max_allowed_dE && suggested_step>0.5*stepsize) {
    suggested_step = 0.5*stepsize;
  }
  next_timestep = suggested_step;

  if(!forcestep && !goodstep) {
    // Reject step
    return 0;
  }

  // Otherwise, accept step.

  // If next_timestep is much smaller than the timestep
  // that brought us to cstate, then the reduction was
  // probably due to a stage stopping_time requirement.
  // So in this case bump the next_stepsize request up
  // to the minimum allowed reduction.
  OC_REAL8m timestep_lower_limit = max_step_decrease * step_headroom;
  timestep_lower_limit *= timestep_lower_limit * cstate.last_timestep;
  if(next_timestep<timestep_lower_limit) {
    next_timestep = timestep_lower_limit;
  }

  if(!nstate.AddDerivedData("Timestep lower bound",
			    new_timestep_lower_bound) ||
     !nstate.AddDerivedData("Max dm/dt",new_max_dm_dt) ||
     !nstate.AddDerivedData("dE/dt",new_dE_dt) ||
     !nstate.AddDerivedData("Delta E",dE) ||
     !nstate.AddDerivedData("pE/pt",new_pE_pt)) {
    throw Oxs_ExtError(this,
       "Oxs_EulerEvolve::Step:"
       " Programming error; data cache already set.");
  }

  dm_dt_output.cache.value.Swap(new_dm_dt);
  dm_dt_output.cache.state_id = nstate.Id();

  energy.Swap(new_energy);
  energy_state_id = nstate.Id();

  return 1;  // Good step
}

void Oxs_EulerEvolve::UpdateDerivedOutputs(const Oxs_SimState& state)
{ // This routine fills all the Oxs_EulerEvolve Oxs_ScalarOutput's to
  // the appropriate value based on the import "state", and any of
  // Oxs_VectorOutput's that have CacheRequest enabled are filled.
  // It also makes sure all the expected WOO objects in state are
  // filled.
  max_dm_dt_output.cache.state_id
    = dE_dt_output.cache.state_id
    = delta_E_output.cache.state_id
    = 0;  // Mark change in progress

  OC_REAL8m dummy_value;
  if(!state.GetDerivedData("Max dm/dt",max_dm_dt_output.cache.value) ||
     !state.GetDerivedData("dE/dt",dE_dt_output.cache.value) ||
     !state.GetDerivedData("Delta E",delta_E_output.cache.value) ||
     !state.GetDerivedData("pE/pt",dummy_value) ||
     !state.GetDerivedData("Timestep lower bound",dummy_value) ||
     (dm_dt_output.GetCacheRequestCount()>0
      && dm_dt_output.cache.state_id != state.Id()) ||
     (mxH_output.GetCacheRequestCount()>0
      && mxH_output.cache.state_id != state.Id())) {

    // Missing at least some data, so calculate from scratch

    // Calculate H and mxH outputs
    Oxs_MeshValue<ThreeVector>& mxH = mxH_output.cache.value;
    OC_REAL8m pE_pt;
    GetEnergyDensity(state,energy,&mxH,NULL,pE_pt);
    energy_state_id=state.Id();
    mxH_output.cache.state_id=state.Id();
    if(!state.GetDerivedData("pE/pt",dummy_value)) {
      state.AddDerivedData("pE/pt",pE_pt);
    }

    // Calculate dm/dt, Max dm/dt and dE/dt
    Oxs_MeshValue<ThreeVector>& dm_dt
      = dm_dt_output.cache.value;
    dm_dt_output.cache.state_id=0;
    OC_REAL8m timestep_lower_bound;
    Calculate_dm_dt(*(state.mesh),*(state.Ms),mxH,state.spin,
		    pE_pt,dm_dt,
		    max_dm_dt_output.cache.value,
		    dE_dt_output.cache.value,timestep_lower_bound);
    dm_dt_output.cache.state_id=state.Id();
    if(!state.GetDerivedData("Max dm/dt",dummy_value)) {
      state.AddDerivedData("Max dm/dt",max_dm_dt_output.cache.value);
    }
    if(!state.GetDerivedData("dE/dt",dummy_value)) {
      state.AddDerivedData("dE/dt",dE_dt_output.cache.value);
    }
    if(!state.GetDerivedData("Timestep lower bound",dummy_value)) {
      state.AddDerivedData("Timestep lower bound",
			   timestep_lower_bound);
    }

    if(!state.GetDerivedData("Delta E",dummy_value)) {
      if(state.previous_state_id!=0 && state.stage_iteration_count>0) {
	// Strictly speaking, we should be able to create dE for
	// stage_iteration_count==0 for stages>0, but as a practical
	// matter we can't at present.  Should give this more thought.
	// -mjd, 27-July-2001
	throw Oxs_ExtError(this,
	   "Oxs_EulerEvolve::UpdateDerivedOutputs:"
	   " Can't derive Delta E from single state.");
      }
      state.AddDerivedData("Delta E",0.0);
      dummy_value = 0.;
    }
    delta_E_output.cache.value=dummy_value;
  }

  max_dm_dt_output.cache.value*=(180e-9/PI);
  /// Convert from radians/second to deg/ns

  max_dm_dt_output.cache.state_id
    = dE_dt_output.cache.state_id
    = delta_E_output.cache.state_id
    = state.Id();
}
