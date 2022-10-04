/* File: crc.cc
 *
 * Table-based 32-bit crc calculator.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2016/02/12 06:11:05 $
 * Last modified by: $Author: donahue $
 */

#include <cstdio>
#include <cstring>

#include "crc.h"

/* End includes */     

// The CRC-32 polynomial is 0x04C11DB7.  The algorithm below is a
// "reversed" algorithm, so we need to bit-reverse this polynomial.
const OC_UINT4m POLY        = 0xEDB88320;

const OC_UINT4m START_VALUE = 0xFFFFFFFF;
const OC_UINT4m END_FLIP    = 0xFFFFFFFF;

const unsigned int TABLE_SIZE = 256;
static bool table_initialized = 0;
static OC_UINT4m crctable[TABLE_SIZE];

// If available, Tcl_Obj's are used in the channel version of
// Nb_ComputeCRC.
#ifndef NB_CRC_TCL_OBJ_VERSION
# if (TCL_MAJOR_VERSION > 8 || (TCL_MAJOR_VERSION==8 && TCL_MINOR_VERSION>1))
#  define NB_CRC_TCL_OBJ_VERSION 1
# else
#  define NB_CRC_TCL_OBJ_VERSION 0
# endif
#endif

static void NbCRCInitLookupTable()
{
  for(OC_UINT4m i=0;i<TABLE_SIZE;++i) {
    OC_UINT4m r = i;
    for(int j=0;j<8;++j) {
      if (r & 0x01) {
	r >>= 1;
	r ^= POLY;
      } else {
	r >>= 1;
      }
    }
    crctable[i] = r;
  }
  table_initialized = 1;
}

OC_UINT4m Nb_ComputeCRC(OC_UINDEX size,const unsigned char* bytebuf)
{
  if(!table_initialized) NbCRCInitLookupTable();
  OC_UINT4m r = START_VALUE;
  for(OC_UINDEX i=0;i<size;++i) {
    r = crctable[(r&0xFF)^bytebuf[i]] ^ (r>>8);
  }
  r ^= END_FLIP;
  return r;
}

#if OC_HAS_INT8 && OC_INDEX_WIDTH<8
// Big file support.  On 32-bit machines, OC_UINDEX will only span 4
// GB, but Nb_ComputeCRC only works with a small chunk of the file at
// a time, so in principle can handle larger files.  Using an 8-byte
// int for byte count allows for this.

typedef OC_UINT8m NB_CRCFCNTYPE;

// Wrapper for small index compatibility.  WARNING: MAY OVERFLOW!
OC_UINT4m Nb_ComputeCRC(Tcl_Channel chan,OC_UINDEX* bytes_read) {
  OC_UINT8m bigbytes;
  OC_UINDEX smallbytes;

  OC_UINT4m val = Nb_ComputeCRC(chan,&bigbytes);
  if(bytes_read==NULL) return val;

  smallbytes = static_cast<OC_UINDEX>(bigbytes);
  if(static_cast<OC_UINT8m>(smallbytes)!=bigbytes) {
    *bytes_read = static_cast<OC_UINDEX>(-1);  // OVERFLOW
  } else {
    *bytes_read = smallbytes;
  }
  return val;
}

// printf output support (using C99 hack)
#ifndef PRIu64
#if OC_SYSTEM_TYPE == OC_WINDOWS
# define PRIu64 "I64u"
#else
# define PRIu64 "llu"
#endif
#endif

#else

typedef OC_UINDEX NB_CRCFCNTYPE;

#endif

