/* FILE: tclcommand.cc                 -*-Mode: c++-*-
 *
 * Wrapper for Tcl_Eval().
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2013/05/22 16:25:54 $
 * Last modified by: $Author: donahue $
 */

#include <exception>

#include "errhandlers.h"
#include "functions.h"
#include "tclcommand.h"

OC_USE_STD_NAMESPACE;

/* End includes */


////////////////////////////////////////////////////////////////////////

// Nb_SplitList
// Wrapper for Tcl_SplitList().  This class makes sure memory allocated
// by Tcl_SplitList() is released, even if normal execution path is
// interrupted by an exception.
void Nb_SplitList::Release()
{
  if(argv!=NULL) {
    Tcl_Free((char *)argv);
    argv=NULL;
  }
  argc=0;
}

Nb_SplitList::~Nb_SplitList()
{
  Release();
}

int Nb_SplitList::Split(const char* argstr)
{
  Release();
  char *str = new char[strlen(argstr)+1];
  strcpy(str,argstr);
  int errcode=Tcl_SplitList(NULL,str,&argc,&argv);
  delete[] str;
  return errcode;
}

void Nb_SplitList::FillParams(vector<String>& params) const
{
  params.clear();
  for(int i=0;i<argc;i++) params.push_back(String(argv[i]));
}


const char* Nb_SplitList::operator[](int n) const
{
  if(n<0 || n>=argc || argv==NULL) return NULL;
  return argv[n];
}

////////////////////////////////////////////////////////////////////////

// Wrappers for Tcl_Merge().
String Nb_MergeList(const vector<String>* args)
{
  String result;
  if(args==NULL || args->empty()) {
    return result; // Return empty string
  }
  int argc = static_cast<int>(args->size());
  assert(argc>0 && args->size() == size_t(argc)); // Overflow check

#if TCL_MAJOR_VERSION<8 || (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 4)
  // Life would be so much simpler if Tcl would use const char*
  // instead of char* in parameter lists.  Sigh...
  Oc_AutoBuf* mybuf = new Oc_AutoBuf[size_t(argc)];
  char **argv = new char*[size_t(argc)];
  for(int i=0;i<argc;i++) {
    mybuf[i].Dup((*args)[size_t(i)].c_str());
    argv[i] = mybuf[i].GetStr();
  }
  char* merged_string = Tcl_Merge(argc,argv);
  result = merged_string; // Copy into result string
  Tcl_Free(merged_string); // Free memory allocated inside Tcl_Merge()
  delete[] argv;
  delete[] mybuf;
#else // TCL version
  // Yeah!  Life is easier.
  const char* *argv =  new const char*[size_t(argc)];
  for(int i=0;i<argc;i++) argv[i] = (*args)[i].c_str();
  char* merged_string = Tcl_Merge(argc,argv);
  result = merged_string; // Copy into result string
  Tcl_Free(merged_string); // Free memory allocated inside Tcl_Merge()
  delete[] argv;
#endif // TCL version

  return result;
}

String Nb_MergeList(const vector<String>& args)
{
  return Nb_MergeList(&args);
}

#if (TCL_MAJOR_VERSION > 8 || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>0))
String Nb_MergeList(const Nb_TclObjArray& arr)
{
  String result;
  if(arr.Size()<1) {
    return result; // Return empty string
  }
  int argc = static_cast<int>(arr.Size());
  assert(argc>0 && arr.Size() == argc); // Overflow check

#if TCL_MAJOR_VERSION<8 || (TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION < 4)
  // Life would be so much simpler if Tcl would use const char*
  // instead of char* in parameter lists.  Sigh...
  Oc_AutoBuf* mybuf = new Oc_AutoBuf[size_t(argc)];
  char **argv = new char*[size_t(argc)];
  for(int i=0;i<argc;i++) {
    mybuf[i].Dup(arr.GetString(i).c_str());
    argv[i] = mybuf[i].GetStr();
  }
  char* merged_string = Tcl_Merge(argc,argv);
  result = merged_string; // Copy into result string
  Tcl_Free(merged_string); // Free memory allocated inside Tcl_Merge()
  delete[] argv;
  delete[] mybuf;
#else // TCL version
  // Yeah!  Life is easier.
  const char* *argv =  new const char*[size_t(argc)];
  for(int i=0;i<argc;i++) argv[i] = arr.GetString(i).c_str();
  char* merged_string = Tcl_Merge(argc,argv);
  result = merged_string; // Copy into result string
  Tcl_Free(merged_string); // Free memory allocated inside Tcl_Merge()
  delete[] argv;
#endif // TCL version

  return result;
}
#endif // Tcl version check

