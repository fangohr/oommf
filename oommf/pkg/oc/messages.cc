/* FILE: messages.cc             -*-Mode: c++-*-
 *
 *      Routines which provide message display services.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2013/05/22 07:15:35 $
 * Last modified by: $Author: donahue $
 */

#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include "oc.h"

#include "messages.h"

/* End includes */     /* Optional directive to pimake */

#if (OC_SYSTEM_TYPE == OC_WINDOWS)
static int
WindowsMessageBox(const char *buf)
{
  MessageBeep(MB_ICONEXCLAMATION);
  // Title?
  MessageBox(NULL, Oc_AutoTBuf(buf), Oc_AutoTBuf("OOMMF Error"),
             MB_ICONSTOP | MB_OK | MB_TASKMODAL | MB_SETFOREGROUND);

  return TCL_OK;
}

int
Oc_WindowsMessageBoxCmd(ClientData, Tcl_Interp *interp,
                        int argc,CONST84 char **argv)
{
    Tcl_ResetResult(interp);
    if (argc != 2) {
        Tcl_AppendResult(interp, argv[0], " must be called with"
            " 1 argument: messsage", (char *) NULL);
        return TCL_ERROR;
    }
    return WindowsMessageBox(argv[1]);
}
#endif

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
                     CONST84 char **argv)
{
    Tcl_ResetResult(interp);
    if (argc != 2) {
        Tcl_AppendResult(interp, argv[0], " must be called with"
            " 1 argument: messsage", (char *) NULL);
        return TCL_ERROR;
    }
    Oc_SetPanicHeader(argv[1]);
    return TCL_OK;
}

#if OC_HAS_VSNPRINTF
// Wrapper around vsnprintf that throws an exception if buffer is too
// short.  If platform is missing a system vsnprintf routine, then
// define the program_compiler_c++_property_no_vsnprintf property
// to 1 in the corresponding oommf/config/platform/ file.
OC_INDEX
Oc_Vsnprintf(char *str, size_t n, const char *format, va_list ap)
{
  // Am I the only one who is concerned that the return type for
  // vsnprintf, "int", is potentially more narrow than import size
  // of type "size_t"?  Not to mention signed versus unsigned?
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
#else  // OC_HAVE_VSNPRINTF
// Apparently missing C99  vsnprintf
/*
 * The routines snprintf() and vsnprintf() supplied by some C libraries
 * are very helpful to prevent buffer overflows.  Here we provide a
 * poor man's substitute which doesn't prevent problems, but detects
 * them and causes a fatal error.  Too bad the real routines aren't ANSI.
 *
 * Note: Annoyingly, the docs say the vsprintf return type is int,
 *       which, hey!, may be narrower than size_t.  Use with care.
 */
OC_INDEX
Oc_Vsnprintf(char *str, size_t n, const char *format, va_list ap)
{
  if(str==NULL || format==NULL) {
    Tcl_Panic(OC_CONST84_CHAR("NULL pointer passed to Oc_Vsnprintf"));
  }
  str[n-1] = '\0';
  OC_INDEX len = OC_SPRINTF_WRAP(vsprintf(str,format,ap));
  if (str[n-1] != '\0') {
    // If we get here, then a buffer overflow has occurred.
    // Since we don't know how serious this may be, we
    // come to a full stop (panic) rather than throw
    // a catchable exception.
    Tcl_Panic(OC_CONST84_CHAR("Buffer overflow in Oc_Vsnprintf"));
  }
  return len;
}
#endif // OC_HAVE_VSNPRINTF

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
  static char buf[4096];// Good size? -- must be at least long enough for
			// panicHeader plus the buffer overflow message.
			// Really long error stacks might still overflow
			// this.  Consider printing truncated message
			// even if buffer overflows.
  va_list arg_ptr;
  va_start(arg_ptr,format);

  strncpy(buf,GetPanicHeader(),sizeof(buf));
  Oc_Vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), format, arg_ptr);
  va_end(arg_ptr);

#if (OC_SYSTEM_TYPE==OC_WINDOWS)
    WindowsMessageBox(TruncateMessage(buf,40,sizeof(buf)));
# if (TCL_MAJOR_VERSION == 8) && (TCL_MINOR_VERSION <= 4)
    Tcl_Finalize();  // Otherwise ExitProcess can hang.
# endif
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
    sprintf(outbuf,"%02X",bytes[i]);
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
    Tcl_SavedResult saved;
    va_list arg_ptr;
    va_start(arg_ptr,format);

    Oc_Vsnprintf(buf, sizeof(buf), format, arg_ptr);
    va_end(arg_ptr);

    Tcl_DStringInit(&cmd);
    Tcl_DStringAppend(&cmd, start, -1);
    Tcl_SaveResult(interp, &saved);
    Tcl_DStringAppendElement(&cmd, buf);
    if (Tcl_Eval(interp, Tcl_DStringValue(&cmd))
	    == TCL_ERROR) {
	if (Tcl_Write(Tcl_GetStdChannel(TCL_STDERR), buf, -1)
	    != int(strlen(buf))) {
	    fprintf(stderr,"%s",buf);
	}
    }
    Tcl_RestoreResult(interp, &saved);
    Tcl_DStringFree(&cmd);
}

