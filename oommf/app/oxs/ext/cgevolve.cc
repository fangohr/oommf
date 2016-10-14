/* FILE: cgevolve.cc                 -*-Mode: c++-*-
 *
 * Concrete minimization evolver class, using conjugate gradients
 *
 */

#include <assert.h>
#include <ctype.h>
#include <float.h>

#include <algorithm>
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

OC_USE_STRING;

// Oxs_Ext registration support
OXS_EXT_REGISTER(Oxs_CGEvolve);

/* End includes */

// #define TS(x) fprintf(stderr,OC_STRINGIFY(x)"\n"); x
#define TS(x) x

#ifndef NDEBUG
# define NDEBUG 0  // Turn on debug checks
#endif

/*****************
Can't tell the players without a program:

_Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadA::Cmd
  SSE: Full
timer: 0
   BW: passes = 271,  bytes/pass = 288 MB, 33.09 GB/sec
   Cycles/pass: 52
DATA: Read: direction, best_spin
     Write: spin;
COMPUTATIONS: 1 sqrt, 7 mul, 4 add, 1 MakeUnit
             MakeUnit: 1 sqrt, 2 div, 10 mul, 7 add

_Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::Cmd
  SSE: Not aligned
timer: 1
   BW: passes = 271,  bytes/pass = 288 MB, 34.56 GB/sec
   Cycles/pass: 50
DATA: Read: tmpenergy, bestpt_energy, Ms, direction, mxHxm, mesh->Volume()
COMPUTATIONS: 15 mul, 6 add, 1 sqrt, 1 div, 3 xpfloat_accum

_Oxs_CGEvolve_SetBasePoint_ThreadA::Cmd
  SSE: None
timer: 3
   BW: passes = 125,  bytes/pass = 192 MB, 36.42 GB/sec (Fletcher-Reeves)
   Cycles/pass: 31
DATA: Read: preconditioner_Ms2_V2, bestpt_mxHxm
       R+W: (POLAK_RIBIERE only) basept.mxHxm
COMPUTATIONS: 6 mul + 3 xpfloat_accum

_Oxs_CGEvolve_SetBasePoint_ThreadB::Cmd
  SSE: Full
timer: 5
   BW: passes = 125,  bytes/pass = 512 MB, 37.03 GB/sec
   Cycles/pass: 82
DATA: Read: preconditioner_Ms_V, Ms_V, spin, bestpt_mxHxm
       R+W: basept_direction
COMPUTATIONS: 16 mul, 10 adds, 2 xpfloat_accum

_Oxs_CGEvolve_SetBasePoint_ThreadC::Cmd
  SSE: Not aligned
timer: 6
   BW: passes =  11,  bytes/pass = 320 MB, 35.16 GB/sec
   Cycles/pass: 55
DATA: Read: preconditioner_Ms_V, Ms_V, bestpt_mxHxm
     Write: basept_direction
COMPUTATIONS: 10 mul, 4 xpfloat_accum

_Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::Cmd
  SSE: Full
timer: 8
   BW: passes = 272,  bytes/pass =  32 MB, 29.33 GB/sec
   Cycles/pass:  6
DATA: Read: energy, mesh->Volume()
COMPUTATIONS: mesh->Volume() call, 1 mul + xpfloat_accum


SSE COMPUTATIONS xpfloat_accum: 4 unpack, 4 add

Most common numbered timer sequences are
8 1 0
8 1 3 5 0
8 1 6 0

Mepuche cache size: 8MB/processor x 2 processors
                    4 cores/processor; HT enabled

*****************/


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

// #define INSTRUMENT

// Constructor
Oxs_CGEvolve::Oxs_CGEvolve(
  const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr)   // MIF input block parameters
  : Oxs_MinEvolver(name,newdtr,argstr),
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
    sum_error_estimate(0),
    temp_id(0)
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

  bracket.energy_precision = GetRealInitValue("energy_precision",1e-14);

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
  temp_id=0;
  temp_energy.Release();
  temp_mxHxm.Release();

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
    for(OC_INDEX i=0;i<size;++i) {
      OC_REAL8m val = ( Ms[i] == 0.0 ? 0.0 : 1.0 );
      preconditioner_Ms_V[i].Set(val,val,val);
    }
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
    for(OC_INDEX i=0;i<size;++i) {
      ThreeVector v = preconditioner_Ms_V[i];
      if(v.x<0 || v.y<0 || v.z<0) {
        throw Oxs_ExtError(this,
              "Invalid preconditioner: negative value detected.");
      }
      OC_REAL8m tmp = v.x;
      if(v.y>tmp) tmp = v.y;
      if(v.z>tmp) tmp = v.z;
      if(tmp>maxval) maxval = tmp;
      tmp *= Ms[i]*mesh->Volume(i);
      if(tmp>maxvalMsV) maxvalMsV = tmp;
    }
    if(maxval == 0.0) init_ok = 0;  // Preconditioner matrix all zeroes.
  }

# if 1 // For debugging
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
    OC_REAL8m err_mxHxm_sumsq = 0.0;
    OC_REAL8m err_dir_sumsq = 0.0;
    for(OC_INDEX i=0;i<size;++i) {
      OC_REAL8m scale = Ms[i]*mesh->Volume(i);
      OC_REAL8m errtmp = Ms[i]*scale;
      err_mxHxm_sumsq += errtmp*errtmp;
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
        OC_REAL8m errdirtmp = Ms[i]*(cx+cy+cz); // Divide by 3 => average c.
        err_dir_sumsq += errdirtmp*errdirtmp;
      }
    }
  } else {
    // Initialization failed; take instead C^-2 = 1 (no preconditioning)
    OC_REAL8m err_mxHxm_sumsq = 0.0;
    for(OC_INDEX i=0;i<size;++i) {
      const OC_REAL8m scale = Ms[i]*mesh->Volume(i);
      OC_REAL8m errtmp = Ms[i]*scale;
      err_mxHxm_sumsq += errtmp*errtmp;
      Ms_V[i] = scale;
      preconditioner_Ms_V[i].Set(scale,scale,scale);
      const OC_REAL8m scalesq = scale*scale;
      preconditioner_Ms2_V2[i].Set(scalesq,scalesq,scalesq);
    }
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
    for(j=0;j<jbreak;++j) {
      const ThreeVector& dvec = scratch_direction[j];
      OC_REAL8m dsq = dvec.MagSq();
      ThreeVector temp = scratch_best_spin[j];
      temp *= sqrt(1+tsq*dsq);
      temp.Accum(dvec_scale,dvec);
      temp.MakeUnit();
      Nb_NOP(&temp);          // Safety; icpc optimizer showed problems
      scratch_spin[j] = temp; // with this stanza in some circumstances.
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
      Oc_Duet mult = Oc_Sqrt(Oc_Duet(1)
                             +Oc_Duet(tsq)*(dvx*dvx+dvy*dvy+dvz*dvz));

      Oc_Duet bsx,bsy,bsz;
      Oxs_ThreeVectorPairLoadAligned(&(scratch_best_spin[j]),bsx,bsy,bsz);

      Oc_Duet spx = mult*bsx + Oc_Duet(dvec_scale)*dvx;
      Oc_Duet spy = mult*bsy + Oc_Duet(dvec_scale)*dvy;
      Oc_Duet spz = mult*bsz + Oc_Duet(dvec_scale)*dvz;

      Oxs_ThreeVectorPairMakeUnit(spx,spy,spz);

      Oxs_ThreeVectorPairStreamAligned(spx,spy,spz,scratch_spin+j);
    }

    for(;j<jsize;++j) {
      const ThreeVector& dvec = scratch_direction[j];
      OC_REAL8m dsq = dvec.MagSq();
      ThreeVector temp = scratch_best_spin[j];
      temp *= sqrt(1+tsq*dsq);
      temp.Accum(dvec_scale,dvec);
      temp.MakeUnit();
      Nb_NOP(&temp);          // Safety; icpc optimizer showed problems
      scratch_spin[j] = temp; // with this stanza in some circumstances.
    }

  }
}

