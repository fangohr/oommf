/* FILE: xf_thermheunevolve.cc                 -*-Mode: c++-*-
 *
 * Concrete evolver class, built on top of the Oxs_RungeKuttaEvolve
 * class, but including a temperature dependent fluctuating field.
 *
 *  Xf_ThermHeunEvolve is a variant of Oxs_RungeKuttaEvolve written by
 * Xuanyao (Kelvin) Fong, which adds support for non-zero temperatures.
 *
 * Xf_ThermHeunEvolve version date: 27 April 2013
 * Requires OOMMF 1.2a5 or later.
 *
 */

#include <float.h>
#include <string>
#include <stdlib.h>
#include <time.h>

#include "nb.h"
#include "director.h"
#include "timedriver.h"
#include "simstate.h"
#include "xf_thermheunevolve.h"
#include "rectangularmesh.h"
#include "key.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Xf_ThermHeunEvolve);

/* End includes */

OC_REAL8m Xf_ThermHeunEvolve::EvaluateTemperatureProfileScript
(OC_UINT4m stage,
 OC_REAL8m stage_time,
 OC_REAL8m total_time) const
{
  if(!has_tempscript) return 1.0;

  int index;
  if((index = tempscript_opts[0].position)>=0) { // stage
    tempscript_cmd.SetCommandArg(index,stage);
  }
  if((index = tempscript_opts[1].position)>=0) { // stage_time
    tempscript_cmd.SetCommandArg(index,stage_time);
  }
  if((index = tempscript_opts[2].position)>=0) { // total_time
    tempscript_cmd.SetCommandArg(index,total_time);
  }

  tempscript_cmd.SaveInterpResult();
  tempscript_cmd.Eval();
  if(tempscript_cmd.GetResultListSize()!=1) {
    String msg
      = String("Return script value is not a single scalar: ")
      + tempscript_cmd.GetWholeResult();
    tempscript_cmd.RestoreInterpResult();
    throw Oxs_ExtError(this,msg.c_str());
  }
  OC_REAL8m result;
  tempscript_cmd.GetResultListItem(0,result);
  tempscript_cmd.RestoreInterpResult();

  return result;
}

// Constructor
Xf_ThermHeunEvolve::Xf_ThermHeunEvolve(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_TimeEvolver(name,newdtr,argstr),mesh_id(0),
    max_step_decrease(0.03125), max_step_increase_limit(4.0),
    max_step_increase_adj_ratio(1.9),
    reject_goal(0.05), reject_ratio(0.05),
    KBoltzmann(1.38062e-23),temperature(0.),
    tmpStepsize(1e-17),has_tempscript(0),
    energy_state_id(0),next_timestep(1e-27),
    rkstep_ptr(NULL)
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
    = GetRealInitValue("relative_step_error",0.01);

  expected_energy_precision =
    GetRealInitValue("energy_precision",1e-10);

  energy_check_slack =
    GetRealInitValue("energy_slack",0.10);

  reject_goal = GetRealInitValue("reject_goal",0.05);
  if(reject_goal<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"reject_goal\" value must be non-negative.");
  }

  min_step_headroom = GetRealInitValue("min_step_headroom",0.33);
  if(min_step_headroom<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"min_step_headroom\" value must be bigger than 0.");
  }

  max_step_headroom = GetRealInitValue("max_step_headroom",0.95);
  if(max_step_headroom<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"max_step_headroom\" value must be bigger than 0.");
  }

  if(min_step_headroom>max_step_headroom) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"min_step_headroom\" value must not be larger than"
         " \"max_step_headroom\".");
  }


  if(HasInitValue("alpha")) {
    OXS_GET_INIT_EXT_OBJECT("alpha",Oxs_ScalarField,alpha_init);
  } else {
    alpha_init.SetAsOwner(dynamic_cast<Oxs_ScalarField *>
                          (MakeNew("Oxs_UniformScalarField",director,
                                   "value 0.5")));
  }

  // User may specify either gamma_G (Gilbert) or
  // gamma_LL (Landau-Lifshitz).
  gamma_style = GS_INVALID;
  if(HasInitValue("gamma_G") && HasInitValue("gamma_LL")) {
    throw Oxs_ExtError(this,"Invalid Specify block; "
                         "both gamma_G and gamma_LL specified.");
  } else if(HasInitValue("gamma_G")) {
    gamma_style = GS_G;
    OXS_GET_INIT_EXT_OBJECT("gamma_G",Oxs_ScalarField,gamma_init);
  } else if(HasInitValue("gamma_LL")) {
    gamma_style = GS_LL;
    OXS_GET_INIT_EXT_OBJECT("gamma_LL",Oxs_ScalarField,gamma_init);
  } else {
    gamma_style = GS_G;
    gamma_init.SetAsOwner(dynamic_cast<Oxs_ScalarField *>
                          (MakeNew("Oxs_UniformScalarField",director,
                                   "value -2.211e5")));
  }
  allow_signed_gamma = GetIntInitValue("allow_signed_gamma",0);
  do_precess = GetIntInitValue("do_precess",1);

  start_dm = GetRealInitValue("start_dm",0.01);
  if(start_dm<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
         " \"start_dm\" value must be nonnegative.");
  }
  start_dm *= PI/180.; // Convert from deg to rad

  stage_init_step_control = SISC_AUTO;  // Safety
  String stage_start = GetStringInitValue("stage_start","auto");
  Oxs_ToLower(stage_start);
  if(stage_start.compare("start_dm")==0) {
    stage_init_step_control = SISC_START_DM;
  } else if(stage_start.compare("continuous")==0) {
    stage_init_step_control = SISC_CONTINUOUS;
  } else if(stage_start.compare("auto")==0
            || stage_start.compare("automatic")==0) {
    stage_init_step_control = SISC_AUTO;
  } else {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
                         " \"stage_start\" value must be one of"
                         " start_dm, continuous, or auto.");
  }

  // The new parameters for thermal are set up here
  if(HasInitValue("temperature")) {
    temperature = GetRealInitValue("temperature", 0.); // Get temperature of simulation
  }

  if(HasInitValue("tempscript")) { // Get time dependent multiplier to scale temperature
    has_tempscript=1;
    String cmdoptreq = GetStringInitValue("tempscript_args",
                                          "stage stage_time total_time");
    tempscript_opts.push_back(Nb_TclCommandLineOption("stage",1));
    tempscript_opts.push_back(Nb_TclCommandLineOption("stage_time",1));
    tempscript_opts.push_back(Nb_TclCommandLineOption("total_time",1));
    tempscript_cmd.SetBaseCommand(InstanceName(),
				  director->GetMifInterp(),
				  GetStringInitValue("tempscript"),
				  Nb_ParseTclCommandLineRequest(InstanceName(),
								 tempscript_opts,
								 cmdoptreq));
  }
  if(HasInitValue("uniform_seed")) {
    srand(time(NULL));
    uniform_seed = GetIntInitValue("uniform_seed",(rand() % 65533 + 2)); // Default seed value is time dependent
    has_unifSeed = 1;
  } else {
    has_unifSeed = 0;
  }

  // Re-initialize random number generator based on defined seed
  if(has_unifSeed) {
    Oc_Srand(uniform_seed);
  } else {
    Oc_Srand();
  }

  if(temperature>0.) {
    if(min_timestep==0.) {
      min_timestep=1e-15;
    }
    max_timestep = min_timestep;      //set temperature to zero to get an estimate for a reasonable stepsize
                                      //or use it for comparison (acts like eulerevolve with temperature=0K)
    min_step_headroom = 0.001;
    max_step_headroom = 1.0;
    allowed_error_rate = PI*1e9;
    allowed_absolute_step_error = PI;
    allowed_relative_step_error = PI;
    String method = GetStringInitValue("method","rk2heun");
    Oxs_ToLower(method); // Do case insensitive match
    if(method.compare("rk2heun")==0) {
      rkstep_ptr = &Xf_ThermHeunEvolve::TakeRungeKuttaStep2Heun;
    } else {
      throw Oxs_ExtError(this,"Invalid initialization detected:"
			      " \"method\" value must be one of"
			      " rk2heun for non-zero temperature.");
    }
  } else {
    String method = GetStringInitValue("method","rkf54");
    Oxs_ToLower(method); // Do case insensitive match
    if(method.compare("rk2")==0) {
      rkstep_ptr = &Xf_ThermHeunEvolve::TakeRungeKuttaStep2;
    } else if(method.compare("rk2heun")==0) {
      rkstep_ptr = &Xf_ThermHeunEvolve::TakeRungeKuttaStep2Heun;
    } else if(method.compare("rk4")==0) {
      rkstep_ptr = &Xf_ThermHeunEvolve::TakeRungeKuttaStep4;
    } else if(method.compare("rkf54m")==0) {
      rkstep_ptr = &Xf_ThermHeunEvolve::TakeRungeKuttaFehlbergStep54M;
    } else if(method.compare("rkf54s")==0) {
      rkstep_ptr = &Xf_ThermHeunEvolve::TakeRungeKuttaFehlbergStep54S;
    } else if(method.compare("rkf54")==0) {
      rkstep_ptr = &Xf_ThermHeunEvolve::TakeRungeKuttaFehlbergStep54;
    } else {
      throw Oxs_ExtError(this,"Invalid initialization detected:"
			      " \"method\" value must be one of"
			      " rk2, rk2heun, rk4, rk54, rkf54m, or rk54s.");
    }
  }

  VerifyAllInitArgsUsed();

  // Setup outputs
  max_dm_dt_output.Setup(this,InstanceName(),"Max dm/dt","deg/ns",0,
     &Xf_ThermHeunEvolve::UpdateDerivedOutputs);
  dE_dt_output.Setup(this,InstanceName(),"dE/dt","J/s",0,
     &Xf_ThermHeunEvolve::UpdateDerivedOutputs);
  delta_E_output.Setup(this,InstanceName(),"Delta E","J",0,
     &Xf_ThermHeunEvolve::UpdateDerivedOutputs);
  temperature_output.Setup(this,InstanceName(),"Temperature","K",0,
     &Xf_ThermHeunEvolve::UpdateTemperatureOutputs);
  dm_dt_output.Setup(this,InstanceName(),"dm/dt","rad/s",1,
     &Xf_ThermHeunEvolve::UpdateDerivedOutputs);
  mxH_output.Setup(this,InstanceName(),"mxH","A/m",1,
     &Xf_ThermHeunEvolve::UpdateDerivedOutputs);

  // Register outputs
  max_dm_dt_output.Register(director,-5);
  dE_dt_output.Register(director,-5);
  delta_E_output.Register(director,-5);
  temperature_output.Register(director,-5);
  dm_dt_output.Register(director,-5);
  mxH_output.Register(director,-5);

  // dm_dt and mxH output caches are used for intermediate storage,
  // so enable caching.
  dm_dt_output.CacheRequestIncrement(1);
  mxH_output.CacheRequestIncrement(1);

  // Reserve space for temp_state; see Step() method below
  director->ReserveSimulationStateRequest(1);
}

