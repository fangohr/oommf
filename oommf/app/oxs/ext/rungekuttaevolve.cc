/* FILE: rungekuttaevolve.cc                 -*-Mode: c++-*-
 *
 * Concrete evolver class, using Runge-Kutta steps
 *
 */

#include <cfloat>
#include <string>

#include "nb.h"
#include "director.h"
#include "timedriver.h"
#include "simstate.h"
#include "rectangularmesh.h"   // For Zhang damping
#include "rungekuttaevolve.h"
#include "key.h"
#include "energy.h"             // Needed to make MSVC++ 5 happy

#include "oxsthread.h"

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_RungeKuttaEvolve);

/* End includes */

#define DO_M_SUM 0     // Compute ave M only (see AdjustState)
#define DO_MdM_SUM 1   // Compute ave M and dM/dt together in
                      /// stepper routines (endstates only)
#if DO_M_SUM && DO_MdM_SUM
# error At most one of DO_M_SUM and DO_MdM_SUM can be enabled.
#endif

#define USE_XPFLOATDUALACCUM 1 // Optional use of Nb_XpfloatDualAccum in
                              /// M + dM sums

/////////////////////////////////////////////////////////////////////////
///////// DEBUGGING DEBUGGING DEBUGGING DEBUGGING DEBUGGING /////////////
#define DEBUG_DIFF_DUMP 0
#if !DEBUG_DIFF_DUMP
void SpinDiff(const char*,const Oxs_SimState&) {}
void VecDiff(const char*,const Oxs_MeshValue<ThreeVector>&) {}
void RealDiff(const char*,const Oxs_MeshValue<OC_REAL8m>&) {}
#else
void SpinDiff(const char* label,const Oxs_SimState& state) {
  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const OC_INDEX size = spin.Size();
  if(size<2) return;
  ThreeVector ref = spin[0];
  OC_REAL8m maxdiffsq = 0.0;
  for(OC_INDEX i=1; i<size; ++i) {
    ThreeVector tmp = spin[i];
    tmp -= ref;
    OC_REAL8m diffsq = tmp.MagSq();
    if(diffsq>maxdiffsq) {
      maxdiffsq = diffsq;
    }
  }
  fprintf(stderr,"%s: Max diff in state %u/%u = %g\n",
          label,
          state.iteration_count,state.Id(),sqrt(maxdiffsq));
}

void VecDiff(const char* label,const Oxs_MeshValue<ThreeVector>& arr) {
  const OC_INDEX size = arr.Size();
  if(size<2) return;
  ThreeVector ref = arr[0];
  OC_REAL8m maxdiffsq = 0.0;
  for(OC_INDEX i=1; i<size; ++i) {
    ThreeVector tmp = arr[i];
    tmp -= ref;
    OC_REAL8m diffsq = tmp.MagSq();
    if(diffsq>maxdiffsq) {
      maxdiffsq = diffsq;
    }
  }
  fprintf(stderr,"%s: Max diff in arr = %g\n",
          label,sqrt(maxdiffsq));
}

void RealDiff(const char* label,const Oxs_MeshValue<OC_REAL8m>& arr) {
  const OC_INDEX size = arr.Size();
  if(size<2) return;
  OC_REAL8m ref = arr[0];
  OC_REAL8m maxdiff = 0.0;
  for(OC_INDEX i=1; i<size; ++i) {
    OC_REAL8m tmp = arr[i];
    tmp -= ref;
    OC_REAL8m diff = fabs(tmp);
    if(diff>maxdiff) {
      maxdiff = diff;
    }
  }
  fprintf(stderr,"%s: Max diff in arr = %g\n",
          label,maxdiff);
}
#endif // DEBUG_DIFF_DUMP

///////// DEBUGGING DEBUGGING DEBUGGING DEBUGGING DEBUGGING /////////////
/////////////////////////////////////////////////////////////////////////


template <typename DMDT>
void Oxs_RungeKuttaEvolve::AssignFuncPtrs(String rk_method)
{
  Oxs_ToLower(rk_method); // Do case insensitive match
  if(rk_method.compare("rk2")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2<DMDT>;
  } else if(rk_method.compare("rk2heun")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2Heun<DMDT>;
  } else if(rk_method.compare("rk4")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaStep4<DMDT>;
  } else if(rk_method.compare("rkf54m")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54M<DMDT>;
  } else if(rk_method.compare("rkf54s")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54S<DMDT>;
  } else if(rk_method.compare("rkf54")==0) {
    rkstep_ptr = &Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54<DMDT>;
  } else {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
                            " \"rk_method\" value must be one of"
                            " rk2, rk2heun, rk4, rkf54, rkf54m, or rkf54s.");
  }

  calculate_dm_dt_ptr = &Oxs_RungeKuttaEvolve::Calculate_dm_dt<DMDT>;
}


// Constructor
Oxs_RungeKuttaEvolve::Oxs_RungeKuttaEvolve(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_TimeEvolver(name,newdtr,argstr),
    mesh_id(0),
    max_step_decrease(0.03125), max_step_increase_limit(4.0),
    max_step_increase_adj_ratio(1.9),
    reject_goal(0.05), reject_ratio(0.05),
    dmdt_style(DMDT_INVALID),
    mesh_ispan(0),mesh_jspan(0),mesh_kspan(0),
    mesh_delx(0.0),mesh_dely(0.0),mesh_delz(0.0),
    energy_state_id(0),next_timestep(0.),
    rkstep_ptr(NULL),calculate_dm_dt_ptr(NULL)
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

  allowed_error_rate = GetRealInitValue("error_rate",1.0);
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

  // Alternative damping selection
  if(HasInitValue("zhang_damping") && HasInitValue("baryakhtar_sigma")) {
    throw Oxs_ExtError(this,"Invalid Specify block: "
          "Zhang and Baryakhtar damping cannot be simultaneously enabled.");
  }
  dmdt_style = DMDT_STANDARD;
  String dmdt_style_string("standard LLG damping");
  /// String used in constructor error reports.
  if(HasInitValue("zhang_damping")) {
    OXS_GET_INIT_EXT_OBJECT("zhang_damping",Oxs_ScalarField,zhang_damping_init);
    dmdt_style = DMDT_ZHANG;
    dmdt_style_string = "Zhang damping";
    // The zhang_damping parameter is eta = g mu_B Hbar G0 / (4 e^2 Ms)
    // in the paper Shufeng Zhang and Steven S.-L. Zhang,
    // "Generalization of the Landau-Lifshitz-Gilbert equation for
    // conducting ferromagnetics," Physical Review Letters, 102, 086601
    // (2009).  For Py, eta = 4.768e-19 m^2.  See also NOTES VI,
    // 1-June-2012, p 40.
  } else if(HasInitValue("baryakhtar_sigma")) {
    OXS_GET_INIT_EXT_OBJECT("baryakhtar_sigma",Oxs_ScalarField,baryakhtar_sigma_init);
    dmdt_style = DMDT_BARYAKHTAR;
    dmdt_style_string = "Baryakhtar damping";
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
  start_dm *= PI/180.; // Convert from deg to rad

  start_dt = GetRealInitValue("start_dt",max_timestep/8.);

  if(start_dm<0. && start_dt<0.) {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
                       " at least one of \"start_dm\" and \"start_dt\""
                       "  must be nonnegative.");
  }

  stage_init_step_control = SISC_AUTO;  // Safety
  String stage_start = GetStringInitValue("stage_start","auto");
  Oxs_ToLower(stage_start);
  if(stage_start.compare("start_conditions")==0) {
    stage_init_step_control = SISC_START_COND;
  } else if(stage_start.compare("continuous")==0) {
    stage_init_step_control = SISC_CONTINUOUS;
  } else if(stage_start.compare("auto")==0
            || stage_start.compare("automatic")==0) {
    stage_init_step_control = SISC_AUTO;
  } else {
    throw Oxs_ExtError(this,"Invalid initialization detected:"
                         " \"stage_start\" value must be one of"
                         " start_conditions, continuous, or auto.");
  }

  String method = GetStringInitValue("method","rkf54");

  // Set up rkstep_ptr and calculate_dm_dt_ptr
  switch(dmdt_style) {
  case DMDT_ZHANG:      AssignFuncPtrs<Zhang_dmdt>(method);      break;
  case DMDT_BARYAKHTAR: AssignFuncPtrs<Baryakhtar_dmdt>(method); break;
  case DMDT_STANDARD:   AssignFuncPtrs<Standard_dmdt>(method);   break;
  default:
    throw Oxs_ExtError(this,"Unsupported damping style selected.");
  }

  // Setup outputs
  max_dm_dt_output.Setup(this,InstanceName(),"Max dm/dt","deg/ns",0,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);
  dE_dt_output.Setup(this,InstanceName(),"dE/dt","J/s",0,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);
  delta_E_output.Setup(this,InstanceName(),"Delta E","J",0,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);
  dm_dt_output.Setup(this,InstanceName(),"dm/dt","rad/s",1,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);
  mxH_output.Setup(this,InstanceName(),"mxH","A/m",1,
     &Oxs_RungeKuttaEvolve::UpdateDerivedOutputs);

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

  // Reserve space for temp_state; see Step() method below
  director->ReserveSimulationStateRequest(1);

#if REPORT_TIME_RKDEVEL
  timer.resize(10);
  timer_counts.resize(10);
#endif
}

OC_BOOL Oxs_RungeKuttaEvolve::Init()
{
  Oxs_TimeEvolver::Init();

#if REPORT_TIME_RKDEVEL
  Oc_TimeVal cpu,wall;
  for(unsigned int ti=0;ti<timer.size();++ti) {
    timer[ti].GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"               timer %2u ...   %7.2f cpu /%7.2f wall,"
              " (%s/%s)\n",
              ti,double(cpu),double(wall),InstanceName(),
              timer_counts[ti].name.c_str());
      if(timer_counts[ti].pass_count>0) {
        fprintf(stderr,"                \\---> passes = %d,"
                " bytes=%.2f MB, %.2f GB/sec\n",
                timer_counts[ti].pass_count,
                double(timer_counts[ti].bytes)/double(1024*1024),
                double(timer_counts[ti].bytes)
                /(double(1024*1024*1024)*double(wall)));
      }
    }
    timer[ti].Reset();
    timer_counts[ti].Reset();
  }
#endif // REPORT_TIME_RKDEVEL

  // Free scratch space allocated by previous problem (if any)
  vtmpA.Release();   vtmpB.Release();
  vtmpC.Release();   vtmpD.Release();

  // Free memory used by LLG gamma and alpha
  mesh_id = 0;
  alpha.Release();
  gamma.Release();
  zhang_damping.Release();
  zhang_delm.Release();
  baryakhtar_sigma.Release();
  D2Hperp.Release();

  max_step_increase = max_step_increase_limit;

  // Setup step_headroom and reject_ratio
  step_headroom = max_step_headroom;
  reject_ratio = reject_goal;

  energy_state_id=0;   // Mark as invalid state
  next_timestep=0.;    // Dummy value
  return 1;
}


Oxs_RungeKuttaEvolve::~Oxs_RungeKuttaEvolve()
{
#if REPORT_TIME_RKDEVEL
  Oc_TimeVal cpu,wall;
  for(unsigned int ti=0;ti<timer.size();++ti) {
    timer[ti].GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"               timer %2u ...   %7.2f cpu /%7.2f wall,"
              " (%s/%s)\n",
              ti,double(cpu),double(wall),InstanceName(),
              timer_counts[ti].name.c_str());
      if(timer_counts[ti].pass_count>0) {
        fprintf(stderr,"                \\---> passes = %d,"
                " bytes=%.2f MB, %.2f GB/sec\n",
                timer_counts[ti].pass_count,
                double(timer_counts[ti].bytes)/double(1024*1024),
                double(timer_counts[ti].bytes)
                /(double(1024*1024*1024)*double(wall)));
      }
    }
 }
#endif // REPORT_TIME_RKDEVEL
}

void Oxs_RungeKuttaEvolve::UpdateMeshArrays(const Oxs_Mesh* mesh)
{
  mesh_id = 0; // Mark update in progress
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

  if(dmdt_style==DMDT_ZHANG || dmdt_style==DMDT_BARYAKHTAR) {
    // Require a rectangular mesh for damping variants that compute
    // spatial derivatives.  In principle we could allow either periodic
    // or non-periodic meshes, but for at present only non-periodic case
    // is supported.  If this code gets extended to periodic case, then
    // the upcast goes to Oxs_CommonRectangularMesh, and we need to set
    // flags to tell the spatial derivatives to handle boundaries
    // appropriately.
    const Oxs_RectangularMesh* rmesh
      = dynamic_cast<const Oxs_RectangularMesh*>(mesh);
    if(rmesh==NULL) {
      String msg=String("Zhang and Baryakhtar damping code requires a standard"
                        " rectangular mesh, but mesh object ")
        + String(mesh->InstanceName())
        + String(" is not a rectangular mesh.");
      throw Oxs_ExtError(this,msg);
    }
    mesh_ispan = rmesh->DimX();
    mesh_jspan = rmesh->DimY();
    mesh_kspan = rmesh->DimZ();
    mesh_delx = rmesh->EdgeLengthX();
    mesh_dely = rmesh->EdgeLengthY();
    mesh_delz = rmesh->EdgeLengthZ();
    if(dmdt_style == DMDT_ZHANG) {
      zhang_damping_init->FillMeshValue(mesh,zhang_damping);
    }
    if(dmdt_style == DMDT_BARYAKHTAR) {
      baryakhtar_sigma_init->FillMeshValue(mesh,baryakhtar_sigma);
    }
  }

  mesh_id = mesh->Id();
}

