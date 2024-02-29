/* FILE: if.cc                      -*-Mode: c++-*-
 *
 *	The OOMMF Image Formats extension.
 *
 *	This extension supplies classes which can manipulate
 * various image formats.  When the extension is loaded into
 * an interpreter in which Tk has been loaded, it registers
 * corresponding photo image formats with Tk.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2013/05/22 07:15:30 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>
#include <cctype>
#include <limits>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <vector>

//#define USE_OLD_IMAGE
#include "oc.h"
#include "if.h"

/* End includes */     // Optional directive to pimake

#if OC_USE_TK

////////////////////////////////////////////////////////////////////////
// MsBitmap and related classes
struct If_RGBQuad {
  OC_BYTE Red;
  OC_BYTE Green;
  OC_BYTE Blue;
  OC_BYTE Reserved;
  int MSFill32(Tcl_Channel chan);  // Fills structure from chan
  /// of MS RGBQuad's, returning the number of bytes read (4),
  /// or 0 on error.
  inline int MSFill32(const unsigned char* carr); // Analogous
  /// to above, but takes input from a 4-byte long buffer.
  inline int MSFill24(const unsigned char* carr); // Analogous
  /// to above, but takes input from a 3-byte long buffer.
  inline int MSFill16_RGB555(const unsigned char* carr); // Fills
  /// from a 16-bit (2-byte) buffer, using a 5-5-5 RGB bit-mask.
  ///  Returns number of bytes read (i.e., 2).
  inline int MSFill16_RGB565(const unsigned char* carr); // Fills
  /// from a 16-bit (2-byte) buffer, using a 5-6-5 RGB bit-mask.
  ///  Returns number of bytes read (i.e., 2).
};

int If_RGBQuad::MSFill32(Tcl_Channel chan)
{ // Note: MS RGBQuad's are ordered "Blue Green Red Reserved"
  //       Assumes 8-8-8 RGB bit-mask.
  char carr[4];
  int count=Tcl_Read(chan,carr,4);
  if(count!=4) return 0;
  Blue=static_cast<OC_BYTE>(carr[0]);    Green=static_cast<OC_BYTE>(carr[1]);
  Red =static_cast<OC_BYTE>(carr[2]); Reserved=static_cast<OC_BYTE>(carr[3]);
  return count;
}

int If_RGBQuad::MSFill32(const unsigned char* carr)
{ // Note: MS RGBQuad's are ordered "Blue Green Red Reserved"
  Blue=carr[0];    Green=carr[1];
  Red=carr[2];     Reserved=carr[3];
  return 4;
}

int If_RGBQuad::MSFill24(const unsigned char* carr)
{ // Note: MS RGBQuad's are ordered "Blue Green Red Reserved"
  Blue=carr[0];    Green=carr[1];
  Red=carr[2];     Reserved=0;
  return 3;
}

int If_RGBQuad::MSFill16_RGB555(const unsigned char* carr)
{ // Note: MS RGBQuad's are ordered "Blue Green Red Reserved"
  // Uses 5-5-5 bit mask
  const unsigned int val = static_cast<unsigned int>(carr[0])
    + static_cast<unsigned int>(carr[1])*256u;
  const unsigned int rmask = 0x7C00;
  const unsigned int gmask = 0x03E0;
  const unsigned int bmask = 0x001F;
  Red   = static_cast<OC_BYTE>((val & rmask)>>7); //  7 == 10 - 3
  Green = static_cast<OC_BYTE>((val & gmask)>>2); //  2 ==  5 - 3
  Blue  = static_cast<OC_BYTE>((val & bmask)<<3); // -3 ==  0 - 3
  Reserved=0;
  return 2;
}

int If_RGBQuad::MSFill16_RGB565(const unsigned char* carr)
{ // Note: MS RGBQuad's are ordered "Blue Green Red Reserved"
  // Uses 5-5-5 bit mask
  const unsigned int val = static_cast<unsigned int>(carr[0])
    + static_cast<unsigned int>(carr[1])*256u;
  const unsigned int rmask = 0xF800;
  const unsigned int gmask = 0x07E0;
  const unsigned int bmask = 0x001F;
  Red   = static_cast<OC_BYTE>((val & rmask)>>8); //  8 == 11 - 3
  Green = static_cast<OC_BYTE>((val & gmask)>>3); //  3 ==  5 - 2
  Blue  = static_cast<OC_BYTE>((val & bmask)<<3); // -3 ==  0 - 3
  Reserved=0;
  return 2;
}

class If_MSBitmap;  // Forward declaration for typedef
typedef int (If_MSBitmap::*BmpConvert)(const unsigned char* read_buf,
				       If_RGBQuad* &pix,
				       OC_UINT4 startcol,
				       OC_UINT4 stopcol);

class If_MSBitmap {
private:
  OC_BYTE  Type[2];
  OC_UINT4 FileSize;
  OC_UINT2 Reserved1;
  OC_UINT2 Reserved2;
  OC_UINT4 OffBits;
  OC_UINT4 BmiSize;
  OC_UINT4 Width;
  OC_UINT4 Height;
  OC_UINT2 Planes;
  OC_UINT2 BitCount;
  OC_UINT4 Compression;
  OC_UINT4 SizeImage;
  OC_UINT4 XPelsPerMeter;
  OC_UINT4 YPelsPerMeter;
  OC_UINT4 ClrUsed;
  OC_UINT4 ClrImportant;
  If_RGBQuad* Palette;

  // Values for Compression field, as defined in WinGDI.h:
  //
  //    BI_RGB        0L
  //    BI_RLE8       1L
  //    BI_RLE4       2L
  //    BI_BITFIELDS  3L
  //    BI_JPEG       4L
  //    BI_PNG        5L
  //
  // NOTE on BitCount and Compression fields:
  // In Windows 3.x, the only supported values for BitCount
  // are 1, 2, 4, 8, and 24.  There is an extension (Win95?)
  // that allows BitCount to be in addition either 16 or 32.
  // These are both direct-mapped (no palette) formats.
  // If BitCount is 16 or 32, and Compression is set to
  // BI_RGB (==0), then the default format masks are used,
  // i.e., RGB555 for 16 bpp, and RGB888 for 32 bpp.  If
  // Compression is set to BI_BITFIELDS (==3), then 3 DWORDS
  // (DWORD = OC_UINT4) follow the header (where the palette
  // would usually be) that specify the RGB format masks.
  // For Win95, the only supported format masks are RGB555
  // and RGB565 for 16bpp, and RGB888 for 32bpp.

  int AllocPalette(OC_UINT4 size); // Returns 1 on success
  void FreePalette();

  int FillHeader(Tcl_Channel chan,OC_BOOL fillpalette);
  /// If 'fillpalette' is true, then fills all data members;
  /// If 'fillpalette' is 0, then fill everything *except*
  /// the palette and related members.
  ///   Returns 1 if successful, 0 if the input does not appear
  /// to be a valid MS bitmap.  Adjusts for alignment and byte
  /// ordering as needed.

  // Support variables, computed from the above
  // Calculate number of bytes in per row in file; by spec,
  // must be divisible by 4.
  OC_UINT4 FileRowSize;
  OC_UINT4 PaletteSize;

  // FillPhoto() bitmap->rgb conversion routines.
  int  Bmp1toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
		  OC_UINT4 startcol,OC_UINT4 stopcol);        //  1 bit/pixel
  int  Bmp4toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
		  OC_UINT4 startcol,OC_UINT4 stopcol);        //  4 bits/pixel
  int  Bmp8toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
		  OC_UINT4 startcol,OC_UINT4 stopcol);        //  8 bits/pixel
  int  Bmp16toRgbq_RGB555(const unsigned char* read_buf,If_RGBQuad* &pix,
                 OC_UINT4 startcol,OC_UINT4 stopcol); // 16 bits/pixel, RGB555
  int  Bmp16toRgbq_RGB565(const unsigned char* read_buf,If_RGBQuad* &pix,
                 OC_UINT4 startcol,OC_UINT4 stopcol); // 16 bits/pixel, RGB565
  int  Bmp24toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
		  OC_UINT4 startcol,OC_UINT4 stopcol);        // 24 bits/pixel
  int  Bmp32toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
		  OC_UINT4 startcol,OC_UINT4 stopcol);        // 32 bits/pixel
  BmpConvert DataConvert;

public:
  If_MSBitmap();
  ~If_MSBitmap();
  int ReadCheck(Tcl_Channel chan,int& imagewidth,int& imageheight);
  /// Returns 1 if the file on chan looks like a Microsoft BMP
  /// file that we know how to read.

  int FillPhoto(Tcl_Interp*  interp,Tcl_Channel  chan,
	   const char* fileName,Tk_PhotoHandle imageHandle,
	   int destX,int destY,int width,int height,int srcX,int srcY);
  /// Fills in imageHandle as requested, one row at a time.

  int WritePhoto(Tcl_Interp*  interp,const char* filename,
		 Tk_PhotoImageBlock* blockPtr);
  // Write PhotoImageBlock to file "filename" in Microsoft
  // 24 bits-per-pixel format.
};

int If_MSBitmap::AllocPalette(OC_UINT4 size)
{
  FreePalette();
  if((Palette=new If_RGBQuad[size])!=NULL) {
    PaletteSize=size;
    return 1;
  }
  return 0;
}

void If_MSBitmap::FreePalette()
{
  if(Palette!=NULL) { delete[] Palette; Palette=NULL; }
  PaletteSize=0;
}

If_MSBitmap::~If_MSBitmap() { FreePalette(); }

If_MSBitmap::If_MSBitmap() :
  FileSize(0),Reserved1(0),Reserved2(0),OffBits(0),
  BmiSize(0),Width(0),Height(0),Planes(0),BitCount(0),Compression(0),
  SizeImage(0),XPelsPerMeter(0),YPelsPerMeter(0),
  ClrUsed(0),ClrImportant(0),Palette(NULL),
  FileRowSize(0),PaletteSize(0),DataConvert(BmpConvert(NULL))
{
  Type[0]=Type[1]='\0';
}