////////////////////////////////////////////////////////////////////////

void Nb_TclCommand::Dump(String& contents) const
{
  char buf[4096];
  contents = "exception_prefix: \"";
  contents += exception_prefix;
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  contents += "\"\ncommand_base: \"";
  contents += command_base;
  contents += "\"\n";
  Oc_Snprintf(buf,sizeof(buf),"extra args count: %d\n",
              extra_args_count);
  contents += buf;
  Oc_Snprintf(buf,sizeof(buf),"argc count: %d\n",command_args.size());
  contents += buf;
  for(int i=0;i<static_cast<int>(command_args.size());i++) {
    contents += " ->";
    contents += command_args[i];
    contents += "<-\n";
  }
  contents += "eval_result: \"";
  contents += eval_result;
  contents += "\"";
#else
  Oc_Snprintf(buf,sizeof(buf),"\"\nbase command size: %d\n",
              base_command_size);
  contents += buf;
  Oc_Snprintf(buf,sizeof(buf),"extra args count: %d\n",
              extra_args_count);
  contents += buf;
  Oc_Snprintf(buf,sizeof(buf),"objcmd size: %d\n",objcmd.Size());
  contents += buf;
  Tcl_Obj* const * arr = objcmd.Array();
  for(int i=0;i<objcmd.Size();i++) {
    char item[1000];
    Oc_EllipsizeMessage(item,sizeof(item),objcmd.GetString(i).c_str());
    Oc_Snprintf(buf,sizeof(buf)," Addr: %p ->%.1000s<-\n",
                arr[i],item);
    contents += buf;
    Oc_Snprintf(buf,sizeof(buf),
                "   refcount: %d, byteptr: %p, length: %d\n",
                arr[i]->refCount,arr[i]->bytes,arr[i]->length);
    contents += buf;
  }
  if(eval_result==NULL) {
      contents += "eval_result, Addr: <nilptr>";
  } else {
    char item[4000];
    Oc_EllipsizeMessage(item,sizeof(item),Tcl_GetString(eval_result));
    Oc_Snprintf(buf,sizeof(buf),
                "eval_result, Addr: %p, ->%.4000s<-\n",
                eval_result,item);
    contents += buf;
    Oc_Snprintf(buf,sizeof(buf),
                "   refcount: %d, byteptr: %p, length: %d\n",
                eval_result->refCount,eval_result->bytes,eval_result->length);
    contents += buf;
    if(eval_result->typePtr==NULL) {
      contents += "   Invalid type";
    } else {
      Oc_EllipsizeMessage(item,sizeof(item),eval_result->typePtr->name);
      Oc_Snprintf(buf,sizeof(buf),"   Type: %.4000s",item);
      contents += buf;
    }
  }
#endif
}

Nb_TclCommand::Nb_TclCommand()
  : interp(NULL), no_too_few_restores_warning(0),
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
    eval_list_set(0),
#else
    base_command_size(0),objcmd(0),eval_result(NULL),
#endif // Tcl version check
    extra_args_count(0)
{}

Nb_TclCommand::~Nb_TclCommand()
{
  ReleaseBaseCommand();
}

