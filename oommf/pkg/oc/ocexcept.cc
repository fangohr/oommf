/* FILE: except.cc                    -*-Mode: c++-*-
 *
 *   Simple exception object for C++ exceptions.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2012/08/31 20:06:51 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>
#include <cctype> // std::tolower
#include <csignal>
#include <cstdlib>
#include <cstring>

#include <iostream>
#include <vector>

#include "ocexcept.h"
#include "messages.h"

/* End includes */     /* Optional directive to pimake */

// Some declarations from oc.h
OC_BOOL Oc_IsGlobalInterpThread();
OC_BOOL Oc_GetOcOption(const std::string& classname,
                       const std::string& option,
                       std::string& value);
#if (OC_SYSTEM_TYPE == OC_WINDOWS)
std::string Oc_WinStrError(DWORD errorID);
#endif

#define INT_DISPLAY_LENGTH_BOUND 256   // Should be safe.

// Constructor helper.  This is broken out to make it available to
// child classes.
void Oc_Exception::SetErrMsg
(size_t errmsg_size_in,
 const char* errfmt_in,
 va_list arg_ptr)
{
  if(errfmt_in == NULL) {
    errmsg.Dup("");
  } else {
    errmsg_size_in += 15; // Add a little extra space, in case the client
    errmsg.SetLength(errmsg_size_in);   /// is arithmetically challenged.
    Oc_Vsnprintf(errmsg.GetStr(),errmsg_size_in,errfmt_in,arg_ptr);
  }
}

// Base constructors
Oc_Exception::Oc_Exception
(const char* file_in,      // File from which exception is thrown
 int lineno_in,            // Line number from which exception is thrown
 const char* classname_in, // Name of class throwing exception
 const char* funcname_in,  // Name of function throwing exception
 size_t errmsg_size_in,    // Buffer size needed to hold extd. error msg
 const char* errfmt_in,    // Format string for extended error message
 ...)                      // arguments for errfmt
  : file(file_in), lineno(lineno_in),
    classname(classname_in), funcname(funcname_in)
{
  va_list arg_ptr;
  va_start(arg_ptr,errfmt_in);
  SetErrMsg(errmsg_size_in,errfmt_in,arg_ptr);
  va_end(arg_ptr);
}

Oc_Exception::Oc_Exception
(const char* file_in,      // File from which exception is thrown
 int lineno_in,            // Line number from which exception is thrown
 const char* classname_in, // Name of class throwing exception
 const char* funcname_in,  // Name of function throwing exception
 const char* errmsg_in)       // Error message
  : file(file_in), lineno(lineno_in),
    classname(classname_in), funcname(funcname_in),
    errmsg(errmsg_in)
{}

void Oc_Exception::PrependMessage(const char* prefix)
{
  size_t prefix_len = strlen(prefix);
  Oc_AutoBuf tmp;
  tmp.SetLength(prefix_len+errmsg.GetLength());
  strcpy(tmp.GetStr(),prefix);
  strcpy(tmp.GetStr()+prefix_len,errmsg.GetStr());
  errmsg = tmp;
}

void Oc_Exception::PostpendMessage(const char* suffix)
{
  size_t suffix_len = strlen(suffix);
  errmsg.SetLength(errmsg.GetLength()+suffix_len);
  strcpy(errmsg.GetStr()+suffix_len,suffix);
}