OC_REAL8m
Oxs_RungeKuttaEvolve::PositiveTimestepBound
(OC_REAL8m max_dm_dt)
{ // Computes an estimate on the minimum time needed to
  // change the magnetization state, subject to floating
  // points limits.
  OC_REAL8m tbound = DBL_MAX/64.;
  if(max_dm_dt>1 || OC_REAL8_EPSILON<tbound*max_dm_dt) {
    tbound = OC_REAL8_EPSILON/max_dm_dt;
    // A timestep of size tbound will be hopelessly lost in roundoff
    // error.  So increase a bit, based on an empirical fudge factor.
    // This fudge factor can be tested by running a problem with
    // start_dm = 0.  If the evolver can't climb its way out of the
    // stepsize=0 hole, then this fudge factor is too small.  So far,
    // the most challenging examples have been problems with small cells
    // with nearly aligned spins, e.g., in a remanent state with an
    // external field is applied at t=0.  Damping ratio doesn't seem to
    // have much effect, either way.
    tbound *= 64;
  } else {
    // Degenerate case: max_dm_dt_ must be exactly or very nearly
    // zero.  Punt.
    tbound = max_timestep;
  }
  return tbound;
}


template <typename DMDT>
void Oxs_RungeKuttaEvolve::Calculate_dm_dt
(const Oxs_SimState& state_,
 const Oxs_MeshValue<ThreeVector>& mxH_,
 OC_REAL8m pE_pt_,
 Oxs_MeshValue<ThreeVector>& dm_dt_,
 OC_REAL8m& max_dm_dt_,OC_REAL8m& dE_dt_,OC_REAL8m& min_timestep_export)
{ // Imports: state, mxH_, pE_pt_
  // Exports: dm_dt_, max_dm_dt_, dE_dt_
  // NOTE: dm_dt_ is allowed, and in fact is encouraged,
  //   to be the same as mxH_.  In this case, mxH_ is
  //   overwritten by dm_dt on return.

  const Oxs_Mesh* mesh_ = state_.mesh;

  // Fill out alpha and gamma meshvalue arrays, as necessary.
  if(mesh_id != mesh_->Id()
     || !gamma.CheckMesh(mesh_)
     || !alpha.CheckMesh(mesh_)
     || (dmdt_style==DMDT_ZHANG      && zhang_damping.CheckMesh(mesh_))
     || (dmdt_style==DMDT_BARYAKHTAR && baryakhtar_sigma.CheckMesh(mesh_))) {
    UpdateMeshArrays(mesh_);
  }

  dm_dt_.AdjustSize(mesh_);  // For case &dm_dt_ != &mxH_

  OC_REAL8m dE_dt_sum=0.0;
  OC_REAL8m max_dm_dt_sq = 0.0;
#if DO_MdM_SUM
  ThreeVector ave_M;
  ThreeVector ave_dM_dt;
#endif // DO_MdM_SUM

  { // Compute dm_dt with damping type determined by template parameter
    // DMDT
    DMDT dmdt(this,state_,mxH_,dm_dt_);
    dmdt.Initialize();

    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_max_dm_dt_sq(number_of_threads,-1.0);
    Oc_AlignedVector<Oxs_Energy::SUMTYPE>
      thread_pE_pM_sum(number_of_threads,Oxs_Energy::SUMTYPE(0.0));
#if DO_MdM_SUM
    std::vector<ThreeVector>
      thread_M_sum(number_of_threads,ThreeVector(0.,0.,0.));
    std::vector<ThreeVector>
      thread_dM_dt_sum(number_of_threads,ThreeVector(0.,0.,0.));
#endif // DO_MdM_SUM
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(state_.Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        Oxs_Energy::SUMTYPE thd_pE_pM_sum = 0.0; // Thread local storage
        OC_REAL8m thd_max_dm_dt_sq = 0.0;
#if DO_MdM_SUM
        Nb_Xpfloat thd_M_sum_x(0.),thd_M_sum_y(0.),thd_M_sum_z(0.);
        Nb_Xpfloat thd_dM_dt_sum_x(0.),thd_dM_dt_sum_y(0.),thd_dM_dt_sum_z(0.);
        const Oxs_MeshValue<OC_REAL8m>& Ms = *(state_.Ms);
        const Oxs_MeshValue<ThreeVector>& spin = state_.spin;
#endif
        for(OC_INDEX j=jstart;j<jstop;++j) {
          OC_REAL8m dummy=0.0;
          dmdt.Compute(j,thd_max_dm_dt_sq,dummy); // Fills dm_dt_ with dmdt
          thd_pE_pM_sum += dummy;
#if DO_MdM_SUM
          const OC_REAL8m Mtmp = Ms[j];
# if USE_XPFLOATDUALACCUM
          Nb_XpfloatDualAccum(thd_dM_dt_sum_x,Mtmp*dm_dt_[j].x,
                              thd_dM_dt_sum_y,Mtmp*dm_dt_[j].y);
          Nb_XpfloatDualAccum(thd_dM_dt_sum_z,Mtmp*dm_dt_[j].z,
                              thd_M_sum_x,Mtmp*spin[j].x);
          Nb_XpfloatDualAccum(thd_M_sum_y,Mtmp*spin[j].y,
                              thd_M_sum_z,Mtmp*spin[j].z);
# else // USE_XPFLOATDUALACCUM
          thd_dM_dt_sum_x.Accum(Mtmp*dm_dt_[j].x);
          thd_dM_dt_sum_y.Accum(Mtmp*dm_dt_[j].y);
          thd_dM_dt_sum_z.Accum(Mtmp*dm_dt_[j].z);
          thd_M_sum_x.Accum(Mtmp*spin[j].x);
          thd_M_sum_y.Accum(Mtmp*spin[j].y);
          thd_M_sum_z.Accum(Mtmp*spin[j].z);
# endif // USE_XPFLOATDUALACCUM
#endif
        }
        // Allow for multiple jstart/jstop chunks
        thread_max_dm_dt_sq[threadid]
          = OC_MAX(thd_max_dm_dt_sq,thread_max_dm_dt_sq[threadid]);
        thread_pE_pM_sum[threadid] += thd_pE_pM_sum;
#if DO_MdM_SUM
        thread_M_sum[threadid].x += thd_M_sum_x.GetValue();
        thread_M_sum[threadid].y += thd_M_sum_y.GetValue();
        thread_M_sum[threadid].z += thd_M_sum_z.GetValue();
        thread_dM_dt_sum[threadid].x += thd_dM_dt_sum_x.GetValue();
        thread_dM_dt_sum[threadid].y += thd_dM_dt_sum_y.GetValue();
        thread_dM_dt_sum[threadid].z += thd_dM_dt_sum_z.GetValue();
#endif
      });
    max_dm_dt_sq = thread_max_dm_dt_sq[0];
    for(int i=1;i<number_of_threads;++i) {
      if(thread_max_dm_dt_sq[i] > max_dm_dt_sq) {
        max_dm_dt_sq = thread_max_dm_dt_sq[i];
      }
      thread_pE_pM_sum[0] += thread_pE_pM_sum[i];
#if DO_MdM_SUM
      thread_M_sum[0]     += thread_M_sum[i];
      thread_dM_dt_sum[0] += thread_dM_dt_sum[i];
#endif // DO_MdM_SUM
    }
    dE_dt_sum = thread_pE_pM_sum[0];

    dmdt.Finalize(max_dm_dt_sq,dE_dt_sum); // Scale max_dm_dt_sq and dE_dt_sum

#if DO_MdM_SUM
    const OC_INDEX meshsize = state_.mesh->Size();
    ave_M.x = thread_M_sum[0].x/meshsize;
    ave_M.y = thread_M_sum[0].y/meshsize;
    ave_M.z = thread_M_sum[0].z/meshsize;
    ave_dM_dt.x = thread_dM_dt_sum[0].x/meshsize;
    ave_dM_dt.y = thread_dM_dt_sum[0].y/meshsize;
    ave_dM_dt.z = thread_dM_dt_sum[0].z/meshsize;
#endif // DO_MdM_SUM
  }

  max_dm_dt_ = sqrt(max_dm_dt_sq);
  dE_dt_ = dE_dt_sum + pE_pt_;
  /// The first term is (partial E/partial M)*dM/dt, the
  /// second term is (partial E/partial t)*dt/dt.  Note that,
  /// provided Ms_[i]>=0, that by constructions dE_dt_sum above
  /// is always non-negative, so dE_dt_ can only be made positive
  /// by positive pE_pt_.

  // Get bound on smallest stepsize that would actually
  // change spin new_max_dm_dt_index:
  min_timestep_export = PositiveTimestepBound(max_dm_dt_);

#if DO_MdM_SUM
  // Store average M and dM_dt data in state derived data space.
  // The ave M value might already be set; if so, check that it
  // agrees with the current computation.
  if(!state_.AddDerivedData(DataName("Mx"),ave_M.x) ||
     !state_.AddDerivedData(DataName("My"),ave_M.y) ||
     !state_.AddDerivedData(DataName("Mz"),ave_M.z)) {
    // Cache already set.  Check that values agree
    OC_REAL8m mx=0.0, my=0.0, mz=0.0; // Dummy inits to pacify compilers
    if(!state_.GetDerivedData(DataName("Mx"),mx) ||
       !state_.GetDerivedData(DataName("My"),my) ||
       !state_.GetDerivedData(DataName("Mz"),mz)) {
      throw Oxs_ExtError(this,
         "Oxs_RungeKuttaEvolve::Calculate_dm_dt:"
                         " Programming error; ave M cache not properly set.");
    }

    const OC_INDEX meshsize = state_.mesh->Size();
    OC_REAL8m slop = 4*OC_REAL8m_EPSILON*state_.max_absMs*sqrt(meshsize);
    if(fabs(ave_M.x-mx) > slop || fabs(ave_M.y-my) > slop
       || fabs(ave_M.z-mz) > slop) {
      char msg[1000];
      snprintf(msg,sizeof(msg),
               "Oxs_RungeKuttaEvolve::Calculate_dm_dt:"
               " Programming error; Inconsistent ave M cache values:\n"
               " ave_M=(%25.16e,%25.16e,%25.16e)\n"
               "     m=(%25.16e,%25.16e,%25.16e)\n",
               ave_M.x,ave_M.y,ave_M.z,mx,my,mz);
      throw Oxs_ExtError(this,msg);
      // NB: If USE_XPFLOATDUALACCUM is set, then failure here might
      // trace back to floating point non-associativity issues in
      //   void Nb_XpfloatDualAccum(Nb_Xpfloat&,Nb_Xpfloat&,__m128d)
      // or
      //   void Nb_Xpfloat::Accum(NB_XPFLOAT_TYPE y)
      // in file oommf/pkg/nb/xpfloat.h.
    }
  }

  if(!state_.AddDerivedData(DataName("dMx/dt"),ave_dM_dt.x) ||
     !state_.AddDerivedData(DataName("dMy/dt"),ave_dM_dt.y) ||
     !state_.AddDerivedData(DataName("dMz/dt"),ave_dM_dt.z)) {
      throw Oxs_ExtError(this,
         "Oxs_RungeKuttaEvolve::Calculate_dm_dt:"
         " Programming error; ave dM/dt cache already set.");
    }
#endif // DO_MdM_SUM
    return;
}

void Oxs_RungeKuttaEvolve::CheckCache(const Oxs_SimState& cstate)
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

  cache_good &= cstate.GetDerivedData(DataName("Max dm/dt"),max_dm_dt);
  cache_good &= cstate.GetDerivedData(DataName("dE/dt"),dE_dt);
  cache_good &= cstate.GetDerivedData(DataName("Delta E"),delta_E);
  cache_good &= cstate.GetDerivedData(DataName("pE/pt"),pE_pt);
  cache_good &= cstate.GetDerivedData(DataName("Timestep lower bound"),
                                      timestep_lower_bound);
  cache_good &= (energy_state_id == cstate.Id());
  cache_good &= (dm_dt_output.cache.state_id == cstate.Id());

  if(!cache_good) {
    throw Oxs_ExtError(this,
       "Oxs_RungeKuttaEvolve::CheckCache: Invalid data cache.");
  }
}

void Oxs_RungeKuttaEvolve::UpdateTimeFields
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

