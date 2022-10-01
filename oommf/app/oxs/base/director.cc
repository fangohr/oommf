/* FILE: director.cc                 -*-Mode: c++-*-
 *
 * 3D solver director class.
 *
 */

#undef REPORT_CONSTRUCT_TIMES  // #define this to get timing report on stderr

#include <cstdio> // For some versions of g++ on Windows, if stdio.h is
/// #include'd after a C++-style header, then printf format modifiers
/// like OC_INDEX_MOD and OC_INT4m_MOD aren't handled properly.

#include <limits>
#include <string>
#include <vector>
#include "oc.h"
#include "nb.h"
#include "director.h"
#include "driver.h"
#include "simstate.h"
#include "ext.h"
#include "energy.h"
#include "util.h"
#include "key.h"
#include "mesh.h"
#include "meshvalue.h"
#include "threevector.h"
#include "oxswarn.h"

// Read <algorithm> last, because with some pgc++ installs the
// <emmintrin.h> header is not interpreted properly if <algorithm> is
// read first.
#include <algorithm>

#ifdef REPORT_CONSTRUCT_TIMES
#include "nb.h"
#endif

/* End includes */

// The following two routines make sure that all Oxs_Director
// instances are destroyed as part of a Tcl_Exit().
void Oxs_Director::ExitProc(ClientData clientData)
{
    Oxs_Director *d = (Oxs_Director *) clientData;
    delete d;
    // On Windows, streams are not automatically flushed on exit
    fflush(stderr); fflush(stdout);
}

// Constructor
Oxs_Director::Oxs_Director(Tcl_Interp *i)
  : interp(i),
    problem_count(0),problem_id(0),
    restart_flag(0),restart_crc_check_flag(1),mif_crc(0),
    error_status(0),driver(NULL),
    simulation_state_reserve_count(0)
{
  // Register cleanup handler
  Tcl_CreateExitHandler((Tcl_ExitProc *) ExitProc, (ClientData) this);

  // Make sure tcl_precision is full
  Tcl_Eval(interp,OC_CONST84_CHAR("set tcl_precision 17"));

  if (TCL_OK != Tcl_Eval(interp,
      OC_CONST84_CHAR("Oc_Main Preload Oc_TempName Oc_TempFile"))) {
    OXS_THROW(Oxs_IncompleteInitialization,"tempfile support unavilable");
  }
}

int Oxs_Director::ProbReset()
{ // Sets micromag problem to initial state.
  // Returns TCL_OK on success, TCL_ERROR otherwise.
  int errcode = TCL_OK;
  if(error_status!=0) {
    OXS_THROW(Oxs_ProgramLogicError,"error status not cleared");
  }

  // (Re)set random number generators.
  {
    Oc_AutoBuf buf(mif_object.c_str());
    buf.Append(" InitRandom");
    Oxs_TclInterpState tcl_state(interp);
    errcode = Tcl_Eval(interp,buf.GetStr());
    if(errcode!=TCL_OK) return errcode;
    tcl_state.Restore();
  }

  // Run Init() on all Oxs_Ext's
  vector<Oxs_Ext*>::iterator it = ext_obj.begin();
  try {
    while(it!=ext_obj.end() && errcode==TCL_OK) {
      if(!((*it)->Init())) errcode = TCL_ERROR;
      it++;
    }
  } catch (Oxs_ExtError& err) {
    String head = String("Error in Init() function of ")
      + String((*it)->InstanceName())
      + String(" --- ");
    err.Prepend(head);
    throw;
  } catch (Oxs_Exception& err) {
    String head = String("Error in Init() function of ")
      + String((*it)->InstanceName())
      + String(" --- ");
    err.Prepend(head);
    throw;
  }

  return errcode;
}

void Oxs_Director::ProbRun(vector<OxsRunEvent>& results)
{ // Fills results with event list.  Throws exception on error.

  if(driver==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
  }
  driver->Run(results);  // This may throw several exceptions.
}

int Oxs_Director::SetRestartFlag(int flag)
{ // The restart_flag controls problem load behavior. If restart_flag
  // is 0, then the problem is loaded fresh.  If restart_flag is 1,
  // then checkpoint data is used, or an error is raised if no
  // checkpoint data is available.  If restart_flag is 2, then
  // checkpoint data is used, if available, otherwise the problem is
  // loaded fresh without raising an error.  Other values are invalid.
  //   On success, the return value is the old flag value.  An exception
  // is thrown on error.
  if(flag<0 || flag>2) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid restart flag value: %d; should be 0, 1, or 2.",
                flag);
    OXS_THROW(Oxs_BadParameter,buf);
  }
  int old_flag = restart_flag;
  restart_flag = flag;
  return old_flag;
}

int Oxs_Director::GetIteration()
{
  if(driver==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
  }

  int iteration = -1;
  try {
    iteration = static_cast<int>(driver->GetIteration());
  } catch (Oxs_ExtError& err) {
    String head =
      String("Oxs_Ext error inside GetIteration function of driver ")
      + String(driver->InstanceName())
      + String(" --- ");
    err.Prepend(head);
    throw;
  } catch (Oxs_Exception& err) {
    String head =
      String("Error inside GetIteration function of driver ")
      + String(driver->InstanceName()) + String(" --- ");
    err.Prepend(head);
    throw;
  }

  return iteration;
}

int Oxs_Director::GetStage()
{
  if(driver==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
  }

  int stage = -1;
  try {
    stage = static_cast<int>(driver->GetStage());
  } catch (Oxs_ExtError& err) {
    String head =
      String("Oxs_Ext error inside GetStage function of driver ")
      + String(driver->InstanceName())
      + String(" --- ");
    err.Prepend(head);
    throw;
  } catch (Oxs_Exception& err) {
    String head =
      String("Error inside GetStage function of driver ")
      + String(driver->InstanceName()) + String(" --- ")
      + String(err.MessageType()) + String(" --- ");
    err.Prepend(head);
    throw;
  }

  return stage;
}

int Oxs_Director::GetNumberOfStages()
{
  if(driver==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
  }

  int stage_count = -1;
  try {
    stage_count = static_cast<int>(driver->GetNumberOfStages());
  } catch (Oxs_ExtError& err) {
    String head =
      String("Oxs_Ext error inside GetNumberOfStages function of driver ")
      + String(driver->InstanceName())
      + String(" --- ");
    err.Prepend(head);
    throw;
  } catch (Oxs_Exception& err) {
    String head =
      String("Error inside GetNumberOfStages function of driver ")
      + String(driver->InstanceName()) + String(" --- ");
    err.Prepend(head);
    throw;
  }

  return stage_count;
}

int Oxs_Director::SetStage
(int requestedStage,
 vector<OxsRunEvent>& events)
{
  if(driver==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
  }

  if(requestedStage<0) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "Invalid stage request: %d; stage must be non-negative.",
                requestedStage);
    OXS_THROW(Oxs_BadParameter,buf);
  }
  // Should we also check for requestedStage too large?

  try {
    driver->SetStage(static_cast<OC_UINT4m>(requestedStage),events);
  } catch (Oxs_ExtError& err) {
    String head =
      String("Oxs_Ext error inside SetStage function of driver ")
      + String(driver->InstanceName())
      + String(" --- ");
    err.Prepend(head);
    throw;
  } catch (Oxs_Exception& err) {
    String head =
      String("Error inside SetStage function of driver ")
      + String(driver->InstanceName()) + String(" --- ");
    err.Prepend(head);
    throw;
  }

  return driver->GetStage();
}

int Oxs_Director::IsRunDone()
{ // Reads current Oxs_SimState run_done status.  Returns 1 if run_done
  // == DONE, 0 if == NOT_DONE, and -1 otherwise.  Note that this call
  // does not explicitly check any stopping criteria, it only peeks at
  // the current Oxs_SimState structure to see if a done state has been
  // recorded.  In particular, when a problem is loaded from scratch the
  // run done status is UNKNOWN until the first step is taken.  This
  // allows a problem to be loaded quickly and the MIF file checked for
  // structural errors, without requiring an potentially expensive demag
  // computation.  The main use of this call is to check checkpoint file
  // loads.  (Checkpoint files contain the Oxs_SimState::SimStateStatus
  // values.)
  if(driver==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
  }
  const Oxs_SimState* cstate = driver->GetCurrentState();
  if(cstate->run_done == Oxs_SimState::SimStateStatus::NOT_DONE) {
    return 0;
  }
  if(cstate->run_done == Oxs_SimState::SimStateStatus::DONE) {
    return 1;
  }
  return -1;
}


