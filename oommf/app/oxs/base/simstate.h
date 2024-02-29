/* FILE: simstate.h                 -*-Mode: c++-*-
 *
 * Simulation state class.
 *
 */

#ifndef _OXS_SIMSTATE
#define _OXS_SIMSTATE


#include <memory> // Shared pointers
#include <string>
#include <unordered_map>
#include <vector>

#include "oc.h"
#include "lock.h"
#include "meshvalue.h"
#include "util.h"
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
  //  have locks placed on them by other Oxs_Ext objects.  Therefore,
  //  Oxs_SimState objects are placed at the root of the dependency
  //  hierarchy, and are deleted by Oxs_Director only after all Oxs_Ext
  //  objects are destroyed.  It is therefore the responsibility of each
  //  object holding a lock on an Oxs_SimState object to place locks on
  //  or otherwise guarantee the correctness of all pointers held within
  //  the Oxs_SimState object.
  // Note 2: If the Oxs_SimState structure changes, then the convenience
  //  method CloneHeader should be updated to reflect those changes.
private:
  typedef String DerivedDataKey;
  typedef String AuxDataKey;

  // NOTE: The key type for std::unordered_map can't be const; this
  //       might be related to LWG issue 2469 fixed in C++17.
  mutable std::unordered_map<DerivedDataKey,OC_REAL8m> derived_data;
  mutable std::unordered_map<DerivedDataKey, Oxs_MeshValue<OC_REAL8m> >
  derived_scalar_meshvalue_data;
  mutable std::unordered_map<DerivedDataKey,Oxs_MeshValue<ThreeVector> >
     derived_threevector_meshvalue_data;
  mutable std::unordered_map<AuxDataKey,OC_REAL8m> auxiliary_data;

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

  // Ms arrays. Client objects typically use Ms and Ms_inverse.
  // reference_Ms can be used by Oxs_StateInitializer objects to modify
  // Ms (and Ms_inverse---use MsSetup) between states. The CloneHeader
  // code simply copies all three pointers.

  //    The MsSetup convenience function moves ownership from Ms_import
  // (unique_ptr) into the const shared_ptr Ms, and creates a new
  // Oxs_MeshValue<OC_REAL8m> object and fills it with 1/Ms[i] values
  // (with the convention that Ms_inverse[i]=0 if Ms[i]=0). Ms_inverse
  // is pointed to this object, and max_absMs is set appropriately.
  std::shared_ptr< const Oxs_MeshValue<OC_REAL8m> > reference_Ms;
  std::shared_ptr< const Oxs_MeshValue<OC_REAL8m> > Ms;
  std::shared_ptr< const Oxs_MeshValue<OC_REAL8m> > Ms_inverse; // 1/Ms
  OC_REAL8m max_absMs;
  void MsSetup(std::unique_ptr< Oxs_MeshValue<OC_REAL8m> > Ms_import);
  // NB: MsSetup requires transfer of ownership of Ms_import so that
  //     (const) Ms can be freely shared with other Oxs_SimState objects
  //     without concern of any code surreptitiously modifying Ms.

  // Spin array
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
  static String StatusToString(SimStateStatus& stat) {
    String buf = "Invalid SimStateStatus value";
    switch(stat) {
    case UNKNOWN:  buf = "Unknown";   break;
    case NOT_DONE: buf = "Not_Done" ; break;
    case DONE:     buf = "Done" ;     break;
    default: break;
    }
    return buf;
  }

  // AddDerivedData and GetDerivedData return 1 if operation
  // was successful, 0 otherwise.  In particular, AddDerivedData
  // fails if name is already used.  (Maybe this should be changed
  // to throw an exception?)
  //
  // Single scalar values:
  OC_BOOL AddDerivedData(const String& name,OC_REAL8m value) const;
  OC_BOOL AddDerivedData(const char* name,OC_REAL8m value) const {
    return AddDerivedData(String(name),value);
  }

  OC_BOOL GetDerivedData(const String& name,OC_REAL8m& value) const;
  OC_BOOL GetDerivedData(const char* name,OC_REAL8m& value) const {
    return GetDerivedData(String(name),value);
  }

  OC_BOOL HaveDerivedData(const String& name) const {
    return (derived_data.count(name)>0 ? 1 : 0);
  }
  OC_BOOL HaveDerivedData(const char* name) const {
    return HaveDerivedData(String(name));
  }

  // NB AddDerivedData+Oxs_MeshValue PERFORMANCE NOTE:
  // The const AddDerivedData(char*/String,const Oxs_MeshValue<T>&)
  // routines use the expensive Oxs_MeshValue<T> copy constructors to
  // fill their unordered_map objects. If you want a cheap shared copy
  // then create an rvalue with Oxs_MeshValue<T>.SharedCopy() and feed
  // that into AddDerivedData(char*/String,Oxs_MeshValue<T>&&) instead,
  // e.g.,
  //    foo.AddDerivedData("bar-name",std::move(bar.SharedCopy()));
  // Note that as a side effect of SharedCopy(), both bar and the image
  // in foo will be marked as read-only.

  // UPDATE: Expensive copy versions are disabled until a compelling
  //         use case is found. If clients want an unshared copy,
  //         they can use the MeshValue copy constructor and move
  //         the copy into DerivedData. For example, you can do
  //         something like this:
  //
  //    state.AddDerivedData(total_H_name,
  //                         std::move(Oxs_MeshValue<ThreeVector>(*H)));

  // MeshValue array of scalar values:
#if 0 // Expensive copy
  OC_BOOL AddDerivedData(const String& name,
                         const Oxs_MeshValue<OC_REAL8m>& value) const;
  OC_BOOL AddDerivedData(const char* name,
                         const Oxs_MeshValue<OC_REAL8m>& value) const {
    return AddDerivedData(String(name),value);
  }
#endif // Expensive copy
  OC_BOOL AddDerivedData(const String& name, // Move assignment version
                         Oxs_MeshValue<OC_REAL8m>&& value) const;
  OC_BOOL AddDerivedData(const char* name,   // Move assignment version
                         Oxs_MeshValue<OC_REAL8m>&& value) const {
    return AddDerivedData(String(name),std::move(value));
  }
  OC_BOOL GetDerivedData(const String& name,
                         const Oxs_MeshValue<OC_REAL8m>* &value) const;
  OC_BOOL GetDerivedData(const char* name,
                         const Oxs_MeshValue<OC_REAL8m>* &value) const {
    return GetDerivedData(String(name),value);
  }
  OC_BOOL HaveDerivedData(const String& name,
                          const Oxs_MeshValue<OC_REAL8m>*) const {
    // Second argument is simply for overload selection (not used)
    return (derived_scalar_meshvalue_data.count(name)>0 ? 1 : 0);
  }
  OC_BOOL HaveDerivedData(const char* name,
                          const Oxs_MeshValue<OC_REAL8m>* dummy) const {
    return HaveDerivedData(String(name),dummy);
  }

  // MeshValue array of ThreeVector values:
#if 0 // Expensive copy
  OC_BOOL AddDerivedData(const String& name,
                         const Oxs_MeshValue<ThreeVector>& value) const;
  OC_BOOL AddDerivedData(const char* name,
                         const Oxs_MeshValue<ThreeVector>& value) const {
    return AddDerivedData(String(name),value);
  }
#endif // Expensive copy
  OC_BOOL AddDerivedData(const String& name, // Move assignment version
                         Oxs_MeshValue<ThreeVector>&& value) const;
  OC_BOOL AddDerivedData(const char* name,   // Move assignment version
                         Oxs_MeshValue<ThreeVector>&& value) const {
    return AddDerivedData(String(name),std::move(value));
  }
  OC_BOOL GetDerivedData(const String& name,
                         const Oxs_MeshValue<ThreeVector>* &value) const;
  OC_BOOL GetDerivedData(const char* name,
                         const Oxs_MeshValue<ThreeVector>* &value) const {
    return GetDerivedData(String(name),value);
  }
  OC_BOOL HaveDerivedData(const String& name,
                          const Oxs_MeshValue<ThreeVector>*) const {
    // Second argument is simply for overload selection (not used)
    return (derived_threevector_meshvalue_data.count(name)>0 ? 1 : 0);
  }
  OC_BOOL HaveDerivedData(const char* name,
                          const Oxs_MeshValue<ThreeVector>* dummy) const {
    return HaveDerivedData(String(name),dummy);
  }

  // NB: Oxs_Ext objects accessing any of the Add/GetDerivedData
  // or Add/GetAuxData routines should use the Oxs_Ext::DataName()
  // wrapper to get an instance-specific item name.

  // Note that since all the List* variants have the same signature, we
  // have to use distinct names to distinguish between them.
  void ListDerivedData(vector<String>& names) const;
  void ListDerivedScalarMeshValueData(vector<String>& names) const;
  void ListDerivedThreeVectorMeshValueData(vector<String>& names) const;

  void ClearDerivedData(); // This function only changes mutable
  /// values (the derived_data maps), so it could be made "const".
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
  void AddAuxData(const String& name,OC_REAL8m value) const;
  void AddAuxData(const char* name,OC_REAL8m value) const {
    AddAuxData(String(name),value);
  }
  OC_BOOL GetAuxData(const String& name,OC_REAL8m& value) const;
  OC_BOOL GetAuxData(const char* name,OC_REAL8m& value) const {
    // Returns 1 on success, 0 if name is an unknown key.
    return GetAuxData(String(name),value);
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
		    const Oxs_Director* director,
		    String& export_MIF_info);
  /// Note: RestoreState will throw an error if the input file can't
  /// be found, has bad header and/or data, or if the restored problem
  /// doesn't match the current (active) problem, as detected by CRC
  /// comparison or problem "parameter" check.

  // IMPORTANT NOTICE:  Any changes to Oxs_SimState's public interface
  // should be reflected in Oxs_Driver and children state initialization
  // functions.

#ifndef NDEBUG
  String DumpStateOverview() const;
#endif // NDEBUG

};

#endif // _OXS_SIMSTATE