void
Oxs_RungeKuttaEvolve::AdjustState
(OC_REAL8m hstep,
 OC_REAL8m mstep,
 const Oxs_SimState& old_state,
 const Oxs_MeshValue<ThreeVector>& dm_dt,
 Oxs_SimState& new_state) const
{
  new_state.ClearDerivedData();

  const Oxs_MeshValue<ThreeVector>& old_spin = old_state.spin;
  Oxs_MeshValue<ThreeVector>& new_spin = new_state.spin;

  if(!dm_dt.CheckMesh(old_state.mesh)) {
    throw Oxs_ExtError(this,
                         "Oxs_RungeKuttaEvolve::AdjustState:"
                         " Import spin and dm_dt are different sizes.");
  }
  new_spin.AdjustSize(old_state.mesh);

  // Advance spins (threaded using lambda expression)
#if DO_M_SUM // Compute ave M and store results in state AuxData store
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(new_state.Ms);
  const int number_of_threads = Oc_GetMaxThreadCount();
  std::vector<ThreeVector>
    thread_sumM(number_of_threads,ThreeVector(0.,0.,0.));
  Oxs_RunThreaded<ThreeVector,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
                  (old_spin,
                   [&](OC_INT4m thread_id,OC_INDEX jstart,OC_INDEX jstop) {
                     ThreeVector local_sumM(0.,0.,0.);
                     for(OC_INDEX j=jstart;j<jstop;++j) {
                      ThreeVector tempspin = old_spin[j];
                      tempspin.Accum(mstep,dm_dt[j]);
                      tempspin.MakeUnit();
                      new_spin[j] = tempspin;
                      local_sumM.Accum(Ms[j],tempspin);
                     }
                     thread_sumM[thread_id] += local_sumM;
  });
  for(int i=1;i<number_of_threads;++i) {
    thread_sumM[0] += thread_sumM[i];
  }
  new_state.AddAuxData(String(InstanceName()) + String("Mx"),
     thread_sumM[0].x/static_cast<OC_REAL8m>(new_spin.Size()));
  new_state.AddAuxData(String(InstanceName()) + String("My"),
     thread_sumM[0].y/static_cast<OC_REAL8m>(new_spin.Size()));
  new_state.AddAuxData(String(InstanceName()) + String("Mz"),
     thread_sumM[0].z/static_cast<OC_REAL8m>(new_spin.Size()));
#else // !DO_M_SUM (original code)
  Oxs_RunThreaded<ThreeVector,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
                  (old_spin,
                   [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
                    for(OC_INDEX j=jstart;j<jstop;++j) {
                      ThreeVector tempspin = old_spin[j];
                      tempspin.Accum(mstep,dm_dt[j]);
                      tempspin.MakeUnit();
                      new_spin[j] = tempspin;
                    }});
#endif // DO_M_SUM

  // Adjust time fields in new_state
  UpdateTimeFields(old_state,new_state,hstep);

  // Don't touch iteration counts. (?!)  The problem is that one call
  // to Oxs_RungeKuttaEvolve::Step() takes 2 half-steps, and it is the
  // result from these half-steps that are used as the export state.
  // If we increment the iteration count each time through here, then
  // the iteration count goes up by 2 for each call to Step().  So
  // instead, we leave iteration count at whatever value was filled
  // in by the Oxs_RungeKuttaEvolve::NegotiateTimeStep() method.
}


void Oxs_RungeKuttaEvolve::NegotiateTimeStep
(const Oxs_TimeDriver* driver,
 const Oxs_SimState&  cstate,
 Oxs_SimState& nstate,
 OC_REAL8m stepsize,
 OC_BOOL use_start_cond,
 OC_BOOL& force_step,
 OC_BOOL& driver_set_step) const
{ // This routine negotiates with driver over the proper step size.
  // As a side-effect, also initializes the nstate data structure,
  // where nstate is the "next state".

  // Pull needed cached values out from cstate.
  OC_REAL8m max_dm_dt;
  if(!cstate.GetDerivedData(DataName("Max dm/dt"),max_dm_dt)) {
    throw Oxs_ExtError(this,
       "Oxs_RungeKuttaEvolve::NegotiateTimeStep: max_dm_dt not cached.");
  }
  OC_REAL8m timestep_lower_bound=0.;  // Smallest timestep that can actually
  /// change spin with max_dm_dt (due to OC_REAL8_EPSILON restrictions).
  if(!cstate.GetDerivedData(DataName("Timestep lower bound"),
                            timestep_lower_bound)) {
    throw Oxs_ExtError(this,
       "Oxs_RungeKuttaEvolve::NegotiateTimeStep: "
       " timestep_lower_bound not cached.");
  }
  if(timestep_lower_bound<=0.0) {
    throw Oxs_ExtError(this,
       "Oxs_RungeKuttaEvolve::NegotiateTimeStep: "
       " cached timestep_lower_bound value not positive.");
  }

  // The next timestep is based on the error from the last step.  If
  // there is no last step (either because this is the first step,
  // or because the last state handled by this routine is different
  // from the incoming current_state), then timestep is calculated
  // so that max_dm_dt * timestep = start_dm or
  // timestep = start_dt.
  if(use_start_cond || stepsize<=0.0) {
    if(start_dm>=0.0) {
      if(start_dm < sqrt(DBL_MAX/4) * max_dm_dt) {
        stepsize = step_headroom * start_dm / max_dm_dt;
      } else {
        stepsize = sqrt(DBL_MAX/4);
      }
    }
    if(start_dt>=0.0) {
      if(start_dm<0.0 || stepsize>start_dt) {
        stepsize = start_dt;
      }
    }
  }

  // Insure step is not outside requested step bounds
  if(!use_start_cond && stepsize<timestep_lower_bound) {
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
Oxs_RungeKuttaEvolve::CheckError
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
  // NOTE: A copy of this routine lives in eulerevolve.cc.  Updates
  //     should be copied there.

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

template <typename DMDT>
void Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
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

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound;

  vtmpA.AdjustSize(cstate->mesh);
  vtmpB.AdjustSize(cstate->mesh);

  // Calculate dm_dt2
  AdjustState(stepsize/2,stepsize/2,*cstate,current_dm_dt,
              next_state_key.GetWriteReference());
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(cstate->mesh);
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_norm_error(number_of_threads,-1.0);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        OC_REAL8m min_normsq = DBL_MAX;
        OC_REAL8m max_normsq = 0.0;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dmdt
          ThreeVector vtmp = vtmpA[j];
          vtmp *= stepsize;
          vtmp += old_spin[j];
          OC_REAL8m magsq = vtmp.MakeUnit();
          new_spin[j] = vtmp;
          if(magsq<min_normsq) min_normsq=magsq;
          if(magsq>max_normsq) max_normsq=magsq;
        }
        OC_REAL8m thderror = OC_MAX(sqrt(max_normsq)-1.0,
                                    1.0 - sqrt(min_normsq));
        thread_norm_error[threadid]
          = OC_MAX(thderror,thread_norm_error[threadid]);
        /// Allow for multiple jstart/jstop chunks
      });
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,newstate,stepsize);
    OC_REAL8m maxerror = thread_norm_error[0];
    for(int i=1;i<number_of_threads;++i) {
      if(thread_norm_error[i]>maxerror) maxerror = thread_norm_error[i];
    }
    norm_error = maxerror;
  }

  const Oxs_SimState* endstate
    = &(next_state_key.GetReadReference()); // Lock down

  // To estimate error, compute dm_dt at end state.
  OC_REAL8m total_E;
  OC_REAL8m max_err_sq;
#if DO_MdM_SUM
  ThreeVector ave_M;
  ThreeVector ave_dM_dt;
#endif // DO_MdM_SUM
  GetEnergyDensity(*endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt,total_E);
  mxH_output.cache.state_id=endstate->Id();
  {
    DMDT dmdt(this,*endstate,mxH_output.cache.value,vtmpB);
    dmdt.Initialize();
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_err_sq(number_of_threads,-1.0);
    std::vector<OC_REAL8m> thread_max_dm_dt_sq(number_of_threads,-1.0);
#if DO_MdM_SUM
    std::vector<ThreeVector>
      thread_M_sum(number_of_threads,ThreeVector(0.,0.,0.));
    std::vector<ThreeVector>
      thread_dM_dt_sum(number_of_threads,ThreeVector(0.,0.,0.));
#endif // DO_MdM_SUM
    Oc_AlignedVector<Oxs_Energy::SUMTYPE>
      thread_pE_pM_sum(number_of_threads,Oxs_Energy::SUMTYPE(0.0));
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(endstate->Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        Oxs_Energy::SUMTYPE thd_pE_pM_sum = 0.0;
        OC_REAL8m thd_max_dm_dt_sq = 0.0;
        OC_REAL8m thd_max_err_sq = 0.0;
#if DO_MdM_SUM
        Nb_Xpfloat thd_M_sum_x(0.),thd_M_sum_y(0.),thd_M_sum_z(0.);
        Nb_Xpfloat thd_dM_dt_sum_x(0.),thd_dM_dt_sum_y(0.),thd_dM_dt_sum_z(0.);
        const Oxs_MeshValue<OC_REAL8m>& Ms = *(endstate->Ms);
        const Oxs_MeshValue<ThreeVector>& spin = endstate->spin;
#endif
        for(OC_INDEX j=jstart;j<jstop;++j) {
          OC_REAL8m dummy=0.0;
          dmdt.Compute(j,thd_max_dm_dt_sq,dummy); // Fills vtmpB with dmdt
#if DO_MdM_SUM
          thd_dM_dt_sum_x.Accum(Ms[j]*vtmpB[j].x);
          thd_dM_dt_sum_y.Accum(Ms[j]*vtmpB[j].y);
          thd_dM_dt_sum_z.Accum(Ms[j]*vtmpB[j].z);
          thd_M_sum_x.Accum(Ms[j]*spin[j].x);
          thd_M_sum_y.Accum(Ms[j]*spin[j].y);
          thd_M_sum_z.Accum(Ms[j]*spin[j].z);
#endif
          thd_pE_pM_sum += dummy;
          ThreeVector tvec = current_dm_dt[j];
          tvec += vtmpB[j];
          tvec *= 0.5;
          tvec -= vtmpA[j];
          OC_REAL8m err_sq = tvec.MagSq();
          if(err_sq>thd_max_err_sq) thd_max_err_sq = err_sq;
          // NB: No spin advancement
        }
        // Allow for multiple jstart/jstop chunks
        thread_err_sq[threadid]
          = OC_MAX(thd_max_err_sq,thread_err_sq[threadid]);
        thread_max_dm_dt_sq[threadid]
          = OC_MAX(thd_max_dm_dt_sq,thread_max_dm_dt_sq[threadid]);
        thread_pE_pM_sum[threadid] += thd_pE_pM_sum;
#if DO_MdM_SUM
        thread_M_sum[threadid].x += thd_M_sum_x.GetValue();
        thread_M_sum[threadid].y += thd_M_sum_y.GetValue();
        thread_M_sum[threadid].z += thd_M_sum_z.GetValue();
        thread_dM_dt_sum[threadid].x += thd_dM_dt_sum_x.GetValue();
        thread_dM_dt_sum[threadid].y += thd_dM_dt_sum_y.GetValue();
        thread_dM_dt_sum[threadid].z += thd_dM_dt_sum_z.GetValue();
#endif
      });
    max_err_sq = thread_err_sq[0];
    OC_REAL8m max_dm_dt_sq = thread_max_dm_dt_sq[0];
    for(int i=1;i<number_of_threads;++i) {
      if(thread_err_sq[i] > max_err_sq) {
        max_err_sq = thread_err_sq[i];
      }
      if(thread_max_dm_dt_sq[i] > max_dm_dt_sq) {
        max_dm_dt_sq = thread_max_dm_dt_sq[i];
      }
      thread_pE_pM_sum[0] += thread_pE_pM_sum[i];
#if DO_MdM_SUM
      thread_M_sum[0]     += thread_M_sum[i];
      thread_dM_dt_sum[0] += thread_dM_dt_sum[i];
#endif // DO_MdM_SUM
    }
    OC_REAL8m pE_pM_sum = thread_pE_pM_sum[0];
    dmdt.Finalize(max_dm_dt_sq,pE_pM_sum); // Scale max_dm_dt_sq and pE_pM_sum

    max_dm_dt = sqrt(max_dm_dt_sq);
    timestep_lower_bound = PositiveTimestepBound(max_dm_dt);
    dE_dt = pE_pt + pE_pM_sum;
#if DO_MdM_SUM
    ave_M.x = thread_M_sum[0].x/endstate->Ms->Size();
    ave_M.y = thread_M_sum[0].y/endstate->Ms->Size();
    ave_M.z = thread_M_sum[0].z/endstate->Ms->Size();
    ave_dM_dt.x = thread_dM_dt_sum[0].x/endstate->Ms->Size();
    ave_dM_dt.y = thread_dM_dt_sum[0].y/endstate->Ms->Size();
    ave_dM_dt.z = thread_dM_dt_sum[0].z/endstate->Ms->Size();
#endif // DO_MdM_SUM
  }
  if(!endstate->AddDerivedData(DataName("Timestep lower bound"),
                               timestep_lower_bound) ||
     !endstate->AddDerivedData(DataName("Max dm/dt"),max_dm_dt) ||
#if DO_MdM_SUM
     !endstate->AddDerivedData(DataName("Mx"),ave_M.x) ||
     !endstate->AddDerivedData(DataName("My"),ave_M.y) ||
     !endstate->AddDerivedData(DataName("Mz"),ave_M.z) ||
     !endstate->AddDerivedData(DataName("dMx/dt"),ave_dM_dt.x) ||
     !endstate->AddDerivedData(DataName("dMy/dt"),ave_dM_dt.y) ||
     !endstate->AddDerivedData(DataName("dMz/dt"),ave_dM_dt.z) ||
#endif // DO_MdM_SUM
     !endstate->AddDerivedData(DataName("pE/pt"),pE_pt) ||
     !endstate->AddDerivedData(DataName("Total E"),total_E) ||
     !endstate->AddDerivedData(DataName("dE/dt"),dE_dt)
     ) {
    throw Oxs_ExtError(this,
                       "Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2:"
                       " Programming error; data cache already set.");
  }
  // Move end dm_dt data into vtmpA, for use by calling routine.
  // Note that end energy is already in temp_energy, as per
  // contract.
  vtmpA.Swap(vtmpB);

  error_estimate = stepsize*sqrt(max_err_sq);
  global_error_order = 2.0;
  new_energy_and_dmdt_computed = 1;
}