OC_BOOL Xf_ThermHeunEvolve::Init()
{
  // Free scratch space allocated by previous problem (if any)
  vtmpA.Release();   vtmpB.Release();
  vtmpC.Release();   vtmpD.Release();

  mesh_id = 0;

  // Free memory used by LLG damping parameter alpha
  alpha.Release();
  gamma.Release();

  max_step_increase = max_step_increase_limit;

  // Setup step_headroom and reject_ratio
  step_headroom = max_step_headroom;
  reject_ratio = reject_goal;

  energy_state_id=0;   // Mark as invalid state
  next_timestep=0.;    // Dummy value

  return Oxs_TimeEvolver::Init();
}


Xf_ThermHeunEvolve::~Xf_ThermHeunEvolve()
{}

void Xf_ThermHeunEvolve::UpdateMeshArrays(const Oxs_Mesh* mesh)
{
  mesh_id = 0; // Mark update in progress

  const Oxs_RectangularMesh* rmesh
    = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
  if(rmesh==NULL) {
    String msg="Import mesh to Xf_ThermHeunEvolve::UpdateMeshArrays"
      " is not an Oxs_RectangularMesh object.";
    throw Oxs_ExtError(msg.c_str());
  }

  const OC_INDEX size = mesh->Size();
  OC_INDEX i;

  alpha_init->FillMeshValue(mesh,alpha);
  gamma_init->FillMeshValue(mesh,gamma);

  if(gamma_style == GS_G) { // Convert to LL form
    for(i=0;i<size;++i) {
      OC_REAL8m cell_alpha = alpha[i];
      gamma[i] /= (1+cell_alpha*cell_alpha);
    }
  }
  if(!allow_signed_gamma) {
    for(i=0;i<size;++i) gamma[i] = -1*fabs(gamma[i]);
  }

  mesh_id = mesh->Id();
}


OC_REAL8m
Xf_ThermHeunEvolve::PositiveTimestepBound
(OC_REAL8m max_dm_dt)
{ // Computes an estimate on the minimum time needed to
  // change the magnetization state, subject to floating
  // points limits.
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

void Xf_ThermHeunEvolve::Calculate_dm_dt
(const Oxs_SimState& state_,
 const Oxs_MeshValue<ThreeVector>& mxH_,
 OC_REAL8m pE_pt_,
 Oxs_MeshValue<ThreeVector>& dm_dt_,
 OC_REAL8m& max_dm_dt_,OC_REAL8m& dE_dt_,OC_REAL8m& min_timestep_)
{ // Imports: state, mxH_, pE_pt_
  // Exports: dm_dt_, max_dm_dt_, dE_dt_
  //
  // NOTE 1: dm_dt_ is allowed, and in fact is encouraged,
  //   to be the same as mxH_.  In this case, mxH_ is
  //   overwritten by dm_dt on return.
  //

  const Oxs_Mesh* mesh = state_.mesh;

  const Oxs_MeshValue<OC_REAL8m>& Ms_ = *(state_.Ms);
  const Oxs_MeshValue<OC_REAL8m>& Ms_inverse_ = *(state_.Ms_inverse);
  const Oxs_MeshValue<ThreeVector>& spin_ = state_.spin;
  const OC_INDEX size = mesh->Size(); // Assume import data are compatible

  // Move mxH_ data into dm_dt_.  This is fallback behavior for
  // the case where mxH_ and dm_dt_ are not physically the same
  // storage.  The operator=() member function of Oxs_MeshValue
  // is smart enough to turn this into a NOP if &dm_dt_ == &mxH_.
  // NOTE: dm_dt_ and mxH_ MAY BE THE SAME, so don't use mxH_
  // past this point!
  dm_dt_ = mxH_;

  // Zero torque data on fixed spins
  OC_INDEX i,j;
  UpdateFixedSpinList(mesh);
  const OC_INDEX fixed_count = GetFixedSpinCount();
  for(j=0;j<fixed_count;j++) {
    dm_dt_[GetFixedSpin(j)].Set(0.,0.,0.);
  }

  // Check and update as necessary spin-transfer polarization
  // field data structures
  if(mesh_id != mesh->Id()) {
    // This is either the first pass through, or else mesh
    // has changed.
    UpdateMeshArrays(mesh);
    Hfluct.AdjustSize(mesh);
    h_fluctVarConst.AdjustSize(mesh);
    FillHfluctConst(mesh);
    InitHfluct(mesh);
  }


  // Compute dm_dt and dE_dt.  For details, see mjd's NOTES III,
  // 27-28 July 2004, pp. 186-193, and 15-Aug-2004, pp. 197-199.

  OC_REAL8m dE_dt_sum=0.0;
  OC_REAL8m max_dm_dt_sq = 0.0;

  const OC_REAL8m Tmult = EvaluateTemperatureProfileScript(state_.stage_number,
							  state_.stage_elapsed_time,
							  state_.stage_start_time+state_.stage_elapsed_time);
  if(temperature_output.GetCacheRequestCount()>0) {
    temperature_output.cache.value = temperature * Tmult;
    temperature_output.cache.state_id=state_.Id();
  }

  for(i=0;i<size;i++) {
    if(Ms_[i]==0) {
      dm_dt_[i].Set(0.0,0.0,0.0);
    } else {
      const OC_REAL8m cell_alpha = alpha[i];
      const OC_REAL8m cell_mgamma = -1*gamma[i]; // -1 * Landau-Lifshitz gamma

      ThreeVector m = spin_[i];

      ThreeVector mxHfluct = m; // For fluctuating field

      ThreeVector mxH = dm_dt_[i];

      ThreeVector scratchA = mxH;
      ThreeVector tmpHfluct; // Use temporary variable to store fluctuation vector
      // h_fluctVarConst stores 2*kB*alpha/((1+alpha^2)*gamma*MU0*Vol)
      if(temperature>0.) {
	tmpHfluct = Hfluct[i];                             // Standard normal random variable (eta)
	tmpHfluct *= sqrt(h_fluctVarConst[i]*Ms_inverse_[i]*Tmult); // eta*sqrt(2*kB*T*alpha/((1+alpha^2)*gamma*MU0*Ms*Vol*dt))
	mxHfluct ^= tmpHfluct;
	scratchA += mxHfluct;
      }

      OC_REAL8m mxH_sq = scratchA.MagSq();
      scratchA *= cell_alpha;
      dE_dt_sum += (mxH_sq)*Ms_[i]*mesh->Volume(i)*cell_mgamma;

      /// This still needs to be multiplied by MU0 and added
      /// to pE_pt (see below).

      scratchA ^= m;
      if(do_precess) {
	if(temperature>0.) {
	  scratchA -= (mxH + mxHfluct);
	} else {
	  scratchA -= mxH;
	}
      }

      scratchA *= cell_mgamma;
      dm_dt_[i] = scratchA;

#if 0
      if(i==size/2) {
        fprintf(stdout,"  dm_dt=(%g,%g,%g)\n",dm_dt_[i].x,dm_dt_[i].y,dm_dt_[i].z);
      }
#endif

      OC_REAL8m dm_dt_sq = scratchA.MagSq();
      if(dm_dt_sq>max_dm_dt_sq) {
        max_dm_dt_sq = dm_dt_sq;
      }
    }
  }
  max_dm_dt_ = sqrt(max_dm_dt_sq);
  dE_dt_ =  pE_pt_ + dE_dt_sum * MU0;

  // Get bound on smallest stepsize that would actually
  // change spin new_max_dm_dt_index:
  if(temperature>0.) {
    min_timestep_ = tmpStepsize;
  } else {
    min_timestep_ = PositiveTimestepBound(max_dm_dt_);
  }

  return;
}

void Xf_ThermHeunEvolve::CheckCache(const Oxs_SimState& cstate)
{
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
       "Xf_ThermHeunEvolve::CheckCache: Invalid data cache.");
  }
}


