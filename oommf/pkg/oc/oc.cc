/* FILE: oc.cc                   -*-Mode: c++-*-
 *
 *      The OOMMF Core extension.
 *
 *      This extension provides a set of C++ classes and functions and
 * Tcl commands of general utility to all OOMMF applications.  This
 * includes portability support for managing the differences among
 * computing platforms, compilers, and Tcl/Tk library versions.
 *
 * Because there are platform and compiler differences in which C++ function
 * serves as the main entry point to an application, and what arguments
 * it receives from the operating system, those entry points are provided
 * as part of this extension, so the portability burden is borne here and
 * is not passed to application writers.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/10/13 19:43:40 $
 * Last modified by: $Author: donahue $
 */

/* Header files for standard libraries */
#include <cassert>
#include <cerrno>
#include <cstring>
#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <exception>
#include <iostream>
#include <list>
#include <string>
#include <thread>
#include <vector>

/* Header file for this extension */
#include "autobuf.h"
#include "oc.h"
#include "octhread.h"

/* Implementation-defined limits (ANSI C) */
#include <climits>
#include <cfloat>

#if defined(OC_SET_TASKBAR_ID) && OC_SET_TASKBAR_ID
# include <Shobjidl.h>  // For SetCurrentProcessExplicitAppUserModelId
#endif

#if defined(OC_SET_MACOSX_APPNAME) && OC_SET_MACOSX_APPNAME
# include <CoreFoundation/CoreFoundation.h>
#endif

#include <list>

// Read <algorithm> last, because with some pgc++ installs the
// <emmintrin.h> header is not interpreted properly if <algorithm> is
// read first.
#include <algorithm>

OC_USE_STD_NAMESPACE;
OC_USE_EXCEPTION;
/* End includes */

/*
 * Function to return cache size, in bytes.
 * Currently hardcoded to return 16KB for
 * L1 cache, 1MB for L2 cache, 0 for L3 cache.
 */
size_t Oc_CacheSize(int level)
{
  size_t csize = 0;
  switch(level)
    {
    case 0: csize = 1024 * 1024;  break;   // Biggest cache value.
    case 1: csize =   16 * 1024;  break;   // L1
    case 2: csize = 1024 * 1024;  break;   // L2
    default: csize = 0; break;             // L3 or higher
    }
  return csize;
}

/*
 * The global interpreter of the application.
 */
namespace { // Unnamed namespace
  Tcl_Interp *globalInterp = (Tcl_Interp *)nullptr;
  std::atomic<std::thread::id> globalInterpThread;
}
OC_BOOL Oc_IsGlobalInterpThread() {
  return (globalInterpThread == std::this_thread::get_id());
}

/*
 * Do we load the Tk extension? (Default = yes)
 */
static OC_BOOL use_tk = 1;

/*
 * Do we parse the command line?
 */
static OC_BOOL parseCommandLine = 0;

/*
 * The following static strings contain Tcl scripts which are evaluated
 * to provide a small library of routines needed to accomplish the task
 * of finding the initialization Tcl scripts for the Oc extension.
 *
 * NOTE: There must be several separate strings because some compilers
 * place a rather small limit on the maximum length of a string literal.
 */

// Fix for bug in Tk_Init in Tk 4.2
static char patchScript0[] =
"if {[string match 7.6 $tcl_version] && ![info exists tk_library]} {\n\
   set tk_library [file join [file dirname $tcl_library] tk4.2]\n\
}";

// NOMAC: Assumes '..' means 'parent directory'
static char initScript0[] =
"proc Oc_DirectPathname { pathname } {\n\
    global Oc_DirectPathnameCache\n\
    set canCdTo [file dirname $pathname]\n\
    set rest [file tail $pathname]\n\
    switch -exact -- $rest {. - .. {\n\
        set canCdTo [file join $canCdTo $rest]\n\
        set $rest {}\n\
    }}\n\
    if {[string match absolute [file pathtype $canCdTo]]} {\n\
        set index $canCdTo\n\
    } else {\n\
        set index [file join [pwd] $canCdTo]\n\
    }\n\
    if {[info exists Oc_DirectPathnameCache($index)]} {\n\
        return [file join $Oc_DirectPathnameCache($index) $rest]\n\
    }\n\
    if {[catch {set savedir [pwd]} msg]} {\n\
        return -code error \"Can't determine pathname for\n\
\t$pathname:\n\t$msg\"\n\
    }\n\
    while {[catch {cd $canCdTo}]} {\n\
        switch -exact -- [file tail $canCdTo] {\n\
            {} {set Oc_DirectPathnameCache($index) $canCdTo\n\
                return [file join $Oc_DirectPathnameCache($index) $rest]}\n\
            . {} .. {\n\
                set Oc_DirectPathnameCache($index) [file dirname \\\n\
                        [Oc_DirectPathname [file dirname $canCdTo]]]\n\
                return [file join $Oc_DirectPathnameCache($index) $rest]}\n\
            default {set rest [file join [file tail $canCdTo] $rest]}\n\
        }\n\
        set canCdTo [file dirname $canCdTo]\n\
        set index [file dirname $index]\n\
    }\n\
    catch {set Oc_DirectPathnameCache($index) [pwd]}\n\
    cd $savedir\n\
    if {![info exists Oc_DirectPathnameCache($index)]} {\n\
        set Oc_DirectPathnameCache($index) [Oc_DirectPathname $canCdTo]\n\
    }\n\
    return [file join $Oc_DirectPathnameCache($index) $rest]\n\
}";

static char initScript1[] =
"proc Oc_AppendUniqueReadableDirectory {listref dir} {\n\
    upvar $listref list\n\
    if {[file isdirectory $dir] && [file readable $dir]} {\n\
        set dir [Oc_DirectPathname $dir]\n\
        if {[lsearch -exact $list $dir] < 0} {\n\
            lappend list $dir\n\
        }\n\
    }\n\
}";

static char initScript2[] =
"proc Oc_ScriptPath {Pkg ver} {\n\
    global env \n\
    set dirs {}\n\
    set envVar [string toupper $Pkg]_LIBRARY\n\
    if [info exists env($envVar)] {\n\
        Oc_AppendUniqueReadableDirectory dirs $env($envVar)\n\
    }\n\
    set pkg [string tolower $Pkg]\n\
    set tclVar ${pkg}_library\n\
    if [info exists $tclVar] {\n\
        Oc_AppendUniqueReadableDirectory dirs [set $tclVar]\n\
    }\n\
    set execName [Oc_DirectPathname [info nameofexecutable]] \n\
    set parDir [file dirname [file dirname $execName]]\n\
    set ggparDir [file dirname [file dirname $parDir]]\n\
    Oc_AppendUniqueReadableDirectory dirs \\\n\
            [file join $ggparDir pkg $pkg$ver]\n\
    Oc_AppendUniqueReadableDirectory dirs \\\n\
            [file join $ggparDir pkg $pkg]\n\
    Oc_AppendUniqueReadableDirectory dirs $parDir\n\
    Oc_AppendUniqueReadableDirectory dirs [file join $parDir base]\n\
    return $dirs\n\
 }";

static char initScript3[] =
"proc Oc_InitScript {Pkg ver} {\n\
    set pkg [string tolower $Pkg]\n\
    global ${pkg}_library\n\
    set dirs [Oc_ScriptPath $Pkg $ver]\n\
    foreach i $dirs {\n\
        set ${pkg}_library $i\n\
        set script [file join $i $pkg.tcl]\n\
        if {[file isfile $script] && [file readable $script]} {\n\
            uplevel #0 [list source $script]\n\
            return\n\
        }\n\
    }\n\
    set msg \"\nCan't find the '$Pkg' initialization script, $pkg.tcl,\"\n\
    append msg \"\nin the following directories:\n\n\"\n\
    append msg \"[join $dirs {\n}]\n\n\"\n\
    append msg \"$Pkg probably hasn't been \"\n\
    append msg \"installed properly.\n\n\"\n\
    append msg \"Either (re-)install $Pkg, \"\n\
    append msg \"or set the environment variable \"\n\
    append msg \"[string toupper $Pkg]_LIBRARY\nto the name \"\n\
    append msg \"of a directory containing $pkg.tcl.\"\n\
    return -code error $msg\n\
}";

#if OC_TCL_TYPE==OC_WINDOWS && defined(__CYGWIN__)
#if 0
static char initScript_cygwin[] =
"rename cd orig_win_cd\n\
proc cd {path} {\n\
   orig_win_cd $path\n\
   OcCygwinChDir $path\n\
}";
#else
static char initScript_cygwin[] =
"rename cd orig_win_cd\n\
proc cd {path} {\n\
   OcCygwinChDir $path\n\
}";
#endif
#endif // OC_WINDOWS && __CYGWIN__

int
Oc_InitScript(Tcl_Interp *interp, const char *pkg, const char *version)
{
    Tcl_DString buf;
    int code;

    Tcl_DStringInit(&buf);
    Tcl_DStringAppendElement(&buf, OC_CONST84_CHAR("Oc_InitScript"));
    Tcl_DStringAppendElement(&buf, OC_CONST84_CHAR(pkg));
    Tcl_DStringAppendElement(&buf, OC_CONST84_CHAR(version));
    code = Tcl_GlobalEval(interp, Tcl_DStringValue(&buf));
    Tcl_DStringFree(&buf);
    return code;
}

int
Oc_RegisterCommand(Tcl_Interp* interp, const char* name,
                   Tcl_CmdProc* cmd)
{
  if(Tcl_CreateCommand(interp,OC_CONST84_CHAR(name),cmd,
		       NULL,NULL)==NULL) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "Unable to register command ->",
                     name, "<- with Tcl interpreter", (char *) NULL);
    return TCL_ERROR;
  }
  return TCL_OK;
}

#if TCL_MAJOR_VERSION >= 8  // Obj interface available
int
Oc_RegisterObjCommand(Tcl_Interp* interp, const char* name,
                      Tcl_ObjCmdProc* cmd)
{
  if(Tcl_CreateObjCommand(interp,OC_CONST84_CHAR(name),cmd,
                          NULL,NULL)==NULL) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "Unable to register command ->",
                     name, "<- with Tcl interpreter", (char *) NULL);
    return TCL_ERROR;
  }
  return TCL_OK;
}
#endif

// C++ interface to the Oc_DirectPathname Tcl command.  NB: This uses
// the global Tcl interp, so it can only be called from the main thread.
void Oc_DirectPathname(const char *nickname,Oc_AutoBuf& fullname)
{
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL) {
    OC_THROW("No Tcl interpreter available (Oc_DirectPathname)");
  }

  // Construct a command to call Oc_DirectPathname,
  // using Tcl_Merge to protect spaces in nickname.
  int argc = 2;
  Oc_AutoBuf mybuf[2];
  char* argv[2];
  mybuf[0].Dup("Oc_DirectPathname");
  mybuf[1].Dup(nickname);
  argv[0]=mybuf[0].GetStr();
  argv[1]=mybuf[1].GetStr();

  char* merged_string = Tcl_Merge(argc,argv);
  Oc_AutoBuf cmd = merged_string; // Copy list into autobuf
  Tcl_Free(merged_string); // Free memory allocated inside Tcl_Merge()

  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  if(Tcl_Eval(interp,cmd.GetStr()) != TCL_OK) {
    Oc_AutoBuf msg;
    const char* errfmt = "Error in Oc_DirectPathname:\n"
      "Command: %s\nError: %s";
    msg.SetLength(strlen(errfmt)+strlen(cmd.GetStr())
                  +strlen(Tcl_GetStringResult(interp)));
    Oc_Snprintf(msg.GetStr(),msg.GetLength(),
                errfmt,cmd.GetStr(),Tcl_GetStringResult(interp));
    Tcl_DiscardResult(&saved);
    OC_THROW(msg); // Don't throw msg.GetStr(), because msg gets
    /// deleted out-of-scope and the pointer to its character string
    /// becomes invalid.
  }
  fullname.Dup(Tcl_GetStringResult(interp));
  Tcl_RestoreResult(interp, &saved);
}

