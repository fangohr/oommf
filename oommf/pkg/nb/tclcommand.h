/* FILE: tclcommand.h                 -*-Mode: c++-*-
 *
 * Wrapper for Tcl_Eval().
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/11/09 00:19:10 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_TCLCOMMAND
#define _NB_TCLCOMMAND

#include <string>
#include <vector>

#include "oc.h"

#include "tclobjarray.h"

OC_USE_STRING;

/* End includes */

// Use the TclObj interface with Tcl later than 8.0; earlier than that
// we must use the string interface.  However, one can set the macro
// NB_TCL_COMMAND_USE_STRING_INTERFACE to 1 to force the use of the
// string interface.  This may be useful for debugging purposes.
#ifndef NB_TCL_COMMAND_USE_STRING_INTERFACE
# if TCL_MAJOR_VERSION < 8 || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION==0)
#  define NB_TCL_COMMAND_USE_STRING_INTERFACE 1
# else
#  define NB_TCL_COMMAND_USE_STRING_INTERFACE 0
# endif
#endif

////////////////////////////////////////////////////////////////////////

// Wrapper for Tcl_SplitList().  This class makes sure memory allocated
// by Tcl_SplitList() is released, even if normal execution path is
// interrupted by an exception.
class Nb_SplitList
{
private:
  int argc;
  CONST84 char ** argv;
public:
  Nb_SplitList() : argc(0), argv(NULL) {}
  void Release();
  int Split(const char* argstr);
  int Split(const String& argstr) { return Split(argstr.c_str()); }
  int Count() const { return argc; }
  const char* operator[](int n) const;
  void FillParams(vector<String>& params) const;
  ~Nb_SplitList();
};

////////////////////////////////////////////////////////////////////////

// Wrappers for Tcl_Merge().
String Nb_MergeList(const vector<String>* args);
String Nb_MergeList(const vector<String>& args);
#if (TCL_MAJOR_VERSION > 8 || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>0))
String Nb_MergeList(const Nb_TclObjArray& arr);
#endif // Tcl version check

////////////////////////////////////////////////////////////////////////

// Nb_TclCommand is a wrapper for Tcl_Eval.  The script to be evaluated
// is divided into two parts: the base command and the args.  Setting
// the args and evaluating the command are viewed as a function call on
// the base command, and as such are conceptually const.  For this
// reason, the args and result are stored in mutable storage, so that
// they may be changed by a const Nb_TclCommand object.
//   For similar reasons, saving and restoring the interp result is
// also considered conceptually const.
class Nb_TclCommand
{
private:
  String exception_prefix;
  Tcl_Interp* interp;
  mutable vector<Tcl_SavedResult> result_stack;
  mutable OC_BOOL no_too_few_restores_warning; // If this is set,
  /// then no warning is raised on cleanup if there are unpopped
  /// results.  This is intended for exception handling.  It
  /// resets to 0 after each cleanup.
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  String command_base;  // Non-mutable!
  mutable vector<String> command_args;
  mutable String eval_result;
  mutable OC_BOOL eval_list_set;
  mutable Nb_SplitList eval_list; // Cached list version of eval_result
  void FillEvalList() const;
#else
  int base_command_size;
  mutable Nb_TclObjArray objcmd;
  mutable Tcl_Obj* eval_result;
#endif // Tcl version check
  int extra_args_count;
public:
  Nb_TclCommand();
  ~Nb_TclCommand();

  void Dump(String& contents) const; // For debugging

  void SetBaseCommand(const char* exception_prefix_,
		      Tcl_Interp* interp_,
		      const char* cmdbase,
		      int extra_args_count_);

  void SetBaseCommand(const char* exception_prefix_,
		      Tcl_Interp* interp_,
		      const String& cmdbase,
		      int extra_args_count_) {
    SetBaseCommand(exception_prefix_,interp_,
                   cmdbase.c_str(),extra_args_count_);
  }

  void ReleaseBaseCommand();