int If_MSBitmap::FillHeader(Tcl_Channel chan,OC_BOOL fillpalette)
{ // Reads members one at a time to avoid differing packing
  // alignment restrictions on various machines.  Returns 1
  // on success.

  // Read and check signature
  Tcl_Read(chan,(char *)Type,2*sizeof(OC_BYTE));
  if(Type[0]!='B' || Type[1]!='M') return 0;

  // Read rest of header
  Tcl_Read(chan,(char *)&FileSize,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&Reserved1,sizeof(OC_UINT2));
  Tcl_Read(chan,(char *)&Reserved2,sizeof(OC_UINT2));
  Tcl_Read(chan,(char *)&OffBits,sizeof(OC_UINT4));

  // MS Bitmap format is little-endian ordered;
  // Adjust byte ordering, if necessary.
#if OC_BYTEORDER != 4321
#if OC_BYTEORDER != 1234
#error "Unsupported byte-order format"
#endif
  Oc_Flip4(&FileSize);
  Oc_Flip2(&Reserved1); Oc_Flip2(&Reserved2);
  Oc_Flip4(&OffBits);
#endif

  // Check file size.  Is there no Tcl C interface to the 'file size'
  // proc???
  Oc_SeekPos bmistart=Tcl_Tell(chan);
  Oc_SeekPos real_size=Tcl_Seek(chan,0,SEEK_END);
  Tcl_Seek(chan,bmistart,SEEK_SET);
  if(real_size != static_cast<Oc_SeekPos>(-1) &&
     real_size != static_cast<Oc_SeekPos>(FileSize)) return 0;

  // Otherwise, read in rest of header
  Tcl_Read(chan,(char *)&BmiSize,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&Width,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&Height,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&Planes,sizeof(OC_UINT2));
  Tcl_Read(chan,(char *)&BitCount,sizeof(OC_UINT2));
  Tcl_Read(chan,(char *)&Compression,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&SizeImage,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&XPelsPerMeter,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&YPelsPerMeter,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&ClrUsed,sizeof(OC_UINT4));
  Tcl_Read(chan,(char *)&ClrImportant,sizeof(OC_UINT4));

  // MS Bitmap format is little-endian ordered;
  // Adjust byte ordering, if necessary.
#if OC_BYTEORDER != 4321
#if OC_BYTEORDER != 1234
#error "Unsupported byte-order format"
#endif
  Oc_Flip4(&BmiSize);
  Oc_Flip4(&Width);          Oc_Flip4(&Height);
  Oc_Flip2(&Planes);         Oc_Flip2(&BitCount);
  Oc_Flip4(&Compression);    Oc_Flip4(&SizeImage);
  Oc_Flip4(&XPelsPerMeter);  Oc_Flip4(&YPelsPerMeter);
  Oc_Flip4(&ClrUsed);        Oc_Flip4(&ClrImportant);
#endif

  FileRowSize=4*((Width*BitCount+31)/32); // Rows lie on a 4-byte bdry.

  switch(BitCount)
    {
    case 1:   DataConvert = &If_MSBitmap::Bmp1toRgbq;        break;
    case 4:   DataConvert = &If_MSBitmap::Bmp4toRgbq;        break;
    case 8:   DataConvert = &If_MSBitmap::Bmp8toRgbq;        break;
    case 16:  DataConvert = &If_MSBitmap::Bmp16toRgbq_RGB555; break;
    case 24:  DataConvert = &If_MSBitmap::Bmp24toRgbq;       break;
    case 32:  DataConvert = &If_MSBitmap::Bmp32toRgbq;       break;
    default:  DataConvert = BmpConvert(NULL);  break;
    }

  // For BitCount == 16, the default color mask is RGB555.  However,
  // this code also supports RGB565, which is selected by setting the
  // Compression field to BI_BITFIELDS (==3) and putting the proper
  // mask values in the palette area.
  if(Compression == 3) {
    if(BitCount == 16) {
      Tcl_Seek(chan,bmistart+BmiSize,SEEK_SET);
      OC_UINT4 rmask,gmask,bmask;
      Tcl_Read(chan,(char *)&rmask,sizeof(OC_UINT4));
      Tcl_Read(chan,(char *)&gmask,sizeof(OC_UINT4));
      Tcl_Read(chan,(char *)&bmask,sizeof(OC_UINT4));
      if(rmask == 0x7C00 && gmask == 0x03E0 && bmask == 0x001F) {
        DataConvert = &If_MSBitmap::Bmp16toRgbq_RGB555;
      } else if(rmask == 0xF800 && gmask == 0x07E0 && bmask == 0x001F) {
        DataConvert = &If_MSBitmap::Bmp16toRgbq_RGB565;
      } else {
        DataConvert = BmpConvert(NULL); // Unsupported format
      }
    } else {
      DataConvert = BmpConvert(NULL); // Unsupported format
    }
  }

  // If requested, read palette info from file
  FreePalette();
  if(fillpalette) {
    // Position at end of BITMAPINFOHEADER
    Tcl_Seek(chan,bmistart+BmiSize,SEEK_SET);

    OC_UINT4 palsize=ClrUsed;
    if(palsize==0) {
      switch(BitCount)
	{
	case  1: palsize=2;    break;
	case  4: palsize=16;   break;
	case  8: palsize=256;  break;
	case 16: palsize=0;    break;  // High color
	case 24: palsize=0;    break;  // True color
	case 32: palsize=0;    break;  // True color
	default: return 0;     // Invalid BitCount value
	}
    }
    if(palsize>0 &&
       AllocPalette(palsize)!=0) {
      for(OC_UINT4m i=0;i<palsize;i++)
	Palette[i].MSFill32(chan);
    }
  }

  return 1;
}

int
If_MSBitmap::Bmp1toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
		     OC_UINT4 startcol,OC_UINT4 stopcol)
{ // 1 bit per pixel, with palette
  OC_UINT4 jstart=8*(startcol/8);
  OC_UINT4 jstop=8*((stopcol+7)/8);
  OC_UINT4 j=jstart;
  If_RGBQuad offbit=Palette[0];
  If_RGBQuad onbit=Palette[1];
  while(j<jstop) {
    unsigned int datum=read_buf[j/8];
    for(unsigned int mask=0x80; mask!=0 ; mask>>=1) {
      // Note bit-order!
      if(j<stopcol && j>=startcol) {
	if((datum & mask)==0) pix[j-startcol]=offbit;
	else                  pix[j-startcol]=onbit;
      }
      j++;
    }
  }
  return 0;
}

int
If_MSBitmap::Bmp4toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
		     OC_UINT4 startcol,OC_UINT4 stopcol)
{ // 4 bits per pixel, with palette
  OC_UINT4 jstart=2*(startcol/2);
  OC_UINT4 jstop=2*((stopcol+1)/2);
  OC_UINT4 j=jstart;
  while(j<jstop) {
    unsigned int datum=read_buf[j/2];
    OC_UINT4 index1= datum & 0x0F;        // Note bit-order!
    OC_UINT4 index0= (datum & 0xF0)>>4;
    if(startcol<=j) {
      if(index0>=PaletteSize) return 1;
      pix[j-startcol]=Palette[index0];
    }
    j++;
    if(j<stopcol) {
      if(index1>=PaletteSize) return 1;
      pix[j-startcol]=Palette[index1];
    }
    j++;
  }
  return 0;
}

int
If_MSBitmap::Bmp8toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
		     OC_UINT4 startcol,OC_UINT4 stopcol)
{ // 8 bits per pixel, with palette
  for(OC_UINT4 j=startcol;j<stopcol;j++) {
    // Copy values from palette
    unsigned int index=read_buf[j];
    if(index>=PaletteSize) {
      return 1;
    }
    pix[j-startcol]=Palette[index];
  }
  return 0;
}

int
If_MSBitmap::Bmp16toRgbq_RGB555(const unsigned char* read_buf,
                 If_RGBQuad* &pix,OC_UINT4 startcol,OC_UINT4 stopcol)
{ // "High" color input file; 16 bits per pixel, no palette,
  // RGB555 color mask.
  const int data_width=2;
  for(OC_UINT4 j=startcol;j<stopcol;j++) {
    pix[j-startcol].MSFill16_RGB555(read_buf+j*data_width);
  }
  return 0;
}

int
If_MSBitmap::Bmp16toRgbq_RGB565(const unsigned char* read_buf,
                 If_RGBQuad* &pix,OC_UINT4 startcol,OC_UINT4 stopcol)
{ // "High" color input file; 16 bits per pixel, no palette,
  // RGB565 color mask.
  const int data_width=2;
  for(OC_UINT4 j=startcol;j<stopcol;j++) {
    pix[j-startcol].MSFill16_RGB565(read_buf+j*data_width);
  }
  return 0;
}

int
If_MSBitmap::Bmp24toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
                         OC_UINT4 startcol,OC_UINT4 stopcol)
{ // True color input file; 24 bits per pixel, no palette
  const int data_width=3;
  for(OC_UINT4 j=startcol;j<stopcol;++j) {
    pix[j-startcol].MSFill24(read_buf+j*data_width);
  }
  return 0;
}

int
If_MSBitmap::Bmp32toRgbq(const unsigned char* read_buf,If_RGBQuad* &pix,
                         OC_UINT4 startcol,OC_UINT4 stopcol)
{ // True color input file; 24 bits per pixel, no palette
  const int data_width=4;
  for(OC_UINT4 j=startcol;j<stopcol;++j) {
    pix[j-startcol].MSFill32(read_buf+j*data_width);
  }
  return 0;
}

int If_MSBitmap::ReadCheck(Tcl_Channel chan,
			int& imagewidth,int& imageheight)
{ // Returns 1 if the file on chan looks like a Microsoft BMP file that
  // we know how to read.
  if(FillHeader(chan,0)==0) return 0; // Invalid header

  // Otherwise, looks like an MS bitmap file.  Check that it is a
  // subformat we support.
  if(Planes!=1         ||      // Only known value
     (Compression!=0 && Compression!=3) ||  // No compression
     DataConvert==BmpConvert(NULL)) { // Bits per pixel check
    return 0;
  }
  assert(Width<=INT_MAX && Height<=INT_MAX);
  imagewidth=static_cast<int>(Width);
  imageheight=static_cast<int>(Height);

  return 1;
}

