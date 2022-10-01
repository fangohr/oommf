/* FILE: messages.h                   -*-Mode: c++-*-
 *
 *	Routines which provide message display services
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/06/02 08:42:29 $
 * Last modified by: $Author: donahue $
 */

#ifndef _OC_MESSAGES
#define _OC_MESSAGES

#include <cstdio>
#include <cstdarg>
#include <iostream>
#include <string>

#include "ocport.h"
#include "autobuf.h"
#include "imports.h"

/* End includes */     /* Optional directive to pimake */

/* Prototypes for the routines defined in messages.cc */
#if (OC_SYSTEM_TYPE == OC_WINDOWS)
Tcl_CmdProc Oc_WindowsMessageBoxCmd;
#endif

void
Oc_EllipsizeMessage(char* buf,OC_INDEX bufsize,const char* msg);

Tcl_CmdProc Oc_SetPanicHeaderCmd;
void Oc_InitPanic(const char *);
void Oc_SetPanicHeader(const char *);
OC_INDEX Oc_Snprintf(char *, size_t, const char *, ...);
OC_INDEX Oc_Vsnprintf(char *, size_t, const char *, va_list);
void Oc_BytesToAscii(const unsigned char *,OC_INDEX,Oc_AutoBuf&);
void Oc_ErrorWrite(const char *,...);

class Oc_LogSupport {
public:
  void static InitPrefix(const std::string& prefix) { log_prefix = prefix; }
  static std::string GetTimeStamp();
  static std::string GetLogMark(); // Prefix + timestamp
private:
  static std::string log_prefix;
};

Tcl_CmdProc Oc_LogSupportInitPrefixCmd;
Tcl_CmdProc Oc_LogSupportGetLogMarkCmd;

namespace Oc_Report {
  class TeeBuffer; // Forward declaration
  class TeeStream : public std::ostream {
  public:
    TeeStream(TeeBuffer* buf);
    TeeStream() = delete;
    TeeStream(const TeeStream&) = delete;
    TeeStream& operator=(const TeeStream&) = delete;
    void AddFile(const std::string& filename);
  private:
    TeeBuffer* tbuf;
  };
  extern TeeStream Log;
} // namespace Oc_Report

Tcl_CmdProc Oc_ReportAddCLogFile;

#endif /* _OC_MESSAGES */
