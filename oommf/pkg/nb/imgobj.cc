/* FILE: imgobj.cc                 -*-Mode: c++-*-
 *
 * C++ wrapper for 2D image data.  At present, the data are stored as
 * 24-bit color values, red+green+blue, in consecutive elements of a
 * char array.  The interface is intended to be flexible enough that,
 * potentially, the backend could be changed to support monochromatic
 * storage (as single elements of char array, to reduce memory
 * footprint) or wider data types (to provide greater color depth).
 *
 * This class is designed with primary emphasis on minimizing memory
 * use for large images.  Access speed is only a secondary criterion.
 *
 * NOTE: This class does not require Tk.  However, if the input file
 * is not in format supported by this code, then any2ppm is called,
 * and any2ppm does use Tk.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2013/05/22 07:15:34 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>
#include <cctype>
#include <climits>
#include <cmath>        /* For floor() on Solaris with Forte compiler */
#include <cstring>

#include "oc.h"
#include "imgobj.h"

/* End includes */     /* Optional directive to build.tcl */

#define NB_IMGOBJ_ERRBUFSIZE 4096  // Buffer size used for error messages
#define NB_IMGOBJ_CMDBUFSIZE 16384 // Buffer size used to contruct and
                                  /// hold Tcl commands

////////////////////////////////////////////////////////////////////////

// Read one text value routine, for PPM files.  This routine is needed
// by both text and binary PPM formats, because the header in both
// cases is in ASCII text.
OC_INT4m Nb_ImgObj::ReadPPMTextInt(Nb_InputChannel& chan,OC_INT4m& value)
{ //    Returns 0 on success, or on error returns
  //         1 => EOF
  //         2 => Input contains character not allowed in an integer value

  int byte;

  // Read past any initial whitespace or comments on chan
  while((byte=chan.ReadChar())>=0) {
    if(isspace(byte)) continue;
    if(byte=='#') {
      // Comment; read until end of line
      while((byte=chan.ReadChar())>=0) {
        if(byte=='\n' || byte=='\r') break;
      }
    } else {
      break;
    }
  }

  if(byte<0) return 1; // EOF
  if(byte<'0' || '9'<byte) return 2; // Illegal value

  // Otherwise, byte holds the first non-whitespace,
  // non-comment character, i.e., the first character
  // of the token.
  OC_INT4m tmpval = byte - '0';

  // Read until first non-numeric value, shifting values into tmpval;
  while((byte=chan.ReadChar())>=0) {
    byte -= '0';
    if(byte<0 || byte>9) break; // Non-numeric value
    tmpval = 10*tmpval+byte;
  }

  value = tmpval;
  return 0;
}

OC_INT4m Nb_ImgObj::ReadThreeIntPPMText
(Nb_InputChannel& chan,
 OC_INT4m& value1,
 OC_INT4m& value2,
 OC_INT4m& value3)
{
  OC_INT4m err;
  if((err=ReadPPMTextInt(chan,value1))!=0) {
    return err;
  }
  if((err=ReadPPMTextInt(chan,value2))!=0) {
    return err;
  }
  return ReadPPMTextInt(chan,value3);
}

OC_INT4m Nb_ImgObj::ReadThreeIntPPMBin8
(Nb_InputChannel& chan,
 OC_INT4m& value1,
 OC_INT4m& value2,
 OC_INT4m& value3)
{
  if((value1 = static_cast<OC_INT4m>(chan.ReadChar()))<0 ||
     (value2 = static_cast<OC_INT4m>(chan.ReadChar()))<0 ||
     (value3 = static_cast<OC_INT4m>(chan.ReadChar()))<0) {
    return 1; // Presumed EOF
  }
  return 0;
}

OC_INT4m Nb_ImgObj::ReadThreeIntPPMBin16
(Nb_InputChannel& chan,
 OC_INT4m& value1,
 OC_INT4m& value2,
 OC_INT4m& value3)
{
  int tmp1,tmp2;
  if((tmp1 = chan.ReadChar())<0 || (tmp2 = chan.ReadChar())<0) {
    return 1; // Presumed EOF
  }
  value1 = tmp1*256 + tmp2;
  if((tmp1 = chan.ReadChar())<0 || (tmp2 = chan.ReadChar())<0) {
    return 1; // Presumed EOF
  }
  value2 = tmp1*256 + tmp2;
  if((tmp1 = chan.ReadChar())<0 || (tmp2 = chan.ReadChar())<0) {
    return 1; // Presumed EOF
  }
  value3 = tmp1*256 + tmp2;
  return 0;
}


