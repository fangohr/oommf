/* FILE: messagelocker.cc                    -*-Mode: c++-*-
 *
 * Some error handling routines that should be phased out
 * and replaced with code in the oc extension.
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/11/09 00:50:03 $
 * Last modified by: $Author: donahue $
 */

#include "messagelocker.h"

/* End includes */     /* Optional directive to build.tcl */

#define BigBufSize 16000

// Error & warning messages storage
Nb_DString MessageLocker::msg;

int MessageLocker::Append(const OC_CHAR *fmt,...)
{
  char ErrorBuf[BigBufSize];
  va_list arg_ptr;
  va_start(arg_ptr,fmt);
  Oc_Vsnprintf(ErrorBuf,sizeof(ErrorBuf),fmt,arg_ptr);
  va_end(arg_ptr);
  msg.Append(ErrorBuf,sizeof(ErrorBuf));
  return 0;
}

int MessageLocker::GetMessage( Nb_DString& _msg)
{
  _msg=msg;  // Duplicates
  return 0;
}