void Nb_TclCommand::ReleaseBaseCommand()
{
  extra_args_count=0;
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  eval_list_set=0;
  eval_list.Release();
  eval_result.erase();
  command_base.erase();
  command_args.clear();
#else
  if(eval_result!=NULL) {
    Tcl_DecrRefCount(eval_result);
    eval_result=NULL;
  }
  objcmd.Resize(0);
  base_command_size=0;
#endif // Tcl version check

  // Empty result stack.  We discard all stacked results, on the theory
  // that if the destructor is being called with items still in the
  // stack, then we are probably in an exceptional (read, error) state,
  // and perhaps the current Tcl result holds some useful error
  // information.  The alternative is to dump all the results except the
  // one at the bottom of the stack, with a command like
  //
  // if(it == result_stack.rend()-1) Tcl_RestoreResult(interp,&(*it));
  //
  // The boolean "unbalanced" is set true if the result stack is not
  // empty.  This presumably indicates a programming error.  After
  // freeing resources, we print a warning if unbalanced is true,
  // provided an exception is not currently being processed, and also
  // provided an earlier exception did not explicitly request no warning
  // by setting the member variable no_too_few_restores_warning true.
  vector<Tcl_SavedResult>::reverse_iterator it = result_stack.rbegin();
  OC_BOOL unbalanced = (it != result_stack.rend());
  while(it != result_stack.rend()) {
    Tcl_DiscardResult(&(*it));
    ++it;
  }
  result_stack.clear();

  interp=NULL;

  if(unbalanced &&
#if defined(__cplusplus) && __cplusplus>201700
 !uncaught_exceptions()
#else
 !uncaught_exception()
#endif
     && !no_too_few_restores_warning) {
    char item[4000];
    Oc_EllipsizeMessage(item,sizeof(item),exception_prefix.c_str());
    PlainWarning("%.4000s --- Unbalanced Tcl save/restore result calls"
                 " detected in Nb_TclCommand: too few restores.",
                 item);
  }

  no_too_few_restores_warning = 0; // Reset in any case

  exception_prefix.erase();
}

void Nb_TclCommand::SetBaseCommand
(const char* exception_prefix_,
 Tcl_Interp* interp_,
 const char* cmdbase,
 int extra_args_count_)
{
  // Free old command, if any
  ReleaseBaseCommand();

  // Setup new command
  exception_prefix = exception_prefix_;
  interp = interp_;
  extra_args_count = extra_args_count_;
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  // Tcl string interface.
  command_base = cmdbase;
  for(int iarg=0;iarg<extra_args_count;iarg++) {
    command_args.push_back(String("")); // Dummy fill
  }
#else
  // Tcl obj interface
  Nb_SplitList cmdlist;
  cmdlist.Split(cmdbase);
  base_command_size = cmdlist.Count();
  objcmd.Resize(base_command_size+extra_args_count);
  for(int iarg=0;iarg<base_command_size;iarg++) {
    objcmd.WriteString(iarg,cmdlist[iarg]);
  }
#endif // Tcl version check
}

void Nb_TclCommand::SaveInterpResult() const
{ // Conceptually const
  result_stack.resize(result_stack.size()+1);
  Tcl_SaveResult(interp,&(result_stack.back()));
}

void Nb_TclCommand::RestoreInterpResult() const
{ // Conceptually const
  if(result_stack.empty()) {
    String msg = exception_prefix
      + String(" --- Nb_ProgramLogicError: Tcl result restore call"
               " with no interp result saved.");
    OC_THROWTEXT(msg);
  }
  Tcl_RestoreResult(interp,&(result_stack.back()));
  result_stack.pop_back();
}

void Nb_TclCommand::DiscardInterpResult() const
{ // Conceptually const
  if(result_stack.empty()) {
    String msg = exception_prefix
      + String(" --- Nb_ProgramLogicError: Tcl result discard call"
               " with no interp result saved.");
    OC_THROWTEXT(msg);
  }
  Tcl_DiscardResult(&(result_stack.back()));
  result_stack.pop_back();
}

void Nb_TclCommand::SetCommandArg(int index,const char* arg) const
{ // Conceptually const
#ifndef NDEBUG
  if(index>=extra_args_count || index<0) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[4096];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::SetCommandArg: "
                "Array out-of-bounds; index=%d, arg=%.3000s",
                exception_prefix.c_str(),index,arg);
    OC_THROWTEXT(buf);
  }
#endif
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  command_args[index] = arg;
#else
  objcmd.WriteString(base_command_size+index,arg);
#endif // Tcl version check
}

void Nb_TclCommand::SetCommandArg(int index,OC_INT4m arg) const
{ // Conceptually const
#ifndef NDEBUG
  if(index>=extra_args_count || index<0) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::SetCommandArg: "
                "Array out-of-bounds; index=%d, arg=%ld",
                exception_prefix.c_str(),index,(long)arg);
    OC_THROWTEXT(buf);
  }
#endif
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%ld",static_cast<long>(arg));
  command_args[index] = buf;
#else
  objcmd.WriteLong(base_command_size+index,static_cast<long>(arg));
#endif // Tcl version check
}