void Nb_ImgObj::LoadFile(const char* infilename)
{
  Tcl_Interp* interp=Oc_GlobalInterpreter();

  Free();

  filename.Dup(infilename);

  Nb_InputChannel chan;
  chan.OpenFile(filename,"binary");

  // See if we file is a Microsoft .bmp format that we can read.
  {
    Tcl_Channel tclchan = chan.DetachChannel();
    if(ReadMSBitmap(tclchan)) return;  // Yes.

    // No. Reset channel.
    Tcl_Seek(tclchan,0,SEEK_SET);
    chan.AttachChannel(tclchan,filename);
  }

  // Check file type.  NOTE: PPM headers are in ASCII format.  The
  // following magic number checks should be modified to compare
  // against pure values (e.g., 'P'=0x50, '3'=0x33) if this code is
  // ever ported to an EBCDIC. :^)
  ImgFmt image_type = IMG_UNKNOWN;
  int byte1=0,byte2=0,byte3=0;
  if((byte1=chan.ReadChar())<0 || (byte2=chan.ReadChar())<0
                               || (byte3=chan.ReadChar())<0) {
    if(chan.Close(interp)!=TCL_OK) {
      // Close error.
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                            NB_IMGOBJ_ERRBUFSIZE+2000,
                            "Premature EOF reading header"
                            " plus error closing input channel"
                            " for file \"%.2000s\".",
                            filename.GetStr()));
    }
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                          NB_IMGOBJ_ERRBUFSIZE+2000,
                          "Premature EOF reading header"
                          " on input channel for file \"%.2000s\".",
                          filename.GetStr()));
  }

  if(byte1=='P' && byte2 == '3' && isspace(byte3)) {
    image_type = P3;
  } else if(byte1=='P' && byte2 == '6' && isspace(byte3)) {
    image_type = P6_8; // Temporary assignment.  This may be
    // reassigned to P6_16 depending on the maxvalue value.
  }
  if(image_type == IMG_UNKNOWN) {
    // Unsupported format.  Close and send to any2ppm for conversion.
    // Re-purpose chan to point to command channel.
    char buf[NB_IMGOBJ_CMDBUFSIZE];
    if(chan.Close(interp)!=TCL_OK) {
      // Close error.
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                         NB_IMGOBJ_ERRBUFSIZE+2000,
                         "Error closing input channel for file \"%.2000s\".",
                         filename.GetStr()));
    }

    Tcl_SavedResult saved;
    Tcl_SaveResult(interp, &saved);

    // Find name of null device on platform
    Oc_Snprintf(buf,sizeof(buf),
                "[Oc_Config RunPlatform] GetValue path_device_null");
    if(Tcl_Eval(interp,buf)!=TCL_OK) {
      Tcl_RestoreResult(interp, &saved);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                         NB_IMGOBJ_ERRBUFSIZE,
                         "Can't determine null device name."));
    }
    Oc_AutoBuf nulldevice = Tcl_GetStringResult(interp);
    Tcl_ResetResult(interp);

    // Construct command line for launching any2ppm
    Oc_Snprintf(buf,sizeof(buf),"Oc_Application CommandLine any2ppm"
                " -noinfo -format P6 -o - {%s}",
                filename.GetStr());
    if(Tcl_Eval(interp,buf)!=TCL_OK) {
      Tcl_RestoreResult(interp, &saved);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                         NB_IMGOBJ_ERRBUFSIZE+2000,
                         "Can't find any2ppm program to"
                         " convert image file \"%.2000s\" to PPM P6 format.",
                         filename.GetStr()));
    }
#if (OC_SYSTEM_SUBTYPE==OC_DARWIN)
    // On Mac OS X/Aqua, if stdin redirects from /dev/null, then the
    // console is hooked up to the standard channels, redirecting stdout
    // and stderr.  (stdin == /dev/null is used to detect "double-click"
    // launching of wish from Finder.)  This will, of course, break our
    // any2ppm filter, so as a workaround just don't redirect stdin.
    // BTW, this is not an issue on Mac OS X/X11, but I'm not sure what
    // a reliable compile-time way to detect X11 vs. Aqua is.
    Oc_Snprintf(buf,sizeof(buf),"%s 2> %s",Tcl_GetStringResult(interp),
                nulldevice.GetStr());
#else
    Oc_Snprintf(buf,sizeof(buf),"%s < %s 2> %s",Tcl_GetStringResult(interp),
                nulldevice.GetStr(),nulldevice.GetStr());
