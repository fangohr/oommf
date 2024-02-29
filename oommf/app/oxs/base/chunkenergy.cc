/* FILE: chunkenergy.cc                 -*-Mode: c++-*-
 *
 * Abstract chunk energy class, derived from Oxs_Energy class.  This
 * file also contains the implementation of the Oxs_ComputeEnergies()
 * friend function.
 */

#include <cassert>
#include <string>

#include "chunkenergy.h"
#include "director.h"
#include "energy.h"
#include "mesh.h"

OC_USE_STRING;

/* End includes */

////////////////////////////////////////////////////////////////////////
///////////////////////// CHUNK ENERGY /////////////////////////////////

struct Oxs_ComputeEnergies_ChunkStruct {
public:
  Oxs_ChunkEnergy* energy;
  Oxs_ComputeEnergyDataThreaded ocedt;
  Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux> thread_ocedtaux;

  Oxs_ComputeEnergies_ChunkStruct
  (Oxs_ChunkEnergy* import_energy,
   const Oxs_ComputeEnergyDataThreaded& import_ocedt,
   int thread_count)
    : energy(import_energy), ocedt(import_ocedt),
      thread_ocedtaux(thread_count) {}

  Oxs_ComputeEnergies_ChunkStruct(const Oxs_ComputeEnergies_ChunkStruct&)
     = default;

  Oxs_ComputeEnergies_ChunkStruct&
  operator=(const Oxs_ComputeEnergies_ChunkStruct& right)
  { // The const ocedt element blocks creation of a default assignment
    // operator.  Workaround using default copy constructor.
    if (this == &right) return *this;
    this->~Oxs_ComputeEnergies_ChunkStruct();
    new (this) Oxs_ComputeEnergies_ChunkStruct(right);
    return *this;
  }

};

class Oxs_ComputeEnergiesChunkThread : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket;
  /// job_basket is static, so only one "set" of this class is allowed.

  const Oxs_SimState* state;
  std::vector<Oxs_ComputeEnergies_ChunkStruct>* energy_terms;

  Oxs_MeshValue<ThreeVector>* mxH;
  Oxs_MeshValue<ThreeVector>* mxH_accum;
  Oxs_MeshValue<ThreeVector>* mxHxm;
  const std::vector<OC_INDEX>* fixed_spins;
  std::vector<OC_REAL8m> max_mxH;

  OC_INDEX cache_blocksize;

  OC_BOOL accums_initialized;

  Oxs_ComputeEnergiesChunkThread()
    : state(0),
      mxH(0),mxH_accum(0),
      mxHxm(0), fixed_spins(0),
      cache_blocksize(0), accums_initialized(0) {}

  void Cmd(int threadnumber, void* data);

  static void Init(int thread_count,
                   const Oxs_StripedArray<OC_REAL8m>* arrblock) {
    job_basket.Init(thread_count,arrblock);
  }

  Oxs_ComputeEnergiesChunkThread(const Oxs_ComputeEnergiesChunkThread&)
     = default;
  Oxs_ComputeEnergiesChunkThread&
     operator=(const Oxs_ComputeEnergiesChunkThread&) = default;
};

Oxs_JobControl<OC_REAL8m> Oxs_ComputeEnergiesChunkThread::job_basket;