void
Xf_ThermHeunEvolve::AdjustState
(OC_REAL8m hstep,
 OC_REAL8m mstep,
 const Oxs_SimState& old_state,
 const Oxs_MeshValue<ThreeVector>& dm_dt,
 Oxs_SimState& new_state,
 OC_REAL8m& norm_error) const
{
  new_state.ClearDerivedData();

  const Oxs_MeshValue<ThreeVector>& old_spin = old_state.spin;
  Oxs_MeshValue<ThreeVector>& new_spin = new_state.spin;


  if(!dm_dt.CheckMesh(old_state.mesh)) {
    throw Oxs_ExtError(this,
                         "Xf_ThermHeunEvolve::AdjustState:"
                         " Import spin and dm_dt are different sizes.");
  }
  new_spin.AdjustSize(old_state.mesh);
  const OC_INDEX size = old_state.mesh->Size();

  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  ThreeVector tempspin;
  OC_INDEX i;
  for(i=0;i<size;++i) {
    tempspin = dm_dt[i];
    tempspin *= mstep;

#ifdef OLDE_CODE
    // For improved accuracy, adjust step vector so that
    // to first order m0 + adjusted_step = v/|v| where
    // v = m0 + step.
    OC_REAL8m adj = 0.5 * tempspin.MagSq();
    tempspin -= adj*old_spin[i];
    tempspin *= 1.0/(1.0+adj);
#endif // OLDE_CODE

    tempspin += old_spin[i];
    OC_REAL8m magsq = tempspin.MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;

    new_spin[i] = tempspin;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));

  // Adjust time and iteration fields in new_state
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
  // to Xf_ThermHeunEvolve::Step() takes 2 half-steps, and it is the
  // result from these half-steps that are used as the export state.
  // If we increment the iteration count each time through here, then
  // the iteration count goes up by 2 for each call to Step().  So
  // instead, we leave iteration count at whatever value was filled
  // in by the Xf_ThermHeunEvolve::NegotiateTimeStep() method.
}


void Xf_ThermHeunEvolve::UpdateTimeFields
(const Oxs_SimState& cstate,
 Oxs_SimState& nstate,
 OC_REAL8m stepsize) const
{
  nstate.last_timestep=stepsize;
  if(cstate.stage_number != nstate.stage_number) {
    // New stage
    nstate.stage_start_time = cstate.stage_start_time
                              + cstate.stage_elapsed_time;
    nstate.stage_elapsed_time = stepsize;
  } else {
    nstate.stage_start_time = cstate.stage_start_time;
    nstate.stage_elapsed_time = cstate.stage_elapsed_time
                                + stepsize;
  }
}

void Xf_ThermHeunEvolve::NegotiateTimeStep
(const Oxs_TimeDriver* driver,
 const Oxs_SimState&  cstate,
 Oxs_SimState& nstate,
 OC_REAL8m stepsize,
 OC_BOOL use_start_dm,
 OC_BOOL& force_step,
 OC_BOOL& driver_set_step) const
{ // This routine negotiates with driver over the proper step size.
  // As a side-effect, also initializes the nstate data structure,
  // where nstate is the "next state".

  // Pull needed cached values out from cstate.
  OC_REAL8m max_dm_dt;
  if(!cstate.GetDerivedData("Max dm/dt",max_dm_dt)) {
    throw Oxs_ExtError(this,
       "Xf_ThermHeunEvolve::NegotiateTimeStep: max_dm_dt not cached.");
  }
  OC_REAL8m timestep_lower_bound=0.;  // Smallest timestep that can actually
  /// change spin with max_dm_dt (due to OC_REAL8_EPSILON restrictions).
  if(!cstate.GetDerivedData("Timestep lower bound",
                            timestep_lower_bound)) {
    throw Oxs_ExtError(this,
       "Xf_ThermHeunEvolve::NegotiateTimeStep: "
       " timestep_lower_bound not cached.");
  }
  if(timestep_lower_bound<=0.0) {
    throw Oxs_ExtError(this,
       "Xf_ThermHeunEvolve::NegotiateTimeStep: "
       " cached timestep_lower_bound value not positive.");
  }

  // The next timestep is based on the error from the last step.  If
  // there is no last step (either because this is the first step,
  // or because the last state handled by this routine is different
  // from the incoming current_state), then timestep is calculated
  // so that max_dm_dt * timestep = start_dm.
  if(use_start_dm || stepsize<=0.0) {
    if(start_dm < sqrt(DBL_MAX/4) * max_dm_dt) {
      stepsize = step_headroom * start_dm / max_dm_dt;
    } else {
      stepsize = sqrt(DBL_MAX/4);
    }
  }

  // Insure step is not outside requested step bounds
  if(!use_start_dm && stepsize<timestep_lower_bound) {
    // Check for this before max_timestep, so max_timestep can
    // override.  On the one hand, if the timestep is too small to
    // move any spins, then taking the step just advances the
    // simulation time without changing the state; instead what would
    // be small accumulations are just lost.  On the other hand, if
    // the applied field is changing with time, then perhaps all that
    // is needed to get the magnetization moving is to advance the
    // simulation time.  In general, it is hard to tell which is
    // which, so we assume that the user knews what he was doing when
    // he set the max_timestep value (or accepted the default), and it
    // is up to him to notice if the simulation time is advancing
    // without any changes to the magnetization pattern.
    stepsize = timestep_lower_bound;
  }
  if(stepsize>max_timestep) stepsize = max_timestep;
  if(stepsize<min_timestep) stepsize = min_timestep;

  // Negotiate with driver over size of next step
  driver->FillStateMemberData(cstate,nstate);
  UpdateTimeFields(cstate,nstate,stepsize);

  // Update iteration count
  nstate.iteration_count = cstate.iteration_count + 1;
  nstate.stage_iteration_count = cstate.stage_iteration_count + 1;

  // Additional timestep control
  driver->FillStateSupplemental(nstate);

  // Check for forced step.
  // Note: The driver->FillStateSupplemental call may increase the
  //       timestep slightly to match a stage time termination
  //       criteria.  We should tweak the timestep size check
  //       to recognize that changes smaller than a few epsilon
  //       of the stage time are below our effective timescale
  //       resolution.
  force_step = 0;
  OC_REAL8m timestepcheck = nstate.last_timestep
                         - 4*OC_REAL8_EPSILON*nstate.stage_elapsed_time;
  if(timestepcheck<=min_timestep || timestepcheck<=timestep_lower_bound) {
    // Either driver wants to force stepsize, or else step size can't
    // be reduced any further.
    force_step=1;
  }

  // Is driver responsible for stepsize?
  if(nstate.last_timestep == stepsize) driver_set_step = 0;
  else                                 driver_set_step = 1;
}


OC_BOOL
Xf_ThermHeunEvolve::CheckError
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

void Xf_ThermHeunEvolve::TakeRungeKuttaStep2
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_REAL8m& min_dE_dt, OC_REAL8m& max_dE_dt,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // This routine performs a second order Runge-Kutta step, with
  // error estimation.  The form used is the "modified Euler"
  // method due to Collatz (1960).
  //
  // A general RK2 step involves
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+a.h,m1+a.h.dm_dt1)
  //  m2 = m1 + h.( (1-1/(2a)).dm_dt1 + (1/(2a)).dm_dt2 )
  //
  // where 0<a<=1 is a free parameter.  Taking a=1/2 gives
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h/2,m1+dm_dt1.h/2)
  //  m2 = m1 + dm_dt2*h + O(h^3)
  //
  // This is the "modified Euler" method from Collatz (1960).
  // Setting a=1 yields
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h,m1+dm_dt1.h)
  //  m2 = m1 + (dm_dt1 + dm_dt2).h/2 + O(h^3)
  //
  // which is the method of Heun (1900).  For details see
  // J. Stoer and R. Bulirsch, "Introduction to Numerical
  // Analysis," Springer 1993, Section 7.2.1, p438.
  //
  // In the code below, we use the "modified Euler" approach,
  // i.e., select a=1/2.

  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());

  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound,dummy_error;

  // Calculate dm_dt2
  AdjustState(stepsize/2,stepsize/2,*cstate,current_dm_dt,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  tmpStepsize = stepsize;
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  min_dE_dt=max_dE_dt=dE_dt;

  // vtmpA holds dm_dt2
  Oxs_SimState* nstate = &(next_state_key.GetWriteReference());
  nstate->ClearDerivedData();
  nstate->spin = cstate->spin;
  nstate->spin.Accumulate(stepsize,vtmpA);

  // Tweak "last_timestep" field in next_state, and adjust other
  // time fields to protect against rounding errors.
  UpdateTimeFields(*cstate,*nstate,stepsize);

  // Normalize spins in nstate, and collect norm error info
  // Normalize m2, including norm error check
  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  for(i=0;i<size;++i) {
    OC_REAL8m magsq = nstate->spin[i].MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));
  const Oxs_SimState* endstate
    = &(next_state_key.GetReadReference()); // Lock down

  // To estimate error, compute dm_dt at end state.
  GetEnergyDensity(*endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt);
  mxH_output.cache.state_id=endstate->Id();
  Calculate_dm_dt(*endstate,mxH_output.cache.value,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  if(!endstate->AddDerivedData("Timestep lower bound",
                                timestep_lower_bound) ||
     !endstate->AddDerivedData("Max dm/dt",max_dm_dt) ||
     !endstate->AddDerivedData("pE/pt",pE_pt) ||
     !endstate->AddDerivedData("dE/dt",dE_dt)) {
    throw Oxs_ExtError(this,
                         "Xf_ThermHeunEvolve::TakeRungeKuttaStep2:"
                         " Programming error; data cache already set.");
  }

  // Best guess at error compares computed endpoint against what
  // we would get if we ran a Heun-type step with dm/dt at this
  // endpoint.
  OC_REAL8m max_err_sq = 0.0;
  for(i=0;i<size;++i) {
    ThreeVector tvec = current_dm_dt[i];
    tvec += vtmpB[i];
    tvec *= 0.5;
    tvec -= vtmpA[i];
    OC_REAL8m err_sq = tvec.MagSq();
    if(err_sq>max_err_sq) max_err_sq = err_sq;
  }
  error_estimate = sqrt(max_err_sq)*stepsize;
  global_error_order = 2.0;

  // Move end dm_dt data into vtmpA, for use by calling routine.
  // Note that end energy is already in temp_energy, as per
  // contract.
  vtmpA.Swap(vtmpB);
  new_energy_and_dmdt_computed = 1;
}