#endif
    Tcl_ResetResult(interp);

    // Parse command line
    int argc;
    CONST84 char ** argv;
    if(Tcl_SplitList(NULL,buf,&argc,&argv)!=TCL_OK) {
      Tcl_RestoreResult(interp, &saved);
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                         NB_IMGOBJ_ERRBUFSIZE+2000,
                         "Can't parse command string: \"%.2000s\"",buf));
    }

    // Launch any2ppm command pipe
    Tcl_Channel tmpchan = Tcl_OpenCommandChannel(interp,argc,argv,TCL_STDOUT);
    if(tmpchan==NULL) {
      Oc_Exception errmsg(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                 NB_IMGOBJ_ERRBUFSIZE+strlen(Tcl_GetStringResult(interp)),
                 "any2ppm launch error: %s",Tcl_GetStringResult(interp));
      Tcl_RestoreResult(interp, &saved);
      Tcl_Free((char *)argv);
      OC_THROW(errmsg);
    }
    Tcl_Free((char *)argv);
    Tcl_RestoreResult(interp, &saved);

    if(Tcl_SetChannelOption(NULL,tmpchan,OC_CONST84_CHAR("-translation"),
			    OC_CONST84_CHAR("binary"))!=TCL_OK ||
       Tcl_SetChannelOption(NULL,tmpchan,OC_CONST84_CHAR("-buffering"),
			    OC_CONST84_CHAR("full"))!=TCL_OK ||
       Tcl_SetChannelOption(NULL,tmpchan,OC_CONST84_CHAR("-buffersize"),
			    OC_CONST84_CHAR("65536"))!=TCL_OK) {
      // Channel configuration error.
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                NB_IMGOBJ_ERRBUFSIZE+2000,
                "Unable to configure any2ppm command pipe"
                " for file \"%.2000s\".",
                filename.GetStr()));
    }
    Oc_Snprintf(buf,sizeof(buf),"any2ppm < \"%s\" |",filename.GetStr());
    chan.AttachChannel(tmpchan,buf);

    // Read new file type info
    if((byte1=chan.ReadChar())<0 || (byte2=chan.ReadChar())<0
       || (byte3=chan.ReadChar())<0) {
      if(chan.Close(interp)!=TCL_OK) {
        // Close error.
        OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                              NB_IMGOBJ_ERRBUFSIZE+4000,
                              "Premature EOF reading child process"
                              " any2ppm output plus"
                              " error closing input channel"
                              " from file \"%.2000s\".",
                              filename.GetStr()));
      }
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                            NB_IMGOBJ_ERRBUFSIZE+2000,
                            "Premature EOF reading child process"
                            " any2ppm output"
                            " from file \"%.2000s\".",
                            filename.GetStr()));
    }
    if(byte1=='P' && byte2 == '3' && isspace(byte3)) {
      image_type = P3;
    } else if(byte1=='P' && byte2 == '6' && isspace(byte3)) {
      image_type = P6_8; // Temporary assignment.  This may be
      // reassigned to P6_16 depending on the maxvalue value.
    }
    if(image_type == IMG_UNKNOWN) {
      // Not a PPM file?
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                 NB_IMGOBJ_ERRBUFSIZE+2000,
                 "Failure (byte1 = %d != %d) in PPM conversion for file"
                 " \"%.2000s\".",
                 byte1,'P',filename.GetStr()));
    }
  }

  // Read PPM header
  OC_INT4m errcode = 0;
  errcode += ReadPPMTextInt(chan,width);
  errcode += ReadPPMTextInt(chan,height);
  errcode += ReadPPMTextInt(chan,maxvalue);
  if(errcode!=0) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Read error in header for file \"%.2000s\".",
             filename.GetStr()));
  }

  if(width<1 || height<1 || maxvalue<0) {
    // Bad file data.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Bad header reading file \"%.2000s\".",
             filename.GetStr()));
  }
  data.SetSize(height,NB_IMGOBJ_DATUMSIZE*width);

  if(image_type == P6_8 && maxvalue>255) {
    // Correction.  This is actually P6_16 format
    image_type = P6_16;
    if(maxvalue>65535) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
               NB_IMGOBJ_ERRBUFSIZE+2000,
               "Error in Nb_ImgObj::LoadFile: "
               "Bad header reading file \"%.2000s\"; PPM P6 file "
               "has maxvalue=%d>65535.",filename.GetStr(),maxvalue));
    }
  }

  // Point ReadThreeValues member function pointer to the
  // appropriate routine for the given image type.
  OC_INT4m (*ReadThreeValues)(Nb_InputChannel&,OC_INT4m&,OC_INT4m&,OC_INT4m&)
    = NULL;
  switch(image_type) {
  case P3:
    ReadThreeValues = &Nb_ImgObj::ReadThreeIntPPMText;
    break;
  case P6_8:
    ReadThreeValues = &Nb_ImgObj::ReadThreeIntPPMBin8;
    break;
  case P6_16:
    ReadThreeValues = &Nb_ImgObj::ReadThreeIntPPMBin16;
    break;
  default:
    {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
               NB_IMGOBJ_ERRBUFSIZE+2000,
               "Bad format reading file \"%.2000s\"; Programming error?",
               filename.GetStr()));
    }
  }

  assert(NB_IMGOBJ_DATUMSIZE==3);
  OC_INT4m red,green,blue;
  if(maxvalue<=255) {
    // Usual case: Input data fit into one byte each
    for(OC_INT4m i=0;i<height;++i) {
      for(OC_INT4m j=0;j<NB_IMGOBJ_DATUMSIZE*width;j+=NB_IMGOBJ_DATUMSIZE) {
        if((errcode = (*ReadThreeValues)(chan,red,green,blue))!=0) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                   NB_IMGOBJ_ERRBUFSIZE+2000,
                   "Bad data or EOF in file \"%.2000s\"; "
                   "offset=(%d,%d), errcode=%d.",
                   filename.GetStr(),static_cast<int>(i),
                   static_cast<int>(j/NB_IMGOBJ_DATUMSIZE),
                   static_cast<int>(errcode)));
        }
        data[i][j]   = static_cast<unsigned char>(red);
        data[i][j+1] = static_cast<unsigned char>(green);
        data[i][j+2] = static_cast<unsigned char>(blue);
      }
    }
  } else { // maxvalue>255
    // Nb_ImgObj data storage only supports 1 byte widths.  If
    // maxvalue is larger than 255, adjust for this on import.
    OC_REAL8m scale = 255.5/static_cast<OC_REAL8m>(maxvalue);
    maxvalue = 255;
    for(OC_INT4m i=0;i<height;++i) {
      for(OC_INT4m j=0;j<NB_IMGOBJ_DATUMSIZE*width;j+=NB_IMGOBJ_DATUMSIZE) {
        if((errcode = (*ReadThreeValues)(chan,red,green,blue))!=0) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
                   NB_IMGOBJ_ERRBUFSIZE+2000,
                   "Bad data or EOF in file \"%.2000s\"; "
                   "offset=(%d,%d), errcode=%d.",
                   filename.GetStr(),static_cast<int>(i),
                   static_cast<int>(j/NB_IMGOBJ_DATUMSIZE),
                   static_cast<int>(errcode)));
        }
        data[i][j]   =
          static_cast<unsigned char>(floor(static_cast<OC_REAL8m>(red)*scale));
        data[i][j+1] =
          static_cast<unsigned char>(floor(static_cast<OC_REAL8m>(green)*scale));
        data[i][j+2] =
          static_cast<unsigned char>(floor(static_cast<OC_REAL8m>(blue)*scale));
      }
    }
  }

  if(chan.Close(interp)!=TCL_OK) {
    // Close error.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","LoadFile",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Error closing input channel for file \"%.2000s\".",
             filename.GetStr()));
  }

  return;
}

