/* FILE: oxscmd.cc                 -*-Mode: c++-*-
 *
 * Tcl interface to C/C++ code in oxs.
 *
 * Each routine in the file is registered with the Tcl interpreter
 * by OxsRegisterInterfaceCommands() at the bottom of this file.
 * OxsRegisterInterfaceCommands() is called by Oxs_Init().
 * (Note: OxsRegisterInterfaceCommands() is not in any header file
 * because it should only be called by Oxs_Init().)
 *
 */

#include <cassert>

#include <string>
#include <vector>

#include "atlas.h"
#include "director.h"
#include "driver.h"  // For Oxs_Driver::RunEvent structure
#include "ext.h"
#include "oc.h"
#include "outputderiv.h"
#include "oxsthread.h"
#include "oxswarn.h"
#include "scalarfield.h"
#include "util.h"
#include "vectorfield.h"

#include "energy.h"	// Needed to make MSVC++ 5 happy

OC_USE_STRING;
OC_USE_EXCEPTION;
OC_USE_BAD_ALLOC;

/* End includes */

/*
 *----------------------------------------------------------------------
 *
 * Interface commands.  These are accessed through the general purpose
 * OxsCmdsSwitchboard command.  The return value should be a string that
 * will be appended to *interp's result.  All errors should be handled
 * by throwing exceptions.  Normal return (i.e., not via an exception)
 * is interpreted as meaning sucess, and a TCL_OK return is passed
 * back to *interp.
 *  For historical reasons, some commands place their output into
 * *interp's result by themselves.  In this case the interface command
 * should return an empty string.
 *
 *---------------------------------------------------------------------- */
typedef String Oxs_CmdProc(Oxs_Director* director,Tcl_Interp *interp,
			   int argc,CONST84 char** argv);
Oxs_CmdProc Oxs_SetRestartFlag;
Oxs_CmdProc Oxs_SetRestartCrcCheck;
Oxs_CmdProc Oxs_SetRestartFileDir;
Oxs_CmdProc Oxs_GetRestartFileDir;
Oxs_CmdProc Oxs_GetMifCrc;
Oxs_CmdProc Oxs_GetMifParameters;
Oxs_CmdProc Oxs_GetStage;
Oxs_CmdProc Oxs_GetRunStateCounts;
Oxs_CmdProc Oxs_GetCurrentStateId;
Oxs_CmdProc Oxs_SetStage;
Oxs_CmdProc Oxs_IsProblemLoaded;
Oxs_CmdProc Oxs_IsRunDone;
Oxs_CmdProc Oxs_GetMif;
Oxs_CmdProc Oxs_ProbInit;
Oxs_CmdProc Oxs_ProbReset;
Oxs_CmdProc Oxs_Run;
Oxs_CmdProc Oxs_DriverName;
Oxs_CmdProc Oxs_MeshGeometryString;
Oxs_CmdProc Oxs_MifOption;
Oxs_CmdProc Oxs_ListRegisteredExtClasses;
Oxs_CmdProc Oxs_ListExtObjects;
Oxs_CmdProc Oxs_ListEnergyObjects;
Oxs_CmdProc Oxs_ListOutputObjects;
Oxs_CmdProc Oxs_OutputGet;
Oxs_CmdProc Oxs_GetAllScalarOutputs;
Oxs_CmdProc Oxs_OutputNames;
Oxs_CmdProc Oxs_QueryState;
Oxs_CmdProc Oxs_EvalScalarField;
Oxs_CmdProc Oxs_EvalVectorField;
Oxs_CmdProc Oxs_GetAtlasRegions;
Oxs_CmdProc Oxs_GetAtlasRegionByPosition;
Oxs_CmdProc Oxs_GetThreadStatus; // For debugging
Oxs_CmdProc Oxs_ExtCreateAndRegister;
Oxs_CmdProc Oxs_GetCheckpointFilename;
Oxs_CmdProc Oxs_GetCheckpointDisposal;
Oxs_CmdProc Oxs_SetCheckpointDisposal;
Oxs_CmdProc Oxs_GetCheckpointInterval;
Oxs_CmdProc Oxs_SetCheckpointInterval;
Oxs_CmdProc Oxs_GetCheckpointAge;
Oxs_CmdProc Oxs_DirectorDevelopTest;
Oxs_CmdProc Oxs_DriverLoadTestSetup;

Tcl_CmdProc Oxs_ProbRelease;
Tcl_CmdProc OxsCmdsSwitchboard;

/*
 *----------------------------------------------------------------------
 *
 * Client data struct used by Tcl_CmdProc's in oxscmds.cc
 *
 *----------------------------------------------------------------------
 */
struct OxsCmdsClientData {
public:
  Oxs_Director* director;
  Oxs_CmdProc* cmd;
  String function_name; // Used to provide context information to
  /// user when generating error messages.
  OxsCmdsClientData(Oxs_Director* d,Oxs_CmdProc* c,const String& n)
    : director(d), cmd(c), function_name(n) {}
};

/*
 *----------------------------------------------------------------------
 *
 * OxsCmdsTclException --
 *   Special exception class used by Oxs_CmdProc routines.  It indicates
 *   that the error message and other Tcl error handling info has already
 *   been set into the Tcl interpreter.  The handling routine (i.e.,
 *   OxsCmdsSwitchboard) that catches this exception should simply return
 *   control to Tcl with the specified error code (usually TCL_ERROR).
 *   This is used, for example, by header code in each Oxs_CmdProc
 *   that validates the argument count.
 *
 *----------------------------------------------------------------------
 */

class OxsCmdsProcTclException {
private:
  // operator= not possible because of const data member.
  OxsCmdsProcTclException& operator=(const OxsCmdsProcTclException&);
public:
  const int error_code;
  OxsCmdsProcTclException(int code) : error_code(code) {}
};

/*
 *----------------------------------------------------------------------
 *
 * General command cleanup routine.  Deletes clientData structure.
 *  Note: This routine is called when a command is deleted from
 *        the interpreter.  It is not called when the interpreter
 *        is destroyed as part of Oxs shutdown.  So, in practice,
 *        this routine is never called.
 *
 *----------------------------------------------------------------------
 */
Tcl_CmdDeleteProc OxsCmdsCleanup;
void OxsCmdsCleanup(ClientData clientData)
{
  OxsCmdsClientData* cd = static_cast<OxsCmdsClientData*>(clientData);
  delete cd;
}