int
If_MSBitmap::FillPhoto(Tcl_Interp*  interp,Tcl_Channel  chan,
		    const char* fileName,Tk_PhotoHandle imageHandle,
		    int destX,int destY,int reqwidth,int reqheight,
		    int srcX,int srcY)
{ // Fills in imageHandle as requested, one row at a time.
  // Returns TCL_OK on success, TCL_ERROR on failure.

  if(FillHeader(chan,1) == 0) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,"Header in file ",fileName,
                     " not recognized as a Microsoft .bmp header",
                     (char *)NULL);
    return TCL_ERROR;
  }

  // Looks like an MS bitmap file.  Check that it is a subformat
  // we support.
  if(Planes!=1         ||      // Only known value
     (Compression!=0 && Compression!=3) ||  // No compression
     DataConvert==BmpConvert(NULL)) { // Bits per pixel check
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,"Sub-format of Microsoft .bmp file ",
                     fileName," is not currently supported.",
                     (char *)NULL);
    return TCL_ERROR;
  }

  // Deduce working control parameters
  assert(Height<=INT_MAX);
  int stoprow  = static_cast<int>(Height) - srcY;
  int startrow = stoprow - reqheight;
  if(startrow<0) { reqheight += startrow; startrow=0; }
  if(stoprow > static_cast<int>(Height)) {
    reqheight -= stoprow-Height;   // Safety
    stoprow = static_cast<int>(Height);
  }
  int startcol = srcX;
  int stopcol = startcol+reqwidth;
  if(startcol<0) { reqwidth += startcol; startcol=0; }
  assert(Width<=INT_MAX);
  if(stopcol>static_cast<int>(Width)) {
    reqwidth -= stopcol-Width;
    stopcol = static_cast<int>(Width);
  }
  if(startrow>=stoprow || startcol>=stopcol) {
    return TCL_OK; // Nothing to do
  }

  assert(reqwidth>=0);
  unsigned char *read_buf=new unsigned char[FileRowSize];
  If_RGBQuad *pix=new If_RGBQuad[size_t(reqwidth)];

  Tk_PhotoImageBlock pib;
  pib.pixelPtr=(unsigned char *)pix;
  pib.width=reqwidth;
  pib.height=1;
  pib.pitch=int(sizeof(If_RGBQuad)*Width);
  pib.pixelSize=sizeof(If_RGBQuad);
  pib.offset[0]=int(((unsigned char*)&pix[0].Red)   - (unsigned char*)pix);
  pib.offset[1]=int(((unsigned char*)&pix[0].Green) - (unsigned char*)pix);
  pib.offset[2]=int(((unsigned char*)&pix[0].Blue)  - (unsigned char*)pix);
  // Jan Nijtmans recommends this safe way to disable alpha processing
  pib.offset[3]=pib.offset[0];

  // Skip unrequested leading rows
  Tcl_Seek(chan,OffBits+startrow*FileRowSize,SEEK_SET);
  assert(FileRowSize <= INT_MAX);
  for(int i= startrow;i<stoprow;i++) {
    if(Tcl_Read(chan,(char *)read_buf,static_cast<int>(FileRowSize))==-1) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp,"Input error during read of"
                       " Microsoft .bmp file ",fileName,
                       " bitmap.",(char *)NULL);
      return TCL_ERROR;
    }
    assert(0<=startcol && startcol==int(OC_UINT4(startcol)));
    assert(0<=stopcol  &&  stopcol==int(OC_UINT4(stopcol)));
    switch((this->*DataConvert)(read_buf,pix,
                                OC_UINT4(startcol),OC_UINT4(stopcol)))
      {
      case 0:  break;
      default:
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp,"Input error during read of"
			 " Microsoft .bmp file ",fileName,
			 " bitmap: Illegal palette index",
			 (char *)NULL);
	return TCL_ERROR;
      }
    Tk_PhotoPutBlock( interp, imageHandle, &pib, destX, destY+stoprow-i-1,
                     reqwidth, 1, TK_PHOTO_COMPOSITE_OVERLAY);
  }
  delete[] pix;
  delete[] read_buf;

  return TCL_OK;
}

int If_MSBitmap::WritePhoto
(Tcl_Interp*  interp,
 const char* fileName,
 Tk_PhotoImageBlock* blockPtr)
{ // Write PhotoImageBlock to file "fileName" in Microsoft
  // 24 bits-per-pixel format.

  // Open Tcl channel to handle output
  Tcl_Channel chan
    = Tcl_OpenFileChannel(interp,fileName, "w",0666);
  if(chan==NULL) return TCL_ERROR;
  int errcode = Tcl_SetChannelOption(interp,chan, "-translation", "binary");
  if(errcode!=TCL_OK) {
    Tcl_Close(interp,chan);
    return errcode;
  }

  // Row buffer space
  char bgr[3];
  int padsize = (3*blockPtr->width) % 4; // Rows should end on
  if(padsize>0) padsize = 4 - padsize;        // 4-byte boundary.
  char padbgr[3];
  padbgr[0]=padbgr[1]=padbgr[2]='\0';

  // Fill BMP header
  const int headsize=54; // Header size; Don't use sizeof(head) because
  /// that can be influenced by machine-specific alignment restrictions
  assert(blockPtr->width>=0
         && blockPtr->width == int(OC_UINT4(blockPtr->width)));
  assert(blockPtr->height>=0 &&
         blockPtr->height == int(OC_UINT4(blockPtr->height)));
  Width     = static_cast<OC_UINT4>(blockPtr->width);
  Height    = static_cast<OC_UINT4>(blockPtr->height);
  Type[0]='B';  Type[1]='M';
  FileSize = (OC_UINT4(headsize) + Height*(3*Width + OC_UINT4(padsize)));
  Reserved1 = Reserved2 = 0;
  OffBits   = headsize;
  BmiSize   = headsize-14;
  Planes    = 1;
  BitCount  = 24;
  Compression = 0;
  SizeImage = FileSize - headsize;
  XPelsPerMeter = YPelsPerMeter = 0;  // Any better ideas?
  ClrUsed      = 0;
  ClrImportant = 0;
  // MS Bitmap format is little-endian ordered;
  // Adjust byte ordering, if necessary.
#if OC_BYTEORDER != 4321
# if OC_BYTEORDER != 1234
#  error "Unsupported byte-order format"
# endif
  Oc_Flip4(&FileSize);
  Oc_Flip2(&Reserved1);      Oc_Flip2(&Reserved2);
  Oc_Flip4(&OffBits);        Oc_Flip4(&BmiSize);
  Oc_Flip4(&Width);          Oc_Flip4(&Height);
  Oc_Flip2(&Planes);         Oc_Flip2(&BitCount);
  Oc_Flip4(&Compression);    Oc_Flip4(&SizeImage);
  Oc_Flip4(&XPelsPerMeter);  Oc_Flip4(&YPelsPerMeter);
  Oc_Flip4(&ClrUsed);        Oc_Flip4(&ClrImportant);
#endif

  // Write header info
  if((-1 == Tcl_Write(chan,(char *)Type,sizeof(Type)))
     || (-1 == Tcl_Write(chan,(char *)&FileSize,sizeof(FileSize)))
     || (-1 == Tcl_Write(chan,(char *)&Reserved1,sizeof(Reserved1)))
     || (-1 == Tcl_Write(chan,(char *)&Reserved1,sizeof(Reserved2)))
     || (-1 == Tcl_Write(chan,(char *)&OffBits,sizeof(OffBits)))
     || (-1 == Tcl_Write(chan,(char *)&BmiSize,sizeof(BmiSize)))
     || (-1 == Tcl_Write(chan,(char *)&Width,sizeof(Width)))
     || (-1 == Tcl_Write(chan,(char *)&Height,sizeof(Height)))
     || (-1 == Tcl_Write(chan,(char *)&Planes,sizeof(Planes)))
     || (-1 == Tcl_Write(chan,(char *)&BitCount,sizeof(BitCount)))
     || (-1 == Tcl_Write(chan,(char *)&Compression,sizeof(Compression)))
     || (-1 == Tcl_Write(chan,(char *)&SizeImage,sizeof(SizeImage)))
     || (-1 == Tcl_Write(chan,(char *)&XPelsPerMeter,sizeof(XPelsPerMeter)))
     || (-1 == Tcl_Write(chan,(char *)&YPelsPerMeter,sizeof(YPelsPerMeter)))
     || (-1 == Tcl_Write(chan,(char *)&ClrUsed,sizeof(ClrUsed)))
     || (-1 == Tcl_Write(chan,(char *)&ClrImportant,sizeof(ClrImportant)))) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,"Output error writing"
		     " Microsoft .bmp file ",fileName,
		     " header.",(char *)NULL);
    Tcl_Close(interp,chan);
    return TCL_ERROR;
  }

  // Write data, from bottom to top.
  for(int i = blockPtr->height;i>0;i--) {
    int pixoff = (i-1) * blockPtr->pitch;
    for(int j=0;j<blockPtr->width;j++,pixoff+=blockPtr->pixelSize) {
      bgr[0] = (char)blockPtr->pixelPtr[pixoff+blockPtr->offset[2]]; // blue
      bgr[1] = (char)blockPtr->pixelPtr[pixoff+blockPtr->offset[1]]; // green
      bgr[2] = (char)blockPtr->pixelPtr[pixoff+blockPtr->offset[0]]; // red
      if(Tcl_Write(chan,bgr,3)==-1) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp,"Output error writing"
			 " Microsoft .bmp file ",fileName,
			 " bitmap.",(char *)NULL);
	Tcl_Close(interp,chan);
	return TCL_ERROR;
      }
    }
    if(padsize>0) {
      if(Tcl_Write(chan,padbgr,padsize)==-1) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp,"Output error writing"
			 " Microsoft .bmp file ",fileName,
			 " bitmap.",(char *)NULL);
	Tcl_Close(interp,chan);
	return TCL_ERROR;
      }
    }
  }

  // Close channel and exit
  return Tcl_Close(interp,chan);
}

