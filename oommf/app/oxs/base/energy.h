/* FILE: energy.h                 -*-Mode: c++-*-
 *
 * Abstract energy class, derived from Oxs_Ext class.  This file also
 * includes the declaration of the Oxs_EnergyPreconditionerSupport
 * class, which optional provides an additional interface for Oxs_Energy
 * objects via multiple inheritance.  The declaration an implementation
 * of the Oxs_Energy child class Oxs_ChunkEnergy, is in separate
 * chunkenergy.h and chunkenergy.cc files.
 *
 * Note: The friend function Oxs_ComputeEnergies() is declared in the
 * energy.h header (since its interface only references the base
 * Oxs_Energy class), but the implementation is in chunkenergy.cc
 * (because the implementation includes accesses to the Oxs_ChunkEnergy
 * API).
 */

#ifndef _OXS_ENERGY
#define _OXS_ENERGY

#include <vector>

#include "ext.h"
#include "simstate.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "outputderiv.h"
#include "util.h"

/* End includes */

/* Optional #ifdef's:
 *   #define EXPORT_CALC_COUNT
 *     to enable Oxs_ScalarOutput<Oxs_Energy> calc_count_output (for
 *     each individual energy term).  Note that if you do this then
 *     the standard Oxs regression tests will fail because the
 *     reference results do not have this option enabled.
 */
#if 0
# define EXPORT_CALC_COUNT 1
#endif

struct Oxs_EnergyData;
struct Oxs_ComputeEnergyData;
struct Oxs_ComputeEnergyExtraData;

////////////////////////////////////////////////////////////////////////
class Oxs_Energy:public Oxs_Ext {
  friend void Oxs_ComputeEnergies(const Oxs_SimState&,
                                  Oxs_ComputeEnergyData&,
                                  const vector<Oxs_Energy*>&,
                                  Oxs_ComputeEnergyExtraData& oceed);
  // Note: The declaration of this friend function is farther down
  // in this file, but the implementation of Oxs_ComputeEnergies
  // is in the file chunkenergy.cc.
private:
  // Track count of number of times GetEnergy() has been
  // called in current problem run.
  OC_UINT4m calc_count;
#ifdef EXPORT_CALC_COUNT
  // Make calc_count available for output.
  Oxs_ScalarOutput<Oxs_Energy> calc_count_output;
  void FillCalcCountOutput(const Oxs_SimState&);
#endif // EXPORT_CALC_COUNT

#if REPORT_TIME
  // energytime records time (cpu and wall) spent in the GetEnergyData
  // member function.  The results are written to stderr when this
  // object is destroyed or re-initialized.
  // Note: Timing errors may occur if an exception is thrown from inside
  //       GetEnergyData, because there is currently no attempt made to
  //       catch such exceptions and stop the stopwatch.  This could be
  //       done, but it probably isn't necessary for a facility which
  //       is intended only for development purposes.
protected:
  Nb_StopWatch energytime;
private:
#endif // REPORT_TIME

  void SetupOutputs(); // Utility routine called by constructors.

  // Expressly disable default constructor, copy constructor and
  // assignment operator by declaring them without defining them.
  Oxs_Energy();
  Oxs_Energy(const Oxs_Energy&);
  Oxs_Energy& operator=(const Oxs_Energy&);

protected:

  Oxs_Energy(const char* name,      // Child instance id
             Oxs_Director* newdtr); // App director
  Oxs_Energy(const char* name,
             Oxs_Director* newdtr,
	     const char* argstr);   // MIF block argument string

  // Standard outputs presented by all energy objects.  These are
  // conceptually public, but are specified private to force clients
  // to use the Oxs_Director output_map interface.
  // Question: Do we want to add an mxH output?
  void UpdateStandardOutputs(const Oxs_SimState&);
  Oxs_ScalarOutput<Oxs_Energy> energy_sum_output;
  Oxs_VectorFieldOutput<Oxs_Energy> field_output;
  Oxs_ScalarFieldOutput<Oxs_Energy> energy_density_output;