void
Oxs_ComputeEnergiesChunkThread::Cmd
(int threadnumber,
 void* /* data */)
{
  OC_REAL8m max_mxH_sq = 0.0;
  OC_REAL8m max_mxH_sq_invscale = 1.0;
  const Oxs_MeshValue<ThreeVector>& spin = state->spin;
  const Oxs_MeshValue<OC_REAL8m>& Ms = *(state->Ms);

  // In chunk post-processing segment, the torque at fixed spins
  // is forced to zero.  Variable i_fixed holds the latest working
  // position in the fixed_spins array between chunks.
  OC_INDEX i_fixed = 0;
  OC_INDEX i_fixed_total = 0;
  if(fixed_spins) i_fixed_total = fixed_spins->size();

  // Local vector to hold Oxs_ComputeEnergyDataThreadedAux results.
  // These data are copied over into this->energy_terms at the end.
  Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>
    eit_ocedtaux(energy_terms->size());

  while(1) {
    // Claim a chunk
    OC_INDEX index_start,index_stop;
    job_basket.GetJob(threadnumber,index_start,index_stop);

    if(index_start>=index_stop) break;

    // We claim by blocksize, but work by cache_blocksize (which is
    // presumably smaller).  We want blocksize big enough to reduce
    // mutex collisions and other overhead, but cache_blocksize small
    // enough that the spin data can reside in cache acrosss all the
    // energy terms.

    for(OC_INDEX icache_start=index_start;
        icache_start<index_stop; icache_start+=cache_blocksize) {
      OC_INDEX icache_stop = icache_start + cache_blocksize;
      if(icache_stop>index_stop) icache_stop = index_stop;

      // Process chunk
      OC_INDEX energy_item = 0;
      for(std::vector<Oxs_ComputeEnergies_ChunkStruct>::iterator eit
            = energy_terms->begin();
          eit != energy_terms->end() ; ++eit, ++energy_item) {

        // Set up some refs for convenience
        Oxs_ChunkEnergy& eterm = *(eit->energy);
        Oxs_ComputeEnergyDataThreaded ocedt = eit->ocedt; // Local copy
        Oxs_ComputeEnergyDataThreadedAux& ocedtaux = eit_ocedtaux[energy_item];
        if(!accums_initialized && energy_item==0) {
          // Note: Each thread has its own copy of the ocedt and
          // ocedtaux data, so we can tweak these as desired without
          // stepping on other threads

          // Move each accum pointer to corresponding non-accum member
          // for initialization.
          assert(ocedt.mxH == 0);
          Oxs_MeshValue<OC_REAL8m>* energy_accum_save = ocedt.energy_accum;
          if(ocedt.energy == 0) ocedt.energy = ocedt.energy_accum;
          ocedt.energy_accum = 0;

          Oxs_MeshValue<ThreeVector>* H_accum_save = ocedt.H_accum;
          if(ocedt.H == 0)      ocedt.H      = ocedt.H_accum;
          ocedt.H_accum      = 0;

          // Note: ocedt.mxH should always be zero, but check here anyway
          // for easier code maintenance.
          Oxs_MeshValue<ThreeVector>* mxH_accum_save = ocedt.mxH_accum;
          if(ocedt.mxH == 0)    ocedt.mxH    = ocedt.mxH_accum;
          ocedt.mxH_accum    = 0;

          eterm.ComputeEnergyChunk(*state,ocedt,ocedtaux,
                                   icache_start,icache_stop,
                                   threadnumber);

          // Copy data as necessary
          if(energy_accum_save) {
            if(ocedt.energy != energy_accum_save) {
              for(OC_INDEX i=icache_start;i<icache_stop;++i) {
                (*energy_accum_save)[i] = (*(ocedt.energy))[i];
              }
            } else {
              ocedt.energy = 0;
            }
          }
          ocedt.energy_accum = energy_accum_save;

          if(H_accum_save) {
            if(ocedt.H != H_accum_save) {
              for(OC_INDEX i=icache_start;i<icache_stop;++i) {
                (*H_accum_save)[i] = (*(ocedt.H))[i];
              }
            } else {
              ocedt.H = 0;
            }
          }
          ocedt.H_accum = H_accum_save;

          if(mxH_accum_save) {
            if(ocedt.mxH != mxH_accum_save) {
              // This branch should never run
              abort();
            } else {
              ocedt.mxH = 0;
            }
          }
          ocedt.mxH_accum = mxH_accum_save;

        } else {
          // Standard processing: accum elements already initialized.
          eterm.ComputeEnergyChunk(*state,ocedt,ocedtaux,
                                   icache_start,icache_stop,
                                   threadnumber);
        }
      }

      // Post-processing, for this energy term and chunk.

      // Zero torque on fixed spins.  This code assumes that, 1) the
      // fixed_spins list is sorted in increasing order, and 2) the
      // chunk indices come in strictly monotonically increasing
      // order.
      // NB: Outside this loop, "i_fixed" stores the search start
      // location for the next chunk.
      while(i_fixed < i_fixed_total) {
        OC_INDEX index = (*fixed_spins)[i_fixed];
        if(index <  icache_start) { ++i_fixed; continue; }
        if(index >= icache_stop) break;
        if(mxH)       (*mxH)[index].Set(0.,0.,0.);
        if(mxH_accum) (*mxH_accum)[index].Set(0.,0.,0.);
        ++i_fixed;
      }

      // Note: The caller must pre-size mxHxm as appropriate.
      OC_REAL8m max_mxH_sq_cache = 0.0;
      if(mxHxm) {
        // Compute mxHxm and max_mxH
        if(mxH_accum == mxHxm) {
          for(OC_INDEX i=icache_start;i<icache_stop;++i) {
            if(Ms[i]==0.0) { // Ignore zero-moment spins
              (*mxHxm)[i].Set(0.,0.,0.);
              continue;
            }
            OC_REAL8m tx = (*mxHxm)[i].x;
            OC_REAL8m ty = (*mxHxm)[i].y;
            OC_REAL8m tz = (*mxHxm)[i].z;
            OC_REAL8m mx = spin[i].x;
            OC_REAL8m my = spin[i].y;
            OC_REAL8m mz = spin[i].z;

            OC_REAL8m magsq = tx*tx + ty*ty + tz*tz;
            if(magsq > max_mxH_sq_cache) max_mxH_sq_cache = magsq;

            (*mxHxm)[i].x = ty*mz - tz*my;
            (*mxHxm)[i].y = tz*mx - tx*mz;
            (*mxHxm)[i].z = tx*my - ty*mx;
          }
        } else {
          // It is the responsibility of the caller to make certain
          // that if mxHxm is non-zero, then so is mxH_accum.
          assert(mxH_accum != 0);
          for(OC_INDEX i=icache_start;i<icache_stop;++i) {
            if(Ms[i]==0.0) { // Ignore zero-moment spins
              (*mxH_accum)[i].Set(0.,0.,0.);
              (*mxHxm)[i].Set(0.,0.,0.);
              continue;
            }
            OC_REAL8m tx = (*mxH_accum)[i].x;
            OC_REAL8m ty = (*mxH_accum)[i].y;
            OC_REAL8m tz = (*mxH_accum)[i].z;
            OC_REAL8m mx = spin[i].x;
            OC_REAL8m my = spin[i].y;
            OC_REAL8m mz = spin[i].z;

            OC_REAL8m magsq = tx*tx + ty*ty + tz*tz;
            if(magsq > max_mxH_sq_cache) max_mxH_sq_cache = magsq;

            (*mxHxm)[i].x = ty*mz - tz*my;
            (*mxHxm)[i].y = tz*mx - tx*mz;
            (*mxHxm)[i].z = tx*my - ty*mx;
          }
        }
      } else if(mxH_accum) {
          for(OC_INDEX i=icache_start;i<icache_stop;++i) {
            if(Ms[i]==0.0) { // Ignore zero-moment spins
              (*mxH_accum)[i].Set(0.,0.,0.);
              continue;
            }
            OC_REAL8m tx = (*mxH_accum)[i].x;
            OC_REAL8m ty = (*mxH_accum)[i].y;
            OC_REAL8m tz = (*mxH_accum)[i].z;
            OC_REAL8m magsq = tx*tx + ty*ty + tz*tz;
            if(magsq > max_mxH_sq_cache) max_mxH_sq_cache = magsq;
          }
      }
      // Otherwise, don't compute max_mxH

      // Overflow check
      OC_REAL8m cache_invscale = 1.0;
      if(!isfinite(max_mxH_sq_cache)) {
        // Overflow. Try to recompute with scaling.
        assert(mxH_accum != nullptr);
        max_mxH_sq_cache = max_mxH_sq;  // Current overall max
        cache_invscale = max_mxH_sq_invscale;
        const OC_REAL8m testval = OC_SQRT_REAL8m_MAX;
        for(OC_INDEX i=icache_start;i<icache_stop;++i) {
          if(Ms[i]==0.0) { // Ignore zero-moment spins
            continue;
          }
          OC_REAL8m tx = (*mxH_accum)[i].x;
          OC_REAL8m ty = (*mxH_accum)[i].y;
          OC_REAL8m tz = (*mxH_accum)[i].z;
          OC_REAL8m scale = fabs(tx)+fabs(ty)+fabs(tz);
          if(scale>=testval) {
            // Potentially overflow on squaring.
            // We know overflow occurred for at least one i, so we only
            // need to check overflow candidates.
            if(!isfinite(scale)) {
              // Unrecoverable overflow
              Oxs_ExtError("Floating point overflow detected"
                           " in torque computation.");
            }
            OC_REAL8m invscale = 1.0/scale;
            tx *= invscale; ty*=invscale; tz*=invscale;
            OC_REAL8m magsq = tx*tx + ty*ty + tz*tz;
            OC_REAL8m scale_ratio = Nb_NOP(invscale / cache_invscale);
            if(Nb_NOP(magsq/scale_ratio)
               > Nb_NOP(scale_ratio*max_mxH_sq_cache)) {
              max_mxH_sq_cache = magsq;
              cache_invscale = invscale;
            }
          }
        }
      }

      // If torque is computed, then track maximum value
      OC_REAL8m scale_ratio = Nb_NOP(cache_invscale / max_mxH_sq_invscale);
      if(Nb_NOP(max_mxH_sq_cache/scale_ratio)
         > Nb_NOP(scale_ratio*max_mxH_sq)) {
        max_mxH_sq = max_mxH_sq_cache;
        max_mxH_sq_invscale = cache_invscale;
      }

    }
  }

  // Copy out thread scalar results
  max_mxH[threadnumber] = sqrt(max_mxH_sq)/max_mxH_sq_invscale;
  for(size_t ei=0;ei<energy_terms->size();++ei) {
    (*energy_terms)[ei].thread_ocedtaux[threadnumber] = eit_ocedtaux[ei];
  }
}