// C++ interface to 'file' Tcl command.
// Example: Oc_TclFileCmd("dirname",fullname,dirname);
void Oc_TclFileCmd(const char* subcmd,const char* filename,Oc_AutoBuf& result)
{
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL) {
    OC_THROW("No Tcl interpreter available (Oc_TclFileCmd)");
  }

  // Construct a command to call file dirname,
  // using Tcl_Merge to protect spaces in filename.
  const int argc = 3;
  Oc_AutoBuf mybuf[3];
  char* argv[3];
  mybuf[0].Dup("file");
  mybuf[1].Dup(subcmd);
  mybuf[2].Dup(filename);
  argv[0]=mybuf[0].GetStr();
  argv[1]=mybuf[1].GetStr();
  argv[2]=mybuf[2].GetStr();

  char* merged_string = Tcl_Merge(argc,argv);
  Oc_AutoBuf cmd = merged_string; // Copy list into autobuf
  Tcl_Free(merged_string); // Free memory allocated inside Tcl_Merge()

  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  if(Tcl_Eval(interp,cmd.GetStr()) != TCL_OK) {
    Oc_AutoBuf msg;
    const char* errfmt = "Error in Oc_TclFileCmd:\n"
      "Command: %s\nError: %s";
    msg.SetLength(strlen(errfmt)+strlen(cmd.GetStr())
                  +strlen(Tcl_GetStringResult(interp)));
    Oc_Snprintf(msg.GetStr(),msg.GetLength(),
                errfmt,cmd.GetStr(),Tcl_GetStringResult(interp));
    Tcl_DiscardResult(&saved);
    OC_THROW(msg); // Don't throw msg.GetStr(), because msg gets
    /// deleted out-of-scope and the pointer to its character string
    /// becomes invalid.
  }
  result.Dup(Tcl_GetStringResult(interp));
  Tcl_RestoreResult(interp, &saved);
}


/***********************************************************************
 * Signal handlers
 *
 * The Unix signal set is
 *
 *  1) SIGHUP       2) SIGINT       3) SIGQUIT      4) SIGILL
 *  5) SIGTRAP      6) SIGIOT       7) SIGEMT       8) SIGFPE
 *  9) SIGKILL     10) SIGBUS      11) SIGSEGV     12) SIGSYS
 * 13) SIGPIPE     14) SIGALRM     15) SIGTERM     16) SIGURG
 * 17) SIGSTOP     18) SIGTSTP     19) SIGCONT     20) SIGCHLD
 * 21) SIGTTIN     22) SIGTTOU     23) SIGIO       24) SIGXCPU
 * 25) SIGXFSZ     26) SIGVTALRM   27) SIGPROF     28) SIGWINCH
 * 29) SIGPWR      30) SIGUSR1     31) SIGUSR2
 *
 * The Windows signal set is
 *
 *  2) SIGINT       4) SIGILL       8) SIGFPE      11) SIGSEGV
 * 15) SIGTERM     21) SIGBREAK    22) SIGABRT
 *
 * Note 1) The first five Windows signals match the Unix assignments.
 *         SIGBREAK and SIGABRT are not in the Unix list, but are
 *         assigned the values of SIGTTIN and SIGTTOU, respectively.
 * Note 2) The SIGBREAK macro is defined in the current (Jan 2006)
 *         Windows signal.h header file, with a comment indicating
 *         that it corresponds to the Ctrl-Break sequence.  However,
 *         SIGBREAK is not mentioned in the Microsoft VC++ 8
 *         documentation for signal().
 *
 * Observed behavior on Windows 7 (June 2015): Ctrl-C sends a SIGINT,
 *         Ctrl-Break sends a SIGBREAK.  Closing the console window
 *         sends a SIGBREAK, and then five seconds later the
 *         application is forcibly closed.  If a signal is raised
 *         twice, the second instance will not be handled unless the
 *         handler is reset between the two occurrences.
 *
 *         Alternatively, on Windows the SetConsoleCtrlHandler()
 *         routine can be used to set handlers for Ctrl-C, Ctrl-Break,
 *         and window close messages.  As the DWORD import to the
 *         handler, Ctrl-C generates a ctrltype value of 0, Ctrl-Break
 *         generates a 1, and closing the console window generates a
 *         2.  As in the signal() case, 5 seconds after sending a
 *         close message the application is forcibly closed.  Handlers
 *         set via SetConsoleCtrlHandler() remain in place between
 *         messages (so two successive messages will call the handler
 *         twice).  However, handlers have a BOOL return type that
 *         affects behavior.  If the return value is TRUE then the
 *         message is treated as handled and processing for that
 *         message stops.  If the return value is FALSE then the next
 *         handler is called --- multiple calls to
 *         SetConsoleCtrlHandler() places the handlers into a FILO
 *         stack.
 *
 *         With either method, the handler is run in a separate
 *         thread.  If both signal() and SetConsoleCtrlHandler() are
 *         used, then the signal handlers are called.
 */

extern "C" void DisableStdio(int);
void DisableStdio(int)
{ // Redirects stdio channels to NULL
  // Get name of NULL device
  Tcl_Eval(globalInterp,
     OC_CONST84_CHAR("[Oc_Config RunPlatform] GetValue path_device_null"));
  Oc_AutoBuf nulin(Tcl_GetStringResult(globalInterp));
#ifndef DEBUG_DISABLESTDIO
  Oc_AutoBuf nulout(Tcl_GetStringResult(globalInterp));
#else
  char buf[512];
  sprintf(buf,"/tmp/dummynul-%d",Oc_GetPid());
  Oc_AutoBuf nulout(buf);
#endif // DEBUG_DISABLESTDIO

  // Reset C FILE*'s
  if(!freopen((char*)nulin,"r",stdin) ||
     !freopen((char*)nulout,"a",stdout) ||
     !freopen((char*)nulout,"a",stderr)) {
    // Is it possible to successfully report an error here?
    // Give it a shot...
    fprintf(stderr,"ERROR: Unable to reset one or more of"
            " stdin/stdout/stderr\n");
    fprintf(stdout,"ERROR: Unable to reset one or more of"
            " stdin/stdout/stderr\n");
  }

#ifdef DEBUG_DISABLESTDIO
  setvbuf(stdout,NULL,_IONBF,0);
  setvbuf(stderr,NULL,_IONBF,0);
  fprintf(stderr,"nul opened by %d\n",Oc_GetPid());
#endif // DEBUG_DISABLESTDIO

  // Reset Tcl channels
  Tcl_Channel nulchanin =
    Tcl_OpenFileChannel(NULL,(char*)nulin,OC_CONST84_CHAR("r"),0644);
  Tcl_Channel nulchanout =
    Tcl_OpenFileChannel(NULL,(char*)nulout,OC_CONST84_CHAR("a+"),0644);
  Tcl_SetStdChannel(nulchanin,TCL_STDIN);
  Tcl_SetStdChannel(nulchanout,TCL_STDOUT);
  Tcl_SetStdChannel(nulchanout,TCL_STDERR);
  Tcl_RegisterChannel(globalInterp,nulchanin);
  Tcl_RegisterChannel(globalInterp,nulchanout);
#ifdef DEBUG_DISABLESTDIO
  Tcl_SetChannelOption(NULL,nulchanout,OC_CONST84_CHAR("-buffering"),
		       OC_CONST84_CHAR("none"));
#endif // DEBUG_DISABLESTDIO

  // Disable future SIGHUP, SIGTTIN and SIGTTOU signals
#ifdef SIGHUP
  signal(SIGHUP,SIG_IGN);
#endif
#ifdef SIGTTIN
  signal(SIGTTIN,SIG_IGN);
#endif
#ifdef SIGTTOU
  signal(SIGTTOU,SIG_IGN);
#endif
}

static int
IgnoreSignal(int signo)
{
  // Note: OOMMF records ca. 1998 indicate unconfirmed reports of
  // Windows crashing if signal() is sent a signo value outside the
  // Windows signal set.  This behavior is confirmed for Windows 2000
  // and Windows XP when OOMMF is built using Microsoft VC++ 8 (aka VC++
  // 2005); the signal() routine in the VC++ 8 runtime library does an
  // explicit parameter check and if signo is not one of the recognized
  // signal values, the "invalid parameter" handler is called.  The
  // default invalid parameter handler raises an "Access Violation"
  // exception, which will normally crash the process.  There is also
  // some indication in the 1998 records that signal handling in Windows
  // NT on the Alpha processor is somehow different; perhaps a larger
  // signal set was supported?  Whatever the issue, as of Jan 2006 it
  // has been lost in the mists of time.
  //   Normally this routine is called through the Oc_IgnoreSignal Tcl
  // wrapper, which only passes signo's that have defined macros.  The
  // DisableStdio() routine above also follows this convention.  We
  // assume that this convention will protect against the invalid
  // parameter issue describe in the last paragraph.

  int disable_stdio = 0;

#ifdef SIGHUP
  if(SIGHUP == signo) disable_stdio = 1;
#endif
#ifdef SIGTTIN
  if(SIGTTIN == signo) disable_stdio = 1;
#endif
#ifdef SIGTTOU
  if(SIGTTOU == signo) disable_stdio = 1;
#endif

  if(disable_stdio) {
#ifdef __GNUC__
    if( (void*)signal(signo,(omf_sighandler)DisableStdio) == (void*)SIG_ERR )
#else
    if( signal(signo,(omf_sighandler)DisableStdio) == SIG_ERR )
#endif
      return TCL_ERROR;
  } else {
#ifdef __GNUC__
    if( (void*)signal(signo,SIG_IGN) == (void*)SIG_ERR )
#else
    if( signal(signo,SIG_IGN) == SIG_ERR )
#endif
      return TCL_ERROR;
  }
  /// The (void*) casts above are to work around a gcc-2.7.2.2 (and
  /// others?) bug that shows up on platforms with a non-ANSI signal(),
  /// e.g., SunOS.  In that circumstance, the return value from signal()
  /// is treated as void (*)(...) instead of void (*)(), i.e.,
  /// void (*)(void).
  ///
  /// UPDATE: dgp: 1999 May 19: Some compilers, notably Sun Workshop 5.0
  /// refuse the cast to void *.  So the casting above is now limited to
  /// those compilers which define __GNUC__ (presumably only gcc, etc.)
  return TCL_OK;
}

