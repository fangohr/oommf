/* FILE: director.h                 -*-Mode: c++-*-
 *
 * 3D solver director class.
 *
 */

#ifndef _OXS_DIRECTOR
#define _OXS_DIRECTOR

#include <map>
#include <string>
#include <vector>

#include "oc.h"
#include "key.h"
#include "simstate.h"
#include "mesh.h"
#include "meshvalue.h"
#include "threevector.h"
#include "oxsthread.h"

// Microsoft Visual C++ version 5 apparently wants energy.h included
// here.  However, this causes problems with the SGI "MIPSpro Version
// 7.4.4m" (aka "CC") compiler during compilation of output.cc.
// output.cc needs to include director.h, and energy.h includes
// output.h, and this apparently causes some circular dependency
// problems.  The VC++ 5 compiler is rather old at this point, so
// given the choice of supporting either VC++ or the SGI compiler,
// we'll opt for the latter.  However, if someone is trying to build
// with MSVC++ 5 and is running into problems, they can try
// uncommenting the following line.
//#include "energy.h"   // Needed by MSVC++ 5?

OC_USE_STD_NAMESPACE;  // Specify std namespace, if supported
OC_USE_EXCEPTION;
OC_USE_STRING;

/* End includes */

class Oxs_Ext;       // Forward declarations
class Oxs_Energy;
class Oxs_Driver;
class Oxs_Output;

// KLUDGE KLUDGE KLUDGE
// Oxs_ProbRelease should be declared using the Tcl_CmdProc
// typedef, but when done that way the Compaq C++ compiler
// (V6.3-005 for linux) complains about linkage mismatch.
// This kludge works around the problem.  See also the
// declaration as "friend" in the Oxs_Director class.
extern "C" {
  int Oxs_ProbRelease(ClientData,Tcl_Interp*,int,CONST84 char**);
}
// KLUDGE KLUDGE KLUDGE

// Supplemental types
enum OxsRunEventTypes { OXS_INVALID_EVENT,
                        OXS_STEP_EVENT,
                        OXS_STAGE_DONE_EVENT,
                        OXS_RUN_DONE_EVENT };
struct OxsRunEvent {
public:
  OxsRunEventTypes event;
  Oxs_ConstKey<Oxs_SimState> state_key;
  OxsRunEvent() : event(OXS_INVALID_EVENT) {}
  OxsRunEvent(const OxsRunEventTypes& event_,
              const Oxs_ConstKey<Oxs_SimState>& state_key_)
    : event(event_), state_key(state_key_) {
    state_key.GetReadReference();  // Set and hold read lock
  }
};
/// IMPORTANT NOTE: OxsRunEvent holds a read lock on its associated
/// state from the time of the OxsRunEvent's creation until its
/// destruction.
OC_BOOL operator<(const OxsRunEvent&, const OxsRunEvent&);
OC_BOOL operator>(const OxsRunEvent&, const OxsRunEvent&);
OC_BOOL operator==(const OxsRunEvent&, const OxsRunEvent&);

// 3D solver director
class Oxs_Director {
private:
  static void ExitProc(ClientData);

  Tcl_Interp* const interp;

  OC_UINT4m problem_count; // Number of problems loaded across history
  /// of this instance.  Initialized to 0, then incremented to 1 when
  /// first problem is loaded, and incremented by 1 more each time
  /// a new problem is loaded.

  OC_UINT4m problem_id; // Current problem count id.  Set to 0
  /// if no problem is loaded.  Otherwise should equal problem_count.
  /// NB: ProbRun is disabled if problem_id==0.

  String problem_name; // Name of MIF file providing current
  /// problem.  Might want to include some additional identifying
  /// information, such as perhaps a CRC?

  int restart_flag;  // The restart_flag controls problem load
  /// behavior. If restart_flag is 0, then the problem is loaded fresh.
  /// If restart_flag is 1, then checkpoint data is used, or an error
  /// is raised if no checkpoint data is available.  If restart_flag is
  /// 2, then checkpoint data is used, if available, otherwise the
  /// problem is loaded fresh without raising an error.  Other values
  /// are invalid.

  OC_BOOL restart_crc_check_flag; // Used in conjuction with the
  /// restart flag to specify whether CRC checking is performed
  /// to validate correspondence between MIF and restart files.

  // MIF object, which holds mif_interp, a slave of interp.  This is
  // created inside ProbInit(), and is held until Release()
  String mif_object;