#if REPORT_TIME
Nb_StopWatch Oxs_ChunkEnergy::chunktime;

void Oxs_ChunkEnergy::ReportTime()
{
  Oc_TimeVal cpu,wall;
  chunktime.GetTimes(cpu,wall);
  if(double(wall)>0.0) {
    fprintf(stderr,"GetEnergy time (secs)%7.2f cpu /%7.2f wall,"
            " ChunkEnergies total (%u evals)\n",
            double(cpu),double(wall),GetEnergyEvalCount());
    chunktime.Reset();  // Only print once (per run).
  }
}
#endif // REPORT_TIME

///////////////////////// CHUNK ENERGY /////////////////////////////////
////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////
//////////////////////// OXS_COMPUTEENERGIES ///////////////////////////

OC_REAL8m Oxs_ComputeEnergiesErrorEstimate
(const Oxs_SimState& state,
 OC_REAL8m energy_density_error_estimate,
 OC_REAL8m energy_sum)
{
  if(energy_density_error_estimate>=0.0) {
    return energy_density_error_estimate;
  }
  OC_REAL8m term_energy_density = fabs(energy_sum);
  const OC_REAL8m simulation_volume = state.mesh->TotalVolume();
  if(simulation_volume >= 1.0
     || term_energy_density < OC_REAL8m_MAX*simulation_volume) {
    term_energy_density /= simulation_volume;
    // Cells with Ms=0 don't contribute to energy sum, so we might
    // want to replace state.mesh.TotalVolume() with a value
    // (possibly weighted?) representing the magnetically active
    // volume.  OTOH, ideally energy terms should fill in the energy
    // density error estimate in oced.
  } else {
    if(simulation_volume == 0.0) {
      term_energy_density = 0.0;
    } else {
      term_energy_density = OC_REAL8m_MAX/256; // Punt
    }
  }
  return term_energy_density*Oxs_ComputeEnergyData::edee_round_error;
}