static int
Oc_IgnoreSignal(ClientData, Tcl_Interp *interp, int argc, char **argv)
{
  Tcl_ResetResult(interp);
  if (argc != 2) {
    Tcl_AppendResult(interp, argv[0], " must be called with"
                     " 1 argument: signal", (char *) NULL);
    return TCL_ERROR;
  }

  int sigcode = -1;

#ifdef SIGHUP
  if(strcmp("SIGHUP",argv[1])==0) {
    sigcode = SIGHUP;
  }
#endif
#ifdef SIGINT
  if(strcmp("SIGINT",argv[1])==0) {
    sigcode = SIGINT;
  }
#endif
#ifdef SIGQUIT
  if(strcmp("SIGQUIT",argv[1])==0) {
    sigcode = SIGQUIT;
  }
#endif
#ifdef SIGILL
  if(strcmp("SIGILL",argv[1])==0) {
    sigcode = SIGILL;
  }
#endif
#ifdef SIGTRAP
  if(strcmp("SIGTRAP",argv[1])==0) {
    sigcode = SIGTRAP;
  }
#endif
#ifdef SIGIOT
  if(strcmp("SIGIOT",argv[1])==0) {
    sigcode = SIGIOT;
  }
#endif
#ifdef SIGEMT
  if(strcmp("SIGEMT",argv[1])==0) {
    sigcode = SIGEMT;
  }
#endif
#ifdef SIGFPE
  if(strcmp("SIGFPE",argv[1])==0) {
    sigcode = SIGFPE;
  }
#endif
#ifdef SIGKILL
  if(strcmp("SIGKILL",argv[1])==0) {
    sigcode = SIGKILL;
  }
#endif
#ifdef SIGBUS
  if(strcmp("SIGBUS",argv[1])==0) {
    sigcode = SIGBUS;
  }
#endif
#ifdef SIGSEGV
  if(strcmp("SIGSEGV",argv[1])==0) {
    sigcode = SIGSEGV;
  }
#endif
#ifdef SIGSYS
  if(strcmp("SIGSYS",argv[1])==0) {
    sigcode = SIGSYS;
  }
#endif
#ifdef SIGPIPE
  if(strcmp("SIGPIPE",argv[1])==0) {
    sigcode = SIGPIPE;
  }
#endif
#ifdef SIGALRM
  if(strcmp("SIGALRM",argv[1])==0) {
    sigcode = SIGALRM;
  }
#endif
#ifdef SIGTERM
  if(strcmp("SIGTERM",argv[1])==0) {
    sigcode = SIGTERM;
  }
#endif
#ifdef SIGURG
  if(strcmp("SIGURG",argv[1])==0) {
    sigcode = SIGURG;
  }
#endif
#ifdef SIGSTOP
  if(strcmp("SIGSTOP",argv[1])==0) {
    sigcode = SIGSTOP;
  }
#endif
#ifdef SIGTSTP
  if(strcmp("SIGTSTP",argv[1])==0) {
    sigcode = SIGTSTP;
  }
#endif
#ifdef SIGCONT
  if(strcmp("SIGCONT",argv[1])==0) {
    sigcode = SIGCONT;
  }
#endif
#ifdef SIGCHLD
  if(strcmp("SIGCHLD",argv[1])==0) {
    sigcode = SIGCHLD;
  }
#endif
#ifdef SIGTTIN
  if(strcmp("SIGTTIN",argv[1])==0) {
    sigcode = SIGTTIN;
  }
#endif
#ifdef SIGTTOU
  if(strcmp("SIGTTOU",argv[1])==0) {
    sigcode = SIGTTOU;
  }
#endif
#ifdef SIGIO
  if(strcmp("SIGIO",argv[1])==0) {
    sigcode = SIGIO;
  }
#endif
#ifdef SIGXCPU
  if(strcmp("SIGXCPU",argv[1])==0) {
    sigcode = SIGXCPU;
  }
#endif
#ifdef SIGXFSZ
  if(strcmp("SIGXFSZ",argv[1])==0) {
    sigcode = SIGXFSZ;
  }
#endif
#ifdef SIGVTALRM
  if(strcmp("SIGVTALRM",argv[1])==0) {
    sigcode = SIGVTALRM;
  }
#endif
#ifdef SIGPROF
  if(strcmp("SIGPROF",argv[1])==0) {
    sigcode = SIGPROF;
  }
#endif
#ifdef SIGWINCH
  if(strcmp("SIGWINCH",argv[1])==0) {
    sigcode = SIGWINCH;
  }
#endif
#ifdef SIGPWR
  if(strcmp("SIGPWR",argv[1])==0) {
    sigcode = SIGPWR;
  }
#endif
#ifdef SIGUSR1
  if(strcmp("SIGUSR1",argv[1])==0) {
    sigcode = SIGUSR1;
  }
#endif
#ifdef SIGUSR2
  if(strcmp("SIGUSR2",argv[1])==0) {
    sigcode = SIGUSR2;
  }
#endif

  // The following two are specific to Windows
#ifdef SIGBREAK
  if(strcmp("SIGBREAK",argv[1])==0) {
    sigcode = SIGBREAK;
  }
#endif
#ifdef SIGABRT
  if(strcmp("SIGABRT",argv[1])==0) {
    sigcode = SIGABRT;
  }
#endif

  if (sigcode == -1) {
    Tcl_AppendResult(interp, argv[0], " unrecognized signal: ",
                     argv[1], (char *) NULL);
    return TCL_ERROR;
  }

  return IgnoreSignal(sigcode);
}

// SIGTERM handlers.  Sets up a FILO stack of handlers for clean
// shutdown on reception of a SIGTERM signal on posix, or
// Ctrl-C/Ctrl-Break/Window close on Windows.  These routines use the
// cross-platform signal() call, as opposed to sigaction().  One of
// the knocks against signal() is that a handler that is registered is
// called once when a signal is raised; subsequent instances of that
// signal go to the default handler unless the registered handler
// re-registers itself inside the handler --- but even if the
// re-registration occurs first thing in the handler there is still a
// small window of time during which the handler is not registered, so
// if two instances of a signal are raised rapidly enough then the
// second will not be handled.  This should not be a major problem in
// this setting, however, because OcSigTermHandler is intended for
// shutdown clean-up, and therefore isn't designed for repeated calls
// anyway.
extern "C" { void OcSigTermHandler(int); }
extern "C" { typedef void ocsighandler_type(int); }
static ocsighandler_type *OcSavedTermHandler = SIG_DFL;
static std::list< pair<OcSigFunc*,ClientData> > sigterm_handlers;
static Oc_Mutex ocsigtermhandler_mutex;

void OcSigTermHandler(int signo)
{ // On unix, signo should be SIGTERM (from kill) or SIGINT (from
  // Ctrl-C); on Windows it may be SIGINT (from Ctrl-C), SIGBREAK (from
  // Ctrl-Break/Pause, or window close).
  Oc_LockGuard lck(ocsigtermhandler_mutex);
  try {
    // Loop through handlers, from back (last registered) to
    // front(first registered).
    for(std::list< pair<OcSigFunc*,ClientData> >::reverse_iterator rit
          = sigterm_handlers.rbegin();
        rit != sigterm_handlers.rend(); ++rit) {
      try {
        OcSigFunc* handler = rit->first;
        handler(signo,rit->second);
      } catch(...) {}
    }
  } catch(...) {
    // Note: OcSigTermHandler is extern "C" type, and shouldn't throw
    // exceptions.
  }
  // Note that OcSavedTermHandler may be SIG_IGN or SIG_DFL
  signal(signo,OcSavedTermHandler);
  raise(signo);
}

void Oc_AppendSigTermHandler(OcSigFunc* handler,ClientData cd)
{
  Oc_LockGuard lck(ocsigtermhandler_mutex);
  sigterm_handlers.push_back(pair<OcSigFunc*,ClientData>(handler,cd));
  if(sigterm_handlers.size()==1) {
#if (OC_SYSTEM_TYPE==OC_WINDOWS)
    OcSavedTermHandler = signal(SIGINT,OcSigTermHandler);
    signal(SIGBREAK,OcSigTermHandler); // Should we save SIGBREAK
    signal(SIGTERM,OcSigTermHandler);  // handler too?
#else
    OcSavedTermHandler = signal(SIGTERM,OcSigTermHandler);
    signal(SIGINT,OcSigTermHandler); // Save?
#endif
  }
}
void Oc_RemoveSigTermHandler(OcSigFunc* handler,ClientData cd)
{
  Oc_LockGuard lck(ocsigtermhandler_mutex);
  std::list< pair<OcSigFunc*,ClientData> >::iterator it
    = std::find(sigterm_handlers.begin(),
                sigterm_handlers.end(),
                pair<OcSigFunc*,ClientData>(handler,cd));
  if(it != sigterm_handlers.end()) {
    sigterm_handlers.erase(it);
    if(sigterm_handlers.size()==0) {
#if (OC_SYSTEM_TYPE==OC_WINDOWS)
      signal(SIGINT,OcSavedTermHandler);
#else
      signal(SIGTERM,OcSavedTermHandler);
#endif
      OcSavedTermHandler = SIG_DFL;
    }
  }
}

// Access to Oc_Option database in global interp. Returns 1 on success
// and stores the value in the export "value". If the value is not set
// in the the database then the return is 0 and the export "value" is
// unchanged. Throws an Oc_Exception on alloc or Tcl_Eval failure.
// Note 1: This routine accesses the Oc_Option database in globalInterp,
//         and so must only be called from the thread that set up
//         globalInterp (i.e., the main thread.)
// Note 2: The return value for Oc_GetOcOption is opposite that of
//         Oc_Option Get, which returns 0 on success and 1 on failure.
// Note 3: Throws on alloc failures don't release memory from successful
//         allocs.
OC_BOOL Oc_GetOcOption
(const std::string& classname,
 const std::string& option,
 std::string& value)
{
#define OGOOTHROW(errmsg) OC_THROWEXCEPT("","Oc_GetOcOption",errmsg)
  if(!Oc_IsGlobalInterpThread()) OGOOTHROW("Call from non-main thread.");
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp == nullptr) OGOOTHROW("Global Tcl interp not set");

  // Create a vector representing the Oc_Option Get command
  std::vector<Tcl_Obj*> cmdvec;
  cmdvec.push_back(Tcl_NewStringObj("Oc_Option",-1));
  cmdvec.push_back(Tcl_NewStringObj("GetValue",-1));
  cmdvec.push_back(Tcl_NewStringObj(classname.c_str(),int(classname.size())));
  cmdvec.push_back(Tcl_NewStringObj(option.c_str(),int(option.size())));
  for(auto it = cmdvec.begin(); it != cmdvec.end(); ++it) {
    if(*it == nullptr) OGOOTHROW("Tcl_NewStringObj fail");
  }
  // Convert cmdvec into a Tcl list
  Tcl_Obj* cmd = Tcl_NewListObj(int(cmdvec.size()),cmdvec.data());
  if(cmd == nullptr) OGOOTHROW("Tcl_NewListObj fail");
  Tcl_IncrRefCount(cmd);
  // Note: Tcl_NewListObj automatically increments the ref count on each
  // element. When the list is destroyed it decrements the ref count on
  // each element it still holds.

  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);
  const int tcl_result
    = Tcl_EvalObjEx(interp,cmd,TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
  if(tcl_result == TCL_OK) {
    value = std::string(Tcl_GetStringResult(interp));
  }
  Tcl_RestoreResult(interp, &saved);
  Tcl_DecrRefCount(cmd);
#undef OGOOTHROW
  return (tcl_result == TCL_OK);
}


// Multi-platform version of strerror_r.  Thread-safe.
void Oc_StrError(int errnum,char* buf,size_t buflen)
{
  if(buflen<1) {
    OC_THROW("Bad parameter value; buflen<1 (Oc_StrError)");
  }
#if defined(_MSC_VER) &&  (_MSC_VER >= 1400)
  // strerror_s is thread-safe routine in Visual C++
  if(strerror_s(buf,buflen,errnum)==0) return;
#else
  // Some platforms don't have strerror_r, and some that do (e.g.,
  // linux) provide multiple versions with different behavior overlaid
  // on the same signature (such as a glibc version and a posix
  // version).  So, for portability, and working on the assumption that
  // this is not performance critical code, fall back to C89 strerror.
  // Wrap with mutex locking since strerror is not guaranteed to be
  // thread-safe.
  static Oc_Mutex ocstrerror_mutex;
  try {
    Oc_LockGuard lck(ocstrerror_mutex);
    const char* cptr = strerror(errnum);
    size_t j=0;
    for(;j<buflen-1;++j) {
      if(cptr[j] == '\0') break;
      buf[j] = cptr[j];
    }
    buf[j] = '\0';
    return;
  } catch(...) {}
#endif // OC_SYSTEM_TYPE
 const char* unk = "Unknown error";
 size_t i=0;
 for(;i<buflen-1;++i) {
   if(unk[i] == '\0') break;
   buf[i] = unk[i];
 }
 buf[i] = '\0';
}

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
// Windows equivalent of strerror.
std::string Oc_WinStrError(DWORD errorID) {
  // errorID is usually obtained via GetLastError().
  HMODULE hMsgTable = LoadLibrary("NTDLL.DLL");
  WCHAR* errbuf = nullptr;
  DWORD msgsize = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER
                  | FORMAT_MESSAGE_FROM_SYSTEM
                  | FORMAT_MESSAGE_FROM_HMODULE
                  | FORMAT_MESSAGE_IGNORE_INSERTS,
                  hMsgTable, errorID,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPWSTR)(&errbuf), 0, nullptr);
  // The system allocated errbuf is somehow fragile?! AFAICT any access
  // beyond immediately copying it causes a crash. So, just copy and
  // release as fast as possible.
  Oc_AutoWideBuf errmsg(errbuf);
  LocalFree(&errbuf);
  FreeLibrary(hMsgTable);
  std::string message;
  if(msgsize) {
    errmsg.Trim();
    errmsg.NormalizeEOLs();
    message = errmsg.GetUtf8Str();
  } else {
    message = std::string("Unrecognized error code ")
      + std::to_string(errorID);
  }
  return message;
}