////////////////////////////////////////////////////////////////////////
// OOMMF If_PPM class
class If_PPM {
public:
  If_PPM() {}
  ~If_PPM() {}
  int ReadCheck(Tcl_Channel chan,int& imagewidth,int& imageheight);
  /// Returns 1 if the file on chan looks like a PPM P3.

  int FillPhoto(Tcl_Interp*  interp,Tcl_Channel  chan,
                const char* fileName,Tk_PhotoHandle imageHandle,
                int destX,int destY,int width,int height,int srcX,int srcY) {
    return PhotoFill<Tcl_Channel>(interp,chan,fileName,imageHandle,
                                  destX,destY,width,height,srcX,srcY);
  }

  int WritePhoto(Tcl_Interp*  interp,const char* filename,
		 Tk_PhotoImageBlock* blockPtr);
  // Write PhotoImageBlock to file "filename" in PPM P3 format.

  // String versions of the above
  int ReadCheck(const unsigned char* data,int& imagewidth,int& imageheight);
  int FillPhoto(Tcl_Interp*  interp,const unsigned char* data,
                Tk_PhotoHandle imageHandle,
		int destX,int destY,int width,int height,int srcX,int srcY) {
      return PhotoFill<const unsigned char*>(interp,data,nullptr,imageHandle,
                                          destX,destY,width,height,srcX,srcY);
  }

  int WritePhoto(Tcl_Interp*  interp,Tcl_DString* dataPtr,
		 Tk_PhotoImageBlock* blockPtr);

private:
  enum class FileType { INVALID, P1, P2, P3, P4, P5, P6 };

  // Hack workaround for unsigned char <-> signed char functions
  static const unsigned char* uustrchr(const unsigned char *s,int c) {
    return reinterpret_cast<const unsigned char*>
      (strchr(reinterpret_cast<const char*>(s),c));
  }

  static long int uustrtol (const unsigned char* str,
                            const unsigned char* * endptr, int base) {
    return strtol(reinterpret_cast<const char*>(str),
                  const_cast<char**>(reinterpret_cast<const char**>(endptr)),
                  base);
  }
  
  class ValueText { // Class for whitespace separated plain text values.
    // Unlike the other parallel "ReadNumber" classes, which are used
    // only to read data from the PPM value block for particular PPM
    // sub-types, this class is additionally used to read the header for
    // all PPM file types. For convenient access, the ReadNumber
    // functions in this class are static. These are member functions
    // for the others, which supports instance-specific storage (in
    // particular, a byte cache in the binary ValueBit class).
  public:
    static void ReadNumber(Tcl_Channel chan, int& number);
    static void ReadNumber(const unsigned char* &data, int& number);
    // Note: The ReadNumber routines in this class are
    //       static
  };
  class ValueTextBit { // Class for single text digit, 0 or 1.
    // This class maps "0" to the integer 1, and "1" to 0, to match the
    // PBM file format to the brightness interpretation used by the rest
    // of the code.
  public:
    void ReadNumber(Tcl_Channel chan, int& number);
    void ReadNumber(const unsigned char* &data, int& number);
  };
  class ValueBit { // Class for binary single bit values.
    // This is the binary analog to ValueTextBit, intended
    // for use reading PPM P4 files. As in that class, a
    // 0 bit is returned as 1, and a 1 bit as 0.
    // NB: The ReadNumber routines read one byte from the source,
    //     cache it, and advance the pointer.  The next seven
    //     subsequent calls return bits from the cached value
    //     without advancing the pointer.  Therefore, don't
    //     intersperse reads with ValueBit on a source with other
    //     accesses.
  public:
    void ReadNumber(Tcl_Channel chan, int& number);
    void ReadNumber(const unsigned char* &data, int& number);
    ValueBit() : cache(0), cache_mask(0)
#ifndef NDEBUG
              , cache_source(nullptr)
#endif
    {}
  private:
    unsigned char cache;
    unsigned char cache_mask;
#ifndef NDEBUG
    const void* cache_source;
#endif
  };
  class ValueByte { // Class for binary single byte values
  public:
    void ReadNumber(Tcl_Channel chan, int& number);
    void ReadNumber(const unsigned char* &data, int& number);
  };
  class ValueDyad { // Class for binary big endian double byte values
  public:
    void ReadNumber(Tcl_Channel chan, int& number);
    void ReadNumber(const unsigned char* &data, int& number);
  };

  int  ReadHeader(Tcl_Channel chan,
                  int& imagewidth, int& imageheight, int& maxvalue,
                  FileType &filetype);
  int  ReadHeader(const unsigned char* &data,
                  int& imagewidth, int& imageheight, int& maxvalue,
                  FileType &filetype);


  // Templates to handle filling in a Tk_PhotoHandle as requested.
  template<typename T,typename R>
  int PhotoFillData
  (Tcl_Interp*  interp,T inhandle,
   const char* fileName,Tk_PhotoHandle imageHandle,
   int destX,int destY,int width,int height,int srcX,int srcY,
   int imagewidth,int imageheight,int maxvalue,
   If_PPM::FileType filetype);

  template<typename T> int PhotoFill
  (Tcl_Interp*  interp,T inhandle,
   const char* fileName,Tk_PhotoHandle imageHandle,
   int destX,int destY,int width,int height,int srcX,int srcY);
  /// Fills in imageHandle as requested, one row at a time.

  int SprintColorValues(unsigned char* buf,
                        int red,int green,int blue);
};

void If_PPM::ValueText::ReadNumber(Tcl_Channel chan,int& number)
{ // Consumes the next integer on chan, and also the first subsequent
  // non-number character, from chan.  Converts from text to int,
  // storing the result in the export "number". Throws a const char* on
  // error.

  number = 0; // Safety

  // Pass over whitespace and comments
  char ch;
  while(1) {
    if(Tcl_Read(chan,&ch,sizeof(char))!=1) {
      // Premature EOF
      throw("Premature EOF in PPM file.");
    }
    if(!isspace(ch)) {
      if(ch!='#') break; // Start of number detected
      // Otherwise, comment detected; read to end of line
      while(Tcl_Read(chan,&ch,sizeof(char))==1 && ch!='\n') {}
    }
  }

  // At this point, ch should hold first non-whitespace
  // character outside of any comments.

  char buf[65]; // Should be big enough
  buf[0]=ch;
  int i=1;
  while(Tcl_Read(chan,buf+i,sizeof(char))==1 && isdigit(buf[i])) {
    i++;
    if( i >= static_cast<int>(sizeof(buf)-1) ) break;
  }
  buf[i]='\0';

  char* cptr;
  long int lnum = strtol(buf,&cptr,10);
  number = static_cast<int>(lnum);
  if(*cptr != '\0') {
    throw("Invalid data in PPM file.");
  }
}

void If_PPM::ValueText::ReadNumber(const unsigned char* &data,int& number)
{ // After skipping leading whitespace and comments, converts the first
  // integer from text to int, storing the result in the export
  // "number". On success, data is adjusted to point to the second
  // character past the end of the converted number. (This mimics the
  // ReadNumber Tcl_Channel variant.) Throws a const char* on failure.

  number = 0; // Safety

  // Pass over whitespace and comments
  const unsigned char* cptr = data;
  while(cptr!=nullptr) {
    if(!isspace(*cptr)) {
      if(*cptr!='#') break; // Start of number detected
      // Otherwise, comment detected; read to end of line
      if((cptr = uustrchr(cptr,'\n'))==nullptr) {
        throw("Premature EOD in PPM data.");
      }
    }
    ++cptr;
  }

  // At this point, cptr should point to the first non-whitespace
  // character outside of any comments.
  long int lnum = uustrtol(cptr,&cptr,10);
  number = static_cast<int>(lnum);
  data = ++cptr; // Skip over trailing whitespace
}

void If_PPM::ValueTextBit::ReadNumber(Tcl_Channel chan,int& number)
{ // Skips leading whitespace and comments on chan, and tries to
  // converts the next character to an integer value to store in import
  // "number". The conversion fails if that character is neither "0" or
  // "1", in which case a const char* error is thrown. Otherwise, "0" is
  // mapped to the integer 1, and "1" to 0, matching the PBM file format
  // interpretation to the usual brightness connotation.

  // Pass over whitespace and comments
  char ch;
  while(1) {
    if(Tcl_Read(chan,&ch,sizeof(char))!=1) {
      // Premature EOF
      throw("Premature EOF in PPM file.");
    }
    if(!isspace(ch)) {
      if(ch!='#') break; // Start of number detected
      // Otherwise, comment detected; read to end of line
      while(Tcl_Read(chan,&ch,sizeof(char))==1 && ch!='\n') {}
    }
  }

  // At this point, cptr should point to the first non-whitespace
  // character outside of any comments. This value should be
  // either "0" or "1".
  switch(ch) {
  case '0': number = 1; break;
  case '1': number = 0; break;
  default: throw("Invalid character in PPM data.");
  }
}

void If_PPM::ValueTextBit::ReadNumber(const unsigned char* &data,int& number)
{ // Same as If_PPM::ValueTextBit::ReadNumber(Tcl_Channel,int&)
  // but reads from a character array instead of Tcl_Channel,
  // and advances &data instead of chan.

  // Pass over whitespace and comments
  const unsigned char* cptr = data;
  while(cptr!=nullptr) {
    if(!isspace(*cptr)) {
      if(*cptr!='#') break; // Start of number detected
      // Otherwise, comment detected; read to end of line
      if((cptr = uustrchr(cptr,'\n'))==nullptr) {
        throw("Premature EOD in PPM data.");
      }
    }
    ++cptr;
  }

  // At this point, cptr should point to the first non-whitespace
  // character outside of any comments. This value should be
  // either "0" or "1".
  switch(*cptr) {
  case '0': number = 1; break;
  case '1': number = 0; break;
  default: throw("Invalid character in PPM data.");
  }
  data = ++cptr;
}

