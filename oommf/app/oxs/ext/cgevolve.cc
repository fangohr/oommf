/* FILE: cgevolve.cc                 -*-Mode: c++-*-
 *
 * Concrete minimization evolver class, using conjugate gradients
 *
 */

#include <cassert>
#include <cctype>
#include <cfloat>

#include <string>

#include "nb.h"
#include "director.h"
#include "mindriver.h"
#include "simstate.h"
#include "cgevolve.h"
#include "key.h"
#include "threevector.h"
#include "oxswarn.h"

#if OOMMF_THREADS
# include <vector>
# include "oxsthread.h"
#endif // OOMMF_THREADS

#if OC_USE_SSE
# include <emmintrin.h>
#endif

#if OC_USE_NUMA
# include <numaif.h>
#endif

// Read <algorithm> last, because with some pgc++ installs the
// <emmintrin.h> header is not interpreted properly if <algorithm> is
// read first.
#include <algorithm>

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_CGEvolve);

/* End includes */

// #define TS(x) fprintf(stderr,OC_STRINGIFY(x)"\n"); x
#define TS(x) x

// Wrapper around tolower, to make it the right type for use in
// std::transform with strings.  Most compilers figure this out, but
// Borland C++ 5.5.1 needs this help.
static char convert_char_to_lowercase(char ch) { return (char)tolower(ch); }

// Revision information, set via CVS keyword substitution
static const Oxs_WarningMessageRevisionInfo revision_info
  (__FILE__,
   "$Revision: 1.164 $",
   "$Date: 2015/09/30 07:29:29 $",
   "$Author: donahue $",
   "Michael J. Donahue (michael.donahue@nist.gov)");

// Constructor
Oxs_CGEvolve::Oxs_CGEvolve(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_MinEvolver(name,newdtr,argstr),
    energy_error_adj(1.0),
    step_attempt_count(0),
    energy_calc_count(0),
    cycle_count(0),
    cycle_sub_count(0),
    bracket_count(0),
    line_minimum_count(0),
    conjugate_cycle_count(0),
    gradient_reset_score(0.0),
    gradient_reset_wgt(31./32.),
    gradient_reset_angle_cotangent(0.0),
    gradient_reset_trigger(0.5),
    kludge_adjust_angle_cos(0.0),
    preconditioner_type(Oxs_EnergyPreconditionerSupport::NONE),
    preconditioner_weight(0.5),
    preconditioner_mesh_id(0),
    sum_error_estimate(0)
{
  // Check code assumptions, if any
#if OC_USE_SSE
  // Some of the SSE code assumes that Oxs_ThreeVector arrays
  // are tightly packed, with a particular order on the components.
  {
    ThreeVector foo[10];
    if((char*)(foo+9) - (char*)(foo+0) != 9*3*sizeof(OC_REAL8m)) {
      throw Oxs_ExtError(this,"ThreeVectors are not tightly packed;"
                         " this will break some of the code using"
                         " SSE intrinsics.  Edit the OOMMF platform"
                         " file to disable SSE intrinsics (see config"
                         " sse_level option) and rebuild.");
    }
    if((void*)(&(foo[0].x)) != (void*)(foo+0) ||
       &(foo[0].x) + 1 != &(foo[0].y) ||
       &(foo[0].x) + 2 != &(foo[0].z)) {
      throw Oxs_ExtError(this,"ThreeVector packing order is not"
                         " x:y:z.  This will break some of the SSE"
                         " intrinsics code.  Edit the OOMMF platform"
                         " file to disable SSE intrinsics (see config"
                         " sse_level option) and rebuild.");
    }
  }
#endif // OC_USE_SSE

  // Process arguments

  // gradient_reset_angle is input in degrees, but then converted to
  // cotangent of that value, for ease of use inside ::SetBasePoint().
  gradient_reset_angle_cotangent
    = GetRealInitValue("gradient_reset_angle",87.5);
  gradient_reset_angle_cotangent
    = tan(fabs(90.0-gradient_reset_angle_cotangent)*PI/180.);

  gradient_reset_count = GetUIntInitValue("gradient_reset_count",5000);

  // kludge_adjust_angle is input in degrees, but then converted to cos
  // of that value, for ease of use inside ::SetBasePoint().
  kludge_adjust_angle_cos = GetRealInitValue("kludge_adjust_angle",89.2);
  kludge_adjust_angle_cos = cos(kludge_adjust_angle_cos*PI/180.);

  OC_REAL8m min_ang = GetRealInitValue("minimum_bracket_step",0.05);
  min_ang *= PI/180.; // Convert from degrees to radians
  bracket.minstep = tan(min_ang);

  OC_REAL8m max_ang = GetRealInitValue("maximum_bracket_step",10);
  max_ang *= PI/180.; // Convert from degrees to radians
  bracket.maxstep = tan(max_ang);

  if(bracket.minstep<0.0 || bracket.minstep>bracket.maxstep) {
    throw Oxs_ExtError(this,"Invalid value for minimum_bracket_step"
			 " and/or maximum_bracket_step.");
  }

  bracket.angle_precision
    = GetRealInitValue("line_minimum_angle_precision",1);
  // Convert from degrees to sin(angle).  We want sin(angle)
  // instead of cos(angle) because we are interested in differences
  // from 90 degrees (use angle-sum formula on sin(90-acos(dot))).
  bracket.angle_precision = sin(bracket.angle_precision*PI/180.);

  bracket.relative_minspan
    = GetRealInitValue("line_minimum_relwidth",1);

  energy_error_adj = GetRealInitValue("energy_precision",1e-14)/1e-14;

  String method_name = GetStringInitValue("method","Fletcher-Reeves");
  if(method_name.compare("Fletcher-Reeves")==0) {
    basept.method = Basept_Data::FLETCHER_REEVES;
  } else if(method_name.compare("Polak-Ribiere")==0) {
    basept.method = Basept_Data::POLAK_RIBIERE;
  } else {
    String msg=String("Invalid conjugate-gradient method request: ")
      + method_name
      + String("\n Should be either Fletcher-Reeves or Polak-Ribiere.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  String preconditioner_type_name
    = GetStringInitValue("preconditioner","none");
  std::transform(preconditioner_type_name.begin(),
                 preconditioner_type_name.end(),
                 preconditioner_type_name.begin(),
                 convert_char_to_lowercase); // make lowercase
  if(preconditioner_type_name.compare("none")==0) {
    preconditioner_type = Oxs_EnergyPreconditionerSupport::NONE;
  } else if(preconditioner_type_name.compare("msv")==0) {
    preconditioner_type = Oxs_EnergyPreconditionerSupport::MSV;
  } else if(preconditioner_type_name.compare("diagonal")==0) {
    preconditioner_type = Oxs_EnergyPreconditionerSupport::DIAGONAL;
  } else {
    String msg=String("Invalid preconditioner type request: ")
      + preconditioner_type_name
      + String("\n Should be either none or diagonal.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  preconditioner_weight = GetRealInitValue("preconditioner_weight",0.5);
  // 1 ==> full preconditioner, 0 ==> no preconditioning (equivalent to
  // preconditioner_type == NONE).
  if(preconditioner_weight<0.0 || 1.0<preconditioner_weight) {
    throw Oxs_ExtError(this,"Invalid preconditioner_weight;"
                       " should be in range [0,1]");
  }

  // Setup output.  Note: MSVC++ 6.0 requires fully qualified
  // member function names.
  total_H_field_output.Setup(this,InstanceName(),"H","A/m",1,
              &Oxs_CGEvolve::UpdateDerivedFieldOutputs);
  mxHxm_output.Setup(this,InstanceName(),"mxHxm","A/m",1,
	      &Oxs_CGEvolve::UpdateDerivedFieldOutputs);
  total_energy_density_output.Setup(this,InstanceName(),
              "Total energy density","J/m^3",1,
              &Oxs_CGEvolve::UpdateDerivedFieldOutputs);
  max_mxHxm_output.Setup(this,InstanceName(),"Max mxHxm","A/m",0,
	      &Oxs_CGEvolve::UpdateDerivedOutputs);
  total_energy_output.Setup(this,InstanceName(),"Total energy","J",0,
              &Oxs_CGEvolve::UpdateDerivedOutputs);
  delta_E_output.Setup(this,InstanceName(),"Delta E","J",0,
              &Oxs_CGEvolve::UpdateDerivedOutputs);
  bracket_count_output.Setup(this,InstanceName(),"Bracket count","",0,
              &Oxs_CGEvolve::UpdateDerivedOutputs);
  line_min_count_output.Setup(this,InstanceName(),"Line min count","",0,
              &Oxs_CGEvolve::UpdateDerivedOutputs);
  conjugate_cycle_count_output.Setup(this,InstanceName(),
              "Conjugate cycle count","",0,
              &Oxs_CGEvolve::UpdateDerivedOutputs);
  cycle_count_output.Setup(this,InstanceName(),
              "Cycle count","",0,
              &Oxs_CGEvolve::UpdateDerivedOutputs);
  cycle_sub_count_output.Setup(this,InstanceName(),
              "Cycle sub count","",0,
              &Oxs_CGEvolve::UpdateDerivedOutputs);
  energy_calc_count_output.Setup(this,InstanceName(),
              "Energy calc count","",0,
              &Oxs_CGEvolve::UpdateDerivedOutputs);

  total_H_field_output.Register(director,-5);
  mxHxm_output.Register(director,-5);
  total_energy_density_output.Register(director,-5);
  max_mxHxm_output.Register(director,-5);
  total_energy_output.Register(director,-5);
  delta_E_output.Register(director,-5);
  bracket_count_output.Register(director,-5);
  line_min_count_output.Register(director,-5);
  conjugate_cycle_count_output.Register(director,-5);
  cycle_count_output.Register(director,-5);
  cycle_sub_count_output.Register(director,-5);
  energy_calc_count_output.Register(director,-5);

  VerifyAllInitArgsUsed();

  // Each bracket holds a key to an Oxs_SimState.  There are three
  // brackets (bracket.left, bracket.right, and extra_bracket), so
  // reserve space for three Oxs_SimState's.  (One of these keys
  // probably points to space reserved by the Driver, so it probably
  // suffices to reserve two slots; OTOH there doesn't appear to be
  // any particular downside to reserving an unused slot.)
  director->ReserveSimulationStateRequest(3);

#if REPORT_TIME_CGDEVEL
  timer.resize(10);
  timer_counts.resize(10);
#endif
}

OC_BOOL Oxs_CGEvolve::Init()
{
#if REPORT_TIME
  Oc_TimeVal cpu,wall;

#if REPORT_TIME_CGDEVEL
  energyobjtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"   Step energy  .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
#endif // REPORT_TIME_CGDEVEL

  steponlytime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"   Step-only    .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

#if REPORT_TIME_CGDEVEL
  basepttime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      Base point   .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  findbrackettime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      Find bracket .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  findlinemintime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      Find line min ....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  fillbrackettime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"         Fill bracket  ....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  getenergyandmxHxmtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"         Energy + mxHxm ...   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  for(unsigned int ti=0;ti<timer.size();++ti) {
    timer[ti].GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"               timer %2u ...   %7.2f cpu /%7.2f wall,"
              " (%.1000s)\n",
              ti,double(cpu),double(wall),InstanceName());
      if(timer_counts[ti].pass_count>0) {
        if(timer_counts[ti].throw_away_count>0 &&
           timer_counts[ti].pass_count > timer_counts[ti].throw_away_count) {
          OC_INT4m dropped = timer_counts[ti].throw_away_count;
          fprintf(stderr,"                \\---> passes =%4d (-%d),"
                  " bytes =%9.2f MB, bytes/pass =%7.2f MB,%6.2f GB/sec\n",
                  timer_counts[ti].pass_count,dropped,
                  double(timer_counts[ti].bytes)/double(1000*1000),
                  double(timer_counts[ti].bytes)/double(1000*1000)
                  /double(timer_counts[ti].pass_count - dropped),
                  double(timer_counts[ti].bytes)
                  /(double(1000*1000*1000)*double(wall)));
        } else {
          fprintf(stderr,"                \\---> passes =%4d,"
                  " bytes =%9.2f MB, bytes/pass =%7.2f MB,%6.2f GB/sec\n",
                  timer_counts[ti].pass_count,
                  double(timer_counts[ti].bytes)/double(1000*1000),
                  double(timer_counts[ti].bytes)/double(1000*1000)
                  /double(timer_counts[ti].pass_count),
                  double(timer_counts[ti].bytes)
                  /(double(1000*1000*1000)*double(wall)));
        }
      }
    }
    timer[ti].Reset();
    timer_counts[ti].Reset();
  }
#endif // REPORT_TIME_CGDEVEL


  steponlytime.Reset();
#if REPORT_TIME_CGDEVEL
  energyobjtime.Reset();
  basepttime.Reset();
  findbrackettime.Reset();
  findlinemintime.Reset();
  fillbrackettime.Reset();
  getenergyandmxHxmtime.Reset();
#endif // REPORT_TIME_CGDEVEL

#endif // REPORT_TIME

  // Initialize instance variables
  step_attempt_count=0;
  energy_calc_count=0;
  cycle_count=0;
  cycle_sub_count=0;
  bracket_count=0;
  line_minimum_count=0;
  conjugate_cycle_count=0;
  gradient_reset_score=0.0;
  basept.Init();
  bestpt.Init();
  bracket.Init();

  preconditioner_mesh_id = 0; // Invalidate
  preconditioner_Ms2_V2.Release();
  preconditioner_Ms_V.Release();
  Ms_V.Release();

  scratch_energy.Release();
  scratch_field.Release();

  return Oxs_MinEvolver::Init();
}


Oxs_CGEvolve::~Oxs_CGEvolve()
{
#if REPORT_TIME
  Oc_TimeVal cpu,wall;

#if REPORT_TIME_CGDEVEL
  energyobjtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"   Step energy  .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
#endif // REPORT_TIME_CGDEVEL

  steponlytime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"   Step-only    .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

#if REPORT_TIME_CGDEVEL
  basepttime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      Base point   .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  findbrackettime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      Find bracket .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  findlinemintime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"      Find line min ....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  fillbrackettime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"         Fill bracket  ....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  getenergyandmxHxmtime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"         Energy + mxHxm ...   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }

  for(unsigned int ti=0;ti<timer.size();++ti) {
    timer[ti].GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"               timer %2u ...   %7.2f cpu /%7.2f wall,"
              " (%.1000s)\n",
              ti,double(cpu),double(wall),InstanceName());
      if(timer_counts[ti].pass_count>0) {
        if(timer_counts[ti].throw_away_count>0 &&
           timer_counts[ti].pass_count > timer_counts[ti].throw_away_count) {
          OC_INT4m dropped = timer_counts[ti].throw_away_count;
          fprintf(stderr,"                \\---> passes =%4d (-%d),"
                  " bytes =%9.2f MB, bytes/pass =%7.2f MB,%6.2f GB/sec\n",
                  timer_counts[ti].pass_count,dropped,
                  double(timer_counts[ti].bytes)/double(1000*1000),
                  double(timer_counts[ti].bytes)/double(1000*1000)
                  /double(timer_counts[ti].pass_count - dropped),
                  double(timer_counts[ti].bytes)
                  /(double(1000*1000*1000)*double(wall)));
        } else {
          fprintf(stderr,"                \\---> passes =%4d,"
                  " bytes =%9.2f MB, bytes/pass =%7.2f MB,%6.2f GB/sec\n",
                  timer_counts[ti].pass_count,
                  double(timer_counts[ti].bytes)/double(1000*1000),
                  double(timer_counts[ti].bytes)/double(1000*1000)
                  /double(timer_counts[ti].pass_count),
                  double(timer_counts[ti].bytes)
                  /(double(1000*1000*1000)*double(wall)));
        }
      }
    }
  }
#endif // REPORT_TIME_CGDEVEL

#endif // REPORT_TIME
}

void Oxs_CGEvolve::InitializePreconditioner(const Oxs_SimState* state)
{ // Note: At present, InitializePreconditioner is only called when a
  // change in the state->mesh is detected, which therefore assumes that
  // cell Ms and volume don't change without a change in the mesh.  If
  // this assumption becomes untrue, then modifications will be
  // required.

  // For code details, see NOTES VI, 18-28 July 2011, pp 6-15.

  preconditioner_mesh_id = 0; // Work in progress

  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state->Ms);
  const Oxs_Mesh* mesh = state->mesh;
  const OC_INDEX size = mesh->Size();
  preconditioner_Ms_V.AdjustSize(mesh);
  preconditioner_Ms2_V2.AdjustSize(mesh);
  Ms_V.AdjustSize(mesh);

  int init_ok = 0;

  switch(preconditioner_type) {
  case Oxs_EnergyPreconditionerSupport::NONE:
    // No preconditioning
    break;

  case Oxs_EnergyPreconditionerSupport::MSV:
    // Use 1/(Ms[i]*Volume(i)) as preconditioner
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (Ms,
       [&](OC_INT4m,OC_INDEX istart,OC_INDEX istop) {
        for(OC_INDEX i=istart;i<istop;++i) {
          OC_REAL8m val = ( Ms[i] == 0.0 ? 0.0 : 1.0 );
          preconditioner_Ms_V[i].Set(val,val,val);
        }
      });
    init_ok = 1;
    break;

  case Oxs_EnergyPreconditionerSupport::DIAGONAL:
    // Use A_ii as preconditioner.  This requires explicit
    // support from energy modules.
    preconditioner_Ms_V = ThreeVector(0.,0.,0.);
    /// Values for each energy term are added on.
    Oxs_EnergyPreconditionerSupport::PreconditionerData pcd;
    pcd.type = preconditioner_type;
    pcd.state = state;
    pcd.val = &preconditioner_Ms_V;
    OC_INT4m count = 0;
    vector<Oxs_Energy*> energy = director->GetEnergyObjects();
    vector<Oxs_Energy*>::iterator ei = energy.begin();
    while(ei != energy.end()) {
      Oxs_EnergyPreconditionerSupport* eps
        = dynamic_cast<Oxs_EnergyPreconditionerSupport*>(*ei);
      if(eps != NULL && eps->IncrementPreconditioner(pcd) != 0) {
        ++count;
      } else {
        String sbuf = String("Energy term \"")
          + String((*ei)->InstanceName())
          + String("\" does not support diagonal preconditioning.");
        static Oxs_WarningMessage foo(-1);
        foo.Send(revision_info,OC_STRINGIFY(__LINE__),sbuf.c_str());
      }
      ++ei;
    }
    if(count!=0) init_ok = 1;
    break;
  }

  // Find maximum preconditioner value, for scaling.  Might want to
  // change this to an average value.
  OC_REAL8m maxval = 0.0;
  OC_REAL8m maxvalMsV = 0.0;
  if(init_ok) {
    const int number_of_threads = Oc_GetMaxThreadCount();
    std::vector<OC_REAL8m> thread_maxval(number_of_threads,maxval);
    std::vector<OC_REAL8m> thread_maxvalMsV(number_of_threads,maxvalMsV);
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (Ms,
       [&](OC_INT4m thread_id,OC_INDEX istart,OC_INDEX istop) {
        OC_REAL8m thd_maxval = thread_maxval[thread_id]; // Thread local copy
        OC_REAL8m thd_maxvalMsV = thread_maxvalMsV[thread_id]; // Ditto
        for(OC_INDEX i=istart;i<istop;++i) {
          ThreeVector v = preconditioner_Ms_V[i];
          if(v.x<0 || v.y<0 || v.z<0) {
            throw Oxs_ExtError(this,
                    "Invalid preconditioner: negative value detected.");
          }
          OC_REAL8m tmp = v.x;
          if(v.y>tmp) tmp = v.y;
          if(v.z>tmp) tmp = v.z;
          if(tmp>thd_maxval) thd_maxval = tmp;
          tmp *= Ms[i]*mesh->Volume(i);
          if(tmp>thd_maxvalMsV) thd_maxvalMsV = tmp;
        }
        thread_maxval[thread_id]    = thd_maxval;
        thread_maxvalMsV[thread_id] = thd_maxvalMsV;
      });
    for(int i=1;i<number_of_threads;++i) {
      if(thread_maxval[0]<thread_maxval[i]) {
        thread_maxval[0] = thread_maxval[i];
      }
      if(thread_maxvalMsV[0]<thread_maxvalMsV[i]) {
        thread_maxvalMsV[0] = thread_maxvalMsV[i];
      }
    }
    maxval = thread_maxval[0];
    maxvalMsV = thread_maxvalMsV[0];
    if(maxval == 0.0) init_ok = 0;  // Preconditioner matrix all zeroes.
  }

# ifndef NDEBUG
  OC_REAL8m minval = OC_REAL8m_MAX;
  if(init_ok) {
    for(OC_INDEX i=0;i<size;++i) {
      ThreeVector v = preconditioner_Ms_V[i];
      if(v.x<0 || v.y<0 || v.z<0) {
        throw Oxs_ExtError(this,
              "Invalid preconditioner: negative value detected.");
      }
      if(0<v.x && v.x<minval) minval = v.x;
      if(0<v.y && v.y<minval) minval = v.y;
      if(0<v.z && v.z<minval) minval = v.z;
    }
    fprintf(stderr,
            "Min/Max pre-preconditioner values: %"
            OC_REAL8m_MOD "g / %" OC_REAL8m_MOD "g\n",
            minval,maxval);
  }
# endif

  if(init_ok) {
    // preconditioner_Ms_V initialized above
    // If pw<>1, implement "blended" preconditioner
    const OC_REAL8m pw = preconditioner_weight;
    const OC_REAL8m cpw = 1 - pw;
    // OC_REAL8m err_mxHxm_sumsq = 0.0;  // (Not used?)
    // OC_REAL8m err_dir_sumsq = 0.0;    // (Not used?)
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (Ms,
       [&](OC_INT4m,OC_INDEX istart,OC_INDEX istop) {
        for(OC_INDEX i=istart;i<istop;++i) {
          OC_REAL8m scale = Ms[i]*mesh->Volume(i);
          // OC_REAL8m errtmp = Ms[i]*scale;
          // err_mxHxm_sumsq += errtmp*errtmp;
          Ms_V[i] = scale;
          if(scale<1 && maxvalMsV>OC_REAL8m_MAX*scale) {
            // Assumes that m[i] is ignored if Ms[i] is zero.
            preconditioner_Ms_V[i].x
              = preconditioner_Ms_V[i].y = preconditioner_Ms_V[i].z
              = preconditioner_Ms2_V2[i].x
              = preconditioner_Ms2_V2[i].y = preconditioner_Ms2_V2[i].z = 0.0;
          } else {
            OC_REAL8m c0 = maxvalMsV*cpw/scale;
            OC_REAL8m cx =  c0 + pw*preconditioner_Ms_V[i].x;
            if(cx>=1.0 || maxval<OC_REAL8m_MAX*cx) cx = maxval/cx;
            else cx = 1.0; // Safety
            OC_REAL8m cy =  c0 + pw*preconditioner_Ms_V[i].y;
            if(cy>=1.0 || maxval<OC_REAL8m_MAX*cy) cy = maxval/cy;
            else cy = 1.0; // Safety
            OC_REAL8m cz =  c0 + pw*preconditioner_Ms_V[i].z;
            if(cz>=1.0 || maxval<OC_REAL8m_MAX*cz) cz = maxval/cz;
            else cz = 1.0; // Safety
            preconditioner_Ms_V[i].Set(cx,cy,cz);
            preconditioner_Ms2_V2[i].Set(scale*cx,scale*cy,scale*cz);
            // OC_REAL8m errdirtmp = Ms[i]*(cx+cy+cz); // Divide by 3 => average c.
            // err_dir_sumsq += errdirtmp*errdirtmp;
          }
        }
      });
  } else {
    // Initialization failed; take instead C^-2 = 1 (no preconditioning)
    // OC_REAL8m err_mxHxm_sumsq = 0.0;  // (Not used?)
    Oxs_RunThreaded<OC_REAL8m,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (Ms,
       [&](OC_INT4m,OC_INDEX istart,OC_INDEX istop) {
        for(OC_INDEX i=istart;i<istop;++i) {
          const OC_REAL8m scale = Ms[i]*mesh->Volume(i);
          // OC_REAL8m errtmp = Ms[i]*scale;
          // err_mxHxm_sumsq += errtmp*errtmp;
          Ms_V[i] = scale;
          preconditioner_Ms_V[i].Set(scale,scale,scale);
          const OC_REAL8m scalesq = scale*scale;
          preconditioner_Ms2_V2[i].Set(scalesq,scalesq,scalesq);
        }
      });
  }
  sum_error_estimate = OC_REAL8m_EPSILON*sqrt(static_cast<OC_REAL8m>(size));

  preconditioner_mesh_id = mesh->Id();
}


