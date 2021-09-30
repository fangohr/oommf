/* FILE: simstate.h                 -*-Mode: c++-*-
 *
 * Simulation state class.
 *
 */

#ifndef _OXS_SIMSTATE
#define _OXS_SIMSTATE


#include <map>
#include <string>
#include <vector>

#include "oc.h"
#include "lock.h"
#include "meshvalue.h"
#include "threevector.h"
#include "util.h"

OC_USE_STRING;

/* End includes */

class Oxs_Director; // Forward references
class Oxs_Mesh;

class Oxs_SimState : public Oxs_Lock {
  // Perhaps the inheritance from Oxs_Lock should be private, but then
  // exposing the Oxs_Lock member functions is awkward.
  // Note 1: To maintain a clean dependency hierarchy and avoid cyclic
  //  dependencies, Oxs_SimState objects should not contain any keys,
  //  because Oxs_SimState objects are held by Oxs_Ext objects which may
  //  have locks places on them by other Oxs_Ext objects.  Therefore,
  //  Oxs_SimState objects are placed at the root of the dependency
  //  hierarchy, and are deleted by Oxs_Director only after all Oxs_Ext
  //  objects are destroyed.  It is therefore the responsibility of each
  //  object holding a lock on an Oxs_SimState object to place locks on
  //  or otherwise guarantee the correctness of all pointers held within
  //  the Oxs_SimState object.
  // Note 2: If the Oxs_SimState structure changes, then the convenience
  //  method CloneStateHeaders should be updated to reflect those
  //  changes.
private:
#ifdef OC_STL_MAP_BROKEN_CONST_KEY
  typedef String DerivedDataKey;
  typedef String AuxDataKey;
#else
  typedef const String DerivedDataKey;
  typedef const String AuxDataKey;
#endif
  mutable map<DerivedDataKey,OC_REAL8m> derived_data;
  mutable map<AuxDataKey,OC_REAL8m> auxiliary_data;

  void AppendListDerivedData(vector<String>& names) const;
  void AppendListAuxData(vector<String>& names) const;
  /// Internal implementation of ListDerivedData and ListAuxData

public:
  Oxs_SimState();
  ~Oxs_SimState();

  void Reset(); // Called by Oxs_Director (and others) to reuse state
  /// structure.  The (potentially large) Oxs_MeshValue array "spin" is
  /// not touched, for reasons of efficiency.  However, for debugging
  /// checks one might want to wipe "spin" too.

  // The following elements should perhaps be wrapped up in a
  // STL map array, and indexed by name.
  OC_UINT4m previous_state_id; // 0 if no previous state
  OC_UINT4m iteration_count;  // Total number of distinct spin configurations
  OC_UINT4m stage_number;     // Stage index
  OC_UINT4m stage_iteration_count; // Number of iterations for this stage
  OC_REAL8m stage_start_time; // Time index corresponding to current stage
  /// with stage_iteration_count == 0
  OC_REAL8m stage_elapsed_time; // Time elapsed in current stage.
  /// NOTE: total elapsed time = stage_start_time + stage_elapsed_time.
  OC_REAL8m last_timestep; // Seconds between this state and previous state.

  // It is the responsibility of any object using this Oxs_SimState
  // object to insure the validity of these pointers.
  const Oxs_Mesh* mesh;
  const Vf_Ovf20_MeshNodes* GetMeshNodesPtr() const;  // Returns
  /// a pointer to mesh, but upcasted to Vf_Ovf20_MeshNodes*.  This
  /// allows code (such as output.h) that needs this header file to use
  /// it without needing to include mesh.h.  Including mesh.h is an
  /// issue for output.h, because output.h is included by ext.h,
  /// so a circular dependency would occur if output.h included
  /// mesh.h, since mesh.h inherits from ext.h.

  const Oxs_MeshValue<OC_REAL8m>* Ms;
  const Oxs_MeshValue<OC_REAL8m>* Ms_inverse; // 1/Ms
  OC_REAL8m max_absMs;
  // Note: Client code may assume that Ms and Ms_inverse are fixed for
  // the lifetime of mesh.

  Oxs_MeshValue<ThreeVector> spin;

  void CloneHeader(Oxs_SimState& new_state) const;
  /// Copies all of the above data, except for the spin data.
  /// Does set new_state.spin to the size appropriate for
  /// mesh, however.  Sets the SimStateStatus variables to
  /// UNKNOWN, and empties the DerivedData area.  This routine
  /// is intended for use by code that needs to create a
  /// temporary duplicate state, but with some modifications to
  /// to the spin array.  This is preferable to copying the
  /// header information directly, because this routine will
  /// be updated as necessary as the Oxs_SimState structure
  /// evolves.