void If_PPM::ValueBit::ReadNumber(Tcl_Channel chan,int& number)
{ // Reads one byte from chan, caches it, and returns lead bit (as a 0
  // or 1). Subsequent calls return the next bit downward without
  // reading from chan, until all bits have been consumed. Rinse and
  // repeat.  Caller is responsible for ensuring chan is in binary
  // mode. Throws a const char* on error.
  //
  // This class is the binary analog to ValueTextBit, intended for use
  // reading PPM P4 files. As in that class, a 0 bit is returned as 1,
  // and a 1 bit as 0.
  //
  // Note: Tcl 8.6 docs say Tcl_Read is deprecated, but use it anyway as
  //       it does exactly what we want. But we may need to change this
  //       in the future. If so, we may want to change export "number"
  //       to type ObjPtr.

  // chan should point inside the file binary data block.
#ifndef NDEBUG
  if(!cache_source) {
    cache_source = chan;
  }
#endif
  assert(cache_source == chan); // Consistency check

  if(cache_mask == 0) {
    if(Tcl_Read(chan,reinterpret_cast<char*>(&cache),1)!=1) {
      throw("Premature EOF in PPM file.");
    }
    cache_mask = 0x80;
  }
  number = (cache & cache_mask ? 0 : 1);
  cache_mask >>= 1;
}

void If_PPM::ValueBit::ReadNumber(const unsigned char* &data,int& number)
{ // Pulls one byte from data[], caches it, advances data ptr, and
  // returns lead bit (as a 0 or 1). Subsequent calls return the next
  // bit downward on the cache without accessing data[], until all bits
  // have been consumed. Rinse and repeat.
  //
  // This class is the binary analog to ValueTextBit, intended for use
  // reading PPM P4 files. As in that class, a 0 bit is returned as 1,
  // and a 1 bit as 0.

#ifndef NDEBUG
  if(!cache_source) {
    cache_source = data;
  }
#endif
  assert(cache_source == data); // Consistency check

  // *data should point inside the file binary data block.
  if(cache_mask == 0) {
    cache = *data;
    ++data;
    cache_mask = 0x80;
#ifndef NDEBUG
    cache_source = data;
#endif
  }
  number = (cache & cache_mask ? 0 : 1);
  cache_mask >>= 1;
}

void If_PPM::ValueByte::ReadNumber(Tcl_Channel chan,int& number)
{ // Consumes the next binary byte on chan, and stores the result as an
  // unsigned int in number. Caller is responsible for ensuring
  // chan is in binary mode. Throws a const char* on error.

  // Note: Tcl 8.6 docs say Tcl_Read is deprecated, but use it anyway as
  //       it does exactly what we want. But we may need to change this
  //       in the future. If so, we may want to change export "number"
  //       to type ObjPtr.

  // At this point, ch should hold first non-whitespace
  // character outside of any comments.
  unsigned char byte;
  if(Tcl_Read(chan,reinterpret_cast<char*>(&byte),1)!=1) {
    throw("Premature EOF in PPM file.");
  }
  number = static_cast<int>(byte);
}

void If_PPM::ValueByte::ReadNumber(const unsigned char* &data,int& number)
{ // Copies byte at data as an unsigned character into number
  // and increments data.
  number = static_cast<int>(*data);
  ++data;
}

void If_PPM::ValueDyad::ReadNumber(Tcl_Channel chan,int& number)
{ // Consumes the next two binary bytes on chan, and stores the result
  // as an unsigned two-byte big-endian value in number. Caller is
  // responsible for ensuring chan is in binary mode. Throws a const
  // char* on error.

  // Note: Tcl 8.6 docs say Tcl_Read is deprecated, but use it anyway as
  //       it does exactly what we want. But we may need to change this
  //       in the future. If so, we may want to change export "number"
  //       to type ObjPtr.

  // At this point, ch should hold first non-whitespace
  // character outside of any comments.
  unsigned char byte[2];
  if(Tcl_Read(chan,reinterpret_cast<char*>(byte),2)!=2) {
    throw("Premature EOF in PPM file.");
  }
  number = static_cast<int>(static_cast<unsigned int>((byte[0])<<8) + byte[1]);
}

void If_PPM::ValueDyad::ReadNumber(const unsigned char* &data,int& number)
{ // Copies byte at data as an unsigned character into number
  // and increments data.
  unsigned int hibyte = data[0];
  unsigned int lobyte = data[1];
  number = static_cast<int>((hibyte<<8) + lobyte);
  data+=2;
}


int If_PPM::ReadHeader
(Tcl_Channel chan,
 int& imagewidth,int& imageheight,int& maxvalue,
 FileType &filetype)
{ // Returns 1 if the data doesn't looks like PPM data.  Otherwise,
  // returns 0 and advances channel pointer to end of header.

  // Check signature
  char type[2];
  Tcl_Read(chan,type,2*sizeof(char));
  if(type[0]!='P') return 1;
  switch(type[1]) {
  case '1': filetype = FileType::P1; break;
  case '2': filetype = FileType::P2; break;
  case '3': filetype = FileType::P3; break;
  case '4': filetype = FileType::P4; break;
  case '5': filetype = FileType::P5; break;
  case '6': filetype = FileType::P6; break;
  default: return 1;
  }
  try {
    ValueText::ReadNumber(chan,imagewidth);
    ValueText::ReadNumber(chan,imageheight);
    if(filetype == FileType::P1 || filetype == FileType::P4) {
      maxvalue = 1;  // Simple bitmap
    } else {
      ValueText::ReadNumber(chan,maxvalue);
    }
  } catch(...) {
    return 1;
  }
  return 0;
}


int If_PPM::ReadHeader
(const unsigned char* &data,
 int& imagewidth,int& imageheight,int& maxvalue,
 FileType &filetype)
{ // Returns 1 if the data doesn't looks like PPM data.  Otherwise,
  // fills the export values, and adjusts data to point to first byte
  // past header, and returns 0.

  // Check signature
  if(data[0]!='P') return 1;
  switch(data[1]) {
  case '1': filetype = FileType::P1; break;
  case '2': filetype = FileType::P2; break;
  case '3': filetype = FileType::P3; break;
  case '4': filetype = FileType::P4; break;
  case '5': filetype = FileType::P5; break;
  case '6': filetype = FileType::P6; break;
  default: return 1;
  }
  const unsigned char* cptr = data + 2;
  try {
    ValueText::ReadNumber(cptr,imagewidth);
    ValueText::ReadNumber(cptr,imageheight);
    if(filetype == FileType::P1 || filetype == FileType::P4) {
      maxvalue = 1;  // Simple bitmap
    } else {
      ValueText::ReadNumber(cptr,maxvalue);
    }
  } catch(...) {
    return 1;
  }
  data = cptr;
  return 0;
}

int If_PPM::ReadCheck
(Tcl_Channel chan,int& imagewidth,int& imageheight)
{ // Returns 1 if the data looks like a PPM P3
  int maxvalue;
  FileType filetype;
  if(ReadHeader(chan,imagewidth,imageheight,maxvalue,filetype)!=0) {
    return 0;
  }
  return 1;
}

int If_PPM::ReadCheck
(const unsigned char* data,int& imagewidth,int& imageheight)
{ // Returns 1 if the data looks like a PPM P3
  int maxvalue;
  FileType filetype;
  if(ReadHeader(data,imagewidth,imageheight,maxvalue,filetype)!=0) {
    return 0;
  }
  return 1;
}