////////////////////////////////////////////////////////////////////////
// MsBitmap and related classes
struct Nb_ImgObj_RGBQuad {
public:
  // Note: MS RGBQuad's are ordered "Blue Green Red Reserved"
  OC_BYTE Blue;
  OC_BYTE Green;
  OC_BYTE Red;
  OC_BYTE Reserved;
};

struct Nb_ImgObj_MSHeader {
public:
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
  Nb_ImgObj_RGBQuad* Palette;

  void FreePalette() {
    if(Palette!=NULL) delete[] Palette;
    Palette = NULL;
  }

  void AllocPalette(OC_UINT4m size) {
    FreePalette();
    Palette=new Nb_ImgObj_RGBQuad[size];
  }

  Nb_ImgObj_MSHeader() :
    FileSize(0),Reserved1(0),Reserved2(0),OffBits(0),
    BmiSize(0),Width(0),Height(0),Planes(0),BitCount(0),Compression(0),
    SizeImage(0),XPelsPerMeter(0),YPelsPerMeter(0),
    ClrUsed(0),ClrImportant(0),Palette(NULL),
    PaletteSize(0)
  {
    Type[0]=Type[1]='\0';
  }

  ~Nb_ImgObj_MSHeader() { FreePalette(); }

  int FillHeader(Tcl_Channel chan);
  ///   Returns 1 if successful, 0 if the input does not appear
  /// to be a valid MS bitmap.  Adjusts for alignment and byte
  /// ordering as needed.

  OC_UINT4m GetFileRowSize() {
    return 4*((Width*BitCount+31)/32); // Rows lie on a 4-byte bdry.
  }

  OC_UINT4m GetPaletteSize() {
    return PaletteSize;
  }
private:
  OC_UINT4m PaletteSize;
};

int Nb_ImgObj_MSHeader::FillHeader(Tcl_Channel chan)
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

  // Fill palette
  FreePalette();
  Tcl_Seek(chan,bmistart+BmiSize,SEEK_SET); // Position at
  /// end of BITMAPINFOHEADER

  PaletteSize = ClrUsed;
  if(PaletteSize==0) {
    switch(BitCount)
      {
      case  1: PaletteSize=2;    break;
      case  4: PaletteSize=16;   break;
      case  8: PaletteSize=256;  break;
      case 24: PaletteSize=0;    break;  // True color
      default: return 0;     // Invalid BitCount value
      }
  }
  if(PaletteSize>0) {
    AllocPalette(PaletteSize);
    size_t rsize = size_t(PaletteSize)*sizeof(Palette[0]);
    assert(rsize <= INT_MAX);
    Tcl_Read(chan,(char *)Palette,static_cast<int>(rsize));
  }

  return 1;
}

