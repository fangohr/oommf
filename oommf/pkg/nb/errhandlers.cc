/* FILE: errhandlers.cc                    -*-Mode: c++-*-
 *
 * Some error handling routines that should be phased out
 * and replaced with code in the oc extension.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/11/09 00:50:01 $
 * Last modified by: $Author: donahue $
 */

#include <cstdarg>  // For va_list declarations
#include <cstdio>
#include <cstring>

#include "oc.h"
#include "errhandlers.h"
#include "messagelocker.h"

/* End includes */     /* Optional directive to build.tcl */

const char* ErrArrOutOfBounds = "Array index out of bounds";
const char* ErrBadFile        = "File error";
const char* ErrBadFileHeader  = "Bad file header";
const char* ErrBadFileOpen    = "Unable to open file";
const char* ErrInputEOF       = "Premature End-Of-File on input";
const char* ErrOutputEOF      = "Premature End-Of-File on output";
const char* ErrBadParam       = "Bad parameter";
const char* ErrDispInit       = "Unable to initialize display";
const char* ErrMisc           = "Miscellaneous error";
const char* ErrNoMem          = "Insufficient working memory";
const char* ErrOverflow       = "Arithmetic overflow";
const char* ErrProgramming    = "Programming error";

#define BigBufSize 16000

// Things worth dying over...
void FatalError(OC_INT4m errorcode,
		const OC_CHAR *membername,
		const ClassDoc &class_doc,
		const OC_CHAR *filename,OC_INT4m lineno,
		const OC_CHAR *fmt,...)
{
  char ErrorBuf[BigBufSize];
  va_list arg_ptr;
  char *cptr;
  Oc_Snprintf(ErrorBuf,sizeof(ErrorBuf),
	  "Fatal error occurred in %s::%s (File %s, line %d)\n"
	  "Class revision/date: %s/%s\n"
	  "Class maintainer: %s\nERROR: ",
	  class_doc.classname,membername,filename,lineno,
	  class_doc.revision,class_doc.revdate,
	  class_doc.maintainer);
  cptr=strchr(ErrorBuf,'\0');
  va_start(arg_ptr,fmt);
  Oc_Vsnprintf(cptr,sizeof(ErrorBuf)-(cptr-ErrorBuf),fmt,arg_ptr);
  va_end(arg_ptr);

  TkMessage(NB_MSG_ERROR,ErrorBuf,errorcode);

  // We shouldn't get to here, but just in case...
  Tcl_Exit(1);
}

// Fatal Error handler for non-class functions
void PlainError(OC_INT4m errorcode,const OC_CHAR *fmt,...)
{
  char ErrorBuf[BigBufSize];
  va_list arg_ptr;
  va_start(arg_ptr,fmt);
  Oc_Vsnprintf(ErrorBuf,sizeof(ErrorBuf),fmt,arg_ptr);
  va_end(arg_ptr);

  TkMessage(NB_MSG_ERROR,ErrorBuf,errorcode);

  // We shouldn't get to here, but just in case...
  Tcl_Exit(1);
}


// Warnings
void NonFatalError(const OC_CHAR *membername,
		   const ClassDoc &class_doc,
		   const OC_CHAR *filename,OC_INT4m lineno,
		   const OC_CHAR *fmt,...)
{
  char ErrorBuf[BigBufSize];
  va_list arg_ptr;
  char *cptr;
  Oc_Snprintf(ErrorBuf,sizeof(ErrorBuf),
	  "Nonfatal error occurred in %s::%s (File %s, line %d)\n"
	  "Class revision/date: %s/%s\n"
	  "Class maintainer: %s\nWARNING: ",
	  class_doc.classname,membername,filename,lineno,
	  class_doc.revision,class_doc.revdate,
	  class_doc.maintainer);
  cptr=strchr(ErrorBuf,'\0');
  va_start(arg_ptr,fmt);
  Oc_Vsnprintf(cptr,sizeof(ErrorBuf)-(cptr-ErrorBuf),fmt,arg_ptr);
  va_end(arg_ptr);

  TkMessage(NB_MSG_WARNING,ErrorBuf,0);
}

// Informational messages.  Non-blocking.
void ClassMessage(const OC_CHAR *membername,
		 const ClassDoc &class_doc,
		 const OC_CHAR *filename,OC_INT4m lineno,
		 const OC_CHAR *fmt,...)
{
  char ErrorBuf[BigBufSize];
  va_list arg_ptr;
  char *cptr;
  Oc_Snprintf(ErrorBuf,sizeof(ErrorBuf),
	  "Message from %s::%s (File %s, line %d)\n"
	  "Class revision/date: %s/%s\n"
	  "Class maintainer: %s ---\n",
	  class_doc.classname,membername,filename,lineno,
	  class_doc.revision,class_doc.revdate,
	  class_doc.maintainer);
  cptr=strchr(ErrorBuf,'\0');
  va_start(arg_ptr,fmt);
  Oc_Vsnprintf(cptr,sizeof(ErrorBuf)-(cptr-ErrorBuf),fmt,arg_ptr);
  va_end(arg_ptr);

  TkMessage(NB_MSG_INFO,ErrorBuf,0);
}

#ifndef NDEBUG
void ClassDebugMessage(const OC_CHAR *membername,
		 const ClassDoc &class_doc,
		 const OC_CHAR *filename,OC_INT4m lineno,
		 const OC_CHAR *fmt,...)
{
  char ErrorBuf[BigBufSize];
  va_list arg_ptr;
  char *cptr;
  Oc_Snprintf(ErrorBuf,sizeof(ErrorBuf),
	  "Debug info from %s::%s (File %s, line %d)\n"
	  "Class revision/date: %s/%s\n"
	  "Class maintainer: %s ---\n",
	  class_doc.classname,membername,filename,lineno,
	  class_doc.revision,class_doc.revdate,
	  class_doc.maintainer);
  cptr=strchr(ErrorBuf,'\0');
  va_start(arg_ptr,fmt);
  Oc_Vsnprintf(cptr,sizeof(ErrorBuf)-(cptr-ErrorBuf),fmt,arg_ptr);
  va_end(arg_ptr);

  TkMessage(NB_MSG_DEBUG,ErrorBuf,0);
}
#endif // NDEBUG