  // Status done/not_done cache.  The driver Done and StageDone methods
  // should set these fields appropriately.  Caching done status can
  // help protect against cache thrashing for derived data stored outside
  // of the state.  Perhaps this should be moved into the DerivedData area?
  enum SimStateStatus { UNKNOWN=-1, NOT_DONE=0, DONE=1 };
  mutable SimStateStatus step_done;  // step_done==1 means state was
  /// accepted as good step.  step_done==0 means step was rejected.
  mutable SimStateStatus stage_done;
  mutable SimStateStatus run_done;

  // AddDerivedData and GetDerivedData return 1 if operation
  // was successful, 0 otherwise.  In particular, AddDerivedData
  // fails if name is already used.
  OC_BOOL AddDerivedData(const char* name,OC_REAL8m value) const;
  OC_BOOL AddDerivedData(const String& name,OC_REAL8m value) const {
    return AddDerivedData(name.c_str(),value);
  }

  OC_BOOL GetDerivedData(const char* name,OC_REAL8m& value) const;
  OC_BOOL GetDerivedData(const String& name,OC_REAL8m& value) const {
    return GetDerivedData(name.c_str(),value);
  }

  // NB: Oxs_Ext objects accessing any of the Add/GetDerivedData
  // or Add/GetAuxData routines should use the Oxs_Ext::DataName()
  // wrapper to get an instance-specific item name.

  void ListDerivedData(vector<String>& names) const;
  void ClearDerivedData(); // This function only changes mutable
  /// values (the derived_data map), so it could be made "const".
  /// However, one of the properties of derived_data is that once
  /// calculated for a given state, it doesn't change --- AddDerivedData
  /// fails if you try to overwrite an existing value.  Making
  /// ClearDerivedData() const would remove that property, because if
  /// ClearDerivedData() is non-const then you need a non-const pointer
  /// to the state to call it, and in that case the simstate data
  /// structure (and in particular the spin array) should be marked as
  /// non-set.  Arguably, the Oxs_Key::GetWriteReference call should
  /// automatically wipe out the derived_data data.

  // Auxiliary data is provided as a convenience to client classes.
  // It is similar to derived data, but is mutable.  Use with care!
  // Note: The AddAuxData functions silently overwrite old data with
  // same key.  If you don't want this behavior use the DerivedData
  // map.
  void AddAuxData(const char* name,OC_REAL8m value) const;
  void AddAuxData(const String& name,OC_REAL8m value) const {
    AddAuxData(name.c_str(),value);
  }
  OC_BOOL GetAuxData(const char* name,OC_REAL8m& value) const;
  OC_BOOL GetAuxData(const String& name,OC_REAL8m& value) const {
    // Returns 1 on success, 0 if name is an unknown key.
    return GetAuxData(name.c_str(),value);
  }
  void ListAuxData(vector<String>& names) const;
  void ClearAuxData();

  // Query interface
  //
  // The *Names interface returns a list of strings representing
  // available key names for the *Values lookup function.  The
  // *Values function is called with a list of keys, which fills
  // the values export list with the matching values.  If any
  // key request is invalid, then an error is thrown and the
  // values export state is undefined.  In particular, this means
  // that the length of the values export will match the length
  // of the keys import unless an error is thrown.
  void QueryScalarNames(vector<String>& keys) const;
  void QueryScalarValues(const vector<String>& keys,
                         vector<Oxs_MultiType>& values) const;

  // Routines to write and read state, intended for restart
  // operations.  The Ms, Ms_inverse, and mesh data are not
  // written or read, but must be supplied independently
  // for the restore operations.
  void SaveState(const char* filename,
		 const char* title,
		 const Oxs_Director* director) const;
  void RestoreState(const char* filename,
		    const Oxs_Mesh* import_mesh,
		    const Oxs_MeshValue<OC_REAL8m>* import_Ms,
		    const Oxs_MeshValue<OC_REAL8m>* import_Ms_inverse,
                    OC_REAL8m import_max_absMs,
		    const Oxs_Director* director,
		    String& export_MIF_info);
  /// Note: RestoreState will throw an error if the input file can't
  /// be found, has bad header and/or data, or if the restored problem
  /// doesn't match the current (active) problem, as detected by CRC
  /// comparison or problem "parameter" check.

  // IMPORTANT NOTICE:  Any changes to Oxs_SimState's public interface
  // should be reflected in Oxs_Driver and children state initialization
  // functions.
};

#endif // _OXS_SIMSTATE