void
Nb_ImgObj::ReadBmp1bit
(Tcl_Channel chan,
 Nb_ImgObj_MSHeader* header)
{
  // Note: Rows in the input file run from bottom to top.
  if(header->GetPaletteSize() != 2) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp1bit",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Invalid palette size (%u) specified in header---palette"
             " size should be 2; Bad header reading file \"%.2000s\".",
             header->GetPaletteSize(),filename.GetStr()));
  }
  Nb_ImgObj_RGBQuad offbit = header->Palette[0];
  Nb_ImgObj_RGBQuad onbit  = header->Palette[1];
  assert(header->GetFileRowSize() <= INT_MAX);
  const int filerowsize = static_cast<int>(header->GetFileRowSize());
  if((width-1)/8 >= filerowsize) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp1bit",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Image width (%d) is wider than file row width (%d)"
             " as specified in header;"
             " Bad header reading file \"%.2000s\".",
             width,8*filerowsize,filename.GetStr()));
  }
  Oc_AutoBuf buf;
  assert(filerowsize>=0);
  buf.SetLength(size_t(filerowsize));
  unsigned char* const chbuf = (unsigned char*)buf.GetStr();
  for(OC_INT4m i=height-1;i>=0;--i) {
    if(Tcl_Read(chan,(char*)chbuf,filerowsize) != filerowsize) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp1bit",
              NB_IMGOBJ_ERRBUFSIZE+2000,
              "Premature end-of-file reading image file \"%.2000s\".",
              filename.GetStr()));
    }
    OC_INT4m j=0;
    while(j<width) {
      unsigned int datum=chbuf[j/8];
      for(unsigned int mask=0x80; mask!=0 && j<width ; mask>>=1, ++j) {
        if((datum & mask)==0) {
          data[i][NB_IMGOBJ_DATUMSIZE*j]   = offbit.Red;
          data[i][NB_IMGOBJ_DATUMSIZE*j+1] = offbit.Green;
          data[i][NB_IMGOBJ_DATUMSIZE*j+2] = offbit.Blue;
        } else {
          data[i][NB_IMGOBJ_DATUMSIZE*j]   = onbit.Red;
          data[i][NB_IMGOBJ_DATUMSIZE*j+1] = onbit.Green;
          data[i][NB_IMGOBJ_DATUMSIZE*j+2] = onbit.Blue;
        }
      }
    }
  }
}

void
Nb_ImgObj::ReadBmp4bit
(Tcl_Channel chan,
 Nb_ImgObj_MSHeader* header)
{
  // Note: Rows in the input file run from bottom to top.
  const Nb_ImgObj_RGBQuad* pal = header->Palette;
  assert(header->GetFileRowSize() <= INT_MAX);
  const int filerowsize = static_cast<int>(header->GetFileRowSize());
  if( (width-1)/2 >= filerowsize) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp4bit",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Image width (%d) is wider than file row width (%d)"
             " as specified in header;"
             " Bad header reading file \"%.2000s\".",
             width,2*filerowsize,filename.GetStr()));
  }
  const OC_UINT4m palsize = header->GetPaletteSize();
  Oc_AutoBuf buf;
  assert(filerowsize>=0);
  buf.SetLength(size_t(filerowsize));
  unsigned char* const chbuf = (unsigned char*)buf.GetStr();
  for(OC_INT4m i=height-1;i>=0;--i) {
    if(Tcl_Read(chan,(char*)chbuf,filerowsize) != filerowsize) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp4bit",
               NB_IMGOBJ_ERRBUFSIZE+2000,
               " Premature end-of-file reading image file \"%.2000s\".",
               filename.GetStr()));
    }
    OC_INT4m j=0;
    while(j<width) {
      unsigned int datum=chbuf[j/2];
      OC_UINT4 index1=  datum & 0x0F;        // Note bit-order!
      OC_UINT4 index0= (datum & 0xF0)>>4;
      if(index0 >= palsize) {
        // palsize will usually be 16, in which case this check isn't
        // needed.  If this check turns out to be a performance
        // problem, then we should probably handle the palsize<16
        // case in a separate branch.
        OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp4bit",
                 NB_IMGOBJ_ERRBUFSIZE+2000,
                 "Palette index request (%u) is outside palette range"
                 " (0-%u); Bad data reading file \"%.2000s\".",
                 index0,palsize,filename.GetStr()));
      }
      data[i][NB_IMGOBJ_DATUMSIZE*j]   = pal[index0].Red;
      data[i][NB_IMGOBJ_DATUMSIZE*j+1] = pal[index0].Green;
      data[i][NB_IMGOBJ_DATUMSIZE*j+2] = pal[index0].Blue;
      if(++j>=width) break;
      if(index1 >= palsize) {
        OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp4bit",
                 NB_IMGOBJ_ERRBUFSIZE+2000,
                 "Palette index request (%u) is outside palette range"
                 " (0-%u); Bad data reading file \"%.2000s\".",
                 index1,palsize,filename.GetStr()));
      }
      data[i][NB_IMGOBJ_DATUMSIZE*j]   = pal[index1].Red;
      data[i][NB_IMGOBJ_DATUMSIZE*j+1] = pal[index1].Green;
      data[i][NB_IMGOBJ_DATUMSIZE*j+2] = pal[index1].Blue;
      ++j;
    }
  }
}