OC_UINT4m Oxs_Director::GetCurrentStateId()
{
  if(driver==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
  }

  OC_UINT4m id = 0;
  try {
    id = driver->GetCurrentStateId();
  } catch (Oxs_ExtError& err) {
    String head =
      String("Oxs_Ext error inside GetCurrentStateId function of driver ")
      + String(driver->InstanceName())
      + String(" --- ");
    err.Prepend(head);
    throw;
  } catch (Oxs_Exception& err) {
    String head =
      String("Error inside GetCurrentStateId function of driver ")
      + String(driver->InstanceName()) + String(" --- ");
    err.Prepend(head);
    throw;
  }

  return id;
}

String Oxs_Director::GetMifHandle() const
{
  if(problem_id == 0) {
    OXS_THROW(Oxs_IncompleteInitialization,"no problem loaded");
  }
  return mif_object;
}

OC_UINT4m Oxs_Director::ComputeMifCrc()
{ // Returns the CRC of the buffer representation of the MIF file
  // most recently read into the current MIF object.
  // This routine throws an exception if a problem is not loaded.
  //
  // Note: This code checks mif_object.empty() to determine
  // if a MIF object is available, instead of problem_id.  This
  // allows this routine to be called during ProbInit.

  if(mif_object.empty()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Error reading MIF CRC: no MIF object found.");
  }
  String cmd = mif_object + String(" GetCrc");

  Oxs_TclInterpState orig_result(interp);
  int errcode = Tcl_Eval(interp,OC_CONST84_CHAR(cmd.c_str()));
  String evalresult = Tcl_GetStringResult(interp);
  if(errcode!=TCL_OK) {
    // Extended error info
    const char* ei = Tcl_GetVar(interp,OC_CONST84_CHAR("errorInfo"),
                                TCL_GLOBAL_ONLY);
    const char* ec = Tcl_GetVar(interp,OC_CONST84_CHAR("errorCode"),
                                TCL_GLOBAL_ONLY);
    if(ei==NULL) ei = "";
    if(ec==NULL) ec = "";
    OXS_TCLTHROW(evalresult,String(ei),String(ec));
  }
  orig_result.Restore();

  OC_UINT4m crc = static_cast<OC_UINT4m>(strtoul(evalresult.c_str(),NULL,0));

  return crc;
}

OC_UINT4m Oxs_Director::GetMifCrc() const
{
  if(mif_object.empty()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Error reading MIF CRC: no MIF object found.");
  }
  return mif_crc;
}

String Oxs_Director::ReadMifParameters()
{ // Returns a string representation of a the "-parameters" list
  // specified on the command line, as stored in the current MIF object.
  // When parsed, this list should have an even number of elements.
  // This routine throws an error if a problem is not loaded.
  //
  // Note: This code checks mif_object.empty() to determine
  // if a MIF object is available, instead of problem_id.  This
  // allows this routine to be called during ProbInit.

  if(mif_object.empty()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Error reading MIF parameters: no MIF object found.");
  }
  String cmd = mif_object + String(" GetParameters");

  Oxs_TclInterpState orig_result(interp);
  int errcode = Tcl_Eval(interp,OC_CONST84_CHAR(cmd.c_str()));
  String params = Tcl_GetStringResult(interp);
  if(errcode!=TCL_OK) {
    // Extended error info
    const char* ei = Tcl_GetVar(interp,OC_CONST84_CHAR("errorInfo"),
                                TCL_GLOBAL_ONLY);
    const char* ec = Tcl_GetVar(interp,OC_CONST84_CHAR("errorCode"),
                                TCL_GLOBAL_ONLY);
    if(ei==NULL) ei = "";
    if(ec==NULL) ec = "";
    OXS_TCLTHROW(params,String(ei),String(ec));
  }
  orig_result.Restore();

  return params;
}

String Oxs_Director::GetMifParameters() const
{
  if(mif_object.empty()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Error reading MIF CRC: no MIF object found.");
  }
  return mif_parameters;
}

int Oxs_Director::CheckMifParameters(const String& test_params) const
{ // Compares import test_params to the parameter list held
  // in the current MIF object.  Returns 1 if they are the same
  // 0 if different.
  //
  // Note: This code checks mif_object.empty() to determine
  // if a MIF object is available, instead of problem_id.  This
  // allows this routine to be called during ProbInit.

  if(mif_object.empty()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Error in Oxs_Directory::CheckMifParameters:"
                " no MIF object found.");
  }
  String cmd = mif_object + String(" CheckParameters")
    + String(" [list ") + test_params + String(" ]");

  Oxs_TclInterpState orig_result(interp);
  int errcode = Tcl_Eval(interp,OC_CONST84_CHAR(cmd.c_str()));
  String evalresult = static_cast<String>(Tcl_GetStringResult(interp));
  if(errcode!=TCL_OK) {
    // Extended error info
    const char* ei = Tcl_GetVar(interp,OC_CONST84_CHAR("errorInfo"),
                                TCL_GLOBAL_ONLY);
    const char* ec = Tcl_GetVar(interp,OC_CONST84_CHAR("errorCode"),
                                TCL_GLOBAL_ONLY);
    if(ei==NULL) ei = "";
    if(ec==NULL) ec = "";
    OXS_TCLTHROW(evalresult,String(ei),String(ec));
  }
  orig_result.Restore();

  int check = atoi(evalresult.c_str());

  return check;
}


OC_BOOL Oxs_Director::GetMifOption(const char* label,String& value) const
{ // Fills "value" with the value associated with "label".
  // Returns 1 on success, or 0 if "label" has not been set.
  // Throws an error if no problem is loaded.
  // NOTE: "value" is guaranteed unchanged if return value is 0.

  OC_BOOL success = 0;

  if(mif_object.empty()) {
      OXS_THROW(Oxs_ProgramLogicError,
                "Error in Oxs_Directory::GetMifOption:"
                " no MIF object found.");
  }

  String cmd = mif_object + String(" GetOption")
    + String(" [list ") + String(label) + String(" ]");
  /// The "list" construct allows for labels with embedded spaces.

  Oxs_TclInterpState orig_result(interp);
  if(Tcl_Eval(interp,OC_CONST84_CHAR(cmd.c_str())) == TCL_OK) {
    value = static_cast<String>(Tcl_GetStringResult(interp));
    success = 1;
  } // Otherwise, assume failure due to option not being set.
  orig_result.Restore();

  return success;
}