template<typename T,typename R>
int If_PPM::PhotoFillData
(Tcl_Interp*  interp,T inhandle,
 const char* fileName,Tk_PhotoHandle imageHandle,
 int destX,int destY,int width,int height,int srcX,int srcY,
 int imagewidth,int imageheight,int maxvalue,
 If_PPM::FileType filetype)
{ // T selects Tcl_Channel vs data[],
  // R select data type
  std::string errheader = std::string("Data from ");
  if(fileName) {
    errheader += std::string(fileName);
  } else {
    errheader += std::string("[data]");
  }

  const int srcwidth
    = (srcX+width <= imagewidth ? width : imagewidth-srcX);
  const int srcheight
    = (srcY+height <= imageheight ? height : imageheight-srcY);
  if(srcwidth<=0 || srcheight<=0) {
    return TCL_OK;  // ?! Nothing to copy
  }

  // Restrict block size to source dimensions.

  // The 8.6 Tk_PhotoImageBlock docs state:
  //
  //    The pixelPtr field points to the first pixel, that is, the
  //    top-left pixel in the block. The width and height fields specify
  //    the dimensions of the block of pixels. The pixelSize field
  //    specifies the address difference between two horizontally
  //    adjacent pixels. It should be 4 for RGB and 2 for grayscale
  //    image data. Other values are possible, if the offsets in the
  //    offset array are adjusted accordingly (e.g. for red, green and
  //    blue data stored in different planes). Using such a layout is
  //    strongly discouraged, though. Due to a bug, it might not work
  //    correctly if an alpha channel is provided. (see the BUGS section
  //    below). The pitch field specifies the address difference between
  //    two vertically adjacent pixels. The offset array contains the
  //    offsets from the address of a pixel to the addresses of the
  //    bytes containing the red, green, blue and alpha (transparency)
  //    components. If the offsets for red, green and blue are equal,
  //    the image is interpreted as grayscale. If they differ, RGB data
  //    is assumed. Normally the offsets will be 0, 1, 2, 3 for RGB data
  //    and 0, 0, 0, 1 for grayscale. It is possible to provide image
  //    data without an alpha channel by setting the offset for alpha to
  //    a negative value and adjusting the pixelSize field
  //    accordingly. This use is discouraged, though (see the BUGS
  //    section below)...
  //
  //    [BUGS] ... The problem is that the pixelSize field is
  //    (incorrectly) used to determine whether the image has an alpha
  //    channel. Currently, if the offset for the alpha channel is
  //    greater or equal than pixelSize, tk_PhotoPutblock assumes no
  //    alpha data is present and makes the image fully opaque. This
  //    means that for layouts where the channels are separate (or any
  //    other exotic layout where pixelSize has to be smaller than the
  //    alpha offset), the alpha channel will not be read correctly. In
  //    order to be on the safe side if this issue will be corrected in
  //    a future release, it is strongly recommended you always provide
  //    alpha data - even if the image has no transparency - and only
  //    use the "standard" layout with a pixelSize of 2 for grayscale
  //    and 4 for RGB data with offsets of 0, 0, 0, 1 or 0, 1, 2, 3
  //    respectively.
  //
  // I don't know the origin of this, but I also have a note that Jan
  // Nijtmans recommends this safe way to disable alpha processing is
  // set pib.offset[3]=pib.offset[0], which AFAICT does work in
  // practice.
  //
  // Considering this, the code below does the following:
  //  (1) For RGB color, sets pixelSize=4, offset[]={0,1,2,3}, scales
  //      R/B/G values to [0,255], and sets the alpha channel to 255
  //      (fully opaque).
  //
  //  (2) For grayscale with maxvalue<=255, sets pixelSize=2,
  //      offset[]={0,0,0,1}, scales the gray intensity value to
  //      [0,255], and sets the alpha channel to 255 (fully opaque).
  //
  //  (3) For grayscale with maxvalue>255, the safe thing to do is to
  //      repeat (2). But it would be nice to try to retain higher
  //      intensity bit depth when present, so the code instead sets
  //      pixelSize=2 with big endian format, offset[]={0,0,0,2}, scales
  //      the gray intensity value to [0,65535], with no alpha
  //      setting. If hiccups occur one can try following Nijtmans'
  //      advice and set offset[]={0,0,0,0}, or the docs with
  //      offset[]={0,0,0,-1}. I don't know if the Tk library even
  //      retains more than 8 color bits, but regardless this
  //      capability may be useful if I ever code up an interface
  //      to this code that bypasses Tk.

  Tk_PhotoImageBlock pib; // See Tk_FindPhoto documentation for
  pib.width=srcwidth;    /// Tk_PhotoImageBlock details.
  pib.height=srcheight;
  int ppm_pixel_width = 0;
  switch(filetype) {
    // Gray scale images
  case FileType::P1:
  case FileType::P2:
  case FileType::P4:
  case FileType::P5:
    ppm_pixel_width = 1; // Input value count per pixel
    pib.pixelSize = 2;   // Output pixel information
    pib.offset[0]=0;    pib.offset[1]=0;
    pib.offset[2]=0;    pib.offset[3]=(maxvalue<256 ? 1 : 2);
    // Tk uses 2 bytes for grayscale images. Typically this
    // is one byte for brightness and one for alpha, but
    // PGM supports greater depths. The above tries to cram
    // up to two bytes of brightness info into the Tk photo
    // structure in a way which is suppose to disable alpha.
    // But this may be fragile --- see the BUGS note above.
    break;

    // RGB images
  case FileType::P3:
  case FileType::P6:
    ppm_pixel_width = 3; // Input value count per pixel
    pib.pixelSize = 4; // Tk uses 3 bytes for R+G+B and 1 byte for alpha
    pib.offset[0]=0;    pib.offset[1]=1;
    pib.offset[2]=2;    pib.offset[3]=3;
    break;

  default:
    Tcl_AppendResult(interp,errheader.c_str(),
                     " is not a supported PPM type.",
                     nullptr);
    return TCL_ERROR;
  }
  pib.pitch=static_cast<int>(pib.pixelSize*srcwidth);
  std::vector<unsigned char> pixvec(pib.pixelSize*srcwidth*srcheight);
  unsigned char* pix = pixvec.data();
  pib.pixelPtr=pix;

  // Create a buffer for storing one row of data. The size of this
  // buffer depends on the image width and the ppm input pixel width,
  // i.e., the number of values for each pixel: three for RGB and 1 for
  // grayscale.
  std::vector<int> ppm_data(ppm_pixel_width*width);

  // Skip over unrequested rows and left margin on first requested row
  R reader;
  try {
    int dummy;
    for(int i=0;i<srcY;i++) {
      for(int j=0;j<ppm_pixel_width*imagewidth;j++) {
        reader.ReadNumber(inhandle,dummy);
      }
    }
    for(int j=0;j<ppm_pixel_width*srcX;j++) {
        reader.ReadNumber(inhandle,dummy);
    }
  } catch(...) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,errheader.c_str(),
                     " doesn't conform to PPM format (bad data).",
                     nullptr);
    return TCL_ERROR;
  }

  // Read and write data. Note that at the start of each
  // row pass the read pointer is already placed just past
  // the left hand read margin.
  try {
    for(int i=0;i<srcheight;i++) {
      unsigned char* pixrow = &(pix[i*pib.pixelSize*srcwidth]);
      // Process one row
      for(int j=0;j<ppm_pixel_width*srcwidth;j++) {
        reader.ReadNumber(inhandle,ppm_data[j]);
      }
      // Convert to Tk photo values. Scale through unsigned long to
      // protect against overflow.
      if(pib.pixelSize == 2 && pib.offset[3] == 1) {
        // 1 byte grayscale, 1 byte alpha
        for(int j=0;j<width;j++) {
          unsigned long tmp =
            (static_cast<unsigned long>(ppm_data[j])*2*255+maxvalue)
            /(2*maxvalue);
          pixrow[2*j]   = static_cast<unsigned char>(tmp);
          pixrow[2*j+1] = 255; // Opaque
        }
      } else if(pib.pixelSize == 2) {
        // 2 byte grayscale, no alpha
        for(int j=0;j<width;j++) {
          unsigned long tmp =
            (static_cast<unsigned long>(ppm_data[j])*2*65535+maxvalue)
            /(2*maxvalue);
          // Tk FindPhoto man page does not specify byte order???
          // Testing on Windows indicates big endian.
          pixrow[2*j]   = static_cast<unsigned char>(tmp >> 8);
          pixrow[2*j+1] = static_cast<unsigned char>(tmp & 0xFF);
        }
      } else {
        // RGB + alpha
        for(int j=0;j<width;j++) {
          unsigned long tmp;
          tmp = (static_cast<unsigned long>(ppm_data[3*j])*2*255+maxvalue)
            /(2*maxvalue);
          pixrow[4*j] = static_cast<unsigned char>(tmp);
          tmp = (static_cast<unsigned long>(ppm_data[3*j+1])*2*255+maxvalue)
            /(2*maxvalue);
          pixrow[4*j+1] = static_cast<unsigned char>(tmp);
          tmp = (static_cast<unsigned long>(ppm_data[3*j+2])*2*255+maxvalue)
            /(2*maxvalue);
          pixrow[4*j+2] = static_cast<unsigned char>(tmp);
          pixrow[4*j+3] = 255; // Fully opaque
        }
      }

      // Skip ahead over right margin in current row and past left
      // margin in next row (i.e., imagewidth-srcwidth pixels)
      int dummy;
      for(int j=ppm_pixel_width*srcwidth;j<ppm_pixel_width*imagewidth;j++) {
        reader.ReadNumber(inhandle,dummy);
      }
    } // for(int i=0;i<height;i++) ...

    // Copy block. NB: Tk_PhotoPutBlock will clip or copy pib data
    // as necessary to fill width x height region in imageHandle.
    // (See Tk_PhotoPutBlock documentation for details.)
    Tk_PhotoPutBlock(interp,imageHandle,&pib,destX,destY,width,height,
                     TK_PHOTO_COMPOSITE_OVERLAY);

  } catch(const char* errmsg) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,errheader.c_str(),
                     " doesn't conform to PPM format (bad data): ",
                     errmsg,nullptr);
    return TCL_ERROR;
  }
  return TCL_OK;
}

template<typename T>
int If_PPM::PhotoFill
(Tcl_Interp*  interp,T inhandle,
 const char* fileName,Tk_PhotoHandle imageHandle,
 int destX,int destY,int width,int height,int srcX,int srcY)
{ // Fills in imageHandle as requested, one row at a time.
  // Returns TCL_OK on success, TCL_ERROR on failure.
  std::string errheader = std::string("Data from");
  if(fileName) {
    // Fill from file
    errheader += std::string(" file ") + string(fileName);
  } else {
    // Fill from string
    errheader += std::string(" string");
  }

  int imagewidth,imageheight,maxvalue;
  FileType filetype;
  int foo=-1;
  if((foo=ReadHeader(inhandle,imagewidth,imageheight,maxvalue,filetype))!=0) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,errheader.c_str(),
                     " not recognized as a PPM P1|2|3|4|5|6 header.",
                     nullptr);
    return TCL_ERROR;
  }

  // Safety checks
  if(srcX<0) { width += srcX; srcX=0; }
  if(srcX+width>imagewidth) {
    width = imagewidth-srcX;
  }
  if(srcY<0) {height += srcY; srcY=0; }
  if(srcY+height>imageheight) {
    height = imageheight-srcY;
  }
  if(width<1 || height<1) return TCL_OK; // Nothing to do

  int fill_result=TCL_ERROR;
  switch(filetype) {
    // Plain text input formats //////////////////////////////////////
  case FileType::P1:
    fill_result= If_PPM::PhotoFillData<T,If_PPM::ValueTextBit>
      (interp,inhandle,fileName,imageHandle,
       destX,destY,width,height,srcX,srcY,
       imagewidth,imageheight,maxvalue,filetype);
    break;
  case FileType::P2:
    // fall through
  case FileType::P3:
    fill_result= If_PPM::PhotoFillData<T,If_PPM::ValueText>
      (interp,inhandle,fileName,imageHandle,
       destX,destY,width,height,srcX,srcY,
       imagewidth,imageheight,maxvalue,filetype);
    break;

    // Binary input formats //////////////////////////////////////////
  case FileType::P4: // One bit per value
    fill_result= If_PPM::PhotoFillData<T,If_PPM::ValueBit>
      (interp,inhandle,fileName,imageHandle,
       destX,destY,width,height,srcX,srcY,
       imagewidth,imageheight,maxvalue,filetype);
    break;
  case FileType::P5:
  case FileType::P6:
    if(maxvalue<256) { // One byte per value
      fill_result= If_PPM::PhotoFillData<T,If_PPM::ValueByte>
        (interp,inhandle,fileName,imageHandle,
         destX,destY,width,height,srcX,srcY,
         imagewidth,imageheight,maxvalue,filetype);
    } else if(maxvalue<65536) { // Two bytes per value
      fill_result= If_PPM::PhotoFillData<T,If_PPM::ValueDyad>
        (interp,inhandle,fileName,imageHandle,
         destX,destY,width,height,srcX,srcY,
         imagewidth,imageheight,maxvalue,filetype);
    } else {
      Tcl_AppendResult(interp,errheader.c_str(),
                       " has unsupported binary width.",
                       nullptr);
      return TCL_ERROR;

    }
    break;

  default:
    Tcl_AppendResult(interp,errheader.c_str(),
                     " is not a supported PPM type.",
                     nullptr);
    return TCL_ERROR;
  }

  return fill_result;
}


