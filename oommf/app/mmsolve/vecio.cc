/* FILE: vecio.cc             -*-Mode: c++-*-
 *
 * This file contains class definitions for vector input/output routines.
 *
 */

#include <cstdio>
#include <cstring>

#include "oc.h"
#include "nb.h"

#include "constants.h"
#include "vecio.h"

/* End includes */

#ifdef ALL_CHECKS
#define CODE_CHECKS
#define INFO_LEVEL 10
#endif

const char   VecFileMagic02[8] = "VecFil\x02";
const double VecFileEndianTest = 1.234;
// The bit pattern for 1.234 on a little endian machine (e.g.,
// Intel's x86 series) should be 58 39 B4 C8 76 BE F3 3F, while
// on a big endian machine it is 3F F3 BE 76 C8 B4 39 58.  Examples
// of big endian machines include ones using the MIPS R4000 and R8000
// series chips, for example SGI's.

FileHeader::FileHeader(void)
{
  int i;
  for(i=0;i<(int)sizeof(Reserved);i++) Reserved[i]='\0';
  for(i=0;i<(int)sizeof(Note);i++)     Note[i]='\0';
}

int FileHeader::Verify(void)
{
  if(strcmp((const char *)Magic,VecFileMagic02)!=0)
    return VFH_BADMAGIC;
  if(EndianTest!=VecFileEndianTest) {
    OC_REAL8 dummy=EndianTest;
    Oc_Flip8(&dummy);
    if(dummy!=VecFileEndianTest) return VFH_UNKNOWNENDIAN;
  }
  if(dataoffset!=sizeof(FileHeader))
    return VFH_BADOFFSET;
  if(strlen((const char *)Note)>=sizeof(Note))
    return VFH_BADNOTE;
  if(Nx<1 || Ny<1 || Nz<1 || dimen<1)
    return VFH_BADVALUES;
  return 0;
}