int Oxs_Director::ExtCreateAndRegister
(const char* key,
 const char* initstr,
 String& bad_specs)
{ // Given a spec block, creates Oxs_Ext object and stores the pointer
  // in ext_obj, and mapping on id in ext_map.  On success, 0 is
  // returned and bad_specs is unmodified.
  //   If an error occurs because the Oxs_Ext type is unrecognized,
  // or if the instance name is already in use, then an error message
  // is appended to the bad_specs import, and 1 is returned.
  //   For errors that throw Oxs_ExtError or Oxs_Exception errors, the
  // bad_specs list is appended to the error message and the exception
  // is re-thrown.
  //   Older versions of this function would call Release() on any
  // exception other than Oxs_ExtErrorUnregisteredType.  This would
  // delete any existing Oxs_Ext objects, the Oxs_Mif object, and
  // the child interp associated with the Oxs_Mif object.  This
  // functionality has been removed because it interferes with error
  // reporting by the Oxs_Mif object.  Instead, it is assumed that
  // the (re-)thrown exception will be caught at some higher place
  // in the call stack and appropriate clean-up will commence from
  // that point.

  int errcheck = 1;
  try {

#ifdef REPORT_CONSTRUCT_TIMES
    Nb_StopWatch watch;
    watch.Reset();
    watch.Start();
#endif

    Oxs_OwnedPointer<Oxs_Ext> spec_obj;
    spec_obj.SetAsOwner(Oxs_Ext::MakeNew(key,this,initstr));

#ifdef REPORT_CONSTRUCT_TIMES
    watch.Stop();
    Oc_TimeVal cpu,wall;
    watch.GetTimes(cpu,wall);
    fprintf(stderr,"# %40s  %12.3f %12.3f\n",
            key,double(cpu),double(wall));
#endif

    int store_error = StoreExtObject(spec_obj);
    if(store_error!=0) {
      bad_specs += String("\n ") + String(key)
        + String(" (ID name ") + spec_obj->InstanceName();
      if(store_error==2) {
        bad_specs += String("already in use.)");
      } else {
        bad_specs += String("; programming error.)");
      }
    } else {
      errcheck = 0;
    }
  } catch (Oxs_ExtErrorUnregisteredType&) {
    // Drop exception, but append to bad_spec list for later reporting.
    bad_specs += String("\n  ") + String(key);
  } catch (Oxs_ExtError& err) {
    String msg =
      String("Oxs_Ext initialization error in construction of ")
      + String(key) + String(" --- ") + String(err);
    if(bad_specs.length()>0) {
      msg += String("\nUnrecognized Specify Blocks --- ")
        + bad_specs;
    }
    Tcl_ResetResult(interp); // All result info is stored in err

    // Convert from Oxs_ExtError to an Oxs_Exception error, primarily
    // so that the OxsCmdsSwitchboard routine in oxscmds.cc doesn't
    // treat this as a fatal error and thereby short-circuit proper
    // Oxs_Mif cleanup.
    throw Oxs_IncompleteInitialization(msg);
  } catch (Oxs_Exception& err) {
    String msg =
      String("Oxs_Ext initialization error in construction of ")
      + String(key) + String(" --- ");
    err.Prepend(msg);
    if(bad_specs.length()>0) {
      msg = String("\nUnrecognized Specify Blocks --- ")
        + bad_specs;
      err.Postpend(msg);
    }
    Tcl_ResetResult(interp); // All result info is stored in err
    throw;
  } catch (Oc_Exception& err) {
    String msg =
      String("Oxs_Ext initialization error in construction of ")
      + String(key) + String(" --- ");
    err.PrependMessage(msg.c_str());
    if(bad_specs.length()>0) {
      msg = String("\nUnrecognized Specify Blocks --- ")
        + bad_specs;
      err.PostpendMessage(msg.c_str());
    }
    Tcl_ResetResult(interp); // All result info is stored in err

    // Convert from Oxs_ExtError to and Oxs_Exception error,
    // primarily so that the OxsCmdsSwitchboard routine in
    // oxscmds.cc doesn't treat this as a fatal error and
    // thereby short-circuit proper Oxs_Mif cleanup.
    Oc_AutoBuf ab;
    err.ConstructMessage(ab);
    throw Oxs_IncompleteInitialization(ab.GetStr());
  } catch (...) {
    // Unrecognized error
    throw;
  }

  return errcheck;
}


int Oxs_Director::ProbInit(const char* filename,
                            const char* mif_params)
{ // Creates an Oxs_Mif object in the master interp, and
  // has it source the problem specified by filename, which
  // should be in MIF 2.x format.  The Oxs_Mif object is
  // deleted by the director when the problem is released,
  // but no sooner.  Note that the Oxs_Mif object creates
  // a slave interpreter in its constructor, which it holds
  // until it deletes it in its destructor.
  int error_code;
  String cmd;

  Release();
  Oxs_ThreadError::ClearError();
  simulation_state_reserve_count=0;  // Safety

  problem_name = String(filename);
  problem_id = ++problem_count;

#if OC_USE_NUMA
  if(Oc_NumaReady()) {
    Oc_AutoBuf ab;
    Oc_NumaGetInterleaveMask(ab);
    fprintf(stderr,"NUMA node count/memory interleave mask: %d/%s\n",
            Oc_NumaGetNodeCount(),ab.GetStr());
  } else {
    fprintf(stderr,"NUMA not enabled.\n");
  }
#endif

  // Initialize Oxs threads.  This is a NOP on non-threaded builds.
  Oxs_ThreadTree::InitThreads(Oc_GetMaxThreadCount());

  // Create mif object; side effect: creates a slave interpreter
  error_code = Tcl_Eval(interp,OC_CONST84_CHAR("Oxs_Mif New _"));
  if(error_code!=TCL_OK) return error_code;
  mif_object = String(Tcl_GetStringResult(interp));
  Tcl_ResetResult(interp); // Make frequent calls to Tcl_ResetResult
  /// to insure that we have a clean slate on which to build error
  /// messages generated by C++ exceptions.

  // Read file into Oxs_Mif object
#ifdef REPORT_CONSTRUCT_TIMES
  fprintf(stderr,
          "#    TIMING (seconds)               "
          "                CPU        Elapsed\n");
#endif
  vector<String> cmdvec;
  cmdvec.push_back(mif_object);
  cmdvec.push_back("ReadMif");
  cmdvec.push_back(filename);
  cmdvec.push_back(mif_params);
  cmd = Nb_MergeList(cmdvec);
  cmdvec.clear();
  error_code = Tcl_Eval(interp,OC_CONST84_CHAR(cmd.c_str()));
  if(error_code!=TCL_OK) {
    Release();
    return error_code;
  }
  Tcl_ResetResult(interp);

  mif_parameters = ReadMifParameters();
  mif_crc = ComputeMifCrc();

  // Note: Once mif_object is set, the Oxs_Director functions
  // GetMifCrc(), GetMifParameters(), and CheckMifParameters()
  // become available.  For safety, the 'ReadMif' call should
  // immediately follow the 'Oxs_Mif New' call so there is no
  // opportunity for a call to one of those functions to slip
  // in before the MIF CRC and parameter values are set.  We
  // might want to consider moving the ReadMif functionality
  // into the Oxs_Mif constructor.

  // Checks for invalid input
  if(driver==NULL) {
    Tcl_AppendResult(interp,"No driver specified\n",(char *)NULL);
    Release(); // Delete any Oxs_Ext objects already constructed
    return TCL_ERROR;
  }
  Tcl_ResetResult(interp);

  // Run Init() on all Oxs_Ext's
  if(ProbReset()!=TCL_OK) {
    Release(); // Delete any Oxs_Ext objects already constructed
    return TCL_ERROR;
  }

  return TCL_OK;
}

// The following file-local not_null function is used with find_if
// on output_obj to determine if there are any registered output
// objects.
static bool not_null(const Oxs_Output* obj) { return obj!=nullptr; }

void Oxs_Director::ForceRelease()
{ // Force problem release, without worrying about
  // memory leaks.  This is intended as a failsafe
  // in case the usual Release fails.
  problem_id=0;
  problem_name.erase();
  driver=NULL;
  ext_obj.clear();
  ext_map.clear();
  energy_obj.clear();
  simulation_state.clear();
  simulation_state_reserve_count=0;
  mif_object.erase();
  output_obj.clear();
  scalar_output_obj.clear();
  chunk_output_compute_obj.clear();
  error_status = 0; // Clear error indicator (if any)
  Oxs_ThreadTree::EndThreads();  // Thread cleanup; NOP if non-threaded.
  fflush(stderr);  fflush(stdout);  // Safety
}