void Nb_TclCommand::SetCommandArg(int index,OC_UINT4m arg) const
{ // Conceptually const
#ifndef NDEBUG
  if(index>=extra_args_count || index<0) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::SetCommandArg: "
                "Array out-of-bounds; index=%d, arg=%ld",
                exception_prefix.c_str(),index,(long)arg);
    OC_THROWTEXT(buf);
  }
#endif
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%lu",static_cast<unsigned long>(arg));
  command_args[index] = buf;
#else
  objcmd.WriteLong(base_command_size+index,static_cast<long>(arg));
  /// NOTE: there is no unsigned Tcl obj.  We could overflow
  /// check, but for now we'll just assume arg will fit into a long.
#endif // Tcl version check
}

void Nb_TclCommand::SetCommandArg(int index,OC_REAL8m arg) const
{ // Conceptually const
#ifndef NDEBUG
  if(index>=extra_args_count || index<0) {
    char buf[1024];
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::SetCommandArg: "
                "Array out-of-bounds; index=%d, arg=%.17g",
                exception_prefix.c_str(),index,
                static_cast<double>(arg));
    OC_THROWTEXT(buf);
  }
#endif
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  char buf[64];
  Oc_Snprintf(buf,sizeof(buf),"%.17g",static_cast<double>(arg));
  command_args[index] = buf;
#else
  objcmd.WriteDouble(base_command_size+index,static_cast<double>(arg));
#endif // Tcl version check
}

#if !NB_TCL_COMMAND_USE_STRING_INTERFACE
void Nb_TclCommand::SetCommandArg(int index,const Nb_TclObjArray& arg) const
{ // Conceptually const
#ifndef NDEBUG
  if(index>=extra_args_count || index<0) {
    char buf[1024];
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::SetCommandArg: "
                "Array out-of-bounds; index=%d, <list arg>",
                exception_prefix.c_str(),index);
    OC_THROWTEXT(buf);
  }
#endif
  // Note: No version of this routine in string interface
  objcmd.WriteList(base_command_size+index,arg);
}
#endif // !NB_TCL_COMMAND_USE_STRING_INTERFACE

void Nb_TclCommand::Eval() const
{ // Conceptually const
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  // For Tcl eval string interface
  if(eval_list_set) {
    eval_list.Release();
    eval_list_set=0;
  }
  String script = command_base
    + String(" ") + Nb_MergeList(command_args);

  int errcode = TCL_OK;
  try {
    errcode = Tcl_Eval(interp,OC_CONST84_CHAR(script.c_str()));
  } catch(...) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// This is expected to unbalance the results stack.  There does
    /// not seem to be any real reason to print an unbalanced result
    /// stack warning too.
    throw;
  }
  eval_result = Tcl_GetStringResult(interp);
#else // ! (Tcl version < 8.1)
  // Use Tcl eval obj interface
  if(eval_result!=NULL) {
    Tcl_DecrRefCount(eval_result); // Free old result
    eval_result=NULL; // Safety
  }

  int errcode = TCL_OK;
  try {
    errcode = Tcl_EvalObjv(interp,objcmd.Size(),objcmd.Array(),
                           TCL_EVAL_GLOBAL);
  } catch(...) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// This is expected to unbalance the results stack.  There does
    /// not seem to be any real reason to print an unbalanced result
    /// stack warning too.
    throw;
  }
  eval_result = Tcl_GetObjResult(interp);
  Tcl_IncrRefCount(eval_result);
#endif // (Tcl version < 8.1)

  if(errcode!=TCL_OK) {
    String msg = exception_prefix
      + String(" --- Error evaluating Tcl script: ");
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
    msg += script + String("\n--- Error message: ") + eval_result;
#else
    msg += Nb_MergeList(objcmd)
      + String("\n--- Error message: ")
      + String(Tcl_GetString(eval_result));
#endif
    // Extended error info
    const char* ei = Tcl_GetVar(interp,OC_CONST84_CHAR("errorInfo"),
                                TCL_GLOBAL_ONLY);
    const char* ec = Tcl_GetVar(interp,OC_CONST84_CHAR("errorCode"),
                                TCL_GLOBAL_ONLY);
    if(ei==NULL) ei = "";
    if(ec==NULL) ec = "";

    // NOTE TO SELF: Change this to 0 to test Don's improved
    // DisplayError proc in oxsii.tcl (mjd, 25-June-2002)
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// This is expected to unbalance the results stack, unless the
    /// client wrapped up the Eval call in a try block.  There does
    /// not seem to be any real reason to print an unbalanced result
    /// stack warning too.
    OC_THROW(Oc_TclError(__FILE__,__LINE__,
                         "Nb_TclCommand","Eval",ei,ec,
                         msg.size()+1,msg.c_str()));
  }
}