int If_PPM::SprintColorValues(unsigned char* buf,
                              int red,int green,int blue) {
  // Code assumes 0<=red,green,blue<=999, and that the character
  // values for 0,1,...,9 are consecutive.

  unsigned char* cptr=buf;

  // Red part
  int rh = red/100;     // Hundreds
  int rtmp = red - rh*100;
  int rt = rtmp/10;      // Tens
  int ro = rtmp - rt*10; // Ones
  if(rh>0)         *(cptr++) = static_cast<unsigned char>('0' + rh);
  if(rh>0 || rt>0) *(cptr++) = static_cast<unsigned char>('0' + rt);
  *(cptr++) = static_cast<unsigned char>('0' + ro);
  *(cptr++) = ' ';

  // Green part
  int gh = green/100;     // Hundreds
  int gtmp = green - gh*100;
  int gt = gtmp/10;      // Tens
  int go = gtmp - gt*10; // Ones
  if(gh>0)         *(cptr++) = static_cast<unsigned char>('0' + gh);
  if(gh>0 || gt>0) *(cptr++) = static_cast<unsigned char>('0' + gt);
  *(cptr++) = static_cast<unsigned char>('0' + go);
  *(cptr++) = ' ';

  // Blue part
  int bh = blue/100;     // Hundreds
  int btmp = blue - bh*100;
  int bt = btmp/10;      // Tens
  int bo = btmp - bt*10; // Ones
  if(bh>0)         *(cptr++) = static_cast<unsigned char>('0' + bh);
  if(bh>0 || bt>0) *(cptr++) = static_cast<unsigned char>('0' + bt);
  *(cptr++) = static_cast<unsigned char>('0' + bo);
  *(cptr++) = '\n';

  return static_cast<int>(cptr-buf);
}

int If_PPM::WritePhoto
(Tcl_Interp*  interp,const char* filename,
 Tk_PhotoImageBlock* blockPtr)
{ // Write PhotoImageBlock to file "fileName" in PPM P3 format.

  // Open Tcl channel to handle output
  Tcl_Channel chan
    = Tcl_OpenFileChannel(interp,filename, "w",0666);
  if(chan==NULL) return TCL_ERROR;
  int errcode = Tcl_SetChannelOption(interp,chan, "-translation", "auto");
  if(errcode!=TCL_OK) {
    Tcl_Close(interp,chan);
    return errcode;
  }
  errcode = Tcl_SetChannelOption(interp,chan,"-buffering", "full");
  if(errcode!=TCL_OK) {
    Tcl_Close(interp,chan);
    return errcode;
  }
  errcode = Tcl_SetChannelOption(interp,chan,"-buffersize", "131072");
  if(errcode!=TCL_OK) {
    Tcl_Close(interp,chan);
    return errcode;
  }

  // Buffer space
# define IF_PPM_MAXCHUNKSIZE (3*4) // Max length of string repr. for pixel
# define IF_PPM_WPBUFSIZE (4096*IF_PPM_MAXCHUNKSIZE)
  unsigned char buf[IF_PPM_WPBUFSIZE+1];
  /// Space for minimum of IF_PPM_WPBUFSIZE/IF_PPM_MAXCHUNKSIZE pixels,
  /// + 1 (safety).
  assert(IF_PPM_WPBUFSIZE>IF_PPM_MAXCHUNKSIZE); // Otherwise
  /// ::SprintColorValues will overflow buffer.

  // Header info
  int width  = blockPtr->width;
  int height = blockPtr->height;
  int maxvalue = 255; // Assumed

  // Write header
  Oc_Snprintf(reinterpret_cast<char *>(buf),
              sizeof(buf),
              "P3\n# Created by OOMMF If library\n%d %d\n%d\n",
	      width,height,maxvalue);
  size_t buf_strlen = strlen(reinterpret_cast<char *>(buf));
  assert(buf_strlen<=INT_MAX);
  if(Tcl_Write(chan,reinterpret_cast<char *>(buf),
               static_cast<int>(buf_strlen))==-1) {
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp,"Output error writing"
		     " PPM P3 file: \"",filename,
		     "\"",(char *)NULL);
    Tcl_Close(interp,chan);
    return TCL_ERROR;
  }

  // Write data, from top to bottom.
  int bufoff = 0;
  for(int i = 0 ; i < height ; i++) {
    int pixoff = i * blockPtr->pitch;
    for(int j=0;j<width;j++,pixoff+=blockPtr->pixelSize) {
      int red   = blockPtr->pixelPtr[pixoff+blockPtr->offset[0]];
      int green = blockPtr->pixelPtr[pixoff+blockPtr->offset[1]];
      int blue  = blockPtr->pixelPtr[pixoff+blockPtr->offset[2]];
      if(bufoff + IF_PPM_MAXCHUNKSIZE >= IF_PPM_WPBUFSIZE) {
        // Flush buffer
        if(Tcl_Write(chan,(char*)buf,bufoff)==-1) {
          Tcl_ResetResult(interp);
          Tcl_AppendResult(interp,"Output error writing"
                           " PPM P3 file: \"",filename,
                           "\"",(char *)NULL);
          Tcl_Close(interp,chan);
          return TCL_ERROR;
        }
        bufoff=0;
      }
      bufoff += SprintColorValues(buf+bufoff,
                                  red,green,blue);
    }
  }
  if(bufoff>0) {
    // Flush buffer
    if(Tcl_Write(chan,(char*)buf,bufoff)==-1) {
      Tcl_ResetResult(interp);
      Tcl_AppendResult(interp,"Output error writing"
                       " PPM P3 file: \"",filename,
                       "\"",(char *)NULL);
      Tcl_Close(interp,chan);
      return TCL_ERROR;
    }
  }
# undef IF_PPM_WPBUFSIZE
# undef IF_PPM_MAXCHUNKSIZE

  // Close channel and exit
  return Tcl_Close(interp,chan);
}

int If_PPM::WritePhoto
(Tcl_Interp* /* interp */,Tcl_DString* dataPtr,
 Tk_PhotoImageBlock* blockPtr)
{ // Write PhotoImageBlock to data string in PPM P3 format.

  // Buffer space
  char buf[64*4];
  assert(sizeof(buf)>3*4); // Otherwise ::SprintColorValues will
     /// overflow buffer.
  int outlen; // Length of string in buffer

  // Header info
  int width  = blockPtr->width;
  int height = blockPtr->height;
  int maxvalue = 255; // Assumed

  // Write header
  outlen = (int)Oc_Snprintf(buf,sizeof(buf),
                  "P3\n# Created by OOMMF If library (data)\n%d %d\n%d\n",
                  width,height,maxvalue);
  Tcl_DStringInit(dataPtr);
  Tcl_DStringAppend(dataPtr,buf,outlen);

  // Write data, from top to bottom.
  for(int i = 0 ; i < height ; i++) {
    int pixoff = i * blockPtr->pitch;
    for(int j=0;j<width;j++,pixoff+=blockPtr->pixelSize) {
      int red   = blockPtr->pixelPtr[pixoff+blockPtr->offset[0]];
      int green = blockPtr->pixelPtr[pixoff+blockPtr->offset[1]];
      int blue  = blockPtr->pixelPtr[pixoff+blockPtr->offset[2]];
      outlen = (int)SprintColorValues((unsigned char*)buf,red,green,blue);
      Tcl_DStringAppend(dataPtr,buf,outlen);
    }
  }

  return TCL_OK;
}


extern "C" {
  Tk_ImageFileMatchProc bmpFileMatchProcWrap;
  Tk_ImageFileReadProc  bmpFileReadProcWrap;
  Tk_ImageFileWriteProc bmpFileWriteProcWrap;
}


static char bmpnamestr[] = "bmp";
// NB: According to the Tk_CreatePhotoImageFormat man page (Tk 8.6.13),
// the first letter of the namestring must not be an uppercase ASCII
// character (i.e., A-Z), as those are reserved for the legacy interface
// (Tk 8.2 and earlier). In practice if both "BMP" and "bmp" are
// registered (say one from here and the other by the Img library) then
// the lowercase registration takes precedence. There is a note in the
// Tk 8.6 docs that the legacy interface is expected to disappear in
// Tcl/Tk 9, so this undocumented behavior may change.

extern "C" int
bmpFileMatchProc(Tcl_Channel chan,
                 char*        /* fileName */,
                 char*        /* formatString */,
                 int         *widthPtr,
                 int         *heightPtr)
{
  If_MSBitmap msb;
  return msb.ReadCheck(chan,*widthPtr,*heightPtr);
}

extern "C" int
bmpFileMatchProcWrap(	Tcl_Channel	chan,
			const char*	/* fileName */,
			Tcl_Obj *	/* formatString */,
			int *		widthPtr,
			int *		heightPtr,
			Tcl_Interp *	/* interp */)
{
  return bmpFileMatchProc(chan, NULL, NULL, widthPtr, heightPtr);
}