// ConstructMessage is intended to build a generic error essage
// out of the data stored in this object.  We may add interfaces
// to the private data if there arises a need for specially
// tailored error messages.
const char* Oc_Exception::ConstructMessage(Oc_AutoBuf& msg) const {
  // File info
  Oc_AutoBuf fileinfo;
  const char* fileinfo_fmt
    = "Error detected; refer to program source file %s, line %d, ";
  size_t fileinfo_bufsize = strlen(fileinfo)
    + strlen(file.GetStr()) + INT_DISPLAY_LENGTH_BOUND;
  fileinfo.SetLength(fileinfo_bufsize);
  Oc_Snprintf(fileinfo.GetStr(),fileinfo_bufsize,
              fileinfo_fmt,file.GetStr(),lineno);

  // Class info
  Oc_AutoBuf proginfo;
  if(strlen(classname.GetStr())>0) {
    const char* proginfo_fmt="in class %s, member function %s.";
    size_t proginfo_bufsize = strlen(proginfo_fmt)
      + strlen(classname.GetStr()) + strlen(funcname.GetStr());
    proginfo.SetLength(proginfo_bufsize);
    Oc_Snprintf(proginfo.GetStr(),proginfo_bufsize,
                proginfo_fmt,classname.GetStr(),funcname.GetStr());
  } else {
    const char* proginfo_fmt="in function %s.";
    size_t proginfo_bufsize = strlen(proginfo_fmt)
      + strlen(funcname.GetStr());
    proginfo.SetLength(proginfo_bufsize);
    Oc_Snprintf(proginfo.GetStr(),proginfo_bufsize,
                proginfo_fmt,funcname.GetStr());
  }

  // Build message.
  const char* combine_fmt = "%s%s\nERROR DETAILS: %s";
  msg.SetLength(strlen(combine_fmt)
                +strlen(fileinfo.GetStr())
                +strlen(proginfo.GetStr())
                +strlen(errmsg.GetStr()));
  Oc_Snprintf(msg.GetStr(),msg.GetLength(),combine_fmt,
              fileinfo.GetStr(),proginfo.GetStr(),errmsg.GetStr());

  return msg.GetStr();
}