void Xf_ThermHeunEvolve::TakeRungeKuttaStep2Heun
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_REAL8m& min_dE_dt, OC_REAL8m& max_dE_dt,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // This routine performs a second order Runge-Kutta step, with
  // error estimation.  The form used in this routine is the "Heun"
  // method.
  //
  // A general RK2 step involves
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+a.h,m1+a.h.dm_dt1)
  //  m2 = m1 + h.( (1-1/(2a)).dm_dt1 + (1/(2a)).dm_dt2 )
  //
  // where 0<a<=1 is a free parameter.  Taking a=1/2 gives
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h/2,m1+dm_dt1.h/2)
  //  m2 = m1 + dm_dt2*h + O(h^3)
  //
  // This is the "modified Euler" method from Collatz (1960).
  // Setting a=1 yields
  //
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h,m1+dm_dt1.h)
  //  m2 = m1 + (dm_dt1 + dm_dt2).h/2 + O(h^3)
  //
  // which is the method of Heun (1900).  For details see
  // J. Stoer and R. Bulirsch, "Introduction to Numerical
  // Analysis," Springer 1993, Section 7.2.1, p438.
  //
  // In the code below, we use the Heun approach,
  // i.e., select a=1.

  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());

  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound,dummy_error;

  // Calculate dm_dt2
  AdjustState(stepsize,stepsize,*cstate,current_dm_dt,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  UpdateHfluct(cstate->mesh);// Update the random field before calculating slopes
  // Oxs_SimState& workstate = next_state_key.GetWriteReference();
  tmpStepsize = stepsize;
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  min_dE_dt=max_dE_dt=dE_dt;
  // vtmpA holds dm_dt2
  vtmpC = vtmpA;  // Make a copy to be used in error control

  // Form 0.5*(dm_dt1+dm_dt2)
  vtmpA += current_dm_dt;
  vtmpA *= 0.5;

  // Create new state
  Oxs_SimState* nstate = &(next_state_key.GetWriteReference());
  nstate->ClearDerivedData();
  nstate->spin = cstate->spin;
  nstate->spin.Accumulate(stepsize,vtmpA);

  // Tweak "last_timestep" field in next_state, and adjust other
  // time fields to protect against rounding errors.
  UpdateTimeFields(*cstate,*nstate,stepsize);

  // Normalize spins in nstate, and collect norm error info
  // Normalize m2, including norm error check
  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  for(i=0;i<size;++i) {
    OC_REAL8m magsq = nstate->spin[i].MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));
  const Oxs_SimState* endstate
    = &(next_state_key.GetReadReference()); // Lock down

  // To estimate error, compute dm_dt at end state.
  GetEnergyDensity(*endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt);
  mxH_output.cache.state_id=endstate->Id();
  Calculate_dm_dt(*endstate,mxH_output.cache.value,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  if(!endstate->AddDerivedData("Timestep lower bound",
                                timestep_lower_bound) ||
     !endstate->AddDerivedData("Max dm/dt",max_dm_dt) ||
     !endstate->AddDerivedData("pE/pt",pE_pt) ||
     !endstate->AddDerivedData("dE/dt",dE_dt)) {
    throw Oxs_ExtError(this,
                         "Xf_ThermHeunEvolve::TakeRungeKuttaStep2Heun:"
                         " Programming error; data cache already set.");
  }

  // Best guess at error compares computed endpoint against what
  // we would get if we ran a Heun-type step with dm/dt at this
  // endpoint.
  OC_REAL8m max_err_sq = 0.0;
  for(i=0;i<size;++i) {
    ThreeVector tvec = vtmpC[i] - vtmpB[i];
    OC_REAL8m err_sq = tvec.MagSq();
    if(err_sq>max_err_sq) max_err_sq = err_sq;
  }
  error_estimate = sqrt(max_err_sq)*stepsize/2.;
  global_error_order = 2.0;

  // Move end dm_dt data into vtmpA, for use by calling routine.
  // Note that end energy is already in temp_energy, as per
  // contract.
  vtmpA.Swap(vtmpB);
  new_energy_and_dmdt_computed = 1;
}