////////////////////////////////////////////////////////////////////////
// Windows security IDs (SIDs)

// Obtain SID string for the owner of a file. On failure, empty string
// is returned.
std::string Oc_WinGetFileSID(const std::string& filename)
{
  std::string result;
  Oc_TclObj filepath(filename.c_str());

  const void* nativepath = Tcl_FSGetNativePath(filepath.GetObj());
  PSID ownerSid = nullptr;
  PSECURITY_DESCRIPTOR secd = nullptr;
  LPSTR sidstr = nullptr; // Pointer to buffer
  if (GetNamedSecurityInfoW(static_cast<const WCHAR*>(nativepath),
                            SE_FILE_OBJECT,OWNER_SECURITY_INFORMATION,
                            &ownerSid,nullptr, nullptr, nullptr,
                            &secd) == ERROR_SUCCESS) {
      // Success; convert to string
    if(ConvertSidToStringSidA(ownerSid,&sidstr)) {
      result = sidstr; // ANSI string
    }
  }
  if(sidstr) LocalFree(sidstr);
  if(secd)  LocalFree(secd); // Note: ownerSid points into secd
  return result;
}

int OcWinGetFileSID // Tcl wrapper for preceding
(ClientData, Tcl_Interp *interp,
 int argc,CONST84 char **argv)
{
  Tcl_ResetResult(interp);
  if (argc != 2) {
    Tcl_AppendResult(interp, "Oc_WinGetFileSID must be called with"
                     " one argument: filename", nullptr);
    return TCL_ERROR;
  }
  std::string sidstr = Oc_WinGetFileSID(std::string(argv[1]));
  Tcl_AppendResult(interp,sidstr.c_str(),nullptr);
  return TCL_OK;
}

// Obtain SID string for current process. On failure, empty string is
// returned. Based on code from TclWinFileOwned (Tcl 8.6.10).
std::string Oc_WinGetCurrentProcessSID()
{
  std::string result;
  HANDLE token;
  if(OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)) {
    DWORD bufsz = 0;
    GetTokenInformation(token,TokenUser,nullptr,0,&bufsz);
    if(bufsz) {
      std::vector<unsigned char> buffer(bufsz,'\0');
      if(GetTokenInformation(token,TokenUser,buffer.data(),bufsz,&bufsz)) {
        PSID usersid = (reinterpret_cast<PTOKEN_USER>(buffer.data()))->User.Sid;
        LPSTR sidstr = nullptr; // Pointer to buffer
        if(ConvertSidToStringSidA(usersid,&sidstr)) {
          result = sidstr; // ANSI string
        }
        if(sidstr) LocalFree(sidstr);
      }
    }
    CloseHandle(token);
  }
  return result;
}

int OcWinGetCurrentProcessSID // Tcl wrapper for preceding
(ClientData, Tcl_Interp *interp,
 int argc,CONST84 char ** /* argv */)
{
  Tcl_ResetResult(interp);
  if (argc != 1) {
    Tcl_AppendResult(interp,
                     "Oc_WinGetCurrentProcessSID must be called with"
                     " no arguments", nullptr);
    return TCL_ERROR;
  }
  std::string sidstr = Oc_WinGetCurrentProcessSID();
  Tcl_AppendResult(interp,sidstr.c_str(),nullptr);
  return TCL_OK;
}

// Returns domain/account associated with a SID string.
// Return is an empty string on error.
std::string Oc_WinGetSIDAccountName
(const std::string& sidstr)
{
  std::string result;
  PSID psid = nullptr;
  DWORD namesz = 0;
  DWORD domainsz = 0;
  SID_NAME_USE Use;
  if(ConvertStringSidToSidA(sidstr.c_str(),&psid)) {
    // Get buffer sizes
    LookupAccountSidW(nullptr,psid,nullptr,&namesz,nullptr,&domainsz,&Use);
    if(namesz>0 && domainsz>0) {
      std::vector<WCHAR> namebuf(namesz,0);
      std::vector<WCHAR> domainbuf(domainsz,0);
      if(LookupAccountSidW(nullptr,psid,namebuf.data(),&namesz,
                           domainbuf.data(),&domainsz,&Use)) {
        result = Oc_AutoWideBuf(domainbuf.data()).GetUtf8Str()
          + std::string("/")
          + Oc_AutoWideBuf(namebuf.data()).GetUtf8Str();
      }
    }
    if(psid) LocalFree(psid);
  }
  return result;
}

int OcWinGetSIDAccountName // Tcl wrapper for preceding
(ClientData, Tcl_Interp *interp,
 int argc,CONST84 char **argv)
{
  Tcl_ResetResult(interp);
  if (argc != 2) {
    Tcl_AppendResult(interp, "Oc_WinGetSIDAccountName must be called with"
                     " one argument: SID string", nullptr);
    return TCL_ERROR;
  }
  std::string sidstr = Oc_WinGetSIDAccountName(std::string(argv[1]));
  Tcl_AppendResult(interp,sidstr.c_str(),nullptr);
  return TCL_OK;
}

#endif // OC_WINDOWS


static int
LockChannel(Tcl_Interp *interp,const char* channelName,int writespec)
{
  // Returns 0 on success, 1 if file already locked,
  // -1 on GetChannel error, -2 on GetChannelHandle error
  Tcl_Channel chan=Tcl_GetChannel(interp,OC_CONST84_CHAR(channelName),NULL);
  if(chan==NULL) return -1;
  int direction=TCL_WRITABLE;
  if(!writespec) direction=TCL_READABLE;

  ClientData cd;
  if(Tcl_GetChannelHandle(chan,direction,&cd)!=TCL_OK) {
    return -2;
  }

#if OC_SYSTEM_TYPE==OC_UNIX
  int handle = reinterpret_cast<OC_INDEX>(cd);
  struct flock lock;
  lock.l_type=F_WRLCK;
  if(!writespec) lock.l_type=F_RDLCK;
  lock.l_start=0;
  lock.l_whence=SEEK_SET;
  lock.l_len=0; // Lock entire file
  if(fcntl(handle,F_SETLK,&lock)==0) return 0; // Success!
#elif OC_SYSTEM_TYPE==OC_WINDOWS
  HANDLE osfhandle = reinterpret_cast<HANDLE>(cd);
  if(LockFile(osfhandle,0,0,1,0)) return 0; // Success!
#endif // OC_SYSTEM_TYPE

  return 1;  // Lock already held
}

static int
Oc_LockChannel(ClientData, Tcl_Interp *interp, int argc, char **argv)
{ // Warning: On Unix, locks are indexed by file+pid, so code like
  //          fd1 = open("myfile",...);
  //          LOCK(fd1);
  //          fd2 = open("myfile",...);
  //          LOCK(fd2);
  //          UNLOCK(fd2);
  //       unlocks fd1 as well.
  // WARNING: Closing a file releases all locks on the file, so
  //          fd1 = open("myfile","w");
  //          LOCK(fd1);
  //          fd2 = open("myfile","r");
  //          close(fd2);
  //       also unlocks fd1!!!  This is dangerous, and I don't see
  //       any way around it. :^(
  //          See "Advanced Programming in the Unix Environment,"
  //       W.R. Stevens, Addison-Wesley (1993) p373, for more
  //       details. -mjd 29-July-1999
  //
  //  The Windows locking mechanism behaves differently.  Only one write
  //  lock is permitted at any time on any file, and if one tries to
  //  re-lock an already locked file, the lock attempt will fail.  Also,
  //  unlocking a non-locked file fails.  Closing a file handle that was
  //  used to lock a file releases the lock, but closing a different
  //  file handle opened from the same file has no effect on the lock.
  //  NOTE: I could not determine a way to lock the entire file, so we
  //  just lock the first byte instead.  This should suffice for the
  //  cooperative locking scheme for which this routine is intended.
  //  -mjd, 30-July-1999

  Tcl_ResetResult(interp);
  if (argc != 3) {
    Tcl_AppendResult(interp, argv[0], " must be called with"
                     " 2 arguments: channel <r|w>", (char *) NULL);
    return TCL_ERROR;
  }
  const char* channelName=argv[1];
  int writespec=1;
  if(argv[2][0]=='r') writespec=0;
  int result=LockChannel(interp,channelName,writespec);
  if(result<0) {
    Tcl_AppendResult(interp,"LOCKING ERROR: ",(char *) NULL);
    switch(result)
      {
      case -1:
        Tcl_AppendResult(interp,"Bad channel name",(char *) NULL);
        break;
      case -2:
        Tcl_AppendResult(interp,"Bad read/write mode",(char *) NULL);
        break;
      default:
        Tcl_AppendResult(interp,"Bad lock",(char *) NULL);
        break;
      }
    return TCL_ERROR;
  }
  if(result==0) Tcl_AppendResult(interp,"1",(char *) NULL); // Success
  else          Tcl_AppendResult(interp,"0",(char *) NULL); // Failure
  return TCL_OK;
}

static int
UnlockChannel(Tcl_Interp *interp,const char* channelName,int writespec)
{
  // Returns 0 on success
  // -1 on GetChannel error, -2 on GetChannelHandle error
  // -3 on locking error
  Tcl_Channel chan=Tcl_GetChannel(interp,OC_CONST84_CHAR(channelName),NULL);
  if(chan==NULL) return -1;
  int direction=TCL_WRITABLE;
  if(!writespec) direction=TCL_READABLE;

  ClientData cd;
  if(Tcl_GetChannelHandle(chan,direction,&cd)!=TCL_OK) {
    return -2;
  }

#if OC_SYSTEM_TYPE==OC_UNIX
  int handle = reinterpret_cast<OC_INDEX>(cd);
  struct flock lock;
  lock.l_type=F_UNLCK;
  lock.l_start=0;
  lock.l_whence=SEEK_SET;
  lock.l_len=0; // Entire file
  if(fcntl(handle,F_SETLK,&lock)==0) return 0; // Success!
#elif OC_SYSTEM_TYPE
  HANDLE osfhandle = reinterpret_cast<HANDLE>(cd);
  if(UnlockFile(osfhandle,0,0,1,0)) return 0; // Success!
#endif // OC_SYSTEM_TYPE

  return -3; // Error; file probably not locked.
}

static int
Oc_UnlockChannel(ClientData, Tcl_Interp *interp, int argc, char **argv)
{
  Tcl_ResetResult(interp);
  if (argc != 3) {
    Tcl_AppendResult(interp, argv[0], " must be called with"
                     " 2 arguments: channel <r|w>", (char *) NULL);
    return TCL_ERROR;
  }
  const char* channelName=argv[1];
  int writespec=1;
  if(argv[2][0]=='r') writespec=0;
  int result=UnlockChannel(interp,channelName,writespec);
  if(result<0) {
    Tcl_AppendResult(interp,"LOCKING ERROR: ",(char *) NULL);
    switch(result)
      {
      case -1:
        Tcl_AppendResult(interp,"Bad channel name",(char *) NULL);
        break;
      case -2:
        Tcl_AppendResult(interp,"Bad read/write mode",(char *) NULL);
        break;
      default:
        Tcl_AppendResult(interp,"Bad lock",(char *) NULL);
        break;
      }
    return TCL_ERROR;
  }
  if(result==0) Tcl_AppendResult(interp,"1",(char *) NULL); // Success
  else          Tcl_AppendResult(interp,"0",(char *) NULL); // Failure
  return TCL_OK;
}