  // Problem release/director reset routines.
  void ForceRelease();
  void Release();
  // KLUDGE KLUDGE KLUDGE
  friend int Oxs_ProbRelease(ClientData,Tcl_Interp*,int,CONST84 char**);
  // friend Tcl_CmdProc Oxs_ProbRelease;
  // Oxs_ProbRelease should be declared using the Tcl_CmdProc
  // typedef, but when done that way the Compaq C++ compiler
  // (V6.3-005 for linux) complains about linkage mismatch.
  // This kludge works around the problem.  See also the forward
  // declaration of Oxs_ProbRelease earlier in this file.
  // KLUDGE KLUDGE KLUDGE

  OC_UINT4m error_status; // Can be set from outside Oxs_Director
  /// to indicate an error has occurred.  This can be checked
  /// by objects during cleanup, to possibly modify their
  /// behavior.  In particular, this is used by the checkpoint
  /// cleanup code in Oxs_Driver.  This flag is automatically
  /// cleared at the bottom of the Release() method.

  vector<Oxs_Ext*> ext_obj;  // Ordered list of instantiated Oxs_Ext
  /// objects.  This list "owns" the objects, and is responsible for
  /// their destruction.
  map<String,Oxs_Ext*> ext_map; // Mapping of Ext names to objects
  vector<Oxs_Energy*> energy_obj; // Ordered list of energy terms.  This
  // is a sublist of ext_obj.

  vector<Oxs_Output*> output_obj; // List of all registered output objects
  /// When an output object deregisters, its slot in output_obj is
  /// replaced with a NULL pointer.  This makes it easy for external
  /// code (in particular, Tcl script) to refer to a registered object---
  /// once an object is registered its index (i.e., position in the
  /// output_obj list) never changes.  The output_obj list is reset each
  /// time a new problem is loaded.  However, this is a poor algorithm
  /// if during one problem many output objects register and deregister,
  /// since the size of output_obj will grow accordingly.  At some point
  /// a different indexing scheme should replace this.  Perhaps a hash
  /// lookup?

  const char* MakeOutputToken(unsigned long index,unsigned long probid) const;
  Oxs_Output* InterpretOutputToken(const char* token) const;

  Oxs_Driver* driver; // Unique driver object.  This can be changed
  /// to a container if in the future we want to support multiple
  /// drivers in one problem run.

  // Array of held simulation states.  See discussion below concerning
  // ReserveSimulationStateRequest() and GetNewSimulationState().  This
  // array "owns" the simulations states, i.e., is responsible for their
  // destruction.
  OC_UINT4m simulation_state_reserve_count;
  vector<Oxs_SimState*> simulation_state;

  // NB: Because ext_map and energy_obj hold duplicate pointers to
  // objects stored in ext_obj, one has to be careful to release all
  // pointers at the same time.  This is handled inside the Release()
  // function.  We could store Oxs_Keys in ext_map and energy_obj
  // instead of pointers; this would provide notification of hanging
  // pointers, but since Oxs_Director is the owner of these objects, at
  // present I don't think this is necessary. -mjd

  // Director is responsible for deletion of simulation_state objects
  // on destruction.  This responsibility shouldn't be copied, so
  // disable copy constructor and assignment operator by declaring them
  // but then not providing a definition.  In any event, there should be
  // only one director!
  Oxs_Director(const Oxs_Director&);
  Oxs_Director& operator=(const Oxs_Director&);

public:
  Oxs_Director(Tcl_Interp* interp_);
  ~Oxs_Director();  // Non-virtual.  Oxs_Director is not intended to
  // have children.

  int VerifyInterp(Tcl_Interp* i) { return (i==interp ? 1 : 0); }


  int ExtCreateAndRegister(const char* key,
                           const char* initstr,
                           String& bad_specs);
  /// ExtCreateAndRegister is conceptually a helper class for ProbInit.
  /// In practice, it is called (through oxscmds.cc) by the Specify
  /// command in mif.tcl.
  ///   Given a spec block, creates Oxs_Ext object and stores the pointer
  /// in ext_obj, and mapping on id in ext_map.  On success, 0 is
  /// returned and bad_specs is unmodified.
  ///   If an error occurs because the Oxs_Ext type is unrecognized,
  /// of if the instance name is already in use, then an error message
  /// is appended to the bad_specs import, and 1 is returned.
  ///   If some other error occurs during construction, then any Oxs_Ext
  /// objects previously constructed are deleted.  For errors that throw
  /// Oxs_ExtError or Oxs_Exception errors, the bad_specs list is
  /// appended to the error message and the exception is re-thrown.