void Oxs_Director::Release()
{
  if(problem_id==0) {
    // problem_id==0 indicates that no problem is loaded.
    // Normally we should be able to just return here, but
    // if an error occured during a previous release, then
    // this routine may be called again from exit handlers.
    // Instead, do some safe cleanup that may leak memory,
    // but should be otherwise well behaved.
    ForceRelease();
    return;
  }

  try {
    problem_id=0;
    problem_name.erase();

    driver=NULL;  // Make sure we don't use this pointer anymore.

    // Release all Oxs_Ext's, from the back forward.  The
    // order is important so that objects at the back of the
    // list can release any dependency locks held on objects
    // at the front of the list.
    vector<Oxs_Ext*>::reverse_iterator ext_it = ext_obj.rbegin();
    while(ext_it!=ext_obj.rend()) {
      delete (*ext_it);
      ++ext_it;
    }
    // Release *all* pointers to these objects.
    ext_obj.clear();
    ext_map.clear();
    energy_obj.clear();

    // Release simulation states.  We don't care about the order.
    vector<Oxs_SimState*>::iterator sim_it = simulation_state.begin();
    while(sim_it!=simulation_state.end()) {
      OC_UINT4m lock_count;
      if((lock_count=(*sim_it)->DepLockCount())!=0) {
        fprintf(stderr,"ERROR: simstate %lu DepLockCount=%lu on exit\n",
                static_cast<unsigned long>((*sim_it)->Id()),
                static_cast<unsigned long>(lock_count));
      }
      if((lock_count=(*sim_it)->ReadLockCount())!=0) {
        fprintf(stderr,"ERROR: simstate %lu ReadLockCount=%lu on exit\n",
                static_cast<unsigned long>((*sim_it)->Id()),
                static_cast<unsigned long>(lock_count));
      }
      if((lock_count=(*sim_it)->WriteLockCount())!=0) {
        fprintf(stderr,"ERROR: simstate %lu ReadLockCount=%lu on exit\n",
                static_cast<unsigned long>((*sim_it)->Id()),
                static_cast<unsigned long>(lock_count));
      }
      delete (*sim_it);
      ++sim_it;
    }
    simulation_state.clear();
    simulation_state_reserve_count=0;

    // Delete MIF object
    Oxs_TclInterpState tcl_state(interp);
    if(mif_object.length()>0) {
      String cmd = mif_object + String(" Delete");
      Tcl_ResetResult(interp);
      if(Tcl_Eval(interp,OC_CONST84_CHAR(cmd.c_str()))!=TCL_OK) {
        tcl_state.Discard();
        OXS_THROW(Oxs_BadResourceDealloc,
                  "Error deleting MIF object during problem release.");
      }
      mif_object.erase();
    }
    tcl_state.Restore();

    mif_parameters.erase();
    mif_crc = 0;

    // Check that all output objects have deregistered
    vector<Oxs_Output*>::const_iterator output_it
      = find_if(output_obj.begin(),output_obj.end(),not_null);
    if(output_it!=output_obj.end()) {
      OXS_THROW(Oxs_BadResourceDealloc,
                "Not all output objects deregistered upon problem release.");
    }
    output_obj.clear(); // Reset output object registration list
    scalar_output_obj.clear();
    chunk_output_compute_obj.clear();

    Oxs_ThreadTree::EndThreads();  // Thread cleanup

    // Warning message cleanup
    Oxs_WarningMessage::TransmitMessageHold();
    Oxs_WarningMessage::ClearHoldAndCounts();

  } catch(...) {
    ForceRelease();
    throw;
  }
  error_status = 0; // Clear error indicator (if any)
  fflush(stderr);  fflush(stdout);  // Safety
}

Oxs_Director::~Oxs_Director()
{
  Release();
  Tcl_DeleteExitHandler((Tcl_ExitProc *) ExitProc, (ClientData) this);
  // On Windows, streams are not automatically flushed on exit
  fflush(stderr);  fflush(stdout);
}

Tcl_Interp* Oxs_Director::GetMifInterp() const
{
  Tcl_Interp* mif_interp = NULL;
  Oxs_TclInterpState istate(interp);
  String cmd = mif_object + String(" Cget -mif_interp");
  if(Tcl_Eval(interp,OC_CONST84_CHAR(cmd.c_str()))==TCL_OK) {
    mif_interp = Tcl_GetSlave(interp,Tcl_GetStringResult(interp));
  }
  istate.Restore();
  return mif_interp;
}

int Oxs_Director::GetLogFilename(String& logname) const
{ // Returns 0 on success, and the export "logname" is filled the
  // name of the log file, as currently registered with FileLogger.
  // On failure, the return value is 1, and logname is set to an
  // empty string.
  OC_BOOL err = 0;
  Oxs_TclInterpState istate(interp);
  if(Tcl_Eval(interp,OC_CONST84_CHAR("FileLogger GetFile"))!=TCL_OK) {
    err=1;
    logname = "";
  } else {
    logname = Tcl_GetStringResult(interp);
    if(logname.length()<1) err=1;
  }
  istate.Restore();
  return err;
}


int Oxs_Director::GetDriverInstanceName(String& name) const
{
  if (driver == NULL) return 1;
  name = driver->InstanceName();
  return 0;
}


int Oxs_Director::StoreExtObject(Oxs_OwnedPointer<Oxs_Ext>& obj)
{ // Stores *obj in the ext_obj array and registers it in ext_map.  On
  // entry, obj *must* have ownership of obj.  As a result of this call,
  // ownership is transferred to the director through the ext_obj array.
  // This is reflected upon return in obj.  In other words,
  // obj.IsOwner() must be true on entry, and if this function is
  // successful then obj.IsOwner() will be false on exit.  The ptr value
  // of obj will not be changed in any event.
  //    If appropriate, puts a pointer in energy_obj and sets the
  // "driver" member variable.
  //   This routine is used primarily from the Oxs_Director::ProbInit
  // member function during the processing of the input MIF file; i.e.,
  // all the Specify blocks create an Oxs_Ext object that is stored
  // through this function.  However, this interface is exposed for the
  // case where one Oxs_Ext object wants to create another Oxs_Ext
  // object itself, but make the new object both visible to the world
  // (via ext_map) and to pass off resposibility (ownership) to the
  // director.
  //   This routine returns 0 on success.  Otherwise ownership of obj
  // was *not* transfered to ext_obj, so the client remains responsible
  // for proper cleanup.  A return value of 1 means that obj did not
  // have ownership of the Oxs_Ext object.  A return value of 2
  // indicates that the failure was caused by a naming conflict in
  // ext_map, i.e., the instance name was already registered.

  if(!obj.IsOwner()) return 1;  // obj not owner

  String idname=obj->InstanceName();
  if(ext_map.find(idname)!=ext_map.end()) {
    // Name already in use.
    return 2;
  }

  Oxs_Ext* obj_ptr = obj.GetPtr();

  // Store mapping
  ext_map[idname]=obj_ptr;

  // Store in ordered list
  ext_obj.push_back(obj_ptr);
  obj.ReleaseOwnership();

  // If this is an Oxs_Energy object, then append it to
  // the energy_obj list as well.
  Oxs_Energy* energy_ptr
    = dynamic_cast<Oxs_Energy*>(obj_ptr);
  if(energy_ptr!=NULL)
    energy_obj.push_back(energy_ptr);

  // Set driver data member if this is the first driver object
  if(driver==NULL) {
    Oxs_Driver* driver_ptr
      = dynamic_cast<Oxs_Driver*>(obj_ptr);
    if(driver_ptr!=NULL) driver=driver_ptr;
  }

  return 0;
}


Oxs_Ext*
Oxs_Director::FindExtObject
(const String& id,
 OC_UINT4m& match_count) const
{ // Tries to find a match of id against entries in ext_map.
  // If "id" contains a ":" at any position other than the
  // first character, then it must match exactly against
  // the key.  If the first character of "id" is ":", then
  // it is compared against the back portion of each key.
  // If "id" does not include a ":" separator, then it is
  // compared first against the front portion of each key,
  // and failing that it is compared against the back portion
  // of each key.  An invalid key is returned if a unique
  // match is not obtained.

  match_count=0;
  Oxs_Ext* extobj=NULL;
  map<String,Oxs_Ext*>::const_iterator it;

  enum MATCH_TYPES { FRONT, BACK } match_type;
  if(id[0]==':')                      match_type = BACK;
  else if(id.find(':')==String::npos) match_type = FRONT;
  else {
    // Full exact match required.
    // Note: We use ext_map.find(id) rather than
    // ext_map[id] because the latter will insert a
    // dummy element into the map if 'id' is not in
    // the map.
    it = ext_map.find(id);
    if( it == ext_map.end() ) return NULL; // No match
    return it->second;
  }

  if(match_type==FRONT) {
    it = ext_map.begin();
    while(it!=ext_map.end()) {
      const String& test_key = it->first;
      int matched=0;
      // STL defines substr versions of string.compare(), but
      // they seem to be missing from some implementations.
      String front = test_key.substr(0,id.size());
      if(front.compare(id)==0) {
        matched=1;
      }
      if(matched) {
        if(match_count==0) extobj = it->second;
        ++match_count;
      }
      ++it;
    }
  }

  if(match_type==BACK || (match_type==FRONT && match_count==0) ) {
    it = ext_map.begin();
    while(it!=ext_map.end()) {
      const String& test_key = it->first;
      int matched=0;
      // STL defines substr versions of string.compare(), but
      // they seem to be missing from some implementations.
      String::size_type index=test_key.find(':');
      if(index!=String::npos) {
        if(match_type==FRONT) ++index; // Skip over ':'
        String back = test_key.substr(index);
        if(back.compare(id)==0) {
          matched=1;
        }
      }
      if(matched) {
        if(match_count==0) extobj = it->second;
        ++match_count;
      }
      ++it;
    }
  }

  if(match_count==1) return extobj;
  return NULL;
}