class _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket;
  static Oxs_Mutex result_mutex;
  static Nb_Xpfloat etemp;
  static Nb_Xpfloat dtemp;
  static Nb_Xpfloat stemp;

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
    job_basket.Init(thread_count,arrblock);
    result_mutex.Lock();
    etemp = 0.0;
    dtemp = 0.0;
    stemp = 0.0;
    result_mutex.Unlock();
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::job_basket;
Oxs_Mutex _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::result_mutex;
Nb_Xpfloat _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::etemp(0.0);
Nb_Xpfloat _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::dtemp(0.0);
Nb_Xpfloat _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::stemp(0.0);


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
    for(j=0;j<jbreak;++j) {
      OC_REAL8m vol = mesh->Volume(istart+j);
      const ThreeVector& vtemp = sdir[j];
      OC_REAL8m scale_adj = sMs[j]*vol/sqrt(1+offset_sq * vtemp.MagSq());
      work_etemp.Accum((stenergy[j] - sbenergy[j]) * vol);
      work_dtemp.Accum((smxHxm[j]*vtemp)*scale_adj);
      work_stemp.Accum(smxHxm[j].MagSq()*scale_adj*scale_adj);
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
      Oc_Duet denom = Oc_Sqrt(Oc_Duet(1.) + dt_offset_sq * vt_magsq);

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
      Nb_XpfloatDualAccum(work_stemp,work_stemp_b,mh_magsq*scale_adj*scale_adj);
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

  // Sum results into static accumulators
  work_etemp += work_etemp_b;
  work_dtemp += work_dtemp_b;
  work_stemp += work_stemp_b;
  result_mutex.Lock();
  etemp += work_etemp;
  dtemp += work_dtemp;
  stemp += work_stemp;
  result_mutex.Unlock();
}

class _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket; // Note: These are static, so
  static Oxs_Mutex result_mutex;               // only one "set" of this class
  static Nb_Xpfloat etemp;                     // may be run at one time.

  const Oxs_Mesh* mesh;
  const Oxs_MeshValue<OC_REAL8m>* energy;

  _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC()
    : mesh(0),energy(0) {}

  static void Init(int thread_count,const Oxs_StripedArray<OC_REAL8m>* arrblock) {
    job_basket.Init(thread_count,arrblock);
    result_mutex.Lock();
    etemp = 0.0;
    result_mutex.Unlock();
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::job_basket;
Oxs_Mutex _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::result_mutex;
Nb_Xpfloat _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::etemp(0.0);

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
  result_mutex.Lock();
  etemp += work_etemp_a;
  result_mutex.Unlock();
}

class _Oxs_CGEvolve_SetBasePoint_ThreadA : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<ThreeVector> job_basket;
  static Oxs_Mutex result_mutex;
  static Nb_Xpfloat gamma_sum; // Export --- use result_mutex
  static Nb_Xpfloat g_sum_sq;  // Export --- use result_mutex

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
    result_mutex.Lock();
    gamma_sum = 0.0;
    g_sum_sq = 0.0;
    result_mutex.Unlock();
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<ThreeVector> _Oxs_CGEvolve_SetBasePoint_ThreadA::job_basket;
Oxs_Mutex _Oxs_CGEvolve_SetBasePoint_ThreadA::result_mutex;
Nb_Xpfloat _Oxs_CGEvolve_SetBasePoint_ThreadA::gamma_sum = 0.0;
Nb_Xpfloat _Oxs_CGEvolve_SetBasePoint_ThreadA::g_sum_sq = 0.0;

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
  result_mutex.Lock();
  g_sum_sq += work_g_sum_sq;
  gamma_sum += work_gamma_sum;
  result_mutex.Unlock();
}

class _Oxs_CGEvolve_SetBasePoint_ThreadB : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket;
  static Oxs_Mutex result_mutex;
  static Nb_Xpfloat normsumsq; // Export --- use result_mutex
  static Nb_Xpfloat Ep;        // Export --- use result_mutex
  static Nb_Xpfloat gradsumsq; // Export --- use result_mutex
  static OC_REAL8m maxmagsq;   // Export --- use result_mutex

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
    result_mutex.Lock();
    maxmagsq = 0.0;
    normsumsq = 0.0;
    Ep = 0.0;
    gradsumsq = 0.0;
    result_mutex.Unlock();
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_CGEvolve_SetBasePoint_ThreadB::job_basket;
Oxs_Mutex _Oxs_CGEvolve_SetBasePoint_ThreadB::result_mutex;
OC_REAL8m  _Oxs_CGEvolve_SetBasePoint_ThreadB::maxmagsq = 0.0;
Nb_Xpfloat _Oxs_CGEvolve_SetBasePoint_ThreadB::normsumsq = 0.0;
Nb_Xpfloat _Oxs_CGEvolve_SetBasePoint_ThreadB::Ep = 0.0;
Nb_Xpfloat _Oxs_CGEvolve_SetBasePoint_ThreadB::gradsumsq = 0.0;

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

  normsumsq0.Accum(normsumsq1);
  Ep0.Accum(Ep1);
  gradsumsq0.Accum(gradsumsq1);

  result_mutex.Lock();
  if(maxmagsq0>maxmagsq) maxmagsq = maxmagsq0;
  normsumsq.Accum(normsumsq0);
  Ep.Accum(Ep0);
  gradsumsq.Accum(gradsumsq0);
  result_mutex.Unlock();
}


class _Oxs_CGEvolve_SetBasePoint_ThreadC : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket;
  static Oxs_Mutex result_mutex;
  static Nb_Xpfloat sumsq;     // Export --- use result_mutex
  static Nb_Xpfloat Ep;        // Export --- use result_mutex
  static Nb_Xpfloat gradsumsq; // Export --- use result_mutex
  static OC_REAL8m maxmagsq;   // Export --- use result_mutex

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
    result_mutex.Lock();
    sumsq = 0.0;
    Ep = 0.0;
    gradsumsq = 0.0;
    maxmagsq = 0.0;
    result_mutex.Unlock();
  }

  void Cmd(int threadnumber, void* data);
};

