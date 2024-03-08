/* FILE: messages.cc             -*-Mode: c++-*-
 *
 *      Routines which provide message display services.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2013/05/22 07:15:35 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <ctime>

#include <chrono>    // chrono
#include <fstream>   // filebuf
#include <memory>    // unique_ptr
#include <streambuf> // streambuf
#include <utility>   // move
#include <vector>    // vector

#include "messages.h"
#include "oc.h"

/* End includes */     /* Optional directive to pimake */

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
static int
WindowsMessageBox(const char *buf)
{
  MessageBeep(MB_ICONEXCLAMATION);
  // Title?
  MessageBoxExW(NULL,Oc_AutoWideBuf(buf),Oc_AutoWideBuf("OOMMF Error"),
                MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND,
                0);
  return TCL_OK;
}

int
Oc_WindowsMessageBoxCmd(ClientData, Tcl_Interp *interp,
                        int argc,const char **argv)
{
    Tcl_ResetResult(interp);
    if (argc != 2) {
        Tcl_AppendResult(interp, argv[0], " must be called with"
            " 1 argument: message", (char *) NULL);
        return TCL_ERROR;
    }
    return WindowsMessageBox(argv[1]);
}

#endif // OC_WINDOWS

/*
 * Return msg, truncated to have at most maxLines lines.
 * Don't write past (msg + bufLength).
 */
static char *
TruncateMessage(char* msg, int maxLines, int bufLength)
{
    const char *tag = "(message truncated)";
    char* p = msg;
    char* limit = msg + bufLength - strlen(tag) - 1;
    int numLines = 0;
    while ((p < limit) && (numLines < maxLines) && (*p != '\0')) {
        if (*p == '\n') {
            numLines++;
        }
        p++;
    }
    if (*p != '\0') {
        strcpy(p, tag);
    }
    return msg;
}

/*
 * Copies  msg into buf, truncating and adding "..." if msg
 * is longer than bufsize.  Note that bufsize must be at
 * least 5, so there is room for content plus ellipsis
 * plus trailing null.  This routine guarantees a trailing
 * null.
 */
void
Oc_EllipsizeMessage(char* buf,OC_INDEX bufsize,const char* msg)
{
  if(bufsize<5) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_EllipsizeMessage",1024,
                          "Import bufsize must not be smaller than 5."));
  }
  buf[bufsize-1] = '\0';
  strncpy(buf,msg,size_t(bufsize));
  if(buf[bufsize-1]!='\0') { // Overflow
    strcpy(buf+bufsize-4,"...");
  }
}


static Oc_AutoBuf panicHeader = "";

static const char *
GetPanicHeader()
{
    return panicHeader.GetStr();
}

void
Oc_SetPanicHeader(const char* header)
{
    panicHeader.Dup(header);
}

int
Oc_SetPanicHeaderCmd(ClientData, Tcl_Interp *interp, int argc,
                     const char **argv)
{
    Tcl_ResetResult(interp);
    if (argc != 2) {
        Tcl_AppendResult(interp, argv[0], " must be called with"
            " 1 argument: message", (char *) NULL);
        return TCL_ERROR;
    }
    Oc_SetPanicHeader(argv[1]);
    return TCL_OK;
}

// Wrappers around snprintf and vsnprintf that throw an exception if
// buffer is too short.
// Q: Am I the only one who is concerned that the return type for
// snprintf and vsnprint, "int", is potentially more narrow than import
// size of type "size_t"?  Not to mention signed versus unsigned?
OC_INDEX
Oc_Vsnprintf(char *str, size_t n, const char *format, va_list ap)
{
  int result = vsnprintf(str,n,format,ap);
  if(result<0 || static_cast<size_t>(result)>=n) {
    // Some error occurred --- probably output truncation, but
    // differences in library support make it difficult to be certain.
    if(str==NULL) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_Vsnprintf",1024,
                            "Error in Oc_Vsnprintf; buffer pointer is NULL."));
    }
    if(format==NULL) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_Vsnprintf",1024,
                            "Error in Oc_Vsnprintf; format pointer is NULL."));
    }
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Oc_Vsnprintf",1024,
                 "Error in Oc_Vsnprintf; probably buffer overflow."));
  }
  return static_cast<OC_INDEX>(result);
}

OC_INDEX
Oc_Snprintf(char *str, size_t n, const char *format, ...)
{
  va_list arg_ptr;
  va_start(arg_ptr,format);
  OC_INDEX len = Oc_Vsnprintf(str,n,format,arg_ptr);
  va_end(arg_ptr);
  return len;
}

static void
CustomPanic(char *format,...)
{
  const int bufsize = 4096; // Good size? -- must be at least long
                            // enough for panicHeader plus the buffer
                            // overflow message. Really long error
                            // stacks might still overflow this.
                            // Consider printing truncated message even
                            // if buffer overflows.
  static char buf[bufsize];

  va_list arg_ptr;
  va_start(arg_ptr,format);

  strncpy(buf,GetPanicHeader(),bufsize-1);
  buf[bufsize-1] = '\0'; // Safety
  Oc_Vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), format, arg_ptr);
  va_end(arg_ptr);

