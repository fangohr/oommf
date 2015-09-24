/* FILE: energy.cc                 -*-Mode: c++-*-
 *
 * Abstract energy class, derived from Oxs_Ext class.  Note: The
 * implementation of the Oxs_ComputeEnergies() friend function is
 * in the file chunkenergy.cc.
 */

#include <assert.h>
#include <string>

#include "energy.h"
#include "mesh.h"

OC_USE_STRING;

/* End includes */

#ifdef EXPORT_CALC_COUNT
void Oxs_Energy::FillCalcCountOutput(const Oxs_SimState& state)
{
  calc_count_output.cache.state_id = state.Id();
  calc_count_output.cache.value    = static_cast<OC_REAL8m>(calc_count);
}
#endif // EXPORT_CALC_COUNT



void Oxs_Energy::SetupOutputs()
{
  energy_sum_output.Setup(this,InstanceName(),"Energy","J",1,
                          &Oxs_Energy::UpdateStandardOutputs);
  field_output.Setup(this,InstanceName(),"Field","A/m",1,
                     &Oxs_Energy::UpdateStandardOutputs);
  energy_density_output.Setup(this,InstanceName(),"Energy density","J/m^3",1,
                     &Oxs_Energy::UpdateStandardOutputs);
#ifdef EXPORT_CALC_COUNT
  calc_count_output.Setup(this,InstanceName(),"Calc count","",0,
                          &Oxs_Energy::FillCalcCountOutput);
#endif // EXPORT_CALC_COUNT
  // Note: MS VC++ 6.0 requires fully qualified member names

  // Question: Do we want to add a mxH output?

  // Register outputs
  energy_sum_output.Register(director,0);
  field_output.Register(director,0);
  energy_density_output.Register(director,0);
#ifdef EXPORT_CALC_COUNT
  calc_count_output.Register(director,0);
#endif // EXPORT_CALC_COUNT

  // Eventually, caching should be handled by controlling Tcl script.
  // Until then, request caching of scalar energy output by default.
  energy_sum_output.CacheRequestIncrement(1);
}

// Constructors
Oxs_Energy::Oxs_Energy
( const char* name,     // Child instance id
  Oxs_Director* newdtr  // App director
  ) : Oxs_Ext(name,newdtr),calc_count(0)
{ SetupOutputs(); }

Oxs_Energy::Oxs_Energy
( const char* name,     // Child instance id
  Oxs_Director* newdtr, // App director
  const char* argstr    // MIF block argument string
  ) : Oxs_Ext(name,newdtr,argstr),calc_count(0)
{ SetupOutputs(); }

//Destructor
Oxs_Energy::~Oxs_Energy() {
#if REPORT_TIME
  Oc_TimeVal cpu,wall;
  energytime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"GetEnergy time (secs)%7.2f cpu /%7.2f wall,"
            " module %.1000s (%u evals)\n",double(cpu),double(wall),
            InstanceName(),GetEnergyEvalCount());
  }
#endif // REPORT_TIME
}

// Default problem initializer routine.  Any child that redefines
// this function should embed a call to this Init() inside
// the child specific version.
OC_BOOL Oxs_Energy::Init()
{
  if(!Oxs_Ext::Init()) return 0;

#if REPORT_TIME
  Oc_TimeVal cpu,wall;
  energytime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"GetEnergy time (secs)%7.2f cpu /%7.2f wall,"
            " module %.1000s (%u evals)\n",double(cpu),double(wall),
            InstanceName(),GetEnergyEvalCount());
  }
  energytime.Reset();
#endif // REPORT_TIME

  calc_count=0;
  return 1;
}