String Oxs_Director::ListExtObjects() const
{
  String result;
  vector<Oxs_Ext*>::const_iterator it = ext_obj.begin();
  if(it!=ext_obj.end()) {
    result.append((*it)->InstanceName());
    while((++it)!=ext_obj.end()) {
      result.append("\n");
      result.append((*it)->InstanceName());
    }
  }
  return result;
}

String Oxs_Director::ListEnergyObjects() const
{
  String result;
  vector<Oxs_Energy*>::const_iterator it = energy_obj.begin();
  if(it!=energy_obj.end()) {
    result.append((*it)->InstanceName());
    while((++it)!=energy_obj.end()) {
      result.append("\n");
      result.append((*it)->InstanceName());
    }
  }
  return result;
}

// OxsDirectorOutputKey defines output sort order
struct OxsDirectorOutputKey {
public:
  OC_INDEX index;
  OC_INT4m priority;
  OxsDirectorOutputKey() : index(0), priority(0) {}
  /// Default constructor required by STL containers.
  OxsDirectorOutputKey(size_t index_,OC_INT4m priority_)
    : index(index_), priority(priority_) {}
};

bool operator<(const OxsDirectorOutputKey& a,
               const OxsDirectorOutputKey& b)
{
  return a.priority<b.priority
    || (a.priority == b.priority && a.index<b.index);
}

// The following 2 functions are declared just to keep MSVC++ 5.0 happy
bool operator>(const OxsDirectorOutputKey&, const OxsDirectorOutputKey&);
bool operator==(const OxsDirectorOutputKey&, const OxsDirectorOutputKey&);


// Routine to build chunk_output_compute_obj list.  There does not
// appear to be any simple way to construct this list incrementally,
// because a compute routine may compute multiple outputs, and some of
// those outputs might not yet be registered.  For flexibility and
// robustness, this routine is called by the output registration and
// deregistration code directly, on each event.  This means that the
// chunk_output_compute_obj list is formed from scratch on each
// registration, even though it really only needs to be called once
// time, after all outputs are registered.  But the performance impact
// will be tiny, so we ignore the extra expense.
void Oxs_Director::CreateChunkOutputComputeList()
{
  std::vector<int> coverage(scalar_output_obj.size(),0);
  chunk_output_compute_obj.clear();

  std::vector<Oxs_BaseChunkScalarOutput*> buddy_list;

  std::list<Oxs_BaseScalarOutput*>::iterator bsit;
  for(bsit=scalar_output_obj.begin();
      bsit != scalar_output_obj.end(); ++bsit) {
    Oxs_BaseChunkScalarOutput* bcsptr
      = dynamic_cast<Oxs_BaseChunkScalarOutput*>(*bsit);
    if(bcsptr == 0) continue; // Non-chunk output

    buddy_list.clear();
    bcsptr->ChunkSharedScalarOutputs(buddy_list);
    std::vector<Oxs_BaseChunkScalarOutput*>::const_iterator buddies;
    for(buddies=buddy_list.begin();
        buddies != buddy_list.end(); ++buddies) {
      std::list<Oxs_BaseScalarOutput*>::iterator findit
        = std::find(scalar_output_obj.begin(),
                    scalar_output_obj.end(),*buddies);
      if(findit != scalar_output_obj.end()) {
        std::vector<int>::iterator covit
          = coverage.begin()
          + std::distance(scalar_output_obj.begin(),findit);
        if(*covit == 0) break;
      }
    }
    if(buddies != buddy_list.end()) {
      // This item added coverage; check to make sure there is no
      // duplicate coverage.
      for(buddies=buddy_list.begin();
          buddies != buddy_list.end(); ++buddies) {
        std::list<Oxs_BaseScalarOutput*>::iterator findit
          = std::find(scalar_output_obj.begin(),
                      scalar_output_obj.end(),*buddies);
        if(findit != scalar_output_obj.end()) {
          std::vector<int>::iterator covit
            = coverage.begin()
            + std::distance(scalar_output_obj.begin(),findit);
          if(*covit != 0) {
            String msg = String("Output object ")
              + bcsptr->LongName()
              + String(" duplicates evaluation of output ")
              + (*findit)->LongName();
            OXS_THROW(Oxs_ProgramLogicError,msg.c_str());
          }
          ++(*covit);
        }
      }
      chunk_output_compute_obj.push_back(bcsptr);
    }
  }
}


// Routines to register and deregister simulation outputs.
// NOTE: All objects registering output are expected to
// automatically deregister as a consequence of the object
// destructions occuring in Oxs_Director::Release().
void Oxs_Director::RegisterOutput(Oxs_Output* obj)
{
  // Check that object not already registered
  std::vector<Oxs_Output*>::iterator it
    = std::find(output_obj.begin(),output_obj.end(),obj);
  if(it != output_obj.end()) {
    // Object already registered
    String msg = String("Output object <")
      + obj->OwnerName() + String(":")
      + obj->OutputName() + String("> already registered.");
    OXS_THROW(Oxs_BadIndex,msg.c_str());
  }

#ifndef NDEBUG
  // Check for repeat names
  std::vector<Oxs_Output*>::const_iterator nit;
  for(nit=output_obj.begin();nit!=output_obj.end();++nit) {
    if( *nit == nullptr) continue;  // Deregistered object
    if(obj->OwnerName().compare((*nit)->OwnerName())==0
       && obj->OutputName().compare((*nit)->OutputName())==0) {
      String msg = String("Multiple registrations for output <")
        + obj->OwnerName() + String(":") + obj->OutputName()
        + String(">.");
      OXS_THROW(Oxs_BadCode,msg.c_str());
    }
  }
#endif // NDEBUG

  // Otherwise, append to end of output_obj list
  output_obj.push_back(obj);

  // If obj is a scalar output object, then insert it at the proper
  // (sorted) place in the scalar_output_obj list.
  Oxs_BaseScalarOutput* bsobj = dynamic_cast<Oxs_BaseScalarOutput*>(obj);
  if(bsobj) {
    OxsDirectorOutputKey bskey(scalar_output_obj.size(),bsobj->Priority());
    std::list<Oxs_BaseScalarOutput*>::iterator bsit;
    size_t soo_index;
    for(bsit=scalar_output_obj.begin(), soo_index=0;
        bsit != scalar_output_obj.end(); ++bsit, ++soo_index) {
      OxsDirectorOutputKey testkey(soo_index,(*bsit)->Priority());
      if(bskey < testkey) {
        break; // Found spot; bskey goes immediately before testkey
      }
    }
    scalar_output_obj.insert(bsit,bsobj);
    /// In worst case, bsit == scalar_output_obj.end()
  }
  CreateChunkOutputComputeList();
}

void Oxs_Director::DeregisterOutput(const Oxs_Output* obj)
{
  vector<Oxs_Output*>::iterator it
    = std::find(output_obj.begin(),output_obj.end(),obj);
  if(it == output_obj.end()) {
    OXS_THROW(Oxs_BadIndex,"Attempted deregistration of "
             "non-registered output object.");
  }
  *it = NULL;
  // If obj is a scalar output, then remove it from scalar_output_obj
  // list too.
  const Oxs_BaseScalarOutput* bsobj
    = dynamic_cast<const Oxs_BaseScalarOutput*>(obj);
  if(bsobj) {
    std::list<Oxs_BaseScalarOutput*>::iterator bsit
      = std::find(scalar_output_obj.begin(),scalar_output_obj.end(),bsobj);
    if(bsit == scalar_output_obj.end()) {
      OXS_THROW(Oxs_BadCode,"Deregistration of scalar output object failed.");
    }
    scalar_output_obj.erase(bsit);
  }
  CreateChunkOutputComputeList();
}