void Xf_ThermHeunEvolve::TakeRungeKuttaStep4
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_REAL8m& min_dE_dt, OC_REAL8m& max_dE_dt,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // This routine performs two successive "classical" Runge-Kutta
  // steps of size stepsize/2, and stores the resulting magnetization
  // state into the next_state export.  Additionally, a single
  // step of size stepsize is performed, and used for estimating
  // the error.  This error is returned in the export error_estimate;
  // This is the largest error detected cellwise, in radians.  The
  // export global_error_order is always set to 4 by this routine.
  // (The local error order is one better, i.e., 5.)  The norm_error
  // export is set to the cellwise maximum deviation from unit norm
  // across all the spins in the final state, before renormalization.

  // A single RK4 step involves
  //  dm_dt1 = dm_dt(t1,m1)
  //  dm_dt2 = dm_dt(t1+h/2,m1+dm_dt1*h/2)
  //  dm_dt3 = dm_dt(t1+h/2,m1+dm_dt2*h/2)
  //  dm_dt4 = dm_dt(t1+h,m1+dm_dt3*h)
  //  m2 = m1 + dm_dt1*h/6 + dm_dt2*h/3 + dm_dt3*h/3 + dm_dt4*h/6 + O(h^5)
  // To improve accuracy, for each step accumulate dm_dt?, where
  // ?=1,2,3,4, into m2 with proper weights, and add in m1 at the end.

  // To calculate dm_dt, we first fill next_state with the proper
  // spin data (e.g., m1+dm_dt2*h/2).  The utility routine AdjustState
  // is applied for this purpose.  Then next_state is locked down,
  // so that it will have a valid state_id, and GetEnergyDensity
  // is called in order to get mxH and related data.  This is then
  // passed to Calculate_dm_dt.  Note that dm_dt is not calculated
  // for the final state.  This is left for the calling routine,
  // which first examines the error estimates to decide whether
  // or not the step will be accepted.  If the step is rejected,
  // then the energy and dm_dt do not need to be calculated.

  // Scratch space usage:
  //   vtmpA is used to store, successively, dm_dt2, dm_dt3 and
  //      dm_dt4.  In calculating dm_dt?, vtmpA is first filled
  //      with mxH by the GetEnergyDensity() routine.
  //      Calculate_dm_dt() allows import mxH and export dm_dt
  //      to be the same MeshValue structure.
  //   vtmpB is used to accumulate the weighted sums of the dm_dt's,
  //      as explained above.  To effect a small efficiency gain,
  //      dm_dt1 is calculated directly into vtmpB, in the same
  //      manner as the other dm_dt's are filled into vtmpA.
  //   tempState is used to hold the middle state when calculating
  //      the two half steps, and the end state for the single
  //      full size step.

  // Any locks arising from temp_state_key will be released when
  // temp_state_key is destroyed on method exit.
  Oxs_SimState* nstate = &(next_state_key.GetWriteReference());
  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());
  Oxs_Key<Oxs_SimState> temp_state_key;
  director->GetNewSimulationState(temp_state_key);
  Oxs_SimState* tstate = &(temp_state_key.GetWriteReference());
  nstate->CloneHeader(*tstate);

  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound;
  OC_REAL8m dummy_error;

  // Do first half step.  Because dm_dt1 is already calculated,
  // we fill dm_dt2 directly into vtmpB.
  AdjustState(stepsize/4,stepsize/4,*cstate,current_dm_dt,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  tmpStepsize = stepsize;
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  min_dE_dt = max_dE_dt = dE_dt;
  // vtmpB currently holds dm_dt2
  AdjustState(stepsize/4,stepsize/4,*cstate,vtmpB,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt2 + dm_dt3.
  AdjustState(stepsize/2,stepsize/2,*cstate,vtmpA,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  // vtmpA holds dm_dt4
  vtmpA += current_dm_dt;
  vtmpB.Accumulate(0.5,vtmpA); // Add in 0.5*(dm_dt1+dm_dt4)
  tstate = &(temp_state_key.GetWriteReference());
  tstate->ClearDerivedData();
  tstate->spin = cstate->spin;
  tstate->spin.Accumulate(stepsize/6.,vtmpB);
  // Note: state time index set to lasttime + stepsize/2
  // by dm_dt4 calculation above.  Note that "h" here is
  // stepsize/2, so the weights on dm_dt1, dm_dt2, dm_dt3 and
  // dn_dt4 are (stepsize/2)(1/6, 1/3, 1/3, 1/6), respectively.

  // Save vtmpB for error estimate
  vtmpB.Swap(vtmpC);

  // Normalize spins in tstate
  for(i=0;i<size;++i) tstate->spin[i].MakeUnit();

  // At this point, temp_state holds the middle point.
  // Calculate dm_dt for this state, and store in vtmpB.
  tstate = NULL; // Disable non-const access
  const Oxs_SimState* midstate
    = &(temp_state_key.GetReadReference());
  GetEnergyDensity(*midstate,temp_energy,&vtmpB,NULL,pE_pt);
  Calculate_dm_dt(*midstate,vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;

  // Next do second half step.  Store end result in next_state
  AdjustState(stepsize/4,stepsize/4,*midstate,vtmpB,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  // vtmpB currently holds dm_dt1, vtmpA holds dm_dt2
  vtmpB *= 0.5;
  vtmpB += vtmpA;
  AdjustState(stepsize/4,stepsize/4,*midstate,vtmpA,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt1/2 + dm_dt2 + dm_dt3.
  AdjustState(stepsize/2,stepsize/2,*midstate,vtmpA,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  // vtmpA holds dm_dt4
  vtmpB.Accumulate(0.5,vtmpA);
  nstate = &(next_state_key.GetWriteReference());
  nstate->ClearDerivedData();
  nstate->spin = midstate->spin;
  nstate->spin.Accumulate(stepsize/6.,vtmpB);
  midstate = NULL; // We're done using midstate

  // Combine vtmpB with results from first half-step.
  // This is used for error estimation.
  vtmpC += vtmpB;

  // Tweak "last_timestep" field in next_state, and adjust other
  // time fields to protect against rounding errors.
  UpdateTimeFields(*cstate,*nstate,stepsize);

  // Normalize spins in nstate, and collect norm error info
  // Normalize m2, including norm error check
  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  for(i=0;i<size;++i) {
    OC_REAL8m magsq = nstate->spin[i].MakeUnit();
    if(magsq<min_normsq) min_normsq=magsq;
    if(magsq>max_normsq) max_normsq=magsq;
  }
  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));
  nstate = NULL;
  next_state_key.GetReadReference(); // Lock down (safety)

  // Repeat now for full step, storing end result into temp_state
  AdjustState(stepsize/2,stepsize/2,*cstate,current_dm_dt,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  // vtmpB currently holds dm_dt2
  AdjustState(stepsize/2,stepsize/2,*cstate,vtmpB,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  vtmpB += vtmpA;
  // vtmpA holds dm_dt3, vtmpB holds dm_dt2 + dm_dt3.
  AdjustState(stepsize,stepsize,*cstate,vtmpA,
              temp_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(temp_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  // vtmpA holds dm_dt4
  vtmpA += current_dm_dt;
  vtmpB.Accumulate(0.5,vtmpA); // Add in 0.5*(dm_dt1+dm_dt4)
  tstate = &(temp_state_key.GetWriteReference());
  tstate->ClearDerivedData();  // Safety
  tstate->spin = cstate->spin;
  tstate->spin.Accumulate(stepsize/3.,vtmpB);
  for(i=0;i<size;++i) tstate->spin[i].MakeUnit(); // Normalize
  tstate = NULL; // Disable write access

#if 0
  // Error is max|next_state.spin-temp_state.spin|/15 + O(stepsize^6)
  const Oxs_SimState* teststate = &(temp_state_key.GetReadReference());
  const Oxs_SimState* endstate = &(next_state_key.GetReadReference());
  OC_REAL8m max_error_sq = 0.0;
  for(i=0;i<size;++i) {
    ThreeVector tvec = endstate->spin[i] - teststate->spin[i];
    OC_REAL8m error_sq = tvec.MagSq();
    if(error_sq>max_error_sq) max_error_sq = error_sq;
    // If we want, we can experiment with adding
    // tvec/15 to endstate->spin.  In theory, this changes
    // the error from O(stepsize^5) to O(stepsize^6)
  }
  error_estimate = sqrt(max_error_sq)/15.;
#else
  // Estimate error
  OC_REAL8m max_error_sq = 0.0;
  for(i=0;i<size;++i) {
    ThreeVector tvec = vtmpB[i];
    tvec.Accum(-0.5,vtmpC[i]);  // 0.5 because vtmpC holds sum of
                               /// two half-steps.
    OC_REAL8m error_sq = tvec.MagSq();
    if(error_sq>max_error_sq) max_error_sq = error_sq;
  }
  error_estimate = sqrt(max_error_sq)*stepsize/3.;
  /// vtmpB hold 0.5*dm_dt1 + dm_dt2 + dm_dt3 + 0.5*dm_dt4,
  /// but successor state looks like
  ///    m2 = m1 + dm_dt1*h/6 + dm_dt2*h/3 + dm_dt3*h/3 + dm_dt4*h/6 + O(h^5)
#endif
  global_error_order = 4.0;
  new_energy_and_dmdt_computed = 0;
}

void Xf_ThermHeunEvolve::RungeKuttaFehlbergBase54
(RKF_SubType method,
 OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_REAL8m& min_dE_dt, OC_REAL8m& max_dE_dt,
 OC_BOOL& new_energy_and_dmdt_computed)
{ // Runge-Kutta-Fehlberg routine with combined 4th and 5th
  // order Runge-Kutta steps.  The difference between the
  // two results (4th vs. 5th) is used to estimate the error.
  // The largest detected error detected cellsize is returned
  // in export error_estimate.  The units on this are radians.
  // The export global_error_order is set to 4 by this routine.
  // (The local error order is one better, i.e., 5.)  The norm_error
  // export is set to the cellwise maximum deviation from unit norm
  // across all the spins in the final state, before renormalization.

  // The following coefficients appear in
  //
  //   J. R. Dormand and P. J. Prince, ``A family of embedded
  //   Runge-Kutta formulae,'' J. Comp. Appl. Math., 6, 19--26
  //   (1980).
  //
  // They are also listed in J. Stoer and R. Bulirsch's book,
  // ``Introduction to Numerical Analysis,'' Springer, 2nd edition,
  // Sec. 7.2.5, p 454, but there are a number of errors in the S&B
  // account; the reader is recommended to refer directly to the D&P
  // paper.  A follow-up paper,
  //
  //   J. R. Dormand and P. J. Prince, ``A reconsieration of some
  //   embedded Runge-Kutta formulae,'' J. Comp. Appl. Math., 15,
  //   203--211 (1986)
  //
  // provides formulae with improved stability and higher order.
  // See also
  //
  //   J. H. Williamson, ``Low-Storage Runge-Kutta Schemes,''
  //   J. Comp. Phys., 35, 48--56 (1980).
  //
  // FORMULAE for RK5(4)7FM:
  //
  //     dm_dt1 = dm_dt(t1,m1)
  //     dm_dt2 = dm_dt(t1+ (1/5)*h, m1+h*k1);
  //     dm_dt3 = dm_dt(t1+(3/10)*h, m1+h*k2);
  //     dm_dt4 = dm_dt(t1+ (4/5)*h, m1+h*k3);
  //     dm_dt5 = dm_dt(t1+ (8/9)*h, m1+h*k4);
  //     dm_dt6 = dm_dt(t1+     1*h, m1+h*k5);
  //     dm_dt7 = dm_dt(t1+     1*h, m1+h*k6);
  //  where
  //     k1 = dm_dt1*1/5
  //     k2 = dm_dt1*3/40       + dm_dt2*9/40
  //     k3 = dm_dt1*44/45      - dm_dt2*56/15      + dm_dt3*32/9
  //     k4 = dm_dt1*19372/6561 - dm_dt2*25360/2187 + dm_dt3*64448/6561
  //               - dm_dt4*212/729
  //     k5 = dm_dt1*9017/3168  - dm_dt2*355/33     + dm_dt3*46732/5247
  //               + dm_dt4*49/176   - dm_dt5*5103/18656
  //     k6 = dm_dt1*35/384     + 0                 + dm_dt3*500/1113
  //               + dm_dt4*125/192  - dm_dt5*2187/6784  + dm_dt6*11/84
  // Then
  //     Da = dm_dt1*35/384     + 0 + dm_dt3*500/1113   + dm_dt4*125/192
  //              - dm_dt5*2187/6784      + dm_dt6*11/84
  //     Db = dm_dt1*5179/57600 + 0 + dm_dt3*7571/16695 + dm_dt4*393/640
  //              - dm_dt5*92097/339200   + dm_dt6*187/2100   + dm_dt7*1/40
  // and
  //     m2a = m1 + h*Da
  //     m2b = m1 + h*Db.
  //
  // where m2a is the 5th order estimate, which is the candidate
  // for the next state, and m2b is the 4th order estimate used
  // only for error estimation/stepsize control.  Note that the
  // 4th order estimate uses more dm/dt evaluations (7) than the
  // 5th order method (6).  This is intentional; the coefficients
  // are selected to minimize error (see D&P paper cited above).
  // The extra calculation costs are minimal, because the 7th dm_dt
  // evaluation is at the candidate next state, so it is re-used
  // on the next step unless the step rejected.
  //
  // The error estimate is
  //
  //     E = |m2b-m2a| = h*|Db-Da| = C*h^5
  //
  // where C is a constant that can be estimated in terms of E.
  // Note that we don't need to know C in order to adjust the
  // stepsize, because stepsize adjustment involves only the
  // ratio of the old stepsize to the new, so C drops out.

  // Scratch space usage:
  //  The import next_state is used for intermediate state
  // storage for all dm_dt computations.  The final computation
  // is for dm_dt7 at m1+h*k6 = m1+h*Da, which is the candidate
  // next state.  (Da=k6; see FSAL note below.)

  // Four temporary arrays, A-D, are used:
  //
  // Step \  Temp
  // Index \ Array:  A         B         C         D
  // ------+---------------------------------------------
  //   1   |      dm_dt2       -         -         -
  //   2   |      dm_dt2      k2         -         -
  //   3   |      dm_dt2     dm_dt3      -         -
  //   4   |      dm_dt2     dm_dt3     k3         -
  //   5   |      dm_dt2     dm_dt3    dm_dt4      -
  //   6   |      dm_dt2     dm_dt3    dm_dt4     k4
  //   7   |      dm_dt2     dm_dt3    dm_dt4    dm_dt5
  //   8   |        k5       dm_dt3    dm_dt4    dm_dt5
  //   9   |      dm_dt6     dm_dt3    dm_dt4    dm_dt5
  //  10   |      k6(3,6)    dD(3,6)   dm_dt4    dm_dt5
  //  11   |      dm_dt7     dD(3,6)   dm_dt4    dm_dt5
  //  12   |      dm_dt7       dD      dm_dt4    dm_dt5
  //
  // Here dD os Db-Da.  We don't need to compute Db directly.
  // The parenthesized numbers, e.g., k6(3,6) indicates
  // a partially formed value.  The total value k6 depends
  // upon dm_dt1, dm_dt3, dm_dt4, dm_dt5, and dm_dt6.  But
  // if we form k6 directly at step 11 in array A, then we
  // lose dm_dt6 which is needed to compute dD.  Rather than
  // use an additional array, we accumulate partial results
  // into arrays A and B for k6 and dD as indicated.
  //   Note that Da = k6.  Further, note that dm_dt7 is
  // dm_dt for the next state candidate.  (This is the 'F',
  // short for 'FSAL' ("first same as last"?) in the method
  // name, RK5(4)7FM.  The 'M' means minimized error norm,
  // 7 is the number of stages, and 5(4) is the main/subsidiary
  // integration formula order.  See the D&P 1986 paper for
  // details and additional references.)

  // Coefficient arrays, a, b, dc, defined by:
  //
  //   dm_dtN = dm_dt(t1+aN*h,m1+h*kN)
  //       kN = \sum_{M=1}^{M=N} dm_dtM*bNM
  //  Db - Da = \sum dm_dtM*dcM
  //
  OC_REAL8m a1,a2,a3,a4; // a5 and a6 are 1.0
  OC_REAL8m b11,b21,b22,b31,b32,b33,b41,b42,b43,b44,
    b51,b52,b53,b54,b55,b61,b63,b64,b65,b66; // b62 is 0.0
  OC_REAL8m dc1,dc3,dc4,dc5,dc6,dc7;  // c[k] = b6k, and c^[2]=c[2]=0.0,
  /// where c are the coeffs for Da, c^ for Db, and dcM = c^[M]-c[M].

  switch(method) {
  case RK547FC:
    /////////////////////////////////////////////////////////////////
    // Coefficients for Dormand & Prince RK5(4)7FC
    a1 = 1./5.;
    a2 = 3./10.;
    a3 = 6./13.;
    a4 = 2./3.;
    // a5 and a6 are 1.0

    b11 =      1./5.;

    b21 =      3./40.;
    b22 =      9./40.;

    b31 =    264./2197.;
    b32 =    -90./2197.;
    b33 =    840./2197.;

    b41 =    932./3645.;
    b42 =    -14./27.;
    b43 =   3256./5103.;
    b44 =   7436./25515.;

    b51 =   -367./513.;
    b52 =     30./19.;
    b53 =   9940./5643.;
    b54 = -29575./8208.;
    b55 =   6615./3344.;

    b61 =     35./432.;
    b63 =   8500./14553.;
    b64 = -28561./84672.;
    b65 =    405./704.;
    b66 =     19./196.;
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =     11./108.    - b61;
    dc3 =   6250./14553.  - b63;
    dc4 =  -2197./21168.  - b64;
    dc5 =     81./176.    - b65;
    dc6 =    171./1960.   - b66;
    dc7 =      1./40.;
    break;

  case RK547FM:
    /////////////////////////////////////////////////////////////////
    // Coefficients for Dormand & Prince RK5(4)7FM
    a1 = 1./5.;
    a2 = 3./10.;
    a3 = 4./5.;
    a4 = 8./9.;
    // a5 and a6 are 1.0

    b11 =      1./5.;

    b21 =      3./40.;
    b22 =      9./40.;

    b31 =     44./45.;
    b32 =    -56./15.;
    b33 =     32./9.;

    b41 =  19372./6561.;
    b42 = -25360./2187.;
    b43 =  64448./6561.;
    b44 =   -212./729.;

    b51 =   9017./3168.;
    b52 =   -355./33.;
    b53 =  46732./5247.;
    b54 =     49./176.;
    b55 =  -5103./18656.;

    b61 =     35./384.;
    b63 =    500./1113.;
    b64 =    125./192.;
    b65 =  -2187./6784.;
    b66 =     11./84.;
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =   5179./57600.  - b61;
    dc3 =   7571./16695.  - b63;
    dc4 =    393./640.    - b64;
    dc5 = -92097./339200. - b65;
    dc6 =    187./2100.   - b66;
    dc7 =      1./40.;
    break;
  case RK547FS:
    /////////////////////////////////////////////////////////////////
    // Coefficients for Dormand & Prince RK5(4)7FS
    a1 = 2./9.;
    a2 = 1./3.;
    a3 = 5./9.;
    a4 = 2./3.;
    // a5 and a6 are 1.0

    b11 =      2./9.;

    b21 =      1./12.;
    b22 =      1./4.;

    b31 =     55./324.;
    b32 =    -25./108.;
    b33 =     50./81.;

    b41 =     83./330.;
    b42 =    -13./22.;
    b43 =     61./66.;
    b44 =      9./110.;

    b51 =    -19./28.;
    b52 =      9./4.;
    b53 =      1./7.;
    b54 =    -27./7.;
    b55 =     22./7.;

    b61 =     19./200.;
    b63 =      3./5.;
    b64 =   -243./400.;
    b65 =     33./40.;
    b66 =      7./80.;
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =    431./5000.  - b61;
    dc3 =    333./500.   - b63;
    dc4 =  -7857./10000. - b64;
    dc5 =    957./1000.  - b65;
    dc6 =    193./2000.  - b66;
    dc7 =     -1./50.;
    break;
  default:
    throw Oxs_ExtError(this,
                 "Xf_ThermHeunEvolve::RungeKuttaFehlbergBase54:"
                 " Programming error; Invalid sub-type.");
  }

#ifdef CODE_CHECKS
  // COEFFICIENT CHECKS ////////////////////////////////////////
  // Try to catch some simple typing errors
#define EPS (6*OC_REAL8_EPSILON)
  if(fabs(b11-a1)>EPS ||
     fabs(b21+b22-a2)>EPS ||
     fabs(b31+b32+b33-a3)>EPS ||
     fabs(b41+b42+b43+b44-a4)>EPS ||
     fabs(b51+b52+b53+b54+b55-1.0)>EPS ||
     fabs(b61+b63+b64+b65+b66-1.0)>EPS) {
    char buf[512];
    Oc_Snprintf(buf,sizeof(buf),
                "Coefficient check failed:\n"
                "1: %g\n2: %g\n3: %g\n4: %g\n5: %g\n6: %g",
                static_cast<double>(b11-a1),
                static_cast<double>(b21+b22-a2),
                static_cast<double>(b31+b32+b33-a3),
                static_cast<double>(b41+b42+b43+b44-a4),
                static_cast<double>(b51+b52+b53+b54+b55-1.0),
                static_cast<double>(b61+b63+b64+b65+b66-1.0));
    throw Oxs_ExtError(this,buf);
  }
#endif // CODE_CHECKS

  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound;
  OC_REAL8m dummy_error;

  // Step 1
  AdjustState(stepsize*a1,stepsize*b11,*cstate,current_dm_dt,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  tmpStepsize = stepsize;
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  min_dE_dt = max_dE_dt = dE_dt;

  // Steps 2 and 3
  vtmpB = current_dm_dt;
  vtmpB *= b21;
  vtmpB.Accumulate(b22,vtmpA);
  AdjustState(stepsize*a2,stepsize,*cstate,vtmpB,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpB,pE_pt,
                  vtmpB,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;

  // Steps 4 and 5
  vtmpC = current_dm_dt;
  vtmpC *= b31;
  vtmpC.Accumulate(b32,vtmpA);
  vtmpC.Accumulate(b33,vtmpB);
  AdjustState(stepsize*a3,stepsize,*cstate,vtmpC,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpC,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpC,pE_pt,
                  vtmpC,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;

  // Steps 6 and 7
  vtmpD = current_dm_dt;
  vtmpD *= b41;
  vtmpD.Accumulate(b42,vtmpA);
  vtmpD.Accumulate(b43,vtmpB);
  vtmpD.Accumulate(b44,vtmpC);
  AdjustState(stepsize*a4,stepsize,*cstate,vtmpD,
              next_state_key.GetWriteReference(),dummy_error);
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpD,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpD,pE_pt,
                  vtmpD,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  // Array holdings: A=dm_dt2   B=dm_dt3   C=dm_dt4   D=dm_dt5

  // Step 8 and 9
  vtmpA *= b52;
  vtmpA.Accumulate(b51,current_dm_dt);
  vtmpA.Accumulate(b53,vtmpB);
  vtmpA.Accumulate(b54,vtmpC);
  vtmpA.Accumulate(b55,vtmpD);
  AdjustState(stepsize,stepsize,*cstate,vtmpA,
              next_state_key.GetWriteReference(),dummy_error); // a5=1.0
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  Calculate_dm_dt(next_state_key.GetReadReference(),vtmpA,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  // Array holdings: A=dm_dt6   B=dm_dt3   C=dm_dt4   D=dm_dt5

  // Step 10
  OC_INDEX i;
  const OC_INDEX size = cstate->mesh->Size();
  for(i=0;i<size;i++) {
    ThreeVector dm_dt3 = vtmpB[i];
    ThreeVector dm_dt6 = vtmpA[i];
    vtmpA[i] = b63 * dm_dt3  +  b66 * dm_dt6;  // k6(3,6)
    vtmpB[i] = dc3 * dm_dt3  +  dc6 * dm_dt6;  // dD(3,6)
  }
  // Array holdings: A=k6(3,6)   B=dD(3,6)   C=dm_dt4   D=dm_dt5

  // Step 11
  vtmpA.Accumulate(b61,current_dm_dt);   // Note: b62=0.0
  vtmpA.Accumulate(b64,vtmpC);
  vtmpA.Accumulate(b65,vtmpD);
  AdjustState(stepsize,stepsize,*cstate,vtmpA,
              next_state_key.GetWriteReference(),norm_error); // a6=1.0
  const Oxs_SimState& endstate
    = next_state_key.GetReadReference(); // Candidate next state
  GetEnergyDensity(endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt);
  mxH_output.cache.state_id=endstate.Id();
  Calculate_dm_dt(endstate,mxH_output.cache.value,pE_pt,
                  vtmpA,max_dm_dt,dE_dt,timestep_lower_bound);
  if(dE_dt<min_dE_dt) min_dE_dt = dE_dt;
  if(dE_dt>max_dE_dt) max_dE_dt = dE_dt;
  if(!endstate.AddDerivedData("Timestep lower bound",
                                timestep_lower_bound) ||
     !endstate.AddDerivedData("Max dm/dt",max_dm_dt) ||
     !endstate.AddDerivedData("pE/pt",pE_pt) ||
     !endstate.AddDerivedData("dE/dt",dE_dt)) {
    throw Oxs_ExtError(this,
                 "Xf_ThermHeunEvolve::RungeKuttaFehlbergBase54:"
                 " Programming error; data cache already set.");
  }
  // Array holdings: A=dm_dt7   B=dD(3,6)   C=dm_dt4   D=dm_dt5

  // Step 12
  vtmpB.Accumulate(dc1,current_dm_dt);
  vtmpB.Accumulate(dc4,vtmpC);
  vtmpB.Accumulate(dc5,vtmpD);
  vtmpB.Accumulate(dc7,vtmpA);
  // Array holdings: A=dm_dt7   B=dD   C=dm_dt4   D=dm_dt5

  // next_state holds candidate next state, normalized and
  // with proper time field settings; see Step 11.  Note that
  // Step 11 also set norm_error.

  // Error estimate is max|m2a-m2b| = h*max|dD|
  OC_REAL8m max_dD_sq=0.0;
  for(i=0;i<size;i++) {
    OC_REAL8m magsq = vtmpB[i].MagSq();
    if(magsq>max_dD_sq) max_dD_sq = magsq;
  }
  error_estimate = stepsize * sqrt(max_dD_sq);
  global_error_order = 4.0;
  new_energy_and_dmdt_computed = 1;
}

void Xf_ThermHeunEvolve::TakeRungeKuttaFehlbergStep54
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_REAL8m& min_dE_dt, OC_REAL8m& max_dE_dt,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54(RK547FC,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     min_dE_dt,max_dE_dt,
     new_energy_and_dmdt_computed);
}

void Xf_ThermHeunEvolve::TakeRungeKuttaFehlbergStep54M
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_REAL8m& min_dE_dt, OC_REAL8m& max_dE_dt,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54(RK547FM,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     min_dE_dt,max_dE_dt,
     new_energy_and_dmdt_computed);
}

void Xf_ThermHeunEvolve::TakeRungeKuttaFehlbergStep54S
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_REAL8m& min_dE_dt, OC_REAL8m& max_dE_dt,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54(RK547FS,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     min_dE_dt,max_dE_dt,
     new_energy_and_dmdt_computed);
}

OC_REAL8m Xf_ThermHeunEvolve::MaxDiff
(const Oxs_MeshValue<ThreeVector>& vecA,
 const Oxs_MeshValue<ThreeVector>& vecB)
{
  OC_INDEX size = vecA.Size();
  if(vecB.Size()!=size) {
    throw Oxs_ExtError(this,
                 "Xf_ThermHeunEvolve::MaxDiff:"
                 " Import MeshValues incompatible (different lengths).");
  }
  OC_REAL8m max_magsq = 0.0;
  for(OC_INDEX i=0;i<size;i++) {
    ThreeVector vtemp = vecB[i] - vecA[i];
    OC_REAL8m magsq = vtemp.MagSq();
    if(magsq>max_magsq) max_magsq = magsq;
  }
  return sqrt(max_magsq);
}

void Xf_ThermHeunEvolve::AdjustStepHeadroom(OC_INT4m step_reject)
{ // step_reject should be 0 or 1, reflecting whether the current
  // step was rejected or not.  This routine updates reject_ratio
  // and adjusts step_headroom appropriately.

  // First adjust reject_ratio, weighing mostly the last
  // thirty or so results.
  reject_ratio = (31*reject_ratio + step_reject)/32.;

  // Adjust step_headroom
  if(reject_ratio>reject_goal && step_reject>0) {
    // Reject ratio too high and getting worse
    step_headroom *= 0.925;
  }
  if(reject_ratio<reject_goal && step_reject<1) {
    // Reject ratio too small and getting smaller
    step_headroom *= 1.075;
  }

  if(step_headroom>max_step_headroom) step_headroom=max_step_headroom;
  if(step_headroom<min_step_headroom) step_headroom=min_step_headroom;
}

OC_BOOL
Xf_ThermHeunEvolve::Step(const Oxs_TimeDriver* driver,
                      Oxs_ConstKey<Oxs_SimState> current_state_key,
                      const Oxs_DriverStepInfo& step_info,
                      Oxs_Key<Oxs_SimState>& next_state_key)
{
  const OC_REAL8m bad_energy_cut_ratio = 0.75;
  const OC_REAL8m bad_energy_step_increase = 1.3;

  const OC_REAL8m previous_next_timestep = next_timestep;

  const Oxs_SimState& cstate = current_state_key.GetReadReference();

  CheckCache(cstate);

  // Note if start_dm is being used
  OC_BOOL start_dm_active=0;
  if(next_timestep<=0.0 ||
     (cstate.stage_iteration_count<1
      && step_info.current_attempt_count==0)) {
    if(cstate.stage_number==0
       || stage_init_step_control == SISC_START_DM) {
      start_dm_active = 1;
    } else if(stage_init_step_control == SISC_CONTINUOUS) {
      start_dm_active = 0;  // Safety
    } else if(stage_init_step_control == SISC_AUTO) {
      // Automatic detection based on energy values across
      // stage boundary.

      OC_REAL8m total_E,E_diff;
      if(cstate.GetDerivedData("Total E",total_E) &&
         cstate.GetDerivedData("Delta E",E_diff)  &&
         fabs(E_diff) <= 256*OC_REAL8_EPSILON*fabs(total_E) ) {
        // The factor of 256 in the preceding line is a fudge factor,
        // selected with no particular justification.
        start_dm_active = 0;  // Continuous case
      } else {
        start_dm_active = 1;  // Assume discontinuous
      }
    } else {
      throw Oxs_ExtError(this,
           "Xf_ThermHeunEvolve::Step; Programming error:"
           " unrecognized stage_init_step_control value");
    }
  }

  // Negotiate timestep, and also initialize both next_state and
  // temp_state structures.
  Oxs_SimState* work_state = &(next_state_key.GetWriteReference());
  OC_BOOL force_step=0,driver_set_step=0;
  NegotiateTimeStep(driver,cstate,*work_state,next_timestep,
                    start_dm_active,force_step,driver_set_step);
  OC_REAL8m stepsize = work_state->last_timestep;
  work_state=NULL; // Safety: disable pointer

  // Step
  OC_REAL8m error_estimate,norm_error;
  OC_REAL8m global_error_order;
  OC_REAL8m min_dE_dt,max_dE_dt;
  OC_BOOL new_energy_and_dmdt_computed;
  OC_BOOL reject_step=0;
  (this->*rkstep_ptr)(stepsize,current_state_key,
                      dm_dt_output.cache.value,
                      next_state_key,
                      error_estimate,global_error_order,norm_error,
                      min_dE_dt,max_dE_dt,
                      new_energy_and_dmdt_computed);
  const Oxs_SimState& nstate = next_state_key.GetReadReference();
  driver->FillStateDerivedData(cstate,nstate);

  OC_REAL8m max_dm_dt;
  cstate.GetDerivedData("Max dm/dt",max_dm_dt);
  OC_REAL8m reference_stepsize = stepsize;
  if(driver_set_step) reference_stepsize = previous_next_timestep;
  OC_BOOL good_step = CheckError(global_error_order,error_estimate,
                              stepsize,reference_stepsize,
                              max_dm_dt,next_timestep);
  OC_REAL8m timestep_grit = PositiveTimestepBound(max_dm_dt);

  /// Note: Might want to use average or larger of max_dm_dt
  /// and new_max_dm_dt (computed below.)

  if(!good_step && !force_step) {
    // Bad step; The only justfication to do energy and dm_dt
    // computation would be to get an energy-based stepsize
    // adjustment estimate, which we all need to try if
    // next_timestep is larger than cut applied by energy
    // rejection code (i.e., bad_energy_cut_ratio).
    if(next_timestep<=stepsize*bad_energy_cut_ratio) {
      AdjustStepHeadroom(1);
      return 0; // Don't bother with energy calculation
    }
    reject_step=1; // Otherwise, mark step rejected and see what energy
    /// info suggests for next stepsize
  }

  if(start_dm_active && !force_step) {
    // Check that no spin has moved by more than start_dm
    OC_REAL8m diff = MaxDiff(cstate.spin,nstate.spin);
    if(diff>start_dm) {
      next_timestep = step_headroom * stepsize * (start_dm/diff);
      if(next_timestep<=stepsize*bad_energy_cut_ratio) {
        AdjustStepHeadroom(1);
        return 0; // Don't bother with energy calculation
      }
      reject_step=1; // Otherwise, mark step rejected and see what energy
      /// info suggests for next stepsize
    }
  }

#ifdef OLDE_CODE
  if(norm_error>0.0005) {
    fprintf(stderr,
            "Iteration %u passed error check; norm_error=%8.5f\n",
            nstate.iteration_count,static_cast<double>(norm_error));
  } /**/
#endif // OLDE_CODE

  // Energy timestep control:
  //   The relationship between energy error and stepsize appears to be
  // highly nonlinear, so that estimating appropriate stepsize from energy
  // increase if difficult.  Perhaps it is possible to include energy
  // interpolation into RK step routines, but for the present we just
  // reduce the step by a fixed ratio if we detect energy increase beyond
  // that which can be attributed to numerical errors.  This code assumes
  // that the maximum dE/dt occurs at one of the endpoints of the step,
  // which is not necessarily true for higher order integration schemes.
  // This is another reason to try to build this check directly into the
  // RK step routines.
  //   One might argue that a symmetric check on the underside should
  // also be done.  Or one could argue that the whole energy check is
  // specious, that in fact the error check built into the RK stepper
  // routine is all that is needed, and this energy check just unnecessarily
  // reduces the step size, and perhaps the effective order of the method.
  // To date I have not done any testing of these hypotheses.
  // -mjd, 21-July-2004
  {
    OC_REAL8m current_dE_dt,new_dE_dt;
    if(!cstate.GetDerivedData("dE/dt",current_dE_dt)) {
      throw Oxs_ExtError(this,"Xf_ThermHeunEvolve::Step:"
                         " current dE/dt not cached.");
    }
    if(new_energy_and_dmdt_computed) {
      if(!nstate.GetDerivedData("dE/dt",new_dE_dt)) {
        throw Oxs_ExtError(this,"Xf_ThermHeunEvolve::Step:"
                           " new dE/dt not cached.");
      }
    } else {
      OC_REAL8m new_pE_pt;
      GetEnergyDensity(nstate,temp_energy,
                       &mxH_output.cache.value,
                       NULL,new_pE_pt);
      mxH_output.cache.state_id=nstate.Id();
      if(!nstate.AddDerivedData("pE/pt",new_pE_pt)) {
        throw Oxs_ExtError(this,
                           "Xf_ThermHeunEvolve::Step: Programming error;"
                           " data cache (pE/pt) already set.");
      }
      OC_REAL8m new_max_dm_dt,new_timestep_lower_bound;
      Calculate_dm_dt(nstate,
                      mxH_output.cache.value,new_pE_pt,
                      vtmpA,new_max_dm_dt,
                      new_dE_dt,new_timestep_lower_bound);
      if(new_dE_dt<min_dE_dt) min_dE_dt = new_dE_dt;
      if(new_dE_dt>max_dE_dt) max_dE_dt = new_dE_dt;
      new_energy_and_dmdt_computed = 1;
      if(!nstate.AddDerivedData("Timestep lower bound",
                                new_timestep_lower_bound) ||
         !nstate.AddDerivedData("Max dm/dt",new_max_dm_dt) ||
         !nstate.AddDerivedData("dE/dt",new_dE_dt)) {
        throw Oxs_ExtError(this,
                           "Xf_ThermHeunEvolve::Step:"
                           " Programming error; data cache already set.");
      }
    }
    if(current_dE_dt<min_dE_dt) min_dE_dt = current_dE_dt;
    if(current_dE_dt>max_dE_dt) max_dE_dt = current_dE_dt;
  }

  OC_REAL8m dE=0.0;
  OC_REAL8m var_dE=0.0;
  OC_REAL8m total_E=0.0;
  OC_INDEX i;
  const OC_INDEX size = nstate.mesh->Size();
  for(i=0;i<size;++i) {
    OC_REAL8m vol = nstate.mesh->Volume(i);
    OC_REAL8m e = energy[i];
    total_E += e * vol;
    OC_REAL8m new_e = temp_energy[i];
    dE += (new_e - e) * vol;
    var_dE += (new_e*new_e + e*e)*vol*vol;
  }
  if(!nstate.AddDerivedData("Delta E",dE)) {
    throw Oxs_ExtError(this,
         "Xf_ThermHeunEvolve::Step:"
         " Programming error; data cache (Delta E) already set.");
  }

  if(expected_energy_precision>=0.) {
    var_dE *= expected_energy_precision * expected_energy_precision;
    /// Variance, assuming error in each energy[i] term is independent,
    /// uniformly distributed, 0-mean, with range
    ///        +/- expected_energy_precision*energy[i].
    /// It would probably be better to get an error estimate directly
    /// from each energy term.
    OC_REAL8m E_numerror = 2*OC_MAX(fabs(total_E)*expected_energy_precision,
                                 2*sqrt(var_dE));
    OC_REAL8m max_allowed_dE = max_dE_dt * stepsize + E_numerror;
    OC_REAL8m min_allowed_dE = min_dE_dt * stepsize - E_numerror;
    OC_REAL8m allowed_dE_span = max_allowed_dE - min_allowed_dE;
    max_allowed_dE += allowed_dE_span*energy_check_slack;
    min_allowed_dE -= allowed_dE_span*energy_check_slack;
    if(dE<min_allowed_dE || max_allowed_dE<dE) {
      OC_REAL8m teststep = bad_energy_cut_ratio*stepsize;
      if(teststep<next_timestep) {
        next_timestep=teststep;
        max_step_increase = bad_energy_step_increase;
        // Damp the enthusiasm of the RK stepper routine for
        // stepsize growth, for a step or two.
      }
      if(!force_step) reject_step=1;
    }
  }

  // Guarantee that next_timestep is large enough to move
  // at least one spin by an amount perceptible to the
  // floating point representation.
  if(next_timestep<timestep_grit) next_timestep = timestep_grit;

  if(!force_step && reject_step) {
    AdjustStepHeadroom(1);
    return 0;
  }

  // Otherwise, we are accepting the new step.

  // Calculate dm_dt at new point, and fill in cache.
  if(new_energy_and_dmdt_computed) {
    dm_dt_output.cache.value.Swap(vtmpA);
  } else {
    OC_REAL8m new_max_dm_dt,new_dE_dt,new_timestep_lower_bound;
    if(mxH_output.cache.state_id != nstate.Id()) { // Safety
    throw Oxs_ExtError(this,
         "Xf_ThermHeunEvolve::Step:"
         " Programming error; mxH_output cache improperly filled.");
    }
    OC_REAL8m new_pE_pt=0.;
    Calculate_dm_dt(nstate,
                    mxH_output.cache.value,new_pE_pt,
                    dm_dt_output.cache.value,new_max_dm_dt,
                    new_dE_dt,new_timestep_lower_bound);
    if(!nstate.AddDerivedData("Timestep lower bound",
                              new_timestep_lower_bound) ||
       !nstate.AddDerivedData("Max dm/dt",new_max_dm_dt) ||
       !nstate.AddDerivedData("dE/dt",new_dE_dt)) {
      throw Oxs_ExtError(this,
                           "Xf_ThermHeunEvolve::Step:"
                           " Programming error; data cache already set.");
    }
  }
  dm_dt_output.cache.state_id = nstate.Id();

  energy.Swap(temp_energy);
  energy_state_id = nstate.Id();
  AdjustStepHeadroom(0);
  if(!force_step && max_step_increase<max_step_increase_limit) {
    max_step_increase *= max_step_increase_adj_ratio;
  }
  if(max_step_increase>max_step_increase_limit) {
    max_step_increase = max_step_increase_limit;
  }
  return 1; // Accept step
}

void Xf_ThermHeunEvolve::InitHfluct(const Oxs_Mesh* mesh)
{
  const OC_INDEX size = mesh->Size();
  for(OC_INDEX i=0;i<size;i++) {
    Hfluct[i].Set(0., 0., 0.);
  }
}

void Xf_ThermHeunEvolve::UpdateHfluct(const Oxs_Mesh* mesh)
{
  const OC_INDEX size = mesh->Size();
  for(OC_INDEX i=0;i<size;i++) {
    Hfluct[i].x=Gaussian_Random(0.,1.);
    Hfluct[i].y=Gaussian_Random(0.,1.);
    Hfluct[i].z=Gaussian_Random(0.,1.);
  }
}

void Xf_ThermHeunEvolve::FillHfluctConst(const Oxs_Mesh* mesh)
{
  // Update variables that will be constant factors in the simulation
  // h_fluctVarConst will store 2*kB*T*alpha/((1+alpha^2)*gamma*MU0*Vol*dt)
  const OC_INDEX size = mesh->Size();
  for(OC_INDEX i=0;i<size;i++) {
    h_fluctVarConst[i] = 2*fabs(alpha[i])/(1+alpha[i]*alpha[i]);     // 2*alpha/(1+alpha^2)
    h_fluctVarConst[i] /= (MU0*fabs(gamma[i])*min_timestep);      // 2*alpha/((1+alpha^2)*MU0*gamma*dt)
    h_fluctVarConst[i] *= KBoltzmann*temperature/mesh->Volume(i); // 2*kB*T*alpha/((1+alpha^2)*MU0*gamma*Volume*dt)
  }
}

void Xf_ThermHeunEvolve::UpdateDerivedOutputs(const Oxs_SimState& state)
{ // This routine fills all the Xf_ThermHeunEvolve Oxs_ScalarOutput's to
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
    Calculate_dm_dt(state,mxH,pE_pt,dm_dt,
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
           "Xf_ThermHeunEvolve::UpdateDerivedOutputs:"
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

void Xf_ThermHeunEvolve::UpdateTemperatureOutputs(const Oxs_SimState& state)
{

  OC_REAL8m Tmult = EvaluateTemperatureProfileScript(state.stage_number,
                          state.stage_elapsed_time,
                          state.stage_start_time+state.stage_elapsed_time);

  if(temperature_output.GetCacheRequestCount()>0) {
    temperature_output.cache.value=temperature*Tmult;
    temperature_output.cache.state_id = state.Id();
  }

}

OC_REAL8m Xf_ThermHeunEvolve::Gaussian_Random(const OC_REAL8m muGaus, const OC_REAL8m sigmaGaus){

// Box-Muller algorithm, see W.H. Press' "Numerical recipes" chapter7.2 for details
// the above generator is found there also. Modified from original function within
// thetaevolve class to use Oc_UnifRand instead
OC_REAL8m R, gaus1, FAC;

	if (!gaus2_isset) {
		R = 1.;
		while (R >= 1.){
			gaus1 = 2. * Oc_UnifRand() - 1.;
			gaus2 = 2. * Oc_UnifRand() - 1.;
			R  = gaus1*gaus1 + gaus2*gaus2;
		}
		gaus2_isset = 1;
		FAC = sqrt(-2. * log(R) / R);
		gaus1 = gaus1 * FAC * sigmaGaus + muGaus;
		gaus2 = gaus2 * FAC * sigmaGaus + muGaus;
		return gaus1;
	}

	gaus2_isset = false;
	return gaus2;
} // end Gaussian_Random