template <typename DMDT>
void Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2Heun
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
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

  OC_REAL8m pE_pt,max_dm_dt,dE_dt,timestep_lower_bound;

  vtmpA.AdjustSize(cstate->mesh);
  vtmpB.AdjustSize(cstate->mesh);
  vtmpC.AdjustSize(cstate->mesh);

  // Calculate dm_dt2
  AdjustState(stepsize,stepsize,*cstate,current_dm_dt,
              next_state_key.GetWriteReference());
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(cstate->mesh);
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_norm_error(number_of_threads,-1.0);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        OC_REAL8m min_normsq = DBL_MAX;
        OC_REAL8m max_normsq = 0.0;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dmdt2
          ThreeVector vtmp
            = vtmpC[j] = vtmpA[j]; // vtmpC used for error control
          vtmp += current_dm_dt[j];
          vtmp *= 0.5*stepsize;
          vtmp += old_spin[j];
          OC_REAL8m magsq = vtmp.MakeUnit();
          new_spin[j] = vtmp;
          if(magsq<min_normsq) min_normsq=magsq;
          if(magsq>max_normsq) max_normsq=magsq;
        }
        OC_REAL8m thderror = OC_MAX(sqrt(max_normsq)-1.0,
                                    1.0 - sqrt(min_normsq));
        thread_norm_error[threadid]
          = OC_MAX(thderror,thread_norm_error[threadid]);
        /// Allow for multiple jstart/jstop chunks
      });
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,newstate,stepsize);
    OC_REAL8m maxerror = thread_norm_error[0];
    for(int i=1;i<number_of_threads;++i) {
      if(thread_norm_error[i]>maxerror) maxerror = thread_norm_error[i];
    }
    norm_error = maxerror;
  }
  const Oxs_SimState* endstate
    = &(next_state_key.GetReadReference()); // Lock down

  // To estimate error, compute dm_dt at end state.
  OC_REAL8m total_E;
  OC_REAL8m max_err_sq;
#if DO_MdM_SUM
  ThreeVector ave_M;
  ThreeVector ave_dM_dt;
#endif // DO_MdM_SUM
  GetEnergyDensity(*endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt,total_E);
  mxH_output.cache.state_id=endstate->Id();

  {
    // Error guess compares computed endpoint against Euler step.
    DMDT dmdt(this,*endstate,mxH_output.cache.value,vtmpB);
    dmdt.Initialize();
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_err_sq(number_of_threads,-1.0);
    std::vector<OC_REAL8m> thread_max_dm_dt_sq(number_of_threads,-1.0);
#if DO_MdM_SUM
    std::vector<ThreeVector>
      thread_M_sum(number_of_threads,ThreeVector(0.,0.,0.));
    std::vector<ThreeVector>
      thread_dM_dt_sum(number_of_threads,ThreeVector(0.,0.,0.));
#endif // DO_MdM_SUM
    Oc_AlignedVector<Oxs_Energy::SUMTYPE>
      thread_pE_pM_sum(number_of_threads,Oxs_Energy::SUMTYPE(0.0));
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(endstate->Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        Oxs_Energy::SUMTYPE thd_pE_pM_sum = 0.0;
        OC_REAL8m thd_max_dm_dt_sq = 0.0;
        OC_REAL8m thd_max_err_sq = 0.0;
#if DO_MdM_SUM
        Nb_Xpfloat thd_M_sum_x(0.),thd_M_sum_y(0.),thd_M_sum_z(0.);
        Nb_Xpfloat thd_dM_dt_sum_x(0.),thd_dM_dt_sum_y(0.),thd_dM_dt_sum_z(0.);
        const Oxs_MeshValue<OC_REAL8m>& Ms = *(endstate->Ms);
        const Oxs_MeshValue<ThreeVector>& spin = endstate->spin;
#endif
        for(OC_INDEX j=jstart;j<jstop;++j) {
          OC_REAL8m dummy=0.0;
          dmdt.Compute(j,thd_max_dm_dt_sq,dummy); // Fills vtmpB with dmdt
#if DO_MdM_SUM
          thd_dM_dt_sum_x.Accum(Ms[j]*vtmpB[j].x);
          thd_dM_dt_sum_y.Accum(Ms[j]*vtmpB[j].y);
          thd_dM_dt_sum_z.Accum(Ms[j]*vtmpB[j].z);
          thd_M_sum_x.Accum(Ms[j]*spin[j].x);
          thd_M_sum_y.Accum(Ms[j]*spin[j].y);
          thd_M_sum_z.Accum(Ms[j]*spin[j].z);
#endif
          thd_pE_pM_sum += dummy;
          ThreeVector tvec = vtmpC[j]-vtmpB[j];
          OC_REAL8m err_sq = tvec.MagSq();
          if(err_sq>thd_max_err_sq) thd_max_err_sq = err_sq;
          // NB: No spin advancement
        }
        // Allow for multiple jstart/jstop chunks
        thread_err_sq[threadid]
          = OC_MAX(thd_max_err_sq,thread_err_sq[threadid]);
        thread_max_dm_dt_sq[threadid]
          = OC_MAX(thd_max_dm_dt_sq,thread_max_dm_dt_sq[threadid]);
        thread_pE_pM_sum[threadid] += thd_pE_pM_sum;
#if DO_MdM_SUM
        thread_M_sum[threadid].x += thd_M_sum_x.GetValue();
        thread_M_sum[threadid].y += thd_M_sum_y.GetValue();
        thread_M_sum[threadid].z += thd_M_sum_z.GetValue();
        thread_dM_dt_sum[threadid].x += thd_dM_dt_sum_x.GetValue();
        thread_dM_dt_sum[threadid].y += thd_dM_dt_sum_y.GetValue();
        thread_dM_dt_sum[threadid].z += thd_dM_dt_sum_z.GetValue();
#endif
      });
    max_err_sq = thread_err_sq[0];
    OC_REAL8m max_dm_dt_sq = thread_max_dm_dt_sq[0];
    for(int i=1;i<number_of_threads;++i) {
      if(thread_err_sq[i] > max_err_sq) {
        max_err_sq = thread_err_sq[i];
      }
      if(thread_max_dm_dt_sq[i] > max_dm_dt_sq) {
        max_dm_dt_sq = thread_max_dm_dt_sq[i];
      }
      thread_pE_pM_sum[0] += thread_pE_pM_sum[i];
#if DO_MdM_SUM
      thread_M_sum[0]     += thread_M_sum[i];
      thread_dM_dt_sum[0] += thread_dM_dt_sum[i];
#endif // DO_MdM_SUM
    }
    OC_REAL8m pE_pM_sum = thread_pE_pM_sum[0];
    dmdt.Finalize(max_dm_dt_sq,pE_pM_sum); // Scale max_dm_dt_sq and pE_pM_sum

    max_dm_dt = sqrt(max_dm_dt_sq);
    timestep_lower_bound = PositiveTimestepBound(max_dm_dt);
    dE_dt = pE_pt + pE_pM_sum;
#if DO_MdM_SUM
    ave_M.x = thread_M_sum[0].x/endstate->Ms->Size();
    ave_M.y = thread_M_sum[0].y/endstate->Ms->Size();
    ave_M.z = thread_M_sum[0].z/endstate->Ms->Size();
    ave_dM_dt.x = thread_dM_dt_sum[0].x/endstate->Ms->Size();
    ave_dM_dt.y = thread_dM_dt_sum[0].y/endstate->Ms->Size();
    ave_dM_dt.z = thread_dM_dt_sum[0].z/endstate->Ms->Size();
#endif // DO_MdM_SUM
  }
  if(!endstate->AddDerivedData(DataName("Timestep lower bound"),
                               timestep_lower_bound) ||
     !endstate->AddDerivedData(DataName("Max dm/dt"),max_dm_dt) ||
#if DO_MdM_SUM
     !endstate->AddDerivedData(DataName("Mx"),ave_M.x) ||
     !endstate->AddDerivedData(DataName("My"),ave_M.y) ||
     !endstate->AddDerivedData(DataName("Mz"),ave_M.z) ||
     !endstate->AddDerivedData(DataName("dMx/dt"),ave_dM_dt.x) ||
     !endstate->AddDerivedData(DataName("dMy/dt"),ave_dM_dt.y) ||
     !endstate->AddDerivedData(DataName("dMz/dt"),ave_dM_dt.z) ||
#endif // DO_MdM_SUM
     !endstate->AddDerivedData(DataName("pE/pt"),pE_pt) ||
     !endstate->AddDerivedData(DataName("Total E"),total_E) ||
     !endstate->AddDerivedData(DataName("dE/dt"),dE_dt)) {
    throw Oxs_ExtError(this,
                       "Oxs_RungeKuttaEvolve::TakeRungeKuttaStep2Heun:"
                       " Programming error; data cache already set.");
  }
  // Move end dm_dt data into vtmpA, for use by calling routine.
  // Note that end energy is already in temp_energy, as per
  // contract.
  vtmpA.Swap(vtmpB);

  error_estimate = sqrt(max_err_sq)*stepsize/2.;
  global_error_order = 2.0;
  new_energy_and_dmdt_computed = 1;
}