void
Nb_ImgObj::ReadBmp8bit
(Tcl_Channel chan,
 Nb_ImgObj_MSHeader* header)
{
  // Note: Rows in the input file run from bottom to top.
  const Nb_ImgObj_RGBQuad* pal = header->Palette;
  assert(header->GetFileRowSize() <= INT_MAX);
  const int filerowsize = static_cast<int>(header->GetFileRowSize());
  if(width > filerowsize) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp8bit",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Image width (%d) is wider than file row width (%d)"
             " as specified in header;"
             " Bad header reading file \"%.2000s\".",
             width,filerowsize,filename.GetStr()));
  }
  const OC_UINT4m palsize = header->GetPaletteSize();
  Oc_AutoBuf buf;
  assert(filerowsize>=0);
  buf.SetLength(size_t(filerowsize));
  unsigned char* const chbuf = (unsigned char*)buf.GetStr();
  for(OC_INT4m i=height-1;i>=0;--i) {
    if(Tcl_Read(chan,(char*)chbuf,filerowsize) != filerowsize) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp8bit",
               NB_IMGOBJ_ERRBUFSIZE+2000,
               "Premature end-of-file reading image file \"%.2000s\".",
               filename.GetStr()));
    }
    for(OC_INT4m j=0;j<width;++j) {
      OC_UINT4m index = chbuf[j];
      if(index >= palsize) {
        // palsize will usually be 256, in which case this check
        // isn't needed.  If this check turns out to be a performance
        // problem, then we should probably handle the palsize<256
        // case in a separate branch.
        OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp8bit",
                 NB_IMGOBJ_ERRBUFSIZE+2000,
                 "Palette index request (%u) is outside palette range"
                 " (0-%u); Bad data reading file \"%.2000s\".",
                 index,palsize,filename.GetStr()));
      }
      data[i][NB_IMGOBJ_DATUMSIZE*j]   = pal[index].Red;
      data[i][NB_IMGOBJ_DATUMSIZE*j+1] = pal[index].Green;
      data[i][NB_IMGOBJ_DATUMSIZE*j+2] = pal[index].Blue;
    }
  }
}

void
Nb_ImgObj::ReadBmp24bit
(Tcl_Channel chan,
 Nb_ImgObj_MSHeader* header)
{ // True color input file; 24 bits per pixel, no palette.
  // Note 1: Byte order in input file is "Blue Green Red".
  // Note 2: Rows end on 4-byte boundary.
  // Note 3: Rows in the input file run from bottom to top.
  assert(header->GetFileRowSize() <= INT_MAX);
  const int filerowsize = static_cast<int>(header->GetFileRowSize());
  if(width > filerowsize) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp24bit",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Image width (%d) is wider than file row width (%d)"
             " as specified in header;"
             " Bad header reading file \"%.2000s\".",
             width,filerowsize,filename.GetStr()));
  }
  Oc_AutoBuf buf;
  assert(filerowsize>=0);
  buf.SetLength(size_t(filerowsize));
  unsigned char* const chbuf = (unsigned char*)buf.GetStr();
  for(OC_INT4m i=height-1;i>=0;--i) {
    if(Tcl_Read(chan,(char*)chbuf,filerowsize) != filerowsize) {
      OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadBmp24bit",
               NB_IMGOBJ_ERRBUFSIZE+2000,
               "Premature end-of-file reading image file \"%.2000s\".",
               filename.GetStr()));
    }
    for(OC_INT4m j=0;j<NB_IMGOBJ_DATUMSIZE*width;j+=NB_IMGOBJ_DATUMSIZE) {
      data[i][j]   = chbuf[j+2];  // Red
      data[i][j+1] = chbuf[j+1];  // Green
      data[i][j+2] = chbuf[j];    // Blue
    }
  }
}

int
Nb_ImgObj::ReadMSBitmap(Tcl_Channel  chan)
{ // Attempts to fill in data, one row at a time.  Returns
  // 1 on success, 0 if the data on chan is either not in
  // MS .bmp format, or is in an MS .bmp format we don't
  // support.

  Nb_ImgObj_MSHeader header;

  if(header.FillHeader(chan) == 0) return 0;

  // Looks like an MS bitmap file.  Is it in a subformat
  // we support?
  if(header.Planes!=1         ||      // Only known value
     header.Compression!=0    ||      // No compression
     (header.BitCount!=1 && header.BitCount!=4 && header.BitCount!=8
      && header.BitCount!=24) // Bits per pixel check
     ) {
    return 0;
  }

  width = static_cast<OC_INT4m>(header.Width);
  height = static_cast<OC_INT4m>(header.Height);
  assert(width>=0 && height>=0); // Overflow check
  maxvalue = 255;  // I guess...

  if(width<1 || height<1) {
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadMSBitmap",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Bad header reading file \"%.2000s\"",
             filename.GetStr()));
  }

  data.SetSize(height,NB_IMGOBJ_DATUMSIZE*width);

  // Fill in data, using routine determined by bits-per-pixel value.
  switch(header.BitCount) {
  case 1:
    ReadBmp1bit(chan,&header);
    break;
  case 4:
    ReadBmp4bit(chan,&header);
    break;
  case 8:
    ReadBmp8bit(chan,&header);
    break;
  case 24:
    ReadBmp24bit(chan,&header);
    break;
  default:
    // The subformat check above should insure that we don't get here.
    OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj","ReadMSBitmap",
             NB_IMGOBJ_ERRBUFSIZE+2000,
             "Bad format reading file \"%.2000s\"; Programming error?",
             filename.GetStr()));
  }

  return 1;
}