// TODO: Move Oxs_ComputeEnergies into a class, with an init function
//       that can be given a pointer to the director.  Then the
//       computation member function can access both attach requests and
//       the list of energies itself. We might want to break the
//       computation member function into two parts, one which queries
//       the director for attach requests and list of energies, and then
//       a second which takes that as import and performs the
//       computation.  That way, if an output request for example needs
//       to re-compute energy on a state it can do so while computing
//       only the needed energies and w/o attempts to overwrite already
//       attached quantities.
//         The init function could also advertise the
//       well-known-quantities that the compute routine can attach.

void Oxs_ComputeEnergies
(const Oxs_ComputeEnergiesImports& ocei,
 Oxs_ComputeEnergiesExports& ocee)
{ // Compute sums of energies, fields, and/or torques for all energies
  // in "energies" import.  On entry, ocee.scratch_energy and
  // ocee.scratch_H must be non-null. ocee.energy, ocee.H, ocee.mxH, and
  // ocee.mxHxm can be null or set as desired.  Non-null fields will be
  // filled. Output can also be generated based on attachment requests.
  // All output arrays are automatically adjusted to meet the mesh size.
  // Energy calculation counts and timers are also handled
  // appropriately.
  //
  //  This routine provides attachment of the following quantities:
  //    total_energy_density
  //    total_H
  //    total_mxH
  //    total_mxHxm
  // Note that it does NOT provide support for attaching dm_dt.
  // TODO: Figure out how to advertise the above.
  //
  //  The individual energy terms know how to fill or accumulate into
  // energy density, H, and mxH arrays. When possible, the fill
  // interfaces are used on the first pass to initialized the ocee
  // arrays, and the accumulate interfaces are used thereafter. This means
  // that ocee.energy and friends pointers are copied into the "non-accum"
  // members of the individual energy term import struct for the first
  // pass, but into the "accum" members thereafter.
  //
  //  Furthermore, the energy terms don't support direct computation
  // of mxHxm. Instead, if mxHxm output is requested, then the energy
  // terms are asked to compute mxH, and then on the backside of
  // the chunk energy computations mxHxm is computed from mxH, while
  // the mxH values are in (chunk) cache.
  //
  //  If mxH output is not requested, but mxHxm output is, then the the
  // mxH pointer is aliased to mxHxm. Then when the energy terms fill
  // mxH they are actually filling the space for mxHxm, and the backsize
  // mxHxm computation works in-place on mxHxm rather than across two
  // arrays (mxH and mxHxm) which reduces memory bandwidth use. The
  // threaded chunk energy computation handles this by checking if mxH
  // and mxHxm are aliased, and responds accordingly.
  //
  //   Those "energies" members that are actually Oxs_ChunkEnergies will
  // use the ComputeEnergyChunk interface in a collated fashion to help
  // minimize memory bandwidth usage.  On threaded OOMMF builds, these
  // calls will be run in parallel and load balanced.  Also, the number
  // of threads launched will not exceed the number of chunks.  This
  // insures that the main thread (threadnumber == 0) has an opportunity
  // to run for any older Oxs_ChunkEnergies that perform initialization
  // in their Oxs_ChunkEnergy::ComputeEnergyChunk() routine.  This is a
  // brittle way to perform initialization; the preferred method (as of
  // May 2009) is to use the ComputeEnergyChunkInitialize() and
  // ComputeEnergyChunkFinalize() routines for pre- and post-handling of
  // computation details. This routines run serially on threadnumber 0;
  // this allows access to the Tcl interpreter which per Tcl specs can
  // only be accessed from the thread that launched the interpreter.
  //
  //
  // TODO: Output requests may require re-computation of energies, but
  //       if the "well known" or "standard" quantities were previously
  //       computed then we don't *need* to compute those again, and
  //       moreover we can't re-add then to the state derived
  //       quantities.  Also, for big arrays we also don't want to
  //       verify that the newly computed values agree with the old.
  //       So, what to do???

  // Shortcuts to import info.
  const Oxs_Director &director             = ocei.director;
  const Oxs_SimState &state                = ocei.state;
  const std::vector<Oxs_Energy*> &energies = ocei.energies;

  if(state.Id()==0) {
    String msg = String("Programming error:"
                        " Invalid (unlocked) state detected"
                        " in Oxs_ComputeEnergies");
    throw Oxs_ExtError(msg);
  }

  if(ocei.scratch_energy==nullptr || ocei.scratch_H==nullptr) {
    // Bad input
    String msg = String("Oxs_ComputeEnergies import"
                        " contains NULL scratch pointers.");
    throw Oxs_ExtError(msg);
  }

  const int thread_count = Oc_GetMaxThreadCount();

  // Export names for attach support. These should become const member
  // variables if this routine is moved into a class.
  using wkq = Oxs_Director::WellKnownQuantity; // shortcut
  const String energy_density_name
    = director.GetWellKnownQuantityLabel(wkq::total_energy_density);
  const String total_H_name
    = director.GetWellKnownQuantityLabel(wkq::total_H);
  const String total_mxH_name
    = director.GetWellKnownQuantityLabel(wkq::total_mxH);
  const String total_mxHxm_name
    = director.GetWellKnownQuantityLabel(wkq::total_mxHxm);
#ifndef NODEBUG
  for(auto q : Oxs_Director::WellKnownQuantity_List) {
    assert(!director.WellKnownQuantityRequestStatus(q)
           || !director.GetWellKnownQuantityLabel(q).empty());
  }
#endif // NODEBUG

  // Storage space for attach requests. Default constructor doesn't
  // allocate array space, and so is cheap. Array space is allocated
  // only if an attachment is requested and the import Oxs_MeshValue<T>*
  // is null.
  Oxs_MeshValue<OC_REAL8m> energy_storage;
  Oxs_MeshValue<ThreeVector> H_storage;
  Oxs_MeshValue<ThreeVector> mxH_storage;
  Oxs_MeshValue<ThreeVector> mxHxm_storage;

  // Handle pointer assignment and array allocation for
  // attachment-eligible quantities.
  Oxs_MeshValue<OC_REAL8m>* energy = ocee.energy;
  if(energy==nullptr &&
     director.WellKnownQuantityRequestStatus(wkq::total_energy_density)) {
    // Don't re-set if value already in state DerivedData (which is
    // anyway improper since DerivedData are supposed to be const).
    const Oxs_MeshValue<OC_REAL8m>* value = nullptr;
    if(!state.GetDerivedData(energy_density_name,value)) {
      energy = &energy_storage;
  }
  }
  if(energy) energy->AdjustSize(state.mesh);

  Oxs_MeshValue<ThreeVector>* H = ocee.H;
  if(H==nullptr &&
     director.WellKnownQuantityRequestStatus(wkq::total_H)) {
    // Don't re-set if value already in state DerivedData
    const Oxs_MeshValue<ThreeVector>* value = nullptr;
    if(!state.GetDerivedData(total_H_name,value)) {
      H = &H_storage;
  }
  }
  if(H) H->AdjustSize(state.mesh);

  Oxs_MeshValue<ThreeVector>* mxH = ocee.mxH;
  if(mxH==nullptr &&
     director.WellKnownQuantityRequestStatus(wkq::total_mxH)) {
    // Don't re-set if value already in state DerivedData
    const Oxs_MeshValue<ThreeVector>* value = nullptr;
    if(!state.GetDerivedData(total_mxH_name,value)) {
      mxH = &mxH_storage;
    }
  }
  if(mxH) mxH->AdjustSize(state.mesh);

  Oxs_MeshValue<ThreeVector>* mxHxm = ocee.mxHxm;
  if(mxHxm==nullptr &&
     director.WellKnownQuantityRequestStatus(wkq::total_mxHxm)) {
    // Don't re-set if value already in state DerivedData
    const Oxs_MeshValue<ThreeVector>* value = nullptr;
    if(!state.GetDerivedData(total_mxHxm_name,value)) {
      mxHxm = &mxHxm_storage;
    }
  }
  if(mxHxm) mxHxm->AdjustSize(state.mesh);

  ocee.energy_sum = 0.0;
  ocee.max_mxH = 0.0;
  ocee.pE_pt = 0.0;
  ocee.energy_density_error_estimate = 0.0;

  if(energies.size() == 0) {
    // No energies.  Zero requested outputs and return.
    // This is a corner case that shouldn't occur in practical use.
    // NB: SharedCopy() marks *H as read-only.
    const ThreeVector zerovec = ThreeVector(0.0,0.0,0.0);
    if(energy) *(energy)     = 0.0;
    if(H)      *(ocee.H)     = zerovec;
    if(mxH)    *(ocee.mxH)   = zerovec;
    if(mxHxm)  *(ocee.mxHxm) = zerovec;
    if(director.WellKnownQuantityRequestStatus(wkq::total_energy_density)) {
      state.AddDerivedData(energy_density_name,std::move(energy->SharedCopy()));
      }
    if(director.WellKnownQuantityRequestStatus(wkq::total_H)) {
      state.AddDerivedData(total_H_name,std::move(H->SharedCopy()));
    }
    if(director.WellKnownQuantityRequestStatus(wkq::total_mxH)) {
      state.AddDerivedData(total_mxH_name,std::move(mxH->SharedCopy()));
      }
    if(director.WellKnownQuantityRequestStatus(wkq::total_mxHxm)) {
      state.AddDerivedData(total_mxHxm_name,std::move(mxHxm->SharedCopy()));
    }
    return;
  }

  // If mxHxm output is requested, then Oxs_ComputeEnergiesChunkThread
  // objects compute mxHxm from in-cache mxH_accum output. If mxHxm
  // output is requested but mxH_accum is not, then we can reduce memory
  // traffic by storing computed mxH_accum data directly into mxHxm
  // space, and perform the trailing xm operation in (mxH)xm
  // in-place. Oxs_ComputeEnergiesChunkThread objects know this, and
  // respond appropriately if mxH_accum == mxHxm.
  if(mxH==nullptr && mxHxm!=nullptr) {
    mxH = mxHxm;
    // Hack mxHxm into mxH_accum. We need to have mxH_accum computed by
    // both chunk and non-chunk energies, so we have to do this hack
    // here rather than inside Oxs_ComputeEnergiesChunkThread::Cmd().)
  }

  // Ensure scratch space is sized appropriately
  Oxs_MeshValue<OC_REAL8m>* scratch_energy = ocei.scratch_energy;
  scratch_energy->AdjustSize(state.mesh);
  Oxs_MeshValue<ThreeVector>* scratch_H = ocei.scratch_H;
  scratch_H->AdjustSize(state.mesh);

  std::vector<Oxs_ComputeEnergies_ChunkStruct> chunk;
  std::vector<Oxs_Energy*> nonchunk;

  // Initialize those parts of ChunkStruct that are independent
  // of any particular energy term.
  Oxs_ComputeEnergyDataThreaded ocedt_base;
  ocedt_base.state_id = state.Id();
  ocedt_base.scratch_energy = scratch_energy;
  ocedt_base.scratch_H      = scratch_H;
  ocedt_base.energy_accum   = energy;
  ocedt_base.H_accum        = H;
  ocedt_base.mxH_accum      = mxH;

  for(std::vector<Oxs_Energy*>::const_iterator it = energies.begin();
      it != energies.end() ; ++it ) {
    Oxs_ChunkEnergy* ceptr =
      dynamic_cast<Oxs_ChunkEnergy*>(*it);
    if(ceptr != NULL) {
      // Set up and initialize chunk energy structures
      if(ceptr->energy_density_output.GetCacheRequestCount()>0) {
        ceptr->energy_density_output.cache.state_id=0;
        ocedt_base.energy = &(ceptr->energy_density_output.cache.value);
        ocedt_base.energy->AdjustSize(state.mesh);
      } else {
        ocedt_base.energy = 0;
      }
      if(ceptr->field_output.GetCacheRequestCount()>0) {
        ceptr->field_output.cache.state_id=0;
        ocedt_base.H = &(ceptr->field_output.cache.value);
        ocedt_base.H->AdjustSize(state.mesh);
      } else {
        ocedt_base.H = 0;
      }
      chunk.push_back(Oxs_ComputeEnergies_ChunkStruct(ceptr,
                                           ocedt_base,thread_count));
    } else {
      nonchunk.push_back(*it);
    }
  }

  // The "accum" elements are initialized on the first pass by moving
  // each accum pointer to the corresponding non-accum member.  After
  // filling by the first energy term, the pointers are moved back to
  // the accum member.  This way we avoid a pass through memory storing
  // zeros, and a pass through memory loading zeros.  Zero load/stores
  // are cheap in the chunk memory case, because in that case the
  // load/stores are just to and from cache, but we prefer here to run
  // non-chunk energies first so that we can compute mxHxm and max |mxH|
  // on the back side of the chunk energy runs (where m and mxH are in
  // cache and so don't have to be loaded).  Create a boolean to track
  // initialization.
  OC_BOOL accums_initialized = 0;

  // Non-chunk energies //////////////////////////////////////
  for(std::vector<Oxs_Energy*>::const_iterator ncit = nonchunk.begin();
      ncit != nonchunk.end() ; ++ncit ) {
    Oxs_Energy& eterm = *(*ncit);  // Convenience

#if REPORT_TIME
    eterm.energytime.Start();
#endif // REPORT_TIME

    Oxs_ComputeEnergyData term_oced(state);
    term_oced.scratch_energy = scratch_energy;
    term_oced.scratch_H      = scratch_H;
    term_oced.energy_accum = energy;
    term_oced.H_accum      = H;
    term_oced.mxH_accum    = mxH;

    // TODO: Rework output classes to use state DerivedData space.  As
    //       an interim step, make use of Oxs_MeshValue<T>.SharedCopy().
    if(eterm.energy_density_output.GetCacheRequestCount()>0) {
      eterm.energy_density_output.cache.state_id=0;
      term_oced.energy = &(eterm.energy_density_output.cache.value);
      term_oced.energy->AdjustSize(state.mesh);
    }

    if(eterm.field_output.GetCacheRequestCount()>0) {
      eterm.field_output.cache.state_id=0;
      term_oced.H = &(eterm.field_output.cache.value);
      term_oced.H->AdjustSize(state.mesh);
    }

    if(!accums_initialized) {
      // Initialize by filling (as opposed to accumulating)
      term_oced.energy_accum = nullptr;
      term_oced.H_accum      = nullptr;
      term_oced.mxH_accum    = nullptr;
      if(term_oced.energy == nullptr) term_oced.energy = energy;
      if(term_oced.H      == nullptr)      term_oced.H = H;
      if(term_oced.mxH    == nullptr)    term_oced.mxH = mxH;
    }

    ++(eterm.calc_count);
    eterm.ComputeEnergy(state,term_oced);

    if(eterm.field_output.GetCacheRequestCount()>0) {
      eterm.field_output.cache.state_id=state.Id();
    }
    if(eterm.energy_density_output.GetCacheRequestCount()>0) {
      eterm.energy_density_output.cache.state_id=state.Id();
    }
    if(eterm.energy_sum_output.GetCacheRequestCount()>0) {
      eterm.energy_sum_output.cache.value=term_oced.energy_sum;
      eterm.energy_sum_output.cache.state_id=state.Id();
    }

    if(!accums_initialized) {
      // If output buffer spaced was used instead of accum space, then
      // copy from output buffer to accum space.  This hurts from a
      // memory bandwidth perspective, but is rather hard to avoid.
      // (Options: Do accum initialization in chunk-energy branch,
      // but that hurts with respect to mxHxm and max |mxH| computations.
      // Or one could have the ComputeEnergy class fill more than one
      // array with the non-accum output (say, via a parameter that
      // says to set to accum rather than add to accum), but that is
      // rather awkward.  Instead, we assume that if the user wants
      // high speed then he won't enable term energy or H outputs.)
      if(energy && term_oced.energy != energy) {
        *(energy) = *(term_oced.energy);
      }
      if(H && term_oced.H != H) {
        *(H) = *(term_oced.H);
      }
      if(mxH  && term_oced.mxH != mxH) {
        *(ocee.mxH) = *(term_oced.mxH);
      }
      accums_initialized = 1;
    }

    ocee.pE_pt += term_oced.pE_pt;
    ocee.energy_sum += term_oced.energy_sum;
    ocee.energy_density_error_estimate
      += Oxs_ComputeEnergiesErrorEstimate(state,
                                term_oced.energy_density_error_estimate,
                                term_oced.energy_sum.GetValue());

#if REPORT_TIME
    eterm.energytime.Stop();
#endif // REPORT_TIME
  }


  // Chunk energies ///////////////////////////////////////////
  // Note: The ChunkThread command is run even if there are no chunk
  //       energies. This is needed for mxHxm compution.
#if REPORT_TIME
  Oxs_ChunkEnergy::chunktime.Start();
#endif

  // Compute cache_blocksize
  const OC_INDEX meshsize = state.mesh->Size();
  const OC_INDEX cache_size = Oc_CacheSize();

  const OC_INDEX recsize = sizeof(ThreeVector) + sizeof(OC_REAL8m);
  /// May want to query individual energies for this.

#define FUDGE 8
  OC_INDEX tcblocksize = (cache_size>FUDGE*recsize ?
                        cache_size/(FUDGE*recsize) : 1);
  if(thread_count*tcblocksize>meshsize) {
    tcblocksize = meshsize/thread_count;
  }
  if(0 == tcblocksize) {
    tcblocksize = 1;    // Safety
  } else if(0 != tcblocksize%16) {
    tcblocksize += 16 - (tcblocksize%16);  // Make multiple of 16
  }
  const OC_INDEX cache_blocksize = tcblocksize;

  // Thread control
  Oxs_ComputeEnergiesChunkThread::Init(thread_count,
                                       state.Ms->GetArrayBlock());

  Oxs_ComputeEnergiesChunkThread chunk_thread;
  chunk_thread.state     = &state;
  chunk_thread.energy_terms = &chunk;
  chunk_thread.mxH       = nullptr;
  chunk_thread.mxH_accum = mxH;
  chunk_thread.mxHxm     = mxHxm;
  chunk_thread.fixed_spins = ocei.fixed_spin_list;
  chunk_thread.max_mxH.resize(thread_count);
  chunk_thread.cache_blocksize = cache_blocksize;
  chunk_thread.accums_initialized = accums_initialized;

  // Initialize chunk energy computations
  for(std::vector<Oxs_ComputeEnergies_ChunkStruct>::iterator itc
        = chunk.begin(); itc != chunk.end() ; ++itc ) {
    Oxs_ChunkEnergy& eterm = *(itc->energy);  // For code clarity
    Oxs_ComputeEnergyDataThreaded& ocedt = itc->ocedt;
    Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>&
      thread_ocedtaux = itc->thread_ocedtaux;
    eterm.ComputeEnergyChunkInitialize(state,ocedt,thread_ocedtaux,
                                       thread_count);
  }

  // Run threads to compute chunk energy computations
  static Oxs_ThreadTree threadtree;
  threadtree.LaunchTree(chunk_thread,0);

  // Note: If chunk.size()>0, then we are guaranteed that accums are
  // initialized.  If accums_initialized is ever needed someplace
  // downstream, then uncomment the following line:
  // if(chunk.size()>0) accums_initialized = 1;

  // Finalize chunk energy computations
  for(OC_INDEX ei=0;static_cast<size_t>(ei)<chunk.size();++ei) {
    // NB: This block won't run if there are no chunk energy terms.

    Oxs_ChunkEnergy& eterm = *(chunk[ei].energy);  // Convenience
    Oxs_ComputeEnergyDataThreaded& ocedt = chunk[ei].ocedt;
    Oc_AlignedVector<Oxs_ComputeEnergyDataThreadedAux>&
      thread_ocedtaux = chunk[ei].thread_ocedtaux;

    eterm.ComputeEnergyChunkFinalize(state,ocedt,thread_ocedtaux,
                                     thread_count);

    ++(eterm.calc_count);

    // For each energy term, loop though all threads and sum
    // energy and pE_pt contributions.
    Oxs_Energy::SUMTYPE pE_pt_term = 0.0;
    Oxs_Energy::SUMTYPE energy_term = 0.0;
    std::vector<Oxs_ComputeEnergies_ChunkStruct>& et
      = *(chunk_thread.energy_terms);
    for(int ithread=0;ithread<thread_count;++ithread) {
      pE_pt_term += et[ei].thread_ocedtaux[ithread].pE_pt_accum;
      energy_term += et[ei].thread_ocedtaux[ithread].energy_total_accum;
    }
    ocee.pE_pt += pE_pt_term;
    ocee.energy_sum += energy_term;

    ocee.energy_density_error_estimate
      += Oxs_ComputeEnergiesErrorEstimate(state,
                                ocedt.energy_density_error_estimate,
                                energy_term.GetValue());

    if(eterm.energy_sum_output.GetCacheRequestCount()>0) {
      eterm.energy_sum_output.cache.value=energy_term;
      eterm.energy_sum_output.cache.state_id=state.Id();
    }

    if(eterm.field_output.GetCacheRequestCount()>0) {
      eterm.field_output.cache.state_id=state.Id();
    }

    if(eterm.energy_density_output.GetCacheRequestCount()>0) {
      eterm.energy_density_output.cache.state_id=state.Id();
    }

  }

  OC_REAL8m max_mxH_test = 0.0;
  for(int imh=0;imh<thread_count;++imh) {
    if(chunk_thread.max_mxH[imh]>max_mxH_test) {
      max_mxH_test = chunk_thread.max_mxH[imh];
    }
  }
  ocee.max_mxH = max_mxH_test;

  if(mxHxm!=0 && mxH == mxHxm) {
    // Undo mxHxm hack
    mxH = nullptr;
  }

  // Honor attachment requests
  // NB: SharedCopy() marks *H as read-only.
  if(director.WellKnownQuantityRequestStatus(wkq::total_energy_density)) {
    state.AddDerivedData(energy_density_name,std::move(energy->SharedCopy()));
  }
  if(director.WellKnownQuantityRequestStatus(wkq::total_H)) {
    state.AddDerivedData(total_H_name,std::move(H->SharedCopy()));
  }
  if(director.WellKnownQuantityRequestStatus(wkq::total_mxH)) {
    state.AddDerivedData(total_mxH_name,std::move(mxH->SharedCopy()));
  }
  if(director.WellKnownQuantityRequestStatus(wkq::total_mxHxm)) {
    state.AddDerivedData(total_mxHxm_name,std::move(mxHxm->SharedCopy()));
  }

#if REPORT_TIME
  Oxs_ChunkEnergy::chunktime.Stop();
#endif
}

//////////////////////// OXS_COMPUTEENERGIES ///////////////////////////
////////////////////////////////////////////////////////////////////////