#if OOMMF_THREADS
class _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadA : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<ThreeVector> job_basket;

  const Oxs_MeshValue<ThreeVector>* direction;
  const Oxs_MeshValue<ThreeVector>* best_spin;
  Oxs_MeshValue<ThreeVector>* spin;

  OC_REAL8m tsq;
  OC_REAL8m dvec_scale;

  _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadA()
    : direction(0), best_spin(0), spin(0),
      tsq(0.),dvec_scale(0.) {}

  static void Init(int thread_count,
                   const Oxs_StripedArray<ThreeVector>* arrblock) {
    job_basket.Init(thread_count,arrblock);
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<ThreeVector> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadA::job_basket;

void _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadA::Cmd(int threadnumber,
                                                  void* /* data */)
{
  const ThreeVector *scratch_direction;
  const ThreeVector *scratch_best_spin;
  ThreeVector *scratch_spin;

  while(1) {
    OC_INDEX istart,istop;
    job_basket.GetJob(threadnumber,istart,istop);
    if(istart>=istop) break; // No more jobs

    scratch_direction = &((*direction)[istart]);
    scratch_best_spin = &((*best_spin)[istart]);
    scratch_spin      = &((*spin)[istart]);
    const OC_INDEX jsize = istop - istart;

    // Note: The following alignment jig works identically for all of
    // the arrays because all have the same length modulo the ALIGNMENT
    // value.
    const OC_INDEX ALIGNMENT = 16;  // In bytes
    const OC_INDEX jbreak = (OC_UINDEX(scratch_direction)%ALIGNMENT)/8;
    const OC_INDEX STRIDE = 2;

#if OC_USE_SSE
    // Each of direction, best_spin, and spin are Oxs_MeshValue objects,
    // therefore the start of each of the component arrays should start
    // on a page boundary.  Each array is an array of ThreeVectors, so
    // if one is aligned to a 16-byte boundary at offset j, then the
    // others should be as well.  This allows for SSE aligned loads and
    // stores for all three.
    assert(size_t(scratch_direction + jbreak)%ALIGNMENT == 0);
    assert(size_t(scratch_best_spin + jbreak)%ALIGNMENT == 0);
    assert(size_t(scratch_spin + jbreak)%ALIGNMENT == 0);
#endif

    OC_INDEX j;
    const OC_REAL8m local_dvec_scale = dvec_scale;
    const Oc_Duet local_dvec_scale_duet(local_dvec_scale);
    for(j=0;j<jbreak;++j) {
#if 0
      const ThreeVector& dvec = scratch_direction[j];
      OC_REAL8m dsq = dvec.MagSq();
      ThreeVector temp = scratch_best_spin[j];
      temp *= sqrt(1+tsq*dsq);
      temp.Accum(dvec_scale,dvec);
      temp.MakeUnit();
      Nb_NOP(&temp);          // Safety; icpc optimizer showed problems
      scratch_spin[j] = temp; // with this stanza in some circumstances.
#else
      // Use same computation code as in main block, so rounding
      // errors on job edges match those in interior:
      Oc_Duet dvx(scratch_direction[j].x); // Fills both upper and lower
      Oc_Duet dvy(scratch_direction[j].y); // parts with same value.
      Oc_Duet dvz(scratch_direction[j].z);
      Oc_Duet mult = sqrt(Oc_Duet(1)
                             +Oc_Duet(tsq)*(dvx*dvx+dvy*dvy+dvz*dvz));
      Oc_Duet bsx(scratch_best_spin[j].x);
      Oc_Duet bsy(scratch_best_spin[j].y);
      Oc_Duet bsz(scratch_best_spin[j].z);
      Oc_Duet sdvx(local_dvec_scale_duet*dvx);
      Oc_Duet sdvy(local_dvec_scale_duet*dvy);
      Oc_Duet sdvz(local_dvec_scale_duet*dvz);
      Oc_Duet spx = Oc_FMA(mult,bsx,sdvx);
      Oc_Duet spy = Oc_FMA(mult,bsy,sdvy);
      Oc_Duet spz = Oc_FMA(mult,bsz,sdvz);
      Oxs_ThreeVectorPairMakeUnit(spx,spy,spz); // Has different
      /// rounding than ThreeVector::MakeUnit().
      scratch_spin[j].x = spx.GetA();
      scratch_spin[j].y = spy.GetA();
      scratch_spin[j].z = spz.GetA();
#endif
    }
    assert(STRIDE == 2);
    for(; j+1<jsize; j+=STRIDE) { // asdf 1
      // Note: This loop involves two sqrts and two divisions, which on
      //       current processors makes it compute bound.  So this loop
      //       scales well with additional cores, and there isn't much
      //       gain to overlapping arithmetic with loads inside one loop
      //       --- there is enough arithmetic space that up to 8 threads
      //       overlap load space onto arithmetic space fairly easily.
      // Note 2: A low cycle count double precision reciprocal square
      //       root would nicely speed up ThreeVector::MakeUnit().
#if OC_USE_SSE
      const int FOFF=12;
      Oc_Prefetch<Ocpd_T0>((const char *)(scratch_direction+j+FOFF));
      Oc_Prefetch<Ocpd_T0>((const char *)(scratch_best_spin+j+FOFF));
#endif
      Oc_Duet dvx,dvy,dvz;
      Oxs_ThreeVectorPairLoadAligned(&(scratch_direction[j]),dvx,dvy,dvz);
      Oc_Duet mult = sqrt(Oc_Duet(1)
                             +Oc_Duet(tsq)*(dvx*dvx+dvy*dvy+dvz*dvz));

      Oc_Duet bsx,bsy,bsz;
      Oxs_ThreeVectorPairLoadAligned(&(scratch_best_spin[j]),bsx,bsy,bsz);
      Oc_Duet sdvx(local_dvec_scale_duet*dvx);
      Oc_Duet sdvy(local_dvec_scale_duet*dvy);
      Oc_Duet sdvz(local_dvec_scale_duet*dvz);
      Oc_Duet spx = Oc_FMA(mult,bsx,sdvx);
      Oc_Duet spy = Oc_FMA(mult,bsy,sdvy);
      Oc_Duet spz = Oc_FMA(mult,bsz,sdvz);
      Oxs_ThreeVectorPairMakeUnit(spx,spy,spz); // Has different
      /// rounding than ThreeVector::MakeUnit().
      Oxs_ThreeVectorPairStreamAligned(spx,spy,spz,scratch_spin+j);
    }

    for(;j<jsize;++j) {
#if 0
      const ThreeVector& dvec = scratch_direction[j];
      OC_REAL8m dsq = dvec.MagSq();
      ThreeVector temp = scratch_best_spin[j];
      temp *= sqrt(1+tsq*dsq);
      temp.Accum(dvec_scale,dvec);
      temp.MakeUnit();
      Nb_NOP(&temp);          // Safety; icpc optimizer showed problems
      scratch_spin[j] = temp; // with this stanza in some circumstances.
#else
      // Use same computation code as in main block, so rounding
      // errors on job edges match those in interior:
      Oc_Duet dvx(scratch_direction[j].x); // Fills both upper and lower
      Oc_Duet dvy(scratch_direction[j].y); // parts with same value.
      Oc_Duet dvz(scratch_direction[j].z);
      Oc_Duet mult = sqrt(Oc_Duet(1)
                             +Oc_Duet(tsq)*(dvx*dvx+dvy*dvy+dvz*dvz));
      Oc_Duet bsx(scratch_best_spin[j].x);
      Oc_Duet bsy(scratch_best_spin[j].y);
      Oc_Duet bsz(scratch_best_spin[j].z);
      Oc_Duet sdvx(local_dvec_scale_duet*dvx);
      Oc_Duet sdvy(local_dvec_scale_duet*dvy);
      Oc_Duet sdvz(local_dvec_scale_duet*dvz);
      Oc_Duet spx = Oc_FMA(mult,bsx,sdvx);
      Oc_Duet spy = Oc_FMA(mult,bsy,sdvy);
      Oc_Duet spz = Oc_FMA(mult,bsz,sdvz);
      Oxs_ThreeVectorPairMakeUnit(spx,spy,spz); // Has different
      /// rounding than ThreeVector::MakeUnit().
      scratch_spin[j].x = spx.GetA();
      scratch_spin[j].y = spy.GetA();
      scratch_spin[j].z = spz.GetA();
#endif
    }

  }
}

class _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket;
  static Oc_AlignedVector<Nb_Xpfloat> etemp;
  static Oc_AlignedVector<Nb_Xpfloat> dtemp;
  static Oc_AlignedVector<Nb_Xpfloat> stemp;

  const Oxs_Mesh* mesh;
  const Oxs_MeshValue<OC_REAL8m>* tmpenergy;
  const Oxs_MeshValue<OC_REAL8m>* bestpt_energy;
  const Oxs_MeshValue<OC_REAL8m>* Ms;

  const Oxs_MeshValue<ThreeVector>* direction;
  const Oxs_MeshValue<ThreeVector>* mxHxm;

  OC_REAL8m offset_sq;

  _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB()
    : mesh(0),tmpenergy(0),bestpt_energy(0), Ms(0),
      direction(0), mxHxm(0),
      offset_sq(0.) {}

  static void Init(int thread_count,
                   const Oxs_StripedArray<OC_REAL8m>* arrblock) {
    // NB: This routine must be called from the master thread
    // prior to launching the Cmd threads.
    job_basket.Init(thread_count,arrblock);
    etemp.resize(thread_count);
    dtemp.resize(thread_count);
    stemp.resize(thread_count);
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::job_basket;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::etemp;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::dtemp;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::stemp;


void _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::Cmd(int threadnumber,
                                                  void* /* data */)
{
  Nb_Xpfloat work_etemp = 0.0,   work_dtemp = 0.0,   work_stemp = 0.0;
  Nb_Xpfloat work_etemp_b = 0.0, work_dtemp_b = 0.0, work_stemp_b = 0.0;

  while(1) {
    OC_INDEX istart,istop;
    job_basket.GetJob(threadnumber,istart,istop);
    if(istart>=istop) break; // No more jobs

    const OC_REAL8m* stenergy    = &((*tmpenergy)[istart]);
    const OC_REAL8m* sbenergy    = &((*bestpt_energy)[istart]);
    const OC_REAL8m* sMs         = &((*Ms)[istart]);
    const ThreeVector* sdir   = &((*direction)[istart]);
    const ThreeVector* smxHxm = &((*mxHxm)[istart]);

    OC_INDEX jsize = istop - istart;

    // Note: The following alignment jig works identically for all of
    // the arrays because all have the same length modulo the ALIGNMENT
    // value.
    const OC_INDEX ALIGNMENT = 16;  // In bytes
    const OC_INDEX jbreak = (OC_UINDEX(stenergy)%ALIGNMENT)/8;

#if OC_USE_SSE
    // Each of direction, best_spin, and spin are Oxs_MeshValue objects,
    // therefore the start of each of the component arrays should start
    // on a page boundary.  Each array is an array of ThreeVectors, so
    // if one is aligned to a 16-byte boundary at offset j, then the
    // others should be as well.  This allows for SSE aligned loads and
    // stores for all three.
    assert(size_t(stenergy + jbreak)%ALIGNMENT == 0);
    assert(size_t(sbenergy + jbreak)%ALIGNMENT == 0);
    assert(size_t(sMs      + jbreak)%ALIGNMENT == 0);
    assert(size_t(sdir     + jbreak)%ALIGNMENT == 0);
    assert(size_t(smxHxm   + jbreak)%ALIGNMENT == 0);
#endif

    OC_INDEX j;

    if(mesh->HasUniformCellVolumes()) {
      // Cell volume adjustment handled by caller
     for(j=0;j<jbreak;++j) {
       const ThreeVector& vtemp = sdir[j];
       OC_REAL8m scale_adj = sMs[j]/sqrt(1+offset_sq * vtemp.MagSq());
       work_etemp.Accum((stenergy[j] - sbenergy[j]));
       work_dtemp.Accum((smxHxm[j]*vtemp)*scale_adj);
       work_stemp.Accum(smxHxm[j].MagSq()*scale_adj*scale_adj);
       /// See mjd's NOTES II, 29-May-2002, p156, which includes
       /// the derivation of the scale_adj term above.
     }
     Oc_Duet dt_offset_sq(offset_sq);
     for(;j+1<jsize;j+=2) { // asdf 3->1
       // Note: This loop involves a sqrt and a division, which on
       //       current processors makes it compute bound.  So this loop
       //       scales well with additional cores, and there isn't much
       //       gain to overlapping arithmetic with loads inside one loop
       //       --- there is enough arithmetic space that up to 8 threads
       //       overlap load space onto arithmetic space fairly easily.
       // Note 2: If a low cycle count double-precision reciprocal
       //       square-root becomes available, then the counts change
       //       dramatically and this loop would be memory bandwidth
       //       bound.
 #if OC_USE_SSE
       const int OFFSET=12; // BLAHBLAHBLAH;
       Oc_Prefetch<Ocpd_T0>((const char *)(sdir+j+OFFSET));
       Oc_Prefetch<Ocpd_T0>((const char *)(smxHxm+j+OFFSET));
       Oc_Prefetch<Ocpd_T0>((const char *)(sMs+j+OFFSET));
       Oc_Prefetch<Ocpd_T0>((const char *)(stenergy+j+OFFSET));
       Oc_Prefetch<Ocpd_T0>((const char *)(sbenergy+j+OFFSET));
 #endif
       Oc_Duet vtx,vty,vtz; Oxs_ThreeVectorPairLoadAligned(&(sdir[j]),
                                                           vtx,vty,vtz);
       Oc_Duet vt_magsq = vtx*vtx + vty*vty + vtz*vtz;
       Oc_Duet denom = sqrt(Oc_Duet(1.) + dt_offset_sq * vt_magsq);

       Oc_Duet scale_adj;
       scale_adj.LoadAligned(sMs[j]);
       scale_adj /= denom;

       Oc_Duet ste;   ste.LoadAligned(stenergy[j]);
       Oc_Duet sbe;   sbe.LoadAligned(sbenergy[j]);

       Oc_Duet mhx, mhy, mhz;   Oxs_ThreeVectorPairLoadAligned(&(smxHxm[j]),
                                                               mhx,mhy,mhz);
       Oc_Duet mh_magsq = mhx*mhx + mhy*mhy + mhz*mhz;
       Oc_Duet mh_dot_vt = mhx*vtx + mhy*vty + mhz*vtz;

       Nb_XpfloatDualAccum(work_etemp,work_etemp_b,(ste-sbe));
       Nb_XpfloatDualAccum(work_dtemp,work_dtemp_b,mh_dot_vt*scale_adj);
       Nb_XpfloatDualAccum(work_stemp,work_stemp_b,
                           mh_magsq*scale_adj*scale_adj);
     }
     for(;j<jsize;++j) {
       const ThreeVector& vtemp = sdir[j];
       OC_REAL8m scale_adj = sMs[j]/sqrt(1+offset_sq * vtemp.MagSq());
       work_etemp.Accum((stenergy[j] - sbenergy[j]));
       work_dtemp.Accum((smxHxm[j]*vtemp)*scale_adj);
       work_stemp.Accum(smxHxm[j].MagSq()*scale_adj*scale_adj);
     }

    } else { // Cell volumes vary by cell

     for(j=0;j<jbreak;++j) {
       OC_REAL8m vol = mesh->Volume(istart+j);
       const ThreeVector& vtemp = sdir[j];
       OC_REAL8m scale_adj = sMs[j]*vol/sqrt(1+offset_sq * vtemp.MagSq());
       work_etemp.Accum((stenergy[j] - sbenergy[j]) * vol);
       work_dtemp.Accum((smxHxm[j]*vtemp)*scale_adj);
       work_stemp.Accum(smxHxm[j].MagSq()*scale_adj*scale_adj);
       /// See mjd's NOTES II, 29-May-2002, p156, which includes
       /// the derivation of the scale_adj term above.
     }
     Oc_Duet dt_offset_sq(offset_sq);
     for(;j+1<jsize;j+=2) { // asdf 3->1
       // Note: This loop involves a sqrt and a division, which on
       //       current processors makes it compute bound.  So this loop
       //       scales well with additional cores, and there isn't much
       //       gain to overlapping arithmetic with loads inside one loop
       //       --- there is enough arithmetic space that up to 8 threads
       //       overlap load space onto arithmetic space fairly easily.
       // Note 2: If a low cycle count double-precision reciprocal
       //       square-root becomes available, then the counts change
       //       dramatically and this loop would be memory bandwidth
       //       bound.
 #if OC_USE_SSE
       const int OFFSET=12; // BLAHBLAHBLAH;
       Oc_Prefetch<Ocpd_T0>((const char *)(sdir+j+OFFSET));
       Oc_Prefetch<Ocpd_T0>((const char *)(smxHxm+j+OFFSET));
       Oc_Prefetch<Ocpd_T0>((const char *)(sMs+j+OFFSET));
       Oc_Prefetch<Ocpd_T0>((const char *)(stenergy+j+OFFSET));
       Oc_Prefetch<Ocpd_T0>((const char *)(sbenergy+j+OFFSET));
 #endif
       Oc_Duet vol;   mesh->VolumePair(j,vol);

       Oc_Duet vtx,vty,vtz; Oxs_ThreeVectorPairLoadAligned(&(sdir[j]),
                                                           vtx,vty,vtz);
       Oc_Duet vt_magsq = vtx*vtx + vty*vty + vtz*vtz;
       Oc_Duet denom = sqrt(Oc_Duet(1.) + dt_offset_sq * vt_magsq);

       Oc_Duet dt_Ms;  dt_Ms.LoadAligned(sMs[j]);
       Oc_Duet numer = dt_Ms*vol;
       Oc_Duet scale_adj = numer/denom;

       Oc_Duet ste;   ste.LoadAligned(stenergy[j]);
       Oc_Duet sbe;   sbe.LoadAligned(sbenergy[j]);

       Oc_Duet mhx, mhy, mhz;   Oxs_ThreeVectorPairLoadAligned(&(smxHxm[j]),
                                                               mhx,mhy,mhz);
       Oc_Duet mh_magsq = mhx*mhx + mhy*mhy + mhz*mhz;
       Oc_Duet mh_dot_vt = mhx*vtx + mhy*vty + mhz*vtz;

       Nb_XpfloatDualAccum(work_etemp,work_etemp_b,(ste-sbe)*vol);
       Nb_XpfloatDualAccum(work_dtemp,work_dtemp_b,mh_dot_vt*scale_adj);
       Nb_XpfloatDualAccum(work_stemp,work_stemp_b,
                           mh_magsq*scale_adj*scale_adj);
     }
     for(;j<jsize;++j) {
       OC_REAL8m vol = mesh->Volume(istart+j);
       const ThreeVector& vtemp = sdir[j];
       OC_REAL8m scale_adj = sMs[j]*vol/sqrt(1+offset_sq * vtemp.MagSq());
       work_etemp.Accum((stenergy[j] - sbenergy[j]) * vol);
       work_dtemp.Accum((smxHxm[j]*vtemp)*scale_adj);
       work_stemp.Accum(smxHxm[j].MagSq()*scale_adj*scale_adj);
     }
    }
  }

  // Sum results into static accumulators
  work_etemp += work_etemp_b;
  work_dtemp += work_dtemp_b;
  work_stemp += work_stemp_b;
  etemp[threadnumber] = work_etemp;
  dtemp[threadnumber] = work_dtemp;
  stemp[threadnumber] = work_stemp;
}

class _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC : public Oxs_ThreadRunObj {
public:
  // Note: The following are static, so only one "set" of this class may
  // be run at one time.
  static Oxs_JobControl<OC_REAL8m> job_basket; 
  static Oc_AlignedVector<Nb_Xpfloat> etemp;
  const Oxs_Mesh* mesh;
  const Oxs_MeshValue<OC_REAL8m>* energy;

  _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC()
    : mesh(0),energy(0) {}

  static void Init(int thread_count,const Oxs_StripedArray<OC_REAL8m>* arrblock) {
    // NB: This routine must be called from the master thread
    // prior to launching the Cmd threads.
    job_basket.Init(thread_count,arrblock);
    etemp.resize(thread_count);
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::job_basket;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::etemp;

void _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::Cmd(int threadnumber,
                                                  void* /* data */)
{
  Nb_Xpfloat work_etemp_a = 0.0;
  Nb_Xpfloat work_etemp_b = 0.0;
  Nb_Xpfloat work_etemp_c = 0.0;
  Nb_Xpfloat work_etemp_d = 0.0;

  const OC_REAL8m* const senergy  = &((*energy)[OC_INDEX(0)]);
  const Oxs_Mesh* const smesh = mesh;
  while(1) {

    OC_INDEX istart,istop;
    job_basket.GetJob(threadnumber,istart,istop);
    if(istart>=istop) break; // No more jobs

    OC_INDEX i;

    // We want to process senergy at a multiple of 16, so we can use SSE
    // aligned loads.  Note that ibreak is at most istart+1, and from
    // above istart<istop, so we are guaranteed ibreak<=istop.  BTW,
    // this code guarantees 16-byte alignment only if the array element
    // size is a multiple of 8.  If we are using SSE, then
    // sizeof(OC_REAL8m)==8, so no problem.  On some platforms
    // sizeof(long double) might be 10 or 12, but in that case we can't
    // use SSE on the array anyway, so it doesn't matter if we actually
    // get 16-byte alignment or not.
    const OC_UINDEX ALIGNMENT = 16;  // In bytes
    const OC_INDEX ibreak = istart + (OC_UINDEX(senergy+istart)%ALIGNMENT)/8;
    for(i=istart;i<ibreak;++i) {
      work_etemp_a.Accum(senergy[i] * smesh->Volume(i));
    }

    Oc_Duet seA, seB, seC, seD;
    Oc_Duet vA, vB, vC, vD;
    if(i+7<istop) { // asdf +7
      smesh->VolumeQuad(i,  vA,vB);
      smesh->VolumeQuad(i+4,vC,vD);
    }
    const unsigned int LOOP_SIZE = 8;
    for(;i+OC_INDEX(7)<istop;i+=OC_INDEX(LOOP_SIZE)) { // asdf +7
      // Code below assumes LOOP_SIZE == 8
      Oc_Prefetch<Ocpd_T0>((const char *)(senergy+i)+16*64);
      seA.LoadAligned(senergy[i]);   seA *= vA;
      seB.LoadAligned(senergy[i+2]); seB *= vB;
      smesh->VolumeQuad(i+ 8,vA,vB);
      seC.LoadAligned(senergy[i+4]); seC *= vC;
      seD.LoadAligned(senergy[i+6]); seD *= vD;
      smesh->VolumeQuad(i+12,vC,vD);
      Nb_XpfloatDualAccum(work_etemp_a,work_etemp_b,seA);
      Nb_XpfloatDualAccum(work_etemp_c,work_etemp_d,seB);
      Nb_XpfloatDualAccum(work_etemp_a,work_etemp_b,seC);
      Nb_XpfloatDualAccum(work_etemp_c,work_etemp_d,seD);
    }

    for(;i<istop;++i) {
      work_etemp_a.Accum(senergy[i] * smesh->Volume(i));
    }
  }

  // Sum results into static accumulators
  work_etemp_a += work_etemp_b;
  work_etemp_c += work_etemp_d;
  work_etemp_a += work_etemp_c;
  etemp[threadnumber] = work_etemp_a;
}

class _Oxs_CGEvolve_SetBasePoint_ThreadA : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<ThreeVector> job_basket;
  static Oc_AlignedVector<Nb_Xpfloat> gamma_sum;
  static Oc_AlignedVector<Nb_Xpfloat> g_sum_sq;

  // Imports
  const Oxs_MeshValue<ThreeVector>* preconditioner_Ms2_V2;
  const Oxs_MeshValue<ThreeVector>* bestpt_mxHxm;

  // basept_mxHxm is used only by Polak-Ribiere method,
  // where it is both import and export
  Oxs_MeshValue<ThreeVector>* basept_mxHxm;

  enum Conjugate_Method { FLETCHER_REEVES, POLAK_RIBIERE } method;

  _Oxs_CGEvolve_SetBasePoint_ThreadA()
    :  preconditioner_Ms2_V2(0),bestpt_mxHxm(0), basept_mxHxm(0),
       method(FLETCHER_REEVES) {}

  static void Init(int thread_count,
                   const Oxs_StripedArray<ThreeVector>* arrblock) {
    job_basket.Init(thread_count,arrblock);
    gamma_sum.resize(thread_count);
    g_sum_sq.resize(thread_count);
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<ThreeVector> _Oxs_CGEvolve_SetBasePoint_ThreadA::job_basket;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_SetBasePoint_ThreadA::gamma_sum;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_SetBasePoint_ThreadA::g_sum_sq;

void _Oxs_CGEvolve_SetBasePoint_ThreadA::Cmd(int threadnumber,
                                             void* /* data */)
{
  Nb_Xpfloat work_gamma_sum = 0.0, work_g_sum_sq = 0.0;
  while(1) {
    OC_INDEX istart,istop;
    job_basket.GetJob(threadnumber,istart,istop);

    if(istart>=istop) break; // No more jobs

    const ThreeVector* sP = &((*preconditioner_Ms2_V2)[istart]);
    const ThreeVector* sbest_mxHxm = &((*bestpt_mxHxm)[istart]);

    const OC_UINDEX ALIGNMENT = 16;  // In bytes
#if OC_USE_SSE
    assert(OC_UINDEX(sP)%ALIGNMENT == OC_UINDEX(sbest_mxHxm)%ALIGNMENT);
#endif
    // This alignment trick requires sizeof(ThreeVector) to be a
    // multiple of 8, so that OC_UINDEX(sdir)%ALIGNMENT is 0 or 8.
    // This is true if we are using SSE, since then OC_REAL8m is an
    // 8-byte double.  It is also only under SSE that we need this
    // restriction.

    OC_INDEX jsize = istop - istart;

    if(method != POLAK_RIBIERE) {
      // Fletcher-Reeves method
      OC_INDEX j;
      Nb_Xpfloat work_g_sum_sq_0 = 0.0, work_g_sum_sq_1 = 0.0;
      Nb_Xpfloat work_g_sum_sq_2 = 0.0, work_g_sum_sq_3 = 0.0;

      const OC_INDEX jbreak = (OC_UINDEX(sP)%ALIGNMENT)/8;

      for(j=0;j<jbreak;++j) {
        work_g_sum_sq.Accum(sbest_mxHxm[j].x*sbest_mxHxm[j].x*sP[j].x);
        work_g_sum_sq.Accum(sbest_mxHxm[j].y*sbest_mxHxm[j].y*sP[j].y);
        work_g_sum_sq.Accum(sbest_mxHxm[j].z*sbest_mxHxm[j].z*sP[j].z);
        // preconditioner has (Ms[i]*mesh->Volume(i))^2 built in.
      }
#if OC_USE_SSE
      assert(OC_UINDEX(sbest_mxHxm+j)%ALIGNMENT == 0);
      assert(OC_UINDEX(sP+j)%ALIGNMENT == 0);
#endif
      const OC_INDEX STRIDE = 4;
      Oc_Duet a0,b0,a1,b1,a2,b2,a3,b3,a4,b4,a5,b5;
      while(j+2*STRIDE-1<jsize) {
        Oc_Prefetch<Ocpd_T0>((const char *)(sbest_mxHxm+j)+4*64);
        Oc_Prefetch<Ocpd_T0>((const char *)(sbest_mxHxm+j)+5*64);
        Oc_Prefetch<Ocpd_T0>((const char *)(sbest_mxHxm+j)+6*64);
        Oc_Prefetch<Ocpd_T0>((const char *)(sP+j)+4*64);
        Oc_Prefetch<Ocpd_T0>((const char *)(sP+j)+5*64);
        Oc_Prefetch<Ocpd_T0>((const char *)(sP+j)+6*64);
        for(OC_INDEX j2=0; j2<2; ++j2, j+=STRIDE) {
          a0.LoadAligned(sbest_mxHxm[j].x);   a0 *= a0;
          a1.LoadAligned(sbest_mxHxm[j].z);   a1 *= a1;
          b0.LoadAligned(sP[j].x);            a0 *= b0;
          b1.LoadAligned(sP[j].z);            a1 *= b1;


          a2.LoadAligned(sbest_mxHxm[j+1].y); a2 *= a2;
          a3.LoadAligned(sbest_mxHxm[j+2].x); a3 *= a3;
          b2.LoadAligned(sP[j+1].y);          a2 *= b2;
          b3.LoadAligned(sP[j+2].x);          a3 *= b3;


          a4.LoadAligned(sbest_mxHxm[j+2].z); a4 *= a4;
          a5.LoadAligned(sbest_mxHxm[j+3].y); a5 *= a5;
          b4.LoadAligned(sP[j+2].z);          a4 *= b4;
          b5.LoadAligned(sP[j+3].y);          a5 *= b5;

          Nb_XpfloatDualAccum(work_g_sum_sq_0,work_g_sum_sq_1,a0);
          Nb_XpfloatDualAccum(work_g_sum_sq_2,work_g_sum_sq_3,a1);
          Nb_XpfloatDualAccum(work_g_sum_sq_0,work_g_sum_sq_1,a2);
          Nb_XpfloatDualAccum(work_g_sum_sq_2,work_g_sum_sq_3,a3);
          Nb_XpfloatDualAccum(work_g_sum_sq_0,work_g_sum_sq_1,a4);
          Nb_XpfloatDualAccum(work_g_sum_sq_2,work_g_sum_sq_3,a5);
        }
      }
      for(;j<jsize;++j) {
        work_g_sum_sq.Accum(sbest_mxHxm[j].x*sbest_mxHxm[j].x*sP[j].x);
        work_g_sum_sq.Accum(sbest_mxHxm[j].y*sbest_mxHxm[j].y*sP[j].y);
        work_g_sum_sq.Accum(sbest_mxHxm[j].z*sbest_mxHxm[j].z*sP[j].z);
      }
      work_g_sum_sq += work_g_sum_sq_0 + work_g_sum_sq_1;
      work_g_sum_sq += work_g_sum_sq_2 + work_g_sum_sq_3;
      work_gamma_sum = work_g_sum_sq;
    } else {
      // Polak-Ribiere method
      ThreeVector* sbase_mxHxm = &((*basept_mxHxm)[istart]);
      for(OC_INDEX j=0;j<jsize;++j) {
	ThreeVector temp = sbest_mxHxm[j];
        // preconditioner has (Ms[i]*mesh->Volume(i))^2 built in.
        work_g_sum_sq.Accum(temp.x*temp.x*sP[j].x);
        work_g_sum_sq.Accum(temp.y*temp.y*sP[j].y);
        work_g_sum_sq.Accum(temp.z*temp.z*sP[j].z);
	temp -= sbase_mxHxm[j];  // Polak-Ribiere adjustment
	sbase_mxHxm[j] = sbest_mxHxm[j];
        work_gamma_sum.Accum(temp.x*sbest_mxHxm[j].x*sP[j].x);
        work_gamma_sum.Accum(temp.y*sbest_mxHxm[j].y*sP[j].y);
        work_gamma_sum.Accum(temp.z*sbest_mxHxm[j].z*sP[j].z);
      }
    }
  }

  g_sum_sq[threadnumber] = work_g_sum_sq;
  gamma_sum[threadnumber] = work_gamma_sum;
}

class _Oxs_CGEvolve_SetBasePoint_ThreadB : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket;
  static Oc_AlignedVector<Nb_Xpfloat> normsumsq; // Export
  static Oc_AlignedVector<Nb_Xpfloat> Ep;        // Export
  static Oc_AlignedVector<Nb_Xpfloat> gradsumsq; // Export
  static std::vector<OC_REAL8m> maxmagsq;   // Export

  // Imports
  const Oxs_MeshValue<ThreeVector>* preconditioner_Ms_V;
  const Oxs_MeshValue<OC_REAL8m>* Ms_V;
  const Oxs_MeshValue<ThreeVector>* spin;
  const Oxs_MeshValue<ThreeVector>* bestpt_mxHxm;

  // basept_direction is both import and export
  Oxs_MeshValue<ThreeVector>* basept_direction;

  OC_REAL8m gamma;  // Import --- same for all threads

  _Oxs_CGEvolve_SetBasePoint_ThreadB()
    : preconditioner_Ms_V(0), Ms_V(0), spin(0),
      bestpt_mxHxm(0), basept_direction(0),
      gamma(0.0) {}

  static void Init(int thread_count,
                   const Oxs_StripedArray<OC_REAL8m>* arrblock) {
    job_basket.Init(thread_count,arrblock);
    maxmagsq.resize(thread_count);
    normsumsq.resize(thread_count);
    Ep.resize(thread_count);
    gradsumsq.resize(thread_count);
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_CGEvolve_SetBasePoint_ThreadB::job_basket;
std::vector<OC_REAL8m>  _Oxs_CGEvolve_SetBasePoint_ThreadB::maxmagsq;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_SetBasePoint_ThreadB::normsumsq;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_SetBasePoint_ThreadB::Ep;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_SetBasePoint_ThreadB::gradsumsq;

void _Oxs_CGEvolve_SetBasePoint_ThreadB::Cmd(int threadnumber,
                                             void* /* data */)
{
  OC_REAL8m maxmagsq0=0.0;
  Nb_Xpfloat normsumsq0=0.0, normsumsq1=0.0;
  Nb_Xpfloat Ep0=0.0, Ep1=0.0;
  Nb_Xpfloat gradsumsq0=0.0, gradsumsq1=0.0;
  Oc_Duet maxmagsq_pair(0,0);

  while(1) {
    OC_INDEX istart,istop;
    job_basket.GetJob(threadnumber,istart,istop);

    if(istart>=istop) break; // No more jobs

    const OC_REAL8m* sMs_V    = &((*Ms_V)[istart]);
    const ThreeVector* sP     = &((*preconditioner_Ms_V)[istart]);
    const ThreeVector* sspin  = &((*spin)[istart]);
    const ThreeVector* smxHxm = &((*bestpt_mxHxm)[istart]);
    ThreeVector* sdir   = &((*basept_direction)[istart]);

    OC_INDEX jsize = istop - istart;
    OC_INDEX j;

    const size_t ALIGNMENT = 16;  // In bytes
    const OC_INDEX jbreak  = (OC_UINDEX(sdir)%ALIGNMENT)/8;
    // This alignment trick requires sizeof(ThreeVector) to be a
    // multiple of 8, so that OC_UINDEX(sdir)%ALIGNMENT is 0 or 8.
    // This is true if we are using SSE, since then OC_REAL8m is an
    // 8-byte double.  It is also only under SSE that we need this
    // restriction.

    for(j=0;j<jbreak;++j) {
      ThreeVector temp = smxHxm[j];
      temp.x *= sP[j].x;  temp.y *= sP[j].y;  temp.z *= sP[j].z;
      /// Preconditioner has Ms[i]*mesh->Volume(i) built in.
      temp.Accum(gamma,sdir[j]);
      // Make temp orthogonal to sspin[j].  If the angle between
      // temp and spin[i] is larger than about 45 degrees, then
      // it may be beneficial to divide by spin[i].MagSq(),
      // to offset effects of non-unit spin.  For small angles
      // it appears to be better to leave out this adjustment.
      // Or, better still, use temp -= (temp.m)m formulation
      // for large angles, w/o size correction.
      temp.Accum(-1*(temp*sspin[j]),sspin[j]);
      sdir[j] = temp;

      OC_REAL8m magsq = temp.MagSq();
      if(magsq>maxmagsq0) maxmagsq0 = magsq;
      normsumsq0 += magsq;
      Ep0 += (temp * smxHxm[j]) * sMs_V[j];
      gradsumsq0 += (smxHxm[j].MagSq()*sMs_V[j]*sMs_V[j]);
      /// See mjd's NOTES II, 29-May-2002, p156.
    }
#if OC_USE_SSE
    assert(OC_UINDEX(smxHxm+j)%ALIGNMENT == 0);
    assert(OC_UINDEX(sP+j)%ALIGNMENT == 0);
    assert(OC_UINDEX(sdir+j)%ALIGNMENT == 0);
    assert(OC_UINDEX(sspin+j)%ALIGNMENT == 0);
    assert(OC_UINDEX(sMs_V+j)%ALIGNMENT == 0);
#endif
    for(;j+1<jsize;j+=2) { // asdf +1
      Oc_Duet tqx,tqy,tqz;
      Oxs_ThreeVectorPairLoadAligned(&(smxHxm[j]),tqx,tqy,tqz);

      Oc_Duet tx,ty,tz;
      Oxs_ThreeVectorPairLoadAligned(&(sP[j]),tx,ty,tz);

      tx*=tqx; ty*=tqy; tz*=tqz;

      Oc_Duet dx,dy,dz;
      Oxs_ThreeVectorPairLoadAligned(&(sdir[j]),dx,dy,dz);

      Oc_Duet gp(gamma,gamma);
      dx *= gp;  dy *= gp;  dz *= gp;
      tx += dx;  ty += dy;  tz += dz;

      Oc_Duet mx,my,mz;
      Oxs_ThreeVectorPairLoadAligned(&(sspin[j]),mx,my,mz);
      Oc_Duet dot = tx*mx;  dot += ty*my;  dot += tz*mz;
      tx -= dot*mx;          ty -= dot*my;  tz -= dot*mz;

      // Note: sdir[j] is in cache, so don't use _mm_stream_pd (which is
      // significantly slower in this case!).
      Oxs_ThreeVectorPairStoreAligned(tx,ty,tz,sdir+j);

      Oc_Duet magsq = tx*tx + (ty*ty + tz*tz);
      maxmagsq_pair.KeepMax(magsq);

      Nb_XpfloatDualAccum(normsumsq0,normsumsq1,magsq);

      Oc_Duet MsV; MsV.LoadAligned(sMs_V[j]);
      Oc_Duet sum = (tx*tqx + ty*tqy + tz*tqz) * MsV;
      Oc_Duet gsum = (tqx*tqx + tqy*tqy + tqz*tqz) * MsV * MsV;
      Nb_XpfloatDualAccum(Ep0,Ep1,sum);
      Nb_XpfloatDualAccum(gradsumsq0,gradsumsq1,gsum);
    }
    for(;j<jsize;++j) {
      ThreeVector temp = smxHxm[j];
      temp.x *= sP[j].x;  temp.y *= sP[j].y;  temp.z *= sP[j].z;
      temp.Accum(gamma,sdir[j]);
      temp.Accum(-1*(temp*sspin[j]),sspin[j]);
      sdir[j] = temp;
      OC_REAL8m magsq = temp.MagSq();
      if(magsq>maxmagsq0) maxmagsq0 = magsq;
      normsumsq0 += magsq;
      Ep0 += (temp * smxHxm[j]) * sMs_V[j];
      gradsumsq0 += (smxHxm[j].MagSq()*sMs_V[j]*sMs_V[j]);
    }
  }

  if(maxmagsq_pair.GetA()>maxmagsq0) {
    maxmagsq0 = maxmagsq_pair.GetA();
  }
  if(maxmagsq_pair.GetB()>maxmagsq0) {
    maxmagsq0 = maxmagsq_pair.GetB();
  }
  maxmagsq[threadnumber] = maxmagsq0;

  normsumsq0.Accum(normsumsq1);
  normsumsq[threadnumber] = normsumsq0;

  Ep0.Accum(Ep1);
  Ep[threadnumber] = Ep0;

  gradsumsq0.Accum(gradsumsq1);
  gradsumsq[threadnumber] = gradsumsq0;
}


class _Oxs_CGEvolve_SetBasePoint_ThreadC : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket;
  static Oc_AlignedVector<Nb_Xpfloat> sumsq;     // Export
  static Oc_AlignedVector<Nb_Xpfloat> Ep;        // Export
  static Oc_AlignedVector<Nb_Xpfloat> gradsumsq; // Export
  static std::vector<OC_REAL8m> maxmagsq;   // Export

  // Imports
  const Oxs_MeshValue<ThreeVector>* preconditioner_Ms_V;
  const Oxs_MeshValue<OC_REAL8m>* Ms_V;
  const Oxs_MeshValue<ThreeVector>* bestpt_mxHxm;

  // basept_direction is both import and export
  Oxs_MeshValue<ThreeVector>* basept_direction;

  _Oxs_CGEvolve_SetBasePoint_ThreadC()
    : preconditioner_Ms_V(0), Ms_V(0),
      bestpt_mxHxm(0), basept_direction(0) {}

  static void Init(int thread_count,
                   const Oxs_StripedArray<OC_REAL8m>* arrblock) {
    job_basket.Init(thread_count,arrblock);
    sumsq.resize(thread_count);
    Ep.resize(thread_count);
    gradsumsq.resize(thread_count);
    maxmagsq.resize(thread_count);
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_CGEvolve_SetBasePoint_ThreadC::job_basket;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_SetBasePoint_ThreadC::sumsq;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_SetBasePoint_ThreadC::Ep;
Oc_AlignedVector<Nb_Xpfloat> _Oxs_CGEvolve_SetBasePoint_ThreadC::gradsumsq;
std::vector<OC_REAL8m> _Oxs_CGEvolve_SetBasePoint_ThreadC::maxmagsq;

void _Oxs_CGEvolve_SetBasePoint_ThreadC::Cmd(int threadnumber,
                                             void* /* data */)
{
  Nb_Xpfloat sumsq_0 = 0.0, sumsq_1 = 0.0;
  Nb_Xpfloat Ep_0 = 0.0, Ep_1 = 0.0;
  Nb_Xpfloat gradsumsq0=0.0, gradsumsq1=0.0;
  OC_REAL8m maxmagsq_local(0.0);
  Oc_Duet maxmagsq_pair(0.0,0.0);

  while(1) {
    OC_INDEX istart,istop;
    job_basket.GetJob(threadnumber,istart,istop);

    if(istart>=istop) break; // No more jobs

    const ThreeVector* sP       = &((*preconditioner_Ms_V)[istart]);
    const OC_REAL8m* sMs_V      = &((*Ms_V)[istart]);
    const ThreeVector* sbest_mxHxm = &((*bestpt_mxHxm)[istart]);
    ThreeVector* sbase_direction = &((*basept_direction)[istart]);

    OC_INDEX jsize = istop - istart;

    // Note: The following alignment jig works identically for all of
    // the arrays because all have the same length modulo the ALIGNMENT
    // value.
    const size_t ALIGNMENT = 16;  // In bytes
    const OC_INDEX jbreak = (OC_UINDEX(sbase_direction)%ALIGNMENT)/8;

    OC_INDEX j;
    for(j=0;j<jbreak;++j) {
      sbase_direction[j] = sbest_mxHxm[j];
      sbase_direction[j].x *= sP[j].x; // Preconditioner has
      sbase_direction[j].y *= sP[j].y; // Ms[i]*mesh->Volume(i)
      sbase_direction[j].z *= sP[j].z; // built in.
      Ep_0 += (sbase_direction[j] * sbest_mxHxm[j]) * sMs_V[j];
      gradsumsq0 += (sbest_mxHxm[j].MagSq()*sMs_V[j]*sMs_V[j]);

      OC_REAL8m magsq = sbase_direction[j].MagSq();
      if(magsq>maxmagsq_local) maxmagsq_local = magsq;
      sumsq_0.Accum(sbase_direction[j].x*sbase_direction[j].x);
      sumsq_0.Accum(sbase_direction[j].y*sbase_direction[j].y);
      sumsq_0.Accum(sbase_direction[j].z*sbase_direction[j].z);
      /// See mjd's NOTES II, 29-May-2002, p156.
    }
    Oc_Duet Ntx,Nty,Ntz; 
    Oc_Duet Nsx,Nsy,Nsz; 
    Oc_Duet Nmsv;        
    if(j+3<jsize) { // asdf +3
      Oxs_ThreeVectorPairLoadAligned(sbest_mxHxm+j,Ntx,Nty,Ntz);
      Oxs_ThreeVectorPairLoadAligned(sP+j,Nsx,Nsy,Nsz);
      Nmsv.LoadAligned(sMs_V[j]);
    }
    for(;j+3<jsize;j+=2) { // asdf +3
      Oc_Prefetch<Ocpd_T0>((const char *)(sbest_mxHxm+j+8));
      Oc_Prefetch<Ocpd_T0>((const char *)(sP+j+8));
      Oc_Prefetch<Ocpd_T0>((const char *)(sMs_V+j+8));
      Oc_Duet tx=Ntx, ty=Nty, tz=Ntz;
      Oc_Duet msv=Nmsv;
      Oc_Duet gsum = (tx*tx + ty*ty + tz*tz) * msv * msv;
      Nb_XpfloatDualAccum(gradsumsq0,gradsumsq1,gsum);

      Oxs_ThreeVectorPairLoadAligned(sbest_mxHxm+j+2,Ntx,Nty,Ntz);

      Oc_Duet sx=Nsz, sy=Nsy, sz=Nsz;
      Oxs_ThreeVectorPairLoadAligned(sP+j+2,Nsx,Nsy,Nsz);

      sx *= tx;  sy *= ty;  sz *= tz;
      tx *= sx;  ty *= sy;  tz *= sz;

      Nmsv.LoadAligned(sMs_V[j+2]);

      Oc_Duet esummand = (tx+ty+tz)*msv;

      Oxs_ThreeVectorPairStreamAligned(sx,sy,sz,sbase_direction+j);
      sx *= sx;  sy *= sy;  sz *= sz;

      Nb_XpfloatDualAccum(Ep_0,Ep_1,esummand);
      Oc_Duet magsq(sx+sy+sz);
      Nb_XpfloatDualAccum(sumsq_0,sumsq_1,sx);
      Nb_XpfloatDualAccum(sumsq_0,sumsq_1,sy);
      Nb_XpfloatDualAccum(sumsq_0,sumsq_1,sz);
      maxmagsq_pair.KeepMax(magsq);
    }
    for(;j<jsize;++j) {
      sbase_direction[j] = sbest_mxHxm[j];
      sbase_direction[j].x *= sP[j].x; // Preconditioner has
      sbase_direction[j].y *= sP[j].y; // Ms[i]*mesh->Volume(i)
      sbase_direction[j].z *= sP[j].z; // built in.
      Ep_0 += (sbase_direction[j] * sbest_mxHxm[j]) * sMs_V[j];
      gradsumsq0 += (sbest_mxHxm[j].MagSq()*sMs_V[j]*sMs_V[j]);
      OC_REAL8m magsq = sbase_direction[j].MagSq();
      if(magsq>maxmagsq_local) maxmagsq_local = magsq;
      sumsq_0.Accum(sbase_direction[j].x*sbase_direction[j].x);
      sumsq_0.Accum(sbase_direction[j].y*sbase_direction[j].y);
      sumsq_0.Accum(sbase_direction[j].z*sbase_direction[j].z);
      /// See mjd's NOTES II, 29-May-2002, p156.
    }
  }

  sumsq_0.Accum(sumsq_1);
  sumsq[threadnumber] = sumsq_0;

  Ep_0.Accum(Ep_1);
  Ep[threadnumber] = Ep_0;

  gradsumsq0.Accum(gradsumsq1);
  gradsumsq[threadnumber] = gradsumsq0;

  if(maxmagsq_pair.GetA()>maxmagsq_local) {
    maxmagsq_local = maxmagsq_pair.GetA();
  }
  if(maxmagsq_pair.GetB()>maxmagsq_local) {
    maxmagsq_local = maxmagsq_pair.GetB();
  }
  maxmagsq[threadnumber] = maxmagsq_local;
}

#endif // OOMMF_THREADS

void Oxs_CGEvolve::GetEnergyAndmxHxm
(const Oxs_SimState* state,          // Import
 Oxs_MeshValue<OC_REAL8m>& export_energy,      // Export
 Oxs_MeshValue<ThreeVector>& export_mxHxm,  // Export
 Oxs_MeshValue<ThreeVector>* export_Hptr)   // Export
{ // Fills export_energy and export_mxHxm, which must be different than
  // scratch_energy and scratch_mxHxm.  Also fills export_Hptr with
  // total field, unless export_Hptr == NULL, in which case the total
  // field is not saved.
  //   This routine also updates energy_calc_count, and fills "Total
  // energy", "Bracket_count", "Line min count", "Energy calc count",
  // and "Cycle count" derived data into state.  These class values
  // should be updated as desired before calling this routine.
  //   SIDE EFFECTS: scratch_energy & scratch_field are altered
  // Note: (mxH)xm = mx(Hxm) = -mx(mxH)

#if REPORT_TIME
  OC_BOOL sot_running = steponlytime.IsRunning();
# if REPORT_TIME_CGDEVEL
  TS(getenergyandmxHxmtime.Start());
  OC_BOOL bpt_running = basepttime.IsRunning();
  OC_BOOL fbt_running = findbrackettime.IsRunning();
  OC_BOOL flt_running = findlinemintime.IsRunning();
  OC_BOOL fill_running = fillbrackettime.IsRunning();
# endif // REPORT_TIME_CGDEVEL
#endif

  if(&export_energy == &scratch_energy) {
      throw Oxs_ExtError(this,
        "Programming error in Oxs_CGEvolve::GetEnergyAndmxHxm():"
	" export_energy is same as scratch_energy.");
  }
  if(&export_mxHxm == &scratch_field) {
      throw Oxs_ExtError(this,
        "Programming error in Oxs_CGEvolve::GetEnergyAndmxHxm():"
	" export_mxHxm is same as scratch_field.");
  }
  if(export_Hptr == &scratch_field) {
      throw Oxs_ExtError(this,
        "Programming error in Oxs_CGEvolve::GetEnergyAndmxHxm():"
	" export_Hptr is same as scratch_field.");
  }

  ++energy_calc_count;    // Update call count

  // For convenience
  const Oxs_Mesh* mesh = state->mesh;

  // Set up energy computation output data structure
  Oxs_ComputeEnergyData oced(*state);
  oced.scratch_energy = &scratch_energy;
  oced.scratch_H      = &scratch_field;
  oced.energy_accum   = &export_energy;
  oced.H_accum        = export_Hptr;
  oced.mxH_accum      = 0;  // Fill export_mxHxm instead
  oced.energy         = 0;
  oced.H              = 0;
  oced.mxH            = 0;

  UpdateFixedSpinList(mesh);
  Oxs_ComputeEnergyExtraData oceed(GetFixedSpinList(),
                                   &export_mxHxm);

  // Compute total energy and torque
#if REPORT_TIME
  if(sot_running) {
    TS(steponlytime.Stop());
#if REPORT_TIME_CGDEVEL
    if(bpt_running)  TS(basepttime.Stop());
    if(fbt_running)  TS(findbrackettime.Stop());
    if(flt_running)  TS(findlinemintime.Stop());
    if(fill_running) TS(fillbrackettime.Stop());
    TS(energyobjtime.Start());
#endif // REPORT_TIME_CGDEVEL
  }
#if REPORT_TIME_CGDEVEL
  TS(getenergyandmxHxmtime.Stop());
#endif // REPORT_TIME_CGDEVEL
#endif // REPORT_TIME
  Oxs_ComputeEnergies(*state,oced,director->GetEnergyObjects(),oceed);
#if REPORT_TIME
#if REPORT_TIME_CGDEVEL
  TS(getenergyandmxHxmtime.Start());
#endif // REPORT_TIME_CGDEVEL
  if(sot_running) {
#if REPORT_TIME_CGDEVEL
    TS(energyobjtime.Stop());
    if(bpt_running) TS(basepttime.Start());
    if(fbt_running) TS(findbrackettime.Start());
    if(flt_running) TS(findlinemintime.Start());
    if(fill_running) TS(fillbrackettime.Start());
#endif // REPORT_TIME_CGDEVEL
    TS(steponlytime.Start());
  }
#endif // REPORT_TIME
  if(oced.pE_pt != 0.0) {
    String msg =
      String("Oxs_CGEvolve::GetEnergyAndmxHxm:"
             " At least one energy object is time varying; this"
             " property is not supported by minimization evolvers.");
    throw Oxs_Ext::Error(this,msg.c_str());
  }

  // Fill supplemental derived data.
  state->AddDerivedData("Max mxHxm",oceed.max_mxH);
  state->AddDerivedData("Energy density error estimate",
                        oced.energy_density_error_estimate);

#if REPORT_TIME_CGDEVEL
TS(timer[8].Start()); /**/ // 2 secs
#endif // REPORT_TIME_CGDEVEL

#if !OOMMF_THREADS
  Nb_Xpfloat total_energy = 0.0;
  const OC_INDEX vecsize = mesh->Size();
  for(OC_INDEX i=0;i<vecsize;++i) {
    total_energy.Accum(export_energy[i] * mesh->Volume(i));
  }
  state->AddDerivedData("Total energy",total_energy.GetValue());
#else // OOMMF_THREADS
  {
    const int thread_count = Oc_GetMaxThreadCount();
    _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::Init(thread_count,
                                                  export_energy.GetArrayBlock());
    _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC threadC;
    threadC.mesh = mesh;
    threadC.energy = &export_energy;

    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(threadC,0);

    Nb_Xpfloat etemp = _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::etemp[0];
    for(int i=1;i<thread_count;++i) {
      etemp += _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::etemp[i];
    }
    state->AddDerivedData("Total energy",etemp.GetValue());
  }
#endif // OOMMF_THREADS

#if REPORT_TIME_CGDEVEL
TS(timer[8].Stop()); /**/
timer_counts[8].Increment(timer[8],mesh->Size()*(1*sizeof(OC_REAL8m)));
/// Volume(i) call can be done w/o main memory access
#endif // REPORT_TIME_CGDEVEL

  state->AddDerivedData("Bracket count",
			OC_REAL8m(bracket_count));
  state->AddDerivedData("Line min count",
			OC_REAL8m(line_minimum_count));
  state->AddDerivedData("Energy calc count",
			OC_REAL8m(energy_calc_count));
#if REPORT_TIME_CGDEVEL
  TS(getenergyandmxHxmtime.Stop());
#endif // REPORT_TIME_CGDEVEL
}


void Oxs_CGEvolve::GetRelativeEnergyAndDerivative(
 Oxs_ConstKey<Oxs_SimState> statekey, // Import
 OC_REAL8m offset,                    // Import
 Bracket& endpt)                      // Export
{ // Calculates total energy relative to that of best_state,
  // and derivative in base_direction (-mu0.H*base_direction).
  // The base_direction and best_energy arrays _must_ be set
  // before calling this function.

  // Uses mxHxm instead of H in calculation of derivative, because
  // energy is calculated off of normalized m, so component of H in m
  // direction doesn't actually have any effect.  Plus, experiments
  // appear to show generally faster convergence with mxHxm than with H.

  const Oxs_SimState* state = statekey.GetPtr();
  endpt.key = statekey;
  endpt.key.GetReadReference();
  GetEnergyAndmxHxm(state,endpt.energy,endpt.mxHxm,NULL);

#if REPORT_TIME_CGDEVEL
  TS(timer[1].Start()); /**/ // 2secs
#endif // REPORT_TIME_CGDEVEL

  // Set up some convenience variables
  const Oxs_Mesh* mesh = state->mesh; // For convenience

  OC_REAL8m energy_density_error_estimate;
  if(!state->GetDerivedData("Energy density error estimate",
                            energy_density_error_estimate)) {
      throw Oxs_ExtError(this,
        "Missing \"Energy density error estimate\" data"
        " in Oxs_CGEvolve::GetRelativeEnergyAndDerivative()."
	" Programming error?");
  }
  OC_REAL8m bestpt_energy_density_error_estimate;
  if(!(bestpt.bracket->key.GetPtr()->GetDerivedData(
                                      "Energy density error estimate",
                                      bestpt_energy_density_error_estimate))) {
    throw Oxs_ExtError(this,
        "Missing best point \"Energy density error estimate\" data"
        " in Oxs_CGEvolve::GetRelativeEnergyAndDerivative()."
	" Programming error?");
  }
  energy_density_error_estimate += bestpt_energy_density_error_estimate;

  // Working variables, which map back to fields in endpt
  OC_REAL8m relenergy = 0;
  OC_REAL8m derivative = 0;
  OC_REAL8m grad_norm;

#if !OOMMF_THREADS
  OC_INDEX i;
  Nb_Xpfloat etemp = 0.0;
  const OC_INDEX vecsize = mesh->Size();
  OC_REAL8m cell_volume = 0.0;
  const Oxs_MeshValue<OC_REAL8m>& new_E  = endpt.energy;
  const Oxs_MeshValue<OC_REAL8m>& best_E = bestpt.bracket->energy;
  const Oxs_MeshValue<ThreeVector>& new_mxHxm = endpt.mxHxm;
  if(mesh->HasUniformCellVolumes(cell_volume)) {
    for(i=0;i<vecsize;++i) {
      etemp.Accum(new_E[i] - best_E[i]);
    }
    relenergy = etemp.GetValue() * cell_volume;
  } else {
    for(i=0;i<vecsize;++i) {
      etemp.Accum((new_E[i] - best_E[i]) * mesh->Volume(i));
    }
    relenergy = etemp.GetValue();
  }

  Nb_Xpfloat dtemp = 0.0;
  Nb_Xpfloat stemp = 0.0;
  OC_REAL8m offset_sq = offset*offset;
  for(i=0;i<vecsize;++i) {
    const ThreeVector& vtemp = basept.direction[i];
    OC_REAL8m scale_adj = Ms_V[i]/sqrt(1+offset_sq * vtemp.MagSq());
    dtemp.Accum((new_mxHxm[i]*vtemp)*scale_adj);
    stemp.Accum(new_mxHxm[i].MagSq()*scale_adj*scale_adj);
  }

  derivative = -MU0 * dtemp.GetValue();
  grad_norm = sqrt(stemp.GetValue());
  /// See mjd's NOTES II, 29-May-2002, p156, which includes
  /// the derivation of the scale_adj term above.
#else // OOMMF_THREADS
  {
    const Oxs_MeshValue<OC_REAL8m>& Ms = *(state->Ms);
    const int thread_count = Oc_GetMaxThreadCount();
    _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::Init(thread_count,
                                                  Ms.GetArrayBlock());
    _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB threadB;
    threadB.mesh = mesh;
    threadB.tmpenergy = &endpt.energy;
    threadB.bestpt_energy = &bestpt.bracket->energy;
    threadB.Ms = &Ms;
    threadB.direction = &basept.direction;
    threadB.mxHxm = &endpt.mxHxm;
    threadB.offset_sq = offset*offset;

    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(threadB,0);

    Nb_Xpfloat etemp = _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::etemp[0];
    Nb_Xpfloat dtemp = _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::dtemp[0];
    Nb_Xpfloat stemp = _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::stemp[0];
    for(int i=1;i<thread_count;++i) {
      etemp += _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::etemp[i];
      dtemp += _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::dtemp[i];
      stemp += _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::stemp[i];
    }
    relenergy = etemp.GetValue();
    derivative = -MU0 * dtemp.GetValue();
    grad_norm = sqrt(stemp.GetValue());
    OC_REAL8m cell_volume = 0.0;
    if(mesh->HasUniformCellVolumes(cell_volume)) {
      // If (and only if) mesh has uniform cells, then threaded
      // code computes values without including cell volume.  So
      // code here has to include cell_volume adjustment.
      relenergy *= cell_volume;
      derivative *= cell_volume;
      grad_norm *= cell_volume;
    }
    /// See mjd's NOTES II, 29-May-2002, p156.
  }
#endif // OOMMF_THREADS

  // Update export values
  endpt.offset = offset;

  endpt.E = relenergy;
  state->AddDerivedData("Relative energy",relenergy);
  state->AddDerivedData("Energy best state id",
                    static_cast<OC_REAL8m>(bestpt.bracket->key.ObjectId()));
  /// Note: Relative energy is relative to energy in best state.

  endpt.E_error_estimate
    = fabs(relenergy)*OC_REAL8m_EPSILON*8; // 8 is fudge
  if(mesh->Size()>0) {
    endpt.E_error_estimate += energy_density_error_estimate
      * mesh->TotalVolume()/sqrt(2.0*OC_REAL8m(mesh->Size()));
    // See NOTES VII, 5-7 July 2017, p 154-155.  
    // Might want to revisit scaling if mesh is not uniform.  Here
    // cell volume is TotalVolume()/Size(), error grows like
    // sqrt(2*Size()) (factor 2 comes from difference operation
    // between new_E and best_E).  Note that
    // energy_density_error_estimate essentially includes a factor of
    // 2, because it is the sum of error estimates from current state
    // and bestpt.
  }

  endpt.Ep = derivative;
  endpt.grad_norm = grad_norm;

#if REPORT_TIME_CGDEVEL
  TS(timer[1].Stop()); /**/
  timer_counts[1].Increment(timer[1],
                  mesh->Size()*(3*sizeof(OC_REAL8m)+2*sizeof(ThreeVector)));
#endif // REPORT_TIME_CGDEVEL
}


OC_REAL8m Oxs_CGEvolve::EstimateEnergySlack() const
{ // Uses bracket data to make an estimate on rounding error
  // in energy computation.  If |E1-E2|<EstimateEnergySlack() then for
  // practical purposes E1 and E2 should be considered equal.

  // In addition to energy error estimates stored in bracket data,
  // include a safety term based on expected E (actually difference
  // between energy and bestpt energy) across interval predicted by
  // E derivative data.  This might help in case where E data are small
  // due to cancellation at bracket test states.
  OC_REAL8m edelta_guess=0;
  if(bracket.right.offset>0) {
    // bracket.right.offset has value -1 if it has not been initialized
    edelta_guess
      = (fabs(basept.Ep)
         + fabs(bracket.left.Ep) + fabs(bracket.right.Ep)) * 0.5
      * bracket.right.offset;  /// 0.5 is 0.33 plus fudge.
  }
  OC_REAL8m slack
    = bracket.left.E_error_estimate + bracket.right.E_error_estimate
    + edelta_guess * OC_REAL8m_EPSILON;

  slack *= energy_error_adj; // User requested tweak

  assert(slack >= 0.0);
  return slack;
}

OC_BOOL Oxs_CGEvolve::BadPrecisionTest
(const Oxs_CGEvolve::Bracket& left,
 const Oxs_CGEvolve::Bracket& right,
 OC_REAL8m energy_slack)
{ // Test for total loss of numeric precision.  One might include a
  // check on the size of Ediff = |left.E - right.E|, say
  // Ediff<=energy_slack, but it can be difficult to get reliable data
  // for energy_slack.
  OC_REAL8m span = right.offset - left.offset;
  OC_REAL8m lEp = left.Ep * span;
  OC_REAL8m rEp = right.Ep * span;
  if(span<=256*bracket.stop_span
     && fabs(rEp-lEp)<fabs(lEp)/16.
     && fabs(lEp) < energy_slack) {
    // The numerics appear bad -- in principle this could happen by
    // chance, but it seems unlikely.
    return 1;
  }
  return 0;
}

void Oxs_CGEvolve::InitializeWorkState
(const Oxs_MinDriver* driver,
 const Oxs_SimState* cstate,
 Oxs_Key<Oxs_SimState>& work_state_key)
{
  // Get Oxs_SimState from director and place a write lock on it.
  director->GetNewSimulationState(work_state_key);

  // Do the first part of work_state structure initialization.
  driver->FillStateMemberData(*cstate,
                              work_state_key.GetWriteReference());
  // Note: Leaves work_state_key holding a write lock
}


void Oxs_CGEvolve::FillBracket
(const Oxs_MinDriver* driver,
 OC_REAL8m offset,
 const Oxs_SimState* oldstateptr,
 Oxs_Key<Oxs_SimState>& statekey,
 Bracket& endpt)
{
  // bestpt and basept must be set prior to calling this routine
#if REPORT_TIME_CGDEVEL
  TS(fillbrackettime.Start());
#endif
  
  try {
    InitializeWorkState(driver,oldstateptr,statekey);
    Oxs_SimState& workstate
      = statekey.GetWriteReference(); // Write lock
    Oxs_MeshValue<ThreeVector>& spin = workstate.spin;
    const Oxs_MeshValue<ThreeVector>& best_spin
      = bestpt.bracket->key.GetPtr()->spin;
    OC_REAL8m t1sq = bestpt.bracket->offset * bestpt.bracket->offset;

#if REPORT_TIME_CGDEVEL
TS(timer[0].Start()); /**/ // 7 secs
#endif // REPORT_TIME_CGDEVEL

#if !OOMMF_THREADS
    OC_INDEX vecsize = workstate.mesh->Size();
    const OC_REAL8m dvec_scale = offset - bestpt.bracket->offset;
    for(OC_INDEX i=0;i<vecsize;i++) {
      const ThreeVector& dvec = basept.direction[i];
      OC_REAL8m dsq = dvec.MagSq();
      ThreeVector temp = best_spin[i];
      temp *= sqrt(1+t1sq*dsq);
      temp.Accum(dvec_scale,dvec);
      temp.MakeUnit();
      Nb_NOP(&temp);  // Safety; icpc optimizer showed problems
      spin[i] = temp; // with this stanza in some circumstances.
    }
#else // OOMMF_THREADS
      {
        _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadA::Init(Oc_GetMaxThreadCount(),
                                             basept.direction.GetArrayBlock());
        _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadA threadA;
        threadA.direction = &basept.direction;
        threadA.best_spin = &best_spin;
        threadA.spin = &spin;
        threadA.tsq = t1sq;
        threadA.dvec_scale = offset-bestpt.bracket->offset;

        Oxs_ThreadTree threadtree;
        threadtree.LaunchTree(threadA,0);
      }
#endif // OOMMF_THREADS

#if REPORT_TIME_CGDEVEL
TS(timer[0].Stop()); /**/
timer_counts[0].Increment(timer[0],
                          workstate.mesh->Size()*(3*sizeof(ThreeVector)));
#endif // REPORT_TIME_CGDEVEL

    workstate.iteration_count = oldstateptr->iteration_count + 1;
    workstate.stage_iteration_count
      = oldstateptr->stage_iteration_count + 1;

    // Release write lock and update energy data
    statekey.GetReadReference();
#if REPORT_TIME_CGDEVEL
    TS(fillbrackettime.Stop());
#endif
    GetRelativeEnergyAndDerivative(statekey,offset,endpt);
#if REPORT_TIME_CGDEVEL
    TS(fillbrackettime.Start());
#endif
    endpt.offset=offset;
  }
  catch (...) {
    statekey.GetReadReference(); // Release write lock
    throw;
  }
  endpt.key = statekey;
  endpt.key.GetReadReference();
#if REPORT_TIME_CGDEVEL
  TS(fillbrackettime.Stop());
#endif
}


void
Oxs_CGEvolve::UpdateBrackets
(Bracket& tbracket, // Import and export
 OC_BOOL force_bestpt)
{ // Replaces either bracket.left or bracket.right with tbracket, and
  // changes bestpt if appropriate.  If bestpt is changed, then
  // bracket E values will also be modified.  On exit tbracket is
  // fill with the data from the replaced (left or right) bracket.
  //
  // If tbracket.offset lies to the right of bracket.right,
  // then a check is made that the minimum was not bracketed
  // coming in and then shift bracket space rightward.
  //
  // OTOH, if tbracket.offset lies between left and right bracket, then
  // it is assumed that the minimum is bracketed coming in and tbracket
  // will replace eith the left or right bracket in such a way to keep
  // the minimum bracketed.  The minimum bracket condition is ideally
  //
  //   left.Ep<0 && (right.Ep>=0 || right.E>left.E+energy_slack)
  //
  // but sometimes only the weaker condition
  //
  //   left.Ep<0 && right.E>left.E
  //
  // can be maintained.  This case (i.e., right.Ep<0 &&
  // right.E<left.E+energy_slack) probably indicates significant
  // precision loss; the client should check for this and handle
  // accordingly.
  //
  // bestpt is adjusted if any of the following is true:
  //  1) force_bestpt is true
  //  2) Bracket endpts shift so that bestpt is not one of the new
  //      bracket endpts
  //  3) One of the bracket endpts is "better" than bestpt.
  //
  // In case 1 bestpt is adjusted to point to the endpt holding the
  // tbracket import data.  In cases 2 and 3 bestpt is adjusted to
  // point to the endpt with the "better" data, where "better" means
  // either smaller energy E, or else close E and smaller |Ep|.
  //
  // NOTE: Typically tbracket is Oxs_CGEvolve::extra_bracket

  OC_REAL8m energy_slack = EstimateEnergySlack();

  // Manage brackets
  if(bracket.right.offset<0) {
    // Right bracket not yet set; initialize with tbracket
    bracket.right.Swap(tbracket);
  } else if(tbracket.offset>bracket.right.offset) {
    // Minimum not bracketed; shift rightward
    if(bracket.right.E<=bracket.left.E+energy_slack
       && bracket.right.Ep<0) { // This should always be
      /// true, since otherwise (left,right] already bracketed the
      /// minimum, but check anyway for robustness.

      /// There is empirical evidence that it might be useful to
      /// make this shift only if the bracket span is relatively
      /// large.  Theoretically, shifting the left bracket can skip
      /// over close minimum.
      if(bracket.right.E<=bracket.left.E - energy_slack ||
         (bracket.right.E <= bracket.left.E + energy_slack
          && fabs(bracket.right.Ep) <= fabs(bracket.left.Ep))) {
        // A related but perhaps better secondary test might be
        //  ave_slope = (bracket.right.E - bracket.left.E)/(bracket.right.offset - bracket.left.offset);
        //  Test: bracket.left.Ep < ave_slope < bracket.right.Ep
        bracket.left.Swap(bracket.right);
      }
    }
    bracket.right.Swap(tbracket);
  } else {
    // Minimum is bracketed; determine which bracket pair to keep to
    // retain minimum.
    int keepbracket = 0;  // -1 to keep left bracket, 1 to keep right
    if(tbracket.Ep>=0) {
      keepbracket = -1; // Keep left
    } else if(bracket.right.Ep>=0) {
      keepbracket =  1; // Keep right
    } else {
      // All Ep's are negative, so bracket decision needs to rely on E
      // data --- which may be numerically noisy.
      if(bracket.left.E >= tbracket.E) {
        keepbracket = 1; // Left not good, so keep right
        // Note that at least one of bracket.left.E < tbracket.E or
        // tbracket.E < bracket.right.E must be true, since minimum is
        // bracketed.
      } else if(tbracket.E >= bracket.right.E) {
        keepbracket = -1; // Right not good, so keep left
      } else {
        // Both bracket.left.E < tbracket.E and tbracket.E <
        // bracket.right.E are true.  This means that either two
        // minima are bracketed, or else numerics are bad.  The code
        // below checks the numerics, and if one fails then keeps the
        // other.  Otherwise, we keep interval with largest E
        // difference.  (If we really trust the numerics it would seem
        // better to keep the left interval as that is less likely to
        // "hop" over a local minimum, but if there isn't really a minimum
        // in the left interval then we can waste a lot of time searching
        // for something that's not there.)
        OC_BOOL lbad = BadPrecisionTest(bracket.left,tbracket,energy_slack);
        OC_BOOL rbad = BadPrecisionTest(tbracket,bracket.right,energy_slack);
        if(lbad && !rbad) {
          keepbracket = 1;
        } else if(!lbad && rbad) {
          keepbracket = -1;
        } else if(tbracket.E-bracket.left.E > bracket.right.E-tbracket.E) {
          keepbracket = -1;
          if(lbad) bracket.bad_Edata=1;
        } else {
          if(rbad) bracket.bad_Edata=1;
          keepbracket = 1;
        }
      }
    }
    assert(keepbracket == -1 || keepbracket == 1);
    if(keepbracket == -1) {
      // Keep left pair
      bracket.right.Swap(tbracket);
      if(force_bestpt) bestpt.SetBracket(bracket.right);
    } else {
      // Keep right pair
      bracket.left.Swap(tbracket);
      if(force_bestpt) bestpt.SetBracket(bracket.left);
    }
  }

  // Update bestpt if necessary.  See NOTES VII, 4-Aug-2017, p164.
  if(!force_bestpt) {
    const Bracket* best_endpt = &(bracket.left);
    if(bracket.right.E < bracket.left.E - energy_slack ||
       (bracket.right.E < bracket.left.E + energy_slack &&
        fabs(bracket.right.Ep) < fabs(bracket.left.Ep))) {
      best_endpt = &(bracket.right);
    }
    bestpt.SetBracket(*best_endpt);
  }

  // If bestpt not changed, then bestpt.bracket->E will be zero.
  // Otherwise, update all bracket total energy values relative
  // to new bestpt.
  const OC_REAL8m bestpt_energy = bestpt.bracket->E;
  bracket.left.E  -= bestpt_energy;
  bracket.right.E -= bestpt_energy;
  if(tbracket.offset>=0) {
    // tbracket.offset will be -1 if tbracket is not set up, which
    // occurs if bracket.right wasn't set up yet for current line pass
    // on entry to this routine.
    tbracket.E -= bestpt_energy;
  }
  assert(bracket.left.E == 0 || bracket.right.E == 0);
}

OC_REAL8m Oxs_CGEvolve::EstimateQuadraticMinimum
(OC_REAL8m wgt,
 OC_REAL8m h,
 OC_REAL8m f0,    // f(0)
 OC_REAL8m fh,    // f(h)
 OC_REAL8m fp0,   // f'(0)
 OC_REAL8m fph)   // f'(h)
{ // Performs a least-squares fit of the input data to a quadratic,
  // and returns the position of the minimum of that quadratic.  If
  // the fit quadratic is actually linear or concave down (so there is
  // no minimum), then OC_REAL8m_MAX is returned.
  //   The import "wgt" adjusts the weighting between the direct f
  // data and the derivative data (more or less).  If wgt==0 then f0
  // and fh are ignored.  For details see NOTES VII, 18-Mar-2014, pp
  // 14-15, and in particular (15).

  //   Import wgt is assumed to be in the range [0,1], and h is
  // assumed>0.
  assert(0.0<=wgt && wgt<=1.0 && h>0.0);

  OC_REAL8m fdiff  = fh - f0;
  OC_REAL8m fpdiff = fph - fp0;

  // Coefficient on x^2 term is 0.5*h*fpdiff, so the quadratic has
  // a proper minimum iff fpdiff>0.
  if(fpdiff<=0.0) return OC_REAL8m_MAX;

  OC_REAL8m numer = wgt*(0.5*fpdiff - h*fdiff) - 4.*(1.-wgt)*fp0;
  OC_REAL8m denom = (wgt*h*h + 4.*(1.-wgt))*fpdiff;

  assert(fabs(h*numer)<=OC_REAL8m_MAX);

  OC_REAL8m offset = OC_REAL8m_MAX;
  if(denom>=1.0 || h*numer < OC_REAL8m_MAX*denom) {
    offset = numer/denom;
    offset *= h; // Doing the h multiply last may slightly improve
                /// numerics for wgt=0 case.
  } 

  return offset;
}


OC_REAL8m Oxs_CGEvolve::FindCubicMinimum
(OC_REAL8m Ediff, // f(1) - f(0)
 OC_REAL8m lEp,   // f'(0)
 OC_REAL8m rEp)   // f'(1)
{ // Returns location of minimum for cubic polynomial with given
  // function and derivative values.
  assert(lEp<0 && (rEp>0 || Ediff>=0));

  OC_REAL8m lambda = 0.5; // Default

  OC_REAL8m a = -2*Ediff +   lEp + rEp;
  OC_REAL8m b =  3*Ediff - 2*lEp - rEp;
  OC_REAL8m c = lEp;

  if(a==0.0) {
    // Quadratic
    if(b!=0.0) { // Safety check. b should be >= -c/2 > 0.
      lambda = -c/(2*b);
    }
  } else {
    OC_REAL8m disc = b*b - 3*a*c;
    if(disc<=0.0) disc=0.0;          // Safety check.  See NOTES II,
    else          disc = sqrt(disc); // 1-Sep-2001, p135.
    if(b>=0.) {
      if(fabs(c)>=b+disc) lambda = Nb_Signum(-c);
      else                   lambda = -c/(b + disc);
    } else {
      if(fabs(3*a)<=(-b + disc)) lambda = Nb_Signum(a);
      else                          lambda = (-b + disc)/(3*a);
    }
  }
  return lambda;
}

void Oxs_CGEvolve::FindBracketStep
(const Oxs_MinDriver* driver,
 const Oxs_SimState* oldstate,
 Oxs_Key<Oxs_SimState>& statekey)
{ // Takes one step; assumes right.offset >= left.offset and
  // minimum to be bracketed is to the right of right.offset

  // Note: If bracket.right hasn't been initialized then
  // bracket.right.offset will be -1 (see the end of SetBasePoint()).
  const OC_REAL8m bracket_right_offset = OC_MAX(0.0,bracket.right.offset);
  assert(bracket_right_offset>=bracket.left.offset && bracket.left.Ep<=0.0);

  if(bracket.left.Ep == 0.0) { // Special case handling
    bracket.min_bracketed=1;
    return;
  }

  ++bracket_count;
#ifndef NDEBUG
  const OC_REAL8m starting_offset = bracket_right_offset;
#endif

  OC_REAL8m offset = 0.0;
  const OC_REAL8m h = bracket_right_offset - bracket.left.offset;
  const OC_REAL8m maxoff
    = OC_MAX(bracket.left.offset,bracket.scaled_maxstep);
  const OC_REAL8m minoff
    = OC_MIN(OC_MAX(bracket.start_step,2*bracket_right_offset),maxoff);

  if(h<=0.0) {
    offset = minoff;
  } else {
    const OC_REAL8m wgt = (bracket.bad_Edata ? 0.0 : 0.5);
    /// Weighting for EstimateQuadraticMinimum
    offset = bracket.left.offset
      + 1.75*EstimateQuadraticMinimum(wgt,h,
                                      bracket.left.E,bracket.right.E,
                                      bracket.left.Ep,bracket.right.Ep);
    if(offset<minoff) offset = minoff;
    if(offset>maxoff) offset = maxoff;
  }

  // Zero span check
  if(offset <= bracket_right_offset) {
    // Proposed bracket span increase is floating point 0.
    if(bracket_right_offset>0) {
      offset = bracket_right_offset*(1+16*OC_REAL8m_EPSILON);
    } else {
      if(bracket.scaled_maxstep>0) {
	offset = bracket.scaled_maxstep;
      } else {
	// Punt
	offset = OC_REAL8m_EPSILON;
      }
    }
  }
  if(fabs(offset-bestpt.bracket->offset)*basept.direction_max_mag
     <16*OC_REAL8m_EPSILON) {
    // If LHS above is smaller that OC_REAL8m_EPSILON, and we assume
    // that the bestpt spins are unit vectors, then there is a
    // good chance that the proposed offset won't actually
    // change any of the discretized spins.  For bracketing
    // purposes, it shouldn't hurt to bump this up a bit.
    if(basept.direction_max_mag >= 0.5 ||
       OC_REAL8m_MAX*basept.direction_max_mag > 16*OC_REAL8m_EPSILON) {
      offset += 16*OC_REAL8m_EPSILON/basept.direction_max_mag;
    }
  }
  FillBracket(driver,offset,oldstate,statekey,extra_bracket);
  UpdateBrackets(extra_bracket,0);

  // Classify situation.  We always require leftpt have Ep<0,
  // so there are two possibilities:
  //  1) right.E>left.E+energy_slack or right.Ep>=0
  //          ==> Minimum is bracketed
  //  2) right.E<=left.E+energy_slack and right.Ep<0
  //          ==> Minimum not bracketed
  assert(0 <= bracket.left.offset && bracket.left.offset <= bracket.right.offset);
  OC_REAL8m energy_slack = EstimateEnergySlack();
  if((bracket.bad_Edata || bracket.right.E<=bracket.left.E+energy_slack)
     && bracket.right.Ep<0) {
    // Minimum not bracketed.
    bracket.min_bracketed=0; // Safety
    bracket.stop_span = 0.0;
    if(bracket.right.offset >= bracket.scaled_maxstep) {
      // Unable to bracket minimum inside allowed range.  Accept
      // bestpt, and mark line minimization complete.  NOTE: This may
      // cause solver to get stuck with bestpt at basept, if the
      // surface is very flat and noisy.  May want to include some
      // logic either here or in UpdateBrackets() to detect this
      // situation and move bestpt away from basept.
      bracket.min_bracketed=1;
      bracket.min_found = 1;
    }
  } else {
    // Minimum is bracketed.
    bracket.min_bracketed=1;
    bracket.stop_span = bracket.relative_minspan*bracket.right.offset;
    if(bracket.stop_span*basept.direction_max_mag < 4*OC_REAL8m_EPSILON) {
      bracket.stop_span = 4*OC_REAL8m_EPSILON/basept.direction_max_mag;
      /// This is a reasonable guess at the smallest variation in
      /// offset that leads to a detectable (i.e., discretizational)
      /// change in spins.
    }
  }

#ifndef NDEBUG
  assert(bracket.min_bracketed ||
         (bracket.right.offset>0 && bracket.right.offset>=2*starting_offset));
#endif
}

void Oxs_CGEvolve::FindLineMinimumStep
(const Oxs_MinDriver* driver,
 const Oxs_SimState* oldstate,
 Oxs_Key<Oxs_SimState>& statekey)
{ // Compresses bracket span.
  // Assumes coming in and forces on exit requirement that leftpt.Ep<0,
  // and either rightpt.E>leftpt.E or rightpt.Ep>=0.
  //
  // Note: A bracket is "weak" if rightpt.E>leftpt.E but rightpt.Ep<0.
  //       E data tends to be noiser than Ep data, and the concern with
  //       a weak bracket is that it might not really bracket a minimum.
  //       This routine tracks the number of weak bracket events for the
  //       current line direction, and if that number gets too high
  //       decides that the bracket is false.  In this case bad_Edata is
  //       set true and the bracket is kicked out for re-bracketing.
  //       (In this regard, note that the UpdateBrackets routine is
  //       designed so that if the outer bracket is strong (i.e.,
  //       rightpt.Ep<0), then the updated bracket pair will also be
  //       strong.)

  assert(0 <= bracket.left.offset && bracket.left.offset <= bracket.right.offset);
  assert(bracket.left.Ep<=0.0
	 && (bracket.right.E>bracket.left.E || bracket.right.Ep>=0));
  OC_REAL8m span = bracket.right.offset - bracket.left.offset;
  OC_REAL8m energy_slack = EstimateEnergySlack();
  OC_REAL8m nudge = OC_REAL8m_MAX/2;
  if(basept.direction_max_mag>=1.
     || OC_REAL8m_EPSILON<nudge*basept.direction_max_mag) {
    nudge = OC_REAL8m_EPSILON/basept.direction_max_mag;
    /// nudge is a upper bound on the smallest variation in offset
    /// that leads to a detectable (i.e., discretizational) change in
    /// spins.
    if(nudge >= 0.125*span
       && bracket.right.Ep > bracket.left.Ep*(1-OC_REAL8m_EPSILON)) {
      // If span is small relative to nudge, but there are discernible
      // differences in Ep at the two bracket endpoints, then the
      // nudge upper bound is presumably loose, so we allow additional
      // reduction to span.
      nudge = 0.125*span;
    }
  } else {
    // Degenerate case.  basept.direction_max_mag must be exactly
    // or very nearly zero.  Punt.
    nudge = span; // This will kick out in one of the following
    /// two stanzas.
  }

  // The first test below is an orthogonality check.
  //   The second test is a rough check to see if applying the
  // conjugation procedure to the gradient at this point will yield a
  // downhill direction.  This condition is trivially obtained if
  // bestpt.bracket->Ep is 0, so it should be always possible in
  // principle, ignoring discretization effects.  This check here is
  // not exactly the same as in SetBasePoint(), because a) it uses the
  // Fletcher-Reeves conjugation procedure, regardless of the selected
  // Conjugate_Method, and b) bestpt.bracket->Ep, used here, is
  // computed in GetRelativeEnergyAndDerivative() with a "scale_adj"
  // that differs from that used in SetBasePoint().
  //   The last test group are sanity checks, and the obsoleted span
  // size control.
  if(fabs(bestpt.bracket->Ep)
     < MU0 * bestpt.bracket->grad_norm * basept.direction_norm
       * bracket.angle_precision * (1+2*sum_error_estimate)
     && (bestpt.bracket->Ep==0 || bestpt.bracket->Ep > basept.Ep)
     && (bestpt.bracket->Ep==0 || span<=bracket.stop_span || nudge>=span))
  {
    // Minimum found
    bracket.min_found=1;
    bestpt.is_line_minimum=1; // Good gradient info
    bracket.last_min_reduction_ratio=0.0;
    bracket.next_to_last_min_reduction_ratio=0.0;
    assert(bracket.left.Ep<=0.0
           && (bracket.right.E>bracket.left.E || bracket.right.Ep>=0));
    return;
  }

  if(bracket.left.Ep>=0. || nudge>=span*(1-OC_REAL8m_EPSILON)
     || bracket.right.Ep==0.) {
    /// left.Ep==0 means minimum found. >0 is an error, but included
    ///    anyway for robustness.
    /// If nudge>=span, than further refinement doesn't appreciably
    ///    change any spins, so we might as well stop.
    /// right.Ep can have either sign, but regardless exact 0
    ///    indicate minimum.
    if(nudge>=span*(1-OC_REAL8m_EPSILON)) {
      // Don't reset CG procedure just because of this...
      bestpt.is_line_minimum=1;
    } else {
      bestpt.is_line_minimum=0; // Bad gradient info
    }
    bracket.min_found=1;
    bracket.last_min_reduction_ratio=0.0;
    bracket.next_to_last_min_reduction_ratio=0.0;
    assert(bracket.left.Ep<=0.0
	   && (bracket.right.E>bracket.left.E || bracket.right.Ep>=0));
    return;
  }

  // Test for total numerics loss
  if(BadPrecisionTest(bracket.left,bracket.right,energy_slack)) {
    bracket.min_found=1;
    bracket.bad_Edata=1;
    bracket.last_min_reduction_ratio=0.0;
    bracket.next_to_last_min_reduction_ratio=0.0;
    return;
  }

  // Pick test point.  Several methods are used:
  //  1) Cubic fit using E and Ep at bracket endpoints
  //  2) Quadratic fit on Ep using left, right and extra bracket data.
  //  3) Linear fit on Ep using left and right bracket data.
  //  4) Quadratic fit using E at bracket endpoints and Ep at left endpoint.
  // Option 1 is believed to be the most accurate, provided the E data
  // is reliable.  Options 2 and 3 are only used if right.Ep is
  // positive.  Additionally, option 2 relies on having an extra
  // bracket, which mostly doesn't happen because empirically the
  // bracketing stage completes in one try over 95% of the time, and
  // multiple line minimization tries are needed even less often.
  // Option 4 is rarely needed.
  //
  // If more than a few line minimization attempts have been made
  // along the given direction, then the code assumes that the E data
  // are bad and option 1 is thrown out of the mix, and the test point
  // is selected using the available remaining method with lowest
  // option number (i.e, 2 is preferred to 3 which is preferred to 4,
  // but 2 requires an extra bracket, and both 2 and 3 require
  // right.Ep>0).  Otherwise, test point locations are computed using
  // option 1 and also using the "best" of the other available
  // options.  The consensus test point is then a weighted average of
  // the two, with weight determined by the computed error estimate of
  // the cubic approximate.  (The cubic error estimate is computed
  // using the declared E density error estimate.)

  OC_REAL8m lambda = 0.5; // Relative offset
  OC_REAL8m lEp = bracket.left.Ep * span;
  OC_REAL8m rEp = bracket.right.Ep * span;
  OC_REAL8m Ediff = bracket.right.E - bracket.left.E;

  // Compute cubic estimate and approximate error using E and Ep.  See
  // NOTES II, 1-Sep-2001, p134-136. Also NOTES VII, 20-Jul-2017,
  // p156-163.
  OC_REAL8m cubic_testpt=0.5;
  OC_REAL8m cubic_error=1.0;
  if(rEp>0 || Ediff-energy_slack>=0.0) {
    cubic_testpt=FindCubicMinimum(Ediff,lEp,rEp);
    OC_REAL8m cubic_chk_a = FindCubicMinimum(Ediff+energy_slack,lEp,rEp);
    OC_REAL8m cubic_chk_b = FindCubicMinimum(Ediff-energy_slack,lEp,rEp);
    // Aside from rounding error, cubic_chk_b should be strictly greater than
    // cubic_chk_a --- see NOTES VII, 20-Jul-2017, p156-163.
    if(0<cubic_chk_a && cubic_chk_b<1.0) {
      cubic_error = fabs(cubic_chk_b-cubic_chk_a);
      // Absolute value is safety to handle rounding error
    }
  }
  const OC_REAL8m cubic_error_lower_bound = 0.125; // Empirically derived
  const OC_REAL8m cubic_error_upper_bound = 0.625; // values.
  assert(cubic_error_lower_bound < cubic_error_upper_bound);

  // Alternative test point computation
  OC_REAL8m alt_testpt = -1.0;
  if(cubic_error > cubic_error_lower_bound) {
    // Alternative test point only used if estimated cubic error is
    // larger than cubic_error_lower_bound.
    if(rEp>0.0) {
      // If we have a valid "extra_bracket", then use the Ep data
      // at the three bracket points to fit a quadratic.  Otherwise,
      // use a linear fit through the two main bracket Ep values.
      // See NOTES VII, 27-Mar-2014, p24 for details of quadratic fit.
      OC_REAL8m extra_bracket_size = -1;
      if(0.0<=extra_bracket.offset && extra_bracket.offset < bracket.left.offset*(1-OC_REAL8m_EPSILON)) {
        extra_bracket_size = bracket.left.offset - extra_bracket.offset;
      } else if(bracket.right.offset*(1+OC_REAL8m_EPSILON)<extra_bracket.offset) {
        extra_bracket_size = extra_bracket.offset - bracket.right.offset;
      }
      const OC_REAL8m extra_bracket_lower_bound = 0.05 * span; // Empirically derived
      const OC_REAL8m extra_bracket_upper_bound = 1.50 * span; // values.
      if(extra_bracket_lower_bound < extra_bracket_size &&
         extra_bracket_size < extra_bracket_upper_bound) {
        // Don't use extra bracket if it doesn't exist
        // (extra_bracket_size == -1), or if the size differs too much
        // from working bracket size.
        if(0.0<=extra_bracket.offset &&
           extra_bracket.offset < bracket.left.offset*(1-OC_REAL8m_EPSILON)) {
          // Extra bracket to left of main brackets
          OC_REAL8m lspan = bracket.left.offset - extra_bracket.offset;
          OC_REAL8m rspan = span;
          OC_REAL8m tspan = bracket.right.offset - extra_bracket.offset;

          OC_REAL8m A = extra_bracket.Ep;
          OC_REAL8m B = bracket.left.Ep;
          OC_REAL8m C = bracket.right.Ep;

          OC_REAL8m a = (lspan*C+rspan*A-tspan*B)*tspan/(lspan*rspan);
          OC_REAL8m b = (tspan*tspan*B-lspan*lspan*C-rspan*(tspan+lspan)*A)
                        /(lspan*rspan);
          OC_REAL8m c = A;

          OC_REAL8m disc = b*b-4*a*c;
          if(disc >= 0.0) {
            disc = sqrt(disc);
            if(b>=0.0) {
              alt_testpt = -2*c/(b+disc);
            } else {
              alt_testpt = 0.5*(disc-b)/a;
            }
            // The computed alt_testpt is across tspan.  Subsequent
            // code wants alt_testpt across [left.offset,right.offset].
            alt_testpt = (alt_testpt*tspan - lspan)/rspan;
          }
        } else if(bracket.right.offset*(1+OC_REAL8m_EPSILON)<extra_bracket.offset) {
          // Extra bracket to right of main brackets
          OC_REAL8m lspan = span;
          OC_REAL8m rspan = extra_bracket.offset - bracket.right.offset;
          OC_REAL8m tspan = extra_bracket.offset - bracket.left.offset;

          OC_REAL8m A = bracket.left.Ep;
          OC_REAL8m B = bracket.right.Ep;
          OC_REAL8m C = extra_bracket.Ep;

          OC_REAL8m a = (lspan*C+rspan*A-tspan*B)*tspan/(lspan*rspan);
          OC_REAL8m b = (tspan*tspan*B-lspan*lspan*C-rspan*(tspan+lspan)*A)
                        /(lspan*rspan);
          OC_REAL8m c = A;

          OC_REAL8m disc = b*b-4*a*c;
          if(disc >= 0.0) {
            disc = sqrt(disc);
            if(b>=0.0) {
              alt_testpt = -2*c/(b+disc);
            } else {
              alt_testpt = 0.5*(disc-b)/a;
            }
            // The computed alt_testpt is across tspan.  Subsequent
            // code wants alt_testpt across [left.offset,right.offset].
            alt_testpt *= tspan/lspan;
          }
        }
      }
      if(alt_testpt<=0.0 || 1.0<=alt_testpt) {
        // Fallback to linear fit
        OC_REAL8m Ep_diff = bracket.right.Ep - bracket.left.Ep;
        /// Since left.Ep<0,/ Ep_diff = |right.Ep| + |left.Ep|.
        alt_testpt = -bracket.left.Ep/Ep_diff;
        // Provided Ep_diff is a valid floating point number, then
        // alt_testpt should not overflow, and in fact must lay inside
        // [0,1].
      }
    } else {
      // Bad rightpt.Ep.  Guess at minpt using leftpt.E/Ep and rightpt.E
      const OC_REAL8m reduce_limit = 1./32.; // The
      /// situation rightpt.Ep<0 is suspicious, so limit reduction.
      OC_REAL8m numerator = -1.0 * lEp; // This is > 0.
      OC_REAL8m denominator = 2*(Ediff - lEp);
      // denominator must also be >0, because rightpt.E>=leftpt.E
      // if a minimum is bracketed with rightpt.Ep<0.
      denominator = fabs(denominator); // But play it safe, because
      /// checks below depend on denominator>0.
      if(numerator<reduce_limit*denominator) {
        alt_testpt = reduce_limit;
      } else if(numerator>(1-reduce_limit)*denominator) {
        alt_testpt = 1-reduce_limit;
      } else {
        alt_testpt = numerator/denominator;
      }
    }
    if(alt_testpt<0.0) {
      alt_testpt = 0;
    } else if(alt_testpt>1.0) {
      alt_testpt = 1;
    }
  }

  if(cubic_error <= cubic_error_lower_bound) {
    lambda = cubic_testpt;
  } else if(cubic_error >= cubic_error_upper_bound) {
    lambda = alt_testpt;
  } else {
    lambda = ((cubic_error_upper_bound - cubic_error)*cubic_testpt
              + (cubic_error-cubic_error_lower_bound)*alt_testpt)
           / (cubic_error_upper_bound - cubic_error_lower_bound);
  }

  // Curb reduction a bit to improve odds that minimum
  // lands in smaller interval
  const OC_REAL8m SAFETY = 1.0/(1024.0*1024.0);
  if(lambda<0.25)      lambda *= (1.0+SAFETY);
  else if(lambda>0.75) lambda *= (1.0-SAFETY);

  // Restrict reduction.
  const OC_REAL8m max_reduce_base=0.5; // Don't set this any larger than 0.707
  OC_REAL8m max_reduce=max_reduce_base;
  OC_REAL8m last_reduce = bracket.last_min_reduction_ratio;
  OC_REAL8m next_to_last_reduce = bracket.next_to_last_min_reduction_ratio;
  if(last_reduce<max_reduce) max_reduce=last_reduce;
  if(next_to_last_reduce<max_reduce) max_reduce=next_to_last_reduce;
  max_reduce *= max_reduce;
  if(span*max_reduce < OC_REAL8m_EPSILON * bracket.right.offset) {
    OC_REAL8m temp = OC_REAL8m_EPSILON * bracket.right.offset;
    if(temp<0.5*span) max_reduce = temp/span;
    else              max_reduce = 0.5;
  }
  if(/* span>256*nudge && */ span*max_reduce<nudge) max_reduce = nudge/span;
  /// There are some difficulties with the above nudge barrier control.
  /// In particular, even if the difference in offset values between
  /// test_offset and left/right.offset is less than OC_REAL8m_EPSILON,
  /// the difference in the spin configuration at the test point may
  /// be different than the spin configuration at the closer endpt
  /// because of roundoff errors.  Some tests have appeared to indicate
  /// that this sloppiness aids in the small mxHxm situation, so we
  /// restrict the nudge barrier operation to just those intervals that
  /// are wide relative to nudge.
  assert(max_reduce<=0.5);
  if(max_reduce>0.5) max_reduce=0.5; // Safety; This shouldn't trigger.
  if(lambda>0.5) {
    // Don't reverse the order of these two branches (lambda>0.5
    // vs. lambda<=0.5), because otherwise we run afoul of a bug in g++
    // 3.2.3 with -O3 optimization.  The bug manifests by effectively
    // removing this block, allowing the solver to get stuck in a
    // sequence of barely reducing intervals.
    if(lambda > 1.0-max_reduce) lambda = 1.0-max_reduce;
  } else {
    if(lambda < max_reduce) lambda = max_reduce;
  }

  OC_REAL8m test_offset = bracket.left.offset + lambda * span;
  if(test_offset<=bracket.left.offset
     || test_offset>=bracket.right.offset) {
    // Roundoff check
    lambda = 0.5;
    test_offset = 0.5 * (bracket.left.offset + bracket.right.offset);
    if(test_offset<=bracket.left.offset
       || test_offset>=bracket.right.offset) {
      // Interval width effectively machine 0; assume minimum reached
      bracket.min_found=1;
      bracket.last_min_reduction_ratio=0.0;
      bracket.next_to_last_min_reduction_ratio=0.0;
      return;
    }
  }

  ++line_minimum_count;
  FillBracket(driver,test_offset,oldstate,statekey,extra_bracket);

  // Determine which interval, [left,test] or [test,right] to keep.
  UpdateBrackets(extra_bracket,0);
  OC_REAL8m newspan = bracket.right.offset - bracket.left.offset;
  assert(bracket.next_to_last_min_reduction_ratio
         *bracket.last_min_reduction_ratio
         *newspan/span<1-max_reduce_base*max_reduce_base);
  bracket.next_to_last_min_reduction_ratio
    = bracket.last_min_reduction_ratio;
  bracket.last_min_reduction_ratio =  newspan/span;

  // Is minimum found?  The second test here is a rough check to
  // see if the conjugation procedure applied to mxHxm will yield
  // a downhill direction.  See note in the up-front check for
  // additional details.
  if(fabs(bestpt.bracket->Ep)
     < MU0 * bestpt.bracket->grad_norm * basept.direction_norm
       * bracket.angle_precision * (1+2*sum_error_estimate)
     && (bestpt.bracket->Ep==0 || bestpt.bracket->Ep > basept.Ep)
     && (bestpt.bracket->Ep==0 || span<=bracket.stop_span
         || (nudge>=span && bracket.right.Ep <= bracket.left.Ep)))
  {
    bracket.min_found=1;
    bestpt.is_line_minimum=1; // Here and in the up-front check at the
    /// top of this routine should be the only two places where
    /// is_line_minimum gets set true.
  } else if(bracket.right.Ep<0) {
    // Weak bracket
    if(++bracket.weak_bracket_count > 4) {
      bracket.bad_Edata = 1;
      bracket.weak_bracket_count = 0;
      bracket.min_bracketed = 0; // Redo bracketing stage
    }
  }

  assert(bracket.left.Ep<0.0
	 && (bracket.right.E>bracket.left.E || bracket.right.Ep>=0));
}

void Oxs_CGEvolve::AdjustDirection
(const Oxs_SimState* cstate,
 OC_REAL8m gamma,
 OC_REAL8m& maxmagsq,
 Nb_Xpfloat& work_normsumsq,
 Nb_Xpfloat& work_gradsumsq,
 Nb_Xpfloat& work_Ep)
{ //   Performs operation direction = torque + gamma*old_direction,
  // and returns some related values.  Intended solely for internal
  // use by Oxs_CGEvolve::SetBasePoint method.
  //   The threaded version of this code is _Oxs_CGEvolve_SetBasePoint_ThreadB.
  const Oxs_MeshValue<ThreeVector>& spin = cstate->spin;
  const Oxs_Mesh* mesh = cstate->mesh;
  const OC_INDEX size = mesh->Size();
  const Oxs_MeshValue<ThreeVector>& bestpt_mxHxm = bestpt.bracket->mxHxm;

  maxmagsq = 0.0;
  work_normsumsq = 0.0;
  work_gradsumsq = 0.0;
  work_Ep = 0.0;
  for(OC_INDEX i=0;i<size;++i) {
    const ThreeVector& pc = preconditioner_Ms_V[i];
    ThreeVector temp = bestpt_mxHxm[i];
    temp.x *= pc.x;  temp.y *= pc.y;  temp.z *= pc.z;
    /// Preconditioner has Ms[i]*mesh->Volume(i) built in.
    temp.Accum(gamma,basept.direction[i]);
    // Make temp orthogonal to spin[i].  If the angle between
    // temp and spin[i] is larger than about 45 degrees, then
    // it may be beneficial to divide by spin[i].MagSq(),
    // to offset effects of non-unit spin.  For small angles
    // it appears to be better to leave out this adjustment.
    // Or, better still, use temp -= (temp.m)m formulation
    // for large angles, w/o size correction.
    // NOTE: This code assumes |spin[i]| == 1.
    temp.Accum(-1*(temp*spin[i]),spin[i]);
    basept.direction[i] = temp;

    OC_REAL8m magsq = temp.MagSq();
    if(magsq>maxmagsq) maxmagsq = magsq;
    work_normsumsq.Accum(temp.x*temp.x);
    work_normsumsq.Accum(temp.y*temp.y);
    work_normsumsq.Accum(temp.z*temp.z);
    Nb_Xpfloat work_temp = temp.x * bestpt_mxHxm[i].x;
    work_temp.Accum(temp.y * bestpt_mxHxm[i].y);
    work_temp.Accum(temp.z * bestpt_mxHxm[i].z);
    work_temp *= Ms_V[i];
    work_Ep.Accum(work_temp);
    /// See mjd's NOTES II, 29-May-2002, p156.
    work_gradsumsq.Accum(bestpt_mxHxm[i].MagSq()*Ms_V[i]*Ms_V[i]);
  }
}

 void Oxs_CGEvolve::KludgeDirection(const Oxs_SimState* cstate,
                                    OC_REAL8m kludge,
                                    OC_REAL8m& maxmagsq,
                                    Nb_Xpfloat& work_normsumsq,
                                    Nb_Xpfloat& work_gradsumsq,
                                    Nb_Xpfloat& work_Ep)
 { // Similar to AdjustDirection, but performs
   //   new_direction = old_direction + kludge*torque
   // instead of
   //   new_direction = gamma*old_direction + torque
   // Intended solely for internal use by Oxs_CGEvolve::SetBasePoint
   // method.  See NOTES VII, 16_may-2014, p 26-29.
  const Oxs_MeshValue<ThreeVector>& spin = cstate->spin;
  const Oxs_MeshValue<ThreeVector>& bestpt_mxHxm = bestpt.bracket->mxHxm;

  // Per-thread workspace
  const int number_of_threads = Oc_GetMaxThreadCount();
  std::vector<OC_REAL8m> thread_maxmagsq(number_of_threads,0.0);
  Oc_AlignedVector<Nb_Xpfloat> thread_normsumsq(number_of_threads,0.0);
  Oc_AlignedVector<Nb_Xpfloat> thread_gradsumsq(number_of_threads,0.0);
  Oc_AlignedVector<Nb_Xpfloat> thread_Ep(number_of_threads,0.0);

  // Run threads
  Oxs_RunThreaded<ThreeVector,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
    (spin,
     [&](OC_INT4m thread_id,OC_INDEX istart,OC_INDEX istop) {

      OC_REAL8m thd_maxmagsq = thread_maxmagsq[thread_id]; // Thread local copy
      Nb_Xpfloat thd_normsumsq = 0.0;                      // Ditto
      Nb_Xpfloat thd_gradsumsq = 0.0;                      // Ditto
      Nb_Xpfloat thd_Ep        = 0.0;                      // Ditto

      for(OC_INDEX i=istart;i<istop;++i) {
        const ThreeVector& pc = preconditioner_Ms_V[i];
        ThreeVector temp = bestpt_mxHxm[i];
        temp.x *= kludge*pc.x;  temp.y *= kludge*pc.y;  temp.z *= kludge*pc.z;
        /// Preconditioner has Ms[i]*mesh->Volume(i) built in.
        temp += basept.direction[i];
        // Make temp orthogonal to spin[i].  If the angle between
        // temp and spin[i] is larger than about 45 degrees, then
        // it may be beneficial to divide by spin[i].MagSq(),
        // to offset effects of non-unit spin.  For small angles
        // it appears to be better to leave out this adjustment.
        // Or, better still, use temp -= (temp.m)m formulation
        // for large angles, w/o size correction.
        // NOTE: This code assumes |spin[i]| == 1.
        temp.Accum(-1*(temp*spin[i]),spin[i]);
        basept.direction[i] = temp;

        OC_REAL8m magsq = temp.MagSq();
        if(magsq>thd_maxmagsq) thd_maxmagsq = magsq;
        thd_normsumsq.Accum(temp.x*temp.x);
        thd_normsumsq.Accum(temp.y*temp.y);
        thd_normsumsq.Accum(temp.z*temp.z);
        Nb_Xpfloat work_temp = temp.x * bestpt_mxHxm[i].x;
        work_temp.Accum(temp.y * bestpt_mxHxm[i].y);
        work_temp.Accum(temp.z * bestpt_mxHxm[i].z);
        work_temp *= Ms_V[i];
        thd_Ep.Accum(work_temp);
        /// See mjd's NOTES II, 29-May-2002, p156.
        thd_gradsumsq.Accum(bestpt_mxHxm[i].MagSq()*Ms_V[i]*Ms_V[i]);
      }
      thread_maxmagsq[thread_id] = thd_maxmagsq;
      thread_normsumsq[thread_id] += thd_normsumsq;
      thread_gradsumsq[thread_id] += thd_gradsumsq;
      thread_Ep[thread_id]        += thd_Ep;
    });

  // Collect thread results
  for(int i=1;i<number_of_threads;++i) {
    if(thread_maxmagsq[0]<thread_maxmagsq[i]) {
      thread_maxmagsq[0] = thread_maxmagsq[i];
    }
    thread_normsumsq[0] += thread_normsumsq[i];
    thread_gradsumsq[0] += thread_gradsumsq[i];
    thread_Ep[0] += thread_Ep[i];
  }
  maxmagsq = thread_maxmagsq[0];
  work_normsumsq = thread_normsumsq[0];
  work_gradsumsq = thread_gradsumsq[0];
  work_Ep = thread_Ep[0];
}

void Oxs_CGEvolve::SetBasePoint(Oxs_ConstKey<Oxs_SimState> cstate_key)
{
  const Oxs_SimState* cstate = cstate_key.GetPtr();
  if(cstate->Id() == basept.id && basept.valid) {
    return;  // Already set
  }

  if(preconditioner_mesh_id !=  cstate->mesh->Id()) {
    InitializePreconditioner(cstate);
  }

  ++cycle_count;
  ++cycle_sub_count;
  cstate->AddDerivedData("Cycle count",
                         OC_REAL8m(cycle_count));
  // cycle_sub_count will change if conjugate direction is reset to
  // gradient below.  So wait to set "Cycle sub count" derived data.

  // Some convenience variables
  const Oxs_Mesh* mesh = cstate->mesh;
  OC_REAL8m Ep = 0.0;
  // const OC_REAL8m last_scaling = basept.direction_max_mag;
  OC_REAL8m next_step_guess = 0.0;
  if(bestpt.bracket != 0) {
    // If bestpt.bracket is 0 then bestpt hasn't been set up yet.
    next_step_guess = bestpt.bracket->offset;
    if(bestpt.is_line_minimum && bracket.left.Ep<0 && bracket.right.Ep>0) {
      // Compute improved estimate of minimum, via linear fit to last Ep data
      next_step_guess = (bracket.right.Ep*bracket.left.offset
                         - bracket.left.Ep*bracket.right.offset)
        / (bracket.right.Ep - bracket.left.Ep);
    }
  }
  OC_BOOL last_step_is_minimum = bestpt.is_line_minimum;

  const OC_REAL8m last_direction_norm = basept.direction_norm;

  // cstate and bestpt should be same
  if(bestpt.bracket && bestpt.bracket->key.SameState(cstate_key)) {
    // Move bestpt to bracket.left
    assert(bestpt.bracket == &(bracket.left) || bestpt.bracket == &(bracket.right));
    bracket.left.Swap(*const_cast<Oxs_CGEvolve::Bracket*>(bestpt.bracket));
    bestpt.bracket = &(bracket.left);
  } else {
    // Fill left bracket from cstate and point bestpt to bracket.left
    bracket.left.key = cstate_key;
    bracket.left.key.GetReadReference();
    GetEnergyAndmxHxm(cstate,bracket.left.energy,bracket.left.mxHxm,NULL);
    /// Should next_step_guess be set to 0 in this case???
  }
  OC_REAL8m edee;
  if(!(cstate->GetDerivedData("Energy density error estimate",edee))) {
    throw Oxs_ExtError(this,
              "Missing \"Energy density error estimate\" data"
              " in Oxs_CGEvolve::SetBasePoint()."
              " Programming error?");
  }
  bracket.left.offset  = 0.;
  bracket.left.E       = 0.;
  bracket.left.E_error_estimate = edee
    * mesh->TotalVolume()/sqrt(2.0*OC_REAL8m(mesh->Size()));
  // See NOTES VII, 5-7 July 2017, p 154-155.  
  bestpt.SetBracket(bracket.left);

  // Determine new direction
  const Oxs_MeshValue<ThreeVector>& bestpt_mxHxm = bestpt.bracket->mxHxm;
  if(!basept.valid
     || cstate->stage_number!=basept.stage
     || cycle_sub_count >= gradient_reset_count
     || basept.direction.Size()!=bestpt.bracket->mxHxm.Size()
     || !last_step_is_minimum) {
    basept.valid=0;
    if(basept.method == Basept_Data::POLAK_RIBIERE) {
      basept.mxHxm = bestpt_mxHxm;  // Element-wise copy
    }
  } else {
    // Use conjugate gradient as new direction, where
    //              n = cycle count
    //              k = cell index
    //         g_n[k] = V[k]*Ms[k]*mxHxm_n[k]
    //    gamma = (g_n^T P g_n)/(g_(n-1)^T P g_(n-1))      (Fletcher-Reeves)
    // or gamma = (g_n^T P (g_n-g_(n-1))/(g_(n-1)^T P g_(n-1)) (Polak-Ribiere)
    //    Direction_n = (P g_n)+gamma*direction_(n-1).
    // where matrix P is the preconditioner.
    // Note that the g_(n-1) (or equivalently, mxHxm[n-1]) array is only
    // needed for the Polak-Ribiere method.
    // If using the Fletcher-Reeves method, only the g_(n-1)^T P g_(n-1)
    // scalar value needs to be saved between cycles, reducing memory
    // requirements.
    //   We also perform an additional correction step to Direction_n,
    // making it orthogonal to m[k] (=m_n[k]) at each k.  The wisdom of
    // this is uncertain.
    //   See NOTES IV, 27-Dec-2004, pp. 1-3 for details, also NOTES II,
    // 29-May-2002, p. 156, and NOTES III, 30-Jan-2003, p. 35.


#if REPORT_TIME_CGDEVEL
    TS(timer[3].Start()); /**/ // asdf
#endif // REPORT_TIME_CGDEVEL

    OC_REAL8m gamma, new_g_sum_sq;
#if !OOMMF_THREADS
    const OC_INDEX size = mesh->Size();
    OC_INDEX i;
    gamma = new_g_sum_sq = 0.0;
    if(basept.method != Basept_Data::POLAK_RIBIERE) {
      // Fletcher-Reeves method
      Nb_Xpfloat work_sum = 0.0;
      for(i=0;i<size;i++) {
        // Note: preconditioner has (Ms[i]*mesh->Volume(i))^2 built in.
        work_sum.Accum(bestpt_mxHxm[i].x*bestpt_mxHxm[i].x
                       *preconditioner_Ms2_V2[i].x);
        work_sum.Accum(bestpt_mxHxm[i].y*bestpt_mxHxm[i].y
                       *preconditioner_Ms2_V2[i].y);
        work_sum.Accum(bestpt_mxHxm[i].z*bestpt_mxHxm[i].z
                       *preconditioner_Ms2_V2[i].z);
      }
      gamma = new_g_sum_sq = work_sum.GetValue();
    } else {
      // Polak-Ribiere method
      Nb_Xpfloat work_sum = 0.0;
      Nb_Xpfloat work_gamma = 0.0;
      for(i=0;i<size;i++) {
	ThreeVector temp = bestpt_mxHxm[i];
        const ThreeVector& scale = preconditioner_Ms2_V2[i];
        // Note: preconditioner has (Ms[i]*mesh->Volume(i))^2 built in.
        work_sum.Accum(temp.x*temp.x*scale.x);
        work_sum.Accum(temp.y*temp.y*scale.y);
        work_sum.Accum(temp.z*temp.z*scale.z);
#if 0
	temp -= basept.mxHxm[i];  // Polak-Ribiere adjustment
	gamma += temp.x*bestpt_mxHxm[i].x*scale.x
               + temp.y*bestpt_mxHxm[i].y*scale.y
               + temp.z*bestpt_mxHxm[i].z*scale.z;
#else
        work_gamma.Accum((temp.x-basept.mxHxm[i].x)*temp.x*scale.x);
        work_gamma.Accum((temp.y-basept.mxHxm[i].y)*temp.y*scale.y);
        work_gamma.Accum((temp.z-basept.mxHxm[i].z)*temp.z*scale.z);
#endif
        basept.mxHxm[i] = bestpt_mxHxm[i];
      }
      new_g_sum_sq = work_sum.GetValue();
      gamma = work_gamma.GetValue();
    }
    gamma /= basept.g_sum_sq;
#else // !OOMMF_THREADS
    {
      _Oxs_CGEvolve_SetBasePoint_ThreadA threadA;
      const int thread_count = Oc_GetMaxThreadCount();
      _Oxs_CGEvolve_SetBasePoint_ThreadA::Init(thread_count,
                                               bestpt_mxHxm.GetArrayBlock());
      threadA.preconditioner_Ms2_V2 = &preconditioner_Ms2_V2;
      threadA.bestpt_mxHxm = &(bestpt_mxHxm);
      threadA.basept_mxHxm = &(basept.mxHxm);
      if(basept.method == Basept_Data::POLAK_RIBIERE) {
        threadA.method = _Oxs_CGEvolve_SetBasePoint_ThreadA::POLAK_RIBIERE;
      } else {
        threadA.method = _Oxs_CGEvolve_SetBasePoint_ThreadA::FLETCHER_REEVES;
      }
      Oxs_ThreadTree threadtree;
      threadtree.LaunchTree(threadA,0);

      // Store results
      for(int i=1;i<thread_count;++i) {
        _Oxs_CGEvolve_SetBasePoint_ThreadA::gamma_sum[0] += 
          _Oxs_CGEvolve_SetBasePoint_ThreadA::gamma_sum[i];
        _Oxs_CGEvolve_SetBasePoint_ThreadA::g_sum_sq[0] +=
          _Oxs_CGEvolve_SetBasePoint_ThreadA::g_sum_sq[i];
      }
      gamma  = _Oxs_CGEvolve_SetBasePoint_ThreadA::gamma_sum[0].GetValue();
      gamma /= basept.g_sum_sq;
      new_g_sum_sq = _Oxs_CGEvolve_SetBasePoint_ThreadA::g_sum_sq[0].GetValue();
    }
#endif // !OOMMF_THREADS

#if REPORT_TIME_CGDEVEL
    TS(timer[3].Stop()); /**/
    if(basept.method != Basept_Data::POLAK_RIBIERE) {
      timer_counts[3].Increment(timer[3],mesh->Size()*(2*sizeof(ThreeVector)));
    } else {
      timer_counts[3].Increment(timer[3],mesh->Size()*(4*sizeof(ThreeVector)));
    }
#endif // REPORT_TIME_CGDEVEL

#if REPORT_TIME_CGDEVEL
    TS(timer[5].Start()); /**/ // asdf
#endif // REPORT_TIME_CGDEVEL

    OC_REAL8m maxmagsq, normsumsq;
#if !OOMMF_THREADS
    {
      Nb_Xpfloat work_normsumsq,work_Ep,work_gradsumsq;
      AdjustDirection(cstate,gamma,maxmagsq,work_normsumsq,
                      work_gradsumsq,work_Ep);
      normsumsq = work_normsumsq.GetValue();
      bracket.left.grad_norm = sqrt(work_gradsumsq.GetValue());
      Ep = work_Ep.GetValue();
    }
#else // OOMMF_THREADS /////////////////////////////////////////////////
    {
      const Oxs_MeshValue<ThreeVector>& spin = cstate->spin;
      _Oxs_CGEvolve_SetBasePoint_ThreadB threadB;
      const int thread_count = Oc_GetMaxThreadCount();
      _Oxs_CGEvolve_SetBasePoint_ThreadB::Init(thread_count,
                                               Ms_V.GetArrayBlock());
      threadB.preconditioner_Ms_V = &preconditioner_Ms_V;
      threadB.Ms_V = &Ms_V;
      threadB.spin = &spin;
      threadB.bestpt_mxHxm = &(bestpt_mxHxm);
      threadB.basept_direction = &(basept.direction);
      threadB.gamma = gamma;

      Oxs_ThreadTree threadtree;
      threadtree.LaunchTree(threadB,0);

      // Accumulate results
      for(int i=1;i<thread_count;++i) {
        if(_Oxs_CGEvolve_SetBasePoint_ThreadB::maxmagsq[0]
           < _Oxs_CGEvolve_SetBasePoint_ThreadB::maxmagsq[i]) {
          _Oxs_CGEvolve_SetBasePoint_ThreadB::maxmagsq[0]
            = _Oxs_CGEvolve_SetBasePoint_ThreadB::maxmagsq[i];
        }
        _Oxs_CGEvolve_SetBasePoint_ThreadB::normsumsq[0]
          += _Oxs_CGEvolve_SetBasePoint_ThreadB::normsumsq[i];
        _Oxs_CGEvolve_SetBasePoint_ThreadB::Ep[0]
          += _Oxs_CGEvolve_SetBasePoint_ThreadB::Ep[i];
        _Oxs_CGEvolve_SetBasePoint_ThreadB::gradsumsq[0]
          += _Oxs_CGEvolve_SetBasePoint_ThreadB::gradsumsq[i];
      }
      maxmagsq = _Oxs_CGEvolve_SetBasePoint_ThreadB::maxmagsq[0];
      normsumsq = _Oxs_CGEvolve_SetBasePoint_ThreadB::normsumsq[0].GetValue();
      Ep = _Oxs_CGEvolve_SetBasePoint_ThreadB::Ep[0].GetValue();
      bracket.left.grad_norm
        = sqrt(_Oxs_CGEvolve_SetBasePoint_ThreadB::gradsumsq[0].GetValue());
    }
#endif // OOMMF_THREADS ////////////////////////////////////////////////

#if REPORT_TIME_CGDEVEL
    TS(timer[5].Stop()); /**/
    timer_counts[5].Increment(timer[5],
                mesh->Size()*(sizeof(OC_REAL8m)+5*sizeof(ThreeVector)));
#endif // REPORT_TIME_CGDEVEL

    if(bestpt.bracket->grad_norm
       < gamma*last_direction_norm*gradient_reset_angle_cotangent) {
      // If new gradient were exactly orthogonal to last direction
      // (as it would be if minimization results were exact), then
      // new direction (not including the preconditioner, if any)
      // is less than gradient_reset_angle from last direction.
      // Count this as evidence towards the need for a CG reset.
      gradient_reset_score = gradient_reset_wgt*gradient_reset_score
        + (1-gradient_reset_wgt)*1.0;
    } else {
      gradient_reset_score *= gradient_reset_wgt;
    }

    if(gradient_reset_score>=gradient_reset_trigger) {
      basept.valid=0;
    } else {
      // Kludge for case direction*torque<=0
      if(Ep <= kludge_adjust_angle_cos * sqrt(normsumsq)
         * bestpt.bracket->grad_norm * (1+8*OC_REAL8m_EPSILON)) {
        // See NOTES VII, 16-May-2014, p 26-29.
        OC_REAL8m Tsq = bestpt.bracket->grad_norm * bestpt.bracket->grad_norm;
        OC_REAL8m betasq = kludge_adjust_angle_cos*kludge_adjust_angle_cos
          * (1+8*OC_REAL8m_EPSILON);
        OC_REAL8m A = (1-betasq)*Tsq*Tsq;
        OC_REAL8m B = 2*(1-betasq)*Ep*Tsq;
        OC_REAL8m C = Ep*Ep-betasq*normsumsq*Tsq;
        OC_REAL8m Delta = B*B - 4*A*C;
        OC_REAL8m alpha = 0.0;
        if(Delta>0.0) {
          if(B>=0.0) {
            alpha = -2*C/(sqrt(Delta)+B);
          } else {
            alpha = (sqrt(Delta)-B)/(2*A);
          }
        } else {
          // According to (9) in NOTES VII, 16-May-2014, p 27, Delta
          // should be strictly positive unless P and T are parallel,
          // in which case Delta is zero.  What to do in case Delta
          // evaluates under floating-point computations to <= 0?  As
          // a first guess just force Delta to 0 and throw in some fudge
          if(B>0.0) {
            alpha = -2*C/B;
          } else {
            alpha = -B/(2*A);
          }
          alpha *= (1.0 + 1024*OC_REAL8m_EPSILON);
        }
        Nb_Xpfloat work_normsumsq,work_Ep,work_gradsumsq;
        KludgeDirection(cstate,alpha,maxmagsq,work_normsumsq,
                        work_gradsumsq,work_Ep);
        normsumsq = work_normsumsq.GetValue();
        bracket.left.grad_norm = sqrt(work_gradsumsq.GetValue());
        Ep = work_Ep.GetValue();
      }
      basept.direction_max_mag = sqrt(maxmagsq);
      basept.direction_norm = sqrt(normsumsq);
      basept.g_sum_sq = new_g_sum_sq;
      Ep *= -MU0; // See mjd's NOTES II, 29-May-2002, p156.
      basept.Ep = bracket.left.Ep = Ep;
      if(Ep < 0.0) { // Safety
        basept.valid=1;
      } else {
        basept.valid=0;
      }
    }
  }

  if(!basept.valid) {
#if REPORT_TIME_CGDEVEL
    TS(timer[6].Start()); /**/
#endif // REPORT_TIME_CGDEVEL
    // Use gradient as new direction
    basept.direction.AdjustSize(mesh);
    cycle_sub_count=0;
    ++conjugate_cycle_count;
    gradient_reset_score=0.0;
#if !OOMMF_THREADS
    const OC_INDEX size = mesh->Size();
    OC_REAL8m maxmagsq = 0.0;
    OC_REAL8m sumsq = 0.0;
    Nb_Xpfloat work_Ep = 0.0;
    Nb_Xpfloat work_sumsq = 0.0;
    Nb_Xpfloat work_gradsumsq = 0.0;
    for(OC_INDEX i=0;i<size;i++) {
      basept.direction[i] = bestpt_mxHxm[i];
      basept.direction[i].x *= preconditioner_Ms_V[i].x;
      basept.direction[i].y *= preconditioner_Ms_V[i].y;
      basept.direction[i].z *= preconditioner_Ms_V[i].z;
      // Note: Preconditioner has Ms[i]*mesh->Volume(i) built in.

      Nb_Xpfloat work_temp = basept.direction[i].x * bestpt_mxHxm[i].x;
      work_temp.Accum(basept.direction[i].y * bestpt_mxHxm[i].y);
      work_temp.Accum(basept.direction[i].z * bestpt_mxHxm[i].z);
      work_temp *= Ms_V[i];
      work_Ep.Accum(work_temp);
      work_gradsumsq.Accum(bestpt_mxHxm[i].MagSq()*Ms_V[i]*Ms_V[i]);

      OC_REAL8m magsq = basept.direction[i].MagSq();
      if(magsq>maxmagsq) maxmagsq = magsq;
      work_sumsq.Accum(basept.direction[i].x*basept.direction[i].x);
      work_sumsq.Accum(basept.direction[i].y*basept.direction[i].y);
      work_sumsq.Accum(basept.direction[i].z*basept.direction[i].z);
      /// See mjd's NOTES II, 29-May-2002, p156.
    }
    sumsq = work_sumsq.GetValue();
    Ep = work_Ep.GetValue();
    bracket.left.grad_norm = sqrt(work_gradsumsq.GetValue());
#else // !OOMMF_THREADS
    OC_REAL8m maxmagsq, sumsq;
    {
      _Oxs_CGEvolve_SetBasePoint_ThreadC threadC;
      const int thread_count = Oc_GetMaxThreadCount();
      _Oxs_CGEvolve_SetBasePoint_ThreadC::Init(thread_count,
                                               Ms_V.GetArrayBlock());
      threadC.preconditioner_Ms_V = &preconditioner_Ms_V;
      threadC.Ms_V = &Ms_V;
      threadC.bestpt_mxHxm = &(bestpt_mxHxm);
      threadC.basept_direction = &(basept.direction);

      Oxs_ThreadTree threadtree;
      threadtree.LaunchTree(threadC,0);

      // Accumulate results
      for(int i=1;i<thread_count;++i) {
        if(_Oxs_CGEvolve_SetBasePoint_ThreadC::maxmagsq[0]
           < _Oxs_CGEvolve_SetBasePoint_ThreadC::maxmagsq[i]) {
          _Oxs_CGEvolve_SetBasePoint_ThreadC::maxmagsq[0]
            = _Oxs_CGEvolve_SetBasePoint_ThreadC::maxmagsq[i];
        }
        _Oxs_CGEvolve_SetBasePoint_ThreadC::sumsq[0]
          += _Oxs_CGEvolve_SetBasePoint_ThreadC::sumsq[i];
        _Oxs_CGEvolve_SetBasePoint_ThreadC::Ep[0]
          += _Oxs_CGEvolve_SetBasePoint_ThreadC::Ep[i];
        _Oxs_CGEvolve_SetBasePoint_ThreadC::gradsumsq[0]
          += _Oxs_CGEvolve_SetBasePoint_ThreadC::gradsumsq[i];
      }
      maxmagsq = _Oxs_CGEvolve_SetBasePoint_ThreadC::maxmagsq[0];
      sumsq = _Oxs_CGEvolve_SetBasePoint_ThreadC::sumsq[0].GetValue();
      Ep = _Oxs_CGEvolve_SetBasePoint_ThreadC::Ep[0].GetValue();
      bracket.left.grad_norm
        = sqrt(_Oxs_CGEvolve_SetBasePoint_ThreadC::gradsumsq[0].GetValue());
    }
#endif // !OOMMF_THREADS

    basept.direction_max_mag = sqrt(maxmagsq);
    Nb_NOP(basept.direction_max_mag); // Workaround for icpc optimization bug
    basept.g_sum_sq = sumsq;
    basept.direction_norm = sqrt(sumsq);
    Ep *= -MU0; // See mjd's NOTES II, 29-May-2002, p156.
    basept.Ep = bracket.left.Ep = Ep;
    basept.valid=1;

#if REPORT_TIME_CGDEVEL
    TS(timer[6].Stop()); /**/
    timer_counts[6].Increment(timer[6],
                mesh->Size()*(sizeof(OC_REAL8m)+3*sizeof(ThreeVector)));
#endif // REPORT_TIME_CGDEVEL
  }

  cstate->AddDerivedData("Cycle sub count",
                         OC_REAL8m(cycle_sub_count));
  cstate->AddDerivedData("Conjugate cycle count",
                         OC_REAL8m(conjugate_cycle_count));

  // Fill remaining basept fields
  basept.id = cstate->Id();
  basept.stage = cstate->stage_number;
  if(!cstate->GetDerivedData("Total energy",basept.total_energy)) {
      throw Oxs_ExtError(this,
        "Missing \"Total energy\" data in Oxs_CGEvolve::SetBasePoint()."
	" Programming error?");
  }

  // Initialize bracket data
  bracket.min_bracketed=0;
  bracket.min_found=0;
  bracket.bad_Edata=0;
  bracket.weak_bracket_count=0;

  if(basept.direction_max_mag>=1.0
     || bracket.maxstep<basept.direction_max_mag*OC_REAL8m_MAX) {
    bracket.scaled_minstep=bracket.minstep/basept.direction_max_mag;
    bracket.scaled_maxstep=bracket.maxstep/basept.direction_max_mag;
  } else {
    // Safety
    if(bracket.maxstep>0.0) {
      bracket.scaled_maxstep = 0.5*OC_REAL8m_MAX;
      bracket.scaled_minstep
	= bracket.scaled_maxstep*(bracket.minstep/bracket.maxstep);
    } else {
      bracket.scaled_maxstep = 0.0;
      bracket.scaled_minstep = 0.0;
    }
  }

  // Size initial step to match that from previous
  // line minimization, if applicable
  bracket.start_step = bracket.scaled_minstep;
  if(next_step_guess>0.0) {
    OC_REAL8m scaling_ratio = 1.25;
    // The 1.25 adjusment increases odds that first step will
    // bracket the minimum.  This is mostly helpful in the large
    // |mxHxm| regime, where line minimization largely doesn't come
    // into play.
    if(next_step_guess<OC_REAL8m_MAX/scaling_ratio) {
      bracket.start_step = next_step_guess*scaling_ratio;
    }
    if(bracket.start_step>bracket.scaled_maxstep) {
      bracket.start_step = bracket.scaled_maxstep;
    }
  }

  // To force maximal restriction on initial bracket interval
  // reduction (see FindLineMinimumStep()), set both
  // minimum_reduction_ratio's to 1.  Moderately conservative
  // values are last_mrr=0.5, next_to_last_mrr=0.25.  Or set
  // both values to 0 to allow FindLineMinimumStep complete
  // freedom on first two steps.
  bracket.last_min_reduction_ratio=1./16.;
  bracket.next_to_last_min_reduction_ratio=1./256.;
  bracket.left.Ep      = Ep;
  /// Other members of bracket.left set above
  bracket.right.Clear();
  extra_bracket.Clear();
  bestpt.is_line_minimum = 0;
}

void Oxs_CGEvolve::RuffleBasePoint
(const Oxs_MinDriver* driver,
 const Oxs_SimState* oldstate,
 Oxs_Key<Oxs_SimState>& statekey)
{ // Tweak all the spins in oldstate by epsilon, storing result in
  // statekey.  The new state is then set as the new base point.  It is
  // difficult to write a regression test for this bit of code because
  // the nudge amount depends on sizeof(OC_REAL8m) --- specifically
  // OC_REAL8m_EPSILON.
  try {
    InitializeWorkState(driver,oldstate,statekey);
    Oxs_SimState& workstate
      = statekey.GetWriteReference(); // Write lock
    Oxs_MeshValue<ThreeVector>& workspin = workstate.spin;
    const Oxs_MeshValue<ThreeVector>& oldspin = oldstate->spin;
    Oxs_RunThreaded<ThreeVector,std::function<void(OC_INT4m,OC_INDEX,OC_INDEX)> >
      (workspin,
       [&](OC_INT4m thread_number,OC_INDEX istart,OC_INDEX istop) {
        // Note: Results will vary run-to-run, depending on number of
        // threads and also the order in which the threads get random
        // seed from global random number generator (Oc_Random::Random()).
        Oc_RandomState rs; // Thread local random number generator
        Oc_Random::Srandom(rs,
                    static_cast<OC_UINT4m>(Oc_Random::Random())+thread_number);
        for(OC_INDEX i=istart;i<istop;++i) {
          // The tweaking is not uniform on the sphere, but do we care?
          workspin[i].x = oldspin[i].x
            + (2*Oc_UnifRand(rs)-1)*OC_REAL8m_EPSILON;
          workspin[i].y = oldspin[i].y
            + (2*Oc_UnifRand(rs)-1)*OC_REAL8m_EPSILON;
          workspin[i].z = oldspin[i].z
            + (2*Oc_UnifRand(rs)-1)*OC_REAL8m_EPSILON;
          workspin[i].MakeUnit();
        }
      });
    workstate.iteration_count = oldstate->iteration_count + 1;
    workstate.stage_iteration_count
      = oldstate->stage_iteration_count + 1;
    statekey.GetReadReference(); // Release write lock
  }
  catch (...) {
    statekey.GetReadReference(); // Release write lock
    throw;
  }
  basept.valid=0;
  SetBasePoint(statekey);
}

void Oxs_CGEvolve::NudgeBestpt
(const Oxs_MinDriver* driver,
 const Oxs_SimState* oldstate,
 Oxs_Key<Oxs_SimState>& statekey)
{ // Line minimization didn't change state, which is
  // a problem because in this case the next pass of
  // the base-direction code may re-generate the same
  // direction, in which case the algorithm gets
  // stuck.  So, we nudge.
  assert(bracket.left.Ep<=0.0
	 && (bracket.right.E>bracket.left.E || bracket.right.Ep>=0));
  {
    char warnbuf[1024];
    Oc_Snprintf(warnbuf,sizeof(warnbuf),
               "Failure in conjugate-gradient algorithm:"
               " no movement in line minimization."
                "  Nudging base point to attempt recovery."
                "  (cycle_sub_count=%d)",
                (int)cycle_sub_count);
    static Oxs_WarningMessage foo(3);
    foo.Send(revision_info,OC_STRINGIFY(__LINE__),warnbuf);
  }
  OC_REAL8m nudge = OC_REAL8m_MAX/2;
  if(basept.direction_max_mag<1.
     && nudge*basept.direction_max_mag<4*OC_REAL8m_EPSILON) {
    // Degenerate case.  basept.direction_max_mag must be exactly or
    // very nearly zero.  Twiddle each spin by epsilon and punt.
    RuffleBasePoint(driver,oldstate,statekey);
    return;
  }

  nudge = 4*OC_REAL8m_EPSILON/basept.direction_max_mag;
  /// nudge is a reasonable guess at the smallest variation in
  /// offset that leads to a detectable (i.e., discretizational)
  /// change in spins.
  OC_REAL8m test_offset = 0.5;
  if(bracket.right.Ep>0.) {
    if(-bracket.left.Ep
       < test_offset*(bracket.right.Ep-bracket.left.Ep)) {
      test_offset
        = -bracket.left.Ep/(bracket.right.Ep-bracket.left.Ep);
    }
  } else {
    // Ep data is not informative
    test_offset = 0.0;  // This will get bumped up by
    /// "nudge" test below.
  }
  test_offset *= bracket.right.offset/2.0;  // Scale best guess of
  // minima to interval, and then reduce to err on the safe side.

  // Make sure offset can be felt.
  if(test_offset<nudge) test_offset=nudge;
  ++line_minimum_count;
  FillBracket(driver,test_offset,oldstate,statekey,extra_bracket);
  UpdateBrackets(extra_bracket,1);
  // One may want to force testpt acceptance only if
  // extra_bracket.E<energy_slack, but if this doesn't hold
  // then one needs a backup plan.
}


OC_BOOL Oxs_CGEvolve::InitNewStage
(const Oxs_MinDriver* /* driver */,
 Oxs_ConstKey<Oxs_SimState> state,
 Oxs_ConstKey<Oxs_SimState> /* prevstate */)
{
  SetBasePoint(state);
  return 1;
}


OC_BOOL Oxs_CGEvolve::TryStep(const Oxs_MinDriver* driver,
                        Oxs_ConstKey<Oxs_SimState> current_state_key,
                        const Oxs_DriverStepInfo& /* step_info */,
                        Oxs_ConstKey<Oxs_SimState>& next_state_key)
{
#if REPORT_TIME
  TS(steponlytime.Start());
#endif // REPORT_TIME

  // If needed, work_state is initialized by InitializeWorkState()
  // calls in FillBracket() and RuffleBasePoint().  Code can test
  // work_state_key.GetPtr() != 0 to see if work_state_key has been
  // used.
  Oxs_Key<Oxs_SimState> work_state_key;

  const Oxs_SimState* cstate = &(current_state_key.GetReadReference());

  if(++step_attempt_count == 1) {
    // Update counts from current state, if available.  This keeps
    // counts in-sync with data saved in checkpoint (restart) files.
    // Wrap-around on step_attempt_count could cause some loss here,
    // but then we would also be wrapping around on the *count's
    // below; I'm not going to lose sleep over it.  This is rather
    // hackish.  It seems like it would be better to get these
    // values set up inside the Init() function.
    OC_REAL8m temp_value;
    if(cstate->GetDerivedData("Energy calc count",temp_value)) {
      energy_calc_count = static_cast<OC_UINT4m>(temp_value);
    }
    if(cstate->GetDerivedData("Cycle count",temp_value)) {
      cycle_count = static_cast<OC_UINT4m>(temp_value);
    }
    if(cstate->GetDerivedData("Cycle sub count",temp_value)) {
      cycle_sub_count = static_cast<OC_UINT4m>(temp_value);
    }
    if(cstate->GetDerivedData("Bracket count",temp_value)) {
      bracket_count = static_cast<OC_UINT4m>(temp_value);
    }
    if(cstate->GetDerivedData("Line min count",temp_value)) {
      line_minimum_count = static_cast<OC_UINT4m>(temp_value);
    }
    if(cstate->GetDerivedData("Conjugate cycle count",temp_value)) {
      conjugate_cycle_count = static_cast<OC_UINT4m>(temp_value);
    }
  }

  if(!basept.valid
     || basept.stage != cstate->stage_number
     || bracket.min_found) {
#if REPORT_TIME_CGDEVEL
  TS(basepttime.Start());
#endif // REPORT_TIME_CGDEVEL
    SetBasePoint(current_state_key);
#if REPORT_TIME_CGDEVEL
  TS(basepttime.Stop());
#endif // REPORT_TIME_CGDEVEL
  }

  if(!bracket.min_bracketed) {
#if REPORT_TIME_CGDEVEL
    TS(findbrackettime.Start());
#endif // REPORT_TIME_CGDEVEL
    FindBracketStep(driver,cstate,work_state_key);
#if REPORT_TIME_CGDEVEL
    TS(findbrackettime.Stop());
#endif // REPORT_TIME_CGDEVEL
  } else if(!bracket.min_found) {
#if REPORT_TIME_CGDEVEL
    TS(findlinemintime.Start());
#endif // REPORT_TIME_CGDEVEL
    FindLineMinimumStep(driver,cstate,work_state_key);
    if(bracket.min_found && bestpt.bracket->offset==0.0) {
      if(cycle_sub_count==0) {
        NudgeBestpt(driver,cstate,work_state_key);
      } else {
        basept.valid = 0;
        SetBasePoint(current_state_key);
      }
    }
#if REPORT_TIME_CGDEVEL
  TS(findlinemintime.Stop());
#endif // REPORT_TIME_CGDEVEL
  }

  // If work state was used (work_state_key.GetPtr()!=0), then finish
  // filling in work_state structure.  If it has been used, then
  // work_state_key should not hold a write lock, which can be tested
  // by checking that work_state_key.ObjectId() is non-zero.
  assert(work_state_key.GetPtr()==0 || work_state_key.ObjectId() != 0);
  if(work_state_key.GetPtr() != 0) {
    const Oxs_SimState* workstate = &(work_state_key.GetReadReference());
    driver->FillStateDerivedData(*cstate,*workstate);
  }

#if REPORT_TIME
  TS(steponlytime.Stop());
#endif // REPORT_TIME

  // Return bestpt in next_state_key.  Note that if it was used, then
  // the work_state_key state will also be held by one of the
  // brackets, so there will remain a lock on that state after
  // work_state_key is deleted.  (See NOTES VII 4-Aug-2017, p164,
  // example (4): in the case multiple local minima are bracketed then
  // bestpt can shift from one bracket endpt to the other, bypassing
  // the new working state.  In this setting the new working state
  // replaces the old bestpt endpt.)
  next_state_key = bestpt.bracket->key;
  next_state_key.GetReadReference();

  return !next_state_key.SameState(current_state_key);
}


void Oxs_CGEvolve::UpdateDerivedFieldOutputs(const Oxs_SimState& state)
{ // Fill Oxs_VectorOutput's that have CacheRequest enabled.
  Oxs_MeshValue<ThreeVector>* Hptr
    = (total_H_field_output.GetCacheRequestCount()>0
       ? &total_H_field_output.cache.value : 0);
  if((Hptr != 0 && total_H_field_output.cache.state_id != state.Id())
     || (total_energy_density_output.GetCacheRequestCount()>0
         && total_energy_density_output.cache.state_id != state.Id())
     || (mxHxm_output.GetCacheRequestCount()>0
         && mxHxm_output.cache.state_id != state.Id())) {
    // Need to call GetEnergyAndmxHxm
    total_energy_density_output.cache.state_id
      = mxHxm_output.cache.state_id
      = total_H_field_output.cache.state_id = 0;
    GetEnergyAndmxHxm(&state,
                      total_energy_density_output.cache.value,
                      mxHxm_output.cache.value,Hptr);
    total_energy_density_output.cache.state_id
      = mxHxm_output.cache.state_id
      = total_H_field_output.cache.state_id = state.Id();
  }
}

void Oxs_CGEvolve::UpdateDerivedOutputs(const Oxs_SimState& state)
{ // This routine fills all the Oxs_CGEvolve Oxs_ScalarOutput's to
  // the appropriate value based on the import "state".

  energy_calc_count_output.cache.state_id
    = max_mxHxm_output.cache.state_id
    = delta_E_output.cache.state_id
    = total_energy_output.cache.state_id
    = bracket_count_output.cache.state_id
    = line_min_count_output.cache.state_id
    = cycle_count_output.cache.state_id
    = cycle_sub_count_output.cache.state_id
    = conjugate_cycle_count_output.cache.state_id
    = 0;  // Mark change in progress

  OC_REAL8m last_energy;
  if(!state.GetDerivedData("Energy calc count",
			   energy_calc_count_output.cache.value) ||
     !state.GetDerivedData("Max mxHxm",max_mxHxm_output.cache.value) ||
     !state.GetDerivedData("Last energy",last_energy) ||
     !state.GetDerivedData("Total energy",
			   total_energy_output.cache.value) ||
     !state.GetDerivedData("Bracket count",
			   bracket_count_output.cache.value) ||
     !state.GetDerivedData("Line min count",
			   line_min_count_output.cache.value)) {
    // Missing at least some data
    Oxs_MeshValue<ThreeVector>* Hptr
      = (total_H_field_output.GetCacheRequestCount()>0
         ? &total_H_field_output.cache.value : 0);
    GetEnergyAndmxHxm(&state,
                      total_energy_density_output.cache.value,
                      mxHxm_output.cache.value,Hptr);
    if(!state.GetDerivedData("Energy calc count",
			     energy_calc_count_output.cache.value) ||
       !state.GetDerivedData("Max mxHxm",max_mxHxm_output.cache.value) ||
       !state.GetDerivedData("Last energy",last_energy) ||
       !state.GetDerivedData("Total energy",
			     total_energy_output.cache.value) ||
       !state.GetDerivedData("Bracket count",
			     bracket_count_output.cache.value) ||
       !state.GetDerivedData("Line min count",
			     line_min_count_output.cache.value)) {
      throw Oxs_ExtError(this,"Missing output data."
			   " Programming error?");
    }
  }
  delta_E_output.cache.value
    = total_energy_output.cache.value - last_energy;

  // cycle_count, cycle_sub_count, and conjugate_cycle_count are not
  // filled in GetEnergyAndmxHxm, but rather in SetBasePoint.  We
  // assume here that if these are not already set in state, then
  // the current values in the shadow variables are correct.
  if(!state.GetDerivedData("Cycle count",
			   cycle_count_output.cache.value)) {
    state.AddDerivedData("Cycle count",
                         OC_REAL8m(cycle_count));
    cycle_count_output.cache.value = OC_REAL8m(cycle_count);
  }
  if(!state.GetDerivedData("Cycle sub count",
			   cycle_sub_count_output.cache.value)) {
    state.AddDerivedData("Cycle sub count",
                         OC_REAL8m(cycle_sub_count));
    cycle_sub_count_output.cache.value = OC_REAL8m(cycle_sub_count);
  }
  if(!state.GetDerivedData("Conjugate cycle count",
			   conjugate_cycle_count_output.cache.value)) {
    state.AddDerivedData("Conjugate cycle count",
                         OC_REAL8m(conjugate_cycle_count));
    conjugate_cycle_count_output.cache.value
      = OC_REAL8m(conjugate_cycle_count);
  }

  energy_calc_count_output.cache.state_id
    = max_mxHxm_output.cache.state_id
    = delta_E_output.cache.state_id
    = total_energy_output.cache.state_id
    = bracket_count_output.cache.state_id
    = line_min_count_output.cache.state_id
    = cycle_count_output.cache.state_id
    = cycle_sub_count_output.cache.state_id
    = conjugate_cycle_count_output.cache.state_id
    = state.Id();
}