OC_UINT4m Nb_ComputeCRC(Tcl_Channel chan,NB_CRCFCNTYPE* bytes_read)
{
  const int READSIZE = 1024*32;

  if(!table_initialized) NbCRCInitLookupTable();
  
  int count;
  NB_CRCFCNTYPE total_count = 0;
  OC_UINT4m r = START_VALUE;

#if NB_CRC_TCL_OBJ_VERSION
  Tcl_Obj* obj = Tcl_NewObj();
  Tcl_IncrRefCount(obj);
  const unsigned char* buf = NULL;
#else
  unsigned char buf[READSIZE];
#endif

  do {

#if NB_CRC_TCL_OBJ_VERSION
    count = Tcl_ReadChars(chan,obj,READSIZE,0);
#else
    count = Tcl_Read(chan,(char*)buf,READSIZE);
#endif // Tcl_Obj version

    if(count<0) { // Error
      if(bytes_read!=NULL) *bytes_read = static_cast<NB_CRCFCNTYPE>(-1);
#if NB_CRC_TCL_OBJ_VERSION
      Tcl_DecrRefCount(obj);
      obj = NULL; // Safety
#endif
      return 0;
    }

#if NB_CRC_TCL_OBJ_VERSION
    buf = Tcl_GetByteArrayFromObj(obj,&count);
#endif

    total_count += static_cast<NB_CRCFCNTYPE>(count);
    for(int i=0;i<count;++i) {
      r = crctable[(r&0xFF)^buf[i]] ^ (r>>8);
    }
  } while(count==READSIZE);
  r ^= END_FLIP;

  if(bytes_read!=NULL) *bytes_read = total_count;

#if NB_CRC_TCL_OBJ_VERSION
  Tcl_DecrRefCount(obj);
  obj = NULL; // Safety
#endif

  return r;
}

// Tcl wrappers
int NbComputeCRCBufferCmd(ClientData,Tcl_Interp *interp,
			  int argc,CONST84 char** argv)
{
  char msg[1024];
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(msg,sizeof(msg),
		"Nb_ComputeCRCBuffer must be called with 1 argument,"
		" varname (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,msg,(char *)NULL);
    return TCL_ERROR;
  }

  const char* crcbuf = Tcl_GetVar(interp,argv[1],TCL_LEAVE_ERR_MSG);
  if(crcbuf==NULL) return TCL_ERROR;

  OC_UINDEX crcbufsize = static_cast<OC_UINDEX>(strlen(crcbuf));
  OC_UINT4m crc = Nb_ComputeCRC(crcbufsize,(const unsigned char*)crcbuf);

  Oc_Snprintf(msg,sizeof(msg),"0x%08lX %lu",
	      static_cast<unsigned long>(crc),
	      static_cast<unsigned long>(crcbufsize));
  Tcl_AppendResult(interp,msg,(char *)NULL);
  return TCL_OK;
}

int NbComputeCRCChannelCmd(ClientData,Tcl_Interp *interp,
			int argc,CONST84 char** argv)
{
  char msg[1024];
  Tcl_ResetResult(interp);
  if(argc!=2) {
    Oc_Snprintf(msg,sizeof(msg),
		"Nb_ComputeCRCChannel must be called with 1 argument,"
		" channel (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,msg,(char *)NULL);
    return TCL_ERROR;
  }

  int mode;
  Tcl_Channel chan = Tcl_GetChannel(interp,argv[1],&mode);
  if(!(mode & TCL_READABLE)) {
    Oc_Snprintf(msg,sizeof(msg),"Channel not opened for reading");
    Tcl_AppendResult(interp,msg,(char *)NULL);
    return TCL_ERROR;
  }

  NB_CRCFCNTYPE bytes_read;
  OC_UINT4m crc = Nb_ComputeCRC(chan,&bytes_read);

  if(bytes_read == static_cast<NB_CRCFCNTYPE>(-1)) {
    // Error
    Tcl_AppendResult(interp,Tcl_PosixError(interp),(char *)NULL);
    return TCL_ERROR;
  }

#if OC_HAS_INT8 && OC_INDEX_WIDTH<8
  // Special handling for big files
  Oc_Snprintf(msg,sizeof(msg),"0x%08lX %" PRIu64,
	      static_cast<unsigned long>(crc),bytes_read);
# else
  Oc_Snprintf(msg,sizeof(msg),"0x%08lX %lu",
	      static_cast<unsigned long>(crc),
	      static_cast<unsigned long>(bytes_read));
#endif
  Tcl_AppendResult(interp,msg,(char *)NULL);
  return TCL_OK;
}