  void SaveInterpResult() const;
  void RestoreInterpResult() const;
  void DiscardInterpResult() const;

  void SetCommandArg(int index,const char* arg) const;
  void SetCommandArg(int index,const String& arg) const
    { SetCommandArg(index,arg.c_str()); }
  void SetCommandArg(int index,OC_INT4m arg) const;
  void SetCommandArg(int index,OC_UINT4m arg) const;
  void SetCommandArg(int index,OC_REAL8m arg) const;

#if !NB_TCL_COMMAND_USE_STRING_INTERFACE
  void SetCommandArg(int index,const Nb_TclObjArray& arg) const;
#endif

  void Eval() const;

  String GetWholeResult() const;

  int GetResultListSize() const;
  void GetResultListItem(int index,String& result) const;
  void GetResultListItem(int index,OC_INT4m& result) const;
  void GetResultListItem(int index,OC_UINT4m& result) const;
  void GetResultListItem(int index,OC_REAL8m& result) const;
#if !OC_REAL8m_IS_OC_REAL8
  void GetResultListItem(int index,OC_REAL8& result) const;
#elif !OC_REAL8m_IS_DOUBLE
  void GetResultListItem(int index,double& result) const;
#endif
  void GetResultList(vector<String>& result_list) const;
};


/////////////////////////////////////////////////////////////////////
//
// Nb_ParseTclCommandLineRequest, intended to be used in support of
// Nb_TclCommand. The label and param_count fields of each
// Nb_TclCommandLineOptions object in the import/export "options"
// parameter should be filled on entry. Import request_string is
// a Tcl list, where each list item matches the label portion of
// some Nb_TclCommandLineOptions object in "options".
// Nb_ParseTclCommandLineRequest fills in the position field of
// each Nb_TclCommandLineOptions object corresponding to the
// position of that option in the request_string.  If the option
// is not used, the position field is set to -1.  The position
// is padded to account for the number of parameters in each
// preceding option in request_string.  The return value is the
// number of total parameters associated with the options selected
// in request_string.  Exceptions are thrown if request_string
// contains an option not included in "options", or if the same
// options is selected more than once.
//   NOTE: If the param_count value for an Nb_TclCommandLineOption
// is <1, then that option is not available in the current instance.
// This is a convenience for clients of Nb_TclCommand, because it
// allows an option to hold a place in the Nb_TclCommandLineOption
// "options" vector returned by Nb_ParseTclCommandLineRequest, w/o
// allowing it to be used.  (Intended for options which are
// conditionally available, where <1 indicates that the necessary
// prerequisites are missing.)
//
class Nb_TclCommandLineOption
{
private:
  String label; // Identifying string for option
  int param_count; // Number of parameters associated with option
  /// I'd like to make label and param_count const, but that would
  /// disallow operator=, which is needed if the class is used
  /// inside STL containers such as vector.
public:
  const String& Label() { return label; }
  int ParamCount() { return param_count; }
  int position; // Command line position; -1 indicates option
  /// not used.
  Nb_TclCommandLineOption() {};
  Nb_TclCommandLineOption(String label_,int param_count_)
    : label(label_), param_count(param_count_), position(-1) {}
};
OC_BOOL operator<(const Nb_TclCommandLineOption&, const Nb_TclCommandLineOption&);
OC_BOOL operator>(const Nb_TclCommandLineOption&, const Nb_TclCommandLineOption&);
OC_BOOL operator==(const Nb_TclCommandLineOption&, const Nb_TclCommandLineOption&);
//OC_BOOL operator<(const Tcl_SavedResult&, const Tcl_SavedResult&);
//OC_BOOL operator>(const Tcl_SavedResult&, const Tcl_SavedResult&);
//OC_BOOL operator==(const Tcl_SavedResult&, const Tcl_SavedResult&);

int Nb_ParseTclCommandLineRequest
(const char* exception_prefix,
 vector<Nb_TclCommandLineOption>& options,
 const String& request_string);

#endif // _NB_TCLCOMMAND