#if (OC_SYSTEM_TYPE==OC_WINDOWS)
    WindowsMessageBox(TruncateMessage(buf,40,sizeof(buf)));
    ExitProcess(1);
#else
    fputs(TruncateMessage(buf,40,sizeof(buf)),stderr);
    fputs("\n",stderr);
    fflush(stderr);
    abort();
#endif
}

void
Oc_InitPanic(const char *nameOfExecutable)
{
    char buf[4096];
    Oc_Snprintf(buf, sizeof(buf), "<%lu> %s %s panic:\n",
	    (unsigned long) Oc_GetPid(), nameOfExecutable, OC_VERSION);
    Oc_SetPanicHeader(buf);
    Tcl_SetPanicProc((Tcl_PanicProc *)CustomPanic);
}


// Code to convert an array of bytes into a null-terminated
// string of ASCII hex digits; each byte is converted into
// two hex characters, with a null byte appended to the end.
// Import len is the length of the byte array.
//   This routine may be useful for debugging operations.
void
Oc_BytesToAscii(const unsigned char *bytes,OC_INDEX len,Oc_AutoBuf& result)
{
  assert(len>=0);
  result.SetLength(2*size_t(len));
  char* outbuf = result.GetStr();
  for(OC_INDEX i=0;i<len;++i) {
    snprintf(outbuf,len-2*i,"%02X",bytes[i]);
    outbuf += 2;
  }
  *outbuf = '\0';
}

void
Oc_ErrorWrite(const char *format, ...) {
    static char buf[4096];
    static char start[] = "puts -nonewline stderr";
    Tcl_DString cmd;
    Tcl_Interp *interp = Oc_GlobalInterpreter();
    Tcl_InterpState saved;
    va_list arg_ptr;
    va_start(arg_ptr,format);

    Oc_Vsnprintf(buf, sizeof(buf), format, arg_ptr);
    va_end(arg_ptr);

    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd, start, -1);
    saved = Tcl_SaveInterpState(interp, TCL_OK);
    Tcl_DStringAppendElement(&cmd, buf);
    if (Tcl_Eval(interp, Tcl_DStringValue(&cmd))
	    == TCL_ERROR) {
	if (Tcl_Write(Tcl_GetStdChannel(TCL_STDERR), buf, -1)
	    != int(strlen(buf))) {
	    fprintf(stderr,"%s",buf);
	}
    }
    Tcl_RestoreInterpState(interp, saved);
    Tcl_DStringFree(&cmd);
}


// Oc_LogSupport
std::string Oc_LogSupport::log_prefix;

std::string
Oc_LogSupport::GetTimeStamp()
{
  using std::chrono::system_clock;
  using std::chrono::time_point;
  using std::chrono::duration_cast;

  const time_point<system_clock> current_time = system_clock::now();
  std::chrono::seconds sec =
    duration_cast<std::chrono::seconds>(current_time.time_since_epoch());
  std::chrono::milliseconds ms =
    duration_cast<std::chrono::milliseconds>(current_time.time_since_epoch())
    - duration_cast<std::chrono::milliseconds>(sec);
  // ms is the overhang time in milliseconds

  char tfmt[32];
  Oc_Snprintf(tfmt,sizeof(tfmt),"%%H:%%M:%%S.%03d %%Y-%%m-%%d",
              static_cast<int>(ms.count()));

  std::time_t ct = system_clock::to_time_t(current_time);
  char tbuf[32];
  std::strftime(tbuf,sizeof(tbuf),tfmt,localtime(&ct));

  return std::string(tbuf);
}

std::string
Oc_LogSupport::GetLogMark()
{
  std::string mark = std::string("[")
    + log_prefix +  std::string(" ")
    + GetTimeStamp() + std::string("] ");
  return mark;
}

int
Oc_LogSupportInitPrefixCmd
(ClientData, Tcl_Interp *interp,
 int argc,const char **argv)
{
    Tcl_ResetResult(interp);
    if (argc != 2) {
        Tcl_AppendResult(interp, argv[0], " must be called with"
            " 1 argument: message", (char *) NULL);
        return TCL_ERROR;
    }
    Oc_LogSupport::InitPrefix(std::string(argv[1]));
    return TCL_OK;
}

int
Oc_LogSupportGetLogMarkCmd
(ClientData, Tcl_Interp *interp,
 int argc,const char **argv)
{
    Tcl_ResetResult(interp);
    if (argc != 1) {
        Tcl_AppendResult(interp, argv[0], " must be called with"
            " no arguments", (char *) NULL);
        return TCL_ERROR;
    }
    std::string mark = Oc_LogSupport::GetLogMark();
    Tcl_AppendResult(interp,mark.c_str(),(char *)NULL);
    return TCL_OK;
}