////////////////////////////////////////////////////////////////////////
// Tcl command interface to Nb_ImgObj.  The input is the name
// of the file to read.  The output is a list, in pseudo-P3
// format: image_width image_height max_color_value
//         red0 green0 blue0  red1 green1 blue1  ...
//         ... redN greenN blueN
// where N + 1 = image_width*image_height, and each red/green/blue
// component lies between 0 and max_color_value-1.  The scan direction
// is from the upper lefthand corner of the image, progressing across
// and down in normal English reading order.  In other words, the
// list looks like a P3 file, but without the leading "P3" and with
// no comments.
//   If the "-double" option is specified, then the max_color_value
// in the return list has (integer) value 1, and the remainder of
// the return is a list of double values, in the range [0,1].
//   WARNING: If you are using the old string-interface (i.e., Tcl
// version 7.x), then the pixel values under the -double option
// are formatted using "%.17g", which in toto may be quite large.
//
#if TCL_MAJOR_VERSION < 8  // Use string interface
int NbImgObjCmd(ClientData,Tcl_Interp *interp,int argc,
                CONST84 char** argv)
{
  Tcl_ResetResult(interp);

  int double_flag = 0;
  const char* filename = NULL;
  for(int index=1;index<argc;++index) {
    const char* tmpstr = argv[index];
    if(strcmp("-double",tmpstr)==0) {
      double_flag = 1;
    } else {
      if(filename != NULL) {
        char buf[1024];
        Oc_Snprintf(buf,sizeof(buf),"wrong # args: should be \"Nb_ImgObj"
                    " ?-double? filename\" (%d arguments passed)",argc-1);
        Tcl_AppendResult(interp,buf,(char *)NULL);
        return TCL_ERROR;
      }
      filename = tmpstr;
    }
  }
  if(filename == NULL) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"wrong # args: should be \"Nb_ImgObj"
                " ?-double? filename\" (%d arguments passed)",argc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Nb_ImgObj image;
  image.LoadFile(filename);
  const OC_INT4m imgwidth  = image.Width();
  const OC_INT4m imgheight = image.Height();
  const OC_INT4m maxval    = image.MaxValue();
  const double dblscale = 1.0/static_cast<double>(maxval);

  char cbuf[1024];

  Tcl_DString tbuf;
  Tcl_DStringInit(&tbuf);

  Oc_Snprintf(cbuf,sizeof(cbuf),"%ld %ld %d\n",
              (long int)imgwidth,(long int)imgheight,
              (double_flag ? 1 : (int)maxval));
  Tcl_DStringAppend(&tbuf, cbuf, -1);

  for(OC_INT4m i=0;i<imgheight;++i) {
    for(OC_INT4m j=0;j<imgwidth;++j) {
      OC_INT4m r,g,b;
      image.PixelValue(i,j,r,g,b);
      if(!double_flag) {
        Oc_Snprintf(cbuf,sizeof(cbuf),"%ld %ld %ld\n",
                    (long int)r,(long int)g,(long int)b);
      } else {
        Oc_Snprintf(cbuf,sizeof(cbuf),"%.17g %.17g %.17g\n",
                    (double)r*dblscale,(double)g*dblscale,
                    (double)b*dblscale);
      }
      Tcl_DStringAppend(&tbuf, cbuf, -1);  // Alternatively,
      /// we could use Tcl_DStringAppendElement().
    }
  }
  Tcl_DStringResult(interp,&tbuf);
  Tcl_DStringFree(&tbuf);
  return TCL_OK;
}

#else // TCL_MAJOR_VERSION >=8 --> use obj interface