////////////////////////////////////////////////////////////////////////
// Asynchronous error handling
//
// OVERVIEW: Two types of asynchronous (i.e., outside normal C++ flow)
// errors handled by this code, POSIX signals, and Microsoft "Structured
// Exception Handling" errors. The latter occur only on Windows with
// code compiled with the /EHa switch to the Visual C++ "cl"
// compiler. POSIX signals are supported on all platforms supported by
// OOMMF --- although there as of Apr 2022 there is an issue on macOS,
// set note (***) below.
//
// POSIX signals are caught by a function registered with the C++
// std::signal(int signum, sighandler_t handler) call. (POSIX deprecates
// signal() in favor of sigaction(), but the latter is not in the C++
// standard.) The signal handler should be as minimalistic as possible,
// similar to say an interrupt handler. You can check the signal docs to
// see exactly what operations and library functions calls are
// guaranteed to work inside a signal handler, but one of those is
// quick_exit. The signal handling code below merely sets
// the unnamed namespace variable ocasyncerror_signal_status to
// the signum import and calls std::quick_exit().
//
// On Windows, the cl /EHa switch causes asynchronous errors to be
// converted C++ exceptions; however, by default these C++ exceptions
// are untyped and can only be caught by a catch(...) clause in a
// try/catch block. But Microsoft provides a translator hook,
// _set_se_translator(funcptr()), which catch "structured exceptions"
// before they flow back to any C++ try/catch and allow them to be
// converted and thrown to a user-specified C++ type. The import to
// _set_se_translator has a signature
//
//    func(unsigned int,EXCEPTION_POINTERS*)
//
// and it is suppose to do nothing except use its imports to create
// and throw a C++ object.
//
// The OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH code below
// hijacks this mechanism by having funcptr set the unnamed namespace
// variable ocasyncerror_seh_errcode to the unsigned int import and then
// calling std::quick_exit() to terminate the program.
//
// Selection of asynchronous and quick_exit handlers depends on the
// macro OC_ASYNC_EXCEPTION_HANDLING (determined at compile time by the
// program_compiler_c++_async_exception_handling value in the config
// <platform>.tcl file) and run-time Oc_Option AsyncExceptionHandling
// setting (from the config options.tcl file).
//
// The only std::signal() handler employed is
//
//   Oc_AsyncError::CatchSignal(int)
//
// This is set if OC_ASYNC_EXCEPTION_HANDLING != OC_ASYNC_EXCEPT_NONE
// and Oc_Option AsyncExceptionHandling is either POSIX or SEH. On
// Windows this needs to be set in all execution threads; on unix-like
// systems signal handlers are inherited in children.
//
// If OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH then the Visual
// C++ cl /EHa switch is enabled and _set_se_translator needs to be
// set. It will be either
//
//   Oc_AsyncError::CatchSEH(unsigned int,EXCEPTION_POINTERS*)
//
// if Oc_Option AsyncExceptionHandling is either POSIX or SEH, or
//
//   Oc_AsyncError::NoCatchSEH(unsigned int,EXCEPTION_POINTERS*)
//
// if Oc_Option AsyncExceptionHandling is NONE. As with the std:signal()
// handler, _set_se_translator must be set separately in each execution
// thread.
//
// As mentioned above, asynchronous handlers, whether invoked via SEH or
// some signal, are suppose to be spartan. This code observes that but
// instead places complexity inside quick_exit handlers. The docs don't
// explicitly state what limitations are inherent in quick_exit
// handlers, and some online comments claim they inherit restrictions
// from their caller; i.e., if called from a signal handler then signal
// handler restrictions apply inside the quick_exit handler too. Testing
// of this code on Linux/x86-64 and Windows 10 has not revealed any
// problems; these are our major platforms so we'll assume we're OK
// unless evidence to the contrary arises. If a problem comes up a
// fallback is to disable the quick_exit code with
//
//  $config SetValue program_compiler_c++_async_exception_handling none
//
// in the platform/<platform>.tcl files. That is the current setting
// on macOS, because at this time (Apr 2022) macOS doesn't implement
// quick_exit --- possibly because of weaknesses in the quick_exit
// specification.
//
// One last comment on quick_exit handlers. These are intended to be
// chained, LIFO, with support for a stack at least 32 entries deep. In
// particular, each exit handler is suppose to return so subsequent
// handlers can have a turn. However, in addition to logging the
// captured error, we want a core dump for debugging. So our exit
// handlers call std::abort() directly, which short-circuits this
// mechanism. Again, if problems manifest a workaround is to set
// program_compiler_c++_async_exception_handling to none.
//
// There are three quick_exit handlers in play,
//
//    Oc_AsyncError::HandleSignal()
//    Oc_AsyncError::SignalSEH()
//    Oc_AsyncError::HandleSEH()
//
// with selection depending on the values of the compile-time macro
// OC_ASYNC_EXCEPTION_HANDLING and Oc_Option AsyncExceptionHandling
// according to the following table:
//
//   ------------- quick_exit() handler --------------
//   Compile-time |       Run-time Oc_Option
//     platform(*)|  none  |   POSIX      |    SEH
//   -------------+--------+--------------+-----------
//      POSIX     |  none  | HandleSignal |   N/A(**)
//       SEH      |  none  | SignalSEH    | HandleSEH
//   -------------------------------------------------
//
// (*) For the compile-time platform setting "none", refer to the
// OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_NONE code block.
//
// (**) Selecting run-type option SEH with compile-time option POSIX
// issues a warning and falls back to POSIX handling.
//
// (***) As of Apr-2022 macOS does not support std::quick_exit(), so
// only OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_NONE is
// implemented.
//
// Side note: In the Windows SEH framework, signals raised by
// std::raise() or std::abort() are not treated as "asynchronous"
// signals, and are not caught by the registered _set_se_translator
// handler.  However, they are still signals and are caught by the
// registered std::signal() handler. This is why std:signal() handlers
// are registered in all exception-handling modes.

#if OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_NONE ////////////////

void Oc_AsyncError::RegisterHandler()
{ // In OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_NONE
  // code branch ::RegisterHandler() just issues a warning
  // if async exception handling was requested in Oc_Option.
  if(!Oc_IsGlobalInterpThread()) {
    // This routine should only be called from the thread
    // holding globalInterp.
    OC_THROWEXCEPT("Oc_AsyncError","RegisterHandler",
                   "Register request from child thread.");
  }
  std::string request = "none"; // Default setting
  Oc_GetOcOption("","AsyncExceptionHandling",request);
  for(auto it=request.begin(); it!= request.end(); ++it) {
    // Make lowercase to simplify comparison
    *it = static_cast<char>(std::tolower(*it));
  }
  if(request.compare("none")!=0) {
    std::clog << std::endl;
    Oc_Report::Log << Oc_LogSupport::GetLogMark()
                   << "WARNING: async exception handling not available ("
                   << request << " requested)."
                   << std::endl;
  }
}