  // NOTE: The GetEnergy and GetEnergyData interfaces are deprecated.
  //       New work (post Oct 2008) should use the ComputeEnergy
  //       interface described below.
  //
  // Energy and energy derivatives calculation function.  This
  // should be "conceptually const."  The energy return is average
  // energy density for the corresponding cell, in J/m^3.  The
  // field is in A/m, pE_pt is in J/s.
  //   There are two GetEnergy functions, GetEnergy and GetEnergyData.
  // The first is a private, virtual member function of Oxs_Energy.
  // It takes as imports Oxs_MeshValue references into which to store
  // the energy and H results.  The second is a non-virtual public
  // member function that passes in Oxs_MeshValue references that
  // *may* be used to store the results, and separate parameters
  // that return pointers to where the results actually were stored.
  // If output caching in enabled, the Oxs_Energy base class method
  // GetEnergyData calls the virtual GetEnergy function with references
  // to the output cache Oxs_MeshValue objects.  Otherwise the import
  // buffer space is used.
  //   The "accum" and "mxH" members of Oxs_ComputeEnergyData are not
  // used by the GetEnergy functions, and may be set NULL.  They are
  // used by the AccumEnergyAndTorque function, described below.
  //   NOTE : The pE_pt export is the partial derivative of energy
  // with respect to time.  For most energy terms this will be
  // 0.  It will only be non-zero if there is an explicit dependence
  // on time, as for example with a time-varying applied field.
  //
  virtual void GetEnergy(const Oxs_SimState& state,
			 Oxs_EnergyData& oed) const =0;


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
  // by AccumEnergyAndTorque for the "energy", "H" and "mxH" members, but
  // not for the "accum" members.
  //
  // The remaining members, energy_sum and pE_pt are exports that are
  // always filled by ComputeEnergy.
  //
  // The main practical advantage of ComputeEnergy over GetEnergy is
  // that use of the "accum" fields can significantly reduce memory
  // bandwidth use in evolvers.  This can be especially important in
  // multi-threaded settings.
  //
  virtual void ComputeEnergy(const Oxs_SimState& state,
                             Oxs_ComputeEnergyData& oced) const;

  // GetEnergyAlt is an adapter that allows a child-defined
  // ComputeEnergy class to provide GetEnergy services.  Such children
  // can simply define their GetEnergy to be a simple wrapper to
  // GetEnergyAlt.  NOTE: Children must NOT wrap GetEnergyAlt with
  // GetEnergy without also overriding the default ComputeEnergy
  // implementation.  Otherwise an infinite circular call sequence will
  // result:
  //   GetEnergy -> GetEnergyAlt -> ComputeEnergy -> GetEnergy -> ...
  //
  // ANOTHER REALLY IMPORTANT NOTE: Nobody should call GetEnergyAlt
  //     except for a child-defined GetEnergy.
  //
  void GetEnergyAlt(const Oxs_SimState& state,
                    Oxs_EnergyData& oed) const;


  // *** ComputeEnergy vs. GetEnergy interfaces ***
  //
  // The preferred interface for energy computations is the
  // ComputeEnergy function.  This replaces the now deprecated GetEnergy
  // function.
  //
  // For backward compatibility, a default implementation of
  // ComputeEnergy is provided, which is a simple wrapper around
  // GetEnergy.  This allows new and updated evolvers and other clients
  // to use the ComputeEnergy interface with existing (pre Oct 2008)
  // Oxs_Energy children.
  //
  // All new Oxs_Energy children should directly define ComputeEnergy
  // (overriding the default implementation), and can define GetEnergy
  // as a simple wrapper around GetEnergyAlt.  GetEnergyAlt is an
  // adapter that calls ComputeEnergy.
  //
  // To summarize, old Oxs_Energy children define GetEnergy and use the
  // default ComputeEnergy for compatibility with new evolvers.  They do
  // not use GetEnergyAlt.  New Oxs_Energy children define ComputeEnergy
  // (overriding the default version) and define GetEnergy as a wrapper
  // to GetEnergyAlt (which calls ComputeEnergy).
  //
  // Do NOT define GetEnergy as a wrapper to GetEnergyAlt without also
  // defining ComputeEnergy.  If you do this, then a call to GetEnergy
  // would call GetEnergyAlt which would call ComputeEnergy; the default
  // version of ComputeEnergy would then call GetEnergy and around in a
  // circle until the recursive ceiling is hit.
  //
  // At the time of this writing, the public interface to ComputeEnergy
  // is AccumEnergyAndTorque (see below).

public:

  typedef Nb_Xpfloat SUMTYPE; // Variable type used for energy
  /// summations.  Either OC_REAL8m or Nb_Xpfloat.

  virtual ~Oxs_Energy();

  // Default problem initializer routine.  This sets up default
  // output objects, so any child that redefines this function
  // should embed a call to this Init() inside the child
  // specific version.
  virtual OC_BOOL Init();

  void GetEnergyData(const Oxs_SimState& state,Oxs_EnergyData& oed);
  //   NOTE : The energy_buffer and field_buffer pointers in oed
  // must point to valid Oxs_MeshValue objects.  An exception will
  // be thrown if either is a null pointer.  However, GetEnergyData
  // will resize the buffers as necessary, so empty, 0-sized
  // Oxs_MeshValue objects may be passed in.  This also means that
  // allocated memory inside these objects may change as a side
  // effect of the call.