int FileHeader::Read(FILE* fptr,int& swapinbytes)
{
  if(fread((char *)Magic,sizeof(OC_BYTE),sizeof(Magic),fptr)!=sizeof(Magic))
    return VF_PREMATUREEOF;
  if(fread((char *)&EndianTest,sizeof(OC_REAL8),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fread((char *)&Nx,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fread((char *)&Ny,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fread((char *)&Nz,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fread((char *)&dimen,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fread((char *)&dataoffset,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fread((char *)Reserved,sizeof(OC_BYTE),sizeof(Reserved),fptr)
     !=sizeof(Reserved))
    return VF_PREMATUREEOF;
  if(fread((char *)Note,sizeof(OC_BYTE),sizeof(Note),fptr)!=sizeof(Note))
    return VF_PREMATUREEOF;

  if(EndianTest==VecFileEndianTest) swapinbytes=0;
  else {
    OC_REAL8 dummy=EndianTest;
    Oc_Flip8(&dummy);
    if(dummy==VecFileEndianTest) swapinbytes=1;
    else
      PlainError(1,"Error in FileHeader::Read: Unknown binary byte order");
  }

  if(swapinbytes) {
    printf("Reversing binary byte order on input\n");
    Oc_Flip4(&Nx); Oc_Flip4(&Ny); Oc_Flip4(&Nz);
    Oc_Flip4(&dimen); Oc_Flip4(&dataoffset);
  }
  return Verify();
}

int FileHeader::Write(FILE* fptr)
{
  if(fwrite((char *)Magic,sizeof(OC_BYTE),sizeof(Magic),fptr)!=sizeof(Magic))
    return VF_PREMATUREEOF;
  if(fwrite((char *)&EndianTest,sizeof(OC_REAL8),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fwrite((char *)&Nx,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fwrite((char *)&Ny,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fwrite((char *)&Nz,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fwrite((char *)&dimen,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fwrite((char *)&dataoffset,sizeof(OC_INT4),1,fptr)!=1)
    return VF_PREMATUREEOF;
  if(fwrite((char *)Reserved,sizeof(OC_BYTE),sizeof(Reserved),fptr)
     !=sizeof(Reserved))
    return VF_PREMATUREEOF;
  if(fwrite((char *)Note,sizeof(OC_BYTE),sizeof(Note),fptr)!=sizeof(Note))
    return VF_PREMATUREEOF;
  return Verify();
}

FileVector::FileVector()
{
  fptr=NULL;
  Nx=Ny=Nz=dimen=0;
}

FileVector::~FileVector(void)
{
  if(fptr!=NULL) Close();
}

void FileVector::SetNote(OC_BYTE *newnote)
{
  int i;
  if(newnote!=NULL) {
    strncpy((char *)fh.Note,(char *)newnote,sizeof(fh.Note));
    fh.Note[sizeof(fh.Note)-1]='\0'; // Safety
  }
  else {
    for(i=0;i<(int)sizeof(fh.Note);i++) fh.Note[i]='\0';
  }
}

int FileVector::ReadHeader(const char *filename,
			   int &Xdimen,int &Ydimen,int &Zdimen,int &Vecdimen)
{
  if(filename == NULL || filename[0] == '\0') fptr=stdin;
  else if((fptr=Nb_FOpen(filename,"rb"))==NULL) {
    PlainWarning("Warning in FileVector::ReadHeader: %s",ErrBadFileOpen);
    return -1;
  }
  if(fh.Read(fptr,SwapInBytes)!=0) {
    PlainWarning("Warning in FileVector::ReadHeader: %s",ErrBadFile);
    return -2;
  }
  Xdimen=Nx=fh.Nx;
  Ydimen=Ny=fh.Ny;
  Zdimen=Nz=fh.Nz;
  Vecdimen=dimen=fh.dimen;
  dataoffset=fh.dataoffset;
  fseek(fptr,dataoffset,SEEK_SET);  // Reposition to start of data
  return 0;
}

int FileVector::WriteHeader(const char *filename,
			   int Xdimen,int Ydimen,int Zdimen,int Vecdimen)
{
  if(filename == NULL || filename[0] == '\0') fptr=stdout;
  else if((fptr=Nb_FOpen(filename,"wb"))==NULL) {
    PlainWarning("Warning in FileVector::WriteHeader: %s",ErrBadFileOpen);
    return -1;
  }

  strncpy((char *)fh.Magic,VecFileMagic02,sizeof(fh.Magic)-1);
  fh.Magic[sizeof(fh.Magic)-1]='\0';  // Safety
  fh.EndianTest=VecFileEndianTest;
  fh.Nx=Nx=Xdimen;
  fh.Ny=Ny=Ydimen;
  fh.Nz=Nz=Zdimen;
  fh.dimen=dimen=Vecdimen;
  fh.dataoffset=dataoffset=sizeof(struct FileHeader);

  if(fh.Write(fptr)!=0) {
    PlainWarning("Warning in FileVector::WriteHeader: %s",ErrBadFile);
    return -2;
  }
  return 0;
}

int FileVector::ReadVecs(int count,double* arr)
{
  if(sizeof(double)!=2*sizeof(int))
    PlainError(1,"Error in FileVector::ReadVecs: "
	       "Code assumes data type double is twice as wide as int,"
	       " but sizeof(double)=%d, and sizeof(int)=%d",
	       sizeof(double),sizeof(int));
  if(fread((char *)arr,sizeof(double)*dimen,count,fptr)!=(unsigned)count)
    return VF_PREMATUREEOF;
  if(SwapInBytes) Oc_SwapWords8(arr,count*dimen);

  return 0;
}

int FileVector::WriteVecs(int count,double* arr)
{
  if(fwrite((char *)arr,sizeof(double)*dimen,count,fptr)
     !=(unsigned)count)
    return VF_PREMATUREEOF;
  return 0;
}

void FileVector::Close(void)
{
  if(fptr!=(FILE *)NULL) {
    fclose(fptr);  fptr=NULL;
  }
}