String Nb_TclCommand::GetWholeResult() const
{
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  return eval_result;
#else // ! (Tcl version < 8.1)
  if(eval_result==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    OC_THROWTEXT("ProgramLogicError: No eval result;"
              " Eval() has not been called on Nb_TclCommand");
  }
  return String(Tcl_GetString(eval_result));
#endif // (Tcl version < 8.1)
}

#if NB_TCL_COMMAND_USE_STRING_INTERFACE
void Nb_TclCommand::FillEvalList() const
{
  if(eval_list.Split(eval_result.c_str())!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    String msg = exception_prefix
      + String(" --- TclBadReturnType: Tcl script return is not a list: ")
      + eval_result;
    OC_THROWTEXT(msg);
  }
  eval_list_set=1;
}
#endif

int Nb_TclCommand::GetResultListSize() const
{
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  if(!eval_list_set) FillEvalList();
  return eval_list.Count();
#else // ! (Tcl version < 8.1)
  if(eval_result==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    OC_THROWTEXT("ProgramLogicError: No eval result;"
              " Eval() has not been called on Nb_TclCommand");
  }
  int size;
  if(Tcl_ListObjLength(NULL,eval_result,&size)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    String msg = exception_prefix
      + String(" --- TclBadReturnType: Tcl script return is not a list: ")
      + GetWholeResult();
    OC_THROWTEXT(msg);
  }
  return size;
#endif // (Tcl version < 8.1)
}

void Nb_TclCommand::GetResultListItem(int index,String& result) const
{
#ifndef NDEBUG
  int size = GetResultListSize();
  if(index>=size || index<0) {
    char buf[1024];
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Array out-of-bounds; index=%d"
                " (should be >0 and <%d).",
                exception_prefix.c_str(),index,size);
    OC_THROWTEXT(buf);
  }
#endif
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  if(!eval_list_set) FillEvalList();
  result = eval_list[index];
#else // ! (Tcl version < 8.1)
  if(eval_result==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    OC_THROWTEXT("ProgramLogicError: No eval result;"
              " Eval() has not been called on Nb_TclCommand");
  }
  Tcl_Obj* obj;
  if(Tcl_ListObjIndex(NULL,eval_result,index,&obj)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    String msg = exception_prefix
      + String(" --- TclBadReturnType: Tcl script return is not a list: ")
      + GetWholeResult();
    OC_THROWTEXT(msg);
  }
  if(obj==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Array out-of-bounds; index=%d.",
                exception_prefix.c_str(),index);
    OC_THROWTEXT(buf);
  }
  result = Tcl_GetString(obj);
#endif // (Tcl version < 8.1)
}

void Nb_TclCommand::GetResultListItem(int index,OC_INT4m& result) const
{
#ifndef NDEBUG
  int size = GetResultListSize();
  if(index>=size || index<0) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Array out-of-bounds; index=%d"
                " (should be >0 and <%d).",
                exception_prefix.c_str(),index,size);
    OC_THROWTEXT(buf);
  }
#endif
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  if(!eval_list_set) FillEvalList();
  char* cptr;
  result = static_cast<OC_INT4m>(strtol(eval_list[index],&cptr,10));
  if(eval_list[index][0]=='\0' || *cptr!='\0') {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[2048];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Tcl script return item at index=%d"
                " is not an integer: %.500s",
                exception_prefix.c_str(),index,eval_list[index]);
    OC_THROWTEXT(buf);
  }
#else // ! (Tcl version < 8.1)
  if(eval_result==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    OC_THROWTEXT("ProgramLogicError: No eval result;"
              " Eval() has not been called on Nb_TclCommand");
  }
  Tcl_Obj* obj;
  if(Tcl_ListObjIndex(NULL,eval_result,index,&obj)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    String msg = exception_prefix
      + String(" --- TclBadReturnType: Tcl script return is not a list: ")
      + GetWholeResult();
    OC_THROWTEXT(msg);
  }
  if(obj==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Array out-of-bounds; index=%d.",
                exception_prefix.c_str(),index);
    OC_THROWTEXT(buf);
  }
  long lresult;
  if(Tcl_GetLongFromObj(NULL,obj,&lresult)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[2048];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Tcl script return item at index=%d"
                " is not a long integer: %.500s",
                exception_prefix.c_str(),index,Tcl_GetString(obj));
    OC_THROWTEXT(buf);
  }
  result = static_cast<OC_INT4m>(lresult);