  int ProbInit(const char* filename,const char* mif_parameters);

  const char* GetProblemName() const { return problem_name.c_str(); }

  int SetRestartFlag(int flag);
  int GetRestartFlag() const { return restart_flag; }
  /// See explanation of restart_flag above.

  int ProbReset();

  void SetErrorStatus(OC_UINT4m code) { error_status = code; }
  OC_UINT4m GetErrorStatus() const { return error_status; }
  /// See notes above concerning error_status.

  void ProbRun(vector<OxsRunEvent>& results);
  /// Fills results with event list.  Throws an exception on error.


  // GetIteration, GetStage, GetNumberOfStages, and SetStage
  // all throw exceptions on error.  SetStage returns the
  // actual stage number.
  int GetIteration();
  int GetStage();
  int GetNumberOfStages();
  int SetStage(int requestedStage);

  String GetMifHandle() const;
  Tcl_Interp* GetMifInterp() const;

  OC_UINT4m GetMifCrc() const;
  /// Returns CRC of buffer representation of current MIF file.
  /// Throws an error if no problem is loaded.

  void SetRestartCrcCheck(OC_BOOL flag) {
    restart_crc_check_flag = flag;
  }
  OC_BOOL CheckRestartCrc() const { return restart_crc_check_flag; }

  String GetMifParameters() const;
  /// Returns parameters string as stored in current MIF
  /// object.  Throws an error if no problem is loaded.

  int CheckMifParameters(const String& test_params) const;

  OC_BOOL GetMifOption(const char* label,String& value) const;
  /// Fills "value" with the value associated with "label".
  /// Returns 1 on success, or 0 if "label" has not been set.
  /// Throws an error if no problem is loaded.
  /// NOTE: "value" is guaranteed unchanged if return value is 0.

  int GetLogFilename(String& logname) const;
  /// Returns name of log file registered with FileLogger
  /// in the main Tcl interpreter.

  int GetDriverInstanceName(String& name) const;

  const Oxs_Driver* GetDriver() const {
    return driver;
  }

  int StoreExtObject(Oxs_OwnedPointer<Oxs_Ext>& obj);
  /// Stores *obj in the ext_obj array and registers it in ext_map.  On
  /// entry, obj *must* have ownership of obj.  As a result of this call,
  /// ownership is transferred to the director through the ext_obj array.
  /// This is reflected upon return in obj.  In other words,
  /// obj.IsOwner() must be true on entry, and if this function is
  /// successful then obj.IsOwner() will be false on exit.  The ptr value
  /// of obj will not be changed in any event.
  ///    If appropriate, puts a pointer in energy_obj and sets the
  /// "driver" member variable.
  ///   This routine is used primarily from the Oxs_Director::ProbInit
  /// member function during the processing of the input MIF file; i.e.,
  /// all the Specify blocks create an Oxs_Ext object that is stored
  /// through this function.  However, this interface is exposed for the
  /// case where one Oxs_Ext object wants to create another Oxs_Ext
  /// object itself, but make the new object both visible to the world
  /// (via ext_map) and to pass off resposibility (ownership) to the
  /// director.
  ///   This routine returns 0 on success.  Otherwise ownership of obj
  /// was *not* transfered to ext_obj, so the client remains responsible
  /// for proper cleanup.  A return value of 1 means that obj did not
  /// have ownership of the Oxs_Ext object.  A return value of 2
  /// indicates that the failure was caused by a naming conflict in
  /// ext_map, i.e., the instance name was already registered.

  Oxs_Ext* FindExtObject(const String& id) const;
  /// Returns a pointer to the element of ext_map with instance id
  /// 'id'.  Returns NULL if the requested object is not
  /// found.  For convenience, some partial matches are allowed.  See
  /// the routine implementation for details.
  ///    It is recommended that the client place a dep lock on the
  /// object if it is to be used for an extended period.  All such locks
  /// must be released before the corresponding object in ext_map can be
  /// deleted.  This means that locks held by ext_map objects to other
  /// ext_map objects should be only to objects earlier in the ext_obj
  /// list than themselves.  Perhaps there should be some type of access
  /// control on this function?

  String ListExtObjects() const;  // Mainly for debugging.
  String ListEnergyObjects() const;  // Mainly for debugging.