OC_BOOL Oxs_Director::IsRegisteredOutput(const Oxs_Output* obj) const
{
  vector<Oxs_Output*>::const_iterator it
    = std::find(output_obj.begin(),output_obj.end(),obj);
  if(it == output_obj.end()) return 0;
  return 1;
}

const char*
Oxs_Director::MakeOutputToken(OC_INDEX index,OC_UINT4m probid) const
{
  static Oc_AutoBuf buf;
  buf.SetLength(4*sizeof(unsigned long)+8);
  sprintf(static_cast<char *>(buf),"%" OC_INDEX_MOD "X %" OC_INT4m_MOD "X",
          static_cast<OC_UINDEX>(index),probid);
  return static_cast<char *>(buf);
}

Oxs_Output*
Oxs_Director::InterpretOutputToken(const char* token) const
{
  // Note: The following code assumes that index and probid are small
  // enough to fit into an unsigned long so that strtoul can handle
  // them.  Given the origin of token, this assumption should hold.
  OC_INDEX index=0;
  OC_UINT4m probid=0;
  char* endptr;
  index = static_cast<OC_INDEX>(strtoul(token,&endptr,16));
  if(endptr==NULL || endptr==token) {
    OXS_THROW(Oxs_BadParameter,
             "Invalid token passed to"
             " Oxs_Director::InterpretOutputToken");
  }
  if(*endptr != '\0') {
    probid = static_cast<OC_UINT4m>(strtoul(endptr+1,&endptr,16));
  }
  if(*endptr != '\0') {
    OXS_THROW(Oxs_BadParameter,
             "Invalid token passed to"
             " Oxs_Director::InterpretOutputToken");
  }
  if(probid!=problem_id) {
    OXS_THROW(Oxs_BadParameter,
             "Problem ID mismatch on token passed to"
             " Oxs_Director::InterpretOutputToken");
  }
  if(index >= static_cast<OC_INDEX>(output_obj.size())) {
    OXS_THROW(Oxs_BadIndex,
             "Out-of-range index in token passed to"
             " Oxs_Director::InterpretOutputToken");
  }
  Oxs_Output* obj = output_obj[index];
  if(obj==nullptr) {
    OXS_THROW(Oxs_BadParameter,
             "Token refers to deregistered output object in"
             " Oxs_Director::InterpretOutputToken");
  }
  return obj;
}

void Oxs_Director::ListOutputObjects(vector<String> &outputs) const
{
  // Create list of output objects sorted first by priority and
  // second by registration order.
  vector<OxsDirectorOutputKey> keylist;
  for(size_t i=0;i<output_obj.size();++i) {
    const Oxs_Output* obj = output_obj[i];
    if(obj!=nullptr) {
      keylist.push_back(OxsDirectorOutputKey(i,obj->Priority()));
    }
  }
  sort(keylist.begin(),keylist.end());

  // Fill export "outputs" with sorted list
  outputs.clear();
  vector<OxsDirectorOutputKey>::const_iterator it;
  for(it=keylist.begin();it!=keylist.end();++it) {
    outputs.push_back(MakeOutputToken(it->index,problem_id));
  }
}

Oxs_Output* Oxs_Director::FindOutputObject(const char* regexp) const
{ // C++ interface for searching the output object list.  This routine
  // matches the import regexp against the output objects list.  If a
  // unique match is found, then a pointer to that object is returned.
  // Otherwise, a NULL pointer is returned.  The match is performed
  // using the Tcl regexp engine.  Since this routine will typically
  // check through the entire list of output objects, ideally this
  // routine is called after problem initialization, and the result is
  // cached until the problem is released.

  // Get internal representation for regexp
  Tcl_Interp* mif_interp = GetMifInterp();
  if(mif_interp==NULL) {
    OXS_THROW(Oxs_BadPointer,"MIF interpreter not initialized in "
             "Oxs_Director::FindOutputObject()");
  }
  Tcl_RegExp rp = Tcl_RegExpCompile(mif_interp,OC_CONST84_CHAR(regexp));
  if(rp==NULL) {
    String msg = "Invalid regular expression in "
      "Oxs_Director::FindOutputObject(): ";
    msg += String(Tcl_GetStringResult(interp));
    OXS_THROW(Oxs_BadParameter,msg);
  }
  Oxs_Output* matched_obj=NULL;
  size_t size = output_obj.size();
  for(size_t i=0;i<size;i++) {
    Oxs_Output* obj = output_obj[i];
    if(obj==nullptr) continue; // Deregistered object
    String output_name = obj->LongName();
    Oc_AutoBuf ab = output_name.c_str();
    int match_result
      = Tcl_RegExpExec(mif_interp,rp,
                       OC_CONST84_CHAR(output_name.c_str()),
                       OC_CONST84_CHAR(output_name.c_str()));
    if(match_result==0) continue; // No match
    if(match_result<0) { // Error
      String msg = "Regular expression match error in "
        "Oxs_Director::FindOutputObject(): ";
      msg += String(Tcl_GetStringResult(mif_interp));
      // Extended error info
      const char* ei = Tcl_GetVar(mif_interp,OC_CONST84_CHAR("errorInfo"),
                                  TCL_GLOBAL_ONLY);
      const char* ec = Tcl_GetVar(mif_interp,OC_CONST84_CHAR("errorCode"),
                                  TCL_GLOBAL_ONLY);
      if(ei==NULL) ei = "";
      if(ec==NULL) ec = "";
      OXS_TCLTHROW(msg,String(ei),String(ec));
    }
    // Match found
    if(matched_obj!=NULL) { // More than one matched found
      return NULL;
    }
    matched_obj = obj; // First match against this regexp
  }
  return matched_obj;
}

Oxs_Output* Oxs_Director::FindOutputObjectExact(const char* outname) const
{ // Analogous to ::FindOutputObject, except this routine does a
  // standard strcmp instead of a regexp search.  Furthermore, this
  // routine assumes that registered output names are distinct,
  // so a NULL return indicates no match (as opposed to ::FindOutputObject,
  // where a NULL return possibly means more than one match).
  Oxs_Output* matched_obj=NULL;
  size_t size = output_obj.size();
  for(size_t i=0;i<size;i++) {
    Oxs_Output* obj = output_obj[i];
    if(obj==nullptr) continue; // Deregistered object
    String regname = obj->LongName();
    if(regname.compare(outname)==0) {
      matched_obj = obj;
      break;
    }
  }
  return matched_obj;
}

int Oxs_Director::Output(const char* output_token,
                         Tcl_Interp* use_interp,
                         int argc,CONST84 char** argv)
{
  if(driver==NULL) {
    Tcl_AppendResult(use_interp,"No driver specified\n",(char *)NULL);
    return TCL_ERROR;
  }
  const Oxs_SimState* state=driver->GetCurrentState();
  if(state==NULL) {
    Tcl_AppendResult(use_interp,"Current state not initialized\n",
                     (char *)NULL);
    return TCL_ERROR;
  }
  Oxs_Output* obj = InterpretOutputToken(output_token);

  int errcode = TCL_ERROR;
  String errmsg;
  try {
    errcode = obj->Output(state,use_interp,argc,argv);
  } catch (Oxs_ExtError& err) {
    String head =
      String("Error evaluating output \"")
      + obj->OutputName()
      + String("\" of object \"")
      + obj->OwnerName()
      + String("\" --- ");
    err.Prepend(head);
    throw;
  } catch (Oxs_Exception& err) {
    String head =
      String("Error evaluating output \"")
      + obj->OutputName()
      + String("\" of object \"")
      + obj->OwnerName()
      + String("\" --- ");
    err.Prepend(head);
    throw;
  } catch (Oc_Exception& err) {
    String head =
      String("Error evaluating output \"")
      + obj->OutputName()
      + String("\" of object \"")
      + obj->OwnerName()
      + String("\" --- ");
    err.PrependMessage(head.c_str());
    throw;
  } catch (EXCEPTION& err) {
    errmsg = String("Error evaluating output \"")
      + obj->OutputName()
      + String("\" of object \"")
      + obj->OwnerName()
      + String("\" --- ")
      + String(err.what());
    errcode = TCL_ERROR; // Safety
  } catch (...) {
    errmsg = String("Unrecognized error evaluating output \"")
      + obj->OutputName()
      + String("\" of object \"")
      + obj->OwnerName()
      + String("\".");
    errcode = TCL_ERROR; // Safety
  }

  if(errcode == TCL_ERROR && !errmsg.empty()) {
    Tcl_AppendResult(use_interp," --- ",errmsg.c_str(),(char *)NULL);
  }

  return errcode;
}

