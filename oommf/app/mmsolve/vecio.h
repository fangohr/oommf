/* FILE: vecio.h             -*-Mode: c++-*-
 *
 * This file contains class declarations vector input/output.
 *
 */

#ifndef VECIO_H
#define VECIO_H

#include <cstdio>
#include <cstring>

/* End includes */

#define VFH_BADMAGIC      1
#define VFH_UNKNOWNENDIAN 2
#define VFH_BADOFFSET     3
#define VFH_BADNOTE       4
#define VFH_BADVALUES     5
#define VF_PREMATUREEOF   6

extern const char VecFileMagic02[8];
extern const double VecFileEndianTest;

struct FileHeader {
 public:
          FileHeader(void);
  OC_BYTE    Magic[8];
  OC_REAL8   EndianTest;
  OC_INT4    Nx,Ny,Nz,dimen,dataoffset;
  OC_BYTE    Reserved[36];
  OC_BYTE    Note[448];
  int     Read(FILE* fptr,int& swapinbytes);
  int     Write(FILE* fptr);
  int     Verify(void);
};

class FileVector {
 private:
  int          SwapInBytes; // 1 => swap byte order on binary input
  FILE*        fptr;
  FileHeader   fh;
  int          Nx,Ny,Nz,dimen,dataoffset;
 public:
  FileVector();
  ~FileVector(void);
  void    SetNote(OC_BYTE *newnote);
  const OC_BYTE* GetNote(void) { return fh.Note; }
  int     ReadHeader(const char *filename,
		     int &Xdimen,int &Ydimen,int &Zdimen,int &Vecdimen);
  /// Set filename==NULL or filename=="" to use stdin.
  int     ReadVecs(int count,double* arr);
  int     WriteHeader(const char *filename,
		      int Xdimen,int Ydimen,int Zdimen,int Vecdimen);
  /// Set filename==NULL or filename=="" to use stdout.
  int     WriteVecs(int count,double* arr);
  void    Close(void);
};

#endif /* .NOT. VECIO_H */