Oxs_JobControl<OC_REAL8m> _Oxs_CGEvolve_SetBasePoint_ThreadC::job_basket;
Oxs_Mutex _Oxs_CGEvolve_SetBasePoint_ThreadC::result_mutex;
Nb_Xpfloat _Oxs_CGEvolve_SetBasePoint_ThreadC::sumsq = 0.0;
Nb_Xpfloat _Oxs_CGEvolve_SetBasePoint_ThreadC::Ep = 0.0;
Nb_Xpfloat _Oxs_CGEvolve_SetBasePoint_ThreadC::gradsumsq = 0.0;
OC_REAL8m _Oxs_CGEvolve_SetBasePoint_ThreadC::maxmagsq = 0.0;

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
  Ep_0.Accum(Ep_1);
  gradsumsq0.Accum(gradsumsq1);

  if(maxmagsq_pair.GetA()>maxmagsq_local) {
    maxmagsq_local = maxmagsq_pair.GetA();
  }
  if(maxmagsq_pair.GetB()>maxmagsq_local) {
    maxmagsq_local = maxmagsq_pair.GetB();
  }

  result_mutex.Lock();
  sumsq.Accum(sumsq_0);
  Ep.Accum(Ep_0);
  gradsumsq.Accum(gradsumsq0);
  if(maxmagsq_local>maxmagsq) maxmagsq = maxmagsq_local;
  result_mutex.Unlock();
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
    _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::Init(Oc_GetMaxThreadCount(),
                                                  export_energy.GetArrayBlock());
    _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC threadC;
    threadC.mesh = mesh;
    threadC.energy = &export_energy;

    Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(threadC,0);

    _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::result_mutex.Lock();
    state->AddDerivedData("Total energy",
             _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::etemp.GetValue());
    _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadC::result_mutex.Unlock();
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
 const Oxs_SimState* state, // Import
 OC_REAL8m offset,             // Import
 OC_REAL8m &relenergy,         // Export
 OC_REAL8m &derivative,        // Export
 OC_REAL8m &grad_norm)         // Export
{ // Calculates total energy relative to that of best_state,
  // and derivative in base_direction (-mu0.H*base_direction).
  // The base_direction and best_energy arrays _must_ be set
  // before calling this function.

  // Uses mxHxm instead of H in calculation of derivative, because
  // energy is calculated off of normalized m, so component of H in m
  // direction doesn't actually have any effect.  Plus, experiments
  // appear to show generally faster convergence with mxHxm than with H.

  temp_id = 0;
  GetEnergyAndmxHxm(state,temp_energy,temp_mxHxm,NULL);
  temp_id = state->Id();

  // Set up some convenience variables
  const Oxs_Mesh* mesh = state->mesh; // For convenience

#if REPORT_TIME_CGDEVEL
TS(timer[1].Start()); /**/ // 2secs
#endif // REPORT_TIME_CGDEVEL

#if !OOMMF_THREADS
  OC_INDEX i;
  Nb_Xpfloat etemp = 0.0;
  const OC_INDEX vecsize = mesh->Size();
  for(i=0;i<vecsize;++i) {
    etemp.Accum((temp_energy[i] - bestpt.energy[i]) * mesh->Volume(i));
  }

  Nb_Xpfloat dtemp = 0.0;
  Nb_Xpfloat stemp = 0.0;
  OC_REAL8m offset_sq = offset*offset;
  for(i=0;i<vecsize;++i) {
    const ThreeVector& vtemp = basept.direction[i];
    OC_REAL8m scale_adj = Ms_V[i]/sqrt(1+offset_sq * vtemp.MagSq());
    dtemp.Accum((temp_mxHxm[i]*vtemp)*scale_adj);
    stemp.Accum(temp_mxHxm[i].MagSq()*scale_adj*scale_adj);
  }

  relenergy = etemp.GetValue();
  derivative = -MU0 * dtemp.GetValue();
  grad_norm = sqrt(stemp.GetValue());
  /// See mjd's NOTES II, 29-May-2002, p156, which includes
  /// the derivation of the scale_adj term above.
#else // OOMMF_THREADS
      {
        const Oxs_MeshValue<OC_REAL8m>& Ms = *(state->Ms);
        _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::Init(Oc_GetMaxThreadCount(),
                                                      Ms.GetArrayBlock());
        _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB threadB;
        threadB.mesh = mesh;
        threadB.tmpenergy = &temp_energy;
        threadB.bestpt_energy = &bestpt.energy;
        threadB.Ms = &Ms;
        threadB.direction = &basept.direction;
        threadB.mxHxm = &temp_mxHxm;
        threadB.offset_sq = offset*offset;

        Oxs_ThreadTree threadtree;
        threadtree.LaunchTree(threadB,0);

        _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::result_mutex.Lock();
        relenergy
          = _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::etemp.GetValue();
        derivative
          = -MU0 * _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::dtemp.GetValue();
        grad_norm
          = sqrt(_Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::stemp.GetValue());
        _Oxs_CGEvolve_GetEnergyAndmxHxm_ThreadB::result_mutex.Unlock();
        /// See mjd's NOTES II, 29-May-2002, p156, which includes
        /// the derivation of the scale_adj term above.
      }
#endif // OOMMF_THREADS

#if REPORT_TIME_CGDEVEL
TS(timer[1].Stop()); /**/
timer_counts[1].Increment(timer[1],
                mesh->Size()*(3*sizeof(OC_REAL8m)+2*sizeof(ThreeVector)));
#endif // REPORT_TIME_CGDEVEL

  state->AddDerivedData("Relative energy",relenergy);
  state->AddDerivedData("Energy best state id",
			static_cast<OC_REAL8m>(bestpt.key.ObjectId()));
}


void Oxs_CGEvolve::FillBracket(OC_REAL8m offset,
			       const Oxs_SimState* oldstateptr,
			       Oxs_Key<Oxs_SimState>& statekey,
			       Bracket& endpt)
{
  // bestpt and basept must be set prior to calling this routine
#if REPORT_TIME_CGDEVEL
  TS(fillbrackettime.Start());
#endif
  try {
    Oxs_SimState& workstate
      = statekey.GetWriteReference(); // Write lock
    Oxs_MeshValue<ThreeVector>& spin = workstate.spin;
    const Oxs_MeshValue<ThreeVector>& best_spin
      = bestpt.key.GetPtr()->spin;
    OC_REAL8m t1sq = bestpt.offset*bestpt.offset;

#if REPORT_TIME_CGDEVEL
TS(timer[0].Start()); /**/ // 7 secs
#endif // REPORT_TIME_CGDEVEL

#if !OOMMF_THREADS
    OC_INDEX vecsize = workstate.mesh->Size();
    const OC_REAL8m dvec_scale = offset - bestpt.offset;
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
        threadA.dvec_scale = offset-bestpt.offset;

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
    const Oxs_SimState& newstate = statekey.GetDepReference();
#if REPORT_TIME_CGDEVEL
    TS(fillbrackettime.Stop());
#endif
    GetRelativeEnergyAndDerivative(&newstate,offset,
                                   endpt.E,endpt.Ep,endpt.grad_norm);
#if REPORT_TIME_CGDEVEL
    TS(fillbrackettime.Start());
#endif
    endpt.offset=offset;
  }
  catch (...) {
    statekey.GetReadReference(); // Release write lock
    throw;
  }
#if REPORT_TIME_CGDEVEL
  TS(fillbrackettime.Stop());
#endif
}