static void MakeNice()
{
#if (OC_SYSTEM_TYPE==OC_WINDOWS)
  // Set solver to run at idle priority
  SetPriorityClass(GetCurrentProcess(),NICE_DEFAULT);
  SetThreadPriority(GetCurrentThread(),NICE_THREAD_DEFAULT);
#else
  errno=0;
  if(nice(NICE_DEFAULT) == -1 && errno!=0) {
    char buf[1024];
    Oc_StrError(errno,buf,sizeof(buf));
    fprintf(stderr,"Unable to renice application to nice level %d: %s\n",
            NICE_DEFAULT,buf);
  }
#endif
}

static int
Oc_MakeNice(ClientData, Tcl_Interp *interp, int argc, char **argv)
{
  Tcl_ResetResult(interp);
  if (argc != 1) {
    Tcl_AppendResult(interp, argv[0], " takes no arguments", (char *) NULL);
    return TCL_ERROR;
  }
  MakeNice();
  return TCL_OK;
}

Tcl_Interp *
Oc_GlobalInterpreter()
{
  return globalInterp;
}

static void
ClearGlobalInterpreter(ClientData, Tcl_Interp *interp)
{
  if (interp == globalInterp) {
    globalInterp = (Tcl_Interp *) NULL;
    globalInterpThread = std::thread::id(); // Reset to null thread
  } else {
    Tcl_Panic(OC_CONST84_CHAR("Global interpreter mismatch!"));
  }
}

void
Oc_SetDefaultTkFlag(int val)
{
  use_tk = val;
}

static const char* tk_usage_info="\n\
{-colormap window {Specify \"new\" to use private colormap}}\n\
{-display  display {Display to use}}\n\
{-geometry geometry {Initial geometry for window}}\n\
{-name     name {Name to use for application}}\n\
{-sync     {}     {Use synchronous mode for display server}}\n\
{-visual   visual {Visual for main window}}\n\
{-use      id {Id of window in which to embed application}}";

static int
separateArgumentStrings(Tcl_Interp *interp,
CONST84 char *wholestr,
int &tkc, CONST84 char * &tkstr,
int &appc, CONST84 char * &appstr,
int &consoleRequested)
{
  int i,j;
  struct _Option_Info {
    char* name;
    int count;  // >=0; Number of *additional* parameters
  };

  // Extract Tk options from tk_usage_info
  int tkuic; CONST84 char ** tkuiv;

  // No argv?  Then no argument strings.
  Tcl_SplitList(interp,OC_CONST84_CHAR(tk_usage_info),&tkuic,&tkuiv);
  _Option_Info *oi=new _Option_Info[size_t(tkuic)];
  for(j=0;j<tkuic;j++) {
    int optionc; CONST84 char ** optionv;
    // Split option list into 3 components: name, type(s), description
    Tcl_SplitList(interp,tkuiv[j],&optionc,&optionv);
    oi[j].name=new char[strlen(optionv[0])+1];
    strcpy(oi[j].name,optionv[0]);
    int typec; CONST84 char ** typev;
    // Parse type(s) list to determine parameter count
    Tcl_SplitList(interp,optionv[1],&typec,&typev);
    oi[j].count=typec;
    Tcl_Free((char*)typev); Tcl_Free((char*)optionv);
  }
  Tcl_Free((char*)tkuiv);

  // Note: tkstr and appstr are dynamically allocated.  It is
  //       the responsibility of the calling routine to call
  //       Tcl_Free to release their memory.
  int wc;  CONST84 char ** wv;  // Whole list
  if (wholestr == NULL) {
    wc = 0;
    // Some versions of g++ complain about casting the return from
    // Tcl_Alloc directly to a char**, claiming the latter has increased
    // alignment restrictions.  Just hack around via a void*.
    wv = static_cast<CONST84 char **>((void*)Tcl_Alloc(1));
    (*wv) = NULL;
  } else if (Tcl_SplitList(interp,wholestr,&wc,&wv) != TCL_OK) {
    return TCL_ERROR;
  }

  int tc; CONST84 char ** tv=new CONST84 char *[size_t(wc)];  // Tk's arg list
  int ac; CONST84 char ** av=new CONST84 char *[size_t(wc)];  // App's arg list

  i=tc=ac=0;
  int opts_done=0;  // Terminate options processing, due to "--" flag.
  while(i<wc) {
    if(!opts_done) {
      if(strcmp("--",wv[i])==0) opts_done=1;
      else {
        for(j=0;j<tkuic;j++) {
          if(strcmp(oi[j].name,wv[i])==0) break; // Match found
        }
      }
    }
    if(!opts_done && j<tkuic) {
      // wv[i] is a recognized Tk argument.  Place it and
      // any additional parameters into tv.
      for(int k=0;k<1+oi[j].count;k++)
        tv[tc++]=wv[i++];
    } else {
      // wv[i] is not a recognized Tk argument.  Place it
      // into av;
      if (strcmp(wv[i],"-console") == 0) {
        consoleRequested = 1;
      }
      av[ac++]=wv[i++];
    }
  }

  // Create new list strings
  tkc=tc;  tkstr=Tcl_Merge(tc,tv);
  appc=ac; appstr=Tcl_Merge(ac,av);

  // Release unneeded arrays
  for(j=0;j<tkuic;j++) delete[] oi[j].name;
  delete[] oi;
  delete[] tv;
  delete[] av;
  Tcl_Free((char *)wv);

  return TCL_OK;
}

/*
 * Removes argument "remove_index" from argument list by
 * translating all arguments after it down one spot and
 * decrementing argc.
 */
static void
removeArg(int &argc, CONST84 char ** argv, int remove_index)
{
  if(remove_index<0 || remove_index>=argc) return;
  for(int j=remove_index+1;j<argc;j++) {
    argv[j-1]=argv[j];
  }
  argc--;
}

/*
 * Extract Oc options from Tcl argv variable.
 */
static void
extractOcOptions(Tcl_Interp *interp)
{
  char scratch[256];

  // Pull out and decompose Tcl argv variable
  int argc; CONST84 char ** argv; CONST84 char * argvstr;
  if((argvstr=Tcl_GetVar(interp,OC_CONST84_CHAR("argv"),
			 TCL_GLOBAL_ONLY))==NULL) return;
  Tcl_SplitList(interp,argvstr,&argc,&argv);

  int i=0;
  while(i<argc) {
    if(strcmp(argv[i],"--")==0){
      // End flag processing
      break;
    }

    // Known options
    if(i+1<argc && strcmp(argv[i],"-tk")==0) {
      // Use/don't use Tk option: -tk OC_BOOL
      use_tk=atoi(argv[i+1]);
      removeArg(argc,argv,i);
      removeArg(argc,argv,i);
    }
    else {
      // Anything else is left on the command line untouched, for
      // possible use by the shell script or other extensions.
      i++;
    }
  }

  // Re-assemble Tcl argv variable
  argvstr=Tcl_Merge(argc,argv);
  Tcl_SetVar(interp,OC_CONST84_CHAR("argv"),OC_CONST84_CHAR(argvstr),
	     TCL_GLOBAL_ONLY);
  sprintf(scratch,"%d",argc);
  Tcl_SetVar(interp,OC_CONST84_CHAR("argc"),scratch,TCL_GLOBAL_ONLY);

  // Release memory dynamically allocated by Tcl library routines.
  Tcl_Free((char *)argv);
  Tcl_Free((char *)argvstr);
}