#elif (OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_POSIX) \
  ||  (OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH) /////////////

namespace { // Unnamed namespace
  static volatile std::sig_atomic_t ocasyncerror_signal_status = 0;
  struct OcAsyncErrorSignalType {
    int signo;
    const char* sigdesc;
  };
  // For POSIX and SEH exception handling, a signal handler is
  // registered for each signal in OcAsyncErrorSignalTypeList:
  const std::vector<OcAsyncErrorSignalType> OcAsyncErrorSignalTypeList = {
# ifdef SIGABRT
    {SIGABRT, "Abort (SIGABRT)"},
# endif
# ifdef SIGBUS
    {SIGBUS, "Bus error (SIGBUS)"},
# endif
# ifdef SIGFPE
    {SIGFPE, "Floating point exception (SIGFPE)"},
# endif
# ifdef SIGILL
    {SIGILL, "Illegal instruction (SIGILL)"},
# endif
# ifdef SIGINT
    {SIGINT, "External interrupt (SIGINT)"},
# endif
# ifdef SIGQUIT
    {SIGQUIT, "Quit from keyboard (SIGQUIT)"},
# endif
# ifdef SIGSEGV
    {SIGSEGV, "Invalid memory reference (SIGSEGV)"},
# endif
# ifdef SIGSYS
    {SIGSYS, "Bad system call (SIGSYS)"},
# endif
# ifdef SIGTERM
    {SIGTERM, "Termination signal (SIGTERM)"},
# endif
    // Add more blocks as needed.
    {-1, "Unrecognized signal"} // Safety, also no trailing comma
    /// NB: This guarantees that Oc_ExceptSigInfoList always has at
    ///     least one element, so Oc_ExceptSigInfoList.size()-1>=0
    ///     and can be used as an unsigned int.
  };
  // Use std::vector so we can easily extract the size.
  // With C++17 we could use a C array,
  //    SigInfoListType SigInfoList[] = { ... }
  // and
  //    int count = std::size(SigInfoList)
  // to get the number of elements in SigInfoList, but prior to C++17 we
  // have to resort to something like
  //    int count = sizeof(SigInfoList)/sizeof(SigInfoListType);
  // Argh. And std::array doesn't help because you need to declare the
  // array size as a template parameter in the declaration.

# if OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH
  // Asynchronous error handling with Windows' structured
  // exception handling (Visual C++ /EHa compile flag).
  std::atomic<unsigned int> ocasyncerror_seh_errcode = 0;
  // Map some Windows error codes to POSIX signal values
  struct OcAsyncErrorWinSignalMap {
    unsigned int winerr;
    int signo;
  };
  // If code is built with Visual C++ (Windows) with /EHa option but
  // AsyncExceptionHandling in Oc_Option is set to POSIX, then the
  // following mapping is used to translate Windows error code to POSIX
  // codes before calling the POSIX signal handler HandleSignal()
  // (cf. Oc_AsyncError::SignalSEH()).
  const std::vector<OcAsyncErrorWinSignalMap> OcAsyncErrorWinSignalList = {
    // NB: Some of these are only approximate translations.
    // First two should be SIGBUS, but Windows doesn't #define SIGBUS
    {EXCEPTION_DATATYPE_MISALIGNMENT, SIGSEGV}, // should be SIGBUS
    {EXCEPTION_IN_PAGE_ERROR, SIGSEGV},         // should be SIGBUS
    {EXCEPTION_ACCESS_VIOLATION, SIGSEGV},
    {EXCEPTION_ARRAY_BOUNDS_EXCEEDED, SIGSEGV},
    {EXCEPTION_GUARD_PAGE, SIGSEGV},
    {EXCEPTION_INVALID_HANDLE, SIGSEGV},
    {EXCEPTION_STACK_OVERFLOW, SIGSEGV},
    {EXCEPTION_ILLEGAL_INSTRUCTION, SIGILL},
    {EXCEPTION_PRIV_INSTRUCTION, SIGILL},
    {EXCEPTION_FLT_DENORMAL_OPERAND, SIGFPE},
    {EXCEPTION_FLT_DIVIDE_BY_ZERO, SIGFPE},
    {EXCEPTION_FLT_INEXACT_RESULT, SIGFPE},
    {EXCEPTION_FLT_INVALID_OPERATION, SIGFPE},
    {EXCEPTION_FLT_OVERFLOW, SIGFPE},
    {EXCEPTION_FLT_STACK_CHECK, SIGFPE},
    {EXCEPTION_FLT_UNDERFLOW, SIGFPE},
    // Add more blocks as needed.
    {0, -1} // Safety, also no trailing comma
  };
# endif // OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH
} // Unnamed namespace close

