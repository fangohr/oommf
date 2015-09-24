/* FILE: timeevolver.cc                 -*-Mode: c++-*-
 *
 * Abstract time evolver class
 *
 */

#include "director.h"
#include "timeevolver.h"

#include "energy.h"

/* End includes */

// Constructors
Oxs_TimeEvolver::Oxs_TimeEvolver
(const char* name,     // Child instance id
 Oxs_Director* newdtr) // App director
  : Oxs_Evolver(name,newdtr), energy_calc_count(0)
{}

Oxs_TimeEvolver::Oxs_TimeEvolver
(const char* name,
 Oxs_Director* newdtr,
 const char* argstr)      // MIF block argument string
  : Oxs_Evolver(name,newdtr,argstr), energy_calc_count(0)
{
  total_energy_output.Setup(this,InstanceName(),
                            "Total energy","J",1,
                            &Oxs_TimeEvolver::UpdateEnergyOutputs);
  total_energy_density_output.Setup(this,InstanceName(),
                           "Total energy density","J/m^3",1,
                           &Oxs_TimeEvolver::UpdateEnergyOutputs);
  total_field_output.Setup(this,InstanceName(),
                           "Total field","A/m",1,
                           &Oxs_TimeEvolver::UpdateEnergyOutputs);
  energy_calc_count_output.Setup(this,InstanceName(),
                           "Energy calc count","",0,
                           &Oxs_TimeEvolver::FillEnergyCalcCountOutput);
  /// Note: MSVC++ 6.0 requires fully qualified member names

  total_energy_output.Register(director,-5);
  total_energy_density_output.Register(director,-5);
  total_field_output.Register(director,-5);
  energy_calc_count_output.Register(director,-5);

  // Eventually, caching should be handled by controlling Tcl script.
  // Until then, request caching of scalar energy output by default.
  total_energy_output.CacheRequestIncrement(1);

}


OC_BOOL Oxs_TimeEvolver::Init()
{
#if REPORT_TIME
  Oc_TimeVal cpu,wall;
  steponlytime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"   Step-only    .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
  steponlytime.Reset();
#endif // REPORT_TIME

  energy_calc_count=0;

  // Release scratch space.
  temp_energy.Release();
  temp_field.Release();

  return Oxs_Evolver::Init();
}


Oxs_TimeEvolver::~Oxs_TimeEvolver()
{
#if REPORT_TIME
  Oc_TimeVal cpu,wall;
  steponlytime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"   Step-only    .....   %7.2f cpu /%7.2f wall,"
            " (%.1000s)\n",
            double(cpu),double(wall),InstanceName());
  }
#endif // REPORT_TIME
}


// GetEnergyDensity: Note that mxH is returned, as opposed to MxH.
// This relieves this routine from needing to know what Ms is, and saves
// an unneeded multiplication (since the evolver is just going to divide
// it back out again to calculate dm/dt (as opposed again to dM/dt)).
// The returned energy array is average energy density for the
// corresponding cell in J/m^3; mxH is in A/m, pE_pt (partial derivative
// of E with respect to t) is in J/s.  Any of mxH or H may be
// NULL, which disables assignment for that variable.
void Oxs_TimeEvolver::GetEnergyDensity
(const Oxs_SimState& state,
 Oxs_MeshValue<OC_REAL8m>& energy,
 Oxs_MeshValue<ThreeVector>* mxH_req,
 Oxs_MeshValue<ThreeVector>* H_req,
 OC_REAL8m& pE_pt,
 OC_REAL8m& total_E)
{
  // Update call count
  ++energy_calc_count;

  Oxs_MeshValue<ThreeVector>* H_fill = H_req;
  Oxs_MeshValue<ThreeVector>* field_cache = NULL;
  if(total_field_output.GetCacheRequestCount()>0) {
    total_field_output.cache.state_id=0;
    field_cache = &total_field_output.cache.value;
    if(H_fill==NULL) H_fill = field_cache;
  }
  /// If field is requested by both H_req and total_field_output,
  /// then fill H_req first, and copy to field_cache at end.

  // Set up energy computation output data structure
  Oxs_ComputeEnergyData oced(state);
  oced.scratch_energy = &temp_energy;
  oced.scratch_H      = &temp_field;
  oced.energy_accum   = &energy;
  oced.H_accum        = H_fill;
  oced.mxH_accum      = mxH_req;
  oced.energy         = NULL;  // Required null
  oced.H              = NULL;  // Required null
  oced.mxH            = NULL;  // Required null

  UpdateFixedSpinList(state.mesh);
  Oxs_ComputeEnergyExtraData oceed(GetFixedSpinList(),0);

  // Compute total energy and torque
#if REPORT_TIME
  OC_BOOL sot_running = steponlytime.IsRunning();
  if(sot_running) {
    steponlytime.Stop();
  }
#endif // REPORT_TIME
  Oxs_ComputeEnergies(state,oced,director->GetEnergyObjects(),oceed);
#if REPORT_TIME
  if(sot_running) {
    steponlytime.Start();
  }
#endif // REPORT_TIME

  if(total_energy_density_output.GetCacheRequestCount()>0) {
    // Energy density field output requested.  Copy results
    // to output cache.
    total_energy_density_output.cache.state_id=0;
    total_energy_density_output.cache.value = energy;
    total_energy_density_output.cache.state_id=state.Id();
  }

  if(field_cache!=NULL) {
    if(field_cache!=H_fill) {
      // Field requested by both H_req and total_field_output,
      // so copy from H_req to field_cache.
      *field_cache = *H_fill;
    }
    field_cache=NULL; // Safety
    total_field_output.cache.state_id=state.Id();
  }

  // Store total energy sum if output object total_energy_output
  // has cache enabled.
  if (total_energy_output.GetCacheRequestCount()>0) {
    total_energy_output.cache.state_id=0;
    total_energy_output.cache.value=oced.energy_sum;
    total_energy_output.cache.state_id=state.Id();
  }

  pE_pt = oced.pE_pt;  // Export pE_pt value
  total_E = oced.energy_sum; // Export total energy
}

void Oxs_TimeEvolver::UpdateEnergyOutputs(const Oxs_SimState& state)
{
  if(state.Id()==0) { // Safety
    return;
  }
  Oxs_MeshValue<OC_REAL8m> energy(state.mesh);
  OC_REAL8m pE_pt;
  GetEnergyDensity(state,energy,NULL,NULL,pE_pt);
}

void Oxs_TimeEvolver::FillEnergyCalcCountOutput(const Oxs_SimState& state)
{
  energy_calc_count_output.cache.state_id=state.Id();
  energy_calc_count_output.cache.value
    = static_cast<OC_REAL8m>(energy_calc_count);
}
