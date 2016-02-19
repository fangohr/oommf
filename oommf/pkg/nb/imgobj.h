/* FILE: imgobj.h                 -*-Mode: c++-*-
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
 * is not in format supported by this code, then avf2ppm is called,
 * and avf2ppm does use Tk.
 *
 * NOTICE: Please see the file ../../LICENSE
 *
 * Last modified on: $Date: 2010/07/16 22:33:58 $
 * Last modified by: $Author: donahue $
*/

#ifndef _NB_IMGOBJ
#define _NB_IMGOBJ

#include <tcl.h>

#include "arraywrapper.h"
#include "chanwrap.h"

/* End includes */     /* Optional directive to build.tcl */

#define NB_IMGOBJ_DATUMSIZE 3

struct Nb_ImgObj_MSHeader; // Forward declaration

class Nb_ImgObj {
 public:

  Nb_ImgObj() : width(0),height(0),maxvalue(0) {}
  ~Nb_ImgObj() { Free(); }

  void Free() {
    data.Free();
    width=height=maxvalue=0;
    filename.Dup("");
  }

  void LoadFile(const char* infilename);

  OC_INT4m Width() { return width; }
  OC_INT4m Height() { return height; }
  OC_INT4m MaxValue() { return maxvalue; }
  void PixelValue(OC_INT4m i,OC_INT4m j,
                  OC_INT4m& red,OC_INT4m& green, OC_INT4m& blue) {
    // i is row offset, j is column offset.
    // Items [i][j] and [i][j+1] are contiguous in memory.
    OC_INT4m joff = NB_IMGOBJ_DATUMSIZE*j;
    red   = static_cast<OC_INT4m>(data[i][joff]);
    green = static_cast<OC_INT4m>(data[i][joff+1]);
    blue  = static_cast<OC_INT4m>(data[i][joff+2]);
  }
 private:
  Oc_AutoBuf filename;
  OC_INT4m width;
  OC_INT4m height;
  OC_INT4m maxvalue;

  Nb_2DArrayWrapper<unsigned char> data;

  // Directly supported input formats
  enum ImgFmt { 
    IMG_UNKNOWN,  // Unknown or unsupported format
    P3,    // PPM P3 text format, RGB
    P6_8,  // PPM P6 binary format, RGB, 1 byte x 3
    P6_16, // PPM P6 binary format, RGB, 2 bytes x 3
    BMP_24 // Microsoft BMP binary format, 1 byte x RGB
  };

  // Support functions:
  // ReadPPMTextInt reads a single integer value in text mode from
  //    a channel.  Returns 0 on success, or on error returns
  //         1 => EOF
  //         2 => Input contains characters not allowed in a
  //              positive integer value
  static OC_INT4m ReadPPMTextInt(Nb_InputChannel& chan,OC_INT4m& value);

  // The ReadThreeInt* functions all read 3 int's from a chan,
  // according to image format.
  static OC_INT4m
  ReadThreeIntPPMText(Nb_InputChannel& chan, // P3 format
                      OC_INT4m& value1,
                      OC_INT4m& value2,
                      OC_INT4m& value3);
  static OC_INT4m
  ReadThreeIntPPMBin8(Nb_InputChannel& chan, // P6_8 format
                      OC_INT4m& value1,
                      OC_INT4m& value2,
                      OC_INT4m& value3);
  static OC_INT4m
  ReadThreeIntPPMBin16(Nb_InputChannel& chan, // P6_16 format
                       OC_INT4m& value1,
                       OC_INT4m& value2,
                       OC_INT4m& value3);

  // Support functions for Microsoft .bmp format.
  // The ReadBmp*bit routines assume that chan is positioned
  // at the start of the bitmap data, that the "width" and
  // "height" member values are already set, and that "data"
  // has been configured to the right size.  The ReadMSBitmap
  // routine is the entry point into the MS bitmap code.  It
  // returns 1 if the chan was successfully used to fill "data",
  // or 0 if the file could not be read.
  // Side-effects of calling ReadMSBitmap include re-positioning of
  // the chan pointer.  On failure, subsequent read attempts should
  // first reset the channel to the beginning of the stream.
  void ReadBmp1bit(Tcl_Channel chan,
                   Nb_ImgObj_MSHeader* header);
  void ReadBmp4bit(Tcl_Channel chan,
                   Nb_ImgObj_MSHeader* header);
  void ReadBmp8bit(Tcl_Channel chan,
                   Nb_ImgObj_MSHeader* header);
  void ReadBmp24bit(Tcl_Channel chan,
                    Nb_ImgObj_MSHeader* header);

  int ReadMSBitmap(Tcl_Channel  chan);


  // Disable copy constructor and assignment operator by
  // declaring them w/o defining them.
  Nb_ImgObj(const Nb_ImgObj&);
  Nb_ImgObj& operator=(const Nb_ImgObj&);
};

// Tcl command interface to Nb_ImgObj.  The input is the name
// of the file to read.  The output is a list, in pseudo-P3
// format: image_width image_height max_color_value
//         red0 green0 blue0  red1 green1 blue1  ...
//         ... redN greenN blueN
// where N +1 = image_width*image_height, and each red/green/blue
// component lies between 0 and max_color_value-1.  The scan direction
// is from the upper lefthand corner of the image, progressing across
// and down in normal English reading order.  In other words, the
// list looks like a P3 file, but without the leading "P3" and with
// no comments.
//
#if TCL_MAJOR_VERSION < 8  // String interface
Tcl_CmdProc NbImgObjCmd;
#else // Obj interface
Tcl_ObjCmdProc NbImgObjCmd;
#endif

#endif // _NB_IMGOBJ