volatile std::atomic<int>
Oc_AsyncError::handler_type(OC_ASYNC_EXCEPT_NOT_SET);

void Oc_AsyncError::CatchSignal(int signalNumber)
{ // By spec, signal handlers are suppose to be very minimalistic. There
  // are only a handful of library functions that are allowed (the list
  // includes std::abort, std::_Exit std::quick_exit, and std::signal),
  // heap allocation is not safe, and in general the only static storage
  // duration objects that can be safely accessed are those of type
  // std::atomic and volatile std::sig_atomic_t. So, in this routine we
  // merely save the signal number in oc_signal_status and call
  // quick_exit(). The quick_exit() handler can then read
  // oc_signal_status and respond with more complex handling.
  std::signal(signalNumber,SIG_DFL);
  ocasyncerror_signal_status = signalNumber;
  std::quick_exit(99);
}

void Oc_AsyncError::HandleSignal() {
  int errcode = ocasyncerror_signal_status;
  if(errcode == 0) return; // errcode not set
  const size_t list_size = OcAsyncErrorSignalTypeList.size();
  const char* errdesc = OcAsyncErrorSignalTypeList[list_size-1].sigdesc;
  for(size_t i=0;i<list_size-1;++i) {
    if(static_cast<int>(errcode) == OcAsyncErrorSignalTypeList[i].signo) {
      errdesc = OcAsyncErrorSignalTypeList[i].sigdesc;
      break;
    }
  }
  std::clog << std::endl; // clog probably shares output with cout and
                         /// cerr, so move clog to a clean line.
  Oc_Report::Log << Oc_LogSupport::GetLogMark()
                 << "FATAL ERROR: " << errdesc << std::endl;
  /// Oc_Report::Log writes to both std::clog and also any disk file(s)
  /// registered via Oc_Report::Log.AddFile().
# ifdef SIGABRT
  std::signal(SIGABRT,SIG_DFL); // std::abort() raises SIGABRT. We've
  /// already done our handling, so fall back to system default.
# endif
  std::abort();
}

# if OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH
void Oc_AsyncError::CatchSEH(unsigned int errcode,EXCEPTION_POINTERS*)
{ // _set_se_translator handler.
  // WARNING: _set_se_translator handlers are suppose to throw an
  //          exception, but we want to terminate instead.  This works
  //          in testing, but be forewarned that it may break in some
  //          circumstances or following compiler or OS changes.
  // Should we call MiniDumpWriteDump()?
  _set_se_translator(nullptr); // Docs say this is legal, but don't
  /// say what it does...

  ocasyncerror_seh_errcode = errcode;
  std::quick_exit(99);
}

void Oc_AsyncError::HandleSEH()
{ // std::quick_exit handler for SEH async error handling
  const unsigned int errcode = ocasyncerror_seh_errcode;
  if(errcode == 0) {
    // Fall back to signal handling. HandleSignal() returns iff
    // ocasyncerror_signal_status is also zero, in which case
    // nothing to handle so return.
    Oc_AsyncError::HandleSignal();
    return;
  }
  DWORD msgid = RtlNtStatusToDosError(errcode); // Requires ntdll.lib at
  std::string errdesc = Oc_WinStrError(msgid); /// link time.
  std::clog << std::endl; // clog probably shares output with cout and
                         /// cerr, so move clog to a clean line.
  Oc_Report::Log << Oc_LogSupport::GetLogMark()
                 << "FATAL ERROR: " << errdesc << std::endl;
  /// Oc_Report::Log writes to both std::clog and also any disk file(s)
  /// registered via Oc_Report::Log.AddFile().
# ifdef SIGABRT
  std::signal(SIGABRT,SIG_DFL); // std::abort() raises SIGABRT. We've
  /// already done our handling, so fall back to system default.
# endif
  std::abort();
}

