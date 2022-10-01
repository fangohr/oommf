/* FILE: fileio.h                 -*-Mode: c++-*-
 *
 * File input/output routines for deprecated VIO format
 *
 * Last modified on: $Date: 2010/07/16 22:34:01 $
 * Last modified by: $Author: donahue $
 */

#ifndef _VF_FILEIO
#define _VF_FILEIO

#include <cstdio>

#include "nb.h"

#include "mesh.h"

/* End includes */     /* Optional directive to build.tcl */

class Vf_VioFile {
private:
  static const ClassDoc class_doc;
  OC_BOOL swap_input_bytes; // If true, swap bytes on input
  FILE *fptr;
  OC_INDEX width,height,depth;
  /// Note: width, height and depth correspond to Nx, Ny and Nz
  /// in the Vio file header, but the latter are defined to be
  /// 4-byte ints, which may be smaller than OC_INDEX.
  OC_INT4m ReadHeader(); // Assumes fptr is correctly positioned
public:
  Vf_VioFile(void) { fptr=(FILE *)NULL; width=height=depth=0; }
  ~Vf_VioFile(void) { CloseFile(); }

  OC_INT4m OpenFileForInput(const char *filename);

  OC_INT4m SetFilePtr(FILE *newfptr) { 
    CloseFile();  fptr=newfptr;
    return ReadHeader();
  }

  OC_INDEX GetDimensions(OC_INDEX &xwidth,OC_INDEX &yheight,OC_INDEX &zdepth) {
    xwidth=width; yheight=height; zdepth=depth;
    return width*height*depth;
  }

  OC_INT4m FillArray(Vf_GridVec3f &grid);
  // Reads magnetization vector array; Assumes fptr is correctly positioned

  void CloseFile();
};

#endif // _VF_FILEIO
