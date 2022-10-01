/* FILE: fileio.cc                 -*-Mode: c++-*-
 *
 * File input/output routines
 *
 * Last modified on: $Date: 2013/05/22 07:15:37 $
 * Last modified by: $Author: donahue $
 */

#include <cassert>
#include <cstring>
#include "oc.h"
#include "nb.h"

#include "fileio.h"

/* End includes */     // This is an optional directive to build.tcl,
                       // that excludes the remainder of the file from
                       // '#include ".*"' dependency building

#define VECDIM 3  // Currently *assume* all vectors are 3D

// Regular 3D grid of Nb_Vec3<OC_REAL4>'s
const ClassDoc Vf_VioFile::class_doc("Vf_VioFile",
		    "Michael J. Donahue (michael.donahue@nist.gov)",
		    "1.0.0","16-Dec-1996");

OC_INT4m Vf_VioFile::OpenFileForInput(const char *filename)
{
#define MEMBERNAME "OpenFileForInput"
  CloseFile();
  if((fptr=Nb_FOpen(filename,"rb"))==(FILE *)NULL) {
    NonFatalError(STDDOC,"%s %s",ErrBadFileOpen,filename);
    return 1;
  }
  return ReadHeader();
#undef MEMBERNAME
}

OC_INT4m Vf_VioFile::ReadHeader(void)
{
#define MEMBERNAME "ReadHeader"
  // Init class member variables to 0 (in case of errors).
  width=height=depth=0;

  if(fptr==(FILE *)NULL)
    { NonFatalError(STDDOC,"File stream not initialized"); return 1; }

  struct Vf_VioFileHeader {
    OC_CHAR    Magic[8];
    OC_REAL8   EndianTest;
    OC_INT4    Nx,Ny,Nz,dimen,dataoffset;
    OC_CHAR    Reserved[36];
    OC_CHAR    Note[448];
  } head;

  const char   *VecFileMagic02 = "VecFil\x02";
  const double VecFileEndianTest = 1.234;
  // The bit pattern for 1.234 on a little endian machine (e.g.,
  // Intel's x86 series) should be 58 39 B4 C8 76 BE F3 3F, while
  // on a big endian machine it is 3F F3 BE 76 C8 B4 39 58.  Examples
  // of big endian machines include ones using the MIPS R4000 and R8000
  // series chips, for example SGI's.

  // Read & test magic cookie
  if(fread((char *)head.Magic,1,sizeof(head.Magic),fptr)
     !=sizeof(head.Magic))
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }
  if(strcmp(head.Magic,VecFileMagic02)!=0)
    { NonFatalError(STDDOC,ErrBadFileHeader); return 1; }

  // Read rest of header
  if(fread((char *)&head.EndianTest,sizeof(head.EndianTest),1,fptr)!=1)
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }
  if(fread((char *)&head.Nx,sizeof(head.Nx),1,fptr)!=1)
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }
  if(fread((char *)&head.Ny,sizeof(head.Ny),1,fptr)!=1)
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }
  if(fread((char *)&head.Nz,sizeof(head.Nz),1,fptr)!=1)
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }
  if(fread((char *)&head.dimen,sizeof(head.dimen),1,fptr)!=1)
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }
  if(fread((char *)&head.dataoffset,sizeof(head.dataoffset),1,fptr)!=1)
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }
  if(fread((char *)head.Reserved,1,sizeof(head.Reserved),fptr)
     !=sizeof(head.Reserved))
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }
  if(fread((char *)head.Note,1,sizeof(head.Note),fptr)
     !=sizeof(head.Note))
    { NonFatalError(STDDOC,ErrInputEOF); return 1; }

  // Determine byte order
  if(head.EndianTest==VecFileEndianTest) swap_input_bytes=0;
  else {
    OC_REAL8 dummy=head.EndianTest;
    Oc_Flip8(&dummy);
    if(dummy==VecFileEndianTest) swap_input_bytes=1;
    else { NonFatalError(STDDOC,"Unknown binary byte order"); return 1; }
  }
  if(swap_input_bytes) {
    if(Verbosity>=100) Message("Reversing binary byte order on input\n");
    Oc_Flip4(&head.Nx);
    Oc_Flip4(&head.Ny);
    Oc_Flip4(&head.Nz);
    Oc_Flip4(&head.dimen);
    Oc_Flip4(&head.dataoffset);
  }

  // Check for nonsensical input
  if(head.dataoffset!=sizeof(Vf_VioFileHeader))
    { NonFatalError(STDDOC,ErrBadFileHeader); return 1; }
  if(strlen(head.Note)>=sizeof(head.Note))
    { NonFatalError(STDDOC,ErrBadFileHeader); return 1; }
  if(head.Nx<1 || head.Ny<1 || head.Nz<1 || head.dimen<1)
    { NonFatalError(STDDOC,ErrBadFileHeader); return 1; }
  if(head.dimen!=VECDIM)
    { NonFatalError(STDDOC,"Only %dD vectors (not %dD) currently supported",
		    VECDIM,head.dimen); return 1; }

  // Set class member variables
  width=head.Nx;  height=head.Nz; depth=head.Ny;
  // vio files use x & z in plane, y pointing into plane

  return 0;
