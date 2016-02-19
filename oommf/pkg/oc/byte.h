/* FILE: byte.h                       -*-Mode: c++-*-
 *
 *	Routines which convert endian of data
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2012/04/17 21:31:51 $
 * Last modified by: $Author: donahue $
 */

#ifndef _OC_BYTE
#define _OC_BYTE

#include "ocport.h"
/* End includes */     /* Optional directive to pimake */

/* Inline declarations */
inline void Oc_Flip2(void* input);
inline void Oc_Flip4(void* input);
inline void Oc_Flip8(void* input);  // May be too big to inline

/* Prototypes for the routines defined in byte.cc */
void Oc_SwapWords4(void *buf,OC_INDEX count);
void Oc_SwapWords8(void *buf,OC_INDEX count);

void Oc_SwapBuf(void *buf,OC_INT4m wordsize,OC_INDEX wordcount);
void Oc_SwapBuf(const void* inbuf,void* outbuf,OC_INT4m wordsize,OC_INDEX wordcount);
/// Converts the array 'buf' from big endian to little endian
/// (or vice versa), where each word is of width 'wordsize',
/// and there are 'wordcount' elements (words) in the array.
/// The latter (two buffer) version assumes that inbuf and outbuf
/// DO NOT OVERLAP.

/* Inline implementations */
inline void Oc_Flip2(void* input)
{ // In place byte reversal for all 2 byte wide types.
  OC_UCHAR temp;
  OC_UCHAR* byte=(OC_UCHAR*)input;
  temp=byte[0]; byte[0]=byte[1]; byte[1]=temp;
}

inline void Oc_Flip4(void* input)
{ // In place byte reversal for all 4 byte wide types.
  OC_UCHAR* byte=(OC_UCHAR*)input;
  OC_UCHAR tempB=byte[1]; byte[1]=byte[2]; byte[2]=tempB;
  OC_UCHAR tempA=byte[0]; byte[0]=byte[3]; byte[3]=tempA;
}

inline void Oc_Flip8(void* input)
{ // In place byte reversal for all 8 byte wide types.
  // Probably too big to be inlined.
  OC_UCHAR* byte=(OC_UCHAR*)input;
  OC_UCHAR tempD=byte[3]; byte[3]=byte[4]; byte[4]=tempD;
  OC_UCHAR tempC=byte[2]; byte[2]=byte[5]; byte[5]=tempC;
  OC_UCHAR tempB=byte[1]; byte[1]=byte[6]; byte[6]=tempB;
  OC_UCHAR tempA=byte[0]; byte[0]=byte[7]; byte[7]=tempA;
}

#endif /* _OC_BYTE */