int
Oxs_Director::OutputCacheRequestIncrement(const char* output_token,
                                          OC_INT4m incr) const
{
  Oxs_Output* obj = InterpretOutputToken(output_token);
  if (obj->CacheRequestIncrement(incr)) return TCL_OK;
  return TCL_ERROR;
}

void Oxs_Director::OutputNames(const char* output_token,
                               vector<String>& names) const
{ // Fills export "names" with, in order, owner_name, output_name,
  // output_type, and output_units, for output object associated
  // with output_token.
  Oxs_Output* obj = InterpretOutputToken(output_token);
  names.push_back(obj->OwnerName());
  names.push_back(obj->OutputName());
  names.push_back(obj->OutputType());
  names.push_back(obj->OutputUnits());
}

class OxsDirectorChunkOutputThread : public Oxs_ThreadRunObj {
public:
  static Oxs_JobControl<OC_REAL8m> job_basket;

  const Oxs_SimState* state;
  std::vector<Oxs_BaseChunkScalarOutput*>& chunk_output_evaluators;

  OC_INDEX cache_blocksize;

  OxsDirectorChunkOutputThread
  (const Oxs_SimState* in_state,
   std::vector<Oxs_BaseChunkScalarOutput*>& in_chunk_output_evaluators,
   OC_INDEX in_cache_blocksize,
   int thread_count)
    : state(in_state), chunk_output_evaluators(in_chunk_output_evaluators),
      cache_blocksize(in_cache_blocksize) {
    job_basket.Init(thread_count,state->Ms->GetArrayBlock());
  }

  void Cmd(int threadnumber, void* data);

  // Note: Disable copy constructor and assignment operator by
  // declaring w/o implementation.
  OxsDirectorChunkOutputThread(const OxsDirectorChunkOutputThread&);
  OxsDirectorChunkOutputThread& operator=(const OxsDirectorChunkOutputThread&);
};

Oxs_JobControl<OC_REAL8m> OxsDirectorChunkOutputThread::job_basket;

void
OxsDirectorChunkOutputThread::Cmd
(int threadnumber,void* /* data */)
{
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

      // Send icache_start to icache_stop to output objects
      for(std::vector<Oxs_BaseChunkScalarOutput*>::iterator it
            = chunk_output_evaluators.begin();
          it != chunk_output_evaluators.end(); ++it) {
        (*it)->ChunkScalarOutput(state,icache_start,
                                 icache_stop,threadnumber);
      } // for-it
    } // for-icache_start
  } // while(1)
}


void Oxs_Director::GetAllScalarOutputs(vector<String>& triples)
{
  if(driver==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"No driver specified");
  }
  const Oxs_SimState* state=driver->GetCurrentState();
  if(state==NULL) {
    OXS_THROW(Oxs_IncompleteInitialization,"Current state not initialized");
  }
  const OC_UINT4m state_id = state->Id();

  // Run chunk output compute code, as needed
  std::vector<Oxs_BaseChunkScalarOutput*> bsco_as_needed;
  std::vector<Oxs_BaseChunkScalarOutput*> buddy_list;
  for(std::vector<Oxs_BaseChunkScalarOutput*>::iterator bscoit
        = chunk_output_compute_obj.begin();
      bscoit != chunk_output_compute_obj.end(); ++ bscoit) {
    buddy_list.clear();
    (*bscoit)->ChunkSharedScalarOutputs(buddy_list);
    for(std::vector<Oxs_BaseChunkScalarOutput*>::iterator buddy_it
          = buddy_list.begin(); buddy_it != buddy_list.end(); ++buddy_it) {
      if((*buddy_it)->cache.state_id != state_id) {
        bsco_as_needed.push_back(*bscoit); // This evaluator is needed
        break;
      }
    }
  }

  if(bsco_as_needed.size()>0) { // At least one chunk evaluator is needed
    const int number_of_threads = Oc_GetMaxThreadCount();
    // Initialize
    for(std::vector<Oxs_BaseChunkScalarOutput*>::iterator bscoit
          =bsco_as_needed.begin(); bscoit!=bsco_as_needed.end(); ++bscoit) {
      (*bscoit)->ChunkScalarOutputInitialize(state,number_of_threads);
    }
    // Compute in parallel
#if 1
    OC_INDEX cachechunk = Oc_CacheSize()
      /(4*(sizeof(ThreeVector)+sizeof(OC_REAL8m))); // 4 is fudge
    if(cachechunk<1) cachechunk = 1; // Safety
    OxsDirectorChunkOutputThread chunk_thread(state,bsco_as_needed,
                                              cachechunk,
                                              Oc_GetMaxThreadCount());
    static Oxs_ThreadTree threadtree;
    threadtree.LaunchTree(chunk_thread,0);
#else
    for(std::vector<Oxs_BaseChunkScalarOutput*>::iterator bscoit
          =bsco_as_needed.begin(); bscoit!=bsco_as_needed.end(); ++bscoit) {
      (*bscoit)->ChunkScalarOutput(state,0,state->spin.Size(),0);
    }
#endif
    // Finalize
    for(std::vector<Oxs_BaseChunkScalarOutput*>::iterator bscoit
          =bsco_as_needed.begin(); bscoit!=bsco_as_needed.end(); ++bscoit) {
      (*bscoit)->ChunkScalarOutputFinalize(state,number_of_threads);
    }
  }

  triples.clear();
  char numbuf[100];
  for(std::list<Oxs_BaseScalarOutput*>::iterator it
        = scalar_output_obj.begin(); it != scalar_output_obj.end(); ++it) {
    // Loop through all outputs.  Note that scalar_output_obj is a
    // sorted list.
    Oxs_BaseScalarOutput* obj = *it;
    try {
      vector<String> entry;
      entry.push_back(obj->LongName());
      entry.push_back(obj->OutputUnits());
      OC_REAL8m value = obj->GetValue(state);
      Oc_Snprintf(numbuf,sizeof(numbuf),
                  (obj->GetOutputFormat()).c_str(),
                  static_cast<double>(value));
      entry.push_back(String(numbuf));
      triples.push_back(Nb_MergeList(entry));
    } catch (Oxs_ExtError& err) {
      String head =
        String("Error evaluating output \"")
        + obj->OutputName()
        + String("\" of object \"")
        + obj->OwnerName()
        + String("\" --- ");
      err.Prepend(head);
      throw;
    } catch (Oxs_Exception& err) {
      String head =
        String("Error evaluating output \"")
        + obj->OutputName()
        + String("\" of object \"")
        + obj->OwnerName()
        + String("\" --- ");
      err.Prepend(head);
      throw;
    } catch (Oc_Exception& err) {
      String head =
        String("Error evaluating output \"")
        + obj->OutputName()
        + String("\" of object \"")
        + obj->OwnerName()
        + String("\" --- ");
      err.PrependMessage(head.c_str());
      throw;
    } catch (EXCEPTION& err) {
      String errmsg = String("Error evaluating output \"")
        + obj->OutputName()
        + String("\" of object \"")
        + obj->OwnerName()
        + String("\" --- ")
        + String(err.what());
      throw errmsg;
    } catch (...) {
      String errmsg = String("Unrecognized error evaluating output \"")
        + obj->OutputName()
        + String("\" of object \"")
        + obj->OwnerName()
        + String("\".");
      throw errmsg;
    }
  }
}

// Energy object index access
Oxs_Energy* Oxs_Director::GetEnergyObj(OC_UINT4m i) const {
  if(i >=  energy_obj.size()) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "Request for energy object (%u) past end of list (%u)",
                (unsigned int)i,(unsigned int)energy_obj.size());
    OXS_THROW(Oxs_BadIndex,buf);
  }
  return energy_obj[i];
}