  void AccumEnergyAndTorque(const Oxs_SimState& state,
                            Oxs_ComputeEnergyData& oced);
  // The AccumEnergyAndTorque method is similar to GetEnergyData,
  // except that the former uses the newer ComputeEnergy interface
  // rather than the deprecated GetEnergy interface.
  //
  // The "oced.scratch_*" members must be set non-NULL before entry.
  // They point to scratch space that may or may not be used.  This
  // space will be resized as needed.  If desired, "scratch_energy"
  // may point to the same place as "energy", and likewise for
  // "scratch_H" and "H".
  //
  // The remaining Oxs_MeshValue pointers are output requests.  If one
  // of these pointers is NULL on entry, then that output is
  // requested.  The output will to into the pointed-to space,
  // *unless* that output is associated with one of the Oxs_Energy
  // standard outputs (energy_density_output or field_output) and
  // caching of that output is enabled, in which case the output will
  // go into the output cache and the corresponding oced pointer
  // (energy, H or mxH) will be changed to point to the cache.  Pay
  // ATTENTION to this point: the "energy", "H", and "mxH" arrays sent
  // in *may* not be the ones used, so clients should always check and
  // use the oced pointers directly, rather than the array pointers
  // sent in.  Also, if caching is enabled, then on return the energy,
  // H, and/or mxH pointers in oced will be set to the cache, even if
  // energy, H and/or mxH were set to NULL on entry.
  //
  // The oced.*_accum members are accumulated (added) into rather than
  // set.  In all cases, the oced.*_accum members are guaranteed on
  // exit to point to the same place as on entry.

  // For development:
  OC_UINT4m GetEnergyEvalCount() const { return calc_count; }
};


////////////////////////////////////////////////////////////////////////
// Optional interface for energy terms.  Supports preconditioner
// for minimization evolvers.
class Oxs_EnergyPreconditionerSupport {
public:
  enum Preconditioner_Type { NONE, MSV, DIAGONAL };
  struct PreconditionerData {
    // Imports to IncrementPreconditioner
    enum Preconditioner_Type type;
    const Oxs_SimState* state;

    // Exports from IncrementPreconditioner.  This should be
    // diag(A)/(Ms*Volume)
    Oxs_MeshValue<ThreeVector>* val;
  };
  virtual OC_INT4m IncrementPreconditioner(PreconditionerData& pcd) =0;
  /// Adds preconditioner data for *this energy term to the val array in
  /// pcd.  It is the responsibility of the calling routine to zero val
  /// before calling this interface for the first energy term.
  /// Returns 1 if successful, 0 if preconditioner not supported.

protected:
  Oxs_EnergyPreconditionerSupport() {}
  virtual ~Oxs_EnergyPreconditionerSupport() {}
};


////////////////////////////////////////////////////////////////////////
// Oxs_ComputeEnergies compute sums of energies, fields, and/or
// torques for all energies in "energies" import.  On entry,
// oced.energy_accum, oced.H_accum, and oced.mxH_accum should be set
// or null as desired.  oced.scratch_energy and oced.scratch_H must be
// non-null.  oced.energy, oced.H, and oced.mxH *must* be *null* on
// entry.  This routine does not fill these fields, but rather the
// accumulated values are collected as necessary in oced.*_accum
// entries.
//   This routine handles outputs, energy calculation counts, and timers
// appropriately.
//   Those "energies" members that are actually Oxs_ChunkEnergies (see
// files chunkenergy.h and chunkenergy.cc) will use the
// ComputeEnergyChunk interface in a collated fashion to help minimize
// memory bandwidth usage.  On threaded OOMMF builds, these calls will
// be run in parallel and load balanced.
//   This function is declared here because the interface only requires
// knowledge of the base Oxs_Energy class; however, the implementation
// requires detailed knowledge of the Oxs_ChunkEnergy class as well,
// so the implementation of this class is in the file chunkenergy.cc.

void Oxs_ComputeEnergies(const Oxs_SimState& state,
                         Oxs_ComputeEnergyData& oced,
                         const vector<Oxs_Energy*>& energies,
                         Oxs_ComputeEnergyExtraData& oceed);



////////////////////////////////////////////////////////////////////////
struct Oxs_EnergyData {
public:
  const OC_UINT4m state_id;

  // Buffer space that the Oxs_Energy client object is allowed to use.
  // Oxs_Energy::GetEnergyData() sets the size to match the mesh prior
  // to calling Oxs_Energy::GetEnergy().
  Oxs_MeshValue<OC_REAL8m>* energy_buffer;
  Oxs_MeshValue<ThreeVector>* field_buffer;