extern "C" int
bmpFileReadProc(Tcl_Interp*  interp,
                Tcl_Channel  chan,
                const char*  fileName,
                char*        /* formatString */,
                Tk_PhotoHandle imageHandle,
                int destX,int destY,
                int width,int height,
                int srcX,int srcY)
{
  If_MSBitmap msb;
  return msb.FillPhoto(interp,chan,fileName,imageHandle,
		       destX,destY,width,height,srcX,srcY);
}

extern "C" int
bmpFileReadProcWrap(Tcl_Interp*  interp,
                Tcl_Channel  chan,
                const char*  fileName,
                Tcl_Obj *    /* formatString */,
                Tk_PhotoHandle imageHandle,
                int destX,int destY,
                int width,int height,
                int srcX,int srcY)
{
  return bmpFileReadProc(interp, chan, fileName, NULL, imageHandle, destX, destY,
		width, height, srcX, srcY);
}

extern "C" int
bmpFileWriteProc(Tcl_Interp* interp,
		 const char* fileName,
		 char* /* format */,
		 Tk_PhotoImageBlock* blockPtr)
{
  If_MSBitmap msb;
  return msb.WritePhoto(interp,fileName,blockPtr);
}

extern "C" int
bmpFileWriteProcWrap(Tcl_Interp* interp,
		 const char* fileName,
		 Tcl_Obj* /* format */,
		 Tk_PhotoImageBlock* blockPtr)
{
  return bmpFileWriteProc(interp, fileName, NULL, blockPtr);
}

static Tk_PhotoImageFormat bmpformat =
{
  bmpnamestr,             // Format name
  bmpFileMatchProcWrap,   // File format identifier routine
  NULL,                   // String format id routine
  bmpFileReadProcWrap,    // File read routine
  NULL,                   // String read routine
  bmpFileWriteProcWrap,   // File write routine
  NULL,                   // String write routine
  NULL			  // NULL nextPtr to be overwritten by Tk
};

extern "C" {
  Tk_ImageFileMatchProc   ppmFileMatchProcWrap;
  Tk_ImageFileReadProc    ppmFileReadProcWrap;
  Tk_ImageFileWriteProc   ppmFileWriteProcWrap;
  Tk_ImageStringMatchProc ppmStringMatchProcWrap;
  Tk_ImageStringReadProc  ppmStringReadProcWrap;
  Tk_ImageStringWriteProc ppmStringWriteProcWrap;
}


static char ppmnamestr[] = "p3";
// NB: According to the Tk_CreatePhotoImageFormat man page (Tk 8.6.13),
// the first letter of the namestring must not be an uppercase ASCII
// character (i.e., A-Z), as those are reserved for the legacy interface
// (Tk 8.2 and earlier). In practice if both upper and lowercase forms
// are registered (say one from here and the other by the Img library)
// then the lowercase registration takes precedence. The Tk 8.6 docs
// state that the legacy interface is expected to disappear in Tcl/Tk 9,
// so this undocumented behavior may change.

extern "C" int
ppmFileMatchProc(Tcl_Channel chan,
                 char*        /* filename */,
                 char*        /* formatString */,
                 int*         widthPtr,
                 int*         heightPtr)
{
  If_PPM ppm;
  return ppm.ReadCheck(chan,*widthPtr,*heightPtr);
}

extern "C" int
ppmFileMatchProcWrap(Tcl_Channel chan,
                 const char*  /* filename */,
                 Tcl_Obj*     /* formatString */,
                 int*         widthPtr,
                 int*         heightPtr,
		 Tcl_Interp * /* interp */)
{
  return ppmFileMatchProc(chan, NULL, NULL, widthPtr, heightPtr);
}

extern "C" int
ppmFileReadProc(Tcl_Interp*  interp,
                Tcl_Channel  chan,
                const char*  filename,
                char*        /* formatString */,
                Tk_PhotoHandle imageHandle,
                int destX, int destY,
                int width, int height,
                int srcX,  int srcY)
{
  If_PPM ppm;
  return ppm.FillPhoto(interp,chan,filename,imageHandle,
		       destX,destY,width,height,srcX,srcY);
}

extern "C" int
ppmFileReadProcWrap(Tcl_Interp*  interp,
                Tcl_Channel  chan,
                const char*  filename,
                Tcl_Obj*     /* formatString */,
                Tk_PhotoHandle imageHandle,
                int destX, int destY,
                int width, int height,
                int srcX,  int srcY)
{
  return ppmFileReadProc(interp, chan, filename, NULL, imageHandle,
			destX, destY, width, height, srcX, srcY);
}

extern "C" int
ppmFileWriteProc(Tcl_Interp* interp,
		 const char* filename,
		 char* /* format */,
		 Tk_PhotoImageBlock* blockPtr)
{
  If_PPM ppm;
  return ppm.WritePhoto(interp,filename,blockPtr);
}

extern "C" int
ppmFileWriteProcWrap(Tcl_Interp* interp,
		 const char* filename,
		 Tcl_Obj* /* format */,
		 Tk_PhotoImageBlock* blockPtr)
{
  return ppmFileWriteProc(interp, filename, NULL, blockPtr);
}

extern "C" int
ppmStringMatchProc(const unsigned char* data,
		   char* /* formatString */,
		   int* widthPtr,
		   int* heightPtr)
{
  If_PPM ppm;
  return ppm.ReadCheck(data,*widthPtr,*heightPtr);
}

extern "C" int
ppmStringMatchProcWrap(Tcl_Obj* data,
		   Tcl_Obj* /* formatString */,
		   int* widthPtr,
		   int* heightPtr,
		   Tcl_Interp * /* interp */)
{
  return ppmStringMatchProc(Tcl_GetByteArrayFromObj(data,(int *)nullptr),
                            nullptr, widthPtr, heightPtr);
}

extern "C" int
ppmStringReadProc(Tcl_Interp *interp,
		  const unsigned char* data,
		  char* /* formatString */,
		  Tk_PhotoHandle imageHandle,
		  int destX, int destY,
		  int width, int height,
		  int srcX,  int srcY)
{
  If_PPM ppm;
  return ppm.FillPhoto(interp,data,imageHandle,
		       destX,destY,width,height,srcX,srcY);
}

extern "C" int
ppmStringReadProcWrap(Tcl_Interp *interp,
		  Tcl_Obj* data,
		  Tcl_Obj* /* formatString */,
		  Tk_PhotoHandle imageHandle,
		  int destX, int destY,
		  int width, int height,
		  int srcX,  int srcY)
{
  return ppmStringReadProc(interp, Tcl_GetByteArrayFromObj(data,(int *)nullptr),
                           nullptr, imageHandle,
                           destX, destY, width, height, srcX, srcY);
}

extern "C" int
ppmStringWriteProc(Tcl_Interp* interp,
		   Tcl_DString* dataPtr,
		   char* /* formatString */,
		   Tk_PhotoImageBlock* blockPtr)
{
  If_PPM ppm;
  return ppm.WritePhoto(interp,dataPtr,blockPtr);
}

extern "C" int
ppmStringWriteProcWrap(Tcl_Interp* interp,
		   Tcl_Obj* /* formatString */,
		   Tk_PhotoImageBlock* blockPtr)
{
  Tcl_DString buffer;
  int code;

  code = ppmStringWriteProc(interp, &buffer, NULL, blockPtr);
  if (code == TCL_OK) {
    Tcl_DStringResult(interp, &buffer);
  } else {
    Tcl_DStringFree(&buffer);
  }
  return code;
}


static Tk_PhotoImageFormat ppmformat =
{
  ppmnamestr,             // Format name
  ppmFileMatchProcWrap,   // File format identifier routine
  ppmStringMatchProcWrap, // String format id routine
  ppmFileReadProcWrap,    // File read routine
  ppmStringReadProcWrap,  // String read routine
  ppmFileWriteProcWrap,   // File write routine
  ppmStringWriteProcWrap, // String write routine
  NULL			  // NULL nextPtr to be overwritten by Tk
};

int
If_Init(Tcl_Interp *interp)
{
#define RETURN_TCL_ERROR                                               \
    Tcl_AddErrorInfo(interp, "\n    (in If_Init())");                  \
    return TCL_ERROR

    if (Tcl_PkgPresent(interp, "Oc", "2", 0) == NULL) {
        Tcl_AppendResult(interp, "\n\t(If " IF_VERSION " needs Oc 2)", NULL);
        RETURN_TCL_ERROR;
    }

// See note above.
# if 0
    if(Tcl_PkgPresent(interp,"Img",NULL,0)==NULL) {
      // The Img package provides support for BMP, so
      // don't load our BMP handler if Img is already
      // loaded.
      Tk_CreatePhotoImageFormat(&bmpformat);
    }
# else
    // As of 2007-10-30, the BMP handler in this file handles
    // some 16-bit BMP formats not inside Img, so we load our
    // BMP code regardless. (Note: The omfsh/filtersh shell
    // will load Img if possible.)
    Tk_CreatePhotoImageFormat(&bmpformat);
# endif
    Tk_CreatePhotoImageFormat(&ppmformat);

    if (Tcl_PkgProvide(interp, "If", IF_VERSION) != TCL_OK) {
        RETURN_TCL_ERROR;
    }

/*
 * Currently there is no Tcl part to the If extension
 *
    if (Oc_InitScript(interp, "If", IF_VERSION) != TCL_OK) {
        RETURN_TCL_ERROR;
    }
 *
 */

    return TCL_OK;

#undef RETURN_TCL_ERROR
}

#else /* !OC_USE_TK */

int
If_Init(Tcl_Interp *interp)
{
  Tcl_AppendResult(interp, "\n\t(If " IF_VERSION
                     " requires Tk (this is a no-Tk build of OOMMF)",
                   NULL);
  Tcl_AddErrorInfo(interp, "\n    (in If_Init())");
  return TCL_ERROR;
}

#endif /* OC_USE_TK */