int
Oc_Init(Tcl_Interp *interp)
{
  char scratch[256];

  if (globalInterp == (Tcl_Interp *)nullptr) {
    globalInterpThread = std::this_thread::get_id();
    globalInterp = interp;
    Tcl_CallWhenDeleted(interp,
			(Tcl_InterpDeleteProc *) ClearGlobalInterpreter,
			(ClientData) NULL);

    // Normally the function Oc_Init(Tcl_Interp *interp) will be called by
    // the application initialization function of the application using this
    // extension.  The conventional name for that function is
    // Tcl_AppInit(Tcl_Interp *interp).  The argument 'interp' passed to
    // Tcl_AppInit (and passed on to Oc_Init) was presumably returned
    // by a call to Tcl_CreateInterp(), usually from within Oc_Main().
    // Tcl_CreateInterp() contains a call to TclPlatformInit(), an internal
    // routine inside the Tcl library which takes care of basic
    // platform-specific initialization.  This means that although Tcl_Init
    // hasn't yet been called (and therefore the interpreter is not fully
    // initialized), the interpreter is usable in a platform-independent
    // manner, and the Tcl variables tcl_pkgPath, tcl_library, tcl_platform,
    // tcl_patchLevel, tcl_version, tcl_precision and env(HOME) have already
    // been set.  Also the "Tcl" package has already been provided.
    //
    // Until the Oc_Log cross-platform facility for displaying messages to
    // the user is supported by evaluating the Tcl portion of this extension
    // below, we must fall back on calls to Tcl_Panic().

    // Initialize Tcl.
#ifdef CONFIG_TCL_LIBRARY
    // Set environment variable TCL_LIBRARY, unless already set.
    // We set this instead of tcl_library, because the tcl_library
    // is not inherited into slave interpreters.
    if(Tcl_GetVar2(interp,OC_CONST84_CHAR("env"),
                   OC_CONST84_CHAR("TCL_LIBRARY"),
                   TCL_GLOBAL_ONLY)==NULL) {
      Tcl_SetVar2(interp,OC_CONST84_CHAR("env"),
                  OC_CONST84_CHAR("TCL_LIBRARY"),
                  OC_CONST84_CHAR(OC_STRINGIFY(CONFIG_TCL_LIBRARY)),
                  TCL_GLOBAL_ONLY);
    }
#endif
    if (Tcl_Init(interp) != TCL_OK) {
      if(strncmp("Can't find a usable",
                 Tcl_GetStringResult(interp),19)==0) {
        Tcl_Panic(OC_CONST84_CHAR("Tcl initialization error:\n\n%s\n\n"
                                  "As a workaround, set the environment variable\n"
                                  "TCL_LIBRARY to the name of a directory\n"
                                  "containing an error-free init.tcl."),
                  Tcl_GetStringResult(interp));
      } else {
        Tcl_Panic(OC_CONST84_CHAR("Tcl initialization error:\n\n%s"),
                  Tcl_GetStringResult(interp));
      }
    }
  }

  if (Nullchannel_Init(interp) != TCL_OK) {
      Tcl_Panic(OC_CONST84_CHAR("Nullchannel initialization error:\n\n%s"),
                Tcl_GetStringResult(interp));
  }

  // Extract Oc options.  We must do this before initializing Tk,
  // because one of the Oc options is whether or not to use Tk.
  // However, we can not initialize Oc before Tk, because some parts
  // of Oc initialize differently if Tk is available.
  extractOcOptions(interp);

  // Initialize Tk, if requested.
  if (use_tk &&
      (Tcl_PkgPresent(interp,OC_CONST84_CHAR("Tk"),NULL,0) == NULL)) {
    // Patch a bug in Tk 4.2
    if (Tcl_Eval(interp, OC_CONST84_CHAR(patchScript0)) != TCL_OK) {
      Tcl_Panic(OC_CONST84_CHAR("Tcl/Tk initialization error:\n\n"
               "Unable to patch installation bug in Tk 4.2\n\n"
               "Patch script returned error:%s"),
	    Tcl_GetStringResult(interp));
    }

    // Break Tk arguments out of command line argument list.
    // Provide dummy initial values for tk_argc, tk_argv,
    // app_argc, and app_argv to quiet some nanny compilers.
    int tk_argc=0;
    int app_argc=0;
    CONST84 char* orig_argv;
    CONST84 char* tk_argv=0;
    CONST84 char* app_argv=0;
    int consoleRequested = 0;
    orig_argv=Tcl_GetVar2(interp,OC_CONST84_CHAR("argv"),
                          (char *)NULL,TCL_GLOBAL_ONLY);
    separateArgumentStrings(interp,orig_argv,
                            tk_argc,tk_argv,
                            app_argc,app_argv,consoleRequested);

#if ((TK_MAJOR_VERSION == 8) && (TK_MINOR_VERSION > 0))       \
     || (TK_MAJOR_VERSION > 8)
    // We have to call Tk_InitConsoleChannels before Tk_Init (arguably a
    // Tk bug).  Tk_InitConsoleChannels first appears in Tk 8.1
    if (consoleRequested && (interp == globalInterp)) {
      Tk_InitConsoleChannels(interp);
    }
#endif

    // Initialize Tk, with argv copied from tk_argv
    Tcl_SetVar(interp,OC_CONST84_CHAR("argv"),
	       OC_CONST84_CHAR(tk_argv),TCL_GLOBAL_ONLY);
    sprintf(scratch,"%d",tk_argc);
    Tcl_SetVar(interp,OC_CONST84_CHAR("argc"),scratch,TCL_GLOBAL_ONLY);
    if (Tk_Init(interp) != TCL_OK) {
      if (globalInterp != interp) {
        return TCL_ERROR;
      }
      if(strncmp("Can't find a usable",
                 Tcl_GetStringResult(interp),19)==0) {
        Tcl_Panic(OC_CONST84_CHAR("Tk initialization error:\n\n%s\n\n"
                 "As a workaround, set the environment variable\n"
                 "TK_LIBRARY to the name of a directory\n"
                 "containing an error-free tk.tcl."),
              Tcl_GetStringResult(interp));
      } else {
        Tcl_Panic(OC_CONST84_CHAR("Tk initialization error:\n\n%s"),
                  Tcl_GetStringResult(interp));
      }
    }

    // Reset argv with app arguments
    Tcl_SetVar(interp,OC_CONST84_CHAR("argv"),
	       OC_CONST84_CHAR(app_argv),TCL_GLOBAL_ONLY);
    sprintf(scratch,"%d",app_argc);
    Tcl_SetVar(interp,OC_CONST84_CHAR("argc"),scratch,TCL_GLOBAL_ONLY);
    Tcl_Free((char *)tk_argv); Tcl_Free((char *)app_argv);  // Release

    // Set up so that [info loaded] result includes Tk
    Tcl_StaticPackage(interp,OC_CONST84_CHAR("Tk"), Tk_Init, Tk_SafeInit);

    if (consoleRequested && (interp == globalInterp)) {
      if (Tk_CreateConsoleWindow(interp) != TCL_OK) {
        Tcl_Panic(OC_CONST84_CHAR("Tk console initialization error:\n\n%s"),
                  Tcl_GetStringResult(interp));
      }
      Tcl_Eval(interp,OC_CONST84_CHAR("console hide"));
    }
  }

  // Initialize Oc
  // Register C++-based commands of this extension
#if OC_SYSTEM_TYPE==OC_WINDOWS
  Oc_RegisterCommand(interp,"Oc_WindowsMessageBox",
                     Oc_WindowsMessageBoxCmd);
  Oc_RegisterCommand(interp,"Oc_WinGetFileSID",
                     (Tcl_CmdProc *)OcWinGetFileSID);
  Oc_RegisterCommand(interp,"Oc_WinGetCurrentProcessSID",
                     (Tcl_CmdProc *)OcWinGetCurrentProcessSID);
  Oc_RegisterCommand(interp,"Oc_WinGetSIDAccountName",
                     (Tcl_CmdProc *)OcWinGetSIDAccountName);
#endif
#if OC_TCL_TYPE==OC_WINDOWS && defined(__CYGWIN__)
  Oc_RegisterCommand(interp,"OcCygwinChDir",OcCygwinChDir);
#endif // OC_WINDOWS && __CYGWIN__
  Oc_RegisterCommand(interp,"Oc_IgnoreSignal",
                     (Tcl_CmdProc *) Oc_IgnoreSignal);
  Oc_RegisterCommand(interp,"Oc_LockChannel",
                     (Tcl_CmdProc *) Oc_LockChannel);
  Oc_RegisterCommand(interp,"Oc_UnlockChannel",
                     (Tcl_CmdProc *) Oc_UnlockChannel);
  Oc_RegisterCommand(interp,"Oc_MakeNice",(Tcl_CmdProc *) Oc_MakeNice);
  Oc_RegisterCommand(interp,"Oc_SetPanicHeader",Oc_SetPanicHeaderCmd);

  Oc_RegisterCommand(interp,"Oc_Srand",(Tcl_CmdProc *)OcSrand);
  Oc_RegisterCommand(interp,"Oc_UnifRand",(Tcl_CmdProc *)OcUnifRand);
  Oc_RegisterCommand(interp,"Oc_ReadRNGState",
                     (Tcl_CmdProc *)OcReadRNGState);
  Oc_RegisterCommand(interp,"Oc_InitLogPrefix",
                     (Tcl_CmdProc *)Oc_LogSupportInitPrefixCmd);
  Oc_RegisterCommand(interp,"Oc_GetLogMark",
                     (Tcl_CmdProc *)Oc_LogSupportGetLogMarkCmd);
  Oc_RegisterCommand(interp,"Oc_AddCLogFile",
                     (Tcl_CmdProc *)Oc_ReportAddCLogFile);

  // Import Oc_Atan2 into Tcl, and replace default expr/mathfunc atan2
  // with it. NB: Child interpreters appear to revert back to default
  // behavior; the interp alias command can be used to override.
  Oc_RegisterCommand(interp,"Oc_Atan2",(Tcl_CmdProc *)OcAtan2);
  Oc_RegisterCommand(interp,"::tcl::mathfunc::atan2",(Tcl_CmdProc *)OcAtan2);

  // Thread support code
  Oc_RegisterCommand(interp,"Oc_HaveThreads",
		     (Tcl_CmdProc *)OcHaveThreads);
  Oc_RegisterCommand(interp,"Oc_GetMaxThreadCount",
		     (Tcl_CmdProc *)OcGetMaxThreadCount);
  Oc_RegisterCommand(interp,"Oc_SetMaxThreadCount",
		     (Tcl_CmdProc *)OcSetMaxThreadCount);
  Oc_RegisterCommand(interp,"Oc_NumaAvailable",
		     (Tcl_CmdProc *)OcNumaAvailable);
  Oc_RegisterCommand(interp,"Oc_NumaDisable",
		     (Tcl_CmdProc *)OcNumaDisable);
  Oc_RegisterCommand(interp,"Oc_NumaInit",
		     (Tcl_CmdProc *)OcNumaInit);

  if (Tcl_Eval(interp, OC_CONST84_CHAR(initScript0)) != TCL_OK
      || Tcl_Eval(interp, OC_CONST84_CHAR(initScript1)) != TCL_OK
      || Tcl_Eval(interp, OC_CONST84_CHAR(initScript2)) != TCL_OK
      || Tcl_Eval(interp, OC_CONST84_CHAR(initScript3)) != TCL_OK) {
    if (globalInterp != interp) {
      return TCL_ERROR;
    }
    Tcl_Panic(OC_CONST84_CHAR("Error in extension 'Oc':\n\n"
             "Error evaluating pre-initialization scripts:\n%s"),
              Tcl_GetStringResult(interp));
  }

#if OC_TCL_TYPE==OC_WINDOWS && defined(__CYGWIN__)
  if (Tcl_Eval(interp, OC_CONST84_CHAR(initScript_cygwin)) != TCL_OK) {
    if (globalInterp != interp) {
      return TCL_ERROR;
    }
    Tcl_Panic(OC_CONST84_CHAR("Error in extension 'Oc':\n\n"
             "Error evaluating cygwin pre-initialization script:\n%s"),
              Tcl_GetStringResult(interp));
  }
#endif // OC_WINDOWS && __CYGWIN__

  if (Oc_InitScript(interp, "Oc", OC_VERSION) != TCL_OK) {
    if (globalInterp != interp) {
      return TCL_ERROR;
    }
    Tcl_Panic(OC_CONST84_CHAR("%s"),
	  Tcl_GetVar(interp, OC_CONST84_CHAR("errorInfo"), TCL_GLOBAL_ONLY));
  }

  // At this stage Oc_Log is available for displaying error messages
  // to the user.


  // Some routines perform process-wide initialization which is intended
  // to be run only once during process initialization. Control this by
  // tying initialization to the global interp. If globalInterp were to
  // be reset (cf. ClearGlobalInterpreter()) then this code would be
  // allowed to run again. It is unclear if this is the desired
  // behavior, but at present it seems that globalInterp is reset only
  // on program exit, so the question is moot.
  if (globalInterp == interp) {
    // Set up asynchronous exception handling. Note
    // Oc_AsyncError::RegisterHandler calls Oc_GetOcOption, and so needs
    // to be delayed until after Oc_InitScript has been run in order to
    // get the Oc_Option database set up.
    Oc_AsyncError::RegisterHandler();

    // Default thread request count.  This may be overridden by
    // application (for example, using a value from the command line).
    // Most of the logic is in the Oc_GetDefaultThreadCount proc,
    // which is in octhread.tcl.
#if OOMMF_THREADS
    int otc = -1;
    if(Tcl_Eval(interp,OC_CONST84_CHAR("Oc_GetDefaultThreadCount")) == TCL_OK) {
      otc = atoi(Tcl_GetStringResult(interp));
    }
    if(otc<1) otc = 1; // Safety

    // Set value
    Oc_SetMaxThreadCount(otc);
#endif
  }

  // Constants from tcl.h, limits.h, float.h, and ocport.h
  if(Tcl_Eval(interp,
      OC_CONST84_CHAR("if {[catch {set config [Oc_Config RunPlatform]} msg]} "
		      " {catch {Oc_Log Log $msg error} "
		      "; error {Command \"Oc_Config RunPlatform\" failed} }")) != TCL_OK) {
    if (globalInterp != interp) {
      return TCL_ERROR;
    }
    Tcl_Panic(OC_CONST84_CHAR("Error in extension 'Oc':\n\n"
             "Error evaluating post-initialization scripts:\n%s"),
              Tcl_GetStringResult(interp));
  }

  char buf[1024];  // *Should* be big enough
  int errcount=0;
  Tcl_DString tbuf;
# define CTC_SET(name,format,macro)                                  \
  do {                                                               \
  Oc_Snprintf(buf,sizeof(buf),                                       \
              "$config SetValue "  name  " "  format,macro);         \
  if(Tcl_Eval(interp,OC_CONST84_CHAR(buf)) != TCL_OK) {              \
     if (globalInterp != interp) return TCL_ERROR;                   \
     Tcl_DStringInit(&tbuf);                                         \
     Tcl_DStringAppend(&tbuf, OC_CONST84_CHAR("Oc_Log Log"), -1);    \
     Tcl_DStringAppendElement(&tbuf, Tcl_GetStringResult(interp));   \
     Tcl_DStringAppendElement(&tbuf, OC_CONST84_CHAR("error"));      \
     Tcl_Eval(interp, Tcl_DStringValue(&tbuf));                      \
     Tcl_DStringFree(&tbuf);                                         \
     errcount++;                                                     \
  } } while(0)
  CTC_SET("compiletime_tcl_patch_level","%s",TCL_PATCH_LEVEL);
  CTC_SET("compiletime_int_min","%d",INT_MIN);
  CTC_SET("compiletime_int_max","%d",INT_MAX);
  CTC_SET("compiletime_uint_max","%u",UINT_MAX);
  CTC_SET("compiletime_long_min","%ld",LONG_MIN);
  CTC_SET("compiletime_long_max","%ld",LONG_MAX);
  CTC_SET("compiletime_ulong_max","%lu",ULONG_MAX);
  CTC_SET("compiletime_flt_dig","%d",FLT_DIG);
  CTC_SET("compiletime_flt_epsilon","%.17g",FLT_EPSILON);
  CTC_SET("compiletime_flt_min","%.17g",FLT_MIN);
  CTC_SET("compiletime_flt_max","%.17g",FLT_MAX);
  CTC_SET("compiletime_dbl_dig","%d",DBL_DIG);
  CTC_SET("compiletime_dbl_epsilon","%.17g",DBL_EPSILON);
  CTC_SET("compiletime_dbl_min","%.17g",DBL_MIN);
  CTC_SET("compiletime_dbl_max","%.17g",DBL_MAX);

  // System type info from ocport.h, created by procs.tcl.
  if(OC_SYSTEM_TYPE == OC_UNIX) {
    CTC_SET("compiletime_system_type","%s","OC_UNIX");
  } else if(OC_SYSTEM_TYPE == OC_WINDOWS) {
    CTC_SET("compiletime_system_type","%s","OC_WINDOWS");
  } else if(OC_SYSTEM_TYPE == OC_VANILLA) {
    CTC_SET("compiletime_system_type","%s","OC_VANILLA");
  }
  if(OC_SYSTEM_SUBTYPE == OC_DARWIN) {
    CTC_SET("compiletime_system_subtype","%s","OC_DARWIN");
  } else if(OC_SYSTEM_SUBTYPE == OC_WINNT) {
    CTC_SET("compiletime_system_subtype","%s","OC_WINNT");
  } else if(OC_SYSTEM_SUBTYPE == OC_CYGWIN) {
    CTC_SET("compiletime_system_subtype","%s","OC_CYGWIN");
  }
# undef CTC_SET

  if(Tcl_Eval(interp,
	      OC_CONST84_CHAR("catch {unset config} ; catch {unset msg}"))
     != TCL_OK) {
     if (globalInterp != interp) return TCL_ERROR;
     Tcl_DStringInit(&tbuf);
     Tcl_DStringAppend(&tbuf, OC_CONST84_CHAR("Oc_Log Log"), -1);
     Tcl_DStringAppendElement(&tbuf, Tcl_GetStringResult(interp));
     Tcl_DStringAppendElement(&tbuf, OC_CONST84_CHAR("error"));
     Tcl_Eval(interp, Tcl_DStringValue(&tbuf));
     Tcl_DStringFree(&tbuf);
     errcount++;
  }

  // Make sure DBL values above are in-range
  Tcl_Eval(interp,
	   OC_CONST84_CHAR("Oc_FixupConfigBadDouble compiletime_dbl_min"));
  Tcl_Eval(interp,
	   OC_CONST84_CHAR("Oc_FixupConfigBadDouble compiletime_dbl_max"));

  // If we have no script to evaluate, parse the command line ourselves
  // to display a console
  if ((interp == globalInterp) && parseCommandLine) {
    if (Tcl_Eval(interp,
		 OC_CONST84_CHAR("Oc_CommandLine Parse $argv")) != TCL_OK) {
        Tcl_DStringInit(&tbuf);
        Tcl_DStringAppend(&tbuf, OC_CONST84_CHAR("Oc_Log Log"), -1);
        Tcl_DStringAppendElement(&tbuf, Tcl_GetStringResult(interp));
        Tcl_DStringAppendElement(&tbuf, OC_CONST84_CHAR("error"));
        Tcl_Eval(interp, Tcl_DStringValue(&tbuf));
        Tcl_DStringFree(&tbuf);
        errcount++;
    }
  }
  if(errcount>0) return TCL_ERROR;

  return TCL_OK;
}