  // The following output (de)registration routines throw exceptions
  // on failure. NOTE: All objects registering output are expected to
  // automatically deregister as a consequence of the object destructions
  // occuring in Oxs_Director::Release().  See also output_map object
  // above.
  void RegisterOutput(Oxs_Output*);
  void DeregisterOutput(const Oxs_Output*);
  OC_BOOL IsRegisteredOutput(const Oxs_Output*) const;
  void ListOutputObjects(vector<String> &outputs) const; // Returns token
  /// list, sorted first by priority and second by registration order.
  int Output(const char* output_token,
             Tcl_Interp* interp,
             int argc,CONST84 char** argv);
  int OutputCacheRequestIncrement(const char* output_token,
                                  OC_INT4m incr) const;
  void OutputNames(const char* output_token,vector<String>& names) const;
  /// Fills export "names" with, in order, owner_name, output_name,
  /// output_type, and output_units, for output object associated
  /// with output_token.

  Oxs_Output* FindOutputObject(const char* regexp) const;
  /// C++ interface for searching the output object list.  This routine
  /// matches the import regexp against the output objects list.  If a
  /// unique match is found, then a pointer to that object is returned.
  /// Otherwise, a NULL pointer is returned.  The match is performed
  /// using the Tcl regexp engine.  Since this routine will typically
  /// check through the entire list of output objects, ideally this
  /// routine is called after problem initialization, and the result is
  /// cached until the problem is released.

  Oxs_Output* FindOutputObjectExact(const char* outname) const;
  /// Analogous to ::FindOutputObject, except this routine does a
  /// standard strcmp instead of a regexp search.  Furthermore, this
  /// routine assumes that registered output names are distinct, so a
  /// NULL return indicates no match (as opposed to
  /// ::FindOutputObject, where a NULL return possibly means more than
  /// one match).

  // Energy object index access.  The last returns a *copy* of the
  // energy_obj vector.
  OC_UINT4m GetEnergyObjCount() const {
    return static_cast<OC_UINT4m>(energy_obj.size());
  }
  Oxs_Energy* GetEnergyObj(OC_UINT4m i) const;
  vector<Oxs_Energy*> GetEnergyObjects() const { return energy_obj; }

  // Collect stage requests from all registered Oxs_Ext objects.
  void ExtObjStageRequestCounts(unsigned int& min,
                                unsigned int& max) const;

  // SIMULATION STATE MANAGEMENT: During construction, each Oxs_Ext
  // object that wants to create and/or hold simulation states should
  // register its intent with the director using
  // ReserveSimulationStateRequest().  The states are actually
  // constructed only as needed, but the count obtained via reserve
  // calls is used as an upper bound to protect against memory leaks
  // swallowing up all of system memory.  Objects obtain keys (handles)
  // to new states with the GetNewSimulationState() call.  The key
  // initially holds a write lock on the object.  The caller can change
  // the lock state, make copies, etc., as desired.  The
  // GetNewSimulationState() routine scans through the list of states
  // held in the simulation_state array, looking for a pre-created state
  // that has no locks on it.  If one is found, a key is returned for
  // that state.  If all existing states in the simulation_state array
  // are locked, then a new state is created (subject to the max state
  // count obtained from ReserveSimulationStateRequest() calls).  In all
  // cases the returned key holds a write lock on the state.  The states
  // are actually deleted only inside the director Release() command,
  // after destruction of all Oxs_Ext objects.
  //   NOTE: The key passed in to GetNewSimulationState() is allowed to
  // hold a lock on an Oxs_SimState, *IF* that state was obtained from a
  // previous call to GetNewSimulationState().  On entry,
  // GetNewSimulationState() releases any lock currently held by the
  // import key, but then immediately tests if the corresponding
  // Oxs_SimState is then free.  If so, then GetNewSimulationState()
  // resets the key to the same object.  Therefore, if the chance is
  // good that the input key holds the sole lock on an object, then the
  // lock should *not* be released before calling
  // GetNewSimulationState().
  void ReserveSimulationStateRequest(OC_UINT4m reserve_count);
  void GetNewSimulationState(Oxs_Key<Oxs_SimState>& write_key);

  // FindExistingSimulationState scans through the simulation state
  // list and returns a read-only pointer to the one matching the
  // import id.  The import id must by positive.  The return value
  // is NULL if no matching id is found.
  const Oxs_SimState* FindExistingSimulationState(OC_UINT4m id) const;

  // Hook for develop tests
  int DoDevelopTest(const String& in, String& out);
};

#endif // _OXS_DIRECTOR