  // The following Oxs_WriteOnceObjects are exports filled by the
  // individual Oxs_Energy term.  These exports are all individual
  // termwise results (as opposed to be a rolling sum across multiple
  // terms).  The energy and field pointers point to where the term
  // actually wrote the energy and field; most commonly these point
  // back to the provided energy_buffer and field_buffer arrays, but
  // this is not required.
  Oxs_WriteOnceObject<Oxs_Energy::SUMTYPE> energy_sum;
  Oxs_WriteOnceObject<OC_REAL8m> pE_pt;
  Oxs_WriteOnceObject<const Oxs_MeshValue<OC_REAL8m>* > energy;
  Oxs_WriteOnceObject<const Oxs_MeshValue<ThreeVector>* > field;

  Oxs_EnergyData(const Oxs_SimState& state)
    : state_id(state.Id()), energy_buffer(0), field_buffer(0) {}

private:
  // The following are left undefined on purpose:
  Oxs_EnergyData();
  Oxs_EnergyData(const Oxs_EnergyData&);
  const Oxs_EnergyData& operator=(const Oxs_EnergyData&);
};

////////////////////////////////////////////////////////////////////////
struct Oxs_ComputeEnergyData {
  // Data structure for Oxs_Energy::ComputeEnergy()
  // NB: There is a thread-friendly doppelganger for this struct,
  // Oxs_ComputeEnergiesChunkThread, defined in chunkenergy.h.  Changes
  // to this struct should be reflected there.
public:
  OC_UINT4m state_id;

  // "scratch_*" are import scratch space.  Must be set non-null on
  // import.  Oxs_Energy client should automatically resize as needed.
  Oxs_MeshValue<OC_REAL8m>* scratch_energy;
  Oxs_MeshValue<ThreeVector>* scratch_H;

  // Output requests and space.  If null on entry to ComputeEnergy,
  // then associated output is not requested.  Otherwise, it will be
  // filled.  Any which are non-null must be sized properly before
  // entry into Oxs_Energy client routines.
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

  // Required outputs.  These values are for the individual energy
  // term, i.e., these should be overwritten (not summed into) by
  // the Oxs_Energy child ::ComputeEnergy() routine.
  Oxs_Energy::SUMTYPE energy_sum;
  Oxs_Energy::SUMTYPE pE_pt;

  // Recommended output:
  OC_REAL8m energy_density_error_estimate; // Estimate of absolute
  /// error in the cellwise energy density, in J/m^3.  This term was
  /// introduced in the 20170801 OOMMF API.  Older energy terms which
  /// don't set this value will instead leave it unchanged from its
  /// initialized but invalid value of -1.  This is detected by
  /// Oxs_ComputeEnergies, which will create a term error estimate by
  /// dividing the absolute value of the energy sum by the mesh
  /// TotalVolume, and multiplying by a fixed error estimate (see
  /// edee_round_error below.)

  Oxs_ComputeEnergyData(const Oxs_SimState& state)
    : state_id(state.Id()), scratch_energy(0), scratch_H(0),
      energy_accum(0), H_accum(0), mxH_accum(0),
      energy(0), H(0), mxH(0), energy_sum(0.), pE_pt(0.),
      energy_density_error_estimate(-1.0) {}

  Oxs_ComputeEnergyData()
    : state_id(0), scratch_energy(0), scratch_H(0),
      energy_accum(0), H_accum(0), mxH_accum(0),
      energy(0), H(0), mxH(0), energy_sum(0.), pE_pt(0.),
      energy_density_error_estimate(-1.0) {}

private:
  constexpr static OC_REAL8m edee_round_error = 1e-14;
  friend OC_REAL8m Oxs_ComputeEnergiesErrorEstimate(const Oxs_SimState& state,
                                      OC_REAL8m energy_density_error_estimate,
                                                         OC_REAL8m energy_sum);
};

////////////////////////////////////////////////////////////////////////
struct Oxs_ComputeEnergyExtraData {
  // Supplemental data used or generated by the "Oxs_ComputeEnergies"
  // function.  This struct is not passed to Oxs_Energy child classes.
public:
  const vector<OC_INDEX>* fixed_spin_list; // Sorted list of fixed spins.
  /// The torque values (mxH, mxH_accum, and mxHxm) are zeroed at
  /// these locations.  Set fixed_spin_list to zero to disable this
  /// functionality.

  Oxs_MeshValue<ThreeVector>* mxHxm; // Set non-null to request this output.

  OC_REAL8m max_mxH;                // max |mxH_accum|

  Oxs_ComputeEnergyExtraData(const vector<OC_INDEX>* i_fixed_spin_list,
                             Oxs_MeshValue<ThreeVector>* i_mxHxm)
    : fixed_spin_list(i_fixed_spin_list), mxHxm(i_mxHxm), max_mxH(0) {}
};


#endif // _OXS_ENERGY