void
Oxs_CGEvolve::UpdateBrackets
(Oxs_ConstKey<Oxs_SimState> tstate_key,
 const Bracket& tbracket,
 OC_BOOL force_bestpt)
{ // Incorporates tbracket into existing brackets, and changes
  // bestpt if appropriate.  Returns 1 if bestpt was changed,
  // in which case bracket E values and temp space data will
  // be modified.
  //  If force_bestpt is true, and temp_id agrees with
  // tstate_key, then tstate is made new "bestpt"
  //  Also updates data in extra_bracket.

  OC_REAL8m energy_slack = bracket.energy_precision
    * (fabs(basept.total_energy)
       + fabs(bracket.left.E) + fabs(bracket.right.E));

  // Manage brackets
  if(tbracket.offset>bracket.right.offset) {
    // Shift brackets to the right
    if(bracket.right.E<bracket.left.E
       && bracket.right.Ep<0) { // This should always be
      /// true, since otherwise (left,right] already bracketed the
      /// minimum, but check anyway for robustness.
      extra_bracket = bracket.left;
      bracket.left=bracket.right;
    }
    bracket.right = tbracket;
  } else {
    // Collapse brackets
    OC_REAL8m tlspan = tbracket.offset - bracket.left.offset;
    if(tbracket.Ep<0
       && (tbracket.E<=bracket.right.E || bracket.right.Ep>=0)
       && tbracket.E<bracket.left.E+energy_slack
       && fabs(tlspan*bracket.left.Ep) < energy_slack) {
      // Keep right interval.  Note: It may be that leftpt + tbracket
      // also bracket a minimum, but when these two E values are within
      // energy_slack, it is a good bet that the lefthand interval has
      // fallen below the noise floor, in which case we are better off
      // keeping the farther minimum.
      if(bracket.left.offset == bestpt.offset) {
	force_bestpt = 1; // Force shift of bestpt from
	/// old leftpt to new leftpt.  This increases
	/// bestpt E by at most energy_slack.
      }
      if( fabs(extra_bracket.offset - bracket.right.offset) > tlspan) {
        extra_bracket = bracket.left; // Otherwise extra_bracket
        /// was already closer than bracket.left, so don't change it.
      }
      bracket.left=tbracket;
    } else if(tbracket.E>bracket.left.E || tbracket.Ep>=0) {
      // Keep left interval
      if( fabs(bracket.left.offset - extra_bracket.offset)
          > bracket.right.offset - tbracket.offset) {
        extra_bracket = bracket.right; // Otherwise extra_bracket
        /// was already closer than bracket.right, so don't change it.
      }
      bracket.right=tbracket;
    } else {
      // Keep right interval
      if( fabs(extra_bracket.offset - bracket.right.offset) > tlspan) {
        extra_bracket = bracket.left;
      }
      bracket.left=tbracket;
    }
  }

  // Update bestpt
  if((force_bestpt || tbracket.E<=0
      || (tbracket.E<energy_slack && fabs(tbracket.Ep) <= fabs(bestpt.Ep)))
     && temp_id==tstate_key.ObjectId()) {
    // New best point found
    temp_id=bestpt.key.ObjectId();
    bestpt.key = tstate_key;
    bestpt.key.GetReadReference(); // Set read lock
    bestpt.offset = tbracket.offset;
    bestpt.Ep = tbracket.Ep;
    bestpt.grad_norm = tbracket.grad_norm;
    bestpt.energy.Swap(temp_energy);
    bestpt.mxHxm.Swap(temp_mxHxm);
    bracket.left.E -= tbracket.E;
    bracket.right.E -= tbracket.E;
  }
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
  // and fp are ignored.  For details see NOTES VII, 18-Mar-2014, pp
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


void Oxs_CGEvolve::FindBracketStep(const Oxs_SimState* oldstate,
				   Oxs_Key<Oxs_SimState>& statekey)
{ // Takes one step; assumes right.offset >= left.offset and
  // minimum to be bracketed is to the right of right.offset

  assert(bracket.right.offset>=bracket.left.offset && bracket.left.Ep<=0.0);

  if(bracket.left.Ep == 0.0) { // Special case handling
    bracket.min_bracketed=1;
    return;
  }

  ++bracket_count;
#ifndef NDEBUG
  const OC_REAL8m starting_offset = bracket.right.offset;
#endif

  OC_REAL8m offset = 0.0;
  const OC_REAL8m h = bracket.right.offset - bracket.left.offset;
  const OC_REAL8m maxoff
    = OC_MAX(bracket.left.offset,bracket.scaled_maxstep);
  const OC_REAL8m minoff
    = OC_MIN(OC_MAX(bracket.start_step,2*bracket.right.offset),maxoff);

  if(h<=0.0) {
    offset = minoff;
  } else {
    const OC_REAL8m wgt = 0.5;  // Weighting for EstimateQuadraticMinimum
    offset = bracket.left.offset
      + 1.75*EstimateQuadraticMinimum(wgt,h,
                                      bracket.left.E,bracket.right.E,
                                      bracket.left.Ep,bracket.right.Ep);
    if(offset<minoff) offset = minoff;
    if(offset>maxoff) offset = maxoff;
  }

  // Zero span check
  if(offset <= bracket.right.offset) {
    // Proposed bracket span increase is floating point 0.
    if(bracket.right.offset>0) {
      offset = bracket.right.offset*(1+16*OC_REAL8m_EPSILON);
    } else {
      if(bracket.scaled_maxstep>0) {
	offset = bracket.scaled_maxstep;
      } else {
	// Punt
	offset = OC_REAL8m_EPSILON;
      }
    }
  }
  if(fabs(offset-bestpt.offset)*basept.direction_max_mag
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


  Bracket temppt;
  FillBracket(offset,oldstate,statekey,temppt);

  UpdateBrackets(statekey,temppt,0);

  // Classify situation.  We always require leftpt have Ep<0,
  // so there are two possibilities:
  //  1) temppt.E>bracket.left.E+energy_slack or temppt.Ep>=0
  //          ==> Minimum is bracketed
  //  2) temppt.E<=bracket.left.E+energy_slack and temppt.Ep<0
  //          ==> Minimum not bracketed
  OC_REAL8m energy_slack = bracket.energy_precision
    * (fabs(basept.total_energy)
       + fabs(bracket.left.E) + fabs(bracket.right.E));
  if(temppt.E<=bracket.left.E+energy_slack && temppt.Ep<0) {
    // Minimum not bracketed.
    bracket.min_bracketed=0; // Safety
    bracket.stop_span = 0.0;
    if(bracket.right.offset >= bracket.scaled_maxstep) {
      // Unable to bracket minimum inside allowed range.  Collapse
      // bracket to bestpt, and mark line minimization complete.  NOTE:
      // This may cause solver to get stuck with bestpt at basept, if
      // the surface is very flat and noisy.  May want to include some
      // logic either here or in UpdateBracket() to detect this
      // situation and move bestpt away from basept.
      bracket.min_bracketed=1;
      bracket.min_found = 1;
      bracket.right.offset = bestpt.offset;
      bracket.right.E = 0.0;
      bracket.right.Ep = bestpt.Ep;
      bracket.right.grad_norm = bestpt.grad_norm;
      bracket.left = bracket.right;
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

void Oxs_CGEvolve::FindLineMinimumStep(const Oxs_SimState* oldstate,
				       Oxs_Key<Oxs_SimState>& statekey)
{ // Compresses bracket span.
  // Assumes coming in and forces on exit requirement that leftpt.Ep<0,
  // and either rightpt.E>leftpt.E or rightpt.Ep>=0.
  assert(bracket.left.Ep<=0.0
	 && (bracket.right.E>bracket.left.E || bracket.right.Ep>=0));
  OC_REAL8m span = bracket.right.offset - bracket.left.offset;
  OC_REAL8m energy_slack = bracket.energy_precision
    * (fabs(basept.total_energy)
       + fabs(bracket.left.E) + fabs(bracket.right.E));
  OC_REAL8m min_use_energy_span = 0.0;
  if(-1*bracket.left.Ep >= 1.0
     || energy_slack < -1*bracket.left.Ep*OC_REAL8m_MAX) {
    min_use_energy_span = -1*energy_slack/bracket.left.Ep;
  }

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
  // bestpt.Ep is 0, so it should be always possible in principle,
  // ignoring discretization effects.  This check here is not exactly
  // the same as in SetBasePoint(), because a) it uses the
  // Fletcher-Reeves conjugation procedure, regardless of the selected
  // Conjugate_Method, and b) bestpt.Ep, used here, is computed in
  // GetRelativeEnergyAndDerivative() with a "scale_adj" that differs
  // from that used in SetBasePoint().
  //   The last test group are sanity checks, and the obsoleted span
  // size control.

  if(fabs(bestpt.Ep)
     < MU0*bestpt.grad_norm*basept.direction_norm*bracket.angle_precision
          *(1+2*sum_error_estimate)
     && (bestpt.Ep==0 || bestpt.Ep > basept.Ep)
     && (bestpt.Ep==0 || span<=bracket.stop_span || nudge>=span))
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

  // Pick test point.
  OC_REAL8m test_offset;
  OC_REAL8m lambda = 0.5; // Default
  Bracket testpt;
  OC_REAL8m lEp = bracket.left.Ep * span;
  OC_REAL8m rEp = bracket.right.Ep * span;
  OC_REAL8m Ediff = bracket.right.E - bracket.left.E;

  // Test for total numerics loss
  if(span<=256*bracket.stop_span
     && fabs(Ediff)<=energy_slack
     && fabs(rEp-lEp)<fabs(lEp)/16.
     && fabs(lEp) < energy_slack) {
    // Yeah, this could happen by chance, but it's
    // not very likely (I hope...) -mjd, Feb-2003
    bracket.min_found=1;
    bracket.last_min_reduction_ratio=0.0;
    bracket.next_to_last_min_reduction_ratio=0.0;
    return;
  }

  if(span > min_use_energy_span && rEp>=0.) {
    // Cubic fit using E and Ep.  See mjd's NOTES II, 1-Sep-2001, p134-136.
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
	else                lambda = -c/(b + disc);
      } else {
	if(fabs(3*a)<=(-b + disc)) lambda = Nb_Signum(a);
	else                       lambda = (-b + disc)/(3*a);
      }
    }
  } else if(rEp>=0.) {
    // E data looks iffy, so just use derivative information.
    // If we have a valid "extra_bracket", then use the Ep data
    // at the three bracket points to fit a quadratic.  Otherwise,
    // use a linear fit through the two main bracket Ep values.
    // See NOTES VII, 27-Mar-2014, p24 for details of quadratic fit.
    lambda = -1.0;
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
      lambda = -1.0;
      if(disc >= 0.0) {
        disc = sqrt(disc);
        if(b>=0.0) {
          lambda = -2*c/(b+disc);
        } else {
          lambda = 0.5*(disc-b)/a;
        }
        // The computed lambda is across tspan.  Subsequent
        // code wants lambda across [left.offset,right.offset].
        lambda = (lambda*tspan - lspan)/rspan;
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
      lambda = -1.0;
      if(disc >= 0.0) {
        disc = sqrt(disc);
        if(b>=0.0) {
          lambda = -2*c/(b+disc);
        } else {
          lambda = 0.5*(disc-b)/a;
        }
        // The computed lambda is across tspan.  Subsequent
        // code wants lambda across [left.offset,right.offset].
        lambda *= tspan/lspan;
      }
    }
    if(lambda<=0.0 || 1.0<=lambda) {
      // Fallback to linear fit
      OC_REAL8m Ep_diff = bracket.right.Ep - bracket.left.Ep;
      /// Since left.Ep<0,/ Ep_diff = |right.Ep| + |left.Ep|.
      lambda = -bracket.left.Ep/Ep_diff;
      // Provided Ep_diff is a valid floating point number,
      // then lambda should not overflow, and in fact must
      // lay inside [0,1].
    }
  } else {
    // Overly large bracket, or bad rightpt.Ep.
    // Guess at minpt using leftpt.E/Ep and rightpt.E
    const OC_REAL8m reduce_limit = 1./32.; // The
    /// situation rightpt.Ep<0 is a suspicious, so limit
    /// reduction.
    OC_REAL8m numerator = -1.0 * lEp; // This is > 0.
    OC_REAL8m denominator = 2*(Ediff - lEp);
    // denominator must also be >0, because rightpt.E>=leftpt.E
    // if a minimum is bracketed with rightpt.Ep<0.
    denominator = fabs(denominator); // But play it safe, because
    /// checks below depend on denominator>0.
    if(numerator<reduce_limit*denominator) {
      lambda = reduce_limit;
    } else if(numerator>(1-reduce_limit)*denominator) {
      lambda = 1-reduce_limit;
    } else {
      lambda = numerator/denominator;
    }
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

  test_offset = bracket.left.offset + lambda * span;
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
  FillBracket(test_offset,oldstate,statekey,testpt);

  // Determine which interval, [left,test] or [test,right] to keep.
  UpdateBrackets(statekey,testpt,0);
  OC_REAL8m newspan = bracket.right.offset - bracket.left.offset;
  assert(bracket.next_to_last_min_reduction_ratio
         *bracket.last_min_reduction_ratio
         *newspan/span<1-max_reduce_base*max_reduce_base);
  bracket.next_to_last_min_reduction_ratio
    = bracket.last_min_reduction_ratio;
  bracket.last_min_reduction_ratio =  newspan/span;
#ifdef INSTRUMENT
/**/ fprintf(stderr," --Span ratio:%#10.7f\n",
             static_cast<double>(newspan/span));
#endif // INSTRUMENT

  // Is minimum found?  The second test here is a rough check to
  // see if the conjugation procedure applied to mxHxm will yield
  // a downhill direction.  See note in the up-front check for
  // additional details.
  if(fabs(bestpt.Ep)
     < MU0*bestpt.grad_norm*basept.direction_norm*bracket.angle_precision
          *(1+2*sum_error_estimate)
     && (bestpt.Ep==0 || bestpt.Ep > basept.Ep)
     && (bestpt.Ep==0 || span<=bracket.stop_span
         || (nudge>=span && bracket.right.Ep <= bracket.left.Ep)))
  {
    bracket.min_found=1;
    bestpt.is_line_minimum=1; // Here and in the up-front check at the
    /// top of this routine should be the only two places where
    /// is_line_minimum gets set true.
  }

  assert(bracket.left.Ep<0.0
	 && (bracket.right.E>bracket.left.E || bracket.right.Ep>=0));
}

 void Oxs_CGEvolve::AdjustDirection(const Oxs_SimState* cstate,
                                    OC_REAL8m gamma,
                                    OC_REAL8m& maxmagsq,
                                    Nb_Xpfloat& work_normsumsq,
                                    Nb_Xpfloat& work_gradsumsq,
                                    Nb_Xpfloat& work_Ep)
{ //   Performs operation direction = torque + gamma*old_direction,
  // and returns some related values.  Intended solely for internal
  // use by Oxs_CGEvolve::SetBasePoint method.
  //   This code is not threaded, but it would be straightforward to
  // make it so.
  const Oxs_MeshValue<ThreeVector>& spin = cstate->spin;
  const Oxs_Mesh* mesh = cstate->mesh;
  const OC_INDEX size = mesh->Size();

  maxmagsq = 0.0;
  work_normsumsq = 0.0;
  work_gradsumsq = 0.0;
  work_Ep = 0.0;
  for(OC_INDEX i=0;i<size;++i) {
    const ThreeVector& pc = preconditioner_Ms_V[i];
    ThreeVector temp = bestpt.mxHxm[i];
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
    Nb_Xpfloat work_temp = temp.x * bestpt.mxHxm[i].x;
    work_temp.Accum(temp.y * bestpt.mxHxm[i].y);
    work_temp.Accum(temp.z * bestpt.mxHxm[i].z);
    work_temp *= Ms_V[i];
    work_Ep.Accum(work_temp);
    /// See mjd's NOTES II, 29-May-2002, p156.
    work_gradsumsq.Accum(bestpt.mxHxm[i].MagSq()*Ms_V[i]*Ms_V[i]);
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
   //   This code is not threaded, but it would be straightforward to
   // make it so.
  const Oxs_MeshValue<ThreeVector>& spin = cstate->spin;
  const Oxs_Mesh* mesh = cstate->mesh;
  const OC_INDEX size = mesh->Size();

  maxmagsq = 0.0;
  work_normsumsq = 0.0;
  work_gradsumsq = 0.0;
  work_Ep = 0.0;
  for(OC_INDEX i=0;i<size;++i) {
    const ThreeVector& pc = preconditioner_Ms_V[i];
    ThreeVector temp = bestpt.mxHxm[i];
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
    if(magsq>maxmagsq) maxmagsq = magsq;
    work_normsumsq.Accum(temp.x*temp.x);
    work_normsumsq.Accum(temp.y*temp.y);
    work_normsumsq.Accum(temp.z*temp.z);
    Nb_Xpfloat work_temp = temp.x * bestpt.mxHxm[i].x;
    work_temp.Accum(temp.y * bestpt.mxHxm[i].y);
    work_temp.Accum(temp.z * bestpt.mxHxm[i].z);
    work_temp *= Ms_V[i];
    work_Ep.Accum(work_temp);
    /// See mjd's NOTES II, 29-May-2002, p156.
    work_gradsumsq.Accum(bestpt.mxHxm[i].MagSq()*Ms_V[i]*Ms_V[i]);
  }
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

  extra_bracket.offset = -1;  // Mark as not set

  // Some convenience variables
  const Oxs_Mesh* mesh = cstate->mesh;
  OC_REAL8m Ep = 0.0;
  // const OC_REAL8m last_scaling = basept.direction_max_mag;
  OC_REAL8m next_step_guess = bestpt.offset;
  if(bestpt.is_line_minimum && bracket.left.Ep<0 && bracket.right.Ep>0) {
    // Compute improved estimate of minimum, via linear fit to last Ep data
    next_step_guess = (bracket.right.Ep*bracket.left.offset
                       - bracket.left.Ep*bracket.right.offset)
                     / (bracket.right.Ep - bracket.left.Ep);
  }
  OC_BOOL last_step_is_minimum = bestpt.is_line_minimum;

  const OC_REAL8m last_direction_norm = basept.direction_norm;

  // cstate and bestpt should be same
  if(!bestpt.key.SameState(cstate_key)) {
    // Fill bestpt from cstate
    bestpt.key = cstate_key;
    bestpt.key.GetReadReference(); // Set read lock
    GetEnergyAndmxHxm(cstate,bestpt.energy,bestpt.mxHxm,NULL);
    last_step_is_minimum = 0;
    /// Should next_step_guess be set to 0 in this case???
  }
  bestpt.offset=0.0; // Position relative to basept
  bestpt.is_line_minimum=0; // Presumably not minimum wrt new direction

  // Determine new direction
  if(!basept.valid
     || cstate->stage_number!=basept.stage
     || cycle_sub_count >= gradient_reset_count
     || basept.direction.Size()!=bestpt.mxHxm.Size()
     || !last_step_is_minimum) {
    basept.valid=0;
    if(basept.method == Basept_Data::POLAK_RIBIERE) {
      basept.mxHxm = bestpt.mxHxm;  // Element-wise copy
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
        work_sum.Accum(bestpt.mxHxm[i].x*bestpt.mxHxm[i].x
                       *preconditioner_Ms2_V2[i].x);
        work_sum.Accum(bestpt.mxHxm[i].y*bestpt.mxHxm[i].y
                       *preconditioner_Ms2_V2[i].y);
        work_sum.Accum(bestpt.mxHxm[i].z*bestpt.mxHxm[i].z
                       *preconditioner_Ms2_V2[i].z);
      }
      gamma = new_g_sum_sq = work_sum.GetValue();
    } else {
      // Polak-Ribiere method
      Nb_Xpfloat work_sum = 0.0;
      Nb_Xpfloat work_gamma = 0.0;
      for(i=0;i<size;i++) {
	ThreeVector temp = bestpt.mxHxm[i];
        const ThreeVector& scale = preconditioner_Ms2_V2[i];
        // Note: preconditioner has (Ms[i]*mesh->Volume(i))^2 built in.
        work_sum.Accum(temp.x*temp.x*scale.x);
        work_sum.Accum(temp.y*temp.y*scale.y);
        work_sum.Accum(temp.z*temp.z*scale.z);
#if 0
	temp -= basept.mxHxm[i];  // Polak-Ribiere adjustment
	gamma += temp.x*bestpt.mxHxm[i].x*scale.x
               + temp.y*bestpt.mxHxm[i].y*scale.y
               + temp.z*bestpt.mxHxm[i].z*scale.z;
#else
        work_gamma.Accum((temp.x-basept.mxHxm[i].x)*temp.x*scale.x);
        work_gamma.Accum((temp.y-basept.mxHxm[i].y)*temp.y*scale.y);
        work_gamma.Accum((temp.z-basept.mxHxm[i].z)*temp.z*scale.z);
#endif
        basept.mxHxm[i] = bestpt.mxHxm[i];
      }
      new_g_sum_sq = work_sum.GetValue();
      gamma = work_gamma.GetValue();
    }
    gamma /= basept.g_sum_sq;
#else // !OOMMF_THREADS
    {
      _Oxs_CGEvolve_SetBasePoint_ThreadA threadA;
      _Oxs_CGEvolve_SetBasePoint_ThreadA::Init(Oc_GetMaxThreadCount(),
                                               bestpt.mxHxm.GetArrayBlock());
      threadA.preconditioner_Ms2_V2 = &preconditioner_Ms2_V2;
      threadA.bestpt_mxHxm = &(bestpt.mxHxm);
      threadA.basept_mxHxm = &(basept.mxHxm);
      if(basept.method == Basept_Data::POLAK_RIBIERE) {
        threadA.method = _Oxs_CGEvolve_SetBasePoint_ThreadA::POLAK_RIBIERE;
      } else {
        threadA.method = _Oxs_CGEvolve_SetBasePoint_ThreadA::FLETCHER_REEVES;
      }
      Oxs_ThreadTree threadtree;
      threadtree.LaunchTree(threadA,0);

      // Store results
      _Oxs_CGEvolve_SetBasePoint_ThreadA::result_mutex.Lock();
      gamma  = _Oxs_CGEvolve_SetBasePoint_ThreadA::gamma_sum.GetValue();
      new_g_sum_sq = _Oxs_CGEvolve_SetBasePoint_ThreadA::g_sum_sq.GetValue();
      _Oxs_CGEvolve_SetBasePoint_ThreadA::result_mutex.Unlock();
      gamma /= basept.g_sum_sq;
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
    Nb_Xpfloat work_normsumsq,work_Ep,work_gradsumsq;
    AdjustDirection(cstate,gamma,maxmagsq,work_normsumsq,work_gradsumsq,work_Ep);
    normsumsq = work_normsumsq.GetValue();
    bestpt.grad_norm = sqrt(work_gradsumsq.GetValue());
    Ep = work_Ep.GetValue();
#else // OOMMF_THREADS /////////////////////////////////////////////////
    {
      const Oxs_MeshValue<ThreeVector>& spin = cstate->spin;
      _Oxs_CGEvolve_SetBasePoint_ThreadB threadB;
      _Oxs_CGEvolve_SetBasePoint_ThreadB::Init(Oc_GetMaxThreadCount(),
                                               Ms_V.GetArrayBlock());

      threadB.preconditioner_Ms_V = &preconditioner_Ms_V;
      threadB.Ms_V = &Ms_V;
      threadB.spin = &spin;
      threadB.bestpt_mxHxm = &(bestpt.mxHxm);
      threadB.basept_direction = &(basept.direction);
      threadB.gamma = gamma;

      Oxs_ThreadTree threadtree;
      threadtree.LaunchTree(threadB,0);

      // Accumulate results
      _Oxs_CGEvolve_SetBasePoint_ThreadB::result_mutex.Lock();
      maxmagsq = _Oxs_CGEvolve_SetBasePoint_ThreadB::maxmagsq;
      normsumsq = _Oxs_CGEvolve_SetBasePoint_ThreadB::normsumsq.GetValue();
      Ep = _Oxs_CGEvolve_SetBasePoint_ThreadB::Ep.GetValue();
      bestpt.grad_norm
        = sqrt(_Oxs_CGEvolve_SetBasePoint_ThreadB::gradsumsq.GetValue());
      _Oxs_CGEvolve_SetBasePoint_ThreadB::result_mutex.Unlock();
    }
#endif // OOMMF_THREADS ////////////////////////////////////////////////

#if REPORT_TIME_CGDEVEL
TS(timer[5].Stop()); /**/
timer_counts[5].Increment(timer[5],
                mesh->Size()*(sizeof(OC_REAL8m)+5*sizeof(ThreeVector)));
#endif // REPORT_TIME_CGDEVEL

    if(bestpt.grad_norm
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
      if(Ep <= kludge_adjust_angle_cos*sqrt(normsumsq)
         *bestpt.grad_norm*(1+8*OC_REAL8m_EPSILON)) {
        // See NOTES VII, 16-May-2014, p 26-29.
        OC_REAL8m Tsq = bestpt.grad_norm*bestpt.grad_norm;
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
        bestpt.grad_norm = sqrt(work_gradsumsq.GetValue());
        Ep = work_Ep.GetValue();
      }
      basept.direction_max_mag = sqrt(maxmagsq);
      basept.direction_norm = sqrt(normsumsq);
      basept.g_sum_sq = new_g_sum_sq;
      Ep *= -MU0; // See mjd's NOTES II, 29-May-2002, p156.
      basept.Ep = bestpt.Ep = Ep;
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
      basept.direction[i] = bestpt.mxHxm[i];
      basept.direction[i].x *= preconditioner_Ms_V[i].x;
      basept.direction[i].y *= preconditioner_Ms_V[i].y;
      basept.direction[i].z *= preconditioner_Ms_V[i].z;
      // Note: Preconditioner has Ms[i]*mesh->Volume(i) built in.

      Nb_Xpfloat work_temp = basept.direction[i].x * bestpt.mxHxm[i].x;
      work_temp.Accum(basept.direction[i].y * bestpt.mxHxm[i].y);
      work_temp.Accum(basept.direction[i].z * bestpt.mxHxm[i].z);
      work_temp *= Ms_V[i];
      work_Ep.Accum(work_temp);
      work_gradsumsq.Accum(bestpt.mxHxm[i].MagSq()*Ms_V[i]*Ms_V[i]);

      OC_REAL8m magsq = basept.direction[i].MagSq();
      if(magsq>maxmagsq) maxmagsq = magsq;
      work_sumsq.Accum(basept.direction[i].x*basept.direction[i].x);
      work_sumsq.Accum(basept.direction[i].y*basept.direction[i].y);
      work_sumsq.Accum(basept.direction[i].z*basept.direction[i].z);
      /// See mjd's NOTES II, 29-May-2002, p156.
    }
    sumsq = work_sumsq.GetValue();
    Ep = work_Ep.GetValue();
    bestpt.grad_norm = sqrt(work_gradsumsq.GetValue());
#else // !OOMMF_THREADS
    OC_REAL8m maxmagsq, sumsq;
    {
      _Oxs_CGEvolve_SetBasePoint_ThreadC threadC;
      _Oxs_CGEvolve_SetBasePoint_ThreadC::Init(Oc_GetMaxThreadCount(),
                                               Ms_V.GetArrayBlock());
      threadC.preconditioner_Ms_V = &preconditioner_Ms_V;
      threadC.Ms_V = &Ms_V;
      threadC.bestpt_mxHxm = &(bestpt.mxHxm);
      threadC.basept_direction = &(basept.direction);

      Oxs_ThreadTree threadtree;
      threadtree.LaunchTree(threadC,0);

      // Accumulate results
      _Oxs_CGEvolve_SetBasePoint_ThreadC::result_mutex.Lock();
      maxmagsq = _Oxs_CGEvolve_SetBasePoint_ThreadC::maxmagsq;
      sumsq = _Oxs_CGEvolve_SetBasePoint_ThreadC::sumsq.GetValue();
      Ep = _Oxs_CGEvolve_SetBasePoint_ThreadC::Ep.GetValue();
      bestpt.grad_norm
        = sqrt(_Oxs_CGEvolve_SetBasePoint_ThreadC::gradsumsq.GetValue());
      _Oxs_CGEvolve_SetBasePoint_ThreadC::result_mutex.Unlock();
    }
#endif // !OOMMF_THREADS
    basept.direction_max_mag = Oc_Sqrt(maxmagsq);
    Nb_NOP(basept.direction_max_mag); // Workaround for icpc optimization bug
    basept.g_sum_sq = sumsq;
    basept.direction_norm = sqrt(sumsq);
    Ep *= -MU0; // See mjd's NOTES II, 29-May-2002, p156.
    basept.Ep = bestpt.Ep = Ep;
    bestpt.offset = 0.0;
    bestpt.is_line_minimum = 0;
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
  bracket.left.offset = bracket.right.offset = 0.;
  bracket.left.E      = bracket.right.E      = 0.;
  bracket.left.Ep     = bracket.right.Ep     = Ep;
}

void Oxs_CGEvolve::RuffleBasePoint(const Oxs_SimState* oldstate,
                                   Oxs_Key<Oxs_SimState>& statekey)
{ // Tweak all the spins in oldstate by epsilon, storing result in
  // statekey.  The new state is then set as the new base point.  It is
  // difficult to write a regression test for this bit of code because
  // the nudge amount depends on sizeof(OC_REAL8m) --- specifically
  // OC_REAL8m_EPSILON.
  try {
    const Oxs_MeshValue<ThreeVector>& oldspin = oldstate->spin;
    Oxs_SimState& workstate
      = statekey.GetWriteReference(); // Write lock
    Oxs_MeshValue<ThreeVector>& workspin = workstate.spin;

    const OC_INDEX vecsize = oldspin.Size();
    for(OC_INDEX i=0;i<vecsize;i++) {
      // The tweaking is not uniform on the sphere, but do we care?
      workspin[i].x = oldspin[i].x + (2*Oc_UnifRand()-1)*OC_REAL8m_EPSILON;
      workspin[i].y = oldspin[i].y + (2*Oc_UnifRand()-1)*OC_REAL8m_EPSILON;
      workspin[i].z = oldspin[i].z + (2*Oc_UnifRand()-1)*OC_REAL8m_EPSILON;
      workspin[i].MakeUnit();
    }
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

void Oxs_CGEvolve::NudgeBestpt(const Oxs_SimState* oldstate,
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
    RuffleBasePoint(oldstate,statekey);
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
  Bracket testpt;
  FillBracket(test_offset,oldstate,statekey,testpt);
  UpdateBrackets(statekey,testpt,1);
  // One may want to force testpt acceptance only if
  // testpt.E<energy_slack, but if this doesn't hold
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


OC_BOOL Oxs_CGEvolve::Step(const Oxs_MinDriver* driver,
                        Oxs_ConstKey<Oxs_SimState> current_state_key,
                        const Oxs_DriverStepInfo& /* step_info */,
                        Oxs_Key<Oxs_SimState>& next_state_key)
{
  // Note: On entry, next_state_key is holding a write lock.
#ifdef INSTRUMENT
  static OC_REAL8m last_energy = 0.0;
#endif // INSTRUMENT

#if REPORT_TIME
  TS(steponlytime.Start());
#endif // REPORT_TIME

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

  // Do first part of next_state structure initialization.
  driver->FillStateMemberData(*cstate,
                              next_state_key.GetWriteReference());

  if(!basept.valid
     || basept.stage != cstate->stage_number
     || bracket.min_found) {
#ifdef INSTRUMENT
/**/ OC_REAL8m new_energy=0.0;
/**/ cstate->GetDerivedData("Total energy",new_energy);
/**/ fprintf(stderr," Energy drop: %g\n",double(last_energy-new_energy));
/**/ last_energy=new_energy;
/**/ fprintf(stderr,"It=%5d  valid=%d, stages: %d/%d, min_found=%d\n",
/**/  cstate->iteration_count+1,
/**/  basept.valid,basept.stage,cstate->stage_number,bracket.min_found);
#endif // INSTRUMENT
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
    FindBracketStep(cstate,next_state_key);
#if REPORT_TIME_CGDEVEL
  TS(findbrackettime.Stop());
#endif // REPORT_TIME_CGDEVEL
  } else if(!bracket.min_found) {
#if REPORT_TIME_CGDEVEL
  TS(findlinemintime.Start());
#endif // REPORT_TIME_CGDEVEL
    FindLineMinimumStep(cstate,next_state_key);
    if(bracket.min_found && bestpt.offset==0.0) {
      if(cycle_sub_count==0) {
        NudgeBestpt(cstate,next_state_key);
      } else {
        basept.valid = 0;
        SetBasePoint(current_state_key);
      }
    }
#if REPORT_TIME_CGDEVEL
  TS(findlinemintime.Stop());
#endif // REPORT_TIME_CGDEVEL
  }

  // Note: Call to GetReadReference insures that write lock is released.
  const Oxs_SimState* nstate = &(next_state_key.GetReadReference());

  // Finish filling in next_state structure
  driver->FillStateDerivedData(*cstate,*nstate);

#if REPORT_TIME
  TS(steponlytime.Stop());
#endif // REPORT_TIME
  return bestpt.key.SameState(next_state_key);
}


void Oxs_CGEvolve::UpdateDerivedFieldOutputs(const Oxs_SimState& state)
{ // Fill Oxs_VectorOutput's that have CacheRequest enabled.
  if(total_H_field_output.GetCacheRequestCount()>0
     || state.Id()!=temp_id) {
    // Need to call GetEnergyAndmxHxm
    temp_id=0;
    if(total_H_field_output.GetCacheRequestCount()>0) {
      total_H_field_output.cache.state_id=0;
      GetEnergyAndmxHxm(&state,temp_energy,temp_mxHxm,
			&total_H_field_output.cache.value);
      total_H_field_output.cache.state_id=state.Id();
    } else {
      GetEnergyAndmxHxm(&state,temp_energy,temp_mxHxm,NULL);
    }
    temp_id = state.Id();
  }
  if(total_energy_density_output.GetCacheRequestCount()>0) {
    total_energy_density_output.cache.state_id = 0;
    total_energy_density_output.cache.value = temp_energy;
    total_energy_density_output.cache.state_id = state.Id();
  }
  if(mxHxm_output.GetCacheRequestCount()>0) {
    mxHxm_output.cache.state_id = 0;
    mxHxm_output.cache.value    = temp_mxHxm;
    mxHxm_output.cache.state_id = temp_id;
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
    temp_id=0;
    GetEnergyAndmxHxm(&state,temp_energy,temp_mxHxm,NULL);
    temp_id=state.Id();
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