#endif // (Tcl version < 8.1)
}

void Nb_TclCommand::GetResultListItem(int index,OC_UINT4m& result) const
{
#ifndef NDEBUG
  int size = GetResultListSize();
  if(index>=size || index<0) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Array out-of-bounds; index=%d"
                " (should be >0 and <%d).",
                exception_prefix.c_str(),index,size);
    OC_THROWTEXT(buf);
  }
#endif
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  if(!eval_list_set) FillEvalList();
  char* cptr;
  result = static_cast<OC_UINT4m>(strtoul(eval_list[index],&cptr,10));
  if(eval_list[index][0]=='\0' || *cptr!='\0') {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[2048];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Tcl script return item at index=%d"
                " is not an integer: %.500s",
                exception_prefix.c_str(),index,eval_list[index]);
    OC_THROWTEXT(buf);
  }
#else // ! (Tcl version < 8.1)
  if(eval_result==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    OC_THROWTEXT("ProgramLogicError: No eval result;"
              " Eval() has not been called on Nb_TclCommand");
  }
  Tcl_Obj* obj;
  if(Tcl_ListObjIndex(NULL,eval_result,index,&obj)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    String msg = exception_prefix
      + String(" --- TclBadReturnType: Tcl script return is not a list: ")
      + GetWholeResult();
    OC_THROWTEXT(msg);
  }
  if(obj==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Array out-of-bounds; index=%d.",
                exception_prefix.c_str(),index);
    OC_THROWTEXT(buf);
  }
  long lresult;
  if(Tcl_GetLongFromObj(NULL,obj,&lresult)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[2048];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Tcl script return item at index=%d"
                " is not a long integer: %.500s",
                exception_prefix.c_str(),index,Tcl_GetString(obj));
    OC_THROWTEXT(buf);
  }
  result = static_cast<OC_UINT4m>(lresult);
  // Note: There is no unsigned Tcl obj.
#endif // (Tcl version < 8.1)
}

void Nb_TclCommand::GetResultListItem(int index,OC_REAL8m& result) const
{
#ifndef NDEBUG
  int size = GetResultListSize();
  if(index>=size || index<0) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Array out-of-bounds; index=%d"
                " (should be >0 and <%d).",
                exception_prefix.c_str(),index,size);
    OC_THROWTEXT(buf);
  }
#endif
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  if(!eval_list_set) FillEvalList();
  OC_BOOL err;
  result = static_cast<OC_REAL8m>(Nb_Atof(eval_list[index],err));
  if(err) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[2048];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Tcl script return item at index=%d"
                " is not a floating point number: %.500s",
                exception_prefix.c_str(),index,eval_list[index]);
    OC_THROWTEXT(buf);
  }
#else // Obj interface
  if(eval_result==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    OC_THROWTEXT("ProgramLogicError: No eval result;"
              " Eval() has not been called on Nb_TclCommand");
  }
  Tcl_Obj* obj;
  if(Tcl_ListObjIndex(NULL,eval_result,index,&obj)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    String msg = exception_prefix
      + String(" --- TclBadReturnType: Tcl script return is not a list: ")
      + GetWholeResult();
    OC_THROWTEXT(msg);
  }
  if(obj==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Array out-of-bounds; index=%d.",
                exception_prefix.c_str(),index);
    OC_THROWTEXT(buf);
  }
  double tmpresult;
  if(Tcl_GetDoubleFromObj(NULL,obj,&tmpresult)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    char buf[2048];
    Oc_Snprintf(buf,sizeof(buf),
                "%.800s --- void Nb_TclCommand::GetResultListItem:"
                " Tcl script return item at index=%d"
                " is not a floating point number: %.500s",
                exception_prefix.c_str(),index,Tcl_GetString(obj));
    OC_THROWTEXT(buf);
  }
  result = static_cast<OC_REAL8m>(tmpresult);
#endif // (Tcl version < 8.1)
}