/*
 *----------------------------------------------------------------------
 *
 * OxsCmdsMakeEventString --
 *      Internal helper routine for processing OxsRunEvent vectors.
 *
 * Results:
 *      String containing list of events.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String OxsCmdsMakeEventString(const vector<OxsRunEvent>& events)
{
  String whole_list;
  if(!events.empty()) {
    // Build event list
    vector<String> event_list;
    vector<OxsRunEvent>::const_iterator it;
    for(it=events.begin();it!=events.end();++it) {
      const Oxs_SimState* state = it->state_key.GetPtr();
      if(state==NULL || state->Id()==0) {
	OXS_THROW(Oxs_ProgramLogicError,"Invalid state returned");
      }
      static char event_buf[512];
      switch(it->event) {
      case OXS_STEP_EVENT:
	Oc_Snprintf(event_buf,sizeof(event_buf),
		    "STEP %u %u %u",
		    state->Id(),
		    state->stage_number,
		    state->iteration_count);
	break;
      case OXS_STAGE_DONE_EVENT:
	Oc_Snprintf(event_buf,sizeof(event_buf),
		    "STAGE_DONE %u %u %u",
		    state->Id(),
		    state->stage_number,
		    state->iteration_count);
	break;
      case OXS_RUN_DONE_EVENT:
	Oc_Snprintf(event_buf,sizeof(event_buf),
		    "RUN_DONE %u %u %u",
		    state->Id(),
		    state->stage_number,
		    state->iteration_count);
	break;
      case OXS_CHECKPOINT_EVENT:
	Oc_Snprintf(event_buf,sizeof(event_buf),
		    "CHECKPOINT %u %u %u",
		    state->Id(),
		    state->stage_number,
		    state->iteration_count);
	break;
      case OXS_INVALID_EVENT:
        OXS_THROW(Oxs_ProgramLogicError,"Invalid event detected");
        break;
      default:
        {
          char buf[1024];
          Oc_Snprintf(buf,sizeof(buf),
                      "Unknown event type returned: %d",
                      static_cast<int>(it->event));
          OXS_THROW(Oxs_ProgramLogicError,buf);
        }
      }
      event_list.push_back(String(event_buf));
    }
    whole_list = Nb_MergeList(event_list);
  }

  return whole_list;
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_SetErrorCode --
 *      Set the error code of a Tcl interp.
 *      (Might move this to package Oc if others can use it)
 *
 *----------------------------------------------------------------------
 */

void Oxs_SetErrorCode(Tcl_Interp *interp, const char* value)
{
#if TCL_MAJOR_VERSION >= 8
  Tcl_SetObjErrorCode(interp, Tcl_NewStringObj(Oc_AutoBuf(value), -1));
#else
  // The Tcl_SetErrorCode (string interface) routine appears in the Tcl
  // 7.5 tcl.h header file, but (unlike Tcl_SetObjErrorCode) there
  // doesn't appear to be any way to set errorCode to a
  // unknown-at-compile-time length list using it.  This code is for
  // backwards compatibility only, so just set the variable directly and
  // don't worry about it...
  if(value==NULL || value[0]=='\0') {
    Tcl_SetVar(interp,OC_CONST84_CHAR("errorCode"),
               OC_CONST84_CHAR("NONE"),
               TCL_GLOBAL_ONLY);
  } else {
    Tcl_SetVar(interp,OC_CONST84_CHAR("errorCode"),
               OC_CONST84_CHAR(value),
               TCL_GLOBAL_ONLY);
  }
#endif

}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_AddErrorInfo --
 *      Append to the error info of a Tcl interp.
 *      Note: There is a small difference in behavior between the
 *      pre-Tcl 8.0 version of this code, in that Tcl_AddErrorInfo
 *      initializes the errorInfo from the the current interpreter's
 *      result.  This difference shouldn't cause any real problems,
 *      especially since some parts of the OOMMF code base at this time
 *      (May 2008) already assume Tcl 8.0 or later.
 *
 *----------------------------------------------------------------------
 */

