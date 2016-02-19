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

#include <stdio.h>
#include <stdarg.h>
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

#endif /* _OC_MESSAGES */