void Oc_AsyncError::SignalSEH()
{ // std::quick_exit handler implementing POSIX signal style handling
  // with code compiled using the Visual C++ /EHa compiler option.
  const unsigned int winerrcode = ocasyncerror_seh_errcode;
  if(winerrcode != 0) {
    // Convert Windows error code to approximate POSIX signal
    const size_t map_size = OcAsyncErrorWinSignalList.size();
    int errcode = OcAsyncErrorWinSignalList[map_size-1].signo;
    for(size_t i=0;i<map_size-1;++i) {
      if(winerrcode == OcAsyncErrorWinSignalList[i].winerr) {
        errcode = OcAsyncErrorWinSignalList[i].signo;
        break;
      }
    }
    ocasyncerror_signal_status = errcode;
  }
  // Transfer control to standard signal handler.
  HandleSignal();
}

void Oc_AsyncError::NoCatchSEH(unsigned int,EXCEPTION_POINTERS*)
{ // _set_se_translator handler implementing no-catch behavior for code
  // compiled using the Visual C++ /EHa compiler option.
  _set_se_translator(nullptr); // Docs say this is legal, but don't
  /// say what it does...
  std::abort();
}
# endif // OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH

void Oc_AsyncError::RegisterHandler()
{ // Don't call this until after the Oc_InitScript has been run
  // so that the Oc_Option database is set up.
  if(!Oc_IsGlobalInterpThread()) {
    // This routine should only be called from the thread holding
    // globalInterp. Child threads should call RegisterThreadHandler(),
    // but only *after* this routine has been called and completed.
    // (RegisterThreadHandler() is a nop except on Windows.)
    OC_THROWEXCEPT("Oc_AsyncError","RegisterHandler",
                   "Register request from child thread.");
  }
  if(handler_type != OC_ASYNC_EXCEPT_NOT_SET) {
    std::string xr;
    Oc_GetOcOption("","AsyncExceptionHandling",xr);
    std::string msg = std::string("Handler previously set; old val=")
      + std::to_string(handler_type) + std::string(", new val=")
      + xr + std::string("; pid=") + std::to_string(Oc_GetPid());
    OC_THROWEXCEPT("Oc_AsyncError","RegisterHandler",msg.c_str());
  }
  std::string request = "POSIX"; // Default setting
  Oc_GetOcOption("","AsyncExceptionHandling",request);
  for(auto it=request.begin(); it!= request.end(); ++it) {
    // Make lowercase to simplify comparison
    *it = static_cast<char>(std::tolower(*it));
  }

  if(request.compare("none")==0) {
    handler_type = OC_ASYNC_EXCEPT_NONE;
# if OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH
    // Register abort se_translator so that asychronous exceptions abort
    // immediately rather than be transformed into C++ synchronous
    // exceptions that unwind the call stack.
    _set_se_translator(Oc_AsyncError::NoCatchSEH);
# endif // OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH
  } else {
    if(request.compare("posix")!=0 && request.compare("seh")!=0) {
      std::string errmsg =
        std::string("Unsupported asynchronous error handling request: ")
        + std::string(request);
      OC_THROWEXCEPT("Oc_AsyncError","RegisterHandler",errmsg.c_str());
    }

    // std::abort() and signals triggered by std::raise(int signo) are
    // not caught by /EHa handling, so POSIX signal handling needs to be
    // set up for both POSIX and SEH handling modes.
    for(size_t i=0;i<OcAsyncErrorSignalTypeList.size()-1;++i) {
      std::signal(OcAsyncErrorSignalTypeList[i].signo,
                  Oc_AsyncError::CatchSignal);
    }

# if OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH
    // Visual C++ compiler option /EHa in effect. If _set_se_translator
    // is not called then the default unwinds the execution stack with a
    // synchronous C++ exception that can only be caught via
    // try/catch(...). We want to preserve the call stack in all
    // variants, so we put in place a handler (CatchSEH) that calls
    // std::quick_exit, and vary behavior by appropriate selection of
    // routine registered with std::at_quick_exit(), namely HandleSEH
    // for request == SEH, and SignalSEH for request == POSIX.
    //   Note that signal handling is set above.
    _set_se_translator(Oc_AsyncError::CatchSEH); // SEH translator
    if(request.compare("seh")==0) {
      if (std::at_quick_exit(Oc_AsyncError::HandleSEH)) {
        OC_THROWEXCEPT("Oc_AsyncError","RegisterHandler",
                       "Oc_AsyncError SEH handler registration failed");
      }
      handler_type = OC_ASYNC_EXCEPT_SEH;
    } else { //  request == "posix"
      if (std::at_quick_exit(Oc_AsyncError::SignalSEH)) {
        OC_THROWEXCEPT("Oc_AsyncError","RegisterHandler",
                       "Oc_AsyncError SEH/POSIX handler registration failed");
      }
      handler_type = OC_ASYNC_EXCEPT_POSIX;
    }
# else // OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_POSIX
    // Set quick_exit handler to HandleSignal. Signal handling already
    // set.
    if(request.compare("seh")==0) {
      std::clog << std::endl;
      Oc_Report::Log << Oc_LogSupport::GetLogMark()
                     << "WARNING: SEH async exception handling not"
                        " available; using POSIX handling instead."
                     << std::endl;
    }
    if (std::at_quick_exit(Oc_AsyncError::HandleSignal)) {
      OC_THROWEXCEPT("Oc_AsyncError","RegisterHandler",
                     "Oc_AsyncError POSIX handler registration failed");
    }
    handler_type = OC_ASYNC_EXCEPT_POSIX;
# endif //  OC_ASYNC_EXCEPTION_HANDLING
  } // if(request.compare("none")==0)/else
  assert(handler_type != OC_ASYNC_EXCEPT_NOT_SET);
} // void Oc_AsyncError::RegisterHandler()

