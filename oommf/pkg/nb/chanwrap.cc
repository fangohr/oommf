/* FILE: chanwrap.cc                 -*-Mode: c++-*-
 *
 * C++ wrappers for Tcl channels.  These wrappers are exception safe,
 * and also provide low-overhead, buffered byte-at-a-time access to
 * the channels.  Support is only for strictly read-only or write-only
 * channels.  Also, once a channel is attached to one of these objects,
 * only access through the object is allowed; e.g., direct calls to
 * Tcl_Read will almost certainly result in undetected reordering or
 * lossage of bytes on the input channel.  If the channel supports
 * random seeks (via Tcl_Tell and Tcl_Seek), then the channel can
 * be detached from the object, after which all the standard Tcl
 * I/O calls can be used again.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2015/04/17 17:17:03 $
 * Last modified by: $Author: dgp $
 */

#include <cassert>

#include "oc.h"

#include "chanwrap.h"
#include "functions.h"

/* End includes */     /* Optional directive to build.tcl */


////////////////////////////////////////////////////////////////////////
// Wrapper for read-access Tcl channels

#define NB_INPUTCHANNEL_BUFSIZE 65536
#define NB_INPUTCHANNEL_ERRBUFSIZE 4096

Nb_InputChannel::Nb_InputChannel()
  : chan(NULL),buf_fillsize(0),buf_offset(0)
{
    assert(NB_INPUTCHANNEL_BUFSIZE>0);
    buf.SetLength(NB_INPUTCHANNEL_BUFSIZE);
}

void Nb_InputChannel::OpenFile(const char* filename,
                               const char* translation)
{
  Close();

  Oc_DirectPathname(filename,file_descriptor);

  chan = Tcl_OpenFileChannel(NULL,file_descriptor.GetStr(),
			     OC_CONST84_CHAR("r"),0);
  if(chan==NULL) {
    // Unable to open file.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_InputChannel","OpenFile",
                       NB_INPUTCHANNEL_ERRBUFSIZE+2000,
                       "Unable to open file \"%.2000s\" for reading.",
                       file_descriptor.GetStr()));
  }

  if(Tcl_SetChannelOption(NULL,chan,
			  OC_CONST84_CHAR("-translation"),
			  OC_CONST84_CHAR(translation))!=TCL_OK ||
     Tcl_SetChannelOption(NULL,chan,
			  OC_CONST84_CHAR("-buffering"),
			  OC_CONST84_CHAR("full"))!=TCL_OK ||
     Tcl_SetChannelOption(NULL,chan,OC_CONST84_CHAR("-buffersize"),
         OC_CONST84_CHAR(OC_STRINGIFY(NB_INPUTCHANNEL_BUFSIZE)))!=TCL_OK) {
    // Channel configuration error.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_InputChannel","OpenFile",
                       NB_INPUTCHANNEL_ERRBUFSIZE+2000,
                       "Unable to configure input channel"
                       " for file \"%.2000s\".",
                       file_descriptor.GetStr()));
  }
  buf_fillsize = buf_offset = 0;
}

int Nb_InputChannel::FillBuf()
{
  buf_offset=0;
  buf_fillsize = Tcl_Read(chan,(char*)buf,NB_INPUTCHANNEL_BUFSIZE);
  if(buf_fillsize<0) {
    buf_fillsize = 0;
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_InputChannel","FillBuf",
                       NB_INPUTCHANNEL_ERRBUFSIZE+4000,
                       "Read error on \"%.2000s\": %.2000s",
                       file_descriptor.GetStr(),
                       Tcl_ErrnoMsg(Tcl_GetErrno())));
  }
  return buf_fillsize;
}

#undef NB_INPUTCHANNEL_BUFSIZE
#undef NB_INPUTCHANNEL_ERRBUFSIZE

////////////////////////////////////////////////////////////////////////
// Very simple C++ wrapper for Tcl_OpenFileChannel.
// Throws an Oc_Exception on error.

void Nb_FileChannel::Open
(const char* filename_in,
 const char* mode,
 int permissions)
{
  Close();
  filename = filename_in;

  int orig_errno=Tcl_GetErrno();
  Tcl_SetErrno(0);
  chan = Tcl_OpenFileChannel(NULL,filename,
                             const_cast<char*>(mode),
                             permissions);
  /// We'll *ASSUME* Tcl isn't really going to modify filename/mode.
  int new_errno=Tcl_GetErrno();
  Tcl_SetErrno(orig_errno);

  if(chan==NULL) {
    // File open failure
    String msg=String("Failure to open file \"") + String(filename_in)
      + String("\" with mode %.8s permissions 0%o");
    if(new_errno!=0) msg += String(": ") + String(Tcl_ErrnoMsg(new_errno));
    else             msg += String(".");
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Nb_FileChannel","Open",
                          msg.size()+256,msg.c_str(),mode,permissions));
  }

  try {
    // Set default channel options.  Client may call Tcl_SetChannelOption
    // directly to change these if desired.
    Tcl_SetChannelOption(NULL,chan,OC_CONST84_CHAR("-buffering"),
			 OC_CONST84_CHAR("full"));
    Tcl_SetChannelOption(NULL,chan,OC_CONST84_CHAR("-buffersize"),
			 OC_CONST84_CHAR("100000")); // What's a good size???
    // Binary mode
    Tcl_SetChannelOption(NULL,chan,OC_CONST84_CHAR("-translation"),
                         OC_CONST84_CHAR("binary"));
  } catch (...) {
    Close();
    throw;
  }
}

void Nb_FileChannel::Flush()
{
  if(Tcl_Flush(chan)!=TCL_OK) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,
                          "Nb_FileChannel","Flush",
                          4000 + filename.GetLength(),
                          "Error flushing channel for file \"%s\": %.3500s",
                          filename.GetStr(),
                          Tcl_ErrnoMsg(Tcl_GetErrno())));
  }
}

void Nb_FileChannel::Close()
{
  if(chan!=0) {
    if(Tcl_Close(NULL,chan)!=TCL_OK) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"","Nb_CloseChannel",
                            4000 + filename.GetLength(),
                            "Error closing channel for file \"%s\": %.3500s",
                            filename.GetStr(),
                            Tcl_ErrnoMsg(Tcl_GetErrno())));
    
    }
    chan = 0;
  }
  filename.Dup("");
}