// Nonfatal error handler for non-class functions
void PlainWarning(const OC_CHAR *fmt,...)
{
  char ErrorBuf[BigBufSize];
  va_list arg_ptr;
  va_start(arg_ptr,fmt);
  Oc_Vsnprintf(ErrorBuf,sizeof(ErrorBuf),fmt,arg_ptr);
  va_end(arg_ptr);

  TkMessage(NB_MSG_WARNING,ErrorBuf,0);
}

// Informational messages.  Non-blocking, should always return 0.
int Message(const OC_CHAR *fmt,...)
{
  char ErrorBuf[BigBufSize];
  va_list arg_ptr;
  va_start(arg_ptr,fmt);
  Oc_Vsnprintf(ErrorBuf,sizeof(ErrorBuf),fmt,arg_ptr);
  va_end(arg_ptr);

  return TkMessage(NB_MSG_INFO,ErrorBuf,0);
}

// C-interface to Tcl/Tk error boxes.
OC_INT4m TkMessage(Nb_MessageType mt,const OC_CHAR *msg,OC_INT4m exitcode,
                const OC_CHAR *errCode)
{
  Tcl_Interp* interp=Oc_GlobalInterpreter();
  if(interp==NULL) {
    // Tcl interp not setup, so just write message to stderr
    const char* msgtype = "unset";
    switch(mt) {
    case NB_MSG_DEBUG:   msgtype = "DEBUG";        break;
    case NB_MSG_INFO:    msgtype = "INFO";         break;
    case NB_MSG_WARNING: msgtype = "WARNING";      break;
    case NB_MSG_ERROR:   msgtype = "ERROR";        break;
    default:             msgtype = "UNKNOWN TYPE"; break;
    }
    fprintf(stderr,
            "--- %s MESSAGE ---\n%s\n-----------------------\n",
            msgtype,msg);
    fflush(stderr);
    if(mt==NB_MSG_ERROR) {
      fputs("\n+++ FATAL ERROR +++\n",stderr);
      fflush(stderr);
      Tcl_Exit(exitcode);
    }
    return -1;
  }

  // Otherwise, use Tcl interpreter to form and report message.
  static char buf[BigBufSize+32];
  switch(mt) {
  case NB_MSG_DEBUG:
    Oc_Snprintf(buf,sizeof(buf),"Oc_Log Log {%s} debug",msg);
    break;
  case NB_MSG_INFO:
    Oc_Snprintf(buf,sizeof(buf),"Oc_Log Log {%s} info",msg);
    break;
  case NB_MSG_WARNING:
    Oc_Snprintf(buf,sizeof(buf),"Oc_Log Log {%s} warning",msg);
    break;
  case NB_MSG_ERROR:
    Oc_Snprintf(buf,sizeof(buf),"Oc_Log Log {%s} panic",msg);
    break;
  default:
    Oc_Snprintf(buf,sizeof(buf),"Oc_Log Log {Unknown MessageType (%d) detected"
	    " in TkMessage(); Associated message: %s} panic",
	    int(mt),msg);
    break;
  }

  Tcl_SavedResult saved;
  Tcl_SaveResult(interp, &saved);

  // Presumably the call here is from the C-level, and Tcl globals
  // errorInfo and errorCode have not been set.  Clear these so previous
  // values don't obfuscate the real error message.  If errCode is non-null
  // and not empty, then set errorCode to this.
  Tcl_ResetResult(interp); // This should effectively reset errorCode
                          /// and errorInfo
#if (TCL_MAJOR_VERSION >= 8)
  if(errCode!=NULL && errCode[0]!='\0') {
    Oc_AutoBuf ab = errCode;
    Tcl_SetObjErrorCode(interp, Tcl_NewStringObj(ab.GetStr(), -1));
  }
  Oc_AutoBuf ab = "(not available)";
  Tcl_AddErrorInfo(interp,ab.GetStr());
#else // TCL>=8
  // Tcl_SetErrorCode and Tcl_AddErrorInfo routines appear in the Tcl
  // 7.5 tcl.h header file, but (unlike Tcl_SetObjErrorCode) there
  // doesn't appear to be any way to set errorCode to a
  // unknown-at-compile-time length list using Tcl_SetErrorCode.  Since
  // this code is intended for backwards compatibility only, just set
  // the variables directly and don't worry about it...
  if(errCode==NULL || errCode[0]=='\0') {
    Tcl_SetVar(interp,OC_CONST84_CHAR("errorCode"),
               OC_CONST84_CHAR("NONE"),
               TCL_GLOBAL_ONLY);
  } else {
    Tcl_SetVar(interp,OC_CONST84_CHAR("errorCode"),
               OC_CONST84_CHAR(errCode),
               TCL_GLOBAL_ONLY);
  }
  Tcl_SetVar(interp,OC_CONST84_CHAR("errorInfo"),
	     OC_CONST84_CHAR("(not available)"),
	     TCL_GLOBAL_ONLY);
#endif // TCL>=8


  Tcl_Eval(interp,buf);  // We could check for errors
  // from Tcl_Eval, but to where would we send a message???
  int code = atoi(Tcl_GetStringResult(interp));

  Tcl_RestoreResult(interp, &saved);
  return code;
}