template <typename DMDT>
void Oxs_RungeKuttaEvolve::TakeRungeKuttaStep4
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
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

  OC_REAL8m pE_pt;

  vtmpA.AdjustSize(cstate->mesh);
  vtmpB.AdjustSize(cstate->mesh);
  vtmpC.AdjustSize(cstate->mesh);

  // Do first half step.  Because dm_dt1 is already calculated,
  // we fill dm_dt2 directly into vtmpB.
  AdjustState(stepsize/4,stepsize/4,*cstate,current_dm_dt,
              temp_state_key.GetWriteReference());
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);

  {
    DMDT dmdt(this,temp_state_key.GetReadReference(),vtmpB,vtmpB);
    dmdt.InitializeCore();
    Oxs_SimState& tmpstate = temp_state_key.GetWriteReference();
    tmpstate.ClearDerivedData();
    tmpstate.spin.AdjustSize(cstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& tmp_spin = tmpstate.spin;
        const OC_REAL8m mstep = 0.25 * stepsize;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpB with dm_dt2
          ThreeVector vtmp = vtmpB[j];
          vtmp *= mstep;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          tmp_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,tmpstate,0.25*stepsize);
  }


  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);

  {
    DMDT dmdt(this,temp_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& tmpstate = temp_state_key.GetWriteReference();
    tmpstate.ClearDerivedData();
    tmpstate.spin.AdjustSize(cstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& tmp_spin = tmpstate.spin;
        const OC_REAL8m mstep = 0.5 * stepsize;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dm_dt3
          ThreeVector vtmp = vtmpA[j];
          vtmpB[j] += vtmp; // dm_dt2 + dm_dt3 -> vtmpB
          vtmp *= mstep;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          tmp_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,tmpstate,0.5*stepsize);
  }

  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);

  OC_REAL8m min_normsq = DBL_MAX;
  OC_REAL8m max_normsq = 0.0;
  {
    DMDT dmdt(this,temp_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& tmpstate = temp_state_key.GetWriteReference();
    tmpstate.ClearDerivedData();
    tmpstate.spin.AdjustSize(cstate->mesh);
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_min_normsq(number_of_threads,DBL_MAX);
    std::vector<OC_REAL8m> thread_max_normsq(number_of_threads,0.0);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& tmp_spin = tmpstate.spin;
        const OC_REAL8m mstep = stepsize/6.;
        OC_REAL8m thd_min_normsq = DBL_MAX;
        OC_REAL8m thd_max_normsq = 0.0;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dmdt4
          ThreeVector vtmp1 = (vtmpA[j] += current_dm_dt[j]);
#if defined(__GNUC__) && __GNUC__==4 && __GNUC_MINOR__<8
          // Lambda support in GCC 4.7 is crufty
          ThreeVector vtmp2 = vtmp1;
          vtmp2 *= 0.5;
          vtmpB[j] += vtmp2;
          vtmp2 = vtmpB[j];
#else
          ThreeVector vtmp2 = vtmpB[j].Accum(0.5,vtmp1);
          /// (dm_dt2 + dm_dt3) + 0.5*(dm_dt1+dm_dt4) -> vtmpB
#endif
          vtmp2 *= mstep;
          vtmp2 += old_spin[j];
          OC_REAL8m magsq = vtmp2.MakeUnit();
          tmp_spin[j] = vtmp2;
          if(magsq<thd_min_normsq) thd_min_normsq=magsq;
          if(magsq>thd_max_normsq) thd_max_normsq=magsq;
        }
        thread_min_normsq[threadid]
          = OC_MIN(thread_min_normsq[threadid],thd_min_normsq);
        thread_max_normsq[threadid]
          = OC_MAX(thread_max_normsq[threadid],thd_max_normsq);
        /// Allow for multiple jstart/jstop chunks
      });
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,tmpstate,0.5*stepsize);
    // Note: state time index set to lasttime + stepsize/2.  Recall that
    // "h" here is stepsize/2, so the weights on dm_dt1, dm_dt2, dm_dt3
    // and dm_dt4 are (stepsize/2)(1/6, 1/3, 1/3, 1/6), respectively.

    for(int i=0;i<number_of_threads;++i) {
      if(thread_min_normsq[i]<min_normsq) min_normsq = thread_min_normsq[i];
      if(thread_max_normsq[i]>max_normsq) max_normsq = thread_max_normsq[i];
    }

    // Save vtmpB for error estimate
    vtmpB.Swap(vtmpC);
  }

  // At this point, temp_state holds the middle point, and vtmpC holds
  // 0.5*dm_dt1 + dm_dt2 + dm_dt3 + 0.5*dm_dt4 for first half.

  // Calculate dm_dt for this middle state, and store in vtmpB.
  tstate = NULL; // Disable non-const access
  const Oxs_SimState* midstate
    = &(temp_state_key.GetReadReference());
  GetEnergyDensity(*midstate,temp_energy,&vtmpB,NULL,pE_pt);

  // Next do second half step.  Store end result in next_state
  {
    DMDT dmdt(this,*midstate,vtmpB,vtmpB);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(midstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(midstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = midstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        const OC_REAL8m mstep = 0.25 * stepsize;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpB with dm_dt2
          ThreeVector vtmp = vtmpB[j];
          vtmp *= mstep;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          new_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*midstate,newstate,0.25*stepsize);
  }

  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);

  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(midstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(midstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = midstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        const OC_REAL8m mstep = 0.25 * stepsize;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j);
          ThreeVector vtmp = vtmpA[j];
          vtmpB[j] *= 0.5;
          vtmpB[j] += vtmpA[j];
          vtmp *= mstep;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          new_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*midstate,newstate,0.25*stepsize);
  }

  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);

  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(midstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(midstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = midstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        const OC_REAL8m mstep = 0.5 * stepsize;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dm_dt3
          ThreeVector vtmp = vtmpA[j];
          vtmpB[j] += vtmpA[j];
          // vtmpA holds dm_dt3, vtmpB holds dm_dt1/2 + dm_dt2 + dm_dt3.
          vtmp *= mstep;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          new_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*midstate,newstate,0.5*stepsize);
  }

  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    nstate = &(next_state_key.GetWriteReference());
    nstate->ClearDerivedData();
    nstate->spin.AdjustSize(midstate->mesh);
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_min_normsq(number_of_threads,DBL_MAX);
    std::vector<OC_REAL8m> thread_max_normsq(number_of_threads,0.0);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(midstate->Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = midstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = nstate->spin;
        const OC_REAL8m mstep = stepsize/6.;
        OC_REAL8m thd_min_normsq = DBL_MAX;
        OC_REAL8m thd_max_normsq = 0.0;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dmdt4
#if defined(__GNUC__) && __GNUC__==4 && __GNUC_MINOR__<8
          // Lambda support in GCC 4.7 is crufty
          ThreeVector vtmp = vtmpA[j];
          vtmp *= 0.5;
          vtmpB[j] += vtmp;
          vtmp = vtmpB[j];
#else
          ThreeVector vtmp = vtmpB[j].Accum(0.5,vtmpA[j]);
#endif
          vtmpC[j] += vtmp; // Combine vtmpB with results from first
                   /// half-step. This is used for error estimation.
          vtmp *= mstep;
          vtmp += old_spin[j];
          OC_REAL8m magsq = vtmp.MakeUnit();
          new_spin[j] = vtmp;
          if(magsq<thd_min_normsq) thd_min_normsq=magsq;
          if(magsq>thd_max_normsq) thd_max_normsq=magsq;
        }
        thread_min_normsq[threadid]
          = OC_MIN(thread_min_normsq[threadid],thd_min_normsq);
        thread_max_normsq[threadid]
          = OC_MAX(thread_max_normsq[threadid],thd_max_normsq);
        /// Allow for multiple jstart/jstop chunks
      });
    dmdt.FinalizeCore();
    midstate = NULL; // We're done using midstate

    // Tweak "last_timestep" field in next_state, and adjust other
    // time fields to protect against rounding errors.
    UpdateTimeFields(*cstate,*nstate,stepsize);

    for(int i=0;i<number_of_threads;++i) {
      if(thread_min_normsq[i]<min_normsq) min_normsq = thread_min_normsq[i];
      if(thread_max_normsq[i]>max_normsq) max_normsq = thread_max_normsq[i];
    }
  }

  norm_error = OC_MAX(sqrt(max_normsq)-1.0,
                      1.0 - sqrt(min_normsq));

  nstate = NULL;
  next_state_key.GetReadReference(); // Lock down (safety)

  // Repeat now for full step, storing end result into temp_state
  AdjustState(stepsize/2,stepsize/2,*cstate,current_dm_dt,
              temp_state_key.GetWriteReference());
  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);

  {
    DMDT dmdt(this,temp_state_key.GetReadReference(),vtmpB,vtmpB);
    dmdt.InitializeCore();
    Oxs_SimState& tmpstate = temp_state_key.GetWriteReference();
    tmpstate.ClearDerivedData();
    tmpstate.spin.AdjustSize(cstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& tmp_spin = tmpstate.spin;
        const OC_REAL8m mstep = 0.5 * stepsize;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpB with dm_dt2
          ThreeVector vtmp = vtmpB[j];
          vtmp *= mstep;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          tmp_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,tmpstate,0.5*stepsize);
  }

  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  {
    DMDT dmdt(this,temp_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& tmpstate = temp_state_key.GetWriteReference();
    tmpstate.ClearDerivedData();
    tmpstate.spin.AdjustSize(cstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& tmp_spin = tmpstate.spin;
        const OC_REAL8m mstep = stepsize;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dm_dt3
          ThreeVector vtmp = vtmpA[j];
          vtmpB[j] += vtmp; // dm_dt2 + dm_dt3 -> vtmpB
          vtmp *= mstep;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          tmp_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,tmpstate,stepsize);
  }

  GetEnergyDensity(temp_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  OC_REAL8m max_error_sq = 0.0;
  {
    const Oxs_SimState& tmpstate = temp_state_key.GetReadReference();
    DMDT dmdt(this,tmpstate,vtmpA,vtmpA);
    dmdt.InitializeCore();
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_max_error_sq(number_of_threads,0.0);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(tmpstate.Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        OC_REAL8m thd_max_error_sq = 0.0;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dm_dt4
          vtmpA[j] += current_dm_dt[j];
#if defined(__GNUC__) && __GNUC__==4 && __GNUC_MINOR__<8
          // Lambda support in GCC 4.7 is crufty
          ThreeVector vtmp = vtmpA[j];
          vtmp *= 0.5;
          vtmpB[j] += vtmp;
          vtmp = vtmpB[j];
#else
          ThreeVector vtmp = vtmpB[j].Accum(0.5,vtmpA[j]);
          /// Add in 0.5*(dm_dt1+dm_dt4) to vtmpB
#endif

          // Estimate error (Note: no spin advancement)
          vtmp.Accum(-0.5,vtmpC[j]);
          OC_REAL8m error_sq = vtmp.MagSq();
          if(error_sq>thd_max_error_sq) thd_max_error_sq = error_sq;
        }
        thread_max_error_sq[threadid]
          = OC_MAX(thread_max_error_sq[threadid],thd_max_error_sq);
        /// Allow for multiple jstart/jstop chunks
      });
    dmdt.FinalizeCore();
    max_error_sq = thread_max_error_sq[0];
    for(int i=1;i<number_of_threads;++i) {
      if(thread_max_error_sq[i]>max_error_sq) {
        max_error_sq = thread_max_error_sq[i];
      }
    }
  }

  error_estimate = sqrt(max_error_sq)*stepsize/3.;
  /// vtmpB hold 0.5*dm_dt1 + dm_dt2 + dm_dt3 + 0.5*dm_dt4,
  /// but successor state looks like
  ///    m2 = m1 + dm_dt1*h/6 + dm_dt2*h/3 + dm_dt3*h/3 + dm_dt4*h/6 + O(h^5)

  global_error_order = 4.0;
  new_energy_and_dmdt_computed = 0;
}

template <typename DMDT>
void Oxs_RungeKuttaEvolve::RungeKuttaFehlbergBase54
(RKF_SubType method,
 OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
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

  RKTIME_START(1);
  RKTIME_START(2);

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
  //   J. R. Dormand and P. J. Prince, ``A reconsideration of some
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
  // evaluation is at the candidate next state, it is re-used
  // on the next step unless the step rejected.
  //
  // The error estimate is
  //
  //     E = |m2b-m2a| = h*|Db-Da| = C*h^6
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

  // Four temporary arrays, A-D, are used.  The original implementation
  // of this routine broke down into 12 steps, with usage pattern
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
  // Here dD is Db-Da.  We don't need to compute Db directly.
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
  //   When the code was parallelized the steps were collapsed and
  // data movement reduced.  Usage is now
  //
  // Step \  Temp
  // Index \ Array:  A         B         C         D
  // ------+---------------------------------------------
  //  1-2  |      dm_dt2       -         -         -
  //  3-4  |      dm_dt2     dm_dt3      -         -
  //  5-6  |      dm_dt2     dm_dt3    dm_dt4      -
  //  7-8  |      dm_dt2     dm_dt3    dm_dt4    dm_dt5
  //  9-11 |      dm_dt6     dD(3,6)   dm_dt4    dm_dt5
  //  12   |      dm_dt7       dD      dm_dt4    dm_dt5

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
    a1 = OC_REAL8m(1.)/OC_REAL8m(5.);
    a2 = OC_REAL8m(3.)/OC_REAL8m(10.);
    a3 = OC_REAL8m(6.)/OC_REAL8m(13.);
    a4 = OC_REAL8m(2.)/OC_REAL8m(3.);
    // a5 and a6 are 1.0

    b11 =      OC_REAL8m(1.)/OC_REAL8m(5.);

    b21 =      OC_REAL8m(3.)/OC_REAL8m(40.);
    b22 =      OC_REAL8m(9.)/OC_REAL8m(40.);

    b31 =    OC_REAL8m(264.)/OC_REAL8m(2197.);
    b32 =    OC_REAL8m(-90.)/OC_REAL8m(2197.);
    b33 =    OC_REAL8m(840.)/OC_REAL8m(2197.);

    b41 =    OC_REAL8m(932.)/OC_REAL8m(3645.);
    b42 =    OC_REAL8m(-14.)/OC_REAL8m(27.);
    b43 =   OC_REAL8m(3256.)/OC_REAL8m(5103.);
    b44 =   OC_REAL8m(7436.)/OC_REAL8m(25515.);

    b51 =   OC_REAL8m(-367.)/OC_REAL8m(513.);
    b52 =     OC_REAL8m(30.)/OC_REAL8m(19.);
    b53 =   OC_REAL8m(9940.)/OC_REAL8m(5643.);
    b54 = OC_REAL8m(-29575.)/OC_REAL8m(8208.);
    b55 =   OC_REAL8m(6615.)/OC_REAL8m(3344.);

    b61 =     OC_REAL8m(35.)/OC_REAL8m(432.);
    b63 =   OC_REAL8m(8500.)/OC_REAL8m(14553.);
    b64 = OC_REAL8m(-28561.)/OC_REAL8m(84672.);
    b65 =    OC_REAL8m(405.)/OC_REAL8m(704.);
    b66 =     OC_REAL8m(19.)/OC_REAL8m(196.);
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =     OC_REAL8m(11.)/OC_REAL8m(108.)    - b61;
    dc3 =   OC_REAL8m(6250.)/OC_REAL8m(14553.)  - b63;
    dc4 =  OC_REAL8m(-2197.)/OC_REAL8m(21168.)  - b64;
    dc5 =     OC_REAL8m(81.)/OC_REAL8m(176.)    - b65;
    dc6 =    OC_REAL8m(171.)/OC_REAL8m(1960.)   - b66;
    dc7 =      OC_REAL8m(1.)/OC_REAL8m(40.);
    break;

  case RK547FM:
    /////////////////////////////////////////////////////////////////
    // Coefficients for Dormand & Prince RK5(4)7FM
    a1 = OC_REAL8m(1.)/OC_REAL8m(5.);
    a2 = OC_REAL8m(3.)/OC_REAL8m(10.);
    a3 = OC_REAL8m(4.)/OC_REAL8m(5.);
    a4 = OC_REAL8m(8.)/OC_REAL8m(9.);
    // a5 and a6 are 1.0

    b11 =      OC_REAL8m(1.)/OC_REAL8m(5.);

    b21 =      OC_REAL8m(3.)/OC_REAL8m(40.);
    b22 =      OC_REAL8m(9.)/OC_REAL8m(40.);

    b31 =     OC_REAL8m(44.)/OC_REAL8m(45.);
    b32 =    OC_REAL8m(-56.)/OC_REAL8m(15.);
    b33 =     OC_REAL8m(32.)/OC_REAL8m(9.);

    b41 =  OC_REAL8m(19372.)/OC_REAL8m(6561.);
    b42 = OC_REAL8m(-25360.)/OC_REAL8m(2187.);
    b43 =  OC_REAL8m(64448.)/OC_REAL8m(6561.);
    b44 =   OC_REAL8m(-212.)/OC_REAL8m(729.);

    b51 =   OC_REAL8m(9017.)/OC_REAL8m(3168.);
    b52 =   OC_REAL8m(-355.)/OC_REAL8m(33.);
    b53 =  OC_REAL8m(46732.)/OC_REAL8m(5247.);
    b54 =     OC_REAL8m(49.)/OC_REAL8m(176.);
    b55 =  OC_REAL8m(-5103.)/OC_REAL8m(18656.);

    b61 =     OC_REAL8m(35.)/OC_REAL8m(384.);
    b63 =    OC_REAL8m(500.)/OC_REAL8m(1113.);
    b64 =    OC_REAL8m(125.)/OC_REAL8m(192.);
    b65 =  OC_REAL8m(-2187.)/OC_REAL8m(6784.);
    b66 =     OC_REAL8m(11.)/OC_REAL8m(84.);
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =   OC_REAL8m(5179.)/OC_REAL8m(57600.)  - b61;
    dc3 =   OC_REAL8m(7571.)/OC_REAL8m(16695.)  - b63;
    dc4 =    OC_REAL8m(393.)/OC_REAL8m(640.)    - b64;
    dc5 = OC_REAL8m(-92097.)/OC_REAL8m(339200.) - b65;
    dc6 =    OC_REAL8m(187.)/OC_REAL8m(2100.)   - b66;
    dc7 =      OC_REAL8m(1.)/OC_REAL8m(40.);
    break;
  case RK547FS:
    /////////////////////////////////////////////////////////////////
    // Coefficients for Dormand & Prince RK5(4)7FS
    a1 = OC_REAL8m(2.)/OC_REAL8m(9.);
    a2 = OC_REAL8m(1.)/OC_REAL8m(3.);
    a3 = OC_REAL8m(5.)/OC_REAL8m(9.);
    a4 = OC_REAL8m(2.)/OC_REAL8m(3.);
    // a5 and a6 are 1.0

    b11 =      OC_REAL8m(2.)/OC_REAL8m(9.);

    b21 =      OC_REAL8m(1.)/OC_REAL8m(12.);
    b22 =      OC_REAL8m(1.)/OC_REAL8m(4.);

    b31 =     OC_REAL8m(55.)/OC_REAL8m(324.);
    b32 =    OC_REAL8m(-25.)/OC_REAL8m(108.);
    b33 =     OC_REAL8m(50.)/OC_REAL8m(81.);

    b41 =     OC_REAL8m(83.)/OC_REAL8m(330.);
    b42 =    OC_REAL8m(-13.)/OC_REAL8m(22.);
    b43 =     OC_REAL8m(61.)/OC_REAL8m(66.);
    b44 =      OC_REAL8m(9.)/OC_REAL8m(110.);

    b51 =    OC_REAL8m(-19.)/OC_REAL8m(28.);
    b52 =      OC_REAL8m(9.)/OC_REAL8m(4.);
    b53 =      OC_REAL8m(1.)/OC_REAL8m(7.);
    b54 =    OC_REAL8m(-27.)/OC_REAL8m(7.);
    b55 =     OC_REAL8m(22.)/OC_REAL8m(7.);

    b61 =     OC_REAL8m(19.)/OC_REAL8m(200.);
    b63 =      OC_REAL8m(3.)/OC_REAL8m(5.);
    b64 =   OC_REAL8m(-243.)/OC_REAL8m(400.);
    b65 =     OC_REAL8m(33.)/OC_REAL8m(40.);
    b66 =      OC_REAL8m(7.)/OC_REAL8m(80.);
    // b62 is 0.0

    // Coefs for error calculation (c^[k] - c[k]).
    // Note that c[k] = b6k, and c^[2]=c[2]=0.0
    dc1 =    OC_REAL8m(431.)/OC_REAL8m(5000.)  - b61;
    dc3 =    OC_REAL8m(333.)/OC_REAL8m(500.)   - b63;
    dc4 =  OC_REAL8m(-7857.)/OC_REAL8m(10000.) - b64;
    dc5 =    OC_REAL8m(957.)/OC_REAL8m(1000.)  - b65;
    dc6 =    OC_REAL8m(193.)/OC_REAL8m(2000.)  - b66;
    dc7 =     OC_REAL8m(-1.)/OC_REAL8m(50.);
    break;
  default:
    throw Oxs_ExtError(this,
                 "Oxs_RungeKuttaEvolve::RungeKuttaFehlbergBase54:"
                 " Programming error; Invalid sub-type.");
  }

