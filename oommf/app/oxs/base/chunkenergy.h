/* FILE: chunkenergy.h              -*-Mode: c++-*-
 *
 * Abstract chunk energy class, derived from Oxs_Energy class.  Children
 * of the Oxs_ChunkEnergy class include an interface allowing
 * computation on only a specified subrange of the elements (cells) in
 * the state mesh.  These routines reduce memory bandwidth use by
 * sequentially running each chunk energy on the same range of cells,
 * and also allow multiple threads to run concurrently, with each thread
 * claiming a separate cell range.
 *
 * Note: The friend function Oxs_ComputeEnergies() is declared in the
 * energy.h header (since its interface only references the base
 * Oxs_Energy class), but the implementation is in chunkenergy.cc
 * (because the implementation includes accesses to the Oxs_ChunkEnergy
 * API).
 */

#ifndef _OXS_CHUNKENERGY
#define _OXS_CHUNKENERGY

#include <vector>

#include "energy.h"
#include "ext.h"
#include "simstate.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "outputderiv.h"
#include "util.h"

/* End includes */

////////////////////////////////////////////////////////////////////////
struct Oxs_ComputeEnergyDataThreaded {
  // Same as Oxs_Energy::ComputeEnergyData, but more thread friendly:
  // the energy_sum and pE_pt members are broken off into a separate
  // struct and renamed energy_total_accum and pE_pt_accum.
  // Independent copies of these auxiliary structure are made for each
  // energy term for each thread, so concurrent threads can update
  // these terms without stepping on each other.  These structs are
  // used by the Chunk energy routines (see below).
public:
  OC_UINT4m state_id;

  // "scratch_*" are import scratch space.
  //    Must be set non-null on import.  Will be automatically
  // sized as needed.
  Oxs_MeshValue<OC_REAL8m>* scratch_energy;
  Oxs_MeshValue<ThreeVector>* scratch_H;

  // Output requests and space.  If null on entry to ComputeEnergy, then
  // associated output is not requested.  Otherwise, it will be filled.
  // Any which are non-null must be sized properly before entry.
  //    Computed values are added into (i.e., accumulated) into the
  // "accum" arrays.  The energy, H and mxH arrays, OTOH, are simply
  // filled.  It is expressly allowed for energy and scratch_energy to
  // be the same, and ditto for H and scratch_H.
  Oxs_MeshValue<OC_REAL8m>* energy_accum;
  Oxs_MeshValue<ThreeVector>* H_accum;
  Oxs_MeshValue<ThreeVector>* mxH_accum;
  Oxs_MeshValue<OC_REAL8m>* energy;
  Oxs_MeshValue<ThreeVector>* H;
  Oxs_MeshValue<ThreeVector>* mxH;

  Oxs_ComputeEnergyDataThreaded(const Oxs_SimState& state)
    : state_id(state.Id()), scratch_energy(0), scratch_H(0),
      energy_accum(0), H_accum(0), mxH_accum(0),
      energy(0), H(0), mxH(0) {}

  Oxs_ComputeEnergyDataThreaded()
    : state_id(0), scratch_energy(0), scratch_H(0),
      energy_accum(0), H_accum(0), mxH_accum(0),
      energy(0), H(0), mxH(0) {}

  Oxs_ComputeEnergyDataThreaded(const Oxs_ComputeEnergyData& oced)
    : state_id(oced.state_id),
      scratch_energy(oced.scratch_energy), scratch_H(oced.scratch_H),
      energy_accum(oced.energy_accum), H_accum(oced.H_accum),
      mxH_accum(oced.mxH_accum),
      energy(oced.energy), H(oced.H), mxH(oced.mxH) {}

  // Note implicit copy constructor and assignment operator.
};

struct Oxs_ComputeEnergyDataThreadedAux {
  // Required outputs.  One per thread
  OC_REAL8m energy_total_accum;
  OC_REAL8m pE_pt_accum;

  // Timing data; one per thread
#if REPORT_TIME
  Nb_StopWatch energytime;
#endif

  Oxs_ComputeEnergyDataThreadedAux()
    : energy_total_accum(0.0), pE_pt_accum(0.0) {}

  // Note implicit copy constructor and assignment operator.
};


////////////////////////////////////////////////////////////////////////
// Oxs_ChunkEnergy class: child class of Oxs_Energy that supports an
// additional ComputeEnergy interface --- one that allows computation
// on only a specified subrange of the elements (cells) in the state
// mesh.  These routines reduce memory bandwidth use by sequentially
// running each chunk energy on the same range of cells, and also
// allow multiple threads to run concurrently, with each thread
// claiming a separate cell range.
class Oxs_ComputeEnergiesChunkThread; // Thread helper class; 
class Oxs_ChunkEnergy:public Oxs_Energy {
  friend void Oxs_ComputeEnergies(const Oxs_SimState&,
                                  Oxs_ComputeEnergyData&,
                                  const vector<Oxs_Energy*>&,
                                  Oxs_ComputeEnergyExtraData& oceed);
  friend class Oxs_ComputeEnergiesChunkThread;
private:
  // Expressly disable default constructor, copy constructor and
  // assignment operator by declaring them without defining them.
  Oxs_ChunkEnergy();
  Oxs_ChunkEnergy(const Oxs_ChunkEnergy&);
  Oxs_ChunkEnergy& operator=(const Oxs_ChunkEnergy&);

#if REPORT_TIME
  static Nb_StopWatch chunktime;  // Records time spent computing
  /// Oxs_ChunkEnergy energies (primarily by the Oxs_ComputeEnergies
  /// routine).
  void ReportTime();
#else
  void ReportTime() {}
#endif // REPORT_TIME


protected:
  Oxs_ChunkEnergy(const char* name,Oxs_Director* newdtr)
    : Oxs_Energy(name,newdtr) {}
  Oxs_ChunkEnergy(const char* name,Oxs_Director* newdtr,
                  const char* argstr)
    : Oxs_Energy(name,newdtr,argstr) {}