int NbImgObjCmd(ClientData,Tcl_Interp *interp,int objc,
                Tcl_Obj * CONST objv[])
{
  Tcl_ResetResult(interp);

  int double_flag = 0;
  const char* filename = NULL;
  for(int index=1;index<objc;++index) {
    const char* tmpstr = Tcl_GetString(objv[index]);
    if(strcmp("-double",tmpstr)==0) {
      double_flag = 1;
    } else {
      if(filename != NULL) {
        char buf[1024];
        Oc_Snprintf(buf,sizeof(buf),"wrong # args: should be \"Nb_ImgObj"
                    " ?-double? filename\" (%d arguments passed)",objc-1);
        Tcl_AppendResult(interp,buf,(char *)NULL);
        return TCL_ERROR;
      }
      filename = tmpstr;
    }
  }
  if(filename == NULL) {
    char buf[1024];
    Oc_Snprintf(buf,sizeof(buf),"wrong # args: should be \"Nb_ImgObj"
                " ?-double? filename\" (%d arguments passed)",objc-1);
    Tcl_AppendResult(interp,buf,(char *)NULL);
    return TCL_ERROR;
  }

  Nb_ImgObj image;
  image.LoadFile(filename);
  const OC_INT4m imgwidth  = image.Width();
  const OC_INT4m imgheight = image.Height();
  const OC_INT4m maxval    = image.MaxValue();

  const OC_INT4m arrlen = 3 + 3*imgwidth*imgheight;
  const double dblscale = 1.0/static_cast<double>(maxval);

  // In Tcl 8.4 and 8.5, at least, there is no documented interface
  // to pre-allocate space in a Tcl_List object.  Element-by-element
  // appends to a Tcl_List object will result in repeated memory
  // re-allocations and copying, so it is usually faster to create
  // one big array of Tcl_Obj pointers up front, fill that, and do
  // one copy into a Tcl_List.
  // Some versions of g++ complain about casting the return from ckalloc
  // directly to a Tcl_Obj**, claiming the latter has increased
  // alignment restrictions.  Just hack around via a void*.
  Tcl_Obj **imgobjv
    = static_cast<Tcl_Obj **>((void*)ckalloc((int)arrlen * sizeof(Tcl_Obj *)));

  if(maxval>255) {
    // Create separate Tcl_Obj for every pixel component
    imgobjv[0] = Tcl_NewIntObj(static_cast<int>(imgwidth));
    imgobjv[1] = Tcl_NewIntObj(static_cast<int>(imgheight));
    if(!double_flag) {
      // Export integer values
      imgobjv[2] = Tcl_NewIntObj(static_cast<int>(maxval));
    } else {
      // Export floating point values
      imgobjv[2] = Tcl_NewIntObj(static_cast<int>(1));
    }
    OC_INT4m n = 2;
    for(OC_INT4m i=0;i<imgheight;++i) {
      for(OC_INT4m j=0;j<imgwidth;++j) {
        OC_INT4m r,g,b;
        image.PixelValue(i,j,r,g,b);
        if(!double_flag) {
          // Export integer values
          imgobjv[++n] = Tcl_NewIntObj(static_cast<int>(r));
          imgobjv[++n] = Tcl_NewIntObj(static_cast<int>(g));
          imgobjv[++n] = Tcl_NewIntObj(static_cast<int>(b));
        } else {
          // Export floating point values
          imgobjv[++n] = Tcl_NewDoubleObj(static_cast<double>(r)*dblscale);
          imgobjv[++n] = Tcl_NewDoubleObj(static_cast<double>(g)*dblscale);
          imgobjv[++n] = Tcl_NewDoubleObj(static_cast<double>(b)*dblscale);
        }
      }
    }
    Tcl_SetObjResult(interp,
                     Tcl_NewListObj(static_cast<int>(arrlen),imgobjv));
  } else {
    // Create one Tcl_Obj for each possible pixel value, and fill
    // imgobjv with pointers to those objects.  This can save a lot of
    // space, because each of the Tcl_Obj is at least 24 bytes wide.
    // Some versions of g++ complain about casting the return from
    // ckalloc directly to a Tcl_Obj**, claiming the latter has
    // increased alignment restrictions.  Just hack around via a void*.
    Tcl_Obj **pixval
      = static_cast<Tcl_Obj **>((void*)ckalloc((int)(maxval+1)
                                               * sizeof(Tcl_Obj *)));

    if(!double_flag) {
      for(int k=0;k<=maxval;++k) {
        Tcl_IncrRefCount(pixval[k] = Tcl_NewIntObj(k));
      }
    } else {
      for(int k=0;k<=maxval;++k) {
        pixval[k] = Tcl_NewDoubleObj(static_cast<double>(k)*dblscale);
        Tcl_IncrRefCount(pixval[k]);
      }
    }

    imgobjv[0] = Tcl_NewIntObj(static_cast<int>(imgwidth));
    imgobjv[1] = Tcl_NewIntObj(static_cast<int>(imgheight));
    if(!double_flag) {
      // Export integer values
      imgobjv[2] = Tcl_NewIntObj(static_cast<int>(maxval));
    } else {
      // Export floating point values
      imgobjv[2] = Tcl_NewIntObj(static_cast<int>(1));
    }
    OC_INT4m n = 2;
    for(OC_INT4m i=0;i<imgheight;++i) {
      for(OC_INT4m j=0;j<imgwidth;++j) {
        OC_INT4m r,g,b;
        image.PixelValue(i,j,r,g,b);
        if(r>maxval || g>maxval || b>maxval) {
          OC_THROW(Oc_Exception(__FILE__,__LINE__,"Nb_ImgObj",
                                "Tcl Nb_ImgObj command",
                                NB_IMGOBJ_ERRBUFSIZE+2000,
                                "Invalid image detecting reading file"
                                " \"%.2000s\"",filename));
        }
        imgobjv[++n] = pixval[r]; // Pointers to either IntObjs
        imgobjv[++n] = pixval[g]; // or DoubleObjs.
        imgobjv[++n] = pixval[b];
      }
    }
    Tcl_SetObjResult(interp,
                     Tcl_NewListObj(static_cast<int>(arrlen),imgobjv));

    for(int k=0;k<=maxval;++k) {
      Tcl_DecrRefCount(pixval[k]); // Release any unused Tcl_Obj's.
    }
    ckfree((char*)(pixval));
  }

  ckfree((char*)(imgobjv));

  return TCL_OK;
}
#endif // TCL_MAJOR_VERSION