#undef MEMBERNAME
}

OC_INT4m Vf_VioFile::FillArray(Vf_GridVec3f &grid)
{ // Reads magnetization vector array.
  // Assumes fptr is correctly positioned.
#define MEMBERNAME "FillArray"
  if(fptr==(FILE *)NULL)
    FatalError(-1,STDDOC,"File stream not initialized");
  OC_INDEX grid_width,grid_height,grid_depth;
  grid.GetDimens(grid_width,grid_height,grid_depth);
  if(grid_width!=width || grid_height!=height || grid_depth!=depth)
    FatalError(-1,STDDOC,"Grid not properly sized for incoming data."
	       "  Try using Vf_VioFile::GetDimensions().");

  // Read in data, 1 column at a time (vio file is stored columnwise)
  assert(height>=0);
  OC_REAL8 *rowbuf=new OC_REAL8[size_t(height)*VECDIM];
  if(rowbuf==NULL) FatalError(-1,STDDOC,ErrNoMem);
  for(OC_INDEX k=0;k<depth;k++) for(OC_INDEX i=0;i<width;i++) {
    if(fread((char *)rowbuf,sizeof(OC_REAL8)*VECDIM,size_t(height),fptr)
       !=(size_t)height) {
      NonFatalError(STDDOC,"%s (k=%ld, i=%ld)",ErrInputEOF,
                    long(k),long(i));
      delete[] rowbuf;
      return 1;
    }
    if(swap_input_bytes) Oc_SwapWords8(rowbuf,height*VECDIM);
#if VECDIM != 3
#error "Code in Vf_VioFile::FillArray assumes 3D vector input"
#endif
    for(OC_INDEX j=0;j<height;j++) {
      // Check for invalid input
      if(!Nb_IsFinite(rowbuf[3*j]) || !Nb_IsFinite(rowbuf[3*j+1])
         || !Nb_IsFinite(rowbuf[3*j+2])) {
        NonFatalError(STDDOC,"Invalid floating point data detected");
        delete[] rowbuf;
        return 1;
      }
      grid(i,j,k)=Nb_Vec3<OC_REAL8>(OC_REAL8(rowbuf[3*j]),
                                    OC_REAL8(rowbuf[3*j+2]),
                                    -OC_REAL8(rowbuf[3*j+1]));
      // vio file   uses x => left, z => up, y => into plane,
      // while grid uses x => left, y => up, z => out of plane.
    }
  }
  delete[] rowbuf;
  return 0;
#undef MEMBERNAME
}

void Vf_VioFile::CloseFile()
{ 
  // if(fptr!=(FILE *)NULL) fclose(fptr);
  // NOTE: *infile is closed by VFFI base destructor
  fptr=(FILE *)NULL; // Safety
}
