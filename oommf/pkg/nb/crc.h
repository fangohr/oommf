/* File: crc.h
 *
 * Table-based 32-bit crc calculator.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2011/03/27 05:37:05 $
 * Last modified by: $Author: donahue $
 */

#ifndef _NB_CRC
#define _NB_CRC

#include "oc.h"

/* End includes */     /* Optional directive to pimake */

OC_UINT4m Nb_ComputeCRC(OC_UINDEX size,const unsigned char* bytebuf);

#if OC_HAS_INT8 && OC_INDEX_WIDTH<8
// Big file support.  On 32-bit machines, OC_UINDEX will only span 4
// GB, but Nb_ComputeCRC only works with a small chunk of the file at
// a time, so in principle can handle larger files.  Using an 8-byte
// int for byte count allows for this.
OC_UINT4m Nb_ComputeCRC(Tcl_Channel chan,OC_UINT8m* bytes_read=NULL);
OC_UINT4m Nb_ComputeCRC(Tcl_Channel chan,OC_UINDEX* bytes_read);
#else
OC_UINT4m Nb_ComputeCRC(Tcl_Channel chan,OC_UINDEX* bytes_read=NULL);
#endif

// Tcl wrappers for the above.  The first returns the CRC,
// the second returns a two element list consisting of the CRC
// and the number of bytes read.
Tcl_CmdProc NbComputeCRCBufferCmd;
Tcl_CmdProc NbComputeCRCChannelCmd;

#endif // _NB_CRC