// Oc proxy for Tcl/Tk_Main
void Oc_Main(int argc,char** argv,Tcl_AppInitProc* appinit)
{
  char **my_argv = NULL;
  // Any program that uses Oc_Main is setting itself up to be a
  // shell program.  A shell program will treat its first argument
  // as the name of a file containing the script the shell should
  // evaluate, so long as that first argument does not begin with
  // the '-' character.
  //
  // If the shell program gets no arguments, or the first argument
  // does begin with the '-' character, then no script file argument
  // is seen by the shell program.  Instead, it will attempt to run
  // in an interactive mode, where the user can type commands to be
  // run interactively.  The following code sets up the interactive
  // mode by enabling Tk, and setting up to use Tk's interactive
  // console.  This is the only interactive mode we've been able to
  // make work properly across all the platforms and Tcl versions
  // we support.  Note that the additional options are appended at
  // the end, so that they force themselves to be effective.  For
  // example,
  //
  // $ shell -tk 0
  //
  // becomes effectively
  //
  // $ shell -tk 0 -tk 1 -console
  //
  // and the user request to disable Tk is overridden.  This is a bit
  // surprising, but preferable to starting up a mode that just doesn't
  // work at all.  If the user wants interactivity, they need Tk.
  //
  if ((argc == 1) || (argv[1][0] == '-')) {
    my_argv = new char*[size_t(argc+3)];
    for (int i=0; i<argc; i++) my_argv[i] = argv[i];
    my_argv[argc] = new char[4];
    strcpy(my_argv[argc++],"-tk");
    my_argv[argc] = new char[2];
    strcpy(my_argv[argc++],"1");
    my_argv[argc] = new char[9];
    strcpy(my_argv[argc++],"-console");
    argv = my_argv;
    parseCommandLine = 1;
    // Note: The "new" calls above allocate a small amount of
    // memory which is never freed.  It is difficult to do
    // anything about this, however, because the Tk_Main call
    // below doesn't return.
  }

# if defined(OC_SET_MACOSX_APPNAME) && OC_SET_MACOSX_APPNAME
  // The title on the application menu (in Tk this is the .apple
  // menu, which is not to be confused with the leftmost item on the
  // toplevel menubar titled with a bitten apple icon) uses the
  // value of the CFBundleName key in the main bundle info
  // dictionary.  This key is typically set by the Info.plist file
  // in the app's framework.  If, like OOMMF, there is no
  // Info.plist, then (apparently) the CFBundleName is left unset
  // and the application name is derived from the name of the
  // executable.  On Mac OS X, the app name in the menu bar is one
  // of the primary signals notifying the user which application has
  // the focus, so we really would like that name to identify the
  // application, as opposed to being a generic name pointing to the
  // script language interpreter executable (such as Wish or omfsh).
  //
  // A "proper" way to do this is via an Info.plist file.  Is there
  // a straightforward way to implement that in the OOMMF
  // environment?  An alternative, used here, is to modify the main
  // bundle dictionary.  Note that this requires a const_cast to the
  // dictionary pointer, because the return type from
  // CFBundleGetInfoDictionary is CFDictionaryRef, which is a const*
  // type.  So clearly this code is improper.  Is it dangerous?
  // BTW, presumably if this is to work at all the CFBundleName key
  // needs to be set early in the application initialization; it seems
  // the application name is initialized at some point from CFBundleName
  // (if set), but is afterwards stored elsewhere.
  CFBundleRef mainBundle = CFBundleGetMainBundle();
  if (!mainBundle) {
    Tcl_Panic(OC_CONST84_CHAR("%s"),"Mac OS X API error: "
              "Unable to get main bundle.");
  }
  CFDictionaryRef bundleInfoDict = CFBundleGetInfoDictionary(mainBundle);
  if (!bundleInfoDict) {
    Tcl_Panic(OC_CONST84_CHAR("%s"),"Mac OS X API error: "
              "Unable to get main bundle dictionary.");
  }
  CFStringRef key = CFStringCreateWithCString(NULL,"CFBundleName",
					      kCFStringEncodingUTF8);
  Oc_AutoBuf progname;
  if (argc>1 && argv[1][0] != '-') {
    size_t namelen = strlen(argv[1]);
    size_t i = namelen;
    while(i>0) {
      if(argv[1][i-1] == '/') break;  // Dir separator on OS X is '/'
      --i;
    }
    progname = argv[1]+i;
    namelen -= i;
    for(i=0;i<namelen;++i) {
      if(progname[i]=='.') { // Truncate extension, if any
	progname.SetLength(i);
	break;
      }
    }
  } else {
    progname = "OOMMF";
  }
  CFStringRef val = CFStringCreateWithCString(NULL,progname,
					      kCFStringEncodingUTF8);
  if(key && val) {
    CFDictionarySetValue(const_cast<CFMutableDictionaryRef>(bundleInfoDict),
			 key,val);
    // The CFBundleName key is expected to be not set, so either
    // CFDictionarySetValue or CFDictionaryAddValue may be used.
  }
  if(key) CFRelease(key);
  if(val) CFRelease(val);
# endif // OC_SET_MACOSX_APPNAME

  try {
    Tk_Main(argc,argv,appinit);
  }
  catch (bad_alloc&) {
    Tcl_Panic(OC_CONST84_CHAR("Out of memory!\n"));
  }
  catch (Oc_Exception& err) {
    Oc_AutoBuf msg;
    Tcl_Panic(OC_CONST84_CHAR("%s"),err.ConstructMessage(msg));
  }
  catch (EXCEPTION& e) {
    Tcl_Panic(OC_CONST84_CHAR("Uncaught standard exception: %s\n"
                              "Probably out of memory."),
              e.what());
  }
  catch (const char* err) {
    Tcl_Panic(OC_CONST84_CHAR("%s"),err);
  }
}

/*
 * Most commonly on Windows, but also in certain cirumstances on Unix,
 * a child process can get started without a standard channel.  Since
 * Tcl then assigns the next channel it opens to replace any missing
 * stdin, stdout, and stderr, in that order, this is a prescription for
 * unpredictable disaster.  To avoid that, we should check very early
 * for missing standard channels, and supply predictable null replacements
 * for them.
 *
 * It is also possible that Tcl (thinks it) has standard channels, but
 * I/O operations to those channels fail.  This can also bring down an
 * otherwise happy program, so we check for that and replace non-functional
 * standard channels with null replacements as well.
 *
 * Applications using the WinMain entry point should use the
 * VerifyWindowsTclStandardChannel() function to do a rigorous testing
 * of the standard channels.  Unix and Windows console apps can just
 * just use the Tcl_GetStdChannel() call to test.  Either way, in
 * failure use NullifyTclStandardChannel to replace the bad channel
 * with a null channel.
 *
 * NOTE: These routines have no relation to the platform-specific standard
 * channels available at the C/C++ level: fprintf(std*, ...), cout << ..., etc.
 *
 */
static void
NullifyTclStandardChannel(int type) {
    Tcl_Channel channel;
    // Create a null channel to replace a presumably broken standard channel
    if ((channel = Nullchannel_Open()) == NULL) {
      switch (type)
	{
	case TCL_STDIN:
	  Tcl_Panic(OC_CONST84_CHAR("Couldn't create a standard input channel"));
          break; // Keep compilers happy
	case TCL_STDOUT:
	  Tcl_Panic(OC_CONST84_CHAR("Couldn't create a standard output channel"));
          break; // Keep compilers happy
	case TCL_STDERR:
	  Tcl_Panic(OC_CONST84_CHAR("Couldn't create a standard error channel"));
          break; // Keep compilers happy
	default:
	  Tcl_Panic(OC_CONST84_CHAR("Bad standard channel type: %d"), type);
          break; // Keep compilers happy
        }
    }
    /* This may not be necessary, since Tcl_CreateChannel automatically
     * sets empty standard channels, but it shouldn't hurt.
     */
    Tcl_SetStdChannel(channel, type);
}

/*
 * The common startup initializations for all platforms
 */
void
CommonMain(CONST char* exename) {
  // Mask floating-point exceptions to allow rolling Infs and NaNs.
  Oc_FpuControlData::MaskExceptions();


#if (OC_SYSTEM_TYPE==OC_WINDOWS)
  // Disable some Windows system error dialog boxes.  Instead, let
  // OOMMF handle error reporting.
#ifdef _MSC_VER
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif // _MSC_VER
  DWORD dwMode = SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);
  SetErrorMode(dwMode | SEM_NOGPFAULTERRORBOX | SEM_FAILCRITICALERRORS);
