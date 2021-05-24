/* FILE: byte.cc                      -*-Mode: c++-*-
 *
 *	Routines which convert endian of data
 * 
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2010/07/16 22:33:59 $
 * Last modified by: $Author: donahue $
 */

#include "byte.h"
/* End includes */     /* Optional directive to pimake */

void Oc_SwapWords4(void *buf,OC_INDEX count)
{
  OC_UCHAR* bufptr=(OC_UCHAR*)buf;
  for(;count>0;bufptr+=4,--count) Oc_Flip4(bufptr);
}

void Oc_SwapWords8(void *buf,OC_INDEX count)
{
  OC_UCHAR* bufptr=(OC_UCHAR*)buf;
  for(;count>0;bufptr+=8,--count) Oc_Flip8(bufptr);
}

void Oc_SwapBuf(void *buf,OC_INT4m wordsize,OC_INDEX wordcount)
{
  // Converts the array 'buf' from big endian to little endian
  // (or vice versa), where each word is of width 'wordsize',
  // and there are 'wordcount' elements (words) in the array.
  if(wordsize<2) return;  // Nothing to do
  unsigned char *ptr0,*ptr1,*ptr2;
  for(ptr0=(unsigned char *)buf;wordcount>0;wordcount--,ptr0+=wordsize) {
      ptr1=ptr0;
      ptr2=ptr0+wordsize-1;
      for(int j=0;;) {
        unsigned char ctemp;
        ctemp= *ptr1;  *ptr1 = *ptr2; *ptr2 = ctemp;
        if(++j >= wordsize/2) break;
        ++ptr1; --ptr2;
      }
    }
}

void Oc_SwapBuf(const void* inbuf,void* outbuf,OC_INT4m wordsize,OC_INDEX wordcount)
{
  // Converts the array 'buf' from big endian to little endian
  // (or vice versa), where each word is of width 'wordsize',
  // and there are 'wordcount' elements (words) in the array.
  // NOTE: This routine assumes inbuf and outbuf DO NOT OVERLAP.
  if(wordsize<2) return;  // Nothing to do
  const unsigned char *inptr = reinterpret_cast<const unsigned char*>(inbuf);
  unsigned char *outptr = reinterpret_cast<unsigned char*>(outbuf);
  for(OC_INDEX i=0;i<wordcount;++i) {
    for(OC_INT4m j=0;j<wordsize;++j) {
      *(outptr+wordsize-1-j) = *(inptr+j);
    }
    inptr += wordsize;
    outptr += wordsize;
  }
}