void Oxs_AddErrorInfo(Tcl_Interp *interp, const char* msg)
{
#if TCL_MAJOR_VERSION < 8
  Tcl_AddErrorInfo(interp, Oc_AutoBuf(msg));
#else
  Tcl_AddObjErrorInfo(interp, Oc_AutoBuf(msg), -1);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * General purpose switchboard command.  This routine interfaces
 * with the main Tcl interpreter, routing calls based on clientData.
 * This allows error handling to be managed at a single point.
 *
 *----------------------------------------------------------------------
 */
int OxsCmdsSwitchboard(ClientData cd,Tcl_Interp *interp,
		       int argc,CONST84 char** argv)
{
  static String result_message;
  static String error_message,error_info,error_code;
  int result_code = TCL_ERROR;
  int fatal_error = 1;
  OxsCmdsClientData* ocd = static_cast<OxsCmdsClientData*>(cd);
  try {
    // Check for warnings and errors from background threads
    Oxs_WarningMessage::TransmitMessageHold();
    String bkgerr;
    if(Oxs_ThreadError::CheckAndClearError(&bkgerr)) {
      ocd->director->SetErrorStatus(1);
      OXS_THROW(Oxs_BadThread,bkgerr);
    }
    // Run command
    result_message.erase();
    result_message = ocd->cmd(ocd->director,interp,argc,argv);
    result_code = TCL_OK;
    fatal_error = 0;
  } catch (OxsCmdsProcTclException& err) {
    // Append an empty string to errorInfo.  This is a nop
    // if errorInfo is currently set, but otherwise will
    // initialize errorInfo.  For Tcl version <8.0, initializing
    // errorInfo will fill it with the current interp result;
    // this is probably not worth worrying about.
    Oxs_AddErrorInfo(interp,"");
    const char* ei = Tcl_GetVar(interp,OC_CONST84_CHAR("errorInfo"),
                                TCL_GLOBAL_ONLY);
    if(ei!=NULL) {
      error_info = String(ei);
    } else {
      error_info.erase();
    }
    const char* ec = Tcl_GetVar(interp,OC_CONST84_CHAR("errorCode"),
                                TCL_GLOBAL_ONLY);
    if(ec!=NULL) {
      error_code = String(ec);
    } else {
      error_code.erase();
    }
    // In the case of this exception, the error message has been
    // set into *interp's result.  Pull it out and clear the result,
    // both for consistency with the other catch blocks, but also
    // because the Tcl_AddErrorInfo will insert the interp result
    // into the error info.
    // NOTE: Tcl_ResetResult also marks errorInfo and errorCode as
    //       not set.
    error_message = Tcl_GetStringResult(interp);
    Tcl_ResetResult(interp);
    result_code = err.error_code;
    fatal_error = 0; // Assume that this is either not fatal or will
    /// be handled at Tcl code level.
  } catch (Oxs_ExtError& err) {
    error_message =
      String("Error thrown from inside \"")
      + ocd->function_name
      + String("\" --- ")
      + String(err);
    error_info.erase();
    error_code.erase();
    result_code = TCL_ERROR; // Safety
  } catch (Oc_TclError& err) {
    Oc_AutoBuf msgbuf;
    error_message =
      String("Error thrown from inside \"")
      + ocd->function_name
      + String("\" --- ")
      + String(err.MessageType())
      + String(" --- ")
      + String(err.ConstructMessage(msgbuf));
    error_info = String(err.ErrorInfo());
    error_code = String(err.ErrorCode());
    result_code = TCL_ERROR; // Safety
    fatal_error = 0; // Assume that this is either not fatal or will
    /// be handled at Tcl code level.
  } catch (Oxs_Exception& err) {
    error_message =
      String("Error thrown from inside \"")
      + ocd->function_name
      + String("\" --- ")
      + String(err.MessageType())
      + String(" --- ")
      + err.MessageText();

    error_info.erase();  // Safety, to protect against Oxs_Ext
    /// objects with poorly written Tcl interp calls.

    // Error code should be:
    //   OOMMF msg_src msg_id maxcount
    // Use Tcl_Merge to make this a proper list
    vector<String> ecvec;
    ecvec.push_back("OOMMF");
    ecvec.push_back(err.MessageSrc());
    ecvec.push_back(err.FullType());
    ecvec.push_back(err.DisplayCountAsString());
    error_code = Nb_MergeList(ecvec);

    result_code = TCL_ERROR; // Safety
    fatal_error = 0; // Assume that this is either not fatal or will
    /// be handled at Tcl code level.
  } catch (Oc_Exception& err) {
    Oc_AutoBuf msg;
    err.ConstructMessage(msg);
    error_message = msg.GetStr();
    error_info.erase();
    error_code.erase();
    result_code = TCL_ERROR; // Safety
    fatal_error = 0; // Assume that this is either not fatal or will
    /// be handled at Tcl code level.
  } catch (const String& err) {
    error_message
      = String("Error thrown from inside"
               " unspecified OOMMF library module --- ") + err;
    error_info.erase();
    error_code.erase();
    result_code = TCL_ERROR; // Safety
    // Assume fatal
  } catch (const Oc_AutoBuf& err) {
    error_message =
      String("Error thrown from inside"
             " unspecified OOMMF library module --- ")
      + String(err.GetStr());
    error_info.erase();
    error_code.erase();
    result_code = TCL_ERROR; // Safety
    // Assume fatal
  } catch (const char* err) {
    // Some routines in the pkg area throw bare char*'s, Strings, or
    // Oc_AutoBufs.  These should be changed to Oc_Exceptions.
    error_message =
      String("Error thrown from inside"
             " unspecified OOMMF library module --- ")
      + String(err);
    error_info.erase();
    error_code.erase();
    result_code = TCL_ERROR; // Safety
    // Assume fatal
  } catch (BAD_ALLOC&) {
    error_message =
      String("Library error thrown from inside \"")
      + ocd->function_name
      + String("\" --- ")
      + String("Insufficient memory");
    error_info.erase();
    error_code.erase();
    result_code = TCL_ERROR; // Safety
    // Assume fatal
  } catch (EXCEPTION& err) {
    error_message =
      String("Library error thrown from inside \"")
      + ocd->function_name
      + String("\" --- ")
      + String(err.what());
    error_info.erase();
    error_code.erase();
    result_code = TCL_ERROR; // Safety
    // Assume fatal
  } catch (...) {
    error_message =
      String("Unrecognized error thrown from inside \"")
      + ocd->function_name + String("\".");
    error_info.erase();
    error_code.erase();
    result_code = TCL_ERROR; // Safety
    // Assume fatal
  }

  if(!fatal_error) {
    // Note: Some commands may have already placed their output
    // into interp's result.  This is typically signified by
    // an empty result_message, although some commands just
    // have no output.
    if(result_code == TCL_OK) {
      if(result_message.length()>0) {
        Tcl_AppendResult(interp,result_message.c_str(),(char *)NULL);
      }
    } else {
        if(!error_info.empty()) {
          Oxs_AddErrorInfo(interp, error_info.c_str());
        }
        if(!error_code.empty()) {
          Oxs_SetErrorCode(interp, error_code.c_str());
        }
        Tcl_AppendResult(interp,error_message.c_str(),(char *)NULL);
    }
  } else { // ERROR
    // Reentrancy protection
    String tmp_error_info = error_info;       error_info.erase();
    String tmp_error_code = error_code;       error_code.erase();
    String tmp_error_message = error_message; error_message.erase();
    Oxs_TclInterpState tcl_state(interp);
    try {
      ocd->director->SetErrorStatus(1);
      Tcl_Eval(interp,OC_CONST84_CHAR("ReleaseProblem"));
    } catch (...) {}
    tcl_state.Restore();
    if(!tmp_error_info.empty()) {
      Oxs_AddErrorInfo(interp,tmp_error_info.c_str());
    }
    if(!tmp_error_code.empty()) {
      Oxs_SetErrorCode(interp, tmp_error_code.c_str());
      /// Use Obj interface, if available.
    }
    if(!tmp_error_message.empty()) {
      const char* interp_result = Tcl_GetStringResult(interp);
      size_t reslen = strlen(interp_result);
      char spacer[2];
      spacer[0] = '\0';
      if(reslen>0 && interp_result[reslen-1]!='\n') {
        spacer[0] = '\n';
        spacer[1] = '\0';
      }
      Tcl_AppendResult(interp,spacer,tmp_error_message.c_str(),NULL);
    }
  }
  return result_code;
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_SetRestartFlag --
 *      Interactively set the restart flag.
 *
 * Results:
 *      Returns the previous value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_SetRestartFlag(Oxs_Director* director,Tcl_Interp *interp,
			  int argc,CONST84 char** argv)
{
  if (argc != 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " restart_flag\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  int newflag;
  if (Tcl_GetInt(interp, argv[1], &newflag) != TCL_OK) {
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  int oldflag = director->SetRestartFlag(newflag);
  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%d",oldflag);
  return String(buf);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_SetRestartCrcCheck --
 *      Interactively set the restart crc check flag.
 *
 * Results:
 *      Returns the previous value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_SetRestartCrcCheck(Oxs_Director* director,Tcl_Interp *interp,
			   int argc,CONST84 char** argv)
{
  if (argc != 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " check_flag\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  int newflag;
  if (Tcl_GetInt(interp, argv[1], &newflag) != TCL_OK) {
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  int oldflag = static_cast<int>(director->CheckRestartCrc());
  director->SetRestartCrcCheck(static_cast<OC_BOOL>(newflag));

  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%d",oldflag);
  return String(buf);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_SetRestartFileDir --
 *      Interactively set the default directory for storing restart files.
 *
 * Results:
 *      Returns directory name.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_SetRestartFileDir(Oxs_Director* director,Tcl_Interp *interp,
                             int argc,CONST84 char** argv)
{
  if (argc != 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " restart_directory\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  director->SetRestartFileDir(argv[1]);
  return String(argv[1]);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetRestartFileDir --
 *      Returns default directory for restart file storage
 *
 * Results:
 *      Returns directory name.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetRestartFileDir(Oxs_Director* director,Tcl_Interp *interp,
                             int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     "\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  return String(director->GetRestartFileDir());
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetMifCrc --
 *      Returns the CRC of the buffer representation of the most recent
 *      file read by the current Oxs_Mif object.  It is the value used
 *      by the most recent problem load event (i.e., the Oxs_Mif ReadMif
 *      command).  Returns an error if no problem is loaded.
 *
 * Results:
 *      CRC value.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------- */
String Oxs_GetMifCrc(Oxs_Director* director,Tcl_Interp *interp,
		     int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  OC_UINT4m crc;
  crc = director->GetMifCrc();
  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%lu",
	      static_cast<unsigned long>(crc));
  return String(buf);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetMifParameters --
 *      Returns the MIF parameter string held in the current Oxs_Mif
 *      object.  It is the value used by the most recent problem
 *      load event (i.e., the Oxs_Mif ReadMif command).  Returns
 *      an error if no problem is loaded.
 *
 * Results:
 *      MIF parameter string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetMifParameters(Oxs_Director* director,Tcl_Interp *interp,
			    int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  return director->GetMifParameters();
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetStage --
 *      Interactively get the Stage of the problem being solved.
 *
 * Results:
 *      Current stage.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_GetStage(Oxs_Director* director,Tcl_Interp *interp,
		    int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     "\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  int stage = director->GetStage();
  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%d",stage);
  return String(buf);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetRunStateCounts --
 *      Returns a list consisting of the current iteration, stage, and
 *   number of stages.
 *
 * Results:
 *      String form of the list.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_GetRunStateCounts(Oxs_Director* director,Tcl_Interp *interp,
			     int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     "\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  int iteration = director->GetIteration();
  int stage = director->GetStage();
  int stage_count = director->GetNumberOfStages();

  char buf[1024];
  Oc_Snprintf(buf,sizeof(buf),"%d %d %d",iteration,stage,stage_count);
  return String(buf);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetCurrentStateId --
 *      Query directory for identifier for current state
 *
 * Results:
 *      String form of the current state identifier
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_GetCurrentStateId(Oxs_Director* director,Tcl_Interp *interp,
			     int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     "\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  OC_UINT4m id = director->GetCurrentStateId();

  char buf[1024];
  Oc_Snprintf(buf,sizeof(buf),"%" OC_INT4m_MOD "u",id);
  return String(buf);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_SetStage --
 *      Interactively set the Stage of the problem being solved.
 *
 * Results:
 *      A string containing a two item list.  The first item is the
 *      actual stage.  The second item is a list of events (if any).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_SetStage(Oxs_Director* director,Tcl_Interp *interp,
		    int argc,CONST84 char** argv)
{
  if (argc != 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " stage_number\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  int requestedStage;
  if (Tcl_GetInt(interp, argv[1], &requestedStage) != TCL_OK) {
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  vector<OxsRunEvent> events; // Note: Each OxsRunEvent includes
  /// a key tied to a simulation state.  That key will hold a read
  /// lock on the state until the OxsRunEvent is destroyed.  The
  /// easiest way to insure proper destruction, that works even
  /// across exceptions, is for results to be an automatic (local)
  /// variable.

  int stage = director->SetStage(requestedStage,events);

  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%d",stage);

  vector<String> results;
  results.push_back(String(buf));
  results.push_back(OxsCmdsMakeEventString(events));

  return Nb_MergeList(results);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_IsProblemLoaded --
 *      Returns 0 if no problem is loaded.
 *
 * Results:
 *      Problem status as a string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_IsProblemLoaded(Oxs_Director* director,Tcl_Interp *interp,
                           int argc, CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     "\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  String result = String("0");
  if(director->IsProblemLoaded()) {
    result = String("1");
  }
  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_IsRunDone --
 *      Returns 1 if current state is done, 0 if not done,
 *      and -1 if done status is unknown.
 *
 * Results:
 *      Problem status as a string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_IsRunDone(Oxs_Director* director,Tcl_Interp *interp,
                     int argc, CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     "\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  int doneness = director->IsRunDone();
  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%d",doneness);

  return String(buf);
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetMif --
 *      Interactively get the MIF handle of the problem being solved.
 *
 * Results:
 *      MIF handle as a string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_GetMif(Oxs_Director* director,Tcl_Interp *interp,
		  int argc, CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     "\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  return director->GetMifHandle();
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_ProbInit --
 *      Micromagnetic problem initialization
 *
 * Results:
 *      None (an empty string is returned)
 *
 * Side effects:
 *      Creates a slave interpreter
 *
 *----------------------------------------------------------------------
 */

String Oxs_ProbInit(Oxs_Director* director,Tcl_Interp *interp,
		    int argc,CONST84 char** argv)
{
  if (argc != 3) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0]," filename mif_parameters\"",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  int result = director->ProbInit(argv[1],argv[2]);
  if(result!=TCL_OK) {
    throw OxsCmdsProcTclException(result);
  }

  return String("");
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_ProbReset --
 *      Resets micromagnetic problem
 *
 * Results:
 *      None (an empty string is returned)
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_ProbReset(Oxs_Director* director,Tcl_Interp *interp,
		     int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  Oxs_ThreadError::ClearError();

  int restart_flag = director->SetRestartFlag(0); // Disable restart
  /// flag during reset operation
  try {
    int result = director->ProbReset();
    director->SetRestartFlag(restart_flag);
    if(result!=TCL_OK) {
      throw OxsCmdsProcTclException(result);
    }
  } catch (...) {
    director->SetRestartFlag(restart_flag);
    throw;
  }

  return String("");
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_Run --
 *      Runs micromagnetic problem.
 *
 * Results:
 *      String containing list of events.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_Run(Oxs_Director* director,Tcl_Interp *interp,
	    int argc,CONST84 char** argv)
{ // On success, returns a list of events with associated state id
  // info.  Each item in the list has the form
  //      <event_type state_id stage_number iteration_count>
  // In the future, we might want to change this to return instead
  // some general state reference that the script can query for
  // itself to pull out the info that it needs.
  // NOTE: Each element of the "results" vector holds a read lock
  //  on the associated state, until such time as "results" is
  //  destroyed.
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # of args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  vector<OxsRunEvent> results; // Note: Each OxsRunEvent includes
  /// a key tied to a simulation state.  That key will hold a read
  /// lock on the state until the OxsRunEvent is destroyed.  The
  /// easiest way to insure proper destruction, that works even
  /// across exceptions, is for results to be an automatic (local)
  /// variable.
#if REPORT_TIME
  static Nb_StopWatch cruntime;
#endif
  try {
#if REPORT_TIME
    cruntime.Start();
#endif
    director->ProbRun(results);
#if REPORT_TIME
    cruntime.Stop();
#endif
  } catch (Oxs_ExtError& err) {
    String driver_name;
    if(director->GetDriverInstanceName(driver_name)!=0) {
      OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
    }
    String head =
      String("Oxs_Ext error inside Run() function of driver ")
      + driver_name + String(" --- ");
    err.Prepend(head);
    throw;
  } catch (Oxs_Exception& err) {
    String driver_name;
    if(director->GetDriverInstanceName(driver_name)!=0) {
      OXS_THROW(Oxs_IncompleteInitialization,"no driver identified");
    }
    String head =
      String("Error inside Run() function of driver ")
	+ driver_name
	+ String(" --- ");
    err.Prepend(head);
    throw;
  }

  Tcl_ResetResult(interp); // Safety: Wipe away any accidental
  /// scribblings in the result area.


#if REPORT_TIME
  for(vector<OxsRunEvent>::const_iterator cit = results.begin();
      cit != results.end(); ++cit) {
    if(cit->event != OXS_RUN_DONE_EVENT) continue;
    // Otherwise, RUN_DONE event occurred; report
    Oc_TimeVal cpu,wall;
    cruntime.GetTimes(cpu,wall);
    if(double(wall)>0.0) {
      fprintf(stderr,"C++-level run time (secs)%7.2f cpu /%7.2f wall\n",
              double(cpu),double(wall));
    }
    cruntime.Reset();
    break;
  }
#endif // REPORT_TIME

  return OxsCmdsMakeEventString(results);
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_DriverName --
 *      Returns instance name of the driver.
 *
 * Results:
 *      Driver name as a string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_DriverName(Oxs_Director* director,Tcl_Interp *interp,
                      int argc,CONST84 char** argv)
{

  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  String name;
  if(director->GetDriverInstanceName(name)!=0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  return name;
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_MeshGeometryString --
 *      Returns string describing mesh geometry.
 *
 * Results:
 *      Mesh geometry as a string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_MeshGeometryString(Oxs_Director* director,Tcl_Interp *interp,
                              int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  const Oxs_Driver* driver = director->GetDriver();
  if(driver == 0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  String result = driver->GetCurrentState()->mesh->GetGeometryString();

  return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_MifOption --
 *      Read access to MIF options array.
 *
 * Results:
 *      Option value as a string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_MifOption(Oxs_Director* director,Tcl_Interp *interp,
                     int argc,CONST84 char** argv)
{

  if(argc!=2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
                     argv[0]," <label>\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  String value;
  if(!director->GetMifOption(argv[1],value)) {
    Tcl_AppendResult(interp, "Option \"", argv[1],
                     "\" not set in MIF file.",
                     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  return value;
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_ListRegisteredExtClasses --
 *      Lists Ext child classes that are loaded and registered
 *
 * Results:
 *      String consisting of Ext child class list.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_ListRegisteredExtClasses(Oxs_Director*,Tcl_Interp *interp,
				    int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  return Oxs_Ext::ListRegisteredChildClasses();
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_ListExtObjects --
 *      Lists Ext objects that have been created
 *
 * Results:
 *      String.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_ListExtObjects(Oxs_Director* director,Tcl_Interp *interp,
			  int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  return director->ListExtObjects();
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_ListEnergyObjects --
 *      Lists Energy objects that have been created
 *
 * Results:
 *      String.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_ListEnergyObjects(Oxs_Director* director,Tcl_Interp *interp,
			     int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  return director->ListEnergyObjects();
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_ListOutputObjects --
 *      Lists all output objects that are registered with the director.
 *
 * Results:
 *      String.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_ListOutputObjects(Oxs_Director* director, Tcl_Interp *interp,
			     int argc, CONST84 char** argv)
{
    if (argc != 1) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0], "\"", (char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
    }

    vector<String> outputs;
    director->ListOutputObjects(outputs);

    return Nb_MergeList(outputs);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_OutputGet--
 *      Calls output method of director.
 *
 * Results:
 *      Output is written into *interp's result.  Return value is
 *      an empty string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_OutputGet(Oxs_Director* director,Tcl_Interp *interp,
		     int argc,CONST84 char** argv)
{
    if (argc < 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0], " output ...\"", (char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
    }
    int errcode = director->Output(argv[1], interp, argc-2, argv+2);
    if(errcode!=TCL_OK) {
	throw OxsCmdsProcTclException(errcode);
    }
    return String("");
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_OutputNames --
 *      Retrieves output from all scalar output objects
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      Sets result of interp to a triple of strings giving the
 *      (long) name, units, and value for each output.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetAllScalarOutputs(Oxs_Director* director, Tcl_Interp *interp,
                               int argc, CONST84 char** argv)
{
    if (argc != 1) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0], "\"", (char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
    }
    vector<String> triples;
    director->GetAllScalarOutputs(triples);
    return Nb_MergeList(triples);
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_OutputNames --
 *      Retrieves names that describe an output.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *      Sets result of interp to a triple of strings giving the
 *      source, name, and type of output.
 *
 *----------------------------------------------------------------------
 */
String Oxs_OutputNames(Oxs_Director* director, Tcl_Interp *interp,
		       int argc, CONST84 char** argv)
{
    if (argc != 2) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0], " output\"", (char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
    }
    vector<String> names;
    director->OutputNames(argv[1],names);
    return Nb_MergeList(names);
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_QueryState--
 *      Read access to Oxs_SimState data.  This routine is intended to
 *      be used through the Oc_Class Oxs_Mif wrapper procs GetStateKeys
 *      and GetStageValues, which provide wildcard expansions.
 *
 * Results:
 *      String.  If the subcmd is "keys", then the return is a list of
 *      key names.  If the subcmd is "values" then the return is a list
 *      of alternating key value elements, suitable for importing into
 *      a Tcl dict via create/replace or a Tcl array using set.
 *
 *      The present implementation does not support any wildcards.  In
 *      particular, the keys subcommand takes no arguments and returns
 *      the entire list of keys, and for the values subcommand each key
 *      name must be an exact match to a key in the state key list.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_QueryState(Oxs_Director* director,Tcl_Interp *interp,
		     int argc,CONST84 char** argv)
{
    if (argc < 3) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0], " stateid subcmd ...\"", (char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
    }

    // Locate state
    OC_UINT4m id = static_cast<OC_UINT4m>(std::stoul(String(argv[1])));
    const Oxs_SimState* simstate = director->FindExistingSimulationState(id);
    if(!simstate) { // State not found
      Tcl_AppendResult(interp, "Requested state id=",
                       argv[1], " not found", (char *) NULL);
      throw OxsCmdsProcTclException(TCL_ERROR);
    }

    // Process command
    vector<String> result;
    if(strcmp("keys",argv[2])==0) {
      if(argc!=3) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0], " stateid keys\"", (char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
      }
      simstate->QueryScalarNames(result);
    } else if(strcmp("values",argv[2])==0) {
      if(argc<4) {
        Tcl_AppendResult(interp, "wrong # args: should be \"",
                         argv[0], " stateid values key ?key ...?\"",
                         (char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
      }
      // Request values
      vector<String> keys;
      vector<Oxs_MultiType> values;
      for(int i=3;i<argc;++i) {
        keys.push_back(String(argv[i]));
      }
      try {
        simstate->QueryScalarValues(keys,values);
      } catch(Oxs_Exception& err) { // Error
        Tcl_AppendResult(interp, "Query error in ",argv[0],"---",
                         err.MessageType(),"---",
                         err.MessageText().c_str(),(char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
      }
      // Extract result
      assert(keys.size() == values.size());
      String tmpval;
      for(std::size_t i=0;i<keys.size();++i) {
        result.push_back(keys[i]);
        values[i].Fill(tmpval);
        result.push_back(tmpval);
      }
    } else {
        Tcl_AppendResult(interp, "unrecognized subcommand name: \"",
                         argv[2], "\", should be either keys or values",
                         (char *) NULL);
	throw OxsCmdsProcTclException(TCL_ERROR);
    }

    return Nb_MergeList(result);
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_EvalScalarField --
 *      Returns the value of an Oxs_ScalarField at a specified
 *      (import) position.
 *
 * Results:
 *      String representing a single double value, which is the
 *      field value at the specified point.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_EvalScalarField
(Oxs_Director* director,Tcl_Interp *interp,
 int argc,CONST84 char** argv)
{

  if(argc!=5) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
                     argv[0]," FieldName x y z\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  OC_BOOL errx,erry,errz;

  vector<String> fieldname;
  Nb_SplitList fieldname_list;
  if(fieldname_list.Split(argv[1]) != TCL_OK) {
    Tcl_AppendResult(interp, "import field name \"",argv[1],
                     "\" is not a proper Tcl list",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  fieldname_list.FillParams(fieldname);
  fieldname_list.Release();

  OC_REAL8m x = Nb_Atof(argv[2],errx);
  OC_REAL8m y = Nb_Atof(argv[3],erry);
  OC_REAL8m z = Nb_Atof(argv[4],errz);
  if(errx || erry || errz) {
    Tcl_AppendResult(interp, "import position \"",argv[2],
                     "\", \"",argv[3],"\", \"",argv[4],
                     "\", is not a list of three real values.",
                     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  ThreeVector position(x,y,z);

  Oxs_OwnedPointer<Oxs_Ext> obj;
  try {
    Oxs_Ext::FindOrCreateObjectHelper(director,fieldname,obj,
                                      "Oxs_ScalarField");
  } catch(String& errmsg) {
    Tcl_AppendResult(interp,errmsg.c_str(),(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  Oxs_ScalarField* sf = dynamic_cast<Oxs_ScalarField*>(obj.GetPtr());
  if(sf==NULL) {
    Tcl_AppendResult(interp, "import field name \"",argv[1],
                "\" does not refer to a registered Oxs_ScalarField object",
                (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  OC_REAL8m value = sf->Value(position);

  char buf[256];
  Oc_Snprintf(buf,sizeof(buf),"%.17g",
	      static_cast<double>(value));
  return String(buf);
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_EvalVectorField --
 *      Returns the value of an Oxs_VectorField at a specified
 *      (import) position.
 *
 * Results:
 *      List of three double values (as a string), which is the
 *      field value at the specified point.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_EvalVectorField
(Oxs_Director* director,Tcl_Interp *interp,
 int argc,CONST84 char** argv)
{

  if(argc!=5) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
                     argv[0]," FieldName x y z\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  OC_BOOL errx,erry,errz;

  vector<String> fieldname;
  Nb_SplitList fieldname_list;
  if(fieldname_list.Split(argv[1]) != TCL_OK) {
    Tcl_AppendResult(interp, "import field name \"",argv[1],
                     "\" is not a proper Tcl list",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  fieldname_list.FillParams(fieldname);
  fieldname_list.Release();

  OC_REAL8m x = Nb_Atof(argv[2],errx);
  OC_REAL8m y = Nb_Atof(argv[3],erry);
  OC_REAL8m z = Nb_Atof(argv[4],errz);
  if(errx || erry || errz) {
    Tcl_AppendResult(interp, "import position \"",argv[2],
                     "\", \"",argv[3],"\", \"",argv[4],
                     "\", is not a list of three real values.",
                     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  ThreeVector position(x,y,z);

  Oxs_OwnedPointer<Oxs_Ext> obj;
  try {
    Oxs_Ext::FindOrCreateObjectHelper(director,fieldname,obj,
                                      "Oxs_VectorField");
  } catch(String& errmsg) {
    Tcl_AppendResult(interp,errmsg.c_str(),(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  Oxs_VectorField* vf = dynamic_cast<Oxs_VectorField*>(obj.GetPtr());
  if(vf==NULL) {
    Tcl_AppendResult(interp, "import field name \"",argv[1],
                "\" does not refer to a registered Oxs_VectorField object",
                (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  ThreeVector value;
  vf->Value(position,value);

  char buf[1024];
  Oc_Snprintf(buf,sizeof(buf),"%.17g %.17g %.17g",
	      static_cast<double>(value.x),
	      static_cast<double>(value.y),
	      static_cast<double>(value.z));
  return String(buf);
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetAtlasRegions --
 *      Get regions in an atlas.
 *
 * Results:
 *      List of all regions in the specified atlas.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetAtlasRegions
(Oxs_Director* director,Tcl_Interp *interp,
 int argc,CONST84 char** argv)
{

  if(argc!=2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
                     argv[0]," AtlasName\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  vector<String> atlasname;
  Nb_SplitList atlasname_list;
  if(atlasname_list.Split(argv[1]) != TCL_OK) {
    Tcl_AppendResult(interp, "import atlas name \"",argv[1],
                     "\" is not a proper Tcl list",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  atlasname_list.FillParams(atlasname);
  atlasname_list.Release();

  Oxs_OwnedPointer<Oxs_Ext> obj;
  try {
    Oxs_Ext::FindOrCreateObjectHelper(director,atlasname,obj,
                                      "Oxs_Atlas");
  } catch(String& errmsg) {
    Tcl_AppendResult(interp,errmsg.c_str(),(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  Oxs_Atlas* atlas = dynamic_cast<Oxs_Atlas*>(obj.GetPtr());
  if(atlas==NULL) {
    Tcl_AppendResult(interp, "import atlas name \"",argv[1],
                "\" does not refer to a registered Oxs_Atlas object",
                (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  vector<String> regions;
  atlas->GetRegionList(regions);
  String regions_str = Nb_MergeList(regions);

  return regions_str;
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetAtlasRegionByPosition --
 *      Get region in an atlas containing specified position
 *
 * Results:
 *      Region name; "universe" is returned if point is outside atlas.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetAtlasRegionByPosition
(Oxs_Director* director,Tcl_Interp *interp,
 int argc,CONST84 char** argv)
{

  if(argc!=5) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
                     argv[0]," AtlasName x y z\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  vector<String> atlasname;
  Nb_SplitList atlasname_list;
  if(atlasname_list.Split(argv[1]) != TCL_OK) {
    Tcl_AppendResult(interp, "import atlas name \"",argv[1],
                     "\" is not a proper Tcl list",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  atlasname_list.FillParams(atlasname);
  atlasname_list.Release();

  Oxs_OwnedPointer<Oxs_Ext> obj;
  try {
    Oxs_Ext::FindOrCreateObjectHelper(director,atlasname,obj,
                                      "Oxs_Atlas");
  } catch(String& errmsg) {
    Tcl_AppendResult(interp,errmsg.c_str(),(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  Oxs_Atlas* atlas = dynamic_cast<Oxs_Atlas*>(obj.GetPtr());
  if(atlas==NULL) {
    Tcl_AppendResult(interp, "import atlas name \"",argv[1],
                "\" does not refer to a registered Oxs_Atlas object",
                (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  OC_BOOL errx,erry,errz;
  OC_REAL8m x = Nb_Atof(argv[2],errx);
  OC_REAL8m y = Nb_Atof(argv[3],erry);
  OC_REAL8m z = Nb_Atof(argv[4],errz);
  if(errx || erry || errz) {
    Tcl_AppendResult(interp, "import position \"",argv[2],
                     "\", \"",argv[3],"\", \"",argv[4],
                     "\", is not a list of three real values.",
                     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  ThreeVector position(x,y,z);
  OC_INDEX id = atlas->GetRegionId(position);

  String region_name; // Convert id number to region name
  atlas->GetRegionName(id,region_name);

  return region_name;
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetThreadStatus --
 *      Returns human readable string indicating thread status.
 *      FOR DEBUGGING ONLY!
 *
 * Results:
 *      Thread status as a string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetThreadStatus(Oxs_Director* /* director */,Tcl_Interp *interp,
                           int argc,CONST84 char** argv)
{

  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  String results;
  Oxs_ThreadTree::GetStatus(results);

  return results;
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_ExtCreateAndRegister --
 *      Create and register an Oxs_Ext object.
 *
 * Results:
 *      Creates a new Oxs_Ext object.  Returns an empty string on
 *      success, throws an exception on error.
 *
 * Side effects:
 *      Newly created object is registered and stored with the director.
 *
 *----------------------------------------------------------------------
 */
String Oxs_ExtCreateAndRegister
(Oxs_Director* director,Tcl_Interp *interp,
 int argc,CONST84 char** argv)
{

  if(argc!=3) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
                     argv[0]," spec_key init_str\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  String bad_specs;
  int result
    = director->ExtCreateAndRegister(argv[1],argv[2],bad_specs);

  if(result != 0) {
    Tcl_AppendResult(interp, "Unrecognized Specify Block: ",
                     bad_specs.c_str(),
                     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  return String("");
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetCheckpointFilename --
 *      Retrieves full (absolute) path to checkpoint (restart) file.
 *
 * Results:
 *      If checkpointing is disabled then return value is an empty
 *      string.  Otherwise, returns the full, absolute path to the
 *      checkpoint file.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetCheckpointFilename(Oxs_Director* director,Tcl_Interp *interp,
                                int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  Oxs_Driver* driver = director->GetDriver();
  if(driver == 0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  return String(driver->GetCheckpointFilename());
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetCheckpointDisposal --
 *      Retrieves string representing checkpoint file disposal behavior
 *
 * Results:
 *      Results string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetCheckpointDisposal(Oxs_Director* director,Tcl_Interp *interp,
                                 int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  Oxs_Driver* driver = director->GetDriver();
  if(driver == 0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  return String(driver->GetCheckpointDisposal());
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_SetCheckpointDisposal --
 *      Sets checkpoint fila disposal behavior.
 *
 * Results:
 *      None (an empty string is returned)
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_SetCheckpointDisposal(Oxs_Director* director,
                                 Tcl_Interp *interp,
                                 int argc,CONST84 char** argv)
{
  if (argc != 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " cleanup_behavior\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  Oxs_Driver* driver = director->GetDriver();
  if(driver == 0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  driver->SetCheckpointDisposal(String(argv[1]));
  // Errors thrown by SetCheckpointDisposal (invalid disposal request
  // type) will be handled by OxsCmdsSwitchboard wrapper.

  return String("");
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetCheckpointInterval --
 *      Retrieves interval time between checkpoints, in (wall-clock)
 *      minutes
 *
 * Results:
 *      Results string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetCheckpointInterval(Oxs_Director* director,Tcl_Interp *interp,
                                 int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  Oxs_Driver* driver = director->GetDriver();
  if(driver == 0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  double interval = driver->GetCheckpointInterval();

  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%g",interval);
  /// Warning: Possible precision loss.  Might want to use Tcl's float
  /// to string routine instead.

  return String(buf);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_SetCheckpointInterval --
 *      Sets interval time between checkpoints, in (wall-clock)
 *      minutes.
 *
 * Results:
 *      None (an empty string is returned)
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

String Oxs_SetCheckpointInterval(Oxs_Director* director,
                                 Tcl_Interp *interp,
                                 int argc,CONST84 char** argv)
{
  if (argc != 2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		     " checkpoint_minutes\"", (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  Oxs_Driver* driver = director->GetDriver();
  if(driver == 0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  double interval;
  if (Tcl_GetDouble(interp, argv[1], &interval) != TCL_OK) {
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  driver->SetCheckpointInterval(interval);

  return String("");
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_GetCheckpointAge --
 *      Returns number of (wall-clock) seconds (as a double) since last
 *      checkpoint request.  Note that this is not especially accurate,
 *      as time marches forward after the call.  Nonetheless, should
 *      suffice for human interfaces.
 *
 * Results:
 *      Results string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_GetCheckpointAge(Oxs_Director* director,Tcl_Interp *interp,
                            int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  Oxs_Driver* driver = director->GetDriver();
  if(driver == 0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  Oc_Ticks ticks;
  ticks.ReadWallClock();
  ticks -= driver->GetCheckpointTicks();

  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%g",ticks.Seconds());

  return String(buf);
}

/*
 *----------------------------------------------------------------------
 *
 * Oxs_DirectorDevelopTest --
 *      Hook for development/debug testing.
 *
 * Results:
 *      Results string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_DirectorDevelopTest(Oxs_Director* director,Tcl_Interp *interp,
			       int argc,CONST84 char** argv)
{
  if(argc<2) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0]," routine ...\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  String routine = argv[1];
  String result_string;
  int result_code
    = director->DoDevelopTest(routine,result_string,argc-1,argv+1);
  if(result_code!=0) {
    char buf[256];
    Oc_Snprintf(buf,sizeof(buf),"%d",result_code);
    Tcl_AppendResult(interp,
		     "Test error ---\n   error  code: ",buf,
                     "\n result string: ",result_string.c_str(),
		     "\nSee DoDevelopTest source code in"
		     " oommf/app/oxs/base/director.cc"
		     " for details.",
		     (char *)NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }
  return result_string;
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_DriverLoadTestSetup --
 *      Hook for load tests, part of regression test harness.
 *
 * Results:
 *      Results string.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
String Oxs_DriverLoadTestSetup(Oxs_Director* director,Tcl_Interp *interp,
                               int argc,CONST84 char** argv)
{
  if (argc != 1) {
    Tcl_AppendResult(interp, "wrong # args: should be \"",
		     argv[0],"\"",(char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  String driver_name;
  if(director->GetDriverInstanceName(driver_name)!=0) {
    Tcl_AppendResult(interp, "Driver not initialized.",
		     (char *) NULL);
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  OC_UINT4m match_count;
  Oxs_Ext* ext = director->FindExtObject(driver_name,match_count);
  Oxs_Driver* driver = NULL;
  if(ext == NULL ||
     (driver = dynamic_cast<Oxs_Driver*>(ext)) == NULL) {
    if(match_count == 0) {
      Tcl_AppendResult(interp, "Unable to get pointer to Driver.",
                       (char *) NULL);
    } else {
      Tcl_AppendResult(interp, "Multiple Driver blocks in MIF file.",
                       (char *) NULL);
    }
    throw OxsCmdsProcTclException(TCL_ERROR);
  }

  driver->LoadTestSetup();

  return String("");
}


/*
 *----------------------------------------------------------------------
 *
 * Oxs_ProbRelease --
 *      Releases micromagnetic problem statement.  Depending on OS,
 *      this may free some memory, but is intended mainly to put
 *      the solver in a well-defined state.
 *  NB: This routine is unique in that it is called directly by
 *      the interpreter, rather than being routed through the
 *      switchboard routine.  This is to protect against potential
 *      reentrancy problems, and is justified by the special status
 *      of this routine as part of the error recovery path.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
int Oxs_ProbRelease(ClientData cd,Tcl_Interp *interp,
		   int argc,CONST84 char** argv)
{
  if (argc > 2) {
    Tcl_AppendResult(interp, "wrong # of args: should be",
                    " \"Oxs_ProbRelease ?errcode?\"",
		     (char *) NULL);
    return TCL_ERROR;
  }
  int errcode = 0;
  if (argc>1 && Tcl_GetInt(interp, argv[1], &errcode) != TCL_OK) {
    Tcl_AppendResult(interp,
                     "Specified errcode value is not an integer,"
                     " in call to \"Oxs_ProbRelease ?errcode?\"",
		     (char *) NULL);
    return TCL_ERROR;
  }

  Oxs_Director* director = (Oxs_Director *)cd;

  int result = TCL_ERROR;
  try {
    if(errcode) director->SetErrorStatus(errcode);
    director->Release();
    result = TCL_OK;
  } catch (Oxs_ExtError& err) {
    Tcl_AppendResult(interp,"Error thrown from inside"
		     " director ProbRelease function---",
		     err.c_str(),(char *) NULL);
    result = TCL_ERROR; // Safety
  } catch (Oxs_Exception& err) {
    Tcl_AppendResult(interp,"Error thrown from inside"
		     " director ProbRelease function---",
		     err.MessageType(),"---",
		     err.MessageText().c_str(),
		     (char *) NULL);
    result = TCL_ERROR; // Safety
  } catch (BAD_ALLOC&) {
    Tcl_AppendResult(interp,"Library error thrown from inside"
		     " director ProbRelease function---",
		     "Insufficient memory",(char *) NULL);
  } catch (EXCEPTION& err) {
    Tcl_AppendResult(interp,"Library error thrown from inside"
		     " director ProbRelease function---",
		     err.what(),(char *) NULL);
    result = TCL_ERROR; // Safety
#ifndef NDEBUG
  } catch (...) {
    Tcl_AppendResult(interp,"Unrecognized error thrown from inside"
		     " director ProbRelease function.",
		     (char *) NULL);
    result = TCL_ERROR; // Safety
#endif // NDEBUG
  }

  return result;
}


/*
 *----------------------------------------------------------------------
 *
 * OxsRegisterInterfaceCommands --
 *      Registers each of the above routines with the Tcl interpreter
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
void OxsRegisterInterfaceCommands(Oxs_Director* director,
				  Tcl_Interp *interp)
{
#define REGCMD(foocmd,fooname) \
  Tcl_CreateCommand(interp,OC_CONST84_CHAR(#foocmd),OxsCmdsSwitchboard, \
	    new OxsCmdsClientData(director,foocmd,String(fooname)),     \
	    OxsCmdsCleanup)
  // Note: The second argument to REGCMD is the name used for error
  // messages.  The first argument is used for both the C++ routine and
  // Tcl command names.
  REGCMD(Oxs_SetRestartFlag,"Oxs_Director::SetRestartFlag");
  REGCMD(Oxs_SetRestartCrcCheck,"Oxs_Director::SetRestartCrcCheck");
  REGCMD(Oxs_SetRestartFileDir,"Oxs_Director::SetRestartFileDir");
  REGCMD(Oxs_GetRestartFileDir,"Oxs_Director::GetRestartFileDir");
  REGCMD(Oxs_GetMifCrc,"Oxs_Director::GetMifCrc");
  REGCMD(Oxs_GetMifParameters,"Oxs_Director::GetMifParameters");
  REGCMD(Oxs_GetStage,"Oxs_Director::GetStage");
  REGCMD(Oxs_GetRunStateCounts,"Oxs_GetRunStateCounts");
  REGCMD(Oxs_GetCurrentStateId,"Oxs_GetCurrentStateId");
  REGCMD(Oxs_SetStage,"Oxs_Director::SetStage");
  REGCMD(Oxs_IsProblemLoaded,"Oxs_Director::IsProblemLoaded");
  REGCMD(Oxs_IsRunDone,"Oxs_Director::IsRunDone");
  REGCMD(Oxs_GetMif,"Oxs_Director::GetMifHandle");
  REGCMD(Oxs_ProbInit,"Oxs_Director::ProbInit");
  REGCMD(Oxs_ProbReset,"Oxs_Director::ProbReset");
  REGCMD(Oxs_Run,"Oxs_Run");
  REGCMD(Oxs_DriverName,"Oxs_DriverName");
  REGCMD(Oxs_MeshGeometryString,"Oxs_MeshGeometryString");
  REGCMD(Oxs_MifOption,"Oxs_MifOption");
  REGCMD(Oxs_ListRegisteredExtClasses,"Oxs_ListRegisteredExtClasses");
  REGCMD(Oxs_ListExtObjects,"Oxs_Director::ListExtObjects");
  REGCMD(Oxs_ListEnergyObjects,"Oxs_Director::ListEnergyObjects");
  REGCMD(Oxs_ListOutputObjects,"Oxs_Director::ListOutputObjects");
  REGCMD(Oxs_OutputGet,"Oxs_Director::Output");
  REGCMD(Oxs_GetAllScalarOutputs,"Oxs_Director::GetAllScalarOutputs");
  REGCMD(Oxs_OutputNames,"Oxs_Director::OutputNames");
  REGCMD(Oxs_QueryState,"Oxs_QueryState");
  REGCMD(Oxs_EvalScalarField,"Oxs_EvalScalarField");
  REGCMD(Oxs_EvalVectorField,"Oxs_EvalVectorField");
  REGCMD(Oxs_GetAtlasRegions,"Oxs_GetAtlasRegions");
  REGCMD(Oxs_GetAtlasRegionByPosition,"Oxs_GetAtlasRegionByPosition");
  REGCMD(Oxs_GetThreadStatus,"Oxs_GetThreadStatus");
  REGCMD(Oxs_ExtCreateAndRegister,"Oxs_ExtCreateAndRegister");
  REGCMD(Oxs_GetCheckpointFilename,"Oxs_GetCheckpointFilename");
  REGCMD(Oxs_GetCheckpointDisposal,"Oxs_GetCheckpointDisposal");
  REGCMD(Oxs_SetCheckpointDisposal,"Oxs_SetCheckpointDisposal");
  REGCMD(Oxs_GetCheckpointInterval,"Oxs_GetCheckpointInterval");
  REGCMD(Oxs_SetCheckpointInterval,"Oxs_SetCheckpointInterval");
  REGCMD(Oxs_GetCheckpointAge,"Oxs_GetCheckpointAge");
  REGCMD(Oxs_DirectorDevelopTest,"Oxs_DirectorDevelopTest");
  REGCMD(Oxs_DriverLoadTestSetup,"Oxs_DriverLoadTestSetup");
#undef REGCMD
  // The ProbRelease routine is special
  Tcl_CreateCommand(interp,OC_CONST84_CHAR("Oxs_ProbRelease"),
                    Oxs_ProbRelease,director,NULL);
}