  // For a given state, ComputeEnergyChunk (see below) performs the
  // energy/field/torque computation on a subrange of nodes across the
  // mesh.  Before each series of ComputeEnergyChunk calls for a given
  // state, ComputeEnergyChunkInitialize is run in thread 0 to perform
  // any non-threadable, termwise initialization.  For example, memory
  // allocation based on mesh size, or calls into the Tcl interpreter,
  // could be done here.
  //   Similarly, ComputeEnergyChunkFinalize is called at the end of
  // ComputeEnergyChunk processing for a given state.  It is also run in
  // thread 0.  This routine can be used to collate termwise data, such
  // as term-specific output.
  //   Note that as with ComputeEnergyChunk, the *Initialize and
  // *Finalize member functions are const, so only local and mutable
  // data may be modified.
  //   Note also that the default implementation for both *Initialize
  // and *Finalize are NOPs.
  virtual void ComputeEnergyChunkInitialize
  (const Oxs_SimState& /* state */,
   Oxs_ComputeEnergyDataThreaded& /* ocedt */,
   Oxs_ComputeEnergyDataThreadedAux& /* ocedtaux */,
   int /* number_of_threads */) const {}

  virtual void ComputeEnergyChunkFinalize
  (const Oxs_SimState& /* state */,
   const Oxs_ComputeEnergyDataThreaded& /* ocedt */,
   const Oxs_ComputeEnergyDataThreadedAux& /* ocedtaux */,
   int /* number_of_threads */) const {}

  virtual void
  ComputeEnergyChunk(const Oxs_SimState& state,
                     Oxs_ComputeEnergyDataThreaded& ocedt,
                     Oxs_ComputeEnergyDataThreadedAux& ocedtaux,
                     OC_INDEX node_start,OC_INDEX node_stop,
                     int threadnumber) const = 0;
  // Computes energy and/or related properties at nodes node_start
  // through "node_stop - 1" (i.e., the usual C convention).
  // Otherwise, this routine is essentially the same as
  // Oxs_Energy::ComputeEnergy, with the exception of the export
  // ocedtaux, which contains member variables energy_total_accum and
  // pE_pt_accum.  As indicated by the name, these values should be
  // "summed into" rather than set, so that a full energy computation
  // can be done piecewise.  This makes it important, however, that
  // the energy_total_accum and pE_pt_accum elements be zeroed out
  // before the first call in a sequence.  See the ComputeEnergyAlt
  // adapter function for an example of this.
  // NOTE: This routine must be thread safe.  Mostly this is handled
  // by partitioning array access by range (node_start to node_stop)
  // per thread, and also by having separate ocedtaux structs for each
  // threa.  However, if an Oxs_ChunkEnergy child class requires
  // initialization inside the ComputeEnergyChunk function that
  // extends beyond the [node_start,node_stop) range, or sets any
  // other resource shared between threads, then the child class must
  // arrange for proper synchronization (for example, by defining and
  // using mutexes).

  //    In particular, if initialization potentially involves a call
  // back into the Tcl interpreter, then that initializaition *must* be
  // done by threadnumber == 0 (the main thread), as per Tcl specs.
  // Child classes may safely assume that at least one call with
  // threadnumber == 0 will be made from any of the general energy
  // computation routines; the serial routine ComputeEnergyAlt will make
  // one call with threadnumber == 0, and the parallel routine
  // Oxs_ComputeEnergies will always insure that the number of threads
  // launched is not more than the number of chunks (so that if 
  // threads other than "0" block on entry waiting on "0", then
  // "0" will eventually show up).

  // Update May-2009: The now preferred initialization method is to
  // use ComputeEnergyChunkInitialize.  The guarantee that threadnumber
  // 0 will always run is honored for backward compatibility, but new
  // code should use ComputeEnergyChunkInitialize instead.

  // ComputeEnergyAlt is an adapter that can (optionally) be used to
  // allow ComputeEnergyChunk code to provide the parent
  // Oxs_Energy::ComputeEnergy interface.
  void ComputeEnergyAlt(const Oxs_SimState& state,
                        Oxs_ComputeEnergyData& oced) const {
#if REPORT_TIME
    chunktime.Start();
#endif
    Oxs_ComputeEnergyDataThreaded ocedt(oced);
    Oxs_ComputeEnergyDataThreadedAux ocedtaux;

    ComputeEnergyChunkInitialize(state,ocedt,ocedtaux,1);
    ComputeEnergyChunk(state,ocedt,ocedtaux,0,state.mesh->Size(),0);
    // "Main" thread is presumed; thread_number for main thread is 0.
    ComputeEnergyChunkFinalize(state,ocedt,ocedtaux,1);

    oced.energy_sum = ocedtaux.energy_total_accum;
    oced.pE_pt = ocedtaux.pE_pt_accum;
#if REPORT_TIME
    chunktime.Stop();
#endif
  }

public:
  virtual OC_BOOL Init() { ReportTime(); return Oxs_Energy::Init(); }
  virtual ~Oxs_ChunkEnergy() { ReportTime(); }

};

#endif // _OXS_CHUNKENERGY
