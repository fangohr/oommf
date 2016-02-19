/* FILE: chanwrap.h                 -*-Mode: c++-*-
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

#ifndef _NB_CHANWRAP
#define _NB_CHANWRAP

#include "oc.h"  /* Includes tcl.h */

/* End includes */     /* Optional directive to build.tcl */

////////////////////////////////////////////////////////////////////////
// Wrapper for read-access Tcl channels
class Nb_InputChannel {
public:
  Nb_InputChannel();

  ~Nb_InputChannel() { Close(); }

  void OpenFile(const char* filename,const char* translation);

  void AttachChannel(const Tcl_Channel& newchan,const char* description) {
    Close();
    chan = newchan;
    file_descriptor.Dup(description);
  }

  Tcl_Channel DetachChannel() {
    if(chan==NULL) return NULL;
    if(Tcl_Seek(chan,buf_offset - buf_fillsize,SEEK_CUR)<0) {
      OC_THROW(Oc_Exception
               (__FILE__,__LINE__,"Nb_InputChannel","DetachChannel",
                1000+2000,
                "Detach failed due to invalid seek"
                " on file \"%.2000s\".",
                file_descriptor.GetStr()));
    }
    Tcl_Channel chancopy = chan;
    chan=NULL;
    buf_fillsize = buf_offset = 0;
    file_descriptor.Dup("");
    return chancopy;
  }

  int Close(Tcl_Interp* interp=NULL) {
    int errcode = TCL_OK;
    if(chan!=NULL) {
      errcode = Tcl_Close(interp,chan);
    }
    chan=NULL;
    buf_fillsize = buf_offset = 0;
    file_descriptor.Dup("");
    return errcode;
  }

  int ReadChar() {
    if(buf_offset<buf_fillsize) {
      return static_cast<int>
        (static_cast<unsigned char>(buf[buf_offset++]));
    }
    if((buf_fillsize = FillBuf())>0) {
      buf_offset=1;
      return static_cast<int>(static_cast<unsigned char>(buf[0]));
    }
    return -1;
  }

private:
  Oc_AutoBuf file_descriptor;
  Tcl_Channel chan;
  int buf_fillsize;
  int buf_offset;
  Oc_AutoBuf buf;

  int FillBuf();

  // Disable copy constructor and assignment operator
  Nb_InputChannel(const Nb_InputChannel&);
  Nb_InputChannel& operator=(const Nb_InputChannel&);
};

////////////////////////////////////////////////////////////////////////
// Lightweight C++ wrapper for Tcl_OpenFileChannel.  Throws an
// Oc_Exception on error.  Automatically closes channel on object
// delete.
class Nb_FileChannel {
public:
  Nb_FileChannel() : chan(0) {}
  Nb_FileChannel(const char* filename_in,const char* mode,
                 int permissions=0666)
    : chan(0) { Open(filename_in,mode,permissions); }

  ~Nb_FileChannel() { Close(); }

  void Open(const char* filename,const char* mode,
            int permissions=0666);
  void Flush();
  void Close();

  Tcl_Channel GetChannel() { return chan; }
  operator Tcl_Channel() { return chan; }

private:
  Oc_AutoBuf filename;
  Tcl_Channel chan;

  // Disable copy constructor and assignment operator
  Nb_FileChannel(const Nb_FileChannel&);
  Nb_FileChannel& operator=(const Nb_FileChannel&);
};

#endif // _NB_CHANWRAP