#ifndef NDEBUG
  // COEFFICIENT CHECKS ////////////////////////////////////////
  // Try to catch some simple typing errors.  Oc_Nop calls below force
  // evaluation in order shown; otherwise, some compiler optimizations
  // reorder sums into form with less accuracy.
#define EPS (8*OC_REAL8_EPSILON)  // 6*OC_REAL8_EPSILON should be enough,
  /// but include a little slack compilers with bad numeric taste.
  if(fabs(b11-a1)>EPS ||
     fabs(b21+b22-a2)>EPS ||
     fabs(b31+b32+b33-a3)>EPS ||
     fabs(b41+b42+b43+b44-a4)>EPS ||
     fabs(Oc_Nop(b51+b52+b53) -1.0 + Oc_Nop(b54+b55))>EPS ||
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
#endif // NDEBUG

  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());
  SpinDiff("\nPt A",*cstate);

  OC_REAL8m pE_pt;

  vtmpA.AdjustSize(cstate->mesh);
  vtmpB.AdjustSize(cstate->mesh);
  vtmpC.AdjustSize(cstate->mesh);
  vtmpD.AdjustSize(cstate->mesh);

  RKTIME_STOP(2,"RKFB54 setup");

  // Step 1
  RKTIME_START(3);
  AdjustState(stepsize*a1,stepsize*b11,*cstate,current_dm_dt,
              next_state_key.GetWriteReference());
  RKTIME_STOP(3,"RKF54 step 1",(cstate->mesh->Size())*9*sizeof(OC_REAL8m));

  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);

  // Steps 1 and 2
  RKTIME_START(4);
  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(cstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dmdt
          ThreeVector vtmp = b21*current_dm_dt[j] + b22*vtmpA[j];
          vtmp *= stepsize;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          new_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,newstate,a2*stepsize);
  }
  RKTIME_STOP(4,"RKF54 step 2",
              (cstate->mesh->Size())*(11+3*4)*sizeof(OC_REAL8m));

  // Steps 3 and 4
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpB,NULL,pE_pt);
  RKTIME_START(5);
  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpB,vtmpB);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(cstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpB with dmdt
          ThreeVector vtmp
            = b31*current_dm_dt[j] + b32*vtmpA[j] + b33*vtmpB[j];
          vtmp *= stepsize;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          new_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,newstate,a3*stepsize);
  }
  RKTIME_STOP(5,"RKF54 step 5-6",
              (cstate->mesh->Size())*(11+3*5)*sizeof(OC_REAL8m));

  // Steps 5 and 6
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpC,NULL,pE_pt);
  RKTIME_START(6);
  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpC,vtmpC);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(cstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpC with dmdt
          ThreeVector vtmp
            = b41*current_dm_dt[j] + b42*vtmpA[j]
            + b43*vtmpB[j] + b44*vtmpC[j];
          vtmp *= stepsize;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          new_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,newstate,a4*stepsize);
  }
  RKTIME_STOP(6,"RKF54 step 5-6",
              (cstate->mesh->Size())*(11+3*6)*sizeof(OC_REAL8m));

  // Steps 7 and 8
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpD,NULL,pE_pt);

  RKTIME_START(7);
  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpD,vtmpD);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(cstate->mesh);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpD with dmdt
          ThreeVector vtmp
            = b51*current_dm_dt[j] + b52*vtmpA[j]
            + b53*vtmpB[j] + b54*vtmpC[j] + b55*vtmpD[j];
          vtmp *= stepsize;
          vtmp += old_spin[j];
          vtmp.MakeUnit();
          new_spin[j] = vtmp;
        }});
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,newstate,stepsize); // a5==1.0
  }
  RKTIME_STOP(7,"RKF54 step 7-8",
              (cstate->mesh->Size())*(11+3*7)*sizeof(OC_REAL8m));

  // Steps 9-11
  GetEnergyDensity(next_state_key.GetReadReference(),temp_energy,
                   &vtmpA,NULL,pE_pt);
  RKTIME_START(8);
  {
    DMDT dmdt(this,next_state_key.GetReadReference(),vtmpA,vtmpA);
    dmdt.InitializeCore();
    Oxs_SimState& newstate = next_state_key.GetWriteReference();
    newstate.ClearDerivedData();
    newstate.spin.AdjustSize(cstate->mesh);
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_norm_error(number_of_threads,-1.0);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(cstate->Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        const Oxs_MeshValue<ThreeVector>& old_spin = cstate->spin;
        Oxs_MeshValue<ThreeVector>& new_spin = newstate.spin;
        OC_REAL8m min_normsq = DBL_MAX;
        OC_REAL8m max_normsq = 0.0;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          dmdt.ComputeCore(j); // Fills vtmpA with dmdt
          ThreeVector vtmp6 = vtmpA[j];
          ThreeVector vtmp3 = vtmpB[j];
          vtmpB[j] = dc6*vtmp6 + dc3*vtmp3;
          vtmp6 *= b66;
          vtmp6 += b63*vtmp3
            + b61*current_dm_dt[j]
            + b64*vtmpC[j]
            + b65*vtmpD[j];
          vtmp6 *= stepsize;
          vtmp6 += old_spin[j];
          OC_REAL8m magsq = vtmp6.MakeUnit();
          new_spin[j] = vtmp6;
          if(magsq<min_normsq) min_normsq=magsq;
          if(magsq>max_normsq) max_normsq=magsq;
        }
        OC_REAL8m thderror = OC_MAX(sqrt(max_normsq)-1.0,
                                    1.0 - sqrt(min_normsq));
        thread_norm_error[threadid]
          = OC_MAX(thderror,thread_norm_error[threadid]);
        /// Allow for multiple jstart/jstop chunks
      });
    dmdt.FinalizeCore();
    UpdateTimeFields(*cstate,newstate,stepsize); // a6==1.0
    OC_REAL8m maxerror = thread_norm_error[0];
    for(int i=1;i<number_of_threads;++i) {
      if(thread_norm_error[i]>maxerror) maxerror = thread_norm_error[i];
    }
    norm_error = maxerror;
  }
  const Oxs_SimState& endstate
    = next_state_key.GetReadReference(); // Candidate next state
  RKTIME_STOP(8,"RKF54 step 9-11",
              (cstate->mesh->Size())*(11+3*7)*sizeof(OC_REAL8m));

  // Step 12
  OC_REAL8m total_E;
#if DO_MdM_SUM
  ThreeVector ave_M;
  ThreeVector ave_dM_dt;
#endif // DO_MdM_SUM
  GetEnergyDensity(endstate,temp_energy,&mxH_output.cache.value,
                   NULL,pE_pt,total_E);
  RKTIME_START(9);
  OC_REAL8m max_dD_sq=0.0;
  OC_REAL8m max_dm_dt_sq=0.0,pE_pM_sum=0.0; // Be certain to initialize these!
  {
    DMDT dmdt(this,endstate,mxH_output.cache.value,vtmpA);
    dmdt.Initialize();
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_dD_sq(number_of_threads,-1.0);
    std::vector<OC_REAL8m> thread_max_dm_dt_sq(number_of_threads,-1.0);
#if DO_MdM_SUM
    std::vector<ThreeVector>
      thread_M_sum(number_of_threads,ThreeVector(0.,0.,0.));
    std::vector<ThreeVector>
      thread_dM_dt_sum(number_of_threads,ThreeVector(0.,0.,0.));
#endif // DO_MdM_SUM
    Oc_AlignedVector<Oxs_Energy::SUMTYPE>
      thread_pE_pM_sum(number_of_threads,Oxs_Energy::SUMTYPE(0.0));
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (*(endstate.Ms),
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        Oxs_Energy::SUMTYPE thd_pE_pM_sum = 0.0;
        OC_REAL8m thd_max_dm_dt_sq = 0.0;
        OC_REAL8m thd_max_dD_sq = 0.0;
#if DO_MdM_SUM
        Nb_Xpfloat thd_M_sum_x(0.),thd_M_sum_y(0.),thd_M_sum_z(0.);
        Nb_Xpfloat thd_dM_dt_sum_x(0.),thd_dM_dt_sum_y(0.),thd_dM_dt_sum_z(0.);
        const Oxs_MeshValue<OC_REAL8m>& Ms = *(endstate.Ms);
        const Oxs_MeshValue<ThreeVector>& spin = endstate.spin;
#endif
        for(OC_INDEX j=jstart;j<jstop;++j) {
          OC_REAL8m dummy=0.0;
          dmdt.Compute(j,thd_max_dm_dt_sq,dummy);  // Fills vtmpA with dmdt
#if DO_MdM_SUM
          const OC_REAL8m Mtmp = Ms[j];
# if USE_XPFLOATDUALACCUM
          Nb_XpfloatDualAccum(thd_dM_dt_sum_x,Mtmp*vtmpA[j].x,
                              thd_dM_dt_sum_y,Mtmp*vtmpA[j].y);
          Nb_XpfloatDualAccum(thd_dM_dt_sum_z,Mtmp*vtmpA[j].z,
                              thd_M_sum_x,Mtmp*spin[j].x);
          Nb_XpfloatDualAccum(thd_M_sum_y,Mtmp*spin[j].y,
                              thd_M_sum_z,Mtmp*spin[j].z);
# else // USE_XPFLOATDUALACCUM
          thd_dM_dt_sum_x.Accum(Mtmp*vtmpA[j].x);
          thd_dM_dt_sum_y.Accum(Mtmp*vtmpA[j].y);
          thd_dM_dt_sum_z.Accum(Mtmp*vtmpA[j].z);
          thd_M_sum_x.Accum(Mtmp*spin[j].x);
          thd_M_sum_y.Accum(Mtmp*spin[j].y);
          thd_M_sum_z.Accum(Mtmp*spin[j].z);
# endif // USE_XPFLOATDUALACCUM
#endif
          thd_pE_pM_sum += dummy;
          vtmpB[j] += dc1*current_dm_dt[j]
            + dc4*vtmpC[j]
            + dc5*vtmpD[j]
            + dc7*vtmpA[j];
          // NB: No spin advancement
          OC_REAL8m magsq = vtmpB[j].MagSq();
          if(magsq>thd_max_dD_sq) thd_max_dD_sq = magsq;
        }
        // Allow for multiple jstart/jstop chunks
        thread_dD_sq[threadid]
          = OC_MAX(thd_max_dD_sq,thread_dD_sq[threadid]);
        thread_max_dm_dt_sq[threadid]
          = OC_MAX(thd_max_dm_dt_sq,thread_max_dm_dt_sq[threadid]);
        thread_pE_pM_sum[threadid] += thd_pE_pM_sum;
#if DO_MdM_SUM
        thread_M_sum[threadid].x += thd_M_sum_x.GetValue();
        thread_M_sum[threadid].y += thd_M_sum_y.GetValue();
        thread_M_sum[threadid].z += thd_M_sum_z.GetValue();
        thread_dM_dt_sum[threadid].x += thd_dM_dt_sum_x.GetValue();
        thread_dM_dt_sum[threadid].y += thd_dM_dt_sum_y.GetValue();
        thread_dM_dt_sum[threadid].z += thd_dM_dt_sum_z.GetValue();
#endif
      });
    max_dD_sq = thread_dD_sq[0];
    max_dm_dt_sq = thread_max_dm_dt_sq[0];
    for(int i=1;i<number_of_threads;++i) {
      if(thread_dD_sq[i] > max_dD_sq) {
        max_dD_sq = thread_dD_sq[i];
      }
      if(thread_max_dm_dt_sq[i] > max_dm_dt_sq) {
        max_dm_dt_sq = thread_max_dm_dt_sq[i];
      }
      thread_pE_pM_sum[0] += thread_pE_pM_sum[i];
#if DO_MdM_SUM
      thread_M_sum[0]     += thread_M_sum[i];
      thread_dM_dt_sum[0] += thread_dM_dt_sum[i];
#endif // DO_MdM_SUM
    }
    pE_pM_sum = thread_pE_pM_sum[0];
    dmdt.Finalize(max_dm_dt_sq,pE_pM_sum);