#if !OC_REAL8m_IS_OC_REAL8
void Nb_TclCommand::GetResultListItem(int index,OC_REAL8& result) const
{
  OC_REAL8m result8m;
  GetResultListItem(index,result8m);
  result = result8m;
}
#elif !OC_REAL8m_IS_DOUBLE
void Nb_TclCommand::GetResultListItem(int index,double& result) const
{
  OC_REAL8m result8m;
  GetResultListItem(index,result8m);
  result = result8m;
}
#endif

void Nb_TclCommand::GetResultList(vector<String>& result_list) const
{
#if NB_TCL_COMMAND_USE_STRING_INTERFACE
  if(!eval_list_set) FillEvalList();
  eval_list.FillParams(result_list);
#else // ! (Tcl version < 8.1)
  if(eval_result==NULL) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    OC_THROWTEXT("ProgramLogicError: No eval result;"
              " Eval() has not been called on Nb_TclCommand");
  }
  int obj_count;
  Tcl_Obj** obj_arr;
  if(Tcl_ListObjGetElements(NULL,eval_result,&obj_count,&obj_arr)!=TCL_OK) {
    no_too_few_restores_warning = 1; // We're throwing an exception.
    /// See notes in ::Eval() about result stack and exceptions
    String msg = exception_prefix
      + String(" --- TclBadReturnType: Tcl script return is not a list: ")
      + GetWholeResult();
    OC_THROWTEXT(msg);
  }
  result_list.clear();
  for(int i=0;i<obj_count;i++) {
    result_list.push_back(String(Tcl_GetString(obj_arr[i])));
  }
#endif // (Tcl version < 8.1)
}

/////////////////////////////////////////////////////////////////////
//
// Nb_ParseTclCommandLineRequest, intended to be used in support of
// Nb_TclCommand. The label and param_count fields of each
// Nb_TclCommandLineOption object in the import/export "options"
// parameter should be filled on entry. Import request_string is
// a Tcl list, where each list item matches the label portion of
// some Nb_TclCommandLineOption object in "options".
// Nb_ParseTclCommandLineRequest fills in the position field of
// each Nb_TclCommandLineOption object corresponding to the
// position of that option in the request_string.  If the option
// is not used, the position field is set to -1.  The position
// is padded to account for the number of parameters in each
// preceding option in request_string.  The return value is the
// number of total parameters associated with the options selected
// in request_string.  Exceptions are thrown if request_string
// contains an option not included in "options", or if the same
// option is selected more than once.
//   NOTE: If the param_count value for an Nb_TclCommandLineOption
// is <1, then that option is not available in the current instance.
// This is a convenience for clients of Nb_TclCommand, because it
// allows an option to hold a place in the Nb_TclCommandLineOption
// "options" vector returned by Nb_ParseTclCommandLineRequest, w/o
// allowing it to be used.  (Intended for options which are
// conditionally available, where <1 indicates that the necessary
// prerequisites are missing.))
//
int Nb_ParseTclCommandLineRequest
(const char* exception_prefix,
 vector<Nb_TclCommandLineOption>& options,
 const String& request_string)
{
  // Initialize options vector
  vector<Nb_TclCommandLineOption>::iterator opit = options.begin();
  while(opit != options.end()) {
    opit->position = -1;
    ++opit;
  }

  // Split request string
  Nb_SplitList request;
  request.Split(request_string);

  // Process request string
  int total_param_count=0;
  for(int i=0;i<request.Count();i++) {
    opit = options.begin();
    while(opit != options.end()) {
      if(opit->Label().compare(request[i])==0) {
        // Match
        if(opit->position >= 0) {
          // Option already selected
          String msg = String(exception_prefix)
            + String(" --- BadParameter: Duplicate option request: \"")
            + request[i] + String("\"");
          OC_THROWTEXT(msg);
        }
        if(opit->ParamCount()<1) {
          // Option not supported in this instance
          String msg = String(exception_prefix)
            + String(" --- BadParameter: Invalid option request: \"")
            + request[i]
            + String("\"; prerequisite(s) missing.");
          OC_THROWTEXT(msg);
        }
        opit->position = total_param_count;
        total_param_count += opit->ParamCount();
        break;
      }
      ++opit;
    }
    if(opit == options.end()) {
      // No match
      String msg = String(exception_prefix)
        + String("BadParameter: Unrecognized option request: \"")
        + request[i] + String("\"");
      OC_THROWTEXT(msg);
    }
  }

  return total_param_count;
}