# if OC_SYSTEM_TYPE == OC_WINDOWS
void Oc_AsyncError::RegisterThreadHandler()
{ // This function is defined as a nop on non-Windows platforms.  Signal
  // and _set_se_translator handlers need to be set in each thread on
  // Windows, but std::at_quick_exit handler registration apparently
  // holds across all threads.
  const int use_handler = handler_type;
  if(use_handler == OC_ASYNC_EXCEPT_NOT_SET) {
    std::string errmsg =
      std::string("Sequencing error; RegisterThreadHandler()"
                  " called before RegisterHandler() exit.");
    OC_THROWEXCEPT("Oc_AsyncError","RegisterThreadHandler",errmsg.c_str());
  }
  if(use_handler == OC_ASYNC_EXCEPT_NONE) {
#  if OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH
    _set_se_translator(Oc_AsyncError::NoCatchSEH);
#  endif
  } else {
    // Signal handler needed for both POSIX and SEH handling modes
    for(size_t i=0;i<OcAsyncErrorSignalTypeList.size()-1;++i) {
      std::signal(OcAsyncErrorSignalTypeList[i].signo,
                  Oc_AsyncError::CatchSignal);
    }
#  if OC_ASYNC_EXCEPTION_HANDLING == OC_ASYNC_EXCEPT_SEH
    _set_se_translator(Oc_AsyncError::CatchSEH); // SEH translator
#  endif
  }
} // Oc_AsyncError::RegisterThreadHandler()
# endif // OC_SYSTEM_TYPE == OC_WINDOWS

#else  // OC_ASYNC_EXCEPTION_HANDLING == _POSIX or _SEH ////////////////
# error Invalid OC_ASYNC_EXCEPTION_HANDLING; check pkg/oc/<platform>/ocport.h
#endif // OC_ASYNC_EXCEPTION_HANDLING == ??? ///////////////////////////

////////////////////////////////////////////////////////////////////////
