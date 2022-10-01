/* FILE: nullchannel.cc             -*-Mode: c++-*-
 *
 *      Provides a null channel -- cross-platform, cross-Tcl-version
 *		equivalent of Unix's /dev/null
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2008/07/22 23:23:26 $
 * Last modified by: $Author: donahue $
 */

#include <cstdio>

#include "autobuf.h"
#include "imports.h"
#include "nullchannel.h"

extern "C" int
OcNullClose(ClientData, Tcl_Interp *)
{
    return 0;
}

extern "C" int
OcNullClose2(ClientData, Tcl_Interp *,int)
{
    return 0;
}

extern "C" int
OcNullInput(ClientData, char *, int, int *)
{
    return 0;			/* Always return EOF */
}

extern "C" int
OcNullOutput(ClientData, CONST84 char*, int toWrite, int *)
{
    return toWrite;		/* Report success, but don't write anything */
}

extern "C" void
OcNullWatch(ClientData, int)
{
}

extern "C" int
OcNullHandle(ClientData, int, ClientData *)
{
    return TCL_ERROR;
}

#if (TCL_MAJOR_VERSION > 7)
static Tcl_ChannelType nullChannelType = {
    (char *)"null",             /* Type name. */
    NULL,                       /* Always non-blocking.*/
#if (TCL_MAJOR_VERSION > 8)
    NULL,	                /* Close proc. */
#else	/* TCL_MAJOR_VERSION == 8 */
    OcNullClose,                /* Close proc. */
#endif
    OcNullInput,                /* Input proc. */
    OcNullOutput,               /* Output proc. */
    NULL,                       /* Seek proc. */
    NULL,                       /* Set option proc. */
    NULL,                       /* Get option proc. */
    OcNullWatch,                /* Watch for events.  */
    OcNullHandle,               /* Get a handle from the device. */
#if (TCL_MAJOR_VERSION > 8)
    OcNullClose2,		/* Close2 proc or reserved slot */
    NULL,			/* Block mode proc (reserved slot) */
    NULL,			/* Flush proc (reserved slot) */
    NULL,			/* Handler proc (reserved slot) */
    NULL,                       /* WideSeekProc (reserved slot) */
    NULL,                       /* ThreadActionProc (reserved slot) */
    NULL,                       /* TruncateProc (reserved slot) */
#else	/* TCL_MAJOR_VERSION == 8 */
#if !((TCL_MINOR_VERSION == 0) && (TCL_RELEASE_SERIAL < 3))
    NULL,			/* Close2 proc or reserved slot */
#if ((TCL_MINOR_VERSION >= 4) \
    || ((TCL_MINOR_VERSION == 3) && (TCL_RELEASE_LEVEL == TCL_FINAL_RELEASE) \
    && (TCL_RELEASE_SERIAL >= 2)))
    NULL,			/* Block mode proc (reserved slot) */
    NULL,			/* Flush proc (reserved slot) */
    NULL,			/* Handler proc (reserved slot) */
#endif
#if ((TCL_MINOR_VERSION >= 5) \
    || ((TCL_MINOR_VERSION == 4) && (TCL_RELEASE_LEVEL == TCL_FINAL_RELEASE) \
    && (TCL_RELEASE_SERIAL >= 1)))
    NULL,                       /* WideSeekProc (reserved slot) */
#endif
#if ((TCL_MINOR_VERSION >= 5) \
    || ((TCL_MINOR_VERSION == 4) && (TCL_RELEASE_LEVEL == TCL_FINAL_RELEASE) \
    && (TCL_RELEASE_SERIAL >= 10)))
    NULL,                       /* ThreadActionProc (reserved slot) */
#endif
#if (TCL_MINOR_VERSION >= 5)
    NULL,                       /* TruncateProc (reserved slot) */
#endif
#endif
#endif
};
#endif

Tcl_Channel
Nullchannel_Open()
{
#if (TCL_MAJOR_VERSION > 7)
    char channelName[16 + TCL_INTEGER_SPACE];
    static int nullChannelCount = 0;

    sprintf(channelName, "null%d", nullChannelCount++);
    return Tcl_CreateChannel(&nullChannelType, channelName, NULL,
	    TCL_READABLE | TCL_WRITABLE);
#else
# if (OC_SYSTEM_TYPE == OC_WINDOWS)
    return Tcl_OpenFileChannel(NULL, OC_CONST84_CHAR("NUL:"),
			       OC_CONST84_CHAR("a+"), 0666);
# else
#  if (OC_SYSTEM_TYPE == OC_UNIX)
    return Tcl_OpenFileChannel(NULL, OC_CONST84_CHAR("/dev/null"),
			       OC_CONST84_CHAR("a+"), 0666);
#  else
    return NULL;
#  endif
# endif
#endif
}


int
Nullchannel_Init(Tcl_Interp *interp)
{
// Channel creation interfacce changed incompatibly with Tcl 8 release.
// nullChannelType conforms to Tcl 8 interface.
#if (TCL_MAJOR_VERSION > 7)
    // Since we can't redirect to the channel "null", this may not
    // be all that useful :(.
/*
 * Not only is it not useful, but it interferes with Tk_InitConosleChannels,
 * so out it goes...
    static Tcl_Channel nullChan = Tcl_CreateChannel(&nullChannelType,
	    (char *)"null", NULL, TCL_READABLE | TCL_WRITABLE);
    Tcl_RegisterChannel(interp, nullChan);
 */
#endif

    /* Here we could register the package, once this is separated from Oc */
    return (interp ? TCL_OK : TCL_OK);
}