// Collect stage requests from all registered Oxs_Ext objects.
void Oxs_Director::ExtObjStageRequestCounts
(unsigned int& min,
 unsigned int& max) const
{
  min=0;
  max=UINT_MAX;  // Defaults
  vector<Oxs_Ext*>::const_iterator it = ext_obj.begin();
  while(it != ext_obj.end()) {
    unsigned int tmin,tmax;
    (*it)->StageRequestCount(tmin,tmax);
    if(tmin>min) min=tmin;
    if(tmax<max) max=tmax;
    ++it;
  }
}


// SIMULATION STATE MANAGEMENT: During construction, each Oxs_Ext object
// that wants to create and/or hold simulation states should register
// its intent with the director using ReserveSimulationStateRequest().
// The states are actually constructed only as needed, but the count
// obtained via reserve calls is used as an upper bound to protect
// against memory leaks swallowing up all of system memory.  Objects
// obtain keys (handles) to new states with the GetNewSimulationState()
// call.  The key initially holds a write lock on the object.  The
// caller can change the lock state, make copies, etc., as desired.  The
// GetNewSimulationState() routine scans through the list of states held
// in the simulation_state array, looking for a pre-created state that
// has no locks on it.  If one is found, a key is returned for that
// state.  If all existing states in the simulation_state array are
// locked, then a new state is created (subject to the max state count
// obtained from ReserveSimulationStateRequest() calls).  In all cases
// the returned key holds a write lock on the state.  The states are
// actually deleted only inside the director Release() command, after
// destruction of all Oxs_Ext objects.
//   NOTE: The key passed in to GetNewSimulationState() is allowed to
// hold a lock on an Oxs_SimState, *IF* that state was obtained from a
// previous call to GetNewSimulationState().  On entry,
// GetNewSimulationState() releases any lock currently held by the
// import key, but then immediately tests if the corresponding
// Oxs_SimState is then free.  If so, then GetNewSimulationState()
// resets the key to the same object.  Therefore, if the chance is good
// that the input key holds the sole lock on an object, then the lock
// should *not* be released before calling GetNewSimulationState().

void Oxs_Director::ReserveSimulationStateRequest
(OC_UINT4m reserve_count)
{
  simulation_state_reserve_count+=reserve_count;
}

void Oxs_Director::GetNewSimulationState
(Oxs_Key<Oxs_SimState>& write_key)
{
  // Check first to see if import write_key is currently holding a lock.
  // If so, release it, and immediately check to see if that SimState
  // is available.
  const Oxs_SimState* csptr = write_key.GetPtr();
  if(csptr!=NULL) {
    write_key.Release();
    if(csptr->ReadLockCount()==0 && csptr->WriteLockCount()==0 ) {
      Oxs_SimState* sptr = const_cast<Oxs_SimState*>(csptr);
      sptr->Reset();
      write_key.Set(sptr);
      write_key.GetWriteReference();
      return;
    }
  }

  // Otherwise, go to the simulation_state list, and try to find
  // an unlocked state, working from the back forward.
  vector<Oxs_SimState*>::reverse_iterator it
    = simulation_state.rbegin();
  while(it!=simulation_state.rend()) {
    Oxs_SimState* sptr = *it;
    if(sptr->ReadLockCount()==0 && sptr->WriteLockCount()==0) {
      sptr->Reset();
      write_key.Set(sptr);
      write_key.GetWriteReference();
      return;
    }
    ++it;
  }

  // If we get to here, then all existing states have locks
  // on them.  So create a new state.
  if(simulation_state.size()>=simulation_state_reserve_count) {
    OXS_THROW(Oxs_BadResourceAlloc,
             "Simulation state reserve count exhausted.");
  }
  Oxs_SimState* newstate = new Oxs_SimState;
  simulation_state.push_back(newstate);
  write_key.Set(newstate);
  write_key.GetWriteReference();
}

// FindExistingSimulationState scans through the simulation state
// list and returns a read-only pointer to the one matching the
// import id.  The import id must by positive.  The return value
// is nullptr if no matching id is found.
const Oxs_SimState*
Oxs_Director::FindExistingSimulationState(OC_UINT4m id) const
{
  if(id==0) {
    OXS_THROW(Oxs_ProgramLogicError,
             "Find request for invalid simulation state id (0).");
  }
  vector<Oxs_SimState*>::const_iterator it = simulation_state.begin();
  while(it!=simulation_state.end()) {
    const Oxs_SimState* sptr = *it;
    if(sptr->Id() == id) return sptr;
    ++it;
  }
  return 0;
}

// FindBaseStep takes the id of an existing state and checks the
// step_done status for that state.  If the status is not DONE, then
// it traces backwards through the previous_state_id until it either
// finds a state with step_done == DONE, or else runs out of cached
// states.  On success returns a pointer to the DONE state, otherwise
// a null pointer.
const Oxs_SimState*
Oxs_Director::FindBaseStepState(OC_UINT4m id) const
{
  const Oxs_SimState* base = nullptr;
  while(id!=0 && (base=FindExistingSimulationState(id))!=nullptr) {
    if(base->step_done == Oxs_SimState::SimStateStatus::DONE) break;
    id = base->previous_state_id;
    base = nullptr;
  }
  return base;
}
// Variant of FindBaseStep that takes an Oxs_SimState pointer as import.
const Oxs_SimState*
Oxs_Director::FindBaseStepState(const Oxs_SimState* stateptr) const
{
  while(stateptr!=nullptr) {
    if(stateptr->step_done == Oxs_SimState::SimStateStatus::DONE) break;
    OC_UINT4m id = stateptr->previous_state_id;
    if(id==0) return nullptr;
    stateptr=FindExistingSimulationState(id);
  }
  return stateptr;
}


// #define DEVELOP_TEST to make DoDevelopTest active.  It may
// then be accessed from the Oxs solver console (use '-tk 1 -console'
// in Oxsii launch command) via the 'Oxs_DirectorDevelopTest count'
// command.
// #define DEVELOP_TEST

#ifndef DEVELOP_TEST
// Default NOP (inactive) code with error return
int Oxs_Director::DoDevelopTest
(const String& /* subroutine */,
 String& result_str,
 int /* argc */,CONST84 char** /* argv */)
{
  result_str = "DoDevelopTest not enabled.";
  return 1;
}
#else // DEVELOP_TEST
int OxsTestMeshvalue(String& result_str,
                     int argc,const char** argv)
{
  // Parameter check
  result_str = "Usage: meshvalue <array_size> <loop_count>";
  if(argc != 3) return 1;
  OC_INDEX array_size = strtol(argv[1],NULL,0);
  OC_INDEX loop_count = strtol(argv[2],NULL,0);
  if(array_size<1 || loop_count<1) return 1;

  Oxs_MeshValue<double> a;
  Oxs_MeshValue<double> b;
  Oxs_MeshValue<double> c;

  a.AdjustSize(array_size);
  b.AdjustSize(array_size);
  c.AdjustSize(array_size);

  a = 3.1;
  b = 0.99999999;
  c = 1.234567;

  OC_REAL8m val = 0.99999999;

  Nb_WallWatch timer;
  timer.Start();
  for(OC_INDEX loop=0;loop<loop_count;++loop) {
    a = b;
    a = c;
  }
  timer.Stop();
  double iterate_time = timer.GetTime()/double(loop_count);

  char buf[1024];
  Oc_Snprintf(buf,sizeof(buf),
              "Time per iteration: %12.3f microseconds (value: %g)\n",
              iterate_time*1e6,a[OC_INDEX(0)]);

  result_str=String(buf);
  return 0;
}

// Working code; should return 0 on success, >0 on error
int Oxs_Director::DoDevelopTest
(const String& subroutine,
 String& result_str,
 int argc,const char** argv)
{
  int rtncode = -1;
  if(subroutine.compare("meshvalue")==0) {
    rtncode = OxsTestMeshvalue(result_str,argc,argv);
  } else {
    result_str = String("Unrecognized subroutine request: \"")
      + subroutine + String("\"");
    result_str += String("\n Supported routines:");
    result_str += String("\n\tmeshvalue");
    rtncode=1;
  }
  return rtncode;
}
#endif // DEVELOP_TEST

#ifdef DEVELOP_TEST
#undef DEVELOP_TEST
#endif // DEVELOP_TEST