#if DO_MdM_SUM
    ave_M.x = thread_M_sum[0].x/endstate.Ms->Size();
    ave_M.y = thread_M_sum[0].y/endstate.Ms->Size();
    ave_M.z = thread_M_sum[0].z/endstate.Ms->Size();
    ave_dM_dt.x = thread_dM_dt_sum[0].x/endstate.Ms->Size();
    ave_dM_dt.y = thread_dM_dt_sum[0].y/endstate.Ms->Size();
    ave_dM_dt.z = thread_dM_dt_sum[0].z/endstate.Ms->Size();
#endif // DO_MdM_SUM
  }
  mxH_output.cache.state_id=endstate.Id();
  RKTIME_STOP(9,"RKF54 step 12",
              (cstate->mesh->Size())*(11+3*5)*sizeof(OC_REAL8m));

  OC_REAL8m max_dm_dt = sqrt(max_dm_dt_sq);
  OC_REAL8m timestep_lower_bound = PositiveTimestepBound(max_dm_dt);
  OC_REAL8m dE_dt = pE_pt + pE_pM_sum;
  if(!endstate.AddDerivedData(DataName("Timestep lower bound"),
                                timestep_lower_bound) ||
     !endstate.AddDerivedData(DataName("Max dm/dt"),max_dm_dt) ||
#if DO_MdM_SUM
     !endstate.AddDerivedData(DataName("Mx"),ave_M.x) ||
     !endstate.AddDerivedData(DataName("My"),ave_M.y) ||
     !endstate.AddDerivedData(DataName("Mz"),ave_M.z) ||
     !endstate.AddDerivedData(DataName("dMx/dt"),ave_dM_dt.x) ||
     !endstate.AddDerivedData(DataName("dMy/dt"),ave_dM_dt.y) ||
     !endstate.AddDerivedData(DataName("dMz/dt"),ave_dM_dt.z) ||
#endif // DO_MdM_SUM
     !endstate.AddDerivedData(DataName("pE/pt"),pE_pt) ||
     !endstate.AddDerivedData(DataName("Total E"),total_E) ||
     !endstate.AddDerivedData(DataName("dE/dt"),dE_dt)) {
    throw Oxs_ExtError(this,
                 "Oxs_RungeKuttaEvolve::RungeKuttaFehlbergBase54:"
                 " Programming error; data cache already set.");
  }
  // Array holdings: A=dm_dt7   B=dD   C=dm_dt4   D=dm_dt5

  error_estimate = stepsize * sqrt(max_dD_sq);
  global_error_order = 5.0;

  new_energy_and_dmdt_computed = 1;

  RKTIME_STOP(1,"RKFB54 total");
}

template <typename DMDT>
void Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54<DMDT>(RK547FC,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     new_energy_and_dmdt_computed);
}

template <typename DMDT>
void Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54M
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54<DMDT>(RK547FM,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     new_energy_and_dmdt_computed);
}

template <typename DMDT>
void Oxs_RungeKuttaEvolve::TakeRungeKuttaFehlbergStep54S
(OC_REAL8m stepsize,
 Oxs_ConstKey<Oxs_SimState> current_state_key,
 const Oxs_MeshValue<ThreeVector>& current_dm_dt,
 Oxs_Key<Oxs_SimState>& next_state_key,
 OC_REAL8m& error_estimate,OC_REAL8m& global_error_order,
 OC_REAL8m& norm_error,
 OC_BOOL& new_energy_and_dmdt_computed)
{
  RungeKuttaFehlbergBase54<DMDT>(RK547FS,stepsize,
     current_state_key,current_dm_dt,next_state_key,
     error_estimate,global_error_order,norm_error,
     new_energy_and_dmdt_computed);
}