// Standard output object update interface
void Oxs_Energy::UpdateStandardOutputs(const Oxs_SimState& state)
{
  if(state.Id()==0) { // Safety
    return;
  }

#if REPORT_TIME
  energytime.Start();
#endif // REPORT_TIME

  Oxs_ComputeEnergyData oced(state);

  // Dummy buffer space.
  Oxs_MeshValue<OC_REAL8m> dummy_energy;
  Oxs_MeshValue<ThreeVector> dummy_field;
  oced.scratch_energy = &dummy_energy;
  oced.scratch_H      = &dummy_field;

  if(energy_density_output.GetCacheRequestCount()>0) {
    energy_density_output.cache.state_id=0;
    oced.scratch_energy = oced.energy = &energy_density_output.cache.value;
    oced.energy->AdjustSize(state.mesh);
  }

  if(field_output.GetCacheRequestCount()>0) {
    field_output.cache.state_id=0;
    oced.scratch_H = oced.H = &field_output.cache.value;
    oced.H->AdjustSize(state.mesh);
  }

  ++calc_count;
  ComputeEnergy(state,oced);

  if(energy_density_output.GetCacheRequestCount()>0) {
    energy_density_output.cache.state_id=state.Id();
  }
  if(field_output.GetCacheRequestCount()>0) {
    field_output.cache.state_id=state.Id();
  }
  if(energy_sum_output.GetCacheRequestCount()>0) {
    energy_sum_output.cache.value=oced.energy_sum;
    energy_sum_output.cache.state_id=state.Id();
  }

#if REPORT_TIME
  energytime.Stop();
#endif // REPORT_TIME

}