#endif // OC_WINDOWS

  // Initialize the Tcl_Panic() function to use our customized
  // message display routines.
  Oc_InitPanic(exename);

  // Tcl_FindExecutable does some double-secret special
  // initializations without which some Tcl C APIs (like
  // Tcl_CreateChannel()) will crash.  So, call it here.
  Tcl_FindExecutable(OC_CONST84_CHAR(exename));
}

#if (OC_SYSTEM_TYPE==OC_WINDOWS)
static int
VerifyWindowsTclStandardChannel(int type) {
  /*
   * On Windows, programs with entry point WinMain() which are launched
   * from the operating system do not have any standard channels.
   * When such programs are launched as subprocesses by another
   * program via the CreateProcess() system call, the parent
   * process can pass standard channels to the subprocess.  The
   * startInfo structure retrieved from the OS contains
   * information about what instructions the parent process
   * supplied for this program.  If the STARTF_USESTDHANDLES bit
   * of the flag array startInfo.dwFlags is set, then the parent
   * process provided standard channels to this process.  In that
   * case, it is safe to call the Tcl library function
   * Tcl_GetStdChannel().
   *
   * This routine returns 1 if channel looks okay, 0 otherwise.
   */
  STARTUPINFO startInfo;
  GetStartupInfo(&startInfo);
  if (startInfo.dwFlags & STARTF_USESTDHANDLES) {
    Tcl_Channel channel;
    if ((channel=Tcl_GetStdChannel(type)) != NULL) {
      /*
       * On at least some Windows platforms for at least some version(s)
       * of Tcl/Tk, we've found that Tcl_GetStdChannel() returns non-NULL
       * even when it is not returning a valid channel.  Subsequent I/O
       * on the returned channel returns the error "Bad file number".
       * Testing the returned pointer with Tcl_Tell can be used to check
       * for this -- it returns 0 on good channels, -1 on bad.
       *
       * We must be sure *not* to use the Tcl_Tell test on Unix, since
       * the standard channels are often not seekable, even though they
       * are valid on Unix.  Although replacement with a null channel
       * ought to harmless, experience proves it is not.
       *
       * Wish I had time to really figure all this out...
       */
      if (Tcl_Tell(channel)>=0) {
	return 1;     // Std channel verified!
      } else {
	/*
	 * Tcl_GetStdChannel thought it had a channel, but
	 * it doesn't work.
	 */
	Tcl_UnregisterChannel(NULL,channel);
      }
    }
  }   // close if (startInfo....
  return 0; // Bad channel
}

/*
 * The main entry point for OOMMF applications on Windows platforms.
 */

#if defined(_MSC_VER)
// Headers for changing default error handling in Windows
# include <crtdbg.h>
#endif

int APIENTRY
WinMain(HINSTANCE /* hInstance */, HINSTANCE /* hPrevInstance */,
        LPSTR lpszCmdLine, int /* nCmdShow */)
{
#if defined(_MSC_VER)
    // Send error messages handled by the CRT debug report mechanism
    // to stderr.  The default handling produces sundry annoying
    // pop-ups.  (Note: This interface appears to go back to Win95.)
    // BTW, the Digital Mars Compiler 8.57 (macro identifier __DMC__)
    // includes a crtdbg.h header and recognizes the following
    // _CrtSetReport functions and macros, but default behavior is to
    // not open a dialog box on error anyway.
    _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDERR );
    _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
    _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
#endif // _MSC_VER

    // Windows doesn't pass the application name as the first word on the
    // command line like Unix does.  Instead a system call provides the
    // application name (as a full pathname).
    // NOTE: This fixed-length buffer should be long enough even for a full
    // pathname.  Dynamic allocation from the heap is avoided since
    // Tcl_Main and Tk_Main never return, and we'll have no chance to
    // free the memory.  It's not clear whether Windows will free memory
    // when the process exits.
    char *p;
    TCHAR win_exename[32767];
    GetModuleFileName(NULL, win_exename,
                      sizeof(win_exename)/sizeof(win_exename[0]));
    Oc_AutoBuf exename(win_exename); // Converts from TCHAR to char

    // Translate '\' to '/'
    for (p = exename; *p != '\0'; p++) {
        if (*p == '\\') *p = '/';
    }
    for (p = lpszCmdLine; *p != '\0'; p++) {
        if (*p == '\\') *p = '/';
    }

    CommonMain(exename);

    // Control task bar grouping.  Docs say
    //
    //  Minimum supported client: Windows 7 [desktop apps only]
    //  Minimum supported server: Windows Server 2008 R2 [desktop apps only]
    //                    Header: Shobjidl.h
    //                   Library: Shell32.lib
    //                       DLL: Shell32.dll (version 6.1 or later)
    //
    // Windows 7 and Windows Server 2008 both use "subsystem version"
    // code 6.01 (Vista is 6.00, Windows 8 is 6.02).  These can be
    // set in the executable header (see output of Visual C++ tool
    // "dumpbin /header foo.exe".
#if defined(OC_SET_TASKBAR_ID) && OC_SET_TASKBAR_ID
    SetCurrentProcessExplicitAppUserModelID(Oc_AutoWideBuf("NIST.OOMMF.oc"));
#endif

    // Setup standard channels
    if(!VerifyWindowsTclStandardChannel(TCL_STDIN)) {
      NullifyTclStandardChannel(TCL_STDIN);
    }
    if(!VerifyWindowsTclStandardChannel(TCL_STDOUT)) {
      NullifyTclStandardChannel(TCL_STDOUT);
    }
    if(!VerifyWindowsTclStandardChannel(TCL_STDERR)) {
      NullifyTclStandardChannel(TCL_STDERR);
    }

  // Windows doesn't tokenize the command line
  int argc;
  char *argv[256];
  CONST84 char **argv_temp;
  char cmdline[16384];
  if(Tcl_SplitList(NULL,lpszCmdLine,&argc,&argv_temp)!=TCL_OK) {
    Tcl_Panic(OC_CONST84_CHAR("Bad command line: %s"),lpszCmdLine);
  }
  if(size_t(argc+1)>sizeof(argv)/sizeof(argv[0])) {
    Tcl_Panic(OC_CONST84_CHAR("Too many command line parameters: %d>%lu"),
              argc+1,
              static_cast<unsigned long>(sizeof(argv)/sizeof(argv[0])));
  }

  // Copy split string out of temporary memory, and setup
  // argv pointers.
  argv[0]=exename;
  int i;
  size_t total_length=0;
  for(i=0;i<argc;i++) {
    argv[i+1]=cmdline+total_length;
    total_length+=strlen(argv_temp[i])+1;
    if(total_length>sizeof(cmdline)) {
      Tcl_Panic(OC_CONST84_CHAR("Parsed command line too long (%lu)"),
                static_cast<unsigned long>(sizeof(cmdline)));
    }
    strcpy(argv[i+1],argv_temp[i]);
  }
  argc++;
  Tcl_Free((char *)argv_temp);

  // Now we can pretend a Unix system gave us argc and argv
  int errorcode = 0;
  try { // Panic on any uncaught exceptions
    errorcode = Oc_AppMain(argc, argv);
  } catch(std::exception& errexc) {
    std::cerr << "\nUNCAUGHT EXCEPTION: " << errexc.what() << "\n";
    Tcl_Panic(OC_CONST84_CHAR("Uncaught exception: %s\n"),
              errexc.what());
  } catch(string& errstr) {
    std::cerr << "\nUNCAUGHT EXCEPTION: " << errstr << "\n";
    Tcl_Panic(OC_CONST84_CHAR("Uncaught exception (string): %s\n"),
              errstr.c_str());
  } catch(char* err_cstr) {
    std::cerr << "\nUNCAUGHT EXCEPTION: " << err_cstr << "\n";
    Tcl_Panic(OC_CONST84_CHAR("Uncaught exception (char*): %s\n"),
              err_cstr);
  } catch(...) {
    Tcl_Panic(OC_CONST84_CHAR("Uncaught exception\n"));
    errorcode=99;
  }

  return errorcode;
}

#endif // OC_SYSTEM_TYPE == OC_WINDOWS

/*
 * The main entry point for OOMMF applications on Unix platforms.
 * ... and Windows console apps.
 */
int
main(int argc, char **argv)
{
    CommonMain(argv[0]);

    // Setup standard channels.  Use a less stingent testing
    // regime than in the WinMain setting.
    if(Tcl_GetStdChannel(TCL_STDIN)==NULL) {
      NullifyTclStandardChannel(TCL_STDIN);
    }
    if(Tcl_GetStdChannel(TCL_STDOUT)==NULL) {
      NullifyTclStandardChannel(TCL_STDOUT);
    }
    if(Tcl_GetStdChannel(TCL_STDERR)==NULL) {
      NullifyTclStandardChannel(TCL_STDERR);
    }

    int errorcode = 0;
    try { // Panic on any uncaught exceptions
      errorcode = Oc_AppMain(argc, argv);
    } catch(std::exception& errexc) {
      std::cerr << "\nUNCAUGHT EXCEPTION: " << errexc.what() << "\n";
      Tcl_Panic(OC_CONST84_CHAR("Uncaught exception: %s\n"),
                errexc.what());
    } catch(string& errstr) {
      std::cerr << "\nUNCAUGHT EXCEPTION: " << errstr << "\n";
      Tcl_Panic(OC_CONST84_CHAR("Uncaught exception (string): %s\n"),
                errstr.c_str());
    } catch(char* err_cstr) {
      std::cerr << "\nUNCAUGHT EXCEPTION: " << err_cstr << "\n";
      Tcl_Panic(OC_CONST84_CHAR("Uncaught exception (char*): %s\n"),
                err_cstr);
    } catch(...) {
      Tcl_Panic(OC_CONST84_CHAR("Uncaught exception\n"));
      errorcode=99;
    }

    return errorcode;
}


#if defined(OC_MISSING_STARTUP_ENTRY) && OC_MISSING_STARTUP_ENTRY
# ifdef __CYGWIN__
/*
 * This snippet sets up the entry point for Cygwin/GUI apps.
 * An alternative to this is to add '-e _mainCRTStartup' to
 * the gcc/g++ link line.
 */
extern "C" {
int mainCRTStartup();
int WinMainCRTStartup();
}
int WinMainCRTStartup() { mainCRTStartup(); return 0; }
# else
# error "No fixup available for missing startup entry point on this platform."
# endif
#endif

/*
 * Dummy function calls to force expression evaluation.
 * Used to work around some broken compiler optimizations.
 * The types here should all be explicit types, not typedefs,
 * because typedefs can potentially resolve to overlapping
 * types, resulting in overload conflicts.  Also, we probably
 * don't want to use templates here(?), because we want to
 * be sure these calls don't get inlined (that being the whole
 * raison d'etre of these beasts).
 */
float         Oc_Nop(float x) { return x; }
double        Oc_Nop(double x) { return x; }
long double   Oc_Nop(long double x) { return x; }
int           Oc_Nop(int x) { return x; }
long          Oc_Nop(long x) { return x; }
unsigned int  Oc_Nop(unsigned int x) { return x; }
unsigned long Oc_Nop(unsigned long x) { return x; }
const void*   Oc_Nop(const void* x) { return x; }
const String& Oc_Nop(const String& x) { return x; }
Tcl_Channel   Oc_Nop(Tcl_Channel x) { return x; }
#if OC_USE_SSE
__m128d         Oc_Nop(__m128d x) { return x; }
__m128i         Oc_Nop(__m128i x) { return x; }
#endif
#if OC_SYSTEM_TYPE == OC_WINDOWS
__int64          Oc_Nop(__int64 x) { return x; }
unsigned __int64 Oc_Nop(unsigned __int64 x) { return x; }
#endif