////////////////////////////////////////////////////////////////////////
// Oc_Report logging routines. Not called Oc_Log to prevent confusion
// with the OOMMF Oc_Log Tcl (Oc_Class) class.
//   This class is based in part on public domain code by Thomas Guest,
// 2009-05-13:
//    https://wordaligned.org/articles/cpp-streambufs
namespace Oc_Report {
  class TeeBuffer: public std::streambuf {
  public:
    // streambuf which copies output to std::clog and a list of files.
    TeeBuffer() : write_to_clog(true) {}
    bool WriteToClog(bool status) {
      bool old_status = write_to_clog;
      write_to_clog = status;
      return old_status;
    }
    void AddFile(const std::string& filename) {
      std::unique_ptr<std::filebuf> pfile(new std::filebuf);
      if(!pfile->open(filename,ios_base::out|ios_base::app)) {
        std::string errmsg = std::string("Unable to open file ") + filename;
        OC_THROWEXCEPT("Oc_Report","AddFile",errmsg.c_str());
      }
      files.push_back(std::move(pfile));
    }
    // If needed, we could add a method to remove files.
  private:
    // TeeBuffer has no buffer. So every character "overflows"
    // and can be put directly into the teed buffers.
    bool write_to_clog;
    virtual int overflow(int c) {
      if (c == EOF) return !EOF;
      bool eofcheck = false;
      if(write_to_clog) {
        if(std::clog.rdbuf()->sputc(static_cast<char>(c)) == EOF) {
          eofcheck = true;
        }
      }
      for(auto it=files.begin();it!=files.end();++it) {
        if((*it)->sputc(static_cast<char>(c)) == EOF) {
          eofcheck = true;
        }
      }
      return (eofcheck ? EOF : c);
    }

    // It would be nice if the outputs were fully buffered until an
    // explicit flush request occurs. That way a client could make
    // multiple << calls w/o log output getting interleaved between
    // processes. There is a pubsetbuf member which may do this, but the
    // cool kids say the standard doesn't guarantee the buffer gets used
    // and in practice it probably doesn't. IOW, std::filebuf doesn't
    // actually provide support for, uh, buffering? YMMV

    // Sync all buffers
    virtual int sync() {
      bool synccheck = true;
      if(write_to_clog) {
        if(std::clog.rdbuf()->pubsync() != 0) synccheck = false;
      }
      for(auto it=files.begin();it!=files.end();++it) {
        if((*it)->pubsync() != 0) synccheck = false;
      }
      return (synccheck ? 0 : -1);
    }
    std::vector< std::unique_ptr<std::filebuf> > files;
  };

  TeeStream::TeeStream(TeeBuffer* buf) : std::ostream(buf), tbuf(buf) {}
  void TeeStream::AddFile(const std::string& filename) {
    tbuf->AddFile(filename);
  }
  bool TeeStream::WriteToClog(bool status) {
    return tbuf->WriteToClog(status);
  }

  // Instances
  TeeBuffer teebuf; // teebuf should be accessed only by Oc_Report::Log
  TeeStream Log(&teebuf); // Primary external access point
} // namespace Oc_Report


// Tcl interfaces to Oc_Report::Log.AddFile() and ::Log.WriteToClog().
// This allows the log file to be registered and controlled from Tcl
// code. NB
int Oc_ReportAddLogFile
(ClientData, Tcl_Interp *interp,
 int argc,const char **argv)
{
    Tcl_ResetResult(interp);
    if (argc != 2) {
        Tcl_AppendResult(interp, argv[0], " must be called with"
            " 1 argument: message", (char *) NULL);
        return TCL_ERROR;
    }
    try {
      Oc_Report::Log.AddFile(std::string(argv[1]));
    } catch(const Oc_Exception& err) {
      Oc_AutoBuf msg;
      Tcl_AppendResult(interp,err.ConstructMessage(msg),(char *)NULL);
      return TCL_ERROR;
    }
    return TCL_OK;
}
int Oc_ReportWriteToClog
(ClientData, Tcl_Interp *interp,
 int argc,const char **argv)
{
    Tcl_ResetResult(interp);
    if (argc != 2) {
        Tcl_AppendResult(interp, argv[0], " must be called with"
            " 1 argument: status", (char *) NULL);
        return TCL_ERROR;
    }
    bool old_status;
    try {
      old_status = Oc_Report::Log.WriteToClog(std::stoi(argv[1]));
    } catch(const Oc_Exception& err) {
      Oc_AutoBuf msg;
      Tcl_AppendResult(interp,err.ConstructMessage(msg),(char *)NULL);
      return TCL_ERROR;
    }
    const char* value = "0";
    if(old_status) value = "1";
    Tcl_AppendResult(interp,value,(char *)NULL);
    return TCL_OK;
}

// Oc_Report logging class.
////////////////////////////////////////////////////////////////////////