///////////////////////////////////////////////////////////////////
// Energy and energy derivatives calculation function.  This
// should be "conceptually const."  The energy return is average
// energy density for the corresponding cell, in J/m^3.  The
// field is in A/m, pE_pt is in J/s.
//   There are two GetEnergy functions.  The first is a
// private, virtual member function of Oxs_Energy.  It takes as
// imports Oxs_MeshValue references into which to store the
// energy and H results.  The second is a non-virtual public
// member function, GetEnergyData, that passes in Oxs_MeshValue
// references that *may* be used to store the results, and
// separate parameters that return pointers to where the
// results actually were stored.  If output caching in enabled,
// the Oxs_Energy base class calls the virtual GetEnergy
// function with references to the output cache Oxs_MeshValue
// objects.  Otherwise the import buffer space is used.
//   NOTE: The pE_pt export is the partial derivative of energy
// with respect to time.  For most energy terms this will be
// 0.  It will only be non-zero if there is an explicit dependence
// on time, as for example with a time-varying applied field.
void Oxs_Energy::GetEnergyData
(const Oxs_SimState& state,
 Oxs_EnergyData& oed
 )
{
  if(oed.energy_buffer==NULL || oed.field_buffer==NULL) {
    // Bad input
    String msg = String("Oxs_EnergyData object in function"
                        " Oxs_Energy::GetEnergyData"
                        " contains NULL buffer pointers.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  if(state.Id()==0) {
    String msg = String("Programming error:"
                        " Invalid (unlocked) state detected"
                        " in Oxs_Energy::GetEnergyData.");
    throw Oxs_ExtError(this,msg.c_str());
  }

#if REPORT_TIME
  energytime.Start();
#endif // REPORT_TIME

  if(field_output.GetCacheRequestCount()>0) {
    field_output.cache.state_id=0;
    oed.field_buffer = &field_output.cache.value;
  }

  if(energy_density_output.GetCacheRequestCount()>0) {
    energy_density_output.cache.state_id=0;
    oed.energy_buffer = &energy_density_output.cache.value;
  }

  oed.energy_buffer->AdjustSize(state.mesh);
  oed.field_buffer->AdjustSize(state.mesh);

  ++calc_count;
  GetEnergy(state,oed);

  if(field_output.GetCacheRequestCount()>0) {
    field_output.cache.state_id=state.Id();
  }
  if(energy_density_output.GetCacheRequestCount()>0) {
    energy_density_output.cache.state_id=state.Id();
  }
  if(energy_sum_output.GetCacheRequestCount()>0) {
    if(oed.energy_sum.IsSet()) {
      energy_sum_output.cache.value=oed.energy_sum;
    } else {
      OC_INDEX size = state.mesh->Size();
      Nb_Xpfloat energy_sum=0;
      const Oxs_MeshValue<OC_REAL8m>& ebuf = *oed.energy;
      const Oxs_Mesh& mesh = *state.mesh;
      for(OC_INDEX i=0; i<size; ++i) {
        energy_sum += ebuf[i] * mesh.Volume(i);
      }
      energy_sum_output.cache.value=energy_sum.GetValue();
      oed.energy_sum.Set(energy_sum.GetValue()); // Might as well set
      /// this field if we are doing the calculation anyway.
    }
    energy_sum_output.cache.state_id=state.Id();
  }
#if REPORT_TIME
  energytime.Stop();
#endif // REPORT_TIME
}


////////////////////////////////////////////////////////////////////////
// The ComputeEnergy interface replaces the older GetEnergy interface.
// The parameter list is similar, but ComputeEnergy uses the
// Oxs_ComputeEnergyData data struct in place Oxs_EnergyData.  The
// state_id, scratch_energy and scratch_H members of
// Oxs_ComputeEnergyData must be set on entry to ComputeEnergy.  The
// scratch_* members must be non-NULL, but the underlying
// Oxs_MeshValue objects will be size adjusted as (and if) needed.
// The scratch_* members are need for backward compatibility with
// older (pre Oct 2008) Oxs_Energy child classes, but also for
// Oxs_Energy classes like Oxs_Demag that always require space for
// field output.  Member "scratch_energy" is expressly allowed to be
// the same as member "energy", and likewise for "scratch_H" and "H".
//
// The remaining Oxs_MeshValue pointers are output requests.  They can
// be NULL, in which case the output is not requested, or non-NULL, in
// which case output is requested.  If output is requested, then the
// corresponding Oxs_MeshValue object will be filled.  (Note that the
// usual ComputeEnergy caller, AccumEnergyAndTorque, may adjust some
// of these pointers to point into Oxs_Energy standard output cache
// space, but the ComputeEnergy function itself plays no such games.)
// Any of these members that are non-NULL must be pre-sized
// appropriately for the given mesh.  This sizing is done automatically
// by AccumEnergyAndTorque for the "energy", "H", and "mxH" members,
// but not for the "accum" members.
//
// The remaining members, energy_sum and pE_pt are exports that are
// always filled by ComputeEnergy.
//
// The main practical advantage of ComputeEnergy over GetEnergy
// is that use of the "accum" fields can allow significant reduction
// in memory bandwidth use in evolvers.  This can be especially
// important in multi-threaded settings.
//
// The following is a default implementation of the virtual
// ComputeEnergy function.  It is effectively a wrapper/adapter
// to the deprecated GetEnergy function.  New Oxs_Energy child
// classes should override this function with a child-specific
// version, and define their GetEnergy function as a simple wrapper
// to GetEnergyAlt (q.v.).
//
void Oxs_Energy::ComputeEnergy
(const Oxs_SimState& state,
 Oxs_ComputeEnergyData& oced) const
{
  const Oxs_Mesh* mesh = state.mesh;

  if(oced.scratch_energy==NULL || oced.scratch_H==NULL) {
    // Bad input
    String msg = String("Oxs_ComputeEnergyData object in function"
                        " Oxs_Energy::ComputeEnergy"
                        " contains NULL scratch pointers.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  if((oced.energy_accum!=0 && !oced.energy_accum->CheckMesh(mesh))
     || (oced.H_accum!=0   && !oced.H_accum->CheckMesh(mesh))
     || (oced.mxH_accum!=0 && !oced.mxH_accum->CheckMesh(mesh))
     || (oced.energy!=0    && !oced.energy->CheckMesh(mesh))
     || (oced.H!=0         && !oced.H->CheckMesh(mesh))
     || (oced.mxH!=0       && !oced.mxH->CheckMesh(mesh))) {
    // Bad input
    String msg = String("Oxs_ComputeEnergyData object in function"
                        " Oxs_Energy::ComputeEnergy"
                        " contains ill-sized buffers.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  Oxs_EnergyData oed(state);
  if(oced.energy) oed.energy_buffer = oced.energy;
  else            oed.energy_buffer = oced.scratch_energy;
  if(oced.H)      oed.field_buffer  = oced.H;
  else            oed.field_buffer  = oced.scratch_H;

  // Although not stated in the interface docs, some Oxs_Energy children
  // assume that the oed energy and field buffers are pre-sized on
  // entry.  For backwards compatibility, make this so.
  oed.energy_buffer->AdjustSize(mesh);
  oed.field_buffer->AdjustSize(mesh);

  GetEnergy(state,oed);

  // Accum as requested
  OC_INDEX i;
  const OC_INDEX size = mesh->Size();

  const OC_BOOL have_energy_sum = oed.energy_sum.IsSet();
  if(have_energy_sum) oced.energy_sum = oed.energy_sum;
  else                oced.energy_sum = 0.0;

  if(oced.energy_accum) {
    Oxs_MeshValue<OC_REAL8m>& energy_accum = *oced.energy_accum;
    const Oxs_MeshValue<OC_REAL8m>& energy = *(oed.energy.Get());
    for(i=0; i<size; ++i) {
      energy_accum[i] += energy[i];
      if(!have_energy_sum) {
        oced.energy_sum += energy[i] * mesh->Volume(i);
      }
    }
  } else if(!have_energy_sum) {
    const Oxs_MeshValue<OC_REAL8m>& energy = *(oed.energy.Get());
    for(i=0; i<size; ++i) {
      oced.energy_sum += energy[i] * mesh->Volume(i);
    }
  }

  const Oxs_MeshValue<ThreeVector>& spin = state.spin;
  const Oxs_MeshValue<ThreeVector>& H = *(oed.field.Get());
  if(oced.mxH_accum && oced.H_accum) {
    for(i=0; i<size; ++i) {
      (*oced.H_accum)[i] += H[i];
      ThreeVector temp = spin[i];
      temp ^= H[i];
      (*oced.mxH_accum)[i] += temp;
    }
  } else if(oced.mxH_accum) {
    for(i=0; i<size; ++i) {
      ThreeVector temp = spin[i];
      temp ^= H[i];
      (*oced.mxH_accum)[i] += temp;
    }
  } else if(oced.H_accum) {
    (*oced.H_accum) += H;
  }

  // Copy energy and field results, as needed
  if(oced.energy && oced.energy != oed.energy.Get()) (*oced.energy) = (*oed.energy);
  if(oced.H      && oced.H      != oed.field.Get())  (*oced.H)      = (*oed.field);

  // mxH requested?
  if(oced.mxH) {
    for(i=0; i<size; ++i) {
      ThreeVector temp = spin[i];
      temp ^= H[i];
      (*oced.mxH)[i] = temp;
    }
  }

  // pE_pt
 if(oed.pE_pt.IsSet()) oced.pE_pt = oed.pE_pt;
 else                  oced.pE_pt = 0.0;
}

////////////////////////////////////////////////////////////////////////
// GetEnergyAlt is an adapter that allows a child-defined ComputeEnergy
// class to provide GetEnergy services.  Such children can define their
// GetEnergy to be a simple wrapper to GetEnergyAlt.  NOTE: Children
// must NOT wrap GetEnergyAlt with GetEnergy without also overriding the
// default ComputeEnergy implementation.  Otherwise an infinite circular
// call sequence will result:
//  GetEnergy -> GetEnergyAlt -> ComputeEnergy -> GetEnergy -> ...
//
void Oxs_Energy::GetEnergyAlt
(const Oxs_SimState& state,
 Oxs_EnergyData& oed) const
{
  if(oed.energy_buffer==NULL || oed.field_buffer==NULL) {
    // Bad input
    String msg = String("Oxs_EnergyData object in function"
                        " Oxs_Energy::GetEnergyAlt"
                        " contains NULL buffer pointers.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  Oxs_ComputeEnergyData oced(state);

  oed.energy = oced.energy = oced.scratch_energy = oed.energy_buffer;
  oed.field  = oced.H      = oced.scratch_H      = oed.field_buffer;

  oced.energy->AdjustSize(state.mesh);
  oced.H->AdjustSize(state.mesh);

  ComputeEnergy(state,oced);

  oed.energy_sum = oced.energy_sum;
  oed.pE_pt      = oced.pE_pt;
}


////////////////////////////////////////////////////////////////////////
// The AccumEnergyAndTorque method is similar to GetEnergyData, except
// that the former uses the newer ComputeEnergy interface rather than
// the deprecated GetEnergy interface.
//
// The "oced.scratch_*" members must be filled before entry.  They
// point to scratch space that may or may not be used.  This space
// will be resized as needed.  If desired, "scratch_energy" may point
// to the same place as "energy", and likewise for "scratch_H" and
// "H".
//
// The remaining Oxs_MeshValue pointers are output requests.  If one
// of these pointers is NULL on entry, then that output is requested.
// The output will to into the pointed-to space, *unless* that output
// is associated with one of the Oxs_Energy standard outputs
// (energy_density_output or field_output) and caching of that output
// is enabled, in which case the output will go into the output cache
// and the corresponding oced pointer (energy or H) will be changed to
// point to the cache.  Pay ATTENTION to this point: the arrays sent
// in *may* not be the ones used, so clients should always check and
// use the oced pointers directly, rather than the arrays values sent
// in.  Also, if caching is enabled, then on return the energy and/or
// H pointers in oced will be set to the cache, even if energy and/or
// H were set to NULL on entry.
//
// The oced.*_accum members are accumulated (added) into rather than
// set.
//
void Oxs_Energy::AccumEnergyAndTorque
(const Oxs_SimState& state,
 Oxs_ComputeEnergyData& oced)
{
  if(state.Id()==0) {
    String msg = String("Programming error:"
                        " Invalid (unlocked) state detected"
                        " in Oxs_Energy::AccumEnergyAndTorque");
    throw Oxs_ExtError(this,msg.c_str());
  }

  if(oced.scratch_energy==NULL || oced.scratch_H==NULL) {
    // Bad input
    String msg = String("Oxs_ComputeEnergyData object in function"
                        " Oxs_Energy::AccumEnergyAndTorque"
                        " contains NULL scratch pointers.");
    throw Oxs_ExtError(this,msg.c_str());
  }

  if((oced.energy_accum!=0 && !oced.energy_accum->CheckMesh(state.mesh))
     || (oced.H_accum!=0   && !oced.H_accum->CheckMesh(state.mesh))
     || (oced.mxH_accum!=0 && !oced.mxH_accum->CheckMesh(state.mesh))) {
    // Bad input
    String msg = String("Oxs_ComputeEnergyData object in function"
                        " Oxs_Energy::AccumEnergyAndTorque"
                        " contains ill-sized accum buffers.");
    throw Oxs_ExtError(this,msg.c_str());
  }

#if REPORT_TIME
  energytime.Start();
#endif // REPORT_TIME

  if(energy_density_output.GetCacheRequestCount()>0) {
    energy_density_output.cache.state_id=0;
    oced.energy = &energy_density_output.cache.value;
  }

  if(field_output.GetCacheRequestCount()>0) {
    field_output.cache.state_id=0;
    oced.H = &field_output.cache.value;
  }

  if(oced.energy) oced.energy->AdjustSize(state.mesh);
  if(oced.H)      oced.H->AdjustSize(state.mesh);
  if(oced.mxH)    oced.mxH->AdjustSize(state.mesh);

  ++calc_count;
  ComputeEnergy(state,oced);

  if(field_output.GetCacheRequestCount()>0) {
    field_output.cache.state_id=state.Id();
  }
  if(energy_density_output.GetCacheRequestCount()>0) {
    energy_density_output.cache.state_id=state.Id();
  }
  if(energy_sum_output.GetCacheRequestCount()>0) {
    energy_sum_output.cache.value=oced.energy_sum;
    energy_sum_output.cache.state_id=state.Id();
  }
#if REPORT_TIME
  energytime.Stop();
#endif // REPORT_TIME
}

