/* FILE: evolver.h                 -*-Mode: c++-*-
 *
 * Abstract evolver class, derived from Oxs_Ext class.
 *
 */

#ifndef _OXS_EVOLVER
#define _OXS_EVOLVER

#include <string>
#include <vector>

#include "simstate.h"
#include "ext.h"
#include "key.h"
#include "atlas.h"

OC_USE_STRING;

/* End includes */

class Oxs_Driver; // Forward reference

class Oxs_Evolver : public Oxs_Ext {
private:
  Oxs_OwnedPointer<Oxs_Atlas> atlas;
  Oxs_ConstKey<Oxs_Atlas> atlas_key;
  vector<OC_INDEX> fixed_region_ids;
  OC_UINT4m mesh_id;
  vector<OC_INDEX> fixed_spin_list; // Holds a list of
  // indices to spins that should be held constant.  The indexing
  // is that specified by mesh referenced by mesh_id.  This
  // list is sorted into increasing order by index.

  // Oxs_Ext parent can't be copied, so this class can't either.
  // So disable copy constructor and assignment operators by declaring
  // them without providing definitions.
  Oxs_Evolver(const Oxs_Evolver&);
  Oxs_Evolver& operator=(const Oxs_Evolver&);

protected:
  Oxs_Evolver(const char* name,     // Child instance id
             Oxs_Director* newdtr); // App director
  Oxs_Evolver(const char* name,
             Oxs_Director* newdtr,
	     const char* argstr);   // MIF block argument string
  void InitFixedRegions(const vector<String> &names);
  void UpdateFixedSpinList(const Oxs_Mesh* mesh); // If mesh->Id()
  /// == mesh_id, then immediately returns without changing
  /// fixed_spin_list.
  // Note: We may want to make mesh_id and fixed_spin_list mutable
  //  so UpdateFixedSpinList can be "const", in the conceptual
  //  sense where we consider fixed_spin_list to be computed on
  //  the fly when routines like Step are called with a new mesh.
  //  However, this is even a bit riskier than usual because then
  //  the interpretation of "conceptual constantness" has to be
  //  agreed upon by both this class and its children, the latter
  //  of which are likely to be unruly (as youngsters are wont to
  //  be).  At this time (08/2001) we'll keep UpdateFixedSpinList
  //  non-const, inasmuch as Step is also non-const.
  OC_INDEX GetFixedSpinCount() const {
    return static_cast<OC_INDEX>(fixed_spin_list.size());
  }
  OC_INDEX GetFixedSpin(OC_INDEX index) const {
    return fixed_spin_list[index];
  }
  const vector<OC_INDEX>* GetFixedSpinList() const {
    // Use this routine with care.
    return &fixed_spin_list;
  }

  virtual OC_BOOL Init();  // All children of Oxs_Evolver should call
  /// this routine from inside their Init() routines.  The purpose
  /// at this time is to reset the fixed spin data.

public:
  virtual ~Oxs_Evolver();
};

#endif // _OXS_EVOLVER