OC_REAL8m Oxs_RungeKuttaEvolve::MaxDiff
(const Oxs_MeshValue<ThreeVector>& vecA,
 const Oxs_MeshValue<ThreeVector>& vecB)
{
  OC_INDEX size = vecA.Size();
  if(vecB.Size()!=size) {
    throw Oxs_ExtError(this,
                 "Oxs_RungeKuttaEvolve::MaxDiff:"
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

void Oxs_RungeKuttaEvolve::AdjustStepHeadroom(OC_INT4m step_reject)
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

////////////////////////////////////////////////////////////////////////
/// Oxs_RungeKuttaEvolve::ComputeEnergyChange  /////////////////////////
////////////////////////////////////////////////////////////////////////
void
Oxs_RungeKuttaEvolve::ComputeEnergyChange
(const Oxs_Mesh* mesh,
 const Oxs_MeshValue<OC_REAL8m>& current_energy,
 const Oxs_MeshValue<OC_REAL8m>& candidate_energy,
 OC_REAL8m& dE,OC_REAL8m& var_dE,OC_REAL8m& total_E)
{ // Computes cellwise difference between energies, and variance.
  // Export total_E is "current" energy (used for stepsize control).

  const int thread_count = Oc_GetMaxThreadCount();
  Oc_AlignedVector<Oxs_Energy::SUMTYPE>
    thread_dE(thread_count,Oxs_Energy::SUMTYPE(0.0));
  Oc_AlignedVector<Oxs_Energy::SUMTYPE>
    thread_var_dE(thread_count,Oxs_Energy::SUMTYPE(0.0));
  Oc_AlignedVector<Oxs_Energy::SUMTYPE>
    thread_total_E(thread_count,Oxs_Energy::SUMTYPE(0.0));

  OC_REAL8m common_volume;
  if(mesh->HasUniformCellVolumes(common_volume)) {
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (current_energy,
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        // Imports (local copies)
        const Oxs_MeshValue<OC_REAL8m>& thd_current_energy   = current_energy;
        const Oxs_MeshValue<OC_REAL8m>& thd_candidate_energy = candidate_energy;

        Oxs_Energy::SUMTYPE thd_dE=0.0, thd_var_dE=0.0, thd_total_E=0.0;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          OC_REAL8m e = thd_current_energy[j];
          OC_REAL8m new_e = thd_candidate_energy[j];
          thd_var_dE  += new_e*new_e + e*e;
          thd_dE      += (new_e - e);
          thd_total_E += e;
        }
        thread_dE[threadid]      += thd_dE;
        thread_total_E[threadid] += thd_total_E;
        thread_var_dE[threadid]  += thd_var_dE;
      });
    for(OC_INDEX i=1;i<thread_count;++i) {
      thread_var_dE[0]  += thread_var_dE[i];
      thread_dE[0]      += thread_dE[i];
      thread_total_E[0] += thread_total_E[i];
    }
    thread_var_dE[0]  *= common_volume*common_volume;
    thread_dE[0]      *= common_volume;
    thread_total_E[0] *= common_volume;
  } else {
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (current_energy,
      [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
        // Imports (local copies)
        const Oxs_Mesh& thd_mesh                             = *mesh;
        const Oxs_MeshValue<OC_REAL8m>& thd_current_energy   = current_energy;
        const Oxs_MeshValue<OC_REAL8m>& thd_candidate_energy = candidate_energy;

        Oxs_Energy::SUMTYPE thd_dE=0.0, thd_var_dE=0.0, thd_total_E=0.0;
        for(OC_INDEX j=jstart;j<jstop;++j) {
          OC_REAL8m vol   = thd_mesh.Volume(j);
          OC_REAL8m e     = thd_current_energy[j];
          OC_REAL8m new_e = thd_candidate_energy[j];
          thd_var_dE  += vol*vol*(new_e*new_e + e*e);
          thd_dE      += vol*(new_e - e);
          thd_total_E += vol*e;
        }
        thread_dE[threadid] += thd_dE;
        thread_total_E[threadid] += thd_total_E;
        thread_var_dE[threadid] += thd_var_dE;
      });
    for(OC_INDEX i=1;i<thread_count;++i) {
      thread_var_dE[0]  += thread_var_dE[i];
      thread_dE[0]      += thread_dE[i];
      thread_total_E[0] += thread_total_E[i];
    }
  }
  dE      = thread_dE[0];
  var_dE  = thread_var_dE[0];
  total_E = thread_total_E[0];
}

OC_BOOL
Oxs_RungeKuttaEvolve::InitNewStage
(const Oxs_TimeDriver* /* driver */,
 Oxs_ConstKey<Oxs_SimState> state,
 Oxs_ConstKey<Oxs_SimState> prevstate)
{
  // Note 1: state is a copy-by-value import, so its read lock
  //         will be released on exit.
  // Note 2: pstate_ptr will be NULL if prevstate has
  //         "INVALID" status.
  const Oxs_SimState& cstate = state.GetReadReference();
  const Oxs_SimState* pstate_ptr = prevstate.GetPtr();
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(cstate.Ms);
  const Oxs_MeshValue<ThreeVector>& spin = cstate.spin;

  // Compute ave M values in derived data area.  If prevstate!=0 (i.e.,
  // this is not the very first stage) then we could copy the values
  // from prevstate.  But there is no prohibition against the new state
  // initialization code changing the magnetization state (which for
  // example could be useful for running FORC simulations), so instead
  // we just compute ave M from scratch.  We don't expect new stages to
  // be that frequent, but if that expectation is not borne out then we
  // could implement a flag to indicate when M isn't changed across stage
  // boundaries and then copy ave M.
  const int number_of_threads = Oc_GetMaxThreadCount();
  std::vector<ThreeVector>
    thread_M_sum(number_of_threads,ThreeVector(0.,0.,0.));
  Oxs_RunThreaded<ThreeVector,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
                  (spin,
                   [&](OC_INT4m threadid,OC_INDEX jstart,OC_INDEX jstop) {
                     Nb_Xpfloat thd_M_sum_x(0.),thd_M_sum_y(0.),thd_M_sum_z(0.);
                     for(OC_INDEX j=jstart;j<jstop;++j) {
                      thd_M_sum_x.Accum(Ms[j]*spin[j].x);
                      thd_M_sum_y.Accum(Ms[j]*spin[j].y);
                      thd_M_sum_z.Accum(Ms[j]*spin[j].z);
                     }
                     thread_M_sum[threadid].x += thd_M_sum_x.GetValue();
                     thread_M_sum[threadid].y += thd_M_sum_y.GetValue();
                     thread_M_sum[threadid].z += thd_M_sum_z.GetValue();

  });
  for(int i=1;i<number_of_threads;++i) {
    thread_M_sum[0] += thread_M_sum[i];
  }
  cstate.AddDerivedData(DataName("Mx"),
     thread_M_sum[0].x/static_cast<OC_REAL8m>(spin.Size()));
  cstate.AddDerivedData(DataName("My"),
     thread_M_sum[0].y/static_cast<OC_REAL8m>(spin.Size()));
  cstate.AddDerivedData(DataName("Mz"),
     thread_M_sum[0].z/static_cast<OC_REAL8m>(spin.Size()));

  // Update additional derived data in state.
  UpdateDerivedOutputs(cstate,pstate_ptr);

  return 1;
}

OC_BOOL
Oxs_RungeKuttaEvolve::Step(const Oxs_TimeDriver* driver,
                      Oxs_ConstKey<Oxs_SimState> current_state_key,
                      const Oxs_DriverStepInfo& step_info,
                      Oxs_Key<Oxs_SimState>& next_state_key)
{
#if REPORT_TIME
  steponlytime.Start();
#endif // REPORT_TIME
  const OC_REAL8m bad_energy_cut_ratio = 0.75;
  const OC_REAL8m bad_energy_step_increase = 1.3;
  const OC_REAL8m previous_next_timestep = next_timestep;
  const Oxs_SimState& cstate = current_state_key.GetReadReference();

  CheckCache(cstate);

  // Note if start_dm or start_dt is being used
  OC_BOOL start_cond_active=0;
  if(next_timestep<=0.0 ||
     (cstate.stage_iteration_count<1
      && step_info.current_attempt_count==0)) {
    if(cstate.stage_number==0
       || stage_init_step_control == SISC_START_COND) {
      start_cond_active = 1;
    } else if(stage_init_step_control == SISC_CONTINUOUS) {
      start_cond_active = 0;  // Safety
    } else if(stage_init_step_control == SISC_AUTO) {
      // Automatic detection based on energy values across
      // stage boundary.
      OC_REAL8m total_E,E_diff;
      if(cstate.GetDerivedData(DataName("Total E"),total_E) &&
         cstate.GetDerivedData(DataName("Delta E"),E_diff)  &&
         fabs(E_diff) <= 256*OC_REAL8_EPSILON*fabs(total_E) ) {
        // The factor of 256 in the preceding line is a fudge factor,
        // selected with no particular justification.
        start_cond_active = 0;  // Continuous case
      } else {
        start_cond_active = 1;  // Assume discontinuous
      }
    } else {
      throw Oxs_ExtError(this,
           "Oxs_RungeKuttaEvolve::Step; Programming error:"
           " unrecognized stage_init_step_control value");
    }
  }

  // Negotiate timestep, and also initialize both next_state and
  // temp_state structures.
  Oxs_SimState* work_state = &(next_state_key.GetWriteReference());
  OC_BOOL force_step=0,driver_set_step=0;
  NegotiateTimeStep(driver,cstate,*work_state,next_timestep,
                    start_cond_active,force_step,driver_set_step);
  OC_REAL8m stepsize = work_state->last_timestep;
  work_state=NULL; // Safety: disable pointer

  // Step
  OC_REAL8m error_estimate,norm_error;
  OC_REAL8m global_error_order;
  OC_BOOL new_energy_and_dmdt_computed;
  OC_BOOL reject_step=0;
  (this->*rkstep_ptr)(stepsize,current_state_key,
                      dm_dt_output.cache.value,
                      next_state_key,
                      error_estimate,global_error_order,norm_error,
                      new_energy_and_dmdt_computed);
  const Oxs_SimState& nstate = next_state_key.GetReadReference();
  driver->FillStateDerivedData(cstate,nstate);

  OC_REAL8m max_dm_dt;
  cstate.GetDerivedData(DataName("Max dm/dt"),max_dm_dt);
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
    // adjustment estimate, which we only need to try if
    // next_timestep is larger than cut applied by energy
    // rejection code (i.e., bad_energy_cut_ratio).
    if(next_timestep<=stepsize*bad_energy_cut_ratio) {
      AdjustStepHeadroom(1);
#if REPORT_TIME
      steponlytime.Stop();
#endif // REPORT_TIME
      return 0; // Don't bother with energy calculation
    }
    reject_step=1; // Otherwise, mark step rejected and see what energy
    /// info suggests for next stepsize
  }

  if(start_cond_active && !force_step) {
    if(start_dm>=0.0) {
      // Check that no spin has moved by more than start_dm
      OC_REAL8m diff = MaxDiff(cstate.spin,nstate.spin);
      if(diff>start_dm) {
        next_timestep = step_headroom * stepsize * (start_dm/diff);
        if(next_timestep<=stepsize*bad_energy_cut_ratio) {
          AdjustStepHeadroom(1);
#if REPORT_TIME
          steponlytime.Stop();
#endif // REPORT_TIME
          return 0; // Don't bother with energy calculation
        }
        reject_step=1; // Otherwise, mark step rejected and see what energy
        /// info suggests for next stepsize
      }
    }
  }

  // Energy timestep control:
  //   The relationship between energy error and stepsize appears to be
  // highly nonlinear, so that estimating appropriate stepsize from energy
  // increase is difficult.  Perhaps it is possible to include energy
  // interpolation into RK step routines, but for the present we just
  // reduce the step by a fixed ratio if we detect energy increase beyond
  // that which can be attributed to numerical errors.  Of course, this
  // doesn't take into account the expected energy decrease (which depends
  // on the damping ratio alpha), which is another reason to try to build
  // it directly into the high order RK step routines.
  OC_REAL8m pE_pt,new_pE_pt=0.;
  cstate.GetDerivedData(DataName("pE/pt"),pE_pt);
  if(new_energy_and_dmdt_computed) {
    nstate.GetDerivedData(DataName("pE/pt"),new_pE_pt);
  } else {
    OC_REAL8m new_total_E;
    GetEnergyDensity(nstate,temp_energy,
                     &mxH_output.cache.value,
                     NULL,new_pE_pt,new_total_E);
    mxH_output.cache.state_id=nstate.Id();
    if(!nstate.AddDerivedData(DataName("pE/pt"),new_pE_pt)) {
      throw Oxs_ExtError(this,
           "Oxs_RungeKuttaEvolve::Step:"
           " Programming error; data cache (pE/pt) already set.");
    }
    if(!nstate.AddDerivedData(DataName("Total E"),new_total_E)) {
      throw Oxs_ExtError(this,
           "Oxs_RungeKuttaEvolve::Step:"
           " Programming error; data cache (Total E) already set.");
    }
  }

#if REPORT_TIME_RKDEVEL
timer[0].Start();
#endif // REPORT_TIME_RKDEVEL
  OC_REAL8m dE,var_dE,total_E;
  ComputeEnergyChange(nstate.mesh,energy,temp_energy,dE,var_dE,total_E);
  if(!nstate.AddDerivedData(DataName("Delta E"),dE)) {
    throw Oxs_ExtError(this,
         "Oxs_RungeKuttaEvolve::Step:"
         " Programming error; data cache (Delta E) already set.");
  }
#if REPORT_TIME_RKDEVEL
timer[0].Stop();
++timer_counts[0].pass_count;
 timer_counts[0].bytes += (nstate.mesh->Size())*(2*sizeof(OC_REAL8m));
 timer_counts[0].name = "ComputeEnergyChange";
#endif // REPORT_TIME_RKDEVEL

  if(expected_energy_precision>=0.) {
    var_dE *= expected_energy_precision * expected_energy_precision;
    /// Variance, assuming error in each energy[i] term is independent,
    /// uniformly distributed, 0-mean, with range
    ///        +/- expected_energy_precision*energy[i].
    /// It would probably be better to get an error estimate directly
    /// from each energy term.
    OC_REAL8m E_numerror = OC_MAX(fabs(total_E)*expected_energy_precision,
                               2*sqrt(var_dE));
    OC_REAL8m pE_pt_max = OC_MAX(pE_pt,new_pE_pt); // Might want to
    /// change this from a constant to a linear function in timestep.

    OC_REAL8m reject_dE = 2*E_numerror + pE_pt_max * stepsize;
    if(dE>reject_dE) {
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
#if REPORT_TIME
    steponlytime.Stop();
#endif // REPORT_TIME
    return 0;
  }

  // Otherwise, we are accepting the new step.

  // Calculate dm_dt at new point, and fill in cache.
  // This branch is not active when using the RKF54 kernel, but is
  // when using the RK4 kernel.
  if(new_energy_and_dmdt_computed) {
    dm_dt_output.cache.value.Swap(vtmpA);
  } else {
    OC_REAL8m new_max_dm_dt,new_dE_dt,new_timestep_lower_bound;
    if(mxH_output.cache.state_id != nstate.Id()) { // Safety
    throw Oxs_ExtError(this,
         "Oxs_RungeKuttaEvolve::Step:"
         " Programming error; mxH_output cache improperly filled.");
    }
    (this->*calculate_dm_dt_ptr)(nstate,
                                 mxH_output.cache.value,new_pE_pt,
                                 dm_dt_output.cache.value,new_max_dm_dt,
                                 new_dE_dt,new_timestep_lower_bound);
    if(!nstate.AddDerivedData(DataName("Timestep lower bound"),
                              new_timestep_lower_bound) ||
       !nstate.AddDerivedData(DataName("Max dm/dt"),new_max_dm_dt) ||
       !nstate.AddDerivedData(DataName("dE/dt"),new_dE_dt)) {
      throw Oxs_ExtError(this,
                           "Oxs_RungeKuttaEvolve::Step:"
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

#if REPORT_TIME
  steponlytime.Stop();
#endif // REPORT_TIME
  return 1; // Accept step
}

void
Oxs_RungeKuttaEvolve::UpdateDerivedOutputs(const Oxs_SimState& state,
					   const Oxs_SimState* prevstate_ptr)
{ // This routine fills all the Oxs_RungeKuttaEvolve Oxs_ScalarOutput's to
  // the appropriate value based on the import "state", and any of
  // Oxs_VectorOutput's that have CacheRequest enabled are filled.
  // It also makes sure all the expected WOO objects in state are
  // filled.

  max_dm_dt_output.cache.state_id
    = dE_dt_output.cache.state_id
    = delta_E_output.cache.state_id
    = 0;  // Mark change in progress

  OC_REAL8m dummy_value;
  if(!state.GetDerivedData(DataName("Max dm/dt"),
                           max_dm_dt_output.cache.value) ||
     !state.GetDerivedData(DataName("dE/dt"),
                           dE_dt_output.cache.value) ||
     !state.GetDerivedData(DataName("Delta E"),
                           delta_E_output.cache.value) ||
     !state.GetDerivedData(DataName("pE/pt"),dummy_value) ||
     !state.GetDerivedData(DataName("Total E"),dummy_value) ||
     !state.GetDerivedData(DataName("Timestep lower bound"),
                           dummy_value) ||
     (dm_dt_output.GetCacheRequestCount()>0
      && dm_dt_output.cache.state_id != state.Id()) ||
     (mxH_output.GetCacheRequestCount()>0
      && mxH_output.cache.state_id != state.Id())) {

    // Missing at least some data, so calculate from scratch

    // Check ahead for trouble computing Delta E:
    if(!state.GetDerivedData(DataName("Delta E"),dummy_value)
       && state.previous_state_id != 0
       && prevstate_ptr!=NULL
       && state.previous_state_id == prevstate_ptr->Id()) {
      OC_REAL8m old_E;
      if(!prevstate_ptr->GetDerivedData(DataName("Total E"),old_E)) {
	// Previous state doesn't have stored Total E.  Compute it
	// now.
	OC_REAL8m old_pE_pt;
	GetEnergyDensity(*prevstate_ptr,energy,NULL,NULL,old_pE_pt,old_E);
	prevstate_ptr->AddDerivedData(DataName("Total E"),old_E);
      }
    }

    // Calculate H and mxH outputs
    Oxs_MeshValue<ThreeVector>& mxH = mxH_output.cache.value;
    OC_REAL8m pE_pt,total_E;
    GetEnergyDensity(state,energy,&mxH,NULL,pE_pt,total_E);
    energy_state_id=state.Id();
    mxH_output.cache.state_id=state.Id();
    if(!state.GetDerivedData(DataName("pE/pt"),dummy_value)) {
      state.AddDerivedData(DataName("pE/pt"),pE_pt);
    }
    if(!state.GetDerivedData(DataName("Total E"),dummy_value)) {
      state.AddDerivedData(DataName("Total E"),total_E);
    }

    // Calculate dm/dt, Max dm/dt and dE/dt
    Oxs_MeshValue<ThreeVector>& dm_dt
      = dm_dt_output.cache.value;
    dm_dt_output.cache.state_id=0;
    OC_REAL8m timestep_lower_bound;
    (this->*calculate_dm_dt_ptr)(state,mxH,pE_pt,dm_dt,
                                 max_dm_dt_output.cache.value,
                                 dE_dt_output.cache.value,
                                 timestep_lower_bound);
    dm_dt_output.cache.state_id=state.Id();

    if(!state.GetDerivedData(DataName("Max dm/dt"),dummy_value)) {
      state.AddDerivedData(DataName("Max dm/dt"),
                           max_dm_dt_output.cache.value);
    }

    if(!state.GetDerivedData(DataName("dE/dt"),dummy_value)) {
      state.AddDerivedData(DataName("dE/dt"),dE_dt_output.cache.value);
    }

    if(!state.GetDerivedData(DataName("Timestep lower bound"),dummy_value)) {
      state.AddDerivedData(DataName("Timestep lower bound"),
                           timestep_lower_bound);
    }

    if(!state.GetDerivedData(DataName("Delta E"),dummy_value)) {
      if(state.previous_state_id == 0) {
        // No previous state
        dummy_value = 0.0;
      } else if(prevstate_ptr!=NULL
                && state.previous_state_id == prevstate_ptr->Id()) {
        OC_REAL8m old_E;
        if(!prevstate_ptr->GetDerivedData(DataName("Total E"),old_E)) {
          throw Oxs_ExtError(this,
                             "Oxs_RungeKuttaEvolve::UpdateDerivedOutputs:"
                             " \"Total E\" not set in previous state.");
        }
        dummy_value = total_E - old_E; // This is less accurate than adding
        /// up the cell-by-cell differences (as is used in the main code),
        /// but w/o the entire energy map for prevstate this is the best
        /// we can do.
      } else {
        throw Oxs_ExtError(this,
           "Oxs_RungeKuttaEvolve::UpdateDerivedOutputs:"
           " Can't derive Delta E from single state.");
      }
      state.AddDerivedData(DataName("Delta E"),dummy_value);
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
